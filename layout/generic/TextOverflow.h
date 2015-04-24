/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextOverflow_h_
#define TextOverflow_h_

#include "nsDisplayList.h"
#include "nsTHashtable.h"
#include "nsAutoPtr.h"
#include "mozilla/Likely.h"
#include "mozilla/WritingModes.h"
#include <algorithm>

class nsIScrollableFrame;
class nsLineBox;

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
   * @return true if aBlockFrmae has text-overflow:clip on both sides.
   */
  static bool HasClippedOverflow(nsIFrame* aBlockFrame);
  /**
   * @return true if aBlockFrame needs analysis for text overflow.
   */
  static bool CanHaveTextOverflow(nsDisplayListBuilder* aBuilder,
                                  nsIFrame*             aBlockFrame);

  typedef nsTHashtable<nsPtrHashKey<nsIFrame> > FrameHashtable;

 protected:
  TextOverflow(nsDisplayListBuilder* aBuilder,
               nsIFrame* aBlockFrame);

  typedef mozilla::WritingMode WritingMode;
  typedef mozilla::LogicalRect LogicalRect;

  struct AlignmentEdges {
    AlignmentEdges() : mAssigned(false) {}
    void Accumulate(WritingMode aWM, const LogicalRect& aRect)
    {
      if (MOZ_LIKELY(mAssigned)) {
        mIStart = std::min(mIStart, aRect.IStart(aWM));
        mIEnd = std::max(mIEnd, aRect.IEnd(aWM));
      } else {
        mIStart = aRect.IStart(aWM);
        mIEnd = aRect.IEnd(aWM);
        mAssigned = true;
      }
    }
    nscoord ISize() { return mIEnd - mIStart; }
    nscoord mIStart;
    nscoord mIEnd;
    bool mAssigned;
  };

  struct InnerClipEdges {
    InnerClipEdges() : mAssignedIStart(false), mAssignedIEnd(false) {}
    void AccumulateIStart(WritingMode aWM, const LogicalRect& aRect)
    {
      if (MOZ_LIKELY(mAssignedIStart)) {
        mIStart = std::max(mIStart, aRect.IStart(aWM));
      } else {
        mIStart = aRect.IStart(aWM);
        mAssignedIStart = true;
      }
    }
    void AccumulateIEnd(WritingMode aWM, const LogicalRect& aRect)
    {
      if (MOZ_LIKELY(mAssignedIEnd)) {
        mIEnd = std::min(mIEnd, aRect.IEnd(aWM));
      } else {
        mIEnd = aRect.IEnd(aWM);
        mAssignedIEnd = true;
      }
    }
    nscoord mIStart;
    nscoord mIEnd;
    bool mAssignedIStart;
    bool mAssignedIEnd;
  };

  LogicalRect
    GetLogicalScrollableOverflowRectRelativeToBlock(nsIFrame* aFrame) const
  {
    return LogicalRect(mBlockWM,
                       aFrame->GetScrollableOverflowRect() +
                         aFrame->GetOffsetTo(mBlock),
                       mBlockWidth);
  }

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
                           const LogicalRect& aContentArea,
                           const LogicalRect& aInsideMarkersArea,
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
                          const LogicalRect& aInsideMarkersArea,
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
  void PruneDisplayListContents(nsDisplayList* aList,
                                const FrameHashtable& aFramesToHide,
                                const LogicalRect& aInsideMarkersArea);

  /**
   * ProcessLine calls this to create display items for the markers and insert
   * them into mMarkerList.
   * @param aLine the line we're processing
   * @param aCreateIStart if true, create a marker on the inline start side
   * @param aCreateIEnd if true, create a marker on the inline end side
   * @param aInsideMarkersArea is the area inside the markers
   */
  void CreateMarkers(const nsLineBox* aLine,
                     bool aCreateIStart, bool aCreateIEnd,
                     const LogicalRect& aInsideMarkersArea);

  LogicalRect            mContentArea;
  nsDisplayListBuilder*  mBuilder;
  nsIFrame*              mBlock;
  nsIScrollableFrame*    mScrollableFrame;
  nsDisplayList          mMarkerList;
  nscoord                mBlockWidth;
  WritingMode            mBlockWM;
  bool                   mCanHaveInlineAxisScrollbar;
  bool                   mAdjustForPixelSnapping;

  class Marker {
  public:
    void Init(const nsStyleTextOverflowSide& aStyle) {
      mInitialized = false;
      mISize = 0;
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

    // The current width of the marker, the range is [0 .. mIntrinsicISize].
    nscoord                        mISize;
    // The intrinsic width of the marker.
    nscoord                        mIntrinsicISize;
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

  Marker mIStart; // the inline start marker
  Marker mIEnd; // the inline end marker
};

} // namespace css
} // namespace mozilla

#endif /* !defined(TextOverflow_h_) */
