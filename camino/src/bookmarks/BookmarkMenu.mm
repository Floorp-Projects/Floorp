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
*   David Haas   <haasd@cae.wisc.edu>
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

#import "BookmarkManager.h"
#import "BookmarkMenu.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "NSString+Utils.h"

// Definitions
#define MENU_TRUNCATION_CHARS 60

// used to determine if we've already added "Open In Tabs" to a menu. This will be on
// the "Open In Tabs" item, not the separator.
const long kOpenInTabsTag = 0xBEEF;

@interface BookmarkMenu(Private)
- (NSMenu *)menu;
- (BookmarkFolder *)rootBookmarkFolder;
- (int)firstItemIndex;
- (void)setFirstItemIndex:(int)anIndex;
- (void)setRootBookmarkFolder:(BookmarkFolder *)anArray;
- (NSMenu *)locateMenuForItem:(BookmarkItem *)anItem;
- (void)constructMenu:(NSMenu *)menu forBookmarkFolder:(BookmarkFolder *)aFolder;
- (void)flushMenu;
- (void)addItem:(BookmarkItem *)anItem toMenu:(NSMenu *)aMenu atIndex:(int)aIndex;
- (void)dockMenuChanged:(NSNotification *)note;
@end

@implementation BookmarkMenu
// init & dealloc

- (id)initWithMenu:(NSMenu *)aMenu firstItem:(int)anIndex rootBookmarkFolder:(BookmarkFolder *)aFolder
{
  if ((self = [super init]))
  {
    mMenu = [aMenu retain];
    mFirstItemIndex = anIndex;
    [self setRootBookmarkFolder:aFolder];
    [self constructMenu:mMenu forBookmarkFolder:aFolder];
    // Generic notifications for Bookmark Client
    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self selector:@selector(bookmarkAdded:) name:BookmarkFolderAdditionNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:nil];
    [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkIconChangedNotification object:nil];
    if (aFolder == [[BookmarkManager sharedBookmarkManager] dockMenuFolder])
      [nc addObserver:self selector:@selector(dockMenuChanged:) name:BookmarkFolderDockMenuChangeNotificaton object:nil];
  }
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mMenu release];
  [mRootFolder release];
  [super dealloc];
}

// Getters & setters 
-(NSMenu *)menu
{
  return mMenu;
}

-(BookmarkFolder *)rootBookmarkFolder
{
  return mRootFolder;
}

-(int)firstItemIndex
{
  return mFirstItemIndex;
}

-(void)setRootBookmarkFolder:(BookmarkFolder *)aFolder
{
  [aFolder retain];
  [mRootFolder release];
  mRootFolder = aFolder;
}

-(void)setFirstItemIndex:(int)anIndex
{
  mFirstItemIndex=anIndex;
}

//
// Utility methods
//

- (void)flushMenu
{
  int firstItemIndex = [self firstItemIndex];
  NSMenu *menu = [self menu];
  while ([menu numberOfItems] > firstItemIndex)
    [menu removeItemAtIndex:firstItemIndex];
}

//
// -addOpenInTabsToMenu:forBookmarkFolder:
//
// Adds the "Open In Tabs" option to open all items in the folder |inFolder| as a tabgroup.
// The main bookmark menu folder doesn't get this item, nor does any container that doesn't
// have any children. We use |kOpenInTabsTag| to determine if we've already added this
// to the menu so we don't do it twice.
//
- (void)addOpenInTabsToMenu:(NSMenu*)inMenu forBookmarkFolder:(BookmarkFolder*)inFolder
{
  // first make sure it's not already present before we add it
  if ([inMenu indexOfItemWithTag:kOpenInTabsTag] != -1)
    return;
  
  // add the "Open In Tabs" option to open all items in this subfolder (not the main bookmark
  // folder) as a tabgroup.
  unsigned long childCount = [inFolder count];
  if (inFolder != [[BookmarkManager sharedBookmarkManager] bookmarkMenuFolder] && childCount > 0) {
    [inMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *menuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Open in Tabs", nil) action:nil keyEquivalent:@""] autorelease];
    [inMenu addItem:menuItem];
    [menuItem setTarget:[NSApp delegate]];
    [menuItem setTag:kOpenInTabsTag];
    [menuItem setAction:@selector(openMenuBookmark:)];
    [menuItem setRepresentedObject:inFolder];
  }
}

//
// -removeOpenInTabsFromMenu:forBookmarkFolder:
//
// Removes the "Open In Tabs" menu item and separator from |inMenu|. This is used when |inFolder|
// drops to zero children.
//
- (void)removeOpenInTabsFromMenu:(NSMenu*)inMenu forBookmarkFolder:(BookmarkFolder*)inFolder
{
  // first check if there are any children remaining. If so, bail. Then check if the
  // menu is there at all by checking for our special tag. If not, bail.
  unsigned long childCount = [inFolder count];
  if (childCount)
    return;
  long location = [inMenu indexOfItemWithTag:kOpenInTabsTag];
  if (location == -1)
    return;

  // remove the menu item and the seaparator before it
  [inMenu removeItemAtIndex:location--];
  [inMenu removeItemAtIndex:location];
}

- (void)constructMenu:(NSMenu *)menu forBookmarkFolder:(BookmarkFolder *)aFolder
{
  unsigned long childCount = [aFolder count];
  for (unsigned long i = 0; i < childCount; i++)
    [self addItem:[aFolder objectAtIndex:i] toMenu:menu atIndex:i];

  [self addOpenInTabsToMenu:menu forBookmarkFolder:aFolder];
}

- (void)addItem:(BookmarkItem *)anItem toMenu:(NSMenu *)aMenu atIndex:(int)aIndex
{
  NSMenuItem *menuItem = nil;
  NSString *title = [[anItem title] stringByTruncatingTo:MENU_TRUNCATION_CHARS at:kTruncateAtMiddle];
  unsigned realIndex = aIndex;
  if (aMenu == [self menu])
    realIndex += [self firstItemIndex];

  if ([anItem isKindOfClass:[Bookmark class]]) {
    if (![(Bookmark *)anItem isSeparator]) { // normal bookmark
      menuItem = [[NSMenuItem alloc] initWithTitle:title action: NULL keyEquivalent: @""];
      [menuItem setTarget:[NSApp delegate]];
      [menuItem setAction:@selector(openMenuBookmark:)];
      [menuItem setImage:[anItem icon]];
    } else {//separator
      menuItem = [NSMenuItem separatorItem];
    }
    [aMenu insertItem:menuItem atIndex:realIndex];
  } else if ([anItem isKindOfClass:[BookmarkFolder class]]){
    if (![(BookmarkFolder *)anItem isGroup]) { //normal folder
      menuItem = [[NSMenuItem alloc] initWithTitle:title action: NULL keyEquivalent: @""];
      [aMenu insertItem:menuItem atIndex:realIndex];
      [menuItem setImage: [anItem icon]];
      NSMenu* subMenu = [[NSMenu alloc] initWithTitle:title];
      [aMenu setSubmenu: subMenu forItem: menuItem];
      [subMenu setAutoenablesItems: NO];
      [self constructMenu:subMenu forBookmarkFolder:(BookmarkFolder *)anItem];
      [subMenu release];
    } else { //group
      menuItem = [[NSMenuItem alloc] initWithTitle:title action: NULL keyEquivalent: @""];
      [aMenu insertItem:menuItem atIndex:realIndex];
      [menuItem setTarget:[NSApp delegate]];
      [menuItem setAction:@selector(openMenuBookmark:)];
      [menuItem setImage:[anItem icon]];      
    }
  }
  [menuItem setRepresentedObject:anItem];
  if (![menuItem isSeparatorItem])
    [menuItem release];
}

- (NSMenu *)locateMenuForItem:(BookmarkItem *)anItem
{
  if (![anItem isKindOfClass:[BookmarkItem class]])
    return nil; //make sure we haven't gone to top of menu item doesn't live in.
  if (anItem == [self rootBookmarkFolder])
    return [self menu];
  NSMenu* parentMenu = [self locateMenuForItem:[anItem parent]];
  if (parentMenu) {
    int index = [parentMenu indexOfItemWithRepresentedObject:anItem];
    if (index != -1) {
      if ([anItem isKindOfClass:[BookmarkFolder class]]) {
        NSMenuItem* childMenu = [parentMenu itemAtIndex:index];
          return [childMenu submenu];
      } else if ([anItem isKindOfClass:[Bookmark class]]) 
        return parentMenu;
    }
  }
  return nil;
}

#pragma mark -
// For the BookmarksClient Protocol

- (void)bookmarkAdded:(NSNotification *)note
{
  BookmarkFolder *aFolder = [note object];
  NSMenu* menu = nil;
  if (aFolder == [self rootBookmarkFolder])
    menu = [self menu];
  else if (![aFolder isGroup]) 
    menu = [self locateMenuForItem:aFolder];
  if (menu) {
      NSDictionary *dict = [note userInfo];
      [self addItem:[dict objectForKey:BookmarkFolderChildKey] toMenu:menu atIndex:[[dict objectForKey:BookmarkFolderChildIndexKey] unsignedIntValue]];
      [self addOpenInTabsToMenu:menu forBookmarkFolder:aFolder];
  }
}

- (void)bookmarkRemoved:(NSNotification *)note
{
  BookmarkFolder *aFolder = [note object];
  NSMenu* menu = nil;
  if (aFolder == [self rootBookmarkFolder])
    menu = [self menu];
  else if (![aFolder isGroup])
    menu = [self locateMenuForItem:aFolder];
  if (menu) {
    BookmarkItem *anItem = [[note userInfo] objectForKey:BookmarkFolderChildKey];
    [menu removeItemAtIndex:[menu indexOfItemWithRepresentedObject:anItem]];
    [self removeOpenInTabsFromMenu:menu forBookmarkFolder:aFolder];
  }
}

- (void)bookmarkChanged:(NSNotification *)note
{
  BookmarkItem* anItem = [note object];
  NSMenu *menu = nil;
  BOOL isSeparator = NO;
  if ([[self rootBookmarkFolder] isSmartFolder]) 
    menu = [self menu];
  else if ([anItem isKindOfClass:[Bookmark class]]) {
    menu = [self locateMenuForItem:anItem];
    isSeparator = [(Bookmark *)anItem isSeparator];
  }
  else if ([anItem isKindOfClass:[BookmarkFolder class]]) 
    menu = [self locateMenuForItem:[anItem parent]];
  if (menu) {
    int index = [menu indexOfItemWithRepresentedObject:anItem];
    if (index != -1) {
      if (!isSeparator) {
        NSMenuItem *menuItem = [menu itemAtIndex:index];
        [menuItem setTitle:[[anItem title] stringByTruncatingTo:MENU_TRUNCATION_CHARS at:kTruncateAtMiddle]];
        [menuItem setImage:[anItem icon]];
      } else {
        [menu removeItemAtIndex:index];
        [menu insertItem:[NSMenuItem separatorItem] atIndex:index];
      }
    }
  }
}

- (void)dockMenuChanged:(NSNotification *)note
{
  BookmarkFolder *aFolder = [note object];
  [self flushMenu];
  [self setRootBookmarkFolder:aFolder];
  [self constructMenu:[self menu] forBookmarkFolder:aFolder];
}

@end
