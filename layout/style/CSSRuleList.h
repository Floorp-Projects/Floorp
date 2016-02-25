/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSRuleList_h
#define mozilla_dom_CSSRuleList_h

#include "mozilla/CSSStyleSheet.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSRuleList.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

// IID for the CSSRuleList interface
#define NS_ICSSRULELIST_IID \
{ 0x56ac8d1c, 0xc1ed, 0x45fe, \
  { 0x9a, 0x4d, 0x3a, 0xdc, 0xf9, 0xd1, 0xb9, 0x3f } }

class CSSRuleList : public nsIDOMCSSRuleList
                  , public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSSRULELIST_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CSSRuleList)

  virtual CSSStyleSheet* GetParentObject() = 0;
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override final;

  NS_IMETHOD
  GetLength(uint32_t* aLength) override final
  {
    *aLength = Length();
    return NS_OK;
  }
  NS_IMETHOD
  Item(uint32_t aIndex, nsIDOMCSSRule** aReturn) override final
  {
    NS_IF_ADDREF(*aReturn = Item(aIndex));
    return NS_OK;
  }

  // WebIDL API
  nsIDOMCSSRule* Item(uint32_t aIndex)
  {
    bool unused;
    return IndexedGetter(aIndex, unused);
  }

  virtual nsIDOMCSSRule* IndexedGetter(uint32_t aIndex, bool& aFound) = 0;
  virtual uint32_t Length() = 0;

protected:
  virtual ~CSSRuleList() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(CSSRuleList, NS_ICSSRULELIST_IID)

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_CSSRuleList_h */
