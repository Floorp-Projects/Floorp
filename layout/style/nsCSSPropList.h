/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/******

  This file contains the list of all parsed CSS properties
  See nsCSSProps.h for access to the enum values for properties

  It is designed to be used as inline input through the magic of
  C preprocessing.

  All entries must be enclosed in the macro CSS_PROP which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  Requirements:

  Entries are in the form: (name, id, method, hint).

  'name' entries represent a CSS property name and *must* use only
  lowercase characters.

  'id' must always be the same as 'name' except that all hyphens ('-')
  in 'name' are converted to underscores ('_') in 'id'. This lets us
  do nice things with the macros without having to copy/convert strings
  at runtime.

  'method' is designed to be as input for CSS2Properties and similar callers.
  It must always be the same as 'name' except it must use InterCaps and all
  hyphens ('-') must be removed.

  'hint' says what needs to be recomputed when the property changes.

  ** Break these invariants and bad things will happen. **

 ******/

// Assume the user wants internal properties, but not unimplemented ones
#ifndef CSS_PROP_INTERNAL
#define CSS_PROP_INTERNAL(_name, _id, _method, _hint) \
  CSS_PROP(_name, _id, _method, _hint)
#define DEFINED_CSS_PROP_INTERNAL
#endif

#ifndef CSS_PROP_NOTIMPLEMENTED
#define CSS_PROP_NOTIMPLEMENTED(_name, _id, _method, _hint) /* nothing */
#define DEFINED_CSS_PROP_NOTIMPLEMENTED
#endif

// For notes XXX bug 3935 below, the names being parsed do not correspond
// to the constants used internally.  It would be nice to bring the
// constants into line sometime.

// The parser will refuse to parse properties marked with -x-.

// Those marked XXX bug 48973 are CSS2 properties that we support
// differently from the spec for UI requirements.  If we ever
// support them correctly the old constants need to be renamed and
// new ones should be entered.

CSS_PROP(-moz-appearance, appearance, MozAppearance, REFLOW)
CSS_PROP(-moz-border-radius, _moz_border_radius, MozBorderRadius, VISUAL)
CSS_PROP(-moz-border-radius-topleft, _moz_border_radius_topLeft, MozBorderRadiusTopleft, VISUAL)
CSS_PROP(-moz-border-radius-topright, _moz_border_radius_topRight, MozBorderRadiusTopright, VISUAL)
CSS_PROP(-moz-border-radius-bottomleft, _moz_border_radius_bottomLeft, MozBorderRadiusBottomleft, VISUAL)
CSS_PROP(-moz-border-radius-bottomright, _moz_border_radius_bottomRight, MozBorderRadiusBottomright, VISUAL)
CSS_PROP(-moz-outline-radius, _moz_outline_radius, MozOutlineRadius, VISUAL)
CSS_PROP(-moz-outline-radius-topleft, _moz_outline_radius_topLeft, MozOutlineRadiusTopleft, VISUAL)
CSS_PROP(-moz-outline-radius-topright, _moz_outline_radius_topRight, MozOutlineRadiusTopright, VISUAL)
CSS_PROP(-moz-outline-radius-bottomleft, _moz_outline_radius_bottomLeft, MozOutlineRadiusBottomleft, VISUAL)
CSS_PROP(-moz-outline-radius-bottomright, _moz_outline_radius_bottomRight, MozOutlineRadiusBottomright, VISUAL)
CSS_PROP(azimuth, azimuth, Azimuth, AURAL)
CSS_PROP(background, background, Background, VISUAL)
CSS_PROP(background-attachment, background_attachment, BackgroundAttachment, FRAMECHANGE)
CSS_PROP(background-color, background_color, BackgroundColor, VISUAL)
CSS_PROP(background-image, background_image, BackgroundImage, VISUAL)
CSS_PROP(background-position, background_position, BackgroundPosition, VISUAL)
CSS_PROP(background-repeat, background_repeat, BackgroundRepeat, VISUAL)
CSS_PROP_INTERNAL(-x-background-x-position, background_x_position, BackgroundXPosition, VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-background-y-position, background_y_position, BackgroundYPosition, VISUAL) // XXX bug 3935
CSS_PROP(-moz-binding, binding, MozBinding, FRAMECHANGE) // XXX bug 3935
CSS_PROP(border, border, Border, REFLOW)
CSS_PROP(border-bottom, border_bottom, BorderBottom, REFLOW)
CSS_PROP(border-bottom-color, border_bottom_color, BorderBottomColor, VISUAL)
CSS_PROP(-moz-border-bottom-colors, border_bottom_colors, MozBorderBottomColors, VISUAL)
CSS_PROP(border-bottom-style, border_bottom_style, BorderBottomStyle, REFLOW)  // on/off will need reflow
CSS_PROP(border-bottom-width, border_bottom_width, BorderBottomWidth, REFLOW)
CSS_PROP(border-collapse, border_collapse, BorderCollapse, FRAMECHANGE)
CSS_PROP(border-color, border_color, BorderColor, VISUAL)
CSS_PROP(border-left, border_left, BorderLeft, REFLOW)
CSS_PROP(border-left-color, border_left_color, BorderLeftColor, VISUAL)
CSS_PROP(-moz-border-left-colors, border_left_colors, MozBorderLeftColors, VISUAL)
CSS_PROP(border-left-style, border_left_style, BorderLeftStyle, REFLOW)  // on/off will need reflow
CSS_PROP(border-left-width, border_left_width, BorderLeftWidth, REFLOW)
CSS_PROP(border-right, border_right, BorderRight, REFLOW)
CSS_PROP(border-right-color, border_right_color, BorderRightColor, VISUAL)
CSS_PROP(-moz-border-right-colors, border_right_colors, MozBorderRightColors, VISUAL)
CSS_PROP(border-right-style, border_right_style, BorderRightStyle, REFLOW)  // on/off will need reflow
CSS_PROP(border-right-width, border_right_width, BorderRightWidth, REFLOW)
CSS_PROP(border-spacing, border_spacing, BorderSpacing, REFLOW)
CSS_PROP(border-style, border_style, BorderStyle, REFLOW)  // on/off will need reflow
CSS_PROP(border-top, border_top, BorderTop, REFLOW)
CSS_PROP(border-top-color, border_top_color, BorderTopColor, VISUAL)
CSS_PROP(-moz-border-top-colors, border_top_colors, MozBorderTopColors, VISUAL)
CSS_PROP(border-top-style, border_top_style, BorderTopStyle, REFLOW)  // on/off will need reflow
CSS_PROP(border-top-width, border_top_width, BorderTopWidth, REFLOW)
CSS_PROP(border-width, border_width, BorderWidth, REFLOW)
CSS_PROP_INTERNAL(-x-border-x-spacing, border_x_spacing, BorderXSpacing, REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-border-y-spacing, border_y_spacing, BorderYSpacing, REFLOW) // XXX bug 3935
CSS_PROP(bottom, bottom, Bottom, REFLOW)
CSS_PROP(-moz-box-align, box_align, MozBoxAlign, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-direction, box_direction, MozBoxDirection, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-flex, box_flex, MozBoxFlex, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-flex-group, box_flex_group, MozBoxFlexGroup, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-orient, box_orient, MozBoxOrient, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-pack, box_pack, MozBoxPack, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-sizing, box_sizing, MozBoxSizing, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-ordinal-group, box_ordinal_group, MozBoxOrdinalGroup, FRAMECHANGE)
CSS_PROP(caption-side, caption_side, CaptionSide, REFLOW)
CSS_PROP(clear, clear, Clear, REFLOW)
CSS_PROP(clip, clip, Clip, REFLOW)
CSS_PROP_INTERNAL(-x-clip-bottom, clip_bottom, ClipBottom, REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-clip-left, clip_left, ClipLeft, REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-clip-right, clip_right, ClipRight, REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-clip-top, clip_top, ClipTop, REFLOW) // XXX bug 3935
CSS_PROP(color, color, Color, VISUAL)
CSS_PROP(content, content, Content, FRAMECHANGE)
CSS_PROP(counter-increment, counter_increment, CounterIncrement, REFLOW)
CSS_PROP(counter-reset, counter_reset, CounterReset, REFLOW)
CSS_PROP(-moz-counter-increment, _moz_counter_increment, MozCounterIncrement, REFLOW) // XXX bug 137285
CSS_PROP(-moz-counter-reset, _moz_counter_reset, MozCounterReset, REFLOW) // XXX bug 137285
CSS_PROP(cue, cue, Cue, AURAL)
CSS_PROP(cue-after, cue_after, CueAfter, AURAL)
CSS_PROP(cue-before, cue_before, CueBefore, AURAL)
CSS_PROP(cursor, cursor, Cursor, VISUAL)
CSS_PROP(direction, direction, Direction, REFLOW)
CSS_PROP(display, display, Display, FRAMECHANGE)
CSS_PROP(elevation, elevation, Elevation, AURAL)
CSS_PROP(empty-cells, empty_cells, EmptyCells, VISUAL)
CSS_PROP(float, float, CssFloat, FRAMECHANGE)
CSS_PROP(-moz-float-edge, float_edge, MozFloatEdge, REFLOW) // XXX bug 3935
CSS_PROP(font, font, Font, REFLOW)
CSS_PROP(font-family, font_family, FontFamily, REFLOW)
CSS_PROP(font-size, font_size, FontSize, REFLOW)
CSS_PROP(font-size-adjust, font_size_adjust, FontSizeAdjust, REFLOW)
CSS_PROP(font-stretch, font_stretch, FontStretch, REFLOW)
CSS_PROP(font-style, font_style, FontStyle, REFLOW)
CSS_PROP(font-variant, font_variant, FontVariant, REFLOW)
CSS_PROP(font-weight, font_weight, FontWeight, REFLOW)
CSS_PROP(-moz-force-broken-image-icon, force_broken_image_icon, MozForceBrokenImageIcon, FRAMECHANGE) // bug 58646
CSS_PROP(height, height, Height, REFLOW)
CSS_PROP(-moz-image-region, image_region, MozImageRegion, REFLOW)
CSS_PROP_INTERNAL(-x-image-region-bottom, image_region_bottom, MozImageRegionBottom, REFLOW)
CSS_PROP_INTERNAL(-x-image-region-left, image_region_left, MozImageRegionLeft, REFLOW)
CSS_PROP_INTERNAL(-x-image-region-right, image_region_right, MozImageRegionRight, REFLOW)
CSS_PROP_INTERNAL(-x-image-region-top, image_region_top, MozImageRegionTop, REFLOW)
CSS_PROP(-moz-key-equivalent, key_equivalent, MozKeyEquivalent, CONTENT) // This will need some other notification, but what? // XXX bug 3935
CSS_PROP(left, left, Left, REFLOW)
CSS_PROP(letter-spacing, letter_spacing, LetterSpacing, REFLOW)
CSS_PROP(line-height, line_height, LineHeight, REFLOW)
CSS_PROP(list-style, list_style, ListStyle, REFLOW)
CSS_PROP(list-style-image, list_style_image, ListStyleImage, REFLOW)
CSS_PROP(list-style-position, list_style_position, ListStylePosition, REFLOW)
CSS_PROP(list-style-type, list_style_type, ListStyleType, REFLOW)
CSS_PROP(margin, margin, Margin, REFLOW)
CSS_PROP(margin-bottom, margin_bottom, MarginBottom, REFLOW)
CSS_PROP(margin-left, margin_left, MarginLeft, REFLOW)
CSS_PROP(margin-right, margin_right, MarginRight, REFLOW)
CSS_PROP(margin-top, margin_top, MarginTop, REFLOW)
CSS_PROP(marker-offset, marker_offset, MarkerOffset, REFLOW)
CSS_PROP(marks, marks, Marks, VISUAL)
CSS_PROP(max-height, max_height, MaxHeight, REFLOW)
CSS_PROP(max-width, max_width, MaxWidth, REFLOW)
CSS_PROP(min-height, min_height, MinHeight, REFLOW)
CSS_PROP(min-width, min_width, MinWidth, REFLOW)
CSS_PROP(-moz-opacity, opacity, MozOpacity, FRAMECHANGE) // XXX bug 3935
CSS_PROP(orphans, orphans, Orphans, REFLOW)
CSS_PROP(outline, outline, Outline, VISUAL)
CSS_PROP(outline, outline_color, OutlineColor, VISUAL)
CSS_PROP(outline, outline_style, OutlineStyle, VISUAL)
CSS_PROP(outline, outline_width, OutlineWidth, VISUAL)
CSS_PROP(-moz-outline, _moz_outline, MozOutline, VISUAL)  // XXX This is temporary fix for nsbeta3+ Bug 48973, turning outline into -moz-outline  XXX bug 48973
CSS_PROP(-moz-outline-color, _moz_outline_color, MozOutlineColor, VISUAL) // XXX bug 48973
CSS_PROP(-moz-outline-style, _moz_outline_style, MozOutlineStyle, VISUAL) // XXX bug 48973
CSS_PROP(-moz-outline-width, _moz_outline_width, MozOutlineWidth, VISUAL) // XXX bug 48973
CSS_PROP(overflow, overflow, Overflow, FRAMECHANGE)
CSS_PROP(padding, padding, Padding, REFLOW)
CSS_PROP(padding-bottom, padding_bottom, PaddingBottom, REFLOW)
CSS_PROP(padding-left, padding_left, PaddingLeft, REFLOW)
CSS_PROP(padding-right, padding_right, PaddingRight, REFLOW)
CSS_PROP(padding-top, padding_top, PaddingTop, REFLOW)
CSS_PROP(page, page, Page, REFLOW)
CSS_PROP(page-break-after, page_break_after, PageBreakAfter, REFLOW)
CSS_PROP(page-break-before, page_break_before, PageBreakBefore, REFLOW)
CSS_PROP(page-break-inside, page_break_inside, PageBreakInside, REFLOW)
CSS_PROP(pause, pause, Pause, AURAL)
CSS_PROP(pause-after, pause_after, PauseAfter, AURAL)
CSS_PROP(pause-before, pause_before, PauseBefore, AURAL)
CSS_PROP(pitch, pitch, Pitch, AURAL)
CSS_PROP(pitch-range, pitch_range, PitchRange, AURAL)
CSS_PROP(play-during, play_during, PlayDuring, AURAL)
CSS_PROP_INTERNAL(-x-play-during-flags, play_during_flags, PlayDuringFlags, AURAL) // XXX why is this here?
CSS_PROP(position, position, Position, FRAMECHANGE)
CSS_PROP(quotes, quotes, Quotes, REFLOW)
CSS_PROP_INTERNAL(-x-quotes-close, quotes_close, QuotesClose, REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-quotes-open, quotes_open, QuotesOpen, REFLOW) // XXX bug 3935
CSS_PROP(-moz-resizer, resizer, MozResizer, FRAMECHANGE) // XXX bug 3935
CSS_PROP(richness, richness, Richness, AURAL)
CSS_PROP(right, right, Right, REFLOW)
CSS_PROP(size, size, Size, REFLOW)
CSS_PROP_INTERNAL(-x-size-height, size_height, SizeHeight, REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-size-width, size_width, SizeWidth, REFLOW) // XXX bug 3935
CSS_PROP(speak, speak, Speak, AURAL)
CSS_PROP(speak-header, speak_header, SpeakHeader, AURAL)
CSS_PROP(speak-numeral, speak_numeral, SpeakNumeral, AURAL)
CSS_PROP(speak-punctuation, speak_punctuation, SpeakPunctuation, AURAL)
CSS_PROP(speech-rate, speech_rate, SpeechRate, AURAL)
CSS_PROP(stress, stress, Stress, AURAL)
CSS_PROP(table-layout, table_layout, TableLayout, REFLOW)
CSS_PROP(text-align, text_align, TextAlign, REFLOW)
CSS_PROP(text-decoration, text_decoration, TextDecoration, VISUAL)
CSS_PROP(text-indent, text_indent, TextIndent, REFLOW)
CSS_PROP(text-shadow, text_shadow, TextShadow, VISUAL)
CSS_PROP_INTERNAL(-x-text-shadow-color, text_shadow_color, TextShadowColor, VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-text-shadow-radius, text_shadow_radius, TextShadowRadius, VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-text-shadow-x, text_shadow_x, TextShadowX, VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-text-shadow-y, text_shadow_y, TextShadowY, VISUAL) // XXX bug 3935
CSS_PROP(text-transform, text_transform, TextTransform, REFLOW)
CSS_PROP(top, top, Top, REFLOW)
CSS_PROP(unicode-bidi, unicode_bidi, UnicodeBidi, REFLOW)
CSS_PROP(-moz-user-focus, user_focus, MozUserFocus, CONTENT) // XXX bug 3935
CSS_PROP(-moz-user-input, user_input, MozUserInput, FRAMECHANGE) // XXX ??? // XXX bug 3935
CSS_PROP(-moz-user-modify, user_modify, MozUserModify, FRAMECHANGE) // XXX bug 3935
CSS_PROP(-moz-user-select, user_select, MozUserSelect, CONTENT) // XXX bug 3935
CSS_PROP(vertical-align, vertical_align, VerticalAlign, REFLOW)
CSS_PROP(visibility, visibility, Visibility, REFLOW)  // reflow for collapse
CSS_PROP(voice-family, voice_family, VoiceFamily, AURAL)
CSS_PROP(volume, volume, Volume, AURAL)
CSS_PROP(white-space, white_space, WhiteSpace, REFLOW)
CSS_PROP(widows, widows, Widows, REFLOW)
CSS_PROP(width, width, Width, REFLOW)
CSS_PROP(word-spacing, word_spacing, WordSpacing, REFLOW)
CSS_PROP(z-index, z_index, ZIndex, REFLOW)
#ifdef MOZ_SVG
// XXX treat SVG's CSS Properties as internal for now.
// Do we want to create an nsIDOMSVGCSS2Properties interface?
CSS_PROP_INTERNAL(fill, fill, Fill, VISUAL)
CSS_PROP_INTERNAL(fill-opacity, fill_opacity, FillOpacity, VISUAL)
CSS_PROP_INTERNAL(fill-rule, fill_rule, FillRule, VISUAL)
CSS_PROP_INTERNAL(stroke, stroke, Stroke, VISUAL)
CSS_PROP_INTERNAL(stroke-dasharray, stroke_dasharray, StrokeDasharray, VISUAL)
CSS_PROP_INTERNAL(stroke-dashoffset, stroke_dashoffset, StrokeDashoffset, VISUAL)
CSS_PROP_INTERNAL(stroke-linecap, stroke_linecap, StrokeLinecap, VISUAL)
CSS_PROP_INTERNAL(stroke-linejoin, stroke_linejoin, StrokeLinejoin, VISUAL)
CSS_PROP_INTERNAL(stroke-miterlimit, stroke_miterlimit, StrokeMiterlimit, VISUAL)
CSS_PROP_INTERNAL(stroke-opacity, stroke_opacity, StrokeOpacity, VISUAL)
CSS_PROP_INTERNAL(stroke-width, stroke_width, StrokeWidth, VISUAL)
#endif

// Clean up after ourselves
#ifdef DEFINED_CSS_PROP_INTERNAL
#undef CSS_PROP_INTERNAL
#undef DEFINED_CSS_PROP_INTERNAL
#endif

#ifdef DEFINED_CSS_PROP_NOTIMPLEMENTED
#undef CSS_PROP_NOTIMPLEMENTED
#undef DEFINED_CSS_PROP_NOTIMPLEMENTED
#endif
