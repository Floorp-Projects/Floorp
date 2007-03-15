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
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gao, Ming <gaoming@cn.ibm.com>
 *   Aaron Leventhal <aleventh@us.ibm.com>
 *   Alexander Surkov <surkov.alexander@gmail.com>
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

#include "OLEACC.H"
#include "AccessibleRole.h"

const PRUint32 USE_ROLE_STRING = 0;
const PRUint32 ROLE_WINDOWS_LAST_ENTRY = 0xffffffff;

#ifndef ROLE_SYSTEM_SPLITBUTTON
const PRUint32 ROLE_SYSTEM_SPLITBUTTON  = 0x3e; // Not defined in all oleacc.h versions
#endif

#ifndef ROLE_SYSTEM_IPADDRESS
const PRUint32 ROLE_SYSTEM_IPADDRESS = 0x3f; // Not defined in all oleacc.h versions
#endif

#ifndef ROLE_SYSTEM_OUTLINEBUTTON
const PRUint32 ROLE_SYSTEM_OUTLINEBUTTON = 0x40; // Not defined in all oleacc.h versions
#endif

struct WindowsRoleMapItem
{
  PRUint32 msaaRole;
  long ia2Role;
};

// Map array from cross platform roles to MSAA/IA2 roles
static const WindowsRoleMapItem gWindowsRoleMap[] = {
  // nsIAccessibleRole::ROLE_NOTHING
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // nsIAccessibleRole::ROLE_TITLEBAR
  { ROLE_SYSTEM_TITLEBAR, ROLE_SYSTEM_TITLEBAR },

  // nsIAccessibleRole::ROLE_MENUBAR
  { ROLE_SYSTEM_MENUBAR, ROLE_SYSTEM_MENUBAR },

  // nsIAccessibleRole::ROLE_SCROLLBAR
  { ROLE_SYSTEM_SCROLLBAR, ROLE_SYSTEM_SCROLLBAR },

  // nsIAccessibleRole::ROLE_GRIP
  { ROLE_SYSTEM_GRIP, ROLE_SYSTEM_GRIP },

  // nsIAccessibleRole::ROLE_SOUND
  { ROLE_SYSTEM_SOUND, ROLE_SYSTEM_SOUND },

  // nsIAccessibleRole::ROLE_CURSOR
  { ROLE_SYSTEM_CURSOR, ROLE_SYSTEM_CURSOR },

  // nsIAccessibleRole::ROLE_CARET
  { ROLE_SYSTEM_CARET, ROLE_SYSTEM_CARET },

  // nsIAccessibleRole::ROLE_ALERT
  { ROLE_SYSTEM_ALERT, ROLE_SYSTEM_ALERT },

  // nsIAccessibleRole::ROLE_WINDOW
  { ROLE_SYSTEM_WINDOW, ROLE_SYSTEM_WINDOW },

  // nsIAccessibleRole::ROLE_CLIENT
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN},

  // nsIAccessibleRole::ROLE_MENUPOPUP
  { ROLE_SYSTEM_MENUPOPUP, ROLE_SYSTEM_MENUPOPUP },

  // nsIAccessibleRole::ROLE_MENUITEM
  { ROLE_SYSTEM_MENUITEM, ROLE_SYSTEM_MENUITEM },

  // nsIAccessibleRole::ROLE_TOOLTIP
  { ROLE_SYSTEM_TOOLTIP, ROLE_SYSTEM_TOOLTIP },

  // nsIAccessibleRole::ROLE_APPLICATION
  { ROLE_SYSTEM_APPLICATION, ROLE_SYSTEM_APPLICATION },

  // nsIAccessibleRole::ROLE_DOCUMENT
  { ROLE_SYSTEM_DOCUMENT, ROLE_SYSTEM_DOCUMENT },

  // nsIAccessibleRole::ROLE_PANE
  { ROLE_SYSTEM_PANE, ROLE_SYSTEM_PANE },

  // nsIAccessibleRole::ROLE_CHART
  { ROLE_SYSTEM_CHART, ROLE_SYSTEM_CHART },

  // nsIAccessibleRole::ROLE_DIALOG
  { ROLE_SYSTEM_DIALOG, ROLE_SYSTEM_DIALOG },

  // nsIAccessibleRole::ROLE_BORDER
  { ROLE_SYSTEM_BORDER, ROLE_SYSTEM_BORDER },

  // nsIAccessibleRole::ROLE_GROUPING
  { ROLE_SYSTEM_GROUPING, ROLE_SYSTEM_GROUPING },

  // nsIAccessibleRole::ROLE_SEPARATOR
  { ROLE_SYSTEM_SEPARATOR, ROLE_SYSTEM_SEPARATOR },

  // nsIAccessibleRole::ROLE_TOOLBAR
  { ROLE_SYSTEM_TOOLBAR, ROLE_SYSTEM_TOOLBAR },

  // nsIAccessibleRole::ROLE_STATUSBAR
  { ROLE_SYSTEM_STATUSBAR, ROLE_SYSTEM_STATUSBAR },

  // nsIAccessibleRole::ROLE_TABLE
  { ROLE_SYSTEM_TABLE, ROLE_SYSTEM_TABLE },

  // nsIAccessibleRole::ROLE_COLUMNHEADER,
  { ROLE_SYSTEM_COLUMNHEADER, ROLE_SYSTEM_COLUMNHEADER },

  // nsIAccessibleRole::ROLE_ROWHEADER
  { ROLE_SYSTEM_ROWHEADER, ROLE_SYSTEM_ROWHEADER },

  // nsIAccessibleRole::ROLE_COLUMN
  { ROLE_SYSTEM_COLUMN, ROLE_SYSTEM_COLUMN },

  // nsIAccessibleRole::ROLE_ROW
  { ROLE_SYSTEM_ROW, ROLE_SYSTEM_ROW },

  // nsIAccessibleRole::ROLE_CELL
  { ROLE_SYSTEM_CELL, ROLE_SYSTEM_CELL },

  // nsIAccessibleRole::ROLE_LINK
  { ROLE_SYSTEM_LINK, ROLE_SYSTEM_LINK },

  // nsIAccessibleRole::ROLE_HELPBALLOON
  { ROLE_SYSTEM_HELPBALLOON, ROLE_SYSTEM_HELPBALLOON },

  // nsIAccessibleRole::ROLE_CHARACTER
  { ROLE_SYSTEM_CHARACTER, ROLE_SYSTEM_CHARACTER },

  // nsIAccessibleRole::ROLE_LIST
  { ROLE_SYSTEM_LIST, ROLE_SYSTEM_LIST },

  // nsIAccessibleRole::ROLE_LISTITEM
  { ROLE_SYSTEM_LISTITEM, ROLE_SYSTEM_LISTITEM },

  // nsIAccessibleRole::ROLE_OUTLINE
  { ROLE_SYSTEM_OUTLINE, ROLE_SYSTEM_OUTLINE },

  // nsIAccessibleRole::ROLE_OUTLINEITEM
  { ROLE_SYSTEM_OUTLINEITEM, ROLE_SYSTEM_OUTLINEITEM },

  // nsIAccessibleRole::ROLE_PAGETAB
  { ROLE_SYSTEM_PAGETAB, ROLE_SYSTEM_PAGETAB },

  // nsIAccessibleRole::ROLE_PROPERTYPAGE
  { ROLE_SYSTEM_PROPERTYPAGE, ROLE_SYSTEM_PROPERTYPAGE },

  // nsIAccessibleRole::ROLE_INDICATOR
  { ROLE_SYSTEM_INDICATOR, ROLE_SYSTEM_INDICATOR },

  // nsIAccessibleRole::ROLE_GRAPHIC
  { ROLE_SYSTEM_GRAPHIC, ROLE_SYSTEM_GRAPHIC },

  // nsIAccessibleRole::ROLE_STATICTEXT
  { ROLE_SYSTEM_STATICTEXT, ROLE_SYSTEM_STATICTEXT },

  // nsIAccessibleRole::ROLE_TEXT_LEAF
  { ROLE_SYSTEM_TEXT, ROLE_SYSTEM_TEXT },

  // nsIAccessibleRole::ROLE_PUSHBUTTON
  { ROLE_SYSTEM_PUSHBUTTON, ROLE_SYSTEM_PUSHBUTTON },

  // nsIAccessibleRole::ROLE_CHECKBUTTON
  { ROLE_SYSTEM_CHECKBUTTON, ROLE_SYSTEM_CHECKBUTTON },

  // nsIAccessibleRole::ROLE_RADIOBUTTON
  { ROLE_SYSTEM_RADIOBUTTON, ROLE_SYSTEM_RADIOBUTTON },

  // nsIAccessibleRole::ROLE_COMBOBOX
  { ROLE_SYSTEM_COMBOBOX, ROLE_SYSTEM_COMBOBOX },

  // nsIAccessibleRole::ROLE_DROPLIST
  { ROLE_SYSTEM_DROPLIST, ROLE_SYSTEM_DROPLIST },

  // nsIAccessibleRole::ROLE_PROGRESSBAR
  { ROLE_SYSTEM_PROGRESSBAR, ROLE_SYSTEM_PROGRESSBAR },

  // nsIAccessibleRole::ROLE_DIAL
  { ROLE_SYSTEM_DIAL, ROLE_SYSTEM_DIAL },

  // nsIAccessibleRole::ROLE_HOTKEYFIELD
  { ROLE_SYSTEM_HOTKEYFIELD, ROLE_SYSTEM_HOTKEYFIELD },

  // nsIAccessibleRole::ROLE_SLIDER
  { ROLE_SYSTEM_SLIDER, ROLE_SYSTEM_SLIDER },

  // nsIAccessibleRole::ROLE_SPINBUTTON
  { ROLE_SYSTEM_SPINBUTTON, ROLE_SYSTEM_SPINBUTTON },

  // nsIAccessibleRole::ROLE_DIAGRAM
  { ROLE_SYSTEM_DIAGRAM, ROLE_SYSTEM_DIAGRAM },

  // nsIAccessibleRole::ROLE_ANIMATION
  { ROLE_SYSTEM_ANIMATION, ROLE_SYSTEM_ANIMATION },

  // nsIAccessibleRole::ROLE_EQUATION
  { ROLE_SYSTEM_EQUATION, ROLE_SYSTEM_EQUATION },

  // nsIAccessibleRole::ROLE_BUTTONDROPDOWN
  { ROLE_SYSTEM_BUTTONDROPDOWN, ROLE_SYSTEM_BUTTONDROPDOWN },

  // nsIAccessibleRole::ROLE_BUTTONMENU
  { ROLE_SYSTEM_BUTTONMENU, ROLE_SYSTEM_BUTTONMENU },

  // nsIAccessibleRole::ROLE_BUTTONDROPDOWNGRID
  { ROLE_SYSTEM_BUTTONDROPDOWNGRID, ROLE_SYSTEM_BUTTONDROPDOWNGRID },

  // nsIAccessibleRole::ROLE_WHITESPACE
  { ROLE_SYSTEM_WHITESPACE, ROLE_SYSTEM_WHITESPACE },

  // nsIAccessibleRole::ROLE_PAGETABLIST
  { ROLE_SYSTEM_PAGETABLIST, ROLE_SYSTEM_PAGETABLIST },

  // nsIAccessibleRole::ROLE_CLOCK
  { ROLE_SYSTEM_CLOCK, ROLE_SYSTEM_CLOCK },

  // nsIAccessibleRole::ROLE_SPLITBUTTON
  { ROLE_SYSTEM_SPLITBUTTON, ROLE_SYSTEM_SPLITBUTTON },

  // nsIAccessibleRole::ROLE_IPADDRESS
  { ROLE_SYSTEM_IPADDRESS, ROLE_SYSTEM_IPADDRESS },

  // Make up for Gecko roles that we don't have in MSAA or IA2. When in doubt
  // map them to USE_ROLE_STRING (IA2_ROLE_UNKNOWN).

  // nsIAccessibleRole::ROLE_ACCEL_LABEL
  { ROLE_SYSTEM_STATICTEXT, ROLE_SYSTEM_STATICTEXT },

  // nsIAccessibleRole::ROLE_ARROW
  { ROLE_SYSTEM_INDICATOR, ROLE_SYSTEM_INDICATOR },

  // nsIAccessibleRole::ROLE_CANVAS
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // nsIAccessibleRole::ROLE_CHECK_MENU_ITEM
  { ROLE_SYSTEM_MENUITEM, IA2_ROLE_CHECK_MENU_ITEM },

  // nsIAccessibleRole::ROLE_COLOR_CHOOSER
  { ROLE_SYSTEM_DIALOG, IA2_ROLE_COLOR_CHOOSER },

  // nsIAccessibleRole::ROLE_DATE_EDITOR
  { USE_ROLE_STRING, IA2_ROLE_DATE_EDITOR },

  // nsIAccessibleRole::ROLE_DESKTOP_ICON
  { USE_ROLE_STRING, IA2_ROLE_DESKTOP_ICON },

  // nsIAccessibleRole::ROLE_DESKTOP_FRAME
  { USE_ROLE_STRING, IA2_ROLE_DESKTOP_PANE },

  // nsIAccessibleRole::ROLE_DIRECTORY_PANE
  { USE_ROLE_STRING, IA2_ROLE_DIRECTORY_PANE },

  // nsIAccessibleRole::ROLE_FILE_CHOOSER
  { USE_ROLE_STRING, IA2_ROLE_FILE_CHOOSER },

  // nsIAccessibleRole::ROLE_FONT_CHOOSER
  { USE_ROLE_STRING, IA2_ROLE_FONT_CHOOSER },

  // nsIAccessibleRole::ROLE_CHROME_WINDOW
  { ROLE_SYSTEM_APPLICATION, IA2_ROLE_FRAME },

  // nsIAccessibleRole::ROLE_GLASS_PANE
  { USE_ROLE_STRING, IA2_ROLE_GLASS_PANE },

  // nsIAccessibleRole::ROLE_HTML_CONTAINER
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // nsIAccessibleRole::ROLE_ICON
  { ROLE_SYSTEM_PUSHBUTTON, IA2_ROLE_ICON },

  // nsIAccessibleRole::ROLE_LABEL
  { ROLE_SYSTEM_STATICTEXT, IA2_ROLE_LABEL },

  // nsIAccessibleRole::ROLE_LAYERED_PANE
  { USE_ROLE_STRING, IA2_ROLE_LAYERED_PANE },

  // nsIAccessibleRole::ROLE_OPTION_PANE
  { USE_ROLE_STRING, IA2_ROLE_OPTION_PANE },

  // nsIAccessibleRole::ROLE_PASSWORD_TEXT
  { ROLE_SYSTEM_TEXT, ROLE_SYSTEM_TEXT },

  // nsIAccessibleRole::ROLE_POPUP_MENU
  { ROLE_SYSTEM_MENUPOPUP, ROLE_SYSTEM_MENUPOPUP },

  // nsIAccessibleRole::ROLE_RADIO_MENU_ITEM
  { ROLE_SYSTEM_MENUITEM, IA2_ROLE_RADIO_MENU_ITEM },

  // nsIAccessibleRole::ROLE_ROOT_PANE
  { USE_ROLE_STRING, IA2_ROLE_ROOT_PANE },

  // nsIAccessibleRole::ROLE_SCROLL_PANE
  { USE_ROLE_STRING, IA2_ROLE_SCROLL_PANE },

  // nsIAccessibleRole::ROLE_SPLIT_PANE
  { USE_ROLE_STRING, IA2_ROLE_SPLIT_PANE },

  // nsIAccessibleRole::ROLE_TABLE_COLUMN_HEADER
  { ROLE_SYSTEM_COLUMNHEADER, ROLE_SYSTEM_COLUMNHEADER },

  // nsIAccessibleRole::ROLE_TABLE_ROW_HEADER
  { ROLE_SYSTEM_ROWHEADER, ROLE_SYSTEM_ROWHEADER },

  // nsIAccessibleRole::ROLE_TEAR_OFF_MENU_ITEM
  { ROLE_SYSTEM_MENUITEM, ROLE_SYSTEM_MENUITEM },

  // nsIAccessibleRole::ROLE_TERMINAL
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // nsIAccessibleRole::ROLE_TEXT_CONTAINER
  { USE_ROLE_STRING, IA2_ROLE_TEXT_FRAME },

  // nsIAccessibleRole::ROLE_TOGGLE_BUTTON
  { USE_ROLE_STRING, IA2_ROLE_TOGGLE_BUTTON },

  // nsIAccessibleRole::ROLE_TREE_TABLE
  { ROLE_SYSTEM_OUTLINE, ROLE_SYSTEM_OUTLINE },

  // nsIAccessibleRole::ROLE_VIEWPORT
  { ROLE_SYSTEM_PANE, IA2_ROLE_VIEW_PORT },

  // nsIAccessibleRole::ROLE_HEADER
  { USE_ROLE_STRING, IA2_ROLE_HEADER },

  // nsIAccessibleRole::ROLE_FOOTER
  { USE_ROLE_STRING, IA2_ROLE_FOOTER },

  // nsIAccessibleRole::ROLE_PARAGRAPH
  { USE_ROLE_STRING, IA2_ROLE_PARAGRAPH },

  // nsIAccessibleRole::ROLE_RULER
  { USE_ROLE_STRING, IA2_ROLE_RULER },

  // nsIAccessibleRole::ROLE_AUTOCOMPLETE
  { ROLE_SYSTEM_COMBOBOX, ROLE_SYSTEM_COMBOBOX },

  // nsIAccessibleRole::ROLE_EDITBAR
  { ROLE_SYSTEM_TEXT, IA2_ROLE_EDITBAR },

  // nsIAccessibleRole::ROLE_ENTRY
  { ROLE_SYSTEM_TEXT, ROLE_SYSTEM_TEXT },

  // nsIAccessibleRole::ROLE_CAPTION
  { USE_ROLE_STRING, IA2_ROLE_CAPTION },

  // nsIAccessibleRole::ROLE_DOCUMENT_FRAME
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // nsIAccessibleRole::ROLE_HEADING
  { USE_ROLE_STRING, IA2_ROLE_HEADING },

  // nsIAccessibleRole::ROLE_PAGE
  { USE_ROLE_STRING, IA2_ROLE_PAGE },

  // nsIAccessibleRole::ROLE_SECTION
  { USE_ROLE_STRING, IA2_ROLE_SECTION },

  // nsIAccessibleRole::ROLE_REDUNDANT_OBJECT
  { USE_ROLE_STRING, IA2_ROLE_REDUNDANT_OBJECT },

  // nsIAccessibleRole::ROLE_FORM
  { USE_ROLE_STRING, IA2_ROLE_FORM },

  // nsIAccessibleRole::ROLE_IME
  { USE_ROLE_STRING, IA2_ROLE_INPUT_METHOD_WINDOW },

  // nsIAccessibleRole::ROLE_APP_ROOT
  { ROLE_SYSTEM_APPLICATION, ROLE_SYSTEM_APPLICATION },

  // nsIAccessibleRole::ROLE_PARENT_MENUITEM
  { ROLE_SYSTEM_MENUITEM, ROLE_SYSTEM_MENUITEM },

  // nsIAccessibleRole::ROLE_CALENDAR
  { ROLE_SYSTEM_CLIENT, ROLE_SYSTEM_CLIENT },

  // nsIAccessibleRole::ROLE_COMBOBOX_LIST
  { ROLE_SYSTEM_LIST, ROLE_SYSTEM_LIST },

  // nsIAccessibleRole::ROLE_COMBOBOX_LISTITEM
  { ROLE_SYSTEM_LISTITEM, ROLE_SYSTEM_LISTITEM },

  // nsIAccessibleRole::ROLE_LAST_ENTRY
  { ROLE_WINDOWS_LAST_ENTRY, ROLE_WINDOWS_LAST_ENTRY }
};

