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
#ifndef nsIStyleRuleProcessor_h___
#define nsIStyleRuleProcessor_h___

#include <stdio.h>

#include "nslayout.h"
#include "nsISupports.h"

class nsISizeOfHandler;

class nsIStyleSheet;
class nsIStyleContext;
class nsIPresContext;
class nsIContent;
class nsISupportsArray;
class nsIAtom;

// IID for the nsIStyleRuleProcessor interface {015575fe-7b6c-11d3-ba05-001083023c2b}
#define NS_ISTYLE_RULE_PROCESSOR_IID     \
{0x015575fe, 0x7b6c, 0x11d3, {0xba, 0x05, 0x00, 0x10, 0x83, 0x02, 0x3c, 0x2b}}

/* The style rule processor interface is a mechanism to seperate the matching
 * of style rules from style sheet instances.
 * Simple style sheets can and will act as their own processor. 
 * Sheets where rule ordering interlaces between multiple sheets, will need to 
 * share a single rule processor between them (CSS sheets do this for cascading order)
 */
class nsIStyleRuleProcessor : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISTYLE_RULE_PROCESSOR_IID; return iid; }

  // populate supports array with nsIStyleRule*
  // rules are ordered, those with higher precedence come last
  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium, 
                           nsIContent* aContent,
                           nsIStyleContext* aParentContext,
                           nsISupportsArray* aResults) = 0;

  NS_IMETHOD RulesMatching(nsIPresContext* aPresContext,
                           nsIAtom* aMedium, 
                           nsIContent* aParentContent,
                           nsIAtom* aPseudoTag,
                           nsIStyleContext* aParentContext,
                           nsISupportsArray* aResults) = 0;

  // Test if style is dependent on content state
  NS_IMETHOD  HasStateDependentStyle(nsIPresContext* aPresContext,
                                     nsIAtom* aMedium, 
                                     nsIContent*     aContent) = 0;

  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize) = 0;
};

#endif /* nsIStyleRuleProcessor_h___ */
