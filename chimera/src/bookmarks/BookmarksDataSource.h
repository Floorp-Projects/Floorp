/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *			Simon Fraser <sfraser@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#import <Foundation/Foundation.h>
#import <Appkit/Appkit.h>

#import "MainController.h"
#import "BookmarksToolbar.h"
#import "ExtendedOutlineView.h"
#import "BookmarksService.h"

// data source for the bookmarks sidebar. We make one per browser window.
@interface BookmarksDataSource : NSObject<BookmarksClient>
{
  // BookmarksService* mBookmarks;
  BOOL        mRegisteredClient;
  
  IBOutlet id mOutlineView;	
  IBOutlet id mBrowserWindowController;
  IBOutlet id mEditBookmarkButton;
  IBOutlet id mDeleteBookmarkButton;
  
  NSString* mCachedHref;
}

-(id) init;
-(void) windowClosing;

-(void) ensureBookmarks;

-(IBAction)addBookmark:(id)aSender;
-(void)endAddBookmark: (int)aCode;

-(IBAction)deleteBookmarks: (id)aSender;
-(void)deleteBookmark: (id)aItem;

-(IBAction)addFolder:(id)aSender;

-(void)addBookmark:(id)aSender useSelection:(BOOL)aSel isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle;
-(void)addBookmark:(id)aSender withParent:(BookmarkItem*)bmItem isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle;

- (IBAction)openBookmark: (id)aSender;
- (IBAction)openBookmarkInNewTab:(id)aSender;
- (IBAction)openBookmarkInNewWindow:(id)aSender;

- (IBAction)showBookmarkInfo:(id)aSender;
- (BOOL)haveSelectedRow;

// Datasource methods.
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;
- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;

- (BOOL)outlineView:(NSOutlineView *)ov writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard;
- (NSDragOperation)outlineView:(NSOutlineView*)ov validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index;
- (BOOL)outlineView:(NSOutlineView*)ov acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index;

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren;

// Delegate methods
- (void)outlineViewItemWillExpand:(NSNotification *)notification;
- (void)outlineViewItemWillCollapse:(NSNotification *)notification;

@end
