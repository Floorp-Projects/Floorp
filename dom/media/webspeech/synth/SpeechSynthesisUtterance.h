/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesisUtterance_h
#define mozilla_dom_SpeechSynthesisUtterance_h

#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "js/TypeDecls.h"

#include "nsSpeechTask.h"

namespace mozilla {
namespace dom {

class SpeechSynthesisVoice;
class SpeechSynthesis;
class nsSynthVoiceRegistry;

class SpeechSynthesisUtterance final : public DOMEventTargetHelper
{
  friend class SpeechSynthesis;
  friend class nsSpeechTask;
  friend class nsSynthVoiceRegistry;

public:
  SpeechSynthesisUtterance(nsPIDOMWindowInner* aOwnerWindow, const nsAString& aText);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SpeechSynthesisUtterance,
                                           DOMEventTargetHelper)
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static
  already_AddRefed<SpeechSynthesisUtterance> Constructor(GlobalObject& aGlobal,
                                                         ErrorResult& aRv);
  static
  already_AddRefed<SpeechSynthesisUtterance> Constructor(GlobalObject& aGlobal,
                                                         const nsAString& aText,
                                                         ErrorResult& aRv);

  void GetText(nsString& aResult) const;

  void SetText(const nsAString& aText);

  void GetLang(nsString& aResult) const;

  void SetLang(const nsAString& aLang);

  SpeechSynthesisVoice* GetVoice() const;

  void SetVoice(SpeechSynthesisVoice* aVoice);

  float Volume() const;

  void SetVolume(float aVolume);

  float Rate() const;

  void SetRate(float aRate);

  float Pitch() const;

  void SetPitch(float aPitch);

  void GetChosenVoiceURI(nsString& aResult) const;

  enum {
    STATE_NONE,
    STATE_PENDING,
    STATE_SPEAKING,
    STATE_ENDED
  };

  uint32_t GetState() { return mState; }

  bool IsPaused() { return mPaused; }

  IMPL_EVENT_HANDLER(start)
  IMPL_EVENT_HANDLER(end)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(pause)
  IMPL_EVENT_HANDLER(resume)
  IMPL_EVENT_HANDLER(mark)
  IMPL_EVENT_HANDLER(boundary)

private:
  virtual ~SpeechSynthesisUtterance();

  void DispatchSpeechSynthesisEvent(const nsAString& aEventType,
                                    uint32_t aCharIndex,
                                    float aElapsedTime, const nsAString& aName);

  nsString mText;

  nsString mLang;

  float mVolume;

  float mRate;

  float mPitch;

  nsString mChosenVoiceURI;

  uint32_t mState;

  bool mPaused;

  RefPtr<SpeechSynthesisVoice> mVoice;
};

} // namespace dom
} // namespace mozilla

#endif
