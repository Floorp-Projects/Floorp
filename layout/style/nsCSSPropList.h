/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a list of all CSS properties with considerable data about them, for
 * preprocessing
 */

/******

  This file contains the list of all parsed CSS properties.  It is
  designed to be used as inline input through the magic of C
  preprocessing.  All entries must be enclosed in the appropriate
  CSS_PROP_* macro which will have cruel and unusual things done to it.
  It is recommended (but not strictly necessary) to keep all entries in
  alphabetical order.

  The arguments to CSS_PROP are:

  -. 'name' entries represent a CSS property name and *must* use only
  lowercase characters.

  -. 'id' should be the same as 'name' except that all hyphens ('-')
  in 'name' are converted to underscores ('_') in 'id'. For properties
  on a standards track, any '-moz-' prefix is removed in 'id'. This
  lets us do nice things with the macros without having to copy/convert
  strings at runtime.  These are the names used for the enum values of
  the nsCSSPropertyID enumeration defined in nsCSSProps.h.

  -. 'method' is designed to be as input for CSS2Properties and similar
  callers.  It must always be the same as 'name' except it must use
  InterCaps and all hyphens ('-') must be removed.  Callers using this
  parameter must also define the CSS_PROP_PUBLIC_OR_PRIVATE(publicname_,
  privatename_) macro to yield either publicname_ or privatename_.
  The names differ in that publicname_ has Moz prefixes where they are
  used, and also in CssFloat vs. Float.  The caller's choice depends on
  whether the use is for internal use such as eCSSProperty_* or
  nsRuleData::ValueFor* or external use such as exposing DOM properties.

  -. 'flags', a bitfield containing CSS_PROPERTY_* flags.

  -. 'pref' is the name of a pref that controls whether the property
  is enabled.  The property is enabled if 'pref' is an empty string,
  or if the boolean property whose name is 'pref' is set to true.

  -. 'parsevariant', to be passed to ParseVariant in the parser.

  -. 'kwtable', which is either nullptr or the name of the appropriate
  keyword table member of class nsCSSProps, for use in
  nsCSSProps::LookupPropertyValue.

  -. 'animtype_' gives the animation type (see nsStyleAnimType) of this
  property.

  CSS_PROP_SHORTHAND only takes 1-5.

 ******/


/*************************************************************************/


// All includers must explicitly define CSS_PROP_SHORTHAND if they
// want it.
#ifndef CSS_PROP_SHORTHAND
#define CSS_PROP_SHORTHAND(name_, id_, method_, flags_, pref_) /* nothing */
#define DEFINED_CSS_PROP_SHORTHAND
#endif

#define CSS_PROP_DOMPROP_PREFIXED(name_) \
  CSS_PROP_PUBLIC_OR_PRIVATE(Moz ## name_, name_)

// Callers may define CSS_PROP_LIST_EXCLUDE_INTERNAL if they want to
// exclude internal properties that are not represented in the DOM (only
// the DOM style code defines this).  All properties defined in an
// #ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL section must have the
// CSS_PROPERTY_INTERNAL flag set.

// Callers may also define CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
// to exclude properties that are not considered to be components of the 'all'
// shorthand property.  Currently this excludes 'direction' and 'unicode-bidi',
// as required by the CSS Cascading and Inheritance specification, and any
// internal properties that cannot be changed by using CSS syntax.  For example,
// the internal '-moz-system-font' property is not excluded, as it is set by the
// 'font' shorthand, while '-x-lang' is excluded as there is no way to set this
// internal property from a style sheet.

// A caller who wants all the properties can define the |CSS_PROP|
// macro.
#ifdef CSS_PROP

#define USED_CSS_PROP
// We still need this extra level so that CSS_PROP_DOMPROP_PREFIXED has
// a chance to be expanded.
#define CSS_PROP_(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_) CSS_PROP(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_)

#else /* !defined(CSS_PROP) */

// An includer who does not define CSS_PROP can define any or all of the
// per-struct macros that are equivalent to it, and the rest will be
// ignored.

#define CSS_PROP_(name_, id_, method_, flags_, pref_, parsevariant_, kwtable_) /* nothing */

#endif /* !defined(CSS_PROP) */

/*************************************************************************/

// For notes XXX bug 3935 below, the names being parsed do not correspond
// to the constants used internally.  It would be nice to bring the
// constants into line sometime.

// The parser will refuse to parse properties marked with -x-.

// Those marked XXX bug 48973 are CSS2 properties that we support
// differently from the spec for UI requirements.  If we ever
// support them correctly the old constants need to be renamed and
// new ones should be entered.

// CSS2.1 section 5.12.1 says that the properties that apply to
// :first-line are: font properties, color properties, background
// properties, 'word-spacing', 'letter-spacing', 'text-decoration',
// 'vertical-align', 'text-transform', and 'line-height'.
//
// We also allow 'text-shadow', which was listed in CSS2 (where the
// property existed).

// CSS2.1 section 5.12.2 says that the properties that apply to
// :first-letter are: font properties, 'text-decoration',
// 'text-transform', 'letter-spacing', 'word-spacing' (when
// appropriate), 'line-height', 'float', 'vertical-align' (only if
// 'float' is 'none'), margin properties, padding properties, border
// properties, 'color', and background properties.  We also allow
// 'text-shadow' (see above) and 'box-shadow' (which is like the
// border properties).

// Please keep these sorted by property name, ignoring any "-moz-",
// "-webkit-" or "-x-" prefix.

CSS_PROP_(
    align-content,
    align_content,
    AlignContent,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifyContent)
CSS_PROP_(
    align-items,
    align_items,
    AlignItems,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HK,
    kAutoCompletionAlignItems)
CSS_PROP_(
    align-self,
    align_self,
    AlignSelf,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifySelf)
CSS_PROP_SHORTHAND(
    all,
    all,
    All,
    CSS_PROPERTY_PARSE_FUNCTION,
    "layout.css.all-shorthand.enabled")
CSS_PROP_SHORTHAND(
    animation,
    animation,
    Animation,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    animation-delay,
    animation_delay,
    AnimationDelay,
    0,
    "",
    VARIANT_TIME, // used by list parsing
    nullptr)
CSS_PROP_(
    animation-direction,
    animation_direction,
    AnimationDirection,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kAnimationDirectionKTable)
CSS_PROP_(
    animation-duration,
    animation_duration,
    AnimationDuration,
    0,
    "",
    VARIANT_TIME | VARIANT_NONNEGATIVE_DIMENSION, // used by list parsing
    nullptr)
CSS_PROP_(
    animation-fill-mode,
    animation_fill_mode,
    AnimationFillMode,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kAnimationFillModeKTable)
CSS_PROP_(
    animation-iteration-count,
    animation_iteration_count,
    AnimationIterationCount,
    0,
    "",
    VARIANT_KEYWORD | VARIANT_NUMBER, // used by list parsing
    kAnimationIterationCountKTable)
CSS_PROP_(
    animation-name,
    animation_name,
    AnimationName,
    0,
    "",
    // FIXME: The spec should say something about 'inherit' and 'initial'
    // not being allowed.
    VARIANT_NONE | VARIANT_IDENTIFIER_NO_INHERIT | VARIANT_STRING, // used by list parsing
    nullptr)
CSS_PROP_(
    animation-play-state,
    animation_play_state,
    AnimationPlayState,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kAnimationPlayStateKTable)
CSS_PROP_(
    animation-timing-function,
    animation_timing_function,
    AnimationTimingFunction,
    0,
    "",
    VARIANT_KEYWORD | VARIANT_TIMING_FUNCTION, // used by list parsing
    kTransitionTimingFunctionKTable)
CSS_PROP_(
    -moz-appearance,
    _moz_appearance,
    CSS_PROP_DOMPROP_PREFIXED(Appearance),
    0,
    "",
    VARIANT_HK,
    kAppearanceKTable)
CSS_PROP_(
    backface-visibility,
    backface_visibility,
    BackfaceVisibility,
    0,
    "",
    VARIANT_HK,
    kBackfaceVisibilityKTable)
CSS_PROP_SHORTHAND(
    background,
    background,
    Background,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    background-attachment,
    background_attachment,
    BackgroundAttachment,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerAttachmentKTable)
CSS_PROP_(
    background-blend-mode,
    background_blend_mode,
    BackgroundBlendMode,
    0,
    "layout.css.background-blend-mode.enabled",
    VARIANT_KEYWORD, // used by list parsing
    kBlendModeKTable)
CSS_PROP_(
    background-clip,
    background_clip,
    BackgroundClip,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kBackgroundClipKTable)
CSS_PROP_(
    background-color,
    background_color,
    BackgroundColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    background-image,
    background_image,
    BackgroundImage,
    0,
    "",
    VARIANT_IMAGE, // used by list parsing
    nullptr)
CSS_PROP_(
    background-origin,
    background_origin,
    BackgroundOrigin,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kBackgroundOriginKTable)
CSS_PROP_SHORTHAND(
    background-position,
    background_position,
    BackgroundPosition,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    background-position-x,
    background_position_x,
    BackgroundPositionX,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    background-position-y,
    background_position_y,
    BackgroundPositionY,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    background-repeat,
    background_repeat,
    BackgroundRepeat,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerRepeatKTable)
CSS_PROP_(
    background-size,
    background_size,
    BackgroundSize,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kImageLayerSizeKTable)
CSS_PROP_(
    -moz-binding,
    _moz_binding,
    CSS_PROP_DOMPROP_PREFIXED(Binding),
    0,
    "",
    VARIANT_HUO,
    nullptr) // XXX bug 3935
CSS_PROP_(
    block-size,
    block_size,
    BlockSize,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    border,
    border,
    Border,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    border-block-end,
    border_block_end,
    BorderBlockEnd,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-block-end-color,
    border_block_end_color,
    BorderBlockEndColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-block-end-style,
    border_block_end_style,
    BorderBlockEndStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-block-end-width,
    border_block_end_width,
    BorderBlockEndWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-block-start,
    border_block_start,
    BorderBlockStart,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-block-start-color,
    border_block_start_color,
    BorderBlockStartColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-block-start-style,
    border_block_start_style,
    BorderBlockStartStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-block-start-width,
    border_block_start_width,
    BorderBlockStartWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-bottom,
    border_bottom,
    BorderBottom,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-bottom-color,
    border_bottom_color,
    BorderBottomColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-bottom-left-radius,
    border_bottom_left_radius,
    BorderBottomLeftRadius,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-bottom-right-radius,
    border_bottom_right_radius,
    BorderBottomRightRadius,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-bottom-style,
    border_bottom_style,
    BorderBottomStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)  // on/off will need reflow
CSS_PROP_(
    border-bottom-width,
    border_bottom_width,
    BorderBottomWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    border-collapse,
    border_collapse,
    BorderCollapse,
    0,
    "",
    VARIANT_HK,
    kBorderCollapseKTable)
CSS_PROP_SHORTHAND(
    border-color,
    border_color,
    BorderColor,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    border-image,
    border_image,
    BorderImage,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-image-outset,
    border_image_outset,
    BorderImageOutset,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-image-repeat,
    border_image_repeat,
    BorderImageRepeat,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kBorderImageRepeatKTable)
CSS_PROP_(
    border-image-slice,
    border_image_slice,
    BorderImageSlice,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kBorderImageSliceKTable)
CSS_PROP_(
    border-image-source,
    border_image_source,
    BorderImageSource,
    0,
    "",
    VARIANT_IMAGE | VARIANT_INHERIT,
    nullptr)
CSS_PROP_(
    border-image-width,
    border_image_width,
    BorderImageWidth,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    border-inline-end,
    border_inline_end,
    BorderInlineEnd,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-inline-end-color,
    border_inline_end_color,
    BorderInlineEndColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-inline-end-style,
    border_inline_end_style,
    BorderInlineEndStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-inline-end-width,
    border_inline_end_width,
    BorderInlineEndWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-inline-start,
    border_inline_start,
    BorderInlineStart,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-inline-start-color,
    border_inline_start_color,
    BorderInlineStartColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-inline-start-style,
    border_inline_start_style,
    BorderInlineStartStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-inline-start-width,
    border_inline_start_width,
    BorderInlineStartWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-left,
    border_left,
    BorderLeft,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-left-color,
    border_left_color,
    BorderLeftColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-left-style,
    border_left_style,
    BorderLeftStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-left-width,
    border_left_width,
    BorderLeftWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-radius,
    border_radius,
    BorderRadius,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    border-right,
    border_right,
    BorderRight,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-right-color,
    border_right_color,
    BorderRightColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-right-style,
    border_right_style,
    BorderRightStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-right-width,
    border_right_width,
    BorderRightWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    border-spacing,
    border_spacing,
    BorderSpacing,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    border-style,
    border_style,
    BorderStyle,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")  // on/off will need reflow
CSS_PROP_SHORTHAND(
    border-top,
    border_top,
    BorderTop,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    border-top-color,
    border_top_color,
    BorderTopColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-top-left-radius,
    border_top_left_radius,
    BorderTopLeftRadius,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-top-right-radius,
    border_top_right_radius,
    BorderTopRightRadius,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-top-style,
    border_top_style,
    BorderTopStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)  // on/off will need reflow
CSS_PROP_(
    border-top-width,
    border_top_width,
    BorderTopWidth,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-width,
    border_width,
    BorderWidth,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    bottom,
    bottom,
    Bottom,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    -moz-box-align,
    _moz_box_align,
    CSS_PROP_DOMPROP_PREFIXED(BoxAlign),
    0,
    "",
    VARIANT_HK,
    kBoxAlignKTable) // XXX bug 3935
CSS_PROP_(
    box-decoration-break,
    box_decoration_break,
    BoxDecorationBreak,
    0,
    "layout.css.box-decoration-break.enabled",
    VARIANT_HK,
    kBoxDecorationBreakKTable)
CSS_PROP_(
    -moz-box-direction,
    _moz_box_direction,
    CSS_PROP_DOMPROP_PREFIXED(BoxDirection),
    0,
    "",
    VARIANT_HK,
    kBoxDirectionKTable) // XXX bug 3935
CSS_PROP_(
    -moz-box-flex,
    _moz_box_flex,
    CSS_PROP_DOMPROP_PREFIXED(BoxFlex),
    0,
    "",
    VARIANT_HN,
    nullptr) // XXX bug 3935
CSS_PROP_(
    -moz-box-ordinal-group,
    _moz_box_ordinal_group,
    CSS_PROP_DOMPROP_PREFIXED(BoxOrdinalGroup),
    0,
    "",
    VARIANT_HI,
    nullptr)
CSS_PROP_(
    -moz-box-orient,
    _moz_box_orient,
    CSS_PROP_DOMPROP_PREFIXED(BoxOrient),
    0,
    "",
    VARIANT_HK,
    kBoxOrientKTable) // XXX bug 3935
CSS_PROP_(
    -moz-box-pack,
    _moz_box_pack,
    CSS_PROP_DOMPROP_PREFIXED(BoxPack),
    0,
    "",
    VARIANT_HK,
    kBoxPackKTable) // XXX bug 3935
CSS_PROP_(
    box-shadow,
    box_shadow,
    BoxShadow,
    CSS_PROPERTY_PARSE_FUNCTION,
        // NOTE: some components must be nonnegative
    "",
    VARIANT_COLOR | VARIANT_LENGTH | VARIANT_CALC | VARIANT_INHERIT | VARIANT_NONE,
    kBoxShadowTypeKTable)
CSS_PROP_(
    box-sizing,
    box_sizing,
    BoxSizing,
    0,
    "",
    VARIANT_HK,
    kBoxSizingKTable)
CSS_PROP_(
    caption-side,
    caption_side,
    CaptionSide,
    0,
    "",
    VARIANT_HK,
    kCaptionSideKTable)
CSS_PROP_(
    caret-color,
    caret_color,
    CaretColor,
    0,
    "",
    VARIANT_AUTO | VARIANT_HC,
    nullptr)
CSS_PROP_(
    clear,
    clear,
    Clear,
    0,
    "",
    VARIANT_HK,
    kClearKTable)
CSS_PROP_(
    clip,
    clip,
    Clip,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_AH,
    nullptr)
CSS_PROP_(
    clip-path,
    clip_path,
    ClipPath,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    clip-rule,
    clip_rule,
    ClipRule,
    0,
    "",
    VARIANT_HK,
    kFillRuleKTable)
CSS_PROP_(
    color,
    color,
    Color,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    color-adjust,
    color_adjust,
    ColorAdjust,
    0,
    "layout.css.color-adjust.enabled",
    VARIANT_HK,
    kColorAdjustKTable)
CSS_PROP_(
    color-interpolation,
    color_interpolation,
    ColorInterpolation,
    0,
    "",
    VARIANT_HK,
    kColorInterpolationKTable)
CSS_PROP_(
    color-interpolation-filters,
    color_interpolation_filters,
    ColorInterpolationFilters,
    0,
    "",
    VARIANT_HK,
    kColorInterpolationKTable)
CSS_PROP_(
    column-count,
    column_count,
    ColumnCount,
    0,
    "",
    VARIANT_AHI,
    nullptr)
CSS_PROP_(
    column-fill,
    column_fill,
    ColumnFill,
    0,
    "",
    VARIANT_HK,
    kColumnFillKTable)
CSS_PROP_(
    column-gap,
    column_gap,
    ColumnGap,
    0,
    "",
    VARIANT_HLP | VARIANT_NORMAL | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    column-rule,
    column_rule,
    ColumnRule,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    column-rule-color,
    column_rule_color,
    ColumnRuleColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    column-rule-style,
    column_rule_style,
    ColumnRuleStyle,
    0,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    column-rule-width,
    column_rule_width,
    ColumnRuleWidth,
    0,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    column-span,
    column_span,
    ColumnSpan,
    0,
    "layout.css.column-span.enabled",
    VARIANT_HK,
    kColumnSpanKTable)
CSS_PROP_(
    column-width,
    column_width,
    ColumnWidth,
    0,
    "",
    VARIANT_AHL | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    columns,
    columns,
    Columns,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    contain,
    contain,
    Contain,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.contain.enabled",
    // Does not affect parsing, but is needed for tab completion in devtools:
    VARIANT_HK | VARIANT_NONE,
    kContainKTable)
CSS_PROP_(
    content,
    content,
    Content,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HMK | VARIANT_NONE | VARIANT_URL | VARIANT_COUNTER | VARIANT_ATTR,
    kContentKTable)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    // Only intended to be used internally by Mozilla, so prefixed.
    -moz-context-properties,
    _moz_context_properties,
    CSS_PROP_DOMPROP_PREFIXED(ContextProperties),
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_INTERNAL,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-control-character-visibility,
    _moz_control_character_visibility,
    CSS_PROP_DOMPROP_PREFIXED(ControlCharacterVisibility),
    CSS_PROPERTY_INTERNAL,
    "",
    VARIANT_HK,
    kControlCharacterVisibilityKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    counter-increment,
    counter_increment,
    CounterIncrement,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_INHERIT | VARIANT_NONE,
    nullptr) // XXX bug 137285
CSS_PROP_(
    counter-reset,
    counter_reset,
    CounterReset,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_INHERIT | VARIANT_NONE,
    nullptr) // XXX bug 137285
CSS_PROP_(
    cursor,
    cursor,
    Cursor,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kCursorKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    direction,
    direction,
    Direction,
    0,
    "",
    VARIANT_HK,
    kDirectionKTable)
#endif // !defined(CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND)
CSS_PROP_(
    display,
    display,
    Display,
    0,
    "",
    VARIANT_HK,
    kDisplayKTable)
CSS_PROP_(
    dominant-baseline,
    dominant_baseline,
    DominantBaseline,
    0,
    "",
    VARIANT_HK,
    kDominantBaselineKTable)
CSS_PROP_(
    empty-cells,
    empty_cells,
    EmptyCells,
    0,
    "",
    VARIANT_HK,
    kEmptyCellsKTable)
CSS_PROP_(
    fill,
    fill,
    Fill,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kContextPatternKTable)
CSS_PROP_(
    fill-opacity,
    fill_opacity,
    FillOpacity,
    0,
    "",
    VARIANT_HN | VARIANT_KEYWORD,
    kContextOpacityKTable)
CSS_PROP_(
    fill-rule,
    fill_rule,
    FillRule,
    0,
    "",
    VARIANT_HK,
    kFillRuleKTable)
CSS_PROP_(
    filter,
    filter,
    Filter,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    flex,
    flex,
    Flex,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    flex-basis,
    flex_basis,
    FlexBasis,
    0,
    "",
    // NOTE: The parsing implementation for the 'flex' shorthand property has
    // its own code to parse each subproperty. It does not depend on the
    // longhand parsing defined here.
    VARIANT_AHKLP | VARIANT_CALC,
    kFlexBasisKTable)
CSS_PROP_(
    flex-direction,
    flex_direction,
    FlexDirection,
    0,
    "",
    VARIANT_HK,
    kFlexDirectionKTable)
CSS_PROP_SHORTHAND(
    flex-flow,
    flex_flow,
    FlexFlow,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    flex-grow,
    flex_grow,
    FlexGrow,
    0,
    "",
    // NOTE: The parsing implementation for the 'flex' shorthand property has
    // its own code to parse each subproperty. It does not depend on the
    // longhand parsing defined here.
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    flex-shrink,
    flex_shrink,
    FlexShrink,
    0,
    "",
    // NOTE: The parsing implementation for the 'flex' shorthand property has
    // its own code to parse each subproperty. It does not depend on the
    // longhand parsing defined here.
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    flex-wrap,
    flex_wrap,
    FlexWrap,
    0,
    "",
    VARIANT_HK,
    kFlexWrapKTable)
CSS_PROP_(
    float,
    float,
    CSS_PROP_PUBLIC_OR_PRIVATE(CssFloat, Float),
    0,
    "",
    VARIANT_HK,
    kFloatKTable)
CSS_PROP_(
    -moz-float-edge,
    _moz_float_edge,
    CSS_PROP_DOMPROP_PREFIXED(FloatEdge),
    0,
    "",
    VARIANT_HK,
    kFloatEdgeKTable) // XXX bug 3935
CSS_PROP_(
    flood-color,
    flood_color,
    FloodColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    flood-opacity,
    flood_opacity,
    FloodOpacity,
    0,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_SHORTHAND(
    font,
    font,
    Font,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    font-family,
    font_family,
    FontFamily,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    font-feature-settings,
    font_feature_settings,
    FontFeatureSettings,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    font-kerning,
    font_kerning,
    FontKerning,
    0,
    "",
    VARIANT_HK,
    kFontKerningKTable)
CSS_PROP_(
    font-language-override,
    font_language_override,
    FontLanguageOverride,
    0,
    "",
    VARIANT_NORMAL | VARIANT_INHERIT | VARIANT_STRING,
    nullptr)
CSS_PROP_(
    font-optical-sizing,
    font_optical_sizing,
    FontOpticalSizing,
    0,
    "layout.css.font-variations.enabled",
    VARIANT_HK,
    kFontOpticalSizingKTable)
CSS_PROP_(
    font-size,
    font_size,
    FontSize,
    0,
    "",
    VARIANT_HKLP | VARIANT_SYSFONT | VARIANT_CALC,
    kFontSizeKTable)
CSS_PROP_(
    font-size-adjust,
    font_size_adjust,
    FontSizeAdjust,
    0,
    "",
    VARIANT_HON | VARIANT_SYSFONT,
    nullptr)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-font-smoothing-background-color,
    _moz_font_smoothing_background_color,
    CSS_PROP_DOMPROP_PREFIXED(FontSmoothingBackgroundColor),
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS_AND_CHROME,
    "",
    VARIANT_HC,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    font-stretch,
    font_stretch,
    FontStretch,
    0,
    "",
    VARIANT_HK | VARIANT_SYSFONT,
    kFontStretchKTable)
CSS_PROP_(
    font-style,
    font_style,
    FontStyle,
    0,
    "",
    VARIANT_HK | VARIANT_SYSFONT,
    kFontStyleKTable)
CSS_PROP_(
    font-synthesis,
    font_synthesis,
    FontSynthesis,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kFontSynthesisKTable)
CSS_PROP_SHORTHAND(
    font-variant,
    font_variant,
    FontVariant,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    font-variant-alternates,
    font_variant_alternates,
    FontVariantAlternates,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kFontVariantAlternatesKTable)
CSS_PROP_(
    font-variant-caps,
    font_variant_caps,
    FontVariantCaps,
    0,
    "",
    VARIANT_HMK,
    kFontVariantCapsKTable)
CSS_PROP_(
    font-variant-east-asian,
    font_variant_east_asian,
    FontVariantEastAsian,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kFontVariantEastAsianKTable)
CSS_PROP_(
    font-variant-ligatures,
    font_variant_ligatures,
    FontVariantLigatures,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kFontVariantLigaturesKTable)
CSS_PROP_(
    font-variant-numeric,
    font_variant_numeric,
    FontVariantNumeric,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kFontVariantNumericKTable)
CSS_PROP_(
    font-variant-position,
    font_variant_position,
    FontVariantPosition,
    0,
    "",
    VARIANT_HMK,
    kFontVariantPositionKTable)
CSS_PROP_(
    font-variation-settings,
    font_variation_settings,
    FontVariationSettings,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.font-variations.enabled",
    0,
    nullptr)
CSS_PROP_(
    font-weight,
    font_weight,
    FontWeight,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
        // NOTE: This property has range restrictions on interpolation!
    "",
    0,
    kFontWeightKTable)
CSS_PROP_(
    -moz-force-broken-image-icon,
    _moz_force_broken_image_icon,
    CSS_PROP_DOMPROP_PREFIXED(ForceBrokenImageIcon),
    0,
    "",
    VARIANT_HI,
    nullptr) // bug 58646
CSS_PROP_SHORTHAND(
    grid,
    grid,
    Grid,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    grid-area,
    grid_area,
    GridArea,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    grid-auto-columns,
    grid_auto_columns,
    GridAutoColumns,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_(
    grid-auto-flow,
    grid_auto_flow,
    GridAutoFlow,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kGridAutoFlowKTable)
CSS_PROP_(
    grid-auto-rows,
    grid_auto_rows,
    GridAutoRows,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_SHORTHAND(
    grid-column,
    grid_column,
    GridColumn,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    grid-column-end,
    grid_column_end,
    GridColumnEnd,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    grid-column-gap,
    grid_column_gap,
    GridColumnGap,
    0,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    grid-column-start,
    grid_column_start,
    GridColumnStart,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    grid-gap,
    grid_gap,
    GridGap,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    grid-row,
    grid_row,
    GridRow,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    grid-row-end,
    grid_row_end,
    GridRowEnd,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    grid-row-gap,
    grid_row_gap,
    GridRowGap,
    0,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    grid-row-start,
    grid_row_start,
    GridRowStart,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    grid-template,
    grid_template,
    GridTemplate,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    grid-template-areas,
    grid_template_areas,
    GridTemplateAreas,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    grid-template-columns,
    grid_template_columns,
    GridTemplateColumns,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_(
    grid-template-rows,
    grid_template_rows,
    GridTemplateRows,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_(
    height,
    height,
    Height,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    hyphens,
    hyphens,
    Hyphens,
    0,
    "",
    VARIANT_HK,
    kHyphensKTable)
CSS_PROP_(
    image-orientation,
    image_orientation,
    ImageOrientation,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.image-orientation.enabled",
    0,
    kImageOrientationKTable)
CSS_PROP_(
    -moz-image-region,
    _moz_image_region,
    CSS_PROP_DOMPROP_PREFIXED(ImageRegion),
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    image-rendering,
    image_rendering,
    ImageRendering,
    0,
    "",
    VARIANT_HK,
    kImageRenderingKTable)
CSS_PROP_(
    ime-mode,
    ime_mode,
    ImeMode,
    0,
    "",
    VARIANT_HK,
    kIMEModeKTable)
CSS_PROP_(
    initial-letter,
    initial_letter,
    InitialLetter,
    CSS_PROPERTY_PARSE_FUNCTION,
    "layout.css.initial-letter.enabled",
    0,
    nullptr)
CSS_PROP_(
    inline-size,
    inline_size,
    InlineSize,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    isolation,
    isolation,
    Isolation,
    0,
    "layout.css.isolation.enabled",
    VARIANT_HK,
    kIsolationKTable)
CSS_PROP_(
    justify-content,
    justify_content,
    JustifyContent,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifyContent)
CSS_PROP_(
    justify-items,
    justify_items,
    JustifyItems,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HK,
    // for auto-completion we use same values as justify-self:
    kAutoCompletionAlignJustifySelf)
CSS_PROP_(
    justify-self,
    justify_self,
    JustifySelf,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifySelf)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -x-lang,
    _x_lang,
    Lang,
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_INACCESSIBLE,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    left,
    left,
    Left,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    letter-spacing,
    letter_spacing,
    LetterSpacing,
    0,
    "",
    VARIANT_HL | VARIANT_NORMAL | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    lighting-color,
    lighting_color,
    LightingColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    line-height,
    line_height,
    LineHeight,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLPN | VARIANT_KEYWORD | VARIANT_NORMAL | VARIANT_SYSFONT | VARIANT_CALC,
    kLineHeightKTable)
CSS_PROP_SHORTHAND(
    list-style,
    list_style,
    ListStyle,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    list-style-image,
    list_style_image,
    ListStyleImage,
    0,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    list-style-position,
    list_style_position,
    ListStylePosition,
    0,
    "",
    VARIANT_HK,
    kListStylePositionKTable)
CSS_PROP_(
    list-style-type,
    list_style_type,
    ListStyleType,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    margin,
    margin,
    Margin,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    margin-block-end,
    margin_block_end,
    MarginBlockEnd,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-block-start,
    margin_block_start,
    MarginBlockStart,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-bottom,
    margin_bottom,
    MarginBottom,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-inline-end,
    margin_inline_end,
    MarginInlineEnd,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-inline-start,
    margin_inline_start,
    MarginInlineStart,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-left,
    margin_left,
    MarginLeft,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-right,
    margin_right,
    MarginRight,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-top,
    margin_top,
    MarginTop,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    marker,
    marker,
    Marker,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    marker-end,
    marker_end,
    MarkerEnd,
    0,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    marker-mid,
    marker_mid,
    MarkerMid,
    0,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    marker-start,
    marker_start,
    MarkerStart,
    0,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_SHORTHAND(
    mask,
    mask,
    Mask,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    mask-clip,
    mask_clip,
    MaskClip,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kMaskClipKTable)
CSS_PROP_(
    mask-composite,
    mask_composite,
    MaskComposite,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerCompositeKTable)
CSS_PROP_(
    mask-image,
    mask_image,
    MaskImage,
    0,
    "",
    VARIANT_IMAGE, // used by list parsing
    nullptr)
CSS_PROP_(
    mask-mode,
    mask_mode,
    MaskMode,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerModeKTable)
CSS_PROP_(
    mask-origin,
    mask_origin,
    MaskOrigin,
    0,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kMaskOriginKTable)
CSS_PROP_SHORTHAND(
    mask-position,
    mask_position,
    MaskPosition,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    mask-position-x,
    mask_position_x,
    MaskPositionX,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    mask-position-y,
    mask_position_y,
    MaskPositionY,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    mask-repeat,
    mask_repeat,
    MaskRepeat,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerRepeatKTable)
CSS_PROP_(
    mask-size,
    mask_size,
    MaskSize,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kImageLayerSizeKTable)
CSS_PROP_(
    mask-type,
    mask_type,
    MaskType,
    0,
    "",
    VARIANT_HK,
    kMaskTypeKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-math-display,
    _moz_math_display,
    MathDisplay,
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
    "",
    VARIANT_HK,
    kMathDisplayKTable)
CSS_PROP_(
    -moz-math-variant,
    _moz_math_variant,
    MathVariant,
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_INACCESSIBLE,
    "",
    VARIANT_HK,
    kMathVariantKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    max-block-size,
    max_block_size,
    MaxBlockSize,
    0,
    "",
    VARIANT_HLPO | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    max-height,
    max_height,
    MaxHeight,
    0,
    "",
    VARIANT_HKLPO | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    max-inline-size,
    max_inline_size,
    MaxInlineSize,
    0,
    "",
    VARIANT_HKLPO | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    max-width,
    max_width,
    MaxWidth,
    0,
    "",
    VARIANT_HKLPO | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    min-block-size,
    min_block_size,
    MinBlockSize,
    0,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-min-font-size-ratio,
    _moz_min_font_size_ratio,
    CSS_PROP_DOMPROP_PREFIXED(MinFontSizeRatio),
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
    "",
    VARIANT_INHERIT | VARIANT_PERCENT,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    min-height,
    min_height,
    MinHeight,
    0,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    min-inline-size,
    min_inline_size,
    MinInlineSize,
    0,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    min-width,
    min_width,
    MinWidth,
    0,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    mix-blend-mode,
    mix_blend_mode,
    MixBlendMode,
    0,
    "layout.css.mix-blend-mode.enabled",
    VARIANT_HK,
    kBlendModeKTable)
CSS_PROP_(
    object-fit,
    object_fit,
    ObjectFit,
    0,
    "",
    VARIANT_HK,
    kObjectFitKTable)
CSS_PROP_(
    object-position,
    object_position,
    ObjectPosition,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_CALC,
    kImageLayerPositionKTable)
CSS_PROP_(
    offset-block-end,
    offset_block_end,
    OffsetBlockEnd,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    offset-block-start,
    offset_block_start,
    OffsetBlockStart,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    offset-inline-end,
    offset_inline_end,
    OffsetInlineEnd,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    offset-inline-start,
    offset_inline_start,
    OffsetInlineStart,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    opacity,
    opacity,
    Opacity,
    CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    order,
    order,
    Order,
    0,
    "",
    VARIANT_HI,
    nullptr) // <integer>
CSS_PROP_(
    -moz-orient,
    _moz_orient,
    CSS_PROP_DOMPROP_PREFIXED(Orient),
    0,
    "",
    VARIANT_HK,
    kOrientKTable)
CSS_PROP_(
    -moz-osx-font-smoothing,
    _moz_osx_font_smoothing,
    CSS_PROP_DOMPROP_PREFIXED(OsxFontSmoothing),
    0,
    "layout.css.osx-font-smoothing.enabled",
    VARIANT_HK,
    kFontSmoothingKTable)
CSS_PROP_SHORTHAND(
    outline,
    outline,
    Outline,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    outline-color,
    outline_color,
    OutlineColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    outline-offset,
    outline_offset,
    OutlineOffset,
    0,
    "",
    VARIANT_HL | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    -moz-outline-radius,
    _moz_outline_radius,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadius),
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    -moz-outline-radius-bottomleft,
    _moz_outline_radius_bottomleft,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusBottomleft),
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-outline-radius-bottomright,
    _moz_outline_radius_bottomright,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusBottomright),
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-outline-radius-topleft,
    _moz_outline_radius_topleft,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusTopleft),
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-outline-radius-topright,
    _moz_outline_radius_topright,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusTopright),
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    outline-style,
    outline_style,
    OutlineStyle,
    0,
    "",
    VARIANT_HK,
    kOutlineStyleKTable)
CSS_PROP_(
    outline-width,
    outline_width,
    OutlineWidth,
    0,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    overflow,
    overflow,
    Overflow,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    overflow-clip-box,
    overflow_clip_box,
    OverflowClipBox,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
    "layout.css.overflow-clip-box.enabled")
CSS_PROP_(
    overflow-clip-box-block,
    overflow_clip_box_block,
    OverflowClipBoxBlock,
    CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
    "layout.css.overflow-clip-box.enabled",
    VARIANT_HK,
    kOverflowClipBoxKTable)
CSS_PROP_(
    overflow-clip-box-inline,
    overflow_clip_box_inline,
    OverflowClipBoxInline,
    CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
    "layout.css.overflow-clip-box.enabled",
    VARIANT_HK,
    kOverflowClipBoxKTable)
CSS_PROP_(
    overflow-wrap,
    overflow_wrap,
    OverflowWrap,
    0,
    "",
    VARIANT_HK,
    kOverflowWrapKTable)
CSS_PROP_(
    overflow-x,
    overflow_x,
    OverflowX,
    0,
    "",
    VARIANT_HK,
    kOverflowSubKTable)
CSS_PROP_(
    overflow-y,
    overflow_y,
    OverflowY,
    0,
    "",
    VARIANT_HK,
    kOverflowSubKTable)
CSS_PROP_SHORTHAND(
    overscroll-behavior,
    overscroll_behavior,
    OverscrollBehavior,
    CSS_PROPERTY_PARSE_FUNCTION,
    "layout.css.overscroll-behavior.enabled")
CSS_PROP_(
    overscroll-behavior-x,
    overscroll_behavior_x,
    OverscrollBehaviorX,
    0,
    "layout.css.overscroll-behavior.enabled",
    VARIANT_HK,
    kOverscrollBehaviorKTable)
CSS_PROP_(
    overscroll-behavior-y,
    overscroll_behavior_y,
    OverscrollBehaviorY,
    0,
    "layout.css.overscroll-behavior.enabled",
    VARIANT_HK,
    kOverscrollBehaviorKTable)
CSS_PROP_SHORTHAND(
    padding,
    padding,
    Padding,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    padding-block-end,
    padding_block_end,
    PaddingBlockEnd,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-block-start,
    padding_block_start,
    PaddingBlockStart,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-bottom,
    padding_bottom,
    PaddingBottom,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-inline-end,
    padding_inline_end,
    PaddingInlineEnd,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-inline-start,
    padding_inline_start,
    PaddingInlineStart,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-left,
    padding_left,
    PaddingLeft,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-right,
    padding_right,
    PaddingRight,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-top,
    padding_top,
    PaddingTop,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    page-break-after,
    page_break_after,
    PageBreakAfter,
    0,
    "",
    VARIANT_HK,
    kPageBreakKTable) // temp fix for bug 24000
CSS_PROP_(
    page-break-before,
    page_break_before,
    PageBreakBefore,
    0,
    "",
    VARIANT_HK,
    kPageBreakKTable) // temp fix for bug 24000
CSS_PROP_(
    page-break-inside,
    page_break_inside,
    PageBreakInside,
    0,
    "",
    VARIANT_HK,
    kPageBreakInsideKTable)
CSS_PROP_(
    paint-order,
    paint_order,
    PaintOrder,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    perspective,
    perspective,
    Perspective,
    0,
    "",
    VARIANT_NONE | VARIANT_INHERIT | VARIANT_LENGTH |
      VARIANT_NONNEGATIVE_DIMENSION,
    nullptr)
CSS_PROP_(
    perspective-origin,
    perspective_origin,
    PerspectiveOrigin,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_CALC,
    kImageLayerPositionKTable)
CSS_PROP_SHORTHAND(
    place-content,
    place_content,
    PlaceContent,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    place-items,
    place_items,
    PlaceItems,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_SHORTHAND(
    place-self,
    place_self,
    PlaceSelf,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    pointer-events,
    pointer_events,
    PointerEvents,
    0,
    "",
    VARIANT_HK,
    kPointerEventsKTable)
CSS_PROP_(
    position,
    position,
    Position,
    0,
    "",
    VARIANT_HK,
    kPositionKTable)
CSS_PROP_(
    quotes,
    quotes,
    Quotes,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_HOS,
    nullptr)
CSS_PROP_(
    resize,
    resize,
    Resize,
    0,
    "",
    VARIANT_HK,
    kResizeKTable)
CSS_PROP_(
    right,
    right,
    Right,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    rotate,
    rotate,
    Rotate,
    CSS_PROPERTY_PARSE_FUNCTION,
    "layout.css.individual-transform.enabled",
    0,
    nullptr)
CSS_PROP_(
    ruby-align,
    ruby_align,
    RubyAlign,
    0,
    "",
    VARIANT_HK,
    kRubyAlignKTable)
CSS_PROP_(
    ruby-position,
    ruby_position,
    RubyPosition,
    0,
    "",
    VARIANT_HK,
    kRubyPositionKTable)
CSS_PROP_(
    scale,
    scale,
    Scale,
    CSS_PROPERTY_PARSE_FUNCTION,
    "layout.css.individual-transform.enabled",
    0,
    nullptr)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-script-level,
    _moz_script_level,
    ScriptLevel,
    // We only allow 'script-level' when unsafe rules are enabled, because
    // otherwise it could interfere with rulenode optimizations if used in
    // a non-MathML-enabled document.
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
    "",
    // script-level can take Auto, Integer and Number values, but only Auto
    // ("increment if parent is not in displaystyle") and Integer
    // ("relative") values can be specified in a style sheet.
    VARIANT_AHI,
    nullptr)
CSS_PROP_(
    -moz-script-min-size,
    _moz_script_min_size,
    ScriptMinSize,
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_INACCESSIBLE,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-script-size-multiplier,
    _moz_script_size_multiplier,
    ScriptSizeMultiplier,
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_INACCESSIBLE,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    scroll-behavior,
    scroll_behavior,
    ScrollBehavior,
    0,
    "layout.css.scroll-behavior.property-enabled",
    VARIANT_HK,
    kScrollBehaviorKTable)
CSS_PROP_(
    scroll-snap-coordinate,
    scroll_snap_coordinate,
    ScrollSnapCoordinate,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.scroll-snap.enabled",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    scroll-snap-destination,
    scroll_snap_destination,
    ScrollSnapDestination,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.scroll-snap.enabled",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    scroll-snap-points-x,
    scroll_snap_points_x,
    ScrollSnapPointsX,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.scroll-snap.enabled",
    0,
    nullptr)
CSS_PROP_(
    scroll-snap-points-y,
    scroll_snap_points_y,
    ScrollSnapPointsY,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.scroll-snap.enabled",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    scroll-snap-type,
    scroll_snap_type,
    ScrollSnapType,
    CSS_PROPERTY_PARSE_FUNCTION,
    "layout.css.scroll-snap.enabled")
CSS_PROP_(
    scroll-snap-type-x,
    scroll_snap_type_x,
    ScrollSnapTypeX,
    0,
    "layout.css.scroll-snap.enabled",
    VARIANT_HK,
    kScrollSnapTypeKTable)
CSS_PROP_(
    scroll-snap-type-y,
    scroll_snap_type_y,
    ScrollSnapTypeY,
    0,
    "layout.css.scroll-snap.enabled",
    VARIANT_HK,
    kScrollSnapTypeKTable)
CSS_PROP_(
    shape-image-threshold,
    shape_image_threshold,
    ShapeImageThreshold,
    0,
    "layout.css.shape-outside.enabled",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    shape-outside,
    shape_outside,
    ShapeOutside,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.shape-outside.enabled",
    0,
    nullptr)
CSS_PROP_(
    shape-rendering,
    shape_rendering,
    ShapeRendering,
    0,
    "",
    VARIANT_HK,
    kShapeRenderingKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -x-span,
    _x_span,
    Span,
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_INACCESSIBLE,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    -moz-stack-sizing,
    _moz_stack_sizing,
    CSS_PROP_DOMPROP_PREFIXED(StackSizing),
    0,
    "",
    VARIANT_HK,
    kStackSizingKTable)
CSS_PROP_(
    stop-color,
    stop_color,
    StopColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    stop-opacity,
    stop_opacity,
    StopOpacity,
    0,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    stroke,
    stroke,
    Stroke,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    kContextPatternKTable)
CSS_PROP_(
    stroke-dasharray,
    stroke_dasharray,
    StrokeDasharray,
    CSS_PROPERTY_PARSE_FUNCTION,
        // NOTE: Internal values have range restrictions.
    "",
    0,
    kStrokeContextValueKTable)
CSS_PROP_(
    stroke-dashoffset,
    stroke_dashoffset,
    StrokeDashoffset,
    0,
    "",
    VARIANT_HLPN | VARIANT_OPENTYPE_SVG_KEYWORD,
    kStrokeContextValueKTable)
CSS_PROP_(
    stroke-linecap,
    stroke_linecap,
    StrokeLinecap,
    0,
    "",
    VARIANT_HK,
    kStrokeLinecapKTable)
CSS_PROP_(
    stroke-linejoin,
    stroke_linejoin,
    StrokeLinejoin,
    0,
    "",
    VARIANT_HK,
    kStrokeLinejoinKTable)
CSS_PROP_(
    stroke-miterlimit,
    stroke_miterlimit,
    StrokeMiterlimit,
    0,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    stroke-opacity,
    stroke_opacity,
    StrokeOpacity,
    0,
    "",
    VARIANT_HN | VARIANT_KEYWORD,
    kContextOpacityKTable)
CSS_PROP_(
    stroke-width,
    stroke_width,
    StrokeWidth,
    0,
    "",
    VARIANT_HLPN | VARIANT_OPENTYPE_SVG_KEYWORD,
    kStrokeContextValueKTable)
CSS_PROP_(
    -moz-tab-size,
    _moz_tab_size,
    CSS_PROP_DOMPROP_PREFIXED(TabSize),
    0,
    "",
    VARIANT_INHERIT | VARIANT_LNCALC,
    nullptr)
CSS_PROP_(
    table-layout,
    table_layout,
    TableLayout,
    0,
    "",
    VARIANT_HK,
    kTableLayoutKTable)
CSS_PROP_(
    text-align,
    text_align,
    TextAlign,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    // When we support aligning on a string, we can parse text-align
    // as a string....
    VARIANT_HK /* | VARIANT_STRING */,
    kTextAlignKTable)
CSS_PROP_(
    text-align-last,
    text_align_last,
    TextAlignLast,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    VARIANT_HK,
    kTextAlignLastKTable)
CSS_PROP_(
    text-anchor,
    text_anchor,
    TextAnchor,
    0,
    "",
    VARIANT_HK,
    kTextAnchorKTable)
CSS_PROP_(
    text-combine-upright,
    text_combine_upright,
    TextCombineUpright,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.text-combine-upright.enabled",
    0,
    kTextCombineUprightKTable)
CSS_PROP_SHORTHAND(
    text-decoration,
    text_decoration,
    TextDecoration,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    text-decoration-color,
    text_decoration_color,
    TextDecorationColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    text-decoration-line,
    text_decoration_line,
    TextDecorationLine,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kTextDecorationLineKTable)
CSS_PROP_(
    text-decoration-style,
    text_decoration_style,
    TextDecorationStyle,
    0,
    "",
    VARIANT_HK,
    kTextDecorationStyleKTable)
CSS_PROP_SHORTHAND(
    text-emphasis,
    text_emphasis,
    TextEmphasis,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    text-emphasis-color,
    text_emphasis_color,
    TextEmphasisColor,
    0,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    text-emphasis-position,
    text_emphasis_position,
    TextEmphasisPosition,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kTextEmphasisPositionKTable)
CSS_PROP_(
    text-emphasis-style,
    text_emphasis_style,
    TextEmphasisStyle,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    -webkit-text-fill-color,
    _webkit_text_fill_color,
    WebkitTextFillColor,
    0,
    "layout.css.prefixes.webkit",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    text-indent,
    text_indent,
    TextIndent,
    0,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    text-justify,
    text_justify,
    TextJustify,
    0,
    "layout.css.text-justify.enabled",
    VARIANT_HK,
    kTextJustifyKTable)
CSS_PROP_(
    text-orientation,
    text_orientation,
    TextOrientation,
    0,
    "",
    VARIANT_HK,
    kTextOrientationKTable)
CSS_PROP_(
    text-overflow,
    text_overflow,
    TextOverflow,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "",
    0,
    kTextOverflowKTable)
CSS_PROP_(
    text-rendering,
    text_rendering,
    TextRendering,
    0,
    "",
    VARIANT_HK,
    kTextRenderingKTable)
CSS_PROP_(
    text-shadow,
    text_shadow,
    TextShadow,
    CSS_PROPERTY_PARSE_FUNCTION,
        // NOTE: some components must be nonnegative
    "",
    VARIANT_COLOR | VARIANT_LENGTH | VARIANT_CALC | VARIANT_INHERIT | VARIANT_NONE,
    nullptr)
CSS_PROP_(
    -moz-text-size-adjust,
    _moz_text_size_adjust,
    CSS_PROP_DOMPROP_PREFIXED(TextSizeAdjust),
    0,
    "",
    VARIANT_HK,
    kTextSizeAdjustKTable)
CSS_PROP_SHORTHAND(
    -webkit-text-stroke,
    _webkit_text_stroke,
    WebkitTextStroke,
    CSS_PROPERTY_PARSE_FUNCTION,
    "layout.css.prefixes.webkit")
CSS_PROP_(
    -webkit-text-stroke-color,
    _webkit_text_stroke_color,
    WebkitTextStrokeColor,
    0,
    "layout.css.prefixes.webkit",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    -webkit-text-stroke-width,
    _webkit_text_stroke_width,
    WebkitTextStrokeWidth,
    0,
    "layout.css.prefixes.webkit",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    text-transform,
    text_transform,
    TextTransform,
    0,
    "",
    VARIANT_HK,
    kTextTransformKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -x-text-zoom,
    _x_text_zoom,
    TextZoom,
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_INACCESSIBLE,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    top,
    top,
    Top,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-top-layer,
    _moz_top_layer,
    CSS_PROP_DOMPROP_PREFIXED(TopLayer),
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS,
    "",
    VARIANT_HK,
    kTopLayerKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    touch-action,
    touch_action,
    TouchAction,
    CSS_PROPERTY_VALUE_PARSER_FUNCTION,
    "layout.css.touch_action.enabled",
    VARIANT_HK,
    kTouchActionKTable)
CSS_PROP_(
    transform,
    transform,
    Transform,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH |
        CSS_PROPERTY_CAN_ANIMATE_ON_COMPOSITOR,
    "",
    0,
    nullptr)
CSS_PROP_(
    transform-box,
    transform_box,
    TransformBox,
    0,
    "svg.transform-box.enabled",
    VARIANT_HK,
    kTransformBoxKTable)
CSS_PROP_(
    transform-origin,
    transform_origin,
    TransformOrigin,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    transform-style,
    transform_style,
    TransformStyle,
    0,
    "",
    VARIANT_HK,
    kTransformStyleKTable)
CSS_PROP_SHORTHAND(
    transition,
    transition,
    Transition,
    CSS_PROPERTY_PARSE_FUNCTION,
    "")
CSS_PROP_(
    transition-delay,
    transition_delay,
    TransitionDelay,
    0,
    "",
    VARIANT_TIME, // used by list parsing
    nullptr)
CSS_PROP_(
    transition-duration,
    transition_duration,
    TransitionDuration,
    0,
    "",
    VARIANT_TIME | VARIANT_NONNEGATIVE_DIMENSION, // used by list parsing
    nullptr)
CSS_PROP_(
    transition-property,
    transition_property,
    TransitionProperty,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    VARIANT_IDENTIFIER | VARIANT_NONE | VARIANT_ALL, // used only in shorthand
    nullptr)
CSS_PROP_(
    transition-timing-function,
    transition_timing_function,
    TransitionTimingFunction,
    0,
    "",
    VARIANT_KEYWORD | VARIANT_TIMING_FUNCTION, // used by list parsing
    kTransitionTimingFunctionKTable)
CSS_PROP_(
    translate,
    translate,
    Translate,
    CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "layout.css.individual-transform.enabled",
    0,
    nullptr)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    unicode-bidi,
    unicode_bidi,
    UnicodeBidi,
    0,
    "",
    VARIANT_HK,
    kUnicodeBidiKTable)
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    -moz-user-focus,
    _moz_user_focus,
    CSS_PROP_DOMPROP_PREFIXED(UserFocus),
    0,
    "",
    VARIANT_HK,
    kUserFocusKTable) // XXX bug 3935
CSS_PROP_(
    -moz-user-input,
    _moz_user_input,
    CSS_PROP_DOMPROP_PREFIXED(UserInput),
    0,
    "",
    VARIANT_HK,
    kUserInputKTable) // XXX ??? // XXX bug 3935
CSS_PROP_(
    -moz-user-modify,
    _moz_user_modify,
    CSS_PROP_DOMPROP_PREFIXED(UserModify),
    0,
    "",
    VARIANT_HK,
    kUserModifyKTable) // XXX bug 3935
CSS_PROP_(
    -moz-user-select,
    _moz_user_select,
    CSS_PROP_DOMPROP_PREFIXED(UserSelect),
    0,
    "",
    VARIANT_HK,
    kUserSelectKTable) // XXX bug 3935
CSS_PROP_(
    vector-effect,
    vector_effect,
    VectorEffect,
    0,
    "",
    VARIANT_HK,
    kVectorEffectKTable)
// NOTE: vertical-align is only supposed to apply to :first-letter when
// 'float' is 'none', but we don't worry about that since it has no
// effect otherwise
CSS_PROP_(
    vertical-align,
    vertical_align,
    VerticalAlign,
    0,
    "",
    VARIANT_HKLP | VARIANT_CALC,
    kVerticalAlignKTable)
CSS_PROP_(
    visibility,
    visibility,
    Visibility,
    0,
    "",
    VARIANT_HK,
    kVisibilityKTable)  // reflow for collapse
CSS_PROP_(
    white-space,
    white_space,
    WhiteSpace,
    0,
    "",
    VARIANT_HK,
    kWhitespaceKTable)
CSS_PROP_(
    width,
    width,
    Width,
    CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    will-change,
    will_change,
    WillChange,
    CSS_PROPERTY_PARSE_FUNCTION,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-window-dragging,
    _moz_window_dragging,
    CSS_PROP_DOMPROP_PREFIXED(WindowDragging),
    0,
    "",
    VARIANT_HK,
    kWindowDraggingKTable)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-window-opacity,
    _moz_window_opacity,
    CSS_PROP_DOMPROP_PREFIXED(WindowOpacity),
    CSS_PROPERTY_INTERNAL,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    -moz-window-shadow,
    _moz_window_shadow,
    CSS_PROP_DOMPROP_PREFIXED(WindowShadow),
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_ENABLED_IN_UA_SHEETS_AND_CHROME,
    "",
    VARIANT_HK,
    kWindowShadowKTable)
CSS_PROP_(
    -moz-window-transform,
    _moz_window_transform,
    CSS_PROP_DOMPROP_PREFIXED(WindowTransform),
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-window-transform-origin,
    _moz_window_transform_origin,
    CSS_PROP_DOMPROP_PREFIXED(WindowTransformOrigin),
    CSS_PROPERTY_INTERNAL |
        CSS_PROPERTY_PARSE_FUNCTION |
        CSS_PROPERTY_GETCS_NEEDS_LAYOUT_FLUSH,
    "",
    0,
    kImageLayerPositionKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    word-break,
    word_break,
    WordBreak,
    0,
    "",
    VARIANT_HK,
    kWordBreakKTable)
CSS_PROP_(
    word-spacing,
    word_spacing,
    WordSpacing,
    0,
    "",
    VARIANT_HLP | VARIANT_NORMAL | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    writing-mode,
    writing_mode,
    WritingMode,
    0,
    "",
    VARIANT_HK,
    kWritingModeKTable)
CSS_PROP_(
    z-index,
    z_index,
    ZIndex,
    0,
    "",
    VARIANT_AHI,
    nullptr)

#undef CSS_PROP_

#ifdef DEFINED_CSS_PROP_SHORTHAND
#undef CSS_PROP_SHORTHAND
#undef DEFINED_CSS_PROP_SHORTHAND
#endif

#undef CSS_PROP_DOMPROP_PREFIXED
