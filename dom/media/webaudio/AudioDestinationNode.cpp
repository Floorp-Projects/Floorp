/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDestinationNode.h"

#include "AlignmentUtils.h"
#include "AudibilityMonitor.h"
#include "AudioChannelService.h"
#include "AudioContext.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "CubebUtils.h"
#include "MediaTrackGraph.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/AudioDestinationNodeBinding.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/dom/OfflineAudioCompletionEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WakeLock.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "nsContentUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsServiceManagerUtils.h"
#include "Tracing.h"

extern mozilla::LazyLogModule gAudioChannelLog;

#define AUDIO_CHANNEL_LOG(msg, ...) \
  MOZ_LOG(gAudioChannelLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla::dom {

namespace {
class OnCompleteTask final : public Runnable {
 public:
  OnCompleteTask(AudioContext* aAudioContext, AudioBuffer* aRenderedBuffer)
      : Runnable("dom::OfflineDestinationNodeEngine::OnCompleteTask"),
        mAudioContext(aAudioContext),
        mRenderedBuffer(aRenderedBuffer) {}

  NS_IMETHOD Run() override {
    OfflineAudioCompletionEventInit param;
    param.mRenderedBuffer = mRenderedBuffer;

    RefPtr<OfflineAudioCompletionEvent> event =
        OfflineAudioCompletionEvent::Constructor(mAudioContext, u"complete"_ns,
                                                 param);
    mAudioContext->DispatchTrustedEvent(event);

    return NS_OK;
  }

 private:
  RefPtr<AudioContext> mAudioContext;
  RefPtr<AudioBuffer> mRenderedBuffer;
};
}  // anonymous namespace

class OfflineDestinationNodeEngine final : public AudioNodeEngine {
 public:
  explicit OfflineDestinationNodeEngine(AudioDestinationNode* aNode)
      : AudioNodeEngine(aNode),
        mWriteIndex(0),
        mNumberOfChannels(aNode->ChannelCount()),
        mLength(aNode->Length()),
        mSampleRate(aNode->Context()->SampleRate()),
        mBufferAllocated(false) {}

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    TRACE("OfflineDestinationNodeEngine::ProcessBlock");
    // Do this just for the sake of political correctness; this output
    // will not go anywhere.
    *aOutput = aInput;

    // The output buffer is allocated lazily, on the rendering thread, when
    // non-null input is received.
    if (!mBufferAllocated && !aInput.IsNull()) {
      // These allocations might fail if content provides a huge number of
      // channels or size, but it's OK since we'll deal with the failure
      // gracefully.
      mBuffer = ThreadSharedFloatArrayBufferList::Create(mNumberOfChannels,
                                                         mLength, fallible);
      if (mBuffer && mWriteIndex) {
        // Zero leading for any null chunks that were skipped.
        for (uint32_t i = 0; i < mNumberOfChannels; ++i) {
          float* channelData = mBuffer->GetDataForWrite(i);
          PodZero(channelData, mWriteIndex);
        }
      }

      mBufferAllocated = true;
    }

    // Skip copying if there is no buffer.
    uint32_t outputChannelCount = mBuffer ? mNumberOfChannels : 0;

    // Record our input buffer
    MOZ_ASSERT(mWriteIndex < mLength, "How did this happen?");
    const uint32_t duration =
        std::min(WEBAUDIO_BLOCK_SIZE, mLength - mWriteIndex);
    const uint32_t inputChannelCount = aInput.ChannelCount();
    for (uint32_t i = 0; i < outputChannelCount; ++i) {
      float* outputData = mBuffer->GetDataForWrite(i) + mWriteIndex;
      if (aInput.IsNull() || i >= inputChannelCount) {
        PodZero(outputData, duration);
      } else {
        const float* inputBuffer =
            static_cast<const float*>(aInput.mChannelData[i]);
        if (duration == WEBAUDIO_BLOCK_SIZE && IS_ALIGNED16(inputBuffer)) {
          // Use the optimized version of the copy with scale operation
          AudioBlockCopyChannelWithScale(inputBuffer, aInput.mVolume,
                                         outputData);
        } else {
          if (aInput.mVolume == 1.0f) {
            PodCopy(outputData, inputBuffer, duration);
          } else {
            for (uint32_t j = 0; j < duration; ++j) {
              outputData[j] = aInput.mVolume * inputBuffer[j];
            }
          }
        }
      }
    }
    mWriteIndex += duration;

    if (mWriteIndex >= mLength) {
      NS_ASSERTION(mWriteIndex == mLength, "Overshot length");
      // Go to finished state. When the graph's current time eventually reaches
      // the end of the track, then the main thread will be notified and we'll
      // shut down the AudioContext.
      *aFinished = true;
    }
  }

  bool IsActive() const override {
    // Keep processing to track track time, which is used for all timelines
    // associated with the same AudioContext.
    return true;
  }

  already_AddRefed<AudioBuffer> CreateAudioBuffer(AudioContext* aContext) {
    MOZ_ASSERT(NS_IsMainThread());
    // Create the input buffer
    ErrorResult rv;
    RefPtr<AudioBuffer> renderedBuffer =
        AudioBuffer::Create(aContext->GetOwner(), mNumberOfChannels, mLength,
                            mSampleRate, mBuffer.forget(), rv);
    if (rv.Failed()) {
      rv.SuppressException();
      return nullptr;
    }

    return renderedBuffer.forget();
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    if (mBuffer) {
      amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  // The input to the destination node is recorded in mBuffer.
  // When this buffer fills up with mLength frames, the buffered input is sent
  // to the main thread in order to dispatch OfflineAudioCompletionEvent.
  RefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  // An index representing the next offset in mBuffer to be written to.
  uint32_t mWriteIndex;
  uint32_t mNumberOfChannels;
  // How many frames the OfflineAudioContext intends to produce.
  uint32_t mLength;
  float mSampleRate;
  bool mBufferAllocated;
};

class DestinationNodeEngine final : public AudioNodeEngine {
 public:
  explicit DestinationNodeEngine(AudioDestinationNode* aNode)
      : AudioNodeEngine(aNode),
        mSampleRate(CubebUtils::PreferredSampleRate(
            aNode->Context()->ShouldResistFingerprinting())),
        mVolume(1.0f),
        mAudibilityMonitor(
            mSampleRate,
            StaticPrefs::dom_media_silence_duration_for_audibility()),
        mSuspended(false),
        mIsAudible(false) {
    MOZ_ASSERT(aNode);
  }

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    TRACE("DestinationNodeEngine::ProcessBlock");
    *aOutput = aInput;
    aOutput->mVolume *= mVolume;

    if (mSuspended) {
      return;
    }

    mAudibilityMonitor.Process(aInput);
    bool isAudible =
        mAudibilityMonitor.RecentlyAudible() && aOutput->mVolume > 0.0;
    if (isAudible != mIsAudible) {
      mIsAudible = isAudible;
      RefPtr<AudioNodeTrack> track = aTrack;
      auto r = [track, isAudible]() -> void {
        MOZ_ASSERT(NS_IsMainThread());
        RefPtr<AudioNode> node = track->Engine()->NodeMainThread();
        if (node) {
          RefPtr<AudioDestinationNode> destinationNode =
              static_cast<AudioDestinationNode*>(node.get());
          destinationNode->NotifyDataAudibleStateChanged(isAudible);
        }
      };

      aTrack->Graph()->DispatchToMainThreadStableState(NS_NewRunnableFunction(
          "dom::WebAudioAudibleStateChangedRunnable", r));
    }
  }

  bool IsActive() const override {
    // Keep processing to track track time, which is used for all timelines
    // associated with the same AudioContext.  If there are no other engines
    // for the AudioContext, then this could return false to suspend the
    // track, but the track is blocked anyway through
    // AudioDestinationNode::SetIsOnlyNodeForContext().
    return true;
  }

  void SetDoubleParameter(uint32_t aIndex, double aParam) override {
    if (aIndex == VOLUME) {
      mVolume = static_cast<float>(aParam);
    }
  }

  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override {
    if (aIndex == SUSPENDED) {
      mSuspended = !!aParam;
      if (mSuspended) {
        mIsAudible = false;
      }
    }
  }

  enum Parameters {
    VOLUME,
    SUSPENDED,
  };

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  int mSampleRate;
  float mVolume;
  AudibilityMonitor mAudibilityMonitor;
  bool mSuspended;
  bool mIsAudible;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioDestinationNode, AudioNode,
                                   mAudioChannelAgent, mOfflineRenderingPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioDestinationNode)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(AudioDestinationNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(AudioDestinationNode, AudioNode)

const AudioNodeTrack::Flags kTrackFlags =
    AudioNodeTrack::NEED_MAIN_THREAD_CURRENT_TIME |
    AudioNodeTrack::NEED_MAIN_THREAD_ENDED | AudioNodeTrack::EXTERNAL_OUTPUT;

AudioDestinationNode::AudioDestinationNode(AudioContext* aContext,
                                           bool aIsOffline,
                                           uint32_t aNumberOfChannels,
                                           uint32_t aLength)
    : AudioNode(aContext, aNumberOfChannels, ChannelCountMode::Explicit,
                ChannelInterpretation::Speakers),
      mFramesToProduce(aLength),
      mIsOffline(aIsOffline),
      mCreatedTime(TimeStamp::Now()) {
  if (aIsOffline) {
    // The track is created on demand to avoid creating a graph thread that
    // may not be used.
    return;
  }

  // GetParentObject can return nullptr here. This will end up creating another
  // MediaTrackGraph
  MediaTrackGraph* graph = MediaTrackGraph::GetInstance(
      MediaTrackGraph::AUDIO_THREAD_DRIVER, aContext->GetParentObject(),
      aContext->SampleRate(), MediaTrackGraph::DEFAULT_OUTPUT_DEVICE);
  AudioNodeEngine* engine = new DestinationNodeEngine(this);

  mTrack = AudioNodeTrack::Create(aContext, engine, kTrackFlags, graph);
  mTrack->AddMainThreadListener(this);
  // null key is fine: only one output per mTrack
  mTrack->AddAudioOutput(nullptr);
}

void AudioDestinationNode::Init() {
  // The reason we don't do that in ctor is because we have to keep AudioContext
  // holding a strong reference to the destination node first. If we don't do
  // that, initializing the agent would cause an unexpected destroy of the
  // destination node when destroying the local weak reference inside
  // `InitWithWeakCallback()`.
  if (!mIsOffline) {
    CreateAndStartAudioChannelAgent();
  }
}

void AudioDestinationNode::Close() {
  DestroyAudioChannelAgentIfExists();
  ReleaseAudioWakeLockIfExists();
}

void AudioDestinationNode::CreateAndStartAudioChannelAgent() {
  MOZ_ASSERT(!mIsOffline);
  MOZ_ASSERT(!mAudioChannelAgent);

  AudioChannelAgent* agent = new AudioChannelAgent();
  nsresult rv = agent->InitWithWeakCallback(GetOwner(), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AUDIO_CHANNEL_LOG("Failed to init audio channel agent");
    return;
  }

  AudibleState state =
      IsAudible() ? AudibleState::eAudible : AudibleState::eNotAudible;
  rv = agent->NotifyStartedPlaying(state);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    AUDIO_CHANNEL_LOG("Failed to start audio channel agent");
    return;
  }

  mAudioChannelAgent = agent;
  mAudioChannelAgent->PullInitialUpdate();
}

AudioDestinationNode::~AudioDestinationNode() {
  MOZ_ASSERT(!mAudioChannelAgent);
  MOZ_ASSERT(!mWakeLock);
  MOZ_ASSERT(!mCaptureTrackPort);
}

size_t AudioDestinationNode::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  // Might be useful in the future:
  // - mAudioChannelAgent
  return amount;
}

size_t AudioDestinationNode::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

AudioNodeTrack* AudioDestinationNode::Track() {
  if (mTrack) {
    return mTrack;
  }

  AudioContext* context = Context();
  if (!context) {  // This node has been unlinked.
    return nullptr;
  }

  MOZ_ASSERT(mIsOffline, "Realtime tracks are created in constructor");

  // GetParentObject can return nullptr here when the document has been
  // unlinked.
  MediaTrackGraph* graph =
      MediaTrackGraph::CreateNonRealtimeInstance(context->SampleRate());
  AudioNodeEngine* engine = new OfflineDestinationNodeEngine(this);

  mTrack = AudioNodeTrack::Create(context, engine, kTrackFlags, graph);
  mTrack->AddMainThreadListener(this);

  return mTrack;
}

void AudioDestinationNode::DestroyAudioChannelAgentIfExists() {
  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
    if (IsCapturingAudio()) {
      StopAudioCapturingTrack();
    }
  }
}

void AudioDestinationNode::DestroyMediaTrack() {
  Close();
  if (!mTrack) {
    return;
  }

  Context()->ShutdownWorklet();

  mTrack->RemoveMainThreadListener(this);
  AudioNode::DestroyMediaTrack();
}

void AudioDestinationNode::NotifyMainThreadTrackEnded() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTrack->IsEnded());

  if (mIsOffline) {
    AbstractThread::MainThread()->Dispatch(NewRunnableMethod(
        "dom::AudioDestinationNode::FireOfflineCompletionEvent", this,
        &AudioDestinationNode::FireOfflineCompletionEvent));
  }
}

void AudioDestinationNode::FireOfflineCompletionEvent() {
  AudioContext* context = Context();
  context->OfflineClose();

  OfflineDestinationNodeEngine* engine =
      static_cast<OfflineDestinationNodeEngine*>(Track()->Engine());
  RefPtr<AudioBuffer> renderedBuffer = engine->CreateAudioBuffer(context);
  if (!renderedBuffer) {
    return;
  }
  ResolvePromise(renderedBuffer);

  context->Dispatch(do_AddRef(new OnCompleteTask(context, renderedBuffer)));

  context->OnStateChanged(nullptr, AudioContextState::Closed);

  mOfflineRenderingRef.Drop(this);
}

void AudioDestinationNode::ResolvePromise(AudioBuffer* aRenderedBuffer) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIsOffline);
  mOfflineRenderingPromise->MaybeResolve(aRenderedBuffer);
}

uint32_t AudioDestinationNode::MaxChannelCount() const {
  return Context()->MaxChannelCount();
}

void AudioDestinationNode::SetChannelCount(uint32_t aChannelCount,
                                           ErrorResult& aRv) {
  if (aChannelCount > MaxChannelCount()) {
    aRv.ThrowIndexSizeError(
        nsPrintfCString("%u is larger than maxChannelCount", aChannelCount));
    return;
  }

  if (aChannelCount == ChannelCount()) {
    return;
  }

  AudioNode::SetChannelCount(aChannelCount, aRv);
}

void AudioDestinationNode::Mute() {
  MOZ_ASSERT(Context() && !Context()->IsOffline());
  SendDoubleParameterToTrack(DestinationNodeEngine::VOLUME, 0.0f);
}

void AudioDestinationNode::Unmute() {
  MOZ_ASSERT(Context() && !Context()->IsOffline());
  SendDoubleParameterToTrack(DestinationNodeEngine::VOLUME, 1.0f);
}

void AudioDestinationNode::Suspend() {
  SendInt32ParameterToTrack(DestinationNodeEngine::SUSPENDED, 1);
}

void AudioDestinationNode::Resume() {
  SendInt32ParameterToTrack(DestinationNodeEngine::SUSPENDED, 0);
}

void AudioDestinationNode::NotifyAudioContextStateChanged() {
  UpdateFinalAudibleStateIfNeeded(AudibleChangedReasons::ePauseStateChanged);
}

void AudioDestinationNode::OfflineShutdown() {
  MOZ_ASSERT(Context() && Context()->IsOffline(),
             "Should only be called on a valid OfflineAudioContext");

  mOfflineRenderingRef.Drop(this);
}

JSObject* AudioDestinationNode::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return AudioDestinationNode_Binding::Wrap(aCx, this, aGivenProto);
}

void AudioDestinationNode::StartRendering(Promise* aPromise) {
  mOfflineRenderingPromise = aPromise;
  mOfflineRenderingRef.Take(this);
  Track()->Graph()->StartNonRealtimeProcessing(mFramesToProduce);
}

NS_IMETHODIMP
AudioDestinationNode::WindowVolumeChanged(float aVolume, bool aMuted) {
  MOZ_ASSERT(mAudioChannelAgent);
  if (!mTrack) {
    return NS_OK;
  }

  AUDIO_CHANNEL_LOG(
      "AudioDestinationNode %p WindowVolumeChanged, "
      "aVolume = %f, aMuted = %s\n",
      this, aVolume, aMuted ? "true" : "false");

  mAudioChannelVolume = aMuted ? 0.0f : aVolume;
  mTrack->SetAudioOutputVolume(nullptr, mAudioChannelVolume);
  UpdateFinalAudibleStateIfNeeded(AudibleChangedReasons::eVolumeChanged);
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowSuspendChanged(nsSuspendedTypes aSuspend) {
  MOZ_ASSERT(mAudioChannelAgent);
  if (!mTrack) {
    return NS_OK;
  }

  const bool shouldDisable = aSuspend == nsISuspendedTypes::SUSPENDED_BLOCK;
  if (mAudioChannelDisabled == shouldDisable) {
    return NS_OK;
  }
  mAudioChannelDisabled = shouldDisable;

  AUDIO_CHANNEL_LOG(
      "AudioDestinationNode %p WindowSuspendChanged, shouldDisable = %d\n",
      this, mAudioChannelDisabled);

  DisabledTrackMode disabledMode = mAudioChannelDisabled
                                       ? DisabledTrackMode::SILENCE_BLACK
                                       : DisabledTrackMode::ENABLED;
  mTrack->SetDisabledTrackMode(disabledMode);
  UpdateFinalAudibleStateIfNeeded(AudibleChangedReasons::ePauseStateChanged);
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowAudioCaptureChanged(bool aCapture) {
  MOZ_ASSERT(mAudioChannelAgent);
  if (!mTrack) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> ownerWindow = GetOwner();
  if (!ownerWindow) {
    return NS_OK;
  }

  if (aCapture == IsCapturingAudio()) {
    return NS_OK;
  }

  if (aCapture) {
    StartAudioCapturingTrack();
  } else {
    StopAudioCapturingTrack();
  }

  return NS_OK;
}

bool AudioDestinationNode::IsCapturingAudio() const {
  return mCaptureTrackPort != nullptr;
}

void AudioDestinationNode::StartAudioCapturingTrack() {
  MOZ_ASSERT(!IsCapturingAudio());
  nsCOMPtr<nsPIDOMWindowInner> window = Context()->GetParentObject();
  uint64_t id = window->WindowID();
  mCaptureTrackPort = mTrack->Graph()->ConnectToCaptureTrack(id, mTrack);
}

void AudioDestinationNode::StopAudioCapturingTrack() {
  MOZ_ASSERT(IsCapturingAudio());
  mCaptureTrackPort->Destroy();
  mCaptureTrackPort = nullptr;
}

void AudioDestinationNode::CreateAudioWakeLockIfNeeded() {
  if (!mWakeLock && IsAudible()) {
    RefPtr<power::PowerManagerService> pmService =
        power::PowerManagerService::GetInstance();
    NS_ENSURE_TRUE_VOID(pmService);

    ErrorResult rv;
    mWakeLock = pmService->NewWakeLock(u"audio-playing"_ns, GetOwner(), rv);
  }
}

void AudioDestinationNode::ReleaseAudioWakeLockIfExists() {
  if (mWakeLock) {
    IgnoredErrorResult rv;
    mWakeLock->Unlock(rv);
    mWakeLock = nullptr;
  }
}

void AudioDestinationNode::NotifyDataAudibleStateChanged(bool aAudible) {
  MOZ_ASSERT(!mIsOffline);

  AUDIO_CHANNEL_LOG(
      "AudioDestinationNode %p NotifyDataAudibleStateChanged, audible=%d", this,
      aAudible);

  if (mDurationBeforeFirstTimeAudible.IsZero()) {
    MOZ_ASSERT(aAudible);
    mDurationBeforeFirstTimeAudible = TimeStamp::Now() - mCreatedTime;
    Telemetry::Accumulate(Telemetry::WEB_AUDIO_BECOMES_AUDIBLE_TIME,
                          mDurationBeforeFirstTimeAudible.ToSeconds());
  }

  mIsDataAudible = aAudible;
  UpdateFinalAudibleStateIfNeeded(AudibleChangedReasons::eDataAudibleChanged);
}

void AudioDestinationNode::UpdateFinalAudibleStateIfNeeded(
    AudibleChangedReasons aReason) {
  // The audio context has been closed and we've destroyed the agent.
  if (!mAudioChannelAgent) {
    return;
  }
  const bool newAudibleState = IsAudible();
  if (mFinalAudibleState == newAudibleState) {
    return;
  }
  AUDIO_CHANNEL_LOG("AudioDestinationNode %p Final audible state=%d", this,
                    newAudibleState);
  mFinalAudibleState = newAudibleState;
  AudibleState state =
      mFinalAudibleState ? AudibleState::eAudible : AudibleState::eNotAudible;
  mAudioChannelAgent->NotifyStartedAudible(state, aReason);
  if (mFinalAudibleState) {
    CreateAudioWakeLockIfNeeded();
  } else {
    ReleaseAudioWakeLockIfExists();
  }
}

bool AudioDestinationNode::IsAudible() const {
  // The desitionation node will be regarded as audible if all following
  // conditions are true.
  // (1) data audible state : both audio input and output are audible
  // (2) window audible state : the tab isn't muted by tab sound indicator
  // (3) audio context state : audio context should be running
  return Context()->State() == AudioContextState::Running && mIsDataAudible &&
         mAudioChannelVolume != 0.0;
}

}  // namespace mozilla::dom
