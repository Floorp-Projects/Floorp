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
* Combines old BookmarkController and BookmarkDataSource classes.  When
* history gets brought in to new Bookmark system, HistoryDataSource will
* go away, too.
*
*
* ***** END LICENSE BLOCK ***** */

#import <Cocoa/Cocoa.h>
#import "BookmarksClient.h"

@class HistoryDataSource;
@class ExtendedTableView;
@class BrowserWindowController;
@class BookmarkOutlineView;
@class BookmarkFolder;


@interface BookmarkViewController : NSObject {
  IBOutlet NSButton* mAddCollectionButton;
  IBOutlet NSButton* mAddBookmarkButton;
  IBOutlet NSButton* mAddFolderButton;
  IBOutlet NSButton* mAddSeparatorButton;
  IBOutlet NSButton* mInfoButton;
  IBOutlet NSButton* mSearchButton;
  IBOutlet NSTextField *mSearchField;

  IBOutlet NSSplitView* mContainersSplit;       // vertical split
  IBOutlet NSSplitView* mItemSearchSplit;       // horizontal split

  IBOutlet BrowserWindowController* mBrowserWindowController;
  IBOutlet ExtendedTableView* mContainerPane;
  IBOutlet BookmarkOutlineView* mItemPane;
  IBOutlet NSTableView* mSearchPane;            // shows search results, can be hidden
    
  IBOutlet HistoryDataSource* mHistorySource; // can swap to this for history data

  BOOL mSetupComplete;                      // have we been fully initialized?
  BOOL mSplittersRestored;                  // splitters can only be positioned after we resize to fit the window
  NSMutableDictionary *mExpandedStatus;
  NSString* mCachedHref;
  BookmarkFolder *mActiveRootCollection;
  BookmarkFolder *mRootBookmarks;
  NSArray *mSearchResultArray;
  int  mOpenActionFlag;
}

//
// IBActions
//
-(IBAction) setAsDockMenuFolder:(id)aSender;
-(IBAction) addCollection:(id)aSender;
-(IBAction) addBookmark:(id)aSender;
-(IBAction) addFolder:(id)aSender;
-(IBAction) addSeparator:(id)aSender;
-(IBAction) openBookmark: (id)aSender;
-(IBAction) openBookmarkInNewTab:(id)aSender;
-(IBAction) openBookmarkInNewWindow:(id)aSender;
-(IBAction) deleteBookmarks:(id)aSender;
-(IBAction) showBookmarkInfo:(id)aSender;
-(IBAction) startSearch:(id)aSender;
-(IBAction) locateBookmark:(id)aSender;


-(int) containerCount;
-(void) selectContainer:(int)inRowIndex;
-(void) selectLastContainer;
-(NSMutableDictionary *)expandedStatusDictionary;
-(void) restoreFolderExpandedStates;
-(BOOL) isExpanded:(id)anItem;
-(BOOL) haveSelectedRow;
-(int)numberOfSelectedRows;
-(void) setItem:(BookmarkFolder *)anItem isExpanded:(BOOL)aBool;
-(void) setActiveCollection:(BookmarkFolder *)aFolder;
-(BookmarkFolder *)activeCollection;

-(void)addItem:(id)aSender isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle;
-(void)addItem:(id)aSender withParent:(BookmarkFolder *)parent isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle;
-(void)endAddBookmark: (int)aCode;

-(void)deleteCollection:(id)aSender;
-(void) focus;
-(void) completeSetup;
-(void) ensureBookmarks;

@end
