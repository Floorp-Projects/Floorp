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

#import <AppKit/AppKit.h>
#import "BookmarksClient.h"

@class BookmarkItem;
@class BookmarkFolder;
@class BookmarkImportDlgController;
@class BookmarkOutlineView;
@class KindaSmartFolderManager;

extern NSString* const kBookmarkManagerStartedNotification;

extern NSString* const kBookmarksToolbarFolderIdentifier;
extern NSString* const kBookmarksMenuFolderIdentifier;

const int kBookmarksContextMenuArrangeSeparatorTag = 100;


@interface BookmarkManager : NSObject <BookmarksClient>
{
  BookmarkFolder*                 mRootBookmarks;           // root bookmark object
  KindaSmartFolderManager*        mSmartFolderManager;      // brains behind 4 smart folders
  NSUndoManager*                  mUndoManager;             // handles deletes, adds of bookmarks
  BookmarkImportDlgController*    mImportDlgController;
  NSString*                       mPathToBookmarkFile;
  NSString*                       mMetadataPath;            // where we store spotlight cache (strong)
  
  NSMutableDictionary*            mBookmarkURLMap;          // map of cleaned bookmark url to bookmark item set
  NSMutableDictionary*            mBookmarkFaviconURLMap;   // map of cleaned bookmark favicon url to bookmark item set
  
  // smart folders
  BookmarkFolder*                 mTop10Container;
  BookmarkFolder*                 mRendezvousContainer;
  BookmarkFolder*                 mAddressBookContainer;
  
  BookmarkFolder*                 mLastUsedFolder;
  
  BOOL                            mBookmarksLoaded;
  BOOL                            mShowSiteIcons;
  
  int                             mNotificationsSuppressedCount;
  NSRecursiveLock*                mNotificationsSuppressedLock;    // make mNotificationsSuppressedCount threadsafe
}

// Class Methods & shutdown stuff
+ (BookmarkManager*)sharedBookmarkManager;
+ (BookmarkManager*)sharedBookmarkManagerDontCreate;

- (void)loadBookmarksLoadingSynchronously:(BOOL)loadSync;

- (void)shutdown;

- (BOOL)bookmarksLoaded;

- (BOOL)showSiteIcons;

+ (NSArray*)serializableArrayWithBookmarkItems:(NSArray*)bmArray;
+ (NSArray*)bookmarkItemsFromSerializableArray:(NSArray*)bmArray;

// Getters/Setters
-(BookmarkFolder *) rootBookmarks;
-(BookmarkFolder *) toolbarFolder;
-(BookmarkFolder *) bookmarkMenuFolder;
-(BookmarkFolder *) dockMenuFolder;
-(BookmarkFolder *) top10Folder;
-(BookmarkFolder *) rendezvousFolder;
-(BookmarkFolder *) addressBookFolder;
-(BookmarkFolder *) historyFolder;

- (BOOL)isUserCollection:(BookmarkFolder *)inFolder;

// returns NSNotFound if the folder is not a child of the root
- (unsigned)indexOfContainer:(BookmarkFolder*)inFolder;
- (BookmarkFolder*)containerAtIndex:(unsigned)inIndex;
- (BookmarkFolder*)rootBookmarkFolderWithIdentifier:(NSString*)inIdentifier;

- (BOOL)itemsShareCommonParent:(NSArray*)inItems;

// these may be nested, and are threadsafe
- (void)startSuppressingChangeNotifications;
- (void)stopSuppressingChangeNotifications;

- (BOOL)areChangeNotificationsSuppressed;

// get/set folder last used by "Add Bookmarks"
- (BookmarkFolder*)lastUsedBookmarkFolder;
- (void)setLastUsedBookmarkFolder:(BookmarkFolder*)inFolder;

-(BookmarkItem*) itemWithUUID:(NSString*)uuid;
-(NSUndoManager *) undoManager;
-(void) setRootBookmarks:(BookmarkFolder *)anArray;

// clear visit count on all bookmarks
-(void)clearAllVisits;

// Informational things
-(NSArray *)resolveBookmarksKeyword:(NSString *)keyword;
-(NSArray *)searchBookmarksContainer:(BookmarkFolder*)container forString:(NSString *)searchString inFieldWithTag:(int)tag;
-(BOOL) isDropValid:(NSArray *)items toFolder:(BookmarkFolder *)parent;
-(NSMenu *)contextMenuForItems:(NSArray*)items fromView:(BookmarkOutlineView *)outlineView target:(id)target;

// Utilities
- (void)copyBookmarksURLs:(NSArray*)bookmarkItems toPasteboard:(NSPasteboard*)aPasteboard;

// Importing bookmarks
- (void)startImportBookmarks;
- (BOOL)importBookmarks:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;

// Writing bookmark files
- (void)writeHTMLFile:(NSString *)pathToFile;
- (void)writePropertyListFile:(NSString *)pathToFile;
- (void)writeSafariFile:(NSString *)pathToFile;

@end
