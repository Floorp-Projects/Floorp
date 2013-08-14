/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechRecognitionResultList.h"

#include "mozilla/dom/SpeechRecognitionResultListBinding.h"

#include "SpeechRecognition.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(SpeechRecognitionResultList, mParent, mItems)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechRecognitionResultList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechRecognitionResultList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechRecognitionResultList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechRecognitionResultList::SpeechRecognitionResultList(SpeechRecognition* aParent)
  : mParent(aParent)
{
  SetIsDOMBinding();
}

SpeechRecognitionResultList::~SpeechRecognitionResultList()
{
}

nsISupports*
SpeechRecognitionResultList::GetParentObject() const
{
  return static_cast<nsDOMEventTargetHelper*>(mParent.get());
}

JSObject*
SpeechRecognitionResultList::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aScope)
{
  return SpeechRecognitionResultListBinding::Wrap(aCx, aScope, this);
}

already_AddRefed<SpeechRecognitionResult>
SpeechRecognitionResultList::IndexedGetter(uint32_t aIndex, bool& aPresent)
{
  if (aIndex >= Length()) {
    aPresent = false;
    return nullptr;
  }

  aPresent = true;
  return Item(aIndex);
}

uint32_t
SpeechRecognitionResultList::Length() const
{
  return mItems.Length();
}

already_AddRefed<SpeechRecognitionResult>
SpeechRecognitionResultList::Item(uint32_t aIndex)
{
  nsRefPtr<SpeechRecognitionResult> result = mItems.ElementAt(aIndex);
  return result.forget();
}
} // namespace dom
} // namespace mozilla
