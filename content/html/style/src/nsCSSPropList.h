/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/******

  This file contains the list of all parsed CSS properties
  See nsCSSProps.h for access to the enum values for properties

  It is designed to be used as inline input to nsCSSProps.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro CSS_PROP which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  Requirements:

  Entries are in the form: (name,id,impact). 'id' must always be the same as 
  'name' except that all hyphens ('-') in 'name' are converted to underscores 
  ('_') in 'id'. This lets us do nice things with the macros without having to 
  copy/convert strings at runtime. 
  
  'name' entries *must* use only lowercase characters.

  ** Break these invarient and bad things will happen. **    
  
  The third argument is the style impact resultant in a change to the property
 ******/


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
CSS_PROP(background-x-position, background_x_position, VISUAL)
CSS_PROP(background-y-position, background_y_position, VISUAL)
CSS_PROP(behavior, behavior, REFLOW)
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
CSS_PROP(border-x-spacing, border_x_spacing, REFLOW)
CSS_PROP(border-y-spacing, border_y_spacing, REFLOW)
CSS_PROP(bottom, bottom, REFLOW)
CSS_PROP(box-sizing, box_sizing, REFLOW)
CSS_PROP(caption-side, caption_side, REFLOW)
CSS_PROP(clear, clear, REFLOW)
CSS_PROP(clip, clip, VISUAL)
CSS_PROP(clip-bottom, clip_bottom, VISUAL)
CSS_PROP(clip-left, clip_left, VISUAL)
CSS_PROP(clip-right, clip_right, VISUAL)
CSS_PROP(clip-top, clip_top, VISUAL)
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
CSS_PROP(float-edge, float_edge, REFLOW)
CSS_PROP(font, font, REFLOW)
CSS_PROP(font-family, font_family, REFLOW)
CSS_PROP(font-size, font_size, REFLOW)
CSS_PROP(font-size-adjust, font_size_adjust, REFLOW)
CSS_PROP(font-stretch, font_stretch, REFLOW)
CSS_PROP(font-style, font_style, REFLOW)
CSS_PROP(font-variant, font_variant, REFLOW)
CSS_PROP(font-weight, font_weight, REFLOW)
CSS_PROP(height, height, REFLOW)
CSS_PROP(key-equivalent, key_equivalent, CONTENT) // This will need some other notification, but what?
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
CSS_PROP(opacity, opacity, VISUAL)
CSS_PROP(orphans, orphans, REFLOW)
CSS_PROP(outline, outline, VISUAL)
CSS_PROP(outline-color, outline_color, VISUAL)
CSS_PROP(outline-style, outline_style, VISUAL)
CSS_PROP(outline-width, outline_width, VISUAL)
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
CSS_PROP(play-during-flags, play_during_flags, AURAL)
CSS_PROP(position, position, FRAMECHANGE)
CSS_PROP(quotes, quotes, REFLOW)
CSS_PROP(quotes-close, quotes_close, REFLOW)
CSS_PROP(quotes-open, quotes_open, REFLOW)
CSS_PROP(resizer, resizer, FRAMECHANGE)
CSS_PROP(richness, richness, AURAL)
CSS_PROP(right, right, REFLOW)
CSS_PROP(size, size, REFLOW)
CSS_PROP(size-height, size_height, REFLOW)
CSS_PROP(size-width, size_width, REFLOW)
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
CSS_PROP(text-shadow-color, text_shadow_color, VISUAL)
CSS_PROP(text-shadow-radius, text_shadow_radius, VISUAL)
CSS_PROP(text-shadow-x, text_shadow_x, VISUAL)
CSS_PROP(text-shadow-y, text_shadow_y, VISUAL)
CSS_PROP(text-transform, text_transform, REFLOW)
CSS_PROP(top, top, REFLOW)
CSS_PROP(unicode-bidi, unicode_bidi, REFLOW)
CSS_PROP(user-focus, user_focus, CONTENT)
CSS_PROP(user-input, user_input, FRAMECHANGE) // XXX ???
CSS_PROP(user-modify, user_modify, FRAMECHANGE)
CSS_PROP(user-select, user_select, CONTENT)
CSS_PROP(vertical-align, vertical_align, REFLOW)
CSS_PROP(visibility, visibility, REFLOW)  // reflow for collapse
CSS_PROP(voice-family, voice_family, AURAL)
CSS_PROP(volume, volume, AURAL)
CSS_PROP(white-space, white_space, REFLOW)
CSS_PROP(widows, widows, REFLOW)
CSS_PROP(width, width, REFLOW)
CSS_PROP(word-spacing, word_spacing, REFLOW)
CSS_PROP(z-index, z_index, REFLOW)
