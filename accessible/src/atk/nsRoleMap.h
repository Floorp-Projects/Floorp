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

#include <atk/atk.h>
#include "nsAccessibleWrap.h"

const PRUint32 kROLE_ATK_LAST_ENTRY = 0xffffffff;

// Map array from cross platform roles to  ATK roles
static const PRUint32 atkRoleMap[] = {
                                  // Cross Platform Roles                         #
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_NOTHING              0
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_TITLEBAR             1
    ATK_ROLE_MENU_BAR,            // nsIAccessibleRole::ROLE_MENUBAR              2
    ATK_ROLE_SCROLL_BAR,          // nsIAccessibleRole::ROLE_SCROLLBAR            3
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_GRIP                 4
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_SOUND                5
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_CURSOR               6
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_CARET                7
    ATK_ROLE_ALERT,               // nsIAccessibleRole::ROLE_ALERT                8
    ATK_ROLE_WINDOW,              // nsIAccessibleRole::ROLE_WINDOW               9
    ATK_ROLE_INTERNAL_FRAME,      // nsIAccessibleRole::ROLE_INTERNAL_FRAME       10
    ATK_ROLE_MENU,                // nsIAccessibleRole::ROLE_MENUPOPUP            11
    ATK_ROLE_MENU_ITEM,           // nsIAccessibleRole::ROLE_MENUITEM             12
    ATK_ROLE_TOOL_TIP,            // nsIAccessibleRole::ROLE_TOOLTIP              13
    ATK_ROLE_EMBEDDED,            // nsIAccessibleRole::ROLE_APPLICATION          14
    ATK_ROLE_DOCUMENT_FRAME,      // nsIAccessibleRole::ROLE_DOCUMENT             15
    ATK_ROLE_PANEL,               // nsIAccessibleRole::ROLE_PANE                 16
    ATK_ROLE_CHART,               // nsIAccessibleRole::ROLE_CHART                17
    ATK_ROLE_DIALOG,              // nsIAccessibleRole::ROLE_DIALOG               18
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_BORDER               19
    ATK_ROLE_PANEL,               // nsIAccessibleRole::ROLE_GROUPING             20
    ATK_ROLE_SEPARATOR,           // nsIAccessibleRole::ROLE_SEPARATOR            21
    ATK_ROLE_TOOL_BAR,            // nsIAccessibleRole::ROLE_TOOLBAR              22
    ATK_ROLE_STATUSBAR,           // nsIAccessibleRole::ROLE_STATUSBAR            23
    ATK_ROLE_TABLE,               // nsIAccessibleRole::ROLE_TABLE                24
    ATK_ROLE_COLUMN_HEADER,       // nsIAccessibleRole::ROLE_COLUMNHEADER         25
    ATK_ROLE_ROW_HEADER,          // nsIAccessibleRole::ROLE_ROWHEADER            26
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_COLUMN               27
    ATK_ROLE_LIST_ITEM,           // nsIAccessibleRole::ROLE_ROW                  28
    ATK_ROLE_TABLE_CELL,          // nsIAccessibleRole::ROLE_CELL                 29
    ATK_ROLE_LINK,                // nsIAccessibleRole::ROLE_LINK                 30
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_HELPBALLOON          31
    ATK_ROLE_IMAGE,               // nsIAccessibleRole::ROLE_CHARACTER            32
    ATK_ROLE_LIST,                // nsIAccessibleRole::ROLE_LIST                 33
    ATK_ROLE_LIST_ITEM,           // nsIAccessibleRole::ROLE_LISTITEM             34
    ATK_ROLE_TREE,                // nsIAccessibleRole::ROLE_OUTLINE              35
    ATK_ROLE_LIST_ITEM,           // nsIAccessibleRole::ROLE_OUTLINEITEM          36
    ATK_ROLE_PAGE_TAB,            // nsIAccessibleRole::ROLE_PAGETAB              37
    ATK_ROLE_SCROLL_PANE,         // nsIAccessibleRole::ROLE_PROPERTYPAGE         38
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_INDICATOR            39
    ATK_ROLE_IMAGE,               // nsIAccessibleRole::ROLE_GRAPHIC              40
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_STATICTEXT           41
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_TEXT_LEAF            42
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessibleRole::ROLE_PUSHBUTTON           43
    ATK_ROLE_CHECK_BOX,           // nsIAccessibleRole::ROLE_CHECKBUTTON          44
    ATK_ROLE_RADIO_BUTTON,        // nsIAccessibleRole::ROLE_RADIOBUTTON          45
    ATK_ROLE_COMBO_BOX,           // nsIAccessibleRole::ROLE_COMBOBOX             46
    ATK_ROLE_COMBO_BOX,           // nsIAccessibleRole::ROLE_DROPLIST             47
    ATK_ROLE_PROGRESS_BAR,        // nsIAccessibleRole::ROLE_PROGRESSBAR          48
    ATK_ROLE_DIAL,                // nsIAccessibleRole::ROLE_DIAL                 49
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_HOTKEYFIELD          50
    ATK_ROLE_SLIDER,              // nsIAccessibleRole::ROLE_SLIDER               51
    ATK_ROLE_SPIN_BUTTON,         // nsIAccessibleRole::ROLE_SPINBUTTON           52
    ATK_ROLE_IMAGE,               // nsIAccessibleRole::ROLE_DIAGRAM              53
    ATK_ROLE_ANIMATION,           // nsIAccessibleRole::ROLE_ANIMATION            54
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_EQUATION             55
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessibleRole::ROLE_BUTTONDROPDOWN       56
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessibleRole::ROLE_BUTTONMENU           57
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_BUTTONDROPDOWNGRID   58
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_WHITESPACE           59
    ATK_ROLE_PAGE_TAB_LIST,       // nsIAccessibleRole::ROLE_PAGETABLIST          60
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_CLOCK                61
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessibleRole::ROLE_SPLITBUTTON          62
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_IPADDRESS            63
    ATK_ROLE_ACCEL_LABEL,         // nsIAccessibleRole::ROLE_ACCEL_LABEL          64
    ATK_ROLE_ARROW,               // nsIAccessibleRole::ROLE_ARROW                65
    ATK_ROLE_CANVAS,              // nsIAccessibleRole::ROLE_CANVAS               66
    ATK_ROLE_CHECK_MENU_ITEM,     // nsIAccessibleRole::ROLE_CHECK_MENU_ITEM      67
    ATK_ROLE_COLOR_CHOOSER,       // nsIAccessibleRole::ROLE_COLOR_CHOOSER        68
    ATK_ROLE_DATE_EDITOR,         // nsIAccessibleRole::ROLE_DATE_EDITOR          69
    ATK_ROLE_DESKTOP_ICON,        // nsIAccessibleRole::ROLE_DESKTOP_ICON         70
    ATK_ROLE_DESKTOP_FRAME,       // nsIAccessibleRole::ROLE_DESKTOP_FRAME        71
    ATK_ROLE_DIRECTORY_PANE,      // nsIAccessibleRole::ROLE_DIRECTORY_PANE       72
    ATK_ROLE_FILE_CHOOSER,        // nsIAccessibleRole::ROLE_FILE_CHOOSER         73
    ATK_ROLE_FONT_CHOOSER,        // nsIAccessibleRole::ROLE_FONT_CHOOSER         74
    ATK_ROLE_FRAME,               // nsIAccessibleRole::ROLE_CHROME_WINDOW        75
    ATK_ROLE_GLASS_PANE,          // nsIAccessibleRole::ROLE_GLASS_PANE           76
    ATK_ROLE_HTML_CONTAINER,      // nsIAccessibleRole::ROLE_HTML_CONTAINER       77
    ATK_ROLE_ICON,                // nsIAccessibleRole::ROLE_ICON                 78
    ATK_ROLE_LABEL,               // nsIAccessibleRole::ROLE_LABEL                79
    ATK_ROLE_LAYERED_PANE,        // nsIAccessibleRole::ROLE_LAYERED_PANE         80
    ATK_ROLE_OPTION_PANE,         // nsIAccessibleRole::ROLE_OPTION_PANE          81
    ATK_ROLE_PASSWORD_TEXT,       // nsIAccessibleRole::ROLE_PASSWORD_TEXT        82
    ATK_ROLE_POPUP_MENU,          // nsIAccessibleRole::ROLE_POPUP_MENU           83
    ATK_ROLE_RADIO_MENU_ITEM,     // nsIAccessibleRole::ROLE_RADIO_MENU_ITEM      84
    ATK_ROLE_ROOT_PANE,           // nsIAccessibleRole::ROLE_ROOT_PANE            85
    ATK_ROLE_SCROLL_PANE,         // nsIAccessibleRole::ROLE_SCROLL_PANE          86
    ATK_ROLE_SPLIT_PANE,          // nsIAccessibleRole::ROLE_SPLIT_PANE           87
    ATK_ROLE_TABLE_COLUMN_HEADER, // nsIAccessibleRole::ROLE_TABLE_COLUMN_HEADER  88
    ATK_ROLE_TABLE_ROW_HEADER,    // nsIAccessibleRole::ROLE_TABLE_ROW_HEADER     89
    ATK_ROLE_TEAR_OFF_MENU_ITEM,  // nsIAccessibleRole::ROLE_TEAR_OFF_MENU_ITEM   90
    ATK_ROLE_TERMINAL,            // nsIAccessibleRole::ROLE_TERMINAL             91
    ATK_ROLE_TEXT,                // nsIAccessibleRole::ROLE_TEXT_CONTAINER       92
    ATK_ROLE_TOGGLE_BUTTON,       // nsIAccessibleRole::ROLE_TOGGLE_BUTTON        93
    ATK_ROLE_TREE_TABLE,          // nsIAccessibleRole::ROLE_TREE_TABLE           94
    ATK_ROLE_VIEWPORT,            // nsIAccessibleRole::ROLE_VIEWPORT             95
    ATK_ROLE_HEADER,              // nsIAccessibleRole::ROLE_HEADER               96
    ATK_ROLE_FOOTER,              // nsIAccessibleRole::ROLE_FOOTER               97
    ATK_ROLE_PARAGRAPH,           // nsIAccessibleRole::ROLE_PARAGRAPH            98
    ATK_ROLE_RULER,               // nsIAccessibleRole::ROLE_RULER                99
    ATK_ROLE_AUTOCOMPLETE,        // nsIAccessibleRole::ROLE_AUTOCOMPLETE         100
    ATK_ROLE_EDITBAR,             // nsIAccessibleRole::ROLE_EDITBAR              101
    ATK_ROLE_ENTRY,               // nsIAccessibleRole::ROLE_ENTRY                102
    ATK_ROLE_CAPTION,             // nsIAccessibleRole::ROLE_CAPTION              103
    ATK_ROLE_DOCUMENT_FRAME,      // nsIAccessibleRole::ROLE_DOCUMENT_FRAME       104
    ATK_ROLE_HEADING,             // nsIAccessibleRole::ROLE_HEADING              105
    ATK_ROLE_PAGE,                // nsIAccessibleRole::ROLE_PAGE                 106
    ATK_ROLE_SECTION,             // nsIAccessibleRole::ROLE_SECTION              107
    ATK_ROLE_REDUNDANT_OBJECT,    // nsIAccessibleRole::ROLE_REDUNDANT_OBJECT     108
    ATK_ROLE_FORM,                // nsIAccessibleRole::ROLE_FORM                 109
    ATK_ROLE_INPUT_METHOD_WINDOW, // nsIAccessibleRole::ROLE_IME                  110
    ATK_ROLE_APPLICATION,         // nsIAccessibleRole::ROLE_APP_ROOT             111
    ATK_ROLE_MENU,                // nsIAccessibleRole::ROLE_PARENT_MENUITEM      112
    ATK_ROLE_CALENDAR,            // nsIAccessibleRole::ROLE_CALENDAR             113
    ATK_ROLE_MENU,                // nsIAccessibleRole::ROLE_COMBOBOX_LIST        114
    ATK_ROLE_MENU_ITEM,           // nsIAccessibleRole::ROLE_COMBOBOX_OPTION      115
    ATK_ROLE_IMAGE,               // nsIAccessibleRole::ROLE_IMAGE_MAP            116
    ATK_ROLE_LIST_ITEM,           // nsIAccessibleRole::ROLE_OPTION               117
    ATK_ROLE_LIST_ITEM,           // nsIAccessibleRole::ROLE_RICH_OPTION          118
    ATK_ROLE_LIST,                // nsIAccessibleRole::ROLE_LISTBOX              119
    ATK_ROLE_UNKNOWN,             // nsIAccessibleRole::ROLE_FLAT_EQUATION        120
    ATK_ROLE_TABLE_CELL,          // nsIAccessibleRole::ROLE_GRID_CELL            121
    ATK_ROLE_PANEL,               // nsIAccessibleRole::ROLE_EMBEDDED_OBJECT      122
    ATK_ROLE_SECTION,             // nsIAccessibleRole::ROLE_NOTE                 123
    kROLE_ATK_LAST_ENTRY          // nsIAccessibleRole::ROLE_LAST_ENTRY
};

