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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIStyledContent_h___
#define nsIStyledContent_h___

#include "nsIContent.h"

class nsString;
class nsIStyleRule;
class nsIStyleContext;
class nsISupportsArray;
class nsIRuleWalker;

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
  NS_IMETHOD HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const = 0;

  NS_IMETHOD WalkContentStyleRules(nsIRuleWalker* aRuleWalker) = 0;
  NS_IMETHOD WalkInlineStyleRules(nsIRuleWalker* aRuleWalker) = 0;

  /** NRA ***
   * Get a hint that tells the style system what to do when 
   * an attribute on this node changes.
   * This only applies to attributes that map their value
   * DIRECTLY into style contexts via NON-CSS style rules
   * All other attributes return NS_STYLE_HINT_CONTENT
   */
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType, 
                                      PRInt32& aHint) const = 0;

};

#endif /* nsIStyledContent_h___ */
