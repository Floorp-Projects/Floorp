/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechRecognitionError.h"

namespace mozilla {
namespace dom {

SpeechRecognitionError::SpeechRecognitionError(
    mozilla::dom::EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent), mError() {}

SpeechRecognitionError::~SpeechRecognitionError() {}

already_AddRefed<SpeechRecognitionError> SpeechRecognitionError::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const SpeechRecognitionErrorInit& aParam) {
  nsCOMPtr<mozilla::dom::EventTarget> t =
      do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<SpeechRecognitionError> e =
      new SpeechRecognitionError(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitSpeechRecognitionError(aType, aParam.mBubbles, aParam.mCancelable,
                                aParam.mError, aParam.mMessage);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

void SpeechRecognitionError::InitSpeechRecognitionError(
    const nsAString& aType, bool aCanBubble, bool aCancelable,
    SpeechRecognitionErrorCode aError, const nsAString& aMessage) {
  Event::InitEvent(aType, aCanBubble, aCancelable);
  mError = aError;
  mMessage = aMessage;
}

}  // namespace dom
}  // namespace mozilla
