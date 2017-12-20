/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a list of all CSS property aliases with data about them, for preprocessing
 */

/******

  This file contains the list of all CSS properties that are just
  aliases for other properties (e.g., for when we temporarily continue
  to support a prefixed property after adding support for its unprefixed
  form).  It is designed to be used as inline input through the magic of
  C preprocessing.  All entries must be enclosed in the appropriate
  CSS_PROP_ALIAS macro which will have cruel and unusual things done to
  it.

  The arguments to CSS_PROP_ALIAS are:

  -. 'aliasname' entries represent a CSS property name and *must* use
  only lowercase characters.

  -. 'aliasid' represent a CSS property name but in snake case. This
  is used in Servo pref check.

  -. 'id' should be the same as the 'id' field in nsCSSPropList.h for
  the property that 'aliasname' is being aliased to.

  -. 'method' is the CSS2Properties property name.  Unlike
  nsCSSPropList.h, prefixes should just be included in this file (rather
  than needing the CSS_PROP_DOMPROP_PREFIXED(prop) macro).

  -. 'pref' is the name of a pref that controls whether the property
  is enabled.  The property is enabled if 'pref' is an empty string,
  or if the boolean property whose name is 'pref' is set to true.

 ******/

CSS_PROP_ALIAS(word-wrap,
               word_wrap,
               overflow_wrap,
               WordWrap,
               "")
CSS_PROP_ALIAS(-moz-transform-origin,
               _moz_transform_origin,
               transform_origin,
               MozTransformOrigin,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-perspective-origin,
               _moz_perspective_origin,
               perspective_origin,
               MozPerspectiveOrigin,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-perspective,
               _moz_perspective,
               perspective,
               MozPerspective,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-transform-style,
               _moz_transform_style,
               transform_style,
               MozTransformStyle,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-backface-visibility,
               _moz_backface_visibility,
               backface_visibility,
               MozBackfaceVisibility,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-border-image,
               _moz_border_image,
               border_image,
               MozBorderImage,
               "layout.css.prefixes.border-image")
CSS_PROP_ALIAS(-moz-transition,
               _moz_transition,
               transition,
               MozTransition,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-delay,
               _moz_transition_delay,
               transition_delay,
               MozTransitionDelay,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-duration,
               _moz_transition_duration,
               transition_duration,
               MozTransitionDuration,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-property,
               _moz_transition_property,
               transition_property,
               MozTransitionProperty,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-timing-function,
               _moz_transition_timing_function,
               transition_timing_function,
               MozTransitionTimingFunction,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-animation,
               _moz_animation,
               animation,
               MozAnimation,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-delay,
               _moz_animation_delay,
               animation_delay,
               MozAnimationDelay,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-direction,
               _moz_animation_direction,
               animation_direction,
               MozAnimationDirection,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-duration,
               _moz_animation_duration,
               animation_duration,
               MozAnimationDuration,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-fill-mode,
               _moz_animation_fill_mode,
               animation_fill_mode,
               MozAnimationFillMode,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-iteration-count,
               _moz_animation_iteration_count,
               animation_iteration_count,
               MozAnimationIterationCount,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-name,
               _moz_animation_name,
               animation_name,
               MozAnimationName,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-play-state,
               _moz_animation_play_state,
               animation_play_state,
               MozAnimationPlayState,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-timing-function,
               _moz_animation_timing_function,
               animation_timing_function,
               MozAnimationTimingFunction,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-box-sizing,
               _moz_box_sizing,
               box_sizing,
               MozBoxSizing,
               "layout.css.prefixes.box-sizing")
CSS_PROP_ALIAS(-moz-font-feature-settings,
               _moz_font_feature_settings,
               font_feature_settings,
               MozFontFeatureSettings,
               "layout.css.prefixes.font-features")
CSS_PROP_ALIAS(-moz-font-language-override,
               _moz_font_language_override,
               font_language_override,
               MozFontLanguageOverride,
               "layout.css.prefixes.font-features")
CSS_PROP_ALIAS(-moz-padding-end,
               _moz_padding_end,
               padding_inline_end,
               MozPaddingEnd,
               "")
CSS_PROP_ALIAS(-moz-padding-start,
               _moz_padding_start,
               padding_inline_start,
               MozPaddingStart,
               "")
CSS_PROP_ALIAS(-moz-margin-end,
               _moz_margin_end,
               margin_inline_end,
               MozMarginEnd,
               "")
CSS_PROP_ALIAS(-moz-margin-start,
               _moz_margin_start,
               margin_inline_start,
               MozMarginStart,
               "")
CSS_PROP_ALIAS(-moz-border-end,
               _moz_border_end,
               border_inline_end,
               MozBorderEnd,
               "")
CSS_PROP_ALIAS(-moz-border-end-color,
               _moz_border_end_color,
               border_inline_end_color,
               MozBorderEndColor,
               "")
CSS_PROP_ALIAS(-moz-border-end-style,
               _moz_border_end_style,
               border_inline_end_style,
               MozBorderEndStyle,
               "")
CSS_PROP_ALIAS(-moz-border-end-width,
               _moz_border_end_width,
               border_inline_end_width,
               MozBorderEndWidth,
               "")
CSS_PROP_ALIAS(-moz-border-start,
               _moz_border_start,
               border_inline_start,
               MozBorderStart,
               "")
CSS_PROP_ALIAS(-moz-border-start-color,
               _moz_border_start_color,
               border_inline_start_color,
               MozBorderStartColor,
               "")
CSS_PROP_ALIAS(-moz-border-start-style,
               _moz_border_start_style,
               border_inline_start_style,
               MozBorderStartStyle,
               "")
CSS_PROP_ALIAS(-moz-border-start-width,
               _moz_border_start_width,
               border_inline_start_width,
               MozBorderStartWidth,
               "")
CSS_PROP_ALIAS(-moz-hyphens,
               _moz_hyphens,
               hyphens,
               MozHyphens,
               "")
CSS_PROP_ALIAS(-moz-column-count,
               _moz_column_count,
               column_count,
               MozColumnCount,
               "")
CSS_PROP_ALIAS(-moz-column-fill,
               _moz_column_fill,
               column_fill,
               MozColumnFill,
               "")
CSS_PROP_ALIAS(-moz-column-gap,
               _moz_column_gap,
               column_gap,
               MozColumnGap,
               "")
CSS_PROP_ALIAS(-moz-column-rule,
               _moz_column_rule,
               column_rule,
               MozColumnRule,
               "")
CSS_PROP_ALIAS(-moz-column-rule-color,
               _moz_column_rule_color,
               column_rule_color,
               MozColumnRuleColor,
               "")
CSS_PROP_ALIAS(-moz-column-rule-style,
               _moz_column_rule_style,
               column_rule_style,
               MozColumnRuleStyle,
               "")
CSS_PROP_ALIAS(-moz-column-rule-width,
               _moz_column_rule_width,
               column_rule_width,
               MozColumnRuleWidth,
               "")
CSS_PROP_ALIAS(-moz-column-span,
               _moz_column_span,
               column_span,
               MozColumnSpan,
               "layout.css.column-span.enabled")
CSS_PROP_ALIAS(-moz-column-width,
               _moz_column_width,
               column_width,
               MozColumnWidth,
               "")
CSS_PROP_ALIAS(-moz-columns,
               _moz_columns,
               columns,
               MozColumns,
               "")

#define WEBKIT_PREFIX_PREF "layout.css.prefixes.webkit"

// -webkit- prefixes
CSS_PROP_ALIAS(-webkit-animation,
               _webkit_animation,
               animation,
               WebkitAnimation,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-delay,
               _webkit_animation_delay,
               animation_delay,
               WebkitAnimationDelay,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-direction,
               _webkit_animation_direction,
               animation_direction,
               WebkitAnimationDirection,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-duration,
               _webkit_animation_duration,
               animation_duration,
               WebkitAnimationDuration,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-fill-mode,
               _webkit_animation_fill_mode,
               animation_fill_mode,
               WebkitAnimationFillMode,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-iteration-count,
               _webkit_animation_iteration_count,
               animation_iteration_count,
               WebkitAnimationIterationCount,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-name,
               _webkit_animation_name,
               animation_name,
               WebkitAnimationName,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-play-state,
               _webkit_animation_play_state,
               animation_play_state,
               WebkitAnimationPlayState,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-timing-function,
               _webkit_animation_timing_function,
               animation_timing_function,
               WebkitAnimationTimingFunction,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-filter,
               _webkit_filter,
               filter,
               WebkitFilter,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-text-size-adjust,
               _webkit_text_size_adjust,
               _moz_text_size_adjust,
               WebkitTextSizeAdjust,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-transform,
               _webkit_transform,
               transform,
               WebkitTransform,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transform-origin,
               _webkit_transform_origin,
               transform_origin,
               WebkitTransformOrigin,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transform-style,
               _webkit_transform_style,
               transform_style,
               WebkitTransformStyle,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-backface-visibility,
               _webkit_backface_visibility,
               backface_visibility,
               WebkitBackfaceVisibility,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-perspective,
               _webkit_perspective,
               perspective,
               WebkitPerspective,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-perspective-origin,
               _webkit_perspective_origin,
               perspective_origin,
               WebkitPerspectiveOrigin,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-transition,
               _webkit_transition,
               transition,
               WebkitTransition,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-delay,
               _webkit_transition_delay,
               transition_delay,
               WebkitTransitionDelay,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-duration,
               _webkit_transition_duration,
               transition_duration,
               WebkitTransitionDuration,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-property,
               _webkit_transition_property,
               transition_property,
               WebkitTransitionProperty,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-timing-function,
               _webkit_transition_timing_function,
               transition_timing_function,
               WebkitTransitionTimingFunction,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-border-radius,
               _webkit_border_radius,
               border_radius,
               WebkitBorderRadius,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-top-left-radius,
               _webkit_border_top_left_radius,
               border_top_left_radius,
               WebkitBorderTopLeftRadius, // really no dom property
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-top-right-radius,
               _webkit_border_top_right_radius,
               border_top_right_radius,
               WebkitBorderTopRightRadius, // really no dom property
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-bottom-left-radius,
               _webkit_border_bottom_left_radius,
               border_bottom_left_radius,
               WebkitBorderBottomLeftRadius, // really no dom property
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-bottom-right-radius,
               _webkit_border_bottom_right_radius,
               border_bottom_right_radius,
               WebkitBorderBottomRightRadius, // really no dom property
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-background-clip,
               _webkit_background_clip,
               background_clip,
               WebkitBackgroundClip,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-background-origin,
               _webkit_background_origin,
               background_origin,
               WebkitBackgroundOrigin,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-background-size,
               _webkit_background_size,
               background_size,
               WebkitBackgroundSize,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-border-image,
               _webkit_border_image,
               border_image,
               WebkitBorderImage,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-box-shadow,
               _webkit_box_shadow,
               box_shadow,
               WebkitBoxShadow,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-sizing,
               _webkit_box_sizing,
               box_sizing,
               WebkitBoxSizing,
               WEBKIT_PREFIX_PREF)

// Alias -webkit-box properties to their -moz-box equivalents.
// (NOTE: Even though they're aliases, in practice these -webkit properties
// will behave a bit differently from their -moz versions, if they're
// accompanied by "display:-webkit-box", because we generate a different frame
// for those two display values.)
CSS_PROP_ALIAS(-webkit-box-flex,
               _webkit_box_flex,
               _moz_box_flex,
               WebkitBoxFlex,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-ordinal-group,
               _webkit_box_ordinal_group,
               _moz_box_ordinal_group,
               WebkitBoxOrdinalGroup,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-orient,
               _webkit_box_orient,
               _moz_box_orient,
               WebkitBoxOrient,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-direction,
               _webkit_box_direction,
               _moz_box_direction,
               WebkitBoxDirection,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-align,
               _webkit_box_align,
               _moz_box_align,
               WebkitBoxAlign,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-pack,
               _webkit_box_pack,
               _moz_box_pack,
               WebkitBoxPack,
               WEBKIT_PREFIX_PREF)

// Alias -webkit-flex related properties to their unprefixed equivalents:
// (Matching ordering at https://drafts.csswg.org/css-flexbox-1/#property-index )
CSS_PROP_ALIAS(-webkit-flex-direction,
               _webkit_flex_direction,
               flex_direction,
               WebkitFlexDirection,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-wrap,
               _webkit_flex_wrap,
               flex_wrap,
               WebkitFlexWrap,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-flow,
               _webkit_flex_flow,
               flex_flow,
               WebkitFlexFlow,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-order,
               _webkit_order,
               order,
               WebkitOrder,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex,
               _webkit_flex,
               flex,
               WebkitFlex,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-grow,
               _webkit_flex_grow,
               flex_grow,
               WebkitFlexGrow,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-shrink,
               _webkit_flex_shrink,
               flex_shrink,
               WebkitFlexShrink,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-basis,
               _webkit_flex_basis,
               flex_basis,
               WebkitFlexBasis,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-justify-content,
               _webkit_justify_content,
               justify_content,
               WebkitJustifyContent,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-align-items,
               _webkit_align_items,
               align_items,
               WebkitAlignItems,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-align-self,
               _webkit_align_self,
               align_self,
               WebkitAlignSelf,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-align-content,
               _webkit_align_content,
               align_content,
               WebkitAlignContent,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-user-select,
               _webkit_user_select,
               _moz_user_select,
               WebkitUserSelect,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask,
               _webkit_mask,
               mask,
               WebkitMask,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-clip,
               _webkit_mask_clip,
               mask_clip,
               WebkitMaskClip,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-composite,
               _webkit_mask_composite,
               mask_composite,
               WebkitMaskComposite,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-image,
               _webkit_mask_image,
               mask_image,
               WebkitMaskImage,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-origin,
               _webkit_mask_origin,
               mask_origin,
               WebkitMaskOrigin,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-position,
               _webkit_mask_position,
               mask_position,
               WebkitMaskPosition,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-position-x,
               _webkit_mask_position_x,
               mask_position_x,
               WebkitMaskPositionX,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-position-y,
               _webkit_mask_position_y,
               mask_position_y,
               WebkitMaskPositionY,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-repeat,
               _webkit_mask_repeat,
               mask_repeat,
               WebkitMaskRepeat,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-size,
               _webkit_mask_size,
               mask_size,
               WebkitMaskSize,
               WEBKIT_PREFIX_PREF)
#undef WEBKIT_PREFIX_PREF
