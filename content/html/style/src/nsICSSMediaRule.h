/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsICSSMediaRule_h___
#define nsICSSMediaRule_h___

#include "nslayout.h"
#include "nsICSSGroupRule.h"
//#include "nsString.h"

class nsIAtom;

// IID for the nsICSSMediaRule interface {2c1d0110-1a09-11d3-805a-006008159b5a}
#define NS_ICSS_MEDIA_RULE_IID     \
{0x2c1d0110, 0x1a09, 0x11d3, {0x80, 0x5a, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSMediaRule : public nsICSSGroupRule {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_MEDIA_RULE_IID; return iid; }

  NS_IMETHOD  SetMedia(nsISupportsArray* aMedia) = 0;
  NS_IMETHOD_(PRBool)  UseForMedium(nsIAtom* aMedium) const = 0;
};

extern NS_HTML nsresult
  NS_NewCSSMediaRule(nsICSSMediaRule** aInstancePtrResult);

#endif /* nsICSSMediaRule_h___ */
