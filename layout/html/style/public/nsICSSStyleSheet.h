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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsICSSStyleSheet_h___
#define nsICSSStyleSheet_h___

#include "nslayout.h"
#include "nsIStyleSheet.h"

class nsICSSStyleRule;
class nsIDOMNode;

// IID for the nsICSSStyleSheet interface {8f83b0f0-b21a-11d1-8031-006008159b5a}
#define NS_ICSS_STYLE_SHEET_IID     \
{0x8f83b0f0, 0xb21a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSStyleSheet : public nsIStyleSheet {
public:
  virtual PRBool ContainsStyleSheet(nsIURL* aURL) const = 0;

  virtual void AppendStyleSheet(nsICSSStyleSheet* aSheet) = 0;

  virtual void PrependStyleRule(nsICSSStyleRule* aRule) = 0;
  virtual void AppendStyleRule(nsICSSStyleRule* aRule) = 0;

  virtual PRInt32   StyleRuleCount(void) const = 0;
  virtual nsresult  GetStyleRuleAt(PRInt32 aIndex, nsICSSStyleRule*& aRule) const = 0;

  virtual PRInt32   StyleSheetCount(void) const = 0;
  virtual nsresult  GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const = 0;

  NS_IMETHOD  Init(nsIURL* aURL) = 0;
  NS_IMETHOD  SetTitle(const nsString& aTitle) = 0;
  NS_IMETHOD  AppendMedium(nsIAtom* aMedium) = 0;
  NS_IMETHOD  SetOwningNode(nsIDOMNode* aOwningNode) = 0;
};

// XXX for backwards compatibility and convenience
extern NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURL* aURL);

extern NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult);

#endif /* nsICSSStyleSheet_h___ */
