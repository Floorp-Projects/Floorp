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
class nsISpaceManager;

// IID for the nsIHTMLFrame interface 
// a6cf9069-15b3-11d2-932e-00805f8add32
#define NS_IHTMLREFLOW_IID \
 { 0xa6cf9069, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * HTML/CSS specific reflow metrics
 */
struct nsHTMLReflowMetrics : nsReflowMetrics {
  // XXX Explain this better somehow!

  // The caller of nsIFrame::Reflow will set these to the top margin
  // value carried into the child frame. This allows the the child
  // container to collapse the top margin with its first childs
  // margin.
  nscoord mCarriedInTopMargin;          // in

  // These values are set by the child frame indicating its final
  // inner bottom margin value (the value of the childs last child
  // bottom margin)
  nscoord mCarriedOutBottomMargin;      // out

  nsHTMLReflowMetrics(nsSize* aMaxElementSize)
    : nsReflowMetrics(aMaxElementSize)
  {
    mCarriedInTopMargin = 0;
    mCarriedOutBottomMargin = 0;
  }
};

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

struct nsHTMLReflowState : nsReflowState {
  nsISpaceManager*    spaceManager;

  // XXX None of this is currently being used...
#if 0
  nsReflowConstraint   widthConstraint;   // constraint that applies to width dimension
  nsReflowConstraint   heightConstraint;  // constraint that applies to height dimension
  nsSize               minSize;           // the min available space in which to reflow.
                                          // Only used for eReflowSize_Constrained
#endif

  // Constructs an initial reflow state (no parent reflow state) for a
  // non-incremental reflow command
  nsHTMLReflowState(nsIFrame*            aFrame,
                    nsReflowReason       aReason, 
                    const nsSize&        aMaxSize,
                    nsIRenderingContext* aContext,
                    nsISpaceManager*     aSpaceManager = nsnull);

  // Constructs an initial reflow state (no parent reflow state) for an
  // incremental reflow command
  nsHTMLReflowState(nsIFrame*            aFrame,
                    nsIReflowCommand&    aReflowCommand,
                    const nsSize&        aMaxSize,
                    nsIRenderingContext* aContext,
                    nsISpaceManager*     aSpaceManager = nsnull);

  // Construct a reflow state for the given frame, parent reflow state, and
  // max size. Uses the reflow reason and reflow command from the parent's
  // reflow state
  nsHTMLReflowState(nsIFrame*                aFrame,
                    const nsHTMLReflowState& aParentReflowState,
                    const nsSize&            aMaxSize);

  // Constructs a reflow state that overrides the reflow reason of the parent
  // reflow state. Sets the reflow command to NULL
  nsHTMLReflowState(nsIFrame*                aFrame,
                    const nsHTMLReflowState& aParentReflowState,
                    const nsSize&            aMaxSize,
                    nsReflowReason           aReflowReason);
};

/**
 * Generate a reflow interface specific to HTML/CSS frame objects
 */
class nsIHTMLReflow : public nsIFrameReflow<nsHTMLReflowState, nsHTMLReflowMetrics>
{
};

// Constructs an initial reflow state (no parent reflow state) for a
// non-incremental reflow command
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*            aFrame,
                                     nsReflowReason       aReason, 
                                     const nsSize&        aMaxSize,
                                     nsIRenderingContext* aContext,
                                     nsISpaceManager*     aSpaceManager)
  : nsReflowState(aFrame, aReason, aMaxSize, aContext)
{
  spaceManager = aSpaceManager;
}

// Constructs an initial reflow state (no parent reflow state) for an
// incremental reflow command
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*            aFrame,
                                     nsIReflowCommand&    aReflowCommand,
                                     const nsSize&        aMaxSize,
                                     nsIRenderingContext* aContext,
                                     nsISpaceManager*     aSpaceManager)
  : nsReflowState(aFrame, aReflowCommand, aMaxSize, aContext)
{
  spaceManager = aSpaceManager;
}

// Construct a reflow state for the given frame, parent reflow state, and
// max size. Uses the reflow reason and reflow command from the parent's
// reflow state
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*                aFrame,
                                     const nsHTMLReflowState& aParentReflowState,
                                     const nsSize&            aMaxSize)
  : nsReflowState(aFrame, aParentReflowState, aMaxSize)
{
  spaceManager = aParentReflowState.spaceManager;
}

// Constructs a reflow state that overrides the reflow reason of the parent
// reflow state. Sets the reflow command to NULL
inline
nsHTMLReflowState::nsHTMLReflowState(nsIFrame*                aFrame,
                                     const nsHTMLReflowState& aParentReflowState,
                                     const nsSize&            aMaxSize,
                                     nsReflowReason           aReflowReason)
  : nsReflowState(aFrame, aParentReflowState, aMaxSize, aReflowReason)
{
  spaceManager = aParentReflowState.spaceManager;
}

#endif /* nsIHTMLReflow_h___ */


