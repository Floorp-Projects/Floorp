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

#include "nsPresContext.h" // for nsCompatability
#include "nsString.h"
#include "nsChangeHint.h"
#include "nsIContent.h"
#include "nsCSSPseudoElements.h"
#include "nsRuleWalker.h"

class nsIStyleSheet;
class nsIAtom;
class nsICSSPseudoComparator;
class nsAttrValue;

// The implementation of the constructor and destructor are currently in
// nsCSSRuleProcessor.cpp.

struct RuleProcessorData {
  RuleProcessorData(nsPresContext* aPresContext,
                    mozilla::dom::Element* aElement, 
                    nsRuleWalker* aRuleWalker,
                    nsCompatibility* aCompat = nsnull);
  
  // NOTE: not |virtual|
  ~RuleProcessorData();

  // This should be used for all heap-allocation of RuleProcessorData
  static RuleProcessorData* Create(nsPresContext* aPresContext,
                                   mozilla::dom::Element* aElement, 
                                   nsRuleWalker* aRuleWalker,
                                   nsCompatibility aCompat)
  {
    if (NS_LIKELY(aPresContext)) {
      return new (aPresContext) RuleProcessorData(aPresContext, aElement,
                                                  aRuleWalker, &aCompat);
    }

    return new RuleProcessorData(aPresContext, aElement, aRuleWalker,
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
  nsEventStates ContentState();
  nsEventStates DocumentState();
  PRBool IsLink();

  nsEventStates GetContentStateForVisitedHandling(
                  nsRuleWalker::VisitedHandlingType aVisitedHandling,
                  PRBool aIsRelevantLink);

  // Returns a 1-based index of the child in its parent.  If the child
  // is not in its parent's child list (i.e., it is anonymous content),
  // returns 0.
  // If aCheckEdgeOnly is true, the function will return 1 if the result
  // is 1, and something other than 1 (maybe or maybe not a valid
  // result) otherwise.
  PRInt32 GetNthIndex(PRBool aIsOfType, PRBool aIsFromEnd,
                      PRBool aCheckEdgeOnly);

  nsPresContext*    mPresContext;
  mozilla::dom::Element* mElement;       // weak ref, must not be null
  nsIContent*       mParentContent; // mElement->GetParent(); weak ref
  nsRuleWalker*     mRuleWalker; // Used to add rules to our results.
  nsIContent*       mScopedRoot;    // Root of scoped stylesheet (set and unset by the supplier of the scoped stylesheet
  
  nsIAtom*          mContentTag;    // mElement->GetTag()
  nsIAtom*          mContentID;     // mElement->GetID()
  PRPackedBool      mIsHTMLContent; // whether mElement is IsHTML()
  PRPackedBool      mIsHTML;        // mIsHTMLContent && IsInHTMLDocument()
  PRPackedBool      mHasAttributes; // mElement->GetAttrCount() > 0
  nsCompatibility   mCompatMode;    // Possibly remove use of this in SelectorMatches?
  PRInt32           mNameSpaceID;   // mElement->GetNameSapce()
  const nsAttrValue* mClasses;      // mElement->GetClasses()
  // mPreviousSiblingData and mParentData are always RuleProcessorData
  // and never a derived class.  They are allocated lazily, when
  // selectors require matching of prior siblings or ancestors.
  RuleProcessorData* mPreviousSiblingData;
  RuleProcessorData* mParentData;

private:
  nsString *mLanguage; // NULL means we haven't found out the language yet

  // This node's index for :nth-child(), :nth-last-child(),
  // :nth-of-type(), :nth-last-of-type().  If -2, needs to be computed.
  // If -1, needs to be computed but known not to be 1.
  // If 0, the node is not at any index in its parent.
  // The first subscript is 0 for -child and 1 for -of-type, the second
  // subscript is 0 for nth- and 1 for nth-last-.
  PRInt32 mNthIndices[2][2];

  // mContentState is initialized lazily.
  nsEventStates mContentState;  // eventStateMgr->GetContentState() or
                                // mElement->IntrinsicState() if we have no ESM
                                // adjusted for not supporting :visited (but
                                // with visitedness information when we support
                                // it).
  PRPackedBool mGotContentState;
};

struct ElementRuleProcessorData : public RuleProcessorData {
  ElementRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aElement, 
                           nsRuleWalker* aRuleWalker)
  : RuleProcessorData(aPresContext, aElement, aRuleWalker)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
  }
};

struct PseudoElementRuleProcessorData : public RuleProcessorData {
  PseudoElementRuleProcessorData(nsPresContext* aPresContext,
                                 mozilla::dom::Element* aParentElement,
                                 nsRuleWalker* aRuleWalker,
                                 nsCSSPseudoElements::Type aPseudoType)
    : RuleProcessorData(aPresContext, aParentElement, aRuleWalker),
      mPseudoType(aPseudoType)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aPseudoType <
                      nsCSSPseudoElements::ePseudo_PseudoElementCount,
                    "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
  }

  nsCSSPseudoElements::Type mPseudoType;
};

struct AnonBoxRuleProcessorData {
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
struct XULTreeRuleProcessorData : public RuleProcessorData {
  XULTreeRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aParentElement,
                           nsRuleWalker* aRuleWalker,
                           nsIAtom* aPseudoTag,
                           nsICSSPseudoComparator* aComparator)
    : RuleProcessorData(aPresContext, aParentElement, aRuleWalker),
      mPseudoTag(aPseudoTag),
      mComparator(aComparator)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
    NS_PRECONDITION(aPseudoTag, "null pointer");
    NS_PRECONDITION(aRuleWalker, "null pointer");
    NS_PRECONDITION(aComparator, "must have a comparator");
  }

  nsIAtom*                 mPseudoTag;
  nsICSSPseudoComparator*  mComparator;
};
#endif

struct StateRuleProcessorData : public RuleProcessorData {
  StateRuleProcessorData(nsPresContext* aPresContext,
                         mozilla::dom::Element* aElement,
                         nsEventStates aStateMask)
    : RuleProcessorData(aPresContext, aElement, nsnull),
      mStateMask(aStateMask)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
  }
  const nsEventStates mStateMask; // |HasStateDependentStyle| for which state(s)?
                                  //  Constants defined in nsEventStates.h .
};

struct AttributeRuleProcessorData : public RuleProcessorData {
  AttributeRuleProcessorData(nsPresContext* aPresContext,
                             mozilla::dom::Element* aElement,
                             nsIAtom* aAttribute,
                             PRInt32 aModType,
                             PRBool aAttrHasChanged)
    : RuleProcessorData(aPresContext, aElement, nsnull),
      mAttribute(aAttribute),
      mModType(aModType),
      mAttrHasChanged(aAttrHasChanged)
  {
    NS_PRECONDITION(aPresContext, "null pointer");
  }
  nsIAtom* mAttribute; // |HasAttributeDependentStyle| for which attribute?
  PRInt32 mModType;    // The type of modification (see nsIDOMMutationEvent).
  PRBool mAttrHasChanged; // Whether the attribute has already changed.
};

#endif /* !defined(nsRuleProcessorData_h_) */
