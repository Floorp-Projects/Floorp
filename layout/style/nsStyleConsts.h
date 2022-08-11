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
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::Flow),
  Block =
      StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Flow),
  FlowRoot = StyleDisplayFrom(StyleDisplayOutside::Block,
                              StyleDisplayInside::FlowRoot),
  Flex = StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Flex),
  Grid = StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Grid),
  Table =
      StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::Table),
  InlineTable =
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::Table),
  TableCaption = StyleDisplayFrom(StyleDisplayOutside::TableCaption,
                                  StyleDisplayInside::Flow),
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
      StyleDisplayFrom(StyleDisplayOutside::Block, StyleDisplayInside::MozBox),
  MozInlineBox =
      StyleDisplayFrom(StyleDisplayOutside::Inline, StyleDisplayInside::MozBox),
  MozDeck =
      StyleDisplayFrom(StyleDisplayOutside::XUL, StyleDisplayInside::MozDeck),
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

// -moz-inert
enum class StyleInert : uint8_t {
  None,
  Inert,
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

// See nsStyleVisibility
enum class StyleDirection : uint8_t { Ltr, Rtl };

// See nsStyleVisibility
// NOTE: WritingModes.h depends on the particular values used here.

// Single-bit flag, used in combination with VerticalLR and RL to specify
// the corresponding Sideways* modes.
// (To avoid ambiguity, this bit must be high enough such that no other
// values here accidentally use it in their binary representation.)
static constexpr uint8_t kWritingModeSidewaysMask = 4;

enum class StyleWritingModeProperty : uint8_t {
  HorizontalTb = 0,
  VerticalRl = 1,
  // HorizontalBT = 2,    // hypothetical
  VerticalLr = 3,
  SidewaysRl = VerticalRl | kWritingModeSidewaysMask,
  SidewaysLr = VerticalLr | kWritingModeSidewaysMask,
};

// See nsStylePosition
enum class StyleFlexDirection : uint8_t {
  Row,
  RowReverse,
  Column,
  ColumnReverse,
};

// See nsStylePosition
enum class StyleFlexWrap : uint8_t {
  Nowrap,
  Wrap,
  WrapReverse,
};

// See nsStylePosition
// NOTE: This is the initial value of the integer-valued 'order' property
// (rather than an internal numerical representation of some keyword).
#define NS_STYLE_ORDER_INITIAL 0

#define NS_STYLE_MASONRY_PLACEMENT_PACK (1 << 0)
#define NS_STYLE_MASONRY_ORDER_DEFINITE_FIRST (1 << 1)
#define NS_STYLE_MASONRY_AUTO_FLOW_INITIAL_VALUE \
  (NS_STYLE_MASONRY_PLACEMENT_PACK | NS_STYLE_MASONRY_ORDER_DEFINITE_FIRST)

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
enum class StyleMathVariant : uint8_t {
  None = 0,
  Normal = 1,
  Bold = 2,
  Italic = 3,
  BoldItalic = 4,
  Script = 5,
  BoldScript = 6,
  Fraktur = 7,
  DoubleStruck = 8,
  BoldFraktur = 9,
  SansSerif = 10,
  BoldSansSerif = 11,
  SansSerifItalic = 12,
  SansSerifBoldItalic = 13,
  Monospace = 14,
  Initial = 15,
  Tailed = 16,
  Looped = 17,
  Stretched = 18,
};

// See nsStyleFont::mMathStyle
#define NS_STYLE_MATH_STYLE_COMPACT 0
#define NS_STYLE_MATH_STYLE_NORMAL 1

// See nsStyleDisplay.mPosition
enum class StylePositionProperty : uint8_t {
  Static,
  Relative,
  Absolute,
  Fixed,
  Sticky,
};

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
enum class StylePointerEvents : uint8_t {
  None,
  Visiblepainted,
  Visiblefill,
  Visiblestroke,
  Visible,
  Painted,
  Fill,
  Stroke,
  All,
  Auto,
};

enum class StyleIsolation : uint8_t {
  Auto,
  Isolate,
};

// See nsStylePosition.mObjectFit
enum class StyleObjectFit : uint8_t {
  Fill,
  Contain,
  Cover,
  None,
  ScaleDown,
};

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
enum class StyleTopLayer : uint8_t {
  None,
  Top,
};

// See nsStyleVisibility
enum class StyleVisibility : uint8_t {
  Hidden,
  Visible,
  Collapse,
};

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
enum class StyleRubyAlign : uint8_t {
  Start,
  Center,
  SpaceBetween,
  SpaceAround,
};

// See nsStyleText
enum class StyleTextSizeAdjust : uint8_t {
  None,
  Auto,
};

// See nsStyleVisibility
enum class StyleTextOrientation : uint8_t {
  Mixed,
  Upright,
  Sideways,
};

// Whether to emulate -moz-box with flex. See nsStyleVisibility
enum class StyleMozBoxLayout : uint8_t {
  Legacy,
  Flex,
};

// See nsStyleText
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE 0
#define NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL 1

// See nsStyleText
#define NS_STYLE_UNICODE_BIDI_NORMAL 0x0
#define NS_STYLE_UNICODE_BIDI_EMBED 0x1
#define NS_STYLE_UNICODE_BIDI_ISOLATE 0x2
#define NS_STYLE_UNICODE_BIDI_BIDI_OVERRIDE 0x4
#define NS_STYLE_UNICODE_BIDI_ISOLATE_OVERRIDE 0x6
#define NS_STYLE_UNICODE_BIDI_PLAINTEXT 0x8

enum class StyleTableLayout : uint8_t {
  Auto,
  Fixed,
};

enum class StyleEmptyCells : uint8_t {
  Hide,
  Show,
};

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
enum class StyleImeMode : uint8_t {
  Auto,
  Normal,
  Active,
  Disabled,
  Inactive,
};

// See nsStyleSVG

/*
 * -moz-window-shadow
 * Also used in widget code
 */
enum class StyleWindowShadow : uint8_t {
  None,
  Default,

  // These can't be specified in CSS, they get computed from the "default"
  // value.
  Menu,
  Tooltip,
};

// dominant-baseline
enum class StyleDominantBaseline : uint8_t {
  Auto,
  Ideographic,
  Alphabetic,
  Hanging,
  Mathematical,
  Central,
  Middle,
  TextAfterEdge,
  TextBeforeEdge,
};

// mask-type
enum class StyleMaskType : uint8_t {
  Luminance,
  Alpha,
};

// shape-rendering
enum class StyleShapeRendering : uint8_t {
  Auto,
  Optimizespeed,
  Crispedges,
  Geometricprecision,
};

// stroke-linecap
enum class StyleStrokeLinecap : uint8_t {
  Butt,
  Round,
  Square,
};

// stroke-linejoin
enum class StyleStrokeLinejoin : uint8_t {
  Miter,
  Round,
  Bevel,
};

// text-anchor
enum class StyleTextAnchor : uint8_t {
  Start,
  Middle,
  End,
};

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

// color-interpolation and color-interpolation-filters
enum class StyleColorInterpolation : uint8_t {
  Auto = 0,
  Srgb = 1,
  Linearrgb = 2,
};

// vector-effect
enum class StyleVectorEffect : uint8_t { None = 0, NonScalingStroke = 1 };

// 3d Transforms - Backface visibility
enum class StyleBackfaceVisibility : uint8_t { Hidden = 0, Visible = 1 };

// blending
enum class StyleBlend : uint8_t {
  Normal = 0,
  Multiply,
  Screen,
  Overlay,
  Darken,
  Lighten,
  ColorDodge,
  ColorBurn,
  HardLight,
  SoftLight,
  Difference,
  Exclusion,
  Hue,
  Saturation,
  Color,
  Luminosity,
  PlusLighter,
};

// composite
enum class StyleMaskComposite : uint8_t {
  Add = 0,
  Subtract,
  Intersect,
  Exclude
};

// scroll-behavior
enum class StyleScrollBehavior : uint8_t {
  Auto,
  Smooth,
};

}  // namespace mozilla

#endif /* nsStyleConsts_h___ */
