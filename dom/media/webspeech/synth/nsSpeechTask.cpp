/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "AudioSegment.h"
#include "MediaStreamListener.h"
#include "nsSpeechTask.h"
#include "nsSynthVoiceRegistry.h"
#include "SharedBuffer.h"
#include "SpeechSynthesis.h"

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with nsSpeechTask::GetCurrentTime().
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#undef LOG
extern mozilla::LogModule* GetSpeechSynthLog();
#define LOG(type, msg) MOZ_LOG(GetSpeechSynthLog(), type, msg)

#define AUDIO_TRACK 1

namespace mozilla {
namespace dom {

class SynthStreamListener : public MediaStreamListener
{
public:
  SynthStreamListener(nsSpeechTask* aSpeechTask,
                      MediaStream* aStream,
                      AbstractThread* aMainThread)
    : mSpeechTask(aSpeechTask)
    , mStream(aStream)
    , mStarted(false)
  {
  }

  void DoNotifyStarted()
  {
    if (mSpeechTask) {
      mSpeechTask->DispatchStartInner();
    }
  }

  void DoNotifyFinished()
  {
    if (mSpeechTask) {
      mSpeechTask->DispatchEndInner(mSpeechTask->GetCurrentTime(),
                                    mSpeechTask->GetCurrentCharOffset());
    }
  }

  void NotifyEvent(MediaStreamGraph* aGraph,
                   MediaStreamGraphEvent event) override
  {
    switch (event) {
      case MediaStreamGraphEvent::EVENT_FINISHED:
        {
          if (!mStarted) {
            mStarted = true;
            aGraph->DispatchToMainThreadAfterStreamStateUpdate(
              NewRunnableMethod("dom::SynthStreamListener::DoNotifyStarted",
                                this,
                                &SynthStreamListener::DoNotifyStarted));
          }

          aGraph->DispatchToMainThreadAfterStreamStateUpdate(
            NewRunnableMethod("dom::SynthStreamListener::DoNotifyFinished",
                              this,
                              &SynthStreamListener::DoNotifyFinished));
        }
        break;
      case MediaStreamGraphEvent::EVENT_REMOVED:
        mSpeechTask = nullptr;
        // Dereference MediaStream to destroy safety
        mStream = nullptr;
        break;
      default:
        break;
    }
  }

  void NotifyBlockingChanged(MediaStreamGraph* aGraph, Blocking aBlocked) override
  {
    if (aBlocked == MediaStreamListener::UNBLOCKED && !mStarted) {
      mStarted = true;
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(
        NewRunnableMethod("dom::SynthStreamListener::DoNotifyStarted",
                          this,
                          &SynthStreamListener::DoNotifyStarted));
    }
  }

private:
  // Raw pointer; if we exist, the stream exists,
  // and 'mSpeechTask' exclusively owns it and therefor exists as well.
  nsSpeechTask* mSpeechTask;
  // This is KungFuDeathGrip for MediaStream
  RefPtr<MediaStream> mStream;

  bool mStarted;
};

// nsSpeechTask

NS_IMPL_CYCLE_COLLECTION(nsSpeechTask, mSpeechSynthesis, mUtterance, mCallback);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSpeechTask)
  NS_INTERFACE_MAP_ENTRY(nsISpeechTask)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechTask)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSpeechTask)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSpeechTask)

nsSpeechTask::nsSpeechTask(SpeechSynthesisUtterance* aUtterance, bool aIsChrome)
  : mUtterance(aUtterance)
  , mInited(false)
  , mPrePaused(false)
  , mPreCanceled(false)
  , mCallback(nullptr)
  , mIndirectAudio(false)
  , mIsChrome(aIsChrome)
{
  mText = aUtterance->mText;
  mVolume = aUtterance->Volume();
}

nsSpeechTask::nsSpeechTask(float aVolume, const nsAString& aText, bool aIsChrome)
  : mUtterance(nullptr)
  , mVolume(aVolume)
  , mText(aText)
  , mInited(false)
  , mPrePaused(false)
  , mPreCanceled(false)
  , mCallback(nullptr)
  , mIndirectAudio(false)
  , mIsChrome(aIsChrome)
{
}

nsSpeechTask::~nsSpeechTask()
{
  LOG(LogLevel::Debug, ("~nsSpeechTask"));
  if (mStream) {
    if (!mStream->IsDestroyed()) {
      mStream->Destroy();
    }

    // This will finally destroyed by SynthStreamListener becasue
    // MediaStream::Destroy() is async.
    mStream = nullptr;
  }

  if (mPort) {
    mPort->Destroy();
    mPort = nullptr;
  }
}

void
nsSpeechTask::InitDirectAudio()
{
  // nullptr as final argument here means that this is not tied to a window.
  // This is a global MSG.
  mStream = MediaStreamGraph::GetInstance(MediaStreamGraph::AUDIO_THREAD_DRIVER,
                                          AudioChannel::Normal, nullptr)->
    CreateSourceStream();
  mIndirectAudio = false;
  mInited = true;
}

void
nsSpeechTask::InitIndirectAudio()
{
  mIndirectAudio = true;
  mInited = true;
}

void
nsSpeechTask::SetChosenVoiceURI(const nsAString& aUri)
{
  mChosenVoiceURI = aUri;
}

NS_IMETHODIMP
nsSpeechTask::Setup(nsISpeechTaskCallback* aCallback,
                    uint32_t aChannels, uint32_t aRate, uint8_t argc)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(LogLevel::Debug, ("nsSpeechTask::Setup"));

  mCallback = aCallback;

  if (mIndirectAudio) {
    MOZ_ASSERT(!mStream);
    if (argc > 0) {
      NS_WARNING("Audio info arguments in Setup() are ignored for indirect audio services.");
    }
    return NS_OK;
  }

  // mStream is set up in Init() that should be called before this.
  MOZ_ASSERT(mStream);

  mStream->AddListener(
    // Non DocGroup-version of AbstractThread::MainThread for the task in parent.
    new SynthStreamListener(this, mStream, AbstractThread::MainThread()));

  // XXX: Support more than one channel
  if(NS_WARN_IF(!(aChannels == 1))) {
    return NS_ERROR_FAILURE;
  }

  mChannels = aChannels;

  AudioSegment* segment = new AudioSegment();
  mStream->AddAudioTrack(AUDIO_TRACK, aRate, 0, segment);
  mStream->AddAudioOutput(this);
  mStream->SetAudioOutputVolume(this, mVolume);

  return NS_OK;
}

static RefPtr<mozilla::SharedBuffer>
makeSamples(int16_t* aData, uint32_t aDataLen)
{
  RefPtr<mozilla::SharedBuffer> samples =
    SharedBuffer::Create(aDataLen * sizeof(int16_t));
  int16_t* frames = static_cast<int16_t*>(samples->Data());

  for (uint32_t i = 0; i < aDataLen; i++) {
    frames[i] = aData[i];
  }

  return samples;
}

NS_IMETHODIMP
nsSpeechTask::SendAudio(JS::Handle<JS::Value> aData, JS::Handle<JS::Value> aLandmarks,
                        JSContext* aCx)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if(NS_WARN_IF(!(mStream))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(mStream->IsDestroyed())) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(!(mChannels))) {
    return NS_ERROR_FAILURE;
  }
  if(NS_WARN_IF(!(aData.isObject()))) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mIndirectAudio) {
    NS_WARNING("Can't call SendAudio from an indirect audio speech service.");
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> darray(aCx, &aData.toObject());
  JSAutoCompartment ac(aCx, darray);

  JS::Rooted<JSObject*> tsrc(aCx, nullptr);

  // Allow either Int16Array or plain JS Array
  if (JS_IsInt16Array(darray)) {
    tsrc = darray;
  } else {
    bool isArray;
    if (!JS_IsArrayObject(aCx, darray, &isArray)) {
      return NS_ERROR_UNEXPECTED;
    }
    if (isArray) {
      tsrc = JS_NewInt16ArrayFromArray(aCx, darray);
    }
  }

  if (!tsrc) {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  uint32_t dataLen = JS_GetTypedArrayLength(tsrc);
  RefPtr<mozilla::SharedBuffer> samples;
  {
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    int16_t* data = JS_GetInt16ArrayData(tsrc, &isShared, nogc);
    if (isShared) {
      // Must opt in to using shared data.
      return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
    }
    samples = makeSamples(data, dataLen);
  }
  SendAudioImpl(samples, dataLen);

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::SendAudioNative(int16_t* aData, uint32_t aDataLen)
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if(NS_WARN_IF(!(mStream))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(mStream->IsDestroyed())) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(!(mChannels))) {
    return NS_ERROR_FAILURE;
  }

  if (mIndirectAudio) {
    NS_WARNING("Can't call SendAudio from an indirect audio speech service.");
    return NS_ERROR_FAILURE;
  }

  RefPtr<mozilla::SharedBuffer> samples = makeSamples(aData, aDataLen);
  SendAudioImpl(samples, aDataLen);

  return NS_OK;
}

void
nsSpeechTask::SendAudioImpl(RefPtr<mozilla::SharedBuffer>& aSamples, uint32_t aDataLen)
{
  if (aDataLen == 0) {
    mStream->EndAllTrackAndFinish();
    return;
  }

  AudioSegment segment;
  AutoTArray<const int16_t*, 1> channelData;
  channelData.AppendElement(static_cast<int16_t*>(aSamples->Data()));
  segment.AppendFrames(aSamples.forget(), channelData, aDataLen,
                       PRINCIPAL_HANDLE_NONE);
  mStream->AppendToTrack(1, &segment);
  mStream->AdvanceKnownTracksTime(STREAM_TIME_MAX);
}

NS_IMETHODIMP
nsSpeechTask::DispatchStart()
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchStart() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchStartInner();
}

nsresult
nsSpeechTask::DispatchStartInner()
{
  nsSynthVoiceRegistry::GetInstance()->SetIsSpeaking(true);
  return DispatchStartImpl();
}

nsresult
nsSpeechTask::DispatchStartImpl()
{
  return DispatchStartImpl(mChosenVoiceURI);
}

nsresult
nsSpeechTask::DispatchStartImpl(const nsAString& aUri)
{
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchStart"));

  MOZ_ASSERT(mUtterance);
  if(NS_WARN_IF(!(mUtterance->mState == SpeechSynthesisUtterance::STATE_PENDING))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  CreateAudioChannelAgent();

  mUtterance->mState = SpeechSynthesisUtterance::STATE_SPEAKING;
  mUtterance->mChosenVoiceURI = aUri;
  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("start"), 0,
                                           nullptr, 0, EmptyString());

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchEnd(float aElapsedTime, uint32_t aCharIndex)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchEnd() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchEndInner(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchEndInner(float aElapsedTime, uint32_t aCharIndex)
{
  if (!mPreCanceled) {
    nsSynthVoiceRegistry::GetInstance()->SpeakNext();
  }

  return DispatchEndImpl(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex)
{
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchEnd\n"));

  DestroyAudioChannelAgent();

  MOZ_ASSERT(mUtterance);
  if(NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // XXX: This should not be here, but it prevents a crash in MSG.
  if (mStream) {
    mStream->Destroy();
  }

  RefPtr<SpeechSynthesisUtterance> utterance = mUtterance;

  if (mSpeechSynthesis) {
    mSpeechSynthesis->OnEnd(this);
  }

  if (utterance->mState == SpeechSynthesisUtterance::STATE_PENDING) {
    utterance->mState = SpeechSynthesisUtterance::STATE_NONE;
  } else {
    utterance->mState = SpeechSynthesisUtterance::STATE_ENDED;
    utterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("end"),
                                            aCharIndex, nullptr, aElapsedTime,
                                            EmptyString());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchPause(float aElapsedTime, uint32_t aCharIndex)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchPause() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchPauseImpl(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchPauseImpl(float aElapsedTime, uint32_t aCharIndex)
{
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchPause"));
  MOZ_ASSERT(mUtterance);
  if(NS_WARN_IF(mUtterance->mPaused)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mUtterance->mPaused = true;
  if (mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING) {
    mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("pause"),
                                             aCharIndex, nullptr, aElapsedTime,
                                             EmptyString());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchResume(float aElapsedTime, uint32_t aCharIndex)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchResume() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchResumeImpl(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchResumeImpl(float aElapsedTime, uint32_t aCharIndex)
{
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchResume"));
  MOZ_ASSERT(mUtterance);
  if(NS_WARN_IF(!(mUtterance->mPaused))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if(NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mUtterance->mPaused = false;
  if (mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING) {
    mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("resume"),
                                             aCharIndex, nullptr, aElapsedTime,
                                             EmptyString());
  }

  return NS_OK;
}

void
nsSpeechTask::ForceError(float aElapsedTime, uint32_t aCharIndex)
{
  DispatchErrorInner(aElapsedTime, aCharIndex);
}

NS_IMETHODIMP
nsSpeechTask::DispatchError(float aElapsedTime, uint32_t aCharIndex)
{
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchError"));

  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchError() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchErrorInner(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchErrorInner(float aElapsedTime, uint32_t aCharIndex)
{
  if (!mPreCanceled) {
    nsSynthVoiceRegistry::GetInstance()->SpeakNext();
  }

  return DispatchErrorImpl(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mUtterance);
  if(NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (mSpeechSynthesis) {
    mSpeechSynthesis->OnEnd(this);
  }

  mUtterance->mState = SpeechSynthesisUtterance::STATE_ENDED;
  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("error"),
                                           aCharIndex, nullptr, aElapsedTime,
                                           EmptyString());
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchBoundary(const nsAString& aName,
                               float aElapsedTime, uint32_t aCharIndex,
                               uint32_t aCharLength, uint8_t argc)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchBoundary() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchBoundaryImpl(aName, aElapsedTime, aCharIndex, aCharLength, argc);
}

nsresult
nsSpeechTask::DispatchBoundaryImpl(const nsAString& aName,
                                   float aElapsedTime, uint32_t aCharIndex,
                                   uint32_t aCharLength, uint8_t argc)
{
  MOZ_ASSERT(mUtterance);
  if(NS_WARN_IF(!(mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("boundary"),
                                           aCharIndex,
                                           argc ? static_cast<Nullable<uint32_t> >(aCharLength) : nullptr,
                                           aElapsedTime, aName);

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchMark(const nsAString& aName,
                           float aElapsedTime, uint32_t aCharIndex)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchMark() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchMarkImpl(aName, aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchMarkImpl(const nsAString& aName,
                               float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mUtterance);
  if(NS_WARN_IF(!(mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("mark"),
                                           aCharIndex, nullptr, aElapsedTime,
                                           aName);
  return NS_OK;
}

void
nsSpeechTask::Pause()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnPause();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to call onPause() callback");
  }

  if (mStream) {
    mStream->Suspend();
  }

  if (!mInited) {
    mPrePaused = true;
  }

  if (!mIndirectAudio) {
    DispatchPauseImpl(GetCurrentTime(), GetCurrentCharOffset());
  }
}

void
nsSpeechTask::Resume()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnResume();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Unable to call onResume() callback");
  }

  if (mStream) {
    mStream->Resume();
  }

  if (mPrePaused) {
    mPrePaused = false;
    nsSynthVoiceRegistry::GetInstance()->ResumeQueue();
  }

  if (!mIndirectAudio) {
    DispatchResumeImpl(GetCurrentTime(), GetCurrentCharOffset());
  }
}

void
nsSpeechTask::Cancel()
{
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(LogLevel::Debug, ("nsSpeechTask::Cancel"));

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnCancel();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Unable to call onCancel() callback");
  }

  if (mStream) {
    mStream->Suspend();
  }

  if (!mInited) {
    mPreCanceled = true;
  }

  if (!mIndirectAudio) {
    DispatchEndInner(GetCurrentTime(), GetCurrentCharOffset());
  }
}

void
nsSpeechTask::ForceEnd()
{
  if (mStream) {
    mStream->Suspend();
  }

  if (!mInited) {
    mPreCanceled = true;
  }

  DispatchEndInner(GetCurrentTime(), GetCurrentCharOffset());
}

float
nsSpeechTask::GetCurrentTime()
{
  return mStream ? (float)(mStream->GetCurrentTime() / 1000000.0) : 0;
}

uint32_t
nsSpeechTask::GetCurrentCharOffset()
{
  return mStream && mStream->IsFinished() ? mText.Length() : 0;
}

void
nsSpeechTask::SetSpeechSynthesis(SpeechSynthesis* aSpeechSynthesis)
{
  mSpeechSynthesis = aSpeechSynthesis;
}

void
nsSpeechTask::CreateAudioChannelAgent()
{
  if (!mUtterance) {
    return;
  }

  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
  }

  mAudioChannelAgent = new AudioChannelAgent();
  mAudioChannelAgent->InitWithWeakCallback(mUtterance->GetOwner(),
                                           static_cast<int32_t>(AudioChannelService::GetDefaultAudioChannel()),
                                           this);

  AudioPlaybackConfig config;
  nsresult rv = mAudioChannelAgent->NotifyStartedPlaying(&config,
                                                         AudioChannelService::AudibleState::eAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  WindowVolumeChanged(config.mVolume, config.mMuted);
  WindowSuspendChanged(config.mSuspend);
}

void
nsSpeechTask::DestroyAudioChannelAgent()
{
  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
  }
}

NS_IMETHODIMP
nsSpeechTask::WindowVolumeChanged(float aVolume, bool aMuted)
{
  SetAudioOutputVolume(aMuted ? 0.0 : mVolume * aVolume);
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::WindowSuspendChanged(nsSuspendedTypes aSuspend)
{
  if (!mUtterance) {
    return NS_OK;
  }

  if (aSuspend == nsISuspendedTypes::NONE_SUSPENDED &&
      mUtterance->mPaused) {
    Resume();
  } else if (aSuspend != nsISuspendedTypes::NONE_SUSPENDED &&
             !mUtterance->mPaused) {
    Pause();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::WindowAudioCaptureChanged(bool aCapture)
{
  // This is not supported yet.
  return NS_OK;
}

void
nsSpeechTask::SetAudioOutputVolume(float aVolume)
{
  if (mStream && !mStream->IsDestroyed()) {
    mStream->SetAudioOutputVolume(this, aVolume);
  }
  if (mIndirectAudio && mCallback) {
    mCallback->OnVolumeChanged(aVolume);
  }
}

} // namespace dom
} // namespace mozilla
