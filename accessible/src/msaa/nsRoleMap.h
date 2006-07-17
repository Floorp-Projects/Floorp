/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
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
 *   Gao, Ming (gaoming@cn.ibm.com)
 *   Aaron Leventhal (aleventh@us.ibm.com)
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

#define USE_ROLE_STRING 0
#define ROLE_MSAA_LAST_ENTRY -1

#ifndef ROLE_SYSTEM_SPLITBUTTON
#define ROLE_SYSTEM_SPLITBUTTON ( 0x3e )    // Not defined in all oleacc.h versions
#endif

#ifndef ROLE_SYSTEM_IPADDRESS
#define ROLE_SYSTEM_IPADDRESS ( 0x3f )      // Not defined in all oleacc.h versions
#endif

#ifndef ROLE_SYSTEM_OUTLINEBUTTON
#define ROLE_SYSTEM_OUTLINEBUTTON ( 0x40 )  // Not defined in all oleacc.h versions
#endif

// Map array from cross platform roles to MSAA roles
PRUint32 msaaRoleMap[] = {
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_NOTHING
    ROLE_SYSTEM_TITLEBAR,            // nsIAccessible::ROLE_TITLEBAR
    ROLE_SYSTEM_MENUBAR,             // nsIAccessible::ROLE_MENUBAR
    ROLE_SYSTEM_SCROLLBAR,           // nsIAccessible::ROLE_SCROLLBAR
    ROLE_SYSTEM_GRIP,                // nsIAccessible::ROLE_GRIP
    ROLE_SYSTEM_SOUND,               // nsIAccessible::ROLE_SOUND
    ROLE_SYSTEM_CURSOR,              // nsIAccessible::ROLE_CURSOR
    ROLE_SYSTEM_CARET,               // nsIAccessible::ROLE_CARET
    ROLE_SYSTEM_ALERT,               // nsIAccessible::ROLE_ALERT
    ROLE_SYSTEM_WINDOW,              // nsIAccessible::ROLE_WINDOW
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_CLIENT
    ROLE_SYSTEM_MENUPOPUP,           // nsIAccessible::ROLE_MENUPOPUP
    ROLE_SYSTEM_MENUITEM,            // nsIAccessible::ROLE_MENUITEM
    ROLE_SYSTEM_TOOLTIP,             // nsIAccessible::ROLE_TOOLTIP
    ROLE_SYSTEM_APPLICATION,         // nsIAccessible::ROLE_APPLICATION
    ROLE_SYSTEM_DOCUMENT,            // nsIAccessible::ROLE_DOCUMENT
    ROLE_SYSTEM_PANE,                // nsIAccessible::ROLE_PANE
    ROLE_SYSTEM_CHART,               // nsIAccessible::ROLE_CHART
    ROLE_SYSTEM_DIALOG,              // nsIAccessible::ROLE_DIALOG
    ROLE_SYSTEM_BORDER,              // nsIAccessible::ROLE_BORDER
    ROLE_SYSTEM_GROUPING,            // nsIAccessible::ROLE_GROUPING
    ROLE_SYSTEM_SEPARATOR,           // nsIAccessible::ROLE_SEPARATOR
    ROLE_SYSTEM_TOOLBAR,             // nsIAccessible::ROLE_TOOLBAR
    ROLE_SYSTEM_STATUSBAR,           // nsIAccessible::ROLE_STATUSBAR
    ROLE_SYSTEM_TABLE,               // nsIAccessible::ROLE_TABLE
    ROLE_SYSTEM_COLUMNHEADER,        // nsIAccessible::ROLE_COLUMNHEADER,
    ROLE_SYSTEM_ROWHEADER,           // nsIAccessible::ROLE_ROWHEADER
    ROLE_SYSTEM_COLUMN,              // nsIAccessible::ROLE_COLUMN
    ROLE_SYSTEM_ROW,                 // nsIAccessible::ROLE_ROW
    ROLE_SYSTEM_CELL,                // nsIAccessible::ROLE_CELL
    ROLE_SYSTEM_LINK,                // nsIAccessible::ROLE_LINK
    ROLE_SYSTEM_HELPBALLOON,         // nsIAccessible::ROLE_HELPBALLOON
    ROLE_SYSTEM_CHARACTER,           // nsIAccessible::ROLE_CHARACTER
    ROLE_SYSTEM_LIST,                // nsIAccessible::ROLE_LIST
    ROLE_SYSTEM_LISTITEM,            // nsIAccessible::ROLE_LISTITEM
    ROLE_SYSTEM_OUTLINE,             // nsIAccessible::ROLE_OUTLINE
    ROLE_SYSTEM_OUTLINEITEM,         // nsIAccessible::ROLE_OUTLINEITEM
    ROLE_SYSTEM_PAGETAB,             // nsIAccessible::ROLE_PAGETAB
    ROLE_SYSTEM_PROPERTYPAGE,        // nsIAccessible::ROLE_PROPERTYPAGE
    ROLE_SYSTEM_INDICATOR,           // nsIAccessible::ROLE_INDICATOR
    ROLE_SYSTEM_GRAPHIC,             // nsIAccessible::ROLE_GRAPHIC
    ROLE_SYSTEM_STATICTEXT,          // nsIAccessible::ROLE_STATICTEXT
    ROLE_SYSTEM_TEXT,                // nsIAccessible::ROLE_TEXT_LEAF
    ROLE_SYSTEM_PUSHBUTTON,          // nsIAccessible::ROLE_PUSHBUTTON
    ROLE_SYSTEM_CHECKBUTTON,         // nsIAccessible::ROLE_CHECKBUTTON
    ROLE_SYSTEM_RADIOBUTTON,         // nsIAccessible::ROLE_RADIOBUTTON
    ROLE_SYSTEM_COMBOBOX,            // nsIAccessible::ROLE_COMBOBOX
    ROLE_SYSTEM_DROPLIST,            // nsIAccessible::ROLE_DROPLIST
    ROLE_SYSTEM_PROGRESSBAR,         // nsIAccessible::ROLE_PROGRESSBAR
    ROLE_SYSTEM_DIAL,                // nsIAccessible::ROLE_DIAL
    ROLE_SYSTEM_HOTKEYFIELD,         // nsIAccessible::ROLE_HOTKEYFIELD
    ROLE_SYSTEM_SLIDER,              // nsIAccessible::ROLE_SLIDER
    ROLE_SYSTEM_SPINBUTTON,          // nsIAccessible::ROLE_SPINBUTTON
    ROLE_SYSTEM_DIAGRAM,             // nsIAccessible::ROLE_DIAGRAM
    ROLE_SYSTEM_ANIMATION,           // nsIAccessible::ROLE_ANIMATION
    ROLE_SYSTEM_EQUATION,            // nsIAccessible::ROLE_EQUATION
    ROLE_SYSTEM_BUTTONDROPDOWN,      // nsIAccessible::ROLE_BUTTONDROPDOWN
    ROLE_SYSTEM_BUTTONMENU,          // nsIAccessible::ROLE_BUTTONMENU
    ROLE_SYSTEM_BUTTONDROPDOWNGRID,  // nsIAccessible::ROLE_BUTTONDROPDOWNGRID
    ROLE_SYSTEM_WHITESPACE,          // nsIAccessible::ROLE_WHITESPACE
    ROLE_SYSTEM_PAGETABLIST,         // nsIAccessible::ROLE_PAGETABLIST
    ROLE_SYSTEM_CLOCK,               // nsIAccessible::ROLE_CLOCK
    ROLE_SYSTEM_SPLITBUTTON,         // nsIAccessible::ROLE_SPLITBUTTON
    ROLE_SYSTEM_IPADDRESS,           // nsIAccessible::ROLE_IPADDRESS
    // Make up for ATK roles that we don't have in MSAA. When in doubt 
    // map them to USE_ROLE_STRING so that the role string is exposed
    ROLE_SYSTEM_STATICTEXT,          // nsIAccessible::ROLE_ACCEL_LABEL
    ROLE_SYSTEM_INDICATOR,           // nsIAccessible::ROLE_ARROW
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_CANVAS
    ROLE_SYSTEM_MENUITEM,            // nsIAccessible::ROLE_CHECK_MENU_ITEM
    ROLE_SYSTEM_DIALOG,              // nsIAccessible::ROLE_COLOR_CHOOSER
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_DATE_EDITOR
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_DESKTOP_ICON
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_DESKTOP_FRAME
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_DIRECTORY_PANE
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_FILE_CHOOSER
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_FILLER
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_FONT_CHOOSER
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_FRAME
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_GLASS_PANE
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_HTML_CONTAINER
    ROLE_SYSTEM_PUSHBUTTON,          // nsIAccessible::ROLE_ICON
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_INTERNAL_FRAME
    ROLE_SYSTEM_STATICTEXT,          // nsIAccessible::ROLE_LABEL
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_LAYERED_PANE
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_OPTION_PANE
    ROLE_SYSTEM_TEXT,                // nsIAccessible::ROLE_PASSWORD_TEXT
    ROLE_SYSTEM_MENUPOPUP,           // nsIAccessible::ROLE_POPUP_MENU
    ROLE_SYSTEM_MENUITEM,            // nsIAccessible::ROLE_RADIO_MENU_ITEM
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_ROOT_PANE
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_SCROLL_PANE
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_SPLIT_PANE
    ROLE_SYSTEM_COLUMNHEADER,        // nsIAccessible::ROLE_TABLE_COLUMN_HEADER
    ROLE_SYSTEM_ROWHEADER,           // nsIAccessible::ROLE_TABLE_ROW_HEADER
    ROLE_SYSTEM_MENUITEM,            // nsIAccessible::ROLE_TEAR_OFF_MENU_ITEM
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_TERMINAL
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_TEXT_CONTAINER
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_TOGGLE_BUTTON
    ROLE_SYSTEM_OUTLINE,             // nsIAccessible::ROLE_TREE_TABLE
    ROLE_SYSTEM_PANE,                // nsIAccessible::ROLE_VIEWPORT
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_HEADER
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_FOOTER
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_PARAGRAPH
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_RULER
    ROLE_SYSTEM_COMBOBOX,            // nsIAccessible::ROLE_AUTOCOMPLETE
    ROLE_SYSTEM_TEXT,                // nsIAccessible::ROLE_EDITBAR
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_EMBEDDED
    ROLE_SYSTEM_TEXT,                // nsIAccessible::ROLE_ENTRY
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_CAPTION
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_DOCUMENT_FRAME
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_HEADING
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_PAGE
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_SECTION
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_REDUNDANT_OBJECT
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_FORM
    USE_ROLE_STRING,                 // nsIAccessible::ROLE_IME
    ROLE_MSAA_LAST_ENTRY              // nsIAccessible::ROLE_LAST_ENTRY
};
