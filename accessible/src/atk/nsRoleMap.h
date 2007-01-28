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
                                  // Cross Platform Roles                  #
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_NOTHING           0
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_TITLEBAR          1
    ATK_ROLE_MENU_BAR,            // nsIAccessible::ROLE_MENUBAR           2
    ATK_ROLE_SCROLL_BAR,          // nsIAccessible::ROLE_SCROLLBAR         3
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_GRIP              4
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_SOUND             5
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_CURSOR            6
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_CARET             7
    ATK_ROLE_ALERT,               // nsIAccessible::ROLE_ALERT             8
    ATK_ROLE_WINDOW,              // nsIAccessible::ROLE_WINDOW            9
    ATK_ROLE_PANEL,               // nsIAccessible::ROLE_CLIENT            10
    ATK_ROLE_MENU,                // nsIAccessible::ROLE_MENUPOPUP         11
    ATK_ROLE_MENU_ITEM,           // nsIAccessible::ROLE_MENUITEM          12
    ATK_ROLE_TOOL_TIP,            // nsIAccessible::ROLE_TOOLTIP           13
    ATK_ROLE_EMBEDDED,            // nsIAccessible::ROLE_APPLICATION       14
    ATK_ROLE_DOCUMENT_FRAME,      // nsIAccessible::ROLE_DOCUMENT          15
    ATK_ROLE_PANEL,               // nsIAccessible::ROLE_PANE              16
    ATK_ROLE_CHART,               // nsIAccessible::ROLE_CHART             17
    ATK_ROLE_DIALOG,              // nsIAccessible::ROLE_DIALOG            18
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_BORDER            19
    ATK_ROLE_PANEL,               // nsIAccessible::ROLE_GROUPING          20
    ATK_ROLE_SEPARATOR,           // nsIAccessible::ROLE_SEPARATOR         21
    ATK_ROLE_TOOL_BAR,            // nsIAccessible::ROLE_TOOLBAR           22
    ATK_ROLE_STATUSBAR,           // nsIAccessible::ROLE_STATUSBAR         23
    ATK_ROLE_TABLE,               // nsIAccessible::ROLE_TABLE             24
    ATK_ROLE_COLUMN_HEADER,       // nsIAccessible::ROLE_COLUMNHEADER      25
    ATK_ROLE_ROW_HEADER,          // nsIAccessible::ROLE_ROWHEADER         26
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_COLUMN            27
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_ROW               28
    ATK_ROLE_TABLE_CELL,          // nsIAccessible::ROLE_CELL              29
    ATK_ROLE_LINK,                // nsIAccessible::ROLE_LINK              30
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_HELPBALLOON       31
    ATK_ROLE_IMAGE,               // nsIAccessible::ROLE_CHARACTER         32
    ATK_ROLE_LIST,                // nsIAccessible::ROLE_LIST              33
    ATK_ROLE_LIST_ITEM,           // nsIAccessible::ROLE_LISTITEM          34
    ATK_ROLE_TREE,                // nsIAccessible::ROLE_OUTLINE           35
    ATK_ROLE_LIST_ITEM,           // nsIAccessible::ROLE_OUTLINEITEM       36
    ATK_ROLE_PAGE_TAB,            // nsIAccessible::ROLE_PAGETAB           37
    ATK_ROLE_SCROLL_PANE,         // nsIAccessible::ROLE_PROPERTYPAGE      38
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_INDICATOR         39
    ATK_ROLE_IMAGE,               // nsIAccessible::ROLE_GRAPHIC           40
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_STATICTEXT        41
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_TEXT_LEAF         42
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessible::ROLE_PUSHBUTTON        43
    ATK_ROLE_CHECK_BOX,           // nsIAccessible::ROLE_CHECKBUTTON       44
    ATK_ROLE_RADIO_BUTTON,        // nsIAccessible::ROLE_RADIOBUTTON       45
    ATK_ROLE_COMBO_BOX,           // nsIAccessible::ROLE_COMBOBOX          46
    ATK_ROLE_COMBO_BOX,           // nsIAccessible::ROLE_DROPLIST          47
    ATK_ROLE_PROGRESS_BAR,        // nsIAccessible::ROLE_PROGRESSBAR       48
    ATK_ROLE_DIAL,                // nsIAccessible::ROLE_DIAL              49
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_HOTKEYFIELD       50
    ATK_ROLE_SLIDER,              // nsIAccessible::ROLE_SLIDER            51
    ATK_ROLE_SPIN_BUTTON,         // nsIAccessible::ROLE_SPINBUTTON        52
    ATK_ROLE_IMAGE,               // nsIAccessible::ROLE_DIAGRAM           53
    ATK_ROLE_ANIMATION,           // nsIAccessible::ROLE_ANIMATION         54
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_EQUATION          55
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessible::ROLE_BUTTONDROPDOWN    56
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessible::ROLE_BUTTONMENU        57
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_BUTTONDROPDOWNGRID 58
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_WHITESPACE        59
    ATK_ROLE_PAGE_TAB_LIST,       // nsIAccessible::ROLE_PAGETABLIST       60
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_CLOCK             61
    ATK_ROLE_PUSH_BUTTON,         // nsIAccessible::ROLE_SPLITBUTTON       62
    ATK_ROLE_UNKNOWN,             // nsIAccessible::ROLE_IPADDRESS         63
    ATK_ROLE_ACCEL_LABEL,         // nsIAccessible::ROLE_ACCEL_LABEL       64
    ATK_ROLE_ARROW,               // nsIAccessible::ROLE_ARROW             65
    ATK_ROLE_CANVAS,              // nsIAccessible::ROLE_CANVAS            66
    ATK_ROLE_CHECK_MENU_ITEM,     // nsIAccessible::ROLE_CHECK_MENU_ITEM   67
    ATK_ROLE_COLOR_CHOOSER,       // nsIAccessible::ROLE_COLOR_CHOOSER     68
    ATK_ROLE_DATE_EDITOR,         // nsIAccessible::ROLE_DATE_EDITOR       69
    ATK_ROLE_DESKTOP_ICON,        // nsIAccessible::ROLE_DESKTOP_ICON      70
    ATK_ROLE_DESKTOP_FRAME,       // nsIAccessible::ROLE_DESKTOP_FRAME     71
    ATK_ROLE_DIRECTORY_PANE,      // nsIAccessible::ROLE_DIRECTORY_PANE    72
    ATK_ROLE_FILE_CHOOSER,        // nsIAccessible::ROLE_FILE_CHOOSER      73
    ATK_ROLE_FONT_CHOOSER,        // nsIAccessible::ROLE_FONT_CHOOSER      74
    ATK_ROLE_FRAME,               // nsIAccessible::ROLE_CHROME_WINDOW     75
    ATK_ROLE_GLASS_PANE,          // nsIAccessible::ROLE_GLASS_PANE        76
    ATK_ROLE_HTML_CONTAINER,      // nsIAccessible::ROLE_HTML_CONTAINER    77
    ATK_ROLE_ICON,                // nsIAccessible::ROLE_ICON              78
    ATK_ROLE_LABEL,               // nsIAccessible::ROLE_LABEL             79
    ATK_ROLE_LAYERED_PANE,        // nsIAccessible::ROLE_LAYERED_PANE      80
    ATK_ROLE_OPTION_PANE,         // nsIAccessible::ROLE_OPTION_PANE       81
    ATK_ROLE_PASSWORD_TEXT,       // nsIAccessible::ROLE_PASSWORD_TEXT     82
    ATK_ROLE_POPUP_MENU,          // nsIAccessible::ROLE_POPUP_MENU        83
    ATK_ROLE_RADIO_MENU_ITEM,     // nsIAccessible::ROLE_RADIO_MENU_ITEM   84
    ATK_ROLE_ROOT_PANE,           // nsIAccessible::ROLE_ROOT_PANE         85
    ATK_ROLE_SCROLL_PANE,         // nsIAccessible::ROLE_SCROLL_PANE       86
    ATK_ROLE_SPLIT_PANE,          // nsIAccessible::ROLE_SPLIT_PANE        87
    ATK_ROLE_TABLE_COLUMN_HEADER, // nsIAccessible::ROLE_TABLE_COLUMN_HEADER 88
    ATK_ROLE_TABLE_ROW_HEADER,    // nsIAccessible::ROLE_TABLE_ROW_HEADER  89
    ATK_ROLE_TEAR_OFF_MENU_ITEM,  // nsIAccessible::ROLE_TEAR_OFF_MENU_ITEM  90
    ATK_ROLE_TERMINAL,            // nsIAccessible::ROLE_TERMINAL          91
    ATK_ROLE_TEXT,                // nsIAccessible::ROLE_TEXT_CONTAINER    92
    ATK_ROLE_TOGGLE_BUTTON,       // nsIAccessible::ROLE_TOGGLE_BUTTON     93
    ATK_ROLE_TREE_TABLE,          // nsIAccessible::ROLE_TREE_TABLE        94
    ATK_ROLE_VIEWPORT,            // nsIAccessible::ROLE_VIEWPORT          95
    ATK_ROLE_HEADER,              // nsIAccessible::ROLE_HEADER            96
    ATK_ROLE_FOOTER,              // nsIAccessible::ROLE_FOOTER            97
    ATK_ROLE_PARAGRAPH,           // nsIAccessible::ROLE_PARAGRAPH         98
    ATK_ROLE_RULER,               // nsIAccessible::ROLE_RULER             99
    ATK_ROLE_AUTOCOMPLETE,        // nsIAccessible::ROLE_AUTOCOMPLETE      100
    ATK_ROLE_EDITBAR,             // nsIAccessible::ROLE_EDITBAR           101
    ATK_ROLE_ENTRY,               // nsIAccessible::ROLE_ENTRY             102
    ATK_ROLE_CAPTION,             // nsIAccessible::ROLE_CAPTION           103
    ATK_ROLE_DOCUMENT_FRAME,      // nsIAccessible::ROLE_DOCUMENT_FRAME    104
    ATK_ROLE_HEADING,             // nsIAccessible::ROLE_HEADING           105
    ATK_ROLE_PAGE,                // nsIAccessible::ROLE_PAGE              106
    ATK_ROLE_SECTION,             // nsIAccessible::ROLE_SECTION           107
    ATK_ROLE_REDUNDANT_OBJECT,    // nsIAccessible::ROLE_REDUNDANT_OBJECT  108
    ATK_ROLE_FORM,                // nsIAccessible::ROLE_FORM              109
    ATK_ROLE_INPUT_METHOD_WINDOW, // nsIAccessible::ROLE_IME               110
    ATK_ROLE_APPLICATION,         // nsIAccessible::ROLE_APP_ROOT          111
    ATK_ROLE_MENU,                // nsIAccessible::ROLE_PARENT_MENUITEM   112
    ATK_ROLE_CALENDAR,            // nsIAccessible::ROLE_CALENDAR          113
    ATK_ROLE_MENU,                // nsIAccessible::ROLE_COMBOBOX_LIST     114
    ATK_ROLE_MENU_ITEM,           // nsIAccessible::ROLE_COMBOBOX_LISTITEM 115
    kROLE_ATK_LAST_ENTRY          // nsIAccessible::ROLE_LAST_ENTRY
};

