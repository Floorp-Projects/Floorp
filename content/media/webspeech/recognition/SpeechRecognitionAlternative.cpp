/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechRecognitionAlternative.h"

#include "mozilla/dom/SpeechRecognitionAlternativeBinding.h"

#include "SpeechRecognition.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(SpeechRecognitionAlternative, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechRecognitionAlternative)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechRecognitionAlternative)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechRecognitionAlternative)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechRecognitionAlternative::SpeechRecognitionAlternative(SpeechRecognition* aParent)
  : mTranscript(NS_LITERAL_STRING(""))
  , mConfidence(0)
  , mParent(aParent)
{
  SetIsDOMBinding();
}

SpeechRecognitionAlternative::~SpeechRecognitionAlternative()
{
}

JSObject*
SpeechRecognitionAlternative::WrapObject(JSContext* aCx)
{
  return SpeechRecognitionAlternativeBinding::Wrap(aCx, this);
}

nsISupports*
SpeechRecognitionAlternative::GetParentObject() const
{
  return static_cast<DOMEventTargetHelper*>(mParent.get());
}

void
SpeechRecognitionAlternative::GetTranscript(nsString& aRetVal) const
{
  aRetVal = mTranscript;
}

float
SpeechRecognitionAlternative::Confidence() const
{
  return mConfidence;
}
} // namespace dom
} // namespace mozilla
