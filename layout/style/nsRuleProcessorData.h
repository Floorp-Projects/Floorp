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
#include "mozilla/AutoRestore.h"
#include "mozilla/BloomFilter.h"
#include "mozilla/GuardObjects.h"

class nsIStyleSheet;
class nsIAtom;
class nsICSSPseudoComparator;
class nsAttrValue;
struct TreeMatchContext;

/**
 * An AncestorFilter is used to keep track of ancestors so that we can
 * quickly tell that a particular selector is not relevant to a given
 * element.
 */
class NS_STACK_CLASS AncestorFilter {
  friend struct TreeMatchContext;
 public:
  /* Maintenance of our ancestor state */
  void PushAncestor(mozilla::dom::Element *aElement);
  void PopAncestor();

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
  nsTArray<uint32_t> mPopTargets;

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

  void AddScopeElement(mozilla::dom::Element* aElement) {
    NS_PRECONDITION(mHaveSpecifiedScope,
                    "Should be set before calling AddScopeElement()");
    mScopes.AppendElement(aElement);
  }
  bool IsScopeElement(mozilla::dom::Element* aElement) const {
    return mScopes.Contains(aElement);
  }
  void SetHasSpecifiedScope() {
    mHaveSpecifiedScope = true;
  }
  bool HasSpecifiedScope() const {
    return mHaveSpecifiedScope;
  }

  /**
   * Initialize the ancestor filter and list of style scopes.  If aElement is
   * not null, it and all its ancestors will be passed to
   * mAncestorFilter.PushAncestor and PushStyleScope, starting from the root and
   * going down the tree.
   */
  void InitAncestors(mozilla::dom::Element *aElement);

  void PushStyleScope(mozilla::dom::Element* aElement)
  {
    NS_PRECONDITION(aElement, "aElement must not be null");
    if (aElement->IsScopedStyleRoot()) {
      mStyleScopes.AppendElement(aElement);
    }
  }

  void PopStyleScope(mozilla::dom::Element* aElement)
  {
    NS_PRECONDITION(aElement, "aElement must not be null");
    if (mStyleScopes.SafeLastElement(nullptr) == aElement) {
      mStyleScopes.TruncateLength(mStyleScopes.Length() - 1);
    }
  }
 
  bool PopStyleScopeForSelectorMatching(mozilla::dom::Element* aElement)
  {
    NS_ASSERTION(mForScopedStyle, "only call PopStyleScopeForSelectorMatching "
                                  "when mForScopedStyle is true");

    if (!mCurrentStyleScope) {
      return false;
    }
    if (mCurrentStyleScope == aElement) {
      mCurrentStyleScope = nullptr;
    }
    return true;
  }

  bool SetStyleScopeForSelectorMatching(mozilla::dom::Element* aSubject,
                                        mozilla::dom::Element* aScope)
  {
    mForScopedStyle = !!aScope;
    if (!aScope) {
      // This is not for a scoped style sheet; return true, as we want
      // selector matching to proceed.
      mCurrentStyleScope = nullptr;
      return true;
    }
    if (aScope == aSubject) {
      // Although the subject is the same element as the scope, as soon
      // as we continue with selector matching up the tree we don't want
      // to match any more elements.  So we return true to indicate that
      // we want to do the initial selector matching, but set
      // mCurrentStyleScope to null so that no ancestor elements will match.
      mCurrentStyleScope = nullptr;
      return true;
    }
    if (mStyleScopes.Contains(aScope)) {
      // mStyleScopes contains all of the scope elements that are ancestors of
      // aSubject, so if aScope is in mStyleScopes, then we do want selector
      // matching to proceed.
      mCurrentStyleScope = aScope;
      return true;
    }
    // Otherwise, we're not in the scope, and we don't want to proceed
    // with selector matching.
    mCurrentStyleScope = nullptr;
    return false;
  }

  bool IsWithinStyleScopeForSelectorMatching() const
  {
    NS_ASSERTION(mForScopedStyle, "only call IsWithinScopeForSelectorMatching "
                                  "when mForScopedStyle is true");
    return mCurrentStyleScope;
  }

  /* Helper class for maintaining the ancestor state */
  class NS_STACK_CLASS AutoAncestorPusher {
  public:
    AutoAncestorPusher(bool aDoPush,
                       TreeMatchContext &aTreeMatchContext,
                       mozilla::dom::Element *aElement
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mPushed(aDoPush && aElement),
        mTreeMatchContext(aTreeMatchContext),
        mElement(aElement)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      if (mPushed) {
        mTreeMatchContext.mAncestorFilter.PushAncestor(aElement);
        mTreeMatchContext.PushStyleScope(aElement);
      }
    }
    ~AutoAncestorPusher() {
      if (mPushed) {
        mTreeMatchContext.mAncestorFilter.PopAncestor();
        mTreeMatchContext.PopStyleScope(mElement);
      }
    }

  private:
    bool mPushed;
    TreeMatchContext& mTreeMatchContext;
    mozilla::dom::Element* mElement;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /* Helper class for tracking whether we're skipping the ApplyStyleFixups
   * code for flex items.
   *
   * The optional second parameter aSkipFlexItemStyleFixup allows this
   * class to be instantiated but only conditionally activated (e.g.
   * in cases where we may or may not want to be skipping flex-item
   * style fixup for a particular chunk of code).
   */
  class NS_STACK_CLASS AutoFlexItemStyleFixupSkipper {
  public:
    AutoFlexItemStyleFixupSkipper(TreeMatchContext& aTreeMatchContext,
                                  bool aSkipFlexItemStyleFixup = true
                                  MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mAutoRestorer(aTreeMatchContext.mSkippingFlexItemStyleFixup)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      if (aSkipFlexItemStyleFixup) {
        aTreeMatchContext.mSkippingFlexItemStyleFixup = true;
      }
    }

  private:
    mozilla::AutoRestore<bool> mAutoRestorer;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

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

  // If true, then our contextual reference element set is specified,
  // and is given by mScopes.
  bool mHaveSpecifiedScope;

  // How matching should be performed.  See the documentation for
  // nsRuleWalker::VisitedHandlingType.
  nsRuleWalker::VisitedHandlingType mVisitedHandling;

  // For matching :scope
  nsAutoTArray<mozilla::dom::Element*, 1> mScopes;
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

  // Whether we're currently skipping the flex item chunk of ApplyStyleFixups
  // when resolving style (e.g. for children of elements that have a mandatory
  // frame-type and can't be flex containers despite having "display:flex").
  bool mSkippingFlexItemStyleFixup;

  // Whether this TreeMatchContext is being used with an nsCSSRuleProcessor
  // for an HTML5 scoped style sheet.
  bool mForScopedStyle;

  enum MatchVisited {
    eNeverMatchVisited,
    eMatchVisitedDefault
  };

  // List of ancestor elements that define a style scope (due to having a
  // <style scoped> child).
  nsAutoTArray<mozilla::dom::Element*, 1> mStyleScopes;

  // The current style scope element for selector matching.
  mozilla::dom::Element* mCurrentStyleScope;

  // Constructor to use when creating a tree match context for styling
  TreeMatchContext(bool aForStyling,
                   nsRuleWalker::VisitedHandlingType aVisitedHandling,
                   nsIDocument* aDocument,
                   MatchVisited aMatchVisited = eMatchVisitedDefault)
    : mForStyling(aForStyling)
    , mHaveRelevantLink(false)
    , mHaveSpecifiedScope(false)
    , mVisitedHandling(aVisitedHandling)
    , mDocument(aDocument)
    , mScopedRoot(nullptr)
    , mIsHTMLDocument(aDocument->IsHTML())
    , mCompatMode(aDocument->GetCompatibilityMode())
    , mUsingPrivateBrowsing(false)
    , mSkippingFlexItemStyleFixup(false)
    , mForScopedStyle(false)
    , mCurrentStyleScope(nullptr)
  {
    if (aMatchVisited != eNeverMatchVisited) {
      nsCOMPtr<nsISupports> container = mDocument->GetContainer();
      if (container) {
        nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(container);
        NS_ASSERTION(loadContext, "Couldn't get loadContext from container; assuming no private browsing.");
        if (loadContext) {
          mUsingPrivateBrowsing = loadContext->UsePrivateBrowsing();
        }
      }
    }
  }
};

struct NS_STACK_CLASS RuleProcessorData {
  RuleProcessorData(nsPresContext* aPresContext,
                    nsRuleWalker* aRuleWalker)
    : mPresContext(aPresContext),
      mRuleWalker(aRuleWalker),
      mScope(nullptr)
  {
    NS_PRECONDITION(mPresContext, "Must have prescontext");
  }

  nsPresContext* const mPresContext;
  nsRuleWalker* const mRuleWalker; // Used to add rules to our results.
  mozilla::dom::Element* mScope;
};

struct NS_STACK_CLASS ElementDependentRuleProcessorData :
                          public RuleProcessorData {
  ElementDependentRuleProcessorData(nsPresContext* aPresContext,
                                    mozilla::dom::Element* aElement,
                                    nsRuleWalker* aRuleWalker,
                                    TreeMatchContext& aTreeMatchContext)
    : RuleProcessorData(aPresContext, aRuleWalker)
    , mElement(aElement)
    , mTreeMatchContext(aTreeMatchContext)
  {
    NS_ASSERTION(aElement, "null element leaked into SelectorMatches");
    NS_ASSERTION(aElement->OwnerDoc(), "Document-less node here?");
    NS_PRECONDITION(aTreeMatchContext.mForStyling == !!aRuleWalker,
                    "Should be styling if and only if we have a rule walker");
  }
  
  mozilla::dom::Element* const mElement; // weak ref, must not be null
  TreeMatchContext& mTreeMatchContext;
};

struct NS_STACK_CLASS ElementRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  ElementRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aElement, 
                           nsRuleWalker* aRuleWalker,
                           TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aElement, aRuleWalker,
                                        aTreeMatchContext)
  {
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
  }
};

struct NS_STACK_CLASS PseudoElementRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  PseudoElementRuleProcessorData(nsPresContext* aPresContext,
                                 mozilla::dom::Element* aParentElement,
                                 nsRuleWalker* aRuleWalker,
                                 nsCSSPseudoElements::Type aPseudoType,
                                 TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aParentElement, aRuleWalker,
                                        aTreeMatchContext),
      mPseudoType(aPseudoType)
  {
    NS_PRECONDITION(aPseudoType <
                      nsCSSPseudoElements::ePseudo_PseudoElementCount,
                    "invalid aPseudoType value");
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
  }

  nsCSSPseudoElements::Type mPseudoType;
};

struct NS_STACK_CLASS AnonBoxRuleProcessorData : public RuleProcessorData {
  AnonBoxRuleProcessorData(nsPresContext* aPresContext,
                           nsIAtom* aPseudoTag,
                           nsRuleWalker* aRuleWalker)
    : RuleProcessorData(aPresContext, aRuleWalker),
      mPseudoTag(aPseudoTag)
  {
    NS_PRECONDITION(aPseudoTag, "Must have pseudo tag");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
  }

  nsIAtom* mPseudoTag;
};

#ifdef MOZ_XUL
struct NS_STACK_CLASS XULTreeRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  XULTreeRuleProcessorData(nsPresContext* aPresContext,
                           mozilla::dom::Element* aParentElement,
                           nsRuleWalker* aRuleWalker,
                           nsIAtom* aPseudoTag,
                           nsICSSPseudoComparator* aComparator,
                           TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aParentElement,
                                        aRuleWalker, aTreeMatchContext),
      mPseudoTag(aPseudoTag),
      mComparator(aComparator)
  {
    NS_PRECONDITION(aPseudoTag, "null pointer");
    NS_PRECONDITION(aRuleWalker, "Must have rule walker");
    NS_PRECONDITION(aComparator, "must have a comparator");
    NS_PRECONDITION(aTreeMatchContext.mForStyling, "Styling here!");
  }

  nsIAtom*                 mPseudoTag;
  nsICSSPseudoComparator*  mComparator;
};
#endif

struct NS_STACK_CLASS StateRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  StateRuleProcessorData(nsPresContext* aPresContext,
                         mozilla::dom::Element* aElement,
                         nsEventStates aStateMask,
                         TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aElement, nullptr,
                                        aTreeMatchContext),
      mStateMask(aStateMask)
  {
    NS_PRECONDITION(!aTreeMatchContext.mForStyling, "Not styling here!");
  }
  const nsEventStates mStateMask; // |HasStateDependentStyle| for which state(s)?
                                  //  Constants defined in nsEventStates.h .
};

struct NS_STACK_CLASS AttributeRuleProcessorData :
                          public ElementDependentRuleProcessorData {
  AttributeRuleProcessorData(nsPresContext* aPresContext,
                             mozilla::dom::Element* aElement,
                             nsIAtom* aAttribute,
                             int32_t aModType,
                             bool aAttrHasChanged,
                             TreeMatchContext& aTreeMatchContext)
    : ElementDependentRuleProcessorData(aPresContext, aElement, nullptr,
                                        aTreeMatchContext),
      mAttribute(aAttribute),
      mModType(aModType),
      mAttrHasChanged(aAttrHasChanged)
  {
    NS_PRECONDITION(!aTreeMatchContext.mForStyling, "Not styling here!");
  }
  nsIAtom* mAttribute; // |HasAttributeDependentStyle| for which attribute?
  int32_t mModType;    // The type of modification (see nsIDOMMutationEvent).
  bool mAttrHasChanged; // Whether the attribute has already changed.
};

#endif /* !defined(nsRuleProcessorData_h_) */
