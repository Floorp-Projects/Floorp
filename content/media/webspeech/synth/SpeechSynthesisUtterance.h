/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesisUtterance_h
#define mozilla_dom_SpeechSynthesisUtterance_h

#include "nsCOMPtr.h"
#include "nsDOMEventTargetHelper.h"
#include "nsString.h"
#include "js/TypeDecls.h"

#include "nsSpeechTask.h"

namespace mozilla {
namespace dom {

class SpeechSynthesisVoice;
class SpeechSynthesis;
class nsSynthVoiceRegistry;

class SpeechSynthesisUtterance MOZ_FINAL : public nsDOMEventTargetHelper
{
  friend class SpeechSynthesis;
  friend class nsSpeechTask;
  friend class nsSynthVoiceRegistry;

public:
  SpeechSynthesisUtterance(nsPIDOMWindow* aOwnerWindow, const nsAString& aText);
  virtual ~SpeechSynthesisUtterance();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SpeechSynthesisUtterance,
                                           nsDOMEventTargetHelper)
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

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

  void DispatchSpeechSynthesisEvent(const nsAString& aEventType,
                                    uint32_t aCharIndex,
                                    float aElapsedTime, const nsAString& aName);

  nsString mText;

  nsString mLang;

  float mVolume;

  float mRate;

  float mPitch;

  uint32_t mState;

  bool mPaused;

  nsRefPtr<SpeechSynthesisVoice> mVoice;
};

} // namespace dom
} // namespace mozilla

#endif
