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
  NSAccessibilityUnknownRole,                   // ROLE_NOTHING
  NSAccessibilityUnknownRole,                   // ROLE_TITLEBAR. (irrelevant on OS X; windows are always native.)
  NSAccessibilityMenuBarRole,                   // ROLE_MENUBAR. (irrelevant on OS X; the menubar will always be native and on the top of the screen.)
  NSAccessibilityScrollBarRole,                 // ROLE_SCROLLBAR. we might need to make this its own mozAccessible, to support the children objects (valueindicator, down/up buttons).
  NSAccessibilitySplitterRole,                  // ROLE_GRIP
  NSAccessibilityUnknownRole,                   // ROLE_SOUND. unused on OS X
  NSAccessibilityUnknownRole,                   // ROLE_CURSOR. unused on OS X
  NSAccessibilityUnknownRole,                   // ROLE_CARET. unused on OS X
  NSAccessibilityWindowRole,                    // ROLE_ALERT
  NSAccessibilityWindowRole,                    // ROLE_WINDOW. irrelevant on OS X; all window a11y is handled by the system.
  @"AXWebArea",                                 // ROLE_INTERNAL_FRAME
  NSAccessibilityMenuRole,                      // ROLE_MENUPOPUP. the parent of menuitems
  NSAccessibilityMenuItemRole,                  // ROLE_MENUITEM.
  @"AXHelpTag",                                 // ROLE_TOOLTIP. 10.4+ only, so we re-define the constant.
  NSAccessibilityGroupRole,                     // ROLE_APPLICATION. unused on OS X. the system will take care of this.
  NSAccessibilityGroupRole,                     // ROLE_DOCUMENT
  NSAccessibilityGroupRole,                     // ROLE_PANE
  NSAccessibilityUnknownRole,                   // ROLE_CHART
  NSAccessibilityWindowRole,                    // ROLE_DIALOG. there's a dialog subrole.
  NSAccessibilityUnknownRole,                   // ROLE_BORDER. unused on OS X
  NSAccessibilityGroupRole,                     // ROLE_GROUPING
  NSAccessibilityUnknownRole,                   // ROLE_SEPARATOR
  NSAccessibilityToolbarRole,                   // ROLE_TOOLBAR
  NSAccessibilityUnknownRole,                   // ROLE_STATUSBAR. doesn't exist on OS X (a status bar is its parts; a progressbar, a label, etc.)
  NSAccessibilityGroupRole,                     // ROLE_TABLE
  NSAccessibilityGroupRole,                     // ROLE_COLUMNHEADER
  NSAccessibilityGroupRole,                     // ROLE_ROWHEADER
  NSAccessibilityColumnRole,                    // ROLE_COLUMN
  NSAccessibilityRowRole,                       // ROLE_ROW
  NSAccessibilityGroupRole,                     // ROLE_CELL
  @"AXLink",                                    // ROLE_LINK. 10.4+ the attr first define in SDK 10.4, so we define it here too. ROLE_LINK
  @"AXHelpTag",                                 // ROLE_HELPBALLOON
  NSAccessibilityUnknownRole,                   // ROLE_CHARACTER. unusued on OS X
  NSAccessibilityListRole,                      // ROLE_LIST
  NSAccessibilityRowRole,                       // ROLE_LISTITEM
  NSAccessibilityOutlineRole,                   // ROLE_OUTLINE
  NSAccessibilityRowRole,                       // ROLE_OUTLINEITEM. XXX: use OutlineRow as subrole.
  NSAccessibilityGroupRole,                     // ROLE_PAGETAB
  NSAccessibilityGroupRole,                     // ROLE_PROPERTYPAGE
  NSAccessibilityUnknownRole,                   // ROLE_INDICATOR
  NSAccessibilityImageRole,                     // ROLE_GRAPHIC
  NSAccessibilityStaticTextRole,                // ROLE_STATICTEXT
  NSAccessibilityStaticTextRole,                // ROLE_TEXT_LEAF
  NSAccessibilityButtonRole,                    // ROLE_PUSHBUTTON
  NSAccessibilityCheckBoxRole,                  // ROLE_CHECKBUTTON
  NSAccessibilityRadioButtonRole,               // ROLE_RADIOBUTTON
  NSAccessibilityPopUpButtonRole,               // ROLE_COMBOBOX
  NSAccessibilityPopUpButtonRole,               // ROLE_DROPLIST.
  NSAccessibilityProgressIndicatorRole,         // ROLE_PROGRESSBAR
  NSAccessibilityUnknownRole,                   // ROLE_DIAL
  NSAccessibilityUnknownRole,                   // ROLE_HOTKEYFIELD
  NSAccessibilitySliderRole,                    // ROLE_SLIDER
  NSAccessibilityIncrementorRole,               // ROLE_SPINBUTTON. subroles: Increment/Decrement.
  NSAccessibilityUnknownRole,                   // ROLE_DIAGRAM
  NSAccessibilityUnknownRole,                   // ROLE_ANIMATION
  NSAccessibilityUnknownRole,                   // ROLE_EQUATION
  NSAccessibilityPopUpButtonRole,               // ROLE_BUTTONDROPDOWN.
  NSAccessibilityMenuButtonRole,                // ROLE_BUTTONMENU
  NSAccessibilityGroupRole,                     // ROLE_BUTTONDROPDOWNGRID
  NSAccessibilityUnknownRole,                   // ROLE_WHITESPACE
  NSAccessibilityGroupRole,                     // ROLE_PAGETABLIST
  NSAccessibilityUnknownRole,                   // ROLE_CLOCK. unused on OS X
  NSAccessibilityButtonRole,                    // ROLE_SPLITBUTTON
  NSAccessibilityUnknownRole,                   // ROLE_IPADDRESS
  NSAccessibilityStaticTextRole,                // ROLE_ACCEL_LABEL
  NSAccessibilityUnknownRole,                   // ROLE_ARROW
  NSAccessibilityImageRole,                     // ROLE_CANVAS
  NSAccessibilityMenuItemRole,                  // ROLE_CHECK_MENU_ITEM
  NSAccessibilityColorWellRole,                 // ROLE_COLOR_CHOOSER
  NSAccessibilityUnknownRole,                   // ROLE_DATE_EDITOR
  NSAccessibilityImageRole,                     // ROLE_DESKTOP_ICON
  NSAccessibilityUnknownRole,                   // ROLE_DESKTOP_FRAME
  NSAccessibilityBrowserRole,                   // ROLE_DIRECTORY_PANE
  NSAccessibilityUnknownRole,                   // ROLE_FILE_CHOOSER. unused on OS X
  NSAccessibilityUnknownRole,                   // ROLE_FONT_CHOOSER
  NSAccessibilityUnknownRole,                   // ROLE_CHROME_WINDOW. unused on OS X
  NSAccessibilityGroupRole,                     // ROLE_GLASS_PANE
  NSAccessibilityUnknownRole,                   // ROLE_HTML_CONTAINER
  NSAccessibilityImageRole,                     // ROLE_ICON
  NSAccessibilityStaticTextRole,                // ROLE_LABEL
  NSAccessibilityGroupRole,                     // ROLE_LAYERED_PANE
  NSAccessibilityGroupRole,                     // ROLE_OPTION_PANE
  NSAccessibilityTextFieldRole,                 // ROLE_PASSWORD_TEXT
  NSAccessibilityUnknownRole,                   // ROLE_POPUP_MENU. unused
  NSAccessibilityMenuItemRole,                  // ROLE_RADIO_MENU_ITEM
  NSAccessibilityGroupRole,                     // ROLE_ROOT_PANE
  NSAccessibilityScrollAreaRole,                // ROLE_SCROLL_PANE
  NSAccessibilitySplitGroupRole,                // ROLE_SPLIT_PANE
  NSAccessibilityUnknownRole,                   // ROLE_TABLE_COLUMN_HEADER
  NSAccessibilityUnknownRole,                   // ROLE_TABLE_ROW_HEADER
  NSAccessibilityMenuItemRole,                  // ROLE_TEAR_OFF_MENU_ITEM
  NSAccessibilityUnknownRole,                   // ROLE_TERMINAL
  NSAccessibilityGroupRole,                     // ROLE_TEXT_CONTAINER
  NSAccessibilityButtonRole,                    // ROLE_TOGGLE_BUTTON
  NSAccessibilityTableRole,                     // ROLE_TREE_TABLE
  NSAccessibilityUnknownRole,                   // ROLE_VIEWPORT
  NSAccessibilityGroupRole,                     // ROLE_HEADER
  NSAccessibilityGroupRole,                     // ROLE_FOOTER
  NSAccessibilityGroupRole,                     // ROLE_PARAGRAPH
  @"AXRuler",                                   // ROLE_RULER. 10.4+ only, so we re-define the constant.
  NSAccessibilityComboBoxRole,                  // ROLE_AUTOCOMPLETE
  NSAccessibilityTextFieldRole,                 // ROLE_EDITBAR
  NSAccessibilityTextFieldRole,                 // ROLE_ENTRY
  NSAccessibilityStaticTextRole,                // ROLE_CAPTION
  @"AXWebArea",                                 // ROLE_DOCUMENT_FRAME
  NSAccessibilityStaticTextRole,                // ROLE_HEADING
  NSAccessibilityGroupRole,                     // ROLE_PAGE
  NSAccessibilityGroupRole,                     // ROLE_SECTION
  NSAccessibilityUnknownRole,                   // ROLE_REDUNDANT_OBJECT
  NSAccessibilityGroupRole,                     // ROLE_FORM
  NSAccessibilityUnknownRole,                   // ROLE_IME
  NSAccessibilityUnknownRole,                   // ROLE_APP_ROOT. unused on OS X
  NSAccessibilityMenuItemRole,                  // ROLE_PARENT_MENUITEM
  NSAccessibilityGroupRole,                     // ROLE_CALENDAR
  NSAccessibilityMenuRole,                      // ROLE_COMBOBOX_LIST
  NSAccessibilityMenuItemRole,                  // ROLE_COMBOBOX_OPTION
  NSAccessibilityImageRole,                     // ROLE_IMAGE_MAP
  NSAccessibilityRowRole,                       // ROLE_OPTION
  NSAccessibilityRowRole,                       // ROLE_RICH_OPTION
  NSAccessibilityListRole,                      // ROLE_LISTBOX
  NSAccessibilityUnknownRole,                   // ROLE_FLAT_EQUATION
  NSAccessibilityGroupRole,                     // ROLE_GRID_CELL
  NSAccessibilityGroupRole,                     // ROLE_EMBEDDED_OBJECT
  NSAccessibilityGroupRole,                     // ROLE_NOTE
  @"ROLE_LAST_ENTRY"                            // ROLE_LAST_ENTRY. bogus role that will never be shown (just marks the end of this array)!
};
