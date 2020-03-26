/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDestinationNode.h"
#include "AudioContext.h"
#include "AlignmentUtils.h"
#include "AudioContext.h"
#include "CubebUtils.h"
#include "mozilla/dom/AudioDestinationNodeBinding.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "mozilla/dom/OfflineAudioCompletionEvent.h"
#include "mozilla/dom/power/PowerManagerService.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WakeLock.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "MediaTrackGraph.h"
#include "nsContentUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/Promise.h"

extern mozilla::LazyLogModule gAudioChannelLog;

#define AUDIO_CHANNEL_LOG(msg, ...) \
  MOZ_LOG(gAudioChannelLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

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
        OfflineAudioCompletionEvent::Constructor(
            mAudioContext, NS_LITERAL_STRING("complete"), param);
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
        mVolume(1.0f),
        mLastInputAudible(false),
        mSuspended(false),
        mSampleRate(CubebUtils::PreferredSampleRate()) {
    MOZ_ASSERT(aNode);
  }

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    *aOutput = aInput;
    aOutput->mVolume *= mVolume;

    if (mSuspended) {
      return;
    }

    bool isInputAudible =
        !aInput.IsNull() && !aInput.IsMuted() && aInput.IsAudible();

    auto shouldNotifyChanged = [&]() {
      // We don't want to notify state changed frequently if the input track is
      // consist of interleaving audible and inaudible blocks. This situation is
      // really common, especially when user is using OscillatorNode to produce
      // sound. Sending unnessary runnable frequently would cause performance
      // debasing. If the track contains 10 interleaving samples and 5 of them
      // are audible, others are inaudible, user would tend to feel the track
      // is audible. Therefore, we have the loose checking when track is
      // changing from inaudible to audible, but have strict checking when
      // streaming is changing from audible to inaudible. If the inaudible
      // blocks continue over a speicific time threshold, then we will treat the
      // track as inaudible.
      if (isInputAudible && !mLastInputAudible) {
        return true;
      }
      // Use more strict condition, choosing 1 seconds as a threshold.
      if (!isInputAudible && mLastInputAudible &&
          aFrom - mLastInputAudibleTime >= mSampleRate) {
        return true;
      }
      return false;
    };
    if (shouldNotifyChanged()) {
      mLastInputAudible = isInputAudible;
      RefPtr<AudioNodeTrack> track = aTrack;
      auto r = [track, isInputAudible]() -> void {
        MOZ_ASSERT(NS_IsMainThread());
        RefPtr<AudioNode> node = track->Engine()->NodeMainThread();
        if (node) {
          RefPtr<AudioDestinationNode> destinationNode =
              static_cast<AudioDestinationNode*>(node.get());
          destinationNode->NotifyAudibleStateChanged(isInputAudible);
        }
      };

      aTrack->Graph()->DispatchToMainThreadStableState(NS_NewRunnableFunction(
          "dom::WebAudioAudibleStateChangedRunnable", r));
    }

    if (isInputAudible) {
      mLastInputAudibleTime = aFrom;
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
      mVolume = aParam;
    }
  }

  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override {
    if (aIndex == SUSPENDED) {
      mSuspended = !!aParam;
      if (mSuspended) {
        mLastInputAudible = false;
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
  float mVolume;
  bool mLastInputAudible;
  GraphTime mLastInputAudibleTime = 0;
  bool mSuspended;
  int mSampleRate;
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
                                           bool aAllowedToStart,
                                           uint32_t aNumberOfChannels,
                                           uint32_t aLength)
    : AudioNode(aContext, aNumberOfChannels, ChannelCountMode::Explicit,
                ChannelInterpretation::Speakers),
      mFramesToProduce(aLength),
      mIsOffline(aIsOffline),
      mAudioChannelSuspended(false),
      mAudible(AudioChannelService::AudibleState::eAudible),
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

  if (aAllowedToStart) {
    graph->NotifyWhenGraphStarted(mTrack)->Then(
        aContext->GetMainThread(), "AudioDestinationNode OnRunning",
        [context = RefPtr<AudioContext>(aContext)] {
          context->OnStateChanged(nullptr, AudioContextState::Running);
        },
        [] {
          NS_WARNING(
              "AudioDestinationNode's graph never started processing audio");
        });

    CreateAudioWakeLockIfNeeded();
  }
}

AudioDestinationNode::~AudioDestinationNode() {
  ReleaseAudioWakeLockIfExists();
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
  MediaTrackGraph* graph = MediaTrackGraph::CreateNonRealtimeInstance(
      context->SampleRate(), context->GetParentObject());
  AudioNodeEngine* engine = new OfflineDestinationNodeEngine(this);

  mTrack = AudioNodeTrack::Create(context, engine, kTrackFlags, graph);
  mTrack->AddMainThreadListener(this);

  return mTrack;
}

void AudioDestinationNode::DestroyAudioChannelAgent() {
  if (mAudioChannelAgent && !Context()->IsOffline()) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
    // Reset the state, and it would always be regard as audible.
    mAudible = AudioChannelService::AudibleState::eAudible;
    if (IsCapturingAudio()) {
      StopAudioCapturingTrack();
    }
  }
}

void AudioDestinationNode::DestroyMediaTrack() {
  DestroyAudioChannelAgent();

  if (!mTrack) {
    return;
  }

  Context()->ShutdownWorklet();

  mTrack->RemoveMainThreadListener(this);
  MediaTrackGraph* graph = mTrack->Graph();
  if (graph->IsNonRealtime()) {
    MediaTrackGraph::DestroyNonRealtimeInstance(graph);
  }
  AudioNode::DestroyMediaTrack();
}

void AudioDestinationNode::NotifyMainThreadTrackEnded() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mTrack->IsEnded());

  if (mIsOffline && GetAbstractMainThread()) {
    GetAbstractMainThread()->Dispatch(NewRunnableMethod(
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
  DestroyAudioChannelAgent();
  SendInt32ParameterToTrack(DestinationNodeEngine::SUSPENDED, 1);
  ReleaseAudioWakeLockIfExists();
}

void AudioDestinationNode::Resume() {
  CreateAudioChannelAgent();
  SendInt32ParameterToTrack(DestinationNodeEngine::SUSPENDED, 0);
  CreateAudioWakeLockIfNeeded();
}

void AudioDestinationNode::OfflineShutdown() {
  MOZ_ASSERT(Context() && Context()->IsOffline(),
             "Should only be called on a valid OfflineAudioContext");

  if (mTrack) {
    MediaTrackGraph::DestroyNonRealtimeInstance(mTrack->Graph());
    mOfflineRenderingRef.Drop(this);
  }
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
  if (!mTrack) {
    return NS_OK;
  }

  AUDIO_CHANNEL_LOG(
      "AudioDestinationNode %p WindowVolumeChanged, "
      "aVolume = %f, aMuted = %s\n",
      this, aVolume, aMuted ? "true" : "false");

  float volume = aMuted ? 0.0 : aVolume;
  mTrack->SetAudioOutputVolume(nullptr, volume);

  AudioChannelService::AudibleState audible =
      volume > 0.0 ? AudioChannelService::AudibleState::eAudible
                   : AudioChannelService::AudibleState::eNotAudible;
  if (mAudible != audible) {
    mAudible = audible;
    mAudioChannelAgent->NotifyStartedAudible(
        mAudible, AudioChannelService::AudibleChangedReasons::eVolumeChanged);
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowSuspendChanged(nsSuspendedTypes aSuspend) {
  if (!mTrack) {
    return NS_OK;
  }

  bool suspended = (aSuspend != nsISuspendedTypes::NONE_SUSPENDED);
  if (mAudioChannelSuspended == suspended) {
    return NS_OK;
  }

  AUDIO_CHANNEL_LOG(
      "AudioDestinationNode %p WindowSuspendChanged, "
      "aSuspend = %s\n",
      this, SuspendTypeToStr(aSuspend));

  mAudioChannelSuspended = suspended;

  DisabledTrackMode disabledMode =
      suspended ? DisabledTrackMode::SILENCE_BLACK : DisabledTrackMode::ENABLED;
  mTrack->SetEnabled(disabledMode);

  AudioChannelService::AudibleState audible =
      aSuspend == nsISuspendedTypes::NONE_SUSPENDED
          ? AudioChannelService::AudibleState::eAudible
          : AudioChannelService::AudibleState::eNotAudible;
  if (mAudible != audible) {
    mAudible = audible;
    mAudioChannelAgent->NotifyStartedAudible(
        audible,
        AudioChannelService::AudibleChangedReasons::ePauseStateChanged);
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowAudioCaptureChanged(bool aCapture) {
  MOZ_ASSERT(mAudioChannelAgent);

  if (!mTrack || Context()->IsOffline()) {
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
  if (!mWakeLock) {
    RefPtr<power::PowerManagerService> pmService =
        power::PowerManagerService::GetInstance();
    NS_ENSURE_TRUE_VOID(pmService);

    ErrorResult rv;
    mWakeLock = pmService->NewWakeLock(NS_LITERAL_STRING("audio-playing"),
                                       GetOwner(), rv);
  }
}

void AudioDestinationNode::ReleaseAudioWakeLockIfExists() {
  if (mWakeLock) {
    IgnoredErrorResult rv;
    mWakeLock->Unlock(rv);
    mWakeLock = nullptr;
  }
}

nsresult AudioDestinationNode::CreateAudioChannelAgent() {
  if (mIsOffline || mAudioChannelAgent) {
    return NS_OK;
  }

  mAudioChannelAgent = new AudioChannelAgent();
  nsresult rv = mAudioChannelAgent->InitWithWeakCallback(GetOwner(), this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void AudioDestinationNode::NotifyAudibleStateChanged(bool aAudible) {
  MOZ_ASSERT(Context() && !Context()->IsOffline());

  if (!mAudioChannelAgent) {
    if (!aAudible) {
      return;
    }
    CreateAudioChannelAgent();
  }

  AUDIO_CHANNEL_LOG(
      "AudioDestinationNode %p NotifyAudibleStateChanged, audible=%d", this,
      aAudible);

  if (!aAudible) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    // Reset the state, and it would always be regard as audible.
    mAudible = AudioChannelService::AudibleState::eAudible;
    if (IsCapturingAudio()) {
      StopAudioCapturingTrack();
    }
    return;
  }

  if (mDurationBeforeFirstTimeAudible.IsZero()) {
    MOZ_ASSERT(aAudible);
    mDurationBeforeFirstTimeAudible = TimeStamp::Now() - mCreatedTime;
    Telemetry::Accumulate(Telemetry::WEB_AUDIO_BECOMES_AUDIBLE_TIME,
                          mDurationBeforeFirstTimeAudible.ToSeconds());
  }

  nsresult rv = mAudioChannelAgent->NotifyStartedPlaying(mAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  mAudioChannelAgent->PullInitialUpdate();
}

}  // namespace dom
}  // namespace mozilla
