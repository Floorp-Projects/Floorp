/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"

#include "GeneratedEvents.h"
#include "nsIDOMSpeechSynthesisEvent.h"

#include "mozilla/dom/SpeechSynthesisUtteranceBinding.h"
#include "SpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(SpeechSynthesisUtterance)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(SpeechSynthesisUtterance, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeechSynthesisUtterance, nsDOMEventTargetHelper)

SpeechSynthesisUtterance::SpeechSynthesisUtterance(const nsAString& text)
  : mText(text)
  , mVolume(1)
  , mRate(1)
  , mPitch(1)
  , mState(STATE_NONE)
  , mPaused(false)
{
  SetIsDOMBinding();
}

SpeechSynthesisUtterance::~SpeechSynthesisUtterance() {}

JSObject*
SpeechSynthesisUtterance::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return SpeechSynthesisUtteranceBinding::Wrap(aCx, aScope, this);
}

nsISupports*
SpeechSynthesisUtterance::GetParentObject() const
{
  return GetOwner();
}

already_AddRefed<SpeechSynthesisUtterance>
SpeechSynthesisUtterance::Constructor(GlobalObject& aGlobal,
                                      ErrorResult& aRv)
{
  return Constructor(aGlobal, NS_LITERAL_STRING(""), aRv);
}

already_AddRefed<SpeechSynthesisUtterance>
SpeechSynthesisUtterance::Constructor(GlobalObject& aGlobal,
                                      const nsAString& aText,
                                      ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.Get());

  if (!win) {
    aRv.Throw(NS_ERROR_FAILURE);
  }

  MOZ_ASSERT(win->IsInnerWindow());
  SpeechSynthesisUtterance* object = new SpeechSynthesisUtterance(aText);
  NS_ADDREF(object);
  object->BindToOwner(win);
  return object;
}

void
SpeechSynthesisUtterance::GetText(nsString& aResult) const
{
  aResult = mText;
}

void
SpeechSynthesisUtterance::SetText(const nsAString& aText)
{
  mText = aText;
}

void
SpeechSynthesisUtterance::GetLang(nsString& aResult) const
{
  aResult = mLang;
}

void
SpeechSynthesisUtterance::SetLang(const nsAString& aLang)
{
  mLang = aLang;
}

SpeechSynthesisVoice*
SpeechSynthesisUtterance::GetVoice() const
{
  return mVoice;
}

void
SpeechSynthesisUtterance::SetVoice(SpeechSynthesisVoice* aVoice)
{
  mVoice = aVoice;
}

float
SpeechSynthesisUtterance::Volume() const
{
  return mVolume;
}

void
SpeechSynthesisUtterance::SetVolume(float aVolume)
{
  mVolume = aVolume;
}

float
SpeechSynthesisUtterance::Rate() const
{
  return mRate;
}

void
SpeechSynthesisUtterance::SetRate(float aRate)
{
  mRate = aRate;
}

float
SpeechSynthesisUtterance::Pitch() const
{
  return mPitch;
}

void
SpeechSynthesisUtterance::SetPitch(float aPitch)
{
  mPitch = aPitch;
}

void
SpeechSynthesisUtterance::DispatchSpeechSynthesisEvent(const nsAString& aEventType,
                                                       uint32_t aCharIndex,
                                                       float aElapsedTime,
                                                       const nsAString& aName)
{
  nsCOMPtr<nsIDOMEvent> domEvent;
  NS_NewDOMSpeechSynthesisEvent(getter_AddRefs(domEvent), nullptr, nullptr, nullptr);

  nsCOMPtr<nsIDOMSpeechSynthesisEvent> ssEvent = do_QueryInterface(domEvent);
  ssEvent->InitSpeechSynthesisEvent(aEventType, false, false,
                                    aCharIndex, aElapsedTime, aName);

  DispatchTrustedEvent(domEvent);
}

} // namespace dom
} // namespace mozilla
