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

  It is designed to be used as inline input to nsCSSProps.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro CSS_PROP which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  Requirements:

  Entries are in the form: (name,id,effect). 'id' must always be the same as 
  'name' except that all hyphens ('-') in 'name' are converted to underscores 
  ('_') in 'id'. This lets us do nice things with the macros without having to 
  copy/convert strings at runtime. 
  
  'name' entries *must* use only lowercase characters.

  ** Break these invariants and bad things will happen. **    
  
  The third argument says what needs to be recomputed when the property
  changes.
 ******/

// For notes XXX bug 3935 below, the names being parsed do not correspond
// to the constants used internally.  It would be nice to bring the
// constants into line sometime.

// The parser will refuse to parse properties marked with -x-.

// Those marked XXX bug 48973 are CSS2 properties that we support
// differently from the spec for UI requirements.  If we ever
// support them correctly the old constants need to be renamed and
// new ones should be entered.

CSS_PROP(-moz-border-radius, _moz_border_radius, VISUAL)
CSS_PROP(-moz-border-radius-topleft, _moz_border_radius_topLeft, VISUAL)
CSS_PROP(-moz-border-radius-topright, _moz_border_radius_topRight, VISUAL)
CSS_PROP(-moz-border-radius-bottomleft, _moz_border_radius_bottomLeft, VISUAL)
CSS_PROP(-moz-border-radius-bottomright, _moz_border_radius_bottomRight, VISUAL)
CSS_PROP(-moz-outline-radius, _moz_outline_radius, VISUAL)
CSS_PROP(-moz-outline-radius-topleft, _moz_outline_radius_topLeft, VISUAL)
CSS_PROP(-moz-outline-radius-topright, _moz_outline_radius_topRight, VISUAL)
CSS_PROP(-moz-outline-radius-bottomleft, _moz_outline_radius_bottomLeft, VISUAL)
CSS_PROP(-moz-outline-radius-bottomright, _moz_outline_radius_bottomRight, VISUAL)
CSS_PROP(azimuth, azimuth, AURAL)
CSS_PROP(background, background, VISUAL)
CSS_PROP(background-attachment, background_attachment, VISUAL)
CSS_PROP(background-color, background_color, VISUAL)
CSS_PROP(background-image, background_image, VISUAL)
CSS_PROP(background-position, background_position, VISUAL)
CSS_PROP(background-repeat, background_repeat, VISUAL)
CSS_PROP(-x-background-x-position, background_x_position, VISUAL) // XXX bug 3935
CSS_PROP(-x-background-y-position, background_y_position, VISUAL) // XXX bug 3935
CSS_PROP(-moz-binding, binding, FRAMECHANGE) // XXX bug 3935
CSS_PROP(border, border, REFLOW)
CSS_PROP(border-bottom, border_bottom, REFLOW)
CSS_PROP(border-bottom-color, border_bottom_color, VISUAL)
CSS_PROP(border-bottom-style, border_bottom_style, REFLOW)  // on/off will need reflow
CSS_PROP(border-bottom-width, border_bottom_width, REFLOW)
CSS_PROP(border-collapse, border_collapse, REFLOW)
CSS_PROP(border-color, border_color, VISUAL)
CSS_PROP(border-left, border_left, REFLOW)
CSS_PROP(border-left-color, border_left_color, VISUAL)
CSS_PROP(border-left-style, border_left_style, REFLOW)  // on/off will need reflow
CSS_PROP(border-left-width, border_left_width, REFLOW)
CSS_PROP(border-right, border_right, REFLOW)
CSS_PROP(border-right-color, border_right_color, VISUAL)
CSS_PROP(border-right-style, border_right_style, REFLOW)  // on/off will need reflow
CSS_PROP(border-right-width, border_right_width, REFLOW)
CSS_PROP(border-spacing, border_spacing, REFLOW)
CSS_PROP(border-style, border_style, REFLOW)  // on/off will need reflow
CSS_PROP(border-top, border_top, REFLOW)
CSS_PROP(border-top-color, border_top_color, VISUAL)
CSS_PROP(border-top-style, border_top_style, REFLOW)  // on/off will need reflow
CSS_PROP(border-top-width, border_top_width, REFLOW)
CSS_PROP(border-width, border_width, REFLOW)
CSS_PROP(-x-border-x-spacing, border_x_spacing, REFLOW) // XXX bug 3935
CSS_PROP(-x-border-y-spacing, border_y_spacing, REFLOW) // XXX bug 3935
CSS_PROP(bottom, bottom, REFLOW)
CSS_PROP(-moz-box-align, box_align, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-direction, box_direction, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-flex, box_flex, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-flex-group, box_flex_group, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-orient, box_orient, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-pack, box_pack, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-sizing, box_sizing, REFLOW) // XXX bug 3935
CSS_PROP(-moz-box-ordinal-group, box_ordinal_group, FRAMECHANGE)
CSS_PROP(caption-side, caption_side, REFLOW)
CSS_PROP(clear, clear, REFLOW)
CSS_PROP(clip, clip, REFLOW)
CSS_PROP(-x-clip-bottom, clip_bottom, REFLOW) // XXX bug 3935
CSS_PROP(-x-clip-left, clip_left, REFLOW) // XXX bug 3935
CSS_PROP(-x-clip-right, clip_right, REFLOW) // XXX bug 3935
CSS_PROP(-x-clip-top, clip_top, REFLOW) // XXX bug 3935
CSS_PROP(color, color, VISUAL)
CSS_PROP(content, content, FRAMECHANGE)
CSS_PROP(counter-increment, counter_increment, REFLOW)
CSS_PROP(counter-reset, counter_reset, REFLOW)
CSS_PROP(cue, cue, AURAL)
CSS_PROP(cue-after, cue_after, AURAL)
CSS_PROP(cue-before, cue_before, AURAL)
CSS_PROP(cursor, cursor, VISUAL)
CSS_PROP(direction, direction, REFLOW)
CSS_PROP(display, display, FRAMECHANGE)
CSS_PROP(elevation, elevation, AURAL)
CSS_PROP(empty-cells, empty_cells, VISUAL)
CSS_PROP(float, float, FRAMECHANGE)
CSS_PROP(-moz-float-edge, float_edge, REFLOW) // XXX bug 3935
CSS_PROP(font, font, REFLOW)
CSS_PROP(font-family, font_family, REFLOW)
CSS_PROP(font-size, font_size, REFLOW)
CSS_PROP(font-size-adjust, font_size_adjust, REFLOW)
CSS_PROP(font-stretch, font_stretch, REFLOW)
CSS_PROP(font-style, font_style, REFLOW)
CSS_PROP(font-variant, font_variant, REFLOW)
CSS_PROP(font-weight, font_weight, REFLOW)
CSS_PROP(height, height, REFLOW)
CSS_PROP(-moz-key-equivalent, key_equivalent, CONTENT) // This will need some other notification, but what? // XXX bug 3935
CSS_PROP(left, left, REFLOW)
CSS_PROP(letter-spacing, letter_spacing, REFLOW)
CSS_PROP(line-height, line_height, REFLOW)
CSS_PROP(list-style, list_style, REFLOW)
CSS_PROP(list-style-image, list_style_image, REFLOW)
CSS_PROP(list-style-position, list_style_position, REFLOW)
CSS_PROP(list-style-type, list_style_type, REFLOW)
CSS_PROP(margin, margin, REFLOW)
CSS_PROP(margin-bottom, margin_bottom, REFLOW)
CSS_PROP(margin-left, margin_left, REFLOW)
CSS_PROP(margin-right, margin_right, REFLOW)
CSS_PROP(margin-top, margin_top, REFLOW)
CSS_PROP(marker-offset, marker_offset, REFLOW)
CSS_PROP(marks, marks, VISUAL)
CSS_PROP(max-height, max_height, REFLOW)
CSS_PROP(max-width, max_width, REFLOW)
CSS_PROP(min-height, min_height, REFLOW)
CSS_PROP(min-width, min_width, REFLOW)
CSS_PROP(-moz-opacity, opacity, VISUAL) // XXX bug 3935
CSS_PROP(orphans, orphans, REFLOW)
CSS_PROP(-moz-outline, outline, VISUAL)  // XXX This is temporary fix for nsbeta3+ Bug 48973, turning outline into -moz-outline  XXX bug 48973
CSS_PROP(-moz-outline-color, outline_color, VISUAL) // XXX bug 48973
CSS_PROP(-moz-outline-style, outline_style, VISUAL) // XXX bug 48973
CSS_PROP(-moz-outline-width, outline_width, VISUAL) // XXX bug 48973
CSS_PROP(overflow, overflow, FRAMECHANGE)
CSS_PROP(padding, padding, REFLOW)
CSS_PROP(padding-bottom, padding_bottom, REFLOW)
CSS_PROP(padding-left, padding_left, REFLOW)
CSS_PROP(padding-right, padding_right, REFLOW)
CSS_PROP(padding-top, padding_top, REFLOW)
CSS_PROP(page, page, REFLOW)
CSS_PROP(page-break-after, page_break_after, REFLOW)
CSS_PROP(page-break-before, page_break_before, REFLOW)
CSS_PROP(page-break-inside, page_break_inside, REFLOW)
CSS_PROP(pause, pause, AURAL)
CSS_PROP(pause-after, pause_after, AURAL)
CSS_PROP(pause-before, pause_before, AURAL)
CSS_PROP(pitch, pitch, AURAL)
CSS_PROP(pitch-range, pitch_range, AURAL)
CSS_PROP(play-during, play_during, AURAL)
CSS_PROP(-x-play-during-flags, play_during_flags, AURAL) // XXX why is this here?
CSS_PROP(position, position, FRAMECHANGE)
CSS_PROP(quotes, quotes, REFLOW)
CSS_PROP(-x-quotes-close, quotes_close, REFLOW) // XXX bug 3935
CSS_PROP(-x-quotes-open, quotes_open, REFLOW) // XXX bug 3935
CSS_PROP(-moz-resizer, resizer, FRAMECHANGE) // XXX bug 3935
CSS_PROP(richness, richness, AURAL)
CSS_PROP(right, right, REFLOW)
CSS_PROP(size, size, REFLOW)
CSS_PROP(-x-size-height, size_height, REFLOW) // XXX bug 3935
CSS_PROP(-x-size-width, size_width, REFLOW) // XXX bug 3935
CSS_PROP(speak, speak, AURAL)
CSS_PROP(speak-header, speak_header, AURAL)
CSS_PROP(speak-numeral, speak_numeral, AURAL)
CSS_PROP(speak-punctuation, speak_punctuation, AURAL)
CSS_PROP(speech-rate, speech_rate, AURAL)
CSS_PROP(stress, stress, AURAL)
CSS_PROP(table-layout, table_layout, REFLOW)
CSS_PROP(text-align, text_align, REFLOW)
CSS_PROP(text-decoration, text_decoration, VISUAL)
CSS_PROP(text-indent, text_indent, REFLOW)
CSS_PROP(text-shadow, text_shadow, VISUAL)
CSS_PROP(-x-text-shadow-color, text_shadow_color, VISUAL) // XXX bug 3935
CSS_PROP(-x-text-shadow-radius, text_shadow_radius, VISUAL) // XXX bug 3935
CSS_PROP(-x-text-shadow-x, text_shadow_x, VISUAL) // XXX bug 3935
CSS_PROP(-x-text-shadow-y, text_shadow_y, VISUAL) // XXX bug 3935
CSS_PROP(text-transform, text_transform, REFLOW)
CSS_PROP(top, top, REFLOW)
CSS_PROP(unicode-bidi, unicode_bidi, REFLOW)
CSS_PROP(-moz-user-focus, user_focus, CONTENT) // XXX bug 3935
CSS_PROP(-moz-user-input, user_input, FRAMECHANGE) // XXX ??? // XXX bug 3935
CSS_PROP(-moz-user-modify, user_modify, FRAMECHANGE) // XXX bug 3935
CSS_PROP(-moz-user-select, user_select, CONTENT) // XXX bug 3935
CSS_PROP(vertical-align, vertical_align, REFLOW)
CSS_PROP(visibility, visibility, REFLOW)  // reflow for collapse
CSS_PROP(voice-family, voice_family, AURAL)
CSS_PROP(volume, volume, AURAL)
CSS_PROP(white-space, white_space, REFLOW)
CSS_PROP(widows, widows, REFLOW)
CSS_PROP(width, width, REFLOW)
CSS_PROP(word-spacing, word_spacing, REFLOW)
CSS_PROP(z-index, z_index, REFLOW)
