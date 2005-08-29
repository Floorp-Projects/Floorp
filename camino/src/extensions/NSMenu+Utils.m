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
 *   Simon Fraser <sfraser@netscape.com>
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

#import <Carbon/Carbon.h>

#import "NSResponder+Utils.h"

#import "NSMenu+Utils.h"


NSString* const NSMenuWillDisplayNotification = @"NSMenuWillDisplayNotification";
NSString* const NSMenuClosedNotification      = @"NSMenuClosedNotification";

// internal API
extern MenuRef _NSGetCarbonMenu(NSMenu* aMenu);

static OSStatus MenuEventHandler(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
  UInt32 eventKind = GetEventKind(inEvent);
  switch (eventKind)
  {
    case kEventMenuOpening:
    case kEventMenuClosed:
      {
        MenuRef theCarbonMenu;
        OSStatus err = GetEventParameter(inEvent, kEventParamDirectObject, typeMenuRef, NULL, sizeof(MenuRef), NULL, &theCarbonMenu);
        if (err == noErr)
        {
          NS_DURING
            // we can't map from MenuRef to NSMenu, so we have to let receivers of the notification
            // do the test.
            NSString* notificationName = @"";
            if (eventKind == kEventMenuOpening)
              notificationName = NSMenuWillDisplayNotification;
            else if (eventKind == kEventMenuClosed)
              notificationName = NSMenuClosedNotification;
            
            [[NSNotificationCenter defaultCenter] postNotificationName:notificationName
                                                                object:[NSValue valueWithPointer:theCarbonMenu]];
          NS_HANDLER
            NSLog(@"Caught exception %@", localException);
          NS_ENDHANDLER
        }
      }
      break;
  }

  // always let the event propagate  
  return eventNotHandledErr;
}

#pragma mark -

@implementation NSMenu(ChimeraMenuUtils)

+ (void)setupMenuWillDisplayNotifications
{
  static BOOL sInstalled = NO;
  
  if (!sInstalled)
  {
    const EventTypeSpec menuEventList[] = {
      { kEventClassMenu, kEventMenuOpening },
      { kEventClassMenu, kEventMenuClosed  }
    };

    InstallApplicationEventHandler(NewEventHandlerUPP(MenuEventHandler), 
                                   GetEventTypeCount(menuEventList),
                                   menuEventList, (void*)self, NULL);
    sInstalled = YES;
  }
}

- (void)checkItemWithTag:(int)tag uncheckingOtherItems:(BOOL)uncheckOthers
{
  if (uncheckOthers)
  {
    NSEnumerator* itemsEnum = [[self itemArray] objectEnumerator];
    NSMenuItem* curItem;
    while ((curItem = (NSMenuItem*)[itemsEnum nextObject]))
      [curItem setState:NSOffState];
  }
  [[self itemWithTag:tag] setState:NSOnState];
}

- (void)checkItemWithTag:(int)unmaskedTag inGroupWithMask:(int)tagMask
{
  NSEnumerator* itemsEnum = [[self itemArray] objectEnumerator];
  NSMenuItem* curItem;
  while ((curItem = (NSMenuItem*)[itemsEnum nextObject]))
  {
    int itemTag = [curItem tag];
    if ((itemTag & tagMask) == tagMask)
    {
      int rawTag = (itemTag & ~tagMask);
      [curItem setState:(rawTag == unmaskedTag) ? NSOnState : NSOffState];
    }
  }
}

- (void)setAllItemsEnabled:(BOOL)inEnable startingWithItemAtIndex:(int)inFirstItem includingSubmenus:(BOOL)includeSubmenus
{
  NSArray* menuItems = [self itemArray];

  unsigned int i;
  for (i = inFirstItem; i < [menuItems count]; i ++)
  {
    id<NSMenuItem> curItem = [self itemAtIndex:i];
    [curItem setEnabled:inEnable];
    if (includeSubmenus && [curItem hasSubmenu])
    {
      [[curItem submenu] setAllItemsEnabled:inEnable startingWithItemAtIndex:0 includingSubmenus:includeSubmenus];
    }
  }
}

- (id<NSMenuItem>)itemWithTarget:(id)anObject andAction:(SEL)actionSelector
{
  int itemIndex = [self indexOfItemWithTarget:anObject andAction:actionSelector];
  return [self itemAtIndex:itemIndex];
}

- (void)removeItemsAfterItem:(id<NSMenuItem>)inItem
{
  int firstItemToRemoveIndex = 0;

  if (inItem)
    firstItemToRemoveIndex = [self indexOfItem:inItem] + 1;

  [self removeItemsFromIndex:firstItemToRemoveIndex];
}

- (void)removeItemsFromIndex:(int)inItemIndex
{
  if (inItemIndex < 0)
    inItemIndex = 0;

  while ([self numberOfItems] > inItemIndex)
    [self removeItemAtIndex:inItemIndex];
}

// because there's no way to map back from a MenuRef to a Cocoa NSMenu, we have
// to let receivers of the notification do the test by calling this method.
- (BOOL)isTargetOfMenuDisplayNotification:(id)inObject
{
  return ([inObject pointerValue] == _NSGetCarbonMenu(self));
}

@end


@implementation NSMenuItem(ChimeraMenuItemUtils)

- (int)tagRemovingMask:(int)tagMask
{
  return ([self tag] & ~tagMask);
}

- (void)takeStateFromItem:(id<NSMenuItem>)inItem
{
  [self setTitle:[inItem title]];
  [self setEnabled:[inItem isEnabled]];
}

@end
