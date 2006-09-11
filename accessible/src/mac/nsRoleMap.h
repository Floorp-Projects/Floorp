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

#import <Foundation/Foundation.h>

const NSString* AXRoles [] = {
  @"ROLE_NOTHING", // ROLE_NOTHING
  @"ROLE_TITLEBAR", // ROLE_TITLEBAR
  @"ROLE_MENUBAR", // ROLE_MENUBAR
  @"ROLE_SCROLLBAR", // ROLE_SCROLLBAR
  @"ROLE_GRIP", // ROLE_GRIP
  @"ROLE_SOUND", // ROLE_SOUND
  @"ROLE_CURSOR", // ROLE_CURSOR
  @"ROLE_CARET", // ROLE_CARET
  @"ROLE_ALERT", // ROLE_ALERT
  @"ROLE_WINDOW", // ROLE_WINDOW
  @"ROLE_CLIENT", // ROLE_CLIENT
  @"ROLE_MENUPOPUP", // ROLE_MENUPOPUP
  @"ROLE_MENUITEM", // ROLE_MENUITEM
  @"ROLE_TOOLTIP", // ROLE_TOOLTIP
  @"ROLE_APP", // ROLE_APP
  @"ROLE_DOCUMENT", // ROLE_DOCUMENT
  @"ROLE_PANE", // ROLE_PANE
  @"ROLE_CHART", // ROLE_CHART
  @"ROLE_DIALOG", // ROLE_DIALOG
  @"ROLE_BORDER", // ROLE_BORDER
  @"ROLE_GROUPING", // ROLE_GROUPING
  @"ROLE_SEPARATOR", // ROLE_SEPARATOR
  @"ROLE_TOOLBAR", // ROLE_TOOLBAR
  @"ROLE_STATUSBAR", // ROLE_STATUSBAR
  @"ROLE_TABLE", // ROLE_TABLE
  @"ROLE_COLUMNHEADER", // ROLE_COLUMNHEADER
  @"ROLE_ROWHEADER", // ROLE_ROWHEADER
  @"ROLE_COLUMN", // ROLE_COLUMN
  @"ROLE_ROW", // ROLE_ROW
  @"ROLE_CELL", // ROLE_CELL
  @"ROLE_LINK", // ROLE_LINK
  @"ROLE_HELPBALLOON", // ROLE_HELPBALLOON
  @"ROLE_CHARACTER", // ROLE_CHARACTER
  @"ROLE_LIST", // ROLE_LIST
  @"ROLE_LISTITEM", // ROLE_LISTITEM
  @"ROLE_OUTLINE", // ROLE_OUTLINE
  @"ROLE_OUTLINEITEM", // ROLE_OUTLINEITEM
  @"ROLE_PAGETAB", // ROLE_PAGETAB
  @"ROLE_PROPERTYPAGE", // ROLE_PROPERTYPAGE
  @"ROLE_INDICATOR", // ROLE_INDICATOR
  @"ROLE_GRAPHIC", // ROLE_GRAPHIC
  @"ROLE_STATICTEXT", // ROLE_STATICTEXT
  @"ROLE_TEXT", // ROLE_TEXT
  @"ROLE_PUSHBUTTON", // ROLE_PUSHBUTTON
  @"ROLE_CHECKBUTTON", // ROLE_CHECKBUTTON
  @"ROLE_RADIOBUTTON", // ROLE_RADIOBUTTON
  @"ROLE_COMBOBOX", // ROLE_COMBOBOX
  @"ROLE_DROPLIST", // ROLE_DROPLIST
  @"ROLE_PROGRESSBAR", // ROLE_PROGRESSBAR
  @"ROLE_DIAL", // ROLE_DIAL
  @"ROLE_HOTKEYFIELD", // ROLE_HOTKEYFIELD
  @"ROLE_SLIDER", // ROLE_SLIDER
  @"ROLE_SPINBUTTON", // ROLE_SPINBUTTON
  @"ROLE_DIAGRAM", // ROLE_DIAGRAM
  @"ROLE_ANIMATION", // ROLE_ANIMATION
  @"ROLE_EQUATION", // ROLE_EQUATION
  @"ROLE_BUTTONDROPDOWN", // ROLE_BUTTONDROPDOWN
  @"ROLE_BUTTONMENU", // ROLE_BUTTONMENU
  @"ROLE_BUTTONDROPDOWNGRID", // ROLE_BUTTONDROPDOWNGRID
  @"ROLE_WHITESPACE", // ROLE_WHITESPACE
  @"ROLE_PAGETABLIST", // ROLE_PAGETABLIST
  @"ROLE_CLOCK", // ROLE_CLOCK
  @"ROLE_SPLITBUTTON", // ROLE_SPLITBUTTON
  @"ROLE_IPADDRESS", // ROLE_IPADDRESS
  @"ROLE_ACCEL", // ROLE_ACCEL
  @"ROLE_ARROW", // ROLE_ARROW
  @"ROLE_CANVAS", // ROLE_CANVAS
  @"ROLE_CHECK", // ROLE_CHECK
  @"ROLE_COLOR", // ROLE_COLOR
  @"ROLE_DATE", // ROLE_DATE
  @"ROLE_DESKTOP", // ROLE_DESKTOP
  @"ROLE_DESKTOP", // ROLE_DESKTOP
  @"ROLE_DIRECTORY", // ROLE_DIRECTORY
  @"ROLE_FILE", // ROLE_FILE
  @"ROLE_FILLER", // ROLE_FILLER
  @"ROLE_FONT", // ROLE_FONT
  @"ROLE_CHROME", // ROLE_CHROME
  @"ROLE_GLASS", // ROLE_GLASS
  @"ROLE_HTML", // ROLE_HTML
  @"ROLE_ICON", // ROLE_ICON
  @"ROLE_INTERNAL", // ROLE_INTERNAL
  @"ROLE_LABEL", // ROLE_LABEL
  @"ROLE_LAYERED", // ROLE_LAYERED
  @"ROLE_OPTION", // ROLE_OPTION
  @"ROLE_PASSWORD", // ROLE_PASSWORD
  @"ROLE_POPUP", // ROLE_POPUP
  @"ROLE_RADIO", // ROLE_RADIO
  @"ROLE_ROOT", // ROLE_ROOT
  @"ROLE_SCROLL", // ROLE_SCROLL
  @"ROLE_SPLIT", // ROLE_SPLIT
  @"ROLE_TABLE", // ROLE_TABLE
  @"ROLE_TABLE", // ROLE_TABLE
  @"ROLE_TEAR", // ROLE_TEAR
  @"ROLE_TERMINAL", // ROLE_TERMINAL
  @"ROLE_TEXT", // ROLE_TEXT
  @"ROLE_TOGGLE", // ROLE_TOGGLE
  @"ROLE_TREE", // ROLE_TREE
  @"ROLE_VIEWPORT", // ROLE_VIEWPORT
  @"ROLE_HEADER", // ROLE_HEADER
  @"ROLE_FOOTER", // ROLE_FOOTER
  @"ROLE_PARAGRAPH", // ROLE_PARAGRAPH
  @"ROLE_RULER", // ROLE_RULER
  @"ROLE_AUTOCOMPLETE", // ROLE_AUTOCOMPLETE
  @"ROLE_EDITBAR", // ROLE_EDITBAR
  @"ROLE_EMBEDDED", // ROLE_EMBEDDED
  @"ROLE_ENTRY", // ROLE_ENTRY
  @"ROLE_CAPTION", // ROLE_CAPTION
  @"ROLE_DOCUMENT", // ROLE_DOCUMENT
  @"ROLE_HEADING", // ROLE_HEADING
  @"ROLE_PAGE", // ROLE_PAGE
  @"ROLE_SECTION", // ROLE_SECTION
  @"ROLE_REDUNDANT", // ROLE_REDUNDANT
  @"ROLE_FORM", // ROLE_FORM
  @"ROLE_IME", // ROLE_IME
  @"ROLE_APP", // ROLE_APP
  @"ROLE_PARENT", // ROLE_PARENT
  @"ROLE_LAST" // ROLE_LAST
};
