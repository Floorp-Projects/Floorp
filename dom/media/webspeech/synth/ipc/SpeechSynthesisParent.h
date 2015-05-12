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
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  bool RecvReadVoiceList(InfallibleTArray<RemoteVoice>* aVoices,
                         InfallibleTArray<nsString>* aDefaults) override;

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

  bool RecvPSpeechSynthesisRequestConstructor(PSpeechSynthesisRequestParent* aActor,
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

  nsRefPtr<SpeechTaskParent> mTask;

protected:

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvPause() override;

  virtual bool RecvResume() override;

  virtual bool RecvCancel() override;
};

class SpeechTaskParent : public nsSpeechTask
{
  friend class SpeechSynthesisRequestParent;
public:
  SpeechTaskParent(float aVolume, const nsAString& aUtterance)
    : nsSpeechTask(aVolume, aUtterance) {}

  virtual nsresult DispatchStartImpl(const nsAString& aUri);

  virtual nsresult DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchPauseImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchResumeImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchBoundaryImpl(const nsAString& aName,
                                        float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchMarkImpl(const nsAString& aName,
                                    float aElapsedTime, uint32_t aCharIndex);

private:
  SpeechSynthesisRequestParent* mActor;
};

} // namespace dom
} // namespace mozilla

#endif
