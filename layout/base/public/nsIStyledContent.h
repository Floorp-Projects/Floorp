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
#ifndef nsIStyledContent_h___
#define nsIStyledContent_h___

#include "nsIContent.h"

class nsString;
class nsIStyleRule;
class nsIStyleContext;

// IID for the nsIStyledContent class
#define NS_ISTYLEDCONTENT_IID   \
{ 0xc1e84e01, 0xcd15, 0x11d2, { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } }

// Abstract interface for all styled content (that supports ID, CLASS, STYLE, and
// the ability to specify style hints on an attribute change).
class nsIStyledContent : public nsIContent {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISTYLEDCONTENT_IID; return iid; }

  NS_IMETHOD GetID(nsIAtom*& aResult) const = 0;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const = 0;
  NS_IMETHOD HasClass(nsIAtom* aClass) const = 0;

  NS_IMETHOD GetContentStyleRule(nsIStyleRule*& aResult) = 0;
  NS_IMETHOD GetInlineStyleRule(nsIStyleRule*& aResult) = 0;

  /** NRA ***
   * Get a hint that tells the style system what to do when 
   * an attribute on this node changes.
   */
  NS_IMETHOD GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const = 0;

};

#endif /* nsIStyledContent_h___ */
