/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIStyleRuleProcessor_h___
#define nsIStyleRuleProcessor_h___

#include <stdio.h>

#include "nsISupports.h"
#include "nsPresContext.h" // for nsCompatability
#include "nsILinkHandler.h"
#include "nsString.h"
#include "nsChangeHint.h"

class nsIStyleSheet;
class nsPresContext;
class nsIContent;
class nsIStyledContent;
class nsISupportsArray;
class nsIAtom;
class nsICSSPseudoComparator;
class nsRuleWalker;

// The implementation of the constructor and destructor are currently in
// nsCSSStyleSheet.cpp.

struct RuleProcessorData {
  RuleProcessorData(nsPresContext* aPresContext,
                    nsIContent* aContent, 
                    nsRuleWalker* aRuleWalker,
                    nsCompatibility* aCompat = nsnull);
  
  // NOTE: not |virtual|
  ~RuleProcessorData();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~RuleProcessorData();
    aContext->FreeToShell(sizeof(RuleProcessorData), this);
  };

  const nsString* GetLang();

  nsPresContext*   mPresContext;
  nsIContent*       mContent;       // weak ref
  nsIContent*       mParentContent; // if content, content->GetParent(); weak ref
  nsRuleWalker*     mRuleWalker; // Used to add rules to our results.
  nsIContent*       mScopedRoot;    // Root of scoped stylesheet (set and unset by the supplier of the scoped stylesheet
  
  nsIAtom*          mContentTag;    // if content, then content->GetTag()
  nsIAtom*          mContentID;     // if styled content, then styledcontent->GetID()
  nsIStyledContent* mStyledContent; // if content, content->QI(nsIStyledContent)
  PRPackedBool      mIsHTMLContent; // if content, then does QI on HTMLContent, true or false
  PRPackedBool      mIsHTMLLink;    // if content, calls nsStyleUtil::IsHTMLLink
  PRPackedBool      mIsSimpleXLink; // if content, calls nsStyleUtil::IsSimpleXLink
  nsCompatibility   mCompatMode;    // Possibly remove use of this in SelectorMatches?
  PRPackedBool      mHasAttributes; // if content, content->GetAttrCount() > 0
  PRPackedBool      mIsChecked;     // checked/selected attribute for option and select elements
  nsLinkState       mLinkState;     // if a link, this is the state, otherwise unknown
  PRInt32           mEventState;    // if content, eventStateMgr->GetContentState()
  PRInt32           mNameSpaceID;   // if content, content->GetNameSapce()
  RuleProcessorData* mPreviousSiblingData;
  RuleProcessorData* mParentData;

protected:
  nsAutoString *mLanguage; // NULL means we haven't found out the language yet
};

struct ElementRuleProcessorData : public RuleProcessorData {
  ElementRuleProcessorData(nsPresContext* aPresContext,
                           nsIContent* aContent, 
                           nsRuleWalker* aRuleWalker)
  : RuleProcessorData(aPresContext,aContent,aRuleWalker)
  {
    NS_PRECONDITION(aContent, "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
  }
};

struct PseudoRuleProcessorData : public RuleProcessorData {
  PseudoRuleProcessorData(nsPresContext* aPresContext,
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
  StateRuleProcessorData(nsPresContext* aPresContext,
                         nsIContent* aContent,
                         PRInt32 aStateMask)
    : RuleProcessorData(aPresContext, aContent, nsnull),
      mStateMask(aStateMask)
  {
    NS_PRECONDITION(aContent, "null pointer");
  }
  const PRInt32 mStateMask; // |HasStateDependentStyle| for which state(s)?
                            //  Constants defined in nsIEventStateManager.h .
};

struct AttributeRuleProcessorData : public RuleProcessorData {
  AttributeRuleProcessorData(nsPresContext* aPresContext,
                         nsIContent* aContent,
                         nsIAtom* aAttribute,
                         PRInt32 aModType)
    : RuleProcessorData(aPresContext, aContent, nsnull),
      mAttribute(aAttribute),
      mModType(aModType)
  {
    NS_PRECONDITION(aContent, "null pointer");
  }
  nsIAtom* mAttribute; // |HasAttributeDependentStyle| for which attribute?
  PRInt32 mModType;    // The type of modification (see nsIDOMMutationEvent).
};


// IID for the nsIStyleRuleProcessor interface {015575fe-7b6c-11d3-ba05-001083023c2b}
#define NS_ISTYLE_RULE_PROCESSOR_IID     \
{0x015575fe, 0x7b6c, 0x11d3, {0xba, 0x05, 0x00, 0x10, 0x83, 0x02, 0x3c, 0x2b}}

/* The style rule processor interface is a mechanism to separate the matching
 * of style rules from style sheet instances.
 * Simple style sheets can and will act as their own processor. 
 * Sheets where rule ordering interlaces between multiple sheets, will need to 
 * share a single rule processor between them (CSS sheets do this for cascading order)
 */
class nsIStyleRuleProcessor : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTYLE_RULE_PROCESSOR_IID)

  // Shorthand for:
  //  nsCOMArray<nsIStyleRuleProcessor>::nsCOMArrayEnumFunc
  typedef PRBool (* PR_CALLBACK EnumFunc)(nsIStyleRuleProcessor*, void*);

  // populate rule node tree with nsIStyleRule*
  // rules are ordered, those with higher precedence are farthest from the root of the tree
  NS_IMETHOD RulesMatching(ElementRuleProcessorData* aData,
                           nsIAtom* aMedium) = 0;

  NS_IMETHOD RulesMatching(PseudoRuleProcessorData* aData,
                           nsIAtom* aMedium) = 0;

  /**
   * Test whether style is dependent on content state.  This test is
   * used for optimization only, and may err on the side of reporting
   * more dependencies than really exist.
   *
   * Event states are defined in nsIEventStateManager.h.
   */
  NS_IMETHOD HasStateDependentStyle(StateRuleProcessorData* aData,
                                    nsIAtom* aMedium,
                                    nsReStyleHint* aResult) = 0;

  /**
   * Test whether style is dependent the presence or value of an
   * attribute.  This test is used for optimization only, and may err on
   * the side of reporting more dependencies than really exist.
   */
  NS_IMETHOD HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                        nsIAtom* aMedium,
                                        nsReStyleHint* aResult) = 0;
};

#endif /* nsIStyleRuleProcessor_h___ */
