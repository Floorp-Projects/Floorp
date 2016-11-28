/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants used in the style struct data provided by nsStyleContext */

#ifndef nsStyleConsts_h___
#define nsStyleConsts_h___

#include "gfxRect.h"
#include "nsFont.h"
#include "X11UndefineNone.h"

// XXX fold this into nsStyleContext and group by nsStyleXXX struct

namespace mozilla {

#define NS_FOR_CSS_FULL_CORNERS(var_) for (int32_t var_ = 0; var_ < 4; ++var_)

// Indices into "half corner" arrays (nsStyleCorners e.g.)
#define NS_CORNER_TOP_LEFT_X      0
#define NS_CORNER_TOP_LEFT_Y      1
#define NS_CORNER_TOP_RIGHT_X     2
#define NS_CORNER_TOP_RIGHT_Y     3
#define NS_CORNER_BOTTOM_RIGHT_X  4
#define NS_CORNER_BOTTOM_RIGHT_Y  5
#define NS_CORNER_BOTTOM_LEFT_X   6
#define NS_CORNER_BOTTOM_LEFT_Y   7

#define NS_FOR_CSS_HALF_CORNERS(var_) for (int32_t var_ = 0; var_ < 8; ++var_)

// The results of these conversion macros are exhaustively checked in
// nsStyleCoord.cpp.
// Arguments must not have side effects.

#define NS_HALF_CORNER_IS_X(var_) (!((var_)%2))
#define NS_HALF_TO_FULL_CORNER(var_) ((var_)/2)
#define NS_FULL_TO_HALF_CORNER(var_, vert_) ((var_)*2 + !!(vert_))

#define NS_SIDE_IS_VERTICAL(side_) ((side_) % 2)
#define NS_SIDE_TO_FULL_CORNER(side_, second_) \
  (((side_) + !!(second_)) % 4)
#define NS_SIDE_TO_HALF_CORNER(side_, second_, parallel_) \
  ((((side_) + !!(second_))*2 + ((side_) + !(parallel_))%2) % 8)

// Basic shapes
enum class StyleBasicShapeType : uint8_t {
  Polygon,
  Circle,
  Ellipse,
  Inset,
};

// box-align
enum class StyleBoxAlign : uint8_t {
  Stretch,
  Start,
  Center,
  Baseline,
  End,
};

// box-decoration-break
enum class StyleBoxDecorationBreak : uint8_t {
  Slice,
  Clone,
};

// box-direction
enum class StyleBoxDirection : uint8_t {
  Normal,
  Reverse,
};

// box-orient
enum class StyleBoxOrient : uint8_t {
  Horizontal,
  Vertical,
};

// box-pack
enum class StyleBoxPack : uint8_t {
  Start,
  Center,
  End,
  Justify,
};

// box-sizing
enum class StyleBoxSizing : uint8_t {
  Content,
  Border
};

// box-shadow
enum class StyleBoxShadowType : uint8_t {
  Inset,
};

// clear
enum class StyleClear : uint8_t {
  None = 0,
  Left,
  Right,
  InlineStart,
  InlineEnd,
  Both,
  // StyleClear::Line can be added to one of the other values in layout
  // so it needs to use a bit value that none of the other values can have.
  Line = 8,
  Max = 13  // Max = (Both | Line)
};

// clip-path geometry box
enum class StyleClipPathGeometryBox : uint8_t {
  NoBox,
  Content,
  Padding,
  Border,
  Margin,
  Fill,
  Stroke,
  View,
};

// fill-rule
enum class StyleFillRule : uint8_t {
  Nonzero,
  Evenodd,
};

// float
// https://developer.mozilla.org/en-US/docs/Web/CSS/float
enum class StyleFloat : uint8_t {
  None,
  Left,
  Right,
  InlineStart,
  InlineEnd
};

// float-edge
enum class StyleFloatEdge : uint8_t {
  ContentBox,
  MarginBox,
};

// shape-box for shape-outside
enum class StyleShapeOutsideShapeBox : uint8_t {
  NoBox,
  Content,
  Padding,
  Border,
  Margin
};

// Shape source type
enum class StyleShapeSourceType : uint8_t {
  None,
  URL,
  Shape,
  Box,
};

// user-focus
enum class StyleUserFocus : uint8_t {
  None,
  Ignore,
  Normal,
  SelectAll,
  SelectBefore,
  SelectAfter,
  SelectSame,
  SelectMenu,
};

// user-select
enum class StyleUserSelect : uint8_t {
  None,
  Text,
  Element,
  Elements,
  All,
  Toggle,
  TriState,
  Auto,     // internal value - please use nsFrame::IsSelectable()
  MozAll,   // force selection of all children, unless an ancestor has NONE set - bug 48096
  MozText,  // Like TEXT, except that it won't get overridden by ancestors having ALL.
};

// user-input
enum class StyleUserInput : uint8_t {
  None,
  Enabled,
  Disabled,
  Auto,
};

// user-modify
enum class StyleUserModify : uint8_t {
  ReadOnly,
  ReadWrite,
  WriteOnly,
};

// -moz-window-dragging
enum class StyleWindowDragging : uint8_t {
  Default,
  Drag,
  NoDrag,
};

// orient
enum class StyleOrient : uint8_t {
  Inline,
  Block,
  Horizontal,
  Vertical,
};

#define NS_RADIUS_FARTHEST_SIDE 0
#define NS_RADIUS_CLOSEST_SIDE  1

// stack-sizing
#define NS_STYLE_STACK_SIZING_IGNORE         0
#define NS_STYLE_STACK_SIZING_STRETCH_TO_FIT 1

// Azimuth - See nsStyleAural
#define NS_STYLE_AZIMUTH_LEFT_SIDE        0x00
#define NS_STYLE_AZIMUTH_FAR_LEFT         0x01
#define NS_STYLE_AZIMUTH_LEFT             0x02
#define NS_STYLE_AZIMUTH_CENTER_LEFT      0x03
#define NS_STYLE_AZIMUTH_CENTER           0x04
#define NS_STYLE_AZIMUTH_CENTER_RIGHT     0x05
#define NS_STYLE_AZIMUTH_RIGHT            0x06
#define NS_STYLE_AZIMUTH_FAR_RIGHT        0x07
#define NS_STYLE_AZIMUTH_RIGHT_SIDE       0x08
#define NS_STYLE_AZIMUTH_BEHIND           0x80  // bits
#define NS_STYLE_AZIMUTH_LEFTWARDS        0x10  // bits
#define NS_STYLE_AZIMUTH_RIGHTWARDS       0x20  // bits

// See nsStyleAural
#define NS_STYLE_ELEVATION_BELOW          1
#define NS_STYLE_ELEVATION_LEVEL          2
#define NS_STYLE_ELEVATION_ABOVE          3
#define NS_STYLE_ELEVATION_HIGHER         4
#define NS_STYLE_ELEVATION_LOWER          5

// See nsStyleAural
#define NS_STYLE_PITCH_X_LOW              1
#define NS_STYLE_PITCH_LOW                2
#define NS_STYLE_PITCH_MEDIUM             3
#define NS_STYLE_PITCH_HIGH               4
#define NS_STYLE_PITCH_X_HIGH             5

// See nsStyleAural
#define NS_STYLE_SPEAK_NONE               0
#define NS_STYLE_SPEAK_NORMAL             1
#define NS_STYLE_SPEAK_SPELL_OUT          2

// See nsStyleAural
#define NS_STYLE_SPEAK_HEADER_ONCE        0
#define NS_STYLE_SPEAK_HEADER_ALWAYS      1

// See nsStyleAural
#define NS_STYLE_SPEAK_NUMERAL_DIGITS     0
#define NS_STYLE_SPEAK_NUMERAL_CONTINUOUS 1

// See nsStyleAural
#define NS_STYLE_SPEAK_PUNCTUATION_NONE   0
#define NS_STYLE_SPEAK_PUNCTUATION_CODE   1

// See nsStyleAural
#define NS_STYLE_SPEECH_RATE_X_SLOW       0
#define NS_STYLE_SPEECH_RATE_SLOW         1
#define NS_STYLE_SPEECH_RATE_MEDIUM       2
#define NS_STYLE_SPEECH_RATE_FAST         3
#define NS_STYLE_SPEECH_RATE_X_FAST       4
#define NS_STYLE_SPEECH_RATE_FASTER       10
#define NS_STYLE_SPEECH_RATE_SLOWER       11

// See nsStyleAural
#define NS_STYLE_VOLUME_SILENT            0
#define NS_STYLE_VOLUME_X_SOFT            1
#define NS_STYLE_VOLUME_SOFT              2
#define NS_STYLE_VOLUME_MEDIUM            3
#define NS_STYLE_VOLUME_LOUD              4
#define NS_STYLE_VOLUME_X_LOUD            5

// See nsStyleColor
#define NS_STYLE_COLOR_INHERIT_FROM_BODY  2  /* Can't come from CSS directly */

// See nsStyleColor
#define NS_COLOR_CURRENTCOLOR                   -1
#define NS_COLOR_MOZ_DEFAULT_COLOR              -2
#define NS_COLOR_MOZ_DEFAULT_BACKGROUND_COLOR   -3
#define NS_COLOR_MOZ_HYPERLINKTEXT              -4
#define NS_COLOR_MOZ_VISITEDHYPERLINKTEXT       -5
#define NS_COLOR_MOZ_ACTIVEHYPERLINKTEXT        -6
// Only valid as paints in SVG glyphs
#define NS_COLOR_CONTEXT_FILL                   -7
#define NS_COLOR_CONTEXT_STROKE                 -8

// See nsStyleDisplay
#define NS_STYLE_WILL_CHANGE_STACKING_CONTEXT   (1<<0)
#define NS_STYLE_WILL_CHANGE_TRANSFORM          (1<<1)
#define NS_STYLE_WILL_CHANGE_SCROLL             (1<<2)
#define NS_STYLE_WILL_CHANGE_OPACITY            (1<<3)
#define NS_STYLE_WILL_CHANGE_FIXPOS_CB          (1<<4)
#define NS_STYLE_WILL_CHANGE_ABSPOS_CB          (1<<5)

// See AnimationEffectReadOnly.webidl
// and mozilla/dom/AnimationEffectReadOnlyBinding.h
namespace dom {
enum class PlaybackDirection : uint32_t;
enum class FillMode : uint32_t;
}

// See nsStyleDisplay
#define NS_STYLE_ANIMATION_ITERATION_COUNT_INFINITE 0

// See nsStyleDisplay
#define NS_STYLE_ANIMATION_PLAY_STATE_RUNNING     0
#define NS_STYLE_ANIMATION_PLAY_STATE_PAUSED      1

// See nsStyleImageLayers
#define NS_STYLE_IMAGELAYER_ATTACHMENT_SCROLL        0
#define NS_STYLE_IMAGELAYER_ATTACHMENT_FIXED         1
#define NS_STYLE_IMAGELAYER_ATTACHMENT_LOCAL         2

// See nsStyleImageLayers
// Code depends on these constants having the same values as IMAGELAYER_ORIGIN_*
#define NS_STYLE_IMAGELAYER_CLIP_BORDER              0
#define NS_STYLE_IMAGELAYER_CLIP_PADDING             1
#define NS_STYLE_IMAGELAYER_CLIP_CONTENT             2
// One extra constant which does not exist in IMAGELAYER_ORIGIN_*
#define NS_STYLE_IMAGELAYER_CLIP_TEXT                3

// A magic value that we use for our "pretend that background-clip is
// 'padding' when we have a solid border" optimization.  This isn't
// actually equal to NS_STYLE_IMAGELAYER_CLIP_PADDING because using that
// causes antialiasing seams between the background and border.  This
// is a backend-only value.
#define NS_STYLE_IMAGELAYER_CLIP_MOZ_ALMOST_PADDING  127

// See nsStyleImageLayers
// Code depends on these constants having the same values as BG_CLIP_*
#define NS_STYLE_IMAGELAYER_ORIGIN_BORDER            0
#define NS_STYLE_IMAGELAYER_ORIGIN_PADDING           1
#define NS_STYLE_IMAGELAYER_ORIGIN_CONTENT           2

// See nsStyleImageLayers
// The parser code depends on |ing these values together.
#define NS_STYLE_IMAGELAYER_POSITION_CENTER          (1<<0)
#define NS_STYLE_IMAGELAYER_POSITION_TOP             (1<<1)
#define NS_STYLE_IMAGELAYER_POSITION_BOTTOM          (1<<2)
#define NS_STYLE_IMAGELAYER_POSITION_LEFT            (1<<3)
#define NS_STYLE_IMAGELAYER_POSITION_RIGHT           (1<<4)

// See nsStyleImageLayers
#define NS_STYLE_IMAGELAYER_REPEAT_NO_REPEAT         0x00
#define NS_STYLE_IMAGELAYER_REPEAT_REPEAT_X          0x01
#define NS_STYLE_IMAGELAYER_REPEAT_REPEAT_Y          0x02
#define NS_STYLE_IMAGELAYER_REPEAT_REPEAT            0x03
#define NS_STYLE_IMAGELAYER_REPEAT_SPACE             0x04
#define NS_STYLE_IMAGELAYER_REPEAT_ROUND             0x05

// See nsStyleImageLayers
#define NS_STYLE_IMAGELAYER_SIZE_CONTAIN             0
#define NS_STYLE_IMAGELAYER_SIZE_COVER               1

// Mask mode
#define NS_STYLE_MASK_MODE_ALPHA                0
#define NS_STYLE_MASK_MODE_LUMINANCE            1
#define NS_STYLE_MASK_MODE_MATCH_SOURCE         2

// See nsStyleBackground
#define NS_STYLE_BG_INLINE_POLICY_EACH_BOX      0
#define NS_STYLE_BG_INLINE_POLICY_CONTINUOUS    1
#define NS_STYLE_BG_INLINE_POLICY_BOUNDING_BOX  2

// See nsStyleTable
#define NS_STYLE_BORDER_COLLAPSE                0
#define NS_STYLE_BORDER_SEPARATE                1

// Possible enumerated specified values of border-*-width, used by nsCSSMargin
#define NS_STYLE_BORDER_WIDTH_THIN              0
#define NS_STYLE_BORDER_WIDTH_MEDIUM            1
#define NS_STYLE_BORDER_WIDTH_THICK             2
// XXX chopping block #define NS_STYLE_BORDER_WIDTH_LENGTH_VALUE      3

// See nsStyleBorder mBorderStyle
#define NS_STYLE_BORDER_STYLE_NONE              0
#define NS_STYLE_BORDER_STYLE_GROOVE            1
#define NS_STYLE_BORDER_STYLE_RIDGE             2
#define NS_STYLE_BORDER_STYLE_DOTTED            3
#define NS_STYLE_BORDER_STYLE_DASHED            4
#define NS_STYLE_BORDER_STYLE_SOLID             5
#define NS_STYLE_BORDER_STYLE_DOUBLE            6
#define NS_STYLE_BORDER_STYLE_INSET             7
#define NS_STYLE_BORDER_STYLE_OUTSET            8
#define NS_STYLE_BORDER_STYLE_HIDDEN            9
#define NS_STYLE_BORDER_STYLE_AUTO              10 // for outline-style only

// See nsStyleBorder mBorderImage
#define NS_STYLE_BORDER_IMAGE_REPEAT_STRETCH    0
#define NS_STYLE_BORDER_IMAGE_REPEAT_REPEAT     1
#define NS_STYLE_BORDER_IMAGE_REPEAT_ROUND      2
#define NS_STYLE_BORDER_IMAGE_REPEAT_SPACE      3

#define NS_STYLE_BORDER_IMAGE_SLICE_NOFILL      0
#define NS_STYLE_BORDER_IMAGE_SLICE_FILL        1

// See nsStyleContent
#define NS_STYLE_CONTENT_OPEN_QUOTE             0
#define NS_STYLE_CONTENT_CLOSE_QUOTE            1
#define NS_STYLE_CONTENT_NO_OPEN_QUOTE          2
#define NS_STYLE_CONTENT_NO_CLOSE_QUOTE         3
#define NS_STYLE_CONTENT_ALT_CONTENT            4

// See nsStyleColor
#define NS_STYLE_CURSOR_AUTO                    1
#define NS_STYLE_CURSOR_CROSSHAIR               2
#define NS_STYLE_CURSOR_DEFAULT                 3    // ie: an arrow
#define NS_STYLE_CURSOR_POINTER                 4    // for links
#define NS_STYLE_CURSOR_MOVE                    5
#define NS_STYLE_CURSOR_E_RESIZE                6
#define NS_STYLE_CURSOR_NE_RESIZE               7
#define NS_STYLE_CURSOR_NW_RESIZE               8
#define NS_STYLE_CURSOR_N_RESIZE                9
#define NS_STYLE_CURSOR_SE_RESIZE               10
#define NS_STYLE_CURSOR_SW_RESIZE               11
#define NS_STYLE_CURSOR_S_RESIZE                12
#define NS_STYLE_CURSOR_W_RESIZE                13
#define NS_STYLE_CURSOR_TEXT                    14   // ie: i-beam
#define NS_STYLE_CURSOR_WAIT                    15
#define NS_STYLE_CURSOR_HELP                    16
#define NS_STYLE_CURSOR_COPY                    17   // CSS3
#define NS_STYLE_CURSOR_ALIAS                   18
#define NS_STYLE_CURSOR_CONTEXT_MENU            19
#define NS_STYLE_CURSOR_CELL                    20
#define NS_STYLE_CURSOR_GRAB                    21
#define NS_STYLE_CURSOR_GRABBING                22
#define NS_STYLE_CURSOR_SPINNING                23
#define NS_STYLE_CURSOR_ZOOM_IN                 24
#define NS_STYLE_CURSOR_ZOOM_OUT                25
#define NS_STYLE_CURSOR_NOT_ALLOWED             26
#define NS_STYLE_CURSOR_COL_RESIZE              27
#define NS_STYLE_CURSOR_ROW_RESIZE              28
#define NS_STYLE_CURSOR_NO_DROP                 29
#define NS_STYLE_CURSOR_VERTICAL_TEXT           30
#define NS_STYLE_CURSOR_ALL_SCROLL              31
#define NS_STYLE_CURSOR_NESW_RESIZE             32
#define NS_STYLE_CURSOR_NWSE_RESIZE             33
#define NS_STYLE_CURSOR_NS_RESIZE               34
#define NS_STYLE_CURSOR_EW_RESIZE               35
#define NS_STYLE_CURSOR_NONE                    36

// See nsStyleVisibility
#define NS_STYLE_DIRECTION_LTR                  0
#define NS_STYLE_DIRECTION_RTL                  1

// See nsStyleVisibility
// NOTE: WritingModes.h depends on the particular values used here.
#define NS_STYLE_WRITING_MODE_HORIZONTAL_TB     0
#define NS_STYLE_WRITING_MODE_VERTICAL_RL       1
// #define NS_STYLE_WRITING_MODE_HORIZONTAL_BT  2  // hypothetical
#define NS_STYLE_WRITING_MODE_VERTICAL_LR       3

// Single-bit flag, used in combination with VERTICAL_LR and _RL to specify
// the corresponding SIDEWAYS_* modes.
// (To avoid ambiguity, this bit must be high enough such that no other
// values here accidentally use it in their binary representation.)
#define NS_STYLE_WRITING_MODE_SIDEWAYS_MASK     4

#define NS_STYLE_WRITING_MODE_SIDEWAYS_RL         \
          (NS_STYLE_WRITING_MODE_VERTICAL_RL |    \
           NS_STYLE_WRITING_MODE_SIDEWAYS_MASK)
#define NS_STYLE_WRITING_MODE_SIDEWAYS_LR         \
          (NS_STYLE_WRITING_MODE_VERTICAL_LR |    \
           NS_STYLE_WRITING_MODE_SIDEWAYS_MASK)

// See nsStyleDisplay
//
// NOTE: Order is important! If you change it, make sure to take a look at
// the FrameConstructorDataByDisplay stuff (both the XUL and non-XUL version),
// and ensure it's still correct!
enum class StyleDisplay : uint8_t {
  None = 0,
  Block,
  Inline,
  InlineBlock,
  ListItem,
  Table,
  InlineTable,
  TableRowGroup,
  TableColumn,
  TableColumnGroup,
  TableHeaderGroup,
  TableFooterGroup,
  TableRow,
  TableCell,
  TableCaption,
  Flex,
  InlineFlex,
  Grid,
  InlineGrid,
  Ruby,
  RubyBase,
  RubyBaseContainer,
  RubyText,
  RubyTextContainer,
  Contents,
  WebkitBox,
  WebkitInlineBox,
  Box,
  InlineBox,
#ifdef MOZ_XUL
  XulGrid,
  InlineXulGrid,
  XulGridGroup,
  XulGridLine,
  Stack,
  InlineStack,
  Deck,
  Groupbox,
  Popup,
#endif
};

// See nsStyleDisplay
// If these are re-ordered, nsComputedDOMStyle::DoGetContain() and
// nsCSSValue::AppendToString() must be updated.
#define NS_STYLE_CONTAIN_NONE                   0
#define NS_STYLE_CONTAIN_STRICT                 0x1
#define NS_STYLE_CONTAIN_LAYOUT                 0x2
#define NS_STYLE_CONTAIN_STYLE                  0x4
#define NS_STYLE_CONTAIN_PAINT                  0x8
// NS_STYLE_CONTAIN_ALL_BITS does not correspond to a keyword.
#define NS_STYLE_CONTAIN_ALL_BITS               (NS_STYLE_CONTAIN_LAYOUT | \
                                                 NS_STYLE_CONTAIN_STYLE  | \
                                                 NS_STYLE_CONTAIN_PAINT)

// Shared constants for all align/justify properties (nsStylePosition):
#define NS_STYLE_ALIGN_AUTO             0
#define NS_STYLE_ALIGN_NORMAL           1
#define NS_STYLE_ALIGN_START            2
#define NS_STYLE_ALIGN_END              3
#define NS_STYLE_ALIGN_FLEX_START       4
#define NS_STYLE_ALIGN_FLEX_END         5
#define NS_STYLE_ALIGN_CENTER           6
#define NS_STYLE_ALIGN_LEFT             7
#define NS_STYLE_ALIGN_RIGHT            8
#define NS_STYLE_ALIGN_BASELINE         9
#define NS_STYLE_ALIGN_LAST_BASELINE    10
#define NS_STYLE_ALIGN_STRETCH          11
#define NS_STYLE_ALIGN_SELF_START       12
#define NS_STYLE_ALIGN_SELF_END         13
#define NS_STYLE_ALIGN_SPACE_BETWEEN    14
#define NS_STYLE_ALIGN_SPACE_AROUND     15
#define NS_STYLE_ALIGN_SPACE_EVENLY     16
#define NS_STYLE_ALIGN_LEGACY        0x20 // mutually exclusive w. SAFE & UNSAFE
#define NS_STYLE_ALIGN_SAFE          0x40
#define NS_STYLE_ALIGN_UNSAFE        0x80 // mutually exclusive w. SAFE
#define NS_STYLE_ALIGN_FLAG_BITS     0xE0
#define NS_STYLE_ALIGN_ALL_BITS      0xFF
#define NS_STYLE_ALIGN_ALL_SHIFT        8

#define NS_STYLE_JUSTIFY_AUTO             NS_STYLE_ALIGN_AUTO
#define NS_STYLE_JUSTIFY_NORMAL           NS_STYLE_ALIGN_NORMAL
#define NS_STYLE_JUSTIFY_START            NS_STYLE_ALIGN_START
#define NS_STYLE_JUSTIFY_END              NS_STYLE_ALIGN_END
#define NS_STYLE_JUSTIFY_FLEX_START       NS_STYLE_ALIGN_FLEX_START
#define NS_STYLE_JUSTIFY_FLEX_END         NS_STYLE_ALIGN_FLEX_END
#define NS_STYLE_JUSTIFY_CENTER           NS_STYLE_ALIGN_CENTER
#define NS_STYLE_JUSTIFY_LEFT             NS_STYLE_ALIGN_LEFT
#define NS_STYLE_JUSTIFY_RIGHT            NS_STYLE_ALIGN_RIGHT
#define NS_STYLE_JUSTIFY_BASELINE         NS_STYLE_ALIGN_BASELINE
#define NS_STYLE_JUSTIFY_LAST_BASELINE    NS_STYLE_ALIGN_LAST_BASELINE
#define NS_STYLE_JUSTIFY_STRETCH          NS_STYLE_ALIGN_STRETCH
#define NS_STYLE_JUSTIFY_SELF_START       NS_STYLE_ALIGN_SELF_START
#define NS_STYLE_JUSTIFY_SELF_END         NS_STYLE_ALIGN_SELF_END
#define NS_STYLE_JUSTIFY_SPACE_BETWEEN    NS_STYLE_ALIGN_SPACE_BETWEEN
#define NS_STYLE_JUSTIFY_SPACE_AROUND     NS_STYLE_ALIGN_SPACE_AROUND
#define NS_STYLE_JUSTIFY_SPACE_EVENLY     NS_STYLE_ALIGN_SPACE_EVENLY
#define NS_STYLE_JUSTIFY_LEGACY           NS_STYLE_ALIGN_LEGACY
#define NS_STYLE_JUSTIFY_SAFE             NS_STYLE_ALIGN_SAFE
#define NS_STYLE_JUSTIFY_UNSAFE           NS_STYLE_ALIGN_UNSAFE
#define NS_STYLE_JUSTIFY_FLAG_BITS        NS_STYLE_ALIGN_FLAG_BITS
#define NS_STYLE_JUSTIFY_ALL_BITS         NS_STYLE_ALIGN_ALL_BITS
#define NS_STYLE_JUSTIFY_ALL_SHIFT        NS_STYLE_ALIGN_ALL_SHIFT

// See nsStylePosition
#define NS_STYLE_FLEX_DIRECTION_ROW             0
#define NS_STYLE_FLEX_DIRECTION_ROW_REVERSE     1
#define NS_STYLE_FLEX_DIRECTION_COLUMN          2
#define NS_STYLE_FLEX_DIRECTION_COLUMN_REVERSE  3

// See nsStylePosition
#define NS_STYLE_FLEX_WRAP_NOWRAP               0
#define NS_STYLE_FLEX_WRAP_WRAP                 1
#define NS_STYLE_FLEX_WRAP_WRAP_REVERSE         2

// See nsStylePosition
// NOTE: This is the initial value of the integer-valued 'order' property
// (rather than an internal numerical representation of some keyword).
#define NS_STYLE_ORDER_INITIAL                  0

// XXX remove in a later patch after updating flexbox code with the new names
#define NS_STYLE_JUSTIFY_CONTENT_FLEX_START     NS_STYLE_JUSTIFY_FLEX_START
#define NS_STYLE_JUSTIFY_CONTENT_FLEX_END       NS_STYLE_JUSTIFY_FLEX_END
#define NS_STYLE_JUSTIFY_CONTENT_CENTER         NS_STYLE_JUSTIFY_CENTER
#define NS_STYLE_JUSTIFY_CONTENT_SPACE_BETWEEN  NS_STYLE_JUSTIFY_SPACE_BETWEEN
#define NS_STYLE_JUSTIFY_CONTENT_SPACE_AROUND   NS_STYLE_JUSTIFY_SPACE_AROUND

// See nsStyleFilter
#define NS_STYLE_FILTER_NONE                    0
#define NS_STYLE_FILTER_URL                     1
#define NS_STYLE_FILTER_BLUR                    2
#define NS_STYLE_FILTER_BRIGHTNESS              3
#define NS_STYLE_FILTER_CONTRAST                4
#define NS_STYLE_FILTER_GRAYSCALE               5
#define NS_STYLE_FILTER_INVERT                  6
#define NS_STYLE_FILTER_OPACITY                 7
#define NS_STYLE_FILTER_SATURATE                8
#define NS_STYLE_FILTER_SEPIA                   9
#define NS_STYLE_FILTER_HUE_ROTATE              10
#define NS_STYLE_FILTER_DROP_SHADOW             11

// See nsStyleFont
// We should eventually stop using the NS_STYLE_* variants here.
#define NS_STYLE_FONT_STYLE_NORMAL              NS_FONT_STYLE_NORMAL
#define NS_STYLE_FONT_STYLE_ITALIC              NS_FONT_STYLE_ITALIC
#define NS_STYLE_FONT_STYLE_OBLIQUE             NS_FONT_STYLE_OBLIQUE

// See nsStyleFont
// We should eventually stop using the NS_STYLE_* variants here.
#define NS_STYLE_FONT_WEIGHT_NORMAL             NS_FONT_WEIGHT_NORMAL
#define NS_STYLE_FONT_WEIGHT_BOLD               NS_FONT_WEIGHT_BOLD
// The constants below appear only in style sheets and not computed style.
#define NS_STYLE_FONT_WEIGHT_BOLDER             (-1)
#define NS_STYLE_FONT_WEIGHT_LIGHTER            (-2)

// See nsStyleFont
#define NS_STYLE_FONT_SIZE_XXSMALL              0
#define NS_STYLE_FONT_SIZE_XSMALL               1
#define NS_STYLE_FONT_SIZE_SMALL                2
#define NS_STYLE_FONT_SIZE_MEDIUM               3
#define NS_STYLE_FONT_SIZE_LARGE                4
#define NS_STYLE_FONT_SIZE_XLARGE               5
#define NS_STYLE_FONT_SIZE_XXLARGE              6
#define NS_STYLE_FONT_SIZE_XXXLARGE             7  // Only used by <font size="7">. Not specifiable in CSS.
#define NS_STYLE_FONT_SIZE_LARGER               8
#define NS_STYLE_FONT_SIZE_SMALLER              9

// See nsStyleFont
// We should eventually stop using the NS_STYLE_* variants here.
#define NS_STYLE_FONT_STRETCH_ULTRA_CONDENSED   NS_FONT_STRETCH_ULTRA_CONDENSED
#define NS_STYLE_FONT_STRETCH_EXTRA_CONDENSED   NS_FONT_STRETCH_EXTRA_CONDENSED
#define NS_STYLE_FONT_STRETCH_CONDENSED         NS_FONT_STRETCH_CONDENSED
#define NS_STYLE_FONT_STRETCH_SEMI_CONDENSED    NS_FONT_STRETCH_SEMI_CONDENSED
#define NS_STYLE_FONT_STRETCH_NORMAL            NS_FONT_STRETCH_NORMAL
#define NS_STYLE_FONT_STRETCH_SEMI_EXPANDED     NS_FONT_STRETCH_SEMI_EXPANDED
#define NS_STYLE_FONT_STRETCH_EXPANDED          NS_FONT_STRETCH_EXPANDED
#define NS_STYLE_FONT_STRETCH_EXTRA_EXPANDED    NS_FONT_STRETCH_EXTRA_EXPANDED
#define NS_STYLE_FONT_STRETCH_ULTRA_EXPANDED    NS_FONT_STRETCH_ULTRA_EXPANDED

// See nsStyleFont - system fonts
#define NS_STYLE_FONT_CAPTION                   1   // css2
#define NS_STYLE_FONT_ICON                      2
#define NS_STYLE_FONT_MENU                      3
#define NS_STYLE_FONT_MESSAGE_BOX               4
#define NS_STYLE_FONT_SMALL_CAPTION             5
#define NS_STYLE_FONT_STATUS_BAR                6
#define NS_STYLE_FONT_WINDOW                    7   // css3
#define NS_STYLE_FONT_DOCUMENT                  8
#define NS_STYLE_FONT_WORKSPACE                 9
#define NS_STYLE_FONT_DESKTOP                   10
#define NS_STYLE_FONT_INFO                      11
#define NS_STYLE_FONT_DIALOG                    12
#define NS_STYLE_FONT_BUTTON                    13
#define NS_STYLE_FONT_PULL_DOWN_MENU            14
#define NS_STYLE_FONT_LIST                      15
#define NS_STYLE_FONT_FIELD                     16

// grid-auto-flow keywords
#define NS_STYLE_GRID_AUTO_FLOW_ROW             (1 << 0)
#define NS_STYLE_GRID_AUTO_FLOW_COLUMN          (1 << 1)
#define NS_STYLE_GRID_AUTO_FLOW_DENSE           (1 << 2)

// 'subgrid' keyword in grid-template-{columns,rows}
#define NS_STYLE_GRID_TEMPLATE_SUBGRID          0

// CSS Grid <track-breadth> keywords
// Should not overlap with NS_STYLE_GRID_TEMPLATE_SUBGRID
#define NS_STYLE_GRID_TRACK_BREADTH_MAX_CONTENT 1
#define NS_STYLE_GRID_TRACK_BREADTH_MIN_CONTENT 2

// CSS Grid keywords for <auto-repeat>
#define NS_STYLE_GRID_REPEAT_AUTO_FILL          0
#define NS_STYLE_GRID_REPEAT_AUTO_FIT           1

// defaults per MathML spec
#define NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER 0.71f
#define NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT 8

// See nsStyleFont
#define NS_MATHML_MATHVARIANT_NONE                     0
#define NS_MATHML_MATHVARIANT_NORMAL                   1
#define NS_MATHML_MATHVARIANT_BOLD                     2
#define NS_MATHML_MATHVARIANT_ITALIC                   3
#define NS_MATHML_MATHVARIANT_BOLD_ITALIC              4
#define NS_MATHML_MATHVARIANT_SCRIPT                   5
#define NS_MATHML_MATHVARIANT_BOLD_SCRIPT              6
#define NS_MATHML_MATHVARIANT_FRAKTUR                  7
#define NS_MATHML_MATHVARIANT_DOUBLE_STRUCK            8
#define NS_MATHML_MATHVARIANT_BOLD_FRAKTUR             9
#define NS_MATHML_MATHVARIANT_SANS_SERIF              10
#define NS_MATHML_MATHVARIANT_BOLD_SANS_SERIF         11
#define NS_MATHML_MATHVARIANT_SANS_SERIF_ITALIC       12
#define NS_MATHML_MATHVARIANT_SANS_SERIF_BOLD_ITALIC  13
#define NS_MATHML_MATHVARIANT_MONOSPACE               14
#define NS_MATHML_MATHVARIANT_INITIAL                 15
#define NS_MATHML_MATHVARIANT_TAILED                  16
#define NS_MATHML_MATHVARIANT_LOOPED                  17
#define NS_MATHML_MATHVARIANT_STRETCHED               18

// See nsStyleFont::mMathDisplay
#define NS_MATHML_DISPLAYSTYLE_INLINE           0
#define NS_MATHML_DISPLAYSTYLE_BLOCK            1

// See nsStylePosition::mWidth, mMinWidth, mMaxWidth
#define NS_STYLE_WIDTH_MAX_CONTENT              0
#define NS_STYLE_WIDTH_MIN_CONTENT              1
#define NS_STYLE_WIDTH_FIT_CONTENT              2
#define NS_STYLE_WIDTH_AVAILABLE                3

// See nsStyleDisplay.mPosition
#define NS_STYLE_POSITION_STATIC                0
#define NS_STYLE_POSITION_RELATIVE              1
#define NS_STYLE_POSITION_ABSOLUTE              2
#define NS_STYLE_POSITION_FIXED                 3
#define NS_STYLE_POSITION_STICKY                4

// See nsStyleEffects.mClip, mClipFlags
#define NS_STYLE_CLIP_AUTO                      0x00
#define NS_STYLE_CLIP_RECT                      0x01
#define NS_STYLE_CLIP_TYPE_MASK                 0x0F
#define NS_STYLE_CLIP_LEFT_AUTO                 0x10
#define NS_STYLE_CLIP_TOP_AUTO                  0x20
#define NS_STYLE_CLIP_RIGHT_AUTO                0x40
#define NS_STYLE_CLIP_BOTTOM_AUTO               0x80

// FRAME/FRAMESET/IFRAME specific values including backward compatibility. Boolean values with
// the same meaning (e.g. 1 & yes) may need to be distinguished for correct mode processing
#define NS_STYLE_FRAME_YES                      0
#define NS_STYLE_FRAME_NO                       1
#define NS_STYLE_FRAME_0                        2
#define NS_STYLE_FRAME_1                        3
#define NS_STYLE_FRAME_ON                       4
#define NS_STYLE_FRAME_OFF                      5
#define NS_STYLE_FRAME_AUTO                     6
#define NS_STYLE_FRAME_SCROLL                   7
#define NS_STYLE_FRAME_NOSCROLL                 8

// See nsStyleDisplay.mOverflow
#define NS_STYLE_OVERFLOW_VISIBLE               0
#define NS_STYLE_OVERFLOW_HIDDEN                1
#define NS_STYLE_OVERFLOW_SCROLL                2
#define NS_STYLE_OVERFLOW_AUTO                  3
#define NS_STYLE_OVERFLOW_CLIP                  4
#define NS_STYLE_OVERFLOW_SCROLLBARS_HORIZONTAL 5
#define NS_STYLE_OVERFLOW_SCROLLBARS_VERTICAL   6

// See nsStyleDisplay.mOverflowClipBox
#define NS_STYLE_OVERFLOW_CLIP_BOX_PADDING_BOX  0
#define NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX  1

// See nsStyleList
#define NS_STYLE_LIST_STYLE_CUSTOM                -1 // for @counter-style
#define NS_STYLE_LIST_STYLE_NONE                  0
#define NS_STYLE_LIST_STYLE_DISC                  1
#define NS_STYLE_LIST_STYLE_CIRCLE                2
#define NS_STYLE_LIST_STYLE_SQUARE                3
#define NS_STYLE_LIST_STYLE_DECIMAL               4
#define NS_STYLE_LIST_STYLE_HEBREW                5
#define NS_STYLE_LIST_STYLE_JAPANESE_INFORMAL     6
#define NS_STYLE_LIST_STYLE_JAPANESE_FORMAL       7
#define NS_STYLE_LIST_STYLE_KOREAN_HANGUL_FORMAL  8
#define NS_STYLE_LIST_STYLE_KOREAN_HANJA_INFORMAL 9
#define NS_STYLE_LIST_STYLE_KOREAN_HANJA_FORMAL   10
#define NS_STYLE_LIST_STYLE_SIMP_CHINESE_INFORMAL 11
#define NS_STYLE_LIST_STYLE_SIMP_CHINESE_FORMAL   12
#define NS_STYLE_LIST_STYLE_TRAD_CHINESE_INFORMAL 13
#define NS_STYLE_LIST_STYLE_TRAD_CHINESE_FORMAL   14
#define NS_STYLE_LIST_STYLE_ETHIOPIC_NUMERIC      15
#define NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED     16
#define NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN       17
#define NS_STYLE_LIST_STYLE__MAX                  18
// These styles are handled as custom styles defined in counterstyles.css.
// They are preserved here only for html attribute map.
#define NS_STYLE_LIST_STYLE_LOWER_ROMAN           100
#define NS_STYLE_LIST_STYLE_UPPER_ROMAN           101
#define NS_STYLE_LIST_STYLE_LOWER_ALPHA           102
#define NS_STYLE_LIST_STYLE_UPPER_ALPHA           103

// See nsStyleList
#define NS_STYLE_LIST_STYLE_POSITION_INSIDE     0
#define NS_STYLE_LIST_STYLE_POSITION_OUTSIDE    1

// See nsStyleMargin
#define NS_STYLE_MARGIN_SIZE_AUTO               0

// See nsStyleVisibility
#define NS_STYLE_POINTER_EVENTS_NONE            0
#define NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED  1
#define NS_STYLE_POINTER_EVENTS_VISIBLEFILL     2
#define NS_STYLE_POINTER_EVENTS_VISIBLESTROKE   3
#define NS_STYLE_POINTER_EVENTS_VISIBLE         4
#define NS_STYLE_POINTER_EVENTS_PAINTED         5
#define NS_STYLE_POINTER_EVENTS_FILL            6
#define NS_STYLE_POINTER_EVENTS_STROKE          7
#define NS_STYLE_POINTER_EVENTS_ALL             8
#define NS_STYLE_POINTER_EVENTS_AUTO            9

// See nsStyleVisibility.mImageOrientationType
#define NS_STYLE_IMAGE_ORIENTATION_FLIP         0
#define NS_STYLE_IMAGE_ORIENTATION_FROM_IMAGE   1

// See nsStyleDisplay
#define NS_STYLE_ISOLATION_AUTO                 0
#define NS_STYLE_ISOLATION_ISOLATE              1

// See nsStylePosition.mObjectFit
#define NS_STYLE_OBJECT_FIT_FILL                0
#define NS_STYLE_OBJECT_FIT_CONTAIN             1
#define NS_STYLE_OBJECT_FIT_COVER               2
#define NS_STYLE_OBJECT_FIT_NONE                3
#define NS_STYLE_OBJECT_FIT_SCALE_DOWN          4

// See nsStyleDisplay
#define NS_STYLE_RESIZE_NONE                    0
#define NS_STYLE_RESIZE_BOTH                    1
#define NS_STYLE_RESIZE_HORIZONTAL              2
#define NS_STYLE_RESIZE_VERTICAL                3

// See nsStyleText
#define NS_STYLE_TEXT_ALIGN_START                 0
#define NS_STYLE_TEXT_ALIGN_LEFT                  1
#define NS_STYLE_TEXT_ALIGN_RIGHT                 2
#define NS_STYLE_TEXT_ALIGN_CENTER                3
#define NS_STYLE_TEXT_ALIGN_JUSTIFY               4
#define NS_STYLE_TEXT_ALIGN_CHAR                  5   //align based on a certain character, for table cell
#define NS_STYLE_TEXT_ALIGN_END                   6
#define NS_STYLE_TEXT_ALIGN_AUTO                  7
#define NS_STYLE_TEXT_ALIGN_MOZ_CENTER            8
#define NS_STYLE_TEXT_ALIGN_MOZ_RIGHT             9
#define NS_STYLE_TEXT_ALIGN_MOZ_LEFT             10
// NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT is only used in data structs; it
// is never present in stylesheets or computed data.
#define NS_STYLE_TEXT_ALIGN_MOZ_CENTER_OR_INHERIT 11
#define NS_STYLE_TEXT_ALIGN_UNSAFE                12
#define NS_STYLE_TEXT_ALIGN_MATCH_PARENT          13
// Note: make sure that the largest NS_STYLE_TEXT_ALIGN_* value is smaller than
// the smallest NS_STYLE_VERTICAL_ALIGN_* value below!

// See nsStyleText, nsStyleFont
#define NS_STYLE_TEXT_DECORATION_LINE_NONE         0
#define NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE    0x01
#define NS_STYLE_TEXT_DECORATION_LINE_OVERLINE     0x02
#define NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH 0x04
#define NS_STYLE_TEXT_DECORATION_LINE_BLINK        0x08
#define NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS 0x10
// OVERRIDE_ALL does not occur in stylesheets; it only comes from HTML
// attribute mapping (and thus appears in computed data)
#define NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL 0x20
#define NS_STYLE_TEXT_DECORATION_LINE_LINES_MASK   (NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE | NS_STYLE_TEXT_DECORATION_LINE_OVERLINE | NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH)

// See nsStyleText
#define NS_STYLE_TEXT_DECORATION_STYLE_NONE     0 // not in CSS spec, mapped to -moz-none
#define NS_STYLE_TEXT_DECORATION_STYLE_DOTTED   1
#define NS_STYLE_TEXT_DECORATION_STYLE_DASHED   2
#define NS_STYLE_TEXT_DECORATION_STYLE_SOLID    3
#define NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE   4
#define NS_STYLE_TEXT_DECORATION_STYLE_WAVY     5
#define NS_STYLE_TEXT_DECORATION_STYLE_MAX      NS_STYLE_TEXT_DECORATION_STYLE_WAVY

// See nsStyleTextOverflow
#define NS_STYLE_TEXT_OVERFLOW_CLIP     0
#define NS_STYLE_TEXT_OVERFLOW_ELLIPSIS 1
#define NS_STYLE_TEXT_OVERFLOW_STRING   2

// See nsStyleText
#define NS_STYLE_TEXT_TRANSFORM_NONE            0
#define NS_STYLE_TEXT_TRANSFORM_CAPITALIZE      1
#define NS_STYLE_TEXT_TRANSFORM_LOWERCASE       2
#define NS_STYLE_TEXT_TRANSFORM_UPPERCASE       3
#define NS_STYLE_TEXT_TRANSFORM_FULL_WIDTH      4

// See nsStyleDisplay
#define NS_STYLE_TOUCH_ACTION_NONE            (1 << 0)
#define NS_STYLE_TOUCH_ACTION_AUTO            (1 << 1)
#define NS_STYLE_TOUCH_ACTION_PAN_X           (1 << 2)
#define NS_STYLE_TOUCH_ACTION_PAN_Y           (1 << 3)
#define NS_STYLE_TOUCH_ACTION_MANIPULATION    (1 << 4)

// See nsStyleDisplay
#define NS_STYLE_TOP_LAYER_NONE   0 // not in the top layer
#define NS_STYLE_TOP_LAYER_TOP    1 // in the top layer

// See nsStyleDisplay
#define NS_STYLE_TRANSFORM_BOX_BORDER_BOX                0
#define NS_STYLE_TRANSFORM_BOX_FILL_BOX                  1
#define NS_STYLE_TRANSFORM_BOX_VIEW_BOX                  2

// See nsStyleDisplay
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE         0
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR       1
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN      2
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_OUT     3
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE_IN_OUT  4
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START   5
#define NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END     6

// See nsStyleText
// Note: these values pickup after the text-align values because there
// are a few html cases where an object can have both types of
// alignment applied with a single attribute
#define NS_STYLE_VERTICAL_ALIGN_BASELINE             14
#define NS_STYLE_VERTICAL_ALIGN_SUB                  15
#define NS_STYLE_VERTICAL_ALIGN_SUPER                16
#define NS_STYLE_VERTICAL_ALIGN_TOP                  17
#define NS_STYLE_VERTICAL_ALIGN_TEXT_TOP             18
#define NS_STYLE_VERTICAL_ALIGN_MIDDLE               19
#define NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM          20
#define NS_STYLE_VERTICAL_ALIGN_BOTTOM               21
#define NS_STYLE_VERTICAL_ALIGN_MIDDLE_WITH_BASELINE 22

// See nsStyleVisibility
#define NS_STYLE_VISIBILITY_HIDDEN              0
#define NS_STYLE_VISIBILITY_VISIBLE             1
#define NS_STYLE_VISIBILITY_COLLAPSE            2

// See nsStyleText
#define NS_STYLE_TABSIZE_INITIAL                8

// See nsStyleText
#define NS_STYLE_WHITESPACE_NORMAL               0
#define NS_STYLE_WHITESPACE_PRE                  1
#define NS_STYLE_WHITESPACE_NOWRAP               2
#define NS_STYLE_WHITESPACE_PRE_WRAP             3
#define NS_STYLE_WHITESPACE_PRE_LINE             4
#define NS_STYLE_WHITESPACE_PRE_SPACE            5

// See nsStyleText
#define NS_STYLE_WORDBREAK_NORMAL               0
#define NS_STYLE_WORDBREAK_BREAK_ALL            1
#define NS_STYLE_WORDBREAK_KEEP_ALL             2

// See nsStyleText
#define NS_STYLE_OVERFLOWWRAP_NORMAL            0
#define NS_STYLE_OVERFLOWWRAP_BREAK_WORD        1

// See nsStyleText
#define NS_STYLE_HYPHENS_NONE                   0
#define NS_STYLE_HYPHENS_MANUAL                 1
#define NS_STYLE_HYPHENS_AUTO                   2

// ruby-align, see nsStyleText
#define NS_STYLE_RUBY_ALIGN_START               0
#define NS_STYLE_RUBY_ALIGN_CENTER              1
#define NS_STYLE_RUBY_ALIGN_SPACE_BETWEEN       2
#define NS_STYLE_RUBY_ALIGN_SPACE_AROUND        3

// ruby-position, see nsStyleText
#define NS_STYLE_RUBY_POSITION_OVER             0
#define NS_STYLE_RUBY_POSITION_UNDER            1
#define NS_STYLE_RUBY_POSITION_INTER_CHARACTER  2 /* placeholder, not yet parsed */

// See nsStyleText
#define NS_STYLE_TEXT_SIZE_ADJUST_NONE          0
#define NS_STYLE_TEXT_SIZE_ADJUST_AUTO          1

// See nsStyleText
#define NS_STYLE_TEXT_ORIENTATION_MIXED          0
#define NS_STYLE_TEXT_ORIENTATION_UPRIGHT        1
#define NS_STYLE_TEXT_ORIENTATION_SIDEWAYS       2

// See nsStyleText
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE        0
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL         1
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_2    2
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_3    3
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_4    4

// See nsStyleText
#define NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT       0

// See nsStyleText
#define NS_STYLE_UNICODE_BIDI_NORMAL            0x0
#define NS_STYLE_UNICODE_BIDI_EMBED             0x1
#define NS_STYLE_UNICODE_BIDI_ISOLATE           0x2
#define NS_STYLE_UNICODE_BIDI_BIDI_OVERRIDE     0x4
#define NS_STYLE_UNICODE_BIDI_ISOLATE_OVERRIDE  0x6
#define NS_STYLE_UNICODE_BIDI_PLAINTEXT         0x8

#define NS_STYLE_TABLE_LAYOUT_AUTO              0
#define NS_STYLE_TABLE_LAYOUT_FIXED             1

#define NS_STYLE_TABLE_EMPTY_CELLS_HIDE            0
#define NS_STYLE_TABLE_EMPTY_CELLS_SHOW            1

// Constants for the caption-side property. Note that despite having "physical"
// names, these are actually interpreted according to the table's writing-mode:
// TOP and BOTTOM are treated as block-start and -end respectively, and LEFT
// and RIGHT are treated as line-left and -right.
#define NS_STYLE_CAPTION_SIDE_TOP               0
#define NS_STYLE_CAPTION_SIDE_RIGHT             1
#define NS_STYLE_CAPTION_SIDE_BOTTOM            2
#define NS_STYLE_CAPTION_SIDE_LEFT              3
#define NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE       4
#define NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE    5

// constants for cell "scope" attribute
#define NS_STYLE_CELL_SCOPE_ROW                 0
#define NS_STYLE_CELL_SCOPE_COL                 1
#define NS_STYLE_CELL_SCOPE_ROWGROUP            2
#define NS_STYLE_CELL_SCOPE_COLGROUP            3

// See nsStylePage
#define NS_STYLE_PAGE_MARKS_NONE                0x00
#define NS_STYLE_PAGE_MARKS_CROP                0x01
#define NS_STYLE_PAGE_MARKS_REGISTER            0x02

// See nsStylePage
#define NS_STYLE_PAGE_SIZE_AUTO                 0
#define NS_STYLE_PAGE_SIZE_PORTRAIT             1
#define NS_STYLE_PAGE_SIZE_LANDSCAPE            2

// See nsStyleBreaks
#define NS_STYLE_PAGE_BREAK_AUTO                0
#define NS_STYLE_PAGE_BREAK_ALWAYS              1
#define NS_STYLE_PAGE_BREAK_AVOID               2
#define NS_STYLE_PAGE_BREAK_LEFT                3
#define NS_STYLE_PAGE_BREAK_RIGHT               4

// See nsStyleColumn
#define NS_STYLE_COLUMN_COUNT_AUTO              0
#define NS_STYLE_COLUMN_COUNT_UNLIMITED         (-1)

#define NS_STYLE_COLUMN_FILL_AUTO               0
#define NS_STYLE_COLUMN_FILL_BALANCE            1

// See nsStyleUIReset
#define NS_STYLE_IME_MODE_AUTO                  0
#define NS_STYLE_IME_MODE_NORMAL                1
#define NS_STYLE_IME_MODE_ACTIVE                2
#define NS_STYLE_IME_MODE_DISABLED              3
#define NS_STYLE_IME_MODE_INACTIVE              4

// See nsStyleGradient
#define NS_STYLE_GRADIENT_SHAPE_LINEAR          0
#define NS_STYLE_GRADIENT_SHAPE_ELLIPTICAL      1
#define NS_STYLE_GRADIENT_SHAPE_CIRCULAR        2

#define NS_STYLE_GRADIENT_SIZE_CLOSEST_SIDE     0
#define NS_STYLE_GRADIENT_SIZE_CLOSEST_CORNER   1
#define NS_STYLE_GRADIENT_SIZE_FARTHEST_SIDE    2
#define NS_STYLE_GRADIENT_SIZE_FARTHEST_CORNER  3
#define NS_STYLE_GRADIENT_SIZE_EXPLICIT_SIZE    4

// See nsStyleSVG

// dominant-baseline
#define NS_STYLE_DOMINANT_BASELINE_AUTO              0
#define NS_STYLE_DOMINANT_BASELINE_USE_SCRIPT        1
#define NS_STYLE_DOMINANT_BASELINE_NO_CHANGE         2
#define NS_STYLE_DOMINANT_BASELINE_RESET_SIZE        3
#define NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC       4
#define NS_STYLE_DOMINANT_BASELINE_ALPHABETIC        5
#define NS_STYLE_DOMINANT_BASELINE_HANGING           6
#define NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL      7
#define NS_STYLE_DOMINANT_BASELINE_CENTRAL           8
#define NS_STYLE_DOMINANT_BASELINE_MIDDLE            9
#define NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE  10
#define NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE 11

// image-rendering
#define NS_STYLE_IMAGE_RENDERING_AUTO             0
#define NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED    1
#define NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY  2
#define NS_STYLE_IMAGE_RENDERING_CRISPEDGES       3

// mask-type
#define NS_STYLE_MASK_TYPE_LUMINANCE            0
#define NS_STYLE_MASK_TYPE_ALPHA                1

// paint-order
#define NS_STYLE_PAINT_ORDER_NORMAL             0
#define NS_STYLE_PAINT_ORDER_FILL               1
#define NS_STYLE_PAINT_ORDER_STROKE             2
#define NS_STYLE_PAINT_ORDER_MARKERS            3
#define NS_STYLE_PAINT_ORDER_LAST_VALUE NS_STYLE_PAINT_ORDER_MARKERS
// NS_STYLE_PAINT_ORDER_BITWIDTH is the number of bits required to store
// a single paint-order component value.
#define NS_STYLE_PAINT_ORDER_BITWIDTH           2

// shape-rendering
#define NS_STYLE_SHAPE_RENDERING_AUTO               0
#define NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED      1
#define NS_STYLE_SHAPE_RENDERING_CRISPEDGES         2
#define NS_STYLE_SHAPE_RENDERING_GEOMETRICPRECISION 3

// stroke-linecap
#define NS_STYLE_STROKE_LINECAP_BUTT            0
#define NS_STYLE_STROKE_LINECAP_ROUND           1
#define NS_STYLE_STROKE_LINECAP_SQUARE          2

// stroke-linejoin
#define NS_STYLE_STROKE_LINEJOIN_MITER          0
#define NS_STYLE_STROKE_LINEJOIN_ROUND          1
#define NS_STYLE_STROKE_LINEJOIN_BEVEL          2

// stroke-dasharray, stroke-dashoffset, stroke-width
#define NS_STYLE_STROKE_PROP_CONTEXT_VALUE      0

// text-anchor
#define NS_STYLE_TEXT_ANCHOR_START              0
#define NS_STYLE_TEXT_ANCHOR_MIDDLE             1
#define NS_STYLE_TEXT_ANCHOR_END                2

// text-emphasis-position
#define NS_STYLE_TEXT_EMPHASIS_POSITION_OVER    (1 << 0)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER   (1 << 1)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT    (1 << 2)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT   (1 << 3)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT \
  (NS_STYLE_TEXT_EMPHASIS_POSITION_OVER | \
   NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT_ZH \
  (NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER | \
   NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT)

// text-emphasis-style
// Note that filled and none here both have zero as their value. This is
// not an problem because:
// * In specified style, none is represented as eCSSUnit_None.
// * In computed style, 'filled' always has its shape computed, and thus
//   the combined value is never zero.
#define NS_STYLE_TEXT_EMPHASIS_STYLE_NONE           0
#define NS_STYLE_TEXT_EMPHASIS_STYLE_FILL_MASK      (1 << 3)
#define NS_STYLE_TEXT_EMPHASIS_STYLE_FILLED         (0 << 3)
#define NS_STYLE_TEXT_EMPHASIS_STYLE_OPEN           (1 << 3)
#define NS_STYLE_TEXT_EMPHASIS_STYLE_SHAPE_MASK     7
#define NS_STYLE_TEXT_EMPHASIS_STYLE_DOT            1
#define NS_STYLE_TEXT_EMPHASIS_STYLE_CIRCLE         2
#define NS_STYLE_TEXT_EMPHASIS_STYLE_DOUBLE_CIRCLE  3
#define NS_STYLE_TEXT_EMPHASIS_STYLE_TRIANGLE       4
#define NS_STYLE_TEXT_EMPHASIS_STYLE_SESAME         5
#define NS_STYLE_TEXT_EMPHASIS_STYLE_STRING         255

// text-rendering
#define NS_STYLE_TEXT_RENDERING_AUTO               0
#define NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED      1
#define NS_STYLE_TEXT_RENDERING_OPTIMIZELEGIBILITY 2
#define NS_STYLE_TEXT_RENDERING_GEOMETRICPRECISION 3

// adjust-color
#define NS_STYLE_COLOR_ADJUST_ECONOMY               0
#define NS_STYLE_COLOR_ADJUST_EXACT                 1

// color-interpolation and color-interpolation-filters
#define NS_STYLE_COLOR_INTERPOLATION_AUTO           0
#define NS_STYLE_COLOR_INTERPOLATION_SRGB           1
#define NS_STYLE_COLOR_INTERPOLATION_LINEARRGB      2

// vector-effect
#define NS_STYLE_VECTOR_EFFECT_NONE                 0
#define NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE   1

// 3d Transforms - Backface visibility
#define NS_STYLE_BACKFACE_VISIBILITY_VISIBLE        1
#define NS_STYLE_BACKFACE_VISIBILITY_HIDDEN         0

#define NS_STYLE_TRANSFORM_STYLE_FLAT               0
#define NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D        1

// object {fill,stroke}-opacity inherited from context for SVG glyphs
#define NS_STYLE_CONTEXT_FILL_OPACITY               0
#define NS_STYLE_CONTEXT_STROKE_OPACITY             1

// blending
#define NS_STYLE_BLEND_NORMAL                       0
#define NS_STYLE_BLEND_MULTIPLY                     1
#define NS_STYLE_BLEND_SCREEN                       2
#define NS_STYLE_BLEND_OVERLAY                      3
#define NS_STYLE_BLEND_DARKEN                       4
#define NS_STYLE_BLEND_LIGHTEN                      5
#define NS_STYLE_BLEND_COLOR_DODGE                  6
#define NS_STYLE_BLEND_COLOR_BURN                   7
#define NS_STYLE_BLEND_HARD_LIGHT                   8
#define NS_STYLE_BLEND_SOFT_LIGHT                   9
#define NS_STYLE_BLEND_DIFFERENCE                   10
#define NS_STYLE_BLEND_EXCLUSION                    11
#define NS_STYLE_BLEND_HUE                          12
#define NS_STYLE_BLEND_SATURATION                   13
#define NS_STYLE_BLEND_COLOR                        14
#define NS_STYLE_BLEND_LUMINOSITY                   15

// composite
#define NS_STYLE_MASK_COMPOSITE_ADD                 0
#define NS_STYLE_MASK_COMPOSITE_SUBTRACT            1
#define NS_STYLE_MASK_COMPOSITE_INTERSECT           2
#define NS_STYLE_MASK_COMPOSITE_EXCLUDE             3

// See nsStyleText::mControlCharacterVisibility
#define NS_STYLE_CONTROL_CHARACTER_VISIBILITY_HIDDEN  0
#define NS_STYLE_CONTROL_CHARACTER_VISIBILITY_VISIBLE 1

// counter system
#define NS_STYLE_COUNTER_SYSTEM_CYCLIC      0
#define NS_STYLE_COUNTER_SYSTEM_NUMERIC     1
#define NS_STYLE_COUNTER_SYSTEM_ALPHABETIC  2
#define NS_STYLE_COUNTER_SYSTEM_SYMBOLIC    3
#define NS_STYLE_COUNTER_SYSTEM_ADDITIVE    4
#define NS_STYLE_COUNTER_SYSTEM_FIXED       5
#define NS_STYLE_COUNTER_SYSTEM_EXTENDS     6

#define NS_STYLE_COUNTER_RANGE_INFINITE     0

#define NS_STYLE_COUNTER_SPEAKAS_BULLETS    0
#define NS_STYLE_COUNTER_SPEAKAS_NUMBERS    1
#define NS_STYLE_COUNTER_SPEAKAS_WORDS      2
#define NS_STYLE_COUNTER_SPEAKAS_SPELL_OUT  3
#define NS_STYLE_COUNTER_SPEAKAS_OTHER      255 // refer to another style

// See nsStyleDisplay::mScrollBehavior
#define NS_STYLE_SCROLL_BEHAVIOR_AUTO       0
#define NS_STYLE_SCROLL_BEHAVIOR_SMOOTH     1

// See nsStyleDisplay::mScrollSnapType{X,Y}
#define NS_STYLE_SCROLL_SNAP_TYPE_NONE              0
#define NS_STYLE_SCROLL_SNAP_TYPE_MANDATORY         1
#define NS_STYLE_SCROLL_SNAP_TYPE_PROXIMITY         2

/*****************************************************************************
 * Constants for media features.                                             *
 *****************************************************************************/

// orientation
#define NS_STYLE_ORIENTATION_PORTRAIT           0
#define NS_STYLE_ORIENTATION_LANDSCAPE          1

// scan
#define NS_STYLE_SCAN_PROGRESSIVE               0
#define NS_STYLE_SCAN_INTERLACE                 1

// display-mode
#define NS_STYLE_DISPLAY_MODE_BROWSER           0
#define NS_STYLE_DISPLAY_MODE_MINIMAL_UI        1
#define NS_STYLE_DISPLAY_MODE_STANDALONE        2
#define NS_STYLE_DISPLAY_MODE_FULLSCREEN        3

} // namespace mozilla

#endif /* nsStyleConsts_h___ */
