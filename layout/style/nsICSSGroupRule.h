/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsICSSGroupRule_h___
#define nsICSSGroupRule_h___

#include "nslayout.h"
#include "nsICSSRule.h"
#include "nsISupportsArray.h"

class nsIAtom;

// IID for the nsICSSGroupRule interface {5af048aa-1af0-11d3-9d83-0060088f9ff7}
#define NS_ICSS_GROUP_RULE_IID     \
{0x5af048aa, 0x1af0, 0x11d3, {0x9d, 0x83, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

class nsICSSGroupRule : public nsICSSRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_GROUP_RULE_IID; return iid; }

  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule) = 0;

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const = 0;

  NS_IMETHOD  EnumerateRulesForwards(nsISupportsArrayEnumFunc aFunc, void * aData) const = 0;
};

#endif /* nsICSSGroupRule_h___ */
