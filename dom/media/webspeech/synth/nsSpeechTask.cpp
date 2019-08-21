/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelAgent.h"
#include "AudioChannelService.h"
#include "AudioSegment.h"
#include "nsSpeechTask.h"
#include "nsSynthVoiceRegistry.h"
#include "SharedBuffer.h"
#include "SpeechSynthesis.h"

#undef LOG
extern mozilla::LogModule* GetSpeechSynthLog();
#define LOG(type, msg) MOZ_LOG(GetSpeechSynthLog(), type, msg)

#define AUDIO_TRACK 1

namespace mozilla {
namespace dom {

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
    : mUtterance(aUtterance),
      mInited(false),
      mPrePaused(false),
      mPreCanceled(false),
      mCallback(nullptr),
      mIsChrome(aIsChrome) {
  mText = aUtterance->mText;
  mVolume = aUtterance->Volume();
}

nsSpeechTask::nsSpeechTask(float aVolume, const nsAString& aText,
                           bool aIsChrome)
    : mUtterance(nullptr),
      mVolume(aVolume),
      mText(aText),
      mInited(false),
      mPrePaused(false),
      mPreCanceled(false),
      mCallback(nullptr),
      mIsChrome(aIsChrome) {}

nsSpeechTask::~nsSpeechTask() { LOG(LogLevel::Debug, ("~nsSpeechTask")); }

void nsSpeechTask::Init() { mInited = true; }

void nsSpeechTask::SetChosenVoiceURI(const nsAString& aUri) {
  mChosenVoiceURI = aUri;
}

NS_IMETHODIMP
nsSpeechTask::Setup(nsISpeechTaskCallback* aCallback) {
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(LogLevel::Debug, ("nsSpeechTask::Setup"));

  mCallback = aCallback;

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchStart() {
  nsSynthVoiceRegistry::GetInstance()->SetIsSpeaking(true);
  return DispatchStartImpl();
}

nsresult nsSpeechTask::DispatchStartImpl() {
  return DispatchStartImpl(mChosenVoiceURI);
}

nsresult nsSpeechTask::DispatchStartImpl(const nsAString& aUri) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchStartImpl"));

  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(
          !(mUtterance->mState == SpeechSynthesisUtterance::STATE_PENDING))) {
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
nsSpeechTask::DispatchEnd(float aElapsedTime, uint32_t aCharIndex) {
  // After we end, no callback functions should go through.
  mCallback = nullptr;

  if (!mPreCanceled) {
    nsSynthVoiceRegistry::GetInstance()->SpeakNext();
  }

  return DispatchEndImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchEndImpl(float aElapsedTime,
                                       uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchEndImpl"));

  DestroyAudioChannelAgent();

  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
    return NS_ERROR_NOT_AVAILABLE;
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
nsSpeechTask::DispatchPause(float aElapsedTime, uint32_t aCharIndex) {
  return DispatchPauseImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchPauseImpl(float aElapsedTime,
                                         uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchPauseImpl"));
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mUtterance->mPaused)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
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
nsSpeechTask::DispatchResume(float aElapsedTime, uint32_t aCharIndex) {
  return DispatchResumeImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchResumeImpl(float aElapsedTime,
                                          uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchResumeImpl"));
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(!(mUtterance->mPaused))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
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

void nsSpeechTask::ForceError(float aElapsedTime, uint32_t aCharIndex) {
  DispatchError(aElapsedTime, aCharIndex);
}

NS_IMETHODIMP
nsSpeechTask::DispatchError(float aElapsedTime, uint32_t aCharIndex) {
  if (!mPreCanceled) {
    nsSynthVoiceRegistry::GetInstance()->SpeakNext();
  }

  return DispatchErrorImpl(aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchErrorImpl(float aElapsedTime,
                                         uint32_t aCharIndex) {
  LOG(LogLevel::Debug, ("nsSpeechTask::DispatchErrorImpl"));

  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(mUtterance->mState == SpeechSynthesisUtterance::STATE_ENDED)) {
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
nsSpeechTask::DispatchBoundary(const nsAString& aName, float aElapsedTime,
                               uint32_t aCharIndex, uint32_t aCharLength,
                               uint8_t argc) {
  return DispatchBoundaryImpl(aName, aElapsedTime, aCharIndex, aCharLength,
                              argc);
}

nsresult nsSpeechTask::DispatchBoundaryImpl(const nsAString& aName,
                                            float aElapsedTime,
                                            uint32_t aCharIndex,
                                            uint32_t aCharLength,
                                            uint8_t argc) {
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(
          !(mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mUtterance->DispatchSpeechSynthesisEvent(
      NS_LITERAL_STRING("boundary"), aCharIndex,
      argc ? static_cast<Nullable<uint32_t> >(aCharLength) : nullptr,
      aElapsedTime, aName);

  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::DispatchMark(const nsAString& aName, float aElapsedTime,
                           uint32_t aCharIndex) {
  return DispatchMarkImpl(aName, aElapsedTime, aCharIndex);
}

nsresult nsSpeechTask::DispatchMarkImpl(const nsAString& aName,
                                        float aElapsedTime,
                                        uint32_t aCharIndex) {
  MOZ_ASSERT(mUtterance);
  if (NS_WARN_IF(
          !(mUtterance->mState == SpeechSynthesisUtterance::STATE_SPEAKING))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mUtterance->DispatchSpeechSynthesisEvent(
      NS_LITERAL_STRING("mark"), aCharIndex, nullptr, aElapsedTime, aName);
  return NS_OK;
}

void nsSpeechTask::Pause() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnPause();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Unable to call onPause() callback");
  }

  if (!mInited) {
    mPrePaused = true;
  }
}

void nsSpeechTask::Resume() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnResume();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Unable to call onResume() callback");
  }

  if (mPrePaused) {
    mPrePaused = false;
    nsSynthVoiceRegistry::GetInstance()->ResumeQueue();
  }
}

void nsSpeechTask::Cancel() {
  MOZ_ASSERT(XRE_IsParentProcess());

  LOG(LogLevel::Debug, ("nsSpeechTask::Cancel"));

  if (mCallback) {
    DebugOnly<nsresult> rv = mCallback->OnCancel();
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                         "Unable to call onCancel() callback");
  }

  if (!mInited) {
    mPreCanceled = true;
  }
}

void nsSpeechTask::ForceEnd() {
  if (!mInited) {
    mPreCanceled = true;
  }

  DispatchEnd(0, 0);
}

void nsSpeechTask::SetSpeechSynthesis(SpeechSynthesis* aSpeechSynthesis) {
  mSpeechSynthesis = aSpeechSynthesis;
}

void nsSpeechTask::CreateAudioChannelAgent() {
  if (!mUtterance) {
    return;
  }

  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
  }

  mAudioChannelAgent = new AudioChannelAgent();
  mAudioChannelAgent->InitWithWeakCallback(mUtterance->GetOwner(), this);

  AudioPlaybackConfig config;
  nsresult rv = mAudioChannelAgent->NotifyStartedPlaying(
      &config, AudioChannelService::AudibleState::eAudible);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  WindowVolumeChanged(config.mVolume, config.mMuted);
  WindowSuspendChanged(config.mSuspend);
}

void nsSpeechTask::DestroyAudioChannelAgent() {
  if (mAudioChannelAgent) {
    mAudioChannelAgent->NotifyStoppedPlaying();
    mAudioChannelAgent = nullptr;
  }
}

NS_IMETHODIMP
nsSpeechTask::WindowVolumeChanged(float aVolume, bool aMuted) {
  SetAudioOutputVolume(aMuted ? 0.0 : mVolume * aVolume);
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::WindowSuspendChanged(nsSuspendedTypes aSuspend) {
  if (!mUtterance) {
    return NS_OK;
  }

  if (aSuspend == nsISuspendedTypes::NONE_SUSPENDED && mUtterance->mPaused) {
    Resume();
  } else if (aSuspend != nsISuspendedTypes::NONE_SUSPENDED &&
             !mUtterance->mPaused) {
    Pause();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSpeechTask::WindowAudioCaptureChanged(bool aCapture) {
  // This is not supported yet.
  return NS_OK;
}

void nsSpeechTask::SetAudioOutputVolume(float aVolume) {
  if (mCallback) {
    mCallback->OnVolumeChanged(aVolume);
  }
}

}  // namespace dom
}  // namespace mozilla
