/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechGrammar.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/SpeechGrammarBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(SpeechGrammar, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechGrammar)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechGrammar)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechGrammar)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechGrammar::SpeechGrammar(nsISupports* aParent)
  : mParent(aParent)
{
  SetIsDOMBinding();
}

SpeechGrammar::~SpeechGrammar()
{
}

SpeechGrammar*
SpeechGrammar::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  return new SpeechGrammar(aGlobal.Get());
}

nsISupports*
SpeechGrammar::GetParentObject() const
{
  return mParent;
}

JSObject*
SpeechGrammar::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SpeechGrammarBinding::Wrap(aCx, aScope, this);
}

void
SpeechGrammar::GetSrc(nsString& aRetVal, ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

void
SpeechGrammar::SetSrc(const nsAString& aArg, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

float
SpeechGrammar::GetWeight(ErrorResult& aRv) const
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return 0;
}

void
SpeechGrammar::SetWeight(float aArg, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

} // namespace dom
} // namespace mozilla
