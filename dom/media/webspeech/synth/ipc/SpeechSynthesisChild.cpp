/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechSynthesisChild.h"
#include "nsSynthVoiceRegistry.h"

namespace mozilla::dom {

SpeechSynthesisChild::SpeechSynthesisChild() {
  MOZ_COUNT_CTOR(SpeechSynthesisChild);
}

SpeechSynthesisChild::~SpeechSynthesisChild() {
  MOZ_COUNT_DTOR(SpeechSynthesisChild);
}

mozilla::ipc::IPCResult SpeechSynthesisChild::RecvInitialVoicesAndState(
    nsTArray<RemoteVoice>&& aVoices, nsTArray<nsString>&& aDefaults,
    const bool& aIsSpeaking) {
  nsSynthVoiceRegistry::RecvInitialVoicesAndState(aVoices, aDefaults,
                                                  aIsSpeaking);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisChild::RecvVoiceAdded(
    const RemoteVoice& aVoice) {
  nsSynthVoiceRegistry::RecvAddVoice(aVoice);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisChild::RecvVoiceRemoved(
    const nsString& aUri) {
  nsSynthVoiceRegistry::RecvRemoveVoice(aUri);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisChild::RecvSetDefaultVoice(
    const nsString& aUri, const bool& aIsDefault) {
  nsSynthVoiceRegistry::RecvSetDefaultVoice(aUri, aIsDefault);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisChild::RecvIsSpeakingChanged(
    const bool& aIsSpeaking) {
  nsSynthVoiceRegistry::RecvIsSpeakingChanged(aIsSpeaking);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisChild::RecvNotifyVoicesChanged() {
  nsSynthVoiceRegistry::RecvNotifyVoicesChanged();
  return IPC_OK();
}

PSpeechSynthesisRequestChild*
SpeechSynthesisChild::AllocPSpeechSynthesisRequestChild(
    const nsString& aText, const nsString& aLang, const nsString& aUri,
    const float& aVolume, const float& aRate, const float& aPitch,
    const bool& aIsChrome) {
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool SpeechSynthesisChild::DeallocPSpeechSynthesisRequestChild(
    PSpeechSynthesisRequestChild* aActor) {
  delete aActor;
  return true;
}

// SpeechSynthesisRequestChild

SpeechSynthesisRequestChild::SpeechSynthesisRequestChild(SpeechTaskChild* aTask)
    : mTask(aTask) {
  mTask->mActor = this;
  MOZ_COUNT_CTOR(SpeechSynthesisRequestChild);
}

SpeechSynthesisRequestChild::~SpeechSynthesisRequestChild() {
  MOZ_COUNT_DTOR(SpeechSynthesisRequestChild);
}

mozilla::ipc::IPCResult SpeechSynthesisRequestChild::RecvOnStart(
    const nsString& aUri) {
  mTask->DispatchStartImpl(aUri);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestChild::RecvOnEnd(
    const bool& aIsError, const float& aElapsedTime,
    const uint32_t& aCharIndex) {
  SpeechSynthesisRequestChild* actor = mTask->mActor;
  mTask->mActor = nullptr;

  if (aIsError) {
    mTask->DispatchErrorImpl(aElapsedTime, aCharIndex);
  } else {
    mTask->DispatchEndImpl(aElapsedTime, aCharIndex);
  }

  SpeechSynthesisRequestChild::Send__delete__(actor);

  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestChild::RecvOnPause(
    const float& aElapsedTime, const uint32_t& aCharIndex) {
  mTask->DispatchPauseImpl(aElapsedTime, aCharIndex);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestChild::RecvOnResume(
    const float& aElapsedTime, const uint32_t& aCharIndex) {
  mTask->DispatchResumeImpl(aElapsedTime, aCharIndex);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestChild::RecvOnBoundary(
    const nsString& aName, const float& aElapsedTime,
    const uint32_t& aCharIndex, const uint32_t& aCharLength,
    const uint8_t& argc) {
  mTask->DispatchBoundaryImpl(aName, aElapsedTime, aCharIndex, aCharLength,
                              argc);
  return IPC_OK();
}

mozilla::ipc::IPCResult SpeechSynthesisRequestChild::RecvOnMark(
    const nsString& aName, const float& aElapsedTime,
    const uint32_t& aCharIndex) {
  mTask->DispatchMarkImpl(aName, aElapsedTime, aCharIndex);
  return IPC_OK();
}

// SpeechTaskChild

SpeechTaskChild::SpeechTaskChild(SpeechSynthesisUtterance* aUtterance,
                                 bool aIsChrome)
    : nsSpeechTask(aUtterance, aIsChrome), mActor(nullptr) {}

NS_IMETHODIMP
SpeechTaskChild::Setup(nsISpeechTaskCallback* aCallback) {
  MOZ_CRASH("Should never be called from child");
}

void SpeechTaskChild::Pause() {
  MOZ_ASSERT(mActor);
  mActor->SendPause();
}

void SpeechTaskChild::Resume() {
  MOZ_ASSERT(mActor);
  mActor->SendResume();
}

void SpeechTaskChild::Cancel() {
  MOZ_ASSERT(mActor);
  mActor->SendCancel();
}

void SpeechTaskChild::ForceEnd() {
  MOZ_ASSERT(mActor);
  mActor->SendForceEnd();
}

void SpeechTaskChild::SetAudioOutputVolume(float aVolume) {
  if (mActor) {
    mActor->SendSetAudioOutputVolume(aVolume);
  }
}

}  // namespace mozilla::dom
