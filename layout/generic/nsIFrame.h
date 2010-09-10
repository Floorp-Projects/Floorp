/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/I
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

/* interface for all rendering objects */

#ifndef nsIFrame_h___
#define nsIFrame_h___

/* nsIFrame is in the process of being deCOMtaminated, i.e., this file is eventually
   going to be eliminated, and all callers will use nsFrame instead.  At the moment
   we're midway through this process, so you will see inlined functions and member
   variables in this file.  -dwh */

#include <stdio.h>
#include "nsQueryFrame.h"
#include "nsEvent.h"
#include "nsStyleStruct.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsHTMLReflowMetrics.h"
#include "gfxMatrix.h"
#include "nsFrameList.h"
#include "nsAlgorithm.h"
#include "FramePropertyTable.h"

/**
 * New rules of reflow:
 * 1. you get a WillReflow() followed by a Reflow() followed by a DidReflow() in order
 *    (no separate pass over the tree)
 * 2. it's the parent frame's responsibility to size/position the child's view (not
 *    the child frame's responsibility as it is today) during reflow (and before
 *    sending the DidReflow() notification)
 * 3. positioning of child frames (and their views) is done on the way down the tree,
 *    and sizing of child frames (and their views) on the way back up
 * 4. if you move a frame (outside of the reflow process, or after reflowing it),
 *    then you must make sure that its view (or its child frame's views) are re-positioned
 *    as well. It's reasonable to not position the view until after all reflowing the
 *    entire line, for example, but the frame should still be positioned and sized (and
 *    the view sized) during the reflow (i.e., before sending the DidReflow() notification)
 * 5. the view system handles moving of widgets, i.e., it's not our problem
 */

struct nsHTMLReflowState;
class nsHTMLReflowCommand;

class nsIAtom;
class nsPresContext;
class nsIPresShell;
class nsIRenderingContext;
class nsIView;
class nsIWidget;
class nsIDOMRange;
class nsISelectionController;
class nsBoxLayoutState;
class nsIBoxLayout;
class nsILineIterator;
#ifdef ACCESSIBILITY
class nsAccessible;
#endif
class nsDisplayListBuilder;
class nsDisplayListSet;
class nsDisplayList;
class gfxSkipChars;
class gfxSkipCharsIterator;
class gfxContext;
class nsLineList_iterator;

struct nsPeekOffsetStruct;
struct nsPoint;
struct nsRect;
struct nsSize;
struct nsMargin;
struct CharacterDataChangeInfo;

typedef class nsIFrame nsIBox;

/**
 * Indication of how the frame can be split. This is used when doing runaround
 * of floats, and when pulling up child frames from a next-in-flow.
 *
 * The choices are splittable, not splittable at all, and splittable in
 * a non-rectangular fashion. This last type only applies to block-level
 * elements, and indicates whether splitting can be used when doing runaround.
 * If you can split across page boundaries, but you expect each continuing
 * frame to be the same width then return frSplittable and not
 * frSplittableNonRectangular.
 *
 * @see #GetSplittableType()
 */
typedef PRUint32 nsSplittableType;

#define NS_FRAME_NOT_SPLITTABLE             0   // Note: not a bit!
#define NS_FRAME_SPLITTABLE                 0x1
#define NS_FRAME_SPLITTABLE_NON_RECTANGULAR 0x3

#define NS_FRAME_IS_SPLITTABLE(type)\
  (0 != ((type) & NS_FRAME_SPLITTABLE))

#define NS_FRAME_IS_NOT_SPLITTABLE(type)\
  (0 == ((type) & NS_FRAME_SPLITTABLE))

#define NS_INTRINSIC_WIDTH_UNKNOWN nscoord_MIN

//----------------------------------------------------------------------

/**
 * Frame state bits. Any bits not listed here are reserved for future
 * extensions, but must be stored by the frames.
 */
typedef PRUint64 nsFrameState;

#define NS_FRAME_STATE_BIT(n_) (nsFrameState(1) << (n_))

#define NS_FRAME_IN_REFLOW                          NS_FRAME_STATE_BIT(0)

// This is only set during painting
#define NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO    NS_FRAME_STATE_BIT(0)

// This bit is set when a frame is created. After it has been reflowed
// once (during the DidReflow with a finished state) the bit is
// cleared.
#define NS_FRAME_FIRST_REFLOW                       NS_FRAME_STATE_BIT(1)

// For a continuation frame, if this bit is set, then this a "fluid" 
// continuation, i.e., across a line boundary. Otherwise it's a "hard"
// continuation, e.g. a bidi continuation.
#define NS_FRAME_IS_FLUID_CONTINUATION              NS_FRAME_STATE_BIT(2)

// This bit is set whenever the frame has one or more associated
// container layers.
#define NS_FRAME_HAS_CONTAINER_LAYER                NS_FRAME_STATE_BIT(3)

// If this bit is set, then a reference to the frame is being held
// elsewhere.  The frame may want to send a notification when it is
// destroyed to allow these references to be cleared.
#define NS_FRAME_EXTERNAL_REFERENCE                 NS_FRAME_STATE_BIT(4)

// If this bit is set, this frame or one of its descendants has a
// percentage height that depends on an ancestor of this frame.
// (Or it did at one point in the past, since we don't necessarily clear
// the bit when it's no longer needed; it's an optimization.)
#define  NS_FRAME_CONTAINS_RELATIVE_HEIGHT          NS_FRAME_STATE_BIT(5)

// If this bit is set, then the frame corresponds to generated content
#define NS_FRAME_GENERATED_CONTENT                  NS_FRAME_STATE_BIT(6)

// If this bit is set the frame is a continuation that is holding overflow,
// i.e. it is a next-in-flow created to hold overflow after the box's
// height has ended. This means the frame should be a) at the top of the
// page and b) invisible: no borders, zero height, ignored in margin
// collapsing, etc. See nsContainerFrame.h
#define NS_FRAME_IS_OVERFLOW_CONTAINER              NS_FRAME_STATE_BIT(7)

// If this bit is set, then the frame has been moved out of the flow,
// e.g., it is absolutely positioned or floated
#define NS_FRAME_OUT_OF_FLOW                        NS_FRAME_STATE_BIT(8)

// If this bit is set, then the frame reflects content that may be selected
#define NS_FRAME_SELECTED_CONTENT                   NS_FRAME_STATE_BIT(9)

// If this bit is set, then the frame is dirty and needs to be reflowed.
// This bit is set when the frame is first created.
// This bit is cleared by DidReflow after the required call to Reflow has
// finished.
// Do not set this bit yourself if you plan to pass the frame to
// nsIPresShell::FrameNeedsReflow.  Pass the right arguments instead.
#define NS_FRAME_IS_DIRTY                           NS_FRAME_STATE_BIT(10)

// If this bit is set then the frame is too deep in the frame tree, and
// we'll stop updating it and its children, to prevent stack overflow
// and the like.
#define NS_FRAME_TOO_DEEP_IN_FRAME_TREE             NS_FRAME_STATE_BIT(11)

// If this bit is set, either:
//  1. the frame has children that have either NS_FRAME_IS_DIRTY or
//     NS_FRAME_HAS_DIRTY_CHILDREN, or
//  2. the frame has had descendants removed.
// It means that Reflow needs to be called, but that Reflow will not
// do as much work as it would if NS_FRAME_IS_DIRTY were set.
// This bit is cleared by DidReflow after the required call to Reflow has
// finished.
// Do not set this bit yourself if you plan to pass the frame to
// nsIPresShell::FrameNeedsReflow.  Pass the right arguments instead.
#define NS_FRAME_HAS_DIRTY_CHILDREN                 NS_FRAME_STATE_BIT(12)

// If this bit is set, the frame has an associated view
#define NS_FRAME_HAS_VIEW                           NS_FRAME_STATE_BIT(13)

// If this bit is set, the frame was created from anonymous content.
#define NS_FRAME_INDEPENDENT_SELECTION              NS_FRAME_STATE_BIT(14)

// If this bit is set, the frame is "special" (lame term, I know),
// which means that it is part of the mangled frame hierarchy that
// results when an inline has been split because of a nested block.
// See the comments in nsCSSFrameConstructor::ConstructInline for
// more details.
#define NS_FRAME_IS_SPECIAL                         NS_FRAME_STATE_BIT(15)

// If this bit is set, the frame may have a transform that it applies
// to its coordinate system (e.g. CSS transform, SVG foreignObject).
// This is used primarily in GetTransformMatrix to optimize for the
// common case.
#define  NS_FRAME_MAY_BE_TRANSFORMED                NS_FRAME_STATE_BIT(16)

#ifdef IBMBIDI
// If this bit is set, the frame itself is a bidi continuation,
// or is incomplete (its next sibling is a bidi continuation)
#define NS_FRAME_IS_BIDI                            NS_FRAME_STATE_BIT(17)
#endif

// If this bit is set the frame has descendant with a view
#define NS_FRAME_HAS_CHILD_WITH_VIEW                NS_FRAME_STATE_BIT(18)

// If this bit is set, then reflow may be dispatched from the current
// frame instead of the root frame.
#define NS_FRAME_REFLOW_ROOT                        NS_FRAME_STATE_BIT(19)

// Bits 20-31 and 60-63 of the frame state are reserved for implementations.
#define NS_FRAME_IMPL_RESERVED                      nsFrameState(0xF0000000FFF00000)

// This bit is set on floats whose parent does not contain their
// placeholder.  This can happen for two reasons:  (1) the float was
// split, and this piece is the continuation, or (2) the entire float
// didn't fit on the page.
#define NS_FRAME_IS_PUSHED_FLOAT                    NS_FRAME_STATE_BIT(32)

// This bit acts as a loop flag for recursive paint server drawing.
#define NS_FRAME_DRAWING_AS_PAINTSERVER             NS_FRAME_STATE_BIT(33)

// Frame or one of its (cross-doc) descendants may have the
// NS_FRAME_HAS_CONTAINER_LAYER bit.
#define NS_FRAME_HAS_CONTAINER_LAYER_DESCENDANT     NS_FRAME_STATE_BIT(34)

// Frame's overflow area was clipped by the 'clip' property.
#define NS_FRAME_HAS_CLIP                           NS_FRAME_STATE_BIT(35)

// The lower 20 bits and upper 32 bits of the frame state are reserved
// by this API.
#define NS_FRAME_RESERVED                           ~NS_FRAME_IMPL_RESERVED

// Box layout bits
#define NS_STATE_IS_HORIZONTAL                      NS_FRAME_STATE_BIT(22)
#define NS_STATE_IS_DIRECTION_NORMAL                NS_FRAME_STATE_BIT(31)

// Helper macros
#define NS_SUBTREE_DIRTY(_frame)  \
  (((_frame)->GetStateBits() &      \
    (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0)

//----------------------------------------------------------------------

enum nsSelectionAmount {
  eSelectCharacter = 0,
  eSelectWord      = 1,
  eSelectLine      = 2,  //previous drawn line in flow.
  eSelectBeginLine = 3,
  eSelectEndLine   = 4,
  eSelectNoAmount  = 5,   //just bounce back current offset.
  eSelectParagraph = 6    //select a "paragraph"
};

enum nsDirection {
  eDirNext    = 0,
  eDirPrevious= 1
};

enum nsSpread {
  eSpreadNone   = 0,
  eSpreadAcross = 1,
  eSpreadDown   = 2
};

// Carried out margin flags
#define NS_CARRIED_TOP_MARGIN_IS_AUTO    0x1
#define NS_CARRIED_BOTTOM_MARGIN_IS_AUTO 0x2

//----------------------------------------------------------------------

/**
 * Reflow status returned by the reflow methods. There are three
 * completion statuses, represented by two bit flags.
 *
 * NS_FRAME_COMPLETE means the frame is fully complete.
 *
 * NS_FRAME_NOT_COMPLETE bit flag means the frame does not map all its
 * content, and that the parent frame should create a continuing frame.
 * If this bit isn't set it means the frame does map all its content.
 * This bit is mutually exclusive with NS_FRAME_OVERFLOW_INCOMPLETE.
 *
 * NS_FRAME_OVERFLOW_INCOMPLETE bit flag means that the frame has
 * overflow that is not complete, but its own box is complete.
 * (This happens when content overflows a fixed-height box.)
 * The reflower should place and size the frame and continue its reflow,
 * but needs to create an overflow container as a continuation for this
 * frame. See nsContainerFrame.h for more information.
 * This bit is mutually exclusive with NS_FRAME_NOT_COMPLETE.
 * 
 * Please use the SET macro for handling
 * NS_FRAME_NOT_COMPLETE and NS_FRAME_OVERFLOW_INCOMPLETE.
 *
 * NS_FRAME_REFLOW_NEXTINFLOW bit flag means that the next-in-flow is
 * dirty, and also needs to be reflowed. This status only makes sense
 * for a frame that is not complete, i.e. you wouldn't set both
 * NS_FRAME_COMPLETE and NS_FRAME_REFLOW_NEXTINFLOW.
 *
 * The low 8 bits of the nsReflowStatus are reserved for future extensions;
 * the remaining 24 bits are zero (and available for extensions; however
 * API's that accept/return nsReflowStatus must not receive/return any
 * extension bits).
 *
 * @see #Reflow()
 */
typedef PRUint32 nsReflowStatus;

#define NS_FRAME_COMPLETE             0       // Note: not a bit!
#define NS_FRAME_NOT_COMPLETE         0x1
#define NS_FRAME_REFLOW_NEXTINFLOW    0x2
#define NS_FRAME_OVERFLOW_INCOMPLETE  0x4

#define NS_FRAME_IS_COMPLETE(status) \
  (0 == ((status) & NS_FRAME_NOT_COMPLETE))

#define NS_FRAME_IS_NOT_COMPLETE(status) \
  (0 != ((status) & NS_FRAME_NOT_COMPLETE))

#define NS_FRAME_OVERFLOW_IS_INCOMPLETE(status) \
  (0 != ((status) & NS_FRAME_OVERFLOW_INCOMPLETE))

#define NS_FRAME_IS_FULLY_COMPLETE(status) \
  (NS_FRAME_IS_COMPLETE(status) && !NS_FRAME_OVERFLOW_IS_INCOMPLETE(status))

// These macros set or switch incompete statuses without touching th
// NS_FRAME_REFLOW_NEXTINFLOW bit.
#define NS_FRAME_SET_INCOMPLETE(status) \
  status = (status & ~NS_FRAME_OVERFLOW_INCOMPLETE) | NS_FRAME_NOT_COMPLETE

#define NS_FRAME_SET_OVERFLOW_INCOMPLETE(status) \
  status = (status & ~NS_FRAME_NOT_COMPLETE) | NS_FRAME_OVERFLOW_INCOMPLETE

// This macro tests to see if an nsReflowStatus is an error value
// or just a regular return value
#define NS_IS_REFLOW_ERROR(_status) (PRInt32(_status) < 0)

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

// Set when a break was induced by completion of a first-letter
#define NS_INLINE_BREAK_FIRST_LETTER_COMPLETE 0x10000

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

// A frame is "truncated" if the part of the frame before the first
// possible break point was unable to fit in the available vertical
// space.  Therefore, the entire frame should be moved to the next page.
// A frame that begins at the top of the page must never be "truncated".
// Doing so would likely cause an infinite loop.
#define NS_FRAME_TRUNCATED  0x0010
#define NS_FRAME_IS_TRUNCATED(status) \
  (0 != ((status) & NS_FRAME_TRUNCATED))
#define NS_FRAME_SET_TRUNCATION(status, aReflowState, aMetrics) \
  aReflowState.SetTruncated(aMetrics, &status);

// Merge the incompleteness, truncation and NS_FRAME_REFLOW_NEXTINFLOW
// status from aSecondary into aPrimary.
void NS_MergeReflowStatusInto(nsReflowStatus* aPrimary,
                              nsReflowStatus aSecondary);

//----------------------------------------------------------------------

/**
 * DidReflow status values.
 */
typedef PRBool nsDidReflowStatus;

#define NS_FRAME_REFLOW_NOT_FINISHED PR_FALSE
#define NS_FRAME_REFLOW_FINISHED     PR_TRUE

/**
 * The overflow rect may be stored as four 1-byte deltas each strictly
 * LESS THAN 0xff, for the four edges of the rectangle, or the four bytes
 * may be read as a single 32-bit "overflow-rect type" value including
 * at least one 0xff byte as an indicator that the value does NOT
 * represent four deltas.
 * If all four deltas are zero, this means that no overflow rect has
 * actually been set (this is the initial state of newly-created frames).
 */
#define NS_FRAME_OVERFLOW_DELTA_MAX     0xfe // max delta we can store

#define NS_FRAME_OVERFLOW_NONE    0x00000000 // there is no overflow rect;
                                             // code relies on this being
                                             // the all-zero value

#define NS_FRAME_OVERFLOW_LARGE   0x000000ff // overflow is stored as a
                                             // separate rect property

//----------------------------------------------------------------------

/**
 * A frame in the layout model. This interface is supported by all frame
 * objects.
 *
 * Frames can have multiple child lists: the default unnamed child list
 * (referred to as the <i>principal</i> child list, and additional named
 * child lists. There is an ordering of frames within a child list, but
 * there is no order defined between frames in different child lists of
 * the same parent frame.
 *
 * Frames are NOT reference counted. Use the Destroy() member function
 * to destroy a frame. The lifetime of the frame hierarchy is bounded by the
 * lifetime of the presentation shell which owns the frames.
 *
 * nsIFrame is a private Gecko interface. If you are not Gecko then you
 * should not use it. If you're not in layout, then you won't be able to
 * link to many of the functions defined here. Too bad.
 *
 * If you're not in layout but you must call functions in here, at least
 * restrict yourself to calling virtual methods, which won't hurt you as badly.
 */
class nsIFrame : public nsQueryFrame
{
public:
  typedef mozilla::FramePropertyDescriptor FramePropertyDescriptor;
  typedef mozilla::FrameProperties FrameProperties;

  NS_DECL_QUERYFRAME_TARGET(nsIFrame)

  nsPresContext* PresContext() const {
    return GetStyleContext()->GetRuleNode()->GetPresContext();
  }

  /**
   * Called to initialize the frame. This is called immediately after creating
   * the frame.
   *
   * If the frame is a continuing frame, then aPrevInFlow indicates the previous
   * frame (the frame that was split).
   *
   * If you want a view associated with your frame, you should create the view
   * now.
   *
   * @param   aContent the content object associated with the frame
   * @param   aGeometricParent  the geometric parent frame
   * @param   aContentParent  the content parent frame
   * @param   aContext the style context associated with the frame
   * @param   aPrevInFlow the prev-in-flow frame
   */
  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        aPrevInFlow) = 0;

  /**
   * Destroys this frame and each of its child frames (recursively calls
   * Destroy() for each child). If this frame is a first-continuation, this
   * also removes the frame from the primary frame man and clears undisplayed
   * content for its content node.
   * If the frame is a placeholder, it also ensures the out-of-flow frame's
   * removal and destruction.
   */
  void Destroy() { DestroyFrom(this); }

protected:
  /**
   * Implements Destroy(). Do not call this directly except from within a
   * DestroyFrom() implementation.
   * @param  aDestructRoot is the root of the subtree being destroyed
   */
  virtual void DestroyFrom(nsIFrame* aDestructRoot) = 0;
  friend class nsFrameList; // needed to pass aDestructRoot through to children
  friend class nsLineBox;   // needed to pass aDestructRoot through to children
public:

  /**
   * Called to set the initial list of frames. This happens after the frame
   * has been initialized.
   *
   * This is only called once for a given child list, and won't be called
   * at all for child lists with no initial list of frames.
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @param   aChildList list of child frames. Each of the frames has its
   *            NS_FRAME_IS_DIRTY bit set.  Must not be empty.
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame or if the
   *            initial list of frames has already been set for that child list,
   *          NS_OK otherwise.  In this case, SetInitialChildList empties out
   *            aChildList in the process of moving the frames over to its own
   *            child list.
   * @see     #Init()
   */
  NS_IMETHOD  SetInitialChildList(nsIAtom*        aListName,
                                  nsFrameList&    aChildList) = 0;

  /**
   * This method is responsible for appending frames to the frame
   * list.  The implementation should append the frames to the specified
   * child list and then generate a reflow command.
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @param   aFrameList list of child frames to append. Each of the frames has
   *            its NS_FRAME_IS_DIRTY bit set.  Must not be empty.
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise.  In this case, AppendFrames empties out
   *            aChildList in the process of moving the frames over to its own
   *            child list.
   */
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsFrameList&    aFrameList) = 0;

  /**
   * This method is responsible for inserting frames into the frame
   * list.  The implementation should insert the new frames into the specified
   * child list and then generate a reflow command.
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @param   aPrevFrame the frame to insert frames <b>after</b>
   * @param   aFrameList list of child frames to insert <b>after</b> aPrevFrame.
   *            Each of the frames has its NS_FRAME_IS_DIRTY bit set
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise.  In this case, InsertFrames empties out
   *            aChildList in the process of moving the frames over to its own
   *            child list.
   */
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList) = 0;

  /**
   * This method is responsible for removing a frame in the frame
   * list.  The implementation should do something with the removed frame
   * and then generate a reflow command. The implementation is responsible
   * for destroying aOldFrame (the caller mustn't destroy aOldFrame).
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @param   aOldFrame the frame to remove
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_FAILURE if the child frame is not in the specified
   *            child list,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise
   */
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame) = 0;

  /**
   * Get the content object associated with this frame. Does not add a reference.
   */
  nsIContent* GetContent() const { return mContent; }

  /**
   * Get the frame that should be the parent for the frames of child elements
   * May return nsnull during reflow
   */
  virtual nsIFrame* GetContentInsertionFrame() { return this; }

  /**
   * Get the frame that should be scrolled if the content associated
   * with this frame is targeted for scrolling. For frames implementing
   * nsIScrollableFrame this will return the frame itself. For frames
   * like nsTextControlFrame that contain a scrollframe, will return
   * that scrollframe.
   */
  virtual nsIScrollableFrame* GetScrollTargetFrame() { return nsnull; }

  /**
   * Get the offsets of the frame. most will be 0,0
   *
   */
  NS_IMETHOD GetOffsets(PRInt32 &start, PRInt32 &end) const = 0;

  /**
   * Reset the offsets when splitting frames during Bidi reordering
   *
   */
  virtual void AdjustOffsetsForBidi(PRInt32 aStart, PRInt32 aEnd) {}

  /**
   * Get the style context associated with this frame.
   *
   */
  nsStyleContext* GetStyleContext() const { return mStyleContext; }
  void SetStyleContext(nsStyleContext* aContext)
  { 
    if (aContext != mStyleContext) {
      nsStyleContext* oldStyleContext = mStyleContext;
      mStyleContext = aContext;
      if (aContext) {
        aContext->AddRef();
        DidSetStyleContext(oldStyleContext);
      }
      if (oldStyleContext)
        oldStyleContext->Release();
    }
  }
  
  void SetStyleContextWithoutNotification(nsStyleContext* aContext)
  {
    if (aContext != mStyleContext) {
      if (mStyleContext)
        mStyleContext->Release();
      mStyleContext = aContext;
      if (aContext) {
        aContext->AddRef();
      }
    }
  }

  // Style post processing hook
  // Attention: the old style context is the one we're forgetting,
  // and hence possibly completely bogus for GetStyle* purposes.
  // Use PeekStyleData instead.
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) = 0;

  /**
   * Get the style data associated with this frame.  This returns a
   * const style struct pointer that should never be modified.  See
   * |nsIStyleContext::GetStyleData| for more information.
   *
   * The use of the typesafe functions below is preferred to direct use
   * of this function.
   */
  virtual const void* GetStyleDataExternal(nsStyleStructID aSID) const = 0;

  /**
   * Define typesafe getter functions for each style struct by
   * preprocessing the list of style structs.  These functions are the
   * preferred way to get style data.  The macro creates functions like:
   *   const nsStyleBorder* GetStyleBorder();
   *   const nsStyleColor* GetStyleColor();
   */

#ifdef _IMPL_NS_LAYOUT
  #define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                      \
    const nsStyle##name_ * GetStyle##name_ () const {                         \
      NS_ASSERTION(mStyleContext, "No style context found!");                 \
      return mStyleContext->GetStyle##name_ ();                               \
    }
#else
  #define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                      \
    const nsStyle##name_ * GetStyle##name_ () const {                         \
      return static_cast<const nsStyle##name_*>(                              \
                            GetStyleDataExternal(eStyleStruct_##name_));      \
    }
#endif
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

#ifdef _IMPL_NS_LAYOUT
  /** Also forward GetVisitedDependentColor to the style context */
  nscolor GetVisitedDependentColor(nsCSSProperty aProperty)
    { return mStyleContext->GetVisitedDependentColor(aProperty); }
#endif

  /**
   * These methods are to access any additional style contexts that
   * the frame may be holding. These are contexts that are children
   * of the frame's primary context and are NOT used as style contexts
   * for any child frames. These contexts also MUST NOT have any child 
   * contexts whatsoever. If you need to insert style contexts into the
   * style tree, then you should create pseudo element frames to own them
   * The indicies must be consecutive and implementations MUST return an 
   * NS_ERROR_INVALID_ARG if asked for an index that is out of range.
   */
  virtual nsStyleContext* GetAdditionalStyleContext(PRInt32 aIndex) const = 0;

  virtual void SetAdditionalStyleContext(PRInt32 aIndex,
                                         nsStyleContext* aStyleContext) = 0;

  /**
   * @return PR_FALSE if this frame definitely has no borders at all
   */                 
  PRBool HasBorder() const;

  /**
   * Accessor functions for geometric parent
   */
  nsIFrame* GetParent() const { return mParent; }
  virtual void SetParent(nsIFrame* aParent) = 0;

  /**
   * Bounding rect of the frame. The values are in app units, and the origin is
   * relative to the upper-left of the geometric parent. The size includes the
   * content area, borders, and padding.
   *
   * Note: moving or sizing the frame does not affect the view's size or
   * position.
   */
  nsRect GetRect() const { return mRect; }
  nsPoint GetPosition() const { return nsPoint(mRect.x, mRect.y); }
  nsSize GetSize() const { return nsSize(mRect.width, mRect.height); }

  /**
   * When we change the size of the frame's border-box rect, we may need to
   * reset the overflow rect if it was previously stored as deltas.
   * (If it is currently a "large" overflow and could be re-packed as deltas,
   * we don't bother as the cost of the allocation has already been paid.)
   */
  void SetRect(const nsRect& aRect) {
    if (HasOverflowRect() && mOverflow.mType != NS_FRAME_OVERFLOW_LARGE) {
      nsRect r = GetOverflowRect();
      mRect = aRect;
      SetOverflowRect(r);
    } else {
      mRect = aRect;
    }
  }
  void SetSize(const nsSize& aSize) {
    if (HasOverflowRect() && mOverflow.mType != NS_FRAME_OVERFLOW_LARGE) {
      nsRect r = GetOverflowRect();
      mRect.SizeTo(aSize);
      SetOverflowRect(r);
    } else {
      mRect.SizeTo(aSize);
    }
  }
  void SetPosition(const nsPoint& aPt) { mRect.MoveTo(aPt); }

  /**
   * Return frame's computed offset due to relative positioning
   */
  nsPoint GetRelativeOffset(const nsStyleDisplay* aDisplay = nsnull) const;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild)
  { return aChild->GetPosition(); }
  
  nsPoint GetPositionIgnoringScrolling() {
    return mParent ? mParent->GetPositionOfChildIgnoringScrolling(this)
      : GetPosition();
  }

  static void DestroyRegion(void* aPropertyValue)
  {
    delete static_cast<nsRegion*>(aPropertyValue);
  }

  static void DestroyMargin(void* aPropertyValue)
  {
    delete static_cast<nsMargin*>(aPropertyValue);
  }

  static void DestroyRect(void* aPropertyValue)
  {
    delete static_cast<nsRect*>(aPropertyValue);
  }

  static void DestroyPoint(void* aPropertyValue)
  {
    delete static_cast<nsPoint*>(aPropertyValue);
  }

#ifdef _MSC_VER
// XXX Workaround MSVC issue by making the static FramePropertyDescriptor
// non-const.  See bug 555727.
#define NS_PROPERTY_DESCRIPTOR_CONST
#else
#define NS_PROPERTY_DESCRIPTOR_CONST const
#endif

#define NS_DECLARE_FRAME_PROPERTY(prop, dtor)                                                  \
  static const FramePropertyDescriptor* prop() {                                               \
    static NS_PROPERTY_DESCRIPTOR_CONST FramePropertyDescriptor descriptor = { dtor, nsnull }; \
    return &descriptor;                                                                        \
  }
// Don't use this unless you really know what you're doing!
#define NS_DECLARE_FRAME_PROPERTY_WITH_FRAME_IN_DTOR(prop, dtor)                               \
  static const FramePropertyDescriptor* prop() {                                               \
    static NS_PROPERTY_DESCRIPTOR_CONST FramePropertyDescriptor descriptor = { nsnull, dtor }; \
    return &descriptor;                                                                        \
  }

  NS_DECLARE_FRAME_PROPERTY(IBSplitSpecialSibling, nsnull)
  NS_DECLARE_FRAME_PROPERTY(IBSplitSpecialPrevSibling, nsnull)

  NS_DECLARE_FRAME_PROPERTY(ComputedOffsetProperty, DestroyPoint)

  NS_DECLARE_FRAME_PROPERTY(OutlineInnerRectProperty, DestroyRect)
  NS_DECLARE_FRAME_PROPERTY(PreEffectsBBoxProperty, DestroyRect)
  NS_DECLARE_FRAME_PROPERTY(PreTransformBBoxProperty, DestroyRect)

  NS_DECLARE_FRAME_PROPERTY(UsedMarginProperty, DestroyMargin)
  NS_DECLARE_FRAME_PROPERTY(UsedPaddingProperty, DestroyMargin)
  NS_DECLARE_FRAME_PROPERTY(UsedBorderProperty, DestroyMargin)

  /**
   * Return the distance between the border edge of the frame and the
   * margin edge of the frame.  Like GetRect(), returns the dimensions
   * as of the most recent reflow.
   *
   * This doesn't include any margin collapsing that may have occurred.
   *
   * It also treats 'auto' margins as zero, and treats any margins that
   * should have been turned into 'auto' because of overconstraint as
   * having their original values.
   */
  virtual nsMargin GetUsedMargin() const;

  /**
   * Return the distance between the border edge of the frame (which is
   * its rect) and the padding edge of the frame. Like GetRect(), returns
   * the dimensions as of the most recent reflow.
   *
   * Note that this differs from GetStyleBorder()->GetBorder() in that
   * this describes region of the frame's box, and
   * GetStyleBorder()->GetBorder() describes a border.  They differ only
   * for tables, particularly border-collapse tables.
   */
  virtual nsMargin GetUsedBorder() const;

  /**
   * Return the distance between the padding edge of the frame and the
   * content edge of the frame.  Like GetRect(), returns the dimensions
   * as of the most recent reflow.
   */
  virtual nsMargin GetUsedPadding() const;

  nsMargin GetUsedBorderAndPadding() const {
    return GetUsedBorder() + GetUsedPadding();
  }

  /**
   * Apply the result of GetSkipSides() on this frame to an nsMargin by
   * setting to zero any sides that are skipped.
   */
  void ApplySkipSides(nsMargin& aMargin) const;

  /**
   * Like the frame's rect (see |GetRect|), which is the border rect,
   * other rectangles of the frame, in app units, relative to the parent.
   */
  nsRect GetPaddingRect() const;
  nsRect GetContentRect() const;

  /**
   * Get the size, in app units, of the border radii. It returns FALSE iff all
   * returned radii == 0 (so no border radii), TRUE otherwise.
   * For the aRadii indexes, use the NS_CORNER_* constants in nsStyleConsts.h
   * If a side is skipped via aSkipSides, its corners are forced to 0.
   *
   * All corner radii are then adjusted so they do not require more
   * space than aBorderArea, according to the algorithm in css3-background.
   *
   * aFrameSize is used as the basis for percentage widths and heights.
   * aBorderArea is used for the adjustment of radii that might be too
   * large.
   * FIXME: In the long run, we can probably get away with only one of
   * these, especially if we change the way we handle outline-radius (by
   * removing it and inflating the border radius)
   *
   * Return whether any radii are nonzero.
   */
  static PRBool ComputeBorderRadii(const nsStyleCorners& aBorderRadius,
                                   const nsSize& aFrameSize,
                                   const nsSize& aBorderArea,
                                   PRIntn aSkipSides,
                                   nscoord aRadii[8]);

  /*
   * Given a set of border radii for one box (e.g., border box), convert
   * it to the equivalent set of radii for another box (e.g., in to
   * padding box, out to outline box) by reducing radii or increasing
   * nonzero radii as appropriate.
   *
   * Indices into aRadii are the NS_CORNER_* constants in nsStyleConsts.h
   *
   * Note that InsetBorderRadii is lossy, since it can turn nonzero
   * radii into zero, and OutsetBorderRadii does not inflate zero radii.
   * Therefore, callers should always inset or outset directly from the
   * original value coming from style.
   */
  static void InsetBorderRadii(nscoord aRadii[8], const nsMargin &aOffsets);
  static void OutsetBorderRadii(nscoord aRadii[8], const nsMargin &aOffsets);

  /**
   * Fill in border radii for this frame.  Return whether any are
   * nonzero.
   *
   * Indices into aRadii are the NS_CORNER_* constants in nsStyleConsts.h
   */
  virtual PRBool GetBorderRadii(nscoord aRadii[8]) const;

  PRBool GetPaddingBoxBorderRadii(nscoord aRadii[8]) const;
  PRBool GetContentBoxBorderRadii(nscoord aRadii[8]) const;

  /**
   * Get the position of the frame's baseline, relative to the top of
   * the frame (its top border edge).  Only valid when Reflow is not
   * needed and when the frame returned nsHTMLReflowMetrics::
   * ASK_FOR_BASELINE as ascent in its reflow metrics.
   */
  virtual nscoord GetBaseline() const = 0;

  /**
   * Used to iterate the list of additional child list names. Returns the atom
   * name for the additional child list at the specified 0-based index, or a
   * NULL pointer if there are no more named child lists.
   *
   * Note that the list is only the additional named child lists and does not
   * include the unnamed principal child list.
   */
  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const = 0;

  /**
   * Get the specified child list.
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @return  the child list.  If this is an unknown list name, an empty list
   *            will be returned.
   * @see     #GetAdditionalListName()
   */
  // XXXbz if all our frame storage were actually backed by nsFrameList, we
  // could make this return a const reference...  nsBlockFrame is the only real
  // culprit here.  Make sure to assign the return value of this function into
  // a |const nsFrameList&|, not an nsFrameList.
  virtual nsFrameList GetChildList(nsIAtom* aListName) const = 0;
  // XXXbz this method should go away
  nsIFrame* GetFirstChild(nsIAtom* aListName) const {
    return GetChildList(aListName).FirstChild();
  }
  // XXXmats this method should also go away then
  nsIFrame* GetLastChild(nsIAtom* aListName) const {
    return GetChildList(aListName).LastChild();
  }

  /**
   * Child frames are linked together in a doubly-linked list
   */
  nsIFrame* GetNextSibling() const { return mNextSibling; }
  void SetNextSibling(nsIFrame* aNextSibling) {
    NS_ASSERTION(this != aNextSibling, "Creating a circular frame list, this is very bad.");
    if (mNextSibling && mNextSibling->GetPrevSibling() == this) {
      mNextSibling->mPrevSibling = nsnull;
    }
    mNextSibling = aNextSibling;
    if (mNextSibling) {
      mNextSibling->mPrevSibling = this;
    }
  }

  nsIFrame* GetPrevSibling() const { return mPrevSibling; }

  /**
   * Builds the display lists for the content represented by this frame
   * and its descendants. The background+borders of this element must
   * be added first, before any other content.
   * 
   * This should only be called by methods in nsFrame. Instead of calling this
   * directly, call either BuildDisplayListForStackingContext or
   * BuildDisplayListForChild.
   * 
   * See nsDisplayList.h for more information about display lists.
   * 
   * @param aDirtyRect content outside this rectangle can be ignored; the
   * rectangle is in frame coordinates
   */
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) { return NS_OK; }
  /**
   * Displays the caret onto the given display list builder. The caret is
   * painted on top of the rest of the display list items.
   *
   * @param aDirtyRect is the dirty rectangle that we're repainting.
   */
  nsresult DisplayCaret(nsDisplayListBuilder*       aBuilder,
                        const nsRect&               aDirtyRect,
                        nsDisplayList*              aList);

  /**
   * Get the preferred caret color at the offset.
   *
   * @param aOffset is offset of the content.
   */
  virtual nscolor GetCaretColorAt(PRInt32 aOffset);

 
  PRBool IsThemed(nsITheme::Transparency* aTransparencyState = nsnull) {
    return IsThemed(GetStyleDisplay(), aTransparencyState);
  }
  PRBool IsThemed(const nsStyleDisplay* aDisp,
                  nsITheme::Transparency* aTransparencyState = nsnull) {
    if (!aDisp->mAppearance)
      return PR_FALSE;
    nsPresContext* pc = PresContext();
    nsITheme *theme = pc->GetTheme();
    if(!theme || !theme->ThemeSupportsWidget(pc, this, aDisp->mAppearance))
      return PR_FALSE;
    if (aTransparencyState) {
      *aTransparencyState = theme->GetWidgetTransparency(this, aDisp->mAppearance);
    }
    return PR_TRUE;
  }
  
  /**
   * Builds a display list for the content represented by this frame,
   * treating this frame as the root of a stacking context.
   * @param aDirtyRect content outside this rectangle can be ignored; the
   * rectangle is in frame coordinates
   */
  nsresult BuildDisplayListForStackingContext(nsDisplayListBuilder* aBuilder,
                                              const nsRect&         aDirtyRect,
                                              nsDisplayList*        aList);

  /**
   * Clips the display items of aFromSet, putting the results in aToSet.
   * Only items corresponding to frames which are descendants of this frame
   * are clipped. In other words, descendant elements whose CSS boxes do not
   * have this frame as a container are not clipped. Also,
   * border/background/outline items for this frame are not clipped,
   * unless aClipBorderBackground is set to PR_TRUE. (We need this because
   * a scrollframe must overflow-clip its scrolled child's background/borders.)
   *
   * Indices into aClipRadii are the NS_CORNER_* constants in nsStyleConsts.h
   */
  nsresult OverflowClip(nsDisplayListBuilder*   aBuilder,
                        const nsDisplayListSet& aFromSet,
                        const nsDisplayListSet& aToSet,
                        const nsRect&           aClipRect,
                        const nscoord           aClipRadii[8],
                        PRBool                  aClipBorderBackground = PR_FALSE,
                        PRBool                  aClipAll = PR_FALSE);

  enum {
    DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT = 0x01,
    DISPLAY_CHILD_FORCE_STACKING_CONTEXT = 0x02,
    DISPLAY_CHILD_INLINE = 0x04
  };
  /**
   * Adjusts aDirtyRect for the child's offset, checks that the dirty rect
   * actually intersects the child (or its descendants), calls BuildDisplayList
   * on the child if necessary, and puts things in the right lists if the child
   * is positioned.
   *
   * @param aFlags combination of DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT,
   *    DISPLAY_CHILD_FORCE_STACKING_CONTEXT and DISPLAY_CHILD_INLINE
   */
  nsresult BuildDisplayListForChild(nsDisplayListBuilder*   aBuilder,
                                    nsIFrame*               aChild,
                                    const nsRect&           aDirtyRect,
                                    const nsDisplayListSet& aLists,
                                    PRUint32                aFlags = 0);

  /**
   * A helper for replaced elements that want to clip their content to a
   * border radius, but only need clipping at all when they have a
   * border radius.
   */
  void WrapReplacedContentForBorderRadius(nsDisplayListBuilder* aBuilder,
                                          nsDisplayList* aFromList,
                                          const nsDisplayListSet& aToLists);

  /**
   * Does this frame need a view?
   */
  virtual PRBool NeedsView() { return PR_FALSE; }

  /**
   * Returns whether this frame has a transform matrix applied to it.  This is true
   * if we have the -moz-transform property or if we're an SVGForeignObjectFrame.
   */
  virtual PRBool IsTransformed() const;

  /**
   * Event handling of GUI events.
   *
   * @param   aEvent event structure describing the type of event and rge widget
   *            where the event originated
   *          The |point| member of this is in the coordinate system of the
   *          view returned by GetOffsetFromView.
   * @param   aEventStatus a return value indicating whether the event was handled
   *            and whether default processing should be done
   *
   * XXX From a frame's perspective it's unclear what the effect of the event status
   * is. Does it cause the event to continue propagating through the frame hierarchy
   * or is it just returned to the widgets?
   *
   * @see     nsGUIEvent
   * @see     nsEventStatus
   */
  NS_IMETHOD  HandleEvent(nsPresContext* aPresContext,
                          nsGUIEvent*     aEvent,
                          nsEventStatus*  aEventStatus) = 0;

  NS_IMETHOD  GetContentForEvent(nsPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent) = 0;

  // This structure keeps track of the content node and offsets associated with
  // a point; there is a primary and a secondary offset associated with any
  // point.  The primary and secondary offsets differ when the point is over a
  // non-text object.  The primary offset is the expected position of the
  // cursor calculated from a point; the secondary offset, when it is different,
  // indicates that the point is in the boundaries of some selectable object.
  // Note that the primary offset can be after the secondary offset; for places
  // that need the beginning and end of the object, the StartOffset and 
  // EndOffset helpers can be used.
  struct NS_STACK_CLASS ContentOffsets {
    nsCOMPtr<nsIContent> content;
    PRBool IsNull() { return !content; }
    PRInt32 offset;
    PRInt32 secondaryOffset;
    // Helpers for places that need the ends of the offsets and expect them in
    // numerical order, as opposed to wanting the primary and secondary offsets
    PRInt32 StartOffset() { return NS_MIN(offset, secondaryOffset); }
    PRInt32 EndOffset() { return NS_MAX(offset, secondaryOffset); }
    // This boolean indicates whether the associated content is before or after
    // the offset; the most visible use is to allow the caret to know which line
    // to display on.
    PRBool associateWithNext;
  };
  /**
   * This function calculates the content offsets for selection relative to
   * a point.  Note that this should generally only be callled on the event
   * frame associated with an event because this function does not account
   * for frame lists other than the primary one.
   * @param aPoint point relative to this frame
   */
  ContentOffsets GetContentOffsetsFromPoint(nsPoint aPoint,
                                            PRBool aIgnoreSelectionStyle = PR_FALSE);

  virtual ContentOffsets GetContentOffsetsFromPointExternal(nsPoint aPoint,
                                                            PRBool aIgnoreSelectionStyle = PR_FALSE)
  { return GetContentOffsetsFromPoint(aPoint, aIgnoreSelectionStyle); }

  /**
   * This structure holds information about a cursor. mContainer represents a
   * loaded image that should be preferred. If it is not possible to use it, or
   * if it is null, mCursor should be used.
   */
  struct NS_STACK_CLASS Cursor {
    nsCOMPtr<imgIContainer> mContainer;
    PRInt32                 mCursor;
    PRBool                  mHaveHotspot;
    float                   mHotspotX, mHotspotY;
  };
  /**
   * Get the cursor for a given frame.
   */
  NS_IMETHOD  GetCursor(const nsPoint&  aPoint,
                        Cursor&         aCursor) = 0;

  /**
   * Get a point (in the frame's coordinate space) given an offset into
   * the content. This point should be on the baseline of text with
   * the correct horizontal offset
   */
  NS_IMETHOD  GetPointFromOffset(PRInt32                  inOffset,
                                 nsPoint*                 outPoint) = 0;
  
  /**
   * Get the child frame of this frame which contains the given
   * content offset. outChildFrame may be this frame, or nsnull on return.
   * outContentOffset returns the content offset relative to the start
   * of the returned node. You can also pass a hint which tells the method
   * to stick to the end of the first found frame or the beginning of the 
   * next in case the offset falls on a boundary.
   */
  NS_IMETHOD  GetChildFrameContainingOffset(PRInt32       inContentOffset,
                                 PRBool                   inHint,//false stick left
                                 PRInt32*                 outFrameContentOffset,
                                 nsIFrame*                *outChildFrame) = 0;

 /**
   * Get the current frame-state value for this frame. aResult is
   * filled in with the state bits. 
   */
  nsFrameState GetStateBits() const { return mState; }

  /**
   * Update the current frame-state value for this frame. 
   */
  void AddStateBits(nsFrameState aBits) { mState |= aBits; }
  void RemoveStateBits(nsFrameState aBits) { mState &= ~aBits; }

  /**
   * This call is invoked on the primary frame for a character data content
   * node, when it is changed in the content tree.
   */
  NS_IMETHOD  CharacterDataChanged(CharacterDataChangeInfo* aInfo) = 0;

  /**
   * This call is invoked when the value of a content objects's attribute
   * is changed. 
   * The first frame that maps that content is asked to deal
   * with the change by doing whatever is appropriate.
   *
   * @param aNameSpaceID the namespace of the attribute
   * @param aAttribute the atom name of the attribute
   * @param aModType Whether or not the attribute was added, changed, or removed.
   *   The constants are defined in nsIDOMMutationEvent.h.
   */
  NS_IMETHOD  AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType) = 0;

  /**
   * Return how your frame can be split.
   */
  virtual nsSplittableType GetSplittableType() const = 0;

  /**
   * Continuation member functions
   */
  virtual nsIFrame* GetPrevContinuation() const = 0;
  NS_IMETHOD SetPrevContinuation(nsIFrame*) = 0;
  virtual nsIFrame* GetNextContinuation() const = 0;
  NS_IMETHOD SetNextContinuation(nsIFrame*) = 0;
  virtual nsIFrame* GetFirstContinuation() const {
    return const_cast<nsIFrame*>(this);
  }
  virtual nsIFrame* GetLastContinuation() const {
    return const_cast<nsIFrame*>(this);
  }

  /**
   * GetTailContinuation gets the last non-overflow-container continuation
   * in the continuation chain, i.e. where the next sibling element
   * should attach).
   */
  nsIFrame* GetTailContinuation();

  /**
   * Flow member functions
   */
  virtual nsIFrame* GetPrevInFlowVirtual() const = 0;
  nsIFrame* GetPrevInFlow() const { return GetPrevInFlowVirtual(); }
  NS_IMETHOD SetPrevInFlow(nsIFrame*) = 0;

  virtual nsIFrame* GetNextInFlowVirtual() const = 0;
  nsIFrame* GetNextInFlow() const { return GetNextInFlowVirtual(); }
  NS_IMETHOD SetNextInFlow(nsIFrame*) = 0;

  /**
   * Return the first frame in our current flow. 
   */
  virtual nsIFrame* GetFirstInFlow() const {
    return const_cast<nsIFrame*>(this);
  }

  /**
   * Return the last frame in our current flow.
   */
  virtual nsIFrame* GetLastInFlow() const {
    return const_cast<nsIFrame*>(this);
  }


  /**
   * Mark any stored intrinsic width information as dirty (requiring
   * re-calculation).  Note that this should generally not be called
   * directly; nsPresShell::FrameNeedsReflow will call it instead.
   */
  virtual void MarkIntrinsicWidthsDirty() = 0;

  /**
   * Get the intrinsic minimum width of the frame.  This must be less
   * than or equal to the intrinsic width.
   *
   * This is *not* affected by the CSS 'min-width', 'width', and
   * 'max-width' properties on this frame, but it is affected by the
   * values of those properties on this frame's descendants.  (It may be
   * called during computation of the values of those properties, so it
   * cannot depend on any values in the nsStylePosition for this frame.)
   *
   * The value returned should **NOT** include the space required for
   * padding and border.
   *
   * Note that many frames will cache the result of this function call
   * unless MarkIntrinsicWidthsDirty is called.
   *
   * It is not acceptable for a frame to mark itself dirty when this
   * method is called.
   *
   * This method must not return a negative value.
   */
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext) = 0;

  /**
   * Get the intrinsic width of the frame.  This must be greater than or
   * equal to the intrinsic minimum width.
   *
   * Otherwise, all the comments for |GetMinWidth| above apply.
   */
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext) = 0;

  /**
   * |InlineIntrinsicWidth| represents the intrinsic width information
   * in inline layout.  Code that determines the intrinsic width of a
   * region of inline layout accumulates the result into this structure.
   * This pattern is needed because we need to maintain state
   * information about whitespace (for both collapsing and trimming).
   */
  struct InlineIntrinsicWidthData {
    InlineIntrinsicWidthData()
      : line(nsnull)
      , lineContainer(nsnull)
      , prevLines(0)
      , currentLine(0)
      , skipWhitespace(PR_TRUE)
      , trailingWhitespace(0)
    {}

    // The line. This may be null if the inlines are not associated with
    // a block or if we just don't know the line.
    const nsLineList_iterator* line;

    // The line container.
    nsIFrame* lineContainer;

    // The maximum intrinsic width for all previous lines.
    nscoord prevLines;

    // The maximum intrinsic width for the current line.  At a line
    // break (mandatory for preferred width; allowed for minimum width),
    // the caller should call |Break()|.
    nscoord currentLine;

    // True if initial collapsable whitespace should be skipped.  This
    // should be true at the beginning of a block, after hard breaks
    // and when the last text ended with whitespace.
    PRBool skipWhitespace;

    // This contains the width of the trimmable whitespace at the end of
    // |currentLine|; it is zero if there is no such whitespace.
    nscoord trailingWhitespace;

    // Floats encountered in the lines.
    nsTArray<nsIFrame*> floats;
  };

  struct InlineMinWidthData : public InlineIntrinsicWidthData {
    InlineMinWidthData()
      : trailingTextFrame(nsnull)
      , atStartOfLine(PR_TRUE)
    {}

    // We need to distinguish forced and optional breaks for cases where the
    // current line total is negative.  When it is, we need to ignore
    // optional breaks to prevent min-width from ending up bigger than
    // pref-width.
    void ForceBreak(nsIRenderingContext *aRenderingContext);
    void OptionallyBreak(nsIRenderingContext *aRenderingContext);

    // The last text frame processed so far in the current line, when
    // the last characters in that text frame are relevant for line
    // break opportunities.
    nsIFrame *trailingTextFrame;

    // Whether we're currently at the start of the line.  If we are, we
    // can't break (for example, between the text-indent and the first
    // word).
    PRBool atStartOfLine;
  };

  struct InlinePrefWidthData : public InlineIntrinsicWidthData {
    void ForceBreak(nsIRenderingContext *aRenderingContext);
  };

  /**
   * Add the intrinsic minimum width of a frame in a way suitable for
   * use in inline layout to an |InlineIntrinsicWidthData| object that
   * represents the intrinsic width information of all the previous
   * frames in the inline layout region.
   *
   * All *allowed* breakpoints within the frame determine what counts as
   * a line for the |InlineIntrinsicWidthData|.  This means that
   * |aData->trailingWhitespace| will always be zero (unlike for
   * AddInlinePrefWidth).
   *
   * All the comments for |GetMinWidth| apply, except that this function
   * is responsible for adding padding, border, and margin and for
   * considering the effects of 'width', 'min-width', and 'max-width'.
   *
   * This may be called on any frame.  Frames that do not participate in
   * line breaking can inherit the default implementation on nsFrame,
   * which calls |GetMinWidth|.
   */
  virtual void
  AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                    InlineMinWidthData *aData) = 0;

  /**
   * Add the intrinsic preferred width of a frame in a way suitable for
   * use in inline layout to an |InlineIntrinsicWidthData| object that
   * represents the intrinsic width information of all the previous
   * frames in the inline layout region.
   *
   * All the comments for |AddInlineMinWidth| and |GetPrefWidth| apply,
   * except that this fills in an |InlineIntrinsicWidthData| structure
   * based on using all *mandatory* breakpoints within the frame.
   */
  virtual void
  AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                     InlinePrefWidthData *aData) = 0;

  /**
   * Return the horizontal components of padding, border, and margin
   * that contribute to the intrinsic width that applies to the parent.
   */
  struct IntrinsicWidthOffsetData {
    nscoord hPadding, hBorder, hMargin;
    float hPctPadding, hPctMargin;

    IntrinsicWidthOffsetData()
      : hPadding(0), hBorder(0), hMargin(0)
      , hPctPadding(0.0f), hPctMargin(0.0f)
    {}
  };
  virtual IntrinsicWidthOffsetData
    IntrinsicWidthOffsets(nsIRenderingContext* aRenderingContext) = 0;

  /*
   * For replaced elements only. Gets the intrinsic dimensions of this element.
   * The dimensions may only be one of the following three types:
   *
   *   eStyleUnit_Coord   - a length in app units
   *   eStyleUnit_Percent - a percentage of the available space
   *   eStyleUnit_None    - the element has no intrinsic size in this dimension
   */
  struct IntrinsicSize {
    nsStyleCoord width, height;

    IntrinsicSize()
      : width(eStyleUnit_None), height(eStyleUnit_None)
    {}
    IntrinsicSize(const IntrinsicSize& rhs)
      : width(rhs.width), height(rhs.height)
    {}
    IntrinsicSize& operator=(const IntrinsicSize& rhs) {
      width = rhs.width; height = rhs.height; return *this;
    }
    PRBool operator==(const IntrinsicSize& rhs) {
      return width == rhs.width && height == rhs.height;
    }
    PRBool operator!=(const IntrinsicSize& rhs) {
      return !(*this == rhs);
    }
  };
  virtual IntrinsicSize GetIntrinsicSize() = 0;

  /*
   * Get the intrinsic ratio of this element, or nsSize(0,0) if it has
   * no intrinsic ratio.  The intrinsic ratio is the ratio of the
   * height/width of a box with an intrinsic size or the intrinsic
   * aspect ratio of a scalable vector image without an intrinsic size.
   *
   * Either one of the sides may be zero, indicating a zero or infinite
   * ratio.
   */
  virtual nsSize GetIntrinsicRatio() = 0;

  /**
   * Compute the size that a frame will occupy.  Called while
   * constructing the nsHTMLReflowState to be used to Reflow the frame,
   * in order to fill its mComputedWidth and mComputedHeight member
   * variables.
   *
   * The |height| member of the return value may be
   * NS_UNCONSTRAINEDSIZE, but the |width| member must not be.
   *
   * Note that the reason that border and padding need to be passed
   * separately is so that the 'box-sizing' property can be handled.
   * Thus aMargin includes absolute positioning offsets as well.
   *
   * @param aCBSize  The size of the element's containing block.  (Well,
   *                 the |height| component isn't really.)
   * @param aAvailableWidth  The available width for 'auto' widths.
   *                         This is usually the same as aCBSize.width,
   *                         but differs in cases such as block
   *                         formatting context roots next to floats, or
   *                         in some cases of float reflow in quirks
   *                         mode.
   * @param aMargin  The sum of the vertical / horizontal margins
   *                 ***AND*** absolute positioning offsets (top, right,
   *                 bottom, left) of the frame, including actual values
   *                 resulting from percentages and from the
   *                 "hypothetical box" for absolute positioning, but
   *                 not including actual values resulting from 'auto'
   *                 margins or ignored 'auto' values in absolute
   *                 positioning.
   * @param aBorder  The sum of the vertical / horizontal border widths
   *                 of the frame.
   * @param aPadding  The sum of the vertical / horizontal margins of
   *                  the frame, including actual values resulting from
   *                  percentages.
   * @param aShrinkWrap  Whether the frame is in a context where
   *                     non-replaced blocks should shrink-wrap (e.g.,
   *                     it's floating, absolutely positioned, or
   *                     inline-block).
   */
  virtual nsSize ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap) = 0;

  /**
   * Compute a tight bounding rectangle for the frame. This is a rectangle
   * that encloses the pixels that are actually drawn. We're allowed to be
   * conservative and currently we don't try very hard. The rectangle is
   * in appunits and relative to the origin of this frame.
   * @param aContext a rendering context that can be used if we need
   * to do measurement
   */
  virtual nsRect ComputeTightBounds(gfxContext* aContext) const;

  /**
   * Pre-reflow hook. Before a frame is reflowed this method will be called.
   * This call will always be invoked at least once before a subsequent Reflow
   * and DidReflow call. It may be called more than once, In general you will
   * receive on WillReflow notification before each Reflow request.
   *
   * XXX Is this really the semantics we want? Because we have the NS_FRAME_IN_REFLOW
   * bit we can ensure we don't call it more than once...
   */
  NS_IMETHOD  WillReflow(nsPresContext* aPresContext) = 0;

  /**
   * The frame is given an available size and asked for its desired
   * size.  This is the frame's opportunity to reflow its children.
   *
   * If the frame has the NS_FRAME_IS_DIRTY bit set then it is
   * responsible for completely reflowing itself and all of its
   * descendants.
   *
   * Otherwise, if the frame has the NS_FRAME_HAS_DIRTY_CHILDREN bit
   * set, then it is responsible for reflowing at least those
   * children that have NS_FRAME_HAS_DIRTY_CHILDREN or NS_FRAME_IS_DIRTY
   * set.
   *
   * If a difference in available size from the previous reflow causes
   * the frame's size to change, it should reflow descendants as needed.
   *
   * @param aReflowMetrics <i>out</i> parameter where you should return the
   *          desired size and ascent/descent info. You should include any
   *          space you want for border/padding in the desired size you return.
   *
   *          It's okay to return a desired size that exceeds the avail
   *          size if that's the smallest you can be, i.e. it's your
   *          minimum size.
   *
   *          For an incremental reflow you are responsible for invalidating
   *          any area within your frame that needs repainting (including
   *          borders). If your new desired size is different than your current
   *          size, then your parent frame is responsible for making sure that
   *          the difference between the two rects is repainted
   *
   * @param aReflowState information about your reflow including the reason
   *          for the reflow and the available space in which to lay out. Each
   *          dimension of the available space can either be constrained or
   *          unconstrained (a value of NS_UNCONSTRAINEDSIZE).
   *
   *          Note that the available space can be negative. In this case you
   *          still must return an accurate desired size. If you're a container
   *          you must <b>always</b> reflow at least one frame regardless of the
   *          available space
   *
   * @param aStatus a return value indicating whether the frame is complete
   *          and whether the next-in-flow is dirty and needs to be reflowed
   */
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aReflowMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) = 0;

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
  NS_IMETHOD  DidReflow(nsPresContext*           aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus         aStatus) = 0;

  // XXX Maybe these three should be a separate interface?

  /**
   * Helper method used by block reflow to identify runs of text so
   * that proper word-breaking can be done.
   *
   * @return 
   *    PR_TRUE if we can continue a "text run" through the frame. A
   *    text run is text that should be treated contiguously for line
   *    and word breaking.
   */
  virtual PRBool CanContinueTextRun() const = 0;

  /**
   * Append the rendered text to the passed-in string.
   * The appended text will often not contain all the whitespace from source,
   * depending on whether the CSS rule "white-space: pre" is active for this frame.
   * if aStartOffset + aLength goes past end, or if aLength is not specified
   * then use the text up to the string's end.
   * Call this on the primary frame for a text node.
   * @param aAppendToString   String to append text to, or null if text should not be returned
   * @param aSkipChars         if aSkipIter is non-null, this must also be non-null.
   * This gets used as backing data for the iterator so it should outlive the iterator.
   * @param aSkipIter         Where to fill in the gfxSkipCharsIterator info, or null if not needed by caller
   * @param aStartOffset       Skipped (rendered text) start offset
   * @param aSkippedMaxLength  Maximum number of characters to return
   * The iterator can be used to map content offsets to offsets in the returned string, or vice versa.
   */
  virtual nsresult GetRenderedText(nsAString* aAppendToString = nsnull,
                                   gfxSkipChars* aSkipChars = nsnull,
                                   gfxSkipCharsIterator* aSkipIter = nsnull,
                                   PRUint32 aSkippedStartOffset = 0,
                                   PRUint32 aSkippedMaxLength = PR_UINT32_MAX)
  { return NS_ERROR_NOT_IMPLEMENTED; }

  /**
   * Accessor functions to get/set the associated view object
   *
   * GetView returns non-null if and only if |HasView| returns true.
   */
  PRBool HasView() const { return !!(mState & NS_FRAME_HAS_VIEW); }
  nsIView* GetView() const;
  virtual nsIView* GetViewExternal() const;
  nsresult SetView(nsIView* aView);

  /**
   * Find the closest view (on |this| or an ancestor).
   * If aOffset is non-null, it will be set to the offset of |this|
   * from the returned view.
   */
  nsIView* GetClosestView(nsPoint* aOffset = nsnull) const;

  /**
   * Find the closest ancestor (excluding |this| !) that has a view
   */
  nsIFrame* GetAncestorWithView() const;
  virtual nsIFrame* GetAncestorWithViewExternal() const;

  /**
   * Get the offset between the coordinate systems of |this| and aOther.
   * Adding the return value to a point in the coordinate system of |this|
   * will transform the point to the coordinate system of aOther.
   *
   * aOther must be non-null.
   * 
   * This function is fastest when aOther is an ancestor of |this|.
   *
   * This function _DOES NOT_ work across document boundaries.
   * Use this function only when |this| and aOther are in the same document.
   *
   * NOTE: this actually returns the offset from aOther to |this|, but
   * that offset is added to transform _coordinates_ from |this| to
   * aOther.
   */
  nsPoint GetOffsetTo(const nsIFrame* aOther) const;
  virtual nsPoint GetOffsetToExternal(const nsIFrame* aOther) const;

  /**
   * Get the offset between the coordinate systems of |this| and aOther
   * expressed in appunits per dev pixel of |this|' document. Adding the return
   * value to a point that is relative to the origin of |this| will make the
   * point relative to the origin of aOther but in the appunits per dev pixel
   * ratio of |this|.
   *
   * aOther must be non-null.
   * 
   * This function is fastest when aOther is an ancestor of |this|.
   *
   * This function works across document boundaries.
   *
   * Because this function may cross document boundaries that have different
   * app units per dev pixel ratios it needs to be used very carefully.
   *
   * NOTE: this actually returns the offset from aOther to |this|, but
   * that offset is added to transform _coordinates_ from |this| to
   * aOther.
   */
  nsPoint GetOffsetToCrossDoc(const nsIFrame* aOther) const;

  /**
   * Get the screen rect of the frame in pixels.
   * @return the pixel rect of the frame in screen coordinates.
   */
  nsIntRect GetScreenRect() const;
  virtual nsIntRect GetScreenRectExternal() const;

  /**
   * Get the screen rect of the frame in app units.
   * @return the app unit rect of the frame in screen coordinates.
   */
  nsRect GetScreenRectInAppUnits() const;
  virtual nsRect GetScreenRectInAppUnitsExternal() const;

  /**
   * Returns the offset from this frame to the closest geometric parent that
   * has a view. Also returns the containing view or null in case of error
   */
  NS_IMETHOD  GetOffsetFromView(nsPoint&  aOffset,
                                nsIView** aView) const = 0;

  /**
   * Returns true if and only if all views, from |GetClosestView| up to
   * the top of the view hierarchy are visible.
   */
  virtual PRBool AreAncestorViewsVisible() const;

  /**
   * Returns the nearest widget containing this frame. If this frame has a
   * view and the view has a widget, then this frame's widget is
   * returned, otherwise this frame's geometric parent is checked
   * recursively upwards.
   * XXX virtual because gfx callers use it! (themes)
   */
  virtual nsIWidget* GetNearestWidget() const;

  /**
   * Same as GetNearestWidget() above but uses an outparam to return the offset
   * of this frame to the returned widget expressed in appunits of |this| (the
   * widget might be in a different document with a different zoom).
   */
  virtual nsIWidget* GetNearestWidget(nsPoint& aOffset) const;

  /**
   * Get the "type" of the frame. May return a NULL atom pointer
   *
   * @see nsGkAtoms
   */
  virtual nsIAtom* GetType() const = 0;

  /**
   * Returns a transformation matrix that converts points in this frame's coordinate space
   * to points in some ancestor frame's coordinate space.  The frame decides which ancestor
   * it will use as a reference point.  If this frame has no ancestor, aOutAncestor will be
   * set to null.
   *
   * @param aOutAncestor [out] The ancestor frame the frame has chosen.  If this frame has no
   *        ancestor, aOutAncestor will be nsnull.
   * @return A gfxMatrix that converts points in this frame's coordinate space into
   *         points in aOutAncestor's coordinate space.
   */
  virtual gfxMatrix GetTransformMatrix(nsIFrame **aOutAncestor);

  /**
   * Bit-flags to pass to IsFrameOfType()
   */
  enum {
    eMathML =                           1 << 0,
    eSVG =                              1 << 1,
    eSVGForeignObject =                 1 << 2,
    eSVGContainer =                     1 << 3,
    eSVGGeometry =                      1 << 4,
    eSVGPaintServer =                   1 << 5,
    eBidiInlineContainer =              1 << 6,
    // the frame is for a replaced element, such as an image
    eReplaced =                         1 << 7,
    // Frame that contains a block but looks like a replaced element
    // from the outside
    eReplacedContainsBlock =            1 << 8,
    // A frame that participates in inline reflow, i.e., one that
    // requires nsHTMLReflowState::mLineLayout.
    eLineParticipant =                  1 << 9,
    eXULBox =                           1 << 10,
    eCanContainOverflowContainers =     1 << 11,
    eBlockFrame =                       1 << 12,
    // If this bit is set, the frame doesn't allow ignorable whitespace as
    // children. For example, the whitespace between <table>\n<tr>\n<td>
    // will be excluded during the construction of children. 
    eExcludesIgnorableWhitespace =      1 << 13,

    // These are to allow nsFrame::Init to assert that IsFrameOfType
    // implementations all call the base class method.  They are only
    // meaningful in DEBUG builds.
    eDEBUGAllFrames =                   1 << 30,
    eDEBUGNoFrames =                    1 << 31
  };

  /**
   * API for doing a quick check if a frame is of a given
   * type. Returns true if the frame matches ALL flags passed in.
   *
   * Implementations should always override with inline virtual
   * functions that call the base class's IsFrameOfType method.
   */
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
#ifdef DEBUG
    return !(aFlags & ~(nsIFrame::eDEBUGAllFrames));
#else
    return !aFlags;
#endif
  }

  /**
   * Is this frame a containing block for non-positioned elements?
   */
  virtual PRBool IsContainingBlock() const = 0;

  /**
   * Is this frame a containing block for floating elements?
   * Note that very few frames are, so default to false.
   */
  virtual PRBool IsFloatContainingBlock() const { return PR_FALSE; }

  /**
   * Is this a leaf frame?  Frames that want the frame constructor to be able
   * to construct kids for them should return false, all others should return
   * true.  Note that returning true here does not mean that the frame _can't_
   * have kids.  It could still have kids created via
   * nsIAnonymousContentCreator.  Returning true indicates that "normal"
   * (non-anonymous, XBL-bound, CSS generated content, etc) children should not
   * be constructed.
   */
  virtual PRBool IsLeaf() const;

  /**
   * This must only be called on frames that are display roots (see
   * nsLayoutUtils::GetDisplayRootFrame). This causes all invalidates
   * reaching this frame to be performed asynchronously off an event,
   * instead of being applied to the widget immediately. Also,
   * invalidation of areas in aExcludeRegion is ignored completely
   * for invalidates with INVALIDATE_EXCLUDE_CURRENT_PAINT specified.
   * These can't be nested; two invocations of
   * BeginDeferringInvalidatesForDisplayRoot for a frame must have a
   * EndDeferringInvalidatesForDisplayRoot between them.
   */
  void BeginDeferringInvalidatesForDisplayRoot(const nsRegion& aExcludeRegion);

  /**
   * Cancel the most recent BeginDeferringInvalidatesForDisplayRoot.
   */
  void EndDeferringInvalidatesForDisplayRoot();

  /**
   * Mark this frame as using active layers. This marking will time out
   * after a short period. This call does no immediate invalidation,
   * but when the mark times out, we'll invalidate the frame's overflow
   * area.
   */
  void MarkLayersActive();

  /**
   * Return true if this frame is marked as needing active layers.
   */
  PRBool AreLayersMarkedActive();
  
  /**
   * @param aFlags see InvalidateInternal below
   */
  void InvalidateWithFlags(const nsRect& aDamageRect, PRUint32 aFlags);

  /**
   * Invalidate part of the frame by asking the view manager to repaint.
   * aDamageRect is allowed to extend outside the frame's bounds. We'll do the right
   * thing.
   * We deliberately don't have an Invalidate() method that defaults to the frame's bounds.
   * We want all callers to *think* about what has changed in the frame and what area might
   * need to be repainted.
   *
   * @param aDamageRect is in the frame's local coordinate space
   */
  void Invalidate(const nsRect& aDamageRect)
  { return InvalidateWithFlags(aDamageRect, 0); }

  /**
   * As Invalidate above, except that this should be called when the
   * rendering that has changed is performed using layers so we can avoid
   * updating the contents of ThebesLayers.
   * @param aDisplayItemKey must not be zero; indicates the kind of display
   * item that is being invalidated.
   */
  void InvalidateLayer(const nsRect& aDamageRect, PRUint32 aDisplayItemKey);

  /**
   * Helper function that can be overridden by frame classes. The rectangle
   * (plus aOffsetX/aOffsetY) is relative to this frame.
   * 
   * The offset is given as two coords rather than as an nsPoint because
   * gcc optimizes it better that way, in particular in the default
   * implementation that passes the area to the parent frame becomes a tail
   * call.
   *
   * The default implementation will crash if the frame has no parent so
   * frames without parents MUST* override.
   * 
   * @param aForChild if the invalidation is coming from a child frame, this
   * is the frame; otherwise, this is null.
   * @param aFlags INVALIDATE_IMMEDIATE: repaint now if true, repaint later if false.
   *   In case it's true, pending notifications will be flushed which
   *   could cause frames to be deleted (including |this|).
   * @param aFlags INVALIDATE_CROSS_DOC: true if the invalidation
   *   originated in a subdocument
   * @param aFlags INVALIDATE_REASON_SCROLL_BLIT: set if the invalidation
   * was really just the scroll machinery copying pixels from one
   * part of the window to another
   * @param aFlags INVALIDATE_REASON_SCROLL_REPAINT: set if the invalidation
   * was triggered by scrolling
   * @param aFlags INVALIDATE_NO_THEBES_LAYERS: don't invalidate the
   * ThebesLayers of any container layer owned by an ancestor. Set this
   * only if ThebesLayers definitely don't need to be updated.
   * @param aFlags INVALIDATE_EXCLUDE_CURRENT_PAINT: if the invalidation
   * occurs while we're painting (to be precise, while
   * BeginDeferringInvalidatesForDisplayRoot is active on the display root),
   * then invalidation in the current paint region is simply discarded.
   * Use this flag if areas that are being painted do not need
   * to be invalidated. By default, when this flag is not specified,
   * areas that are invalidated while currently being painted will be repainted
   * again.
   * This flag is useful when, during painting, FrameLayerBuilder discovers that
   * a region of the window needs to be drawn differently, and that region
   * may or may not be contained in the currently painted region.
   */
  enum {
    INVALIDATE_IMMEDIATE = 0x01,
    INVALIDATE_CROSS_DOC = 0x02,
    INVALIDATE_REASON_SCROLL_BLIT = 0x04,
    INVALIDATE_REASON_SCROLL_REPAINT = 0x08,
    INVALIDATE_REASON_MASK = INVALIDATE_REASON_SCROLL_BLIT |
                             INVALIDATE_REASON_SCROLL_REPAINT,
    INVALIDATE_NO_THEBES_LAYERS = 0x10,
    INVALIDATE_EXCLUDE_CURRENT_PAINT = 0x20
  };
  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aOffsetX, nscoord aOffsetY,
                                  nsIFrame* aForChild, PRUint32 aFlags);

  /**
   * Helper function that funnels an InvalidateInternal request up to the
   * parent.  This function is used so that if MOZ_SVG is not defined, we still
   * have unified control paths in the InvalidateInternal chain.
   *
   * @param aDamageRect The rect to invalidate.
   * @param aX The x offset from the origin of this frame to the rectangle.
   * @param aY The y offset from the origin of this frame to the rectangle.
   * @param aImmediate Whether to redraw immediately.
   * @return None, though this funnels the request up to the parent frame.
   */
  void InvalidateInternalAfterResize(const nsRect& aDamageRect, nscoord aX,
                                     nscoord aY, PRUint32 aFlags);

  /**
   * Take two rectangles in the coordinate system of this frame which
   * have the same origin and invalidate the difference between them.
   * This is a helper method to be used when a frame is being resized.
   *
   * @param aR1 the first rectangle
   * @param aR2 the second rectangle
   */
  void InvalidateRectDifference(const nsRect& aR1, const nsRect& aR2);

  /**
   * Invalidate the entire frame subtree for this frame. Invalidates this
   * frame's overflow rect, and also ensures that all ThebesLayer children
   * of ContainerLayers associated with frames in this subtree are
   * completely invalidated.
   */
  void InvalidateFrameSubtree();

  /**
   * Invalidate the overflow area for this frame. Invalidates this
   * frame's overflow rect. Does not necessarily cause ThebesLayers for
   * descendant frames to be repainted; only this frame can be relied on
   * to be repainted.
   */
  void InvalidateOverflowRect();

  /**
   * Computes a rect that encompasses everything that might be painted by
   * this frame.  This includes this frame, all its descendent frames, this
   * frame's outline, and descentant frames' outline, but does not include
   * areas clipped out by the CSS "overflow" and "clip" properties.
   *
   * HasOverflowRect() (below) will return PR_TRUE when this overflow rect
   * has been explicitly set, even if it matches mRect.
   * XXX Note: because of a space optimization using the formula above,
   * during reflow this function does not give accurate data if
   * FinishAndStoreOverflow has been called but mRect hasn't yet been
   * updated yet.
   *
   * @return the rect relative to this frame's origin, but after
   * CSS transforms have been applied (i.e. not really this frame's coordinate
   * system, and may not contain the frame's border-box, e.g. if there
   * is a CSS transform scaling it down)
   */
  nsRect GetOverflowRect() const;

  /**
   * Computes a rect that encompasses everything that might be painted by
   * this frame.  This includes this frame, all its descendent frames, this
   * frame's outline, and descentant frames' outline, but does not include
   * areas clipped out by the CSS "overflow" and "clip" properties.
   *
   * HasOverflowRect() (below) will return PR_TRUE when this overflow rect
   * is different from nsRect(0, 0, GetRect().width, GetRect().height).
   * XXX Note: because of a space optimization using the formula above,
   * during reflow this function does not give accurate data if
   * FinishAndStoreOverflow has been called but mRect hasn't yet been
   * updated yet.
   *
   * @return the rect relative to the parent frame, in the parent frame's
   * coordinate system
   */
  nsRect GetOverflowRectRelativeToParent() const;

  /**
   * Computes a rect that encompasses everything that might be painted by
   * this frame.  This includes this frame, all its descendent frames, this
   * frame's outline, and descentant frames' outline, but does not include
   * areas clipped out by the CSS "overflow" and "clip" properties.
   *
   * @return the rect relative to this frame, before any CSS transforms have
   * been applied, i.e. in this frame's coordinate system
   */
  nsRect GetOverflowRectRelativeToSelf() const;

  /**
   * Store the overflow area in the frame's mOverflow.mDeltas fields or
   * as a frame property in the frame manager so that it can be retrieved
   * later without reflowing the frame.
   */
  void FinishAndStoreOverflow(nsRect* aOverflowArea, nsSize aNewSize);

  void FinishAndStoreOverflow(nsHTMLReflowMetrics* aMetrics) {
    FinishAndStoreOverflow(&aMetrics->mOverflowArea, nsSize(aMetrics->width, aMetrics->height));
  }

  /**
   * Returns whether the frame has an overflow rect that is different from
   * its border-box.
   */
  PRBool HasOverflowRect() const {
    return mOverflow.mType != NS_FRAME_OVERFLOW_NONE;
  }

  /**
   * Removes any stored overflow rect from the frame.
   */
  void ClearOverflowRect();

  /**
   * Determine whether borders should not be painted on certain sides of the
   * frame.
   */
  virtual PRIntn GetSkipSides() const { return 0; }

  /** Selection related calls
   */
  /** 
   *  Called to set the selection status of the frame.
   *  
   *  This must be called on the primary frame, but all continuations
   *  will be affected the same way.
   *
   *  This sets or clears NS_FRAME_SELECTED_CONTENT for each frame in the
   *  continuation chain, if the frames are currently selectable.
   *  The frames are unconditionally invalidated, if this selection type
   *  is supported at all.
   *  @param aSelected is it selected?
   *  @param aType the selection type of the selection that you are setting on the frame
   */
  virtual void SetSelected(PRBool        aSelected,
                           SelectionType aType);

  NS_IMETHOD  GetSelected(PRBool *aSelected) const = 0;

  /**
   *  called to discover where this frame, or a parent frame has user-select style
   *  applied, which affects that way that it is selected.
   *    
   *  @param aIsSelectable out param. Set to true if the frame can be selected
   *                       (i.e. is not affected by user-select: none)
   *  @param aSelectStyle  out param. Returns the type of selection style found
   *                        (using values defined in nsStyleConsts.h).
   */
  NS_IMETHOD  IsSelectable(PRBool* aIsSelectable, PRUint8* aSelectStyle) const = 0;

  /** 
   *  Called to retrieve the SelectionController associated with the frame.
   *  @param aSelCon will contain the selection controller associated with
   *  the frame.
   */
  NS_IMETHOD  GetSelectionController(nsPresContext *aPresContext, nsISelectionController **aSelCon) = 0;

  /**
   *  Call to get nsFrameSelection for this frame.
   */
  already_AddRefed<nsFrameSelection> GetFrameSelection();

  /**
   * GetConstFrameSelection returns an object which methods are safe to use for
   * example in nsIFrame code.
   */
  const nsFrameSelection* GetConstFrameSelection();

  /** EndSelection related calls
   */

  /**
   *  called to find the previous/next character, word, or line  returns the actual 
   *  nsIFrame and the frame offset.  THIS DOES NOT CHANGE SELECTION STATE
   *  uses frame's begin selection state to start. if no selection on this frame will 
   *  return NS_ERROR_FAILURE
   *  @param aPOS is defined in nsFrameSelection
   */
  NS_IMETHOD PeekOffset(nsPeekOffsetStruct *aPos);

  /**
   *  called to find the previous/next selectable leaf frame.
   *  @param aDirection [in] the direction to move in (eDirPrevious or eDirNext)
   *  @param aVisual [in] whether bidi caret behavior is visual (PR_TRUE) or logical (PR_FALSE)
   *  @param aJumpLines [in] whether to allow jumping across line boundaries
   *  @param aScrollViewStop [in] whether to stop when reaching a scroll frame boundary
   *  @param aOutFrame [out] the previous/next selectable leaf frame
   *  @param aOutOffset [out] 0 indicates that we arrived at the beginning of the output frame;
   *                          -1 indicates that we arrived at its end.
   *  @param aOutJumpedLine [out] whether this frame and the returned frame are on different lines
   */
  nsresult GetFrameFromDirection(nsDirection aDirection, PRBool aVisual,
                                 PRBool aJumpLines, PRBool aScrollViewStop, 
                                 nsIFrame** aOutFrame, PRInt32* aOutOffset, PRBool* aOutJumpedLine);

  /**
   *  called to see if the children of the frame are visible from indexstart to index end.
   *  this does not change any state. returns PR_TRUE only if the indexes are valid and any of
   *  the children are visible.  for textframes this index is the character index.
   *  if aStart = aEnd result will be PR_FALSE
   *  @param aStart start index of first child from 0-N (number of children)
   *  @param aEnd   end index of last child from 0-N
   *  @param aRecurse should this frame talk to siblings to get to the contents other children?
   *  @param aFinished did this frame have the aEndIndex? or is there more work to do
   *  @param _retval  return value true or false. false = range is not rendered.
   */
  NS_IMETHOD CheckVisibility(nsPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aRecurse, PRBool *aFinished, PRBool *_retval)=0;

  /**
   * Called to tell a frame that one of its child frames is dirty (i.e.,
   * has the NS_FRAME_IS_DIRTY *or* NS_FRAME_HAS_DIRTY_CHILDREN bit
   * set).  This should always set the NS_FRAME_HAS_DIRTY_CHILDREN on
   * the frame, and may do other work.
   */
  virtual void ChildIsDirty(nsIFrame* aChild) = 0;

  /**
   * Called to retrieve this frame's accessible.
   * If this frame implements Accessibility return a valid accessible
   * If not return NS_ERROR_NOT_IMPLEMENTED.
   * Note: nsAccessible must be refcountable. Do not implement directly on your frame
   * Use a mediatior of some kind.
   */
#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible() = 0;
#endif

  /**
   * Get the frame whose style context should be the parent of this
   * frame's style context (i.e., provide the parent style context).
   * This frame must either be an ancestor of this frame or a child.  If
   * this frame returns a child frame, then the child frame must be sure
   * to return a grandparent or higher!
   *
   * @param aPresContext:   PresContext
   * @param aProviderFrame: The frame whose style context should be the
   *                        parent of this frame's style context.  Null
   *                        is permitted, and means that this frame's
   *                        style context should be the root of the
   *                        style context tree.
   * @param aIsChild:       True if |aProviderFrame| is set to a child
   *                        of this frame; false if it is an ancestor or
   *                        null.
   */
  NS_IMETHOD GetParentStyleContextFrame(nsPresContext* aPresContext,
                                        nsIFrame**      aProviderFrame,
                                        PRBool*         aIsChild) = 0;

  /**
   * Determines whether a frame is visible for painting;
   * taking into account whether it is painting a selection or printing.
   */
  PRBool IsVisibleForPainting(nsDisplayListBuilder* aBuilder);
  /**
   * Determines whether a frame is visible for painting or collapsed;
   * taking into account whether it is painting a selection or printing,
   */
  PRBool IsVisibleOrCollapsedForPainting(nsDisplayListBuilder* aBuilder);
  /**
   * As above, but slower because we have to recompute some stuff that
   * aBuilder already has.
   */
  PRBool IsVisibleForPainting();
  /**
   * Check whether this frame is visible in the current selection. Returns
   * PR_TRUE if there is no current selection.
   */
  PRBool IsVisibleInSelection(nsDisplayListBuilder* aBuilder);

  /**
   * Overridable function to determine whether this frame should be considered
   * "in" the given non-null aSelection for visibility purposes.
   */  
  virtual PRBool IsVisibleInSelection(nsISelection* aSelection);

  /**
   * Determines whether this frame is a pseudo stacking context, looking
   * only as style --- i.e., assuming that it's in-flow and not a replaced
   * element.
   */
  PRBool IsPseudoStackingContextFromStyle() {
    const nsStyleDisplay* disp = GetStyleDisplay();
    return disp->mOpacity != 1.0f || disp->IsPositioned() || disp->IsFloating();
  }
  
  virtual PRBool HonorPrintBackgroundSettings() { return PR_TRUE; }

  /**
   * Determine whether the frame is logically empty, which is roughly
   * whether the layout would be the same whether or not the frame is
   * present.  Placeholder frames should return true.  Block frames
   * should be considered empty whenever margins collapse through them,
   * even though those margins are relevant.  Text frames containing
   * only whitespace that does not contribute to the height of the line
   * should return true.
   */
  virtual PRBool IsEmpty() = 0;
  /**
   * Return the same as IsEmpty(). This may only be called after the frame
   * has been reflowed and before any further style or content changes.
   */
  virtual PRBool CachedIsEmpty();
  /**
   * Determine whether the frame is logically empty, assuming that all
   * its children are empty.
   */
  virtual PRBool IsSelfEmpty() = 0;

  /**
   * IsGeneratedContentFrame returns whether a frame corresponds to
   * generated content
   *
   * @return whether the frame correspods to generated content
   */
  PRBool IsGeneratedContentFrame() {
    return (mState & NS_FRAME_GENERATED_CONTENT) != 0;
  }

  /**
   * IsPseudoFrame returns whether a frame is a pseudo frame (eg an
   * anonymous table-row frame created for a CSS table-cell without an
   * enclosing table-row.
   *
   * @param aParentContent the content node corresponding to the parent frame
   * @return whether the frame is a pseudo frame
   */   
  PRBool IsPseudoFrame(nsIContent* aParentContent) {
    return mContent == aParentContent;
  }

  FrameProperties Properties() const {
    return FrameProperties(PresContext()->PropertyTable(), this);
  }

  NS_DECLARE_FRAME_PROPERTY(BaseLevelProperty, nsnull)
  NS_DECLARE_FRAME_PROPERTY(EmbeddingLevelProperty, nsnull)

#define NS_GET_BASE_LEVEL(frame) \
NS_PTR_TO_INT32(frame->Properties().Get(nsIFrame::BaseLevelProperty()))

#define NS_GET_EMBEDDING_LEVEL(frame) \
NS_PTR_TO_INT32(frame->Properties().Get(nsIFrame::EmbeddingLevelProperty()))

  /**
   * Return PR_TRUE if and only if this frame obeys visibility:hidden.
   * if it does not, then nsContainerFrame will hide its view even though
   * this means children can't be made visible again.
   */
  virtual PRBool SupportsVisibilityHidden() { return PR_TRUE; }

  /**
   * Returns PR_TRUE if the frame is absolutely positioned and has a clip
   * rect set via the 'clip' property. If true, then we also set aRect
   * to the computed clip rect coordinates relative to this frame's origin.
   * aRect must not be null!
   */
  PRBool GetAbsPosClipRect(const nsStyleDisplay* aDisp, nsRect* aRect,
                           const nsSize& aSize);

  /**
   * Check if this frame is focusable and in the current tab order.
   * Tabbable is indicated by a nonnegative tabindex & is a subset of focusable.
   * For example, only the selected radio button in a group is in the 
   * tab order, unless the radio group has no selection in which case
   * all of the visible, non-disabled radio buttons in the group are 
   * in the tab order. On the other hand, all of the visible, non-disabled 
   * radio buttons are always focusable via clicking or script.
   * Also, depending on the pref accessibility.tabfocus some widgets may be 
   * focusable but removed from the tab order. This is the default on
   * Mac OS X, where fewer items are focusable.
   * @param  [in, optional] aTabIndex the computed tab index
   *         < 0 if not tabbable
   *         == 0 if in normal tab order
   *         > 0 can be tabbed to in the order specified by this value
   * @param  [in, optional] aWithMouse, is this focus query for mouse clicking
   * @return whether the frame is focusable via mouse, kbd or script.
   */
  virtual PRBool IsFocusable(PRInt32 *aTabIndex = nsnull, PRBool aWithMouse = PR_FALSE);

  // BOX LAYOUT METHODS
  // These methods have been migrated from nsIBox and are in the process of
  // being refactored. DO NOT USE OUTSIDE OF XUL.
  PRBool IsBoxFrame() const
  {
    return IsFrameOfType(nsIFrame::eXULBox);
  }
  PRBool IsBoxWrapped() const
  { return (!IsBoxFrame() && mParent && mParent->IsBoxFrame()); }

  enum Halignment {
    hAlign_Left,
    hAlign_Right,
    hAlign_Center
  };

  enum Valignment {
    vAlign_Top,
    vAlign_Middle,
    vAlign_BaseLine,
    vAlign_Bottom
  };

  /**
   * This calculates the minimum size required for a box based on its state
   * @param[in] aBoxLayoutState The desired state to calculate for
   * @return The minimum size
   */
  virtual nsSize GetMinSize(nsBoxLayoutState& aBoxLayoutState) = 0;

  /**
   * This calculates the preferred size of a box based on its state
   * @param[in] aBoxLayoutState The desired state to calculate for
   * @return The preferred size
   */
  virtual nsSize GetPrefSize(nsBoxLayoutState& aBoxLayoutState) = 0;

  /**
   * This calculates the maximum size for a box based on its state
   * @param[in] aBoxLayoutState The desired state to calculate for
   * @return The maximum size
   */    
  virtual nsSize GetMaxSize(nsBoxLayoutState& aBoxLayoutState) = 0;

  /**
   * This returns the minimum size for the scroll area if this frame is
   * being scrolled. Usually it's (0,0).
   */
  virtual nsSize GetMinSizeForScrollArea(nsBoxLayoutState& aBoxLayoutState) = 0;

  // Implemented in nsBox, used in nsBoxFrame
  PRUint32 GetOrdinal(nsBoxLayoutState& aBoxLayoutState);

  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState) = 0;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) = 0;
  virtual PRBool IsCollapsed(nsBoxLayoutState& aBoxLayoutState) = 0;
  // This does not alter the overflow area. If the caller is changing
  // the box size, the caller is responsible for updating the overflow
  // area. It's enough to just call Layout or SyncLayout on the
  // box. You can pass PR_TRUE to aRemoveOverflowArea as a
  // convenience.
  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         PRBool aRemoveOverflowArea = PR_FALSE)=0;
  NS_HIDDEN_(nsresult) Layout(nsBoxLayoutState& aBoxLayoutState);
  nsIBox* GetChildBox() const
  {
    // box layout ends at box-wrapped frames, so don't allow these frames
    // to report child boxes.
    return IsBoxFrame() ? GetFirstChild(nsnull) : nsnull;
  }
  nsIBox* GetNextBox() const
  {
    return (mParent && mParent->IsBoxFrame()) ? mNextSibling : nsnull;
  }
  nsIBox* GetParentBox() const
  {
    return (mParent && mParent->IsBoxFrame()) ? mParent : nsnull;
  }
  // Box methods.  Note that these do NOT just get the CSS border, padding,
  // etc.  They also talk to nsITheme.
  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetBorder(nsMargin& aBorder)=0;
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding)=0;
  NS_IMETHOD GetMargin(nsMargin& aMargin)=0;
  NS_IMETHOD SetLayoutManager(nsIBoxLayout* aLayout)=0;
  NS_IMETHOD GetLayoutManager(nsIBoxLayout** aLayout)=0;
  NS_HIDDEN_(nsresult) GetClientRect(nsRect& aContentRect);

  // For nsSprocketLayout
  virtual Valignment GetVAlign() const = 0;
  virtual Halignment GetHAlign() const = 0;

  PRBool IsHorizontal() const { return (mState & NS_STATE_IS_HORIZONTAL) != 0; }
  PRBool IsNormalDirection() const { return (mState & NS_STATE_IS_DIRECTION_NORMAL) != 0; }

  NS_HIDDEN_(nsresult) Redraw(nsBoxLayoutState& aState, const nsRect* aRect = nsnull);
  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild)=0;
  virtual PRBool GetMouseThrough() const = 0;

#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug)=0;
  NS_IMETHOD GetDebug(PRBool& aDebug)=0;

  NS_IMETHOD DumpBox(FILE* out)=0;
#endif

  /**
   * @return PR_TRUE if this text frame ends with a newline character.  It
   * should return PR_FALSE if this is not a text frame.
   */
  virtual PRBool HasTerminalNewline() const;

  static PRBool AddCSSPrefSize(nsIBox* aBox, nsSize& aSize, PRBool& aWidth, PRBool& aHeightSet);
  static PRBool AddCSSMinSize(nsBoxLayoutState& aState, nsIBox* aBox,
                              nsSize& aSize, PRBool& aWidth, PRBool& aHeightSet);
  static PRBool AddCSSMaxSize(nsIBox* aBox, nsSize& aSize, PRBool& aWidth, PRBool& aHeightSet);
  static PRBool AddCSSFlex(nsBoxLayoutState& aState, nsIBox* aBox, nscoord& aFlex);

  // END OF BOX LAYOUT METHODS
  // The above methods have been migrated from nsIBox and are in the process of
  // being refactored. DO NOT USE OUTSIDE OF XUL.

  /**
   * gets the first or last possible caret position within the frame
   *
   * @param  [in] aStart
   *         true  for getting the first possible caret position
   *         false for getting the last possible caret position
   * @return The caret position in an nsPeekOffsetStruct (the
   *         fields set are mResultContent and mContentOffset;
   *         the returned value is a 'best effort' in case errors
   *         are encountered rummaging through the frame.
   */
  nsPeekOffsetStruct GetExtremeCaretPosition(PRBool aStart);

  /**
   * Same thing as nsFrame::CheckInvalidateSizeChange, but more flexible.  The
   * implementation of this method must not depend on the mRect or
   * GetOverflowRect() of the frame!  Note that it's safe to assume in this
   * method that the frame origin didn't change.  If it did, whoever moved the
   * frame will invalidate as needed anyway.
   */
  void CheckInvalidateSizeChange(const nsRect& aOldRect,
                                 const nsRect& aOldOverflowRect,
                                 const nsSize& aNewDesiredSize);

  /**
   * Get a line iterator for this frame, if supported.
   *
   * @return nsnull if no line iterator is supported.
   * @note dispose the line iterator using nsILineIterator::DisposeLineIterator
   */
  virtual nsILineIterator* GetLineIterator() = 0;

  /**
   * If this frame is a next-in-flow, and its prev-in-flow has something on its
   * overflow list, pull those frames into the child list of this one.
   */
  virtual void PullOverflowsFromPrevInFlow() {}

protected:
  // Members
  nsRect           mRect;
  nsIContent*      mContent;
  nsStyleContext*  mStyleContext;
  nsIFrame*        mParent;
private:
  nsIFrame*        mNextSibling;  // doubly-linked list of frames
  nsIFrame*        mPrevSibling;  // Do not touch outside SetNextSibling!
protected:
  nsFrameState     mState;

  // When there is an overflow area only slightly larger than mRect,
  // we store a set of four 1-byte deltas from the edges of mRect
  // rather than allocating a whole separate rectangle property.
  // Note that these are unsigned values, all measured "outwards"
  // from the edges of mRect, so /mLeft/ and /mTop/ are reversed from
  // our normal coordinate system.
  // If mOverflow.mType == NS_FRAME_OVERFLOW_LARGE, then the
  // delta values are not meaningful and the overflow area is stored
  // as a separate rect property.
  union {
    PRUint32  mType;
    struct {
      PRUint8 mLeft;
      PRUint8 mTop;
      PRUint8 mRight;
      PRUint8 mBottom;
    } mDeltas;
  } mOverflow;
  
  // Helpers
  /**
   * For frames that have top-level windows (top-level viewports,
   * comboboxes, menupoups) this function will invalidate the window.
   */
  void InvalidateRoot(const nsRect& aDamageRect, PRUint32 aFlags);

  /**
   * Can we stop inside this frame when we're skipping non-rendered whitespace?
   * @param  aForward [in] Are we moving forward (or backward) in content order.
   * @param  aOffset [in/out] At what offset into the frame to start looking.
   *         on output - what offset was reached (whether or not we found a place to stop).
   * @return PR_TRUE: An appropriate offset was found within this frame,
   *         and is given by aOffset.
   *         PR_FALSE: Not found within this frame, need to try the next frame.
   */
  virtual PRBool PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset) = 0;
  
  /**
   * Search the frame for the next character
   * @param  aForward [in] Are we moving forward (or backward) in content order.
   * @param  aOffset [in/out] At what offset into the frame to start looking.
   *         on output - what offset was reached (whether or not we found a place to stop).
   * @return PR_TRUE: An appropriate offset was found within this frame,
   *         and is given by aOffset.
   *         PR_FALSE: Not found within this frame, need to try the next frame.
   */
  virtual PRBool PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset) = 0;
  
  /**
   * Search the frame for the next word boundary
   * @param  aForward [in] Are we moving forward (or backward) in content order.
   * @param  aWordSelectEatSpace [in] PR_TRUE: look for non-whitespace following
   *         whitespace (in the direction of movement).
   *         PR_FALSE: look for whitespace following non-whitespace (in the
   *         direction  of movement).
   * @param  aIsKeyboardSelect [in] Was the action initiated by a keyboard operation?
   *         If PR_TRUE, punctuation immediately following a word is considered part
   *         of that word. Otherwise, a sequence of punctuation is always considered
   *         as a word on its own.
   * @param  aOffset [in/out] At what offset into the frame to start looking.
   *         on output - what offset was reached (whether or not we found a place to stop).
   * @param  aState [in/out] the state that is carried from frame to frame
   * @return PR_TRUE: An appropriate offset was found within this frame,
   *         and is given by aOffset.
   *         PR_FALSE: Not found within this frame, need to try the next frame.
   */
  struct PeekWordState {
    // true when we're still at the start of the search, i.e., we can't return
    // this point as a valid offset!
    PRPackedBool mAtStart;
    // true when we've encountered at least one character of the pre-boundary type
    // (whitespace if aWordSelectEatSpace is true, non-whitespace otherwise)
    PRPackedBool mSawBeforeType;
    // true when the last character encountered was punctuation
    PRPackedBool mLastCharWasPunctuation;
    // true when the last character encountered was whitespace
    PRPackedBool mLastCharWasWhitespace;
    // true when we've seen non-punctuation since the last whitespace
    PRPackedBool mSeenNonPunctuationSinceWhitespace;
    // text that's *before* the current frame when aForward is true, *after*
    // the current frame when aForward is false. Only includes the text
    // on the current line.
    nsAutoString mContext;

    PeekWordState() : mAtStart(PR_TRUE), mSawBeforeType(PR_FALSE),
        mLastCharWasPunctuation(PR_FALSE), mLastCharWasWhitespace(PR_FALSE),
        mSeenNonPunctuationSinceWhitespace(PR_FALSE) {}
    void SetSawBeforeType() { mSawBeforeType = PR_TRUE; }
    void Update(PRBool aAfterPunctuation, PRBool aAfterWhitespace) {
      mLastCharWasPunctuation = aAfterPunctuation;
      mLastCharWasWhitespace = aAfterWhitespace;
      if (aAfterWhitespace) {
        mSeenNonPunctuationSinceWhitespace = PR_FALSE;
      } else if (!aAfterPunctuation) {
        mSeenNonPunctuationSinceWhitespace = PR_TRUE;
      }
      mAtStart = PR_FALSE;
    }
  };
  virtual PRBool PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                                PRInt32* aOffset, PeekWordState* aState) = 0;

  /**
   * Search for the first paragraph boundary before or after the given position
   * @param  aPos See description in nsFrameSelection.h. The following fields are
   *              used by this method: 
   *              Input: mDirection
   *              Output: mResultContent, mContentOffset
   */
   nsresult PeekOffsetParagraph(nsPeekOffsetStruct *aPos);

private:
  nsRect* GetOverflowAreaProperty(PRBool aCreateIfNecessary = PR_FALSE);
  void SetOverflowRect(const nsRect& aRect);
  nsPoint GetOffsetToCrossDoc(const nsIFrame* aOther, const PRInt32 aAPD) const;

#ifdef NS_DEBUG
public:
  // Formerly nsIFrameDebug
  NS_IMETHOD  List(FILE* out, PRInt32 aIndent) const = 0;
  NS_IMETHOD  GetFrameName(nsAString& aResult) const = 0;
  NS_IMETHOD_(nsFrameState)  GetDebugStateBits() const = 0;
  NS_IMETHOD  DumpRegressionData(nsPresContext* aPresContext,
                                 FILE* out, PRInt32 aIndent) = 0;
#endif
};

//----------------------------------------------------------------------

/**
 * nsWeakFrame can be used to keep a reference to a nsIFrame in a safe way.
 * Whenever an nsIFrame object is deleted, the nsWeakFrames pointing
 * to it will be cleared.
 *
 * Create nsWeakFrame object when it is sure that nsIFrame object
 * is alive and after some operations which may destroy the nsIFrame
 * (for example any DOM modifications) use IsAlive() or GetFrame() methods to
 * check whether it is safe to continue to use the nsIFrame object.
 *
 * @note The usage of this class should be kept to a minimum.
 */

class nsWeakFrame {
public:
  nsWeakFrame() : mPrev(nsnull), mFrame(nsnull) { }

  nsWeakFrame(const nsWeakFrame& aOther) : mPrev(nsnull), mFrame(nsnull)
  {
    Init(aOther.GetFrame());
  }

  nsWeakFrame(nsIFrame* aFrame) : mPrev(nsnull), mFrame(nsnull)
  {
    Init(aFrame);
  }

  nsWeakFrame& operator=(nsWeakFrame& aOther) {
    Init(aOther.GetFrame());
    return *this;
  }

  nsWeakFrame& operator=(nsIFrame* aFrame) {
    Init(aFrame);
    return *this;
  }

  nsIFrame* operator->()
  {
    return mFrame;
  }

  operator nsIFrame*()
  {
    return mFrame;
  }

  void Clear(nsIPresShell* aShell) {
    if (aShell) {
      aShell->RemoveWeakFrame(this);
    }
    mFrame = nsnull;
    mPrev = nsnull;
  }

  PRBool IsAlive() { return !!mFrame; }

  nsIFrame* GetFrame() const { return mFrame; }

  nsWeakFrame* GetPreviousWeakFrame() { return mPrev; }

  void SetPreviousWeakFrame(nsWeakFrame* aPrev) { mPrev = aPrev; }

  ~nsWeakFrame()
  {
    Clear(mFrame ? mFrame->PresContext()->GetPresShell() : nsnull);
  }
private:
  void InitInternal(nsIFrame* aFrame);

  void InitExternal(nsIFrame* aFrame) {
    Clear(mFrame ? mFrame->PresContext()->GetPresShell() : nsnull);
    mFrame = aFrame;
    if (mFrame) {
      nsIPresShell* shell = mFrame->PresContext()->GetPresShell();
      NS_WARN_IF_FALSE(shell, "Null PresShell in nsWeakFrame!");
      if (shell) {
        shell->AddWeakFrame(this);
      } else {
        mFrame = nsnull;
      }
    }
  }

  void Init(nsIFrame* aFrame) {
#ifdef _IMPL_NS_LAYOUT
    InitInternal(aFrame);
#else
    InitExternal(aFrame);
#endif
  }

  nsWeakFrame*  mPrev;
  nsIFrame*     mFrame;
};

inline void
nsFrameList::Enumerator::Next()
{
  NS_ASSERTION(!AtEnd(), "Should have checked AtEnd()!");
  mFrame = mFrame->GetNextSibling();
}

inline
nsFrameList::FrameLinkEnumerator::
FrameLinkEnumerator(const nsFrameList& aList, nsIFrame* aPrevFrame)
  : Enumerator(aList)
{
  mPrev = aPrevFrame;
  mFrame = aPrevFrame ? aPrevFrame->GetNextSibling() : aList.FirstChild();
}
#endif /* nsIFrame_h___ */
