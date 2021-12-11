/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of all frame state bits, for preprocessing */

/******

  This file contains definitions for frame state bits -- values used
  in nsIFrame::mState -- and groups of frame state bits and which
  classes they apply to.

  There are three macros that can be defined before #including this
  file:

  FRAME_STATE_GROUP_NAME(name_)

    This denotes the existence of a named group of frame state bits.

    The main group of frame state bits is named "Generic" and is
    defined to apply to nsIFrame, i.e. all frames.  All of the global
    frame state bits -- bits 0..19 and 32..59 -- are in this group.

  FRAME_STATE_GROUP_CLASS(group_, class_)
    class_ is the name of a frame class that uses the frame state bits
    that are a part of the group.

  FRAME_STATE_BIT(group_, value_, name_)

    This denotes the existence of a frame state bit.  group_ indicates
    which group the bit belongs to, value_ is the bit number (0..63),
    and name_ is the name of the frame state bit constant.

  Note that if you add a new frame state group, you'll need to #include
  the header for its frame classes in nsFrameState.cpp and, if they don't
  already, add nsQueryFrame implementations (which can be DEBUG-only) to
  the frame classes.

 ******/

#ifndef FRAME_STATE_GROUP_NAME
#define FRAME_STATE_GROUP_NAME(name_) /* nothing */
#define DEFINED_FRAME_STATE_GROUP_NAME
#endif

#ifndef FRAME_STATE_GROUP_CLASS
#define FRAME_STATE_GROUP_CLASS(name_, class_) /* nothing */
#define DEFINED_FRAME_STATE_GROUP_CLASS
#endif

#ifndef FRAME_STATE_BIT
#define FRAME_STATE_BIT(group_, value_, name_) /* nothing */
#define DEFINED_FRAME_STATE_BIT
#endif

// Helper macro for the common case of a single class
#define FRAME_STATE_GROUP(name_, class_) \
FRAME_STATE_GROUP_NAME(name_)            \
FRAME_STATE_GROUP_CLASS(name_, class_)

// == Frame state bits that apply to all frames ===============================

FRAME_STATE_GROUP(Generic, nsIFrame)

// This bit is set when the frame is actively being reflowed. It is set in many
// frames' Reflow() by calling MarkInReflow() and unset in DidReflow().
FRAME_STATE_BIT(Generic, 0, NS_FRAME_IN_REFLOW)

// This bit is set when a frame is created. After it has been reflowed
// once (during the DidReflow with a finished state) the bit is
// cleared.
FRAME_STATE_BIT(Generic, 1, NS_FRAME_FIRST_REFLOW)

// For a continuation frame, if this bit is set, then this a "fluid"
// continuation, i.e., across a line boundary. Otherwise it's a "hard"
// continuation, e.g. a bidi continuation.
FRAME_STATE_BIT(Generic, 2, NS_FRAME_IS_FLUID_CONTINUATION)

// Free bit here.

// If this bit is set, then a reference to the frame is being held
// elsewhere.  The frame may want to send a notification when it is
// destroyed to allow these references to be cleared.
FRAME_STATE_BIT(Generic, 4, NS_FRAME_EXTERNAL_REFERENCE)

// If this bit is set, this frame or one of its descendants has a
// percentage block-size that depends on an ancestor of this frame.
// (Or it did at one point in the past, since we don't necessarily clear
// the bit when it's no longer needed; it's an optimization.)
FRAME_STATE_BIT(Generic, 5, NS_FRAME_CONTAINS_RELATIVE_BSIZE)

// If this bit is set, then the frame corresponds to generated content
FRAME_STATE_BIT(Generic, 6, NS_FRAME_GENERATED_CONTENT)

// If this bit is set the frame is a continuation that is holding overflow,
// i.e. it is a next-in-flow created to hold overflow after the box's
// height has ended. This means the frame should be a) at the top of the
// page and b) invisible: no borders, zero height, ignored in margin
// collapsing, etc. See nsContainerFrame.h
FRAME_STATE_BIT(Generic, 7, NS_FRAME_IS_OVERFLOW_CONTAINER)

// If this bit is set, then the frame has been moved out of the flow,
// e.g., it is absolutely positioned or floated
FRAME_STATE_BIT(Generic, 8, NS_FRAME_OUT_OF_FLOW)

// Frame can be an abs/fixed pos. container, if its style says so.
// MarkAs[Not]AbsoluteContainingBlock will assert that this bit is set.
// NS_FRAME_HAS_ABSPOS_CHILDREN must not be set when this bit is unset.
FRAME_STATE_BIT(Generic, 9, NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN)

// If this bit is set, then the frame and _all_ of its descendant frames need
// to be reflowed.
// This bit is set when the frame is first created.
// This bit is cleared by DidReflow after the required call to Reflow has
// finished.
// Do not set this bit yourself if you plan to pass the frame to
// PresShell::FrameNeedsReflow.  Pass the right arguments instead.
FRAME_STATE_BIT(Generic, 10, NS_FRAME_IS_DIRTY)

// If this bit is set then the frame is too deep in the frame tree, and
// we'll stop updating it and its children, to prevent stack overflow
// and the like.
FRAME_STATE_BIT(Generic, 11, NS_FRAME_TOO_DEEP_IN_FRAME_TREE)

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
// nsIFrame::Reflow for more.
// This bit is cleared by DidReflow after the required call to Reflow has
// finished.
// Do not set this bit yourself if you plan to pass the frame to
// PresShell::FrameNeedsReflow.  Pass the right arguments instead.
FRAME_STATE_BIT(Generic, 12, NS_FRAME_HAS_DIRTY_CHILDREN)

// If this bit is set, the frame has an associated view
FRAME_STATE_BIT(Generic, 13, NS_FRAME_HAS_VIEW)

// If this bit is set, the frame was created from anonymous content.
FRAME_STATE_BIT(Generic, 14, NS_FRAME_INDEPENDENT_SELECTION)

// If this bit is set, the frame is part of the mangled frame hierarchy
// that results when an inline has been split because of a nested block.
// See the comments in nsCSSFrameConstructor::ConstructInline for
// more details.  (this is only set on nsBlockFrame/nsInlineFrame frames)
FRAME_STATE_BIT(Generic, 15, NS_FRAME_PART_OF_IBSPLIT)

// If this bit is set, then transforms (e.g. CSS or SVG transforms) are allowed
// to affect the frame, and a transform may currently be in affect. If this bit
// is not set, then any transforms on the frame will be ignored.
// This is used primarily in GetTransformMatrix to optimize for the
// common case.
FRAME_STATE_BIT(Generic, 16, NS_FRAME_MAY_BE_TRANSFORMED)

// If this bit is set, the frame itself is a bidi continuation,
// or is incomplete (its next sibling is a bidi continuation)
FRAME_STATE_BIT(Generic, 17, NS_FRAME_IS_BIDI)

// If this bit is set the frame has descendant with a view
FRAME_STATE_BIT(Generic, 18, NS_FRAME_HAS_CHILD_WITH_VIEW)

// If this bit is set, then reflow may be dispatched from the current
// frame instead of the root frame.
FRAME_STATE_BIT(Generic, 19, NS_FRAME_REFLOW_ROOT)

// NOTE: Bits 20-31 and 60-63 of the frame state are reserved for specific
// frame classes.

// This bit is set on floats whose parent does not contain their
// placeholder.  This can happen for two reasons:  (1) the float was
// split, and this piece is the continuation, or (2) the entire float
// didn't fit on the page.
// Note that this bit is also shared by text frames for
// TEXT_IS_IN_TOKEN_MATHML.  That's OK because we only check the
// NS_FRAME_IS_PUSHED_FLOAT bit on frames which we already know are
// out-of-flow.
FRAME_STATE_BIT(Generic, 32, NS_FRAME_IS_PUSHED_FLOAT)

// This bit acts as a loop flag for recursive paint server drawing.
FRAME_STATE_BIT(Generic, 33, NS_FRAME_DRAWING_AS_PAINTSERVER)

// Intrinsic ISize depending on the frame's BSize is rare but possible.
// This flag indicates that the frame has (or once had) a descendant in that
// situation (possibly the frame itself).
FRAME_STATE_BIT(Generic, 34,
                NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE)

// A flag that tells us we can take the common path with respect to style
// properties for this frame when building event regions. This flag is cleared
// when any styles are changed and then we recompute it on the next build
// of the event regions.
FRAME_STATE_BIT(Generic, 35, NS_FRAME_SIMPLE_EVENT_REGIONS)

// Frame is a display root and the retained layer tree needs to be updated
// at the next paint via display list construction.
// Only meaningful for display roots, so we don't really need a global state
// bit; we could free up this bit with a little extra complexity.
FRAME_STATE_BIT(Generic, 36, NS_FRAME_UPDATE_LAYER_TREE)

// Frame can accept absolutely positioned children.
FRAME_STATE_BIT(Generic, 37, NS_FRAME_HAS_ABSPOS_CHILDREN)

// A display item for this frame has been painted as part of a PaintedLayer.
FRAME_STATE_BIT(Generic, 38, NS_FRAME_PAINTED_THEBES)

// Frame is or is a descendant of something with a fixed block-size, unless
// that ancestor is a body or html element, and has no closer ancestor that is
// overflow:auto or overflow:scroll.
FRAME_STATE_BIT(Generic, 39, NS_FRAME_IN_CONSTRAINED_BSIZE)

// This is only set during painting
FRAME_STATE_BIT(Generic, 40, NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)

// Is this frame a container for font size inflation, i.e., is it a
// frame whose width is used to determine the inflation factor for
// everything whose nearest ancestor container for this frame?
FRAME_STATE_BIT(Generic, 41, NS_FRAME_FONT_INFLATION_CONTAINER)

// Does this frame manage a region in which we do font size inflation,
// i.e., roughly, is it an element establishing a new block formatting
// context?
FRAME_STATE_BIT(Generic, 42, NS_FRAME_FONT_INFLATION_FLOW_ROOT)

// This bit is set on SVG frames that are laid out using SVG's coordinate
// system based layout (as opposed to any of the CSS layout models). Note that
// this does not include SVGOuterSVGFrame since it takes part in CSS layout.
FRAME_STATE_BIT(Generic, 43, NS_FRAME_SVG_LAYOUT)

// This bit is set if a frame has a multi-column ancestor (i.e.
// ColumnSetWrapperFrame) within the same block formatting context. A top-level
// ColumnSetWrapperFrame doesn't have this bit set, whereas a
// ColumnSetWrapperFrame nested inside a column does have this bit set.
//
// All the children of the column-spanners or any other type of frames which
// create their own block formatting context do not have this bit set because
// they are not in the same block formatting context created by a multi-column
// ancestor.
FRAME_STATE_BIT(Generic, 44, NS_FRAME_HAS_MULTI_COLUMN_ANCESTOR)

// If this bit is set, then reflow may be dispatched from the current
// frame instead of the root frame.
FRAME_STATE_BIT(Generic, 45, NS_FRAME_DYNAMIC_REFLOW_ROOT)

// This bit indicates that we're tracking visibility for this frame, and that
// the frame has a VisibilityStateProperty property.
FRAME_STATE_BIT(Generic, 46, NS_FRAME_VISIBILITY_IS_TRACKED)

// The frame is a descendant of SVGTextFrame and is thus used for SVG
// text layout.
FRAME_STATE_BIT(Generic, 47, NS_FRAME_IS_SVG_TEXT)

// Frame is marked as needing painting
FRAME_STATE_BIT(Generic, 48, NS_FRAME_NEEDS_PAINT)

// Frame has a descendant frame that needs painting - This includes
// cross-doc children.
FRAME_STATE_BIT(Generic, 49, NS_FRAME_DESCENDANT_NEEDS_PAINT)

// Frame is a descendant of a popup
FRAME_STATE_BIT(Generic, 50, NS_FRAME_IN_POPUP)

// Frame has only descendant frames that needs painting - This includes
// cross-doc children. This guarantees that all descendents have
// NS_FRAME_NEEDS_PAINT and NS_FRAME_ALL_DESCENDANTS_NEED_PAINT, or they
// have no display items.
FRAME_STATE_BIT(Generic, 51, NS_FRAME_ALL_DESCENDANTS_NEED_PAINT)

// Frame is marked as NS_FRAME_NEEDS_PAINT and also has an explicit
// rect stored to invalidate.
FRAME_STATE_BIT(Generic, 52, NS_FRAME_HAS_INVALID_RECT)

// Frame is not displayed directly due to it being, or being under, an SVG
// <defs> element or an SVG resource element (<mask>, <pattern>, etc.)
FRAME_STATE_BIT(Generic, 53, NS_FRAME_IS_NONDISPLAY)

// Frame has a LayerActivityProperty property
FRAME_STATE_BIT(Generic, 54, NS_FRAME_HAS_LAYER_ACTIVITY_PROPERTY)

// Frame owns anonymous boxes whose ComputedStyles it will need to update
// during a stylo tree traversal.
FRAME_STATE_BIT(Generic, 55, NS_FRAME_OWNS_ANON_BOXES)

// Frame maybe has a counter-reset/increment style
FRAME_STATE_BIT(Generic, 56, NS_FRAME_HAS_CSS_COUNTER_STYLE)

// The display list of the frame can be handled by the shortcut for
// COMMON CASE.
FRAME_STATE_BIT(Generic, 57, NS_FRAME_SIMPLE_DISPLAYLIST)

// Set for all descendants of MathML sub/supscript elements (other than the
// base frame) to indicate that the SSTY font feature should be used.
FRAME_STATE_BIT(Generic, 58, NS_FRAME_MATHML_SCRIPT_DESCENDANT)

// This state bit is set on frames within token MathML elements if the
// token represents an <mi> tag whose inner HTML consists of a single
// non-whitespace character to allow special rendering behaviour.
FRAME_STATE_BIT(Generic, 59, NS_FRAME_IS_IN_SINGLE_CHAR_MI)

// NOTE: Bits 20-31 and 60-63 of the frame state are reserved for specific
// frame classes.

// NOTE: No more unused bits. If needed, investigate removing obsolete bits by
// adjusting logic, or moving infrequently-used bits elsewhere. If more space
// for frame state is still needed, look for bit field gaps in nsIFrame.

// == Frame state bits that apply to box frames ===============================

FRAME_STATE_GROUP(Box, nsBoxFrame)

FRAME_STATE_BIT(Box, 20, NS_STATE_BOX_CHILD_RESERVED)
FRAME_STATE_BIT(Box, 22, NS_STATE_IS_HORIZONTAL)
FRAME_STATE_BIT(Box, 23, NS_STATE_AUTO_STRETCH)
FRAME_STATE_BIT(Box, 24, NS_STATE_IS_ROOT)
/* Bits 25, 26, and 27 were used for xul debug flags but are now unused */
FRAME_STATE_BIT(Box, 28, NS_STATE_MENU_HAS_POPUP_LIST)
FRAME_STATE_BIT(Box, 29, NS_STATE_BOX_WRAPS_KIDS_IN_BLOCK)
FRAME_STATE_BIT(Box, 30, NS_STATE_EQUAL_SIZE)
FRAME_STATE_BIT(Box, 31, NS_STATE_IS_DIRECTION_NORMAL)

// == Frame state bits that apply to flex container frames ====================

FRAME_STATE_GROUP(FlexContainer, nsFlexContainerFrame)

// True iff the normal flow children are already in CSS 'order' in the
// order they occur in the child frame list.
FRAME_STATE_BIT(FlexContainer, 20,
                NS_STATE_FLEX_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER)

// Set for a flex container that is emulating a legacy
// 'display:-webkit-{inline-}box' or 'display:-moz-{inline-}box' container.
FRAME_STATE_BIT(FlexContainer, 21, NS_STATE_FLEX_IS_EMULATING_LEGACY_BOX)

// True if the container has no flex items; may lie if there is a pending reflow
FRAME_STATE_BIT(FlexContainer, 22, NS_STATE_FLEX_SYNTHESIZE_BASELINE)

// True if any flex item in the container has a line with a
// -webkit-line-ellipsis marker.
FRAME_STATE_BIT(FlexContainer, 23, NS_STATE_FLEX_HAS_LINE_CLAMP_ELLIPSIS)

// True iff some first-in-flow in-flow children were pushed.
// Note that those child frames may have been removed without this bit
// being updated for performance reasons, so code shouldn't depend on
// actually finding any pushed items when this bit is set.
FRAME_STATE_BIT(FlexContainer, 24, NS_STATE_FLEX_DID_PUSH_ITEMS)

// We've merged some OverflowList children since last reflow.
FRAME_STATE_BIT(FlexContainer, 25, NS_STATE_FLEX_HAS_CHILD_NIFS)

// == Frame state bits that apply to grid container frames ====================

FRAME_STATE_GROUP(GridContainer, nsGridContainerFrame)

// True iff the normal flow children are already in CSS 'order' in the
// order they occur in the child frame list.
FRAME_STATE_BIT(GridContainer, 20,
                NS_STATE_GRID_NORMAL_FLOW_CHILDREN_IN_CSS_ORDER)

// True iff some first-in-flow in-flow children were pushed.
// Note that those child frames may have been removed without this bit
// being updated for performance reasons, so code shouldn't depend on
// actually finding any pushed items when this bit is set.
FRAME_STATE_BIT(GridContainer, 21, NS_STATE_GRID_DID_PUSH_ITEMS)

// True if the container has no grid items; may lie if there is a pending
// reflow.
FRAME_STATE_BIT(GridContainer, 22, NS_STATE_GRID_SYNTHESIZE_BASELINE)

// True if the container is a subgrid in its inline axis.
FRAME_STATE_BIT(GridContainer, 23, NS_STATE_GRID_IS_COL_SUBGRID)

// True if the container is a subgrid in its block axis.
FRAME_STATE_BIT(GridContainer, 24, NS_STATE_GRID_IS_ROW_SUBGRID)

// The container contains one or more items subgridded in its inline axis.
FRAME_STATE_BIT(GridContainer, 25, NS_STATE_GRID_HAS_COL_SUBGRID_ITEM)

// The container contains one or more items subgridded in its block axis.
FRAME_STATE_BIT(GridContainer, 26, NS_STATE_GRID_HAS_ROW_SUBGRID_ITEM)

// We've merged some OverflowList children since last reflow.
FRAME_STATE_BIT(GridContainer, 27, NS_STATE_GRID_HAS_CHILD_NIFS)

// True if the container has masonry layout in its inline axis.
// (mutually exclusive with NS_STATE_GRID_IS_ROW_MASONRY)
FRAME_STATE_BIT(GridContainer, 28, NS_STATE_GRID_IS_COL_MASONRY)

// True if the container has masonry layout in its block axis.
// (mutually exclusive with NS_STATE_GRID_IS_COL_MASONRY)
FRAME_STATE_BIT(GridContainer, 29, NS_STATE_GRID_IS_ROW_MASONRY)

// == Frame state bits that apply to SVG frames ===============================

FRAME_STATE_GROUP_NAME(SVG)
FRAME_STATE_GROUP_CLASS(SVG, ISVGDisplayableFrame)
FRAME_STATE_GROUP_CLASS(SVG, SVGContainerFrame)

// If this bit is set, we are a <clipPath> element or descendant.
FRAME_STATE_BIT(SVG, 20, NS_STATE_SVG_CLIPPATH_CHILD)

// For SVG text, the NS_FRAME_IS_DIRTY and NS_FRAME_HAS_DIRTY_CHILDREN bits
// indicate that our anonymous block child needs to be reflowed, and that
// mPositions will likely need to be updated as a consequence. These are set,
// for example, when the font-family changes. Sometimes we only need to
// update mPositions though. For example if the x/y attributes change.
// mPositioningDirty is used to indicate this latter "things are dirty" case
// to allow us to avoid reflowing the anonymous block when it is not
// necessary.
FRAME_STATE_BIT(SVG, 21, NS_STATE_SVG_POSITIONING_DIRTY)

// For text, whether the values from x/y/dx/dy attributes have any percentage
// values that are used in determining the positions of glyphs.  The value will
// be true even if a positioning value is overridden by a descendant element's
// attribute with a non-percentage length.  For example,
// NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES would be set for:
//
//   <text x="10%"><tspan x="0">abc</tspan></text>
//
// Percentage values beyond the number of addressable characters, however, do
// not influence NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES.  For example,
// NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES would be false for:
//
//   <text x="10 20 30 40%">abc</text>
//
// NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES is used to determine whether
// to recompute mPositions when the viewport size changes.  So although the
// first example above shows that NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES
// can be true even if a viewport size change will not affect mPositions,
// determining a completley accurate value for
// NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES would require extra work that is
// probably not worth it.
FRAME_STATE_BIT(SVG, 22, NS_STATE_SVG_POSITIONING_MAY_USE_PERCENTAGES)

FRAME_STATE_BIT(SVG, 23, NS_STATE_SVG_TEXT_IN_REFLOW)

// Set on SVGTextFrame frames when they need a
// TextNodeCorrespondenceRecorder::RecordCorrespondence call
// to update the cached nsTextNode indexes that they correspond to.
FRAME_STATE_BIT(SVG, 24, NS_STATE_SVG_TEXT_CORRESPONDENCE_DIRTY)

// == Frame state bits that apply to text frames ==============================

FRAME_STATE_GROUP(Text, nsTextFrame)

// -- Flags set during reflow -------------------------------------------------

// nsTextFrame.cpp defines TEXT_REFLOW_FLAGS to be all of these bits.

// This bit is set on the first frame in a continuation indicating
// that it was chopped short because of :first-letter style.
FRAME_STATE_BIT(Text, 20, TEXT_FIRST_LETTER)

// This bit is set on frames that are logically adjacent to the start of the
// line (i.e. no prior frame on line with actual displayed in-flow content).
FRAME_STATE_BIT(Text, 21, TEXT_START_OF_LINE)

// This bit is set on frames that are logically adjacent to the end of the
// line (i.e. no following on line with actual displayed in-flow content).
FRAME_STATE_BIT(Text, 22, TEXT_END_OF_LINE)

// This bit is set on frames that end with a hyphenated break.
FRAME_STATE_BIT(Text, 23, TEXT_HYPHEN_BREAK)

// This bit is set on frames that trimmed trailing whitespace characters when
// calculating their width during reflow.
FRAME_STATE_BIT(Text, 24, TEXT_TRIMMED_TRAILING_WHITESPACE)

// This bit is set on frames that have justification enabled. We record
// this in a state bit because we don't always have the containing block
// easily available to check text-align on.
FRAME_STATE_BIT(Text, 25, TEXT_JUSTIFICATION_ENABLED)

// Set this bit if the textframe has overflow area for IME/spellcheck underline.
FRAME_STATE_BIT(Text, 26, TEXT_SELECTION_UNDERLINE_OVERFLOWED)

// -- Cache bits for IsEmpty() ------------------------------------------------

// nsTextFrame.cpp defines TEXT_WHITESPACE_FLAGS to be both of these bits.

// Set this bit if the textframe is known to be only collapsible whitespace.
FRAME_STATE_BIT(Text, 27, TEXT_IS_ONLY_WHITESPACE)

// Set this bit if the textframe is known to be not only collapsible whitespace.
FRAME_STATE_BIT(Text, 28, TEXT_ISNOT_ONLY_WHITESPACE)

// -- Other state bits --------------------------------------------------------

// Set when this text frame is mentioned in the userdata for mTextRun
FRAME_STATE_BIT(Text, 29, TEXT_IN_TEXTRUN_USER_DATA)

// This state bit is set on frames whose character data offsets need to be
// fixed up
FRAME_STATE_BIT(Text, 30, TEXT_OFFSETS_NEED_FIXING)

// This state bit is set on frames that have some non-collapsed characters after
// reflow
FRAME_STATE_BIT(Text, 31, TEXT_HAS_NONCOLLAPSED_CHARACTERS)

// This state bit is set on children of token MathML elements.
// NOTE: TEXT_IS_IN_TOKEN_MATHML has a global state bit value that is shared
//       with NS_FRAME_IS_PUSHED_FLOAT.
FRAME_STATE_BIT(Text, 32, TEXT_IS_IN_TOKEN_MATHML)

// Set when this text frame is mentioned in the userdata for the
// uninflated textrun property
FRAME_STATE_BIT(Text, 60, TEXT_IN_UNINFLATED_TEXTRUN_USER_DATA)

FRAME_STATE_BIT(Text, 61, TEXT_HAS_FONT_INFLATION)

// Set when this text frame contains nothing that will actually render
FRAME_STATE_BIT(Text, 62, TEXT_NO_RENDERED_GLYPHS)

// Whether this frame is cached in the Offset Frame Cache
// (OffsetToFrameProperty)
FRAME_STATE_BIT(Text, 63, TEXT_IN_OFFSET_CACHE)

// == Frame state bits that apply to block frames =============================

FRAME_STATE_GROUP(Block, nsBlockFrame)

// Something in the block has changed that requires Bidi resolution to be
// performed on the block. This flag must be either set on all blocks in a
// continuation chain or none of them.
FRAME_STATE_BIT(Block, 20, NS_BLOCK_NEEDS_BIDI_RESOLUTION)

FRAME_STATE_BIT(Block, 21, NS_BLOCK_HAS_PUSHED_FLOATS)

// This indicates that this is a frame from which child margins can be
// calculated. The absence of this flag implies that child margin calculations
// should ignore the frame and look further up the parent chain. Used in
// nsBlockReflowContext::ComputeCollapsedBStartMargin() via
// nsBlockFrame::IsMarginRoot().
//
// This causes the BlockReflowInput's constructor to set the
// mIsBStartMarginRoot and mIsBEndMarginRoot flags.
FRAME_STATE_BIT(Block, 22, NS_BLOCK_MARGIN_ROOT)

// This indicates that a block frame should create its own float manager. This
// is required by each block frame that can contain floats. The float manager is
// used to reserve space for the floated frames.
FRAME_STATE_BIT(Block, 23, NS_BLOCK_FLOAT_MGR)

// For setting the relevant bits on a block formatting context:
#define NS_BLOCK_FORMATTING_CONTEXT_STATE_BITS \
(NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT)

FRAME_STATE_BIT(Block, 24, NS_BLOCK_HAS_LINE_CURSOR)

FRAME_STATE_BIT(Block, 25, NS_BLOCK_HAS_OVERFLOW_LINES)

FRAME_STATE_BIT(Block, 26, NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS)

// Set on any block that has descendant frames in the normal
// flow with 'clear' set to something other than 'none'
// (including <BR CLEAR="..."> frames)
FRAME_STATE_BIT(Block, 27, NS_BLOCK_HAS_CLEAR_CHILDREN)

// NS_BLOCK_CLIP_PAGINATED_OVERFLOW is only set in paginated prescontexts, on
// blocks which were forced to not have scrollframes but still need to clip
// the display of their kids.
FRAME_STATE_BIT(Block, 28, NS_BLOCK_CLIP_PAGINATED_OVERFLOW)

// NS_BLOCK_HAS_FIRST_LETTER_STYLE means that the block has first-letter style,
// even if it has no actual first-letter frame among its descendants.
FRAME_STATE_BIT(Block, 29, NS_BLOCK_HAS_FIRST_LETTER_STYLE)

// NS_BLOCK_FRAME_HAS_OUTSIDE_MARKER and NS_BLOCK_FRAME_HAS_INSIDE_MARKER
// means the block has an associated ::marker frame, they are mutually
// exclusive.
FRAME_STATE_BIT(Block, 30, NS_BLOCK_FRAME_HAS_OUTSIDE_MARKER)
FRAME_STATE_BIT(Block, 31, NS_BLOCK_FRAME_HAS_INSIDE_MARKER)

// NS_BLOCK_HAS_LINE_CLAMP_ELLIPSIS indicates that exactly one line in this
// block has the LineClampEllipsis flag set, and that such a line must be found
// and have that flag cleared when reflowing this element's nearest legacy box
// container.
FRAME_STATE_BIT(Block, 60, NS_BLOCK_HAS_LINE_CLAMP_ELLIPSIS)

// This block has had a child marked dirty, so before we reflow we need
// to look through the lines to find any such children and mark
// appropriate lines dirty.
FRAME_STATE_BIT(Block, 61, NS_BLOCK_LOOK_FOR_DIRTY_FRAMES)

// Are our cached intrinsic widths intrinsic widths for font size
// inflation?  i.e., what was the current state of
// GetPresContext()->mInflationDisabledForShrinkWrap at the time they
// were computed?
// nsBlockFrame is the only thing that caches intrinsic widths that
// needs to track this because it's the only thing that caches intrinsic
// widths that lives inside of things (form controls) that do intrinsic
// sizing with font inflation enabled.
FRAME_STATE_BIT(Block, 62, NS_BLOCK_FRAME_INTRINSICS_INFLATED)

// NS_BLOCK_HAS_FIRST_LETTER_CHILD means that there is an inflow first-letter
// frame among the block's descendants. If there is a floating first-letter
// frame, or the block has first-letter style but has no first letter, this
// bit is not set. This bit is set on the first continuation only.
FRAME_STATE_BIT(Block, 63, NS_BLOCK_HAS_FIRST_LETTER_CHILD)

// == Frame state bits that apply to image frames =============================

FRAME_STATE_GROUP(Image, nsImageFrame)

FRAME_STATE_BIT(Image, 20, IMAGE_SIZECONSTRAINED)

// == Frame state bits that apply to inline frames ============================

FRAME_STATE_GROUP(Inline, nsInlineFrame)

/**  In Bidi inline start (or end) margin/padding/border should be applied to
 *  first (or last) frame (or a continuation frame).
 *  This state value shows if this frame is first (or last) continuation
 *  or not.
 */

FRAME_STATE_BIT(Inline, 21, NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET)
FRAME_STATE_BIT(Inline, 22, NS_INLINE_FRAME_BIDI_VISUAL_IS_FIRST)
FRAME_STATE_BIT(Inline, 23, NS_INLINE_FRAME_BIDI_VISUAL_IS_LAST)
// nsRubyTextFrame inherits from nsInlineFrame

// == Frame state bits that apply to ruby text frames =========================

FRAME_STATE_GROUP(RubyText, nsRubyTextFrame)

// inherits from nsInlineFrame
FRAME_STATE_BIT(RubyText, 24, NS_RUBY_TEXT_FRAME_COLLAPSED)

// == Frame state bits that apply to ruby text container frames ===============

FRAME_STATE_GROUP(RubyTextContainer, nsRubyTextContainerFrame)

FRAME_STATE_BIT(RubyTextContainer, 20, NS_RUBY_TEXT_CONTAINER_IS_SPAN)

// == Frame state bits that apply to placeholder frames =======================

FRAME_STATE_GROUP(Placeholder, nsPlaceholderFrame)

// Frame state bits that are used to keep track of what this is a
// placeholder for.

FRAME_STATE_BIT(Placeholder, 20, PLACEHOLDER_FOR_FLOAT)
FRAME_STATE_BIT(Placeholder, 21, PLACEHOLDER_FOR_ABSPOS)
FRAME_STATE_BIT(Placeholder, 22, PLACEHOLDER_FOR_FIXEDPOS)
FRAME_STATE_BIT(Placeholder, 23, PLACEHOLDER_FOR_POPUP)
FRAME_STATE_BIT(Placeholder, 24, PLACEHOLDER_FOR_TOPLAYER)

// This bit indicates that the out-of-flow frame's static position needs to be
// determined using the CSS Box Alignment properties
// ([align,justify]-[self,content]).  When this is set, the placeholder frame's
// position doesn't represent the static position, as it usually would --
// rather, it represents the logical start corner of the alignment containing
// block.  Then, after we've determined the out-of-flow frame's size, we can
// resolve the actual static position using the alignment properties.
FRAME_STATE_BIT(Placeholder, 25, PLACEHOLDER_STATICPOS_NEEDS_CSSALIGN)

// Are all earlier frames on the same block line empty?
FRAME_STATE_BIT(Placeholder, 26, PLACEHOLDER_LINE_IS_EMPTY_SO_FAR)
// Does the above bit have a valid value?
FRAME_STATE_BIT(Placeholder, 27, PLACEHOLDER_HAVE_LINE_IS_EMPTY_SO_FAR)

// == Frame state bits that apply to table cell frames ========================

FRAME_STATE_GROUP(TableCell, nsTableCellFrame)

FRAME_STATE_BIT(TableCell, 20, NS_TABLE_CELL_HAD_SPECIAL_REFLOW)
FRAME_STATE_BIT(TableCell, 21, NS_TABLE_CELL_CONTENT_EMPTY)

// == Frame state bits that apply to table column frames ======================

// Bits 28-31 on nsTableColFrames are used to store the column type.

// == Frame state bits that apply to table column group frames ================

// Bits 30-31 on nsTableColGroupFrames are used to store the column type.

// == Frame state bits that apply to table rows and table row group frames ====

FRAME_STATE_GROUP_NAME(TableRowAndRowGroup)
FRAME_STATE_GROUP_CLASS(TableRowAndRowGroup, nsTableRowFrame)
FRAME_STATE_GROUP_CLASS(TableRowAndRowGroup, nsTableRowGroupFrame)

// see nsTableRowGroupFrame::InitRepeatedFrame
FRAME_STATE_BIT(TableRowAndRowGroup, 28, NS_REPEATED_ROW_OR_ROWGROUP)

// == Frame state bits that apply to table row frames =========================

FRAME_STATE_GROUP(TableRow, nsTableRowFrame)

// Indicates whether this row has any cells that have
// non-auto-bsize and rowspan=1
FRAME_STATE_BIT(TableRow, 29, NS_ROW_HAS_CELL_WITH_STYLE_BSIZE)

FRAME_STATE_BIT(TableRow, 30, NS_TABLE_ROW_HAS_UNPAGINATED_BSIZE)

// == Frame state bits that apply to table row group frames ===================

FRAME_STATE_GROUP(TableRowGroup, nsTableRowGroupFrame)

FRAME_STATE_BIT(TableRowGroup, 27, NS_ROWGROUP_HAS_ROW_CURSOR)
FRAME_STATE_BIT(TableRowGroup, 30, NS_ROWGROUP_HAS_STYLE_BSIZE)

// thead or tfoot should be repeated on every printed page
FRAME_STATE_BIT(TableRowGroup, 31, NS_ROWGROUP_REPEATABLE)

FRAME_STATE_GROUP(Table, nsTableFrame)

FRAME_STATE_BIT(Table, 28, NS_TABLE_PART_HAS_FIXED_BACKGROUND)

// == Frame state bits that apply to page frames ==============================
FRAME_STATE_GROUP(Page, nsPageFrame)

// If set, this bit indicates that the given nsPageFrame has been skipped
// via the user's custom-page-range choice, and should not be rendered.
FRAME_STATE_BIT(Page, 20, NS_PAGE_SKIPPED_BY_CUSTOM_RANGE)

#undef FRAME_STATE_GROUP

#ifdef DEFINED_FRAME_STATE_GROUP_NAME
#undef DEFINED_FRAME_STATE_GROUP_NAME
#undef FRAME_STATE_GROUP_NAME
#endif

#ifdef DEFINED_FRAME_STATE_GROUP_CLASS
#undef DEFINED_FRAME_STATE_GROUP_CLASS
#undef FRAME_STATE_GROUP_CLASS
#endif

#ifdef DEFINED_FRAME_STATE_BIT
#undef DEFINED_FRAME_STATE_BIT
#undef FRAME_STATE_BIT
#endif
