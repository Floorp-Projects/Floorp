/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"
#include "nsSpeechTask.h"
#include "SpeechSynthesis.h"

#undef LOG
#ifdef PR_LOGGING
extern PRLogModuleInfo* GetSpeechSynthLog();
#define LOG(type, msg) PR_LOG(GetSpeechSynthLog(), type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {
namespace dom {

class SynthStreamListener : public MediaStreamListener
{
public:
  SynthStreamListener(nsSpeechTask* aSpeechTask) :
    mSpeechTask(aSpeechTask),
    mStarted(false)
  {
  }

  void DoNotifyStarted()
  {
    if (mSpeechTask) {
      mSpeechTask->DispatchStartImpl();
    }
  }

  void DoNotifyFinished()
  {
    if (mSpeechTask) {
      mSpeechTask->DispatchEndImpl(mSpeechTask->GetCurrentTime(),
                                   mSpeechTask->GetCurrentCharOffset());
    }
  }

  virtual void NotifyFinished(MediaStreamGraph* aGraph)
  {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &SynthStreamListener::DoNotifyFinished);
    aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
  }

  virtual void NotifyBlockingChanged(MediaStreamGraph* aGraph, Blocking aBlocked)
  {
    if (aBlocked == MediaStreamListener::UNBLOCKED && !mStarted) {
      mStarted = true;
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableMethod(this, &SynthStreamListener::DoNotifyStarted);
      aGraph->DispatchToMainThreadAfterStreamStateUpdate(event.forget());
    }
  }

  virtual void NotifyRemoved(MediaStreamGraph* aGraph)
  {
    mSpeechTask = nullptr;
  }

private:
  // Raw pointer; if we exist, the stream exists,
  // and 'mSpeechTask' exclusively owns it and therefor exists as well.
  nsSpeechTask* mSpeechTask;

  bool mStarted;
};

// nsSpeechTask

NS_IMPL_CYCLE_COLLECTION_2(nsSpeechTask, mSpeechSynthesis, mUtterance);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSpeechTask)
  NS_INTERFACE_MAP_ENTRY(nsISpeechTask)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechTask)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSpeechTask)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSpeechTask)

nsSpeechTask::nsSpeechTask(SpeechSynthesisUtterance* aUtterance)
  : mUtterance(aUtterance)
  , mCallback(nullptr)
  , mIndirectAudio(false)
{
  mText = aUtterance->mText;
  mVolume = aUtterance->Volume();
}

nsSpeechTask::nsSpeechTask(float aVolume, const nsAString& aText)
  : mUtterance(nullptr)
  , mVolume(aVolume)
  , mText(aText)
  , mCallback(nullptr)
  , mIndirectAudio(false)
{
}

nsSpeechTask::~nsSpeechTask()
{
  if (mStream) {
    if (!mStream->IsDestroyed()) {
      mStream->Destroy();
    }

    mStream = nullptr;
  }
}

NS_IMETHODIMP
nsSpeechTask::Setup(nsISpeechTaskCallback* aCallback,
                    uint32_t aChannels, uint32_t aRate, uint8_t argc)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  LOG(PR_LOG_DEBUG, ("nsSpeechTask::Setup"));

  mCallback = aCallback;

  if (argc < 2) {
    return NS_OK;
  }

  if (mIndirectAudio) {
    NS_WARNING("Audio info arguments in Setup() are ignored for indirect audio services.");
  }

  // XXX: Is there setup overhead here that hurtls latency?
  mStream = MediaStreamGraph::GetInstance()->CreateSourceStream(nullptr);
  mStream->AddListener(new SynthStreamListener(this));

  // XXX: Support more than one channel
  NS_ENSURE_TRUE(aChannels == 1, NS_ERROR_FAILURE);

  mChannels = aChannels;

  AudioSegment* segment = new AudioSegment();
  mStream->AddTrack(1, aRate, 0, segment);
  mStream->AddAudioOutput(this);
  mStream->SetAudioOutputVolume(this, mVolume);

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::SendAudio(const JS::Value& aData, const JS::Value& aLandmarks,
                        JSContext* aCx)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  NS_ENSURE_TRUE(mStream, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_FALSE(mStream->IsDestroyed(), NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mChannels, NS_ERROR_FAILURE);

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
  } else if (JS_IsArrayObject(aCx, darray)) {
    tsrc = JS_NewInt16ArrayFromArray(aCx, darray);
  }

  if (!tsrc) {
    return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
  }

  SendAudioImpl(JS_GetInt16ArrayData(tsrc),
                JS_GetTypedArrayLength(tsrc));

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::SendAudioNative(int16_t* aData, uint32_t aDataLen)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  NS_ENSURE_TRUE(mStream, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_FALSE(mStream->IsDestroyed(), NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_TRUE(mChannels, NS_ERROR_FAILURE);

  if (mIndirectAudio) {
    NS_WARNING("Can't call SendAudio from an indirect audio speech service.");
    return NS_ERROR_FAILURE;
  }

  SendAudioImpl(aData, aDataLen);

  return NS_OK;
}

void
nsSpeechTask::SendAudioImpl(int16_t* aData, uint32_t aDataLen)
{
  if (aDataLen == 0) {
    // XXX: We should end the track too, an undetermined bug does not allow that.
    mStream->Finish();
    return;
  }

  nsRefPtr<mozilla::SharedBuffer> samples =
    SharedBuffer::Create(aDataLen * sizeof(int16_t));
  int16_t* frames = static_cast<int16_t*>(samples->Data());

  for (uint32_t i = 0; i < aDataLen; i++) {
    frames[i] = aData[i];
  }

  AudioSegment segment;
  nsAutoTArray<const int16_t*, 1> channelData;
  channelData.AppendElement(frames);
  segment.AppendFrames(samples.forget(), channelData, aDataLen);
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

  return DispatchStartImpl();
}

nsresult
nsSpeechTask::DispatchStartImpl()
{
  LOG(PR_LOG_DEBUG, ("nsSpeechTask::DispatchStart"));

  MOZ_ASSERT(mUtterance);
  NS_ENSURE_TRUE(mUtterance->mState == SpeechSynthesisUtterance::STATE_PENDING,
                 NS_ERROR_NOT_AVAILABLE);

  mUtterance->mState = SpeechSynthesisUtterance::STATE_SPEAKING;
  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("start"), 0, 0,
                                           NS_LITERAL_STRING(""));

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchEnd(float aElapsedTime, uint32_t aCharIndex)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchEnd() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchEndImpl(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex)
{
  LOG(PR_LOG_DEBUG, ("nsSpeechTask::DispatchEnd\n"));

  MOZ_ASSERT(mUtterance);
  NS_ENSURE_FALSE(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED,
                  NS_ERROR_NOT_AVAILABLE);

  // XXX: This should not be here, but it prevents a crash in MSG.
  if (mStream) {
    mStream->Destroy();
  }

  nsRefPtr<SpeechSynthesisUtterance> utterance = mUtterance;

  if (mSpeechSynthesis) {
    mSpeechSynthesis->OnEnd(this);
  }

  if (utterance->mState == SpeechSynthesisUtterance::STATE_PENDING) {
    utterance->mState = SpeechSynthesisUtterance::STATE_NONE;
  } else {
    utterance->mState = SpeechSynthesisUtterance::STATE_ENDED;
    utterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("end"),
                                            aCharIndex, aElapsedTime,
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
  LOG(PR_LOG_DEBUG, ("nsSpeechTask::DispatchPause"));
  MOZ_ASSERT(mUtterance);
  NS_ENSURE_FALSE(mUtterance->mPaused, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_FALSE(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED,
                  NS_ERROR_NOT_AVAILABLE);

  mUtterance->mPaused = true;
  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("pause"),
                                           aCharIndex, aElapsedTime,
                                           NS_LITERAL_STRING(""));
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
  LOG(PR_LOG_DEBUG, ("nsSpeechTask::DispatchResume"));
  MOZ_ASSERT(mUtterance);
  NS_ENSURE_TRUE(mUtterance->mPaused, NS_ERROR_NOT_AVAILABLE);
  NS_ENSURE_FALSE(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED,
                  NS_ERROR_NOT_AVAILABLE);

  mUtterance->mPaused = false;
  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("resume"),
                                           aCharIndex, aElapsedTime,
                                           NS_LITERAL_STRING(""));
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchError(float aElapsedTime, uint32_t aCharIndex)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchError() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchErrorImpl(aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mUtterance);
  NS_ENSURE_FALSE(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED,
                  NS_ERROR_NOT_AVAILABLE);

  mUtterance->mState = SpeechSynthesisUtterance::STATE_ENDED;
  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("error"),
                                           aCharIndex, aElapsedTime,
                                           NS_LITERAL_STRING(""));
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchBoundary(const nsAString& aName,
                               float aElapsedTime, uint32_t aCharIndex)
{
  if (!mIndirectAudio) {
    NS_WARNING("Can't call DispatchBoundary() from a direct audio speech service");
    return NS_ERROR_FAILURE;
  }

  return DispatchBoundaryImpl(aName, aElapsedTime, aCharIndex);
}

nsresult
nsSpeechTask::DispatchBoundaryImpl(const nsAString& aName,
                                   float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mUtterance);
  NS_ENSURE_TRUE(mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING,
                 NS_ERROR_NOT_AVAILABLE);

  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("boundary"),
                                           aCharIndex, aElapsedTime,
                                           aName);
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
  NS_ENSURE_TRUE(mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING,
                 NS_ERROR_NOT_AVAILABLE);

  mUtterance->DispatchSpeechSynthesisEvent(NS_LITERAL_STRING("mark"),
                                           aCharIndex, aElapsedTime,
                                           aName);
  return NS_OK;
}

void
nsSpeechTask::Pause()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  if (mUtterance->IsPaused() ||
      mUtterance->GetState() == SpeechSynthesisUtterance::STATE_ENDED) {
    return;
  }

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnPause();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to call onPause() callback");
  }

  if (mStream) {
    mStream->ChangeExplicitBlockerCount(1);
  }

  DispatchPauseImpl(GetCurrentTime(), GetCurrentCharOffset());
}

void
nsSpeechTask::Resume()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  if (!mUtterance->IsPaused()) {
    return;
  }

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnResume();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to call onResume() callback");
  }

  if (mStream) {
    mStream->ChangeExplicitBlockerCount(-1);
  }

  DispatchResumeImpl(GetCurrentTime(), GetCurrentCharOffset());
}

void
nsSpeechTask::Cancel()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);

  LOG(PR_LOG_DEBUG, ("nsSpeechTask::Cancel"));

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnCancel();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Unable to call onCancel() callback");
  }

  if (mStream) {
    mStream->ChangeExplicitBlockerCount(1);
  }

  DispatchEndImpl(GetCurrentTime(), GetCurrentCharOffset());
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

} // namespace dom
} // namespace mozilla
