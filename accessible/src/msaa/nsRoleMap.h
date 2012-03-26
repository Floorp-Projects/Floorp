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
  // roles::NOTHING
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // roles::TITLEBAR
  { ROLE_SYSTEM_TITLEBAR, ROLE_SYSTEM_TITLEBAR },

  // roles::MENUBAR
  { ROLE_SYSTEM_MENUBAR, ROLE_SYSTEM_MENUBAR },

  // roles::SCROLLBAR
  { ROLE_SYSTEM_SCROLLBAR, ROLE_SYSTEM_SCROLLBAR },

  // roles::GRIP
  { ROLE_SYSTEM_GRIP, ROLE_SYSTEM_GRIP },

  // roles::SOUND
  { ROLE_SYSTEM_SOUND, ROLE_SYSTEM_SOUND },

  // roles::CURSOR
  { ROLE_SYSTEM_CURSOR, ROLE_SYSTEM_CURSOR },

  // roles::CARET
  { ROLE_SYSTEM_CARET, ROLE_SYSTEM_CARET },

  // roles::ALERT
  { ROLE_SYSTEM_ALERT, ROLE_SYSTEM_ALERT },

  // roles::WINDOW
  { ROLE_SYSTEM_WINDOW, ROLE_SYSTEM_WINDOW },

  // roles::INTERNAL_FRAME
  { USE_ROLE_STRING, IA2_ROLE_INTERNAL_FRAME},

  // roles::MENUPOPUP
  { ROLE_SYSTEM_MENUPOPUP, ROLE_SYSTEM_MENUPOPUP },

  // roles::MENUITEM
  { ROLE_SYSTEM_MENUITEM, ROLE_SYSTEM_MENUITEM },

  // roles::TOOLTIP
  { ROLE_SYSTEM_TOOLTIP, ROLE_SYSTEM_TOOLTIP },

  // roles::APPLICATION
  { ROLE_SYSTEM_APPLICATION, ROLE_SYSTEM_APPLICATION },

  // roles::DOCUMENT
  { ROLE_SYSTEM_DOCUMENT, ROLE_SYSTEM_DOCUMENT },

  // roles::PANE
  // We used to map to ROLE_SYSTEM_PANE, but JAWS would
  // not read the accessible name for the contaning pane.
  // However, JAWS will read the accessible name for a groupbox.
  // By mapping a PANE to a GROUPING, we get no undesirable effects,
  // but fortunately JAWS will then read the group's label,
  // when an inner control gets focused.
  { ROLE_SYSTEM_GROUPING , ROLE_SYSTEM_GROUPING }, 

  // roles::CHART
  { ROLE_SYSTEM_CHART, ROLE_SYSTEM_CHART },

  // roles::DIALOG
  { ROLE_SYSTEM_DIALOG, ROLE_SYSTEM_DIALOG },

  // roles::BORDER
  { ROLE_SYSTEM_BORDER, ROLE_SYSTEM_BORDER },

  // roles::GROUPING
  { ROLE_SYSTEM_GROUPING, ROLE_SYSTEM_GROUPING },

  // roles::SEPARATOR
  { ROLE_SYSTEM_SEPARATOR, ROLE_SYSTEM_SEPARATOR },

  // roles::TOOLBAR
  { ROLE_SYSTEM_TOOLBAR, ROLE_SYSTEM_TOOLBAR },

  // roles::STATUSBAR
  { ROLE_SYSTEM_STATUSBAR, ROLE_SYSTEM_STATUSBAR },

  // roles::TABLE
  { ROLE_SYSTEM_TABLE, ROLE_SYSTEM_TABLE },

  // roles::COLUMNHEADER,
  { ROLE_SYSTEM_COLUMNHEADER, ROLE_SYSTEM_COLUMNHEADER },

  // roles::ROWHEADER
  { ROLE_SYSTEM_ROWHEADER, ROLE_SYSTEM_ROWHEADER },

  // roles::COLUMN
  { ROLE_SYSTEM_COLUMN, ROLE_SYSTEM_COLUMN },

  // roles::ROW
  { ROLE_SYSTEM_ROW, ROLE_SYSTEM_ROW },

  // roles::CELL
  { ROLE_SYSTEM_CELL, ROLE_SYSTEM_CELL },

  // roles::LINK
  { ROLE_SYSTEM_LINK, ROLE_SYSTEM_LINK },

  // roles::HELPBALLOON
  { ROLE_SYSTEM_HELPBALLOON, ROLE_SYSTEM_HELPBALLOON },

  // roles::CHARACTER
  { ROLE_SYSTEM_CHARACTER, ROLE_SYSTEM_CHARACTER },

  // roles::LIST
  { ROLE_SYSTEM_LIST, ROLE_SYSTEM_LIST },

  // roles::LISTITEM
  { ROLE_SYSTEM_LISTITEM, ROLE_SYSTEM_LISTITEM },

  // roles::OUTLINE
  { ROLE_SYSTEM_OUTLINE, ROLE_SYSTEM_OUTLINE },

  // roles::OUTLINEITEM
  { ROLE_SYSTEM_OUTLINEITEM, ROLE_SYSTEM_OUTLINEITEM },

  // roles::PAGETAB
  { ROLE_SYSTEM_PAGETAB, ROLE_SYSTEM_PAGETAB },

  // roles::PROPERTYPAGE
  { ROLE_SYSTEM_PROPERTYPAGE, ROLE_SYSTEM_PROPERTYPAGE },

  // roles::INDICATOR
  { ROLE_SYSTEM_INDICATOR, ROLE_SYSTEM_INDICATOR },

  // roles::GRAPHIC
  { ROLE_SYSTEM_GRAPHIC, ROLE_SYSTEM_GRAPHIC },

  // roles::STATICTEXT
  { ROLE_SYSTEM_STATICTEXT, ROLE_SYSTEM_STATICTEXT },

  // roles::TEXT_LEAF
  { ROLE_SYSTEM_TEXT, ROLE_SYSTEM_TEXT },

  // roles::PUSHBUTTON
  { ROLE_SYSTEM_PUSHBUTTON, ROLE_SYSTEM_PUSHBUTTON },

  // roles::CHECKBUTTON
  { ROLE_SYSTEM_CHECKBUTTON, ROLE_SYSTEM_CHECKBUTTON },

  // roles::RADIOBUTTON
  { ROLE_SYSTEM_RADIOBUTTON, ROLE_SYSTEM_RADIOBUTTON },

  // roles::COMBOBOX
  { ROLE_SYSTEM_COMBOBOX, ROLE_SYSTEM_COMBOBOX },

  // roles::DROPLIST
  { ROLE_SYSTEM_DROPLIST, ROLE_SYSTEM_DROPLIST },

  // roles::PROGRESSBAR
  { ROLE_SYSTEM_PROGRESSBAR, ROLE_SYSTEM_PROGRESSBAR },

  // roles::DIAL
  { ROLE_SYSTEM_DIAL, ROLE_SYSTEM_DIAL },

  // roles::HOTKEYFIELD
  { ROLE_SYSTEM_HOTKEYFIELD, ROLE_SYSTEM_HOTKEYFIELD },

  // roles::SLIDER
  { ROLE_SYSTEM_SLIDER, ROLE_SYSTEM_SLIDER },

  // roles::SPINBUTTON
  { ROLE_SYSTEM_SPINBUTTON, ROLE_SYSTEM_SPINBUTTON },

  // roles::DIAGRAM
  { ROLE_SYSTEM_DIAGRAM, ROLE_SYSTEM_DIAGRAM },

  // roles::ANIMATION
  { ROLE_SYSTEM_ANIMATION, ROLE_SYSTEM_ANIMATION },

  // roles::EQUATION
  { ROLE_SYSTEM_EQUATION, ROLE_SYSTEM_EQUATION },

  // roles::BUTTONDROPDOWN
  { ROLE_SYSTEM_BUTTONDROPDOWN, ROLE_SYSTEM_BUTTONDROPDOWN },

  // roles::BUTTONMENU
  { ROLE_SYSTEM_BUTTONMENU, ROLE_SYSTEM_BUTTONMENU },

  // roles::BUTTONDROPDOWNGRID
  { ROLE_SYSTEM_BUTTONDROPDOWNGRID, ROLE_SYSTEM_BUTTONDROPDOWNGRID },

  // roles::WHITESPACE
  { ROLE_SYSTEM_WHITESPACE, ROLE_SYSTEM_WHITESPACE },

  // roles::PAGETABLIST
  { ROLE_SYSTEM_PAGETABLIST, ROLE_SYSTEM_PAGETABLIST },

  // roles::CLOCK
  { ROLE_SYSTEM_CLOCK, ROLE_SYSTEM_CLOCK },

  // roles::SPLITBUTTON
  { ROLE_SYSTEM_SPLITBUTTON, ROLE_SYSTEM_SPLITBUTTON },

  // roles::IPADDRESS
  { ROLE_SYSTEM_IPADDRESS, ROLE_SYSTEM_IPADDRESS },

  // Make up for Gecko roles that we don't have in MSAA or IA2. When in doubt
  // map them to USE_ROLE_STRING (IA2_ROLE_UNKNOWN).

  // roles::ACCEL_LABEL
  { ROLE_SYSTEM_STATICTEXT, ROLE_SYSTEM_STATICTEXT },

  // roles::ARROW
  { ROLE_SYSTEM_INDICATOR, ROLE_SYSTEM_INDICATOR },

  // roles::CANVAS
  { USE_ROLE_STRING, IA2_ROLE_CANVAS },

  // roles::CHECK_MENU_ITEM
  { ROLE_SYSTEM_MENUITEM, IA2_ROLE_CHECK_MENU_ITEM },

  // roles::COLOR_CHOOSER
  { ROLE_SYSTEM_DIALOG, IA2_ROLE_COLOR_CHOOSER },

  // roles::DATE_EDITOR
  { USE_ROLE_STRING, IA2_ROLE_DATE_EDITOR },

  // roles::DESKTOP_ICON
  { USE_ROLE_STRING, IA2_ROLE_DESKTOP_ICON },

  // roles::DESKTOP_FRAME
  { USE_ROLE_STRING, IA2_ROLE_DESKTOP_PANE },

  // roles::DIRECTORY_PANE
  { USE_ROLE_STRING, IA2_ROLE_DIRECTORY_PANE },

  // roles::FILE_CHOOSER
  { USE_ROLE_STRING, IA2_ROLE_FILE_CHOOSER },

  // roles::FONT_CHOOSER
  { USE_ROLE_STRING, IA2_ROLE_FONT_CHOOSER },

  // roles::CHROME_WINDOW
  { ROLE_SYSTEM_APPLICATION, IA2_ROLE_FRAME },

  // roles::GLASS_PANE
  { USE_ROLE_STRING, IA2_ROLE_GLASS_PANE },

  // roles::HTML_CONTAINER
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // roles::ICON
  { ROLE_SYSTEM_PUSHBUTTON, IA2_ROLE_ICON },

  // roles::LABEL
  { ROLE_SYSTEM_STATICTEXT, IA2_ROLE_LABEL },

  // roles::LAYERED_PANE
  { USE_ROLE_STRING, IA2_ROLE_LAYERED_PANE },

  // roles::OPTION_PANE
  { USE_ROLE_STRING, IA2_ROLE_OPTION_PANE },

  // roles::PASSWORD_TEXT
  { ROLE_SYSTEM_TEXT, ROLE_SYSTEM_TEXT },

  // roles::POPUP_MENU
  { ROLE_SYSTEM_MENUPOPUP, ROLE_SYSTEM_MENUPOPUP },

  // roles::RADIO_MENU_ITEM
  { ROLE_SYSTEM_MENUITEM, IA2_ROLE_RADIO_MENU_ITEM },

  // roles::ROOT_PANE
  { USE_ROLE_STRING, IA2_ROLE_ROOT_PANE },

  // roles::SCROLL_PANE
  { USE_ROLE_STRING, IA2_ROLE_SCROLL_PANE },

  // roles::SPLIT_PANE
  { USE_ROLE_STRING, IA2_ROLE_SPLIT_PANE },

  // roles::TABLE_COLUMN_HEADER
  { ROLE_SYSTEM_COLUMNHEADER, ROLE_SYSTEM_COLUMNHEADER },

  // roles::TABLE_ROW_HEADER
  { ROLE_SYSTEM_ROWHEADER, ROLE_SYSTEM_ROWHEADER },

  // roles::TEAR_OFF_MENU_ITEM
  { ROLE_SYSTEM_MENUITEM, IA2_ROLE_TEAR_OFF_MENU },

  // roles::TERMINAL
  { USE_ROLE_STRING, IA2_ROLE_TERMINAL },

  // roles::TEXT_CONTAINER
  { USE_ROLE_STRING, IA2_ROLE_TEXT_FRAME },

  // roles::TOGGLE_BUTTON
  { ROLE_SYSTEM_PUSHBUTTON, IA2_ROLE_TOGGLE_BUTTON },

  // roles::TREE_TABLE
  { ROLE_SYSTEM_OUTLINE, ROLE_SYSTEM_OUTLINE },

  // roles::VIEWPORT
  { ROLE_SYSTEM_PANE, IA2_ROLE_VIEW_PORT },

  // roles::HEADER
  { USE_ROLE_STRING, IA2_ROLE_HEADER },

  // roles::FOOTER
  { USE_ROLE_STRING, IA2_ROLE_FOOTER },

  // roles::PARAGRAPH
  { USE_ROLE_STRING, IA2_ROLE_PARAGRAPH },

  // roles::RULER
  { USE_ROLE_STRING, IA2_ROLE_RULER },

  // roles::AUTOCOMPLETE
  { ROLE_SYSTEM_COMBOBOX, ROLE_SYSTEM_COMBOBOX },

  // roles::EDITBAR
  { ROLE_SYSTEM_TEXT, IA2_ROLE_EDITBAR },

  // roles::ENTRY
  { ROLE_SYSTEM_TEXT, ROLE_SYSTEM_TEXT },

  // roles::CAPTION
  { USE_ROLE_STRING, IA2_ROLE_CAPTION },

  // roles::DOCUMENT_FRAME
  { USE_ROLE_STRING, IA2_ROLE_UNKNOWN },

  // roles::HEADING
  { USE_ROLE_STRING, IA2_ROLE_HEADING },

  // roles::PAGE
  { USE_ROLE_STRING, IA2_ROLE_PAGE },

  // roles::SECTION
  { USE_ROLE_STRING, IA2_ROLE_SECTION },

  // roles::REDUNDANT_OBJECT
  { USE_ROLE_STRING, IA2_ROLE_REDUNDANT_OBJECT },

  // roles::FORM
  { USE_ROLE_STRING, IA2_ROLE_FORM },

  // roles::IME
  { USE_ROLE_STRING, IA2_ROLE_INPUT_METHOD_WINDOW },

  // roles::APP_ROOT
  { ROLE_SYSTEM_APPLICATION, ROLE_SYSTEM_APPLICATION },

  // roles::PARENT_MENUITEM
  { ROLE_SYSTEM_MENUITEM, ROLE_SYSTEM_MENUITEM },

  // roles::CALENDAR
  { ROLE_SYSTEM_CLIENT, ROLE_SYSTEM_CLIENT },

  // roles::COMBOBOX_LIST
  { ROLE_SYSTEM_LIST, ROLE_SYSTEM_LIST },

  // roles::COMBOBOX_OPTION
  { ROLE_SYSTEM_LISTITEM, ROLE_SYSTEM_LISTITEM },

  // roles::IMAGE_MAP
  { ROLE_SYSTEM_GRAPHIC, ROLE_SYSTEM_GRAPHIC },

  // roles::OPTION 
  { ROLE_SYSTEM_LISTITEM, ROLE_SYSTEM_LISTITEM },
  
  // roles::RICH_OPTION
  { ROLE_SYSTEM_LISTITEM, ROLE_SYSTEM_LISTITEM },
  
  // roles::LISTBOX
  { ROLE_SYSTEM_LIST, ROLE_SYSTEM_LIST },
  
  // roles::FLAT_EQUATION
  { ROLE_SYSTEM_EQUATION, ROLE_SYSTEM_EQUATION },
  
  // roles::GRID_CELL
  { ROLE_SYSTEM_CELL, ROLE_SYSTEM_CELL },

  // roles::EMBEDDED_OBJECT
  { USE_ROLE_STRING, IA2_ROLE_EMBEDDED_OBJECT },

  // roles::NOTE
  { USE_ROLE_STRING, IA2_ROLE_NOTE },

  // roles::FIGURE
  { ROLE_SYSTEM_GROUPING, ROLE_SYSTEM_GROUPING },

  // roles::CHECK_RICH_OPTION
  { ROLE_SYSTEM_CHECKBUTTON, ROLE_SYSTEM_CHECKBUTTON },

  // roles::LAST_ENTRY
  { ROLE_WINDOWS_LAST_ENTRY, ROLE_WINDOWS_LAST_ENTRY }
};

