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
*    David Haas <haasd@cae.wisc.edu>
*    Josh Aas <josha@mac.com>
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

#import <Appkit/Appkit.h>
#import "BookmarksClient.h"

@class BookmarkFolder;
@class BookmarkImportDlgController;
@class BookmarkOutlineView;
@class KindaSmartFolderManager;
@class RunLoopMessenger;

enum {
  kBookmarkMenuContainerIndex = 0,
  kToolbarContainerIndex = 1,
  kHistoryContainerIndex = 2,
};

// check 1 bookmark every 2 minutes, but only if we haven't been there in a day
#define kTimeBeforeRecheckingBookmark 86400.0
#define kTimeBetweenBookmarkChecks 120  

@interface BookmarkManager : NSObject <BookmarksClient> {
  BookmarkFolder *mRootBookmarks;	// root bookmark object
  KindaSmartFolderManager *mSmartFolderManager; //brains behind 4 smart folders
  NSUndoManager    *mUndoManager;// handles deletes, adds of bookmarks
  BookmarkImportDlgController *mImportDlgController;
  NSString *mPathToBookmarkFile; //exactly what it looks like
  NSTimer *mUpdateTimer; //we don't actually retain this
  
  // smart folders
  BookmarkFolder *mTop10Container;
  BookmarkFolder *mBrokenBookmarkContainer;
  BookmarkFolder *mRendezvousContainer;
  BookmarkFolder *mAddressBookContainer;
}

// Class Methods & shutdown stuff
+ (void)startBookmarksManager:(RunLoopMessenger *)mainThreadRunLoopMessenger;
+ (BookmarkManager*)sharedBookmarkManager;
+ (NSString*)managerStartedNotification;
- (void)shutdown;

// Getters/Setters
-(BookmarkFolder *) rootBookmarks;
-(BookmarkFolder *) toolbarFolder;
-(BookmarkFolder *) bookmarkMenuFolder;
-(BookmarkFolder *) dockMenuFolder;
-(BookmarkFolder *) top10Folder;
-(BookmarkFolder *) brokenLinkFolder;
-(BookmarkFolder *) rendezvousFolder;
-(BookmarkFolder *) addressBookFolder;
-(BookmarkFolder *) historyFolder;
-(NSUndoManager *) undoManager;
-(void) setRootBookmarks:(BookmarkFolder *)anArray;

// Informational things
-(NSArray *)resolveBookmarksKeyword:(NSString *)keyword;
-(NSArray *)searchBookmarksForString:(NSString *)searchString;
-(unsigned) firstUserCollection;
-(BOOL) isDropValid:(NSArray *)items toFolder:(BookmarkFolder *)parent;
-(NSMenu *)contextMenuForItem:(id)item fromView:(BookmarkOutlineView *)outlineView target:(id)target;

// Reading bookmark files
-(BOOL) readBookmarks;
-(void) startImportBookmarks;
-(BOOL) importBookmarks:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;
-(NSString *)decodedHTMLfile:(NSString *)pathToFile;
-(BOOL)readHTMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;
-(BOOL)readCaminoXMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;
-(BOOL)readPropertyListFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;

// Writing bookmark files
-(void)writeHTMLFile:(NSString *)pathToFile;
-(void)writePropertyListFile:(NSString *)pathToFile;
-(void)writeSafariFile:(NSString *)pathToFile;

@end
