/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICSSRuleList_h
#define nsICSSRuleList_h

#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSRuleList.h"

// IID for the nsICSSRuleList interface
#define NS_ICSSRULELIST_IID \
{ 0xccc0135a, 0xbcb6, 0x4654, \
  { 0x8c, 0x78, 0x74, 0x35, 0x97, 0x9b, 0x88, 0x19 } }

class nsICSSRuleList : public nsIDOMCSSRuleList
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSSRULELIST_IID)

  NS_IMETHOD
  GetLength(uint32_t* aLength) MOZ_OVERRIDE MOZ_FINAL
  {
    *aLength = Length();
    return NS_OK;
  }
  NS_IMETHOD
  Item(uint32_t aIndex, nsIDOMCSSRule** aReturn) MOZ_OVERRIDE MOZ_FINAL
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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSRuleList, NS_ICSSRULELIST_IID)

#endif /* nsICSSRuleList_h */
