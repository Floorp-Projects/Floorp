/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDestinationNode.h"
#include "AlignmentUtils.h"
#include "AudioContext.h"
#include "mozilla/dom/AudioDestinationNodeBinding.h"
#include "mozilla/dom/OfflineAudioCompletionEvent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Services.h"
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

namespace mozilla {
namespace dom {

static uint8_t gWebAudioOutputKey;

class OfflineDestinationNodeEngine final : public AudioNodeEngine
{
public:
  OfflineDestinationNodeEngine(AudioDestinationNode* aNode,
                               uint32_t aNumberOfChannels,
                               uint32_t aLength,
                               float aSampleRate)
    : AudioNodeEngine(aNode)
    , mWriteIndex(0)
    , mNumberOfChannels(aNumberOfChannels)
    , mLength(aLength)
    , mSampleRate(aSampleRate)
    , mBufferAllocated(false)
  {
  }

  void ProcessBlock(AudioNodeStream* aStream,
                    GraphTime aFrom,
                    const AudioBlock& aInput,
                    AudioBlock* aOutput,
                    bool* aFinished) override
  {
    // Do this just for the sake of political correctness; this output
    // will not go anywhere.
    *aOutput = aInput;

    // The output buffer is allocated lazily, on the rendering thread, when
    // non-null input is received.
    if (!mBufferAllocated && !aInput.IsNull()) {
      // These allocations might fail if content provides a huge number of
      // channels or size, but it's OK since we'll deal with the failure
      // gracefully.
      mBuffer = ThreadSharedFloatArrayBufferList::
        Create(mNumberOfChannels, mLength, fallible);
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
    const uint32_t duration = std::min(WEBAUDIO_BLOCK_SIZE, mLength - mWriteIndex);
    const uint32_t inputChannelCount = aInput.ChannelCount();
    for (uint32_t i = 0; i < outputChannelCount; ++i) {
      float* outputData = mBuffer->GetDataForWrite(i) + mWriteIndex;
      if (aInput.IsNull() || i >= inputChannelCount) {
        PodZero(outputData, duration);
      } else {
        const float* inputBuffer = static_cast<const float*>(aInput.mChannelData[i]);
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

  bool IsActive() const override
  {
    // Keep processing to track stream time, which is used for all timelines
    // associated with the same AudioContext.
    return true;
  }


  class OnCompleteTask final : public Runnable
  {
  public:
    OnCompleteTask(AudioContext* aAudioContext, AudioBuffer* aRenderedBuffer)
      : Runnable("dom::OfflineDestinationNodeEngine::OnCompleteTask")
      , mAudioContext(aAudioContext)
      , mRenderedBuffer(aRenderedBuffer)
    {}

    NS_IMETHOD Run() override
    {
      OfflineAudioCompletionEventInit param;
      param.mRenderedBuffer = mRenderedBuffer;

      RefPtr<OfflineAudioCompletionEvent> event =
          OfflineAudioCompletionEvent::Constructor(mAudioContext,
                                                   NS_LITERAL_STRING("complete"),
                                                   param);
      mAudioContext->DispatchTrustedEvent(event);

      return NS_OK;
    }
  private:
    RefPtr<AudioContext> mAudioContext;
    RefPtr<AudioBuffer> mRenderedBuffer;
  };

  void FireOfflineCompletionEvent(AudioDestinationNode* aNode)
  {
    AudioContext* context = aNode->Context();
    context->Shutdown();
    // Shutdown drops self reference, but the context is still referenced by aNode,
    // which is strongly referenced by the runnable that called
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

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    if (mBuffer) {
      amount += mBuffer->SizeOfIncludingThis(aMallocSizeOf);
    }
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
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

class InputMutedRunnable final : public Runnable
{
public:
  InputMutedRunnable(AudioNodeStream* aStream, bool aInputMuted)
    : Runnable("dom::InputMutedRunnable")
    , mStream(aStream)
    , mInputMuted(aInputMuted)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    RefPtr<AudioNode> node = mStream->Engine()->NodeMainThread();

    if (node) {
      RefPtr<AudioDestinationNode> destinationNode =
        static_cast<AudioDestinationNode*>(node.get());
      destinationNode->InputMuted(mInputMuted);
    }
    return NS_OK;
  }

private:
  RefPtr<AudioNodeStream> mStream;
  bool mInputMuted;
};

class DestinationNodeEngine final : public AudioNodeEngine
{
public:
  explicit DestinationNodeEngine(AudioDestinationNode* aNode)
    : AudioNodeEngine(aNode)
    , mVolume(1.0f)
    , mLastInputMuted(true)
    , mSuspended(false)
  {
    MOZ_ASSERT(aNode);
  }

  void ProcessBlock(AudioNodeStream* aStream,
                    GraphTime aFrom,
                    const AudioBlock& aInput,
                    AudioBlock* aOutput,
                    bool* aFinished) override
  {
    *aOutput = aInput;
    aOutput->mVolume *= mVolume;

    if (mSuspended) {
      return;
    }

    bool newInputMuted = aInput.IsNull() || aInput.IsMuted();
    if (newInputMuted != mLastInputMuted) {
      mLastInputMuted = newInputMuted;

      RefPtr<InputMutedRunnable> runnable =
        new InputMutedRunnable(aStream, newInputMuted);
      aStream->Graph()->DispatchToMainThreadAfterStreamStateUpdate(
        runnable.forget());
    }
  }

  bool IsActive() const override
  {
    // Keep processing to track stream time, which is used for all timelines
    // associated with the same AudioContext.  If there are no other engines
    // for the AudioContext, then this could return false to suspend the
    // stream, but the stream is blocked anyway through
    // AudioDestinationNode::SetIsOnlyNodeForContext().
    return true;
  }

  void SetDoubleParameter(uint32_t aIndex, double aParam) override
  {
    if (aIndex == VOLUME) {
      mVolume = aParam;
    }
  }

  void SetInt32Parameter(uint32_t aIndex, int32_t aParam) override
  {
    if (aIndex == SUSPENDED) {
      mSuspended = !!aParam;
      if (mSuspended) {
        mLastInputMuted = true;
      }
    }
  }

  enum Parameters {
    VOLUME,
    SUSPENDED,
  };

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  float mVolume;
  bool mLastInputMuted;
  bool mSuspended;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioDestinationNode, AudioNode,
                                   mAudioChannelAgent,
                                   mOfflineRenderingPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioDestinationNode)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(AudioDestinationNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(AudioDestinationNode, AudioNode)

AudioDestinationNode::AudioDestinationNode(AudioContext* aContext,
                                           bool aIsOffline,
                                           AudioChannel aChannel,
                                           uint32_t aNumberOfChannels,
                                           uint32_t aLength, float aSampleRate)
  : AudioNode(aContext, aIsOffline ? aNumberOfChannels : 2,
              ChannelCountMode::Explicit, ChannelInterpretation::Speakers)
  , mFramesToProduce(aLength)
  , mAudioChannel(AudioChannel::Normal)
  , mIsOffline(aIsOffline)
  , mAudioChannelSuspended(false)
  , mCaptured(false)
  , mAudible(AudioChannelService::AudibleState::eAudible)
{
  nsPIDOMWindowInner* window = aContext->GetParentObject();
  MediaStreamGraph* graph =
    aIsOffline
      ? MediaStreamGraph::CreateNonRealtimeInstance(aSampleRate, window)
      : MediaStreamGraph::GetInstance(
          MediaStreamGraph::AUDIO_THREAD_DRIVER, aChannel, window);
  AudioNodeEngine* engine = aIsOffline ?
                            new OfflineDestinationNodeEngine(this, aNumberOfChannels,
                                                             aLength, aSampleRate) :
                            static_cast<AudioNodeEngine*>(new DestinationNodeEngine(this));

  AudioNodeStream::Flags flags =
    AudioNodeStream::NEED_MAIN_THREAD_CURRENT_TIME |
    AudioNodeStream::NEED_MAIN_THREAD_FINISHED |
    AudioNodeStream::EXTERNAL_OUTPUT;
  mStream = AudioNodeStream::Create(aContext, engine, flags, graph);
  mStream->AddMainThreadListener(this);
  mStream->AddAudioOutput(&gWebAudioOutputKey);

  if (!aIsOffline) {
    graph->NotifyWhenGraphStarted(mStream);
  }

  if (aChannel != AudioChannel::Normal) {
    ErrorResult rv;
    SetMozAudioChannelType(aChannel, rv);
  }
}

AudioDestinationNode::~AudioDestinationNode()
{
}

size_t
AudioDestinationNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  // Might be useful in the future:
  // - mAudioChannelAgent
  return amount;
}

size_t
AudioDestinationNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

void
AudioDestinationNode::DestroyAudioChannelAgent()
{
  if (mAudioChannelAgent && !Context()->IsOffline()) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
    // Reset the state, and it would always be regard as audible.
    mAudible = AudioChannelService::AudibleState::eAudible;
  }
}

void
AudioDestinationNode::DestroyMediaStream()
{
  DestroyAudioChannelAgent();

  if (!mStream)
    return;

  mStream->RemoveMainThreadListener(this);
  MediaStreamGraph* graph = mStream->Graph();
  if (graph->IsNonRealtime()) {
    MediaStreamGraph::DestroyNonRealtimeInstance(graph);
  }
  AudioNode::DestroyMediaStream();
}

void
AudioDestinationNode::NotifyMainThreadStreamFinished()
{
  MOZ_ASSERT(mStream->IsFinished());

  if (mIsOffline) {
    NS_DispatchToCurrentThread(
      NewRunnableMethod("dom::AudioDestinationNode::FireOfflineCompletionEvent",
                        this,
                        &AudioDestinationNode::FireOfflineCompletionEvent));
  }
}

void
AudioDestinationNode::FireOfflineCompletionEvent()
{
  OfflineDestinationNodeEngine* engine =
    static_cast<OfflineDestinationNodeEngine*>(Stream()->Engine());
  engine->FireOfflineCompletionEvent(this);
}

void
AudioDestinationNode::ResolvePromise(AudioBuffer* aRenderedBuffer)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIsOffline);
  mOfflineRenderingPromise->MaybeResolve(aRenderedBuffer);
}

uint32_t
AudioDestinationNode::MaxChannelCount() const
{
  return Context()->MaxChannelCount();
}

void
AudioDestinationNode::SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv)
{
  if (aChannelCount > MaxChannelCount()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  AudioNode::SetChannelCount(aChannelCount, aRv);
}

void
AudioDestinationNode::Mute()
{
  MOZ_ASSERT(Context() && !Context()->IsOffline());
  SendDoubleParameterToStream(DestinationNodeEngine::VOLUME, 0.0f);
}

void
AudioDestinationNode::Unmute()
{
  MOZ_ASSERT(Context() && !Context()->IsOffline());
  SendDoubleParameterToStream(DestinationNodeEngine::VOLUME, 1.0f);
}

void
AudioDestinationNode::Suspend()
{
  DestroyAudioChannelAgent();
  SendInt32ParameterToStream(DestinationNodeEngine::SUSPENDED, 1);
}

void
AudioDestinationNode::Resume()
{
  CreateAudioChannelAgent();
  SendInt32ParameterToStream(DestinationNodeEngine::SUSPENDED, 0);
}

void
AudioDestinationNode::OfflineShutdown()
{
  MOZ_ASSERT(Context() && Context()->IsOffline(),
             "Should only be called on a valid OfflineAudioContext");

  MediaStreamGraph::DestroyNonRealtimeInstance(mStream->Graph());
  mOfflineRenderingRef.Drop(this);
}

JSObject*
AudioDestinationNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AudioDestinationNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
AudioDestinationNode::StartRendering(Promise* aPromise)
{
  mOfflineRenderingPromise = aPromise;
  mOfflineRenderingRef.Take(this);
  mStream->Graph()->StartNonRealtimeProcessing(mFramesToProduce);
}

NS_IMETHODIMP
AudioDestinationNode::WindowVolumeChanged(float aVolume, bool aMuted)
{
  if (!mStream) {
    return NS_OK;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioDestinationNode, WindowVolumeChanged, "
          "this = %p, aVolume = %f, aMuted = %s\n",
          this, aVolume, aMuted ? "true" : "false"));

  float volume = aMuted ? 0.0 : aVolume;
  mStream->SetAudioOutputVolume(&gWebAudioOutputKey, volume);

  AudioChannelService::AudibleState audible = volume > 0.0 ?
    AudioChannelService::AudibleState::eAudible :
    AudioChannelService::AudibleState::eNotAudible;
  if (mAudible != audible) {
    mAudible = audible;
    mAudioChannelAgent->NotifyStartedAudible(mAudible,
                                             AudioChannelService::AudibleChangedReasons::eVolumeChanged);
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowSuspendChanged(nsSuspendedTypes aSuspend)
{
  if (!mStream) {
    return NS_OK;
  }

  bool suspended = (aSuspend != nsISuspendedTypes::NONE_SUSPENDED);
  if (mAudioChannelSuspended == suspended) {
    return NS_OK;
  }

  MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
         ("AudioDestinationNode, WindowSuspendChanged, "
          "this = %p, aSuspend = %s\n", this, SuspendTypeToStr(aSuspend)));

  mAudioChannelSuspended = suspended;

  DisabledTrackMode disabledMode = suspended ? DisabledTrackMode::SILENCE_BLACK
                                             : DisabledTrackMode::ENABLED;
  mStream->SetTrackEnabled(AudioNodeStream::AUDIO_TRACK, disabledMode);

  AudioChannelService::AudibleState audible =
    aSuspend == nsISuspendedTypes::NONE_SUSPENDED ?
      AudioChannelService::AudibleState::eAudible :
      AudioChannelService::AudibleState::eNotAudible;
  if (mAudible != audible) {
    mAudible = audible;
    mAudioChannelAgent->NotifyStartedAudible(audible,
                                             AudioChannelService::AudibleChangedReasons::ePauseStateChanged);
  }
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowAudioCaptureChanged(bool aCapture)
{
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

AudioChannel
AudioDestinationNode::MozAudioChannelType() const
{
  return mAudioChannel;
}

void
AudioDestinationNode::SetMozAudioChannelType(AudioChannel aValue, ErrorResult& aRv)
{
  if (Context()->IsOffline()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aValue != mAudioChannel &&
      CheckAudioChannelPermissions(aValue)) {
    mAudioChannel = aValue;

    if (mStream) {
      mStream->SetAudioChannelType(mAudioChannel);
    }

    if (mAudioChannelAgent) {
      CreateAudioChannelAgent();
    }
  }
}

bool
AudioDestinationNode::CheckAudioChannelPermissions(AudioChannel aValue)
{
  // Only normal channel doesn't need permission.
  if (aValue == AudioChannel::Normal) {
    return true;
  }

  // Maybe this audio channel is equal to the default one.
  if (aValue == AudioChannelService::GetDefaultAudioChannel()) {
    return true;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
    services::GetPermissionManager();
  if (!permissionManager) {
    return false;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(GetOwner());
  NS_ASSERTION(sop, "Window didn't QI to nsIScriptObjectPrincipal!");
  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();

  uint32_t perm = nsIPermissionManager::UNKNOWN_ACTION;

  nsCString channel;
  channel.AssignASCII(AudioChannelValues::strings[uint32_t(aValue)].value,
                      AudioChannelValues::strings[uint32_t(aValue)].length);
  permissionManager->TestExactPermissionFromPrincipal(principal,
    nsCString(NS_LITERAL_CSTRING("audio-channel-") + channel).get(),
    &perm);

  return perm == nsIPermissionManager::ALLOW_ACTION;
}

nsresult
AudioDestinationNode::CreateAudioChannelAgent()
{
  if (mIsOffline) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  if (mAudioChannelAgent) {
    rv = mAudioChannelAgent->NotifyStoppedPlaying();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mAudioChannelAgent = new AudioChannelAgent();
  rv = mAudioChannelAgent->InitWithWeakCallback(GetOwner(),
                                           static_cast<int32_t>(mAudioChannel),
                                           this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
AudioDestinationNode::InputMuted(bool aMuted)
{
  MOZ_ASSERT(Context() && !Context()->IsOffline());

  if (!mAudioChannelAgent) {
    if (aMuted) {
      return;
    }
    CreateAudioChannelAgent();
  }

  if (aMuted) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    // Reset the state, and it would always be regard as audible.
    mAudible = AudioChannelService::AudibleState::eAudible;
    return;
  }

  AudioPlaybackConfig config;
  nsresult rv = mAudioChannelAgent->NotifyStartedPlaying(&config,
                                                         mAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  WindowVolumeChanged(config.mVolume, config.mMuted);
  WindowSuspendChanged(config.mSuspend);
}

} // namespace dom
} // namespace mozilla
