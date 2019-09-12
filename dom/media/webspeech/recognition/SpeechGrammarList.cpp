/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechGrammarList.h"

#include "mozilla/dom/SpeechGrammarListBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "SpeechRecognition.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SpeechGrammarList, mParent, mItems)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechGrammarList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechGrammarList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechGrammarList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechGrammarList::SpeechGrammarList(nsISupports* aParent) : mParent(aParent) {}

SpeechGrammarList::~SpeechGrammarList() = default;

already_AddRefed<SpeechGrammarList> SpeechGrammarList::Constructor(
    const GlobalObject& aGlobal) {
  RefPtr<SpeechGrammarList> speechGrammarList =
      new SpeechGrammarList(aGlobal.GetAsSupports());
  return speechGrammarList.forget();
}

JSObject* SpeechGrammarList::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return SpeechGrammarList_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* SpeechGrammarList::GetParentObject() const { return mParent; }

uint32_t SpeechGrammarList::Length() const { return mItems.Length(); }

already_AddRefed<SpeechGrammar> SpeechGrammarList::Item(uint32_t aIndex,
                                                        ErrorResult& aRv) {
  RefPtr<SpeechGrammar> result = mItems.ElementAt(aIndex);
  return result.forget();
}

void SpeechGrammarList::AddFromURI(const nsAString& aSrc,
                                   const Optional<float>& aWeight,
                                   ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void SpeechGrammarList::AddFromString(const nsAString& aString,
                                      const Optional<float>& aWeight,
                                      ErrorResult& aRv) {
  SpeechGrammar* speechGrammar = new SpeechGrammar(mParent);
  speechGrammar->SetSrc(aString, aRv);
  mItems.AppendElement(speechGrammar);
}

already_AddRefed<SpeechGrammar> SpeechGrammarList::IndexedGetter(
    uint32_t aIndex, bool& aPresent, ErrorResult& aRv) {
  if (aIndex >= Length()) {
    aPresent = false;
    return nullptr;
  }
  ErrorResult rv;
  aPresent = true;
  return Item(aIndex, rv);
}

}  // namespace dom
}  // namespace mozilla
