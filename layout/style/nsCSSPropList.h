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

  -. 'pref' is the name of a pref that controls whether the property
  is enabled.  The property is enabled if 'pref' is an empty string,
  or if the boolean property whose name is 'pref' is set to true.

  -. 'parsevariant', to be passed to ParseVariant in the parser.

  -. 'kwtable', which is either nullptr or the name of the appropriate
  keyword table member of class nsCSSProps, for use in
  nsCSSProps::LookupPropertyValue.

  -. 'animtype_' gives the animation type (see nsStyleAnimType) of this
  property.

  CSS_PROP_SHORTHAND only takes 1-4.

 ******/


/*************************************************************************/


// All includers must explicitly define CSS_PROP_SHORTHAND if they
// want it.
#ifndef CSS_PROP_SHORTHAND
#define CSS_PROP_SHORTHAND(name_, id_, method_, pref_) /* nothing */
#define DEFINED_CSS_PROP_SHORTHAND
#endif

#define CSS_PROP_DOMPROP_PREFIXED(name_) \
  CSS_PROP_PUBLIC_OR_PRIVATE(Moz ## name_, name_)

// Callers may define CSS_PROP_LIST_EXCLUDE_INTERNAL if they want to
// exclude internal properties that are not represented in the DOM (only
// the DOM style code defines this).  All properties defined in an
// #ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL section must have the
// CSSPropFlags::Internal flag set.

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
#define CSS_PROP_(name_, id_, method_, pref_, parsevariant_, kwtable_) CSS_PROP(name_, id_, method_, pref_, parsevariant_, kwtable_)

#else /* !defined(CSS_PROP) */

// An includer who does not define CSS_PROP can define any or all of the
// per-struct macros that are equivalent to it, and the rest will be
// ignored.

#define CSS_PROP_(name_, id_, method_, pref_, parsevariant_, kwtable_) /* nothing */

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
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifyContent)
CSS_PROP_(
    align-items,
    align_items,
    AlignItems,
    "",
    VARIANT_HK,
    kAutoCompletionAlignItems)
CSS_PROP_(
    align-self,
    align_self,
    AlignSelf,
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifySelf)
CSS_PROP_SHORTHAND(
    all,
    all,
    All,
    "layout.css.all-shorthand.enabled")
CSS_PROP_SHORTHAND(
    animation,
    animation,
    Animation,
    "")
CSS_PROP_(
    animation-delay,
    animation_delay,
    AnimationDelay,
    "",
    VARIANT_TIME, // used by list parsing
    nullptr)
CSS_PROP_(
    animation-direction,
    animation_direction,
    AnimationDirection,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kAnimationDirectionKTable)
CSS_PROP_(
    animation-duration,
    animation_duration,
    AnimationDuration,
    "",
    VARIANT_TIME | VARIANT_NONNEGATIVE_DIMENSION, // used by list parsing
    nullptr)
CSS_PROP_(
    animation-fill-mode,
    animation_fill_mode,
    AnimationFillMode,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kAnimationFillModeKTable)
CSS_PROP_(
    animation-iteration-count,
    animation_iteration_count,
    AnimationIterationCount,
    "",
    VARIANT_KEYWORD | VARIANT_NUMBER, // used by list parsing
    kAnimationIterationCountKTable)
CSS_PROP_(
    animation-name,
    animation_name,
    AnimationName,
    "",
    // FIXME: The spec should say something about 'inherit' and 'initial'
    // not being allowed.
    VARIANT_NONE | VARIANT_IDENTIFIER_NO_INHERIT | VARIANT_STRING, // used by list parsing
    nullptr)
CSS_PROP_(
    animation-play-state,
    animation_play_state,
    AnimationPlayState,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kAnimationPlayStateKTable)
CSS_PROP_(
    animation-timing-function,
    animation_timing_function,
    AnimationTimingFunction,
    "",
    VARIANT_KEYWORD | VARIANT_TIMING_FUNCTION, // used by list parsing
    kTransitionTimingFunctionKTable)
CSS_PROP_(
    -moz-appearance,
    _moz_appearance,
    CSS_PROP_DOMPROP_PREFIXED(Appearance),
    "",
    VARIANT_HK,
    kAppearanceKTable)
CSS_PROP_(
    backface-visibility,
    backface_visibility,
    BackfaceVisibility,
    "",
    VARIANT_HK,
    kBackfaceVisibilityKTable)
CSS_PROP_SHORTHAND(
    background,
    background,
    Background,
    "")
CSS_PROP_(
    background-attachment,
    background_attachment,
    BackgroundAttachment,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerAttachmentKTable)
CSS_PROP_(
    background-blend-mode,
    background_blend_mode,
    BackgroundBlendMode,
    "layout.css.background-blend-mode.enabled",
    VARIANT_KEYWORD, // used by list parsing
    kBlendModeKTable)
CSS_PROP_(
    background-clip,
    background_clip,
    BackgroundClip,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kBackgroundClipKTable)
CSS_PROP_(
    background-color,
    background_color,
    BackgroundColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    background-image,
    background_image,
    BackgroundImage,
    "",
    VARIANT_IMAGE, // used by list parsing
    nullptr)
CSS_PROP_(
    background-origin,
    background_origin,
    BackgroundOrigin,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kBackgroundOriginKTable)
CSS_PROP_SHORTHAND(
    background-position,
    background_position,
    BackgroundPosition,
    "")
CSS_PROP_(
    background-position-x,
    background_position_x,
    BackgroundPositionX,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    background-position-y,
    background_position_y,
    BackgroundPositionY,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    background-repeat,
    background_repeat,
    BackgroundRepeat,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerRepeatKTable)
CSS_PROP_(
    background-size,
    background_size,
    BackgroundSize,
    "",
    0,
    kImageLayerSizeKTable)
CSS_PROP_(
    -moz-binding,
    _moz_binding,
    CSS_PROP_DOMPROP_PREFIXED(Binding),
    "",
    VARIANT_HUO,
    nullptr) // XXX bug 3935
CSS_PROP_(
    block-size,
    block_size,
    BlockSize,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    border,
    border,
    Border,
    "")
CSS_PROP_SHORTHAND(
    border-block-end,
    border_block_end,
    BorderBlockEnd,
    "")
CSS_PROP_(
    border-block-end-color,
    border_block_end_color,
    BorderBlockEndColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-block-end-style,
    border_block_end_style,
    BorderBlockEndStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-block-end-width,
    border_block_end_width,
    BorderBlockEndWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-block-start,
    border_block_start,
    BorderBlockStart,
    "")
CSS_PROP_(
    border-block-start-color,
    border_block_start_color,
    BorderBlockStartColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-block-start-style,
    border_block_start_style,
    BorderBlockStartStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-block-start-width,
    border_block_start_width,
    BorderBlockStartWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-bottom,
    border_bottom,
    BorderBottom,
    "")
CSS_PROP_(
    border-bottom-color,
    border_bottom_color,
    BorderBottomColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-bottom-left-radius,
    border_bottom_left_radius,
    BorderBottomLeftRadius,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-bottom-right-radius,
    border_bottom_right_radius,
    BorderBottomRightRadius,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-bottom-style,
    border_bottom_style,
    BorderBottomStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)  // on/off will need reflow
CSS_PROP_(
    border-bottom-width,
    border_bottom_width,
    BorderBottomWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    border-collapse,
    border_collapse,
    BorderCollapse,
    "",
    VARIANT_HK,
    kBorderCollapseKTable)
CSS_PROP_SHORTHAND(
    border-color,
    border_color,
    BorderColor,
    "")
CSS_PROP_SHORTHAND(
    border-image,
    border_image,
    BorderImage,
    "")
CSS_PROP_(
    border-image-outset,
    border_image_outset,
    BorderImageOutset,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-image-repeat,
    border_image_repeat,
    BorderImageRepeat,
    "",
    0,
    kBorderImageRepeatKTable)
CSS_PROP_(
    border-image-slice,
    border_image_slice,
    BorderImageSlice,
    "",
    0,
    kBorderImageSliceKTable)
CSS_PROP_(
    border-image-source,
    border_image_source,
    BorderImageSource,
    "",
    VARIANT_IMAGE | VARIANT_INHERIT,
    nullptr)
CSS_PROP_(
    border-image-width,
    border_image_width,
    BorderImageWidth,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    border-inline-end,
    border_inline_end,
    BorderInlineEnd,
    "")
CSS_PROP_(
    border-inline-end-color,
    border_inline_end_color,
    BorderInlineEndColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-inline-end-style,
    border_inline_end_style,
    BorderInlineEndStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-inline-end-width,
    border_inline_end_width,
    BorderInlineEndWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-inline-start,
    border_inline_start,
    BorderInlineStart,
    "")
CSS_PROP_(
    border-inline-start-color,
    border_inline_start_color,
    BorderInlineStartColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-inline-start-style,
    border_inline_start_style,
    BorderInlineStartStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-inline-start-width,
    border_inline_start_width,
    BorderInlineStartWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-left,
    border_left,
    BorderLeft,
    "")
CSS_PROP_(
    border-left-color,
    border_left_color,
    BorderLeftColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-left-style,
    border_left_style,
    BorderLeftStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-left-width,
    border_left_width,
    BorderLeftWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-radius,
    border_radius,
    BorderRadius,
    "")
CSS_PROP_SHORTHAND(
    border-right,
    border_right,
    BorderRight,
    "")
CSS_PROP_(
    border-right-color,
    border_right_color,
    BorderRightColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-right-style,
    border_right_style,
    BorderRightStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    border-right-width,
    border_right_width,
    BorderRightWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    border-spacing,
    border_spacing,
    BorderSpacing,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    border-style,
    border_style,
    BorderStyle,
    "")  // on/off will need reflow
CSS_PROP_SHORTHAND(
    border-top,
    border_top,
    BorderTop,
    "")
CSS_PROP_(
    border-top-color,
    border_top_color,
    BorderTopColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    border-top-left-radius,
    border_top_left_radius,
    BorderTopLeftRadius,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-top-right-radius,
    border_top_right_radius,
    BorderTopRightRadius,
    "",
    0,
    nullptr)
CSS_PROP_(
    border-top-style,
    border_top_style,
    BorderTopStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)  // on/off will need reflow
CSS_PROP_(
    border-top-width,
    border_top_width,
    BorderTopWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    border-width,
    border_width,
    BorderWidth,
    "")
CSS_PROP_(
    bottom,
    bottom,
    Bottom,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    -moz-box-align,
    _moz_box_align,
    CSS_PROP_DOMPROP_PREFIXED(BoxAlign),
    "",
    VARIANT_HK,
    kBoxAlignKTable) // XXX bug 3935
CSS_PROP_(
    box-decoration-break,
    box_decoration_break,
    BoxDecorationBreak,
    "layout.css.box-decoration-break.enabled",
    VARIANT_HK,
    kBoxDecorationBreakKTable)
CSS_PROP_(
    -moz-box-direction,
    _moz_box_direction,
    CSS_PROP_DOMPROP_PREFIXED(BoxDirection),
    "",
    VARIANT_HK,
    kBoxDirectionKTable) // XXX bug 3935
CSS_PROP_(
    -moz-box-flex,
    _moz_box_flex,
    CSS_PROP_DOMPROP_PREFIXED(BoxFlex),
    "",
    VARIANT_HN,
    nullptr) // XXX bug 3935
CSS_PROP_(
    -moz-box-ordinal-group,
    _moz_box_ordinal_group,
    CSS_PROP_DOMPROP_PREFIXED(BoxOrdinalGroup),
    "",
    VARIANT_HI,
    nullptr)
CSS_PROP_(
    -moz-box-orient,
    _moz_box_orient,
    CSS_PROP_DOMPROP_PREFIXED(BoxOrient),
    "",
    VARIANT_HK,
    kBoxOrientKTable) // XXX bug 3935
CSS_PROP_(
    -moz-box-pack,
    _moz_box_pack,
    CSS_PROP_DOMPROP_PREFIXED(BoxPack),
    "",
    VARIANT_HK,
    kBoxPackKTable) // XXX bug 3935
CSS_PROP_(
    box-shadow,
    box_shadow,
    BoxShadow,
        // NOTE: some components must be nonnegative
    "",
    VARIANT_COLOR | VARIANT_LENGTH | VARIANT_CALC | VARIANT_INHERIT | VARIANT_NONE,
    kBoxShadowTypeKTable)
CSS_PROP_(
    box-sizing,
    box_sizing,
    BoxSizing,
    "",
    VARIANT_HK,
    kBoxSizingKTable)
CSS_PROP_(
    caption-side,
    caption_side,
    CaptionSide,
    "",
    VARIANT_HK,
    kCaptionSideKTable)
CSS_PROP_(
    caret-color,
    caret_color,
    CaretColor,
    "",
    VARIANT_AUTO | VARIANT_HC,
    nullptr)
CSS_PROP_(
    clear,
    clear,
    Clear,
    "",
    VARIANT_HK,
    kClearKTable)
CSS_PROP_(
    clip,
    clip,
    Clip,
    "",
    VARIANT_AH,
    nullptr)
CSS_PROP_(
    clip-path,
    clip_path,
    ClipPath,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    clip-rule,
    clip_rule,
    ClipRule,
    "",
    VARIANT_HK,
    kFillRuleKTable)
CSS_PROP_(
    color,
    color,
    Color,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    color-adjust,
    color_adjust,
    ColorAdjust,
    "layout.css.color-adjust.enabled",
    VARIANT_HK,
    kColorAdjustKTable)
CSS_PROP_(
    color-interpolation,
    color_interpolation,
    ColorInterpolation,
    "",
    VARIANT_HK,
    kColorInterpolationKTable)
CSS_PROP_(
    color-interpolation-filters,
    color_interpolation_filters,
    ColorInterpolationFilters,
    "",
    VARIANT_HK,
    kColorInterpolationKTable)
CSS_PROP_(
    column-count,
    column_count,
    ColumnCount,
    "",
    VARIANT_AHI,
    nullptr)
CSS_PROP_(
    column-fill,
    column_fill,
    ColumnFill,
    "",
    VARIANT_HK,
    kColumnFillKTable)
CSS_PROP_(
    column-gap,
    column_gap,
    ColumnGap,
    "",
    VARIANT_HLP | VARIANT_NORMAL | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    column-rule,
    column_rule,
    ColumnRule,
    "")
CSS_PROP_(
    column-rule-color,
    column_rule_color,
    ColumnRuleColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    column-rule-style,
    column_rule_style,
    ColumnRuleStyle,
    "",
    VARIANT_HK,
    kBorderStyleKTable)
CSS_PROP_(
    column-rule-width,
    column_rule_width,
    ColumnRuleWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    column-span,
    column_span,
    ColumnSpan,
    "layout.css.column-span.enabled",
    VARIANT_HK,
    kColumnSpanKTable)
CSS_PROP_(
    column-width,
    column_width,
    ColumnWidth,
    "",
    VARIANT_AHL | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    columns,
    columns,
    Columns,
    "")
CSS_PROP_(
    contain,
    contain,
    Contain,
    "layout.css.contain.enabled",
    // Does not affect parsing, but is needed for tab completion in devtools:
    VARIANT_HK | VARIANT_NONE,
    kContainKTable)
CSS_PROP_(
    content,
    content,
    Content,
    "",
    VARIANT_HMK | VARIANT_NONE | VARIANT_URL | VARIANT_COUNTER | VARIANT_ATTR,
    kContentKTable)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    // Only intended to be used internally by Mozilla, so prefixed.
    -moz-context-properties,
    _moz_context_properties,
    CSS_PROP_DOMPROP_PREFIXED(ContextProperties),
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-control-character-visibility,
    _moz_control_character_visibility,
    CSS_PROP_DOMPROP_PREFIXED(ControlCharacterVisibility),
    "",
    VARIANT_HK,
    kControlCharacterVisibilityKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    counter-increment,
    counter_increment,
    CounterIncrement,
    "",
    VARIANT_INHERIT | VARIANT_NONE,
    nullptr) // XXX bug 137285
CSS_PROP_(
    counter-reset,
    counter_reset,
    CounterReset,
    "",
    VARIANT_INHERIT | VARIANT_NONE,
    nullptr) // XXX bug 137285
CSS_PROP_(
    cursor,
    cursor,
    Cursor,
    "",
    0,
    kCursorKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    direction,
    direction,
    Direction,
    "",
    VARIANT_HK,
    kDirectionKTable)
#endif // !defined(CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND)
CSS_PROP_(
    display,
    display,
    Display,
    "",
    VARIANT_HK,
    kDisplayKTable)
CSS_PROP_(
    dominant-baseline,
    dominant_baseline,
    DominantBaseline,
    "",
    VARIANT_HK,
    kDominantBaselineKTable)
CSS_PROP_(
    empty-cells,
    empty_cells,
    EmptyCells,
    "",
    VARIANT_HK,
    kEmptyCellsKTable)
CSS_PROP_(
    fill,
    fill,
    Fill,
    "",
    0,
    kContextPatternKTable)
CSS_PROP_(
    fill-opacity,
    fill_opacity,
    FillOpacity,
    "",
    VARIANT_HN | VARIANT_KEYWORD,
    kContextOpacityKTable)
CSS_PROP_(
    fill-rule,
    fill_rule,
    FillRule,
    "",
    VARIANT_HK,
    kFillRuleKTable)
CSS_PROP_(
    filter,
    filter,
    Filter,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    flex,
    flex,
    Flex,
    "")
CSS_PROP_(
    flex-basis,
    flex_basis,
    FlexBasis,
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
    "",
    VARIANT_HK,
    kFlexDirectionKTable)
CSS_PROP_SHORTHAND(
    flex-flow,
    flex_flow,
    FlexFlow,
    "")
CSS_PROP_(
    flex-grow,
    flex_grow,
    FlexGrow,
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
    "",
    VARIANT_HK,
    kFlexWrapKTable)
CSS_PROP_(
    float,
    float,
    CSS_PROP_PUBLIC_OR_PRIVATE(CssFloat, Float),
    "",
    VARIANT_HK,
    kFloatKTable)
CSS_PROP_(
    -moz-float-edge,
    _moz_float_edge,
    CSS_PROP_DOMPROP_PREFIXED(FloatEdge),
    "",
    VARIANT_HK,
    kFloatEdgeKTable) // XXX bug 3935
CSS_PROP_(
    flood-color,
    flood_color,
    FloodColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    flood-opacity,
    flood_opacity,
    FloodOpacity,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_SHORTHAND(
    font,
    font,
    Font,
    "")
CSS_PROP_(
    font-family,
    font_family,
    FontFamily,
    "",
    0,
    nullptr)
CSS_PROP_(
    font-feature-settings,
    font_feature_settings,
    FontFeatureSettings,
    "",
    0,
    nullptr)
CSS_PROP_(
    font-kerning,
    font_kerning,
    FontKerning,
    "",
    VARIANT_HK,
    kFontKerningKTable)
CSS_PROP_(
    font-language-override,
    font_language_override,
    FontLanguageOverride,
    "",
    VARIANT_NORMAL | VARIANT_INHERIT | VARIANT_STRING,
    nullptr)
CSS_PROP_(
    font-optical-sizing,
    font_optical_sizing,
    FontOpticalSizing,
    "layout.css.font-variations.enabled",
    VARIANT_HK,
    kFontOpticalSizingKTable)
CSS_PROP_(
    font-size,
    font_size,
    FontSize,
    "",
    VARIANT_HKLP | VARIANT_SYSFONT | VARIANT_CALC,
    kFontSizeKTable)
CSS_PROP_(
    font-size-adjust,
    font_size_adjust,
    FontSizeAdjust,
    "",
    VARIANT_HON | VARIANT_SYSFONT,
    nullptr)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-font-smoothing-background-color,
    _moz_font_smoothing_background_color,
    CSS_PROP_DOMPROP_PREFIXED(FontSmoothingBackgroundColor),
    "",
    VARIANT_HC,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    font-stretch,
    font_stretch,
    FontStretch,
    "",
    VARIANT_HK | VARIANT_SYSFONT | VARIANT_PERCENT,
    kFontStretchKTable)
CSS_PROP_(
    font-style,
    font_style,
    FontStyle,
    "",
    VARIANT_HK | VARIANT_SYSFONT,
    kFontStyleKTable)
CSS_PROP_(
    font-synthesis,
    font_synthesis,
    FontSynthesis,
    "",
    0,
    kFontSynthesisKTable)
CSS_PROP_SHORTHAND(
    font-variant,
    font_variant,
    FontVariant,
    "")
CSS_PROP_(
    font-variant-alternates,
    font_variant_alternates,
    FontVariantAlternates,
    "",
    0,
    kFontVariantAlternatesKTable)
CSS_PROP_(
    font-variant-caps,
    font_variant_caps,
    FontVariantCaps,
    "",
    VARIANT_HMK,
    kFontVariantCapsKTable)
CSS_PROP_(
    font-variant-east-asian,
    font_variant_east_asian,
    FontVariantEastAsian,
    "",
    0,
    kFontVariantEastAsianKTable)
CSS_PROP_(
    font-variant-ligatures,
    font_variant_ligatures,
    FontVariantLigatures,
    "",
    0,
    kFontVariantLigaturesKTable)
CSS_PROP_(
    font-variant-numeric,
    font_variant_numeric,
    FontVariantNumeric,
    "",
    0,
    kFontVariantNumericKTable)
CSS_PROP_(
    font-variant-position,
    font_variant_position,
    FontVariantPosition,
    "",
    VARIANT_HMK,
    kFontVariantPositionKTable)
CSS_PROP_(
    font-variation-settings,
    font_variation_settings,
    FontVariationSettings,
    "layout.css.font-variations.enabled",
    0,
    nullptr)
CSS_PROP_(
    font-weight,
    font_weight,
    FontWeight,
        // NOTE: This property has range restrictions on interpolation!
    "",
    0,
    kFontWeightKTable)
CSS_PROP_(
    -moz-force-broken-image-icon,
    _moz_force_broken_image_icon,
    CSS_PROP_DOMPROP_PREFIXED(ForceBrokenImageIcon),
    "",
    VARIANT_HI,
    nullptr) // bug 58646
CSS_PROP_SHORTHAND(
    gap,
    gap,
    Gap,
    "")
CSS_PROP_SHORTHAND(
    grid,
    grid,
    Grid,
    "")
CSS_PROP_SHORTHAND(
    grid-area,
    grid_area,
    GridArea,
    "")
CSS_PROP_(
    grid-auto-columns,
    grid_auto_columns,
    GridAutoColumns,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_(
    grid-auto-flow,
    grid_auto_flow,
    GridAutoFlow,
    "",
    0,
    kGridAutoFlowKTable)
CSS_PROP_(
    grid-auto-rows,
    grid_auto_rows,
    GridAutoRows,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_SHORTHAND(
    grid-column,
    grid_column,
    GridColumn,
    "")
CSS_PROP_(
    grid-column-end,
    grid_column_end,
    GridColumnEnd,
    "",
    0,
    nullptr)
CSS_PROP_(
    grid-column-start,
    grid_column_start,
    GridColumnStart,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    grid-row,
    grid_row,
    GridRow,
    "")
CSS_PROP_(
    grid-row-end,
    grid_row_end,
    GridRowEnd,
    "",
    0,
    nullptr)
CSS_PROP_(
    grid-row-start,
    grid_row_start,
    GridRowStart,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    grid-template,
    grid_template,
    GridTemplate,
    "")
CSS_PROP_(
    grid-template-areas,
    grid_template_areas,
    GridTemplateAreas,
    "",
    0,
    nullptr)
CSS_PROP_(
    grid-template-columns,
    grid_template_columns,
    GridTemplateColumns,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_(
    grid-template-rows,
    grid_template_rows,
    GridTemplateRows,
    "",
    0,
    kGridTrackBreadthKTable)
CSS_PROP_(
    height,
    height,
    Height,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    hyphens,
    hyphens,
    Hyphens,
    "",
    VARIANT_HK,
    kHyphensKTable)
CSS_PROP_(
    image-orientation,
    image_orientation,
    ImageOrientation,
    "layout.css.image-orientation.enabled",
    0,
    kImageOrientationKTable)
CSS_PROP_(
    -moz-image-region,
    _moz_image_region,
    CSS_PROP_DOMPROP_PREFIXED(ImageRegion),
    "",
    0,
    nullptr)
CSS_PROP_(
    image-rendering,
    image_rendering,
    ImageRendering,
    "",
    VARIANT_HK,
    kImageRenderingKTable)
CSS_PROP_(
    ime-mode,
    ime_mode,
    ImeMode,
    "",
    VARIANT_HK,
    kIMEModeKTable)
CSS_PROP_(
    initial-letter,
    initial_letter,
    InitialLetter,
    "layout.css.initial-letter.enabled",
    0,
    nullptr)
CSS_PROP_(
    inline-size,
    inline_size,
    InlineSize,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    isolation,
    isolation,
    Isolation,
    "layout.css.isolation.enabled",
    VARIANT_HK,
    kIsolationKTable)
CSS_PROP_(
    justify-content,
    justify_content,
    JustifyContent,
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifyContent)
CSS_PROP_(
    justify-items,
    justify_items,
    JustifyItems,
    "",
    VARIANT_HK,
    // for auto-completion we use same values as justify-self:
    kAutoCompletionAlignJustifySelf)
CSS_PROP_(
    justify-self,
    justify_self,
    JustifySelf,
    "",
    VARIANT_HK,
    kAutoCompletionAlignJustifySelf)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -x-lang,
    _x_lang,
    Lang,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    left,
    left,
    Left,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    letter-spacing,
    letter_spacing,
    LetterSpacing,
    "",
    VARIANT_HL | VARIANT_NORMAL | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    lighting-color,
    lighting_color,
    LightingColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    line-height,
    line_height,
    LineHeight,
    "",
    VARIANT_HLPN | VARIANT_KEYWORD | VARIANT_NORMAL | VARIANT_SYSFONT | VARIANT_CALC,
    kLineHeightKTable)
CSS_PROP_SHORTHAND(
    list-style,
    list_style,
    ListStyle,
    "")
CSS_PROP_(
    list-style-image,
    list_style_image,
    ListStyleImage,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    list-style-position,
    list_style_position,
    ListStylePosition,
    "",
    VARIANT_HK,
    kListStylePositionKTable)
CSS_PROP_(
    list-style-type,
    list_style_type,
    ListStyleType,
    "",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    margin,
    margin,
    Margin,
    "")
CSS_PROP_(
    margin-block-end,
    margin_block_end,
    MarginBlockEnd,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-block-start,
    margin_block_start,
    MarginBlockStart,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-bottom,
    margin_bottom,
    MarginBottom,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-inline-end,
    margin_inline_end,
    MarginInlineEnd,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-inline-start,
    margin_inline_start,
    MarginInlineStart,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-left,
    margin_left,
    MarginLeft,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-right,
    margin_right,
    MarginRight,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    margin-top,
    margin_top,
    MarginTop,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    marker,
    marker,
    Marker,
    "")
CSS_PROP_(
    marker-end,
    marker_end,
    MarkerEnd,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    marker-mid,
    marker_mid,
    MarkerMid,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_(
    marker-start,
    marker_start,
    MarkerStart,
    "",
    VARIANT_HUO,
    nullptr)
CSS_PROP_SHORTHAND(
    mask,
    mask,
    Mask,
    "")
CSS_PROP_(
    mask-clip,
    mask_clip,
    MaskClip,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kMaskClipKTable)
CSS_PROP_(
    mask-composite,
    mask_composite,
    MaskComposite,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerCompositeKTable)
CSS_PROP_(
    mask-image,
    mask_image,
    MaskImage,
    "",
    VARIANT_IMAGE, // used by list parsing
    nullptr)
CSS_PROP_(
    mask-mode,
    mask_mode,
    MaskMode,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerModeKTable)
CSS_PROP_(
    mask-origin,
    mask_origin,
    MaskOrigin,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kMaskOriginKTable)
CSS_PROP_SHORTHAND(
    mask-position,
    mask_position,
    MaskPosition,
    "")
CSS_PROP_(
    mask-position-x,
    mask_position_x,
    MaskPositionX,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    mask-position-y,
    mask_position_y,
    MaskPositionY,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    mask-repeat,
    mask_repeat,
    MaskRepeat,
    "",
    VARIANT_KEYWORD, // used by list parsing
    kImageLayerRepeatKTable)
CSS_PROP_(
    mask-size,
    mask_size,
    MaskSize,
    "",
    0,
    kImageLayerSizeKTable)
CSS_PROP_(
    mask-type,
    mask_type,
    MaskType,
    "",
    VARIANT_HK,
    kMaskTypeKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-math-display,
    _moz_math_display,
    MathDisplay,
    "",
    VARIANT_HK,
    kMathDisplayKTable)
CSS_PROP_(
    -moz-math-variant,
    _moz_math_variant,
    MathVariant,
    "",
    VARIANT_HK,
    kMathVariantKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    max-block-size,
    max_block_size,
    MaxBlockSize,
    "",
    VARIANT_HLPO | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    max-height,
    max_height,
    MaxHeight,
    "",
    VARIANT_HKLPO | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    max-inline-size,
    max_inline_size,
    MaxInlineSize,
    "",
    VARIANT_HKLPO | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    max-width,
    max_width,
    MaxWidth,
    "",
    VARIANT_HKLPO | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    min-block-size,
    min_block_size,
    MinBlockSize,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-min-font-size-ratio,
    _moz_min_font_size_ratio,
    CSS_PROP_DOMPROP_PREFIXED(MinFontSizeRatio),
    "",
    VARIANT_INHERIT | VARIANT_PERCENT,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    min-height,
    min_height,
    MinHeight,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    min-inline-size,
    min_inline_size,
    MinInlineSize,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    min-width,
    min_width,
    MinWidth,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    mix-blend-mode,
    mix_blend_mode,
    MixBlendMode,
    "layout.css.mix-blend-mode.enabled",
    VARIANT_HK,
    kBlendModeKTable)
CSS_PROP_(
    object-fit,
    object_fit,
    ObjectFit,
    "",
    VARIANT_HK,
    kObjectFitKTable)
CSS_PROP_(
    object-position,
    object_position,
    ObjectPosition,
    "",
    VARIANT_CALC,
    kImageLayerPositionKTable)
CSS_PROP_(
    offset-block-end,
    offset_block_end,
    OffsetBlockEnd,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    offset-block-start,
    offset_block_start,
    OffsetBlockStart,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    offset-inline-end,
    offset_inline_end,
    OffsetInlineEnd,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    offset-inline-start,
    offset_inline_start,
    OffsetInlineStart,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    opacity,
    opacity,
    Opacity,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    order,
    order,
    Order,
    "",
    VARIANT_HI,
    nullptr) // <integer>
CSS_PROP_(
    -moz-orient,
    _moz_orient,
    CSS_PROP_DOMPROP_PREFIXED(Orient),
    "",
    VARIANT_HK,
    kOrientKTable)
CSS_PROP_(
    -moz-osx-font-smoothing,
    _moz_osx_font_smoothing,
    CSS_PROP_DOMPROP_PREFIXED(OsxFontSmoothing),
    "layout.css.osx-font-smoothing.enabled",
    VARIANT_HK,
    kFontSmoothingKTable)
CSS_PROP_SHORTHAND(
    outline,
    outline,
    Outline,
    "")
CSS_PROP_(
    outline-color,
    outline_color,
    OutlineColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    outline-offset,
    outline_offset,
    OutlineOffset,
    "",
    VARIANT_HL | VARIANT_CALC,
    nullptr)
CSS_PROP_SHORTHAND(
    -moz-outline-radius,
    _moz_outline_radius,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadius),
    "")
CSS_PROP_(
    -moz-outline-radius-bottomleft,
    _moz_outline_radius_bottomleft,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusBottomleft),
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-outline-radius-bottomright,
    _moz_outline_radius_bottomright,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusBottomright),
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-outline-radius-topleft,
    _moz_outline_radius_topleft,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusTopleft),
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-outline-radius-topright,
    _moz_outline_radius_topright,
    CSS_PROP_DOMPROP_PREFIXED(OutlineRadiusTopright),
    "",
    0,
    nullptr)
CSS_PROP_(
    outline-style,
    outline_style,
    OutlineStyle,
    "",
    VARIANT_HK,
    kOutlineStyleKTable)
CSS_PROP_(
    outline-width,
    outline_width,
    OutlineWidth,
    "",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_SHORTHAND(
    overflow,
    overflow,
    Overflow,
    "")
CSS_PROP_SHORTHAND(
    overflow-clip-box,
    overflow_clip_box,
    OverflowClipBox,
    "layout.css.overflow-clip-box.enabled")
CSS_PROP_(
    overflow-clip-box-block,
    overflow_clip_box_block,
    OverflowClipBoxBlock,
    "layout.css.overflow-clip-box.enabled",
    VARIANT_HK,
    kOverflowClipBoxKTable)
CSS_PROP_(
    overflow-clip-box-inline,
    overflow_clip_box_inline,
    OverflowClipBoxInline,
    "layout.css.overflow-clip-box.enabled",
    VARIANT_HK,
    kOverflowClipBoxKTable)
CSS_PROP_(
    overflow-wrap,
    overflow_wrap,
    OverflowWrap,
    "",
    VARIANT_HK,
    kOverflowWrapKTable)
CSS_PROP_(
    overflow-x,
    overflow_x,
    OverflowX,
    "",
    VARIANT_HK,
    kOverflowSubKTable)
CSS_PROP_(
    overflow-y,
    overflow_y,
    OverflowY,
    "",
    VARIANT_HK,
    kOverflowSubKTable)
CSS_PROP_SHORTHAND(
    overscroll-behavior,
    overscroll_behavior,
    OverscrollBehavior,
    "layout.css.overscroll-behavior.enabled")
CSS_PROP_(
    overscroll-behavior-x,
    overscroll_behavior_x,
    OverscrollBehaviorX,
    "layout.css.overscroll-behavior.enabled",
    VARIANT_HK,
    kOverscrollBehaviorKTable)
CSS_PROP_(
    overscroll-behavior-y,
    overscroll_behavior_y,
    OverscrollBehaviorY,
    "layout.css.overscroll-behavior.enabled",
    VARIANT_HK,
    kOverscrollBehaviorKTable)
CSS_PROP_SHORTHAND(
    padding,
    padding,
    Padding,
    "")
CSS_PROP_(
    padding-block-end,
    padding_block_end,
    PaddingBlockEnd,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-block-start,
    padding_block_start,
    PaddingBlockStart,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-bottom,
    padding_bottom,
    PaddingBottom,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-inline-end,
    padding_inline_end,
    PaddingInlineEnd,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-inline-start,
    padding_inline_start,
    PaddingInlineStart,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-left,
    padding_left,
    PaddingLeft,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-right,
    padding_right,
    PaddingRight,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    padding-top,
    padding_top,
    PaddingTop,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    page-break-after,
    page_break_after,
    PageBreakAfter,
    "",
    VARIANT_HK,
    kPageBreakKTable) // temp fix for bug 24000
CSS_PROP_(
    page-break-before,
    page_break_before,
    PageBreakBefore,
    "",
    VARIANT_HK,
    kPageBreakKTable) // temp fix for bug 24000
CSS_PROP_(
    page-break-inside,
    page_break_inside,
    PageBreakInside,
    "",
    VARIANT_HK,
    kPageBreakInsideKTable)
CSS_PROP_(
    paint-order,
    paint_order,
    PaintOrder,
    "",
    0,
    nullptr)
CSS_PROP_(
    perspective,
    perspective,
    Perspective,
    "",
    VARIANT_NONE | VARIANT_INHERIT | VARIANT_LENGTH |
      VARIANT_NONNEGATIVE_DIMENSION,
    nullptr)
CSS_PROP_(
    perspective-origin,
    perspective_origin,
    PerspectiveOrigin,
    "",
    VARIANT_CALC,
    kImageLayerPositionKTable)
CSS_PROP_SHORTHAND(
    place-content,
    place_content,
    PlaceContent,
    "")
CSS_PROP_SHORTHAND(
    place-items,
    place_items,
    PlaceItems,
    "")
CSS_PROP_SHORTHAND(
    place-self,
    place_self,
    PlaceSelf,
    "")
CSS_PROP_(
    pointer-events,
    pointer_events,
    PointerEvents,
    "",
    VARIANT_HK,
    kPointerEventsKTable)
CSS_PROP_(
    position,
    position,
    Position,
    "",
    VARIANT_HK,
    kPositionKTable)
CSS_PROP_(
    quotes,
    quotes,
    Quotes,
    "",
    VARIANT_HOS,
    nullptr)
CSS_PROP_(
    resize,
    resize,
    Resize,
    "",
    VARIANT_HK,
    kResizeKTable)
CSS_PROP_(
    right,
    right,
    Right,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    rotate,
    rotate,
    Rotate,
    "layout.css.individual-transform.enabled",
    0,
    nullptr)
CSS_PROP_(
    row-gap,
    row_gap,
    RowGap,
    "",
    VARIANT_HLP | VARIANT_NORMAL | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    ruby-align,
    ruby_align,
    RubyAlign,
    "",
    VARIANT_HK,
    kRubyAlignKTable)
CSS_PROP_(
    ruby-position,
    ruby_position,
    RubyPosition,
    "",
    VARIANT_HK,
    kRubyPositionKTable)
CSS_PROP_(
    scale,
    scale,
    Scale,
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
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-script-size-multiplier,
    _moz_script_size_multiplier,
    ScriptSizeMultiplier,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    scroll-behavior,
    scroll_behavior,
    ScrollBehavior,
    "layout.css.scroll-behavior.property-enabled",
    VARIANT_HK,
    kScrollBehaviorKTable)
CSS_PROP_(
    scroll-snap-coordinate,
    scroll_snap_coordinate,
    ScrollSnapCoordinate,
    "layout.css.scroll-snap.enabled",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    scroll-snap-destination,
    scroll_snap_destination,
    ScrollSnapDestination,
    "layout.css.scroll-snap.enabled",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    scroll-snap-points-x,
    scroll_snap_points_x,
    ScrollSnapPointsX,
    "layout.css.scroll-snap.enabled",
    0,
    nullptr)
CSS_PROP_(
    scroll-snap-points-y,
    scroll_snap_points_y,
    ScrollSnapPointsY,
    "layout.css.scroll-snap.enabled",
    0,
    nullptr)
CSS_PROP_SHORTHAND(
    scroll-snap-type,
    scroll_snap_type,
    ScrollSnapType,
    "layout.css.scroll-snap.enabled")
CSS_PROP_(
    scroll-snap-type-x,
    scroll_snap_type_x,
    ScrollSnapTypeX,
    "layout.css.scroll-snap.enabled",
    VARIANT_HK,
    kScrollSnapTypeKTable)
CSS_PROP_(
    scroll-snap-type-y,
    scroll_snap_type_y,
    ScrollSnapTypeY,
    "layout.css.scroll-snap.enabled",
    VARIANT_HK,
    kScrollSnapTypeKTable)
CSS_PROP_(
    shape-image-threshold,
    shape_image_threshold,
    ShapeImageThreshold,
    "layout.css.shape-outside.enabled",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    shape-margin,
    shape_margin,
    ShapeMargin,
    "layout.css.shape-outside.enabled",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    shape-outside,
    shape_outside,
    ShapeOutside,
    "layout.css.shape-outside.enabled",
    0,
    nullptr)
CSS_PROP_(
    shape-rendering,
    shape_rendering,
    ShapeRendering,
    "",
    VARIANT_HK,
    kShapeRenderingKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -x-span,
    _x_span,
    Span,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    -moz-stack-sizing,
    _moz_stack_sizing,
    CSS_PROP_DOMPROP_PREFIXED(StackSizing),
    "",
    VARIANT_HK,
    kStackSizingKTable)
CSS_PROP_(
    stop-color,
    stop_color,
    StopColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    stop-opacity,
    stop_opacity,
    StopOpacity,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    stroke,
    stroke,
    Stroke,
    "",
    0,
    kContextPatternKTable)
CSS_PROP_(
    stroke-dasharray,
    stroke_dasharray,
    StrokeDasharray,
        // NOTE: Internal values have range restrictions.
    "",
    0,
    kStrokeContextValueKTable)
CSS_PROP_(
    stroke-dashoffset,
    stroke_dashoffset,
    StrokeDashoffset,
    "",
    VARIANT_HLPN | VARIANT_OPENTYPE_SVG_KEYWORD,
    kStrokeContextValueKTable)
CSS_PROP_(
    stroke-linecap,
    stroke_linecap,
    StrokeLinecap,
    "",
    VARIANT_HK,
    kStrokeLinecapKTable)
CSS_PROP_(
    stroke-linejoin,
    stroke_linejoin,
    StrokeLinejoin,
    "",
    VARIANT_HK,
    kStrokeLinejoinKTable)
CSS_PROP_(
    stroke-miterlimit,
    stroke_miterlimit,
    StrokeMiterlimit,
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    stroke-opacity,
    stroke_opacity,
    StrokeOpacity,
    "",
    VARIANT_HN | VARIANT_KEYWORD,
    kContextOpacityKTable)
CSS_PROP_(
    stroke-width,
    stroke_width,
    StrokeWidth,
    "",
    VARIANT_HLPN | VARIANT_OPENTYPE_SVG_KEYWORD,
    kStrokeContextValueKTable)
CSS_PROP_(
    -moz-tab-size,
    _moz_tab_size,
    CSS_PROP_DOMPROP_PREFIXED(TabSize),
    "",
    VARIANT_INHERIT | VARIANT_LNCALC,
    nullptr)
CSS_PROP_(
    table-layout,
    table_layout,
    TableLayout,
    "",
    VARIANT_HK,
    kTableLayoutKTable)
CSS_PROP_(
    text-align,
    text_align,
    TextAlign,
    "",
    // When we support aligning on a string, we can parse text-align
    // as a string....
    VARIANT_HK /* | VARIANT_STRING */,
    kTextAlignKTable)
CSS_PROP_(
    text-align-last,
    text_align_last,
    TextAlignLast,
    "",
    VARIANT_HK,
    kTextAlignLastKTable)
CSS_PROP_(
    text-anchor,
    text_anchor,
    TextAnchor,
    "",
    VARIANT_HK,
    kTextAnchorKTable)
CSS_PROP_(
    text-combine-upright,
    text_combine_upright,
    TextCombineUpright,
    "layout.css.text-combine-upright.enabled",
    0,
    kTextCombineUprightKTable)
CSS_PROP_SHORTHAND(
    text-decoration,
    text_decoration,
    TextDecoration,
    "")
CSS_PROP_(
    text-decoration-color,
    text_decoration_color,
    TextDecorationColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    text-decoration-line,
    text_decoration_line,
    TextDecorationLine,
    "",
    0,
    kTextDecorationLineKTable)
CSS_PROP_(
    text-decoration-style,
    text_decoration_style,
    TextDecorationStyle,
    "",
    VARIANT_HK,
    kTextDecorationStyleKTable)
CSS_PROP_SHORTHAND(
    text-emphasis,
    text_emphasis,
    TextEmphasis,
    "")
CSS_PROP_(
    text-emphasis-color,
    text_emphasis_color,
    TextEmphasisColor,
    "",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    text-emphasis-position,
    text_emphasis_position,
    TextEmphasisPosition,
    "",
    0,
    kTextEmphasisPositionKTable)
CSS_PROP_(
    text-emphasis-style,
    text_emphasis_style,
    TextEmphasisStyle,
    "",
    0,
    nullptr)
CSS_PROP_(
    -webkit-text-fill-color,
    _webkit_text_fill_color,
    WebkitTextFillColor,
    "layout.css.prefixes.webkit",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    text-indent,
    text_indent,
    TextIndent,
    "",
    VARIANT_HLP | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    text-justify,
    text_justify,
    TextJustify,
    "layout.css.text-justify.enabled",
    VARIANT_HK,
    kTextJustifyKTable)
CSS_PROP_(
    text-orientation,
    text_orientation,
    TextOrientation,
    "",
    VARIANT_HK,
    kTextOrientationKTable)
CSS_PROP_(
    text-overflow,
    text_overflow,
    TextOverflow,
    "",
    0,
    kTextOverflowKTable)
CSS_PROP_(
    text-rendering,
    text_rendering,
    TextRendering,
    "",
    VARIANT_HK,
    kTextRenderingKTable)
CSS_PROP_(
    text-shadow,
    text_shadow,
    TextShadow,
        // NOTE: some components must be nonnegative
    "",
    VARIANT_COLOR | VARIANT_LENGTH | VARIANT_CALC | VARIANT_INHERIT | VARIANT_NONE,
    nullptr)
CSS_PROP_(
    -moz-text-size-adjust,
    _moz_text_size_adjust,
    CSS_PROP_DOMPROP_PREFIXED(TextSizeAdjust),
    "",
    VARIANT_HK,
    kTextSizeAdjustKTable)
CSS_PROP_SHORTHAND(
    -webkit-text-stroke,
    _webkit_text_stroke,
    WebkitTextStroke,
    "layout.css.prefixes.webkit")
CSS_PROP_(
    -webkit-text-stroke-color,
    _webkit_text_stroke_color,
    WebkitTextStrokeColor,
    "layout.css.prefixes.webkit",
    VARIANT_HC,
    nullptr)
CSS_PROP_(
    -webkit-text-stroke-width,
    _webkit_text_stroke_width,
    WebkitTextStrokeWidth,
    "layout.css.prefixes.webkit",
    VARIANT_HKL | VARIANT_CALC,
    kBorderWidthKTable)
CSS_PROP_(
    text-transform,
    text_transform,
    TextTransform,
    "",
    VARIANT_HK,
    kTextTransformKTable)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -x-text-zoom,
    _x_text_zoom,
    TextZoom,
    "",
    0,
    nullptr)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    top,
    top,
    Top,
    "",
    VARIANT_AHLP | VARIANT_CALC,
    nullptr)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-top-layer,
    _moz_top_layer,
    CSS_PROP_DOMPROP_PREFIXED(TopLayer),
    "",
    VARIANT_HK,
    kTopLayerKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    touch-action,
    touch_action,
    TouchAction,
    "layout.css.touch_action.enabled",
    VARIANT_HK,
    kTouchActionKTable)
CSS_PROP_(
    transform,
    transform,
    Transform,
    "",
    0,
    nullptr)
CSS_PROP_(
    transform-box,
    transform_box,
    TransformBox,
    "svg.transform-box.enabled",
    VARIANT_HK,
    kTransformBoxKTable)
CSS_PROP_(
    transform-origin,
    transform_origin,
    TransformOrigin,
    "",
    0,
    kImageLayerPositionKTable)
CSS_PROP_(
    transform-style,
    transform_style,
    TransformStyle,
    "",
    VARIANT_HK,
    kTransformStyleKTable)
CSS_PROP_SHORTHAND(
    transition,
    transition,
    Transition,
    "")
CSS_PROP_(
    transition-delay,
    transition_delay,
    TransitionDelay,
    "",
    VARIANT_TIME, // used by list parsing
    nullptr)
CSS_PROP_(
    transition-duration,
    transition_duration,
    TransitionDuration,
    "",
    VARIANT_TIME | VARIANT_NONNEGATIVE_DIMENSION, // used by list parsing
    nullptr)
CSS_PROP_(
    transition-property,
    transition_property,
    TransitionProperty,
    "",
    VARIANT_IDENTIFIER | VARIANT_NONE | VARIANT_ALL, // used only in shorthand
    nullptr)
CSS_PROP_(
    transition-timing-function,
    transition_timing_function,
    TransitionTimingFunction,
    "",
    VARIANT_KEYWORD | VARIANT_TIMING_FUNCTION, // used by list parsing
    kTransitionTimingFunctionKTable)
CSS_PROP_(
    translate,
    translate,
    Translate,
    "layout.css.individual-transform.enabled",
    0,
    nullptr)
#ifndef CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    unicode-bidi,
    unicode_bidi,
    UnicodeBidi,
    "",
    VARIANT_HK,
    kUnicodeBidiKTable)
#endif // CSS_PROP_LIST_ONLY_COMPONENTS_OF_ALL_SHORTHAND
CSS_PROP_(
    -moz-user-focus,
    _moz_user_focus,
    CSS_PROP_DOMPROP_PREFIXED(UserFocus),
    "",
    VARIANT_HK,
    kUserFocusKTable) // XXX bug 3935
CSS_PROP_(
    -moz-user-input,
    _moz_user_input,
    CSS_PROP_DOMPROP_PREFIXED(UserInput),
    "",
    VARIANT_HK,
    kUserInputKTable) // XXX ??? // XXX bug 3935
CSS_PROP_(
    -moz-user-modify,
    _moz_user_modify,
    CSS_PROP_DOMPROP_PREFIXED(UserModify),
    "",
    VARIANT_HK,
    kUserModifyKTable) // XXX bug 3935
CSS_PROP_(
    -moz-user-select,
    _moz_user_select,
    CSS_PROP_DOMPROP_PREFIXED(UserSelect),
    "",
    VARIANT_HK,
    kUserSelectKTable) // XXX bug 3935
CSS_PROP_(
    vector-effect,
    vector_effect,
    VectorEffect,
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
    "",
    VARIANT_HKLP | VARIANT_CALC,
    kVerticalAlignKTable)
CSS_PROP_(
    visibility,
    visibility,
    Visibility,
    "",
    VARIANT_HK,
    kVisibilityKTable)  // reflow for collapse
CSS_PROP_(
    white-space,
    white_space,
    WhiteSpace,
    "",
    VARIANT_HK,
    kWhitespaceKTable)
CSS_PROP_(
    width,
    width,
    Width,
    "",
    VARIANT_AHKLP | VARIANT_CALC,
    kWidthKTable)
CSS_PROP_(
    will-change,
    will_change,
    WillChange,
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-window-dragging,
    _moz_window_dragging,
    CSS_PROP_DOMPROP_PREFIXED(WindowDragging),
    "",
    VARIANT_HK,
    kWindowDraggingKTable)
#ifndef CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    -moz-window-opacity,
    _moz_window_opacity,
    CSS_PROP_DOMPROP_PREFIXED(WindowOpacity),
    "",
    VARIANT_HN,
    nullptr)
CSS_PROP_(
    -moz-window-shadow,
    _moz_window_shadow,
    CSS_PROP_DOMPROP_PREFIXED(WindowShadow),
    "",
    VARIANT_HK,
    kWindowShadowKTable)
CSS_PROP_(
    -moz-window-transform,
    _moz_window_transform,
    CSS_PROP_DOMPROP_PREFIXED(WindowTransform),
    "",
    0,
    nullptr)
CSS_PROP_(
    -moz-window-transform-origin,
    _moz_window_transform_origin,
    CSS_PROP_DOMPROP_PREFIXED(WindowTransformOrigin),
    "",
    0,
    kImageLayerPositionKTable)
#endif // CSS_PROP_LIST_EXCLUDE_INTERNAL
CSS_PROP_(
    word-break,
    word_break,
    WordBreak,
    "",
    VARIANT_HK,
    kWordBreakKTable)
CSS_PROP_(
    word-spacing,
    word_spacing,
    WordSpacing,
    "",
    VARIANT_HLP | VARIANT_NORMAL | VARIANT_CALC,
    nullptr)
CSS_PROP_(
    writing-mode,
    writing_mode,
    WritingMode,
    "",
    VARIANT_HK,
    kWritingModeKTable)
CSS_PROP_(
    z-index,
    z_index,
    ZIndex,
    "",
    VARIANT_AHI,
    nullptr)

#undef CSS_PROP_

#ifdef DEFINED_CSS_PROP_SHORTHAND
#undef CSS_PROP_SHORTHAND
#undef DEFINED_CSS_PROP_SHORTHAND
#endif

#undef CSS_PROP_DOMPROP_PREFIXED
