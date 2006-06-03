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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
 *   David Haas <haasd@cae.wisc.edu>
 *   Josh Aas <josha@mac.com>
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

#import "BookmarkViewController.h"
#import "BookmarkOutlineView.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "BookmarkManager.h"
#import "NSPasteboard+Utils.h"

@implementation BookmarkOutlineView

- (void)awakeFromNib
{
  [self registerForDraggedTypes:[NSArray arrayWithObjects:kCaminoBookmarkListPBoardType, kWebURLsWithTitlesPboardType, NSStringPboardType, NSURLPboardType, nil]];
}

-(NSMenu*)menu
{
  BookmarkManager *bm = [BookmarkManager sharedBookmarkManager];
  BookmarkFolder *activeCollection = [(BookmarkViewController*)[self delegate] activeCollection];
  // only give a default menu if its the bookmark menu or toolbar
  if ((activeCollection == [bm bookmarkMenuFolder]) || (activeCollection == [bm toolbarFolder])) {
    // set up default menu
    NSMenu *menu = [[[NSMenu alloc] init] autorelease];
    NSMenuItem *menuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Create New Folder...", @"")
                                                       action:@selector(addBookmarkFolder:)
                                                keyEquivalent:@""] autorelease];
    [menuItem setTarget:[self delegate]];
    [menu addItem:menuItem];
    return menu;
  }
  return nil;
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
  if (operation == NSDragOperationDelete) {
    NSPasteboard* pboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    NSArray* bookmarks = [BookmarkManager bookmarkItemsFromSerializableArray:[pboard propertyListForType: kCaminoBookmarkListPBoardType]];
    if (bookmarks) {
      for (unsigned int i = 0; i < [bookmarks count]; ++i) {
        BookmarkItem* item = [bookmarks objectAtIndex:i];
        [[item parent] deleteChild:item];
      }
    }
  }
}

// don't edit URL field of folders or menu separators
- (void)_editItem:(id)dummy
{
  id itemToEdit = [self itemAtRow:mRowToBeEdited];
  if ([itemToEdit isKindOfClass:[BookmarkFolder class]]) {
    if (mColumnToBeEdited == [self columnWithIdentifier:@"url"]) {
      [super _cancelEditItem];
      return;
    }
  } else if ([itemToEdit isKindOfClass:[Bookmark class]]) {
    if ([(Bookmark *)itemToEdit isSeparator]) {
      [super _cancelEditItem];
      return;
    }
  }
  [super _editItem:dummy];
}

//
// Override implementation in ExtendedOutlineView so we can check whether an
// item is selected or whether appropriate data is available on the clipboard.
//
- (BOOL)validateMenuItem:(id)aMenuItem
{
  SEL action = [aMenuItem action];

  if (action == @selector(delete:))
    return [[self delegate] numberOfSelectedRows] > 0;

  if (action == @selector(copy:))
    return [super validateMenuItem:aMenuItem] && [[self delegate] numberOfSelectedRows] > 0;

  if (action == @selector(paste:))
    return [super validateMenuItem:aMenuItem] && [[self delegate] canPasteFromPasteboard:[NSPasteboard generalPasteboard]];

  if (action == @selector(cut:))
    return NO; // XXX fix me [super validateMenuItem:aMenuItem] && [[self delegate] numberOfSelectedRows] > 0;
  
  return YES;
}

- (IBAction)delete:(id)aSender
{
  [[self delegate] deleteBookmarks:aSender];
}

@end
