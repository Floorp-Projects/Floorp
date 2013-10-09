/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* interface for all rendering objects */

#ifndef nsIFrame_h___
#define nsIFrame_h___

#ifndef MOZILLA_INTERNAL_API
#error This header/class should only be used within Mozilla code. It should not be used by extensions.
#endif

#define MAX_REFLOW_DEPTH 200

/* nsIFrame is in the process of being deCOMtaminated, i.e., this file is eventually
   going to be eliminated, and all callers will use nsFrame instead.  At the moment
   we're midway through this process, so you will see inlined functions and member
   variables in this file.  -dwh */

#include <stdio.h>
#include "nsQueryFrame.h"
#include "nsStyleContext.h"
#include "nsStyleStruct.h"
#include "nsHTMLReflowMetrics.h"
#include "nsFrameList.h"
#include "mozilla/layout/FrameChildList.h"
#include "FramePropertyTable.h"
#include "mozilla/TypedEnum.h"
#include "nsDirection.h"
#include <algorithm>
#include "nsITheme.h"
#include "gfx3DMatrix.h"
#include "nsLayoutUtils.h"

#ifdef ACCESSIBILITY
#include "mozilla/a11y/AccTypes.h"
#endif

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

struct gfxMatrix;
class nsIAtom;
class nsPresContext;
class nsIPresShell;
class nsRenderingContext;
class nsView;
class nsIWidget;
class nsIDOMRange;
class nsISelectionController;
class nsBoxLayoutState;
class nsBoxLayout;
class nsILineIterator;
class nsDisplayListBuilder;
class nsDisplayListSet;
class nsDisplayList;
class gfxSkipChars;
class gfxSkipCharsIterator;
class gfxContext;
class nsLineList_iterator;
class nsAbsoluteContainingBlock;
class nsIContent;

struct nsPeekOffsetStruct;
struct nsPoint;
struct nsRect;
struct nsSize;
struct nsMargin;
struct CharacterDataChangeInfo;

namespace mozilla {
namespace layers {
class Layer;
}
}

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
typedef uint32_t nsSplittableType;

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
typedef uint64_t nsFrameState;

#define NS_FRAME_STATE_BIT(n_) (nsFrameState(1) << (n_))

#define NS_FRAME_IN_REFLOW                          NS_FRAME_STATE_BIT(0)

// This bit is set when a frame is created. After it has been reflowed
// once (during the DidReflow with a finished state) the bit is
// cleared.
#define NS_FRAME_FIRST_REFLOW                       NS_FRAME_STATE_BIT(1)

// For a continuation frame, if this bit is set, then this a "fluid" 
// continuation, i.e., across a line boundary. Otherwise it's a "hard"
// continuation, e.g. a bidi continuation.
#define NS_FRAME_IS_FLUID_CONTINUATION              NS_FRAME_STATE_BIT(2)

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

// Frame can be an abs/fixed pos. container, if its style says so.
// MarkAs[Not]AbsoluteContainingBlock will assert that this bit is set.
// NS_FRAME_HAS_ABSPOS_CHILDREN must not be set when this bit is unset.
#define NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN           NS_FRAME_STATE_BIT(9)

// If this bit is set, then the frame and _all_ of its descendant frames need
// to be reflowed.
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

// If this bit is set, then either:
//  1. the frame has at least one child that has the NS_FRAME_IS_DIRTY bit or
//     NS_FRAME_HAS_DIRTY_CHILDREN bit set, or
//  2. the frame has had at least one child removed since the last reflow, or
//  3. the frame has had a style change that requires the frame to be reflowed
//     but does not _necessarily_ require its descendants to be reflowed (e.g.,
//     for a 'height', 'width', 'margin', etc. change, it's up to the
//     applicable Reflow methods to decide whether the frame's children
//     _actually_ need to be reflowed).
// If this bit is set but the NS_FRAME_IS_DIRTY is not set, then Reflow still
// needs to be called on the frame, but Reflow will likely not do as much work
// as it would if NS_FRAME_IS_DIRTY were set. See the comment documenting
// nsFrame::Reflow for more.
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

// If this bit is set, then transforms (e.g. CSS or SVG transforms) are allowed
// to affect the frame, and a transform may currently be in affect. If this bit
// is not set, then any transforms on the frame will be ignored.
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
#define NS_FRAME_RESERVED                           ~NS_FRAME_IMPL_RESERVED

// This bit is set on floats whose parent does not contain their
// placeholder.  This can happen for two reasons:  (1) the float was
// split, and this piece is the continuation, or (2) the entire float
// didn't fit on the page.
// Note that this bit is also shared by text frames for
// TEXT_FORCE_TRIM_WHITESPACE.  That's OK because we only check the
// NS_FRAME_IS_PUSHED_FLOAT bit on frames which we already know are
// out-of-flow.
#define NS_FRAME_IS_PUSHED_FLOAT                    NS_FRAME_STATE_BIT(32)

// This bit acts as a loop flag for recursive paint server drawing.
#define NS_FRAME_DRAWING_AS_PAINTSERVER             NS_FRAME_STATE_BIT(33)

// Frame is a display root and the retained layer tree needs to be updated
// at the next paint via display list construction.
// Only meaningful for display roots, so we don't really need a global state
// bit; we could free up this bit with a little extra complexity.
#define NS_FRAME_UPDATE_LAYER_TREE                  NS_FRAME_STATE_BIT(36)

// Frame can accept absolutely positioned children.
#define NS_FRAME_HAS_ABSPOS_CHILDREN                NS_FRAME_STATE_BIT(37)

// A display item for this frame has been painted as part of a ThebesLayer.
#define NS_FRAME_PAINTED_THEBES                     NS_FRAME_STATE_BIT(38)

// Frame is or is a descendant of something with a fixed height, unless that
// ancestor is a body or html element, and has no closer ancestor that is
// overflow:auto or overflow:scroll.
#define NS_FRAME_IN_CONSTRAINED_HEIGHT              NS_FRAME_STATE_BIT(39)

// This is only set during painting
#define NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO    NS_FRAME_STATE_BIT(40)

// Is this frame a container for font size inflation, i.e., is it a
// frame whose width is used to determine the inflation factor for
// everything whose nearest ancestor container for this frame?
#define NS_FRAME_FONT_INFLATION_CONTAINER           NS_FRAME_STATE_BIT(41)

// Does this frame manage a region in which we do font size inflation,
// i.e., roughly, is it an element establishing a new block formatting
// context?
#define NS_FRAME_FONT_INFLATION_FLOW_ROOT           NS_FRAME_STATE_BIT(42)

// This bit is set on SVG frames that are laid out using SVG's coordinate
// system based layout (as opposed to any of the CSS layout models). Note that
// this does not include nsSVGOuterSVGFrame since it takes part is CSS layout.
#define NS_FRAME_SVG_LAYOUT                         NS_FRAME_STATE_BIT(43)

// Is this frame allowed to have generated (::before/::after) content?
#define NS_FRAME_MAY_HAVE_GENERATED_CONTENT         NS_FRAME_STATE_BIT(44)

// This bit is set on frames that create ContainerLayers with component
// alpha children. With BasicLayers we avoid creating these, so we mark
// the frames for future reference.
#define NS_FRAME_NO_COMPONENT_ALPHA                 NS_FRAME_STATE_BIT(45)

// The frame is a descendant of nsSVGTextFrame2 and is thus used for SVG
// text layout.
#define NS_FRAME_IS_SVG_TEXT                        NS_FRAME_STATE_BIT(47)

// Frame is marked as needing painting
#define NS_FRAME_NEEDS_PAINT                        NS_FRAME_STATE_BIT(48)

// Frame has a descendant frame that needs painting - This includes
// cross-doc children.
#define NS_FRAME_DESCENDANT_NEEDS_PAINT             NS_FRAME_STATE_BIT(49)

// Frame is a descendant of a popup
#define NS_FRAME_IN_POPUP                           NS_FRAME_STATE_BIT(50)

// Frame has only descendant frames that needs painting - This includes
// cross-doc children. This guarantees that all descendents have 
// NS_FRAME_NEEDS_PAINT and NS_FRAME_ALL_DESCENDANTS_NEED_PAINT, or they 
// have no display items.
#define NS_FRAME_ALL_DESCENDANTS_NEED_PAINT         NS_FRAME_STATE_BIT(51)

// Frame is marked as NS_FRAME_NEEDS_PAINT and also has an explicit
// rect stored to invalidate.
#define NS_FRAME_HAS_INVALID_RECT                   NS_FRAME_STATE_BIT(52)

// Frame is not displayed directly due to it being, or being under, an SVG
// <defs> element or an SVG resource element (<mask>, <pattern>, etc.)
#define NS_FRAME_IS_NONDISPLAY                      NS_FRAME_STATE_BIT(53)

// Box layout bits
#define NS_STATE_IS_HORIZONTAL                      NS_FRAME_STATE_BIT(22)
#define NS_STATE_IS_DIRECTION_NORMAL                NS_FRAME_STATE_BIT(31)

// Helper macros
#define NS_SUBTREE_DIRTY(_frame)  \
  (((_frame)->GetStateBits() &      \
    (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) != 0)

/**
 * Constant used to indicate an unconstrained size.
 *
 * @see #Reflow()
 */
#define NS_UNCONSTRAINEDSIZE NS_MAXSIZE

#define NS_INTRINSICSIZE    NS_UNCONSTRAINEDSIZE
#define NS_AUTOHEIGHT       NS_UNCONSTRAINEDSIZE
#define NS_AUTOMARGIN       NS_UNCONSTRAINEDSIZE
#define NS_AUTOOFFSET       NS_UNCONSTRAINEDSIZE
// NOTE: there are assumptions all over that these have the same value, namely NS_UNCONSTRAINEDSIZE
//       if any are changed to be a value other than NS_UNCONSTRAINEDSIZE
//       at least update AdjustComputedHeight/Width and test ad nauseum

//----------------------------------------------------------------------

enum nsSelectionAmount {
  eSelectCharacter = 0, // a single Unicode character;
                        // do not use this (prefer Cluster) unless you
                        // are really sure it's what you want
  eSelectCluster   = 1, // a grapheme cluster: this is usually the right
                        // choice for movement or selection by "character"
                        // as perceived by the user
  eSelectWord      = 2,
  eSelectLine      = 3, // previous drawn line in flow.
  eSelectBeginLine = 4,
  eSelectEndLine   = 5,
  eSelectNoAmount  = 6, // just bounce back current offset.
  eSelectParagraph = 7,  // select a "paragraph"
  eSelectWordNoSpace = 8 // select a "word" without selecting the following
                         // space, no matter what the default platform
                         // behavior is
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
typedef uint32_t nsReflowStatus;

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

// These macros set or switch incomplete statuses without touching the
// NS_FRAME_REFLOW_NEXTINFLOW bit.
#define NS_FRAME_SET_INCOMPLETE(status) \
  status = (status & ~NS_FRAME_OVERFLOW_INCOMPLETE) | NS_FRAME_NOT_COMPLETE

#define NS_FRAME_SET_OVERFLOW_INCOMPLETE(status) \
  status = (status & ~NS_FRAME_NOT_COMPLETE) | NS_FRAME_OVERFLOW_INCOMPLETE

// This macro tests to see if an nsReflowStatus is an error value
// or just a regular return value
#define NS_IS_REFLOW_ERROR(_status) (int32_t(_status) < 0)

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
// the frame will be reflowed later and hence its current completion
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
MOZ_BEGIN_ENUM_CLASS(nsDidReflowStatus, uint32_t)
  NOT_FINISHED,
  FINISHED
MOZ_END_ENUM_CLASS(nsDidReflowStatus)

/**
 * When there is no scrollable overflow rect, the visual overflow rect
 * may be stored as four 1-byte deltas each strictly LESS THAN 0xff, for
 * the four edges of the rectangle, or the four bytes may be read as a
 * single 32-bit "overflow-rect type" value including at least one 0xff
 * byte as an indicator that the value does NOT represent four deltas.
 * If all four deltas are zero, this means that no overflow rect has
 * actually been set (this is the initial state of newly-created frames).
 */
#define NS_FRAME_OVERFLOW_DELTA_MAX     0xfe // max delta we can store

#define NS_FRAME_OVERFLOW_NONE    0x00000000 // there are no overflow rects;
                                             // code relies on this being
                                             // the all-zero value

#define NS_FRAME_OVERFLOW_LARGE   0x000000ff // overflow is stored as a
                                             // separate rect property

namespace mozilla {
/*
 * For replaced elements only. Gets the intrinsic dimensions of this element.
 * The dimensions may only be one of the following two types:
 *
 *   eStyleUnit_Coord   - a length in app units
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
  bool operator==(const IntrinsicSize& rhs) {
    return width == rhs.width && height == rhs.height;
  }
  bool operator!=(const IntrinsicSize& rhs) {
    return !(*this == rhs);
  }
};
}

//----------------------------------------------------------------------

/**
 * A frame in the layout model. This interface is supported by all frame
 * objects.
 *
 * Frames can have multiple child lists: the default child list
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
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layout::FrameChildList ChildList;
  typedef mozilla::layout::FrameChildListID ChildListID;
  typedef mozilla::layout::FrameChildListIDs ChildListIDs;
  typedef mozilla::layout::FrameChildListIterator ChildListIterator;
  typedef mozilla::layout::FrameChildListArrayIterator ChildListArrayIterator;

  NS_DECL_QUERYFRAME_TARGET(nsIFrame)

  nsPresContext* PresContext() const {
    return StyleContext()->RuleNode()->PresContext();
  }

  /**
   * Called to initialize the frame. This is called immediately after creating
   * the frame.
   *
   * If the frame is a continuing frame, then aPrevInFlow indicates the previous
   * frame (the frame that was split).
   *
   * If you want a view associated with your frame, you should create the view
   * after Init() has returned.
   *
   * @param   aContent the content object associated with the frame
   * @param   aParent the parent frame
   * @param   aPrevInFlow the prev-in-flow frame
   */
  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) = 0;

  /**
   * Destroys this frame and each of its child frames (recursively calls
   * Destroy() for each child). If this frame is a first-continuation, this
   * also removes the frame from the primary frame map and clears undisplayed
   * content for its content node.
   * If the frame is a placeholder, it also ensures the out-of-flow frame's
   * removal and destruction.
   */
  void Destroy() { DestroyFrom(this); }

protected:
  /**
   * Return true if the frame is part of a Selection.
   * Helper method to implement the public IsSelected() API.
   */
  virtual bool IsFrameSelected() const;

  /**
   * Implements Destroy(). Do not call this directly except from within a
   * DestroyFrom() implementation.
   *
   * @note This will always be called, so it is not necessary to override
   *       Destroy() in subclasses of nsFrame, just DestroyFrom().
   *
   * @param  aDestructRoot is the root of the subtree being destroyed
   */
  virtual void DestroyFrom(nsIFrame* aDestructRoot) = 0;
  friend class nsFrameList; // needed to pass aDestructRoot through to children
  friend class nsLineBox;   // needed to pass aDestructRoot through to children
  friend class nsContainerFrame; // needed to pass aDestructRoot through to children
public:

  /**
   * Called to set the initial list of frames. This happens after the frame
   * has been initialized.
   *
   * This is only called once for a given child list, and won't be called
   * at all for child lists with no initial list of frames.
   *
   * @param   aListID the child list identifier.
   * @param   aChildList list of child frames. Each of the frames has its
   *            NS_FRAME_IS_DIRTY bit set.  Must not be empty.
   *            This method cannot handle the child list returned by
   *            GetAbsoluteListID().
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame or if the
   *            initial list of frames has already been set for that child list,
   *          NS_OK otherwise.  In this case, SetInitialChildList empties out
   *            aChildList in the process of moving the frames over to its own
   *            child list.
   * @see     #Init()
   */
  NS_IMETHOD  SetInitialChildList(ChildListID     aListID,
                                  nsFrameList&    aChildList) = 0;

  /**
   * This method is responsible for appending frames to the frame
   * list.  The implementation should append the frames to the specified
   * child list and then generate a reflow command.
   *
   * @param   aListID the child list identifier.
   * @param   aFrameList list of child frames to append. Each of the frames has
   *            its NS_FRAME_IS_DIRTY bit set.  Must not be empty.
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise.  In this case, AppendFrames empties out
   *            aFrameList in the process of moving the frames over to its own
   *            child list.
   */
  NS_IMETHOD AppendFrames(ChildListID     aListID,
                          nsFrameList&    aFrameList) = 0;

  /**
   * This method is responsible for inserting frames into the frame
   * list.  The implementation should insert the new frames into the specified
   * child list and then generate a reflow command.
   *
   * @param   aListID the child list identifier.
   * @param   aPrevFrame the frame to insert frames <b>after</b>
   * @param   aFrameList list of child frames to insert <b>after</b> aPrevFrame.
   *            Each of the frames has its NS_FRAME_IS_DIRTY bit set
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise.  In this case, InsertFrames empties out
   *            aFrameList in the process of moving the frames over to its own
   *            child list.
   */
  NS_IMETHOD InsertFrames(ChildListID     aListID,
                          nsIFrame*       aPrevFrame,
                          nsFrameList&    aFrameList) = 0;

  /**
   * This method is responsible for removing a frame in the frame
   * list.  The implementation should do something with the removed frame
   * and then generate a reflow command. The implementation is responsible
   * for destroying aOldFrame (the caller mustn't destroy aOldFrame).
   *
   * @param   aListID the child list identifier.
   * @param   aOldFrame the frame to remove
   * @return  NS_ERROR_INVALID_ARG if there is no child list with the specified
   *            name,
   *          NS_ERROR_FAILURE if the child frame is not in the specified
   *            child list,
   *          NS_ERROR_UNEXPECTED if the frame is an atomic frame,
   *          NS_OK otherwise
   */
  NS_IMETHOD RemoveFrame(ChildListID     aListID,
                         nsIFrame*       aOldFrame) = 0;

  /**
   * Get the content object associated with this frame. Does not add a reference.
   */
  nsIContent* GetContent() const { return mContent; }

  /**
   * Get the frame that should be the parent for the frames of child elements
   * May return nullptr during reflow
   */
  virtual nsIFrame* GetContentInsertionFrame() { return this; }

  /**
   * Move any frames on our overflow list to the end of our principal list.
   * @return true if there were any overflow frames
   */
  virtual bool DrainSelfOverflowList() { return false; }

  /**
   * Get the frame that should be scrolled if the content associated
   * with this frame is targeted for scrolling. For frames implementing
   * nsIScrollableFrame this will return the frame itself. For frames
   * like nsTextControlFrame that contain a scrollframe, will return
   * that scrollframe.
   */
  virtual nsIScrollableFrame* GetScrollTargetFrame() { return nullptr; }

  /**
   * Get the offsets of the frame. most will be 0,0
   *
   */
  NS_IMETHOD GetOffsets(int32_t &start, int32_t &end) const = 0;

  /**
   * Reset the offsets when splitting frames during Bidi reordering
   *
   */
  virtual void AdjustOffsetsForBidi(int32_t aStart, int32_t aEnd) {}

  /**
   * Get the style context associated with this frame.
   */
  nsStyleContext* StyleContext() const { return mStyleContext; }
  void SetStyleContext(nsStyleContext* aContext)
  { 
    if (aContext != mStyleContext) {
      nsStyleContext* oldStyleContext = mStyleContext;
      mStyleContext = aContext;
      aContext->AddRef();
      DidSetStyleContext(oldStyleContext);
      oldStyleContext->Release();
    }
  }

  /**
   * SetStyleContextWithoutNotification is for changes to the style
   * context that should suppress style change processing, in other
   * words, those that aren't really changes.  This generally means only
   * changes that happen during frame construction.
   */
  void SetStyleContextWithoutNotification(nsStyleContext* aContext)
  {
    if (aContext != mStyleContext) {
      mStyleContext->Release();
      mStyleContext = aContext;
      aContext->AddRef();
    }
  }

  // Style post processing hook
  // Attention: the old style context is the one we're forgetting,
  // and hence possibly completely bogus for GetStyle* purposes.
  // Use PeekStyleData instead.
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) = 0;

  /**
   * Define typesafe getter functions for each style struct by
   * preprocessing the list of style structs.  These functions are the
   * preferred way to get style data.  The macro creates functions like:
   *   const nsStyleBorder* StyleBorder();
   *   const nsStyleColor* StyleColor();
   *
   * Callers outside of libxul should use nsIDOMWindow::GetComputedStyle()
   * instead of these accessors.
   */
  #define STYLE_STRUCT(name_, checkdata_cb_)                                  \
    const nsStyle##name_ * Style##name_ () const {                            \
      NS_ASSERTION(mStyleContext, "No style context found!");                 \
      return mStyleContext->Style##name_ ();                                  \
    }
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  /** Also forward GetVisitedDependentColor to the style context */
  nscolor GetVisitedDependentColor(nsCSSProperty aProperty)
    { return mStyleContext->GetVisitedDependentColor(aProperty); }

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
  virtual nsStyleContext* GetAdditionalStyleContext(int32_t aIndex) const = 0;

  virtual void SetAdditionalStyleContext(int32_t aIndex,
                                         nsStyleContext* aStyleContext) = 0;

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
  nsPoint GetPosition() const { return mRect.TopLeft(); }
  nsSize GetSize() const { return mRect.Size(); }

  /**
   * When we change the size of the frame's border-box rect, we may need to
   * reset the overflow rect if it was previously stored as deltas.
   * (If it is currently a "large" overflow and could be re-packed as deltas,
   * we don't bother as the cost of the allocation has already been paid.)
   */
  void SetRect(const nsRect& aRect) {
    if (mOverflow.mType != NS_FRAME_OVERFLOW_LARGE &&
        mOverflow.mType != NS_FRAME_OVERFLOW_NONE) {
      nsOverflowAreas overflow = GetOverflowAreas();
      mRect = aRect;
      SetOverflowAreas(overflow);
    } else {
      mRect = aRect;
    }
  }
  void SetSize(const nsSize& aSize) {
    SetRect(nsRect(mRect.TopLeft(), aSize));
  }
  void SetPosition(const nsPoint& aPt) { mRect.MoveTo(aPt); }

  /**
   * Move the frame, accounting for relative positioning. Use this when
   * adjusting the frame's position by a known amount, to properly update its
   * saved normal position (see GetNormalPosition below).
   *
   * This must be used only when moving a frame *after*
   * nsHTMLReflowState::ApplyRelativePositioning is called.  When moving
   * a frame during the reflow process prior to calling
   * nsHTMLReflowState::ApplyRelativePositioning, the position should
   * simply be adjusted directly (e.g., using SetPosition()).
   */
  void MovePositionBy(const nsPoint& aTranslation);

  /**
   * Return frame's position without relative positioning
   */
  nsPoint GetNormalPosition() const;

  virtual nsPoint GetPositionOfChildIgnoringScrolling(nsIFrame* aChild)
  { return aChild->GetPosition(); }
  
  nsPoint GetPositionIgnoringScrolling() {
    return mParent ? mParent->GetPositionOfChildIgnoringScrolling(this)
      : GetPosition();
  }

  static void DestroyRegion(void* aPropertyValue);

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

  static void DestroyOverflowAreas(void* aPropertyValue)
  {
    delete static_cast<nsOverflowAreas*>(aPropertyValue);
  }

  static void DestroySurface(void* aPropertyValue);
  static void DestroyDT(void* aPropertyValue);

#ifdef _MSC_VER
// XXX Workaround MSVC issue by making the static FramePropertyDescriptor
// non-const.  See bug 555727.
#define NS_PROPERTY_DESCRIPTOR_CONST
#else
#define NS_PROPERTY_DESCRIPTOR_CONST const
#endif

#define NS_DECLARE_FRAME_PROPERTY(prop, dtor)                                                  \
  static const FramePropertyDescriptor* prop() {                                               \
    static NS_PROPERTY_DESCRIPTOR_CONST FramePropertyDescriptor descriptor = { dtor, nullptr }; \
    return &descriptor;                                                                        \
  }
// Don't use this unless you really know what you're doing!
#define NS_DECLARE_FRAME_PROPERTY_WITH_FRAME_IN_DTOR(prop, dtor)                               \
  static const FramePropertyDescriptor* prop() {                                               \
    static NS_PROPERTY_DESCRIPTOR_CONST FramePropertyDescriptor descriptor = { nullptr, dtor }; \
    return &descriptor;                                                                        \
  }

  NS_DECLARE_FRAME_PROPERTY(IBSplitSpecialSibling, nullptr)
  NS_DECLARE_FRAME_PROPERTY(IBSplitSpecialPrevSibling, nullptr)

  NS_DECLARE_FRAME_PROPERTY(NormalPositionProperty, DestroyPoint)
  NS_DECLARE_FRAME_PROPERTY(ComputedOffsetProperty, DestroyMargin)

  NS_DECLARE_FRAME_PROPERTY(OutlineInnerRectProperty, DestroyRect)
  NS_DECLARE_FRAME_PROPERTY(PreEffectsBBoxProperty, DestroyRect)
  NS_DECLARE_FRAME_PROPERTY(PreTransformOverflowAreasProperty,
                            DestroyOverflowAreas)

  // The initial overflow area passed to FinishAndStoreOverflow. This is only set
  // on frames that Preserve3D(), and when at least one of the overflow areas
  // differs from the frame bound rect.
  NS_DECLARE_FRAME_PROPERTY(InitialOverflowProperty, DestroyOverflowAreas)

  NS_DECLARE_FRAME_PROPERTY(UsedMarginProperty, DestroyMargin)
  NS_DECLARE_FRAME_PROPERTY(UsedPaddingProperty, DestroyMargin)
  NS_DECLARE_FRAME_PROPERTY(UsedBorderProperty, DestroyMargin)

  NS_DECLARE_FRAME_PROPERTY(ScrollLayerCount, nullptr)

  NS_DECLARE_FRAME_PROPERTY(LineBaselineOffset, nullptr)

  NS_DECLARE_FRAME_PROPERTY(CachedBackgroundImage, DestroySurface)
  NS_DECLARE_FRAME_PROPERTY(CachedBackgroundImageDT, DestroyDT)

  NS_DECLARE_FRAME_PROPERTY(InvalidationRect, DestroyRect)

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
   * Note that this differs from StyleBorder()->GetBorder() in that
   * this describes region of the frame's box, and
   * StyleBorder()->GetBorder() describes a border.  They differ only
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
   *
   * @param aMargin The margin to apply the result of GetSkipSides() to.
   * @param aReflowState An optional reflow state parameter, which is used if
   *        ApplySkipSides() is being called in the middle of reflow.
   *
   * @note (See also bug 743402, comment 11) GetSkipSides() and its sister
   *       method, ApplySkipSides() checks to see if this frame has a previous
   *       or next continuation to determine if a side should be skipped.
   *       Unfortunately, this only works after reflow has been completed. In
   *       lieu of this, during reflow, an nsHTMLReflowState parameter can be
   *       passed in, indicating that it should be used to determine if sides
   *       should be skipped during reflow.
   */
  void ApplySkipSides(nsMargin& aMargin,
                      const nsHTMLReflowState* aReflowState = nullptr) const;

  /**
   * Like the frame's rect (see |GetRect|), which is the border rect,
   * other rectangles of the frame, in app units, relative to the parent.
   */
  nsRect GetPaddingRect() const;
  nsRect GetPaddingRectRelativeToSelf() const;
  nsRect GetContentRect() const;
  nsRect GetContentRectRelativeToSelf() const;

  /**
   * The area to paint box-shadows around.  The default is the border rect.
   * (nsFieldSetFrame overrides this).
   */
  virtual nsRect VisualBorderRectRelativeToSelf() const {
    return nsRect(0, 0, mRect.width, mRect.height);
  }

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
  static bool ComputeBorderRadii(const nsStyleCorners& aBorderRadius,
                                   const nsSize& aFrameSize,
                                   const nsSize& aBorderArea,
                                   int aSkipSides,
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
  virtual bool GetBorderRadii(nscoord aRadii[8]) const;

  bool GetPaddingBoxBorderRadii(nscoord aRadii[8]) const;
  bool GetContentBoxBorderRadii(nscoord aRadii[8]) const;

  /**
   * Get the position of the frame's baseline, relative to the top of
   * the frame (its top border edge).  Only valid when Reflow is not
   * needed.
   */
  virtual nscoord GetBaseline() const = 0;

  /**
   * Get the position of the baseline on which the caret needs to be placed,
   * relative to the top of the frame.  This is mostly needed for frames
   * which return a baseline from GetBaseline which is not useful for
   * caret positioning.
   */
  virtual nscoord GetCaretBaseline() const {
    return GetBaseline();
  }

  /**
   * Get the specified child list.
   *
   * @param   aListID identifies the requested child list.
   * @return  the child list.  If the requested list is unsupported by this
   *          frame type, an empty list will be returned.
   */
  virtual const nsFrameList& GetChildList(ChildListID aListID) const = 0;
  const nsFrameList& PrincipalChildList() { return GetChildList(kPrincipalList); }
  virtual void GetChildLists(nsTArray<ChildList>* aLists) const = 0;

  /**
   * Gets the child lists for this frame, including
   * ones belong to a child document.
   */
  void GetCrossDocChildLists(nsTArray<ChildList>* aLists);

  // XXXbz this method should go away
  nsIFrame* GetFirstChild(ChildListID aListID) const {
    return GetChildList(aListID).FirstChild();
  }
  // XXXmats this method should also go away then
  nsIFrame* GetLastChild(ChildListID aListID) const {
    return GetChildList(aListID).LastChild();
  }
  nsIFrame* GetFirstPrincipalChild() const {
    return GetFirstChild(kPrincipalList);
  }

  // The individual concrete child lists.
  static const ChildListID kPrincipalList = mozilla::layout::kPrincipalList;
  static const ChildListID kAbsoluteList = mozilla::layout::kAbsoluteList;
  static const ChildListID kBulletList = mozilla::layout::kBulletList;
  static const ChildListID kCaptionList = mozilla::layout::kCaptionList;
  static const ChildListID kColGroupList = mozilla::layout::kColGroupList;
  static const ChildListID kExcessOverflowContainersList = mozilla::layout::kExcessOverflowContainersList;
  static const ChildListID kFixedList = mozilla::layout::kFixedList;
  static const ChildListID kFloatList = mozilla::layout::kFloatList;
  static const ChildListID kOverflowContainersList = mozilla::layout::kOverflowContainersList;
  static const ChildListID kOverflowList = mozilla::layout::kOverflowList;
  static const ChildListID kOverflowOutOfFlowList = mozilla::layout::kOverflowOutOfFlowList;
  static const ChildListID kPopupList = mozilla::layout::kPopupList;
  static const ChildListID kPushedFloatsList = mozilla::layout::kPushedFloatsList;
  static const ChildListID kSelectPopupList = mozilla::layout::kSelectPopupList;
  // A special alias for kPrincipalList that do not request reflow.
  static const ChildListID kNoReflowPrincipalList = mozilla::layout::kNoReflowPrincipalList;

  /**
   * Child frames are linked together in a doubly-linked list
   */
  nsIFrame* GetNextSibling() const { return mNextSibling; }
  void SetNextSibling(nsIFrame* aNextSibling) {
    NS_ASSERTION(this != aNextSibling, "Creating a circular frame list, this is very bad.");
    if (mNextSibling && mNextSibling->GetPrevSibling() == this) {
      mNextSibling->mPrevSibling = nullptr;
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
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) {}
  /**
   * Displays the caret onto the given display list builder. The caret is
   * painted on top of the rest of the display list items.
   *
   * @param aDirtyRect is the dirty rectangle that we're repainting.
   */
  void DisplayCaret(nsDisplayListBuilder* aBuilder,
                    const nsRect&         aDirtyRect,
                    nsDisplayList*        aList);

  /**
   * Get the preferred caret color at the offset.
   *
   * @param aOffset is offset of the content.
   */
  virtual nscolor GetCaretColorAt(int32_t aOffset);

 
  bool IsThemed(nsITheme::Transparency* aTransparencyState = nullptr) const {
    return IsThemed(StyleDisplay(), aTransparencyState);
  }
  bool IsThemed(const nsStyleDisplay* aDisp,
                  nsITheme::Transparency* aTransparencyState = nullptr) const {
    nsIFrame* mutable_this = const_cast<nsIFrame*>(this);
    if (!aDisp->mAppearance)
      return false;
    nsPresContext* pc = PresContext();
    nsITheme *theme = pc->GetTheme();
    if(!theme ||
       !theme->ThemeSupportsWidget(pc, mutable_this, aDisp->mAppearance))
      return false;
    if (aTransparencyState) {
      *aTransparencyState =
        theme->GetWidgetTransparency(mutable_this, aDisp->mAppearance);
    }
    return true;
  }
  
  /**
   * Builds a display list for the content represented by this frame,
   * treating this frame as the root of a stacking context.
   * @param aDirtyRect content outside this rectangle can be ignored; the
   * rectangle is in frame coordinates
   */
  void BuildDisplayListForStackingContext(nsDisplayListBuilder* aBuilder,
                                          const nsRect&         aDirtyRect,
                                          nsDisplayList*        aList);

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
  void BuildDisplayListForChild(nsDisplayListBuilder*   aBuilder,
                                nsIFrame*               aChild,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists,
                                uint32_t                aFlags = 0);

  /**
   * Does this frame need a view?
   */
  virtual bool NeedsView() { return false; }

  /**
   * Returns true if this frame is transformed (e.g. has CSS or SVG transforms)
   * or if its parent is an SVG frame that has children-only transforms (e.g.
   * an SVG viewBox attribute).
   */
  bool IsTransformed() const;
  
  bool HasOpacity() const;

   /**
   * Return true if this frame might be using a transform getter.
   */
  virtual bool HasTransformGetter() const { return false; }

  /**
   * Returns true if this frame is an SVG frame that has SVG transforms applied
   * to it, or if its parent frame is an SVG frame that has children-only
   * transforms (e.g. an SVG viewBox attribute).
   * If aOwnTransforms is non-null and the frame has its own SVG transforms,
   * aOwnTransforms will be set to these transforms. If aFromParentTransforms
   * is non-null and the frame has an SVG parent with children-only transforms,
   * then aFromParentTransforms will be set to these transforms.
   */
  virtual bool IsSVGTransformed(gfxMatrix *aOwnTransforms = nullptr,
                                gfxMatrix *aFromParentTransforms = nullptr) const;

  /**
   * Returns whether this frame will attempt to preserve the 3d transforms of its
   * children. This is a direct indicator of -moz-transform-style: preserve-3d.
   */
  bool Preserves3DChildren() const;

  /**
   * Returns whether this frame has a parent that Preserves3DChildren() and
   * can respect this. Returns false if the frame is clipped.
   */
  bool Preserves3D() const;

  bool HasPerspective() const;

  bool ChildrenHavePerspective() const;

  // Calculate the overflow size of all child frames, taking preserve-3d into account
  void ComputePreserve3DChildrenOverflow(nsOverflowAreas& aOverflowAreas, const nsRect& aBounds);

  void RecomputePerspectiveChildrenOverflow(const nsStyleContext* aStartStyle, const nsRect* aBounds);

  /**
   * Returns the number of ancestors between this and the root of our frame tree
   */
  uint32_t GetDepthInFrameTree() {
    uint32_t result = 0;
    for (nsIFrame* ancestor = GetParent(); ancestor;
         ancestor = ancestor->GetParent()) {
      result++;
    }
    return result;
  }

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
   * @see     WidgetGUIEvent
   * @see     nsEventStatus
   */
  NS_IMETHOD  HandleEvent(nsPresContext* aPresContext,
                          mozilla::WidgetGUIEvent* aEvent,
                          nsEventStatus* aEventStatus) = 0;

  NS_IMETHOD  GetContentForEvent(mozilla::WidgetEvent* aEvent,
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
  struct MOZ_STACK_CLASS ContentOffsets {
    ContentOffsets();
    ContentOffsets(const ContentOffsets&);
    ~ContentOffsets();
    nsCOMPtr<nsIContent> content;
    bool IsNull() { return !content; }
    int32_t offset;
    int32_t secondaryOffset;
    // Helpers for places that need the ends of the offsets and expect them in
    // numerical order, as opposed to wanting the primary and secondary offsets
    int32_t StartOffset() { return std::min(offset, secondaryOffset); }
    int32_t EndOffset() { return std::max(offset, secondaryOffset); }
    // This boolean indicates whether the associated content is before or after
    // the offset; the most visible use is to allow the caret to know which line
    // to display on.
    bool associateWithNext;
  };
  enum {
    IGNORE_SELECTION_STYLE = 0x01,
    // Treat visibility:hidden frames as non-selectable
    SKIP_HIDDEN = 0x02
  };
  /**
   * This function calculates the content offsets for selection relative to
   * a point.  Note that this should generally only be callled on the event
   * frame associated with an event because this function does not account
   * for frame lists other than the primary one.
   * @param aPoint point relative to this frame
   */
  ContentOffsets GetContentOffsetsFromPoint(nsPoint aPoint,
                                            uint32_t aFlags = 0);

  virtual ContentOffsets GetContentOffsetsFromPointExternal(nsPoint aPoint,
                                                            uint32_t aFlags = 0)
  { return GetContentOffsetsFromPoint(aPoint, aFlags); }

  /**
   * Ensure that aImage gets notifed when the underlying image request loads
   * or animates.
   */
  void AssociateImage(const nsStyleImage& aImage, nsPresContext* aPresContext);

  /**
   * This structure holds information about a cursor. mContainer represents a
   * loaded image that should be preferred. If it is not possible to use it, or
   * if it is null, mCursor should be used.
   */
  struct MOZ_STACK_CLASS Cursor {
    nsCOMPtr<imgIContainer> mContainer;
    int32_t                 mCursor;
    bool                    mHaveHotspot;
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
  NS_IMETHOD  GetPointFromOffset(int32_t                  inOffset,
                                 nsPoint*                 outPoint) = 0;
  
  /**
   * Get the child frame of this frame which contains the given
   * content offset. outChildFrame may be this frame, or nullptr on return.
   * outContentOffset returns the content offset relative to the start
   * of the returned node. You can also pass a hint which tells the method
   * to stick to the end of the first found frame or the beginning of the 
   * next in case the offset falls on a boundary.
   */
  NS_IMETHOD  GetChildFrameContainingOffset(int32_t       inContentOffset,
                                 bool                     inHint,//false stick left
                                 int32_t*                 outFrameContentOffset,
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
   * Checks if the current frame-state includes all of the listed bits
   */
  bool HasAllStateBits(nsFrameState aBits) { return (mState & aBits) == aBits; }
  
  /**
   * Checks if the current frame-state includes any of the listed bits
   */
  bool HasAnyStateBits(nsFrameState aBits) { return mState & aBits; }

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
  NS_IMETHOD  AttributeChanged(int32_t         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               int32_t         aModType) = 0;

  /**
   * When the content states of a content object change, this method is invoked
   * on the primary frame of that content object.
   *
   * @param aStates the changed states
   */
  virtual void ContentStatesChanged(nsEventStates aStates) { }

  /**
   * Return how your frame can be split.
   */
  virtual nsSplittableType GetSplittableType() const = 0;

  /**
   * Continuation member functions
   */
  virtual nsIFrame* GetPrevContinuation() const = 0;
  virtual void SetPrevContinuation(nsIFrame*) = 0;
  virtual nsIFrame* GetNextContinuation() const = 0;
  virtual void SetNextContinuation(nsIFrame*) = 0;
  virtual nsIFrame* FirstContinuation() const {
    return const_cast<nsIFrame*>(this);
  }
  virtual nsIFrame* LastContinuation() const {
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
  virtual void SetPrevInFlow(nsIFrame*) = 0;

  virtual nsIFrame* GetNextInFlowVirtual() const = 0;
  nsIFrame* GetNextInFlow() const { return GetNextInFlowVirtual(); }
  virtual void SetNextInFlow(nsIFrame*) = 0;

  /**
   * Return the first frame in our current flow. 
   */
  virtual nsIFrame* FirstInFlow() const {
    return const_cast<nsIFrame*>(this);
  }

  /**
   * Return the last frame in our current flow.
   */
  virtual nsIFrame* LastInFlow() const {
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
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext) = 0;

  /**
   * Get the intrinsic width of the frame.  This must be greater than or
   * equal to the intrinsic minimum width.
   *
   * Otherwise, all the comments for |GetMinWidth| above apply.
   */
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext) = 0;

  /**
   * |InlineIntrinsicWidth| represents the intrinsic width information
   * in inline layout.  Code that determines the intrinsic width of a
   * region of inline layout accumulates the result into this structure.
   * This pattern is needed because we need to maintain state
   * information about whitespace (for both collapsing and trimming).
   */
  struct InlineIntrinsicWidthData {
    InlineIntrinsicWidthData()
      : line(nullptr)
      , lineContainer(nullptr)
      , prevLines(0)
      , currentLine(0)
      , skipWhitespace(true)
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
    bool skipWhitespace;

    // This contains the width of the trimmable whitespace at the end of
    // |currentLine|; it is zero if there is no such whitespace.
    nscoord trailingWhitespace;

    // Floats encountered in the lines.
    class FloatInfo {
    public:
      FloatInfo(const nsIFrame* aFrame, nscoord aWidth)
        : mFrame(aFrame), mWidth(aWidth)
      { }
      const nsIFrame* Frame() const { return mFrame; }
      nscoord         Width() const { return mWidth; }

    private:
      const nsIFrame* mFrame;
      nscoord         mWidth;
    };

    nsTArray<FloatInfo> floats;
  };

  struct InlineMinWidthData : public InlineIntrinsicWidthData {
    InlineMinWidthData()
      : trailingTextFrame(nullptr)
      , atStartOfLine(true)
    {}

    // We need to distinguish forced and optional breaks for cases where the
    // current line total is negative.  When it is, we need to ignore
    // optional breaks to prevent min-width from ending up bigger than
    // pref-width.
    void ForceBreak(nsRenderingContext *aRenderingContext);

    // If the break here is actually taken, aHyphenWidth must be added to the
    // width of the current line.
    void OptionallyBreak(nsRenderingContext *aRenderingContext,
                         nscoord aHyphenWidth = 0);

    // The last text frame processed so far in the current line, when
    // the last characters in that text frame are relevant for line
    // break opportunities.
    nsIFrame *trailingTextFrame;

    // Whether we're currently at the start of the line.  If we are, we
    // can't break (for example, between the text-indent and the first
    // word).
    bool atStartOfLine;
  };

  struct InlinePrefWidthData : public InlineIntrinsicWidthData {
    void ForceBreak(nsRenderingContext *aRenderingContext);
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
  AddInlineMinWidth(nsRenderingContext *aRenderingContext,
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
  AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
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
    IntrinsicWidthOffsets(nsRenderingContext* aRenderingContext) = 0;

  virtual mozilla::IntrinsicSize GetIntrinsicSize() = 0;

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
   * Bit-flags to pass to ComputeSize in |aFlags| parameter.
   */
  enum {
    /* Set if the frame is in a context where non-replaced blocks should
     * shrink-wrap (e.g., it's floating, absolutely positioned, or
     * inline-block). */
    eShrinkWrap =        1 << 0,
    /* Set if we'd like to compute our 'auto' height, regardless of our actual
     * computed value of 'height'. (e.g. to get an intrinsic height for flex
     * items with "min-height: auto" to use during flexbox layout.) */
    eUseAutoHeight =     1 << 1
  };

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
   * @param aPadding The sum of the vertical / horizontal margins of
   *                 the frame, including actual values resulting from
   *                 percentages.
   * @param aFlags   Flags to further customize behavior (definitions above).
   */
  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             uint32_t aFlags) = 0;

  /**
   * Compute a tight bounding rectangle for the frame. This is a rectangle
   * that encloses the pixels that are actually drawn. We're allowed to be
   * conservative and currently we don't try very hard. The rectangle is
   * in appunits and relative to the origin of this frame.
   *
   * This probably only needs to include frame bounds, glyph bounds, and
   * text decorations, but today it sometimes includes other things that
   * contribute to visual overflow.
   *
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
   * Updates the overflow areas of the frame. This can be called if an
   * overflow area of the frame's children has changed without reflowing.
   * @return true if either of the overflow areas for this frame have changed.
   */
  virtual bool UpdateOverflow() = 0;

  /**
   * Helper method used by block reflow to identify runs of text so
   * that proper word-breaking can be done.
   *
   * @return 
   *    true if we can continue a "text run" through the frame. A
   *    text run is text that should be treated contiguously for line
   *    and word breaking.
   */
  virtual bool CanContinueTextRun() const = 0;

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
  virtual nsresult GetRenderedText(nsAString* aAppendToString = nullptr,
                                   gfxSkipChars* aSkipChars = nullptr,
                                   gfxSkipCharsIterator* aSkipIter = nullptr,
                                   uint32_t aSkippedStartOffset = 0,
                                   uint32_t aSkippedMaxLength = UINT32_MAX)
  { return NS_ERROR_NOT_IMPLEMENTED; }

  /**
   * Returns true if the frame contains any non-collapsed characters.
   * This method is only available for text frames, and it will return false
   * for all other frame types.
   */
  virtual bool HasAnyNoncollapsedCharacters()
  { return false; }

  /**
   * Accessor functions to get/set the associated view object
   *
   * GetView returns non-null if and only if |HasView| returns true.
   */
  bool HasView() const { return !!(mState & NS_FRAME_HAS_VIEW); }
  nsView* GetView() const;
  virtual nsView* GetViewExternal() const;
  nsresult SetView(nsView* aView);

  /**
   * Find the closest view (on |this| or an ancestor).
   * If aOffset is non-null, it will be set to the offset of |this|
   * from the returned view.
   */
  nsView* GetClosestView(nsPoint* aOffset = nullptr) const;

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
                                nsView** aView) const = 0;

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
   * Get the "type" of the frame. May return nullptr.
   *
   * @see nsGkAtoms
   */
  virtual nsIAtom* GetType() const = 0;

  /**
   * Returns a transformation matrix that converts points in this frame's
   * coordinate space to points in some ancestor frame's coordinate space.
   * The frame decides which ancestor it will use as a reference point.
   * If this frame has no ancestor, aOutAncestor will be set to null.
   *
   * @param aStopAtAncestor don't look further than aStopAtAncestor. If null,
   *   all ancestors (including across documents) will be traversed.
   * @param aOutAncestor [out] The ancestor frame the frame has chosen.  If
   *   this frame has no ancestor, *aOutAncestor will be set to null. If
   * this frame is not a root frame, then *aOutAncestor will be in the same
   * document as this frame. If this frame IsTransformed(), then *aOutAncestor
   * will be the parent frame (if not preserve-3d) or the nearest non-transformed
   * ancestor (if preserve-3d).
   * @return A gfxMatrix that converts points in this frame's coordinate space
   *   into points in aOutAncestor's coordinate space.
   */
  gfx3DMatrix GetTransformMatrix(const nsIFrame* aStopAtAncestor,
                                 nsIFrame **aOutAncestor);

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
    eTablePart =                        1 << 13,
    // If this bit is set, the frame doesn't allow ignorable whitespace as
    // children. For example, the whitespace between <table>\n<tr>\n<td>
    // will be excluded during the construction of children. 
    eExcludesIgnorableWhitespace =      1 << 14,
    eSupportsCSSTransforms =            1 << 15,

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
  virtual bool IsFrameOfType(uint32_t aFlags) const
  {
#ifdef DEBUG
    return !(aFlags & ~(nsIFrame::eDEBUGAllFrames | nsIFrame::eSupportsCSSTransforms));
#else
    return !(aFlags & ~nsIFrame::eSupportsCSSTransforms);
#endif
  }

  /**
   * Returns true if the frame is a block wrapper.
   */
  bool IsBlockWrapper() const;

  /**
   * Get this frame's CSS containing block.
   *
   * The algorithm is defined in
   * http://www.w3.org/TR/CSS2/visudet.html#containing-block-details.
   *
   * NOTE: This is guaranteed to return a non-null pointer when invoked on any
   * frame other than the root frame.
   */
  nsIFrame* GetContainingBlock() const;

  /**
   * Is this frame a containing block for floating elements?
   * Note that very few frames are, so default to false.
   */
  virtual bool IsFloatContainingBlock() const { return false; }

  /**
   * Is this a leaf frame?  Frames that want the frame constructor to be able
   * to construct kids for them should return false, all others should return
   * true.  Note that returning true here does not mean that the frame _can't_
   * have kids.  It could still have kids created via
   * nsIAnonymousContentCreator.  Returning true indicates that "normal"
   * (non-anonymous, XBL-bound, CSS generated content, etc) children should not
   * be constructed.
   */
  virtual bool IsLeaf() const;

  /**
   * Is this a flex item? (Is the parent a flex container frame?)
   */
  bool IsFlexItem() const
  { return mParent && mParent->GetType() == nsGkAtoms::flexContainerFrame; }

  /**
   * Mark this frame as using active layers. This marking will time out
   * after a short period. This call does no immediate invalidation,
   * but when the mark times out, we'll invalidate the frame's overflow
   * area.
   * @param aChangeHint nsChangeHint_UpdateTransformLayer or
   * nsChangeHint_UpdateOpacityLayer or 0, depending on whether the change
   * triggering the activity is a changing transform, changing opacity, or
   * something else.
   */
  void MarkLayersActive(nsChangeHint aHint);
  /**
   * Return true if this frame is marked as needing active layers.
   */
  bool AreLayersMarkedActive();
  /**
   * Return true if this frame is marked as needing active layers.
   * @param aChangeHint nsChangeHint_UpdateTransformLayer or
   * nsChangeHint_UpdateOpacityLayer. We return true only if
   * a change in the transform or opacity has been recorded while layers have
   * been marked active for this frame.
   */
  bool AreLayersMarkedActive(nsChangeHint aChangeHint);

  /**
   * Marks all display items created by this frame as needing a repaint,
   * and calls SchedulePaint() if requested and one is not already pending.
   *
   * This includes all display items created by this frame, including
   * container types.
   *
   * @param aDisplayItemKey If specified, only issues an invalidate
   * if this frame painted a display item of that type during the 
   * previous paint. SVG rendering observers are always notified.
   */
  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0);

  /**
   * Same as InvalidateFrame(), but only mark a fixed rect as needing
   * repainting.
   *
   * @param aRect The rect to invalidate, relative to the TopLeft of the
   * frame's border box.
   * @param aDisplayItemKey If specified, only issues an invalidate
   * if this frame painted a display item of that type during the 
   * previous paint. SVG rendering observers are always notified.
   */
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0);
  
  /**
   * Calls InvalidateFrame() on all frames descendant frames (including
   * this one).
   * 
   * This function doesn't walk through placeholder frames to invalidate
   * the out-of-flow frames.
   *
   * @param aDisplayItemKey If specified, only issues an invalidate
   * if this frame painted a display item of that type during the 
   * previous paint. SVG rendering observers are always notified.
   */
  void InvalidateFrameSubtree(uint32_t aDisplayItemKey = 0);

  /**
   * Called when a frame is about to be removed and needs to be invalidated.
   * Normally does nothing since DLBI handles removed frames.
   */
  virtual void InvalidateFrameForRemoval() {}

  /**
   * When HasUserData(frame->LayerIsPrerenderedDataKey()), then the
   * entire overflow area of this frame has been rendered in its
   * layer(s).
   */
  static void* LayerIsPrerenderedDataKey() { 
    return &sLayerIsPrerenderedDataKey;
  }
  static uint8_t sLayerIsPrerenderedDataKey;

   /**
   * Try to update this frame's transform without invalidating any
   * content.  Return true iff successful.  If unsuccessful, the
   * caller is responsible for scheduling an invalidating paint.
   */
  bool TryUpdateTransformOnly();

  /**
   * Checks if a frame has had InvalidateFrame() called on it since the
   * last paint.
   *
   * If true, then the invalid rect is returned in aRect, with an
   * empty rect meaning all pixels drawn by this frame should be
   * invalidated.
   * If false, aRect is left unchanged.
   */
  bool IsInvalid(nsRect& aRect);
 
  /**
   * Check if any frame within the frame subtree (including this frame) 
   * returns true for IsInvalid().
   */
  bool HasInvalidFrameInSubtree()
  {
    return HasAnyStateBits(NS_FRAME_NEEDS_PAINT | NS_FRAME_DESCENDANT_NEEDS_PAINT);
  }

  /**
   * Removes the invalid state from the current frame and all
   * descendant frames.
   */
  void ClearInvalidationStateBits();

  /**
   * Ensures that the refresh driver is running, and schedules a view 
   * manager flush on the next tick.
   *
   * The view manager flush will update the layer tree, repaint any 
   * invalid areas in the layer tree and schedule a layer tree
   * composite operation to display the layer tree.
   *
   * In general it is not necessary for frames to call this when they change.
   * For example, changes that result in a reflow will have this called for
   * them by PresContext::DoReflow when the reflow begins. Style changes that 
   * do not trigger a reflow should have this called for them by
   * DoApplyRenderingChangeToTree.
   *
   * @param aFlags PAINT_COMPOSITE_ONLY : No changes have been made
   * that require a layer tree update, so only schedule a layer
   * tree composite.
   */
  enum {
    PAINT_DEFAULT = 0,
    PAINT_COMPOSITE_ONLY = 1 << 0
  };
  void SchedulePaint(uint32_t aFlags = PAINT_DEFAULT);

  /**
   * Checks if the layer tree includes a dedicated layer for this 
   * frame/display item key pair, and invalidates at least aDamageRect
   * area within that layer.
   *
   * If no layer is found, calls InvalidateFrame() instead.
   *
   * @param aDamageRect Area of the layer to invalidate.
   * @param aDisplayItemKey Display item type.
   * @param aFlags UPDATE_IS_ASYNC : Will skip the invalidation
   * if the found layer is being composited by a remote
   * compositor.
   * @return Layer, if found, nullptr otherwise.
   */
  enum {
    UPDATE_IS_ASYNC = 1 << 0
  };
  Layer* InvalidateLayer(uint32_t aDisplayItemKey, const nsIntRect* aDamageRect = nullptr, uint32_t aFlags = 0);

  /**
   * Returns a rect that encompasses everything that might be painted by
   * this frame.  This includes this frame, all its descendent frames, this
   * frame's outline, and descentant frames' outline, but does not include
   * areas clipped out by the CSS "overflow" and "clip" properties.
   *
   * HasOverflowRects() (below) will return true when this overflow
   * rect has been explicitly set, even if it matches mRect.
   * XXX Note: because of a space optimization using the formula above,
   * during reflow this function does not give accurate data if
   * FinishAndStoreOverflow has been called but mRect hasn't yet been
   * updated yet.  FIXME: This actually isn't true, but it should be.
   *
   * The visual overflow rect should NEVER be used for things that
   * affect layout.  The scrollable overflow rect is permitted to affect
   * layout.
   *
   * @return the rect relative to this frame's origin, but after
   * CSS transforms have been applied (i.e. not really this frame's coordinate
   * system, and may not contain the frame's border-box, e.g. if there
   * is a CSS transform scaling it down)
   */
  nsRect GetVisualOverflowRect() const {
    return GetOverflowRect(eVisualOverflow);
  }

  /**
   * Returns a rect that encompasses the area of this frame that the
   * user should be able to scroll to reach.  This is similar to
   * GetVisualOverflowRect, but does not include outline or shadows, and
   * may in the future include more margins than visual overflow does.
   * It does not include areas clipped out by the CSS "overflow" and
   * "clip" properties.
   *
   * HasOverflowRects() (below) will return true when this overflow
   * rect has been explicitly set, even if it matches mRect.
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
  nsRect GetScrollableOverflowRect() const {
    return GetOverflowRect(eScrollableOverflow);
  }

  nsRect GetOverflowRect(nsOverflowType aType) const;

  nsOverflowAreas GetOverflowAreas() const;

  /**
   * Same as GetScrollableOverflowRect, except relative to the parent
   * frame.
   *
   * @return the rect relative to the parent frame, in the parent frame's
   * coordinate system
   */
  nsRect GetScrollableOverflowRectRelativeToParent() const;

  /**
   * Like GetVisualOverflowRect, except in this frame's
   * coordinate system (before transforms are applied).
   *
   * @return the rect relative to this frame, before any CSS transforms have
   * been applied, i.e. in this frame's coordinate system
   */
  nsRect GetVisualOverflowRectRelativeToSelf() const;

  /**
   * Returns this frame's visual overflow rect as it would be before taking
   * account of SVG effects or transforms. The rect returned is relative to
   * this frame.
   */
  nsRect GetPreEffectsVisualOverflowRect() const;

  /**
   * Store the overflow area in the frame's mOverflow.mVisualDeltas
   * fields or as a frame property in the frame manager so that it can
   * be retrieved later without reflowing the frame. Returns true if either of
   * the overflow areas changed.
   */
  bool FinishAndStoreOverflow(nsOverflowAreas& aOverflowAreas,
                              nsSize aNewSize, nsSize* aOldSize = nullptr);

  bool FinishAndStoreOverflow(nsHTMLReflowMetrics* aMetrics) {
    return FinishAndStoreOverflow(aMetrics->mOverflowAreas,
                                  nsSize(aMetrics->width, aMetrics->height));
  }

  /**
   * Returns whether the frame has an overflow rect that is different from
   * its border-box.
   */
  bool HasOverflowAreas() const {
    return mOverflow.mType != NS_FRAME_OVERFLOW_NONE;
  }

  /**
   * Removes any stored overflow rects (visual and scrollable) from the frame.
   * Returns true if the overflow changed.
   */
  bool ClearOverflowRects();

  /**
   * Determine whether borders should not be painted on certain sides of the
   * frame.
   *
   * @note (See also bug 743402, comment 11) GetSkipSides() and its sister
   *       method, ApplySkipSides() checks to see if this frame has a previous
   *       or next continuation to determine if a side should be skipped.
   *       Unfortunately, this only works after reflow has been completed. In
   *       lieu of this, during reflow, an nsHTMLReflowState parameter can be
   *       passed in, indicating that it should be used to determine if sides
   *       should be skipped during reflow.
   */
  virtual int GetSkipSides(const nsHTMLReflowState* aReflowState = nullptr) const { return 0; }

  /**
   * @returns true if this frame is selected.
   */
  bool IsSelected() const;

  /**
   *  called to discover where this frame, or a parent frame has user-select style
   *  applied, which affects that way that it is selected.
   *    
   *  @param aIsSelectable out param. Set to true if the frame can be selected
   *                       (i.e. is not affected by user-select: none)
   *  @param aSelectStyle  out param. Returns the type of selection style found
   *                        (using values defined in nsStyleConsts.h).
   */
  NS_IMETHOD  IsSelectable(bool* aIsSelectable, uint8_t* aSelectStyle) const = 0;

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
  const nsFrameSelection* GetConstFrameSelection() const;

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
   *  @param aVisual [in] whether bidi caret behavior is visual (true) or logical (false)
   *  @param aJumpLines [in] whether to allow jumping across line boundaries
   *  @param aScrollViewStop [in] whether to stop when reaching a scroll frame boundary
   *  @param aOutFrame [out] the previous/next selectable leaf frame
   *  @param aOutOffset [out] 0 indicates that we arrived at the beginning of the output frame;
   *                          -1 indicates that we arrived at its end.
   *  @param aOutJumpedLine [out] whether this frame and the returned frame are on different lines
   */
  nsresult GetFrameFromDirection(nsDirection aDirection, bool aVisual,
                                 bool aJumpLines, bool aScrollViewStop, 
                                 nsIFrame** aOutFrame, int32_t* aOutOffset, bool* aOutJumpedLine);

  /**
   *  called to see if the children of the frame are visible from indexstart to index end.
   *  this does not change any state. returns true only if the indexes are valid and any of
   *  the children are visible.  for textframes this index is the character index.
   *  if aStart = aEnd result will be false
   *  @param aStart start index of first child from 0-N (number of children)
   *  @param aEnd   end index of last child from 0-N
   *  @param aRecurse should this frame talk to siblings to get to the contents other children?
   *  @param aFinished did this frame have the aEndIndex? or is there more work to do
   *  @param _retval  return value true or false. false = range is not rendered.
   */
  NS_IMETHOD CheckVisibility(nsPresContext* aContext, int32_t aStartIndex, int32_t aEndIndex, bool aRecurse, bool *aFinished, bool *_retval)=0;

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
   * Note: Accessible must be refcountable. Do not implement directly on your frame
   * Use a mediatior of some kind.
   */
#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() = 0;
#endif

  /**
   * Get the frame whose style context should be the parent of this
   * frame's style context (i.e., provide the parent style context).
   * This frame must either be an ancestor of this frame or a child.  If
   * this returns a child frame, then the child frame must be sure to
   * return a grandparent or higher!  Furthermore, if a child frame is
   * returned it must have the same GetContent() as this frame.
   *
   * @return The frame whose style context should be the parent of this frame's
   *         style context.  Null is permitted, and means that this frame's
   *         style context should be the root of the style context tree.
   */
  virtual nsIFrame* GetParentStyleContextFrame() const = 0;

  /**
   * Determines whether a frame is visible for painting;
   * taking into account whether it is painting a selection or printing.
   */
  bool IsVisibleForPainting(nsDisplayListBuilder* aBuilder);
  /**
   * Determines whether a frame is visible for painting or collapsed;
   * taking into account whether it is painting a selection or printing,
   */
  bool IsVisibleOrCollapsedForPainting(nsDisplayListBuilder* aBuilder);
  /**
   * As above, but slower because we have to recompute some stuff that
   * aBuilder already has.
   */
  bool IsVisibleForPainting();
  /**
   * Check whether this frame is visible in the current selection. Returns
   * true if there is no current selection.
   */
  bool IsVisibleInSelection(nsDisplayListBuilder* aBuilder);

  /**
   * Overridable function to determine whether this frame should be considered
   * "in" the given non-null aSelection for visibility purposes.
   */  
  virtual bool IsVisibleInSelection(nsISelection* aSelection);

  /**
   * Determines whether this frame is a pseudo stacking context, looking
   * only as style --- i.e., assuming that it's in-flow and not a replaced
   * element and not an SVG element.
   * XXX maybe check IsTransformed()?
   */
  bool IsPseudoStackingContextFromStyle();

  virtual bool HonorPrintBackgroundSettings() { return true; }

  /**
   * Determine whether the frame is logically empty, which is roughly
   * whether the layout would be the same whether or not the frame is
   * present.  Placeholder frames should return true.  Block frames
   * should be considered empty whenever margins collapse through them,
   * even though those margins are relevant.  Text frames containing
   * only whitespace that does not contribute to the height of the line
   * should return true.
   */
  virtual bool IsEmpty() = 0;
  /**
   * Return the same as IsEmpty(). This may only be called after the frame
   * has been reflowed and before any further style or content changes.
   */
  virtual bool CachedIsEmpty();
  /**
   * Determine whether the frame is logically empty, assuming that all
   * its children are empty.
   */
  virtual bool IsSelfEmpty() = 0;

  /**
   * IsGeneratedContentFrame returns whether a frame corresponds to
   * generated content
   *
   * @return whether the frame correspods to generated content
   */
  bool IsGeneratedContentFrame() {
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
  bool IsPseudoFrame(nsIContent* aParentContent) {
    return mContent == aParentContent;
  }

  FrameProperties Properties() const {
    return FrameProperties(PresContext()->PropertyTable(), this);
  }

  NS_DECLARE_FRAME_PROPERTY(BaseLevelProperty, nullptr)
  NS_DECLARE_FRAME_PROPERTY(EmbeddingLevelProperty, nullptr)
  NS_DECLARE_FRAME_PROPERTY(ParagraphDepthProperty, nullptr)

#define NS_GET_BASE_LEVEL(frame) \
NS_PTR_TO_INT32(frame->Properties().Get(nsIFrame::BaseLevelProperty()))

#define NS_GET_EMBEDDING_LEVEL(frame) \
NS_PTR_TO_INT32(frame->Properties().Get(nsIFrame::EmbeddingLevelProperty()))

#define NS_GET_PARAGRAPH_DEPTH(frame) \
NS_PTR_TO_INT32(frame->Properties().Get(nsIFrame::ParagraphDepthProperty()))

  /**
   * Return true if and only if this frame obeys visibility:hidden.
   * if it does not, then nsContainerFrame will hide its view even though
   * this means children can't be made visible again.
   */
  virtual bool SupportsVisibilityHidden() { return true; }

  /**
   * Returns true if the frame has a valid clip rect set via the 'clip'
   * property, and the 'clip' property applies to this frame. The 'clip'
   * property applies to HTML frames if they are absolutely positioned. The
   * 'clip' property applies to SVG frames regardless of the value of the
   * 'position' property.
   *
   * If this method returns true, then we also set aRect to the computed clip
   * rect, with coordinates relative to this frame's origin. aRect must not be
   * null!
   */
  bool GetClipPropClipRect(const nsStyleDisplay* aDisp, nsRect* aRect,
                           const nsSize& aSize) const;

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
  virtual bool IsFocusable(int32_t *aTabIndex = nullptr, bool aWithMouse = false);

  // BOX LAYOUT METHODS
  // These methods have been migrated from nsIBox and are in the process of
  // being refactored. DO NOT USE OUTSIDE OF XUL.
  bool IsBoxFrame() const
  {
    return IsFrameOfType(nsIFrame::eXULBox);
  }
  bool IsBoxWrapped() const
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
  uint32_t GetOrdinal();

  virtual nscoord GetFlex(nsBoxLayoutState& aBoxLayoutState) = 0;
  virtual nscoord GetBoxAscent(nsBoxLayoutState& aBoxLayoutState) = 0;
  virtual bool IsCollapsed() = 0;
  // This does not alter the overflow area. If the caller is changing
  // the box size, the caller is responsible for updating the overflow
  // area. It's enough to just call Layout or SyncLayout on the
  // box. You can pass true to aRemoveOverflowArea as a
  // convenience.
  virtual void SetBounds(nsBoxLayoutState& aBoxLayoutState, const nsRect& aRect,
                         bool aRemoveOverflowAreas = false) = 0;
  NS_HIDDEN_(nsresult) Layout(nsBoxLayoutState& aBoxLayoutState);
  nsIFrame* GetChildBox() const
  {
    // box layout ends at box-wrapped frames, so don't allow these frames
    // to report child boxes.
    return IsBoxFrame() ? GetFirstPrincipalChild() : nullptr;
  }
  nsIFrame* GetNextBox() const
  {
    return (mParent && mParent->IsBoxFrame()) ? mNextSibling : nullptr;
  }
  nsIFrame* GetParentBox() const
  {
    return (mParent && mParent->IsBoxFrame()) ? mParent : nullptr;
  }
  // Box methods.  Note that these do NOT just get the CSS border, padding,
  // etc.  They also talk to nsITheme.
  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);
  NS_IMETHOD GetBorder(nsMargin& aBorder)=0;
  NS_IMETHOD GetPadding(nsMargin& aBorderAndPadding)=0;
  NS_IMETHOD GetMargin(nsMargin& aMargin)=0;
  virtual void SetLayoutManager(nsBoxLayout* aLayout) { }
  virtual nsBoxLayout* GetLayoutManager() { return nullptr; }
  NS_HIDDEN_(nsresult) GetClientRect(nsRect& aContentRect);

  // For nsSprocketLayout
  virtual Valignment GetVAlign() const = 0;
  virtual Halignment GetHAlign() const = 0;

  bool IsHorizontal() const { return (mState & NS_STATE_IS_HORIZONTAL) != 0; }
  bool IsNormalDirection() const { return (mState & NS_STATE_IS_DIRECTION_NORMAL) != 0; }

  NS_HIDDEN_(nsresult) Redraw(nsBoxLayoutState& aState);
  NS_IMETHOD RelayoutChildAtOrdinal(nsBoxLayoutState& aState, nsIFrame* aChild)=0;
  // XXX take this out after we've branched
  virtual bool GetMouseThrough() const { return false; }

#ifdef DEBUG_LAYOUT
  NS_IMETHOD SetDebug(nsBoxLayoutState& aState, bool aDebug)=0;
  NS_IMETHOD GetDebug(bool& aDebug)=0;

  NS_IMETHOD DumpBox(FILE* out)=0;
#endif

  /**
   * @return true if this text frame ends with a newline character.  It
   * should return false if this is not a text frame.
   */
  virtual bool HasTerminalNewline() const;

  static bool AddCSSPrefSize(nsIFrame* aBox, nsSize& aSize, bool& aWidth, bool& aHeightSet);
  static bool AddCSSMinSize(nsBoxLayoutState& aState, nsIFrame* aBox,
                            nsSize& aSize, bool& aWidth, bool& aHeightSet);
  static bool AddCSSMaxSize(nsIFrame* aBox, nsSize& aSize, bool& aWidth, bool& aHeightSet);
  static bool AddCSSFlex(nsBoxLayoutState& aState, nsIFrame* aBox, nscoord& aFlex);

  // END OF BOX LAYOUT METHODS
  // The above methods have been migrated from nsIBox and are in the process of
  // being refactored. DO NOT USE OUTSIDE OF XUL.

  struct CaretPosition {
    CaretPosition();
    ~CaretPosition();

    nsCOMPtr<nsIContent> mResultContent;
    int32_t              mContentOffset;
  };

  /**
   * gets the first or last possible caret position within the frame
   *
   * @param  [in] aStart
   *         true  for getting the first possible caret position
   *         false for getting the last possible caret position
   * @return The caret position in a CaretPosition.
   *         the returned value is a 'best effort' in case errors
   *         are encountered rummaging through the frame.
   */
  CaretPosition GetExtremeCaretPosition(bool aStart);

  /**
   * Get a line iterator for this frame, if supported.
   *
   * @return nullptr if no line iterator is supported.
   * @note dispose the line iterator using nsILineIterator::DisposeLineIterator
   */
  virtual nsILineIterator* GetLineIterator() = 0;

  /**
   * If this frame is a next-in-flow, and its prev-in-flow has something on its
   * overflow list, pull those frames into the child list of this one.
   */
  virtual void PullOverflowsFromPrevInFlow() {}

  /**
   * Clear the list of child PresShells generated during the last paint
   * so that we can begin generating a new one.
   */  
  void ClearPresShellsFromLastPaint() { 
    PaintedPresShellList()->Clear(); 
  }
  
  /**
   * Flag a child PresShell as painted so that it will get its paint count
   * incremented during empty transactions.
   */  
  void AddPaintedPresShell(nsIPresShell* shell) { 
    PaintedPresShellList()->AppendElement(do_GetWeakReference(shell)); 
  }
  
  /**
   * Increment the paint count of all child PresShells that were painted during
   * the last repaint.
   */  
  void UpdatePaintCountForPaintedPresShells() {
    nsTArray<nsWeakPtr> * list = PaintedPresShellList();
    for (int i = 0, l = list->Length(); i < l; i++) {
      nsCOMPtr<nsIPresShell> shell = do_QueryReferent(list->ElementAt(i));
      
      if (shell) {
        shell->IncrementPaintCount();
      }
    }
  }  

  /**
   * Accessors for the absolute containing block.
   */
  bool IsAbsoluteContainer() const { return !!(mState & NS_FRAME_HAS_ABSPOS_CHILDREN); }
  bool HasAbsolutelyPositionedChildren() const;
  nsAbsoluteContainingBlock* GetAbsoluteContainingBlock() const;
  void MarkAsAbsoluteContainingBlock();
  void MarkAsNotAbsoluteContainingBlock();
  // Child frame types override this function to select their own child list name
  virtual mozilla::layout::FrameChildListID GetAbsoluteListID() const { return kAbsoluteList; }

  // Checks if we (or any of our descendents) have NS_FRAME_PAINTED_THEBES set, and
  // clears this bit if so.
  bool CheckAndClearPaintedState();

  // CSS visibility just doesn't cut it because it doesn't inherit through
  // documents. Also if this frame is in a hidden card of a deck then it isn't
  // visible either and that isn't expressed using CSS visibility. Also if it
  // is in a hidden view (there are a few cases left and they are hopefully
  // going away soon).
  // If the VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY flag is passed then we
  // ignore the chrome/content boundary, otherwise we stop looking when we
  // reach it.
  enum {
    VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY = 0x01
  };
  bool IsVisibleConsideringAncestors(uint32_t aFlags = 0) const;

  struct FrameWithDistance
  {
    nsIFrame* mFrame;
    nscoord mXDistance;
    nscoord mYDistance;
  };

  /**
   * Finds a frame that is closer to a specified point than a current
   * distance.  Distance is measured as for text selection -- a closer x
   * distance beats a closer y distance.
   *
   * Normally, this function will only check the distance between this
   * frame's rectangle and the specified point.  nsSVGTextFrame2 overrides
   * this so that it can manage all of its descendant frames and take
   * into account any SVG text layout.
   *
   * If aPoint is closer to this frame's rectangle than aCurrentBestFrame
   * indicates, then aCurrentBestFrame is updated with the distance between
   * aPoint and this frame's rectangle, and with a pointer to this frame.
   * If aPoint is not closer, then aCurrentBestFrame is left unchanged.
   *
   * @param aPoint The point to check for its distance to this frame.
   * @param aCurrentBestFrame Pointer to a struct that will be updated with
   *   a pointer to this frame and its distance to aPoint, if this frame
   *   is indeed closer than the current distance in aCurrentBestFrame.
   */
  virtual void FindCloserFrameForSelection(nsPoint aPoint,
                                           FrameWithDistance* aCurrentBestFrame);

  inline bool IsBlockInside() const;
  inline bool IsBlockOutside() const;
  inline bool IsInlineOutside() const;
  inline uint8_t GetDisplay() const;
  inline bool IsFloating() const;
  inline bool IsPositioned() const;
  inline bool IsRelativelyPositioned() const;
  inline bool IsAbsolutelyPositioned() const;

  /**
   * Returns the vertical-align value to be used for layout, if it is one
   * of the enumerated values.  If this is an SVG text frame, it returns a value
   * that corresponds to the value of dominant-baseline.  If the
   * vertical-align property has length or percentage value, this returns
   * eInvalidVerticalAlign.
   */
  uint8_t VerticalAlignEnum() const;
  enum { eInvalidVerticalAlign = 0xFF };

  bool IsSVGText() const { return mState & NS_FRAME_IS_SVG_TEXT; }

  void CreateOwnLayerIfNeeded(nsDisplayListBuilder* aBuilder, nsDisplayList* aList);

  /**
   * Adds the NS_FRAME_IN_POPUP state bit to aFrame, and
   * all descendant frames (including cross-doc ones).
   */
  static void AddInPopupStateBitToDescendants(nsIFrame* aFrame);
  /**
   * Removes the NS_FRAME_IN_POPUP state bit from aFrame and
   * all descendant frames (including cross-doc ones), unless
   * the frame is a popup itself.
   */
  static void RemoveInPopupStateBitFromDescendants(nsIFrame* aFrame);

  /**
   * Sorts the given nsFrameList, so that for every two adjacent frames in the
   * list, the former is less than or equal to the latter, according to the
   * templated IsLessThanOrEqual method.
   *
   * Note: this method uses a stable merge-sort algorithm.
   */
  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static void SortFrameList(nsFrameList& aFrameList);

  /**
   * Returns true if the given frame list is already sorted, according to the
   * templated IsLessThanOrEqual function.
   */
  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static bool IsFrameListSorted(nsFrameList& aFrameList);

  /**
   * Return true if aFrame is in an {ib} split and is NOT one of the
   * continuations of the first inline in it.
   */
  bool FrameIsNonFirstInIBSplit() const {
    return (GetStateBits() & NS_FRAME_IS_SPECIAL) &&
      FirstContinuation()->Properties().Get(nsIFrame::IBSplitSpecialPrevSibling());
  }

  /**
   * Return true if aFrame is in an {ib} split and is NOT one of the
   * continuations of the last inline in it.
   */
  bool FrameIsNonLastInIBSplit() const {
    return (GetStateBits() & NS_FRAME_IS_SPECIAL) &&
      FirstContinuation()->Properties().Get(nsIFrame::IBSplitSpecialSibling());
  }

  /**
   * Return whether this is a frame whose width is used when computing
   * the font size inflation of its descendants.
   */
  bool IsContainerForFontSizeInflation() const {
    return GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER;
  }

protected:
  // Members
  nsRect           mRect;
  nsIContent*      mContent;
  nsStyleContext*  mStyleContext;
  nsIFrame*        mParent;
private:
  nsIFrame*        mNextSibling;  // doubly-linked list of frames
  nsIFrame*        mPrevSibling;  // Do not touch outside SetNextSibling!

  void MarkAbsoluteFramesForDisplayList(nsDisplayListBuilder* aBuilder, const nsRect& aDirtyRect);

  static void DestroyPaintedPresShellList(void* propertyValue) {
    nsTArray<nsWeakPtr>* list = static_cast<nsTArray<nsWeakPtr>*>(propertyValue);
    list->Clear();
    delete list;
  }

  // Stores weak references to all the PresShells that were painted during
  // the last paint event so that we can increment their paint count during
  // empty transactions
  NS_DECLARE_FRAME_PROPERTY(PaintedPresShellsProperty, DestroyPaintedPresShellList)
  
  nsTArray<nsWeakPtr>* PaintedPresShellList() {
    nsTArray<nsWeakPtr>* list = static_cast<nsTArray<nsWeakPtr>*>(
      Properties().Get(PaintedPresShellsProperty())
    );
    
    if (!list) {
      list = new nsTArray<nsWeakPtr>();
      Properties().Set(PaintedPresShellsProperty(), list);
    }
    
    return list;
  }

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
  struct VisualDeltas {
    uint8_t mLeft;
    uint8_t mTop;
    uint8_t mRight;
    uint8_t mBottom;
    bool operator==(const VisualDeltas& aOther) const
    {
      return mLeft == aOther.mLeft && mTop == aOther.mTop &&
             mRight == aOther.mRight && mBottom == aOther.mBottom;
    }
    bool operator!=(const VisualDeltas& aOther) const
    {
      return !(*this == aOther);
    }
  };
  union {
    uint32_t     mType;
    VisualDeltas mVisualDeltas;
  } mOverflow;

  // Helpers
  /**
   * Can we stop inside this frame when we're skipping non-rendered whitespace?
   * @param  aForward [in] Are we moving forward (or backward) in content order.
   * @param  aOffset [in/out] At what offset into the frame to start looking.
   *         on output - what offset was reached (whether or not we found a place to stop).
   * @return true: An appropriate offset was found within this frame,
   *         and is given by aOffset.
   *         false: Not found within this frame, need to try the next frame.
   */
  virtual bool PeekOffsetNoAmount(bool aForward, int32_t* aOffset) = 0;
  
  /**
   * Search the frame for the next character
   * @param  aForward [in] Are we moving forward (or backward) in content order.
   * @param  aOffset [in/out] At what offset into the frame to start looking.
   *         on output - what offset was reached (whether or not we found a place to stop).
   * @param  aRespectClusters [in] Whether to restrict result to valid cursor locations
   *         (between grapheme clusters) - default TRUE maintains "normal" behavior,
   *         FALSE is used for selection by "code unit" (instead of "character")
   * @return true: An appropriate offset was found within this frame,
   *         and is given by aOffset.
   *         false: Not found within this frame, need to try the next frame.
   */
  virtual bool PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                     bool aRespectClusters = true) = 0;
  
  /**
   * Search the frame for the next word boundary
   * @param  aForward [in] Are we moving forward (or backward) in content order.
   * @param  aWordSelectEatSpace [in] true: look for non-whitespace following
   *         whitespace (in the direction of movement).
   *         false: look for whitespace following non-whitespace (in the
   *         direction  of movement).
   * @param  aIsKeyboardSelect [in] Was the action initiated by a keyboard operation?
   *         If true, punctuation immediately following a word is considered part
   *         of that word. Otherwise, a sequence of punctuation is always considered
   *         as a word on its own.
   * @param  aOffset [in/out] At what offset into the frame to start looking.
   *         on output - what offset was reached (whether or not we found a place to stop).
   * @param  aState [in/out] the state that is carried from frame to frame
   * @return true: An appropriate offset was found within this frame,
   *         and is given by aOffset.
   *         false: Not found within this frame, need to try the next frame.
   */
  struct PeekWordState {
    // true when we're still at the start of the search, i.e., we can't return
    // this point as a valid offset!
    bool mAtStart;
    // true when we've encountered at least one character of the pre-boundary type
    // (whitespace if aWordSelectEatSpace is true, non-whitespace otherwise)
    bool mSawBeforeType;
    // true when the last character encountered was punctuation
    bool mLastCharWasPunctuation;
    // true when the last character encountered was whitespace
    bool mLastCharWasWhitespace;
    // true when we've seen non-punctuation since the last whitespace
    bool mSeenNonPunctuationSinceWhitespace;
    // text that's *before* the current frame when aForward is true, *after*
    // the current frame when aForward is false. Only includes the text
    // on the current line.
    nsAutoString mContext;

    PeekWordState() : mAtStart(true), mSawBeforeType(false),
        mLastCharWasPunctuation(false), mLastCharWasWhitespace(false),
        mSeenNonPunctuationSinceWhitespace(false) {}
    void SetSawBeforeType() { mSawBeforeType = true; }
    void Update(bool aAfterPunctuation, bool aAfterWhitespace) {
      mLastCharWasPunctuation = aAfterPunctuation;
      mLastCharWasWhitespace = aAfterWhitespace;
      if (aAfterWhitespace) {
        mSeenNonPunctuationSinceWhitespace = false;
      } else if (!aAfterPunctuation) {
        mSeenNonPunctuationSinceWhitespace = true;
      }
      mAtStart = false;
    }
  };
  virtual bool PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                                int32_t* aOffset, PeekWordState* aState) = 0;

  /**
   * Search for the first paragraph boundary before or after the given position
   * @param  aPos See description in nsFrameSelection.h. The following fields are
   *              used by this method: 
   *              Input: mDirection
   *              Output: mResultContent, mContentOffset
   */
  nsresult PeekOffsetParagraph(nsPeekOffsetStruct *aPos);

private:
  nsOverflowAreas* GetOverflowAreasProperty();
  nsRect GetVisualOverflowFromDeltas() const {
    NS_ABORT_IF_FALSE(mOverflow.mType != NS_FRAME_OVERFLOW_LARGE,
                      "should not be called when overflow is in a property");
    // Calculate the rect using deltas from the frame's border rect.
    // Note that the mOverflow.mDeltas fields are unsigned, but we will often
    // need to return negative values for the left and top, so take care
    // to cast away the unsigned-ness.
    return nsRect(-(int32_t)mOverflow.mVisualDeltas.mLeft,
                  -(int32_t)mOverflow.mVisualDeltas.mTop,
                  mRect.width + mOverflow.mVisualDeltas.mRight +
                                mOverflow.mVisualDeltas.mLeft,
                  mRect.height + mOverflow.mVisualDeltas.mBottom +
                                 mOverflow.mVisualDeltas.mTop);
  }
  /**
   * Returns true if any overflow changed.
   */
  bool SetOverflowAreas(const nsOverflowAreas& aOverflowAreas);
  nsPoint GetOffsetToCrossDoc(const nsIFrame* aOther, const int32_t aAPD) const;

  // Helper-functions for SortFrameList():
  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static nsIFrame* SortedMerge(nsIFrame *aLeft, nsIFrame *aRight);

  template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
  static nsIFrame* MergeSort(nsIFrame *aSource);

#ifdef DEBUG
public:
  static void IndentBy(FILE* out, int32_t aIndent) {
    while (--aIndent >= 0) fputs("  ", out);
  }
  void ListTag(FILE* out) const {
    ListTag(out, this);
  }
  static void ListTag(FILE* out, const nsIFrame* aFrame) {
    nsAutoString tmp;
    aFrame->GetFrameName(tmp);
    fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
    fprintf(out, "@%p", static_cast<const void*>(aFrame));
  }
  void ListGeneric(FILE* out, int32_t aIndent, uint32_t aFlags) const;
  enum {
    TRAVERSE_SUBDOCUMENT_FRAMES = 0x01
  };
  virtual void List(FILE* out, int32_t aIndent, uint32_t aFlags = 0) const;
  /**
   * lists the frames beginning from the root frame
   * - calls root frame's List(...)
   */
  static void RootFrameList(nsPresContext* aPresContext,
                            FILE* out, int32_t aIndent);
  virtual void DumpFrameTree();

  NS_IMETHOD  GetFrameName(nsAString& aResult) const = 0;
  NS_IMETHOD_(nsFrameState)  GetDebugStateBits() const = 0;
  NS_IMETHOD  DumpRegressionData(nsPresContext* aPresContext,
                                 FILE* out, int32_t aIndent) = 0;
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
  nsWeakFrame() : mPrev(nullptr), mFrame(nullptr) { }

  nsWeakFrame(const nsWeakFrame& aOther) : mPrev(nullptr), mFrame(nullptr)
  {
    Init(aOther.GetFrame());
  }

  nsWeakFrame(nsIFrame* aFrame) : mPrev(nullptr), mFrame(nullptr)
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
    mFrame = nullptr;
    mPrev = nullptr;
  }

  bool IsAlive() { return !!mFrame; }

  nsIFrame* GetFrame() const { return mFrame; }

  nsWeakFrame* GetPreviousWeakFrame() { return mPrev; }

  void SetPreviousWeakFrame(nsWeakFrame* aPrev) { mPrev = aPrev; }

  ~nsWeakFrame()
  {
    Clear(mFrame ? mFrame->PresContext()->GetPresShell() : nullptr);
  }
private:
  void Init(nsIFrame* aFrame);

  nsWeakFrame*  mPrev;
  nsIFrame*     mFrame;
};

inline bool
nsFrameList::ContinueRemoveFrame(nsIFrame* aFrame)
{
  MOZ_ASSERT(!aFrame->GetPrevSibling() || !aFrame->GetNextSibling(),
             "Forgot to call StartRemoveFrame?");
  if (aFrame == mLastChild) {
    MOZ_ASSERT(!aFrame->GetNextSibling(), "broken frame list");
    nsIFrame* prevSibling = aFrame->GetPrevSibling();
    if (!prevSibling) {
      MOZ_ASSERT(aFrame == mFirstChild, "broken frame list");
      mFirstChild = mLastChild = nullptr;
      return true;
    }
    MOZ_ASSERT(prevSibling->GetNextSibling() == aFrame, "Broken frame linkage");
    prevSibling->SetNextSibling(nullptr);
    mLastChild = prevSibling;
    return true;
  }
  if (aFrame == mFirstChild) {
    MOZ_ASSERT(!aFrame->GetPrevSibling(), "broken frame list");
    mFirstChild = aFrame->GetNextSibling();
    aFrame->SetNextSibling(nullptr);
    MOZ_ASSERT(mFirstChild, "broken frame list");
    return true;
  }
  return false;
}

inline bool
nsFrameList::StartRemoveFrame(nsIFrame* aFrame)
{
  if (aFrame->GetPrevSibling() && aFrame->GetNextSibling()) {
    UnhookFrameFromSiblings(aFrame);
    return true;
  }
  return ContinueRemoveFrame(aFrame);
}

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

inline void
nsFrameList::FrameLinkEnumerator::Next()
{
  mPrev = mFrame;
  Enumerator::Next();
}

// Helper-functions for nsIFrame::SortFrameList()
// ---------------------------------------------------

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ nsIFrame*
nsIFrame::SortedMerge(nsIFrame *aLeft, nsIFrame *aRight)
{
  NS_PRECONDITION(aLeft && aRight, "SortedMerge must have non-empty lists");

  nsIFrame *result;
  // Unroll first iteration to avoid null-check 'result' inside the loop.
  if (IsLessThanOrEqual(aLeft, aRight)) {
    result = aLeft;
    aLeft = aLeft->GetNextSibling();
    if (!aLeft) {
      result->SetNextSibling(aRight);
      return result;
    }
  }
  else {
    result = aRight;
    aRight = aRight->GetNextSibling();
    if (!aRight) {
      result->SetNextSibling(aLeft);
      return result;
    }
  }

  nsIFrame *last = result;
  for (;;) {
    if (IsLessThanOrEqual(aLeft, aRight)) {
      last->SetNextSibling(aLeft);
      last = aLeft;
      aLeft = aLeft->GetNextSibling();
      if (!aLeft) {
        last->SetNextSibling(aRight);
        return result;
      }
    }
    else {
      last->SetNextSibling(aRight);
      last = aRight;
      aRight = aRight->GetNextSibling();
      if (!aRight) {
        last->SetNextSibling(aLeft);
        return result;
      }
    }
  }
}

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ nsIFrame*
nsIFrame::MergeSort(nsIFrame *aSource)
{
  NS_PRECONDITION(aSource, "MergeSort null arg");

  nsIFrame *sorted[32] = { nullptr };
  nsIFrame **fill = &sorted[0];
  nsIFrame **left;
  nsIFrame *rest = aSource;

  do {
    nsIFrame *current = rest;
    rest = rest->GetNextSibling();
    current->SetNextSibling(nullptr);

    // Merge it with sorted[0] if present; then merge the result with sorted[1] etc.
    // sorted[0] is a list of length 1 (or nullptr).
    // sorted[1] is a list of length 2 (or nullptr).
    // sorted[2] is a list of length 4 (or nullptr). etc.
    for (left = &sorted[0]; left != fill && *left; ++left) {
      current = SortedMerge<IsLessThanOrEqual>(*left, current);
      *left = nullptr;
    }

    // Fill the empty slot that we couldn't merge with the last result.
    *left = current;

    if (left == fill)
      ++fill;
  } while (rest);

  // Collect and merge the results.
  nsIFrame *result = nullptr;
  for (left = &sorted[0]; left != fill; ++left) {
    if (*left) {
      result = result ? SortedMerge<IsLessThanOrEqual>(*left, result) : *left;
    }
  }
  return result;
}

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ void
nsIFrame::SortFrameList(nsFrameList& aFrameList)
{
  nsIFrame* head = MergeSort<IsLessThanOrEqual>(aFrameList.FirstChild());
  aFrameList = nsFrameList(head, nsLayoutUtils::GetLastSibling(head));
  MOZ_ASSERT(IsFrameListSorted<IsLessThanOrEqual>(aFrameList),
             "After we sort a frame list, it should be in sorted order...");
}

template<bool IsLessThanOrEqual(nsIFrame*, nsIFrame*)>
/* static */ bool
nsIFrame::IsFrameListSorted(nsFrameList& aFrameList)
{
  if (aFrameList.IsEmpty()) {
    // empty lists are trivially sorted.
    return true;
  }

  // We'll walk through the list with two iterators, one trailing behind the
  // other. The list is sorted IFF trailingIter <= iter, across the whole list.
  nsFrameList::Enumerator trailingIter(aFrameList);
  nsFrameList::Enumerator iter(aFrameList);
  iter.Next(); // Skip |iter| past first frame. (List is nonempty, so we can.)

  // Now, advance the iterators in parallel, comparing each adjacent pair.
  while (!iter.AtEnd()) {
    MOZ_ASSERT(!trailingIter.AtEnd(), "trailing iter shouldn't finish first");
    if (!IsLessThanOrEqual(trailingIter.get(), iter.get())) {
      return false;
    }
    trailingIter.Next();
    iter.Next();
  }

  // We made it to the end without returning early, so the list is sorted.
  return true;
}

#endif /* nsIFrame_h___ */
