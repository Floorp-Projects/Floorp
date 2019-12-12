/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* keywords used within CSS property values */

/******

  This file contains the list of all parsed CSS keywords
  See nsCSSKeywords.h for access to the enum values for keywords

  It is designed to be used as inline input to nsCSSKeywords.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro CSS_KEY which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  Requirements:

  Entries are in the form: (name, id). 'id' must always be the same as 'name'
  except that all hyphens ('-') in 'name' are converted to underscores ('_')
  in 'id'. This lets us do nice things with the macros without having to
  copy/convert strings at runtime.

  'name' entries *must* use only lowercase characters.

  ** Break these invariants and bad things will happen. **

 ******/

// OUTPUT_CLASS=nsCSSKeywords
// MACRO_NAME=CSS_KEY

CSS_KEY(-moz-available, _moz_available)
CSS_KEY(-moz-box, _moz_box)
CSS_KEY(-moz-center, _moz_center)
CSS_KEY(-moz-block-height, _moz_block_height)
CSS_KEY(-moz-deck, _moz_deck)
CSS_KEY(-moz-fit-content, _moz_fit_content)
CSS_KEY(-moz-grabbing, _moz_grabbing)
CSS_KEY(-moz-grab, _moz_grab)
CSS_KEY(-moz-grid-group, _moz_grid_group)
CSS_KEY(-moz-grid-line, _moz_grid_line)
CSS_KEY(-moz-grid, _moz_grid)
CSS_KEY(-moz-groupbox, _moz_groupbox)
CSS_KEY(-moz-inline-box, _moz_inline_box)
CSS_KEY(-moz-left, _moz_left)
CSS_KEY(-moz-none, _moz_none)
CSS_KEY(-moz-popup, _moz_popup)
CSS_KEY(-moz-right, _moz_right)
CSS_KEY(-moz-stack, _moz_stack)
CSS_KEY(-moz-zoom-in, _moz_zoom_in)
CSS_KEY(-moz-zoom-out, _moz_zoom_out)
CSS_KEY(-webkit-box, _webkit_box)
CSS_KEY(-webkit-flex, _webkit_flex)
CSS_KEY(-webkit-inline-box, _webkit_inline_box)
CSS_KEY(-webkit-inline-flex, _webkit_inline_flex)
CSS_KEY(alias, alias)
CSS_KEY(all-scroll, all_scroll)
CSS_KEY(auto, auto)
CSS_KEY(baseline, baseline)
CSS_KEY(block, block)
CSS_KEY(cell, cell)
CSS_KEY(center, center)
CSS_KEY(circle, circle)
CSS_KEY(col-resize, col_resize)
CSS_KEY(column, column)
CSS_KEY(contents, contents)
CSS_KEY(context-menu, context_menu)
CSS_KEY(copy, copy)
CSS_KEY(crosshair, crosshair)
CSS_KEY(dashed, dashed)
CSS_KEY(dense, dense)
CSS_KEY(default, default)
CSS_KEY(dot, dot)
CSS_KEY(dotted, dotted)
CSS_KEY(double, double)
CSS_KEY(double-circle, double_circle)
CSS_KEY(e-resize, e_resize)
CSS_KEY(end, end)
CSS_KEY(ew-resize, ew_resize)
CSS_KEY(flex, flex)
CSS_KEY(flex-end, flex_end)
CSS_KEY(flex-start, flex_start)
CSS_KEY(flow-root, flow_root)
CSS_KEY(grab, grab)
CSS_KEY(grabbing, grabbing)
CSS_KEY(grayscale, grayscale)
CSS_KEY(grid, grid)
CSS_KEY(help, help)
CSS_KEY(inline, inline)
CSS_KEY(inline-block, inline_block)
CSS_KEY(inline-flex, inline_flex)
CSS_KEY(inline-grid, inline_grid)
CSS_KEY(inline-table, inline_table)
CSS_KEY(inset, inset)
CSS_KEY(justify, justify)
CSS_KEY(last baseline, last_baseline) // only used for DevTools auto-completion
CSS_KEY(left, left)
CSS_KEY(list-item, list_item)
CSS_KEY(max-content, max_content)
CSS_KEY(min-content, min_content)
CSS_KEY(move, move)
CSS_KEY(n-resize, n_resize)
CSS_KEY(ne-resize, ne_resize)
CSS_KEY(nesw-resize, nesw_resize)
CSS_KEY(no-drop, no_drop)
CSS_KEY(none, none)
CSS_KEY(normal, normal)
CSS_KEY(not-allowed, not_allowed)
CSS_KEY(ns-resize, ns_resize)
CSS_KEY(nw-resize, nw_resize)
CSS_KEY(nwse-resize, nwse_resize)
CSS_KEY(pointer, pointer)
CSS_KEY(progress, progress)
CSS_KEY(right, right)
CSS_KEY(row, row)
CSS_KEY(row-resize, row_resize)
CSS_KEY(ruby, ruby)
CSS_KEY(ruby-base, ruby_base)
CSS_KEY(ruby-base-container, ruby_base_container)
CSS_KEY(ruby-text, ruby_text)
CSS_KEY(ruby-text-container, ruby_text_container)
CSS_KEY(s-resize, s_resize)
CSS_KEY(se-resize, se_resize)
CSS_KEY(self-end, self_end)
CSS_KEY(self-start, self_start)
CSS_KEY(sesame, sesame)
CSS_KEY(solid, solid)
CSS_KEY(space-around, space_around)
CSS_KEY(space-between, space_between)
CSS_KEY(space-evenly, space_evenly)
CSS_KEY(start, start)
CSS_KEY(stretch, stretch)
CSS_KEY(subgrid, subgrid)
CSS_KEY(sw-resize, sw_resize)
CSS_KEY(table, table)
CSS_KEY(table-caption, table_caption)
CSS_KEY(table-cell, table_cell)
CSS_KEY(table-column, table_column)
CSS_KEY(table-column-group, table_column_group)
CSS_KEY(table-footer-group, table_footer_group)
CSS_KEY(table-header-group, table_header_group)
CSS_KEY(table-row, table_row)
CSS_KEY(table-row-group, table_row_group)
CSS_KEY(text, text)
CSS_KEY(triangle, triangle)
CSS_KEY(vertical-text, vertical_text)
CSS_KEY(w-resize, w_resize)
CSS_KEY(wait, wait)
CSS_KEY(wavy, wavy)
CSS_KEY(zoom-in, zoom_in)
CSS_KEY(zoom-out, zoom_out)

// Appearance keywords for widget styles
//CSS_KEY(start, start)

