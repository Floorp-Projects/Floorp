/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICSSRuleList_h___
#define nsICSSRuleList_h___

#include "nsIDOMCSSRuleList.h"

// IID for the nsICSSRuleList interface
#define NS_ICSSRULELIST_IID \
{ 0x7ae746fd, 0x259a, 0x4a69, \
 { 0x97, 0x2d, 0x2c, 0x10, 0xf7, 0xb0, 0x04, 0xa1 } }

class nsICSSRuleList : public nsIDOMCSSRuleList
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICSSRULELIST_IID)

  virtual nsIDOMCSSRule* GetItemAt(uint32_t aIndex, nsresult* aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICSSRuleList, NS_ICSSRULELIST_IID)

#endif /* nsICSSRuleList_h___ */
