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

CSS_PROP(-moz-appearance, appearance, MozAppearance, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-border-radius, _moz_border_radius, MozBorderRadius, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-radius-topleft, _moz_border_radius_topLeft, MozBorderRadiusTopleft, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-radius-topright, _moz_border_radius_topRight, MozBorderRadiusTopright, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-radius-bottomleft, _moz_border_radius_bottomLeft, MozBorderRadiusBottomleft, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-radius-bottomright, _moz_border_radius_bottomRight, MozBorderRadiusBottomright, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-outline-radius, _moz_outline_radius, MozOutlineRadius, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-outline-radius-topleft, _moz_outline_radius_topLeft, MozOutlineRadiusTopleft, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-outline-radius-topright, _moz_outline_radius_topRight, MozOutlineRadiusTopright, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-outline-radius-bottomleft, _moz_outline_radius_bottomLeft, MozOutlineRadiusBottomleft, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-outline-radius-bottomright, _moz_outline_radius_bottomRight, MozOutlineRadiusBottomright, NS_STYLE_HINT_VISUAL)
CSS_PROP(azimuth, azimuth, Azimuth, NS_STYLE_HINT_AURAL)
CSS_PROP(background, background, Background, NS_STYLE_HINT_VISUAL)
CSS_PROP(background-attachment, background_attachment, BackgroundAttachment, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP(-moz-background-clip, _moz_background_clip, MozBackgroundClip, NS_STYLE_HINT_VISUAL)
CSS_PROP(background-color, background_color, BackgroundColor, NS_STYLE_HINT_VISUAL)
CSS_PROP(background-image, background_image, BackgroundImage, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-background-origin, _moz_background_origin, MozBackgroundOrigin, NS_STYLE_HINT_VISUAL)
CSS_PROP(background-position, background_position, BackgroundPosition, NS_STYLE_HINT_VISUAL)
CSS_PROP(background-repeat, background_repeat, BackgroundRepeat, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(-x-background-x-position, background_x_position, BackgroundXPosition, NS_STYLE_HINT_VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-background-y-position, background_y_position, BackgroundYPosition, NS_STYLE_HINT_VISUAL) // XXX bug 3935
CSS_PROP(-moz-binding, binding, MozBinding, NS_STYLE_HINT_FRAMECHANGE) // XXX bug 3935
CSS_PROP(border, border, Border, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-bottom, border_bottom, BorderBottom, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-bottom-color, border_bottom_color, BorderBottomColor, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-bottom-colors, border_bottom_colors, MozBorderBottomColors, NS_STYLE_HINT_VISUAL)
CSS_PROP(border-bottom-style, border_bottom_style, BorderBottomStyle, NS_STYLE_HINT_REFLOW)  // on/off will need reflow
CSS_PROP(border-bottom-width, border_bottom_width, BorderBottomWidth, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-collapse, border_collapse, BorderCollapse, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP(border-color, border_color, BorderColor, NS_STYLE_HINT_VISUAL)
CSS_PROP(border-left, border_left, BorderLeft, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-left-color, border_left_color, BorderLeftColor, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-left-colors, border_left_colors, MozBorderLeftColors, NS_STYLE_HINT_VISUAL)
CSS_PROP(border-left-style, border_left_style, BorderLeftStyle, NS_STYLE_HINT_REFLOW)  // on/off will need reflow
CSS_PROP(border-left-width, border_left_width, BorderLeftWidth, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-right, border_right, BorderRight, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-right-color, border_right_color, BorderRightColor, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-right-colors, border_right_colors, MozBorderRightColors, NS_STYLE_HINT_VISUAL)
CSS_PROP(border-right-style, border_right_style, BorderRightStyle, NS_STYLE_HINT_REFLOW)  // on/off will need reflow
CSS_PROP(border-right-width, border_right_width, BorderRightWidth, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-spacing, border_spacing, BorderSpacing, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-style, border_style, BorderStyle, NS_STYLE_HINT_REFLOW)  // on/off will need reflow
CSS_PROP(border-top, border_top, BorderTop, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-top-color, border_top_color, BorderTopColor, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-border-top-colors, border_top_colors, MozBorderTopColors, NS_STYLE_HINT_VISUAL)
CSS_PROP(border-top-style, border_top_style, BorderTopStyle, NS_STYLE_HINT_REFLOW)  // on/off will need reflow
CSS_PROP(border-top-width, border_top_width, BorderTopWidth, NS_STYLE_HINT_REFLOW)
CSS_PROP(border-width, border_width, BorderWidth, NS_STYLE_HINT_REFLOW)
CSS_PROP_INTERNAL(-x-border-x-spacing, border_x_spacing, BorderXSpacing, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-border-y-spacing, border_y_spacing, BorderYSpacing, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(bottom, bottom, Bottom, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-box-align, box_align, MozBoxAlign, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-direction, box_direction, MozBoxDirection, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-flex, box_flex, MozBoxFlex, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-flex-group, box_flex_group, MozBoxFlexGroup, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-orient, box_orient, MozBoxOrient, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-pack, box_pack, MozBoxPack, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-sizing, box_sizing, MozBoxSizing, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-ordinal-group, box_ordinal_group, MozBoxOrdinalGroup, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP(caption-side, caption_side, CaptionSide, NS_STYLE_HINT_REFLOW)
CSS_PROP(clear, clear, Clear, NS_STYLE_HINT_REFLOW)
CSS_PROP(clip, clip, Clip, nsChangeHint_SyncFrameView)
CSS_PROP_INTERNAL(-x-clip-bottom, clip_bottom, ClipBottom, nsChangeHint_SyncFrameView) // XXX bug 3935
CSS_PROP_INTERNAL(-x-clip-left, clip_left, ClipLeft, nsChangeHint_SyncFrameView) // XXX bug 3935
CSS_PROP_INTERNAL(-x-clip-right, clip_right, ClipRight, nsChangeHint_SyncFrameView) // XXX bug 3935
CSS_PROP_INTERNAL(-x-clip-top, clip_top, ClipTop, nsChangeHint_SyncFrameView) // XXX bug 3935
CSS_PROP(color, color, Color, NS_STYLE_HINT_VISUAL)
CSS_PROP(content, content, Content, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP_NOTIMPLEMENTED(counter-increment, counter_increment, CounterIncrement, NS_STYLE_HINT_REFLOW)
CSS_PROP_NOTIMPLEMENTED(counter-reset, counter_reset, CounterReset, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-counter-increment, _moz_counter_increment, MozCounterIncrement, NS_STYLE_HINT_REFLOW) // XXX bug 137285
CSS_PROP(-moz-counter-reset, _moz_counter_reset, MozCounterReset, NS_STYLE_HINT_REFLOW) // XXX bug 137285
CSS_PROP(cue, cue, Cue, NS_STYLE_HINT_AURAL)
CSS_PROP(cue-after, cue_after, CueAfter, NS_STYLE_HINT_AURAL)
CSS_PROP(cue-before, cue_before, CueBefore, NS_STYLE_HINT_AURAL)
CSS_PROP(cursor, cursor, Cursor, NS_STYLE_HINT_VISUAL)
CSS_PROP(direction, direction, Direction, NS_STYLE_HINT_REFLOW)
CSS_PROP(display, display, Display, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP(elevation, elevation, Elevation, NS_STYLE_HINT_AURAL)
CSS_PROP(empty-cells, empty_cells, EmptyCells, NS_STYLE_HINT_VISUAL)
CSS_PROP(float, float, CssFloat, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP(-moz-float-edge, float_edge, MozFloatEdge, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(font, font, Font, NS_STYLE_HINT_REFLOW)
CSS_PROP(font-family, font_family, FontFamily, NS_STYLE_HINT_REFLOW)
CSS_PROP(font-size, font_size, FontSize, NS_STYLE_HINT_REFLOW)
CSS_PROP(font-size-adjust, font_size_adjust, FontSizeAdjust, NS_STYLE_HINT_REFLOW)
CSS_PROP(font-stretch, font_stretch, FontStretch, NS_STYLE_HINT_REFLOW)
CSS_PROP(font-style, font_style, FontStyle, NS_STYLE_HINT_REFLOW)
CSS_PROP(font-variant, font_variant, FontVariant, NS_STYLE_HINT_REFLOW)
CSS_PROP(font-weight, font_weight, FontWeight, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-force-broken-image-icon, force_broken_image_icon, MozForceBrokenImageIcon, NS_STYLE_HINT_FRAMECHANGE) // bug 58646
CSS_PROP(height, height, Height, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-image-region, image_region, MozImageRegion, NS_STYLE_HINT_REFLOW)
CSS_PROP_INTERNAL(-x-image-region-bottom, image_region_bottom, MozImageRegionBottom, NS_STYLE_HINT_REFLOW)
CSS_PROP_INTERNAL(-x-image-region-left, image_region_left, MozImageRegionLeft, NS_STYLE_HINT_REFLOW)
CSS_PROP_INTERNAL(-x-image-region-right, image_region_right, MozImageRegionRight, NS_STYLE_HINT_REFLOW)
CSS_PROP_INTERNAL(-x-image-region-top, image_region_top, MozImageRegionTop, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-key-equivalent, key_equivalent, MozKeyEquivalent, NS_STYLE_HINT_CONTENT) // This will need some other notification, but what? // XXX bug 3935
CSS_PROP(left, left, Left, NS_STYLE_HINT_REFLOW)
CSS_PROP(letter-spacing, letter_spacing, LetterSpacing, NS_STYLE_HINT_REFLOW)
CSS_PROP(line-height, line_height, LineHeight, NS_STYLE_HINT_REFLOW)
CSS_PROP(list-style, list_style, ListStyle, NS_STYLE_HINT_REFLOW)
CSS_PROP(list-style-image, list_style_image, ListStyleImage, NS_STYLE_HINT_REFLOW)
CSS_PROP(list-style-position, list_style_position, ListStylePosition, NS_STYLE_HINT_REFLOW)
CSS_PROP(list-style-type, list_style_type, ListStyleType, NS_STYLE_HINT_REFLOW)
CSS_PROP(margin, margin, Margin, NS_STYLE_HINT_REFLOW)
CSS_PROP(margin-bottom, margin_bottom, MarginBottom, NS_STYLE_HINT_REFLOW)
CSS_PROP(margin-left, margin_left, MarginLeft, NS_STYLE_HINT_REFLOW)
CSS_PROP(margin-right, margin_right, MarginRight, NS_STYLE_HINT_REFLOW)
CSS_PROP(margin-top, margin_top, MarginTop, NS_STYLE_HINT_REFLOW)
CSS_PROP(marker-offset, marker_offset, MarkerOffset, NS_STYLE_HINT_REFLOW)
CSS_PROP(marks, marks, Marks, NS_STYLE_HINT_VISUAL)
CSS_PROP(max-height, max_height, MaxHeight, NS_STYLE_HINT_REFLOW)
CSS_PROP(max-width, max_width, MaxWidth, NS_STYLE_HINT_REFLOW)
CSS_PROP(min-height, min_height, MinHeight, NS_STYLE_HINT_REFLOW)
CSS_PROP(min-width, min_width, MinWidth, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-opacity, opacity, MozOpacity, NS_STYLE_HINT_FRAMECHANGE) // XXX bug 3935
CSS_PROP(orphans, orphans, Orphans, NS_STYLE_HINT_REFLOW)
CSS_PROP_NOTIMPLEMENTED(outline, outline, Outline, NS_STYLE_HINT_VISUAL)
CSS_PROP_NOTIMPLEMENTED(outline-color, outline_color, OutlineColor, NS_STYLE_HINT_VISUAL)
CSS_PROP_NOTIMPLEMENTED(outline-style, outline_style, OutlineStyle, NS_STYLE_HINT_VISUAL)
CSS_PROP_NOTIMPLEMENTED(outline-width, outline_width, OutlineWidth, NS_STYLE_HINT_VISUAL)
CSS_PROP(-moz-outline, _moz_outline, MozOutline, NS_STYLE_HINT_VISUAL)  // XXX This is temporary fix for nsbeta3+ Bug 48973, turning outline into -moz-outline  XXX bug 48973
CSS_PROP(-moz-outline-color, _moz_outline_color, MozOutlineColor, NS_STYLE_HINT_VISUAL) // XXX bug 48973
CSS_PROP(-moz-outline-style, _moz_outline_style, MozOutlineStyle, NS_STYLE_HINT_VISUAL) // XXX bug 48973
CSS_PROP(-moz-outline-width, _moz_outline_width, MozOutlineWidth, NS_STYLE_HINT_VISUAL) // XXX bug 48973
CSS_PROP(overflow, overflow, Overflow, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP(padding, padding, Padding, NS_STYLE_HINT_REFLOW)
CSS_PROP(padding-bottom, padding_bottom, PaddingBottom, NS_STYLE_HINT_REFLOW)
CSS_PROP(padding-left, padding_left, PaddingLeft, NS_STYLE_HINT_REFLOW)
CSS_PROP(padding-right, padding_right, PaddingRight, NS_STYLE_HINT_REFLOW)
CSS_PROP(padding-top, padding_top, PaddingTop, NS_STYLE_HINT_REFLOW)
CSS_PROP(page, page, Page, NS_STYLE_HINT_REFLOW)
CSS_PROP(page-break-after, page_break_after, PageBreakAfter, NS_STYLE_HINT_REFLOW)
CSS_PROP(page-break-before, page_break_before, PageBreakBefore, NS_STYLE_HINT_REFLOW)
CSS_PROP(page-break-inside, page_break_inside, PageBreakInside, NS_STYLE_HINT_REFLOW)
CSS_PROP(pause, pause, Pause, NS_STYLE_HINT_AURAL)
CSS_PROP(pause-after, pause_after, PauseAfter, NS_STYLE_HINT_AURAL)
CSS_PROP(pause-before, pause_before, PauseBefore, NS_STYLE_HINT_AURAL)
CSS_PROP(pitch, pitch, Pitch, NS_STYLE_HINT_AURAL)
CSS_PROP(pitch-range, pitch_range, PitchRange, NS_STYLE_HINT_AURAL)
CSS_PROP(play-during, play_during, PlayDuring, NS_STYLE_HINT_AURAL)
CSS_PROP_INTERNAL(-x-play-during-flags, play_during_flags, PlayDuringFlags, NS_STYLE_HINT_AURAL) // XXX why is this here?
CSS_PROP(position, position, Position, NS_STYLE_HINT_FRAMECHANGE)
CSS_PROP(quotes, quotes, Quotes, NS_STYLE_HINT_REFLOW)
CSS_PROP_INTERNAL(-x-quotes-close, quotes_close, QuotesClose, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-quotes-open, quotes_open, QuotesOpen, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(-moz-resizer, resizer, MozResizer, NS_STYLE_HINT_FRAMECHANGE) // XXX bug 3935
CSS_PROP(richness, richness, Richness, NS_STYLE_HINT_AURAL)
CSS_PROP(right, right, Right, NS_STYLE_HINT_REFLOW)
CSS_PROP(size, size, Size, NS_STYLE_HINT_REFLOW)
CSS_PROP_INTERNAL(-x-size-height, size_height, SizeHeight, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP_INTERNAL(-x-size-width, size_width, SizeWidth, NS_STYLE_HINT_REFLOW) // XXX bug 3935
CSS_PROP(speak, speak, Speak, NS_STYLE_HINT_AURAL)
CSS_PROP(speak-header, speak_header, SpeakHeader, NS_STYLE_HINT_AURAL)
CSS_PROP(speak-numeral, speak_numeral, SpeakNumeral, NS_STYLE_HINT_AURAL)
CSS_PROP(speak-punctuation, speak_punctuation, SpeakPunctuation, NS_STYLE_HINT_AURAL)
CSS_PROP(speech-rate, speech_rate, SpeechRate, NS_STYLE_HINT_AURAL)
CSS_PROP(stress, stress, Stress, NS_STYLE_HINT_AURAL)
CSS_PROP(table-layout, table_layout, TableLayout, NS_STYLE_HINT_REFLOW)
CSS_PROP(text-align, text_align, TextAlign, NS_STYLE_HINT_REFLOW)
CSS_PROP(text-decoration, text_decoration, TextDecoration, NS_STYLE_HINT_VISUAL)
CSS_PROP(text-indent, text_indent, TextIndent, NS_STYLE_HINT_REFLOW)
CSS_PROP(text-shadow, text_shadow, TextShadow, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(-x-text-shadow-color, text_shadow_color, TextShadowColor, NS_STYLE_HINT_VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-text-shadow-radius, text_shadow_radius, TextShadowRadius, NS_STYLE_HINT_VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-text-shadow-x, text_shadow_x, TextShadowX, NS_STYLE_HINT_VISUAL) // XXX bug 3935
CSS_PROP_INTERNAL(-x-text-shadow-y, text_shadow_y, TextShadowY, NS_STYLE_HINT_VISUAL) // XXX bug 3935
CSS_PROP(text-transform, text_transform, TextTransform, NS_STYLE_HINT_REFLOW)
CSS_PROP(top, top, Top, NS_STYLE_HINT_REFLOW)
CSS_PROP(unicode-bidi, unicode_bidi, UnicodeBidi, NS_STYLE_HINT_REFLOW)
CSS_PROP(-moz-user-focus, user_focus, MozUserFocus, NS_STYLE_HINT_CONTENT) // XXX bug 3935
CSS_PROP(-moz-user-input, user_input, MozUserInput, NS_STYLE_HINT_FRAMECHANGE) // XXX ??? // XXX bug 3935
CSS_PROP(-moz-user-modify, user_modify, MozUserModify, NS_STYLE_HINT_FRAMECHANGE) // XXX bug 3935
CSS_PROP(-moz-user-select, user_select, MozUserSelect, NS_STYLE_HINT_CONTENT) // XXX bug 3935
CSS_PROP(vertical-align, vertical_align, VerticalAlign, NS_STYLE_HINT_REFLOW)
CSS_PROP(visibility, visibility, Visibility, NS_STYLE_HINT_REFLOW)  // reflow for collapse
CSS_PROP(voice-family, voice_family, VoiceFamily, NS_STYLE_HINT_AURAL)
CSS_PROP(volume, volume, Volume, NS_STYLE_HINT_AURAL)
CSS_PROP(white-space, white_space, WhiteSpace, NS_STYLE_HINT_REFLOW)
CSS_PROP(widows, widows, Widows, NS_STYLE_HINT_REFLOW)
CSS_PROP(width, width, Width, NS_STYLE_HINT_REFLOW)
CSS_PROP(word-spacing, word_spacing, WordSpacing, NS_STYLE_HINT_REFLOW)
CSS_PROP(z-index, z_index, ZIndex, NS_STYLE_HINT_REFLOW)
#ifdef MOZ_SVG
// XXX treat SVG's CSS Properties as internal for now.
// Do we want to create an nsIDOMSVGCSS2Properties interface?
CSS_PROP_INTERNAL(fill, fill, Fill, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(fill-opacity, fill_opacity, FillOpacity, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(fill-rule, fill_rule, FillRule, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke, stroke, Stroke, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke-dasharray, stroke_dasharray, StrokeDasharray, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke-dashoffset, stroke_dashoffset, StrokeDashoffset, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke-linecap, stroke_linecap, StrokeLinecap, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke-linejoin, stroke_linejoin, StrokeLinejoin, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke-miterlimit, stroke_miterlimit, StrokeMiterlimit, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke-opacity, stroke_opacity, StrokeOpacity, NS_STYLE_HINT_VISUAL)
CSS_PROP_INTERNAL(stroke-width, stroke_width, StrokeWidth, NS_STYLE_HINT_VISUAL)
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
