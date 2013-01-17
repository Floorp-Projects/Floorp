/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextOverflow_h_
#define TextOverflow_h_

#include "nsDisplayList.h"
#include "nsLineBox.h"
#include "nsStyleStruct.h"
#include "nsTHashtable.h"
#include "mozilla/Likely.h"
#include <algorithm>
class nsIScrollableFrame;

namespace mozilla {
namespace css {

/**
 * A class for rendering CSS3 text-overflow.
 * Usage:
 *  1. allocate an object using WillProcessLines
 *  2. then call ProcessLine for each line you are building display lists for
 */
class TextOverflow {
 public:
  /**
   * Allocate an object for text-overflow processing.
   * @return nullptr if no processing is necessary.  The caller owns the object.
   */
  static TextOverflow* WillProcessLines(nsDisplayListBuilder*   aBuilder,
                                        nsIFrame*               aBlockFrame);
  /**
   * Analyze the display lists for text overflow and what kind of item is at
   * the content edges.  Add display items for text-overflow markers as needed
   * and remove or clip items that would overlap a marker.
   */
  void ProcessLine(const nsDisplayListSet& aLists, nsLineBox* aLine);

  /**
   * Get the resulting text-overflow markers (the list may be empty).
   * @return a DisplayList containing any text-overflow markers.
   */
  nsDisplayList& GetMarkers() { return mMarkerList; }

  /**
   * @return true if aBlockFrame needs analysis for text overflow.
   */
  static bool CanHaveTextOverflow(nsDisplayListBuilder* aBuilder,
                                  nsIFrame*             aBlockFrame);

  typedef nsTHashtable<nsPtrHashKey<nsIFrame> > FrameHashtable;

 protected:
  TextOverflow() {}
  void Init(nsDisplayListBuilder*   aBuilder,
            nsIFrame*               aBlockFrame);

  struct AlignmentEdges {
    AlignmentEdges() : mAssigned(false) {}
    void Accumulate(const nsRect& aRect) {
      if (MOZ_LIKELY(mAssigned)) {
        x = std::min(x, aRect.X());
        xmost = std::max(xmost, aRect.XMost());
      } else {
        x = aRect.X();
        xmost = aRect.XMost();
        mAssigned = true;
      }
    }
    nscoord Width() { return xmost - x; }
    nscoord x;
    nscoord xmost;
    bool mAssigned;
  };

  struct InnerClipEdges {
    InnerClipEdges() : mAssignedLeft(false), mAssignedRight(false) {}
    void AccumulateLeft(const nsRect& aRect) {
      if (MOZ_LIKELY(mAssignedLeft)) {
        mLeft = std::max(mLeft, aRect.X());
      } else {
        mLeft = aRect.X();
        mAssignedLeft = true;
      }
    }
    void AccumulateRight(const nsRect& aRect) {
      if (MOZ_LIKELY(mAssignedRight)) {
        mRight = std::min(mRight, aRect.XMost());
      } else {
        mRight = aRect.XMost();
        mAssignedRight = true;
      }
    }
    nscoord mLeft;
    nscoord mRight;
    bool mAssignedLeft;
    bool mAssignedRight;
  };

  /**
   * Examines frames on the line to determine whether we should draw a left
   * and/or right marker, and if so, which frames should be completely hidden
   * and the bounds of what will be displayed between the markers.
   * @param aLine the line we're processing
   * @param aFramesToHide frames that should have their display items removed
   * @param aAlignmentEdges the outermost edges of all text and atomic
   *   inline-level frames that are inside the area between the markers
   */
  void ExamineLineFrames(nsLineBox*      aLine,
                         FrameHashtable* aFramesToHide,
                         AlignmentEdges* aAlignmentEdges);

  /**
   * LineHasOverflowingText calls this to analyze edges, both the block's
   * content edges and the hypothetical marker edges aligned at the block edges.
   * @param aFrame the descendant frame of mBlock that we're analyzing
   * @param aContentArea the block's content area
   * @param aInsideMarkersArea the rectangle between the markers
   * @param aFramesToHide frames that should have their display items removed
   * @param aAlignmentEdges the outermost edges of all text and atomic
   *   inline-level frames that are inside the area between the markers
   * @param aFoundVisibleTextOrAtomic is set to true if a text or atomic
   *   inline-level frame is visible between the marker edges
   * @param aClippedMarkerEdges the innermost edges of all text and atomic
   *   inline-level frames that are clipped by the current marker width
   */
  void ExamineFrameSubtree(nsIFrame*       aFrame,
                           const nsRect&   aContentArea,
                           const nsRect&   aInsideMarkersArea,
                           FrameHashtable* aFramesToHide,
                           AlignmentEdges* aAlignmentEdges,
                           bool*           aFoundVisibleTextOrAtomic,
                           InnerClipEdges* aClippedMarkerEdges);

  /**
   * ExamineFrameSubtree calls this to analyze a frame against the hypothetical
   * marker edges (aInsideMarkersArea) for text frames and atomic inline-level
   * elements.  A text frame adds its extent inside aInsideMarkersArea where
   * grapheme clusters are fully visible.  An atomic adds its border box if
   * it's fully inside aInsideMarkersArea, otherwise the frame is added to
   * aFramesToHide.
   * @param aFrame the descendant frame of mBlock that we're analyzing
   * @param aFrameType aFrame's frame type
   * @param aInsideMarkersArea the rectangle between the markers
   * @param aFramesToHide frames that should have their display items removed
   * @param aAlignmentEdges the outermost edges of all text and atomic
   *   inline-level frames that are inside the area between the markers
   *                       inside aInsideMarkersArea
   * @param aFoundVisibleTextOrAtomic is set to true if a text or atomic
   *   inline-level frame is visible between the marker edges
   * @param aClippedMarkerEdges the innermost edges of all text and atomic
   *   inline-level frames that are clipped by the current marker width
   */
  void AnalyzeMarkerEdges(nsIFrame*       aFrame,
                          const nsIAtom*  aFrameType,
                          const nsRect&   aInsideMarkersArea,
                          FrameHashtable* aFramesToHide,
                          AlignmentEdges* aAlignmentEdges,
                          bool*           aFoundVisibleTextOrAtomic,
                          InnerClipEdges* aClippedMarkerEdges);

  /**
   * Clip or remove items given the final marker edges. ("clip" here just means
   * assigning mLeftEdge/mRightEdge for any nsCharClipDisplayItem that needs it,
   * see nsDisplayList.h for a description of that item).
   * @param aFramesToHide remove display items for these frames
   * @param aInsideMarkersArea is the area inside the markers
   */
  void PruneDisplayListContents(nsDisplayList*        aList,
                                const FrameHashtable& aFramesToHide,
                                const nsRect&         aInsideMarkersArea);

  /**
   * ProcessLine calls this to create display items for the markers and insert
   * them into mMarkerList.
   * @param aLine the line we're processing
   * @param aCreateLeft if true, create a marker on the left side
   * @param aCreateRight if true, create a marker on the right side
   * @param aInsideMarkersArea is the area inside the markers
   */
  void CreateMarkers(const nsLineBox* aLine,
                     bool             aCreateLeft,
                     bool             aCreateRight,
                     const nsRect&    aInsideMarkersArea);

  nsRect                 mContentArea;
  nsDisplayListBuilder*  mBuilder;
  nsIFrame*              mBlock;
  nsIScrollableFrame*    mScrollableFrame;
  nsDisplayList          mMarkerList;
  bool                   mBlockIsRTL;
  bool                   mCanHaveHorizontalScrollbar;
  bool                   mAdjustForPixelSnapping;

  class Marker {
  public:
    void Init(const nsStyleTextOverflowSide& aStyle) {
      mInitialized = false;
      mWidth = 0;
      mStyle = &aStyle;
    }

    /**
     * Setup the marker string and calculate its size, if not done already.
     */
    void SetupString(nsIFrame* aFrame);

    bool IsNeeded() const {
      return mHasOverflow;
    }
    void Reset() {
      mHasOverflow = false;
    }

    // The current width of the marker, the range is [0 .. mIntrinsicWidth].
    nscoord                        mWidth;
    // The intrinsic width of the marker string.
    nscoord                        mIntrinsicWidth;
    // The marker text.
    nsString                       mMarkerString;
    // The style for this side.
    const nsStyleTextOverflowSide* mStyle;
    // True if there is visible overflowing inline content on this side.
    bool                           mHasOverflow;
    // True if mMarkerString and mWidth have been setup from style.
    bool                           mInitialized;
    // True if the style is text-overflow:clip on this side and the marker
    // won't cause the line to become empty.
    bool                           mActive;
  };

  Marker mLeft;  // the horizontal left marker
  Marker mRight; // the horizontal right marker
};

} // namespace css
} // namespace mozilla

#endif /* !defined(TextOverflow_h_) */
