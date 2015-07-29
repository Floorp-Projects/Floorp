/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDestinationNode.h"
#include "AudioContext.h"
#include "mozilla/dom/AudioDestinationNodeBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "MediaStreamGraph.h"
#include "OfflineAudioCompletionEvent.h"
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
  typedef AutoFallibleTArray<nsAutoArrayPtr<float>, 2> InputChannels;

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

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) override
  {
    // Do this just for the sake of political correctness; this output
    // will not go anywhere.
    *aOutput = aInput;

    // The output buffer is allocated lazily, on the rendering thread.
    if (!mBufferAllocated) {
      // These allocations might fail if content provides a huge number of
      // channels or size, but it's OK since we'll deal with the failure
      // gracefully.
      if (mInputChannels.SetLength(mNumberOfChannels, fallible)) {
        for (uint32_t i = 0; i < mNumberOfChannels; ++i) {
          mInputChannels[i] = new (fallible) float[mLength];
          if (!mInputChannels[i]) {
            mInputChannels.Clear();
            break;
          }
        }
      }

      mBufferAllocated = true;
    }

    // Handle the case of allocation failure in the input buffer
    if (mInputChannels.IsEmpty()) {
      return;
    }

    if (mWriteIndex >= mLength) {
      NS_ASSERTION(mWriteIndex == mLength, "Overshot length");
      // Don't record any more.
      return;
    }

    // Record our input buffer
    MOZ_ASSERT(mWriteIndex < mLength, "How did this happen?");
    const uint32_t duration = std::min(WEBAUDIO_BLOCK_SIZE, mLength - mWriteIndex);
    const uint32_t commonChannelCount = std::min(mInputChannels.Length(),
                                                 aInput.mChannelData.Length());
    // First, copy as many channels in the input as we have
    for (uint32_t i = 0; i < commonChannelCount; ++i) {
      if (aInput.IsNull()) {
        PodZero(mInputChannels[i] + mWriteIndex, duration);
      } else {
        const float* inputBuffer = static_cast<const float*>(aInput.mChannelData[i]);
        if (duration == WEBAUDIO_BLOCK_SIZE) {
          // Use the optimized version of the copy with scale operation
          AudioBlockCopyChannelWithScale(inputBuffer, aInput.mVolume,
                                         mInputChannels[i] + mWriteIndex);
        } else {
          if (aInput.mVolume == 1.0f) {
            PodCopy(mInputChannels[i] + mWriteIndex, inputBuffer, duration);
          } else {
            for (uint32_t j = 0; j < duration; ++j) {
              mInputChannels[i][mWriteIndex + j] = aInput.mVolume * inputBuffer[j];
            }
          }
        }
      }
    }
    // Then, silence all of the remaining channels
    for (uint32_t i = commonChannelCount; i < mInputChannels.Length(); ++i) {
      PodZero(mInputChannels[i] + mWriteIndex, duration);
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

  class OnCompleteTask final : public nsRunnable
  {
  public:
    OnCompleteTask(AudioContext* aAudioContext, AudioBuffer* aRenderedBuffer)
      : mAudioContext(aAudioContext)
      , mRenderedBuffer(aRenderedBuffer)
    {}

    NS_IMETHOD Run() override
    {
      nsRefPtr<OfflineAudioCompletionEvent> event =
          new OfflineAudioCompletionEvent(mAudioContext, nullptr, nullptr);
      event->InitEvent(mRenderedBuffer);
      mAudioContext->DispatchTrustedEvent(event);

      return NS_OK;
    }
  private:
    nsRefPtr<AudioContext> mAudioContext;
    nsRefPtr<AudioBuffer> mRenderedBuffer;
  };

  void FireOfflineCompletionEvent(AudioDestinationNode* aNode)
  {
    AudioContext* context = aNode->Context();
    context->Shutdown();
    // Shutdown drops self reference, but the context is still referenced by aNode,
    // which is strongly referenced by the runnable that called
    // AudioDestinationNode::FireOfflineCompletionEvent.

    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(aNode->GetOwner()))) {
      return;
    }
    JSContext* cx = jsapi.cx();

    // Create the input buffer
    ErrorResult rv;
    nsRefPtr<AudioBuffer> renderedBuffer =
      AudioBuffer::Create(context, mInputChannels.Length(),
                          mLength, mSampleRate, cx, rv);
    if (rv.Failed()) {
      return;
    }
    for (uint32_t i = 0; i < mInputChannels.Length(); ++i) {
      renderedBuffer->SetRawChannelContents(i, mInputChannels[i]);
    }

    aNode->ResolvePromise(renderedBuffer);

    nsRefPtr<OnCompleteTask> onCompleteTask =
      new OnCompleteTask(context, renderedBuffer);
    NS_DispatchToMainThread(onCompleteTask);

    context->OnStateChanged(nullptr, AudioContextState::Closed);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mInputChannels.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  // The input to the destination node is recorded in the mInputChannels buffer.
  // When this buffer fills up with mLength frames, the buffered input is sent
  // to the main thread in order to dispatch OfflineAudioCompletionEvent.
  InputChannels mInputChannels;
  // An index representing the next offset in mInputChannels to be written to.
  uint32_t mWriteIndex;
  uint32_t mNumberOfChannels;
  // How many frames the OfflineAudioContext intends to produce.
  uint32_t mLength;
  float mSampleRate;
  bool mBufferAllocated;
};

class InputMutedRunnable final : public nsRunnable
{
public:
  InputMutedRunnable(AudioNodeStream* aStream,
                     bool aInputMuted)
    : mStream(aStream)
    , mInputMuted(aInputMuted)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
    nsRefPtr<AudioNode> node = mStream->Engine()->NodeMainThread();

    if (node) {
      nsRefPtr<AudioDestinationNode> destinationNode =
        static_cast<AudioDestinationNode*>(node.get());
      destinationNode->InputMuted(mInputMuted);
    }
    return NS_OK;
  }

private:
  nsRefPtr<AudioNodeStream> mStream;
  bool mInputMuted;
};

class DestinationNodeEngine final : public AudioNodeEngine
{
public:
  explicit DestinationNodeEngine(AudioDestinationNode* aNode)
    : AudioNodeEngine(aNode)
    , mVolume(1.0f)
    , mLastInputMuted(false)
  {
    MOZ_ASSERT(aNode);
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) override
  {
    *aOutput = aInput;
    aOutput->mVolume *= mVolume;

    bool newInputMuted = aInput.IsNull() || aInput.IsMuted();
    if (newInputMuted != mLastInputMuted) {
      mLastInputMuted = newInputMuted;

      nsRefPtr<InputMutedRunnable> runnable =
        new InputMutedRunnable(aStream, newInputMuted);
      aStream->Graph()->
        DispatchToMainThreadAfterStreamStateUpdate(runnable.forget());
    }
  }

  virtual void SetDoubleParameter(uint32_t aIndex, double aParam) override
  {
    if (aIndex == VOLUME) {
      mVolume = aParam;
    }
  }

  enum Parameters {
    VOLUME,
  };

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  float mVolume;
  bool mLastInputMuted;
};

static bool UseAudioChannelService()
{
  return Preferences::GetBool("media.useAudioChannelService");
}

static bool UseAudioChannelAPI()
{
  return Preferences::GetBool("media.useAudioChannelAPI");
}

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
  , mAudioChannelAgentPlaying(false)
  , mExtraCurrentTime(0)
  , mExtraCurrentTimeSinceLastStartedBlocking(0)
  , mExtraCurrentTimeUpdatedSinceLastStableState(false)
  , mCaptured(false)
{
  bool startWithAudioDriver = true;
  MediaStreamGraph* graph = aIsOffline ?
                            MediaStreamGraph::CreateNonRealtimeInstance(aSampleRate) :
                            MediaStreamGraph::GetInstance(startWithAudioDriver, aChannel);
  AudioNodeEngine* engine = aIsOffline ?
                            new OfflineDestinationNodeEngine(this, aNumberOfChannels,
                                                             aLength, aSampleRate) :
                            static_cast<AudioNodeEngine*>(new DestinationNodeEngine(this));

  mStream = graph->CreateAudioNodeStream(engine, MediaStreamGraph::EXTERNAL_STREAM);
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
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethod(this, &AudioDestinationNode::FireOfflineCompletionEvent);
    NS_DispatchToCurrentThread(runnable);
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

void
AudioDestinationNode::SetCanPlay(float aVolume, bool aMuted)
{
  if (!mStream) {
    return;
  }

  mStream->SetTrackEnabled(AudioNodeStream::AUDIO_TRACK, !aMuted);
  mStream->SetAudioOutputVolume(&gWebAudioOutputKey, aVolume);
}

NS_IMETHODIMP
AudioDestinationNode::WindowVolumeChanged(float aVolume, bool aMuted)
{
  if (aMuted != mAudioChannelAgentPlaying) {
    mAudioChannelAgentPlaying = aMuted;

    if (UseAudioChannelAPI()) {
      Context()->DispatchTrustedEvent(
        !aMuted ? NS_LITERAL_STRING("mozinterruptend")
                : NS_LITERAL_STRING("mozinterruptbegin"));
    }
  }

  SetCanPlay(aVolume, aMuted);
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowAudioCaptureChanged()
{
  MOZ_ASSERT(mAudioChannelAgent);

  if (!mStream || Context()->IsOffline()) {
    return NS_OK;
  }

  bool captured = GetOwner()->GetAudioCaptured();

  if (captured != mCaptured) {
    if (captured) {
      nsCOMPtr<nsPIDOMWindow> window = Context()->GetParentObject();
      uint64_t id = window->WindowID();
      mCaptureStreamPort =
        mStream->Graph()->ConnectToCaptureStream(id, mStream);
    } else {
      mCaptureStreamPort->Disconnect();
      mCaptureStreamPort->Destroy();
    }
    mCaptured = captured;
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
  if (!UseAudioChannelService()) {
    return true;
  }

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

void
AudioDestinationNode::CreateAudioChannelAgent()
{
  if (mIsOffline || !UseAudioChannelService()) {
    return;
  }

  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
  }

  mAudioChannelAgent = new AudioChannelAgent();
  mAudioChannelAgent->InitWithWeakCallback(GetOwner(),
                                           static_cast<int32_t>(mAudioChannel),
                                           this);

  // The AudioChannelAgent must start playing immediately in order to avoid
  // race conditions with mozinterruptbegin/end events.
  InputMuted(false);

  WindowAudioCaptureChanged();
}

void
AudioDestinationNode::NotifyStableState()
{
  mExtraCurrentTimeUpdatedSinceLastStableState = false;
}

void
AudioDestinationNode::ScheduleStableStateNotification()
{
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &AudioDestinationNode::NotifyStableState);
  // Dispatch will fail if this is called on AudioNode destruction during
  // shutdown, in which case failure can be ignored.
  nsContentUtils::RunInStableState(event.forget(),
                                   nsContentUtils::
                                     DispatchFailureHandling::IgnoreFailure);
}

double
AudioDestinationNode::ExtraCurrentTime()
{
  if (!mStartedBlockingDueToBeingOnlyNode.IsNull() &&
      !mExtraCurrentTimeUpdatedSinceLastStableState) {
    mExtraCurrentTimeUpdatedSinceLastStableState = true;
    mExtraCurrentTimeSinceLastStartedBlocking =
      (TimeStamp::Now() - mStartedBlockingDueToBeingOnlyNode).ToSeconds();
    ScheduleStableStateNotification();
  }
  return mExtraCurrentTime + mExtraCurrentTimeSinceLastStartedBlocking;
}

void
AudioDestinationNode::SetIsOnlyNodeForContext(bool aIsOnlyNode)
{
  if (!mStartedBlockingDueToBeingOnlyNode.IsNull() == aIsOnlyNode) {
    // Nothing changed.
    return;
  }

  if (!mStream) {
    // DestroyMediaStream has been called, presumably during CC Unlink().
    return;
  }

  if (mIsOffline) {
    // Don't block the destination stream for offline AudioContexts, since
    // we expect the zero data produced when there are no other nodes to
    // show up in its result buffer. Also, we would get confused by adding
    // ExtraCurrentTime before StartRendering has even been called.
    return;
  }

  if (aIsOnlyNode) {
    mStream->ChangeExplicitBlockerCount(1);
    mStartedBlockingDueToBeingOnlyNode = TimeStamp::Now();
    // Don't do an update of mExtraCurrentTimeSinceLastStartedBlocking until the next stable state.
    mExtraCurrentTimeUpdatedSinceLastStableState = true;
    ScheduleStableStateNotification();
  } else {
    // Force update of mExtraCurrentTimeSinceLastStartedBlocking if necessary
    ExtraCurrentTime();
    mExtraCurrentTime += mExtraCurrentTimeSinceLastStartedBlocking;
    mExtraCurrentTimeSinceLastStartedBlocking = 0;
    mStream->ChangeExplicitBlockerCount(-1);
    mStartedBlockingDueToBeingOnlyNode = TimeStamp();
  }
}

void
AudioDestinationNode::InputMuted(bool aMuted)
{
  MOZ_ASSERT(Context() && !Context()->IsOffline());

  if (!mAudioChannelAgent) {
    return;
  }

  if (aMuted) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    return;
  }

  float volume = 0.0;
  bool muted = true;
  nsresult rv = mAudioChannelAgent->NotifyStartedPlaying(&volume, &muted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  WindowAudioCaptureChanged();
  WindowVolumeChanged(volume, muted);
}

} // namespace dom
} // namespace mozilla
