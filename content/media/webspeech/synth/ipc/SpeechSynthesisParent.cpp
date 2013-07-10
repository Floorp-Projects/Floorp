/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechSynthesisParent.h"
#include "nsSynthVoiceRegistry.h"

namespace mozilla {
namespace dom {

SpeechSynthesisParent::SpeechSynthesisParent()
{
  MOZ_COUNT_CTOR(SpeechSynthesisParent);
}

SpeechSynthesisParent::~SpeechSynthesisParent()
{
  MOZ_COUNT_DTOR(SpeechSynthesisParent);
}

bool
SpeechSynthesisParent::RecvReadVoiceList(InfallibleTArray<RemoteVoice>* aVoices,
                                         InfallibleTArray<nsString>* aDefaults)
{
  nsSynthVoiceRegistry::GetInstance()->SendVoices(aVoices, aDefaults);
  return true;
}

PSpeechSynthesisRequestParent*
SpeechSynthesisParent::AllocPSpeechSynthesisRequestParent(const nsString& aText,
                                                          const nsString& aLang,
                                                          const nsString& aUri,
                                                          const float& aVolume,
                                                          const float& aRate,
                                                          const float& aPitch)
{
  nsRefPtr<SpeechTaskParent> task = new SpeechTaskParent(aVolume, aText);
  SpeechSynthesisRequestParent* actor = new SpeechSynthesisRequestParent(task);
  return actor;
}

bool
SpeechSynthesisParent::DeallocPSpeechSynthesisRequestParent(PSpeechSynthesisRequestParent* aActor)
{
  delete aActor;
  return true;
}

bool
SpeechSynthesisParent::RecvPSpeechSynthesisRequestConstructor(PSpeechSynthesisRequestParent* aActor,
                                                              const nsString& aText,
                                                              const nsString& aLang,
                                                              const nsString& aUri,
                                                              const float& aVolume,
                                                              const float& aRate,
                                                              const float& aPitch)
{
  MOZ_ASSERT(aActor);
  SpeechSynthesisRequestParent* actor =
    static_cast<SpeechSynthesisRequestParent*>(aActor);
  nsSynthVoiceRegistry::GetInstance()->Speak(aText, aLang, aUri, aRate,
                                             aPitch, actor->mTask);
  return true;
}

// SpeechSynthesisRequestParent

SpeechSynthesisRequestParent::SpeechSynthesisRequestParent(SpeechTaskParent* aTask)
  : mTask(aTask)
{
  mTask->mActor = this;
  MOZ_COUNT_CTOR(SpeechSynthesisRequestParent);
}

SpeechSynthesisRequestParent::~SpeechSynthesisRequestParent()
{
  if (mTask && mTask->mActor) {
    mTask->mActor = nullptr;
  }

  MOZ_COUNT_DTOR(SpeechSynthesisRequestParent);
}

bool
SpeechSynthesisRequestParent::RecvPause()
{
  MOZ_ASSERT(mTask);
  mTask->Pause();
  return true;
}

bool
SpeechSynthesisRequestParent::RecvResume()
{
  MOZ_ASSERT(mTask);
  mTask->Resume();
  return true;
}

bool
SpeechSynthesisRequestParent::RecvCancel()
{
  MOZ_ASSERT(mTask);
  mTask->Cancel();
  return true;
}

// SpeechTaskParent

nsresult
SpeechTaskParent::DispatchStartImpl()
{
  MOZ_ASSERT(mActor);
  NS_ENSURE_TRUE(mActor->SendOnStart(), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
SpeechTaskParent::DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mActor);
  SpeechSynthesisRequestParent* actor = mActor;
  mActor = nullptr;
  NS_ENSURE_TRUE(actor->Send__delete__(actor, false, aElapsedTime, aCharIndex),
                 NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
SpeechTaskParent::DispatchPauseImpl(float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mActor);
  NS_ENSURE_TRUE(mActor->SendOnPause(aElapsedTime, aCharIndex), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
SpeechTaskParent::DispatchResumeImpl(float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mActor);
  NS_ENSURE_TRUE(mActor->SendOnResume(aElapsedTime, aCharIndex), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
SpeechTaskParent::DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mActor);
  SpeechSynthesisRequestParent* actor = mActor;
  mActor = nullptr;
  NS_ENSURE_TRUE(actor->Send__delete__(actor, true, aElapsedTime, aCharIndex),
                 NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
SpeechTaskParent::DispatchBoundaryImpl(const nsAString& aName,
                                       float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mActor);
  NS_ENSURE_TRUE(mActor->SendOnBoundary(nsString(aName), aElapsedTime, aCharIndex),
                 NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult
SpeechTaskParent::DispatchMarkImpl(const nsAString& aName,
                                   float aElapsedTime, uint32_t aCharIndex)
{
  MOZ_ASSERT(mActor);
  NS_ENSURE_TRUE(mActor->SendOnMark(nsString(aName), aElapsedTime, aCharIndex),
                 NS_ERROR_FAILURE);

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
