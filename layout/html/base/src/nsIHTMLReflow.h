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
class nsISpaceManager;
class nsLineLayout;

// IID for the nsIHTMLFrame interface 
// a6cf9069-15b3-11d2-932e-00805f8add32
#define NS_IHTMLREFLOW_IID \
 { 0xa6cf9069, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

//----------------------------------------------------------------------

/**
 * HTML/CSS specific reflow metrics
 */
struct nsHTMLReflowMetrics : nsReflowMetrics {
  // Carried out top/bottom margin values. This is the top and bottom
  // margin values from a frames first/last child.
  nscoord mCarriedOutTopMargin;
  nscoord mCarriedOutBottomMargin;

  // Carried out margin values. If the top/bottom margin values were
  // computed auto values then the corresponding bits are set in this
  // value.
  PRUintn mCarriedOutMarginFlags;

  nsHTMLReflowMetrics(nsSize* aMaxElementSize)
    : nsReflowMetrics(aMaxElementSize)
  {
    mCarriedOutTopMargin = 0;
    mCarriedOutBottomMargin = 0;
    mCarriedOutMarginFlags = 0;
  }
};

// Carried out margin flags
#define NS_CARRIED_TOP_MARGIN_IS_AUTO    0x1
#define NS_CARRIED_BOTTOM_MARGIN_IS_AUTO 0x2

//----------------------------------------------------------------------

/**
 * The type of size constraint that applies to a particular dimension.
 * For the fixed and fixed content cases the min size in the reflow state
 * structure is ignored and you should use the max size value when reflowing
 * the frame.
 *
 * @see nsHTMLReflowState
 */
//XXX enum's are prefixed wrong
enum nsReflowConstraint {
  eReflowSize_Unconstrained = 0,  // choose whatever frame size you want
  eReflowSize_Constrained = 1,    // choose a frame size between the min and max sizes
  eReflowSize_Fixed = 2,          // frame size is fixed
  eReflowSize_FixedContent = 3    // size of your content area is fixed
};

//----------------------------------------------------------------------

/**
 * Frame type. Included as part of the reflow state.
 *
 * @see nsHTMLReflowState
 *
 * XXX This requires some more thought. Are these the correct set?
 * XXX Should we treat 'replaced' as a bit flag instead of doubling the
 *     number of enumerators?
 * XXX Should the name be nsCSSReflowFrameType?
 */
enum nsReflowFrameType {
  eReflowType_Inline = 0,           // inline, non-replaced elements
  eReflowType_InlineReplaced = 1,   // inline, replaced elements (e.g., image)
  eReflowType_Block = 2,            // block-level, non-replaced elements in normal flow
  eReflowType_BlockReplaced = 3,    // block-level, replaced elements in normal flow
  eReflowType_Floating = 4,         // floating, non-replaced elements
  eReflowType_FloatingReplaced = 5, // floating, replaced elements
  eReflowType_Absolute = 6,         // absolutely positioned, non-replaced elements
  eReflowType_AbsoluteReplaced = 7  // absolutely positioned, replaced elements
};

//----------------------------------------------------------------------

struct nsHTMLReflowState : nsReflowState {
  nsReflowFrameType   frameType;
  nsISpaceManager*    spaceManager;
  nsLineLayout*       lineLayout;  // only for inline reflow (set to NULL otherwise)

  // XXX None of this is currently being used...
#if 0
  nsReflowConstraint   widthConstraint;   // constraint that applies to width dimension
  nsReflowConstraint   heightConstraint;  // constraint that applies to height dimension
  nsSize               minSize;           // the min available space in which to reflow.
                                          // Only used for eReflowSize_Constrained
#endif

  // Constructs an initial reflow state (no parent reflow state) for a
  // non-incremental reflow command. Sets reflowType to eReflowType_Block
  nsHTMLReflowState(nsIFrame*            aFrame,
                    nsReflowReason       aReason, 
                    const nsSize&        aMaxSize,
                    nsIRenderingContext* aContext,
                    nsISpaceManager*     aSpaceManager = nsnull);

  // Constructs an initial reflow state (no parent reflow state) for an
  // incremental reflow command. Sets reflowType to eReflowType_Block
  nsHTMLReflowState(nsIFrame*            aFrame,
                    nsIReflowCommand&    aReflowCommand,
                    const nsSize&        aMaxSize,
                    nsIRenderingContext* aContext,
                    nsISpaceManager*     aSpaceManager = nsnull);

  // Construct a reflow state for the given frame, parent reflow state, and
  // max size. Uses the reflow reason, space manager, reflow command, and
  // line layout from the parent's reflow state.  Defaults to a reflow
  // frame type of eReflowType_Block
  nsHTMLReflowState(nsIFrame*                aFrame,
                    const nsHTMLReflowState& aParentReflowState,
                    const nsSize&            aMaxSize,
                    nsReflowFrameType        aFrameType = eReflowType_Block);

  // Construct a reflow state for the given inline frame, parent
  // reflow state, and max size. Uses the reflow reason, space
  // manager, and reflow command from the parent's reflow state. Sets
  // the reflow frame type to eReflowType_Inline
  nsHTMLReflowState(nsIFrame*                aFrame,
                    const nsHTMLReflowState& aParentReflowState,
                    const nsSize&            aMaxSize,
                    nsLineLayout*            aLineLayout);

  // Constructs a reflow state that overrides the reflow reason of the parent
  // reflow state. Uses the space manager from the parent's reflow state and
  // sets the reflow command to NULL. Sets lineLayout to NULL, and defaults to
  // a reflow frame type of eReflowType_Block
  nsHTMLReflowState(nsIFrame*                aFrame,
                    const nsHTMLReflowState& aParentReflowState,
                    const nsSize&            aMaxSize,
                    nsReflowReason           aReflowReason,
                    nsReflowFrameType        aFrameType = eReflowType_Block);
};

//----------------------------------------------------------------------

/**
 * Extensions to the reflow status bits defined by nsIFrameReflow
 */

// This bit is set, when a break is requested. This bit is orthogonal
// to the nsIFrame::nsReflowStatus completion bits.
#define NS_INLINE_BREAK             0x0100

#define NS_INLINE_IS_BREAK(_status) \
  (0 != ((_status) & NS_INLINE_BREAK))

// When a break is requested, this bit when set indicates that the
// break should occur after the frame just reflowed; when the bit is
// clear the break should occur before the frame just reflowed.
#define NS_INLINE_BREAK_BEFORE      0x0000
#define NS_INLINE_BREAK_AFTER       0x0200

#define NS_INLINE_IS_BREAK_AFTER(_status) \
  (0 != ((_status) & NS_INLINE_BREAK_AFTER))

#define NS_INLINE_IS_BREAK_BEFORE(_status) \
  (NS_INLINE_BREAK == ((_status) & (NS_INLINE_BREAK|NS_INLINE_BREAK_AFTER)))

// The type of break requested can be found in these bits.
#define NS_INLINE_BREAK_TYPE_MASK   0xF000

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
};

//----------------------------------------------------------------------

// Constructs an initial reflow state (no parent reflow state) for a
// non-incremental reflow command. Sets reflowType to eReflowType_Block
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*            aFrame,
                                     nsReflowReason       aReason, 
                                     const nsSize&        aMaxSize,
                                     nsIRenderingContext* aContext,
                                     nsISpaceManager*     aSpaceManager)
  : nsReflowState(aFrame, aReason, aMaxSize, aContext)
{
  frameType = eReflowType_Block;
  spaceManager = aSpaceManager;
  lineLayout = nsnull;
}

// Constructs an initial reflow state (no parent reflow state) for an
// incremental reflow command. Sets reflowType to eReflowType_Block
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*            aFrame,
                                     nsIReflowCommand&    aReflowCommand,
                                     const nsSize&        aMaxSize,
                                     nsIRenderingContext* aContext,
                                     nsISpaceManager*     aSpaceManager)
  : nsReflowState(aFrame, aReflowCommand, aMaxSize, aContext)
{
  frameType = eReflowType_Block;
  spaceManager = aSpaceManager;
  lineLayout = nsnull;
}

// Construct a reflow state for the given frame, parent reflow state, and
// max size. Uses the reflow reason, space manager, reflow command, and
// line layout from the parent's reflow state.  Defaults to a reflow
// frame type of eReflowType_Block
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*                aFrame,
                                     const nsHTMLReflowState& aParentReflowState,
                                     const nsSize&            aMaxSize,
                                     nsReflowFrameType        aFrameType)
  : nsReflowState(aFrame, aParentReflowState, aMaxSize)
{
  frameType = aFrameType;
  spaceManager = aParentReflowState.spaceManager;
  lineLayout = aParentReflowState.lineLayout;
}

// Construct a reflow state for the given inline frame, parent reflow state,
// and max size. Uses the reflow reason, space manager, and reflow command from
// the parent's reflow state. Sets the reflow frame type to eReflowType_Inline
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*                aFrame,
                                     const nsHTMLReflowState& aParentReflowState,
                                     const nsSize&            aMaxSize,
                                     nsLineLayout*            aLineLayout)
  : nsReflowState(aFrame, aParentReflowState, aMaxSize)
{
  frameType = eReflowType_Inline;
  spaceManager = aParentReflowState.spaceManager;
  lineLayout = aLineLayout;
}

// Constructs a reflow state that overrides the reflow reason of the parent
// reflow state. Uses the space manager from the parent's reflow state and
// sets the reflow command to NULL. Sets lineLayout to NULL, and defaults to
// a reflow frame type of eReflowType_Block
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*                aFrame,
                                     const nsHTMLReflowState& aParentReflowState,
                                     const nsSize&            aMaxSize,
                                     nsReflowReason           aReflowReason,
                                     nsReflowFrameType        aFrameType)
  : nsReflowState(aFrame, aParentReflowState, aMaxSize, aReflowReason)
{
  frameType = aFrameType;
  spaceManager = aParentReflowState.spaceManager;
  lineLayout = nsnull;
}

#endif /* nsIHTMLReflow_h___ */


