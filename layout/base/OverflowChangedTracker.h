/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_OverflowChangedTracker_h
#define mozilla_OverflowChangedTracker_h

#include "nsIFrame.h"
#include "nsContainerFrame.h"
#include "mozilla/SplayTree.h"

namespace mozilla {

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
    // XXXheycam Temporarily downgrade this assertion (bug 1324647).
    NS_ASSERTION_STYLO_WARNING(mEntryList.empty(), "Need to flush before destroying!");
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

        NS_ASSERTION(frame->GetProperty(
                       nsIFrame::DebugInitialOverflowPropertyApplied()),
                     "InitialOverflowProperty must be set first.");

        nsOverflowAreas* overflow =
          frame->GetProperty(nsIFrame::InitialOverflowProperty());
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
        while (parent &&
               parent != mSubtreeRoot &&
               parent->Combines3DTransformWithAncestors()) {
          // Passing frames in between the frame and the establisher of
          // 3D rendering context.
          parent = parent->GetParent();
          MOZ_ASSERT(parent, "Root frame should never return true for Combines3DTransformWithAncestors");
        }
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

} // namespace mozilla

#endif
