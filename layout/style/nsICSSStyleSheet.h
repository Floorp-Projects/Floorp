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

class nsICSSRule;
class nsIDOMNode;
class nsINameSpace;

// IID for the nsICSSStyleSheet interface {8f83b0f0-b21a-11d1-8031-006008159b5a}
#define NS_ICSS_STYLE_SHEET_IID     \
{0x8f83b0f0, 0xb21a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSStyleSheet : public nsIStyleSheet {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_STYLE_SHEET_IID; return iid; }

  NS_IMETHOD  ContainsStyleSheet(nsIURI* aURL) const = 0;

  NS_IMETHOD  AppendStyleSheet(nsICSSStyleSheet* aSheet) = 0;
  NS_IMETHOD  InsertStyleSheetAt(nsICSSStyleSheet* aSheet, PRInt32 aIndex) = 0;

  NS_IMETHOD  PrependStyleRule(nsICSSRule* aRule) = 0;
  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule) = 0;

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const = 0;

  NS_IMETHOD  StyleSheetCount(PRInt32& aCount) const = 0;
  NS_IMETHOD  GetStyleSheetAt(PRInt32 aIndex, nsICSSStyleSheet*& aSheet) const = 0;

  NS_IMETHOD  Init(nsIURI* aURL) = 0;
  NS_IMETHOD  SetTitle(const nsString& aTitle) = 0;
  NS_IMETHOD  AppendMedium(nsIAtom* aMedium) = 0;
  NS_IMETHOD  ClearMedia(void) = 0;
  NS_IMETHOD  SetOwningNode(nsIDOMNode* aOwningNode) = 0;

  // get head of namespace chain for sheet
  NS_IMETHOD  GetNameSpace(nsINameSpace*& aNameSpace) const = 0;
  // set default namespace for sheet (may be overridden by @namespace)
  NS_IMETHOD  SetDefaultNameSpaceID(PRInt32 aDefaultNameSpaceID) = 0;

  NS_IMETHOD  Clone(nsICSSStyleSheet*& aClone) const = 0;

  NS_IMETHOD  IsUnmodified(void) const = 0; // NS_OK if not modified since construct/last reset, NS_COMFALSE otherwise
  NS_IMETHOD  SetModified(PRBool aModified) = 0;
};

// XXX for backwards compatibility and convenience
extern NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURI* aURL);

extern NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult);

#endif /* nsICSSStyleSheet_h___ */
