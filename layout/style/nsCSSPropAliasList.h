/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
               overflow_wrap,
               WordWrap,
               "")
CSS_PROP_ALIAS(-moz-transform-origin,
               transform_origin,
               MozTransformOrigin,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-perspective-origin,
               perspective_origin,
               MozPerspectiveOrigin,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-perspective,
               perspective,
               MozPerspective,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-transform-style,
               transform_style,
               MozTransformStyle,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-backface-visibility,
               backface_visibility,
               MozBackfaceVisibility,
               "layout.css.prefixes.transforms")
CSS_PROP_ALIAS(-moz-border-image,
               border_image,
               MozBorderImage,
               "layout.css.prefixes.border-image")
CSS_PROP_ALIAS(-moz-transition,
               transition,
               MozTransition,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-delay,
               transition_delay,
               MozTransitionDelay,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-duration,
               transition_duration,
               MozTransitionDuration,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-property,
               transition_property,
               MozTransitionProperty,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-transition-timing-function,
               transition_timing_function,
               MozTransitionTimingFunction,
               "layout.css.prefixes.transitions")
CSS_PROP_ALIAS(-moz-animation,
               animation,
               MozAnimation,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-delay,
               animation_delay,
               MozAnimationDelay,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-direction,
               animation_direction,
               MozAnimationDirection,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-duration,
               animation_duration,
               MozAnimationDuration,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-fill-mode,
               animation_fill_mode,
               MozAnimationFillMode,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-iteration-count,
               animation_iteration_count,
               MozAnimationIterationCount,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-name,
               animation_name,
               MozAnimationName,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-play-state,
               animation_play_state,
               MozAnimationPlayState,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-animation-timing-function,
               animation_timing_function,
               MozAnimationTimingFunction,
               "layout.css.prefixes.animations")
CSS_PROP_ALIAS(-moz-box-sizing,
               box_sizing,
               MozBoxSizing,
               "layout.css.prefixes.box-sizing")
CSS_PROP_ALIAS(-moz-font-feature-settings,
               font_feature_settings,
               MozFontFeatureSettings,
               "layout.css.prefixes.font-features")
CSS_PROP_ALIAS(-moz-font-language-override,
               font_language_override,
               MozFontLanguageOverride,
               "layout.css.prefixes.font-features")
CSS_PROP_ALIAS(-moz-padding-end,
               padding_inline_end,
               MozPaddingEnd,
               "")
CSS_PROP_ALIAS(-moz-padding-start,
               padding_inline_start,
               MozPaddingStart,
               "")
CSS_PROP_ALIAS(-moz-margin-end,
               margin_inline_end,
               MozMarginEnd,
               "")
CSS_PROP_ALIAS(-moz-margin-start,
               margin_inline_start,
               MozMarginStart,
               "")
CSS_PROP_ALIAS(-moz-border-end,
               border_inline_end,
               MozBorderEnd,
               "")
CSS_PROP_ALIAS(-moz-border-end-color,
               border_inline_end_color,
               MozBorderEndColor,
               "")
CSS_PROP_ALIAS(-moz-border-end-style,
               border_inline_end_style,
               MozBorderEndStyle,
               "")
CSS_PROP_ALIAS(-moz-border-end-width,
               border_inline_end_width,
               MozBorderEndWidth,
               "")
CSS_PROP_ALIAS(-moz-border-start,
               border_inline_start,
               MozBorderStart,
               "")
CSS_PROP_ALIAS(-moz-border-start-color,
               border_inline_start_color,
               MozBorderStartColor,
               "")
CSS_PROP_ALIAS(-moz-border-start-style,
               border_inline_start_style,
               MozBorderStartStyle,
               "")
CSS_PROP_ALIAS(-moz-border-start-width,
               border_inline_start_width,
               MozBorderStartWidth,
               "")
CSS_PROP_ALIAS(-moz-hyphens,
               hyphens,
               MozHyphens,
               "")
CSS_PROP_ALIAS(-moz-text-align-last,
               text_align_last,
               MozTextAlignLast,
               "")

#define WEBKIT_PREFIX_PREF "layout.css.prefixes.webkit"

// -webkit- prefixes
CSS_PROP_ALIAS(-webkit-animation,
               animation,
               WebkitAnimation,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-delay,
               animation_delay,
               WebkitAnimationDelay,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-direction,
               animation_direction,
               WebkitAnimationDirection,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-duration,
               animation_duration,
               WebkitAnimationDuration,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-fill-mode,
               animation_fill_mode,
               WebkitAnimationFillMode,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-iteration-count,
               animation_iteration_count,
               WebkitAnimationIterationCount,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-name,
               animation_name,
               WebkitAnimationName,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-play-state,
               animation_play_state,
               WebkitAnimationPlayState,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-animation-timing-function,
               animation_timing_function,
               WebkitAnimationTimingFunction,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-filter,
               filter,
               WebkitFilter,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-text-size-adjust,
               text_size_adjust,
               WebkitTextSizeAdjust,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-transform,
               transform,
               WebkitTransform,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transform-origin,
               transform_origin,
               WebkitTransformOrigin,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transform-style,
               transform_style,
               WebkitTransformStyle,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-backface-visibility,
               backface_visibility,
               WebkitBackfaceVisibility,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-perspective,
               perspective,
               WebkitPerspective,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-perspective-origin,
               perspective_origin,
               WebkitPerspectiveOrigin,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-transition,
               transition,
               WebkitTransition,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-delay,
               transition_delay,
               WebkitTransitionDelay,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-duration,
               transition_duration,
               WebkitTransitionDuration,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-property,
               transition_property,
               WebkitTransitionProperty,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-transition-timing-function,
               transition_timing_function,
               WebkitTransitionTimingFunction,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-border-radius,
               border_radius,
               WebkitBorderRadius,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-top-left-radius,
               border_top_left_radius,
               WebkitBorderTopLeftRadius, // really no dom property
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-top-right-radius,
               border_top_right_radius,
               WebkitBorderTopRightRadius, // really no dom property
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-bottom-left-radius,
               border_bottom_left_radius,
               WebkitBorderBottomLeftRadius, // really no dom property
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-border-bottom-right-radius,
               border_bottom_right_radius,
               WebkitBorderBottomRightRadius, // really no dom property
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-background-clip,
               background_clip,
               WebkitBackgroundClip,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-background-origin,
               background_origin,
               WebkitBackgroundOrigin,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-background-size,
               background_size,
               WebkitBackgroundSize,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-border-image,
               border_image,
               WebkitBorderImage,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-box-shadow,
               box_shadow,
               WebkitBoxShadow,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-sizing,
               box_sizing,
               WebkitBoxSizing,
               WEBKIT_PREFIX_PREF)

// Alias -webkit-box properties to their -moz-box equivalents.
// (NOTE: Even though they're aliases, in practice these -webkit properties
// will behave a bit differently from their -moz versions, if they're
// accompanied by "display:-webkit-box", because we generate a different frame
// for those two display values.)
CSS_PROP_ALIAS(-webkit-box-flex,
               box_flex,
               WebkitBoxFlex,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-ordinal-group,
               box_ordinal_group,
               WebkitBoxOrdinalGroup,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-orient,
               box_orient,
               WebkitBoxOrient,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-direction,
               box_direction,
               WebkitBoxDirection,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-align,
               box_align,
               WebkitBoxAlign,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-box-pack,
               box_pack,
               WebkitBoxPack,
               WEBKIT_PREFIX_PREF)

// Alias -webkit-flex related properties to their unprefixed equivalents:
// (Matching ordering at https://drafts.csswg.org/css-flexbox-1/#property-index )
CSS_PROP_ALIAS(-webkit-flex-direction,
               flex_direction,
               WebkitFlexDirection,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-wrap,
               flex_wrap,
               WebkitFlexWrap,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-flow,
               flex_flow,
               WebkitFlexFlow,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-order,
               order,
               WebkitOrder,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex,
               flex,
               WebkitFlex,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-grow,
               flex_grow,
               WebkitFlexGrow,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-shrink,
               flex_shrink,
               WebkitFlexShrink,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-flex-basis,
               flex_basis,
               WebkitFlexBasis,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-justify-content,
               justify_content,
               WebkitJustifyContent,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-align-items,
               align_items,
               WebkitAlignItems,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-align-self,
               align_self,
               WebkitAlignSelf,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-align-content,
               align_content,
               WebkitAlignContent,
               WEBKIT_PREFIX_PREF)

CSS_PROP_ALIAS(-webkit-user-select,
               user_select,
               WebkitUserSelect,
               WEBKIT_PREFIX_PREF)

#ifdef MOZ_ENABLE_MASK_AS_SHORTHAND
CSS_PROP_ALIAS(-webkit-mask,
               mask,
               WebkitMask,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-clip,
               mask_clip,
               WebkitMaskClip,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-composite,
               mask_composite,
               WebkitMaskComposite,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-image,
               mask_image,
               WebkitMaskImage,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-origin,
               mask_origin,
               WebkitMaskOrigin,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-position,
               mask_position,
               WebkitMaskPosition,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-position-x,
               mask_position_x,
               WebkitMaskPositionX,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-position-y,
               mask_position_y,
               WebkitMaskPositionY,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-repeat,
               mask_repeat,
               WebkitMaskRepeat,
               WEBKIT_PREFIX_PREF)
CSS_PROP_ALIAS(-webkit-mask-size,
               mask_size,
               WebkitMaskSize,
               WEBKIT_PREFIX_PREF)
#endif
#undef WEBKIT_PREFIX_PREF
