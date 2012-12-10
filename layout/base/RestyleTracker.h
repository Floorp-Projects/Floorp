/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A class which manages pending restyles.  This handles keeping track
 * of what nodes restyles need to happen on and so forth.
 */

#ifndef mozilla_css_RestyleTracker_h
#define mozilla_css_RestyleTracker_h

#include "mozilla/dom/Element.h"
#include "nsDataHashtable.h"
#include "nsIFrame.h"
#include "nsTPriorityQueue.h"

class nsCSSFrameConstructor;

namespace mozilla {
namespace css {

/** 
 * Helper class that collects a list of frames that need
 * UpdateOverflow() called on them, and coalesces them
 * to avoid walking up the same ancestor tree multiple times.
 */
class OverflowChangedTracker
{
public:

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
  void AddFrame(nsIFrame* aFrame) {
    mEntryList.Push(Entry(aFrame, true));
  }

  /**
   * Update the overflow of all added frames, and clear the entry list.
   *
   * Start from those deepest in the frame tree and works upwards. This stops 
   * us from processing the same frame twice.
   */
  void Flush() {
    while (!mEntryList.IsEmpty()) {
      Entry entry = mEntryList.Pop();

      // Pop off any duplicate entries and copy back mInitial
      // if any have it set.
      while (!mEntryList.IsEmpty() &&
             mEntryList.Top().mFrame == entry.mFrame) {
        Entry next = mEntryList.Pop();

        if (next.mInitial) {
          entry.mInitial = true;
        }
      }
      nsIFrame *frame = entry.mFrame;

      bool updateParent = false;
      if (entry.mInitial) {
        nsOverflowAreas* pre = static_cast<nsOverflowAreas*>
          (frame->Properties().Get(frame->PreTransformOverflowAreasProperty()));
        if (pre) {
          // FinishAndStoreOverflow will change the overflow areas passed in,
          // so make a copy.
          nsOverflowAreas overflowAreas = *pre;
          frame->FinishAndStoreOverflow(overflowAreas, frame->GetSize());
          // We can't tell if the overflow changed, so update the parent regardless
          updateParent = true;
        }
      }
      
      // If the overflow changed, then we want to also update the parent's
      // overflow. We always update the parent for initial frames.
      if (!updateParent) {
        updateParent = frame->UpdateOverflow() || entry.mInitial;
      }
      if (updateParent) {
        nsIFrame *parent = frame->GetParent();
        if (parent) {
          mEntryList.Push(Entry(parent, entry.mDepth - 1, false));
        }
      }
    }
  }
  
private:
  struct Entry
  {
    Entry(nsIFrame* aFrame, bool aInitial)
      : mFrame(aFrame)
      , mDepth(aFrame->GetDepthInFrameTree())
      , mInitial(aInitial)
    {}
    
    Entry(nsIFrame* aFrame, uint32_t aDepth, bool aInitial)
      : mFrame(aFrame)
      , mDepth(aDepth)
      , mInitial(aInitial)
    {}

    bool operator==(const Entry& aOther) const
    {
      return mFrame == aOther.mFrame;
    }
 
    /**
     * Sort by the depth in the frame tree, and then
     * the frame pointer.
     */
    bool operator<(const Entry& aOther) const
    {
      if (mDepth != aOther.mDepth) {
        // nsTPriorityQueue implements a min-heap and we
        // want the highest depth first, so reverse this check.
        return mDepth > aOther.mDepth;
      }

      return mFrame < aOther.mFrame;
    }

    nsIFrame* mFrame;
    /* Depth in the frame tree */
    uint32_t mDepth;
    /**
     * True if the frame had the actual style change, and we
     * want to check for pre-transform overflow areas.
     */
    bool mInitial;
  };

  /* A list of frames to process, sorted by their depth in the frame tree */
  nsTPriorityQueue<Entry> mEntryList;
};

class RestyleTracker {
public:
  typedef mozilla::dom::Element Element;

  RestyleTracker(uint32_t aRestyleBits,
                 nsCSSFrameConstructor* aFrameConstructor) :
    mRestyleBits(aRestyleBits), mFrameConstructor(aFrameConstructor),
    mHaveLaterSiblingRestyles(false)
  {
    NS_PRECONDITION((mRestyleBits & ~ELEMENT_ALL_RESTYLE_FLAGS) == 0,
                    "Why do we have these bits set?");
    NS_PRECONDITION((mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS) != 0,
                    "Must have a restyle flag");
    NS_PRECONDITION((mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS) !=
                      ELEMENT_PENDING_RESTYLE_FLAGS,
                    "Shouldn't have both restyle flags set");
    NS_PRECONDITION((mRestyleBits & ~ELEMENT_PENDING_RESTYLE_FLAGS) != 0,
                    "Must have root flag");
    NS_PRECONDITION((mRestyleBits & ~ELEMENT_PENDING_RESTYLE_FLAGS) !=
                    (ELEMENT_ALL_RESTYLE_FLAGS & ~ELEMENT_PENDING_RESTYLE_FLAGS),
                    "Shouldn't have both root flags");
  }

  void Init() {
    mPendingRestyles.Init();
  }

  uint32_t Count() const {
    return mPendingRestyles.Count();
  }

  /**
   * Add a restyle for the given element to the tracker.  Returns true
   * if the element already had eRestyle_LaterSiblings set on it.
   */
  bool AddPendingRestyle(Element* aElement, nsRestyleHint aRestyleHint,
                           nsChangeHint aMinChangeHint);

  /**
   * Process the restyles we've been tracking.
   */
  void ProcessRestyles() {
    // Fast-path the common case (esp. for the animation restyle
    // tracker) of not having anything to do.
    if (mPendingRestyles.Count()) {
      DoProcessRestyles();
    }
  }

  // Return our ELEMENT_HAS_PENDING_(ANIMATION_)RESTYLE bit
  uint32_t RestyleBit() const {
    return mRestyleBits & ELEMENT_PENDING_RESTYLE_FLAGS;
  }

  // Return our ELEMENT_IS_POTENTIAL_(ANIMATION_)RESTYLE_ROOT bit
  uint32_t RootBit() const {
    return mRestyleBits & ~ELEMENT_PENDING_RESTYLE_FLAGS;
  }
  
  struct RestyleData {
    nsRestyleHint mRestyleHint;  // What we want to restyle
    nsChangeHint  mChangeHint;   // The minimal change hint for "self"
  };

  /**
   * If the given Element has a restyle pending for it, return the
   * relevant restyle data.  This function will clear everything other
   * than a possible eRestyle_LaterSiblings hint for aElement out of
   * our hashtable.  The returned aData will never have an
   * eRestyle_LaterSiblings hint in it.
   *
   * The return value indicates whether any restyle data was found for
   * the element.  If false is returned, then the state of *aData is
   * undefined.
   */
  bool GetRestyleData(Element* aElement, RestyleData* aData);

  /**
   * The document we're associated with.
   */
  inline nsIDocument* Document() const;

  struct RestyleEnumerateData : public RestyleData {
    nsRefPtr<Element> mElement;
  };

private:
  /**
   * Handle a single mPendingRestyles entry.  aRestyleHint must not
   * include eRestyle_LaterSiblings; that needs to be dealt with
   * before calling this function.
   */
  inline void ProcessOneRestyle(Element* aElement,
                                nsRestyleHint aRestyleHint,
                                nsChangeHint aChangeHint,
                                OverflowChangedTracker& aTracker);

  /**
   * The guts of our restyle processing.
   */
  void DoProcessRestyles();

  typedef nsDataHashtable<nsISupportsHashKey, RestyleData> PendingRestyleTable;
  typedef nsAutoTArray< nsRefPtr<Element>, 32> RestyleRootArray;
  // Our restyle bits.  These will be a subset of ELEMENT_ALL_RESTYLE_FLAGS, and
  // will include one flag from ELEMENT_PENDING_RESTYLE_FLAGS and one flag
  // that's not in ELEMENT_PENDING_RESTYLE_FLAGS.
  uint32_t mRestyleBits;
  nsCSSFrameConstructor* mFrameConstructor; // Owns us
  // A hashtable that maps elements to RestyleData structs.  The
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
};

inline bool RestyleTracker::AddPendingRestyle(Element* aElement,
                                                nsRestyleHint aRestyleHint,
                                                nsChangeHint aMinChangeHint)
{
  RestyleData existingData;
  existingData.mRestyleHint = nsRestyleHint(0);
  existingData.mChangeHint = NS_STYLE_HINT_NONE;

  // Check the RestyleBit() flag before doing the hashtable Get, since
  // it's possible that the data in the hashtable isn't actually
  // relevant anymore (if the flag is not set).
  if (aElement->HasFlag(RestyleBit())) {
    mPendingRestyles.Get(aElement, &existingData);
  } else {
    aElement->SetFlags(RestyleBit());
  }

  bool hadRestyleLaterSiblings =
    (existingData.mRestyleHint & eRestyle_LaterSiblings) != 0;
  existingData.mRestyleHint =
    nsRestyleHint(existingData.mRestyleHint | aRestyleHint);
  NS_UpdateHint(existingData.mChangeHint, aMinChangeHint);

  mPendingRestyles.Put(aElement, existingData);

  // We can only treat this element as a restyle root if we would
  // actually restyle its descendants (so either call
  // ReResolveStyleContext on it or just reframe it).
  if ((aRestyleHint & (eRestyle_Self | eRestyle_Subtree)) ||
      (aMinChangeHint & nsChangeHint_ReconstructFrame)) {
    for (const Element* cur = aElement; !cur->HasFlag(RootBit()); ) {
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
        mRestyleRoots.AppendElement(aElement);
        break;
      }
      cur = parent->AsElement();
    }
    // At this point some ancestor of aElement (possibly aElement
    // itself) is in mRestyleRoots.  Set the root bit on aElement, to
    // speed up searching for an existing root on its descendants.
    aElement->SetFlags(RootBit());
  }

  mHaveLaterSiblingRestyles =
    mHaveLaterSiblingRestyles || (aRestyleHint & eRestyle_LaterSiblings) != 0;
  return hadRestyleLaterSiblings;
}

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_RestyleTracker_h */
