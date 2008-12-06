/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNameUtils.h"

////////////////////////////////////////////////////////////////////////////////
// Name rules to role map.

PRUint32 nsNameUtils::gRoleToNameRulesMap[] =
{
  eNoRule,           // ROLE_NOTHING
  eNoRule,           // ROLE_TITLEBAR
  eNoRule,           // ROLE_MENUBAR
  eNoRule,           // ROLE_SCROLLBAR
  eNoRule,           // ROLE_GRIP
  eNoRule,           // ROLE_SOUND
  eNoRule,           // ROLE_CURSOR
  eNoRule,           // ROLE_CARET
  eNoRule,           // ROLE_ALERT
  eNoRule,           // ROLE_WINDOW
  eNoRule,           // ROLE_INTERNAL_FRAME
  eNoRule,           // ROLE_MENUPOPUP
  eFromSubtree,      // ROLE_MENUITEM
  eFromSubtree,      // ROLE_TOOLTIP
  eNoRule,           // ROLE_APPLICATION
  eNoRule,           // ROLE_DOCUMENT
  eNoRule,           // ROLE_PANE
  eNoRule,           // ROLE_CHART
  eNoRule,           // ROLE_DIALOG
  eNoRule,           // ROLE_BORDER
  eNoRule,           // ROLE_GROUPING
  eNoRule,           // ROLE_SEPARATOR
  eNoRule,           // ROLE_TOOLBAR
  eNoRule,           // ROLE_STATUSBAR
  eNoRule,           // ROLE_TABLE
  eFromSubtree,      // ROLE_COLUMNHEADER
  eFromSubtree,      // ROLE_ROWHEADER
  eFromSubtree,      // ROLE_COLUMN
  eFromSubtree,      // ROLE_ROW
  eFromSubtree,      // ROLE_CELL
  eFromSubtree,      // ROLE_LINK
  eFromSubtree,      // ROLE_HELPBALLOON
  eNoRule,           // ROLE_CHARACTER
  eNoRule,           // ROLE_LIST
  eFromSubtree,      // ROLE_LISTITEM
  eNoRule,           // ROLE_OUTLINE
  eFromSubtree,      // ROLE_OUTLINEITEM
  eFromSubtree,      // ROLE_PAGETAB
  eNoRule,           // ROLE_PROPERTYPAGE
  eNoRule,           // ROLE_INDICATOR
  eNoRule,           // ROLE_GRAPHIC
  eNoRule,           // ROLE_STATICTEXT
  eNoRule,           // ROLE_TEXT_LEAF
  eFromSubtree,      // ROLE_PUSHBUTTON
  eFromSubtree,      // ROLE_CHECKBUTTON
  eFromSubtree,      // ROLE_RADIOBUTTON
  eNoRule,           // ROLE_COMBOBOX
  eNoRule,           // ROLE_DROPLIST
  eNoRule,           // ROLE_PROGRESSBAR
  eNoRule,           // ROLE_DIAL
  eNoRule,           // ROLE_HOTKEYFIELD
  eNoRule,           // ROLE_SLIDER
  eNoRule,           // ROLE_SPINBUTTON
  eNoRule,           // ROLE_DIAGRAM
  eNoRule,           // ROLE_ANIMATION
  eNoRule,           // ROLE_EQUATION
  eFromSubtree,      // ROLE_BUTTONDROPDOWN
  eFromSubtree,      // ROLE_BUTTONMENU
  eFromSubtree,      // ROLE_BUTTONDROPDOWNGRID
  eNoRule,           // ROLE_WHITESPACE
  eNoRule,           // ROLE_PAGETABLIST
  eNoRule,           // ROLE_CLOCK
  eNoRule,           // ROLE_SPLITBUTTON
  eNoRule,           // ROLE_IPADDRESS
  eNoRule,           // ROLE_ACCEL_LABEL
  eNoRule,           // ROLE_ARROW
  eNoRule,           // ROLE_CANVAS
  eFromSubtree,      // ROLE_CHECK_MENU_ITEM
  eNoRule,           // ROLE_COLOR_CHOOSER
  eNoRule,           // ROLE_DATE_EDITOR
  eNoRule,           // ROLE_DESKTOP_ICON
  eNoRule,           // ROLE_DESKTOP_FRAME
  eNoRule,           // ROLE_DIRECTORY_PANE
  eNoRule,           // ROLE_FILE_CHOOSER
  eNoRule,           // ROLE_FONT_CHOOSER
  eNoRule,           // ROLE_CHROME_WINDOW
  eNoRule,           // ROLE_GLASS_PANE
  eNoRule,           // ROLE_HTML_CONTAINER
  eNoRule,           // ROLE_ICON
  eNoRule,           // ROLE_LABEL
  eNoRule,           // ROLE_LAYERED_PANE
  eNoRule,           // ROLE_OPTION_PANE
  eNoRule,           // ROLE_PASSWORD_TEXT
  eNoRule,           // ROLE_POPUP_MENU
  eFromSubtree,      // ROLE_RADIO_MENU_ITEM
  eNoRule,           // ROLE_ROOT_PANE
  eNoRule,           // ROLE_SCROLL_PANE
  eNoRule,           // ROLE_SPLIT_PANE
  eFromSubtree,      // ROLE_TABLE_COLUMN_HEADER
  eFromSubtree,      // ROLE_TABLE_ROW_HEADER
  eFromSubtree,      // ROLE_TEAR_OFF_MENU_ITEM
  eNoRule,           // ROLE_TERMINAL
  eNoRule,           // ROLE_TEXT_CONTAINER
  eFromSubtree,      // ROLE_TOGGLE_BUTTON
  eNoRule,           // ROLE_TREE_TABLE
  eNoRule,           // ROLE_VIEWPORT
  eNoRule,           // ROLE_HEADER
  eNoRule,           // ROLE_FOOTER
  eNoRule,           // ROLE_PARAGRAPH
  eNoRule,           // ROLE_RULER
  eNoRule,           // ROLE_AUTOCOMPLETE
  eNoRule,           // ROLE_EDITBAR
  eNoRule,           // ROLE_ENTRY
  eNoRule,           // ROLE_CAPTION
  eNoRule,           // ROLE_DOCUMENT_FRAME
  eNoRule,           // ROLE_HEADING
  eNoRule,           // ROLE_PAGE
  eNoRule,           // ROLE_SECTION
  eNoRule,           // ROLE_REDUNDANT_OBJECT
  eNoRule,           // ROLE_FORM
  eNoRule,           // ROLE_IME
  eNoRule,           // ROLE_APP_ROOT
  eFromSubtree,      // ROLE_PARENT_MENUITEM
  eNoRule,           // ROLE_CALENDAR
  eNoRule,           // ROLE_COMBOBOX_LIST
  eFromSubtree,      // ROLE_COMBOBOX_OPTION
  eNoRule,           // ROLE_IMAGE_MAP
  eFromSubtree,      // ROLE_OPTION
  eFromSubtree,      // ROLE_RICH_OPTION
  eNoRule,           // ROLE_LISTBOX
  eNoRule            // ROLE_FLAT_EQUATION
};
