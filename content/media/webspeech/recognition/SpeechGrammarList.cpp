/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeechGrammarList.h"

#include "mozilla/dom/SpeechGrammarListBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_1(SpeechGrammarList, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechGrammarList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechGrammarList)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechGrammarList)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

SpeechGrammarList::SpeechGrammarList(nsISupports* aParent)
  : mParent(aParent)
{
  SetIsDOMBinding();
}

SpeechGrammarList::~SpeechGrammarList()
{
}

SpeechGrammarList*
SpeechGrammarList::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  return new SpeechGrammarList(aGlobal.Get());
}

JSObject*
SpeechGrammarList::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SpeechGrammarListBinding::Wrap(aCx, aScope, this);
}

nsISupports*
SpeechGrammarList::GetParentObject() const
{
  return mParent;
}

uint32_t
SpeechGrammarList::Length() const
{
  return 0;
}

already_AddRefed<SpeechGrammar>
SpeechGrammarList::Item(uint32_t aIndex, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

void
SpeechGrammarList::AddFromURI(const nsAString& aSrc,
                              const Optional<float>& aWeight,
                              ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

void
SpeechGrammarList::AddFromString(const nsAString& aString,
                                 const Optional<float>& aWeight,
                                 ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

already_AddRefed<SpeechGrammar>
SpeechGrammarList::IndexedGetter(uint32_t aIndex, bool& aPresent,
                                 ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}
} // namespace dom
} // namespace mozilla
