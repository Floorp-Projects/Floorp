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
#ifndef nsIFrameReflow_h___
#define nsIFrameReflow_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsSize.h"

class nsIFrame;
class nsIPresContext;
class nsIReflowCommand;
class nsIRenderingContext;

//----------------------------------------------------------------------

/**
 * Reflow metrics used to return the frame's desired size and alignment
 * information.
 *
 * @see #Reflow()
 * @see #GetReflowMetrics()
 */
struct nsReflowMetrics {
  nscoord width, height;        // [OUT] desired width and height
  nscoord ascent, descent;      // [OUT] ascent and descent information

  // Set this to null if you don't need to compute the max element size
  nsSize* maxElementSize;       // [IN OUT]

  nsReflowMetrics(nsSize* aMaxElementSize) {
    maxElementSize = aMaxElementSize;

    // XXX These are OUT parameters and so they shouldn't have to be
    // initialized, but there are some bad frame classes that aren't
    // properly setting them when returning from Reflow()...
    width = height = 0;
    ascent = descent = 0;
  }
};

//----------------------------------------------------------------------

/**
 * Constant used to indicate an unconstrained size.
 *
 * @see #Reflow()
 */
#define NS_UNCONSTRAINEDSIZE NS_MAXSIZE

//----------------------------------------------------------------------

/**
 * The reason the frame is being reflowed.
 *
 * XXX Should probably be a #define so it can be extended for specialized
 * reflow interfaces...
 *
 * @see nsReflowState
 */
enum nsReflowReason {
  eReflowReason_Initial = 0,     // initial reflow of a newly created frame
  eReflowReason_Incremental = 1, // an incremental change has occured. see the reflow command for details
  eReflowReason_Resize = 2,      // general request to determine a desired size
  eReflowReason_StyleChange = 3  // request to reflow because of a style change. Note: you must reflow
                                 // all your child frames
};

//----------------------------------------------------------------------

/**
 * Reflow state passed to a frame during reflow. The reflow states are linked
 * together. The availableWidth and availableHeight represent the available
 * space in which to reflow your frame, and are computed based on the parent
 * frame's computed width and computed height. The available space is the total
 * space including margins, border, padding, and content area. A value of
 * NS_UNCONSTRAINEDSIZE means you can choose whatever size you want
 *
 * @see #Reflow()
 */
struct nsReflowState {
  const nsReflowState* parentReflowState; // pointer to parent's reflow state
  nsIFrame*            frame;             // the frame being reflowed
  nsReflowReason       reason;            // the reason for the reflow
  nsIReflowCommand*    reflowCommand;     // the reflow command. only set for eReflowReason_Incremental
  nscoord              availableWidth,
                       availableHeight;   // the available space in which to reflow
  nsIRenderingContext* rendContext;       // rendering context to use for measurement
  PRPackedBool         isTopOfPage;       // is the current context at the top of a page?
  // the following data members are relevant if nsStyleText.mTextAlign is NS_STYLE_TEXT_ALIGN_CHAR
  PRPackedBool         useAlignCharOffset;// if true, the reflow honors alignCharOffset and does not
                                          // set it. if false, the reflow sets alignCharOffset
  nscoord              alignCharOffset;   // distance from reference edge (as specified in nsStyleDisplay.mDirection) 
                                          // to the align character (which will be specified in nsStyleTable)

  // Note: there is no copy constructor, so the compiler can generate an
  // optimal one.

  // Constructs an initial reflow state (no parent reflow state) for a
  // non-incremental reflow command
  nsReflowState(nsIFrame*            aFrame,
                nsReflowReason       aReason, 
                const nsSize&        aAvailableSpace,
                nsIRenderingContext* aContext);

  // Constructs an initial reflow state (no parent reflow state) for an
  // incremental reflow command
  nsReflowState(nsIFrame*            aFrame,
                nsIReflowCommand&    aReflowCommand,
                const nsSize&        aAvailableSpace,
                nsIRenderingContext* aContext);

  // Construct a reflow state for the given frame, parent reflow state, and
  // available space. Uses the reflow reason, reflow command, and isTopOfPage value
  // from the parent's reflow state
  nsReflowState(nsIFrame*            aFrame,
                const nsReflowState& aParentReflowState,
                const nsSize&        aAvailableSpace);

  // Constructs a reflow state that overrides the reflow reason of the parent
  // reflow state. Uses the isTopOfPage value from the parent's reflow state, and
  // sets the reflow command to NULL
  nsReflowState(nsIFrame*            aFrame,
                const nsReflowState& aParentReflowState,
                const nsSize&        aAvailableSpace,
                nsReflowReason       aReflowReason);
};

//----------------------------------------------------------------------

/**
 * Reflow status returned by the reflow methods.
 *
 * NS_FRAME_NOT_COMPLETE bit flag means the frame does not map all its
 * content, and that the parent frame should create a continuing frame.
 * If this bit isn't set it means the frame does map all its content.
 *
 * NS_FRAME_REFLOW_NEXTINFLOW bit flag means that the next-in-flow is
 * dirty, and also needs to be reflowed. This status only makes sense
 * for a frame that is not complete, i.e. you wouldn't set both
 * NS_FRAME_COMPLETE and NS_FRAME_REFLOW_NEXTINFLOW
 *
 * The low 8 bits of the nsReflowStatus are reserved for future extensions;
 * the remaining 24 bits are zero (and available for extensions; however
 * API's that accept/return nsReflowStatus must not receive/return any
 * extension bits).
 *
 * @see #Reflow()
 */
typedef PRUint32 nsReflowStatus;

#define NS_FRAME_COMPLETE          0            // Note: not a bit!
#define NS_FRAME_NOT_COMPLETE      0x1
#define NS_FRAME_REFLOW_NEXTINFLOW 0x2

#define NS_FRAME_IS_COMPLETE(status) \
  (0 == ((status) & NS_FRAME_NOT_COMPLETE))

#define NS_FRAME_IS_NOT_COMPLETE(status) \
  (0 != ((status) & NS_FRAME_NOT_COMPLETE))

// This macro tests to see if an nsReflowStatus is an error value
// or just a regular return value
#define NS_IS_REFLOW_ERROR(_status) (PRInt32(_status) < 0)

//----------------------------------------------------------------------

/**
 * DidReflow status values.
 */
typedef PRBool nsDidReflowStatus;

#define NS_FRAME_REFLOW_NOT_FINISHED PR_FALSE
#define NS_FRAME_REFLOW_FINISHED     PR_TRUE

//----------------------------------------------------------------------

/**
 * Basic layout protocol.
 *
 * This template class defines the basic layout reflow protocol. You instantiate
 * a particular reflow interface by specifying the reflow state and reflow
 * metrics structures. These can be nsReflowState and nsReflowMetrics, or
 * they can be derived structures containing additional information.
 */
template<class ReflowState, class ReflowMetrics> class nsIFrameReflow : public nsISupports {
public:
  /**
   * Pre-reflow hook. Before a frame is reflowed this method will be called.
   * This call will always be invoked at least once before a subsequent Reflow
   * and DidReflow call. It may be called more than once, In general you will
   * receive on WillReflow notification before each Reflow request.
   *
   * XXX Is this really the semantics we want? Because we have the NS_FRAME_IN_REFLOW
   * bit we can ensure we don't call it more than once...
   */
  NS_IMETHOD  WillReflow(nsIPresContext& aPresContext) = 0;

  /**
   * The frame is given a maximum size and asked for its desired size.
   * This is the frame's opportunity to reflow its children.
   *
   * @param aDesiredSize <i>out</i> parameter where you should return the
   *          desired size and ascent/descent info. You should include any
   *          space you want for border/padding in the desired size you return.
   *
   *          It's okay to return a desired size that exceeds the max
   *          size if that's the smallest you can be, i.e. it's your
   *          minimum size.
   *
   *          maxElementSize is an optional parameter for returning your
   *          maximum element size. If may be null in which case you
   *          don't have to compute a maximum element size. The
   *          maximum element size must be less than or equal to your
   *          desired size.
   *
   * @param aReflowState information about your reflow including the reason
   *          for the reflow and the available space in which to lay out. Each
   *          dimension of the available space can either be constrained or
   *          unconstrained (a value of NS_UNCONSTRAINEDSIZE). If constrained
   *          you should choose a value that's less than or equal to the
   *          constrained size. If unconstrained you can choose as
   *          large a value as you like.
   *
   *          Note that the available space can be negative. In this case you
   *          still must return an accurate desired size. If you're a container
   *          you must <b>always</b> reflow at least one frame regardless of the
   *          available space
   *
   * @param aStatus a return value indicating whether the frame is complete
   *          and whether the next-in-flow is dirty and needs to be reflowed
   */
  NS_IMETHOD Reflow(nsIPresContext&    aPresContext,
                    ReflowMetrics&     aDesiredSize,
                    const ReflowState& aReflowState,
                    nsReflowStatus&    aStatus) = 0;

  /**
   * Post-reflow hook. After a frame is reflowed this method will be called
   * informing the frame that this reflow process is complete, and telling the
   * frame the status returned by the Reflow member function.
   *
   * This call may be invoked many times, while NS_FRAME_IN_REFLOW is set, before
   * it is finally called once with a NS_FRAME_REFLOW_COMPLETE value. When called
   * with a NS_FRAME_REFLOW_COMPLETE value the NS_FRAME_IN_REFLOW bit in the
   * frame state will be cleared.
   *
   * XXX This doesn't make sense. If the frame is reflowed but not complete, then
   * the status should be NS_FRAME_NOT_COMPLETE and not NS_FRAME_COMPLETE
   * XXX Don't we want the semantics to dictate that we only call this once for
   * a given reflow?
   */
  NS_IMETHOD  DidReflow(nsIPresContext&   aPresContext,
                        nsDidReflowStatus aStatus) = 0;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

//----------------------------------------------------------------------

// Constructs an initial reflow state (no parent reflow state) for a
// non-incremental reflow command
inline nsReflowState::nsReflowState(nsIFrame*            aFrame,
                                    nsReflowReason       aReason, 
                                    const nsSize&        aAvailableSpace,
                                    nsIRenderingContext* aContext)
{
  NS_PRECONDITION(aReason != eReflowReason_Incremental, "unexpected reflow reason");
  NS_PRECONDITION(!(aContext == nsnull), "no rendering context");
  reason = aReason;
  reflowCommand = nsnull;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  parentReflowState = nsnull;
  frame = aFrame;
  rendContext = aContext;
  isTopOfPage = PR_FALSE;
}

// Constructs an initial reflow state (no parent reflow state) for an
// incremental reflow command
inline nsReflowState::nsReflowState(nsIFrame*            aFrame,
                                    nsIReflowCommand&    aReflowCommand,
                                    const nsSize&        aAvailableSpace,
                                    nsIRenderingContext* aContext)
{
  NS_PRECONDITION(!(aContext == nsnull), "no rendering context");
  reason = eReflowReason_Incremental;
  reflowCommand = &aReflowCommand;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  parentReflowState = nsnull;
  frame = aFrame;
  rendContext = aContext;
  isTopOfPage = PR_FALSE;
}

// Construct a reflow state for the given frame, parent reflow state, and
// max size. Uses the reflow reason, reflow command, and isTopOfPage value
// from the parent's reflow state
inline nsReflowState::nsReflowState(nsIFrame*            aFrame,
                                    const nsReflowState& aParentReflowState,
                                    const nsSize&        aAvailableSpace)
{
  reason = aParentReflowState.reason;
  reflowCommand = aParentReflowState.reflowCommand;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  parentReflowState = &aParentReflowState;
  frame = aFrame;
  rendContext = aParentReflowState.rendContext;
  isTopOfPage = aParentReflowState.isTopOfPage;
}

// Constructs a reflow state that overrides the reflow reason of the parent
// reflow state. Uses the isTopOfPage value from the parent's reflow state, and
// sets the reflow command to NULL
inline nsReflowState::nsReflowState(nsIFrame*            aFrame,
                                    const nsReflowState& aParentReflowState,
                                    const nsSize&        aAvailableSpace,
                                    nsReflowReason       aReflowReason)
{
  reason = aReflowReason;
  reflowCommand = nsnull;
  availableWidth = aAvailableSpace.width;
  availableHeight = aAvailableSpace.height;
  parentReflowState = &aParentReflowState;
  frame = aFrame;
  rendContext = aParentReflowState.rendContext;
  isTopOfPage = aParentReflowState.isTopOfPage;
}

#endif /* nsIFrameReflow_h___ */


