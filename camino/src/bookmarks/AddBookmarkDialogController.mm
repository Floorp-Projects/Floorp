/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "BookmarkManager.h"

#import "BookmarkViewController.h"

#import "AddBookmarkDialogController.h"


NSString* const kAddBookmarkItemURLKey        = @"url";
NSString* const kAddBookmarkItemTitleKey      = @"title";
NSString* const kAddBookmarkItemPrimaryTabKey = @"primary";

@interface AddBookmarkDialogController(Private)

+ (NSString*)bookmarkUrlForItem:(NSDictionary*)inItem;
+ (NSString*)bookmarkTitleForItem:(NSDictionary*)inItem;
+ (NSDictionary*)primaryBookmarkItem:(NSArray*)inItems;

- (void)buildBookmarksFolderPopup;
- (void)createBookmarks;
- (void)clearState;

@end

#pragma mark -


@implementation AddBookmarkDialogController

+ (AddBookmarkDialogController*)sharedAddBookmarkDialogController
{
  static AddBookmarkDialogController* sSharedController = nil;
  if (!sSharedController)
  {
    sSharedController = [[AddBookmarkDialogController alloc] initWithWindowNibName:@"AddBookmark"];
    [sSharedController window];   // force nib loading
  }
  
  return sSharedController;
}

- (void)awakeFromNib
{
  [mTabGroupCheckbox retain];   // so we can remove it
}

- (void)dealloc
{
  [mTabGroupCheckbox release];
  
  [super dealloc];
}

- (IBAction)confirmAddBookmark:(id)sender
{
  [[self window] orderOut:self];
  [NSApp endSheet:[self window] returnCode:1];
  
  [self createBookmarks];
  [self clearState];
}

- (IBAction)cancelAddBookmark:(id)sender
{
  [[self window] orderOut:self];
  [NSApp endSheet:[self window] returnCode:1];
  [self clearState];
}

- (IBAction)parentFolderChanged:(id)sender
{
}

- (void)setDefaultParentFolder:(BookmarkFolder*)inFolder
{
  [mInitialParentFolder autorelease];
  mInitialParentFolder = [inFolder retain];
}

- (void)setBookmarkViewController:(BookmarkViewController*)inBMViewController
{
  mBookmarkViewController = inBMViewController;
}

- (void)showDialogWithLocationsAndTitles:(NSArray*)inItems isFolder:(BOOL)inIsFolder onWindow:(NSWindow*)inWindow
{
  if (!inIsFolder && [inItems count] == 0)
    return;

  [mBookmarkItems autorelease];
  mBookmarkItems = [inItems retain];
  
  mCreatingFolder = inIsFolder;
  
  // set title field
  NSString* titleString = @"";
  if (mCreatingFolder)
    titleString = NSLocalizedString(@"NewBookmarkFolder", @"");
  else
    titleString  = [AddBookmarkDialogController bookmarkTitleForItem:[AddBookmarkDialogController primaryBookmarkItem:inItems]];

  [mTitleField setStringValue:titleString];

  // setup tab checkbox
  if (!mCreatingFolder)
  {
    if (![mTabGroupCheckbox superview])
      [[[self window] contentView] addSubview:mTabGroupCheckbox];

    [mTabGroupCheckbox setEnabled:([inItems count] > 1)];
  }
  else
  {
    [mTabGroupCheckbox removeFromSuperview];
  }

  [self buildBookmarksFolderPopup];

  [NSApp beginSheet:[self window]
     modalForWindow:inWindow
      modalDelegate:self
     didEndSelector:nil
        contextInfo:nil];
}

#pragma mark -

+ (NSString*)bookmarkUrlForItem:(NSDictionary*)inItem
{
  return [inItem objectForKey:kAddBookmarkItemURLKey];
}

+ (NSString*)bookmarkTitleForItem:(NSDictionary*)inItem
{
  NSString* bookmarkTitle = [inItem objectForKey:kAddBookmarkItemTitleKey];
  bookmarkTitle  = [bookmarkTitle stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];
  if (!bookmarkTitle)
    bookmarkTitle = [AddBookmarkDialogController bookmarkUrlForItem:inItem];
  return bookmarkTitle;
}

+ (NSDictionary*)primaryBookmarkItem:(NSArray*)inItems
{
  NSEnumerator* itemsEnum = [inItems objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    if ([[curItem objectForKey:kAddBookmarkItemPrimaryTabKey] boolValue])
      return curItem;
  }
  
  if ([inItems count] > 0)
    return [inItems objectAtIndex:0];
 
  return nil;
 }

- (void)buildBookmarksFolderPopup
{
  [mParentFolderPopup removeAllItems];
  [[[BookmarkManager sharedBookmarkManager] rootBookmarks] buildFlatFolderList:[mParentFolderPopup menu] depth:1];
  
  BookmarkFolder* initialFolder = mInitialParentFolder;
  if (!initialFolder)
    initialFolder = [[BookmarkManager sharedBookmarkManager] lastUsedBookmarkFolder];

  if (initialFolder)
  {
    int initialItemIndex = [[mParentFolderPopup menu] indexOfItemWithRepresentedObject:initialFolder];
    if (initialItemIndex != -1)
      [mParentFolderPopup selectItemAtIndex:initialItemIndex];
  }
  else
  {
    [mParentFolderPopup selectItemAtIndex:0];
  }

  [mParentFolderPopup synchronizeTitleAndSelectedItem];
}

- (void)createBookmarks
{
  BookmarkFolder* parentFolder = [[mParentFolderPopup selectedItem] representedObject];
  NSString*       titleString  = [mTitleField stringValue];

  BookmarkItem*   newItem = nil;
  
  if (mCreatingFolder)
  {
    newItem = [parentFolder addBookmarkFolder:titleString inPosition:[parentFolder count] isGroup:NO];
  }
  else
  {
    if (([mBookmarkItems count] > 1) && ([mTabGroupCheckbox state] == NSOnState))
    {
      // bookmark all tabs
      BookmarkFolder* newGroup = [parentFolder addBookmarkFolder:titleString inPosition:[parentFolder count] isGroup:YES];

      unsigned int numItems = [mBookmarkItems count];
      for (unsigned int i = 0; i < numItems; i++)
      {
        id curItem = [mBookmarkItems objectAtIndex:i];
        NSString* itemURL   = [AddBookmarkDialogController bookmarkUrlForItem:curItem];
        NSString* itemTitle = [AddBookmarkDialogController bookmarkTitleForItem:curItem];

        newItem = [newGroup addBookmark:itemTitle url:itemURL inPosition:i isSeparator:NO];
      }
    }
    else
    {
      id curItem = [AddBookmarkDialogController primaryBookmarkItem:mBookmarkItems];

      NSString* itemURL   = [AddBookmarkDialogController bookmarkUrlForItem:curItem];

      newItem = [parentFolder addBookmark:titleString url:itemURL inPosition:[parentFolder count] isSeparator:NO];
    }  
  }
  
  [mBookmarkViewController revealItem:newItem scrollIntoView:YES selecting:YES byExtendingSelection:NO];
  [[BookmarkManager sharedBookmarkManager] setLastUsedBookmarkFolder:parentFolder];
}

- (void)clearState
{
  [mTitleField setStringValue:@""];
  [mTabGroupCheckbox setState:NSOffState];
  
  [mInitialParentFolder release];
  mInitialParentFolder = nil;

  mCreatingFolder = NO;

  [mBookmarkItems release];
  mBookmarkItems = nil;
  
  mBookmarkViewController = nil;
}

@end
