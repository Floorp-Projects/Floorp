/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesisParent_h
#define mozilla_dom_SpeechSynthesisParent_h

#include "mozilla/dom/PSpeechSynthesisParent.h"
#include "mozilla/dom/PSpeechSynthesisRequestParent.h"
#include "nsSpeechTask.h"

namespace mozilla {
namespace dom {

class ContentParent;
class SpeechTaskParent;
class SpeechSynthesisRequestParent;

class SpeechSynthesisParent : public PSpeechSynthesisParent
{
  friend class ContentParent;
  friend class SpeechSynthesisRequestParent;

public:
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvReadVoicesAndState(InfallibleTArray<RemoteVoice>* aVoices,
                                                 InfallibleTArray<nsString>* aDefaults,
                                                 bool* aIsSpeaking) override;

protected:
  SpeechSynthesisParent();
  virtual ~SpeechSynthesisParent();
  PSpeechSynthesisRequestParent* AllocPSpeechSynthesisRequestParent(const nsString& aText,
                                                                    const nsString& aLang,
                                                                    const nsString& aUri,
                                                                    const float& aVolume,
                                                                    const float& aRate,
                                                                    const float& aPitch)
                                                                    override;

  bool DeallocPSpeechSynthesisRequestParent(PSpeechSynthesisRequestParent* aActor) override;

  mozilla::ipc::IPCResult RecvPSpeechSynthesisRequestConstructor(PSpeechSynthesisRequestParent* aActor,
                                                                 const nsString& aText,
                                                                 const nsString& aLang,
                                                                 const nsString& aUri,
                                                                 const float& aVolume,
                                                                 const float& aRate,
                                                                 const float& aPitch) override;
};

class SpeechSynthesisRequestParent : public PSpeechSynthesisRequestParent
{
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

  mozilla::ipc::IPCResult RecvSetAudioOutputVolume(const float& aVolume) override;

  mozilla::ipc::IPCResult Recv__delete__() override;
};

class SpeechTaskParent : public nsSpeechTask
{
  friend class SpeechSynthesisRequestParent;
public:
  SpeechTaskParent(float aVolume, const nsAString& aUtterance)
    : nsSpeechTask(aVolume, aUtterance) {}

  nsresult DispatchStartImpl(const nsAString& aUri);

  nsresult DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex);

  nsresult DispatchPauseImpl(float aElapsedTime, uint32_t aCharIndex);

  nsresult DispatchResumeImpl(float aElapsedTime, uint32_t aCharIndex);

  nsresult DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex);

  nsresult DispatchBoundaryImpl(const nsAString& aName,
                                float aElapsedTime, uint32_t aCharIndex);

  nsresult DispatchMarkImpl(const nsAString& aName,
                            float aElapsedTime, uint32_t aCharIndex);

private:
  SpeechSynthesisRequestParent* mActor;
};

} // namespace dom
} // namespace mozilla

#endif
