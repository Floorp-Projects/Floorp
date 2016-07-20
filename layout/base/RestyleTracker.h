/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A class which manages pending restyles.  This handles keeping track
 * of what nodes restyles need to happen on and so forth.
 */

#ifndef mozilla_RestyleTracker_h
#define mozilla_RestyleTracker_h

#include "mozilla/dom/Element.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsContainerFrame.h"
#include "mozilla/SplayTree.h"
#include "mozilla/RestyleLogging.h"
#include "GeckoProfiler.h"
#include "mozilla/Maybe.h"

#if defined(MOZ_ENABLE_PROFILER_SPS)
#include "ProfilerBacktrace.h"
#endif

namespace mozilla {

class RestyleManager;
class ElementRestyler;

/** 
 * Helper class that collects a list of frames that need
 * UpdateOverflow() called on them, and coalesces them
 * to avoid walking up the same ancestor tree multiple times.
 */
class OverflowChangedTracker
{
public:
  enum ChangeKind {
    /**
     * The frame was explicitly added as a result of
     * nsChangeHint_UpdatePostTransformOverflow and hence may have had a style
     * change that changes its geometry relative to parent, without reflowing.
     */
    TRANSFORM_CHANGED,
    /**
     * The overflow areas of children have changed
     * and we need to call UpdateOverflow on the frame.
     */
    CHILDREN_CHANGED,
  };

  OverflowChangedTracker() :
    mSubtreeRoot(nullptr)
  {}

  ~OverflowChangedTracker()
  {
    NS_ASSERTION(mEntryList.empty(), "Need to flush before destroying!");
  }

  /**
   * Add a frame that has had a style change, and needs its
   * overflow updated.
   *
   * If there are pre-transform overflow areas stored for this
   * frame, then we will call FinishAndStoreOverflow with those
   * areas instead of UpdateOverflow().
   *
   * If the overflow area changes, then UpdateOverflow will also
   * be called on the parent.
   */
  void AddFrame(nsIFrame* aFrame, ChangeKind aChangeKind) {
    uint32_t depth = aFrame->GetDepthInFrameTree();
    Entry *entry = nullptr;
    if (!mEntryList.empty()) {
      entry = mEntryList.find(Entry(aFrame, depth));
    }
    if (entry == nullptr) {
      // Add new entry.
      mEntryList.insert(new Entry(aFrame, depth, aChangeKind));
    } else {
      // Update the existing entry if the new value is stronger.
      entry->mChangeKind = std::max(entry->mChangeKind, aChangeKind);
    }
  }

  /**
   * Remove a frame.
   */
  void RemoveFrame(nsIFrame* aFrame) {
    if (mEntryList.empty()) {
      return;
    }

    uint32_t depth = aFrame->GetDepthInFrameTree();
    if (mEntryList.find(Entry(aFrame, depth))) {
      delete mEntryList.remove(Entry(aFrame, depth));
    }
  }

  /**
   * Set the subtree root to limit overflow updates. This must be set if and
   * only if currently reflowing aSubtreeRoot, to ensure overflow changes will
   * still propagate correctly.
   */
  void SetSubtreeRoot(const nsIFrame* aSubtreeRoot) {
    mSubtreeRoot = aSubtreeRoot;
  }

  /**
   * Update the overflow of all added frames, and clear the entry list.
   *
   * Start from those deepest in the frame tree and works upwards. This stops 
   * us from processing the same frame twice.
   */
  void Flush() {
    while (!mEntryList.empty()) {
      Entry *entry = mEntryList.removeMin();
      nsIFrame *frame = entry->mFrame;

      bool overflowChanged = false;
      if (entry->mChangeKind == CHILDREN_CHANGED) {
        // Need to union the overflow areas of the children.
        // Only update the parent if the overflow changes.
        overflowChanged = frame->UpdateOverflow();
      } else {
        // Take a faster path that doesn't require unioning the overflow areas
        // of our children.

        NS_ASSERTION(frame->Properties().Get(
                       nsIFrame::DebugInitialOverflowPropertyApplied()),
                     "InitialOverflowProperty must be set first.");

        nsOverflowAreas* overflow = 
          frame->Properties().Get(nsIFrame::InitialOverflowProperty());
        if (overflow) {
          // FinishAndStoreOverflow will change the overflow areas passed in,
          // so make a copy.
          nsOverflowAreas overflowCopy = *overflow;
          frame->FinishAndStoreOverflow(overflowCopy, frame->GetSize());
        } else {
          nsRect bounds(nsPoint(0, 0), frame->GetSize());
          nsOverflowAreas boundsOverflow;
          boundsOverflow.SetAllTo(bounds);
          frame->FinishAndStoreOverflow(boundsOverflow, bounds.Size());
        }

        // We can't tell if the overflow changed, so be conservative
        overflowChanged = true;
      }

      // If the frame style changed (e.g. positioning offsets)
      // then we need to update the parent with the overflow areas of its
      // children.
      if (overflowChanged) {
        nsIFrame *parent = frame->GetParent();
        if (parent && parent != mSubtreeRoot) {
          Entry* parentEntry = mEntryList.find(Entry(parent, entry->mDepth - 1));
          if (parentEntry) {
            parentEntry->mChangeKind = std::max(parentEntry->mChangeKind, CHILDREN_CHANGED);
          } else {
            mEntryList.insert(new Entry(parent, entry->mDepth - 1, CHILDREN_CHANGED));
          }
        }
      }
      delete entry;
    }
  }
  
private:
  struct Entry : SplayTreeNode<Entry>
  {
    Entry(nsIFrame* aFrame, uint32_t aDepth, ChangeKind aChangeKind = CHILDREN_CHANGED)
      : mFrame(aFrame)
      , mDepth(aDepth)
      , mChangeKind(aChangeKind)
    {}

    bool operator==(const Entry& aOther) const
    {
      return mFrame == aOther.mFrame;
    }
 
    /**
     * Sort by *reverse* depth in the tree, and break ties with
     * the frame pointer.
     */
    bool operator<(const Entry& aOther) const
    {
      if (mDepth == aOther.mDepth) {
        return mFrame < aOther.mFrame;
      }
      return mDepth > aOther.mDepth; /* reverse, want "min" to be deepest */
    }

    static int compare(const Entry& aOne, const Entry& aTwo)
    {
      if (aOne == aTwo) {
        return 0;
      } else if (aOne < aTwo) {
        return -1;
      } else {
        return 1;
      }
    }

    nsIFrame* mFrame;
    /* Depth in the frame tree */
    uint32_t mDepth;
    ChangeKind mChangeKind;
  };

  /* A list of frames to process, sorted by their depth in the frame tree */
  SplayTree<Entry, Entry> mEntryList;

  /* Don't update overflow of this frame or its ancestors. */
  const nsIFrame* mSubtreeRoot;
};

class RestyleTracker {
public:
  typedef mozilla::dom::Element Element;

  friend class ElementRestyler; // for AddPendingRestyleToTable

  explicit RestyleTracker(Element::FlagsType aRestyleBits)
    : mRestyleBits(aRestyleBits)
    , mHaveLaterSiblingRestyles(false)
    , mHaveSelectors(false)
  {
    NS_PRECONDITION((mRestyleBits & ~ELEMENT_ALL_RESTYLE_FLAGS) == 0,
                    "Why do we have these bits set?");
    NS_PRECONDITION((mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS) != 0,
                    "Must have a restyle flag");
    NS_PRECONDITION((mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS) !=
                      ELEMENT_PENDING_RESTYLE_FLAGS,
                    "Shouldn't have both restyle flags set");
    NS_PRECONDITION((mRestyleBits & ELEMENT_POTENTIAL_RESTYLE_ROOT_FLAGS) != 0,
                    "Must have root flag");
    NS_PRECONDITION((mRestyleBits & ELEMENT_POTENTIAL_RESTYLE_ROOT_FLAGS) !=
                    ELEMENT_POTENTIAL_RESTYLE_ROOT_FLAGS,
                    "Shouldn't have both root flags");
  }

  void Init(RestyleManager* aRestyleManager) {
    mRestyleManager = aRestyleManager;
  }

  uint32_t Count() const {
    return mPendingRestyles.Count();
  }

  /**
   * Add a restyle for the given element to the tracker.  Returns true
   * if the element already had eRestyle_LaterSiblings set on it.
   *
   * aRestyleRoot is the closest restyle root for aElement.  If the caller
   * does not know what the closest restyle root is, Nothing should be
   * passed.  A Some(nullptr) restyle root can be passed if there is no
   * ancestor element that is a restyle root.
   */
  bool AddPendingRestyle(Element* aElement, nsRestyleHint aRestyleHint,
                         nsChangeHint aMinChangeHint,
                         const RestyleHintData* aRestyleHintData = nullptr,
                         mozilla::Maybe<Element*> aRestyleRoot =
                           mozilla::Nothing());

  Element* FindClosestRestyleRoot(Element* aElement);

  /**
   * Process the restyles we've been tracking.
   */
  void DoProcessRestyles();

  // Return our ELEMENT_HAS_PENDING_(ANIMATION_)RESTYLE bit
  uint32_t RestyleBit() const {
    return mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS;
  }

  // Return our ELEMENT_IS_POTENTIAL_(ANIMATION_)RESTYLE_ROOT bit
  Element::FlagsType RootBit() const {
    return mRestyleBits & ELEMENT_POTENTIAL_RESTYLE_ROOT_FLAGS;
  }

  // Return our ELEMENT_IS_CONDITIONAL_RESTYLE_ANCESTOR bit if present,
  // or 0 if it is not.
  Element::FlagsType ConditionalDescendantsBit() const {
    return mRestyleBits & ELEMENT_IS_CONDITIONAL_RESTYLE_ANCESTOR;
  }

  struct Hints {
    nsRestyleHint mRestyleHint;        // What we want to restyle
    nsChangeHint mChangeHint;          // The minimal change hint for "self"
    RestyleHintData mRestyleHintData;  // Data associated with mRestyleHint
  };

  struct RestyleData : Hints {
    RestyleData() {
      mRestyleHint = nsRestyleHint(0);
      mChangeHint = nsChangeHint(0);
    }

    RestyleData(nsRestyleHint aRestyleHint, nsChangeHint aChangeHint,
                const RestyleHintData* aRestyleHintData) {
      mRestyleHint = aRestyleHint;
      mChangeHint = aChangeHint;
      if (aRestyleHintData) {
        mRestyleHintData = *aRestyleHintData;
      }
    }

    // Descendant elements we must check that we ended up restyling, ordered
    // with the same invariant as mRestyleRoots.  The elements here are those
    // that we called AddPendingRestyle for and found the element this is
    // the RestyleData for as its nearest restyle root.
    nsTArray<RefPtr<Element>> mDescendants;
#if defined(MOZ_ENABLE_PROFILER_SPS)
    UniquePtr<ProfilerBacktrace> mBacktrace;
#endif
  };

  /**
   * If the given Element has a restyle pending for it, return the
   * relevant restyle data.  This function will clear everything other
   * than a possible eRestyle_LaterSiblings hint for aElement out of
   * our hashtable.  The returned aData will never have an
   * eRestyle_LaterSiblings hint in it.
   *
   * The return value indicates whether any restyle data was found for
   * the element.  aData is set to nullptr iff false is returned.
   */
  bool GetRestyleData(Element* aElement, nsAutoPtr<RestyleData>& aData);

  /**
   * Returns whether there is a RestyleData entry in mPendingRestyles
   * for the given element.
   */
  bool HasRestyleData(Element* aElement) {
    return mPendingRestyles.Contains(aElement);
  }

  /**
   * For each element in aElements, appends it to mRestyleRoots if it
   * has its restyle bit set.  This is used to ensure we restyle elements
   * that we did not add as restyle roots initially (due to there being
   * an ancestor with the restyle root bit set), but which we might
   * not have got around to restyling due to the restyle process
   * terminating early with RestyleResul::eStop (see ElementRestyler::Restyle).
   *
   * This function must be called with elements in order such that
   * appending them to mRestyleRoots maintains its ordering invariant that
   * ancestors appear after descendants.
   */
  void AddRestyleRootsIfAwaitingRestyle(
                                  const nsTArray<RefPtr<Element>>& aElements);

  /**
   * Converts any eRestyle_SomeDescendants restyle hints in the pending restyle
   * table into eRestyle_Subtree hints and clears out the associated arrays of
   * nsCSSSelector pointers.  This is called in response to a style sheet change
   * that might have cause an nsCSSSelector to be destroyed.
   */
  void ClearSelectors();

  /**
   * The document we're associated with.
   */
  inline nsIDocument* Document() const;

#ifdef RESTYLE_LOGGING
  // Defined in RestyleTrackerInlines.h.
  inline bool ShouldLogRestyle();
  inline int32_t& LoggingDepth();
#endif

private:
  bool AddPendingRestyleToTable(Element* aElement, nsRestyleHint aRestyleHint,
                                nsChangeHint aMinChangeHint,
                                const RestyleHintData* aRestyleHintData = nullptr);

  /**
   * Handle a single mPendingRestyles entry.  aRestyleHint must not
   * include eRestyle_LaterSiblings; that needs to be dealt with
   * before calling this function.
   */
  inline void ProcessOneRestyle(Element* aElement,
                                nsRestyleHint aRestyleHint,
                                nsChangeHint aChangeHint,
                                const RestyleHintData& aRestyleHintData);

  typedef nsClassHashtable<nsISupportsHashKey, RestyleData> PendingRestyleTable;
  typedef AutoTArray< RefPtr<Element>, 32> RestyleRootArray;
  // Our restyle bits.  These will be a subset of ELEMENT_ALL_RESTYLE_FLAGS, and
  // will include one flag from ELEMENT_PENDING_RESTYLE_FLAGS, one flag
  // from ELEMENT_POTENTIAL_RESTYLE_ROOT_FLAGS, and might also include
  // ELEMENT_IS_CONDITIONAL_RESTYLE_ANCESTOR.
  Element::FlagsType mRestyleBits;
  RestyleManager* mRestyleManager; // Owns us
  // A hashtable that maps elements to pointers to RestyleData structs.  The
  // values only make sense if the element's current document is our
  // document and it has our RestyleBit() flag set.  In particular,
  // said bit might not be set if the element had a restyle posted and
  // then was moved around in the DOM.
  PendingRestyleTable mPendingRestyles;
  // An array that keeps track of our possible restyle roots.  This
  // maintains the invariant that if A and B are both restyle roots
  // and A is an ancestor of B then A will come after B in the array.
  // We maintain this invariant by checking whether an element has an
  // ancestor with the restyle root bit set before appending it to the
  // array.
  RestyleRootArray mRestyleRoots;
  // True if we have some entries with the eRestyle_LaterSiblings
  // flag.  We need this to avoid enumerating the hashtable looking
  // for such entries when we can't possibly have any.
  bool mHaveLaterSiblingRestyles;
  // True if we have some entries with selectors in the restyle hint data.
  // We use this to skip iterating over mPendingRestyles in ClearSelectors.
  bool mHaveSelectors;
};

inline bool
RestyleTracker::AddPendingRestyleToTable(Element* aElement,
                                         nsRestyleHint aRestyleHint,
                                         nsChangeHint aMinChangeHint,
                                         const RestyleHintData* aRestyleHintData)
{
  RestyleData* existingData;

  if (aRestyleHintData &&
      !aRestyleHintData->mSelectorsForDescendants.IsEmpty()) {
    mHaveSelectors = true;
  }

  // Check the RestyleBit() flag before doing the hashtable Get, since
  // it's possible that the data in the hashtable isn't actually
  // relevant anymore (if the flag is not set).
  if (aElement->HasFlag(RestyleBit())) {
    mPendingRestyles.Get(aElement, &existingData);
  } else {
    aElement->SetFlags(RestyleBit());
    existingData = nullptr;
  }

  if (aRestyleHint & eRestyle_SomeDescendants) {
    NS_ASSERTION(ConditionalDescendantsBit(),
                 "why are we getting eRestyle_SomeDescendants in an "
                 "animation-only restyle?");
    aElement->SetFlags(ConditionalDescendantsBit());
  }

  if (!existingData) {
    RestyleData* rd =
      new RestyleData(aRestyleHint, aMinChangeHint, aRestyleHintData);
#if defined(MOZ_ENABLE_PROFILER_SPS)
    if (profiler_feature_active("restyle")) {
      rd->mBacktrace.reset(profiler_get_backtrace());
    }
#endif
    mPendingRestyles.Put(aElement, rd);
    return false;
  }

  bool hadRestyleLaterSiblings =
    (existingData->mRestyleHint & eRestyle_LaterSiblings) != 0;
  existingData->mRestyleHint =
    nsRestyleHint(existingData->mRestyleHint | aRestyleHint);
  existingData->mChangeHint |= aMinChangeHint;
  if (aRestyleHintData) {
    existingData->mRestyleHintData.mSelectorsForDescendants
      .AppendElements(aRestyleHintData->mSelectorsForDescendants);
  }

  return hadRestyleLaterSiblings;
}

inline mozilla::dom::Element*
RestyleTracker::FindClosestRestyleRoot(Element* aElement)
{
  Element* cur = aElement;
  while (!cur->HasFlag(RootBit())) {
    nsIContent* parent = cur->GetFlattenedTreeParent();
    // Stop if we have no parent or the parent is not an element or
    // we're part of the viewport scrollbars (because those are not
    // frametree descendants of the primary frame of the root
    // element).
    // XXXbz maybe the primary frame of the root should be the root scrollframe?
    if (!parent || !parent->IsElement() ||
        // If we've hit the root via a native anonymous kid and that
        // this native anonymous kid is not obviously a descendant
        // of the root's primary frame, assume we're under the root
        // scrollbars.  Since those don't get reresolved when
        // reresolving the root, we need to make sure to add the
        // element to mRestyleRoots.
        (cur->IsInNativeAnonymousSubtree() && !parent->GetParent() &&
         cur->GetPrimaryFrame() &&
         cur->GetPrimaryFrame()->GetParent() != parent->GetPrimaryFrame())) {
      return nullptr;
    }
    cur = parent->AsElement();
  }
  return cur;
}

inline bool
RestyleTracker::AddPendingRestyle(Element* aElement,
                                  nsRestyleHint aRestyleHint,
                                  nsChangeHint aMinChangeHint,
                                  const RestyleHintData* aRestyleHintData,
                                  mozilla::Maybe<Element*> aRestyleRoot)
{
  bool hadRestyleLaterSiblings =
    AddPendingRestyleToTable(aElement, aRestyleHint, aMinChangeHint,
                             aRestyleHintData);

  // We can only treat this element as a restyle root if we would
  // actually restyle its descendants (so either call
  // ReResolveStyleContext on it or just reframe it).
  if ((aRestyleHint & ~eRestyle_LaterSiblings) ||
      (aMinChangeHint & nsChangeHint_ReconstructFrame)) {
    Element* cur =
      aRestyleRoot ? *aRestyleRoot : FindClosestRestyleRoot(aElement);
    if (!cur) {
      mRestyleRoots.AppendElement(aElement);
      cur = aElement;
    }
    // At this point some ancestor of aElement (possibly aElement
    // itself) is in mRestyleRoots.  Set the root bit on aElement, to
    // speed up searching for an existing root on its descendants.
    aElement->SetFlags(RootBit());
    if (cur != aElement) {
      // We are already going to restyle cur, one of aElement's ancestors,
      // but we might not end up restyling all the way down to aElement.
      // Record it in the RestyleData so we can ensure it does get restyled
      // after we deal with cur.
      //
      // As with the mRestyleRoots array, mDescendants maintains the
      // invariant that if two elements appear in the array and one
      // is an ancestor of the other, that the ancestor appears after
      // the descendant.
      RestyleData* curData;
      mPendingRestyles.Get(cur, &curData);
      NS_ASSERTION(curData, "expected to find a RestyleData for cur");
      // If cur has an eRestyle_ForceDescendants restyle hint, then we
      // know that we will get to all descendants.  Don't bother
      // recording the descendant to restyle in that case.
      if (curData && !(curData->mRestyleHint & eRestyle_ForceDescendants)) {
        curData->mDescendants.AppendElement(aElement);
      }
    }
  }

  mHaveLaterSiblingRestyles =
    mHaveLaterSiblingRestyles || (aRestyleHint & eRestyle_LaterSiblings) != 0;
  return hadRestyleLaterSiblings;
}

} // namespace mozilla

#endif /* mozilla_RestyleTracker_h */
