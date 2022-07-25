/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechSynthesisParent.h"
#include "nsSynthVoiceRegistry.h"

namespace mozilla::dom {

SpeechSynthesisParent::SpeechSynthesisParent() {
  MOZ_COUNT_CTOR(SpeechSynthesisParent);
}

SpeechSynthesisParent::~SpeechSynthesisParent() {
  MOZ_COUNT_DTOR(SpeechSynthesisParent);
}

void SpeechSynthesisParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Implement me! Bug 1005141
}

bool SpeechSynthesisParent::SendInit() {
  return nsSynthVoiceRegistry::GetInstance()->SendInitialVoicesAndState(this);
}

PSpeechSynthesisRequestParent*
SpeechSynthesisParent::AllocPSpeechSynthesisRequestParent(
    const nsAString& aText, const nsAString& aLang, const nsAString& aUri,
    const float& aVolume, const float& aRate, const float& aPitch,
    const bool& aIsChrome) {
  RefPtr<SpeechTaskParent> task =
      new SpeechTaskParent(aVolume, aText, aIsChrome);
  SpeechSynthesisRequestParent* actor = new SpeechSynthesisRequestParent(task);
  return actor;
}

bool SpeechSynthesisParent::DeallocPSpeechSynthesisRequestParent(
    PSpeechSynthesisRequestParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult
SpeechSynthesisParent::RecvPSpeechSynthesisRequestConstructor(
    PSpeechSynthesisRequestParent* aActor, const nsAString& aText,
    const nsAString& aLang, const nsAString& aUri, const float& aVolume,
    const float& aRate, const float& aPitch, const bool& aIsChrome) {
  MOZ_ASSERT(aActor);
  SpeechSynthesisRequestParent* actor =
      static_cast<SpeechSynthesisRequestParent*>(aActor);
  nsSynthVoiceRegistry::GetInstance()->Speak(aText, aLang, aUri, aVolume, aRate,
                                             aPitch, actor->mTask);
  return IPC_OK();
}

// SpeechSynthesisRequestParent

SpeechSynthesisRequestParent::SpeechSynthesisRequestParent(
    SpeechTaskParent* aTask)
    : mTask(aTask) {
  mTask->mActor = this;
  MOZ_COUNT_CTOR(SpeechSynthesisRequestParent);
}

SpeechSynthesisRequestParent::~SpeechSynthesisRequestParent() {
  if (mTask) {
    mTask->mActor = nullptr;
    // If we still have a task, cancel it.
    mTask->Cancel();
  }
  MOZ_COUNT_DTOR(SpeechSynthesisRequestParent);
}

void SpeechSynthesisRequestParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Implement me! Bug 1005141
}

mozilla::ipc::IPCResult SpeechSynthesisRequestParent::RecvPause() {
  MOZ_ASSERT(mTask);
  mTask->Pause();
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestParent::Recv__delete__() {
  MOZ_ASSERT(mTask);
  mTask->mActor = nullptr;
  mTask = nullptr;
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestParent::RecvResume() {
  MOZ_ASSERT(mTask);
  mTask->Resume();
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestParent::RecvCancel() {
  MOZ_ASSERT(mTask);
  mTask->Cancel();
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestParent::RecvForceEnd() {
  MOZ_ASSERT(mTask);
  mTask->ForceEnd();
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestParent::RecvSetAudioOutputVolume(
    const float& aVolume) {
  MOZ_ASSERT(mTask);
  mTask->SetAudioOutputVolume(aVolume);
  return IPC_OK();
}

// SpeechTaskParent

nsresult SpeechTaskParent::DispatchStartImpl(const nsAString& aUri) {
  if (!mActor) {
    // Child is already gone.
    return NS_OK;
  }

  if (NS_WARN_IF(!(mActor->SendOnStart(aUri)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SpeechTaskParent::DispatchEndImpl(float aElapsedTime,
                                           uint32_t aCharIndex) {
  if (!mActor) {
    // Child is already gone.
    return NS_OK;
  }

  if (NS_WARN_IF(!(mActor->SendOnEnd(false, aElapsedTime, aCharIndex)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SpeechTaskParent::DispatchPauseImpl(float aElapsedTime,
                                             uint32_t aCharIndex) {
  if (!mActor) {
    // Child is already gone.
    return NS_OK;
  }

  if (NS_WARN_IF(!(mActor->SendOnPause(aElapsedTime, aCharIndex)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SpeechTaskParent::DispatchResumeImpl(float aElapsedTime,
                                              uint32_t aCharIndex) {
  if (!mActor) {
    // Child is already gone.
    return NS_OK;
  }

  if (NS_WARN_IF(!(mActor->SendOnResume(aElapsedTime, aCharIndex)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SpeechTaskParent::DispatchErrorImpl(float aElapsedTime,
                                             uint32_t aCharIndex) {
  if (!mActor) {
    // Child is already gone.
    return NS_OK;
  }

  if (NS_WARN_IF(!(mActor->SendOnEnd(true, aElapsedTime, aCharIndex)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SpeechTaskParent::DispatchBoundaryImpl(const nsAString& aName,
                                                float aElapsedTime,
                                                uint32_t aCharIndex,
                                                uint32_t aCharLength,
                                                uint8_t argc) {
  if (!mActor) {
    // Child is already gone.
    return NS_OK;
  }

  if (NS_WARN_IF(!(mActor->SendOnBoundary(aName, aElapsedTime, aCharIndex,
                                          aCharLength, argc)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult SpeechTaskParent::DispatchMarkImpl(const nsAString& aName,
                                            float aElapsedTime,
                                            uint32_t aCharIndex) {
  if (!mActor) {
    // Child is already gone.
    return NS_OK;
  }

  if (NS_WARN_IF(!(mActor->SendOnMark(aName, aElapsedTime, aCharIndex)))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla::dom
