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
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Fraser <smfr@smfr.org>
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

// notification fire before a menu is displayed. the notification object
// is an opaque id which should be compared with a NSMenu using -isTargetOfWillDisplayNotification
extern NSString* const NSMenuWillDisplayNotification;
extern NSString* const NSMenuClosedNotification;

@interface NSMenu(ChimeraMenuUtils)

// turn on "NSMenuWillDisplayNotification" firing
+ (void)setupMenuWillDisplayNotifications;

// check one item on a menu, optionally unchecking all the others
- (void)checkItemWithTag:(int)tag uncheckingOtherItems:(BOOL)uncheckOthers;

// treat a set of items each sharing the same tagMask as a radio group,
// turning on the one with the given unmasked tag value.
- (void)checkItemWithTag:(int)unmaskedTag inGroupWithMask:(int)tagMask;

// enable or disable all items in the menu including and after inFirstItem,
// optionally recursing into submenus.
- (void)setAllItemsEnabled:(BOOL)inEnable startingWithItemAtIndex:(int)inFirstItem includingSubmenus:(BOOL)includeSubmenus;

// return the first item (if any) with the given target and action.
- (id<NSMenuItem>)itemWithTarget:(id)anObject andAction:(SEL)actionSelector;

// remove items after the given item, or all items if nil
- (void)removeItemsAfterItem:(id<NSMenuItem>)inItem;

// remove all items including and after the given index (i.e. all items if index is 0)
- (void)removeItemsFromIndex:(int)inItemIndex;

// return YES if this menu is the target of the 'will display' notification.
// the param should be the [NSNotification object]
- (BOOL)isTargetOfMenuDisplayNotification:(id)inObject;

@end


@interface NSMenuItem(ChimeraMenuItemUtils)

- (int)tagRemovingMask:(int)tagMask;

// copy the title and enabled state from the given item
- (void)takeStateFromItem:(id<NSMenuItem>)inItem;

@end
