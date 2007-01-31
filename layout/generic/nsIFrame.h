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
#include "nsISupports.h"
#include "nsEvent.h"
#include "nsStyleStruct.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsHTMLReflowMetrics.h"

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
#ifdef ACCESSIBILITY
class nsIAccessible;
#endif
class nsDisplayListBuilder;
class nsDisplayListSet;
class nsDisplayList;

struct nsPeekOffsetStruct;
struct nsPoint;
struct nsRect;
struct nsSize;
struct nsMargin;

typedef class nsIFrame nsIBox;

// IID for the nsIFrame interface 
// 4ea7876f-1550-40d4-a764-739a0e4289a1
#define NS_IFRAME_IID \
{ 0x4ea7876f, 0x1550, 0x40d4, \
  { 0xa7, 0x64, 0x73, 0x9a, 0x0e, 0x42, 0x89, 0xa1 } }

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
typedef PRUint32 nsFrameState;

#define NS_FRAME_IN_REFLOW                            0x00000001
// This is only set during painting
#define NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO      0x00000001

// This bit is set when a frame is created. After it has been reflowed
// once (during the DidReflow with a finished state) the bit is
// cleared.
#define NS_FRAME_FIRST_REFLOW                         0x00000002

// For a continuation frame, if this bit is set, then this a "fluid" 
// continuation, i.e., across a line boundary. Otherwise it's a "hard"
// continuation, e.g. a bidi continuation.
#define NS_FRAME_IS_FLUID_CONTINUATION                0x00000004

// If this bit is set, then there is a child frame in the frame that
// extends outside this frame's bounding box. The implication is that
// the frame's rect does not completely cover its children and
// therefore operations like rendering and hit testing (for example)
// must operate differently.
#define NS_FRAME_OUTSIDE_CHILDREN                     0x00000008

// If this bit is set, then a reference to the frame is being held
// elsewhere.  The frame may want to send a notification when it is
// destroyed to allow these references to be cleared.
#define NS_FRAME_EXTERNAL_REFERENCE                   0x00000010

// If this bit is set, this frame or one of its descendants has a
// percentage height that depends on an ancestor of this frame.
// (Or it did at one point in the past, since we don't necessarily clear
// the bit when it's no longer needed; it's an optimization.)
#define NS_FRAME_CONTAINS_RELATIVE_HEIGHT             0x00000020

// If this bit is set, then the frame corresponds to generated content
#define NS_FRAME_GENERATED_CONTENT                    0x00000040

// If this bit is set, then the frame uses XUL flexible box layout
// for its children.
#define NS_FRAME_IS_BOX                               0x00000080

// If this bit is set, then the frame has been moved out of the flow,
// e.g., it is absolutely positioned or floated
#define NS_FRAME_OUT_OF_FLOW                          0x00000100

// If this bit is set, then the frame reflects content that may be selected
#define NS_FRAME_SELECTED_CONTENT                     0x00000200

// If this bit is set, then the frame is dirty and needs to be reflowed.
// This bit is set when the frame is first created.
// This bit is cleared by DidReflow after the required call to Reflow has
// finished.
#define NS_FRAME_IS_DIRTY                             0x00000400

// If this bit is set then the frame is unflowable.
#define NS_FRAME_IS_UNFLOWABLE                        0x00000800

// If this bit is set, either:
//  1. the frame has children that have either NS_FRAME_IS_DIRTY or
//     NS_FRAME_HAS_DIRTY_CHILDREN, or
//  2. the frame has had descendants removed.
// It means that Reflow needs to be called, but that Reflow will not
// do as much work as it would if NS_FRAME_IS_DIRTY were set.
// This bit is cleared by DidReflow after the required call to Reflow has
// finished.
#define NS_FRAME_HAS_DIRTY_CHILDREN                   0x00001000

// If this bit is set, the frame has an associated view
#define NS_FRAME_HAS_VIEW                             0x00002000

// If this bit is set, the frame was created from anonymous content.
#define NS_FRAME_INDEPENDENT_SELECTION                0x00004000

// If this bit is set, the frame is "special" (lame term, I know),
// which means that it is part of the mangled frame hierarchy that
// results when an inline has been split because of a nested block.
#define NS_FRAME_IS_SPECIAL                           0x00008000

// If this bit is set, the frame doesn't allow ignorable whitespace as
// children. For example, the whitespace between <table>\n<tr>\n<td>
// will be excluded during the construction of children. 
// The bit is set when the frame is first created and remain
// unchanged during the life-time of the frame.
#define NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE         0x00010000

#ifdef IBMBIDI
// If this bit is set, the frame itself is a bidi continuation,
// or is incomplete (its next sibling is a bidi continuation)
#define NS_FRAME_IS_BIDI                              0x00020000
#endif

// If this bit is set the frame has descendant with a view
#define NS_FRAME_HAS_CHILD_WITH_VIEW                  0x00040000

// If this bit is set, then reflow may be dispatched from the current
// frame instead of the root frame.
#define NS_FRAME_REFLOW_ROOT                          0x00080000

// The lower 20 bits of the frame state word are reserved by this API.
#define NS_FRAME_RESERVED                             0x000FFFFF

// The upper 12 bits of the frame state word are reserved for frame
// implementations.
#define NS_FRAME_IMPL_RESERVED                        0xFFF00000

// Box layout bits
#define NS_STATE_IS_HORIZONTAL                        0x00400000
#define NS_STATE_IS_DIRECTION_NORMAL                  0x80000000

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

// The frame (not counting a continuation) did not fit in the available height and 
// wasn't at the top of a page. If it was at the top of a page, then it is not 
// possible to reflow it again with more height, so we don't set it in that case.
#define NS_FRAME_TRUNCATED  0x0010
#define NS_FRAME_IS_TRUNCATED(status) \
  (0 != ((status) & NS_FRAME_TRUNCATED))
#define NS_FRAME_SET_TRUNCATION(status, aReflowState, aMetrics) \
  if (!aReflowState.mFlags.mIsTopOfPage &&                      \
      aReflowState.availableHeight < aMetrics.height)           \
    status |= NS_FRAME_TRUNCATED;                               \
  else                                                          \
    status &= ~NS_FRAME_TRUNCATED;

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
class nsIFrame : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFRAME_IID)

  nsPresContext* GetPresContext() const {
    return GetStyleContext()->GetRuleNode()->GetPresContext();
  }

  /**
   * Called to initialize the frame. This is called immediately after creating
   * the frame.
   *
   * If the frame is a continuing frame, then aPrevInFlow indicates the previous
   * frame (the frame that was split). You should connect the continuing frame to
   * its prev-in-flow, e.g. by using the AppendToFlow() function
   *
   * If you want a view associated with your frame, you should create the view
   * now.
   *
   * @param   aContent the content object associated with the frame
   * @param   aGeometricParent  the geometric parent frame
   * @param   aContentParent  the content parent frame
   * @param   aContext the style context associated with the frame
   * @param   aPrevInFlow the prev-in-flow frame
   * @see #AppendToFlow()
   */
  NS_IMETHOD  Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        aPrevInFlow) = 0;

  /**
   * Destroys this frame and each of its child frames (recursively calls
   * Destroy() for each child)
   */
  virtual void Destroy() = 0;

  /*
   * Notify the frame that it has been removed as the primary frame for its content
   */
  virtual void RemovedAsPrimaryFrame() {}

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
   *            NS_FRAME_IS_DIRTY bit set
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame or if the
   *            initial list of frames has already been set for that child list,
   *          NS_OK otherwise
   * @see     #Init()
   */
  NS_IMETHOD  SetInitialChildList(nsIAtom*        aListName,
                                  nsIFrame*       aChildList) = 0;

  /**
   * This method is responsible for appending frames to the frame
   * list.  The implementation should append the frames to the specified
   * child list and then generate a reflow command.
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @param   aFrameList list of child frames to append. Each of the frames has
   *            its NS_FRAME_IS_DIRTY bit set
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise
   */
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsIFrame*       aFrameList) = 0;

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
   *          NS_OK otherwise
   */
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList) = 0;

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
   */
  virtual nsIFrame* GetContentInsertionFrame() { return this; }

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
      if (mStyleContext)
        mStyleContext->Release();
      mStyleContext = aContext;
      if (aContext) {
        aContext->AddRef();
        DidSetStyleContext();
      }
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
  NS_IMETHOD DidSetStyleContext() = 0;

  /**
   * Get the style data associated with this frame.  This returns a
   * const style struct pointer that should never be modified.  See
   * |nsIStyleContext::GetStyleData| for more information.
   *
   * The use of the typesafe functions below is preferred to direct use
   * of this function.
   */
  virtual const nsStyleStruct* GetStyleDataExternal(nsStyleStructID aSID) const = 0;

  const nsStyleStruct* GetStyleData(nsStyleStructID aSID) const {
#ifdef _IMPL_NS_LAYOUT
    NS_ASSERTION(mStyleContext, "No style context found!");
    return mStyleContext->GetStyleData(aSID);
#else
    return GetStyleDataExternal(aSID);
#endif
  }

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
      return NS_STATIC_CAST(const nsStyle##name_*,                            \
                            GetStyleDataExternal(eStyleStruct_##name_));      \
    }
#endif
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

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
   * Accessor functions for geometric parent
   */
  nsIFrame* GetParent() const { return mParent; }
  NS_IMETHOD SetParent(const nsIFrame* aParent) { mParent = (nsIFrame*)aParent; return NS_OK; }

  /**
   * Bounding rect of the frame. The values are in twips, and the origin is
   * relative to the upper-left of the geometric parent. The size includes the
   * content area, borders, and padding.
   *
   * Note: moving or sizing the frame does not affect the view's size or
   * position.
   */
  nsRect GetRect() const { return mRect; }
  nsPoint GetPosition() const { return nsPoint(mRect.x, mRect.y); }
  nsSize GetSize() const { return nsSize(mRect.width, mRect.height); }
  void SetRect(const nsRect& aRect) { mRect = aRect; }
  void SetPosition(const nsPoint& aPt) { mRect.MoveTo(aPt); }
  void SetSize(const nsSize& aSize) { mRect.SizeTo(aSize); }

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild)
  { return aChild->GetPosition(); }
  
  nsPoint GetPositionIgnoringScrolling() {
    return mParent ? mParent->GetPositionOfChildIgnoringScrolling(this)
      : GetPosition();
  }

  /**
   * Return the distance between the border edge of the frame and the
   * margin edge of the frame.  Can only be called after Reflow for the
   * frame has at least *started*.
   *
   * This doesn't include any margin collapsing that may have occurred.
   */
  virtual nsMargin GetUsedMargin() const;

  /**
   * Return the distance between the border edge of the frame (which is
   * its rect) and the padding edge of the frame.  Can only be called
   * after Reflow for the frame has at least *started*.
   *
   * Note that this differs from GetStyleBorder()->GetBorder() in that
   * this describes region of the frame's box, and
   * GetStyleBorder()->GetBorder() describes a border.  They differ only
   * for tables, particularly border-collapse tables.
   */
  virtual nsMargin GetUsedBorder() const;

  /**
   * Return the distance between the padding edge of the frame and the
   * content edge of the frame.  Can only be called after Reflow for the
   * frame has at least *started*.
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
   * other rectangles of the frame, in twips, relative to the parent.
   */
  nsRect GetMarginRect() const;
  nsRect GetPaddingRect() const;
  nsRect GetContentRect() const;

  /**
   * Get the position of the frame's baseline, relative to the top of
   * the frame (its top border edge).  Only valid when Reflow is not
   * needed and when the frame returned nsHTMLReflowMetrics::
   * ASK_FOR_ASCENT as ascent in its reflow metrics.
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
   * Get the first child frame from the specified child list.
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @return  the child frame, or NULL if there is no such child
   * @see     #GetAdditionalListName()
   */
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const = 0;

  /**
   * Child frames are linked together in a singly-linked list
   */
  nsIFrame* GetNextSibling() const { return mNextSibling; }
  void SetNextSibling(nsIFrame* aNextSibling) {
    NS_ASSERTION(this != aNextSibling, "Creating a circular frame list, this is very bad."); 
    mNextSibling = aNextSibling;
  }

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
                        const nsDisplayListSet&     aLists);

  PRBool IsThemed() {
    return IsThemed(GetStyleDisplay());
  }
  PRBool IsThemed(const nsStyleDisplay* aDisp) {
    if (!aDisp->mAppearance)
      return PR_FALSE;
    nsPresContext* pc = GetPresContext();
    nsITheme *theme = pc->GetTheme();
    return theme && theme->ThemeSupportsWidget(pc, this, aDisp->mAppearance);
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
   */
  nsresult OverflowClip(nsDisplayListBuilder*   aBuilder,
                        const nsDisplayListSet& aFromSet,
                        const nsDisplayListSet& aToSet,
                        const nsRect&           aClipRect,
                        PRBool                  aClipBorderBackground = PR_FALSE,
                        PRBool                  aClipAll = PR_FALSE);

  /**
   * Clips the display items of aFromSet, putting the results in aToSet.
   * All items are clipped.
   */
  nsresult Clip(nsDisplayListBuilder* aBuilder,
                const nsDisplayListSet& aFromSet,
                const nsDisplayListSet& aToSet,
                const nsRect& aClipRect);

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
   * Does this frame type always need a view?
   */
  virtual PRBool NeedsView() { return PR_FALSE; }

  /**
   * This frame needs a view with a widget (e.g. because it's fixed
   * positioned), so we call this to create the widget. If widgets for
   * this frame type need to be of a certain type or require special
   * initialization, that can be done here.
   */
  virtual nsresult CreateWidgetForView(nsIView* aView);

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
  struct ContentOffsets {
    nsCOMPtr<nsIContent> content;
    PRBool IsNull() { return !content; }
    PRInt32 offset;
    PRInt32 secondaryOffset;
    // Helpers for places that need the ends of the offsets and expect them in
    // numerical order, as opposed to wanting the primary and secondary offsets
    PRInt32 StartOffset() { return PR_MIN(offset, secondaryOffset); }
    PRInt32 EndOffset() { return PR_MAX(offset, secondaryOffset); }
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
  struct Cursor {
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
  NS_IMETHOD  GetPointFromOffset(nsPresContext*          inPresContext,
                                 nsIRenderingContext*     inRendContext,
                                 PRInt32                  inOffset,
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
   * This call is invoked when content is changed in the content tree.
   * The first frame that maps that content is asked to deal with the
   * change by generating an incremental reflow command.
   *
   * @param aPresContext the presentation context
   * @param aContent     the content node that was changed
   * @param aAppend      a hint to the frame about the change
   */
  NS_IMETHOD  CharacterDataChanged(nsPresContext* aPresContext,
                                   nsIContent*     aChild,
                                   PRBool          aAppend) = 0;

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
    return NS_CONST_CAST(nsIFrame*, this);
  }
  virtual nsIFrame* GetLastContinuation() const {
    return NS_CONST_CAST(nsIFrame*, this);
  }
  
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
    return NS_CONST_CAST(nsIFrame*, this);
  }

  /**
   * Return the last frame in our current flow.
   */
  virtual nsIFrame* GetLastInFlow() const {
    return NS_CONST_CAST(nsIFrame*, this);
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
      : prevLines(0)
      , currentLine(0)
      , skipWhitespace(PR_TRUE)
      , trailingWhitespace(0)
    {}

    // The maximum intrinsic width for all previous lines.
    nscoord prevLines;

    // The maximum intrinsic width for the current line.  At a line
    // break (mandatory for preferred width; allowed for minimum width),
    // the caller should call |Break()|.
    nscoord currentLine;

    // True if initial collapsable whitespace should be skipped.  This
    // should be true at the beginning of a block and when the last text
    // ended with whitespace.
    PRBool skipWhitespace;

    // This contains the width of the trimmable whitespace at the end of
    // |currentLine|; it is zero if there is no such whitespace.
    nscoord trailingWhitespace;

    // Floats encountered in the lines.
    nsVoidArray floats; // of nsIFrame*
  };

  struct InlineMinWidthData : public InlineIntrinsicWidthData {
    InlineMinWidthData()
      : trailingTextFrame(nsnull)
    {}

    void Break(nsIRenderingContext *aRenderingContext);

    // The last text frame processed so far in the current line, when
    // the last characters in that text frame are relevant for line
    // break opportunities.
    nsIFrame *trailingTextFrame;
  };

  struct InlinePrefWidthData : public InlineIntrinsicWidthData {
    void Break(nsIRenderingContext *aRenderingContext);
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

  // Justification helper method that is used to remove trailing
  // whitespace before justification.
  NS_IMETHOD TrimTrailingWhiteSpace(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth,
                                    PRBool& aLastCharIsJustifiable) = 0;

  /**
   * Accessor functions to get/set the associated view object
   *
   * GetView returns non-null if and only if |HasView| returns true.
   */
  PRBool HasView() const { return mState & NS_FRAME_HAS_VIEW; }
  nsIView* GetView() const;
  virtual nsIView* GetViewExternal() const;
  nsresult SetView(nsIView* aView);

  /**
   * This view will be used to parent the views of any children.
   * This allows us to insert an anonymous inner view to parent
   * some children.
   */
  virtual nsIView* GetParentViewForChildFrame(nsIFrame* aFrame) const;

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
   * NOTE: this actually returns the offset from aOther to |this|, but
   * that offset is added to transform _coordinates_ from |this| to
   * aOther.
   */
  nsPoint GetOffsetTo(const nsIFrame* aOther) const;
  virtual nsPoint GetOffsetToExternal(const nsIFrame* aOther) const;

  /**
   * Get the screen rect of the frame.
   * @return the pixel rect of the frame in screen coordinates.
   */
  nsIntRect GetScreenRect() const;
  virtual nsIntRect GetScreenRectExternal() const;

  /**
   * Returns the offset from this frame to the closest geometric parent that
   * has a view. Also returns the containing view or null in case of error
   */
  NS_IMETHOD  GetOffsetFromView(nsPoint&  aOffset,
                                nsIView** aView) const = 0;

  /**
   * Returns the offset from this frame's upper left corner to the upper
   * left corner of the view returned by a call to GetView(). aOffset
   * will contain the offset to the view or (0,0) if the frame has no
   * view. aView will contain a pointer to the view returned by GetView().
   * aView is optional, that is, you may pass null if you are not interested
   * in getting a pointer to the view.
   */
  NS_IMETHOD  GetOriginToViewOffset(nsPoint&        aOffset,
                                    nsIView**       aView) const = 0;

  /**
   * Returns true if and only if all views, from |GetClosestView| up to
   * the top of the view hierarchy are visible.
   */
  virtual PRBool AreAncestorViewsVisible() const;

  /**
   * Returns the window that contains this frame. If this frame has a
   * view and the view has a window, then this frames window is
   * returned, otherwise this frame's geometric parent is checked
   * recursively upwards.
   * XXX virtual because gfx callers use it! (themes)
   */
  virtual nsIWidget* GetWindow() const;

  /**
   * Get the "type" of the frame. May return a NULL atom pointer
   *
   * @see nsGkAtoms
   */
  virtual nsIAtom* GetType() const = 0;
  

  /**
   * Bit-flags to pass to IsFrameOfType()
   */
  enum {
    eMathML =                           1 << 0,
    eSVG =                              1 << 1,
    eSVGForeignObject =                 1 << 2,
    eBidiInlineContainer =              1 << 3,
    // the frame is for a replaced element, such as an image
    eReplaced =                         1 << 4,
    // Frame that contains a block but looks like a replaced element
    // from the outside
    eReplacedContainsBlock =            1 << 5
  };

  /**
   * API for doing a quick check if a frame is of a given
   * type. Returns true if the frame matches ALL flags passed in.
   */
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return !aFlags;
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
   * Does this frame want to capture the mouse when the user clicks in
   * it or its children? If so, return the view which should be
   * targeted for mouse capture. The view need not be this frame's view,
   * it could be the view on a child.
   */
  virtual nsIView* GetMouseCapturer() const { return nsnull; }

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
  void Invalidate(const nsRect& aDamageRect, PRBool aImmediate = PR_FALSE);

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
   */  
  virtual void InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aOffsetX, nscoord aOffsetY,
                                  nsIFrame* aForChild, PRBool aImmediate);

  /**
   * Computes a rect that includes this frame, all its descendant
   * frames, this frame's outline (if any), and all descendant frames'
   * outlines (if any). This is the union of everything that might be painted by
   * this frame subtree.
   *
   * @return the rect relative to this frame's origin
   */
  nsRect GetOverflowRect() const;

  /**
   * Set/unset the NS_FRAME_OUTSIDE_CHILDREN flag and store the overflow area
   * as a frame property in the frame manager so that it can be retrieved
   * later without reflowing the frame.
   */
  void FinishAndStoreOverflow(nsRect* aOverflowArea, nsSize aNewSize);

  void FinishAndStoreOverflow(nsHTMLReflowMetrics* aMetrics) {
    FinishAndStoreOverflow(&aMetrics->mOverflowArea, nsSize(aMetrics->width, aMetrics->height));
  }

  /**
   * Determine whether borders should not be painted on certain sides of the
   * frame.
   */
  virtual PRIntn GetSkipSides() const { return 0; }

  /** Selection related calls
   */
  /** 
   *  Called to set the selection of the frame based on frame offsets.  you can FORCE the frame
   *  to redraw event if aSelected == the frame selection with the last parameter.
   *  data in struct may be changed when passed in.
   *  @param aRange is the range that will dictate if the frames need to be redrawn null means the whole content needs to be redrawn
   *  @param aSelected is it selected?
   *  @param aSpread should it spread the selection to flow elements around it? or go down to its children?
   */
  NS_IMETHOD  SetSelected(nsPresContext* aPresContext,
                          nsIDOMRange*    aRange,
                          PRBool          aSelected,
                          nsSpread        aSpread) = 0;

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
   *  Call to get nsFrameSelection for this frame; does not addref
   */
  nsFrameSelection* GetFrameSelection();

  /** EndSelection related calls
   */

  /**
   *  Call to turn on/off mouseCapture at the view level. Needed by the ESM so
   *  it must be in the public interface.
   *  @param aPresContext presContext associated with the frame
   *  @param aGrabMouseEvents PR_TRUE to enable capture, PR_FALSE to disable
   */
  NS_IMETHOD CaptureMouse(nsPresContext* aPresContext, PRBool aGrabMouseEvents) = 0;

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
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible) = 0;
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
    return disp->mOpacity != 1.0f || disp->IsPositioned();
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


  NS_HIDDEN_(void*) GetProperty(nsIAtom* aPropertyName,
                                nsresult* aStatus = nsnull) const;
  virtual NS_HIDDEN_(void*) GetPropertyExternal(nsIAtom*  aPropertyName,
                                                nsresult* aStatus) const;
  NS_HIDDEN_(nsresult) SetProperty(nsIAtom*           aPropertyName,
                                   void*              aValue,
                                   NSPropertyDtorFunc aDestructor = nsnull,
                                   void*              aDtorData = nsnull);
  NS_HIDDEN_(nsresult) DeleteProperty(nsIAtom* aPropertyName) const;
  NS_HIDDEN_(void*) UnsetProperty(nsIAtom* aPropertyName,
                                  nsresult* aStatus = nsnull) const;

#define NS_GET_BASE_LEVEL(frame) \
NS_PTR_TO_INT32(frame->GetProperty(nsGkAtoms::baseLevel))

#define NS_GET_EMBEDDING_LEVEL(frame) \
NS_PTR_TO_INT32(frame->GetProperty(nsGkAtoms::embeddingLevel))

  /** Create or retrieve the previously stored overflow area, if the frame does 
    * not overflow and no creation is required return nsnull.
    * @param aPresContext PresContext
    * @param aCreateIfNecessary  create a new nsRect for the overflow area
    * @return pointer to the overflow area rectangle 
    */
  nsRect* GetOverflowAreaProperty(PRBool aCreateIfNecessary = PR_FALSE);

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
  PRBool GetAbsPosClipRect(const nsStyleDisplay* aDisp, nsRect* aRect);

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
  PRBool IsBoxFrame() const { return (mState & NS_FRAME_IS_BOX) != 0; }
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
  NS_IMETHOD SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                       PRBool aRemoveOverflowArea = PR_FALSE)=0;
  NS_HIDDEN_(nsresult) Layout(nsBoxLayoutState& aBoxLayoutState);
  nsresult GetChildBox(nsIBox** aBox)
  {
    // box layout ends at box-wrapped frames, so don't allow these frames
    // to report child boxes.
    *aBox = IsBoxFrame() ? GetFirstChild(nsnull) : nsnull;
    return NS_OK;
  }
  nsresult GetNextBox(nsIBox** aBox)
  {
    *aBox = (mParent && mParent->IsBoxFrame()) ? mNextSibling : nsnull;
    return NS_OK;
  }
  NS_HIDDEN_(nsresult) GetParentBox(nsIBox** aParent);
  // Box methods.  Note that these do NOT just get the CSS border, padding,
  // etc.  They also talk to nsITheme.
  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetBorder(nsMargin& aBorder)=0;
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding)=0;
#ifdef DEBUG_LAYOUT
  NS_IMETHOD GetInset(nsMargin& aInset)=0;
#else
  nsresult GetInset(nsMargin& aInset) { aInset.SizeTo(0, 0, 0, 0); return NS_OK; }
#endif
  NS_IMETHOD GetMargin(nsMargin& aMargin)=0;
  NS_IMETHOD SetLayoutManager(nsIBoxLayout* aLayout)=0;
  NS_IMETHOD GetLayoutManager(nsIBoxLayout** aLayout)=0;
  NS_HIDDEN_(nsresult) GetClientRect(nsRect& aContentRect);
  NS_IMETHOD GetVAlign(Valignment& aAlign) = 0;
  NS_IMETHOD GetHAlign(Halignment& aAlign) = 0;

  PRBool IsHorizontal() const { return (mState & NS_STATE_IS_HORIZONTAL) != 0; }
  PRBool IsNormalDirection() const { return (mState & NS_STATE_IS_DIRECTION_NORMAL) != 0; }

  NS_HIDDEN_(nsresult) Redraw(nsBoxLayoutState& aState, const nsRect* aRect = nsnull, PRBool aImmediate = PR_FALSE);
  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIBox* aChild)=0;
  virtual PRBool GetMouseThrough() const = 0;

#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, PRBool aDebug)=0;
  NS_IMETHOD GetDebug(PRBool& aDebug)=0;

  NS_IMETHOD DumpBox(FILE* out)=0;
#endif

  // Only nsDeckFrame requires that all its children have widgets
  virtual PRBool ChildrenMustHaveWidgets() const { return PR_FALSE; }

  /**
   * @return PR_TRUE if this text frame ends with a newline character.  It
   * should return PR_FALSE if this is not a text frame.
   */
  virtual PRBool HasTerminalNewline() const;

  static PRBool AddCSSPrefSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize);
  static PRBool AddCSSMinSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize);
  static PRBool AddCSSMaxSize(nsBoxLayoutState& aState, nsIBox* aBox, nsSize& aSize);
  static PRBool AddCSSFlex(nsBoxLayoutState& aState, nsIBox* aBox, nscoord& aFlex);
  static PRBool AddCSSCollapsed(nsBoxLayoutState& aState, nsIBox* aBox, PRBool& aCollapsed);
  static PRBool AddCSSOrdinal(nsBoxLayoutState& aState, nsIBox* aBox, PRUint32& aOrdinal);

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

protected:
  // Members
  nsRect           mRect;
  nsIContent*      mContent;
  nsStyleContext*  mStyleContext;
  nsIFrame*        mParent;
  nsIFrame*        mNextSibling;  // singly-linked list of frames
  nsFrameState     mState;
  
  // Helpers
  /**
   * For frames that have top-level windows (top-level viewports,
   * comboboxes, menupoups) this function will invalidate the window.
   */
  void InvalidateRoot(const nsRect& aDamageRect,
                      nscoord aOffsetX, nscoord aOffsetY,
                      PRBool aImmediate);

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
   * @param  aSawBeforeType [in/out] Did we encounter a character of the pre-boundary type
   *         (whitespace if aWordSelectEatSpace is true, non-whitespace otherwise).
   * @return PR_TRUE: An appropriate offset was found within this frame,
   *         and is given by aOffset.
   *         PR_FALSE: Not found within this frame, need to try the next frame.
   */
  virtual PRBool PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                                PRInt32* aOffset, PRBool* aSawBeforeType) = 0;

  /**
   * Search for the first paragraph boundary before or after the given position
   * @param  aPos See description in nsFrameSelection.h. The following fields are
   *              used by this method: 
   *              Input: mDirection
   *              Output: mResultContent, mContentOffset
   */
   nsresult PeekOffsetParagraph(nsPeekOffsetStruct *aPos);

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
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

  nsIFrame* GetFrame() { return mFrame; }

  nsWeakFrame* GetPreviousWeakFrame() { return mPrev; }

  void SetPreviousWeakFrame(nsWeakFrame* aPrev) { mPrev = aPrev; }

  ~nsWeakFrame()
  {
    Clear(mFrame ? mFrame->GetPresContext()->GetPresShell() : nsnull);
  }
private:
  void Init(nsIFrame* aFrame);

  nsWeakFrame*  mPrev;
  nsIFrame*     mFrame;
};


NS_DEFINE_STATIC_IID_ACCESSOR(nsIFrame, NS_IFRAME_IID)

#endif /* nsIFrame_h___ */
