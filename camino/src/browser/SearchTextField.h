/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is SearchTextField UI code.
 *
 * The Initial Developer of the Original Code is
 * Prachi Gauriar.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Prachi Gauriar (pgauria@uark.edu)
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

#import <AppKit/AppKit.h>

@class SearchTextFieldCell;

@interface SearchTextField : NSTextField
{
  IBOutlet NSMenu*        mPopupMenu;
}

+ (Class)cellClass;

- (void)awakeFromNib;
- (void)mouseDown:(NSEvent *)theEvent;
- (void)resetCursorRects;

- (BOOL)isFirstResponder;
- (BOOL)becomeFirstResponder;
- (void)selectText:(id)sender;

- (BOOL)hasPopUpButton;
- (void)setHasPopUpButton:(BOOL)aBoolean;

- (NSString *)titleOfSelectedPopUpItem;

  // Use this method to add menu items to the list
  // If you need to access menu items, message the control's cell

  //  DO NOT EVER ACCESS THE MENU ITEM AT INDEX 0
  //  IT IS A BLANK ITEM REQUIRED SO THAT THE MENU
  //  DISPLAYS CORRECTLY
- (void)addPopUpMenuItemsWithTitles:(NSArray *)itemTitles;

// note that the first menu item is ignored
- (void)setPopupMenu:(NSMenu*)menu;
- (NSMenu*)popupMenu;

- (void)selectPopupMenuItem:(NSMenuItem*)menuItem;
- (NSMenuItem*)selectedPopupMenuItem;

@end
