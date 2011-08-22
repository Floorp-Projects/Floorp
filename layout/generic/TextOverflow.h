/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is an implementation of CSS3 text-overflow.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mats Palmgren <matspal@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef TextOverflow_h_
#define TextOverflow_h_

#include "nsDisplayList.h"
#include "nsLineBox.h"
#include "nsStyleStruct.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace css {

/**
 * A class for rendering CSS3 text-overflow.
 * Usage:
 *  1. allocate an object using WillProcessLines
 *  2. then call ProcessLine for each line you are building display lists for
 *  3. finally call DidProcessLines
 */
class TextOverflow {
 public:
  /**
   * Allocate an object for text-overflow processing.
   * @return nsnull if no processing is necessary.  The caller owns the object.
   */
  static TextOverflow* WillProcessLines(nsDisplayListBuilder*   aBuilder,
                                        const nsDisplayListSet& aLists,
                                        nsIFrame*               aBlockFrame);
  /**
   * Analyze the display lists for text overflow and what kind of item is at
   * the content edges.  Add display items for text-overflow markers as needed
   * and remove or clip items that would overlap a marker.
   */
  void ProcessLine(const nsDisplayListSet& aLists, nsLineBox* aLine);

  /**
   * Do final processing, currently just adds a dummy item for scroll frames
   * to make IsVaryingRelativeToMovingFrame() true for the entire area.
   */
  void DidProcessLines();

  /**
   * @return true if aBlockFrame needs analysis for text overflow.
   */
  static bool CanHaveTextOverflow(nsDisplayListBuilder* aBuilder,
                                  nsIFrame*             aBlockFrame);

  typedef nsTHashtable<nsPtrHashKey<nsIFrame> > FrameHashtable;

 protected:
  TextOverflow() {}

  struct AlignmentEdges {
    AlignmentEdges() : mAssigned(false) {}
    void Accumulate(const nsRect& aRect) {
      if (NS_LIKELY(mAssigned)) {
        x = NS_MIN(x, aRect.X());
        xmost = NS_MAX(xmost, aRect.XMost());
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
   */
  void ExamineFrameSubtree(nsIFrame*       aFrame,
                           const nsRect&   aContentArea,
                           const nsRect&   aInsideMarkersArea,
                           FrameHashtable* aFramesToHide,
                           AlignmentEdges* aAlignmentEdges);

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
   */
  void AnalyzeMarkerEdges(nsIFrame*       aFrame,
                          const nsIAtom*  aFrameType,
                          const nsRect&   aInsideMarkersArea,
                          FrameHashtable* aFramesToHide,
                          AlignmentEdges* aAlignmentEdges);

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
   * them into a display list for the block.
   * @param aLine the line we're processing
   * @param aCreateLeft if true, create a marker on the left side
   * @param aCreateRight if true, create a marker on the right side
   * @param aInsideMarkersArea is the area inside the markers
   */
  void CreateMarkers(const nsLineBox* aLine,
                     bool             aCreateLeft,
                     bool             aCreateRight,
                     const nsRect&    aInsideMarkersArea) const;

  nsRect                 mContentArea;
  nsDisplayListBuilder*  mBuilder;
  nsIFrame*              mBlock;
  nsDisplayList*         mMarkerList;
  bool                   mBlockIsRTL;
  bool                   mCanHaveHorizontalScrollbar;

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

    // The intrinsic width of the marker string.
    nscoord                        mWidth;
    // The marker text.
    nsString                       mMarkerString;
    // The style for this side.
    const nsStyleTextOverflowSide* mStyle;
    // True if there is visible overflowing inline content on this side.
    bool                           mHasOverflow;
    // True if mMarkerString and mWidth have been setup from style.
    bool                           mInitialized;
  };

  Marker mLeft;  // the horizontal left marker
  Marker mRight; // the horizontal right marker
};

} // namespace css
} // namespace mozilla

#endif /* !defined(TextOverflow_h_) */
