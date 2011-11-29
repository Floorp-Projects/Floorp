/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* struct containing the input to nsIFrame::Reflow */

#ifndef nsHTMLReflowState_h___
#define nsHTMLReflowState_h___

#include "nsMargin.h"
#include "nsStyleCoord.h"
#include "nsIFrame.h"

class nsPresContext;
class nsRenderingContext;
class nsFloatManager;
class nsLineLayout;
class nsIPercentHeightObserver;

struct nsStyleDisplay;
struct nsStyleVisibility;
struct nsStylePosition;
struct nsStyleBorder;
struct nsStyleMargin;
struct nsStylePadding;
struct nsStyleText;
struct nsHypotheticalBox;

template <class NumericType>
NumericType
NS_CSS_MINMAX(NumericType aValue, NumericType aMinValue, NumericType aMaxValue)
{
  NumericType result = aValue;
  if (aMaxValue < result)
    result = aMaxValue;
  if (aMinValue > result)
    result = aMinValue;
  return result;
}

/**
 * Constant used to indicate an unconstrained size.
 *
 * @see #Reflow()
 */
#define NS_UNCONSTRAINEDSIZE NS_MAXSIZE

/**
 * CSS Frame type. Included as part of the reflow state.
 */
typedef PRUint32  nsCSSFrameType;

#define NS_CSS_FRAME_TYPE_UNKNOWN         0
#define NS_CSS_FRAME_TYPE_INLINE          1
#define NS_CSS_FRAME_TYPE_BLOCK           2  /* block-level in normal flow */
#define NS_CSS_FRAME_TYPE_FLOATING        3
#define NS_CSS_FRAME_TYPE_ABSOLUTE        4
#define NS_CSS_FRAME_TYPE_INTERNAL_TABLE  5  /* row group frame, row frame, cell frame, ... */

/**
 * Bit-flag that indicates whether the element is replaced. Applies to inline,
 * block-level, floating, and absolutely positioned elements
 */
#define NS_CSS_FRAME_TYPE_REPLACED                0x08000

/**
 * Bit-flag that indicates that the element is replaced and contains a block
 * (eg some form controls).  Applies to inline, block-level, floating, and
 * absolutely positioned elements.  Mutually exclusive with
 * NS_CSS_FRAME_TYPE_REPLACED.
 */
#define NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK 0x10000

/**
 * Helper macros for telling whether items are replaced
 */
#define NS_FRAME_IS_REPLACED_NOBLOCK(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED == ((_ft) & NS_CSS_FRAME_TYPE_REPLACED))

#define NS_FRAME_IS_REPLACED(_ft)            \
  (NS_FRAME_IS_REPLACED_NOBLOCK(_ft) ||      \
   NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(_ft))

#define NS_FRAME_REPLACED(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED | (_ft))

#define NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(_ft)         \
  (NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK ==         \
   ((_ft) & NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK))

#define NS_FRAME_REPLACED_CONTAINS_BLOCK(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK | (_ft))

/**
 * A macro to extract the type. Masks off the 'replaced' bit-flag
 */
#define NS_FRAME_GET_TYPE(_ft)                           \
  ((_ft) & ~(NS_CSS_FRAME_TYPE_REPLACED |                \
             NS_CSS_FRAME_TYPE_REPLACED_CONTAINS_BLOCK))

#define NS_INTRINSICSIZE    NS_UNCONSTRAINEDSIZE
#define NS_AUTOHEIGHT       NS_UNCONSTRAINEDSIZE
#define NS_AUTOMARGIN       NS_UNCONSTRAINEDSIZE
#define NS_AUTOOFFSET       NS_UNCONSTRAINEDSIZE
// NOTE: there are assumptions all over that these have the same value, namely NS_UNCONSTRAINEDSIZE
//       if any are changed to be a value other than NS_UNCONSTRAINEDSIZE
//       at least update AdjustComputedHeight/Width and test ad nauseum

// A base class of nsHTMLReflowState that computes only the padding,
// border, and margin, since those values are needed more often.
struct nsCSSOffsetState {
public:
  // the frame being reflowed
  nsIFrame*           frame;

  // rendering context to use for measurement
  nsRenderingContext* rendContext;

  // Computed margin values
  nsMargin         mComputedMargin;

  // Cached copy of the border + padding values
  nsMargin         mComputedBorderPadding;

  // Computed padding values
  nsMargin         mComputedPadding;

  // Callers using this constructor must call InitOffsets on their own.
  nsCSSOffsetState(nsIFrame *aFrame, nsRenderingContext *aRenderingContext)
    : frame(aFrame)
    , rendContext(aRenderingContext)
  {
  }

  nsCSSOffsetState(nsIFrame *aFrame, nsRenderingContext *aRenderingContext,
                   nscoord aContainingBlockWidth)
    : frame(aFrame)
    , rendContext(aRenderingContext)
  {
    InitOffsets(aContainingBlockWidth, frame->GetType());
  }

#ifdef DEBUG
  // Reflow trace methods.  Defined in nsFrame.cpp so they have access
  // to the display-reflow infrastructure.
  static void* DisplayInitOffsetsEnter(nsIFrame* aFrame,
                                       nsCSSOffsetState* aState,
                                       nscoord aCBWidth,
                                       const nsMargin* aBorder,
                                       const nsMargin* aPadding);
  static void DisplayInitOffsetsExit(nsIFrame* aFrame,
                                     nsCSSOffsetState* aState,
                                     void* aValue);
#endif

private:
  /**
   * Computes margin values from the specified margin style information, and
   * fills in the mComputedMargin member.
   * @return true if the margin is dependent on the containing block width
   */
  bool ComputeMargin(nscoord aContainingBlockWidth);
  
  /**
   * Computes padding values from the specified padding style information, and
   * fills in the mComputedPadding member.
   * @return true if the padding is dependent on the containing block width
   */
   bool ComputePadding(nscoord aContainingBlockWidth, nsIAtom* aFrameType);

protected:

  void InitOffsets(nscoord aContainingBlockWidth,
                   nsIAtom* aFrameType,
                   const nsMargin *aBorder = nsnull,
                   const nsMargin *aPadding = nsnull);

  /*
   * Convert nsStyleCoord to nscoord when percentages depend on the
   * containing block width, and enumerated values are for width,
   * min-width, or max-width.  Does not handle auto widths.
   */
  inline nscoord ComputeWidthValue(nscoord aContainingBlockWidth,
                                   nscoord aContentEdgeToBoxSizing,
                                   nscoord aBoxSizingToMarginEdge,
                                   const nsStyleCoord& aCoord);
  // same as previous, but using mComputedBorderPadding, mComputedPadding,
  // and mComputedMargin
  nscoord ComputeWidthValue(nscoord aContainingBlockWidth,
                            PRUint8 aBoxSizing,
                            const nsStyleCoord& aCoord);
};

/**
 * State passed to a frame during reflow or intrinsic size calculation.
 *
 * XXX Refactor so only a base class (nsSizingState?) is used for intrinsic
 * size calculation.
 *
 * @see nsIFrame#Reflow()
 */
struct nsHTMLReflowState : public nsCSSOffsetState {
  // the reflow states are linked together. this is the pointer to the
  // parent's reflow state
  const nsHTMLReflowState* parentReflowState;

  // pointer to the float manager associated with this area
  nsFloatManager* mFloatManager;

  // LineLayout object (only for inline reflow; set to NULL otherwise)
  nsLineLayout*    mLineLayout;

  // The appropriate reflow state for the containing block (for
  // percentage widths, etc.) of this reflow state's frame.
  const nsHTMLReflowState *mCBReflowState;

  // the available width in which to reflow the frame. The space
  // represents the amount of room for the frame's margin, border,
  // padding, and content area. The frame size you choose should fit
  // within the available width.
  nscoord              availableWidth;

  // A value of NS_UNCONSTRAINEDSIZE for the available height means
  // you can choose whatever size you want. In galley mode the
  // available height is always NS_UNCONSTRAINEDSIZE, and only page
  // mode or multi-column layout involves a constrained height. The
  // element's the top border and padding, and content, must fit. If the
  // element is complete after reflow then its bottom border, padding
  // and margin (and similar for its complete ancestors) will need to
  // fit in this height.
  nscoord              availableHeight;

  // The type of frame, from css's perspective. This value is
  // initialized by the Init method below.
  nsCSSFrameType   mFrameType;

  // The amount the in-flow position of the block is moving vertically relative
  // to its previous in-flow position (i.e. the amount the line containing the
  // block is moving).
  // This should be zero for anything which is not a block outside, and it
  // should be zero for anything which has a non-block parent.
  // The intended use of this value is to allow the accurate determination
  // of the potential impact of a float
  // This takes on an arbitrary value the first time a block is reflowed
  nscoord mBlockDelta;

private:
  // The computed width specifies the frame's content area width, and it does
  // not apply to inline non-replaced elements
  //
  // For replaced inline frames, a value of NS_INTRINSICSIZE means you should
  // use your intrinsic width as the computed width
  //
  // For block-level frames, the computed width is based on the width of the
  // containing block, the margin/border/padding areas, and the min/max width.
  nscoord          mComputedWidth; 

  // The computed height specifies the frame's content height, and it does
  // not apply to inline non-replaced elements
  //
  // For replaced inline frames, a value of NS_INTRINSICSIZE means you should
  // use your intrinsic height as the computed height
  //
  // For non-replaced block-level frames in the flow and floated, a value of
  // NS_AUTOHEIGHT means you choose a height to shrink wrap around the normal
  // flow child frames. The height must be within the limit of the min/max
  // height if there is such a limit
  //
  // For replaced block-level frames, a value of NS_INTRINSICSIZE
  // means you use your intrinsic height as the computed height
  nscoord          mComputedHeight;

public:
  // Computed values for 'left/top/right/bottom' offsets. Only applies to
  // 'positioned' elements
  nsMargin         mComputedOffsets;

  // Computed values for 'min-width/max-width' and 'min-height/max-height'
  // XXXldb The width ones here should go; they should be needed only
  // internally.
  nscoord          mComputedMinWidth, mComputedMaxWidth;
  nscoord          mComputedMinHeight, mComputedMaxHeight;

  // Cached pointers to the various style structs used during intialization
  const nsStyleDisplay*    mStyleDisplay;
  const nsStyleVisibility* mStyleVisibility;
  const nsStylePosition*   mStylePosition;
  const nsStyleBorder*     mStyleBorder;
  const nsStyleMargin*     mStyleMargin;
  const nsStylePadding*    mStylePadding;
  const nsStyleText*       mStyleText;

  // a frame (e.g. nsTableCellFrame) which may need to generate a special 
  // reflow for percent height calculations 
  nsIPercentHeightObserver* mPercentHeightObserver;

  // CSS margin collapsing sometimes requires us to reflow
  // optimistically assuming that margins collapse to see if clearance
  // is required. When we discover that clearance is required, we
  // store the frame in which clearance was discovered to the location
  // requested here.
  nsIFrame** mDiscoveredClearance;

  // This value keeps track of how deeply nested a given reflow state
  // is from the top of the frame tree.
  PRInt16 mReflowDepth;

  struct ReflowStateFlags {
    PRUint16 mSpecialHeightReflow:1; // used by tables to communicate special reflow (in process) to handle
                                     // percent height frames inside cells which may not have computed heights
    PRUint16 mNextInFlowUntouched:1; // nothing in the frame's next-in-flow (or its descendants)
                                     // is changing
    PRUint16 mIsTopOfPage:1;         // Is the current context at the top of a
                                     // page?  When true, we force something
                                     // that's too tall for a page/column to
                                     // fit anyway to avoid infinite loops.
    PRUint16 mBlinks:1;              // Keep track of text-decoration: blink
    PRUint16 mHasClearance:1;        // Block has clearance
    PRUint16 mAssumingHScrollbar:1;  // parent frame is an nsIScrollableFrame and it
                                     // is assuming a horizontal scrollbar
    PRUint16 mAssumingVScrollbar:1;  // parent frame is an nsIScrollableFrame and it
                                     // is assuming a vertical scrollbar

    PRUint16 mHResize:1;             // Is frame (a) not dirty and (b) a
                                     // different width than before?

    PRUint16 mVResize:1;             // Is frame (a) not dirty and (b) a
                                     // different height than before or
                                     // (potentially) in a context where
                                     // percent heights have a different
                                     // basis?
    PRUint16 mTableIsSplittable:1;   // tables are splittable, this should happen only inside a page
                                     // and never insider a column frame
    PRUint16 mHeightDependsOnAncestorCell:1;   // Does frame height depend on
                                               // an ancestor table-cell?
    
  } mFlags;

  // Note: The copy constructor is written by the compiler automatically. You
  // can use that and then override specific values if you want, or you can
  // call Init as desired...

  // Initialize a <b>root</b> reflow state with a rendering context to
  // use for measuring things.
  nsHTMLReflowState(nsPresContext*           aPresContext,
                    nsIFrame*                aFrame,
                    nsRenderingContext*     aRenderingContext,
                    const nsSize&            aAvailableSpace);

  // Initialize a reflow state for a child frames reflow. Some state
  // is copied from the parent reflow state; the remaining state is
  // computed. 
  nsHTMLReflowState(nsPresContext*           aPresContext,
                    const nsHTMLReflowState& aParentReflowState,
                    nsIFrame*                aFrame,
                    const nsSize&            aAvailableSpace,
                    // These two are used by absolute positioning code
                    // to override default containing block w & h:
                    nscoord                  aContainingBlockWidth = -1,
                    nscoord                  aContainingBlockHeight = -1,
                    bool                     aInit = true);

  // This method initializes various data members. It is automatically
  // called by the various constructors
  void Init(nsPresContext* aPresContext,
            nscoord         aContainingBlockWidth = -1,
            nscoord         aContainingBlockHeight = -1,
            const nsMargin* aBorder = nsnull,
            const nsMargin* aPadding = nsnull);
  /**
   * Find the content width of the containing block of aReflowState
   */
  static nscoord
    GetContainingBlockContentWidth(const nsHTMLReflowState* aReflowState);

  /**
   * Calculate the used line-height property. The return value will be >= 0.
   */
  nscoord CalcLineHeight() const;

  /**
   * Same as CalcLineHeight() above, but doesn't need a reflow state.
   *
   * @param aBlockHeight The computed height of the content rect of the block
   *                     that the line should fill.
   *                     Only used with line-height:-moz-block-height.
   *                     NS_AUTOHEIGHT results in a normal line-height for
   *                     line-height:-moz-block-height.
   * @param aFontSizeInflation The result of the appropriate
   *                           nsLayoutUtils::FontSizeInflationFor call,
   *                           or 1.0 if during intrinsic size
   *                           calculation.
   */
  static nscoord CalcLineHeight(nsStyleContext* aStyleContext,
                                nscoord aBlockHeight,
                                float aFontSizeInflation);


  void ComputeContainingBlockRectangle(nsPresContext*          aPresContext,
                                       const nsHTMLReflowState* aContainingBlockRS,
                                       nscoord&                 aContainingBlockWidth,
                                       nscoord&                 aContainingBlockHeight);

  /**
   * Apply the mComputed(Min/Max)(Width/Height) values to the content
   * size computed so far. If a passed-in pointer is null, we skip
   * adjusting that dimension.
   */
  void ApplyMinMaxConstraints(nscoord* aContentWidth, nscoord* aContentHeight) const;

  bool ShouldReflowAllKids() const {
    // Note that we could make a stronger optimization for mVResize if
    // we use it in a ShouldReflowChild test that replaces the current
    // checks of NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN, if it
    // were tested there along with NS_FRAME_CONTAINS_RELATIVE_HEIGHT.
    // This would need to be combined with a slight change in which
    // frames NS_FRAME_CONTAINS_RELATIVE_HEIGHT is marked on.
    return (frame->GetStateBits() & NS_FRAME_IS_DIRTY) ||
           mFlags.mHResize ||
           (mFlags.mVResize && 
            (frame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT));
  }

  nscoord ComputedWidth() const { return mComputedWidth; }
  // This method doesn't apply min/max computed widths to the value passed in.
  void SetComputedWidth(nscoord aComputedWidth);

  nscoord ComputedHeight() const { return mComputedHeight; }
  // This method doesn't apply min/max computed heights to the value passed in.
  void SetComputedHeight(nscoord aComputedHeight);

  void SetComputedHeightWithoutResettingResizeFlags(nscoord aComputedHeight) {
    // Viewport frames reset the computed height on a copy of their reflow
    // state when reflowing fixed-pos kids.  In that case we actually don't
    // want to mess with the resize flags, because comparing the frame's rect
    // to the munged computed width is pointless.
    mComputedHeight = aComputedHeight;
  }

  void SetTruncated(const nsHTMLReflowMetrics& aMetrics, nsReflowStatus* aStatus) const;

  bool WillReflowAgainForClearance() const {
    return mDiscoveredClearance && *mDiscoveredClearance;
  }

#ifdef DEBUG
  // Reflow trace methods.  Defined in nsFrame.cpp so they have access
  // to the display-reflow infrastructure.
  static void* DisplayInitConstraintsEnter(nsIFrame* aFrame,
                                           nsHTMLReflowState* aState,
                                           nscoord aCBWidth,
                                           nscoord aCBHeight,
                                           const nsMargin* aBorder,
                                           const nsMargin* aPadding);
  static void DisplayInitConstraintsExit(nsIFrame* aFrame,
                                         nsHTMLReflowState* aState,
                                         void* aValue);
  static void* DisplayInitFrameTypeEnter(nsIFrame* aFrame,
                                         nsHTMLReflowState* aState);
  static void DisplayInitFrameTypeExit(nsIFrame* aFrame,
                                       nsHTMLReflowState* aState,
                                       void* aValue);
#endif

protected:
  void InitFrameType(nsIAtom* aFrameType);
  void InitCBReflowState();
  void InitResizeFlags(nsPresContext* aPresContext, nsIAtom* aFrameType);

  void InitConstraints(nsPresContext* aPresContext,
                       nscoord         aContainingBlockWidth,
                       nscoord         aContainingBlockHeight,
                       const nsMargin* aBorder,
                       const nsMargin* aPadding,
                       nsIAtom*        aFrameType);

  // Returns the nearest containing block or block frame (whether or not
  // it is a containing block) for the specified frame.  Also returns
  // the left edge and width of the containing block's content area.
  // These are returned in the coordinate space of the containing block.
  nsIFrame* GetHypotheticalBoxContainer(nsIFrame* aFrame,
                                        nscoord& aCBLeftEdge,
                                        nscoord& aCBWidth);

  void CalculateHypotheticalBox(nsPresContext*    aPresContext,
                                nsIFrame*         aPlaceholderFrame,
                                nsIFrame*         aContainingBlock,
                                nscoord           aBlockLeftContentEdge,
                                nscoord           aBlockContentWidth,
                                const nsHTMLReflowState* cbrs,
                                nsHypotheticalBox& aHypotheticalBox,
                                nsIAtom*          aFrameType);

  void InitAbsoluteConstraints(nsPresContext* aPresContext,
                               const nsHTMLReflowState* cbrs,
                               nscoord aContainingBlockWidth,
                               nscoord aContainingBlockHeight,
                               nsIAtom* aFrameType);

  void ComputeRelativeOffsets(const nsHTMLReflowState* cbrs,
                              nscoord aContainingBlockWidth,
                              nscoord aContainingBlockHeight,
                              nsPresContext* aPresContext);

  // Calculates the computed values for the 'min-Width', 'max-Width',
  // 'min-Height', and 'max-Height' properties, and stores them in the assorted
  // data members
  void ComputeMinMaxValues(nscoord                  aContainingBlockWidth,
                           nscoord                  aContainingBlockHeight,
                           const nsHTMLReflowState* aContainingBlockRS);

  void CalculateHorizBorderPaddingMargin(nscoord aContainingBlockWidth,
                                         nscoord* aInsideBoxSizing,
                                         nscoord* aOutsideBoxSizing);

  void CalculateBlockSideMargins(nscoord aAvailWidth,
                                 nscoord aComputedWidth,
                                 nsIAtom* aFrameType);
};

#endif /* nsHTMLReflowState_h___ */

