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
struct nsHTMLReflowMetrics;

class nsIAtom;
class nsIPresContext;
class nsIPresShell;
class nsIRenderingContext;
class nsIView;
class nsIWidget;
class nsIDOMRange;
class nsISelectionController;
#ifdef ACCESSIBILITY
class nsIAccessible;
#endif

struct nsPeekOffsetStruct;
struct nsPoint;
struct nsRect;
struct nsSize;
struct nsMargin;

// Calback function used to destroy the value associated with a property.
typedef void 
(*NSFramePropertyDtorFunc)(nsIPresContext* aPresContext,
                           nsIFrame*       aFrame,
                           nsIAtom*        aPropertyName,
                           void*           aPropertyValue);

// IID for the nsIFrame interface 
// a6cf9050-15b3-11d2-932e-00805f8add32
#define NS_IFRAME_IID \
 { 0xa6cf9050, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

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

#define NS_FRAME_IN_REFLOW                            0x00000001

// This bit is set when a frame is created. After it has been reflowed
// once (during the DidReflow with a finished state) the bit is
// cleared.
#define NS_FRAME_FIRST_REFLOW                         0x00000002

// If this bit is is set, then the view position and size should be
// kept in sync with the frame position and size. If the bit is not
// set then it's the responsibility of the frame itself (or whoever
// created the view) to position and size its associated view
#define NS_FRAME_SYNC_FRAME_AND_VIEW                  0x00000004

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

// If this bit is set, then the frame is a replaced element. For example,
// a frame displaying an image
#define NS_FRAME_REPLACED_ELEMENT                     0x00000020

// If this bit is set, then the frame corresponds to generated content
#define NS_FRAME_GENERATED_CONTENT                    0x00000040

// If this bit is set, then the frame has been moved out of the flow,
// e.g., it is absolutely positioned or floated
#define NS_FRAME_OUT_OF_FLOW                          0x00000100

// If this bit is set, then the frame reflects content that may be selected
#define NS_FRAME_SELECTED_CONTENT                     0x00000200

// If this bit is set, then the frame is dirty and needs to be reflowed.
// This bit is set when the frame is first created
#define NS_FRAME_IS_DIRTY                             0x00000400

// If this bit is set then the frame is unflowable.
#define NS_FRAME_IS_UNFLOWABLE                        0x00000800

// If this bit is set, the frame has dirty children.
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

//----------------------------------------------------------------------

enum nsFramePaintLayer {
  eFramePaintLayer_Underlay = 0,
  eFramePaintLayer_Content = 1,
  eFramePaintLayer_Overlay = 2
};

enum nsSelectionAmount {
  eSelectCharacter = 0,
  eSelectWord      = 1,
  eSelectLine      = 2,  //previous drawn line in flow.
  eSelectBeginLine = 3,
  eSelectEndLine   = 4,
  eSelectNoAmount  = 5,   //just bounce back current offset.
  eSelectDir       = 6,   //select next/previous frame based on direction
  eSelectParagraph = 7    //select a "paragraph"
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

// For HTML reflow we rename with the different paint layers are
// actually used for.
#define NS_FRAME_PAINT_LAYER_BACKGROUND eFramePaintLayer_Underlay
#define NS_FRAME_PAINT_LAYER_FLOATS   eFramePaintLayer_Content
#define NS_FRAME_PAINT_LAYER_FOREGROUND eFramePaintLayer_Overlay
#define NS_FRAME_PAINT_LAYER_DEBUG      eFramePaintLayer_Overlay

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
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFRAME_IID)

  nsIPresContext* GetPresContext() const {
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
  NS_IMETHOD  Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsStyleContext*  aContext,
                   nsIFrame*        aPrevInFlow) = 0;

  /**
   * Destroys this frame and each of its child frames (recursively calls
   * Destroy() for each child)
   */
  NS_IMETHOD  Destroy(nsIPresContext* aPresContext) = 0;

  /*
   * Notify the frame that it has been removed as the primary frame for its content
   */
  virtual void RemovedAsPrimaryFrame(nsIPresContext* aPresContext) {}

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
  NS_IMETHOD  SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
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
  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
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
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
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
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame) = 0;

  /**
   * This method is responsible for replacing the old frame with the
   * new frame. The old frame should be destroyed and the new frame inserted
   * in its place in the specified child list.
   *
   * @param   aListName the name of the child list. A NULL pointer for the atom
   *            name means the unnamed principal child list
   * @param   aOldFrame the frame to remove
   * @param   aNewFrame the frame to replace it with. The new frame has its
   *            NS_FRAME_IS_DIRTY bit set
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_FAILURE if the old child frame is not in the specified
   *            child list,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise
   */
  NS_IMETHOD ReplaceFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame,
                          nsIFrame*       aNewFrame) = 0;

  /**
   * Get the content object associated with this frame. Does not add a reference.
   */
  nsIContent* GetContent() const { return mContent; }

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
  void SetStyleContext(nsIPresContext* aPresContext, nsStyleContext* aContext)
  { 
    if (aContext != mStyleContext) {
      if (mStyleContext)
        mStyleContext->Release();
      mStyleContext = aContext;
      if (aContext) {
        aContext->AddRef();
        DidSetStyleContext(aPresContext);
      }
    }
  }

  // Style post processing hook
  NS_IMETHOD DidSetStyleContext(nsIPresContext* aPresContext) = 0;

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

  #define STYLE_STRUCT(name_, checkdata_cb_, ctor_args_)                      \
    const nsStyle##name_ * GetStyle##name_() const {                          \
      return NS_STATIC_CAST(const nsStyle##name_*,                            \
                            GetStyleData(eStyleStruct_##name_));              \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  // Utility function: more convenient than 2 calls to GetStyleData to get border and padding
  
  NS_IMETHOD  CalcBorderPadding(nsMargin& aBorderPadding) const = 0;

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
  void SetNextSibling(nsIFrame* aNextSibling) { mNextSibling = aNextSibling; }

  /**
   * Paint is responsible for painting the frame. The aWhichLayer
   * argument indicates which layer of painting should be done during
   * the call.
   */
  NS_IMETHOD  Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags = 0) = 0;

  /* 
   * Does the frame paint its background? If not, then all or part of it will be
   * painted by ancestors.
   */
  virtual PRBool CanPaintBackground() { return PR_TRUE; }

  /**
   * Event handling of GUI events.
   *
   * @param   aEvent event structure describing the type of event and rge widget
   *            where the event originated
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
  NS_IMETHOD  HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent*     aEvent,
                          nsEventStatus*  aEventStatus) = 0;

  NS_IMETHOD  GetContentForEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIContent** aContent) = 0;

  NS_IMETHOD GetContentAndOffsetsFromPoint(nsIPresContext* aCX,
                                           const nsPoint&  aPoint,
                                           nsIContent **   aNewContent,
                                           PRInt32&        aContentOffset,
                                           PRInt32&        aContentOffsetEnd,
                                           PRBool&         aBeginFrameContent) = 0;


  /**
   * Get the cursor for a given frame.
   */
  NS_IMETHOD  GetCursor(nsIPresContext* aPresContext,
                        nsPoint&        aPoint,
                        PRInt32&        aCursor) = 0;

  /**
   * Get the frame that should receive events for a given point in the
   * coordinate space of this frame's parent, if the frame is painted in
   * the given paint layer.  A frame should return itself if it should
   * recieve the events.  A successful return value indicates that a
   * point was found.
   */
  NS_IMETHOD  GetFrameForPoint(nsIPresContext* aPresContext,
                               const nsPoint& aPoint, 
                               nsFramePaintLayer aWhichLayer,
                               nsIFrame**     aFrame) = 0;
  
  
  /**
   * Get a point (in the frame's coordinate space) given an offset into
   * the content. This point should be on the baseline of text with
   * the correct horizontal offset
   */
  NS_IMETHOD  GetPointFromOffset(nsIPresContext*          inPresContext,
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
  NS_IMETHOD  CharacterDataChanged(nsIPresContext* aPresContext,
                                   nsIContent*     aChild,
                                   PRBool          aAppend) = 0;

  /**
   * This call is invoked when the value of a content objects's attribute
   * is changed. 
   * The first frame that maps that content is asked to deal
   * with the change by doing whatever is appropriate.
   *
   * @param aChild the content object
   * @param aAttribute the attribute whose value changed
   * @param aHint the level of change that has already been dealt with
   */
  NS_IMETHOD  AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent*     aChild,
                               PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType) = 0;

  /**
   * Return how your frame can be split.
   */
  NS_IMETHOD  IsSplittable(nsSplittableType& aIsSplittable) const = 0;

  /**
   * Flow member functions
   */
  NS_IMETHOD  GetPrevInFlow(nsIFrame** aPrevInFlow) const = 0;
  NS_IMETHOD  SetPrevInFlow(nsIFrame*) = 0;
  NS_IMETHOD  GetNextInFlow(nsIFrame** aNextInFlow) const = 0;
  NS_IMETHOD  SetNextInFlow(nsIFrame*) = 0;

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
   * Pre-reflow hook. Before a frame is reflowed this method will be called.
   * This call will always be invoked at least once before a subsequent Reflow
   * and DidReflow call. It may be called more than once, In general you will
   * receive on WillReflow notification before each Reflow request.
   *
   * XXX Is this really the semantics we want? Because we have the NS_FRAME_IN_REFLOW
   * bit we can ensure we don't call it more than once...
   */
  NS_IMETHOD  WillReflow(nsIPresContext* aPresContext) = 0;

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
   *          For an incremental reflow you are responsible for invalidating
   *          any area within your frame that needs repainting (including
   *          borders). If your new desired size is different than your current
   *          size, then your parent frame is responsible for making sure that
   *          the difference between the two rects is repainted
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
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
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
  NS_IMETHOD  DidReflow(nsIPresContext*           aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus         aStatus) = 0;

  // XXX Maybe these three should be a separate interface?

  /**
   * Helper method used by block reflow to identify runs of text so
   * that proper word-breaking can be done.
   *
   * @param aContinueTextRun A frame should set aContinueTextRun to
   *    PR_TRUE if we can continue a "text run" through the frame. A
   *    text run is text that should be treated contiguously for line
   *    and word breaking.
   *
   * @return The return value is irrelevant.
   */
  NS_IMETHOD CanContinueTextRun(PRBool& aContinueTextRun) const = 0;

  // Justification helper method used to distribute extra space in a
  // line to leaf frames. aUsedSpace is filled in with the amount of
  // space actually used.
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace) = 0;

  // Justification helper method that is used to remove trailing
  // whitespace before justification.
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth) = 0;

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
   * Find the closest view (on |this| or an ancestor).
   */
  nsIView* GetClosestView() const;

  /**
   * Find the closest ancestor (excluding |this| !) that has a view
   */
  nsIFrame* GetAncestorWithView() const;
  virtual nsIFrame* GetAncestorWithViewExternal() const;

  /**
   * Returns the offset from this frame to the closest geometric parent that
   * has a view. Also returns the containing view or null in case of error
   */
  NS_IMETHOD  GetOffsetFromView(nsIPresContext* aPresContext,
                                nsPoint&        aOffset,
                                nsIView**       aView) const = 0;

  /**
   * Returns the offset from this frame's upper left corner to the upper
   * left corner of the view returned by a call to GetView(). aOffset
   * will contain the offset to the view or (0,0) if the frame has no
   * view. aView will contain a pointer to the view returned by GetView().
   * aView is optional, that is, you may pass null if you are not interested
   * in getting a pointer to the view.
   */
  NS_IMETHOD  GetOriginToViewOffset(nsIPresContext* aPresContext,
                                    nsPoint&        aOffset,
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
   */
  virtual nsIWidget* GetWindow() const;

  /**
   * Get the "type" of the frame. May return a NULL atom pointer
   *
   * @see nsLayoutAtoms
   */
  virtual nsIAtom* GetType() const = 0;
  
  /**
   * Is this frame a containing block for non-positioned elements?
   */
  virtual PRBool IsContainingBlock() const = 0;

  /**
   * Invalidate part of the frame by asking the view manager to repaint.
   * aDamageRect is allowed to extend outside the frame's bounds. We'll do the right
   * thing. But it must be within the bounds of the view enclosing this frame.
   * We deliberately don't have an Invalidate() method that defaults to the frame's bounds.
   * We want all callers to *think* about what has changed in the frame and what area might
   * need to be repainted.
   *
   * @param aDamageRect is in the frame's local coordinate space
   */
  void Invalidate(const nsRect& aDamageRect, PRBool aImmediate = PR_FALSE) const;

  /**
   * Computes a rect that includes this frame's outline. The returned rect is
   * relative to this frame's origin.
   *
   * @param if nonnull, we record whether this rect is bigger than the frame's bounds
   * @return the rect relative to this frame's origin
   */
  nsRect GetOutlineRect(PRBool* aAnyOutline = nsnull) const;

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
  NS_IMETHOD  SetSelected(nsIPresContext* aPresContext,
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
  NS_IMETHOD  GetSelectionController(nsIPresContext *aPresContext, nsISelectionController **aSelCon) = 0;

  /** EndSelection related calls
   */

  /**
   *  Call to turn on/off mouseCapture at the view level. Needed by the ESM so
   *  it must be in the public interface.
   *  @param aPresContext presContext associated with the frame
   *  @param aGrabMouseEvents PR_TRUE to enable capture, PR_FALSE to disable
   */
  NS_IMETHOD CaptureMouse(nsIPresContext* aPresContext, PRBool aGrabMouseEvents) = 0;

  /**
   *  called to find the previous/next character, word, or line  returns the actual 
   *  nsIFrame and the frame offset.  THIS DOES NOT CHANGE SELECTION STATE
   *  uses frame's begin selection state to start. if no selection on this frame will 
   *  return NS_ERROR_FAILURE
   *  @param aPOS is defined in nsIFrameSelection
   */
  NS_IMETHOD  PeekOffset(nsIPresContext* aPresContext, nsPeekOffsetStruct *aPos) = 0;

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
  NS_IMETHOD CheckVisibility(nsIPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aRecurse, PRBool *aFinished, PRBool *_retval)=0;

  /**
   *  Called by a child frame on a parent frame to tell the parent frame that the child needs
   *  to be reflowed.  The parent should either propagate the request to its parent frame or 
   *  handle the request by generating a eReflowType_ReflowDirtyChildren reflow command.
   */

  NS_IMETHOD ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild) = 0;

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
  NS_IMETHOD GetParentStyleContextFrame(nsIPresContext* aPresContext,
                                        nsIFrame**      aProviderFrame,
                                        PRBool*         aIsChild) = 0;

  /**
   * Determines whether a frame is visible for painting
   * this takes into account whether it is painting a selection or printing.
   * @param aPresContext PresContext
   * @param aRenderingContext PresContext
   * @param aCheckVis indicates whether it should check for CSS visibility, 
   *                  PR_FALSE skips the check, PR_TRUE does the check
   * @param aIsVisible return value
   */
  NS_IMETHOD IsVisibleForPainting(nsIPresContext *     aPresContext, 
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aCheckVis,
                                  PRBool*              aIsVisible) = 0;

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


  void* GetProperty(nsIAtom* aPropertyName, nsresult* aStatus = nsnull) const;
  virtual void* GetPropertyExternal(nsIAtom*  aPropertyName,
                                    nsresult* aStatus) const;
  void* RemoveProperty(nsIAtom* aPropertyName, nsresult* aStatus = nsnull) const;
  nsresult SetProperty(nsIAtom*                aPropertyName,
                       void*                   aValue,
                       NSFramePropertyDtorFunc aDestructor = nsnull);

#define NS_GET_BASE_LEVEL(frame) \
NS_PTR_TO_INT32(frame->GetProperty(nsLayoutAtoms::baseLevel))

#define NS_GET_EMBEDDING_LEVEL(frame) \
NS_PTR_TO_INT32(frame->GetProperty(nsLayoutAtoms::embeddingLevel))

  /** Create or retrieve the previously stored overflow area, if the frame does 
    * not overflow and no creation is required return nsnull.
    * @param aPresContext PresContext
    * @param aCreateIfNecessary  create a new nsRect for the overflow area
    * @return pointer to the overflow area rectangle 
    */
  virtual nsRect* GetOverflowAreaProperty(nsIPresContext* aPresContext,
                                          PRBool          aCreateIfNecessary = PR_FALSE) = 0;

  /**
   * Return PR_TRUE if and only if this frame obeys visibility:hidden.
   * if it does not, then nsContainerFrame will hide its view even though
   * this means children can't be made visible again.
   */
  virtual PRBool SupportsVisibilityHidden() { return PR_TRUE; }

protected:
  // Members
  nsRect           mRect;
  nsIContent*      mContent;
  nsStyleContext*  mStyleContext;
  nsIFrame*        mParent;
  nsIFrame*        mNextSibling;  // singly-linked list of frames
  nsFrameState     mState;

private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

#endif /* nsIFrame_h___ */
