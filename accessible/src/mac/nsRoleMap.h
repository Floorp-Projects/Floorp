/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Håkan Waara <hwaara@gmail.com>
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

#import <Cocoa/Cocoa.h>

#include "nsIAccessible.h"

static const NSString* AXRoles [] = {
  NSAccessibilityUnknownRole,                   // roles::NOTHING              0
  NSAccessibilityUnknownRole,                   // roles::TITLEBAR             1      Irrelevant on OS X; windows are always native.
  NSAccessibilityScrollBarRole,                 // roles::SCROLLBAR            3      We might need to make this its own mozAccessible, to support the children objects (valueindicator, down/up buttons).
  NSAccessibilityMenuBarRole,                   // roles::MENUBAR              2      Irrelevant on OS X; the menubar will always be native and on the top of the screen.
  NSAccessibilitySplitterRole,                  // roles::GRIP                 4
  NSAccessibilityUnknownRole,                   // roles::SOUND                5      Unused on OS X.
  NSAccessibilityUnknownRole,                   // roles::CURSOR               6      Unused on OS X.
  NSAccessibilityUnknownRole,                   // roles::CARET                7      Unused on OS X.
  NSAccessibilityWindowRole,                    // roles::ALERT                8
  NSAccessibilityWindowRole,                    // roles::WINDOW               9      Irrelevant on OS X; all window a11y is handled by the system.
  NSAccessibilityScrollAreaRole,                // roles::INTERNAL_FRAME       10
  NSAccessibilityMenuRole,                      // roles::MENUPOPUP            11     The parent of menuitems.
  NSAccessibilityMenuItemRole,                  // roles::MENUITEM             12
  @"AXHelpTag",                                 // roles::TOOLTIP              13     10.4+ only, so we re-define the constant.
  NSAccessibilityGroupRole,                     // roles::APPLICATION          14     Unused on OS X. the system will take care of this.
  @"AXWebArea",                                 // roles::DOCUMENT             15
  NSAccessibilityGroupRole,                     // roles::PANE                 16
  NSAccessibilityUnknownRole,                   // roles::CHART                17
  NSAccessibilityWindowRole,                    // roles::DIALOG               18     There's a dialog subrole.
  NSAccessibilityUnknownRole,                   // roles::BORDER               19     Unused on OS X.
  NSAccessibilityGroupRole,                     // roles::GROUPING             20
  NSAccessibilityUnknownRole,                   // roles::SEPARATOR            21
  NSAccessibilityToolbarRole,                   // roles::TOOLBAR              22
  NSAccessibilityUnknownRole,                   // roles::STATUSBAR            23     Doesn't exist on OS X (a status bar is its parts; a progressbar, a label, etc.)
  NSAccessibilityGroupRole,                     // roles::TABLE                24
  NSAccessibilityGroupRole,                     // roles::COLUMNHEADER         25
  NSAccessibilityGroupRole,                     // roles::ROWHEADER            26
  NSAccessibilityColumnRole,                    // roles::COLUMN               27
  NSAccessibilityRowRole,                       // roles::ROW                  28
  NSAccessibilityGroupRole,                     // roles::CELL                 29
  @"AXLink",                                    // roles::LINK                 30     10.4+ the attr first define in SDK 10.4, so we define it here too. ROLE_LINK
  @"AXHelpTag",                                 // roles::HELPBALLOON          31
  NSAccessibilityUnknownRole,                   // roles::CHARACTER            32     Unused on OS X.
  NSAccessibilityListRole,                      // roles::LIST                 33
  NSAccessibilityRowRole,                       // roles::LISTITEM             34
  NSAccessibilityOutlineRole,                   // roles::OUTLINE              35
  NSAccessibilityRowRole,                       // roles::OUTLINEITEM          36     XXX: use OutlineRow as subrole.
  NSAccessibilityRadioButtonRole,               // roles::PAGETAB              37
  NSAccessibilityGroupRole,                     // roles::PROPERTYPAGE         38
  NSAccessibilityUnknownRole,                   // roles::INDICATOR            39
  NSAccessibilityImageRole,                     // roles::GRAPHIC              40
  NSAccessibilityStaticTextRole,                // roles::STATICTEXT           41
  NSAccessibilityStaticTextRole,                // roles::TEXT_LEAF            42
  NSAccessibilityButtonRole,                    // roles::PUSHBUTTON           43
  NSAccessibilityCheckBoxRole,                  // roles::CHECKBUTTON          44
  NSAccessibilityRadioButtonRole,               // roles::RADIOBUTTON          45
  NSAccessibilityPopUpButtonRole,               // roles::COMBOBOX             46
  NSAccessibilityPopUpButtonRole,               // roles::DROPLIST             47
  NSAccessibilityProgressIndicatorRole,         // roles::PROGRESSBAR          48
  NSAccessibilityUnknownRole,                   // roles::DIAL                 49
  NSAccessibilityUnknownRole,                   // roles::HOTKEYFIELD          50
  NSAccessibilitySliderRole,                    // roles::SLIDER               51
  NSAccessibilityIncrementorRole,               // roles::SPINBUTTON           52     Subroles: Increment/Decrement.
  NSAccessibilityUnknownRole,                   // roles::DIAGRAM              53
  NSAccessibilityUnknownRole,                   // roles::ANIMATION            54
  NSAccessibilityUnknownRole,                   // roles::EQUATION             55
  NSAccessibilityPopUpButtonRole,               // roles::BUTTONDROPDOWN       56
  NSAccessibilityMenuButtonRole,                // roles::BUTTONMENU           57
  NSAccessibilityGroupRole,                     // roles::BUTTONDROPDOWNGRID   58
  NSAccessibilityUnknownRole,                   // roles::WHITESPACE           59
  NSAccessibilityTabGroupRole,                  // roles::PAGETABLIST          60
  NSAccessibilityUnknownRole,                   // roles::CLOCK                61     Unused on OS X
  NSAccessibilityButtonRole,                    // roles::SPLITBUTTON          62
  NSAccessibilityUnknownRole,                   // roles::IPADDRESS            63
  NSAccessibilityStaticTextRole,                // roles::ACCEL_LABEL          64
  NSAccessibilityUnknownRole,                   // roles::ARROW                65
  NSAccessibilityImageRole,                     // roles::CANVAS               66
  NSAccessibilityMenuItemRole,                  // roles::CHECK_MENU_ITEM      67
  NSAccessibilityColorWellRole,                 // roles::COLOR_CHOOSER        68
  NSAccessibilityUnknownRole,                   // roles::DATE_EDITOR          69 
  NSAccessibilityImageRole,                     // roles::DESKTOP_ICON         70
  NSAccessibilityUnknownRole,                   // roles::DESKTOP_FRAME        71
  NSAccessibilityBrowserRole,                   // roles::DIRECTORY_PANE       72
  NSAccessibilityUnknownRole,                   // roles::FILE_CHOOSER         73     Unused on OS X
  NSAccessibilityUnknownRole,                   // roles::FONT_CHOOSER         74
  NSAccessibilityUnknownRole,                   // roles::CHROME_WINDOW        75     Unused on OS X
  NSAccessibilityGroupRole,                     // roles::GLASS_PANE           76
  NSAccessibilityUnknownRole,                   // roles::HTML_CONTAINER       77
  NSAccessibilityImageRole,                     // roles::ICON                 78
  NSAccessibilityStaticTextRole,                // roles::LABEL                79
  NSAccessibilityGroupRole,                     // roles::LAYERED_PANE         80
  NSAccessibilityGroupRole,                     // roles::OPTION_PANE          81
  NSAccessibilityTextFieldRole,                 // roles::PASSWORD_TEXT        82
  NSAccessibilityUnknownRole,                   // roles::POPUP_MENU           83     Unused
  NSAccessibilityMenuItemRole,                  // roles::RADIO_MENU_ITEM      84
  NSAccessibilityGroupRole,                     // roles::ROOT_PANE            85
  NSAccessibilityScrollAreaRole,                // roles::SCROLL_PANE          86
  NSAccessibilitySplitGroupRole,                // roles::SPLIT_PANE           87
  NSAccessibilityUnknownRole,                   // roles::TABLE_COLUMN_HEADER  88
  NSAccessibilityUnknownRole,                   // roles::TABLE_ROW_HEADER     89
  NSAccessibilityMenuItemRole,                  // roles::TEAR_OFF_MENU_ITEM   90
  NSAccessibilityUnknownRole,                   // roles::TERMINAL             91
  NSAccessibilityGroupRole,                     // roles::TEXT_CONTAINER       92
  NSAccessibilityButtonRole,                    // roles::TOGGLE_BUTTON        93
  NSAccessibilityTableRole,                     // roles::TREE_TABLE           94
  NSAccessibilityUnknownRole,                   // roles::VIEWPORT             95
  NSAccessibilityGroupRole,                     // roles::HEADER               96
  NSAccessibilityGroupRole,                     // roles::FOOTER               97
  NSAccessibilityGroupRole,                     // roles::PARAGRAPH            98
  @"AXRuler",                                   // roles::RULER                99     10.4+ only, so we re-define the constant.
  NSAccessibilityUnknownRole,                   // roles::AUTOCOMPLETE         100
  NSAccessibilityTextFieldRole,                 // roles::EDITBAR              101
  NSAccessibilityTextFieldRole,                 // roles::ENTRY                102
  NSAccessibilityStaticTextRole,                // roles::CAPTION              103
  NSAccessibilityScrollAreaRole,                // roles::DOCUMENT_FRAME       104
  @"AXHeading",                                 // roles::HEADING              105
  NSAccessibilityGroupRole,                     // roles::PAGE                 106
  NSAccessibilityGroupRole,                     // roles::SECTION              107
  NSAccessibilityUnknownRole,                   // roles::REDUNDANT_OBJECT     108
  NSAccessibilityGroupRole,                     // roles::FORM                 109
  NSAccessibilityUnknownRole,                   // roles::IME                  110
  NSAccessibilityUnknownRole,                   // roles::APP_ROOT             111    Unused on OS X
  NSAccessibilityMenuItemRole,                  // roles::PARENT_MENUITEM      112
  NSAccessibilityGroupRole,                     // roles::CALENDAR             113
  NSAccessibilityMenuRole,                      // roles::COMBOBOX_LIST        114
  NSAccessibilityMenuItemRole,                  // roles::COMBOBOX_OPTION      115
  NSAccessibilityImageRole,                     // roles::IMAGE_MAP            116
  NSAccessibilityRowRole,                       // roles::OPTION               117
  NSAccessibilityRowRole,                       // roles::RICH_OPTION          118
  NSAccessibilityListRole,                      // roles::LISTBOX              119
  NSAccessibilityUnknownRole,                   // roles::FLAT_EQUATION        120
  NSAccessibilityGroupRole,                     // roles::GRID_CELL            121
  NSAccessibilityGroupRole,                     // roles::EMBEDDED_OBJECT      122
  NSAccessibilityGroupRole,                     // roles::NOTE                 123
  NSAccessibilityGroupRole,                     // roles::FIGURE               124
  NSAccessibilityCheckBoxRole,                  // roles::CHECK_RICH_OPTION    125
  @"ROLE_LAST_ENTRY"                            // roles::LAST_ENTRY                  Bogus role that will never be shown (just marks the end of this array)!
};
