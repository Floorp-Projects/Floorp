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

// IID for the nsICSSStyleSheet interface {8f83b0f0-b21a-11d1-8031-006008159b5a}
#define NS_ICSS_STYLE_SHEET_IID     \
{0x8f83b0f0, 0xb21a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsICSSStyleSheet : public nsIStyleSheet {
public:
  virtual PRBool ContainsStyleSheet(nsIURL* aURL) = 0;

  virtual void AppendStyleSheet(nsICSSStyleSheet* aSheet) = 0;

  // XXX do these belong here or are they generic?
  virtual void PrependStyleRule(nsICSSStyleRule* aRule) = 0;
  virtual void AppendStyleRule(nsICSSStyleRule* aRule) = 0;

};

extern NS_HTML nsresult
  NS_NewCSSStyleSheet(nsICSSStyleSheet** aInstancePtrResult, nsIURL* aURL);

#endif /* nsICSSStyleSheet_h___ */
