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
#ifndef nsICSSNameSpaceRule_h___
#define nsICSSNameSpaceRule_h___

#include "nslayout.h"
#include "nsICSSRule.h"
//#include "nsString.h"

class nsIAtom;
class nsIURI;

// IID for the nsICSSNameSpaceRule interface {2469c930-1a09-11d3-805a-006008159b5a}
#define NS_ICSS_NAMESPACE_RULE_IID     \
{0x2469c930, 0x1a09, 0x11d3, {0x80, 0x5a, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSNameSpaceRule : public nsICSSRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_NAMESPACE_RULE_IID; return iid; }

  NS_IMETHOD  GetPrefix(nsIAtom*& aPrefix) const = 0;
  NS_IMETHOD  SetPrefix(nsIAtom* aPrefix) = 0;

  NS_IMETHOD  GetURLSpec(nsString& aURLSpec) const = 0;
  NS_IMETHOD  SetURLSpec(const nsString& aURLSpec) = 0;
};

extern NS_HTML nsresult
  NS_NewCSSNameSpaceRule(nsICSSNameSpaceRule** aInstancePtrResult, 
                         nsIAtom* aPrefix, const nsString& aURLSpec);

#endif /* nsICSSNameSpaceRule_h___ */
