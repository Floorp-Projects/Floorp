/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SpeechRecognitionError_h__
#define SpeechRecognitionError_h__

#include "mozilla/dom/Event.h"
#include "mozilla/dom/SpeechRecognitionErrorBinding.h"

namespace mozilla {
namespace dom {

class SpeechRecognitionError : public Event
{
public:
  SpeechRecognitionError(mozilla::dom::EventTarget* aOwner,
                         nsPresContext* aPresContext,
                         WidgetEvent* aEvent);
  virtual ~SpeechRecognitionError();

  static already_AddRefed<SpeechRecognitionError>
  Constructor(const GlobalObject& aGlobal,
              const nsAString& aType,
              const SpeechRecognitionErrorInit& aParam,
              ErrorResult& aRv);

  virtual JSObject* WrapObjectInternal(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::SpeechRecognitionError_Binding::Wrap(aCx, this, aGivenProto);
  }

  void
  GetMessage(nsAString& aString)
  {
    aString = mMessage;
  }

  SpeechRecognitionErrorCode
  Error()
  {
    return mError;
  }

  void
  InitSpeechRecognitionError(const nsAString& aType,
                             bool aCanBubble,
                             bool aCancelable,
                             SpeechRecognitionErrorCode aError,
                             const nsAString& aMessage);

protected:
  SpeechRecognitionErrorCode mError;
  nsString mMessage;
};

} // namespace dom
} // namespace mozilla

#endif // SpeechRecognitionError_h__
