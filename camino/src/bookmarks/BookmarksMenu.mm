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

#import "NSString+Utils.h"

#import "BookmarksMenu.h"

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsIAtom.h"

@interface BookmarksMenu(Private)

- (NSMenu*)locateMenuForContent:(nsIContent*)content;
- (void)addBookmark:(NSMenu*)menu parent:(nsIContent*)parent child:(nsIContent*)child index:(int)index;
- (void)constructMenu:(NSMenu*)menu content:(nsIContent*)content;

- (void)buildFoldersPopup:(NSPopUpButton*)popup curMenu:(NSMenu*)menu selectTag:(int)tagToMatch depth:(int)depth;

@end


@implementation BookmarksMenu

- (id)initWithMenu:(NSMenu*)menu firstItem:(int)firstItem rootContent:(nsIContent*)content watchedFolder:(EBookmarksFolderType)folderType
{
  if ((self = [super init]))
  {
    mMenu = [menu retain];
    mFirstItemIndex = firstItem;
    mRootContent = content;
    mWatchedFolderType = folderType;
    NS_IF_ADDREF(mRootContent);
    [self constructMenu:mMenu content:mRootContent];
  }
  
  return self;
}

- (void)dealloc
{
  [mMenu release];
  NS_IF_RELEASE(mRootContent);
  [super dealloc];
}

- (void)constructMenu:(NSMenu*)menu content:(nsIContent*)content
{
  // remove existing items
  while ([menu numberOfItems] > mFirstItemIndex)
    [menu removeItemAtIndex:mFirstItemIndex];

  if (!content) return;

  // Now walk our children, and for folders also recur into them.
  PRInt32 childCount;
  content->ChildCount(childCount);
  
  for (PRInt32 i = 0; i < childCount; i++)
  {
    nsCOMPtr<nsIContent> child;
    content->ChildAt(i, *getter_AddRefs(child));
    [self addBookmark:menu parent:content child:child index:-1];
  }
}

- (void)addBookmark:(NSMenu*)menu parent:(nsIContent*)parent child:(nsIContent*)child index:(int)index
{
  if (!child) return;
    
  nsAutoString name;
  child->GetAttr(kNameSpaceID_None, BookmarksService::gNameAtom, name);
  NSString* title = [[NSString stringWith_nsAString: name] stringByTruncatingTo:80 at:kTruncateAtMiddle];

  nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(child);
  NSImage* menuItemImage = BookmarksService::CreateIconForBookmark(elt);
  
  // ensure a wrapper exists
  [[BookmarksManager sharedBookmarksManager] getWrapperForContent:child];

  nsCOMPtr<nsIAtom> tagName;
  child->GetTag(*getter_AddRefs(tagName));

  nsAutoString group;
  if (tagName == BookmarksService::gFolderAtom)
    child->GetAttr(kNameSpaceID_None, BookmarksService::gGroupAtom, group);

  BOOL isFolder = (tagName == BookmarksService::gFolderAtom);
  BOOL isGroup  = isFolder && !group.IsEmpty();

  // Create a menu or menu item for the child.
  NSMenuItem* menuItem = [[[NSMenuItem alloc] initWithTitle: title action: NULL keyEquivalent: @""] autorelease];
  if (index == -1)
    [menu addItem: menuItem];
  else
  {
    PRInt32 insertIndex = index;
    if (menu == mMenu)	// take static menu items into account
      insertIndex += mFirstItemIndex;
    
    [menu insertItem:menuItem atIndex:insertIndex];
  }

  PRUint32 contentID;
  child->GetContentID(&contentID);
  [menuItem setTag: contentID];
  
  if (isFolder && !isGroup) // folder
  {
    NSMenu* subMenu = [[[NSMenu alloc] initWithTitle: title] autorelease];
    [menu setSubmenu: subMenu forItem: menuItem];
    [subMenu setAutoenablesItems: NO];
    [menuItem setImage: menuItemImage];
    [self constructMenu:subMenu content:child];
  }
  else	// non-empty group or bookmark
  {
    [menuItem setImage: menuItemImage];
    [menuItem setTarget: [NSApp delegate]];
    [menuItem setAction: @selector(openMenuBookmark:)];
  }
}

- (NSMenu*)locateMenuForContent:(nsIContent*)content
{
  nsCOMPtr<nsIContent> parent;
  if (content == mRootContent)
    return mMenu;

  content->GetParent(*getter_AddRefs(parent));
  if (!parent)
    return nil;		// not in our subtree

  NSMenu* parentMenu = [self locateMenuForContent:parent];
  if (!parentMenu)
    return nil;

  PRUint32 contentID;
  content->GetContentID(&contentID);

  NSMenuItem* childMenu = [parentMenu itemWithTag: contentID];
  return [childMenu submenu];
}

#pragma mark -

- (void)bookmarkAdded:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
  if (!container) return;
  if (!isRoot) return;			// this code reconstructs submenus itself
    
  PRInt32 index = -1;
  container->IndexOf(bookmark, index);
  NSMenu* menu = [self locateMenuForContent:container];
  if (menu)
    [self addBookmark:menu parent:container child:bookmark index:index];
}

- (void)bookmarkRemoved:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
  if (!bookmark) return;
  if (!isRoot) return;			// this code reconstructs submenus itself

  if (bookmark == mRootContent)
  {
    NS_IF_RELEASE(mRootContent);		// nulls it out
    // we should have got a 'specialFolder' notification to reset the root already.
#if DEBUG
    NSLog(@"bookmarkRemoved called with root folder. Nuking menu");
#endif    
    [self constructMenu:mMenu content:mRootContent];
    return;
  }
  
  PRUint32 contentID = 0;
  bookmark->GetContentID(&contentID);

  NSMenu* menu = [self locateMenuForContent:container];
  if (!menu) return;

  NSMenuItem* childItem = [menu itemWithTag: contentID];
  if (childItem)
    [menu removeItem: childItem];
}

- (void)bookmarkChanged:(nsIContent*)bookmark
{
  if (!bookmark) return;

  nsCOMPtr<nsIContent> parent;
  bookmark->GetParent(*getter_AddRefs(parent));

  PRUint32 contentID = 0;
  bookmark->GetContentID(&contentID);
  NSMenu* menu = [self locateMenuForContent:parent];
  if (!menu) return;
  
  NSMenuItem* menuItem = [menu itemWithTag: contentID];

  nsAutoString name;
  bookmark->GetAttr(kNameSpaceID_None, BookmarksService::gNameAtom, name);

  NSString* bookmarkTitle = [[NSString stringWith_nsAString: name] stringByTruncatingTo:80 at:kTruncateAtMiddle];
  [menuItem setTitle: bookmarkTitle];
  
  // and reset the image
  nsCOMPtr<nsIDOMElement> elt = do_QueryInterface(bookmark);
  NSImage* menuItemImage = BookmarksService::CreateIconForBookmark(elt);
  [menuItem setImage:menuItemImage];
}

- (void)specialFolder:(EBookmarksFolderType)folderType changedTo:(nsIContent*)newFolderContent
{
  if (folderType == mWatchedFolderType)
  {
    NS_IF_RELEASE(mRootContent);
    mRootContent = newFolderContent;
    NS_IF_ADDREF(mRootContent);
    
    [self constructMenu:mMenu content:mRootContent];
  }
}

@end
