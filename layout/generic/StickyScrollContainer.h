/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * compute sticky positioning, both during reflow and when the scrolling
 * container scrolls
 */

#ifndef StickyScrollContainer_h
#define StickyScrollContainer_h

#include "nsPoint.h"
#include "nsTArray.h"
#include "nsIScrollPositionListener.h"

struct nsRect;
class nsIFrame;
class nsIScrollableFrame;

namespace mozilla {

class StickyScrollContainer MOZ_FINAL : public nsIScrollPositionListener
{
public:
  /**
   * Find (and create if necessary) the StickyScrollContainer associated with
   * the scroll container of the given frame, if a scroll container exists.
   */
  static StickyScrollContainer* GetStickyScrollContainerForFrame(nsIFrame* aFrame);

  /**
   * Find the StickyScrollContainer associated with the given scroll frame,
   * if it exists.
   */
  static StickyScrollContainer* GetStickyScrollContainerForScrollFrame(nsIFrame* aScrollFrame);

  /**
   * aFrame may have moved into or out of a scroll frame's frame subtree.
   */
  static void NotifyReparentedFrameAcrossScrollFrameBoundary(nsIFrame* aFrame,
                                                             nsIFrame* aOldParent);

  void AddFrame(nsIFrame* aFrame) {
    mFrames.AppendElement(aFrame);
  }
  void RemoveFrame(nsIFrame* aFrame) {
    mFrames.RemoveElement(aFrame);
  }

  nsIScrollableFrame* ScrollFrame() const {
    return mScrollFrame;
  }

  // Compute the offsets for a sticky position element
  static void ComputeStickyOffsets(nsIFrame* aFrame);

  /**
   * Compute the position of a sticky positioned frame, based on information
   * stored in its properties along with our scroll frame and scroll position.
   */
  nsPoint ComputePosition(nsIFrame* aFrame) const;

  /**
   * Compute where a frame should not scroll with the page, represented by the
   * difference of two rectangles.
   */
  void GetScrollRanges(nsIFrame* aFrame, nsRect* aOuter, nsRect* aInner) const;

  /**
   * Compute and set the position of a frame and its following continuations.
   */
  void PositionContinuations(nsIFrame* aFrame);

  /**
   * Compute and set the position of all sticky frames, given the current
   * scroll position of the scroll frame. If not in reflow, aSubtreeRoot should
   * be null; otherwise, overflow-area updates will be limited to not affect
   * aSubtreeRoot or its ancestors.
   */
  void UpdatePositions(nsPoint aScrollPosition, nsIFrame* aSubtreeRoot);

  // nsIScrollPositionListener
  virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) MOZ_OVERRIDE;
  virtual void ScrollPositionDidChange(nscoord aX, nscoord aY) MOZ_OVERRIDE;

  ~StickyScrollContainer();

private:
  explicit StickyScrollContainer(nsIScrollableFrame* aScrollFrame);

  /**
   * Compute two rectangles that determine sticky positioning: |aStick|, based
   * on the scroll container, and |aContain|, based on the containing block.
   * Sticky positioning keeps the frame position (its upper-left corner) always
   * within |aContain| and secondarily within |aStick|.
   */
  void ComputeStickyLimits(nsIFrame* aFrame, nsRect* aStick,
                           nsRect* aContain) const;

  nsIScrollableFrame* const mScrollFrame;
  nsTArray<nsIFrame*> mFrames;
  nsPoint mScrollPosition;
};

} // namespace mozilla

#endif /* StickyScrollContainer_h */
