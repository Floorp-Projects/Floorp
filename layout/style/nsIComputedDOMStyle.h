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
#ifndef nsIComputedDOMStyle_h___
#define nsIComputedDOMStyle_h___

#include "nslayout.h"
#include "nsIDOMCSS2Properties.h"

class nsIDOMElement;
class nsIPresShell;

#define NS_ICOMPUTEDDOMSTYLE_IID \
 { 0x5f0197a1, 0xa873, 0x44e5, \
    {0x96, 0x31, 0xac, 0xd6, 0xca, 0xb4, 0xf1, 0xe0 } }

class nsIComputedDOMStyle : public nsIDOMCSSStyleDeclaration
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOMPUTEDDOMSTYLE_IID)

  NS_IMETHOD Init(nsIDOMElement *aElement, const nsAReadableString& aPseudoElt,
                  nsIPresShell *aPresShell) = 0;
};

extern NS_HTML nsresult 
NS_NewComputedDOMStyle(nsIComputedDOMStyle** aComputedStyle);

#endif /* nsIComputedDOMStyle_h___ */
