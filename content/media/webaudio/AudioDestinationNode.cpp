/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDestinationNode.h"
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
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocShell.h"
#include "nsIPermissionManager.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsServiceManagerUtils.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"

namespace mozilla {
namespace dom {

static uint8_t gWebAudioOutputKey;

class OfflineDestinationNodeEngine : public AudioNodeEngine
{
public:
  typedef AutoFallibleTArray<nsAutoArrayPtr<float>, 2> InputChannels;

  OfflineDestinationNodeEngine(AudioDestinationNode* aNode,
                               uint32_t aNumberOfChannels,
                               uint32_t aLength,
                               float aSampleRate)
    : AudioNodeEngine(aNode)
    , mWriteIndex(0)
    , mLength(aLength)
    , mSampleRate(aSampleRate)
  {
    // These allocations might fail if content provides a huge number of
    // channels or size, but it's OK since we'll deal with the failure
    // gracefully.
    if (mInputChannels.SetLength(aNumberOfChannels)) {
      static const fallible_t fallible = fallible_t();
      for (uint32_t i = 0; i < aNumberOfChannels; ++i) {
        mInputChannels[i] = new(fallible) float[aLength];
        if (!mInputChannels[i]) {
          mInputChannels.Clear();
          break;
        }
      }
    }
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) MOZ_OVERRIDE
  {
    // Do this just for the sake of political correctness; this output
    // will not go anywhere.
    *aOutput = aInput;

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

    nsRefPtr<OfflineAudioCompletionEvent> event =
        new OfflineAudioCompletionEvent(context, nullptr, nullptr);
    event->InitEvent(renderedBuffer);
    context->DispatchTrustedEvent(event);
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mInputChannels.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
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
  // How many frames the OfflineAudioContext intends to produce.
  uint32_t mLength;
  float mSampleRate;
};

class InputMutedRunnable : public nsRunnable
{
public:
  InputMutedRunnable(AudioNodeStream* aStream,
                     bool aInputMuted)
    : mStream(aStream)
    , mInputMuted(aInputMuted)
  {
  }

  NS_IMETHOD Run()
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

class DestinationNodeEngine : public AudioNodeEngine
{
public:
  explicit DestinationNodeEngine(AudioDestinationNode* aNode)
    : AudioNodeEngine(aNode)
    , mVolume(1.0f)
    , mLastInputMuted(true)
  {
    MOZ_ASSERT(aNode);
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) MOZ_OVERRIDE
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

  virtual void SetDoubleParameter(uint32_t aIndex, double aParam) MOZ_OVERRIDE
  {
    if (aIndex == VOLUME) {
      mVolume = aParam;
    }
  }

  enum Parameters {
    VOLUME,
  };

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const MOZ_OVERRIDE
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

class EventProxyHandler MOZ_FINAL : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS

  explicit EventProxyHandler(nsIDOMEventListener* aNode)
  {
    MOZ_ASSERT(aNode);
    mWeakNode = do_GetWeakReference(aNode);
  }

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) MOZ_OVERRIDE
  {
    nsCOMPtr<nsIDOMEventListener> listener = do_QueryReferent(mWeakNode);
    if (!listener) {
      return NS_OK;
    }

    auto node = static_cast<AudioDestinationNode*>(listener.get());
    return node->HandleEvent(aEvent);
  }

private:
  ~EventProxyHandler()
  { }

  nsWeakPtr mWeakNode;
};

NS_IMPL_ISUPPORTS(EventProxyHandler, nsIDOMEventListener)

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioDestinationNode, AudioNode,
                                   mAudioChannelAgent, mEventProxyHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AudioDestinationNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(AudioDestinationNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(AudioDestinationNode, AudioNode)

AudioDestinationNode::AudioDestinationNode(AudioContext* aContext,
                                           bool aIsOffline,
                                           AudioChannel aChannel,
                                           uint32_t aNumberOfChannels,
                                           uint32_t aLength,
                                           float aSampleRate)
  : AudioNode(aContext,
              aIsOffline ? aNumberOfChannels : 2,
              ChannelCountMode::Explicit,
              ChannelInterpretation::Speakers)
  , mFramesToProduce(aLength)
  , mAudioChannel(AudioChannel::Normal)
  , mIsOffline(aIsOffline)
  , mHasFinished(false)
  , mAudioChannelAgentPlaying(false)
  , mExtraCurrentTime(0)
  , mExtraCurrentTimeSinceLastStartedBlocking(0)
  , mExtraCurrentTimeUpdatedSinceLastStableState(false)
{
  MediaStreamGraph* graph = aIsOffline ?
                            MediaStreamGraph::CreateNonRealtimeInstance(aSampleRate) :
                            MediaStreamGraph::GetInstance();
  AudioNodeEngine* engine = aIsOffline ?
                            new OfflineDestinationNodeEngine(this, aNumberOfChannels,
                                                             aLength, aSampleRate) :
                            static_cast<AudioNodeEngine*>(new DestinationNodeEngine(this));

  mStream = graph->CreateAudioNodeStream(engine, MediaStreamGraph::EXTERNAL_STREAM);
  mStream->AddMainThreadListener(this);
  mStream->AddAudioOutput(&gWebAudioOutputKey);

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
AudioDestinationNode::DestroyMediaStream()
{
  if (mAudioChannelAgent && !Context()->IsOffline()) {
    mAudioChannelAgent->StopPlaying();
    mAudioChannelAgent = nullptr;

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
    NS_ENSURE_TRUE_VOID(target);

    target->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                      mEventProxyHelper,
                                      /* useCapture = */ true);
  }

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
AudioDestinationNode::NotifyMainThreadStateChanged()
{
  if (mStream->IsFinished() && !mHasFinished) {
    mHasFinished = true;
    if (mIsOffline) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethod(this, &AudioDestinationNode::FireOfflineCompletionEvent);
      NS_DispatchToCurrentThread(runnable);
    }
  }
}

void
AudioDestinationNode::FireOfflineCompletionEvent()
{
  AudioNodeStream* stream = static_cast<AudioNodeStream*>(Stream());
  OfflineDestinationNodeEngine* engine =
    static_cast<OfflineDestinationNodeEngine*>(stream->Engine());
  engine->FireOfflineCompletionEvent(this);
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
AudioDestinationNode::WrapObject(JSContext* aCx)
{
  return AudioDestinationNodeBinding::Wrap(aCx, this);
}

void
AudioDestinationNode::StartRendering()
{
  mOfflineRenderingRef.Take(this);
  mStream->Graph()->StartNonRealtimeProcessing(TrackRate(Context()->SampleRate()), mFramesToProduce);
}

void
AudioDestinationNode::SetCanPlay(bool aCanPlay)
{
  mStream->SetTrackEnabled(AudioNodeStream::AUDIO_TRACK, aCanPlay);
}

NS_IMETHODIMP
AudioDestinationNode::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  if (!type.EqualsLiteral("visibilitychange")) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE(docshell, NS_ERROR_FAILURE);

  bool isActive = false;
  docshell->GetIsActive(&isActive);

  mAudioChannelAgent->SetVisibilityState(isActive);
  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::CanPlayChanged(int32_t aCanPlay)
{
  bool playing = aCanPlay == AudioChannelState::AUDIO_CHANNEL_STATE_NORMAL;
  if (playing == mAudioChannelAgentPlaying) {
    return NS_OK;
  }

  mAudioChannelAgentPlaying = playing;
  SetCanPlay(playing);

  Context()->DispatchTrustedEvent(
    playing ? NS_LITERAL_STRING("mozinterruptend")
            : NS_LITERAL_STRING("mozinterruptbegin"));

  return NS_OK;
}

NS_IMETHODIMP
AudioDestinationNode::WindowVolumeChanged()
{
  MOZ_ASSERT(mAudioChannelAgent);

  if (!mStream) {
    return NS_OK;
  }

  float volume;
  nsresult rv = mAudioChannelAgent->GetWindowVolume(&volume);
  NS_ENSURE_SUCCESS(rv, rv);

  mStream->SetAudioOutputVolume(&gWebAudioOutputKey, volume);
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
  if (!Preferences::GetBool("media.useAudioChannelService")) {
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

  if (!mEventProxyHelper) {
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
    if (target) {
      // We use a proxy because otherwise the event listerner would hold a
      // reference of the destination node, and by extension, everything
      // connected to it.
      mEventProxyHelper = new EventProxyHandler(this);
      target->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                     mEventProxyHelper,
                                     /* useCapture = */ true,
                                     /* wantsUntrusted = */ false);
    }
  }

  if (mAudioChannelAgent) {
    mAudioChannelAgent->StopPlaying();
  }

  mAudioChannelAgent = new AudioChannelAgent();
  mAudioChannelAgent->InitWithWeakCallback(GetOwner(),
                                           static_cast<int32_t>(mAudioChannel),
                                           this);

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  if (docshell) {
    bool isActive = false;
    docshell->GetIsActive(&isActive);
    mAudioChannelAgent->SetVisibilityState(isActive);
  }
}

void
AudioDestinationNode::NotifyStableState()
{
  mExtraCurrentTimeUpdatedSinceLastStableState = false;
}

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

void
AudioDestinationNode::ScheduleStableStateNotification()
{
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  if (appShell) {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &AudioDestinationNode::NotifyStableState);
    appShell->RunInStableState(event);
  }
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
    mAudioChannelAgent->StopPlaying();
    return;
  }

  int32_t state = 0;
  mAudioChannelAgent->StartPlaying(&state);
  mAudioChannelAgentPlaying =
    state == AudioChannelState::AUDIO_CHANNEL_STATE_NORMAL;
  SetCanPlay(mAudioChannelAgentPlaying);
}

} // dom namespace
} // mozilla namespace
