/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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

  Underbars in properties are automagically converted into hyphens

  The first argument to CSS_PROP is both the enum identifier of the property
  and the string value
  The second argument is the style impact resultant in a change to the property
 ******/


CSS_PROP(_moz_border_radius, VISUAL)
CSS_PROP(azimuth, AURAL)
CSS_PROP(background, VISUAL)
CSS_PROP(background_attachment, VISUAL)
CSS_PROP(background_color, VISUAL)
CSS_PROP(background_image, VISUAL)
CSS_PROP(background_position, VISUAL)
CSS_PROP(background_repeat, VISUAL)
CSS_PROP(background_x_position, VISUAL)
CSS_PROP(background_y_position, VISUAL)
CSS_PROP(border, REFLOW)
CSS_PROP(border_bottom, REFLOW)
CSS_PROP(border_bottom_color, VISUAL)
CSS_PROP(border_bottom_style, REFLOW)  // on/off will need reflow
CSS_PROP(border_bottom_width, REFLOW)
CSS_PROP(border_collapse, REFLOW)
CSS_PROP(border_color, VISUAL)
CSS_PROP(border_left, REFLOW)
CSS_PROP(border_left_color, VISUAL)
CSS_PROP(border_left_style, REFLOW)  // on/off will need reflow
CSS_PROP(border_left_width, REFLOW)
CSS_PROP(border_right, REFLOW)
CSS_PROP(border_right_color, VISUAL)
CSS_PROP(border_right_style, REFLOW)  // on/off will need reflow
CSS_PROP(border_right_width, REFLOW)
CSS_PROP(border_spacing, REFLOW)
CSS_PROP(border_style, REFLOW)  // on/off will need reflow
CSS_PROP(border_top, REFLOW)
CSS_PROP(border_top_color, VISUAL)
CSS_PROP(border_top_style, REFLOW)  // on/off will need reflow
CSS_PROP(border_top_width, REFLOW)
CSS_PROP(border_width, REFLOW)
CSS_PROP(border_x_spacing, REFLOW)
CSS_PROP(border_y_spacing, REFLOW)
CSS_PROP(bottom, REFLOW)
CSS_PROP(box_sizing, REFLOW)
CSS_PROP(caption_side, REFLOW)
CSS_PROP(clear, REFLOW)
CSS_PROP(clip, VISUAL)
CSS_PROP(clip_bottom, VISUAL)
CSS_PROP(clip_left, VISUAL)
CSS_PROP(clip_right, VISUAL)
CSS_PROP(clip_top, VISUAL)
CSS_PROP(color, VISUAL)
CSS_PROP(content, FRAMECHANGE)
CSS_PROP(counter_increment, REFLOW)
CSS_PROP(counter_reset, REFLOW)
CSS_PROP(cue, AURAL)
CSS_PROP(cue_after, AURAL)
CSS_PROP(cue_before, AURAL)
CSS_PROP(cursor, VISUAL)
CSS_PROP(direction, REFLOW)
CSS_PROP(display, FRAMECHANGE)
CSS_PROP(elevation, AURAL)
CSS_PROP(empty_cells, VISUAL)
CSS_PROP(float, FRAMECHANGE)
CSS_PROP(float_edge, REFLOW)
CSS_PROP(font, REFLOW)
CSS_PROP(font_family, REFLOW)
CSS_PROP(font_size, REFLOW)
CSS_PROP(font_size_adjust, REFLOW)
CSS_PROP(font_stretch, REFLOW)
CSS_PROP(font_style, REFLOW)
CSS_PROP(font_variant, REFLOW)
CSS_PROP(font_weight, REFLOW)
CSS_PROP(height, REFLOW)
CSS_PROP(key_equivalent, CONTENT) // This will need some other notification, but what?
CSS_PROP(left, REFLOW)
CSS_PROP(letter_spacing, REFLOW)
CSS_PROP(line_height, REFLOW)
CSS_PROP(list_style, REFLOW)
CSS_PROP(list_style_image, REFLOW)
CSS_PROP(list_style_position, REFLOW)
CSS_PROP(list_style_type, REFLOW)
CSS_PROP(margin, REFLOW)
CSS_PROP(margin_bottom, REFLOW)
CSS_PROP(margin_left, REFLOW)
CSS_PROP(margin_right, REFLOW)
CSS_PROP(margin_top, REFLOW)
CSS_PROP(marker_offset, REFLOW)
CSS_PROP(marks, VISUAL)
CSS_PROP(max_height, REFLOW)
CSS_PROP(max_width, REFLOW)
CSS_PROP(min_height, REFLOW)
CSS_PROP(min_width, REFLOW)
CSS_PROP(opacity, VISUAL)
CSS_PROP(orphans, REFLOW)
CSS_PROP(outline, VISUAL)
CSS_PROP(outline_color, VISUAL)
CSS_PROP(outline_style, VISUAL)
CSS_PROP(outline_width, VISUAL)
CSS_PROP(overflow, FRAMECHANGE)
CSS_PROP(padding, REFLOW)
CSS_PROP(padding_bottom, REFLOW)
CSS_PROP(padding_left, REFLOW)
CSS_PROP(padding_right, REFLOW)
CSS_PROP(padding_top, REFLOW)
CSS_PROP(page, REFLOW)
CSS_PROP(page_break_after, REFLOW)
CSS_PROP(page_break_before, REFLOW)
CSS_PROP(page_break_inside, REFLOW)
CSS_PROP(pause, AURAL)
CSS_PROP(pause_after, AURAL)
CSS_PROP(pause_before, AURAL)
CSS_PROP(pitch, AURAL)
CSS_PROP(pitch_range, AURAL)
CSS_PROP(play_during, AURAL)
CSS_PROP(play_during_flags, AURAL)
CSS_PROP(position, FRAMECHANGE)
CSS_PROP(quotes, REFLOW)
CSS_PROP(quotes_close, REFLOW)
CSS_PROP(quotes_open, REFLOW)
CSS_PROP(resizer, FRAMECHANGE)
CSS_PROP(richness, AURAL)
CSS_PROP(right, REFLOW)
CSS_PROP(size, REFLOW)
CSS_PROP(size_height, REFLOW)
CSS_PROP(size_width, REFLOW)
CSS_PROP(speak, AURAL)
CSS_PROP(speak_header, AURAL)
CSS_PROP(speak_numeral, AURAL)
CSS_PROP(speak_punctuation, AURAL)
CSS_PROP(speech_rate, AURAL)
CSS_PROP(stress, AURAL)
CSS_PROP(table_layout, REFLOW)
CSS_PROP(text_align, REFLOW)
CSS_PROP(text_decoration, VISUAL)
CSS_PROP(text_indent, REFLOW)
CSS_PROP(text_shadow, VISUAL)
CSS_PROP(text_shadow_color, VISUAL)
CSS_PROP(text_shadow_radius, VISUAL)
CSS_PROP(text_shadow_x, VISUAL)
CSS_PROP(text_shadow_y, VISUAL)
CSS_PROP(text_transform, REFLOW)
CSS_PROP(top, REFLOW)
CSS_PROP(unicode_bidi, REFLOW)
CSS_PROP(user_focus, CONTENT)
CSS_PROP(user_input, FRAMECHANGE) // XXX ???
CSS_PROP(user_modify, FRAMECHANGE)
CSS_PROP(user_select, CONTENT)
CSS_PROP(vertical_align, REFLOW)
CSS_PROP(visibility, REFLOW)  // reflow for collapse
CSS_PROP(voice_family, AURAL)
CSS_PROP(volume, AURAL)
CSS_PROP(white_space, REFLOW)
CSS_PROP(widows, REFLOW)
CSS_PROP(width, REFLOW)
CSS_PROP(word_spacing, REFLOW)
CSS_PROP(z_index, REFLOW)

