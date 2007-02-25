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
 *   Josh Aas <josh@mozilla.com>
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

#include <unistd.h>

#include "nsString.h"
#include "nsIContent.h"
#include "nsIFile.h"
#include "nsAppDirectoryServiceDefs.h"

#import "NSString+Utils.h"
#import "NSArray+Utils.h"
#import "NSThread+Utils.h"
#import "NSFileManager+Utils.h"
#import "NSWorkspace+Utils.h"
#import "NSPasteboard+Utils.h"
#import "NSMenu+Utils.h"

#import "PreferenceManager.h"
#import "BookmarkManager.h"
#import "Bookmark.h"
#import "BookmarkFolder.h"
#import "BookmarkToolbar.h"
#import "BookmarkImportDlgController.h"
#import "BookmarkOutlineView.h"
#import "BookmarkViewController.h"
#import "KindaSmartFolderManager.h"
#import "BrowserWindowController.h"
#import "MainController.h"
#import "SiteIconProvider.h"

NSString* const kBookmarkManagerStartedNotification = @"BookmarkManagerStartedNotification";

// root bookmark folder identifiers (must be unique!)
NSString* const kBookmarksToolbarFolderIdentifier              = @"BookmarksToolbarFolder";
NSString* const kBookmarksMenuFolderIdentifier                 = @"BookmarksMenuFolder";

static NSString* const kTop10BookmarksFolderIdentifier         = @"Top10BookmarksFolder";
static NSString* const kRendezvousFolderIdentifier             = @"RendezvousFolder";   // aka Bonjour
static NSString* const kAddressBookFolderIdentifier            = @"AddressBookFolder";
static NSString* const kHistoryFolderIdentifier                = @"HistoryFolder";

NSString* const kBookmarkImportPathIndentifier                 = @"path";
NSString* const kBookmarkImportNewFolderNameIdentifier         = @"title";

static NSString* const kBookmarkImportStatusIndentifier        = @"flag";
static NSString* const kBookmarkImportNewFolderIdentifier      = @"folder";
static NSString* const kBookmarkImportNewFolderIndexIdentifier = @"index";

// these are suggested indices; we only use them to order the root folders, not to access them.
enum {
  kBookmarkMenuContainerIndex = 0,
  kToolbarContainerIndex = 1,
  kHistoryContainerIndex = 2,
};

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_4
// These are only used for bookmark error logging on 10.4, but
// we need them defined to keep the compiler happy on 10.3 SDKs
enum {
  NSAtomicWrite = 1
};

@interface NSData(TigerErrorLogging)
- (BOOL)writeToFile:(NSString*)path options:(unsigned int)mask error:(NSError**)errorPtr;
@end

@interface NSError(TigerErrorLogging)
- (NSString*)localizedFailureReason;
@end
#endif

@interface BookmarkManager (Private)

+ (NSString*)canonicalBookmarkURL:(NSString*)inBookmarkURL;
+ (NSString*)faviconURLForBookmark:(Bookmark*)inBookmark;

- (void)loadBookmarksThreadEntry:(id)inObject;
- (void)loadBookmarks;
- (void)setPathToBookmarkFile:(NSString *)aString;
- (void)setupSmartCollections;
- (void)delayedStartupItems;
- (void)noteBookmarksChanged;
- (void)writeBookmarks:(NSNotification *)note;
- (BookmarkFolder *)findDockMenuFolderInFolder:(BookmarkFolder *)aFolder;
- (void)writeBookmarksMetadataForSpotlight;

// Reading bookmark files
- (BOOL)readBookmarks;
- (void)showCorruptBookmarksAlert;
- (void)showRestoredBookmarksAlert;

// these versions assume that we're reading all the bookmarks from the file (i.e. not an import into a subfolder)
- (BOOL)readPListBookmarks:(NSString *)pathToFile;    // camino or safari
- (BOOL)readCaminoPListBookmarks:(NSDictionary *)plist;
- (BOOL)readSafariPListBookmarks:(NSDictionary *)plist;
- (BOOL)readCaminoXMLBookmarks:(NSString *)pathToFile;

// these are "import" methods that import into a subfolder.
- (BOOL)importHTMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;
- (BOOL)importCaminoXMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder settingToolbarFolder:(BOOL)setToolbarFolder;
- (BOOL)importPropertyListFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;
- (void)importBookmarksThreadReturn:(NSDictionary *)aDict;

- (BOOL)readOperaFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;

+ (void)addItem:(id)inBookmark toURLMap:(NSMutableDictionary*)urlMap usingURL:(NSString*)inURL;
+ (void)removeItem:(id)inBookmark fromURLMap:(NSMutableDictionary*)urlMap usingURL:(NSString*)inURL;  // url may be nil, in which case exhaustive search is used
+ (NSEnumerator*)enumeratorForBookmarksInMap:(NSMutableDictionary*)urlMap matchingURL:(NSString*)inURL;

- (void)registerBookmarkForLoads:(Bookmark*)inBookmark;
- (void)unregisterBookmarkForLoads:(Bookmark*)inBookmark ignoringURL:(BOOL)inIgnoreURL;
- (void)setAndReregisterFaviconURL:(NSString*)inFaviconURL forBookmark:(Bookmark*)inBookmark;
- (void)onSiteIconLoad:(NSNotification *)inNotification;
- (void)onPageLoad:(NSNotification*)inNotification;

@end

#pragma mark -

@implementation BookmarkManager

static NSString* const    kWriteBookmarkNotification = @"write_bms";

static BookmarkManager* gBookmarkManager = nil;

+ (BookmarkManager*)sharedBookmarkManager
{
  if (!gBookmarkManager)
    gBookmarkManager = [[BookmarkManager alloc] init];

  return gBookmarkManager;
}

+ (BookmarkManager*)sharedBookmarkManagerDontCreate
{
  return gBookmarkManager;
}

// serialize to an array of UUIDs
+ (NSArray*)serializableArrayWithBookmarkItems:(NSArray*)bmArray
{
  NSMutableArray* dataArray = [NSMutableArray arrayWithCapacity:[bmArray count]];
  NSEnumerator* bmEnum = [bmArray objectEnumerator];
  id bmItem;
  while ((bmItem = [bmEnum nextObject])) {
    [dataArray addObject:[bmItem UUID]];
  }

  return dataArray;
}

+ (NSArray*)bookmarkItemsFromSerializableArray:(NSArray*)dataArray
{
  NSMutableArray* itemsArray = [NSMutableArray arrayWithCapacity:[dataArray count]];
  NSEnumerator* dataEnum = [dataArray objectEnumerator];
  BookmarkManager* bmManager = [BookmarkManager sharedBookmarkManager];
  id itemUUID;
  while ((itemUUID = [dataEnum nextObject])) {
    BookmarkItem* foundItem = [bmManager itemWithUUID:itemUUID];
    if (foundItem)
      [itemsArray addObject:foundItem];
    else
      NSLog(@"Failed to find bm item with uuid %@", itemUUID);
  }

  return itemsArray;
}

// return a string with the "canonical" bookmark url (strip trailing slashes, lowercase)
+ (NSString*)canonicalBookmarkURL:(NSString*)inBookmarkURL
{
  NSString* tempURL = inBookmarkURL;

  if ([tempURL hasSuffix:@"/"])
    tempURL = [tempURL substringToIndex:([tempURL length] - 1)];

  return [tempURL lowercaseString];
}

+ (NSString*)faviconURLForBookmark:(Bookmark*)inBookmark
{
  // if the bookmark has one, use it, otherwise assume the default location
  if ([[inBookmark faviconURL] length] > 0)
    return [inBookmark faviconURL];

  return [SiteIconProvider defaultFaviconLocationStringFromURI:[inBookmark url]];
}

#pragma mark -

//
// Init, dealloc
//
- (id)init
{
  if ((self = [super init])) {
    mBookmarkURLMap         = [[NSMutableDictionary alloc] initWithCapacity:50];
    mBookmarkFaviconURLMap  = [[NSMutableDictionary alloc] initWithCapacity:50];

    mBookmarksLoaded        = NO;
    mShowSiteIcons          = [[PreferenceManager sharedInstance] getBooleanPref:"browser.chrome.favicons" withSuccess:NULL];

    mNotificationsSuppressedLock = [[NSRecursiveLock alloc] init];
  }

  return self;
}

- (void)dealloc
{
  if (gBookmarkManager == self)
    gBookmarkManager = nil;

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [mTop10Container release];
  [mRendezvousContainer release];
  [mAddressBookContainer release];
  [mLastUsedFolder release];

  [mUndoManager release];
  [mRootBookmarks release];
  [mPathToBookmarkFile release];
  [mMetadataPath release];
  [mSmartFolderManager release];

  [mImportDlgController release];

  [mBookmarkURLMap release];
  [mBookmarkFaviconURLMap release];

  [mNotificationsSuppressedLock release];

  [super dealloc];
}

- (void)loadBookmarksLoadingSynchronously:(BOOL)loadSync
{
  if (loadSync)
    [self loadBookmarks];
  else
    [NSThread detachNewThreadSelector:@selector(loadBookmarksThreadEntry:) toTarget:self withObject:nil];
}

- (void)loadBookmarksThreadEntry:(id)inObject
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  [self loadBookmarks];
  [pool release];
}

// NB: this is called on a thread!
- (void)loadBookmarks
{
  // Turn off the posting of update notifications while reading in bookmarks.
  // All interested parties haven't been init'd yet, and/or will receive the
  // managerStartedNotification when setup is actually complete.
  // Note that it's important that notifications are off while loading bookmarks
  // on this thread, because notifications are handled on the thread they are posted
  // on, and our code assumes that bookmark notifications happen on the main thread.
  [self startSuppressingChangeNotifications];

  // handle exceptions to ensure that turn notification suppression back off
  NS_DURING
    BookmarkFolder* root = [[BookmarkFolder alloc] init];

    // We used to do this:
    // [root setParent:self];
    // but it was unclear why, and it broke logic in a bunch of places (like -setIsRoot).

    [root setIsRoot:YES];
    [root setTitle:NSLocalizedString(@"BookmarksRootName", nil)];
    [self setRootBookmarks:root];
    [root release];

    BOOL bookmarksReadOK = [self readBookmarks];
    if (!bookmarksReadOK) {
      // We'll come here either when reading the bookmarks totally failed, or
      // when we did a partial read of the xml file. The xml reading code already
      // fixed up the toolbar folder.
      if ([root count] == 0) {
        // failed to read any folders. make some by hand.
        BookmarkFolder* menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
        [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", nil)];
        [root appendChild:menuFolder];

        BookmarkFolder* toolbarFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksToolbarFolderIdentifier] autorelease];
        [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", nil)];
        [toolbarFolder setIsToolbar:YES];
        [root appendChild:toolbarFolder];
      }
    }

    // make sure that the root folder has the special flag
    [[self rootBookmarks] setIsRoot:YES];

    // setup special folders
    [self setupSmartCollections];

    mSmartFolderManager = [[KindaSmartFolderManager alloc] initWithBookmarkManager:self];

    // set the localized titles of these folders
    [[self toolbarFolder] setTitle:NSLocalizedString(@"Bookmark Bar", nil)];
    [[self bookmarkMenuFolder] setTitle:NSLocalizedString(@"Bookmark Menu", nil)];

  NS_HANDLER
      NSLog(@"Exception caught in loadBookmarks: %@", localException);
  NS_ENDHANDLER

  [self stopSuppressingChangeNotifications];

  // don't do this until after we've read in the bookmarks
  mUndoManager = [[NSUndoManager alloc] init];

  // do the other startup stuff over on the main thread
  [self performSelectorOnMainThread:@selector(delayedStartupItems) withObject:nil waitUntilDone:NO];

  // pitch everything in the metadata cache and start over. Changes made from here will be incremental. It's
  // easier this way in case someone changed the bm plist directly, we know at startup we always have
  // the most up-to-date cache.
  [self writeBookmarksMetadataForSpotlight];
}


// Perform additional setup items on the main thread.
- (void)delayedStartupItems
{
  mBookmarksLoaded = YES;

  [mSmartFolderManager postStartupInitialization:self];
  [[self toolbarFolder] refreshIcon];

  NSArray* allBookmarks = [[self rootBookmarks] allChildBookmarks];

  NSEnumerator* bmEnum = [allBookmarks objectEnumerator];
  Bookmark* thisBM;
  while ((thisBM = [bmEnum nextObject])) {
    [self registerBookmarkForLoads:thisBM];
  }

  // load favicons (w/out hitting the network, cache only). Spread it out so that we only get
  // ten every three seconds to avoid locking up the UI with large bookmark lists.
  // XXX probably want a better way to do this. This sets up a timer (internally) for every
  // bookmark
  if ([[PreferenceManager sharedInstance] getBooleanPref:"browser.chrome.favicons" withSuccess:NULL]) {
    float delay = 3.0; //default value
    int count = [allBookmarks count];
    for (int i = 0; i < count; ++i) {
      if (i % 10 == 0)
        delay += 3.0;
      [[allBookmarks objectAtIndex:i] performSelector:@selector(refreshIcon) withObject:nil afterDelay:delay];
    }
  }

  // Generic notifications for Bookmark Client. Don't set these up until after all the smart
  // folders have loaded. Even though we coalesce bookmark update notifications down into a single
  // message, there's no need to write out even once for any of these changes.
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self selector:@selector(bookmarkAdded:) name:BookmarkFolderAdditionNotification object:nil];
  [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
  [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:nil];
  [nc addObserver:self selector:@selector(writeBookmarks:) name:kWriteBookmarkNotification object:nil];

  // listen for site icon and page loads, to forward to bookmarks
  [nc addObserver:self selector:@selector(onSiteIconLoad:) name:SiteIconLoadNotificationName object:nil];
  [nc addObserver:self selector:@selector(onPageLoad:) name:URLLoadNotification object:nil];

  // broadcast to everyone interested that we're loaded and ready for public consumption
  [[NSNotificationCenter defaultCenter] postNotificationName:kBookmarkManagerStartedNotification object:nil];
}

- (void)shutdown
{
  [self writeBookmarks:nil];
  // Temporary logging to try to help nail down bug 337750
  long long bmFileSize = [[NSFileManager defaultManager] sizeOfFileAtPath:mPathToBookmarkFile traverseLink:YES];
  NSLog(@"Bookmarks file '%@' is %qi bytes on shutdown", mPathToBookmarkFile, bmFileSize);
}

- (BOOL)bookmarksLoaded
{
  return mBookmarksLoaded;
}

- (BOOL)showSiteIcons
{
  return mShowSiteIcons;
}

//
// smart collections, as of now, are Rendezvous, Address Book, Top 10 List.
// We also have history, but that just points to the real history stuff.
- (void)setupSmartCollections
{
  int collectionIndex = 2;  // skip 0 and 1, the menu and toolbar folders

  // XXX this reliance of indices of the root for the special folders is bad; it makes it hard
  // for us to reorder the collections without breaking stuff. Also, there's no checking on
  // reading the file that the Nth folder of the root really is the Toolbar (for example).

  // add history
  BookmarkFolder* historyBMFolder = [[BookmarkFolder alloc] initWithIdentifier:kHistoryFolderIdentifier];
  [historyBMFolder setTitle:NSLocalizedString(@"History", nil)];
  [historyBMFolder setIsSmartFolder:YES];
  [mRootBookmarks insertChild:historyBMFolder atIndex:(collectionIndex++) isMove:NO];
  [historyBMFolder release];

  // note: we retain smart folders, so they persist even if turned off and on
  mTop10Container = [[BookmarkFolder alloc] initWithIdentifier:kTop10BookmarksFolderIdentifier];
  [mTop10Container setTitle:NSLocalizedString(@"Top Ten List", nil)];
  [mTop10Container setIsSmartFolder:YES];
  [mRootBookmarks insertChild:mTop10Container atIndex:(collectionIndex++) isMove:NO];

  mRendezvousContainer = [[BookmarkFolder alloc] initWithIdentifier:kRendezvousFolderIdentifier];
  [mRendezvousContainer setTitle:NSLocalizedString(@"Rendezvous", nil)];
  [mRendezvousContainer setIsSmartFolder:YES];
  [mRootBookmarks insertChild:mRendezvousContainer atIndex:(collectionIndex++) isMove:NO];

  mAddressBookContainer = [[BookmarkFolder alloc] initWithIdentifier:kAddressBookFolderIdentifier];
  [mAddressBookContainer setTitle:NSLocalizedString(@"Address Book", nil)];
  [mAddressBookContainer setIsSmartFolder:YES];
  [mRootBookmarks insertChild:mAddressBookContainer atIndex:(collectionIndex++) isMove:NO];

  // set pretty icons
  [[self historyFolder]       setIcon:[NSImage imageNamed:@"historyicon"]];
  [[self top10Folder]         setIcon:[NSImage imageNamed:@"top10_icon"]];
  [[self bookmarkMenuFolder]  setIcon:[NSImage imageNamed:@"bookmarkmenu_icon"]];
  [[self toolbarFolder]       setIcon:[NSImage imageNamed:@"bookmarktoolbar_icon"]];
  [[self rendezvousFolder]    setIcon:[NSImage imageNamed:@"rendezvous_icon"]];
  [[self addressBookFolder]   setIcon:[NSImage imageNamed:@"addressbook_icon"]];
}

//
// Getter/Setter methods
//

- (BookmarkFolder *)rootBookmarks
{
  return mRootBookmarks;
}

- (BookmarkFolder *)dockMenuFolder
{
  BookmarkFolder *folder = [self findDockMenuFolderInFolder:[self rootBookmarks]];
  if (folder)
    return folder;
  else
    return [self top10Folder];
}

- (BookmarkFolder *)findDockMenuFolderInFolder:(BookmarkFolder *)aFolder
{
  NSEnumerator *enumerator = [[aFolder childArray] objectEnumerator];
  id aKid;
  BookmarkFolder *foundFolder = nil;
  while ((!foundFolder) && (aKid = [enumerator nextObject])) {
    if ([aKid isKindOfClass:[BookmarkFolder class]]) {
      if ([(BookmarkFolder *)aKid isDockMenu])
        return aKid;
      else
        foundFolder = [self findDockMenuFolderInFolder:aKid];
    }
  }
  return foundFolder;
}

- (BookmarkFolder*)rootBookmarkFolderWithIdentifier:(NSString*)inIdentifier
{
  NSArray* rootFolders = [[self rootBookmarks] childArray];
  unsigned int numFolders = [rootFolders count];
  for (unsigned int i = 0; i < numFolders; i++) {
    id curItem = [rootFolders objectAtIndex:i];
    if ([curItem isKindOfClass:[BookmarkFolder class]] && [[curItem identifier] isEqualToString:inIdentifier])
      return (BookmarkFolder*)curItem;
  }
  return nil;
}

- (BOOL)itemsShareCommonParent:(NSArray*)inItems
{
  NSEnumerator* itemsEnum = [inItems objectEnumerator];

  id commonParent = nil;
  BookmarkItem* curItem;
  while ((curItem = [itemsEnum nextObject])) {
    if (curItem == [inItems firstObject]) {
      commonParent = [curItem parent];
      if (!commonParent)
        return NO;
    }

    if ([curItem parent] != commonParent)
      return NO;
  }

  return YES;
}

- (void)startSuppressingChangeNotifications
{
  [mNotificationsSuppressedLock lockBeforeDate:[NSDate distantFuture]];
  ++mNotificationsSuppressedCount;
  [mNotificationsSuppressedLock unlock];
}

- (void)stopSuppressingChangeNotifications
{
  [mNotificationsSuppressedLock lockBeforeDate:[NSDate distantFuture]];
  --mNotificationsSuppressedCount;
  [mNotificationsSuppressedLock unlock];
}

- (BOOL)areChangeNotificationsSuppressed
{
  return (mNotificationsSuppressedCount > 0);
}


- (BookmarkFolder *)top10Folder
{
  return mTop10Container;
}

- (BookmarkFolder *)toolbarFolder
{
  return [self rootBookmarkFolderWithIdentifier:kBookmarksToolbarFolderIdentifier];
}

- (BookmarkFolder *)bookmarkMenuFolder
{
  return [self rootBookmarkFolderWithIdentifier:kBookmarksMenuFolderIdentifier];
}

- (BookmarkFolder *)historyFolder
{
  return [self rootBookmarkFolderWithIdentifier:kHistoryFolderIdentifier];
}

- (BOOL)isUserCollection:(BookmarkFolder *)inFolder
{
  return ([inFolder parent] == mRootBookmarks) &&
         ([[inFolder identifier] length] == 0);   // all our special folders have identifiers
}

- (BOOL)searchActive
{
  return mSearchActive;
}

- (void)setSearchActive:(BOOL)inSearching
{
  mSearchActive = inSearching;
}

- (unsigned)indexOfContainer:(BookmarkFolder*)inFolder
{
  return [mRootBookmarks indexOfObject:inFolder];
}

- (BookmarkFolder*)containerAtIndex:(unsigned)inIndex
{
  return [mRootBookmarks objectAtIndex:inIndex];
}

- (BookmarkFolder *)rendezvousFolder
{
  return mRendezvousContainer;
}

- (BookmarkFolder *)addressBookFolder
{
  return mAddressBookContainer;
}

- (BookmarkFolder*)lastUsedBookmarkFolder
{
  if (!mLastUsedFolder)
    return [self toolbarFolder];

  return mLastUsedFolder;
}

- (void)setLastUsedBookmarkFolder:(BookmarkFolder*)inFolder
{
  [mLastUsedFolder autorelease];
  mLastUsedFolder = [inFolder retain];
}

- (BookmarkItem*)itemWithUUID:(NSString*)uuid
{
  return [mRootBookmarks itemWithUUID:uuid];
}

// only the main thread can get the undo manager.
// imports (on a background thread) get nothing, which is ok.
// this keeps things nice and thread safe
- (NSUndoManager *)undoManager
{
  if ([NSThread inMainThread])
    return mUndoManager;
  return nil;
}

- (void)setPathToBookmarkFile:(NSString *)aString
{
  [aString retain];
  [mPathToBookmarkFile release];
  mPathToBookmarkFile = aString;
}

- (void)setRootBookmarks:(BookmarkFolder *)anArray
{
  if (anArray != mRootBookmarks) {
    [anArray retain];
    [mRootBookmarks release];
    mRootBookmarks = anArray;
  }
}

//
// -clearAllVisits:
//
// resets all bookmarks visit counts to zero as part of Reset Camino
//

- (void)clearAllVisits
{
  // XXX this will fire a lot of changed notifications.
  NSEnumerator* bookmarksEnum = [[self rootBookmarks] objectEnumerator];
  BookmarkItem* curItem;
  while (curItem = [bookmarksEnum nextObject]) {
    if ([curItem isKindOfClass:[Bookmark class]])
      [(Bookmark*)curItem setNumberOfVisits:0];
  }
}


- (NSArray *)resolveBookmarksKeyword:(NSString *)keyword
{
  NSArray *resolvedArray = nil;
  if ([keyword length] > 0) {
    NSRange spaceRange = [keyword rangeOfString:@" "];
    NSString *firstWord = nil;
    NSString *secondWord = nil;
    if (spaceRange.location != NSNotFound) {
      firstWord = [keyword substringToIndex:spaceRange.location];
      secondWord = [keyword substringFromIndex:(spaceRange.location + spaceRange.length)];
    }
    else {
      firstWord = keyword;
      secondWord = @"";
    }
    resolvedArray = [[self rootBookmarks] resolveKeyword:firstWord withArgs:secondWord];
  }
  return resolvedArray;
}

// a null container indicates to search all bookmarks
- (NSArray *)searchBookmarksContainer:(BookmarkFolder*)container forString:(NSString *)searchString inFieldWithTag:(int)tag
{
  if ((searchString) && [searchString length] > 0) {
    BookmarkFolder* searchContainer = container ? container : [self rootBookmarks];
    NSSet *matchingSet = [searchContainer bookmarksWithString:searchString inFieldWithTag:tag];
    return [matchingSet allObjects];
  }
  return nil;
}

//
// Drag & drop
//

- (BOOL)isDropValid:(NSArray *)items toFolder:(BookmarkFolder *)parent
{
  // Enumerate through items, make sure we're not being dropped into
  // a child OR ourself OR that the a bookmark or group is going into root bookmarks.
  NSEnumerator *enumerator = [items objectEnumerator];
  id aBookmark;
  while ((aBookmark = [enumerator nextObject])) {
    if ([aBookmark isKindOfClass:[BookmarkFolder class]]) {
      if (aBookmark == parent)
        return NO;
      if ((parent == [self rootBookmarks]) && [(BookmarkFolder *)aBookmark isGroup])
        return NO;
    }
    else if ([aBookmark isKindOfClass:[Bookmark class]]) {
      if (parent == [self rootBookmarks])
        return NO;
    }
    if ([parent isChildOfItem:aBookmark])
      return NO;
  }
  return YES;
}

// unified context menu generator for all kinds of bookmarks
// this can be called from a bookmark outline view
// or from a bookmark button, which should pass a nil outlineView
- (NSMenu *)contextMenuForItems:(NSArray*)items fromView:(BookmarkOutlineView *)outlineView target:(id)target
{
  if ([items count] == 0)
    return nil;

  BOOL itemsContainsFolder = NO;
  BOOL itemsContainsBookmark = NO;
  BOOL itemsAllSeparators = YES;
  BOOL multipleItems = ([items count] > 1);

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject])) {
    itemsContainsFolder   |= [curItem isKindOfClass:[BookmarkFolder class]];
    itemsContainsBookmark |= [curItem isKindOfClass:[Bookmark class]];
    itemsAllSeparators    &= [curItem isSeparator];
  }

  // All the methods in this context menu need to be able to handle > 1 item
  // being selected, and the selected items containing a mixture of folders,
  // bookmarks, and separators.
  NSMenu* contextMenu = [[[NSMenu alloc] initWithTitle:@"notitle"] autorelease];
  NSString* menuTitle = nil;
  NSMenuItem* menuItem = nil;
  NSMenuItem* shiftMenuItem = nil;

  // Selections with only separators shouldn't have these CM items at all.
  // We rely on the called selectors to do the Right Thing(tm) with embedded separators.
  if (!itemsAllSeparators) {
    // open in new window(s)
    if (itemsContainsFolder && !multipleItems)
      menuTitle = NSLocalizedString(@"Open Tabs in New Window", nil);
    else if (multipleItems)
      menuTitle = NSLocalizedString(@"Open in New Windows", nil);
    else
      menuTitle = NSLocalizedString(@"Open in New Window", nil);

    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openBookmarkInNewWindow:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [menuItem setKeyEquivalentModifierMask:0]; //Needed since by default NSMenuItems have NSCommandKeyMask
    [contextMenu addItem:menuItem];

    shiftMenuItem = [NSMenuItem alternateMenuItemWithTitle:menuTitle action:@selector(openBookmarkInNewWindow:) target:target modifiers:NSShiftKeyMask];
    [contextMenu addItem:shiftMenuItem];

    // open in new tabs in new window
    if (multipleItems) {
      menuTitle = NSLocalizedString(@"Open in Tabs in New Window", nil);

      menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openBookmarksInTabsInNewWindow:) keyEquivalent:@""] autorelease];
      [menuItem setKeyEquivalentModifierMask:0];
      [menuItem setTarget:target];
      [contextMenu addItem:menuItem];

      shiftMenuItem = [NSMenuItem alternateMenuItemWithTitle:menuTitle action:@selector(openBookmarksInTabsInNewWindow:) target:target modifiers:NSShiftKeyMask];
      [contextMenu addItem:shiftMenuItem];
    }

    // open in new tab in current window
    if (itemsContainsFolder || multipleItems)
      menuTitle = NSLocalizedString(@"Open in New Tabs", nil);
    else
      menuTitle = NSLocalizedString(@"Open in New Tab", nil);

    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openBookmarkInNewTab:) keyEquivalent:@""] autorelease];
    [menuItem setKeyEquivalentModifierMask:0];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];

    shiftMenuItem = [NSMenuItem alternateMenuItemWithTitle:menuTitle action:@selector(openBookmarkInNewTab:) target:target modifiers:NSShiftKeyMask];
    [contextMenu addItem:shiftMenuItem];
  }

  BookmarkFolder* collection = [target isKindOfClass:[BookmarkViewController class]] ? [target activeCollection] : nil;
  // We only want a "Reveal" menu item if the CM is on a BookmarkButton,
  // if the user is searching somewhere other than the History folder,
  // or if the Top 10 is the active collection.
  if ((!outlineView) ||
      (!multipleItems && (([self searchActive] && !(collection == [self historyFolder])) ||
                          (collection == [self top10Folder]))))
  {
    menuTitle = NSLocalizedString(@"Reveal in Bookmark Manager", nil);
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(revealBookmark:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }

  if (!itemsAllSeparators) {
    [contextMenu addItem:[NSMenuItem separatorItem]];

    if (!outlineView || !multipleItems) {
      menuTitle = NSLocalizedString(@"Bookmark Info", nil);
      menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(showBookmarkInfo:) keyEquivalent:@""] autorelease];
      [menuItem setTarget:target];
      [contextMenu addItem:menuItem];
    }
  }

  // copy URL(s) to clipboard
  // This makes no sense for separators, which have no URL.
  // We rely on |copyURLs:| to handle the selector-embedded-in-multiple-items case.
  if (!itemsAllSeparators) {
    if (itemsContainsFolder || multipleItems)
      menuTitle = NSLocalizedString(@"Copy URLs to Clipboard", nil);
    else
      menuTitle = NSLocalizedString(@"Copy URL to Clipboard", nil);

    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(copyURLs:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }

  if (!multipleItems && itemsContainsFolder) {
    menuTitle = NSLocalizedString(@"Use as Dock Menu", nil);
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(toggleIsDockMenu:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:[items objectAtIndex:0]];
    if ([(BookmarkFolder*)[items objectAtIndex:0] isDockMenu])
      [menuItem setState:NSOnState];
    [contextMenu addItem:menuItem];
  }

  BOOL allowNewFolder = NO;
  if ([target isKindOfClass:[BookmarkViewController class]])
    allowNewFolder = ![[target activeCollection] isSmartFolder];

  // if we're not in a smart collection (other than history)
  if (!outlineView ||
      ![[target activeCollection] isSmartFolder] ||
      ([target activeCollection] == [self historyFolder]))
  {
    if ([contextMenu numberOfItems] != 0)
      // only add a separator if it won't be the first item in the menu
      [contextMenu addItem:[NSMenuItem separatorItem]];

    // delete
    menuTitle = NSLocalizedString(@"Delete", nil);
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(deleteBookmarks:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }

  if (allowNewFolder) {
    // space
    [contextMenu addItem:[NSMenuItem separatorItem]];
    // create new folder
    menuTitle = NSLocalizedString(@"Create New Folder...", nil);
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(addBookmarkFolder:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }

  // Arrange selections of multiple bookmark items or folders.
  // These may get removed again by the caller, so we tag them.
  if ([target isKindOfClass:[BookmarkViewController class]] &&
      ![[target activeCollection] isSmartFolder] &&
      (multipleItems || itemsContainsFolder) &&
      !itemsAllSeparators)
  {
    NSMenuItem* separatorItem = [NSMenuItem separatorItem];
    [separatorItem setTag:kBookmarksContextMenuArrangeSeparatorTag];
    [contextMenu addItem:separatorItem];

    menuTitle = NSLocalizedString(@"Arrange Bookmarks", nil);
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:NULL keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];

    // create submenu
    NSMenu* arrangeSubmenu = [[[NSMenu alloc] initWithTitle:@"notitle"] autorelease];

    NSMenuItem* subMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Arrange Increasing by title", nil)
                                           action:@selector(arrange:)
                                    keyEquivalent:@""] autorelease];
    [subMenuItem setTarget:target];
    [subMenuItem setTag:(kArrangeBookmarksByTitleMask | kArrangeBookmarksAscendingMask)];
    [arrangeSubmenu addItem:subMenuItem];

    subMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Arrange Decreasing by title", nil)
                                           action:@selector(arrange:)
                                    keyEquivalent:@""] autorelease];
    [subMenuItem setTarget:target];
    [subMenuItem setTag:(kArrangeBookmarksByTitleMask | kArrangeBookmarksDescendingMask)];
    [arrangeSubmenu addItem:subMenuItem];

    [arrangeSubmenu addItem:[NSMenuItem separatorItem]];

    subMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Arrange Increasing by location", nil)
                                           action:@selector(arrange:)
                                    keyEquivalent:@""] autorelease];
    [subMenuItem setTarget:target];
    [subMenuItem setTag:(kArrangeBookmarksByLocationMask | kArrangeBookmarksAscendingMask)];
    [arrangeSubmenu addItem:subMenuItem];

    subMenuItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Arrange Decreasing by location", nil)
                                           action:@selector(arrange:)
                                    keyEquivalent:@""] autorelease];
    [subMenuItem setTarget:target];
    [subMenuItem setTag:(kArrangeBookmarksByLocationMask | kArrangeBookmarksDescendingMask)];
    [arrangeSubmenu addItem:subMenuItem];

    [contextMenu setSubmenu:arrangeSubmenu forItem:menuItem];
  }

  // Disable context menu items if the parent window is currently showing a sheet.
  if ((outlineView && [[outlineView window] attachedSheet]) ||
      (target && [target respondsToSelector:@selector(window)] && [[target window] attachedSheet]))
  {
    NSArray* menuArray = [contextMenu itemArray];
    for (unsigned i = 0; i < [menuArray count]; i++) {
      [[menuArray objectAtIndex:i] setEnabled:NO];
    }
  }

  return contextMenu;
}

//
// Copy a set of bookmarks URLs to the specified pasteboard.
// We don't care about item titles here, nor do we care about format.
// Separators have no URL and are ignored.
//
- (void)copyBookmarksURLs:(NSArray*)bookmarkItems toPasteboard:(NSPasteboard*)aPasteboard
{
  // handle URLs, and nothing else, for simplicity.
  [aPasteboard declareTypes:[NSArray arrayWithObject:kCorePasteboardFlavorType_url] owner:nil];

  NSMutableArray* urlList = [NSMutableArray array];
  NSMutableSet* seenBookmarks = [NSMutableSet setWithCapacity:[bookmarkItems count]];
  NSEnumerator* bookmarkItemsEnum = [bookmarkItems objectEnumerator];
  BookmarkItem* curItem;
  while (curItem = [bookmarkItemsEnum nextObject]) {
    if ([curItem isKindOfClass:[Bookmark class]] && ![curItem isSeparator] && ![seenBookmarks containsObject:curItem]) {
      [seenBookmarks addObject:curItem]; // now we've seen it
      [urlList addObject:[(Bookmark*)curItem url]];
    }
    else if ([curItem isKindOfClass:[BookmarkFolder class]]) {
      // get all child bookmarks in a nice flattened array
      NSArray* children = [(BookmarkFolder*)curItem allChildBookmarks];
      NSEnumerator* childrenEnum = [children objectEnumerator];
      Bookmark* curChild;
      while (curChild = [childrenEnum nextObject]) {
        if (![seenBookmarks containsObject:curChild] && ![curItem isSeparator]) {
          [seenBookmarks addObject:curChild]; // now we've seen it
          [urlList addObject:[curChild url]];
        }
      }
    }
  }
  [aPasteboard setURLs:urlList withTitles:nil];
}


#pragma mark -

//
// Methods relating to the multiplexing of page load and site icon load notifications
//

+ (void)addItem:(id)inBookmark toURLMap:(NSMutableDictionary*)urlMap usingURL:(NSString*)inURL
{
  NSMutableSet* urlSet = [urlMap objectForKey:inURL];
  if (!urlSet) {
    urlSet = [[NSMutableSet alloc] initWithCapacity:1];
    [urlMap setObject:urlSet forKey:inURL];
    [urlSet release];
  }
  [urlSet addObject:inBookmark];
}

// url may be nil, in which case exhaustive search is used
+ (void)removeItem:(id)inBookmark fromURLMap:(NSMutableDictionary*)urlMap usingURL:(NSString*)inURL
{
  if (inURL) {
    NSMutableSet* urlSet = [urlMap objectForKey:inURL];
    if (urlSet)
      [urlSet removeObject:inBookmark];
  }
  else {
    NSEnumerator* urlMapEnum = [urlMap objectEnumerator];
    NSMutableSet* curSet;
    while ((curSet = [urlMapEnum nextObject])) {
      if ([curSet containsObject:inBookmark]) {
        [curSet removeObject:inBookmark];
        break;   // it should only be in one set
      }
    }
  }
}

// unregister the bookmark using its old favicon url, set the new one (which might be nil),
// and reregister (setting a nil favicon url makes it use the default)
- (void)setAndReregisterFaviconURL:(NSString*)inFaviconURL forBookmark:(Bookmark*)inBookmark
{
  [BookmarkManager removeItem:inBookmark fromURLMap:mBookmarkFaviconURLMap usingURL:[BookmarkManager faviconURLForBookmark:inBookmark]];
  [inBookmark setFaviconURL:inFaviconURL];
  [BookmarkManager addItem:inBookmark toURLMap:mBookmarkFaviconURLMap usingURL:[BookmarkManager faviconURLForBookmark:inBookmark]];
}


+ (NSEnumerator*)enumeratorForBookmarksInMap:(NSMutableDictionary*)urlMap matchingURL:(NSString*)inURL
{
  return [[urlMap objectForKey:inURL] objectEnumerator];
}

- (void)registerBookmarkForLoads:(Bookmark*)inBookmark
{
  NSString* bookmarkURL = [BookmarkManager canonicalBookmarkURL:[inBookmark url]];

  // add to the bookmark url map
  [BookmarkManager addItem:inBookmark toURLMap:mBookmarkURLMap usingURL:bookmarkURL];

  // and add it to the site icon map
  NSString* faviconURL = [BookmarkManager faviconURLForBookmark:inBookmark];
  if ([faviconURL length] > 0)
    [BookmarkManager addItem:inBookmark toURLMap:mBookmarkFaviconURLMap usingURL:faviconURL];
}

- (void)unregisterBookmarkForLoads:(Bookmark*)inBookmark ignoringURL:(BOOL)inIgnoreURL
{
  NSString* bookmarkURL = inIgnoreURL ? nil : [BookmarkManager canonicalBookmarkURL:[inBookmark url]];
  [BookmarkManager removeItem:inBookmark fromURLMap:mBookmarkURLMap usingURL:bookmarkURL];

  NSString* faviconURL = [BookmarkManager faviconURLForBookmark:inBookmark];
  if ([faviconURL length] > 0)
    [BookmarkManager removeItem:inBookmark fromURLMap:mBookmarkFaviconURLMap usingURL:faviconURL];
}


- (void)onSiteIconLoad:(NSNotification *)inNotification
{
  NSDictionary* userInfo = [inNotification userInfo];
  //NSLog(@"onSiteIconLoad %@", inNotification);
  if (!userInfo)
    return;

  NSImage*  iconImage    = [userInfo objectForKey:SiteIconLoadImageKey];
  NSString* siteIconURI  = [userInfo objectForKey:SiteIconLoadURIKey];
  NSString* pageURI      = [userInfo objectForKey:SiteIconLoadUserDataKey];
  pageURI = [BookmarkManager canonicalBookmarkURL:pageURI];

  BOOL isDefaultSiteIconLocation = [siteIconURI isEqualToString:[SiteIconProvider defaultFaviconLocationStringFromURI:pageURI]];

  if (iconImage) {
    Bookmark* curBookmark;

    // look for bookmarks to this page. we might not have registered
    // this bookmark for a custom <link> favicon url yet
    NSArray* bookmarksForPage = [[mBookmarkURLMap objectForKey:pageURI] allObjects];
    NSEnumerator* bookmarksForPageEnum = [bookmarksForPage objectEnumerator];
    // note that we don't enumerate over the NSMutableSet directly, because we'll be
    // changing it inside the loop
    while ((curBookmark = [bookmarksForPageEnum nextObject])) {
      if (isDefaultSiteIconLocation) {
        // if we've got one from the default location, but the bookmark has a custom linked icon,
        // so remove the custom link
        if ([[curBookmark faviconURL] length] > 0)
          [self setAndReregisterFaviconURL:nil forBookmark:curBookmark];
      }
      else {  // custom location
        if (![[curBookmark faviconURL] isEqualToString:siteIconURI])
          [self setAndReregisterFaviconURL:siteIconURI forBookmark:curBookmark];
      }
    }

    // update bookmarks known to be using this favicon url
    NSEnumerator* bookmarksEnum = [BookmarkManager enumeratorForBookmarksInMap:mBookmarkFaviconURLMap matchingURL:siteIconURI];
    while ((curBookmark = [bookmarksEnum nextObject])) {
      [curBookmark setIcon:iconImage];
    }
  }
  else {
    // we got no image. If this was a network load for a custom favicon url, clear the favicon url from the bookmarks which use it
    BOOL networkLoad = [[userInfo objectForKey:SiteIconLoadUsedNetworkKey] boolValue];
    if (networkLoad && !isDefaultSiteIconLocation) {
      NSArray* bookmarksForPage = [[mBookmarkURLMap objectForKey:pageURI] allObjects];
      NSEnumerator* bookmarksForPageEnum = [bookmarksForPage objectEnumerator];
      // note that we don't enumerate over the NSMutableSet directly, because we'll be
      // changing it inside the loop
      Bookmark* curBookmark;
      while ((curBookmark = [bookmarksForPageEnum nextObject])) {
        // clear any custom favicon urls
        if ([[curBookmark faviconURL] isEqualToString:siteIconURI])
          [self setAndReregisterFaviconURL:nil forBookmark:curBookmark];
      }
    }
  }
}

- (void)onPageLoad:(NSNotification*)inNotification
{
  NSString* loadURL = [BookmarkManager canonicalBookmarkURL:[inNotification object]];
  BOOL successfullLoad = [[[inNotification userInfo] objectForKey:URLLoadSuccessKey] boolValue];

  NSEnumerator* bookmarksEnum = [BookmarkManager enumeratorForBookmarksInMap:mBookmarkURLMap matchingURL:loadURL];
  Bookmark* curBookmark;
  while ((curBookmark = [bookmarksEnum nextObject])) {
    [curBookmark notePageLoadedWithSuccess:successfullLoad];
  }
}

#pragma mark -

//
// BookmarkClient protocol - so we know when to write out
//
- (void)bookmarkAdded:(NSNotification *)inNotification
{
  // we only care about additions to non-smart folders.
  BookmarkItem* bmItem = [[inNotification userInfo] objectForKey:BookmarkFolderChildKey];
  BookmarkFolder* parentFolder = [inNotification object];

  if ([parentFolder isSmartFolder])
    return;

  if ([bmItem isKindOfClass:[Bookmark class]]) {
    if ([NSWorkspace supportsSpotlight])
      [bmItem writeBookmarksMetadataToPath:mMetadataPath];

    [self registerBookmarkForLoads:(Bookmark*)bmItem];
  }

  [self noteBookmarksChanged];
}

- (void)bookmarkRemoved:(NSNotification *)inNotification
{
  BookmarkItem* bmItem = [[inNotification userInfo] objectForKey:BookmarkFolderChildKey];

  if ([bmItem isKindOfClass:[BookmarkFolder class]]) {
    if ([(BookmarkFolder*)bmItem containsChildItem:mLastUsedFolder]) {
      [mLastUsedFolder release];
      mLastUsedFolder = nil;
    }
  }

  BookmarkFolder* parentFolder = [inNotification object];
  if ([parentFolder isSmartFolder])
    return;

  if ([bmItem isKindOfClass:[Bookmark class]]) {
    if ([NSWorkspace supportsSpotlight])
      [bmItem removeBookmarksMetadataFromPath:mMetadataPath];

    [self unregisterBookmarkForLoads:(Bookmark*)bmItem ignoringURL:YES];
  }

  [self noteBookmarksChanged];
}

- (void)bookmarkChanged:(NSNotification *)inNotification
{
  id item = [inNotification object];

  // don't write out the bookmark file or metadata for changes in a smart container.
  // we should really check to see that the bookmarks is in the tree, rather than
  // just checking its parent
  if (![item parent] || [(BookmarkFolder *)[item parent] isSmartFolder])
    return;

  unsigned int changeFlags = kBookmarkItemEverythingChangedMask;
  NSNumber* noteChangeFlags = [[inNotification userInfo] objectForKey:BookmarkItemChangedFlagsKey];
  if (noteChangeFlags)
    changeFlags = [noteChangeFlags unsignedIntValue];

  if ([item isKindOfClass:[Bookmark class]]) {
    // update Spotlight metadata
    if ([NSWorkspace supportsSpotlight] && (changeFlags & kBookmarkItemSignificantChangeFlagsMask))
      [item writeBookmarksMetadataToPath:mMetadataPath];

    // and re-register in the maps if the url changed
    if (changeFlags & kBookmarkItemURLChangedMask) {
      // since we've lost the old url, we have to unregister the slow way
      [self unregisterBookmarkForLoads:item ignoringURL:YES];
      [self registerBookmarkForLoads:item];
    }
  }

  if (changeFlags & kBookmarkItemSignificantChangeFlagsMask)
    [self noteBookmarksChanged];
}

- (void)noteBookmarksChanged
{
  // post a coalescing notification to write the bookmarks file
  NSNotification *note = [NSNotification notificationWithName:kWriteBookmarkNotification object:self userInfo:nil];
  [[NSNotificationQueue defaultQueue] enqueueNotification:note
                                             postingStyle:NSPostASAP
                                             coalesceMask:NSNotificationCoalescingOnName
                                                 forModes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
}

- (void)writeBookmarks:(NSNotification *)inNotification
{
  // NSLog(@"Saving bookmarks");
  [self writePropertyListFile:mPathToBookmarkFile];
}

//
// -writeBookmarksMetadataForSpotlight
//
// If we're running on Tiger, write out a flat list of all bookmarks in the caches folder
// so that Spotlight can parse them. We don't need to write our own metadata plugin, we piggyback
// the one that Safari uses which launches the default browser when selected. This blows
// away any previous cache and ensures that everything is up-to-date.
//
// Note that this is called on a thread, so it takes pains to ensure that the data
// it's working with won't be changing on the UI thread
//
- (void)writeBookmarksMetadataForSpotlight
{
  if (![NSWorkspace supportsSpotlight])
    return;

  // XXX if we quit while this thread is still running, we'll end up with incomplete metadata
  // on disk, but it will get rebuilt on the next launch.

  NSArray* allBookmarkItems = [mRootBookmarks allChildBookmarks];

  // build up the path and ensure the folders are present along the way. Removes the
  // previous version entirely.
  NSString* metadataPath = [@"~/Library/Caches/Metadata" stringByExpandingTildeInPath];
  [[NSFileManager defaultManager] createDirectoryAtPath:metadataPath attributes:nil];

  metadataPath = [metadataPath stringByAppendingPathComponent:@"Camino"];
  [[NSFileManager defaultManager] createDirectoryAtPath:metadataPath attributes:nil];

  // delete any existing contents
  NSEnumerator* dirContentsEnum = [[[NSFileManager defaultManager] directoryContentsAtPath:metadataPath] objectEnumerator];
  NSString* curFile;
  while ((curFile = [dirContentsEnum nextObject])) {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    NSString* curFilePath = [metadataPath stringByAppendingPathComponent:curFile];
    [[NSFileManager defaultManager] removeFileAtPath:curFilePath handler:nil];

    [pool release];
  }

  // save the path for later
  [mMetadataPath autorelease];
  mMetadataPath = [metadataPath retain];

  unsigned int itemCount = 0;
  NSEnumerator* bmEnumerator = [allBookmarkItems objectEnumerator];
  BookmarkItem* curItem;
  while ((curItem = [bmEnumerator nextObject])) {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    [curItem writeBookmarksMetadataToPath:mMetadataPath];

    if (!(++itemCount % 100) && ![NSThread inMainThread])
      usleep(10000);    // 10ms to give the UI some time

    [pool release];
  }
}

#pragma mark -

//
// Reading/Importing bookmark files
//
- (BOOL)readBookmarks
{
  NSString *profileDir = [[PreferenceManager sharedInstance] profilePath];

  //
  // figure out where Bookmarks.plist is and store it as mPathToBookmarkFile
  // if there is a Bookmarks.plist, read it
  // if there isn't a Bookmarks.plist, but there is a bookmarks.xml, read it.
  // if there isn't either, move default Bookmarks.plist to profile dir & read it.
  //
  NSFileManager *fM = [NSFileManager defaultManager];
  NSString *bookmarkPath = [profileDir stringByAppendingPathComponent:@"bookmarks.plist"];
  [self setPathToBookmarkFile:bookmarkPath];
  BOOL bookmarksAreCorrupt = NO;
  if ([fM isReadableFileAtPath:bookmarkPath]) {
    NSString *backupPath = [bookmarkPath stringByAppendingString:@".bak"];
    if ([self readPListBookmarks:bookmarkPath]) {
      // since the bookmarks look good, save them aside as a backup in case something goes
      // wrong later (e.g., bug 337750) since users really don't like losing their bookmarks.
      if ([fM fileExistsAtPath:backupPath])
        [fM removeFileAtPath:backupPath handler:self];
      [fM copyPath:bookmarkPath toPath:backupPath handler:self];

      return YES;
    }
    else {
      bookmarksAreCorrupt = YES;
      // save the corrupted bookmarks to a backup file
      long long bmFileSize = [fM sizeOfFileAtPath:bookmarkPath traverseLink:YES];
      NSLog(@"Corrupted bookmarks file '%@' is %qi bytes", bookmarkPath, bmFileSize);
      NSDictionary* fileAttributesDict = [fM fileAttributesAtPath:bookmarkPath traverseLink:YES];
      NSDate* modificationDate = [fileAttributesDict objectForKey:NSFileModificationDate];
      NSLog(@"Corrupted bookmarks.plist was last modified %@", modificationDate);

      NSString* uniqueName = [fM backupFileNameFromPath:bookmarkPath withSuffix:@"-corrupted"];
      if ([fM movePath:bookmarkPath toPath:uniqueName handler:nil])
        NSLog(@"Moved corrupted bookmarks file to '%@'", uniqueName);
      else
        NSLog(@"Failed to move corrupted bookmarks file to '%@'", uniqueName);

      // Try to recover from the backup, if there is one
      if ([fM fileExistsAtPath:backupPath]) {
        if ([self readPListBookmarks:backupPath]) {
          // This is a background thread, so we can't put up an alert directly.
          [self performSelectorOnMainThread:@selector(showRestoredBookmarksAlert) withObject:nil waitUntilDone:NO];
          NSLog(@"Recovering from backup bookmarks file '%@'", backupPath);

          [fM copyPath:backupPath toPath:bookmarkPath handler:self];
          return YES;
        }
      }
    }
  }
  else if ([fM isReadableFileAtPath:[profileDir stringByAppendingPathComponent:@"bookmarks.xml"]]) {
    if ([self readCaminoXMLBookmarks:[profileDir stringByAppendingPathComponent:@"bookmarks.xml"]])
      return YES;
  }

  // if we're here, we have either no bookmarks or corrupted bookmarks with no backup; either way,
  // install the default plist so the bookmarks aren't totally empty.
  NSString *defaultBookmarks = [[NSBundle mainBundle] pathForResource:@"bookmarks" ofType:@"plist"];
  if ([fM copyPath:defaultBookmarks toPath:bookmarkPath handler:nil]) {
    if ([self readPListBookmarks:bookmarkPath] && !bookmarksAreCorrupt)
      return YES;
  }

  // if we're here, we've had a problem.
  // This is a background thread, so we can't put up an alert directly.
  [self performSelectorOnMainThread:@selector(showCorruptBookmarksAlert) withObject:nil waitUntilDone:NO];

  return NO;
}

- (void)showCorruptBookmarksAlert
{
  NSRunAlertPanel(NSLocalizedString(@"CorruptedBookmarksAlert", nil),
                  NSLocalizedString(@"CorruptedBookmarksMsg", nil),
                  NSLocalizedString(@"OKButtonText", nil),
                  nil,
                  nil);
}

- (void)showRestoredBookmarksAlert
{
  if (NSRunAlertPanel(NSLocalizedString(@"RestoredBookmarksAlert", nil),
                      NSLocalizedString(@"RestoredBookmarksMsg", nil),
                      NSLocalizedString(@"RestoredBookmarksInfoButton", nil),
                      NSLocalizedString(@"CancelButtonText", nil),
                      nil) == NSAlertDefaultReturn)
  {
    [[NSApp delegate] showURL:NSLocalizedStringFromTable(@"CorruptedBookmarksDefault", @"WebsiteDefaults", nil)];
  }
}

- (BOOL)readPListBookmarks:(NSString *)pathToFile
{
  NSDictionary* dict = [NSDictionary dictionaryWithContentsOfFile:pathToFile];
  if (!dict) return NO;

  // see if it's safari
  if ([dict objectForKey:@"WebBookmarkType"])
    return [self readSafariPListBookmarks:dict];

  return [self readCaminoPListBookmarks:dict];
}

- (BOOL)readCaminoPListBookmarks:(NSDictionary *)plist
{
  if (![[self rootBookmarks] readNativeDictionary:plist])
    return NO;    // read failed

  // find the menu and toolbar folders
  BookmarkFolder* menuFolder = nil;
  BookmarkFolder* toolbarFolder = nil;

  NSEnumerator* rootFoldersEnum = [[[self rootBookmarks] childArray] objectEnumerator];
  id curChild;
  while ((curChild = [rootFoldersEnum nextObject])) {
    if ([curChild isKindOfClass:[BookmarkFolder class]]) {
      BookmarkFolder* bmFolder = (BookmarkFolder*)curChild;
      if ([bmFolder isToolbar]) {
        toolbarFolder = bmFolder; // remember that we've seen it
        [bmFolder setIdentifier:kBookmarksToolbarFolderIdentifier];
      }
      else if (!menuFolder) {
        menuFolder = bmFolder;
        [bmFolder setIdentifier:kBookmarksMenuFolderIdentifier];
      }

      if (toolbarFolder && menuFolder)
        break;
    }
  }

  if (!menuFolder) {
    menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
    [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", nil)];
    [[self rootBookmarks] insertChild:menuFolder atIndex:kBookmarkMenuContainerIndex isMove:NO];
  }

  if (!toolbarFolder) {
    toolbarFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksToolbarFolderIdentifier] autorelease];
    [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", nil)];
    [toolbarFolder setIsToolbar:YES];
    [[self rootBookmarks] insertChild:toolbarFolder atIndex:kToolbarContainerIndex isMove:NO];
  }

  return YES;
}

- (BOOL)readSafariPListBookmarks:(NSDictionary *)plist
{
  BOOL readOK = [[self rootBookmarks] readSafariDictionary:plist];
  if (!readOK) return NO;

  // find the menu and toolbar folders
  BookmarkFolder* menuFolder = nil;
  BookmarkFolder* toolbarFolder = nil;

  NSEnumerator* rootFoldersEnum = [[[self rootBookmarks] childArray] objectEnumerator];
  id curChild;
  while ((curChild = [rootFoldersEnum nextObject])) {
    if ([curChild isKindOfClass:[BookmarkFolder class]]) {
      BookmarkFolder* bmFolder = (BookmarkFolder*)curChild;
      if ([[bmFolder title] isEqualToString:@"BookmarksBar"]) {
        toolbarFolder = bmFolder; // remember that we've seen it
        [bmFolder setIsToolbar:YES];
        [bmFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", nil)];
        [bmFolder setIdentifier:kBookmarksToolbarFolderIdentifier];
      }
      else if ([[bmFolder title] isEqualToString:@"BookmarksMenu"]) {
        menuFolder = bmFolder;
        [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", nil)];
        [bmFolder setIdentifier:kBookmarksMenuFolderIdentifier];
      }

      if (toolbarFolder && menuFolder)
        break;
    }
  }

  if (!menuFolder) {
    menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
    [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", nil)];
    [[self rootBookmarks] insertChild:menuFolder atIndex:kBookmarkMenuContainerIndex isMove:NO];
  }

  if (!toolbarFolder) {
    toolbarFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksToolbarFolderIdentifier] autorelease];
    [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", nil)];
    [toolbarFolder setIsToolbar:YES];
    [[self rootBookmarks] insertChild:toolbarFolder atIndex:kToolbarContainerIndex isMove:NO];
  }

  return YES;
}

- (BOOL)readCaminoXMLBookmarks:(NSString *)pathToFile
{
  BookmarkFolder* menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
  [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", nil)];
  [[self rootBookmarks] appendChild:menuFolder];

  BOOL readOK = [self importCaminoXMLFile:pathToFile intoFolder:menuFolder settingToolbarFolder:YES];

  // even if we didn't do a full read, try to fix up the toolbar folder
  BookmarkFolder* toolbarFolder = nil;
  NSEnumerator* bookmarksEnum = [[self rootBookmarks] objectEnumerator];
  BookmarkItem* curItem;
  while ((curItem = [bookmarksEnum nextObject])) {
    if ([curItem isKindOfClass:[BookmarkFolder class]]) {
      BookmarkFolder* curFolder = (BookmarkFolder*)curItem;
      if ([curFolder isToolbar]) {
        toolbarFolder = curFolder;
        break;
      }
    }
  }

  if (toolbarFolder) {
    [[toolbarFolder parent] moveChild:toolbarFolder toBookmarkFolder:[self rootBookmarks] atIndex:kToolbarContainerIndex];
    [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", nil)];
    [toolbarFolder setIdentifier:kBookmarksToolbarFolderIdentifier];
  }

  return readOK;
}

- (void)startImportBookmarks
{
  if (!mImportDlgController)
    mImportDlgController = [[BookmarkImportDlgController alloc] initWithWindowNibName:@"BookmarkImportDlg"];

  [mImportDlgController buildAvailableFileList];
  [mImportDlgController showWindow:nil];
}

- (void)importBookmarksThreadEntry:(NSDictionary *)aDict
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  BOOL success = TRUE;
  int currentFile = 0;
  NSArray *pathArray = [aDict objectForKey:kBookmarkImportPathIndentifier];
  NSArray *titleArray = [aDict objectForKey:kBookmarkImportNewFolderNameIdentifier];
  NSString *pathToFile;
  NSString *aTitle;
  BookmarkFolder *topImportFolder = [[BookmarkFolder alloc] init];
  BookmarkFolder *importFolder = topImportFolder;

  NSEnumerator *pathEnumerator = [pathArray objectEnumerator];
  NSEnumerator *titleEnumerator = [titleArray objectEnumerator];

  [self startSuppressingChangeNotifications];

  while (success && (pathToFile = [pathEnumerator nextObject])) {
    if (!importFolder)
      importFolder = [[BookmarkFolder alloc] init];

    NSString *extension = [[pathToFile pathExtension] lowercaseString];
    if ([extension isEqualToString:@""]) // we'll go out on a limb here
      success = [self readOperaFile:pathToFile intoFolder:importFolder];
    else if ([extension isEqualToString:@"html"] || [extension isEqualToString:@"htm"])
      success = [self importHTMLFile:pathToFile intoFolder:importFolder];
    else if ([extension isEqualToString:@"xml"])
      success = [self importCaminoXMLFile:pathToFile intoFolder:importFolder settingToolbarFolder:NO];
    else if ([extension isEqualToString:@"plist"] || !success)
      success = [self importPropertyListFile:pathToFile intoFolder:importFolder];
    // we don't know the extension, or we failed to load.  we'll take another
    // crack at it trying everything we know.
    if (!success) {
      success = [self readOperaFile:pathToFile intoFolder:importFolder];
      if (!success) {
        success = [self importHTMLFile:pathToFile intoFolder:importFolder];
        if (!success) {
          success = [self importCaminoXMLFile:pathToFile intoFolder:importFolder settingToolbarFolder:NO];
        }
      }
    }

    aTitle = [titleEnumerator nextObject];

    if (!aTitle)
      aTitle = NSLocalizedString(@"Imported Bookmarks", nil);

    [importFolder setTitle:aTitle];

    if (importFolder != topImportFolder) {
      [topImportFolder appendChild:importFolder];
      [importFolder release];
    }

    importFolder = nil;
    currentFile++;
  }

  [self stopSuppressingChangeNotifications];

  NSDictionary *returnDict = [NSDictionary dictionaryWithObjectsAndKeys:
    [NSNumber numberWithBool:success], kBookmarkImportStatusIndentifier,
    [NSNumber numberWithInt:currentFile], kBookmarkImportNewFolderIndexIdentifier,
    pathArray, kBookmarkImportPathIndentifier,
    topImportFolder, kBookmarkImportNewFolderIdentifier,
    nil];

  [self performSelectorOnMainThread:@selector(importBookmarksThreadReturn:)
                         withObject:returnDict
                      waitUntilDone:YES];
  // release the top-level import folder we allocated - somebody else retains it by now if still needed.
  [topImportFolder release];

  [pool release];
}

- (void)importBookmarksThreadReturn:(NSDictionary *)aDict
{
  BOOL success = [[aDict objectForKey:kBookmarkImportStatusIndentifier] boolValue];
  NSArray *fileArray = [aDict objectForKey:kBookmarkImportPathIndentifier];
  int currentIndex = [[aDict objectForKey:kBookmarkImportNewFolderIndexIdentifier] intValue];
  BookmarkFolder *rootFolder = [self rootBookmarks];
  BookmarkFolder *importFolder = [aDict objectForKey:kBookmarkImportNewFolderIdentifier];
  if (success || ((currentIndex - [fileArray count]) > 0)) {
    NSUndoManager *undoManager = [self undoManager];
    [rootFolder appendChild:importFolder];
    [undoManager setActionName:NSLocalizedString(@"Import Bookmarks", nil)];
  }
    [mImportDlgController finishThreadedImport:success
                                     fromFile:[[fileArray objectAtIndex:(--currentIndex)] lastPathComponent] ];
}


// spits out text file as NSString with "proper" encoding based on pretty shitty detection
- (NSString *)decodedTextFile:(NSString *)pathToFile
{
  NSData* fileAsData = [[NSData alloc] initWithContentsOfFile:pathToFile];
  if (!fileAsData) {
    NSLog(@"decodedTextFile: file %@ cannot be read.", pathToFile);
    return nil;
  }
  // we're gonna assume for now it's ascii and hope for the best.
  // i'm doing this because I think we can always read it in as ascii,
  // while it might fail if we assume default system encoding.  i don't
  // know this for sure.  but we'll have to do 2 decodings.  big whoop.
  NSString *fileString = [[NSString alloc] initWithData:fileAsData encoding:NSASCIIStringEncoding];
  if (!fileString) {
    NSLog(@"decodedTextFile: file %@ doesn't want to become a string. Exiting.", pathToFile);
    [fileAsData release];
    return nil;
  }

  // Create a dictionary with possible encodings.  As I figure out more possible encodings,
  // I'll add them to the dictionary.
  NSString *utfdash8Key = @"content=\"text/html; charset=utf-8" ;
  NSString *xmacromanKey = @"content=\"text/html; charset=x-mac-roman";
  NSString *xmacsystemKey = @"CONTENT=\"text/html; charset=X-MAC-SYSTEM";
  NSString *shiftJisKey = @"CONTENT=\"text/html; charset=Shift_JIS";
  NSString *operaUTF8Key = @"encoding = utf8";

  NSDictionary *encodingDict = [NSDictionary dictionaryWithObjectsAndKeys:
    [NSNumber numberWithUnsignedInt:NSUTF8StringEncoding], utfdash8Key,
    [NSNumber numberWithUnsignedInt:NSMacOSRomanStringEncoding], xmacromanKey,
    [NSNumber numberWithUnsignedInt:NSShiftJISStringEncoding], shiftJisKey,
    [NSNumber numberWithUnsignedInt:[NSString defaultCStringEncoding]], xmacsystemKey,
    [NSNumber numberWithUnsignedInt:NSUTF8StringEncoding], operaUTF8Key,
    nil];

  NSEnumerator *keyEnumerator = [encodingDict keyEnumerator];
  id key;
  NSRange aRange;
  while ((key = [keyEnumerator nextObject])) {
    aRange = [fileString rangeOfString:key options:NSCaseInsensitiveSearch];
    if (aRange.location != NSNotFound) {
      [fileString release];
      fileString = [[NSString alloc] initWithData:fileAsData encoding:[[encodingDict objectForKey:key] unsignedIntValue]];
      [fileAsData release];
      return [fileString autorelease];
    }
  }
  // if we're here, we don't have a clue as to the encoding.  we'll guess default
  [fileString release];
  if ((fileString = [[NSString alloc] initWithData:fileAsData encoding:[NSString defaultCStringEncoding]])) {
    NSLog(@"decodedTextFile: file %@ encoding unknown. Assume default and proceed.", pathToFile);
    [fileAsData release];
    return [fileString autorelease];
  }
  // we suck.  this is almost certainly wrong, but oh well.
  NSLog(@"decodedTextFile: file %@ encoding unknown, and NOT default. Use ASCII and proceed.", pathToFile);
  fileString = [[NSString alloc] initWithData:fileAsData encoding:NSASCIIStringEncoding];
  [fileAsData release];
  return [fileString autorelease];
}


- (BOOL)importHTMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder
{
  // get file as NSString
  NSString* fileAsString = [self decodedTextFile:pathToFile];
  if (!fileAsString) {
    NSLog(@"couldn't read file. bailing out");
    return NO;
  }
  // Set up to scan the bookmark file
  NSScanner *fileScanner = [[NSScanner alloc] initWithString:fileAsString];
  [fileScanner setCharactersToBeSkipped:nil];
  BOOL isNetscape = YES;
  // See if it's a netscape/IE style bookmark file, or omniweb
  NSRange aRange = [fileAsString rangeOfString:@"<!DOCTYPE NETSCAPE-Bookmark-file-1>" options:NSCaseInsensitiveSearch];
  if (aRange.location != NSNotFound) {
    // netscape/IE setup - start after Title attribute
    [fileScanner scanUpToString:@"</TITLE>" intoString:NULL];
    [fileScanner setScanLocation:([fileScanner scanLocation] + 7)];
  }
  else {
    isNetscape = NO;
    aRange = [fileAsString rangeOfString:@"<bookmarkInfo" options:NSCaseInsensitiveSearch];
    if (aRange.location != NSNotFound)
      // omniweb setup - start at <bookmarkInfo
      [fileScanner scanUpToString:@"<bookmarkInfo" intoString:NULL];
    else {
      NSLog(@"Unrecognized style of Bookmark File. Read fails.");
      [fileScanner release];
      return NO;
    }
  }
  BookmarkFolder *currentArray = aFolder;
  BookmarkItem *currentItem = nil;
  NSScanner *tokenScanner = nil;
  NSString *tokenTag = nil, *tokenString = nil, *tempItem = nil;
  unsigned long scanIndex = 0;
  NSRange tempRange, keyRange;
  BOOL justSetTitle = NO;
  // Scan through file.  As we find a token, do something useful with it.
  while (![fileScanner isAtEnd]) {
    [fileScanner scanUpToString:@"<" intoString:&tokenString];
    scanIndex = [fileScanner scanLocation];
    if ((scanIndex + 3) < [fileAsString length]) {
      tokenTag = [[NSString alloc] initWithString:[[fileAsString substringWithRange:NSMakeRange(scanIndex, 3)] uppercaseString]];
      // now we pick out if it's something we want to save.
      // check in a "most likely thing first" order
      if ([tokenTag isEqualToString:@"<DT "]) {
        [fileScanner setScanLocation:(scanIndex + 1)];
      }
      else if ([tokenTag isEqualToString:@"<P>"]) {
        [fileScanner setScanLocation:(scanIndex + 1)];
      }
      else if ([tokenTag isEqualToString:@"<A "]) {
        // adding a new bookmark to end of currentArray.
        [fileScanner scanUpToString:@"</A>" intoString:&tokenString];  // fileScanner contains <A HREF="[URL]">[TITLE]</A>
        tokenScanner = [[NSScanner alloc] initWithString:tokenString];
        [tokenScanner scanUpToString:@"href=\"" intoString:nil];  // tokenScanner now contains HREF="[URL]">[TITLE]
        // check for a menu spacer, which will look like this: HREF="">&lt;Menu Spacer&gt; (bug 309008)
        if (![tokenScanner isAtEnd] && [[tokenString substringFromIndex:([tokenScanner scanLocation] + 8)] isEqualToString:@"&lt;Menu Spacer&gt;"])  {
          currentItem = [currentArray addBookmark];
          [(Bookmark *)currentItem setIsSeparator:YES];
          [tokenScanner release];
          [tokenTag release];
          [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
          continue;
        }
        if (![tokenScanner isAtEnd]) {
          [tokenScanner setScanLocation:([tokenScanner scanLocation] + 6)];
          [tokenScanner scanUpToString:@"\"" intoString:&tempItem];
          if ([tokenScanner isAtEnd]) {
            // we scanned up to the </A> but didn't find a " character ending the HREF. This is probably
            // because we're scanning a bookmarklet that contains an embedded <A></A> so we're in the
            // middle of the string. Just bail and don't import this bookmark. The parser should be able
            // to recover on its own once it gets to the next "<A" token.
            [tokenScanner release];
            [tokenTag release];
            [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
            continue;
          }
          currentItem = [currentArray addBookmark];
          [(Bookmark *)currentItem setUrl:[tempItem stringByRemovingAmpEscapes]];
          [tokenScanner scanUpToString:@">" intoString:&tempItem];
          if (![tokenScanner isAtEnd]) {     // protect against malformed files
            [currentItem setTitle:[[tokenString substringFromIndex:([tokenScanner scanLocation] + 1)] stringByRemovingAmpEscapes]];
            justSetTitle = YES;
          }
          // see if we had a keyword
          if (isNetscape) {
            tempRange = [tempItem rangeOfString:@"SHORTCUTURL=\"" options:NSCaseInsensitiveSearch];
            if (tempRange.location != NSNotFound) {
              // throw everything to next " into keyword. A malformed bookmark might not have a closing " which
              // will throw things out of whack slightly, but it's better than crashing.
              keyRange = [tempItem rangeOfString:@"\"" options:0 range:NSMakeRange(tempRange.location + tempRange.length, [tempItem length] - (tempRange.location + tempRange.length))];
              if (keyRange.location != NSNotFound)
                [currentItem setKeyword:[tempItem substringWithRange:NSMakeRange(tempRange.location + tempRange.length, keyRange.location - (tempRange.location + tempRange.length))]];
            }
          }
        }
        [tokenScanner release];
        [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
      }
      else if ([tokenTag isEqualToString:@"<DD"]) {
        // add a description to current item
        [fileScanner scanUpToString:@">" intoString:NULL];
        [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
        [fileScanner scanUpToString:@"<" intoString:&tokenString];
        [currentItem setItemDescription:[tokenString stringByRemovingAmpEscapes]];
        justSetTitle = NO;
      }
      else if ([tokenTag isEqualToString:@"<H3"]) {
        [fileScanner scanUpToString:@"</H3>" intoString:&tokenString];
        currentItem = [currentArray addBookmarkFolder];
        currentArray = (BookmarkFolder *)currentItem;
        tokenScanner = [[NSScanner alloc] initWithString:tokenString];
        if (isNetscape) {
          [tokenScanner scanUpToString:@">" intoString:&tempItem];
          [currentItem setTitle:[[tokenString substringFromIndex:([tokenScanner scanLocation] + 1)] stringByRemovingAmpEscapes]];
          // check for group
          tempRange = [tempItem rangeOfString:@"FOLDER_GROUP=\"true\"" options:NSCaseInsensitiveSearch];
          if (tempRange.location != NSNotFound)
            [(BookmarkFolder *)currentItem setIsGroup:YES];
        }
        else {
          // have to do this in chunks to handle omniweb 5
          [tokenScanner scanUpToString:@"<a" intoString:NULL];
          [tokenScanner scanUpToString:@">" intoString:NULL];
          [tokenScanner setScanLocation:([tokenScanner scanLocation] + 1)];
          [tokenScanner scanUpToString:@"</a>" intoString:&tempItem];
          [currentItem setTitle:[tempItem stringByRemovingAmpEscapes]];
        }
        [tokenScanner release];
        [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
      }
      else if ([tokenTag isEqualToString:@"<DL"]) {
        [fileScanner setScanLocation:(scanIndex + 1)];
      }
      else if ([tokenTag isEqualToString:@"</D"]) {
        // note that we only scan for the first two characters of a tag
        // that is why this tag is "</D" and not "</DL"
        currentArray = (BookmarkFolder *)[currentArray parent];
        [fileScanner setScanLocation:(scanIndex + 1)];
      }
      else if ([tokenTag isEqualToString:@"<H1"]) {
        [fileScanner scanUpToString:@">" intoString:NULL];
        [fileScanner scanUpToString:@"</H1>" intoString:NULL];
        [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
      }
      else if ([tokenTag isEqualToString:@"<BO"]) {
        //omniweb bookmark marker, no doubt
        [fileScanner scanUpToString:@">" intoString:NULL];
        [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
      }
      else if ([tokenTag isEqualToString:@"</A"]) {
        // some smartass has a description with </A in its title. Probably uses </H's, too.  Dork.
        // It will be just what was added, so append to string of last key.
        // This this can only happen on older Camino html exports.
        tempItem = [NSString stringWithString:[@"<" stringByAppendingString:[tokenString stringByRemovingAmpEscapes]]];
        if (justSetTitle)
          [currentItem setTitle:[[currentItem title] stringByAppendingString:tempItem]];
        else
          [currentItem setItemDescription:[[currentItem itemDescription] stringByAppendingString:tempItem]];
        [fileScanner setScanLocation:(scanIndex + 1)];
      }
      else if ([tokenTag isEqualToString:@"</H"]) {
        // if it's not html, we'll include in previous text string
        tempItem = [[NSString alloc] initWithString:[fileAsString substringWithRange:NSMakeRange(scanIndex, 1)]];
        if (([tempItem isEqualToString:@"</HT"]) || ([tempItem isEqualToString:@"</ht"]))
          [fileScanner scanUpToString:@">" intoString:NULL];
        else {
          [tempItem release];
          tempItem = [[NSString alloc] initWithString:[@"<" stringByAppendingString:[tokenString stringByRemovingAmpEscapes]]];
          if (justSetTitle)
            [currentItem setTitle:[[currentItem title] stringByAppendingString:tempItem]];
          else
            [currentItem setItemDescription:[[currentItem itemDescription] stringByAppendingString:tempItem]];
          [fileScanner setScanLocation:([fileScanner scanLocation] + 1)];
        }
        [tempItem release];
      }
      // Firefox menu separator
      else if ([tokenTag isEqualToString:@"<HR"]) {
          currentItem = [currentArray addBookmark];
          [(Bookmark *)currentItem setIsSeparator:YES];
          [fileScanner setScanLocation:(scanIndex + 1)];
      }
      else { //beats me.  just close the tag out and continue.
        [fileScanner scanUpToString:@">" intoString:NULL];
      }
      [tokenTag release];
    }
  }
  [fileScanner release];
  return YES;
}


- (BOOL)importCaminoXMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder settingToolbarFolder:(BOOL)setToolbarFolder
{
  NSURL* fileURL = [NSURL fileURLWithPath:pathToFile];
  if (!fileURL) {
    NSLog(@"URL creation failed");
    return NO;
  }
  // Thanks, Apple, for example XML parsing code.
  // Create CFXMLTree from file.  This needs to be released later
  CFXMLTreeRef xmlFileTree = CFXMLTreeCreateWithDataFromURL (kCFAllocatorDefault,
                                                             (CFURLRef)fileURL,
                                                             kCFXMLParserSkipWhitespace,
                                                             kCFXMLNodeCurrentVersion);
  if (!xmlFileTree) {
    NSLog(@"XMLTree creation failed");
    return NO;
  }

  // process top level nodes.  I think we'll find DTD
  // before data - so only need to make 1 pass.
  int count = CFTreeGetChildCount(xmlFileTree);
  for (int index = 0; index < count; index++) {
    CFXMLTreeRef subFileTree = CFTreeGetChildAtIndex(xmlFileTree, index);
    if (subFileTree) {
      CFXMLNodeRef bookmarkNode = CFXMLTreeGetNode(subFileTree);
      if (bookmarkNode) {
        switch (CFXMLNodeGetTypeCode(bookmarkNode)) {
          // make sure it's Camino/Chimera DTD
          case kCFXMLNodeTypeDocumentType:
            {
              CFXMLDocumentTypeInfo* docTypeInfo = (CFXMLDocumentTypeInfo *)CFXMLNodeGetInfoPtr(bookmarkNode);
              CFURLRef dtdURL = docTypeInfo->externalID.systemID;
              if (![[(NSURL *)dtdURL absoluteString] isEqualToString:@"http://www.mozilla.org/DTDs/ChimeraBookmarks.dtd"]) {
                NSLog(@"not a ChimeraBookmarks xml file. Bail");
                CFRelease(xmlFileTree);
                return NO;
              }
              break;
            }

          case kCFXMLNodeTypeElement:
            {
              BOOL readOK = [aFolder readCaminoXML:subFileTree settingToolbar:setToolbarFolder];
              CFRelease (xmlFileTree);
              return readOK;
            }
          default:
            break;
        }
      }
    }
  }
  CFRelease(xmlFileTree);
  NSLog(@"run through the tree and didn't find anything interesting.  Bailed out");
  return NO;
}

- (BOOL)importPropertyListFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder
{
  NSDictionary* dict = [NSDictionary dictionaryWithContentsOfFile:pathToFile];
  // see if it's safari
  if ([dict objectForKey:@"WebBookmarkType"])
    return [aFolder readSafariDictionary:dict];

  return [aFolder readNativeDictionary:dict];
}

- (BOOL)readOperaFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder
{
  // get file as NSString
  NSString* fileAsString = [self decodedTextFile:pathToFile];
  if (!fileAsString) {
    NSLog(@"couldn't read file. bailing out");
    return NO;
  }
  // Easily fooled check to see if it's an Opera Hotlist
  NSRange aRange;
  aRange = [fileAsString rangeOfString:@"Opera Hotlist" options:NSCaseInsensitiveSearch];
  if (aRange.location == NSNotFound) {
    NSLog(@"Bookmark file not recognized as Opera Hotlist.  Read fails.");
    return NO;
  }

  // Opera hotlists seem pretty easy to parse. Everything is on a line by itself.
  // So we'll split the string up into a giant array by newlines, and march through the array.
  BookmarkFolder *currentArray = aFolder;
  BookmarkItem *currentItem = nil;

  NSArray *arrayOfFileLines = [fileAsString componentsSeparatedByString:@"\n"];
  NSEnumerator *enumerator = [arrayOfFileLines objectEnumerator];
  NSString *aLine =nil;

  while ((aLine = [enumerator nextObject])) {
    // See if we have a new folder.
    if ([aLine hasPrefix:@"#FOLDER"]) {
      currentItem = [currentArray addBookmarkFolder];
      currentArray = (BookmarkFolder *)currentItem;
    }
    // Maybe it's a new URL!
    else if ([aLine hasPrefix:@"#URL"]) {
      currentItem = [currentArray addBookmark];
    }
    // Perhaps a separator? This isn't how I'd spell it, but
    // then again, I'm not Norwagian, so what do I know.
    //                         ^
    //                     That's funny
    else if ([aLine hasPrefix:@"#SEPERATOR"]) {
      currentItem = [currentArray addBookmark];
      [(Bookmark *)currentItem setIsSeparator:YES];
      currentItem = nil;
    }
    // Or maybe this folder is being closed out.
    else if ([aLine hasPrefix:@"-"] && currentArray != aFolder) {
      currentArray = [currentArray parent];
      currentItem = nil;
    }
    // Well, if we don't have a prefix, we'll look something else
    else {
      // We have to check for Name and Short Name at the same time...
      aRange = [aLine rangeOfString:@"NAME="];
      if (NSNotFound != aRange.location) {
        NSRange sRange = [aLine rangeOfString:@"SHORT NAME="];
        if (NSNotFound != sRange.location) {
          [currentItem setKeyword:[aLine substringFromIndex:(sRange.location + sRange.length)]];
        }
        else {
          [currentItem setTitle:[aLine substringFromIndex:(aRange.location + aRange.length)]];
        }
      }
      // ... then URL ...
      aRange = [aLine rangeOfString:@"URL="];
      if (NSNotFound != aRange.location && [currentItem isKindOfClass:[Bookmark class]]) {
        [(Bookmark *)currentItem setUrl:[aLine substringFromIndex:(aRange.location + aRange.length)]];
      }
      // ... followed by Description
      aRange = [aLine rangeOfString:@"DESCRIPTION="];
      if (NSNotFound != aRange.location) {
        [currentItem setItemDescription:[aLine substringFromIndex:(aRange.location + aRange.length)]];
      }
    }
  }

  return YES;
}

//
// Writing bookmark files
//

- (void)writeHTMLFile:(NSString *)pathToFile
{
  NSData *htmlData = [[[self rootBookmarks] writeHTML:0] dataUsingEncoding:NSUTF8StringEncoding];
  if (![htmlData writeToFile:[pathToFile stringByStandardizingPath] atomically:YES])
    NSLog(@"writeHTML: Failed to write file %@", pathToFile);
}

- (void)writeSafariFile:(NSString *)pathToFile
{
  NSDictionary* dict = [[self rootBookmarks] writeSafariDictionary];
  if (![dict writeToFile:[pathToFile stringByStandardizingPath] atomically:YES])
    NSLog(@"writeSafariFile: Failed to write file %@", pathToFile);
}

//
// -writePropertyListFile:
//
// Writes all the bookmarks as a plist to the given file path. Write the file in
// two steps in case the initial write fails.
//
- (void)writePropertyListFile:(NSString *)pathToFile
{
  if (![NSThread inMainThread]) {
    NSLog(@"writePropertyListFile: called from background thread");
    return;
  }

  if (!pathToFile) {
    NSLog(@"writePropertyListFile: nil path argument");
    return;
  }

  BookmarkFolder* rootBookmarks = [self rootBookmarks];
  if (!rootBookmarks)
    return;   // we never read anything

  NSDictionary* dict = [rootBookmarks writeNativeDictionary];
  if (!dict) {
    NSLog(@"writePropertyListFile: writeNativeDictionary returned nil dictionary");
    return;
  }

  NSString* stdPath = [pathToFile stringByStandardizingPath];
  // Use the more roundabout NSPropertyListSerialization/NSData method when possible
  // for now to try to get useful error data for bug 337750
  BOOL success;
  if ([NSData instancesRespondToSelector:@selector(writeToFile:options:error:)]) {
    NSString* errorString = nil;
    NSData* bookmarkData = [NSPropertyListSerialization dataFromPropertyList:dict
                                                                     format:NSPropertyListXMLFormat_v1_0
                                                           errorDescription:&errorString];
    if (!bookmarkData) {
      NSLog(@"writePropertyListFile: dataFromPropertyList returned nil data: %@", errorString);
      [errorString release];
      return;
    }
    NSError* error = nil;
    success = [bookmarkData writeToFile:stdPath options:NSAtomicWrite error:&error];
    if (!success)
      NSLog(@"writePropertyListFile: %@ (%@)",
            [error localizedDescription], [error localizedFailureReason]);
  }
  else {
    success = [dict writeToFile:stdPath atomically:YES];
  }
  if (!success)
    NSLog(@"writePropertyList: Failed to write file %@", pathToFile);
}

- (BOOL)fileManager:(NSFileManager *)manager shouldProceedAfterError:(NSDictionary *)errorInfo
{
  NSLog(@"fileManager:shouldProceedAfterError:%@", errorInfo);
  return NO;
}

@end
