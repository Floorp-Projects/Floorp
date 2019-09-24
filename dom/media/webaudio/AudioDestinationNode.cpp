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
#include "mozilla/dom/OfflineAudioCompletionEvent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/BaseAudioContextBinding.h"
#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "MediaStreamGraph.h"
#include "nsContentUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShell.h"
#include "nsIPermissionManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/Promise.h"

extern mozilla::LazyLogModule gAudioChannelLog;

#define AUDIO_CHANNEL_LOG(msg, ...) \
  MOZ_LOG(gAudioChannelLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

static uint8_t gWebAudioOutputKey;

class OfflineDestinationNodeEngine final : public AudioNodeEngine {
 public:
  explicit OfflineDestinationNodeEngine(AudioDestinationNode* aNode)
      : AudioNodeEngine(aNode),
        mWriteIndex(0),
        mNumberOfChannels(aNode->ChannelCount()),
        mLength(aNode->Length()),
        mSampleRate(aNode->Context()->SampleRate()),
        mBufferAllocated(false) {}

  void ProcessBlock(AudioNodeStream* aStream, GraphTime aFrom,
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
      // the end of the stream, then the main thread will be notified and we'll
      // shut down the AudioContext.
      *aFinished = true;
    }
  }

  bool IsActive() const override {
    // Keep processing to track stream time, which is used for all timelines
    // associated with the same AudioContext.
    return true;
  }

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

  void FireOfflineCompletionEvent(AudioDestinationNode* aNode) {
    AudioContext* context = aNode->Context();
    context->Shutdown();
    // Shutdown drops self reference, but the context is still referenced by
    // aNode, which is strongly referenced by the runnable that called
    // AudioDestinationNode::FireOfflineCompletionEvent.

    // Create the input buffer
    ErrorResult rv;
    RefPtr<AudioBuffer> renderedBuffer =
        AudioBuffer::Create(context->GetOwner(), mNumberOfChannels, mLength,
                            mSampleRate, mBuffer.forget(), rv);
    if (rv.Failed()) {
      rv.SuppressException();
      return;
    }

    aNode->ResolvePromise(renderedBuffer);

    context->Dispatch(do_AddRef(new OnCompleteTask(context, renderedBuffer)));

    context->OnStateChanged(nullptr, AudioContextState::Closed);
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

  void ProcessBlock(AudioNodeStream* aStream, GraphTime aFrom,
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
      // We don't want to notify state changed frequently if the input stream is
      // consist of interleaving audible and inaudible blocks. This situation is
      // really common, especially when user is using OscillatorNode to produce
      // sound. Sending unnessary runnable frequently would cause performance
      // debasing. If the stream contains 10 interleaving samples and 5 of them
      // are audible, others are inaudible, user would tend to feel the stream
      // is audible. Therefore, we have the loose checking when stream is
      // changing from inaudible to audible, but have strict checking when
      // streaming is changing from audible to inaudible. If the inaudible
      // blocks continue over a speicific time threshold, then we will treat the
      // stream as inaudible.
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
      RefPtr<AudioNodeStream> stream = aStream;
      auto r = [stream, isInputAudible]() -> void {
        MOZ_ASSERT(NS_IsMainThread());
        RefPtr<AudioNode> node = stream->Engine()->NodeMainThread();
        if (node) {
          RefPtr<AudioDestinationNode> destinationNode =
              static_cast<AudioDestinationNode*>(node.get());
          destinationNode->NotifyAudibleStateChanged(isInputAudible);
        }
      };

      aStream->Graph()->DispatchToMainThreadStableState(NS_NewRunnableFunction(
          "dom::WebAudioAudibleStateChangedRunnable", r));
    }

    if (isInputAudible) {
      mLastInputAudibleTime = aFrom;
    }
  }

  bool IsActive() const override {
    // Keep processing to track stream time, which is used for all timelines
    // associated with the same AudioContext.  If there are no other engines
    // for the AudioContext, then this could return false to suspend the
    // stream, but the stream is blocked anyway through
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

const AudioNodeStream::Flags kStreamFlags =
    AudioNodeStream::NEED_MAIN_THREAD_CURRENT_TIME |
    AudioNodeStream::NEED_MAIN_THREAD_FINISHED |
    AudioNodeStream::EXTERNAL_OUTPUT;

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
      mCaptured(false),
      mAudible(AudioChannelService::AudibleState::eAudible),
      mCreatedTime(TimeStamp::Now()) {
  if (aIsOffline) {
    // The stream is created on demand to avoid creating a graph thread that
    // may not be used.
    return;
  }

  // GetParentObject can return nullptr here. This will end up creating another
  // MediaStreamGraph
  MediaStreamGraph* graph = MediaStreamGraph::GetInstance(
      MediaStreamGraph::AUDIO_THREAD_DRIVER, aContext->GetParentObject(),
      aContext->SampleRate());
  AudioNodeEngine* engine = new DestinationNodeEngine(this);

  mStream = AudioNodeStream::Create(aContext, engine, kStreamFlags, graph);
  mStream->AddMainThreadListener(this);
  mStream->AddAudioOutput(&gWebAudioOutputKey);

  if (aAllowedToStart) {
    graph->NotifyWhenGraphStarted(mStream);
  }
}

AudioDestinationNode::~AudioDestinationNode() {}

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

AudioNodeStream* AudioDestinationNode::Stream() {
  if (mStream) {
    return mStream;
  }

  AudioContext* context = Context();
  if (!context) {  // This node has been unlinked.
    return nullptr;
  }

  MOZ_ASSERT(mIsOffline, "Realtime streams are created in constructor");

  // GetParentObject can return nullptr here when the document has been
  // unlinked.
  MediaStreamGraph* graph = MediaStreamGraph::CreateNonRealtimeInstance(
      context->SampleRate(), context->GetParentObject());
  AudioNodeEngine* engine = new OfflineDestinationNodeEngine(this);

  mStream = AudioNodeStream::Create(context, engine, kStreamFlags, graph);
  mStream->AddMainThreadListener(this);

  return mStream;
}

void AudioDestinationNode::DestroyAudioChannelAgent() {
  if (mAudioChannelAgent && !Context()->IsOffline()) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
    // Reset the state, and it would always be regard as audible.
    mAudible = AudioChannelService::AudibleState::eAudible;
  }
}

void AudioDestinationNode::DestroyMediaStream() {
  DestroyAudioChannelAgent();

  if (!mStream) return;

  Context()->ShutdownWorklet();

  mStream->RemoveMainThreadListener(this);
  MediaStreamGraph* graph = mStream->Graph();
  if (graph->IsNonRealtime()) {
    MediaStreamGraph::DestroyNonRealtimeInstance(graph);
  }
  AudioNode::DestroyMediaStream();
}

void AudioDestinationNode::NotifyMainThreadStreamFinished() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mStream->IsFinished());

  if (mIsOffline && GetAbstractMainThread()) {
    GetAbstractMainThread()->Dispatch(NewRunnableMethod(
        "dom::AudioDestinationNode::FireOfflineCompletionEvent", this,
        &AudioDestinationNode::FireOfflineCompletionEvent));
  }
}

void AudioDestinationNode::FireOfflineCompletionEvent() {
  OfflineDestinationNodeEngine* engine =
      static_cast<OfflineDestinationNodeEngine*>(Stream()->Engine());
  engine->FireOfflineCompletionEvent(this);
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
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  AudioNode::SetChannelCount(aChannelCount, aRv);
}

void AudioDestinationNode::Mute() {
  MOZ_ASSERT(Context() && !Context()->IsOffline());
  SendDoubleParameterToStream(DestinationNodeEngine::VOLUME, 0.0f);
}

void AudioDestinationNode::Unmute() {
  MOZ_ASSERT(Context() && !Context()->IsOffline());
  SendDoubleParameterToStream(DestinationNodeEngine::VOLUME, 1.0f);
}

void AudioDestinationNode::Suspend() {
  DestroyAudioChannelAgent();
  SendInt32ParameterToStream(DestinationNodeEngine::SUSPENDED, 1);
}

void AudioDestinationNode::Resume() {
  CreateAudioChannelAgent();
  SendInt32ParameterToStream(DestinationNodeEngine::SUSPENDED, 0);
}

void AudioDestinationNode::OfflineShutdown() {
  MOZ_ASSERT(Context() && Context()->IsOffline(),
             "Should only be called on a valid OfflineAudioContext");

  if (mStream) {
    MediaStreamGraph::DestroyNonRealtimeInstance(mStream->Graph());
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
  Stream()->Graph()->StartNonRealtimeProcessing(mFramesToProduce);
}

NS_IMETHODIMP
AudioDestinationNode::WindowVolumeChanged(float aVolume, bool aMuted) {
  if (!mStream) {
    return NS_OK;
  }

  AUDIO_CHANNEL_LOG(
      "AudioDestinationNode %p WindowVolumeChanged, "
      "aVolume = %f, aMuted = %s\n",
      this, aVolume, aMuted ? "true" : "false");

  float volume = aMuted ? 0.0 : aVolume;
  mStream->SetAudioOutputVolume(&gWebAudioOutputKey, volume);

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
  if (!mStream) {
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
  mStream->SetTrackEnabled(AudioNodeStream::AUDIO_TRACK, disabledMode);

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

  if (!mStream || Context()->IsOffline()) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> ownerWindow = GetOwner();
  if (!ownerWindow) {
    return NS_OK;
  }

  if (aCapture != mCaptured) {
    if (aCapture) {
      nsCOMPtr<nsPIDOMWindowInner> window = Context()->GetParentObject();
      uint64_t id = window->WindowID();
      mCaptureStreamPort =
          mStream->Graph()->ConnectToCaptureStream(id, mStream);
    } else {
      mCaptureStreamPort->Destroy();
    }
    mCaptured = aCapture;
  }

  return NS_OK;
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
    return;
  }

  if (mDurationBeforeFirstTimeAudible.IsZero()) {
    MOZ_ASSERT(aAudible);
    mDurationBeforeFirstTimeAudible = TimeStamp::Now() - mCreatedTime;
    Telemetry::Accumulate(Telemetry::WEB_AUDIO_BECOMES_AUDIBLE_TIME,
                          mDurationBeforeFirstTimeAudible.ToSeconds());
  }

  AudioPlaybackConfig config;
  nsresult rv = mAudioChannelAgent->NotifyStartedPlaying(&config, mAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  WindowVolumeChanged(config.mVolume, config.mMuted);
  WindowSuspendChanged(config.mSuspend);
}

}  // namespace dom
}  // namespace mozilla
