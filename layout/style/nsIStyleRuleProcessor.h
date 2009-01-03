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
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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

/*
 * internal abstract interface for containers (roughly origins within
 * the CSS cascade) that provide style rules matching an element or
 * pseudo-element
 */

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
class nsIAtom;
class nsICSSPseudoComparator;
class nsRuleWalker;
class nsAttrValue;

// The implementation of the constructor and destructor are currently in
// nsCSSRuleProcessor.cpp.

struct RuleProcessorData {
  RuleProcessorData(nsPresContext* aPresContext,
                    nsIContent* aContent, 
                    nsRuleWalker* aRuleWalker,
                    nsCompatibility* aCompat = nsnull);
  
  // NOTE: not |virtual|
  ~RuleProcessorData();

  // This should be used for all heap-allocation of RuleProcessorData
  static RuleProcessorData* Create(nsPresContext* aPresContext,
                                   nsIContent* aContent, 
                                   nsRuleWalker* aRuleWalker,
                                   nsCompatibility aCompat)
  {
    if (NS_LIKELY(aPresContext)) {
      return new (aPresContext) RuleProcessorData(aPresContext, aContent,
                                                  aRuleWalker, &aCompat);
    }

    return new RuleProcessorData(aPresContext, aContent, aRuleWalker,
                                 &aCompat);
  }
  
  void Destroy() {
    nsPresContext * pc = mPresContext;
    if (NS_LIKELY(pc)) {
      this->~RuleProcessorData();
      pc->FreeToShell(sizeof(RuleProcessorData), this);
      return;
    }
    delete this;
  }

  // For placement new
  void* operator new(size_t sz, RuleProcessorData* aSlot) CPP_THROW_NEW {
    return aSlot;
  }
private:
  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void* operator new(size_t sz) CPP_THROW_NEW {
    return ::operator new(sz);
  }
public:
  const nsString* GetLang();

  // Returns a 1-based index of the child in its parent.  If the child
  // is not in its parent's child list (i.e., it is anonymous content),
  // returns 0.
  // If aCheckEdgeOnly is true, the function will return 1 if the result
  // is 1, and something other than 1 (maybe or maybe not a valid
  // result) otherwise.
  PRInt32 GetNthIndex(PRBool aIsOfType, PRBool aIsFromEnd,
                      PRBool aCheckEdgeOnly);

  nsPresContext*    mPresContext;
  nsIContent*       mContent;       // weak ref
  nsIContent*       mParentContent; // if content, content->GetParent(); weak ref
  nsRuleWalker*     mRuleWalker; // Used to add rules to our results.
  nsIContent*       mScopedRoot;    // Root of scoped stylesheet (set and unset by the supplier of the scoped stylesheet
  
  nsIAtom*          mContentTag;    // if content, then content->GetTag()
  nsIAtom*          mContentID;     // if styled content, then weak reference to styledcontent->GetID()
  PRPackedBool      mIsHTMLContent; // if content, then does QI on HTMLContent, true or false
  PRPackedBool      mIsLink;        // if content, calls nsStyleUtil::IsHTMLLink or nsStyleUtil::IsLink
  PRPackedBool      mHasAttributes; // if content, content->GetAttrCount() > 0
  nsCompatibility   mCompatMode;    // Possibly remove use of this in SelectorMatches?
  nsLinkState       mLinkState;     // if a link, this is the state, otherwise unknown
  PRInt32           mEventState;    // if content, eventStateMgr->GetContentState()
  PRInt32           mNameSpaceID;   // if content, content->GetNameSapce()
  const nsAttrValue* mClasses;      // if styled content, styledcontent->GetClasses()
  // mPreviousSiblingData and mParentData are always RuleProcessorData
  // and never a derived class.  They are allocated lazily, when
  // selectors require matching of prior siblings or ancestors.
  RuleProcessorData* mPreviousSiblingData;
  RuleProcessorData* mParentData;

protected:
  nsString *mLanguage; // NULL means we haven't found out the language yet

  // This node's index for :nth-child(), :nth-last-child(),
  // :nth-of-type(), :nth-last-of-type().  If -2, needs to be computed.
  // If -1, needs to be computed but known not to be 1.
  // If 0, the node is not at any index in its parent.
  // The first subscript is 0 for -child and 1 for -of-type, the second
  // subscript is 0 for nth- and 1 for nth-last-.
  PRInt32 mNthIndices[2][2];
};

struct ElementRuleProcessorData : public RuleProcessorData {
  ElementRuleProcessorData(nsPresContext* aPresContext,
                           nsIContent* aContent, 
                           nsRuleWalker* aRuleWalker)
  : RuleProcessorData(aPresContext,aContent,aRuleWalker)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
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
    NS_PRECONDITION(aPresContext, "null pointer");
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
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aContent, "null pointer");
  }
  const PRInt32 mStateMask; // |HasStateDependentStyle| for which state(s)?
                            //  Constants defined in nsIEventStateManager.h .
};

struct AttributeRuleProcessorData : public RuleProcessorData {
  AttributeRuleProcessorData(nsPresContext* aPresContext,
                             nsIContent* aContent,
                             nsIAtom* aAttribute,
                             PRInt32 aModType,
                             PRUint32 aStateMask)
    : RuleProcessorData(aPresContext, aContent, nsnull),
      mAttribute(aAttribute),
      mModType(aModType),
      mStateMask(aStateMask)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aContent, "null pointer");
  }
  nsIAtom* mAttribute; // |HasAttributeDependentStyle| for which attribute?
  PRInt32 mModType;    // The type of modification (see nsIDOMMutationEvent).
  PRUint32 mStateMask; // The states that changed with the attr change.
};


// IID for the nsIStyleRuleProcessor interface {015575fe-7b6c-11d3-ba05-001083023c2b}
#define NS_ISTYLE_RULE_PROCESSOR_IID     \
{0x015575fe, 0x7b6c, 0x11d3, {0xba, 0x05, 0x00, 0x10, 0x83, 0x02, 0x3c, 0x2b}}

/* The style rule processor interface is a mechanism to separate the matching
 * of style rules from style sheet instances.
 * Simple style sheets can and will act as their own processor. 
 * Sheets where rule ordering interlaces between multiple sheets, will need to 
 * share a single rule processor between them (CSS sheets do this for cascading order)
 *
 * @see nsIStyleRule (for significantly more detailed comments)
 */
class nsIStyleRuleProcessor : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTYLE_RULE_PROCESSOR_IID)

  // Shorthand for:
  //  nsCOMArray<nsIStyleRuleProcessor>::nsCOMArrayEnumFunc
  typedef PRBool (* EnumFunc)(nsIStyleRuleProcessor*, void*);

  /**
   * Find the |nsIStyleRule|s matching the given content node and
   * position the given |nsRuleWalker| at the |nsRuleNode| in the rule
   * tree representing that ordered list of rules (with higher
   * precedence being farther from the root of the lexicographic tree).
   */
  NS_IMETHOD RulesMatching(ElementRuleProcessorData* aData) = 0;

  /**
   * Just like the previous |RulesMatching|, except for a given content
   * node <em>and pseudo-element</em>.
   */
  NS_IMETHOD RulesMatching(PseudoRuleProcessorData* aData) = 0;

  /**
   * Return how (as described by nsReStyleHint) style can depend on a
   * change of the given content state on the given content node.  This
   * test is used for optimization only, and may err on the side of
   * reporting more dependencies than really exist.
   *
   * Event states are defined in nsIEventStateManager.h.
   */
  NS_IMETHOD HasStateDependentStyle(StateRuleProcessorData* aData,
                                    nsReStyleHint* aResult) = 0;

  /**
   * Return how (as described by nsReStyleHint) style can depend on the
   * presence or value of the given attribute for the given content
   * node.  This test is used for optimization only, and may err on the
   * side of reporting more dependencies than really exist.
   */
  NS_IMETHOD HasAttributeDependentStyle(AttributeRuleProcessorData* aData,
                                        nsReStyleHint* aResult) = 0;

  /**
   * Do any processing that needs to happen as a result of a change in
   * the characteristics of the medium, and return whether this rule
   * processor's rules have changed (e.g., because of media queries).
   */
  NS_IMETHOD MediumFeaturesChanged(nsPresContext* aPresContext,
                                   PRBool* aRulesChanged) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStyleRuleProcessor,
                              NS_ISTYLE_RULE_PROCESSOR_IID)

#endif /* nsIStyleRuleProcessor_h___ */
