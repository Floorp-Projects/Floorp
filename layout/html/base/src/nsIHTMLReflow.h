/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIHTMLReflow_h___
#define nsIHTMLReflow_h___

#include "nsIFrameReflow.h"
#include "nsStyleConsts.h"
#include "nsStyleCoord.h"
#include "nsRect.h"
class nsISpaceManager;
class nsBlockFrame;
class nsLineLayout;
struct nsStyleDisplay;
struct nsStylePosition;
struct nsStyleSpacing;

// IID for the nsIHTMLFrame interface 
// a6cf9069-15b3-11d2-932e-00805f8add32
#define NS_IHTMLREFLOW_IID \
 { 0xa6cf9069, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

//----------------------------------------------------------------------

// For HTML reflow we rename with the different paint layers are
// actually used for.
#define NS_FRAME_PAINT_LAYER_BACKGROUND eFramePaintLayer_Underlay
#define NS_FRAME_PAINT_LAYER_FLOATERS   eFramePaintLayer_Content
#define NS_FRAME_PAINT_LAYER_FOREGROUND eFramePaintLayer_Overlay
#define NS_FRAME_PAINT_LAYER_DEBUG      eFramePaintLayer_Overlay

//----------------------------------------------------------------------

/**
 * HTML/CSS specific reflow metrics
 */
struct nsHTMLReflowMetrics : nsReflowMetrics {
  // Carried out bottom margin values. This is the collapsed
  // (generational) bottom margin value.
  nscoord mCarriedOutBottomMargin;

  // For frames that have children that stick outside their rect
  // (NS_FRAME_OUTSIDE_CHILDREN) this rectangle will contain the
  // absolute bounds of the frame. Since the frame doesn't know where
  // it is going to be positioned in its parent, the assumption is
  // that it is placed at 0,0 when computing this area.
  nsRect mCombinedArea;

  nsHTMLReflowMetrics(nsSize* aMaxElementSize)
    : nsReflowMetrics(aMaxElementSize)
  {
    mCarriedOutBottomMargin = 0;
    mCombinedArea.x = 0;
    mCombinedArea.y = 0;
    mCombinedArea.width = 0;
    mCombinedArea.height = 0;
  }

  void AddBorderPaddingToMaxElementSize(const nsMargin& aBorderPadding) {
    maxElementSize->width += aBorderPadding.left + aBorderPadding.right;
    maxElementSize->height += aBorderPadding.top + aBorderPadding.bottom;
  }
};

// Carried out margin flags
#define NS_CARRIED_TOP_MARGIN_IS_AUTO    0x1
#define NS_CARRIED_BOTTOM_MARGIN_IS_AUTO 0x2

//----------------------------------------------------------------------

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
#define NS_CSS_FRAME_TYPE_REPLACED        0x8000

/**
 * Helper macros for telling whether items are replaced
 */
#define NS_FRAME_IS_REPLACED(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED == ((_ft) & NS_CSS_FRAME_TYPE_REPLACED))

#define NS_FRAME_REPLACED(_ft) \
  (NS_CSS_FRAME_TYPE_REPLACED | (_ft))

/**
 * A macro to extract the type. Masks off the 'replaced' bit-flag
 */
#define NS_FRAME_GET_TYPE(_ft) \
  ((_ft) & ~NS_CSS_FRAME_TYPE_REPLACED)

//----------------------------------------------------------------------

#define NS_INTRINSICSIZE  NS_UNCONSTRAINEDSIZE
#define NS_AUTOHEIGHT     NS_UNCONSTRAINEDSIZE

/**
 * HTML version of the reflow state.
 *
 * Note: the constructors are implemented inline later on in this file
 */
struct nsHTMLReflowState : nsReflowState {
  // The type of frame, from css's perspective. This value is
  // initialized by the Init method below.
  nsCSSFrameType   frameType;

  nsISpaceManager* spaceManager;

  // LineLayout object (only for inline reflow; set to NULL otherwise)
  nsLineLayout*    lineLayout;

  // The computed width specifies the frame's content width, and it does not
  // apply to inline non-replaced elements
  //
  // For replaced inline frames, a value of NS_INTRINSICSIZE means you should
  // use your intrinsic width as the computed width
  //
  // For block-level frames, the computed width is based on the width of the
  // containing block and the margin/border/padding areas and the min/max
  // width
  nscoord          computedWidth; 

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
  nscoord          computedHeight;

  // Computed margin values
  nsMargin         computedMargin;

  // Cached copy of the border values
  nsMargin         mComputedBorderPadding;

  // Computed padding values
  nsMargin         mComputedPadding;

  // Computed values for 'left/top/right/bottom' offsets. Only applies to
  // 'positioned' elements
  nsMargin         computedOffsets;

  // Computed values for 'min-width/max-width' and 'min-height/max-height'
  nscoord          mComputedMinWidth, mComputedMaxWidth;
  nscoord          mComputedMinHeight, mComputedMaxHeight;

  // Compact margin available space
  nscoord          mCompactMarginWidth;

  // The following data members are relevant if nsStyleText.mTextAlign
  // == NS_STYLE_TEXT_ALIGN_CHAR

  // distance from reference edge (as specified in nsStyleDisplay.mDirection) 
  // to the align character (which will be specified in nsStyleTable)
  nscoord          mAlignCharOffset;

  // if true, the reflow honors alignCharOffset and does not
  // set it. if false, the reflow sets alignCharOffset
  PRPackedBool     mUseAlignCharOffset;

  // Cached pointers to the various style structs used during intialization
  const nsStyleDisplay* mStyleDisplay;
  const nsStylePosition* mStylePosition;
  const nsStyleSpacing* mStyleSpacing;

  // Note: The copy constructor is written by the compiler
  // automatically. You can use that and then override specific values
  // if you want, or you can call Init as desired...

  // Initialize a <b>root</b> reflow state with a rendering context to
  // use for measuring things.
  nsHTMLReflowState(nsIPresContext&          aPresContext,
                    nsIFrame*                aFrame,
                    nsReflowReason           aReason,
                    nsIRenderingContext*     aRenderingContext,
                    const nsSize&            aAvailableSpace);

  // Initialize a <b>root</b> reflow state for an <b>incremental</b>
  // reflow.
  nsHTMLReflowState(nsIPresContext&          aPresContext,
                    nsIFrame*                aFrame,
                    nsIReflowCommand&        aReflowCommand,
                    nsIRenderingContext*     aRenderingContext,
                    const nsSize&            aAvailableSpace);

  // Initialize a reflow state for a child frames reflow. Some state
  // is copied from the parent reflow state; the remaining state is
  // computed.
  nsHTMLReflowState(nsIPresContext&          aPresContext,
                    const nsHTMLReflowState& aParentReflowState,
                    nsIFrame*                aFrame,
                    const nsSize&            aAvailableSpace,
                    nsReflowReason           aReason);

  // Same as the previous except that the reason is taken from the
  // parent's reflow state.
  nsHTMLReflowState(nsIPresContext&          aPresContext,
                    const nsHTMLReflowState& aParentReflowState,
                    nsIFrame*                aFrame,
                    const nsSize&            aAvailableSpace);

  /**
   * Returns PR_TRUE if the specified width or height has an value other
   * than 'auto'
   */
  PRBool HaveFixedContentWidth() const;
  PRBool HaveFixedContentHeight() const;

  /**
   * Get the containing block reflow state, starting from a frames
   * <B>parent</B> reflow state (the parent reflow state may or may not end
   * up being the containing block reflow state)
   */
  static const nsHTMLReflowState*
    GetContainingBlockReflowState(const nsReflowState* aParentRS);

  /**
   * First find the containing block's reflow state using
   * GetContainingBlockReflowState, then ask the containing block for
   * it's content width using GetContentWidth
   */
  static nscoord
    GetContainingBlockContentWidth(const nsReflowState* aParentRS);

  /**
   * Get the page box reflow state, starting from a frames
   * <B>parent</B> reflow state (the parent reflow state may or may not end
   * up being the containing block reflow state)
   */
  static const nsHTMLReflowState*
    GetPageBoxReflowState(const nsReflowState* aParentRS);

  /**
   * Compute the border plus padding for <TT>aFrame</TT>. If a
   * percentage needs to be computed it will be computed by finding
   * the containing block, use GetContainingBlockReflowState.
   * aParentReflowState is aFrame's
   * parent's reflow state. The resulting computed border plus padding
   * is returned in aResult.
   */
  static void ComputeBorderPaddingFor(nsIFrame* aFrame,
                                      const nsReflowState* aParentRS,
                                      nsMargin& aResult);

  /**
   * Calculate the raw line-height property for the given frame. The return
   * value, if line-height was applied and is valid will be >= 0. Otherwise,
   * the return value will be <0 which is illegal (CSS2 spec: section 10.8.1).
   */
  static nscoord CalcLineHeight(nsIPresContext& aPresContext,
                                nsIFrame* aFrame);

  static nsCSSFrameType DetermineFrameType(nsIFrame* aFrame);

protected:
  // This method initializes various data members. It is automatically
  // called by the various constructors
  void Init(nsIPresContext& aPresContext);

  void InitConstraints(nsIPresContext& aPresContext);

  void InitAbsoluteConstraints(nsIPresContext& aPresContext,
                               const nsHTMLReflowState* cbrs,
                               nscoord aContainingBlockWidth,
                               nscoord aContainingBlockHeight);

  void ComputeRelativeOffsets(const nsHTMLReflowState* cbrs);

  void ComputeBlockBoxData(nsIPresContext& aPresContext,
                           const nsHTMLReflowState* cbrs,
                           nsStyleUnit aWidthUnit,
                           nsStyleUnit aHeightUnit,
                           nscoord aContainingBlockWidth,
                           nscoord aContainingBlockHeight);

  void CalculateLeftRightMargin(const nsHTMLReflowState* aContainingBlockRS,
                                nscoord                  aComputedWidth);

  void ComputeHorizontalValue(nscoord aContainingBlockWidth,
                                     nsStyleUnit aUnit,
                                     const nsStyleCoord& aCoord,
                                     nscoord& aResult);

  void ComputeVerticalValue(nscoord aContainingBlockHeight,
                                   nsStyleUnit aUnit,
                                   const nsStyleCoord& aCoord,
                                   nscoord& aResult);

  static nsCSSFrameType DetermineFrameType(nsIFrame* aFrame,
                                           const nsStylePosition* aPosition,
                                           const nsStyleDisplay* aDisplay);

  // Computes margin values from the specified margin style information, and
  // fills in the mComputedMargin member
  void ComputeMargin(nscoord aContainingBlockWidth,
                     const nsHTMLReflowState* aContainingBlockRS);
  
  // Computes padding values from the specified padding style information, and
  // fills in the mComputedPadding member
  void ComputePadding(nscoord aContainingBlockWidth,
                      const nsHTMLReflowState* aContainingBlockRS);

  // Calculates the computed values for the 'min-Width', 'max-Width',
  // 'min-Height', and 'max-Height' properties, and stores them in the assorted
  // data members
  void ComputeMinMaxValues(nscoord                  aContainingBlockWidth,
                           nscoord                  aContainingBlockHeight,
                           const nsHTMLReflowState* aContainingBlockRS);

};

//----------------------------------------------------------------------

/**
 * Extensions to the reflow status bits defined by nsIFrameReflow
 */

// This bit is set, when a break is requested. This bit is orthogonal
// to the nsIFrame::nsReflowStatus completion bits.
#define NS_INLINE_BREAK              0x0100

// When a break is requested, this bit when set indicates that the
// break should occur after the frame just reflowed; when the bit is
// clear the break should occur before the frame just reflowed.
#define NS_INLINE_BREAK_BEFORE       0x0000
#define NS_INLINE_BREAK_AFTER        0x0200

// The type of break requested can be found in these bits.
#define NS_INLINE_BREAK_TYPE_MASK    0xF000

//----------------------------------------
// Macros that use those bits

#define NS_INLINE_IS_BREAK(_status) \
  (0 != ((_status) & NS_INLINE_BREAK))

#define NS_INLINE_IS_BREAK_AFTER(_status) \
  (0 != ((_status) & NS_INLINE_BREAK_AFTER))

#define NS_INLINE_IS_BREAK_BEFORE(_status) \
  (NS_INLINE_BREAK == ((_status) & (NS_INLINE_BREAK|NS_INLINE_BREAK_AFTER)))

#define NS_INLINE_GET_BREAK_TYPE(_status) (((_status) >> 12) & 0xF)

#define NS_INLINE_MAKE_BREAK_TYPE(_type)  ((_type) << 12)

// Construct a line-break-before status. Note that there is no
// completion status for a line-break before because we *know* that
// the frame will be reflowed later and hence it's current completion
// status doesn't matter.
#define NS_INLINE_LINE_BREAK_BEFORE()                                   \
  (NS_INLINE_BREAK | NS_INLINE_BREAK_BEFORE |                           \
   NS_INLINE_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

// Take a completion status and add to it the desire to have a
// line-break after. For this macro we do need the completion status
// because the user of the status will need to know whether to
// continue the frame or not.
#define NS_INLINE_LINE_BREAK_AFTER(_completionStatus)                   \
  ((_completionStatus) | NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |      \
   NS_INLINE_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

//----------------------------------------------------------------------

/**
 * Generate a reflow interface specific to HTML/CSS frame objects
 */
class nsIHTMLReflow : public nsIFrameReflow<nsHTMLReflowState, nsHTMLReflowMetrics>
{
public:
  // Helper method used by block reflow to identify runs of text so that
  // proper word-breaking can be done.
  NS_IMETHOD FindTextRuns(nsLineLayout& aLineLayout) = 0;

  // Justification helper method used to distribute extra space in a
  // line to leaf frames. aUsedSpace is filled in with the amount of
  // space actually used.
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace) = 0;

  // Justification helper method that is used to remove trailing
  // whitespace before justification.
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth) = 0;

  // Any objects in the frame that impact the spacemanager (e.g. a
  // floater) are to be moved in the spacemanager by the given delta
  // values.
  NS_IMETHOD MoveInSpaceManager(nsIPresContext* aPresContext,
                                nsISpaceManager* aSpaceManager,
                                nscoord aDeltaX, nscoord aDeltaY) = 0;
};

#endif /* nsIHTMLReflow_h___ */
