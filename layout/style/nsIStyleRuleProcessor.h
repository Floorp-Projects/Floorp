/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIStyleRuleProcessor_h___
#define nsIStyleRuleProcessor_h___

#include <stdio.h>

#include "nsISupports.h"
#include "nsIPresContext.h" // for nsCompatability
#include "nsILinkHandler.h"

class nsISizeOfHandler;

class nsIStyleSheet;
class nsIStyleContext;
class nsIPresContext;
class nsIContent;
class nsIStyledContent;
class nsISupportsArray;
class nsIAtom;
class nsICSSPseudoComparator;
class nsRuleWalker;

// The implementation of the constructor and destructor are currently in
// nsCSSStyleSheet.cpp.

struct RuleProcessorData {
  RuleProcessorData(nsIPresContext* aPresContext,
                    nsIContent* aContent, 
                    nsRuleWalker* aRuleWalker,
                    nsCompatibility* aCompat = nsnull);
  
  virtual ~RuleProcessorData();

  void* operator new(size_t sz, nsIPresContext* aContext) {
    void* result = nsnull;
    aContext->AllocateFromShell(sz, &result);
    return result;
  }
  void Destroy(nsIPresContext* aContext) {
    this->~RuleProcessorData();
    aContext->FreeToShell(sizeof(RuleProcessorData), this);
  };

  nsIPresContext*   mPresContext;
  nsIContent*       mContent;
  nsIContent*       mParentContent; // if content, content->GetParent()
  nsRuleWalker*     mRuleWalker; // Used to add rules to our results.
  nsIContent*       mScopedRoot;    // Root of scoped stylesheet (set and unset by the supplier of the scoped stylesheet
  
  nsIAtom*          mContentTag;    // if content, then content->GetTag()
  nsIAtom*          mContentID;     // if styled content, then styledcontent->GetID()
  nsIStyledContent* mStyledContent; // if content, content->QI(nsIStyledContent)
  PRBool            mIsHTMLContent; // if content, then does QI on HTMLContent, true or false
  PRBool            mIsHTMLLink;    // if content, calls nsStyleUtil::IsHTMLLink
  PRBool            mIsSimpleXLink; // if content, calls nsStyleUtil::IsSimpleXLink
  nsLinkState       mLinkState;     // if a link, this is the state, otherwise unknown
  PRBool            mIsQuirkMode;   // Possibly remove use of this in SelectorMatches?
  PRInt32           mEventState;    // if content, eventStateMgr->GetContentState()
  PRBool            mHasAttributes; // if content, content->GetAttrCount() > 0
  PRInt32           mNameSpaceID;   // if content, content->GetNameSapce()
  RuleProcessorData* mPreviousSiblingData;
  RuleProcessorData* mParentData;
};

struct ElementRuleProcessorData : public RuleProcessorData {
  ElementRuleProcessorData(nsIPresContext* aPresContext,
                           nsIContent* aContent, 
                           nsRuleWalker* aRuleWalker)
  : RuleProcessorData(aPresContext,aContent,aRuleWalker)
  {
    NS_PRECONDITION(aContent, "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
  }
};

struct PseudoRuleProcessorData : public RuleProcessorData {
  PseudoRuleProcessorData(nsIPresContext* aPresContext,
                          nsIContent* aParentContent,
                          nsIAtom* aPseudoTag,
                          nsICSSPseudoComparator* aComparator,
                          nsRuleWalker* aRuleWalker)
  : RuleProcessorData(aPresContext, aParentContent, aRuleWalker)
  {
    NS_PRECONDITION(aPseudoTag, "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
    mPseudoTag = aPseudoTag;
    mComparator = aComparator;
  }

  nsIAtom*                 mPseudoTag;
  nsICSSPseudoComparator*  mComparator;
};

struct StateRuleProcessorData : public RuleProcessorData {
  StateRuleProcessorData(nsIPresContext* aPresContext,
                         nsIContent* aContent)
    : RuleProcessorData(aPresContext, aContent, nsnull)
  {
    NS_PRECONDITION(aContent, "null pointer");
  }
};


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

  // populate rule node tree with nsIStyleRule*
  // rules are ordered, those with higher precedence are farthest from the root of the tree
  NS_IMETHOD RulesMatching(ElementRuleProcessorData* aData,
                           nsIAtom* aMedium) = 0;

  NS_IMETHOD RulesMatching(PseudoRuleProcessorData* aData,
                           nsIAtom* aMedium) = 0;

  // Test if style is dependent on content state
  NS_IMETHOD  HasStateDependentStyle(StateRuleProcessorData* aData,
                                     nsIAtom* aMedium) = 0;

#ifdef DEBUG
  virtual void SizeOf(nsISizeOfHandler *aSizeofHandler, PRUint32 &aSize) = 0;
#endif
};

#endif /* nsIStyleRuleProcessor_h___ */
