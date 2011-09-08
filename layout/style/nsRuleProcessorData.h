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
 * data structures passed to nsIStyleRuleProcessor methods (to pull loop
 * invariant computations out of the loop)
 */

#ifndef nsRuleProcessorData_h_
#define nsRuleProcessorData_h_

#include "nsPresContext.h" // for nsCompatibility
#include "nsString.h"
#include "nsChangeHint.h"
#include "nsIContent.h"
#include "nsCSSPseudoElements.h"
#include "nsRuleWalker.h"
#include "nsNthIndexCache.h"

class nsIStyleSheet;
class nsIAtom;
class nsICSSPseudoComparator;
class nsAttrValue;

/**
 * A |TreeMatchContext| has data about a matching operation.  The
 * data are not node-specific but are invariants of the DOM tree the
 * nodes being matched against are in.
 *
 * Most of the members are in parameters to selector matching.  The
 * one out parameter is mHaveRelevantLink.  Consumers that use a
 * TreeMatchContext for more than one matching operation and care
 * about :visited and mHaveRelevantLink need to
 * ResetForVisitedMatching() and ResetForUnvisitedMatching() as
 * needed.
 */
struct NS_STACK_CLASS TreeMatchContext {
  // Reset this context for matching for the style-if-:visited.
  void ResetForVisitedMatching() {
    NS_PRECONDITION(mForStyling, "Why is this being called?");
    mHaveRelevantLink = PR_FALSE;
    mVisitedHandling = nsRuleWalker::eRelevantLinkVisited;
  }
  
  void ResetForUnvisitedMatching() {
    NS_PRECONDITION(mForStyling, "Why is this being called?");
    mHaveRelevantLink = PR_FALSE;
    mVisitedHandling = nsRuleWalker::eRelevantLinkUnvisited;
  }

  void SetHaveRelevantLink() { mHaveRelevantLink = PR_TRUE; }
  PRBool HaveRelevantLink() const { return mHaveRelevantLink; }

  nsRuleWalker::VisitedHandlingType VisitedHandling() const
  {
    return mVisitedHandling;
  }

  // Is this matching operation for the creation of a style context?
  // (If it is, we need to set slow selector bits on nodes indicating
  // that certain restyling needs to happen.)
  const PRBool mForStyling;

 private:
  // When mVisitedHandling is eRelevantLinkUnvisited, this is set to true if a
  // relevant link (see explanation in definition of VisitedHandling enum) was
  // encountered during the matching process, which means that matching needs
  // to be rerun with eRelevantLinkVisited.  Otherwise, its behavior is
  // undefined (it might get set appropriately, or might not).
  PRBool mHaveRelevantLink;

  // How matching should be performed.  See the documentation for
  // nsRuleWalker::VisitedHandlingType.
  nsRuleWalker::VisitedHandlingType mVisitedHandling;

 public:
  // The document we're working with.
  nsIDocument* const mDocument;

  // Root of scoped stylesheet (set and unset by the supplier of the
  // scoped stylesheet).
  nsIContent* mScopedRoot;

  // Whether our document is HTML (as opposed to XML of some sort,
  // including XHTML).
  // XXX XBL2 issue: Should we be caching this?  What should it be for XBL2?
  const PRPackedBool mIsHTMLDocument;

  // Possibly remove use of mCompatMode in SelectorMatches?
  // XXX XBL2 issue: Should we be caching this?  What should it be for XBL2?
  const nsCompatibility mCompatMode;

  // The nth-index cache we should use
  nsNthIndexCache mNthIndexCache;

  // Constructor to use when creating a tree match context for styling
  TreeMatchContext(PRBool aForStyling,
                   nsRuleWalker::VisitedHandlingType aVisitedHandling,
                   nsIDocument* aDocument)
    : mForStyling(aForStyling)
    , mHaveRelevantLink(PR_FALSE)
    , mVisitedHandling(aVisitedHandling)
    , mDocument(aDocument)
    , mScopedRoot(nsnull)
    , mIsHTMLDocument(aDocument->IsHTML())
    , mCompatMode(aDocument->GetCompatibilityMode())
  {
  }
};

// The implementation of the constructor and destructor are currently in
// nsCSSRuleProcessor.cpp.

struct NS_STACK_CLASS RuleProcessorData  {
  RuleProcessorData(nsPresContext* aPresContext,
                    mozilla::dom::Element* aElement, 
                    nsRuleWalker* aRuleWalker,
                    TreeMatchContext& aTreeMatchContext)
    : mPresContext(aPresContext)
    , mElement(aElement)
    , mRuleWalker(aRuleWalker)
    , mTreeMatchContext(aTreeMatchContext)
  {
    NS_ASSERTION(aElement, "null element leaked into SelectorMatches");
    NS_ASSERTION(aElement->GetOwnerDoc(), "Document-less node here?");
    NS_PRECONDITION(aTreeMatchContext.mForStyling == !!aRuleWalker,
                    "Should be styling if and only if we have a rule walker");
  }
  
  nsPresContext* const mPresContext;
  mozilla::dom::Element* const mElement; // weak ref, must not be null
  nsRuleWalker* const mRuleWalker; // Used to add rules to our results.
  TreeMatchContext& mTreeMatchContext;
};

struct NS_STACK_CLASS ElementRuleProcessorData : public RuleProcessorData {
  ElementRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aElement, 
                           nsRuleWalker* aRuleWalker,
                           TreeMatchContext& aTreeMatchContext)
  : RuleProcessorData(aPresContext, aElement, aRuleWalker, aTreeMatchContext)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
  }
};

struct NS_STACK_CLASS PseudoElementRuleProcessorData : public RuleProcessorData {
  PseudoElementRuleProcessorData(nsPresContext* aPresContext,
                                 mozilla::dom::Element* aParentElement,
                                 nsRuleWalker* aRuleWalker,
                                 nsCSSPseudoElements::Type aPseudoType,
                                 TreeMatchContext& aTreeMatchContext)
    : RuleProcessorData(aPresContext, aParentElement, aRuleWalker,
                        aTreeMatchContext),
      mPseudoType(aPseudoType)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aPseudoType <
                      nsCSSPseudoElements::ePseudo_PseudoElementCount,
                    "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
  }

  nsCSSPseudoElements::Type mPseudoType;
};

struct NS_STACK_CLASS AnonBoxRuleProcessorData {
  AnonBoxRuleProcessorData(nsPresContext* aPresContext,
                           nsIAtom* aPseudoTag,
                           nsRuleWalker* aRuleWalker)
    : mPresContext(aPresContext),
      mPseudoTag(aPseudoTag),
      mRuleWalker(aRuleWalker)
  {
    NS_PRECONDITION(mPresContext, "Must have prescontext");
    NS_PRECONDITION(aPseudoTag, "Must have pseudo tag");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
  }

  nsPresContext* mPresContext;
  nsIAtom* mPseudoTag;
  nsRuleWalker* mRuleWalker;
};

#ifdef MOZ_XUL
struct NS_STACK_CLASS XULTreeRuleProcessorData : public RuleProcessorData {
  XULTreeRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aParentElement,
                           nsRuleWalker* aRuleWalker,
                           nsIAtom* aPseudoTag,
                           nsICSSPseudoComparator* aComparator,
                           TreeMatchContext& aTreeMatchContext)
    : RuleProcessorData(aPresContext, aParentElement, aRuleWalker,
                        aTreeMatchContext),
      mPseudoTag(aPseudoTag),
      mComparator(aComparator)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aPseudoTag, "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
    NS_PRECONDITION(aComparator, "must have a comparator");
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
  }

  nsIAtom*                 mPseudoTag;
  nsICSSPseudoComparator*  mComparator;
};
#endif

struct NS_STACK_CLASS StateRuleProcessorData : public RuleProcessorData {
  StateRuleProcessorData(nsPresContext* aPresContext,
                         mozilla::dom::Element* aElement,
                         nsEventStates aStateMask,
                         TreeMatchContext& aTreeMatchContext)
    : RuleProcessorData(aPresContext, aElement, nsnull, aTreeMatchContext),
      mStateMask(aStateMask)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(!aTreeMatchContext.mForStyling, "Not styling here!");
  }
  const nsEventStates mStateMask; // |HasStateDependentStyle| for which state(s)?
                                  //  Constants defined in nsEventStates.h .
};

struct NS_STACK_CLASS AttributeRuleProcessorData : public RuleProcessorData {
  AttributeRuleProcessorData(nsPresContext* aPresContext,
                             mozilla::dom::Element* aElement,
                             nsIAtom* aAttribute,
                             PRInt32 aModType,
                             PRBool aAttrHasChanged,
                             TreeMatchContext& aTreeMatchContext)
    : RuleProcessorData(aPresContext, aElement, nsnull, aTreeMatchContext),
      mAttribute(aAttribute),
      mModType(aModType),
      mAttrHasChanged(aAttrHasChanged)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(!aTreeMatchContext.mForStyling, "Not styling here!");
  }
  nsIAtom* mAttribute; // |HasAttributeDependentStyle| for which attribute?
  PRInt32 mModType;    // The type of modification (see nsIDOMMutationEvent).
  PRBool mAttrHasChanged; // Whether the attribute has already changed.
};

#endif /* !defined(nsRuleProcessorData_h_) */
