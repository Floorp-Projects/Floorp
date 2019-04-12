/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechGrammar.h"

#include "mozilla/dom/SpeechGrammarBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(SpeechGrammar, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechGrammar)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechGrammar)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechGrammar)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechGrammar::SpeechGrammar(nsISupports* aParent) : mParent(aParent) {}

SpeechGrammar::~SpeechGrammar() = default;

already_AddRefed<SpeechGrammar> SpeechGrammar::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  RefPtr<SpeechGrammar> speechGrammar =
      new SpeechGrammar(aGlobal.GetAsSupports());
  return speechGrammar.forget();
}

nsISupports* SpeechGrammar::GetParentObject() const { return mParent; }

JSObject* SpeechGrammar::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return SpeechGrammar_Binding::Wrap(aCx, this, aGivenProto);
}

void SpeechGrammar::GetSrc(nsString& aRetVal, ErrorResult& aRv) const {
  aRetVal = mSrc;
}

void SpeechGrammar::SetSrc(const nsAString& aArg, ErrorResult& aRv) {
  mSrc = aArg;
}

float SpeechGrammar::GetWeight(ErrorResult& aRv) const {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return 0;
}

void SpeechGrammar::SetWeight(float aArg, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

}  // namespace dom
}  // namespace mozilla
