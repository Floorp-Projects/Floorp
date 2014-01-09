/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechSynthesisChild.h"
#include "nsSynthVoiceRegistry.h"

namespace mozilla {
namespace dom {

SpeechSynthesisChild::SpeechSynthesisChild()
{
  MOZ_COUNT_CTOR(SpeechSynthesisChild);
}

SpeechSynthesisChild::~SpeechSynthesisChild()
{
  MOZ_COUNT_DTOR(SpeechSynthesisChild);
}

bool
SpeechSynthesisChild::RecvVoiceAdded(const RemoteVoice& aVoice)
{
  nsSynthVoiceRegistry::RecvAddVoice(aVoice);
  return true;
}

bool
SpeechSynthesisChild::RecvVoiceRemoved(const nsString& aUri)
{
  nsSynthVoiceRegistry::RecvRemoveVoice(aUri);
  return true;
}

bool
SpeechSynthesisChild::RecvSetDefaultVoice(const nsString& aUri,
                                          const bool& aIsDefault)
{
  nsSynthVoiceRegistry::RecvSetDefaultVoice(aUri, aIsDefault);
  return true;
}

PSpeechSynthesisRequestChild*
SpeechSynthesisChild::AllocPSpeechSynthesisRequestChild(const nsString& aText,
                                                        const nsString& aLang,
                                                        const nsString& aUri,
                                                        const float& aVolume,
                                                        const float& aRate,
                                                        const float& aPitch)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
SpeechSynthesisChild::DeallocPSpeechSynthesisRequestChild(PSpeechSynthesisRequestChild* aActor)
{
  delete aActor;
  return true;
}

// SpeechSynthesisRequestChild

SpeechSynthesisRequestChild::SpeechSynthesisRequestChild(SpeechTaskChild* aTask)
  : mTask(aTask)
{
  mTask->mActor = this;
  MOZ_COUNT_CTOR(SpeechSynthesisRequestChild);
}

SpeechSynthesisRequestChild::~SpeechSynthesisRequestChild()
{
  MOZ_COUNT_DTOR(SpeechSynthesisRequestChild);
}

bool
SpeechSynthesisRequestChild::RecvOnStart()
{
  mTask->DispatchStartImpl();
  return true;
}

bool
SpeechSynthesisRequestChild::Recv__delete__(const bool& aIsError,
                                            const float& aElapsedTime,
                                            const uint32_t& aCharIndex)
{
  mTask->mActor = nullptr;

  if (aIsError) {
    mTask->DispatchErrorImpl(aElapsedTime, aCharIndex);
  } else {
    mTask->DispatchEndImpl(aElapsedTime, aCharIndex);
  }

  return true;
}

bool
SpeechSynthesisRequestChild::RecvOnPause(const float& aElapsedTime,
                                         const uint32_t& aCharIndex)
{
  mTask->DispatchPauseImpl(aElapsedTime, aCharIndex);
  return true;
}

bool
SpeechSynthesisRequestChild::RecvOnResume(const float& aElapsedTime,
                                          const uint32_t& aCharIndex)
{
  mTask->DispatchResumeImpl(aElapsedTime, aCharIndex);
  return true;
}

bool
SpeechSynthesisRequestChild::RecvOnError(const float& aElapsedTime,
                                         const uint32_t& aCharIndex)
{
  mTask->DispatchErrorImpl(aElapsedTime, aCharIndex);
  return true;
}

bool
SpeechSynthesisRequestChild::RecvOnBoundary(const nsString& aName,
                                            const float& aElapsedTime,
                                            const uint32_t& aCharIndex)
{
  mTask->DispatchBoundaryImpl(aName, aElapsedTime, aCharIndex);
  return true;
}

bool
SpeechSynthesisRequestChild::RecvOnMark(const nsString& aName,
                                        const float& aElapsedTime,
                                        const uint32_t& aCharIndex)
{
  mTask->DispatchMarkImpl(aName, aElapsedTime, aCharIndex);
  return true;
}

// SpeechTaskChild

SpeechTaskChild::SpeechTaskChild(SpeechSynthesisUtterance* aUtterance)
  : nsSpeechTask(aUtterance)
{
}

NS_IMETHODIMP
SpeechTaskChild::Setup(nsISpeechTaskCallback* aCallback,
                       uint32_t aChannels, uint32_t aRate, uint8_t argc)
{
  MOZ_CRASH("Should never be called from child");
}

NS_IMETHODIMP
SpeechTaskChild::SendAudio(JS::Handle<JS::Value> aData, JS::Handle<JS::Value> aLandmarks,
                           JSContext* aCx)
{
  MOZ_CRASH("Should never be called from child");
}

NS_IMETHODIMP
SpeechTaskChild::SendAudioNative(int16_t* aData, uint32_t aDataLen)
{
  MOZ_CRASH("Should never be called from child");
}

void
SpeechTaskChild::Pause()
{
  MOZ_ASSERT(mActor);
  mActor->SendPause();
}

void
SpeechTaskChild::Resume()
{
  MOZ_ASSERT(mActor);
  mActor->SendResume();
}

void
SpeechTaskChild::Cancel()
{
  MOZ_ASSERT(mActor);
  mActor->SendCancel();
}

} // namespace dom
} // namespace mozilla
