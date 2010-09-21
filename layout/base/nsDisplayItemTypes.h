/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Michael Ventnor <m.ventnor@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/**
 * It's useful to be able to dynamically check the type of certain items.
 * Every subclass of nsDisplayItem must have a new type added here for the purposes
 * of easy comparison and matching of items in different display lists.
 * 
 * This is #included inside nsDisplayItem.
 */
enum Type {
  TYPE_ALT_FEEDBACK = 1,
  TYPE_BACKGROUND,
  TYPE_BORDER,
  TYPE_BOX_SHADOW_OUTER,
  TYPE_BOX_SHADOW_INNER,
  TYPE_BULLET,
  TYPE_BUTTON_BORDER_BACKGROUND,
  TYPE_BUTTON_BOX_SHADOW_OUTER,
  TYPE_BUTTON_FOREGROUND,
  TYPE_CANVAS,
  TYPE_CANVAS_BACKGROUND,
  TYPE_CANVAS_FOCUS,
  TYPE_CARET,
  TYPE_CHECKED_CHECKBOX,
  TYPE_CHECKED_RADIOBUTTON,
  TYPE_CLIP,
  TYPE_CLIP_ROUNDED_RECT,
  TYPE_COLUMN_RULE,
  TYPE_COMBOBOX_FOCUS,
  TYPE_EVENT_RECEIVER,
  TYPE_FIELDSET_BORDER_BACKGROUND,
  TYPE_FRAMESET_BORDER,
  TYPE_FRAMESET_BLANK,
  TYPE_HEADER_FOOTER,
  TYPE_IMAGE,
  TYPE_LIST_FOCUS,
  TYPE_OPACITY,
  TYPE_OPTION_EVENT_GRABBER,
  TYPE_OUTLINE,
  TYPE_OWN_LAYER,
  TYPE_PAGE_CONTENT,
  TYPE_PAGE_SEQUENCE,
  TYPE_PLUGIN,
  TYPE_PRINT_PREVIEW_BACKGROUND,
  TYPE_PRINT_PLUGIN,
  TYPE_REMOTE,
  TYPE_SELECTION_OVERLAY,
  TYPE_SOLID_COLOR,
  TYPE_TABLE_CELL_BACKGROUND,
  TYPE_TABLE_CELL_SELECTION,
  TYPE_TABLE_ROW_BACKGROUND,
  TYPE_TABLE_ROW_GROUP_BACKGROUND,
  TYPE_TABLE_BORDER_BACKGROUND,
  TYPE_TEXT,
  TYPE_TEXT_DECORATION,
  TYPE_TEXT_SHADOW,
  TYPE_TRANSFORM,
  TYPE_VIDEO,
  TYPE_WRAP_LIST,
  TYPE_ZOOM,

#if defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF)
  TYPE_REFLOW_COUNT,
#endif

#ifdef MOZ_SVG
  TYPE_SVG_EFFECTS,
  TYPE_SVG_EVENT_RECEIVER,
#endif

#ifdef MOZ_XUL
  TYPE_XUL_EVENT_REDIRECTOR,
  TYPE_XUL_GROUP_BACKGROUND,
  TYPE_XUL_IMAGE,
  TYPE_XUL_TEXT_BOX,
  TYPE_XUL_TREE_BODY,
  TYPE_XUL_TREE_COL_SPLITTER_TARGET,
#ifdef DEBUG_LAYOUT
  TYPE_XUL_DEBUG,
#endif
#endif

#ifdef MOZ_MATHML
  TYPE_MATHML_BAR,
  TYPE_MATHML_CHAR_BACKGROUND,
  TYPE_MATHML_CHAR_FOREGROUND,
  TYPE_MATHML_ERROR,
  TYPE_MATHML_MENCLOSE_NOTATION,
  TYPE_MATHML_SELECTION_RECT,
  TYPE_MATHML_SLASH,
#ifdef NS_DEBUG
  TYPE_MATHML_BOUNDING_METRICS,
  TYPE_MATHML_CHAR_DEBUG,
#endif
#endif

#ifdef NS_DEBUG
  TYPE_DEBUG_BORDER,
  TYPE_DEBUG_IMAGE_MAP,
  TYPE_DEBUG_PLACEHOLDER,
  TYPE_EVENT_TARGET_BORDER,
#endif

  TYPE_MAX
};

enum {
  // Number of bits needed to represent all types
  TYPE_BITS = 8
};
