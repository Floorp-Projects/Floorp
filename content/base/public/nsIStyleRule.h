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
#ifndef nsIStyleRule_h___
#define nsIStyleRule_h___

#include <stdio.h>

#include "nslayout.h"
#include "nsISupports.h"

class nsIStyleSheet;
class nsIStyleContext;
class nsIPresContext;
class nsIContent;

// IID for the nsIStyleRule interface {40ae5c90-ad6a-11d1-8031-006008159b5a}
#define NS_ISTYLE_RULE_IID     \
{0x40ae5c90, 0xad6a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIStyleRule : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISTYLE_RULE_IID; return iid; }

  NS_IMETHOD Equals(const nsIStyleRule* aRule, PRBool& aResult) const = 0;
  NS_IMETHOD HashValue(PRUint32& aValue) const = 0;

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const = 0;

  // Strength is an out-of-band weighting, useful for mapping CSS ! important
  NS_IMETHOD GetStrength(PRInt32& aStrength) const = 0;

  // Map only font data into style context
  NS_IMETHOD MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext) = 0;
  // Map all non-font info into style context
  NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext) = 0;

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

#endif /* nsIStyleRule_h___ */
