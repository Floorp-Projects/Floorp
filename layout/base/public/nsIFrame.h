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
#ifndef nsIFrame_h___
#define nsIFrame_h___

#include <stdio.h>
#include "nslayout.h"
#include "nsISupports.h"
#include "nsSize.h"
#include "nsGUIEvent.h"
#include "nsStyleStruct.h"

class nsIContent;
class nsIFrame;
class nsIPresContext;
class nsIPresShell;
class nsIRenderingContext;
class nsISizeOfHandler;
class nsISpaceManager;
class nsIStyleContext;
class nsIView;
class nsIWidget;
class nsIReflowCommand;

struct nsPoint;
struct nsRect;
struct nsReflowMetrics;
struct nsStyleStruct;

struct PRLogModuleInfo;

// IID for the nsIFrame interface {12B193D0-9F70-11d1-8500-00A02468FAB6}
#define NS_IFRAME_IID         \
{ 0x12b193d0, 0x9f70, 0x11d1, \
  {0x85, 0x0, 0x0, 0xa0, 0x24, 0x68, 0xfa, 0xb6}}

/**
 * Reflow metrics used to return the frame's desired size and alignment
 * information.
 *
 * @see #Reflow()
 * @see #GetReflowMetrics()
 */
struct nsReflowMetrics {
  nscoord width, height;   // desired width and height
  nscoord ascent, descent;
  nsSize* maxElementSize;  // null if you don't need to compute the max element size

  nsReflowMetrics(nsSize* aMaxElementSize) {maxElementSize = aMaxElementSize;}
};

/**
 * Constant used to indicate an unconstrained size.
 *
 * @see #Reflow()
 */
#define NS_UNCONSTRAINEDSIZE NS_MAXSIZE

enum nsReflowReason {
  eReflowReason_Initial = 0,     // initial reflow of a newly created frame
  eReflowReason_Incremental = 1, // an incremental change has occured. see the reflow command for details
  eReflowReason_Resize = 2       // general request to determine a desired size
};

/**
 * Reflow state passed to a frame during reflow. The reflow states are linked
 * together. The max size represents the available space on which to reflow
 * your frame, and is computed as the parent frame's available content area
 * minus any room for margins that your frame requests.
 *
 * @see #Reflow()
 */
struct nsReflowState {
  nsReflowReason       reason;            // the reason for the reflow
  nsIReflowCommand*    reflowCommand;     // only used for incremental changes
  nsSize               maxSize;           // the available space in which to reflow
  const nsReflowState* parentReflowState; // pointer to parent's reflow state
  nsIFrame*            frame;             // the frame being reflowed

  // Constructs an initial reflow state (no parent reflow state) for a
  // non-incremental reflow command
  nsReflowState(nsIFrame*      aFrame,
                nsReflowReason aReason, 
                const nsSize&  aMaxSize);

  // Constructs an initial reflow state (no parent reflow state) for an
  // incremental reflow command
  nsReflowState(nsIFrame*         aFrame,
                nsIReflowCommand& aReflowCommand,
                const nsSize&     aMaxSize);

  // Construct a reflow state for the given frame, parent reflow state, and
  // max size. Uses the reflow reason and reflow command from the parent's
  // reflow state
  nsReflowState(nsIFrame*            aFrame,
                const nsReflowState& aParentReflowState,
                const nsSize&        aMaxSize);

  // Constructs a reflow state that overrides the reflow reason of the parent
  // reflow state. Sets the reflow command to NULL
  nsReflowState(nsIFrame*            aFrame,
                const nsReflowState& aParentReflowState,
                const nsSize&        aMaxSize,
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
 * @see #Reflow()
 * @see #CreateContinuingFrame()
 */
typedef PRUint32 nsReflowStatus;

#define NS_FRAME_COMPLETE          0            // Note: not a bit!
#define NS_FRAME_NOT_COMPLETE      0x1
#define NS_FRAME_REFLOW_NEXTINFLOW 0x2

#define NS_FRAME_IS_COMPLETE(status)\
  (0 == ((status) & NS_FRAME_NOT_COMPLETE))

#define NS_FRAME_IS_NOT_COMPLETE(status)\
  (0 != ((status) & NS_FRAME_NOT_COMPLETE))

//----------------------------------------------------------------------

/**
 * Indication of how the frame can be split. This is used when doing runaround
 * of floaters, and when pulling up child frames from a next-in-flow.
 *
 * The choices are splittable, not splittable at all, and splittable in
 * a non-rectangular fashion. This last type only applies to block-level
 * elements, and indicates whether splitting can be used when doing runaround.
 * If you can split across page boundaries, but you expect each continuing
 * frame to be the same width then return frSplittable and not
 * frSplittableNonRectangular.
 *
 * @see #IsSplittable()
 */
typedef PRUint32 nsSplittableType;

#define NS_FRAME_NOT_SPLITTABLE             0   // Note: not a bit!
#define NS_FRAME_SPLITTABLE                 0x1
#define NS_FRAME_SPLITTABLE_NON_RECTANGULAR 0x3

#define NS_FRAME_IS_SPLITTABLE(type)\
  (0 != ((type) & NS_FRAME_SPLITTABLE))

#define NS_FRAME_IS_NOT_SPLITTABLE(type)\
  (0 == ((type) & NS_FRAME_SPLITTABLE))

//----------------------------------------------------------------------

/**
 * Frame state bits. Any bits not listed here are reserved for future
 * extensions, but must be stored by the frames.
 */
typedef PRUint32 nsFrameState;

#define NS_FRAME_IN_REFLOW    0x00000001

// This bit is set when a frame is created. After it has been reflowed
// once (during the DidReflow with a finished state) the bit is
// cleared.
#define NS_FRAME_FIRST_REFLOW 0x00000002

//----------------------------------------------------------------------

/**
 * DidReflow status values.
 */
typedef PRBool nsDidReflowStatus;

#define NS_FRAME_REFLOW_NOT_FINISHED PR_FALSE
#define NS_FRAME_REFLOW_FINISHED     PR_TRUE

//----------------------------------------------------------------------

/**
 * A frame in the layout model. This interface is supported by all frame
 * objects.
 *
 * Frames are NOT reference counted. Use the DeleteFrame() member function
 * to delete a frame
 */
class nsIFrame : private nsISupports
{
public:
  /**
   * QueryInterface() defined in nsISupports. This is the only member
   * function of nsISupports that is public.
   */
  NS_IMETHOD  QueryInterface(const nsIID& aIID, void** aInstancePtr) = 0;

  /**
   * Add this object's size information to the sizeof handler. Note that
   * this does <b>not</b> add in the size of content, style, or view's
   * (those are sized seperately).
   */
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const = 0;

  /**
   * Deletes this frame and each of its child frames (recursively calls
   * DeleteFrame() for each child)
   */
  NS_IMETHOD  DeleteFrame() = 0;

  /**
   * Get the content object associated with this frame. Adds a reference to
   * the content object so the caller must do a release.
   *
   * @see nsISupports#Release()
   */
  NS_IMETHOD  GetContent(nsIContent*& aContent) const = 0;

  /**
   * Get the index in parent of the frame's content object
   */
  NS_IMETHOD  GetContentIndex(PRInt32& aIndexInParent) const = 0;

  /**
   * Get the style context associated with this frame. Note that GetStyleContext()
   * adds a reference to the style context so the caller must do a release.
   *
   * @see nsISupports#Release()
   */
  NS_IMETHOD  GetStyleContext(nsIPresContext*   aContext,
                              nsIStyleContext*& aStyleContext) = 0;
  NS_IMETHOD  SetStyleContext(nsIPresContext* aPresContext,
                              nsIStyleContext* aContext) = 0;

  /**
   * Get the style data associated with this frame
   */
  NS_IMETHOD  GetStyleData(nsStyleStructID aSID, const nsStyleStruct*& aStyleStruct) const = 0;

  /**
   * Accessor functions for geometric and content parent.
   */
  NS_IMETHOD  GetContentParent(nsIFrame*& aParent) const = 0;
  NS_IMETHOD  SetContentParent(const nsIFrame* aParent) = 0;
  NS_IMETHOD  GetGeometricParent(nsIFrame*& aParent) const = 0;
  NS_IMETHOD  SetGeometricParent(const nsIFrame* aParent) = 0;

  /**
   * Bounding rect of the frame. The values are in twips, and the origin is
   * relative to the upper-left of the geometric parent. The size includes the
   * content area, borders, and padding.
   */
  NS_IMETHOD  GetRect(nsRect& aRect) const = 0;
  NS_IMETHOD  GetOrigin(nsPoint& aPoint) const = 0;
  NS_IMETHOD  GetSize(nsSize& aSize) const = 0;
  NS_IMETHOD  SetRect(const nsRect& aRect) = 0;
  NS_IMETHOD  MoveTo(nscoord aX, nscoord aY) = 0;
  NS_IMETHOD  SizeTo(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Child frame enumeration
   */
  NS_IMETHOD  ChildCount(PRInt32& aChildCount) const = 0;
  NS_IMETHOD  ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const = 0;
  NS_IMETHOD  IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const = 0;
  NS_IMETHOD  FirstChild(nsIFrame*& aFirstChild) const = 0;
  NS_IMETHOD  NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const = 0;
  NS_IMETHOD  PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const = 0;
  NS_IMETHOD  LastChild(nsIFrame*& aLastChild) const = 0;

  /**
   * Painting
   */
  NS_IMETHOD  Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect) = 0;

  /**
   * Handle an event. 
   */
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus) = 0;

  /**
   * Get the cursor for a given point in the frame tree. The
   * call returns the desired cursor (or NS_STYLE_CURSOR_INHERIT if
   * no cursor is wanted). In addition, if a cursor is desired
   * then *aFrame is set to the frame that wants the cursor.
   */
  NS_IMETHOD  GetCursorAt(nsIPresContext& aPresContext,
                          const nsPoint&  aPoint,
                          nsIFrame**      aFrame,
                          PRInt32&        aCursor) = 0;

  /**
   * Get the current frame-state value for this frame. aResult is
   * filled in with the state bits. The return value has no
   * meaning.
   */
  NS_IMETHOD  GetFrameState(nsFrameState& aResult) = 0;

  /**
   * Set the current frame-state value for this frame. The return
   * value has no meaning.
   */
  NS_IMETHOD  SetFrameState(nsFrameState aNewState) = 0;

  /**
   * Pre-reflow hook. Before a frame is incrementally reflowed or
   * resize-reflowed this method will be called warning the frame of
   * the impending reflow. This call may be invoked zero or more times
   * before a subsequent DidReflow call. This method when called the
   * first time will set the NS_FRAME_IN_REFLOW bit in the frame
   * state bits.
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
   */
  NS_IMETHOD Reflow(nsIPresContext*      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus) = 0;

  /**
   * Post-reflow hook. After a frame is incrementally reflowed or
   * resize-reflowed this method will be called telling the frame of
   * the outcome. This call may be invoked many times, while
   * NS_FRAME_IN_REFLOW is set, before it is finally called once with
   * a NS_FRAME_REFLOW_COMPLETE value. When called with a
   * NS_FRAME_REFLOW_COMPLETE value the NS_FRAME_IN_REFLOW bit in the
   * frame state will be cleared.
   */
  NS_IMETHOD  DidReflow(nsIPresContext& aPresContext,
                        nsDidReflowStatus aStatus) = 0;

  /**
   * Post-processing reflow method invoked when justification is enabled.
   * This is always called after Reflow()
   *
   * @param aAvailableSpace The amount of available space that the frame
   *         should distribute internally.
   */
  NS_IMETHOD  JustifyReflow(nsIPresContext* aPresContext,
                            nscoord         aAvailableSpace) = 0;

  /**
   * This call is invoked when content is appended to the content tree.
   *
   * This frame is the frame that maps the content object that has appended
   * content. A typical response to this notification is to generate a
   * FrameAppended incremental reflow command. You then handle the incremental
   * reflow command by creating frames for the appended content.
   */
  NS_IMETHOD  ContentAppended(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer) = 0;

  /**
   * This call is invoked when content is inserted in the content
   * tree.
   *
   * This frame is the frame that maps the content object that has inserted
   * content. A typical response to this notification is to update the
   * index-in-parent values for the affected child frames, create and insert
   * new frame(s), and generate a FrameInserted incremental reflow command.
   *
   * You respond to the incremental reflow command by reflowing the newly
   * inserted frame and any impacted frames.
   *
   * @param aIndexInParent the index in the content container where
   *          the new content was inserted.
   */
  NS_IMETHOD  ContentInserted(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aChild,
                              PRInt32         aIndexInParent) = 0;

  /**
   * This call is invoked when content is replaced in the content
   * tree. The container frame that maps that content is asked to deal
   * with the replaced content by deleting old frames and then
   * creating new frames and updating the index-in-parent values for
   * it's affected children. In addition, the call must generate
   * reflow commands that will incrementally reflow and repair the
   * damaged portion of the frame tree.
   *
   * @param aIndexInParent the index in the content container where
   *          the new content was inserted.  */
  NS_IMETHOD  ContentReplaced(nsIPresShell*   aShell,
                              nsIPresContext* aPresContext,
                              nsIContent*     aContainer,
                              nsIContent*     aOldChild,
                              nsIContent*     aNewChild,
                              PRInt32         aIndexInParent) = 0;

  /**
   * This call is invoked when content is deleted from the content
   * tree. The container frame that maps that content is asked to deal
   * with the deleted content by deleting frames and updating the
   * index-in-parent values for it's affected children. In addition,
   * the call must generate reflow commands that will incrementally
   * reflow and repair the damaged portion of the frame tree.
   *
   * @param aIndexInParent the index in the content container where
   *          the new content was deleted.
   */
  NS_IMETHOD  ContentDeleted(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aContainer,
                             nsIContent*     aChild,
                             PRInt32         aIndexInParent) = 0;

  /**
   * This call is invoked when content is changed in the content tree.
   * The first frame that maps that content is asked to deal with the
   * change by generating an incremental reflow command.
   *
   * @param aIndexInParent the index in the content container where
   *          the new content was deleted.
   */
  NS_IMETHOD  ContentChanged(nsIPresShell*   aShell,
                             nsIPresContext* aPresContext,
                             nsIContent*     aChild,
                             nsISupports*    aSubContent) = 0;

  /**
   * Return the reflow metrics for this frame. If the frame is a
   * container then the values for ascent and descent are computed
   * across the the various children in the appropriate manner
   * (e.g. for a line frame the ascent value would be the maximum
   * ascent of the line's children). Note that the metrics returned
   * apply to the frame as it exists at the time of the call.
   */
  NS_IMETHOD  GetReflowMetrics(nsIPresContext*  aPresContext,
                               nsReflowMetrics& aMetrics) = 0;

  /**
   * Return how your frame can be split.
   */
  NS_IMETHOD  IsSplittable(nsSplittableType& aIsSplittable) const = 0;

  /**
   * Flow member functions. CreateContinuingFrame() is responsible for
   * appending the continuing frame to the flow.
   */
  NS_IMETHOD  CreateContinuingFrame(nsIPresContext*  aPresContext,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aStyleContext,
                                    nsIFrame*&       aContinuingFrame) = 0;

  NS_IMETHOD  GetPrevInFlow(nsIFrame*& aPrevInFlow) const = 0;
  NS_IMETHOD  SetPrevInFlow(nsIFrame*) = 0;
  NS_IMETHOD  GetNextInFlow(nsIFrame*& aNextInFlow) const = 0;
  NS_IMETHOD  SetNextInFlow(nsIFrame*) = 0;

  NS_IMETHOD  AppendToFlow(nsIFrame* aAfterFrame) = 0;
  NS_IMETHOD  PrependToFlow(nsIFrame* aBeforeFrame) = 0;
  NS_IMETHOD  RemoveFromFlow() = 0;
  NS_IMETHOD  BreakFromPrevFlow() = 0;
  NS_IMETHOD  BreakFromNextFlow() = 0;

  /**
   * Accessor functions to get/set the associated view object
   */
  NS_IMETHOD  GetView(nsIView*& aView) const = 0;  // may be null
  NS_IMETHOD  SetView(nsIView* aView) = 0;

  /**
   * Find the first geometric parent that has a view
   */
  NS_IMETHOD  GetParentWithView(nsIFrame*& aParent) const = 0;

  /**
   * Returns the offset from this frame to the closest geometric parent that
   * has a view. Also returns the containing view or null in case of error
   */
  NS_IMETHOD  GetOffsetFromView(nsPoint& aOffset, nsIView*& aView) const = 0;

  /**
   * Returns the window that contains this frame. If this frame has a
   * view and the view has a window, then this frames window is
   * returned, otherwise this frame's geometric parent is checked
   * recursively upwards.
   */
  NS_IMETHOD  GetWindow(nsIWidget*&) const = 0;

  /**
   * Is this frame a "containing block"?
   */
  NS_IMETHOD  IsPercentageBase(PRBool& aBase) const = 0;

  /**
   * Gets the size of an "auto" margin.
   */
  NS_IMETHOD  GetAutoMarginSize(PRUint8 aSide, nscoord& aSize) const = 0;

  /**
   * Sibling pointer used to link together frames
   */
  NS_IMETHOD  GetNextSibling(nsIFrame*& aNextSibling) const = 0;
  NS_IMETHOD  SetNextSibling(nsIFrame* aNextSibling) = 0;

  /**
   * Does this frame have content that is considered "transparent"?
   * This is binary transparency as opposed to translucency. MMP
   */
  NS_IMETHOD IsTransparent(PRBool& aTransparent) const = 0;

  // Debugging
  NS_IMETHOD  List(FILE* out = stdout, PRInt32 aIndent = 0) const= 0;
  NS_IMETHOD  ListTag(FILE* out = stdout) const = 0;
  NS_IMETHOD  VerifyTree() const = 0;

  /**
   * See if tree verification is enabled. To enable tree verification add
   * "frameverifytree:1" to your NSPR_LOG_MODULES environment variable
   * (any non-zero debug level will work). Or, call SetVerifyTreeEnable
   * with PR_TRUE.
   */
  static NS_LAYOUT PRBool GetVerifyTreeEnable();

  /**
   * Set the verify-tree enable flag.
   */
  static NS_LAYOUT void SetVerifyTreeEnable(PRBool aEnabled);

  /**
   * The frame class and related classes share an nspr log module
   * for logging frame activity.
   *
   * Note: the log module is created during library initialization which
   * means that you cannot perform logging before then.
   */
  static NS_LAYOUT PRLogModuleInfo* GetLogModuleInfo();

  // Show frame borders when rendering
  static NS_LAYOUT void ShowFrameBorders(PRBool aEnable);
  static NS_LAYOUT PRBool GetShowFrameBorders();

protected:
  static NS_LAYOUT PRLogModuleInfo* gLogModule;
};

// Constructs an initial reflow state (no parent reflow state) for a
// non-incremental reflow command
inline nsReflowState::nsReflowState(nsIFrame*      aFrame,
                                    nsReflowReason aReason, 
                                    const nsSize&  aMaxSize)
{
  NS_PRECONDITION(aReason != eReflowReason_Incremental, "unexpected reflow reason");
#ifdef NS_DEBUG
  nsIFrame* parent;
  aFrame->GetGeometricParent(parent);
  NS_PRECONDITION(nsnull == parent, "not root frame");
#endif
  reason = aReason;
  reflowCommand = nsnull;
  maxSize = aMaxSize;
  parentReflowState = nsnull;
  frame = aFrame;
}

// Constructs an initial reflow state (no parent reflow state) for an
// incremental reflow command
inline nsReflowState::nsReflowState(nsIFrame*         aFrame,
                                    nsIReflowCommand& aReflowCommand,
                                    const nsSize&     aMaxSize)
{
#ifdef NS_DEBUG
  nsIFrame* parent;
  aFrame->GetGeometricParent(parent);
  NS_PRECONDITION(nsnull == parent, "not root frame");
#endif
  reason = eReflowReason_Incremental;
  reflowCommand = &aReflowCommand;
  maxSize = aMaxSize;
  parentReflowState = nsnull;
  frame = aFrame;
}

// Construct a reflow state for the given frame, parent reflow state, and
// max size. Uses the reflow reason and reflow command from the parent's
// reflow state
inline nsReflowState::nsReflowState(nsIFrame*            aFrame,
                                    const nsReflowState& aParentReflowState,
                                    const nsSize&        aMaxSize)
{
  reason = aParentReflowState.reason;
  reflowCommand = aParentReflowState.reflowCommand;
  maxSize = aMaxSize;
  parentReflowState = &aParentReflowState;
  frame = aFrame;
}

// Constructs a reflow state that overrides the reflow reason of the parent
// reflow state. Sets the reflow command to NULL
inline nsReflowState::nsReflowState(nsIFrame*            aFrame,
                                    const nsReflowState& aParentReflowState,
                                    const nsSize&        aMaxSize,
                                    nsReflowReason       aReflowReason)
{
  reason = aReflowReason;
  reflowCommand = nsnull;
  maxSize = aMaxSize;
  parentReflowState = &aParentReflowState;
  frame = aFrame;
}

#endif /* nsIFrame_h___ */
