/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesisParent_h
#define mozilla_dom_SpeechSynthesisParent_h

#include "mozilla/dom/PSpeechSynthesisParent.h"
#include "mozilla/dom/PSpeechSynthesisRequestParent.h"
#include "nsSpeechTask.h"

namespace mozilla::dom {

class ContentParent;
class SpeechTaskParent;
class SpeechSynthesisRequestParent;

class SpeechSynthesisParent : public PSpeechSynthesisParent {
  friend class ContentParent;
  friend class SpeechSynthesisRequestParent;
  friend class PSpeechSynthesisParent;

 public:
  NS_INLINE_DECL_REFCOUNTING(SpeechSynthesisParent, override)

  void ActorDestroy(ActorDestroyReason aWhy) override;

  bool SendInit();

 protected:
  SpeechSynthesisParent();
  virtual ~SpeechSynthesisParent();
  PSpeechSynthesisRequestParent* AllocPSpeechSynthesisRequestParent(
      const nsAString& aText, const nsAString& aLang, const nsAString& aUri,
      const float& aVolume, const float& aRate, const float& aPitch,
      const bool& aShouldResistFingerprinting);

  bool DeallocPSpeechSynthesisRequestParent(
      PSpeechSynthesisRequestParent* aActor);

  mozilla::ipc::IPCResult RecvPSpeechSynthesisRequestConstructor(
      PSpeechSynthesisRequestParent* aActor, const nsAString& aText,
      const nsAString& aLang, const nsAString& aUri, const float& aVolume,
      const float& aRate, const float& aPitch,
      const bool& aShouldResistFingerprinting) override;
};

class SpeechSynthesisRequestParent : public PSpeechSynthesisRequestParent {
 public:
  explicit SpeechSynthesisRequestParent(SpeechTaskParent* aTask);
  virtual ~SpeechSynthesisRequestParent();

  RefPtr<SpeechTaskParent> mTask;

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvPause() override;

  mozilla::ipc::IPCResult RecvResume() override;

  mozilla::ipc::IPCResult RecvCancel() override;

  mozilla::ipc::IPCResult RecvForceEnd() override;

  mozilla::ipc::IPCResult RecvSetAudioOutputVolume(
      const float& aVolume) override;

  mozilla::ipc::IPCResult Recv__delete__() override;
};

class SpeechTaskParent : public nsSpeechTask {
  friend class SpeechSynthesisRequestParent;

 public:
  SpeechTaskParent(float aVolume, const nsAString& aUtterance,
                   bool aShouldResistFingerprinting)
      : nsSpeechTask(aVolume, aUtterance, aShouldResistFingerprinting),
        mActor(nullptr) {}

  nsresult DispatchStartImpl(const nsAString& aUri) override;

  nsresult DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex) override;

  nsresult DispatchPauseImpl(float aElapsedTime, uint32_t aCharIndex) override;

  nsresult DispatchResumeImpl(float aElapsedTime, uint32_t aCharIndex) override;

  nsresult DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex) override;

  nsresult DispatchBoundaryImpl(const nsAString& aName, float aElapsedTime,
                                uint32_t aCharIndex, uint32_t aCharLength,
                                uint8_t argc) override;

  nsresult DispatchMarkImpl(const nsAString& aName, float aElapsedTime,
                            uint32_t aCharIndex) override;

 private:
  SpeechSynthesisRequestParent* mActor;
};

}  // namespace mozilla::dom

#endif
