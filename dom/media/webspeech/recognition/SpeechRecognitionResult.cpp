/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechRecognitionResult.h"
#include "mozilla/dom/SpeechRecognitionResultBinding.h"

#include "SpeechRecognition.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SpeechRecognitionResult, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechRecognitionResult)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechRecognitionResult)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechRecognitionResult)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechRecognitionResult::SpeechRecognitionResult(SpeechRecognition* aParent)
  : mParent(aParent)
{
}

SpeechRecognitionResult::~SpeechRecognitionResult()
{
}

JSObject*
SpeechRecognitionResult::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SpeechRecognitionResultBinding::Wrap(aCx, this, aGivenProto);
}

nsISupports*
SpeechRecognitionResult::GetParentObject() const
{
  return static_cast<DOMEventTargetHelper*>(mParent.get());
}

already_AddRefed<SpeechRecognitionAlternative>
SpeechRecognitionResult::IndexedGetter(uint32_t aIndex, bool& aPresent)
{
  if (aIndex >= Length()) {
    aPresent = false;
    return nullptr;
  }

  aPresent = true;
  return Item(aIndex);
}

uint32_t
SpeechRecognitionResult::Length() const
{
  return mItems.Length();
}

already_AddRefed<SpeechRecognitionAlternative>
SpeechRecognitionResult::Item(uint32_t aIndex)
{
  nsRefPtr<SpeechRecognitionAlternative> alternative = mItems.ElementAt(aIndex);
  return alternative.forget();
}

bool
SpeechRecognitionResult::IsFinal() const
{
  return true; // TODO
}

} // namespace dom
} // namespace mozilla
