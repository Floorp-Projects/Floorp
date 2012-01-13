/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
                                  // Cross Platform Roles       #
    ATK_ROLE_UNKNOWN,             // roles::NOTHING              0
    ATK_ROLE_UNKNOWN,             // roles::TITLEBAR             1
    ATK_ROLE_MENU_BAR,            // roles::MENUBAR              2
    ATK_ROLE_SCROLL_BAR,          // roles::SCROLLBAR            3
    ATK_ROLE_UNKNOWN,             // roles::GRIP                 4
    ATK_ROLE_UNKNOWN,             // roles::SOUND                5
    ATK_ROLE_UNKNOWN,             // roles::CURSOR               6
    ATK_ROLE_UNKNOWN,             // roles::CARET                7
    ATK_ROLE_ALERT,               // roles::ALERT                8
    ATK_ROLE_WINDOW,              // roles::WINDOW               9
    ATK_ROLE_INTERNAL_FRAME,      // roles::INTERNAL_FRAME       10
    ATK_ROLE_MENU,                // roles::MENUPOPUP            11
    ATK_ROLE_MENU_ITEM,           // roles::MENUITEM             12
    ATK_ROLE_TOOL_TIP,            // roles::TOOLTIP              13
    ATK_ROLE_EMBEDDED,            // roles::APPLICATION          14
    ATK_ROLE_DOCUMENT_FRAME,      // roles::DOCUMENT             15
    ATK_ROLE_PANEL,               // roles::PANE                 16
    ATK_ROLE_CHART,               // roles::CHART                17
    ATK_ROLE_DIALOG,              // roles::DIALOG               18
    ATK_ROLE_UNKNOWN,             // roles::BORDER               19
    ATK_ROLE_PANEL,               // roles::GROUPING             20
    ATK_ROLE_SEPARATOR,           // roles::SEPARATOR            21
    ATK_ROLE_TOOL_BAR,            // roles::TOOLBAR              22
    ATK_ROLE_STATUSBAR,           // roles::STATUSBAR            23
    ATK_ROLE_TABLE,               // roles::TABLE                24
    ATK_ROLE_COLUMN_HEADER,       // roles::COLUMNHEADER         25
    ATK_ROLE_ROW_HEADER,          // roles::ROWHEADER            26
    ATK_ROLE_UNKNOWN,             // roles::COLUMN               27
    ATK_ROLE_LIST_ITEM,           // roles::ROW                  28
    ATK_ROLE_TABLE_CELL,          // roles::CELL                 29
    ATK_ROLE_LINK,                // roles::LINK                 30
    ATK_ROLE_UNKNOWN,             // roles::HELPBALLOON          31
    ATK_ROLE_IMAGE,               // roles::CHARACTER            32
    ATK_ROLE_LIST,                // roles::LIST                 33
    ATK_ROLE_LIST_ITEM,           // roles::LISTITEM             34
    ATK_ROLE_TREE,                // roles::OUTLINE              35
    ATK_ROLE_LIST_ITEM,           // roles::OUTLINEITEM          36
    ATK_ROLE_PAGE_TAB,            // roles::PAGETAB              37
    ATK_ROLE_SCROLL_PANE,         // roles::PROPERTYPAGE         38
    ATK_ROLE_UNKNOWN,             // roles::INDICATOR            39
    ATK_ROLE_IMAGE,               // roles::GRAPHIC              40
    ATK_ROLE_UNKNOWN,             // roles::STATICTEXT           41
    ATK_ROLE_UNKNOWN,             // roles::TEXT_LEAF            42
    ATK_ROLE_PUSH_BUTTON,         // roles::PUSHBUTTON           43
    ATK_ROLE_CHECK_BOX,           // roles::CHECKBUTTON          44
    ATK_ROLE_RADIO_BUTTON,        // roles::RADIOBUTTON          45
    ATK_ROLE_COMBO_BOX,           // roles::COMBOBOX             46
    ATK_ROLE_COMBO_BOX,           // roles::DROPLIST             47
    ATK_ROLE_PROGRESS_BAR,        // roles::PROGRESSBAR          48
    ATK_ROLE_DIAL,                // roles::DIAL                 49
    ATK_ROLE_UNKNOWN,             // roles::HOTKEYFIELD          50
    ATK_ROLE_SLIDER,              // roles::SLIDER               51
    ATK_ROLE_SPIN_BUTTON,         // roles::SPINBUTTON           52
    ATK_ROLE_IMAGE,               // roles::DIAGRAM              53
    ATK_ROLE_ANIMATION,           // roles::ANIMATION            54
    ATK_ROLE_UNKNOWN,             // roles::EQUATION             55
    ATK_ROLE_PUSH_BUTTON,         // roles::BUTTONDROPDOWN       56
    ATK_ROLE_PUSH_BUTTON,         // roles::BUTTONMENU           57
    ATK_ROLE_UNKNOWN,             // roles::BUTTONDROPDOWNGRID   58
    ATK_ROLE_UNKNOWN,             // roles::WHITESPACE           59
    ATK_ROLE_PAGE_TAB_LIST,       // roles::PAGETABLIST          60
    ATK_ROLE_UNKNOWN,             // roles::CLOCK                61
    ATK_ROLE_PUSH_BUTTON,         // roles::SPLITBUTTON          62
    ATK_ROLE_UNKNOWN,             // roles::IPADDRESS            63
    ATK_ROLE_ACCEL_LABEL,         // roles::ACCEL_LABEL          64
    ATK_ROLE_ARROW,               // roles::ARROW                65
    ATK_ROLE_CANVAS,              // roles::CANVAS               66
    ATK_ROLE_CHECK_MENU_ITEM,     // roles::CHECK_MENU_ITEM      67
    ATK_ROLE_COLOR_CHOOSER,       // roles::COLOR_CHOOSER        68
    ATK_ROLE_DATE_EDITOR,         // roles::DATE_EDITOR          69
    ATK_ROLE_DESKTOP_ICON,        // roles::DESKTOP_ICON         70
    ATK_ROLE_DESKTOP_FRAME,       // roles::DESKTOP_FRAME        71
    ATK_ROLE_DIRECTORY_PANE,      // roles::DIRECTORY_PANE       72
    ATK_ROLE_FILE_CHOOSER,        // roles::FILE_CHOOSER         73
    ATK_ROLE_FONT_CHOOSER,        // roles::FONT_CHOOSER         74
    ATK_ROLE_FRAME,               // roles::CHROME_WINDOW        75
    ATK_ROLE_GLASS_PANE,          // roles::GLASS_PANE           76
    ATK_ROLE_HTML_CONTAINER,      // roles::HTML_CONTAINER       77
    ATK_ROLE_ICON,                // roles::ICON                 78
    ATK_ROLE_LABEL,               // roles::LABEL                79
    ATK_ROLE_LAYERED_PANE,        // roles::LAYERED_PANE         80
    ATK_ROLE_OPTION_PANE,         // roles::OPTION_PANE          81
    ATK_ROLE_PASSWORD_TEXT,       // roles::PASSWORD_TEXT        82
    ATK_ROLE_POPUP_MENU,          // roles::POPUP_MENU           83
    ATK_ROLE_RADIO_MENU_ITEM,     // roles::RADIO_MENU_ITEM      84
    ATK_ROLE_ROOT_PANE,           // roles::ROOT_PANE            85
    ATK_ROLE_SCROLL_PANE,         // roles::SCROLL_PANE          86
    ATK_ROLE_SPLIT_PANE,          // roles::SPLIT_PANE           87
    ATK_ROLE_TABLE_COLUMN_HEADER, // roles::TABLE_COLUMN_HEADER  88
    ATK_ROLE_TABLE_ROW_HEADER,    // roles::TABLE_ROW_HEADER     89
    ATK_ROLE_TEAR_OFF_MENU_ITEM,  // roles::TEAR_OFF_MENU_ITEM   90
    ATK_ROLE_TERMINAL,            // roles::TERMINAL             91
    ATK_ROLE_TEXT,                // roles::TEXT_CONTAINER       92
    ATK_ROLE_TOGGLE_BUTTON,       // roles::TOGGLE_BUTTON        93
    ATK_ROLE_TREE_TABLE,          // roles::TREE_TABLE           94
    ATK_ROLE_VIEWPORT,            // roles::VIEWPORT             95
    ATK_ROLE_HEADER,              // roles::HEADER               96
    ATK_ROLE_FOOTER,              // roles::FOOTER               97
    ATK_ROLE_PARAGRAPH,           // roles::PARAGRAPH            98
    ATK_ROLE_RULER,               // roles::RULER                99
    ATK_ROLE_AUTOCOMPLETE,        // roles::AUTOCOMPLETE         100
    ATK_ROLE_EDITBAR,             // roles::EDITBAR              101
    ATK_ROLE_ENTRY,               // roles::ENTRY                102
    ATK_ROLE_CAPTION,             // roles::CAPTION              103
    ATK_ROLE_DOCUMENT_FRAME,      // roles::DOCUMENT_FRAME       104
    ATK_ROLE_HEADING,             // roles::HEADING              105
    ATK_ROLE_PAGE,                // roles::PAGE                 106
    ATK_ROLE_SECTION,             // roles::SECTION              107
    ATK_ROLE_REDUNDANT_OBJECT,    // roles::REDUNDANT_OBJECT     108
    ATK_ROLE_FORM,                // roles::FORM                 109
    ATK_ROLE_INPUT_METHOD_WINDOW, // roles::IME                  110
    ATK_ROLE_APPLICATION,         // roles::APP_ROOT             111
    ATK_ROLE_MENU,                // roles::PARENT_MENUITEM      112
    ATK_ROLE_CALENDAR,            // roles::CALENDAR             113
    ATK_ROLE_MENU,                // roles::COMBOBOX_LIST        114
    ATK_ROLE_MENU_ITEM,           // roles::COMBOBOX_OPTION      115
    ATK_ROLE_IMAGE,               // roles::IMAGE_MAP            116
    ATK_ROLE_LIST_ITEM,           // roles::OPTION               117
    ATK_ROLE_LIST_ITEM,           // roles::RICH_OPTION          118
    ATK_ROLE_LIST,                // roles::LISTBOX              119
    ATK_ROLE_UNKNOWN,             // roles::FLAT_EQUATION        120
    ATK_ROLE_TABLE_CELL,          // roles::GRID_CELL            121
    ATK_ROLE_PANEL,               // roles::EMBEDDED_OBJECT      122
    ATK_ROLE_SECTION,             // roles::NOTE                 123
    ATK_ROLE_PANEL,               // roles::FIGURE               124
    kROLE_ATK_LAST_ENTRY          // roles::LAST_ENTRY
};

