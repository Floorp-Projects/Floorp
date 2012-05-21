/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsILoadContext.h"
#include "mozilla/BloomFilter.h"
#include "mozilla/GuardObjects.h"

class nsIStyleSheet;
class nsIAtom;
class nsICSSPseudoComparator;
class nsAttrValue;

/**
 * An AncestorFilter is used to keep track of ancestors so that we can
 * quickly tell that a particular selector is not relevant to a given
 * element.
 */
class NS_STACK_CLASS AncestorFilter {
 public:
  /**
   * Initialize the filter.  If aElement is not null, it and all its
   * ancestors will be passed to PushAncestor, starting from the root
   * and going down the tree.
   */
  void Init(mozilla::dom::Element *aElement);

  /* Maintenance of our ancestor state */
  void PushAncestor(mozilla::dom::Element *aElement);
  void PopAncestor();

  /* Helper class for maintaining the ancestor state */
  class NS_STACK_CLASS AutoAncestorPusher {
  public:
    AutoAncestorPusher(bool aDoPush,
                       AncestorFilter &aFilter,
                       mozilla::dom::Element *aElement
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mPushed(aDoPush && aElement), mFilter(aFilter)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      if (mPushed) {
        mFilter.PushAncestor(aElement);
      }
    }
    ~AutoAncestorPusher() {
      if (mPushed) {
        mFilter.PopAncestor();
      }
    }

  private:
    bool mPushed;
    AncestorFilter &mFilter;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /* Check whether we might have an ancestor matching one of the given
     atom hashes.  |hashes| must have length hashListLength */
  template<size_t hashListLength>
    bool MightHaveMatchingAncestor(const uint32_t* aHashes) const
  {
    MOZ_ASSERT(mFilter);
    for (size_t i = 0; i < hashListLength && aHashes[i]; ++i) {
      if (!mFilter->mightContain(aHashes[i])) {
        return false;
      }
    }

    return true;
  }

  bool HasFilter() const { return mFilter; }

#ifdef DEBUG
  void AssertHasAllAncestors(mozilla::dom::Element *aElement) const;
#endif
  
 private:
  // Using 2^12 slots makes the Bloom filter a nice round page in
  // size, so let's do that.  We get a false positive rate of 1% or
  // less even with several hundred things in the filter.  Note that
  // we allocate the filter lazily, because not all tree match
  // contexts can use one effectively.
  typedef mozilla::BloomFilter<12, nsIAtom> Filter;
  nsAutoPtr<Filter> mFilter;

  // Stack of indices to pop to.  These are indices into mHashes.
  nsTArray<PRUint32> mPopTargets;

  // List of hashes; this is what we pop using mPopTargets.  We store
  // hashes of our ancestor element tag names, ids, and classes in
  // here.
  nsTArray<uint32_t> mHashes;

  // A debug-only stack of Elements for use in assertions
#ifdef DEBUG
  nsTArray<mozilla::dom::Element*> mElements;
#endif
};

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
    mHaveRelevantLink = false;
    mVisitedHandling = nsRuleWalker::eRelevantLinkVisited;
  }
  
  void ResetForUnvisitedMatching() {
    NS_PRECONDITION(mForStyling, "Why is this being called?");
    mHaveRelevantLink = false;
    mVisitedHandling = nsRuleWalker::eRelevantLinkUnvisited;
  }

  void SetHaveRelevantLink() { mHaveRelevantLink = true; }
  bool HaveRelevantLink() const { return mHaveRelevantLink; }

  nsRuleWalker::VisitedHandlingType VisitedHandling() const
  {
    return mVisitedHandling;
  }

  // Is this matching operation for the creation of a style context?
  // (If it is, we need to set slow selector bits on nodes indicating
  // that certain restyling needs to happen.)
  const bool mForStyling;

 private:
  // When mVisitedHandling is eRelevantLinkUnvisited, this is set to true if a
  // relevant link (see explanation in definition of VisitedHandling enum) was
  // encountered during the matching process, which means that matching needs
  // to be rerun with eRelevantLinkVisited.  Otherwise, its behavior is
  // undefined (it might get set appropriately, or might not).
  bool mHaveRelevantLink;

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
  const bool mIsHTMLDocument;

  // Possibly remove use of mCompatMode in SelectorMatches?
  // XXX XBL2 issue: Should we be caching this?  What should it be for XBL2?
  const nsCompatibility mCompatMode;

  // The nth-index cache we should use
  nsNthIndexCache mNthIndexCache;

  // An ancestor filter
  AncestorFilter mAncestorFilter;

  // Whether this document is using PB mode
  bool mUsingPrivateBrowsing;

  // Constructor to use when creating a tree match context for styling
  TreeMatchContext(bool aForStyling,
                   nsRuleWalker::VisitedHandlingType aVisitedHandling,
                   nsIDocument* aDocument)
    : mForStyling(aForStyling)
    , mHaveRelevantLink(false)
    , mVisitedHandling(aVisitedHandling)
    , mDocument(aDocument)
    , mScopedRoot(nsnull)
    , mIsHTMLDocument(aDocument->IsHTML())
    , mCompatMode(aDocument->GetCompatibilityMode())
    , mUsingPrivateBrowsing(false)
  {
    nsCOMPtr<nsISupports> container = mDocument->GetContainer();
    if (container) {
      nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(container);
      NS_ASSERTION(loadContext, "Couldn't get loadContext from container; assuming no private browsing.");
      if (loadContext) {
        mUsingPrivateBrowsing = loadContext->UsePrivateBrowsing();
      }
    }
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
    NS_ASSERTION(aElement->OwnerDoc(), "Document-less node here?");
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
                             bool aAttrHasChanged,
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
  bool mAttrHasChanged; // Whether the attribute has already changed.
};

#endif /* !defined(nsRuleProcessorData_h_) */
