/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants used in the style struct data provided by ComputedStyle */

#ifndef nsStyleConsts_h___
#define nsStyleConsts_h___

#include <inttypes.h>

#include "X11UndefineNone.h"

#include "gfxFontConstants.h"
#include "mozilla/ServoStyleConsts.h"

// XXX fold this into ComputedStyle and group by nsStyleXXX struct

namespace mozilla {

static constexpr uint16_t STYLE_DISPLAY_LIST_ITEM_BIT = 0x8000;
static constexpr uint8_t STYLE_DISPLAY_OUTSIDE_BITS = 7;
static constexpr uint8_t STYLE_DISPLAY_INSIDE_BITS = 8;

// The `display` longhand.
uint16_t constexpr StyleDisplayFrom(StyleDisplayOutside aOuter,
                                    StyleDisplayInside aInner) {
  return uint16_t(uint16_t(aOuter) << STYLE_DISPLAY_INSIDE_BITS) |
         uint16_t(aInner);
}

enum class StyleDisplay : uint16_t {
  // These MUST be in sync with the Rust enum values in
  // servo/components/style/values/specified/box.rs
  /// https://drafts.csswg.org/css-display/#the-display-properties
  None = StyleDisplayFrom(StyleDisplayOutside::None, StyleDisplayInside::None),
  Contents =
      StyleDisplayFrom(StyleDisplayOutside::None, StyleDisplayInside::Contents),
  Inline =
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::Inline),
  InlineBlock = StyleDisplayFrom(StyleDisplayOutside::Inline,
                                 StyleDisplayInside::FlowRoot),
  Block =
      StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Block),
  FlowRoot = StyleDisplayFrom(StyleDisplayOutside::Block,
                              StyleDisplayInside::FlowRoot),
  Flex = StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Flex),
  InlineFlex =
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::Flex),
  Grid = StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Grid),
  InlineGrid =
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::Grid),
  Table =
      StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Table),
  InlineTable =
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::Table),
  TableCaption = StyleDisplayFrom(StyleDisplayOutside::TableCaption,
                                  StyleDisplayInside::Block),
  Ruby =
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::Ruby),
  WebkitBox = StyleDisplayFrom(StyleDisplayOutside::Block,
                               StyleDisplayInside::WebkitBox),
  WebkitInlineBox = StyleDisplayFrom(StyleDisplayOutside::Inline,
                                     StyleDisplayInside::WebkitBox),
  ListItem = Block | STYLE_DISPLAY_LIST_ITEM_BIT,

  /// Internal table boxes.
  TableRowGroup = StyleDisplayFrom(StyleDisplayOutside::InternalTable,
                                   StyleDisplayInside::TableRowGroup),
  TableHeaderGroup = StyleDisplayFrom(StyleDisplayOutside::InternalTable,
                                      StyleDisplayInside::TableHeaderGroup),
  TableFooterGroup = StyleDisplayFrom(StyleDisplayOutside::InternalTable,
                                      StyleDisplayInside::TableFooterGroup),
  TableColumn = StyleDisplayFrom(StyleDisplayOutside::InternalTable,
                                 StyleDisplayInside::TableColumn),
  TableColumnGroup = StyleDisplayFrom(StyleDisplayOutside::InternalTable,
                                      StyleDisplayInside::TableColumnGroup),
  TableRow = StyleDisplayFrom(StyleDisplayOutside::InternalTable,
                              StyleDisplayInside::TableRow),
  TableCell = StyleDisplayFrom(StyleDisplayOutside::InternalTable,
                               StyleDisplayInside::TableCell),

  /// Internal ruby boxes.
  RubyBase = StyleDisplayFrom(StyleDisplayOutside::InternalRuby,
                              StyleDisplayInside::RubyBase),
  RubyBaseContainer = StyleDisplayFrom(StyleDisplayOutside::InternalRuby,
                                       StyleDisplayInside::RubyBaseContainer),
  RubyText = StyleDisplayFrom(StyleDisplayOutside::InternalRuby,
                              StyleDisplayInside::RubyText),
  RubyTextContainer = StyleDisplayFrom(StyleDisplayOutside::InternalRuby,
                                       StyleDisplayInside::RubyTextContainer),

  /// XUL boxes.
  MozBox =
      StyleDisplayFrom(StyleDisplayOutside::XUL, StyleDisplayInside::MozBox),
  MozInlineBox = StyleDisplayFrom(StyleDisplayOutside::XUL,
                                  StyleDisplayInside::MozInlineBox),
  MozGrid =
      StyleDisplayFrom(StyleDisplayOutside::XUL, StyleDisplayInside::MozGrid),
  MozGridGroup = StyleDisplayFrom(StyleDisplayOutside::XUL,
                                  StyleDisplayInside::MozGridGroup),
  MozGridLine = StyleDisplayFrom(StyleDisplayOutside::XUL,
                                 StyleDisplayInside::MozGridLine),
  MozStack =
      StyleDisplayFrom(StyleDisplayOutside::XUL, StyleDisplayInside::MozStack),
  MozDeck =
      StyleDisplayFrom(StyleDisplayOutside::XUL, StyleDisplayInside::MozDeck),
  MozGroupbox = StyleDisplayFrom(StyleDisplayOutside::XUL,
                                 StyleDisplayInside::MozGroupbox),
  MozPopup =
      StyleDisplayFrom(StyleDisplayOutside::XUL, StyleDisplayInside::MozPopup),
};
// The order of the StyleDisplay values isn't meaningful.
bool operator<(const StyleDisplay&, const StyleDisplay&) = delete;
bool operator<=(const StyleDisplay&, const StyleDisplay&) = delete;
bool operator>(const StyleDisplay&, const StyleDisplay&) = delete;
bool operator>=(const StyleDisplay&, const StyleDisplay&) = delete;

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
enum class StyleBoxSizing : uint8_t { Content, Border };

// box-shadow
enum class StyleBoxShadowType : uint8_t {
  Inset,
};

// clear
enum class StyleClear : uint8_t {
  None = 0,
  Left,
  Right,
  Both,
  // StyleClear::Line can be added to one of the other values in layout
  // so it needs to use a bit value that none of the other values can have.
  //
  // FIXME(emilio): Doesn't look like we do that anymore, so probably can be
  // made a single value instead, and Max removed.
  Line = 8,
  Max = 13  // Max = (Both | Line)
};

enum class StyleColumnFill : uint8_t {
  Balance,
  Auto,
};

enum class StyleColumnSpan : uint8_t {
  None,
  All,
};

// Counters and generated content.
enum class StyleContentType : uint8_t {
  String = 1,
  Image = 10,
  Attr = 20,
  Counter = 30,
  Counters = 31,
  OpenQuote = 40,
  CloseQuote = 41,
  NoOpenQuote = 42,
  NoCloseQuote = 43,
  AltContent = 50,
  Uninitialized,
};

// Define geometry box for clip-path's reference-box, background-clip,
// background-origin, mask-clip, mask-origin, shape-box and transform-box.
enum class StyleGeometryBox : uint8_t {
  ContentBox,  // Used by everything, except transform-box.
  PaddingBox,  // Used by everything, except transform-box.
  BorderBox,
  MarginBox,  // XXX Bug 1260094 comment 9.
              // Although margin-box is required by mask-origin and mask-clip,
              // we do not implement that due to lack of support in other
              // browsers. clip-path reference-box only.
  FillBox,    // Used by everything, except shape-box.
  StrokeBox,  // mask-clip, mask-origin and clip-path reference-box only.
  ViewBox,    // Used by everything, except shape-box.
  NoClip,     // mask-clip only.
  Text,       // background-clip only.
  NoBox,      // Depending on which kind of element this style value applied on,
              // the default value of a reference-box can be different.
              // For an HTML element, the default value of reference-box is
              // border-box; for an SVG element, the default value is fill-box.
              // Since we can not determine the default value at parsing time,
              // set it as NoBox so that we make a decision later.
              // clip-path reference-box only.
  MozAlmostPadding = 127  // A magic value that we use for our "pretend that
                          // background-clip is 'padding' when we have a solid
                          // border" optimization.  This isn't actually equal
                          // to StyleGeometryBox::Padding because using that
                          // causes antialiasing seams between the background
                          // and border.
                          // background-clip only.
};

// float-edge
enum class StyleFloatEdge : uint8_t {
  ContentBox,
  MarginBox,
};

// Hyphens
enum class StyleHyphens : uint8_t {
  None,
  Manual,
  Auto,
};

// image-orientation
enum class StyleImageOrientation : uint8_t {
  None,
  FromImage,
};

// scrollbar-width
enum class StyleScrollbarWidth : uint8_t {
  Auto,
  Thin,
  None,
};

// Shape source type
enum class StyleShapeSourceType : uint8_t {
  None,
  Image,  // shape-outside / clip-path only, and clip-path only uses it for
          // <url>s
  Shape,
  Box,
  Path,  // SVG path function
};

// -moz-stack-sizing
enum class StyleStackSizing : uint8_t {
  Ignore,
  StretchToFit,
  IgnoreHorizontal,
  IgnoreVertical,
};

// text-justify
enum class StyleTextJustify : uint8_t {
  None,
  Auto,
  InterWord,
  InterCharacter,
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

// user-input
enum class StyleUserInput : uint8_t {
  None,
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

// See AnimationEffect.webidl
// and mozilla/dom/AnimationEffectBinding.h
namespace dom {
enum class PlaybackDirection : uint8_t;
enum class FillMode : uint8_t;
}  // namespace dom

// Animation play state
enum class StyleAnimationPlayState : uint8_t { Running, Paused };

// See nsStyleImageLayers
enum class StyleImageLayerAttachment : uint8_t { Scroll, Fixed, Local };

// See nsStyleImageLayers
enum class StyleImageLayerRepeat : uint8_t {
  NoRepeat = 0x00,
  RepeatX,
  RepeatY,
  Repeat,
  Space,
  Round
};

// Mask mode
enum class StyleMaskMode : uint8_t { Alpha = 0, Luminance, MatchSource };

// See nsStyleTable
enum class StyleBorderCollapse : uint8_t { Collapse, Separate };

// border-image-repeat
enum class StyleBorderImageRepeat : uint8_t { Stretch, Repeat, Round, Space };

// See nsStyleContent
enum class StyleContent : uint8_t {
  OpenQuote,
  CloseQuote,
  NoOpenQuote,
  NoCloseQuote,
  AltContent
};

// See nsStyleVisibility
#define NS_STYLE_DIRECTION_LTR 0
#define NS_STYLE_DIRECTION_RTL 1

// See nsStyleVisibility
// NOTE: WritingModes.h depends on the particular values used here.
#define NS_STYLE_WRITING_MODE_HORIZONTAL_TB 0
#define NS_STYLE_WRITING_MODE_VERTICAL_RL 1
// #define NS_STYLE_WRITING_MODE_HORIZONTAL_BT  2  // hypothetical
#define NS_STYLE_WRITING_MODE_VERTICAL_LR 3

// Single-bit flag, used in combination with VERTICAL_LR and _RL to specify
// the corresponding SIDEWAYS_* modes.
// (To avoid ambiguity, this bit must be high enough such that no other
// values here accidentally use it in their binary representation.)
#define NS_STYLE_WRITING_MODE_SIDEWAYS_MASK 4

#define NS_STYLE_WRITING_MODE_SIDEWAYS_RL \
  (NS_STYLE_WRITING_MODE_VERTICAL_RL | NS_STYLE_WRITING_MODE_SIDEWAYS_MASK)
#define NS_STYLE_WRITING_MODE_SIDEWAYS_LR \
  (NS_STYLE_WRITING_MODE_VERTICAL_LR | NS_STYLE_WRITING_MODE_SIDEWAYS_MASK)

// Shared constants for all align/justify properties (nsStylePosition):
#define NS_STYLE_ALIGN_AUTO 0
#define NS_STYLE_ALIGN_NORMAL 1
#define NS_STYLE_ALIGN_START 2
#define NS_STYLE_ALIGN_END 3
#define NS_STYLE_ALIGN_FLEX_START 4
#define NS_STYLE_ALIGN_FLEX_END 5
#define NS_STYLE_ALIGN_CENTER 6
#define NS_STYLE_ALIGN_LEFT 7
#define NS_STYLE_ALIGN_RIGHT 8
#define NS_STYLE_ALIGN_BASELINE 9
#define NS_STYLE_ALIGN_LAST_BASELINE 10
#define NS_STYLE_ALIGN_STRETCH 11
#define NS_STYLE_ALIGN_SELF_START 12
#define NS_STYLE_ALIGN_SELF_END 13
#define NS_STYLE_ALIGN_SPACE_BETWEEN 14
#define NS_STYLE_ALIGN_SPACE_AROUND 15
#define NS_STYLE_ALIGN_SPACE_EVENLY 16
#define NS_STYLE_ALIGN_LEGACY 0x20  // mutually exclusive w. SAFE & UNSAFE
#define NS_STYLE_ALIGN_SAFE 0x40
#define NS_STYLE_ALIGN_UNSAFE 0x80  // mutually exclusive w. SAFE
#define NS_STYLE_ALIGN_FLAG_BITS 0xE0
#define NS_STYLE_ALIGN_ALL_BITS 0xFF
#define NS_STYLE_ALIGN_ALL_SHIFT 8

#define NS_STYLE_JUSTIFY_AUTO NS_STYLE_ALIGN_AUTO
#define NS_STYLE_JUSTIFY_NORMAL NS_STYLE_ALIGN_NORMAL
#define NS_STYLE_JUSTIFY_START NS_STYLE_ALIGN_START
#define NS_STYLE_JUSTIFY_END NS_STYLE_ALIGN_END
#define NS_STYLE_JUSTIFY_FLEX_START NS_STYLE_ALIGN_FLEX_START
#define NS_STYLE_JUSTIFY_FLEX_END NS_STYLE_ALIGN_FLEX_END
#define NS_STYLE_JUSTIFY_CENTER NS_STYLE_ALIGN_CENTER
#define NS_STYLE_JUSTIFY_LEFT NS_STYLE_ALIGN_LEFT
#define NS_STYLE_JUSTIFY_RIGHT NS_STYLE_ALIGN_RIGHT
#define NS_STYLE_JUSTIFY_BASELINE NS_STYLE_ALIGN_BASELINE
#define NS_STYLE_JUSTIFY_LAST_BASELINE NS_STYLE_ALIGN_LAST_BASELINE
#define NS_STYLE_JUSTIFY_STRETCH NS_STYLE_ALIGN_STRETCH
#define NS_STYLE_JUSTIFY_SELF_START NS_STYLE_ALIGN_SELF_START
#define NS_STYLE_JUSTIFY_SELF_END NS_STYLE_ALIGN_SELF_END
#define NS_STYLE_JUSTIFY_SPACE_BETWEEN NS_STYLE_ALIGN_SPACE_BETWEEN
#define NS_STYLE_JUSTIFY_SPACE_AROUND NS_STYLE_ALIGN_SPACE_AROUND
#define NS_STYLE_JUSTIFY_SPACE_EVENLY NS_STYLE_ALIGN_SPACE_EVENLY
#define NS_STYLE_JUSTIFY_LEGACY NS_STYLE_ALIGN_LEGACY
#define NS_STYLE_JUSTIFY_SAFE NS_STYLE_ALIGN_SAFE
#define NS_STYLE_JUSTIFY_UNSAFE NS_STYLE_ALIGN_UNSAFE
#define NS_STYLE_JUSTIFY_FLAG_BITS NS_STYLE_ALIGN_FLAG_BITS
#define NS_STYLE_JUSTIFY_ALL_BITS NS_STYLE_ALIGN_ALL_BITS
#define NS_STYLE_JUSTIFY_ALL_SHIFT NS_STYLE_ALIGN_ALL_SHIFT

// See nsStylePosition
enum class StyleFlexDirection : uint8_t {
  Row,
  RowReverse,
  Column,
  ColumnReverse,
};

// See nsStylePosition
#define NS_STYLE_FLEX_WRAP_NOWRAP 0
#define NS_STYLE_FLEX_WRAP_WRAP 1
#define NS_STYLE_FLEX_WRAP_WRAP_REVERSE 2

// See nsStylePosition
// NOTE: This is the initial value of the integer-valued 'order' property
// (rather than an internal numerical representation of some keyword).
#define NS_STYLE_ORDER_INITIAL 0

// See nsStyleFont
#define NS_STYLE_FONT_SIZE_XXSMALL 0
#define NS_STYLE_FONT_SIZE_XSMALL 1
#define NS_STYLE_FONT_SIZE_SMALL 2
#define NS_STYLE_FONT_SIZE_MEDIUM 3
#define NS_STYLE_FONT_SIZE_LARGE 4
#define NS_STYLE_FONT_SIZE_XLARGE 5
#define NS_STYLE_FONT_SIZE_XXLARGE 6
#define NS_STYLE_FONT_SIZE_XXXLARGE \
  7  // Only used by <font size="7">. Not specifiable in CSS.
#define NS_STYLE_FONT_SIZE_LARGER 8
#define NS_STYLE_FONT_SIZE_SMALLER 9
#define NS_STYLE_FONT_SIZE_NO_KEYWORD \
  10  // Used by Servo to track the "no keyword" case

// grid-auto-flow keywords
#define NS_STYLE_GRID_AUTO_FLOW_ROW (1 << 0)
#define NS_STYLE_GRID_AUTO_FLOW_COLUMN (1 << 1)
#define NS_STYLE_GRID_AUTO_FLOW_DENSE (1 << 2)

// 'subgrid' keyword in grid-template-{columns,rows}
#define NS_STYLE_GRID_TEMPLATE_SUBGRID 0

// CSS Grid <track-breadth> keywords
// Should not overlap with NS_STYLE_GRID_TEMPLATE_SUBGRID
enum class StyleGridTrackBreadth : uint8_t {
  MaxContent = 1,
  MinContent = 2,
};

// defaults per MathML spec
#define NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER 0.71f
#define NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT 8

// See nsStyleFont
#define NS_MATHML_MATHVARIANT_NONE 0
#define NS_MATHML_MATHVARIANT_NORMAL 1
#define NS_MATHML_MATHVARIANT_BOLD 2
#define NS_MATHML_MATHVARIANT_ITALIC 3
#define NS_MATHML_MATHVARIANT_BOLD_ITALIC 4
#define NS_MATHML_MATHVARIANT_SCRIPT 5
#define NS_MATHML_MATHVARIANT_BOLD_SCRIPT 6
#define NS_MATHML_MATHVARIANT_FRAKTUR 7
#define NS_MATHML_MATHVARIANT_DOUBLE_STRUCK 8
#define NS_MATHML_MATHVARIANT_BOLD_FRAKTUR 9
#define NS_MATHML_MATHVARIANT_SANS_SERIF 10
#define NS_MATHML_MATHVARIANT_BOLD_SANS_SERIF 11
#define NS_MATHML_MATHVARIANT_SANS_SERIF_ITALIC 12
#define NS_MATHML_MATHVARIANT_SANS_SERIF_BOLD_ITALIC 13
#define NS_MATHML_MATHVARIANT_MONOSPACE 14
#define NS_MATHML_MATHVARIANT_INITIAL 15
#define NS_MATHML_MATHVARIANT_TAILED 16
#define NS_MATHML_MATHVARIANT_LOOPED 17
#define NS_MATHML_MATHVARIANT_STRETCHED 18

// See nsStyleFont::mMathDisplay
#define NS_MATHML_DISPLAYSTYLE_INLINE 0
#define NS_MATHML_DISPLAYSTYLE_BLOCK 1

// See nsStyleDisplay.mPosition
#define NS_STYLE_POSITION_STATIC 0
#define NS_STYLE_POSITION_RELATIVE 1
#define NS_STYLE_POSITION_ABSOLUTE 2
#define NS_STYLE_POSITION_FIXED 3
#define NS_STYLE_POSITION_STICKY 4

// See nsStyleEffects.mClip, mClipFlags
#define NS_STYLE_CLIP_AUTO 0x00
#define NS_STYLE_CLIP_RECT 0x01
#define NS_STYLE_CLIP_TYPE_MASK 0x0F
#define NS_STYLE_CLIP_LEFT_AUTO 0x10
#define NS_STYLE_CLIP_TOP_AUTO 0x20
#define NS_STYLE_CLIP_RIGHT_AUTO 0x40
#define NS_STYLE_CLIP_BOTTOM_AUTO 0x80

// FRAME/FRAMESET/IFRAME specific values including backward compatibility.
// Boolean values with the same meaning (e.g. 1 & yes) may need to be
// distinguished for correct mode processing
#define NS_STYLE_FRAME_YES 0
#define NS_STYLE_FRAME_NO 1
#define NS_STYLE_FRAME_0 2
#define NS_STYLE_FRAME_1 3
#define NS_STYLE_FRAME_ON 4
#define NS_STYLE_FRAME_OFF 5
#define NS_STYLE_FRAME_AUTO 6
#define NS_STYLE_FRAME_SCROLL 7
#define NS_STYLE_FRAME_NOSCROLL 8

// See nsStyleList
#define NS_STYLE_LIST_STYLE_CUSTOM -1  // for @counter-style
#define NS_STYLE_LIST_STYLE_NONE 0
#define NS_STYLE_LIST_STYLE_DECIMAL 1
#define NS_STYLE_LIST_STYLE_DISC 2
#define NS_STYLE_LIST_STYLE_CIRCLE 3
#define NS_STYLE_LIST_STYLE_SQUARE 4
#define NS_STYLE_LIST_STYLE_DISCLOSURE_CLOSED 5
#define NS_STYLE_LIST_STYLE_DISCLOSURE_OPEN 6
#define NS_STYLE_LIST_STYLE_HEBREW 7
#define NS_STYLE_LIST_STYLE_JAPANESE_INFORMAL 8
#define NS_STYLE_LIST_STYLE_JAPANESE_FORMAL 9
#define NS_STYLE_LIST_STYLE_KOREAN_HANGUL_FORMAL 10
#define NS_STYLE_LIST_STYLE_KOREAN_HANJA_INFORMAL 11
#define NS_STYLE_LIST_STYLE_KOREAN_HANJA_FORMAL 12
#define NS_STYLE_LIST_STYLE_SIMP_CHINESE_INFORMAL 13
#define NS_STYLE_LIST_STYLE_SIMP_CHINESE_FORMAL 14
#define NS_STYLE_LIST_STYLE_TRAD_CHINESE_INFORMAL 15
#define NS_STYLE_LIST_STYLE_TRAD_CHINESE_FORMAL 16
#define NS_STYLE_LIST_STYLE_ETHIOPIC_NUMERIC 17
// These styles are handled as custom styles defined in counterstyles.css.
// They are preserved here only for html attribute map.
#define NS_STYLE_LIST_STYLE_LOWER_ROMAN 100
#define NS_STYLE_LIST_STYLE_UPPER_ROMAN 101
#define NS_STYLE_LIST_STYLE_LOWER_ALPHA 102
#define NS_STYLE_LIST_STYLE_UPPER_ALPHA 103

// See nsStyleList
#define NS_STYLE_LIST_STYLE_POSITION_INSIDE 0
#define NS_STYLE_LIST_STYLE_POSITION_OUTSIDE 1

// See nsStyleVisibility
#define NS_STYLE_POINTER_EVENTS_NONE 0
#define NS_STYLE_POINTER_EVENTS_VISIBLEPAINTED 1
#define NS_STYLE_POINTER_EVENTS_VISIBLEFILL 2
#define NS_STYLE_POINTER_EVENTS_VISIBLESTROKE 3
#define NS_STYLE_POINTER_EVENTS_VISIBLE 4
#define NS_STYLE_POINTER_EVENTS_PAINTED 5
#define NS_STYLE_POINTER_EVENTS_FILL 6
#define NS_STYLE_POINTER_EVENTS_STROKE 7
#define NS_STYLE_POINTER_EVENTS_ALL 8
#define NS_STYLE_POINTER_EVENTS_AUTO 9

// See nsStyleVisibility.mImageOrientationType
#define NS_STYLE_IMAGE_ORIENTATION_FLIP 0
#define NS_STYLE_IMAGE_ORIENTATION_FROM_IMAGE 1

// See nsStyleDisplay
#define NS_STYLE_ISOLATION_AUTO 0
#define NS_STYLE_ISOLATION_ISOLATE 1

// See nsStylePosition.mObjectFit
#define NS_STYLE_OBJECT_FIT_FILL 0
#define NS_STYLE_OBJECT_FIT_CONTAIN 1
#define NS_STYLE_OBJECT_FIT_COVER 2
#define NS_STYLE_OBJECT_FIT_NONE 3
#define NS_STYLE_OBJECT_FIT_SCALE_DOWN 4

// See nsStyleText
#define NS_STYLE_TEXT_ALIGN_START 0
#define NS_STYLE_TEXT_ALIGN_LEFT 1
#define NS_STYLE_TEXT_ALIGN_RIGHT 2
#define NS_STYLE_TEXT_ALIGN_CENTER 3
#define NS_STYLE_TEXT_ALIGN_JUSTIFY 4
#define NS_STYLE_TEXT_ALIGN_CHAR \
  5  // align based on a certain character, for table cell
#define NS_STYLE_TEXT_ALIGN_END 6
#define NS_STYLE_TEXT_ALIGN_AUTO 7
#define NS_STYLE_TEXT_ALIGN_MOZ_CENTER 8
#define NS_STYLE_TEXT_ALIGN_MOZ_RIGHT 9
#define NS_STYLE_TEXT_ALIGN_MOZ_LEFT 10

// See nsStyleText
#define NS_STYLE_TEXT_DECORATION_STYLE_NONE \
  0  // not in CSS spec, mapped to -moz-none
#define NS_STYLE_TEXT_DECORATION_STYLE_DOTTED 1
#define NS_STYLE_TEXT_DECORATION_STYLE_DASHED 2
#define NS_STYLE_TEXT_DECORATION_STYLE_SOLID 3
#define NS_STYLE_TEXT_DECORATION_STYLE_DOUBLE 4
#define NS_STYLE_TEXT_DECORATION_STYLE_WAVY 5
#define NS_STYLE_TEXT_DECORATION_STYLE_MAX NS_STYLE_TEXT_DECORATION_STYLE_WAVY

// See nsStyleText
#define NS_STYLE_TEXT_TRANSFORM_NONE 0
#define NS_STYLE_TEXT_TRANSFORM_CAPITALIZE 1
#define NS_STYLE_TEXT_TRANSFORM_LOWERCASE 2
#define NS_STYLE_TEXT_TRANSFORM_UPPERCASE 3
#define NS_STYLE_TEXT_TRANSFORM_FULL_WIDTH 4
#define NS_STYLE_TEXT_TRANSFORM_FULL_SIZE_KANA 5

// See nsStyleDisplay
#define NS_STYLE_TOP_LAYER_NONE 0  // not in the top layer
#define NS_STYLE_TOP_LAYER_TOP 1   // in the top layer

// See nsStyleVisibility
#define NS_STYLE_VISIBILITY_HIDDEN 0
#define NS_STYLE_VISIBILITY_VISIBLE 1
#define NS_STYLE_VISIBILITY_COLLAPSE 2

// See nsStyleText
#define NS_STYLE_TABSIZE_INITIAL 8

// See nsStyleText
enum class StyleWhiteSpace : uint8_t {
  Normal = 0,
  Pre,
  Nowrap,
  PreWrap,
  PreLine,
  PreSpace,
  BreakSpaces,
};

// ruby-align, see nsStyleText
#define NS_STYLE_RUBY_ALIGN_START 0
#define NS_STYLE_RUBY_ALIGN_CENTER 1
#define NS_STYLE_RUBY_ALIGN_SPACE_BETWEEN 2
#define NS_STYLE_RUBY_ALIGN_SPACE_AROUND 3

// ruby-position, see nsStyleText
#define NS_STYLE_RUBY_POSITION_OVER 0
#define NS_STYLE_RUBY_POSITION_UNDER 1
#define NS_STYLE_RUBY_POSITION_INTER_CHARACTER 2  // placeholder, not yet parsed

// See nsStyleText
#define NS_STYLE_TEXT_SIZE_ADJUST_NONE 0
#define NS_STYLE_TEXT_SIZE_ADJUST_AUTO 1

// See nsStyleText
#define NS_STYLE_TEXT_ORIENTATION_MIXED 0
#define NS_STYLE_TEXT_ORIENTATION_UPRIGHT 1
#define NS_STYLE_TEXT_ORIENTATION_SIDEWAYS 2

// See nsStyleText
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE 0
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL 1

// See nsStyleText
#define NS_STYLE_LINE_HEIGHT_BLOCK_HEIGHT 0

// See nsStyleText
#define NS_STYLE_UNICODE_BIDI_NORMAL 0x0
#define NS_STYLE_UNICODE_BIDI_EMBED 0x1
#define NS_STYLE_UNICODE_BIDI_ISOLATE 0x2
#define NS_STYLE_UNICODE_BIDI_BIDI_OVERRIDE 0x4
#define NS_STYLE_UNICODE_BIDI_ISOLATE_OVERRIDE 0x6
#define NS_STYLE_UNICODE_BIDI_PLAINTEXT 0x8

#define NS_STYLE_TABLE_LAYOUT_AUTO 0
#define NS_STYLE_TABLE_LAYOUT_FIXED 1

#define NS_STYLE_TABLE_EMPTY_CELLS_HIDE 0
#define NS_STYLE_TABLE_EMPTY_CELLS_SHOW 1

// Constants for the caption-side property. Note that despite having "physical"
// names, these are actually interpreted according to the table's writing-mode:
// TOP and BOTTOM are treated as block-start and -end respectively, and LEFT
// and RIGHT are treated as line-left and -right.
#define NS_STYLE_CAPTION_SIDE_TOP 0
#define NS_STYLE_CAPTION_SIDE_RIGHT 1
#define NS_STYLE_CAPTION_SIDE_BOTTOM 2
#define NS_STYLE_CAPTION_SIDE_LEFT 3
#define NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE 4
#define NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE 5

// constants for cell "scope" attribute
#define NS_STYLE_CELL_SCOPE_ROW 0
#define NS_STYLE_CELL_SCOPE_COL 1
#define NS_STYLE_CELL_SCOPE_ROWGROUP 2
#define NS_STYLE_CELL_SCOPE_COLGROUP 3

// See nsStylePage
#define NS_STYLE_PAGE_MARKS_NONE 0x00
#define NS_STYLE_PAGE_MARKS_CROP 0x01
#define NS_STYLE_PAGE_MARKS_REGISTER 0x02

// See nsStylePage
#define NS_STYLE_PAGE_SIZE_AUTO 0
#define NS_STYLE_PAGE_SIZE_PORTRAIT 1
#define NS_STYLE_PAGE_SIZE_LANDSCAPE 2

// See nsStyleBreaks
#define NS_STYLE_PAGE_BREAK_AUTO 0
#define NS_STYLE_PAGE_BREAK_ALWAYS 1
#define NS_STYLE_PAGE_BREAK_AVOID 2
#define NS_STYLE_PAGE_BREAK_LEFT 3
#define NS_STYLE_PAGE_BREAK_RIGHT 4

// See nsStyleUIReset
#define NS_STYLE_IME_MODE_AUTO 0
#define NS_STYLE_IME_MODE_NORMAL 1
#define NS_STYLE_IME_MODE_ACTIVE 2
#define NS_STYLE_IME_MODE_DISABLED 3
#define NS_STYLE_IME_MODE_INACTIVE 4

// See nsStyleSVG

/*
 * -moz-window-shadow
 * Also used in widget code
 */

#define NS_STYLE_WINDOW_SHADOW_NONE 0
#define NS_STYLE_WINDOW_SHADOW_DEFAULT 1
#define NS_STYLE_WINDOW_SHADOW_MENU 2
#define NS_STYLE_WINDOW_SHADOW_TOOLTIP 3
#define NS_STYLE_WINDOW_SHADOW_SHEET 4

// dominant-baseline
#define NS_STYLE_DOMINANT_BASELINE_AUTO 0
#define NS_STYLE_DOMINANT_BASELINE_IDEOGRAPHIC 1
#define NS_STYLE_DOMINANT_BASELINE_ALPHABETIC 2
#define NS_STYLE_DOMINANT_BASELINE_HANGING 3
#define NS_STYLE_DOMINANT_BASELINE_MATHEMATICAL 4
#define NS_STYLE_DOMINANT_BASELINE_CENTRAL 5
#define NS_STYLE_DOMINANT_BASELINE_MIDDLE 6
#define NS_STYLE_DOMINANT_BASELINE_TEXT_AFTER_EDGE 7
#define NS_STYLE_DOMINANT_BASELINE_TEXT_BEFORE_EDGE 8

// image-rendering
#define NS_STYLE_IMAGE_RENDERING_AUTO 0
#define NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED 1
#define NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY 2
#define NS_STYLE_IMAGE_RENDERING_CRISP_EDGES 3

// mask-type
#define NS_STYLE_MASK_TYPE_LUMINANCE 0
#define NS_STYLE_MASK_TYPE_ALPHA 1

// shape-rendering
#define NS_STYLE_SHAPE_RENDERING_AUTO 0
#define NS_STYLE_SHAPE_RENDERING_OPTIMIZESPEED 1
#define NS_STYLE_SHAPE_RENDERING_CRISPEDGES 2
#define NS_STYLE_SHAPE_RENDERING_GEOMETRICPRECISION 3

// stroke-linecap
#define NS_STYLE_STROKE_LINECAP_BUTT 0
#define NS_STYLE_STROKE_LINECAP_ROUND 1
#define NS_STYLE_STROKE_LINECAP_SQUARE 2

// stroke-linejoin
#define NS_STYLE_STROKE_LINEJOIN_MITER 0
#define NS_STYLE_STROKE_LINEJOIN_ROUND 1
#define NS_STYLE_STROKE_LINEJOIN_BEVEL 2

// stroke-dasharray, stroke-dashoffset, stroke-width
#define NS_STYLE_STROKE_PROP_CONTEXT_VALUE 0

// text-anchor
#define NS_STYLE_TEXT_ANCHOR_START 0
#define NS_STYLE_TEXT_ANCHOR_MIDDLE 1
#define NS_STYLE_TEXT_ANCHOR_END 2

// text-emphasis-position
#define NS_STYLE_TEXT_EMPHASIS_POSITION_OVER (1 << 0)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER (1 << 1)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT (1 << 2)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT (1 << 3)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT \
  (NS_STYLE_TEXT_EMPHASIS_POSITION_OVER | NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT)
#define NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT_ZH \
  (NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER |         \
   NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT)

// text-rendering
enum class StyleTextRendering : uint8_t {
  Auto,
  Optimizespeed,
  Optimizelegibility,
  Geometricprecision,
};

// color-adjust
enum class StyleColorAdjust : uint8_t {
  Economy = 0,
  Exact = 1,
};

// color-interpolation and color-interpolation-filters
#define NS_STYLE_COLOR_INTERPOLATION_AUTO 0
#define NS_STYLE_COLOR_INTERPOLATION_SRGB 1
#define NS_STYLE_COLOR_INTERPOLATION_LINEARRGB 2

// vector-effect
#define NS_STYLE_VECTOR_EFFECT_NONE 0
#define NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE 1

// 3d Transforms - Backface visibility
#define NS_STYLE_BACKFACE_VISIBILITY_VISIBLE 1
#define NS_STYLE_BACKFACE_VISIBILITY_HIDDEN 0

#define NS_STYLE_TRANSFORM_STYLE_FLAT 0
#define NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D 1

// object {fill,stroke}-opacity inherited from context for SVG glyphs
#define NS_STYLE_CONTEXT_FILL_OPACITY 0
#define NS_STYLE_CONTEXT_STROKE_OPACITY 1

// blending
#define NS_STYLE_BLEND_NORMAL 0
#define NS_STYLE_BLEND_MULTIPLY 1
#define NS_STYLE_BLEND_SCREEN 2
#define NS_STYLE_BLEND_OVERLAY 3
#define NS_STYLE_BLEND_DARKEN 4
#define NS_STYLE_BLEND_LIGHTEN 5
#define NS_STYLE_BLEND_COLOR_DODGE 6
#define NS_STYLE_BLEND_COLOR_BURN 7
#define NS_STYLE_BLEND_HARD_LIGHT 8
#define NS_STYLE_BLEND_SOFT_LIGHT 9
#define NS_STYLE_BLEND_DIFFERENCE 10
#define NS_STYLE_BLEND_EXCLUSION 11
#define NS_STYLE_BLEND_HUE 12
#define NS_STYLE_BLEND_SATURATION 13
#define NS_STYLE_BLEND_COLOR 14
#define NS_STYLE_BLEND_LUMINOSITY 15

// composite
#define NS_STYLE_MASK_COMPOSITE_ADD 0
#define NS_STYLE_MASK_COMPOSITE_SUBTRACT 1
#define NS_STYLE_MASK_COMPOSITE_INTERSECT 2
#define NS_STYLE_MASK_COMPOSITE_EXCLUDE 3

// See nsStyleText::mControlCharacterVisibility
#define NS_STYLE_CONTROL_CHARACTER_VISIBILITY_HIDDEN 0
#define NS_STYLE_CONTROL_CHARACTER_VISIBILITY_VISIBLE 1

// counter system
#define NS_STYLE_COUNTER_SYSTEM_CYCLIC 0
#define NS_STYLE_COUNTER_SYSTEM_NUMERIC 1
#define NS_STYLE_COUNTER_SYSTEM_ALPHABETIC 2
#define NS_STYLE_COUNTER_SYSTEM_SYMBOLIC 3
#define NS_STYLE_COUNTER_SYSTEM_ADDITIVE 4
#define NS_STYLE_COUNTER_SYSTEM_FIXED 5
#define NS_STYLE_COUNTER_SYSTEM_EXTENDS 6

#define NS_STYLE_COUNTER_RANGE_INFINITE 0

#define NS_STYLE_COUNTER_SPEAKAS_BULLETS 0
#define NS_STYLE_COUNTER_SPEAKAS_NUMBERS 1
#define NS_STYLE_COUNTER_SPEAKAS_WORDS 2
#define NS_STYLE_COUNTER_SPEAKAS_SPELL_OUT 3
#define NS_STYLE_COUNTER_SPEAKAS_OTHER 255  // refer to another style

// See nsStyleDisplay::mScrollBehavior
#define NS_STYLE_SCROLL_BEHAVIOR_AUTO 0
#define NS_STYLE_SCROLL_BEHAVIOR_SMOOTH 1

}  // namespace mozilla

#endif /* nsStyleConsts_h___ */
