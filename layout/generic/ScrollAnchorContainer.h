/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ScrollAnchorContainer_h_
#define mozilla_layout_ScrollAnchorContainer_h_

#include "nsPoint.h"
#include "mozilla/Saturate.h"

class nsFrameList;
class nsIFrame;
class nsIScrollableFrame;

namespace mozilla {
class ScrollFrameHelper;
}  // namespace mozilla

namespace mozilla {
namespace layout {

/**
 * A scroll anchor container finds a descendent element of a scrollable frame
 * to be an anchor node. After every reflow, the scroll anchor will apply
 * scroll adjustments to keep the anchor node in the same relative position.
 *
 * See: https://drafts.csswg.org/css-scroll-anchoring/
 */
class ScrollAnchorContainer final {
 public:
  explicit ScrollAnchorContainer(ScrollFrameHelper* aScrollFrame);
  ~ScrollAnchorContainer();

  /**
   * Returns the nearest scroll anchor container that could select aFrame as an
   * anchor node.
   */
  static ScrollAnchorContainer* FindFor(nsIFrame* aFrame);

  /**
   * Returns the frame that is the selected anchor node or null if no anchor
   * is selected.
   */
  nsIFrame* AnchorNode() const { return mAnchorNode; }

  /**
   * Returns the frame that owns this scroll anchor container. This is always
   * non-null.
   */
  nsIFrame* Frame() const;

  /**
   * Returns the frame that owns this scroll anchor container as a scrollable
   * frame. This is always non-null.
   */
  nsIScrollableFrame* ScrollableFrame() const;

  /**
   * Find a suitable anchor node among the descendants of the scrollable frame.
   * This should only be called after the scroll anchor has been invalidated.
   */
  void SelectAnchor();

  /**
   * Whether this scroll frame can maintain an anchor node at the moment.
   */
  bool CanMaintainAnchor() const;

  /**
   * Notify the scroll anchor container that its scroll frame has been
   * scrolled by a user and should invalidate itself.
   */
  void UserScrolled();

  /**
   * Notify the scroll anchor container that a reflow has happened and it
   * should query its anchor to see if a scroll adjustment needs to occur.
   */
  void ApplyAdjustments();

  /**
   * Notify the scroll anchor container that it should suppress any scroll
   * adjustment that may happen after the next layout flush.
   */
  void SuppressAdjustments();

  /**
   * Notify this scroll anchor container that its anchor node should be
   * invalidated, and recomputed at the next available opportunity if
   * ScheduleSelection is Yes.
   */
  enum class ScheduleSelection { No, Yes };
  void InvalidateAnchor(ScheduleSelection = ScheduleSelection::Yes);

  /**
   * Notify this scroll anchor container that it will be destroyed along with
   * its parent frame.
   */
  void Destroy();

 private:
  // Represents an assessment of a frame's suitability as a scroll anchor,
  // from the scroll-anchoring spec's "candidate examination algorithm":
  // https://drafts.csswg.org/css-scroll-anchoring-1/#candidate-examination
  enum class ExamineResult {
    // The frame is an excluded subtree or fully clipped and should be ignored.
    // This corresponds with step 1 in the algorithm.
    Exclude,
    // This frame is an anonymous or inline box and its descendants should be
    // searched to find an anchor node. If none are found, then continue
    // searching. This is implied by the prologue of the algorithm, and
    // should be made explicit in the spec [1].
    //
    // [1] https://github.com/w3c/csswg-drafts/issues/3489
    PassThrough,
    // The frame is partially visible and its descendants should be searched to
    // find an anchor node. If none are found then this frame should be
    // selected. This corresponds with step 3 in the algorithm.
    Traverse,
    // The frame is fully visible and should be selected as an anchor node. This
    // corresponds with step 2 in the algorithm.
    Accept,
  };

  ExamineResult ExamineAnchorCandidate(nsIFrame* aPrimaryFrame) const;

  // Search a frame's children to find an anchor node. Returns the frame for a
  // valid anchor node, if one was found in the frames descendants, or null
  // otherwise.
  nsIFrame* FindAnchorIn(nsIFrame* aFrame) const;

  // Search a child list to find an anchor node. Returns the frame for a valid
  // anchor node, if one was found in this child list, or null otherwise.
  nsIFrame* FindAnchorInList(const nsFrameList& aFrameList) const;

  // Notes that a given adjustment has happened, and maybe disables scroll
  // anchoring on this scroller altogether based on various prefs.
  void AdjustmentMade(nscoord aAdjustment);

  // The owner of this scroll anchor container
  ScrollFrameHelper* mScrollFrame;

  // The anchor node that we will scroll to keep in the same relative position
  // after reflows. This may be null if we were not able to select a valid
  // scroll anchor
  nsIFrame* mAnchorNode;

  // The last offset of the scroll anchor node's scrollable overflow rect start
  // edge relative to the scroll-port start edge, in the block axis of the
  // scroll frame. This is used for calculating the distance to scroll to keep
  // the anchor node in the same relative position
  nscoord mLastAnchorOffset;

  // The number of consecutive scroll anchoring adjustments that have happened
  // without a user scroll.
  SaturateUint32 mConsecutiveScrollAnchoringAdjustments{0};

  // The total length that has been adjusted by all the consecutive adjustments
  // referenced above. Note that this is a sum, so that oscillating adjustments
  // average towards zero.
  nscoord mConsecutiveScrollAnchoringAdjustmentLength{0};

  // True if we've been disabled by the heuristic controlled by
  // layout.css.scroll-anchoring.max-consecutive-adjustments and
  // layout.css.scroll-anchoring.min-adjustment-threshold.
  bool mDisabled : 1;

  // True if we should recalculate our anchor node at the next chance
  bool mAnchorNodeIsDirty : 1;
  // True if we are applying a scroll anchor adjustment
  bool mApplyingAnchorAdjustment : 1;
  // True if we should suppress anchor adjustments
  bool mSuppressAnchorAdjustment : 1;
};

}  // namespace layout
}  // namespace mozilla

#endif  // mozilla_layout_ScrollAnchorContainer_h_
