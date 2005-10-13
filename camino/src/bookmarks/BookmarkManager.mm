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

#include <unistd.h>

#include "nsString.h"
#include "nsIContent.h"
#include "nsIFile.h"
#include "nsAppDirectoryServiceDefs.h"

#import "NSString+Utils.h"
#import "NSThread+Utils.h"
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
NSString* const kBookmarksToolbarFolderIdentifier      = @"BookmarksToolbarFolder";
NSString* const kBookmarksMenuFolderIdentifier         = @"BookmarksMenuFolder";

static NSString* const kTop10BookmarksFolderIdentifier = @"Top10BookmarksFolder";
static NSString* const kRendezvousFolderIdentifier     = @"RendezvousFolder";   // aka Bonjour
static NSString* const kAddressBookFolderIdentifier    = @"AddressBookFolder";
static NSString* const kHistoryFolderIdentifier        = @"HistoryFolder";

// these are suggested indices; we only use them to order the root folders, not to access them.
enum {
  kBookmarkMenuContainerIndex = 0,
  kToolbarContainerIndex = 1,
  kHistoryContainerIndex = 2,
};


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

// these versions assume that we're readinog all the bookmarks from the file (i.e. not an import into a subfolder)
- (BOOL)readPListBookmarks:(NSString *)pathToFile;    // camino or safari
- (BOOL)readCaminoPListBookmarks:(NSDictionary *)plist;
- (BOOL)readSafariPListBookmarks:(NSDictionary *)plist;
- (BOOL)readCaminoXMLBookmarks:(NSString *)pathToFile;

// these are "import" methods that import into a subfolder.
- (BOOL)importHTMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;
- (BOOL)importCaminoXMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder settingToolbarFolder:(BOOL)setToolbarFolder;
- (BOOL)importPropertyListFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder;

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

+ (BookmarkManager*)sharedBookmarkManager
{
  static BookmarkManager* sBookmarkManager = nil;
  if (!sBookmarkManager)
    sBookmarkManager = [[BookmarkManager alloc] init];

  return sBookmarkManager;
}

// serialize to an array of UUIDs
+ (NSArray*)serializableArrayWithBookmarkItems:(NSArray*)bmArray
{
  NSMutableArray* dataArray = [NSMutableArray arrayWithCapacity:[bmArray count]];
  NSEnumerator* bmEnum = [bmArray objectEnumerator];
  id bmItem;
  while ((bmItem = [bmEnum nextObject]))
  {
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
  while ((itemUUID = [dataEnum nextObject]))
  {
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
  if ((self = [super init]))
  {
    mBookmarkURLMap         = [[NSMutableDictionary alloc] initWithCapacity:50];
    mBookmarkFaviconURLMap  = [[NSMutableDictionary alloc] initWithCapacity:50];

    mBookmarksLoaded        = NO;
    mShowSiteIcons          = [[PreferenceManager sharedInstance] getBooleanPref:"browser.chrome.favicons" withSuccess:NULL];
  }
  
  return self;
}

-(void) dealloc
{
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

  [super dealloc];
}

- (void)loadBookmarksLoadingSynchronously:(BOOL)loadSync
{
  if (loadSync)
  {
    [self loadBookmarks];
  }
  else
  {
    [NSThread detachNewThreadSelector:@selector(loadBookmarksThreadEntry:) toTarget:self withObject:nil];
  }
}

- (void)loadBookmarksThreadEntry:(id)inObject
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  [self loadBookmarks];
  [pool release];
}

- (void)loadBookmarks
{
  BookmarkFolder* root = [[BookmarkFolder alloc] init];
  
  // We used to do this:
  // [root setParent:self];
  // but it was unclear why, and it broke logic in a bunch of places (like -setIsRoot).

  [root setIsRoot:YES];
  [root setTitle:NSLocalizedString(@"BookmarksRootName", @"")];
  [self setRootBookmarks:root];
  [root release];

  // Turn off the posting of update notifications while reading in bookmarks.
  // All interested parties haven't been init'd yet, and/or will recieve the
  // managerStartedNotification when setup is actually complete.
  [BookmarkItem setSuppressAllUpdateNotifications:YES];
  BOOL bookmarksReadOK = [self readBookmarks];
  if (!bookmarksReadOK)
  {
    // We'll come here either when reading the bookmarks totally failed, or
    // when we did a partial read of the xml file. The xml reading code already
    // fixed up the toolbar folder.
    if ([root count] == 0)
    {
      // failed to read any folders. make some by hand.
      BookmarkFolder* menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
      [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", @"")];
      [root appendChild:menuFolder];

      BookmarkFolder* toolbarFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksToolbarFolderIdentifier] autorelease];
      [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", @"")];
      [toolbarFolder setIsToolbar:YES];
      [root appendChild:toolbarFolder];
    }
  }
  
  [BookmarkItem setSuppressAllUpdateNotifications:NO];

  // make sure that the root folder has the special flag
  [[self rootBookmarks] setIsRoot:YES];

  // setup special folders
  [self setupSmartCollections];

  mSmartFolderManager = [[KindaSmartFolderManager alloc] initWithBookmarkManager:self];

#if 0
  // at some point, f'd up setting the bookmark toolbar folder special flag.
  // this'll handle that little boo-boo for the time being
  [[self toolbarFolder] setIsToolbar:YES];
  [[self toolbarFolder] setTitle:NSLocalizedString(@"Bookmark Bar", @"")];
  [[self bookmarkMenuFolder] setTitle:NSLocalizedString(@"Bookmark Menu", @"")]; 
#endif

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
  while ((thisBM = [bmEnum nextObject]))
  {
    [self registerBookmarkForLoads:thisBM];
  }

  // load favicons (w/out hitting the network, cache only). Spread it out so that we only get
  // ten every three seconds to avoid locking up the UI with large bookmark lists.
  // XXX probably want a better way to do this. This sets up a timer (internally) for every
  // bookmark
  if ([[PreferenceManager sharedInstance] getBooleanPref:"browser.chrome.favicons" withSuccess:NULL])
  {
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
  [mTop10Container setTitle:NSLocalizedString(@"Top Ten List", @"")];
  [mTop10Container setIsSmartFolder:YES];
  [mRootBookmarks insertChild:mTop10Container atIndex:(collectionIndex++) isMove:NO];
  
  mRendezvousContainer = [[BookmarkFolder alloc] initWithIdentifier:kRendezvousFolderIdentifier];
  [mRendezvousContainer setTitle:NSLocalizedString(@"Rendezvous", @"")];
  [mRendezvousContainer setIsSmartFolder:YES];
  [mRootBookmarks insertChild:mRendezvousContainer atIndex:(collectionIndex++) isMove:NO];
    
  mAddressBookContainer = [[BookmarkFolder alloc] initWithIdentifier:kAddressBookFolderIdentifier];
  [mAddressBookContainer setTitle:NSLocalizedString(@"Address Book", @"")];
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

-(BookmarkFolder *) rootBookmarks
{
  return mRootBookmarks;
}

-(BookmarkFolder *) dockMenuFolder
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
  for (unsigned int i = 0; i < numFolders; i ++)
  {
    id curItem = [rootFolders objectAtIndex:i];
    if ([curItem isKindOfClass:[BookmarkFolder class]] && [[curItem identifier] isEqualToString:inIdentifier])
      return (BookmarkFolder*)curItem;
  }
  return nil;
}

-(BookmarkFolder *)top10Folder
{
  return mTop10Container;
}

-(BookmarkFolder *) toolbarFolder
{
  return [self rootBookmarkFolderWithIdentifier:kBookmarksToolbarFolderIdentifier];
}

-(BookmarkFolder *) bookmarkMenuFolder
{
  return [self rootBookmarkFolderWithIdentifier:kBookmarksMenuFolderIdentifier];
}

-(BookmarkFolder *) historyFolder
{
  return [self rootBookmarkFolderWithIdentifier:kHistoryFolderIdentifier];
}

- (BOOL)isUserCollection:(BookmarkFolder *)inFolder
{
  return ([inFolder parent] == mRootBookmarks) &&
         ([[inFolder identifier] length] == 0);   // all our special folders have identifiers
}

- (unsigned)indexOfContainerItem:(BookmarkItem*)inItem
{
  return [mRootBookmarks indexOfObject:inItem];
}

- (BookmarkItem*)containerItemAtIndex:(unsigned)inIndex
{
  return [mRootBookmarks objectAtIndex:inIndex];
}

-(BookmarkFolder *) rendezvousFolder
{
  return mRendezvousContainer;
}

-(BookmarkFolder *) addressBookFolder
{
  return mAddressBookContainer;
}

- (BookmarkFolder*)lastUsedBookmarkFolder
{
  return mLastUsedFolder;
}

- (void)setLastUsedBookmarkFolder:(BookmarkFolder*)inFolder
{
  [mLastUsedFolder autorelease];
  mLastUsedFolder = [inFolder retain];
}

-(BookmarkItem*) itemWithUUID:(NSString*)uuid
{
  return [mRootBookmarks itemWithUUID:uuid];
}

-(NSUndoManager *) undoManager
{
  return mUndoManager;
}

- (void)setPathToBookmarkFile:(NSString *)aString
{
  [aString retain];
  [mPathToBookmarkFile release];
  mPathToBookmarkFile = aString;
}

-(void) setRootBookmarks:(BookmarkFolder *)anArray
{
  if (anArray != mRootBookmarks) {
    [anArray retain];
    [mRootBookmarks release];
    mRootBookmarks = anArray;
  }
}

-(NSArray *)resolveBookmarksKeyword:(NSString *)keyword
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
  if (resolvedArray)
    return resolvedArray;
  return [NSArray arrayWithObject:keyword];
}

// a null container indicates to search all bookmarks
-(NSArray *)searchBookmarksContainer:(BookmarkFolder*)container forString:(NSString *)searchString inFieldWithTag:(int)tag
{
  if ((searchString) && [searchString length] > 0)
  {
    BookmarkFolder* searchContainer = container ? container : [self rootBookmarks];
    NSSet *matchingSet = [searchContainer bookmarksWithString:searchString inFieldWithTag:tag];
    return [matchingSet allObjects];
  }
  return nil;
}

//
// Drag & drop
//

-(BOOL) isDropValid:(NSArray *)items toFolder:(BookmarkFolder *)parent
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
    } else if ([aBookmark isKindOfClass:[Bookmark class]]) {
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
  if ([items count] == 0) return nil;

  BOOL itemsContainsFolder = NO;
  BOOL itemsContainsBookmark = NO;
  BOOL multipleItems = ([items count] > 1);
  
  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    itemsContainsFolder   |= [curItem isKindOfClass:[BookmarkFolder class]];
    itemsContainsBookmark |= [curItem isKindOfClass:[Bookmark class]];
  }
  
  // All the methods in this context menu need to be able to handle > 1 item
  // being selected, and the selected items containing a mixture of folders
  // and bookmarks.
  NSMenu * contextMenu = [[[NSMenu alloc] initWithTitle:@"notitle"] autorelease];
  NSString * menuTitle = nil;
  
  // open in new window(s)
  if (itemsContainsFolder && [items count] == 1)
    menuTitle = NSLocalizedString(@"Open Tabs in New Window", @"");
  else if (multipleItems)
    menuTitle = NSLocalizedString(@"Open in New Windows", @"");
  else
    menuTitle = NSLocalizedString(@"Open in New Window", @"");

  NSMenuItem *menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openBookmarkInNewWindow:) keyEquivalent:@""] autorelease];
  [menuItem setTarget:target];
  [contextMenu addItem:menuItem];
  
  // open in new tabs in new window
  if (multipleItems)
  {
    menuTitle = NSLocalizedString(@"Open in Tabs in New Window", @"");
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openBookmarksInTabsInNewWindow:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }

  // open in new tab in current window
  if (itemsContainsFolder || multipleItems)
    menuTitle = NSLocalizedString(@"Open in New Tabs", @"");
  else
    menuTitle = NSLocalizedString(@"Open in New Tab", @"");
  menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openBookmarkInNewTab:) keyEquivalent:@""] autorelease];
  [menuItem setTarget:target];
  [contextMenu addItem:menuItem];
  
  if (!outlineView || ([items count] == 1)) {
    [contextMenu addItem:[NSMenuItem separatorItem]];
    menuTitle = NSLocalizedString(@"Get Info", @"");
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(showBookmarkInfo:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }
  
  if (([items count] == 1) && itemsContainsFolder) {
    menuTitle = NSLocalizedString(@"Use as Dock Menu", @"");
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(makeDockMenu:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:[items objectAtIndex:0]];
    [contextMenu addItem:menuItem];
  }
  
  BOOL allowNewFolder = NO;
  if ([target isKindOfClass:[BookmarkViewController class]]) {
    if (![[target activeCollection] isSmartFolder])
      allowNewFolder = YES;
  } else
    allowNewFolder = YES;

  if (allowNewFolder) {
    // space
    [contextMenu addItem:[NSMenuItem separatorItem]];
    // create new folder
    menuTitle = NSLocalizedString(@"Create New Folder...", @"");
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(addBookmarkFolder:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }
  
  // if we're not in a smart collection (other than history)
  if (!outlineView ||
      ![target isKindOfClass:[BookmarkViewController class]] ||
      ![[target activeCollection] isSmartFolder] ||
      ([target activeCollection] == [self historyFolder])) {
    // space
    [contextMenu addItem:[NSMenuItem separatorItem]];
    // delete
    menuTitle = NSLocalizedString(@"Delete", @"");
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(deleteBookmarks:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:target];
    [contextMenu addItem:menuItem];
  }
  return contextMenu;
}

#pragma mark -

// 
// Methods relating to the multiplexing of page load and site icon load notifications
// 

+ (void)addItem:(id)inBookmark toURLMap:(NSMutableDictionary*)urlMap usingURL:(NSString*)inURL
{
  NSMutableSet* urlSet = [urlMap objectForKey:inURL];
  if (!urlSet)
  {
    urlSet = [NSMutableSet setWithCapacity:1];
    [urlMap setObject:urlSet forKey:inURL];
  }
  [urlSet addObject:inBookmark];
}

// url may be nil, in which case exhaustive search is used
+ (void)removeItem:(id)inBookmark fromURLMap:(NSMutableDictionary*)urlMap usingURL:(NSString*)inURL
{
  if (inURL)
  {
    NSMutableSet* urlSet = [urlMap objectForKey:inURL];
    if (urlSet)
      [urlSet removeObject:inBookmark];
  }
  else
  {
    NSEnumerator* urlMapEnum = [urlMap objectEnumerator];
    NSMutableSet* curSet;
    while ((curSet = [urlMapEnum nextObject]))
    {
      if ([curSet containsObject:inBookmark])
      {
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
  [BookmarkManager addItem:inBookmark toURLMap:mBookmarkFaviconURLMap usingURL:[BookmarkManager faviconURLForBookmark:inBookmark]];
}

- (void)unregisterBookmarkForLoads:(Bookmark*)inBookmark ignoringURL:(BOOL)inIgnoreURL
{
  NSString* bookmarkURL = inIgnoreURL ? nil : [BookmarkManager canonicalBookmarkURL:[inBookmark url]];
  [BookmarkManager removeItem:inBookmark fromURLMap:mBookmarkURLMap usingURL:bookmarkURL];
  
  [BookmarkManager removeItem:inBookmark fromURLMap:mBookmarkFaviconURLMap usingURL:[BookmarkManager faviconURLForBookmark:inBookmark]];
}


- (void)onSiteIconLoad:(NSNotification *)inNotification
{
  NSDictionary* userInfo = [inNotification userInfo];
  //NSLog(@"onSiteIconLoad %@", inNotification);
  if (!userInfo)
    return;

	NSImage*  iconImage     = [userInfo objectForKey:SiteIconLoadImageKey];
  NSString* siteIconURI  = [userInfo objectForKey:SiteIconLoadURIKey];
  NSString* pageURI      = [userInfo objectForKey:SiteIconLoadUserDataKey];
  pageURI = [BookmarkManager canonicalBookmarkURL:pageURI];

  BOOL isDefaultSiteIconLocation = [siteIconURI isEqualToString:[SiteIconProvider defaultFaviconLocationStringFromURI:pageURI]];

  if (iconImage)
  {
    Bookmark* curBookmark;

    // look for bookmarks to this page. we might not have registered
    // this bookmark for a custom <link> favicon url yet
    NSArray* bookmarksForPage = [[mBookmarkURLMap objectForKey:pageURI] allObjects];
    NSEnumerator* bookmarksForPageEnum = [bookmarksForPage objectEnumerator];
    // note that we don't enumerate over the NSMutableSet directly, because we'll be
    // changing it inside the loop
    while ((curBookmark = [bookmarksForPageEnum nextObject]))
    {
      if (isDefaultSiteIconLocation)
      {
        // if we've got one from the default location, but the bookmark has a custom linked icon,
        // so remove the custom link
        if ([[curBookmark faviconURL] length] > 0)
          [self setAndReregisterFaviconURL:nil forBookmark:curBookmark];
      }
      else   // custom location
      {
        if (![[curBookmark faviconURL] isEqualToString:siteIconURI])
          [self setAndReregisterFaviconURL:siteIconURI forBookmark:curBookmark];
      }
    }
    
    // update bookmarks known to be using this favicon url
    NSEnumerator* bookmarksEnum = [BookmarkManager enumeratorForBookmarksInMap:mBookmarkFaviconURLMap matchingURL:siteIconURI];
    while ((curBookmark = [bookmarksEnum nextObject]))
    {
      [curBookmark setIcon:iconImage];
    }
  }
  else
  {
    // we got no image. If this was a network load for a custom favicon url, clear the favicon url from the bookmarks which use it
    BOOL networkLoad = [[userInfo objectForKey:SiteIconLoadUsedNetworkKey] boolValue];
    if (networkLoad && !isDefaultSiteIconLocation)
    {
      NSArray* bookmarksForPage = [[mBookmarkURLMap objectForKey:pageURI] allObjects];
      NSEnumerator* bookmarksForPageEnum = [bookmarksForPage objectEnumerator];
      // note that we don't enumerate over the NSMutableSet directly, because we'll be
      // changing it inside the loop
      Bookmark* curBookmark;
      while ((curBookmark = [bookmarksForPageEnum nextObject]))
      {
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
  while ((curBookmark = [bookmarksEnum nextObject]))
  {
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

  if ([bmItem isKindOfClass:[Bookmark class]])
  {
    if ([MainController supportsSpotlight])
      [bmItem writeBookmarksMetadataToPath:mMetadataPath];

    [self registerBookmarkForLoads:bmItem];
  }
  
  [self noteBookmarksChanged];
}

- (void)bookmarkRemoved:(NSNotification *)inNotification
{
  BookmarkItem* bmItem = [[inNotification userInfo] objectForKey:BookmarkFolderChildKey];

  if ([bmItem isKindOfClass:[BookmarkFolder class]])
  {
    if ([(BookmarkFolder*)bmItem containsChildItem:mLastUsedFolder])
    {
      [mLastUsedFolder release];
      mLastUsedFolder = nil;
    }
  }

  BookmarkFolder* parentFolder = [inNotification object];
  if ([parentFolder isSmartFolder])
    return;

  if ([bmItem isKindOfClass:[Bookmark class]])
  {
    if ([MainController supportsSpotlight])
      [bmItem removeBookmarksMetadataFromPath:mMetadataPath];

    [self unregisterBookmarkForLoads:bmItem ignoringURL:YES];
  }
  
  [self noteBookmarksChanged];
}

- (void)bookmarkChanged:(NSNotification *)inNotification
{
  // don't write out the bookmark file or metadata for changes in a smart container.
  id item = [inNotification object];
  if ([[item parent] isSmartFolder])
    return;

  unsigned int changeFlags = kBookmarkItemEverythingChangedMask;
  NSNumber* noteChangeFlags = [[inNotification userInfo] objectForKey:BookmarkItemChangedFlagsKey];
  if (noteChangeFlags)
    changeFlags = [noteChangeFlags unsignedIntValue];

  if ([item isKindOfClass:[Bookmark class]])
  {
    // update Spotlight metadata
    if ([MainController supportsSpotlight] && (changeFlags & kBookmarkItemSignificantChangeFlagsMask))
      [item writeBookmarksMetadataToPath:mMetadataPath];
    
    // and re-register in the maps if the url changed
    if (changeFlags & kBookmarkItemURLChangedMask)
    {
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
  [[NSNotificationQueue defaultQueue] enqueueNotification:note postingStyle:NSPostASAP coalesceMask:NSNotificationCoalescingOnName forModes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];   
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
  if (![MainController supportsSpotlight])
    return;

  // XXX if we quit while this thread is still running, we'll end up with incomplete metadata
  // on disk, but it will get rebuilt on the next launch.

  NSArray* allBookmarkItems = [mRootBookmarks allChildBookmarks];
  
  // build up the path and ensure the folders are present along the way. Removes the
  // previous version entirely.
  NSString* metadataPath = [@"~/Library/Caches/Metadata" stringByExpandingTildeInPath];
  [[NSFileManager defaultManager] createDirectoryAtPath:metadataPath attributes:nil];
  metadataPath = [metadataPath stringByAppendingPathComponent:@"Camino"];
  [[NSFileManager defaultManager] removeFileAtPath:metadataPath handler:nil];
  [[NSFileManager defaultManager] createDirectoryAtPath:metadataPath attributes:nil];
  
  // save the path for later
  [mMetadataPath autorelease];
  mMetadataPath = [metadataPath retain];

  unsigned int itemCount = 0;
  NSEnumerator* bmEnumerator = [allBookmarkItems objectEnumerator];
  BookmarkItem* curItem;
  while ((curItem = [bmEnumerator nextObject]))
  {
    [curItem writeBookmarksMetadataToPath:mMetadataPath];

    if (!(++itemCount % 100) && ![NSThread inMainThread])
      usleep(10000);    // 10ms to give the UI some time
  }
}

#pragma mark -

//
// Reading/Importing bookmark files
//
- (BOOL)readBookmarks
{
  NSString *profileDir = [[PreferenceManager sharedInstance] newProfilePath];  

  //
  // figure out where Bookmarks.plist is and store it as mPathToBookmarkFile
  // if there is a Bookmarks.plist, read it
  // if there isn't a Bookmarks.plist, but there is a bookmarks.xml, read it.
  // if there isn't either, move default Bookmarks.plist to profile dir & read it.
  //
  NSFileManager *fM = [NSFileManager defaultManager];
  NSString *bookmarkPath = [profileDir stringByAppendingPathComponent:@"bookmarks.plist"];
  [self setPathToBookmarkFile:bookmarkPath];
  if ([fM isReadableFileAtPath:bookmarkPath])
  {
    if ([self readPListBookmarks:bookmarkPath])
      return YES;
  }
  else if ([fM isReadableFileAtPath:[profileDir stringByAppendingPathComponent:@"bookmarks.xml"]])
  {
    if ([self readCaminoXMLBookmarks:[profileDir stringByAppendingPathComponent:@"bookmarks.xml"]])
      return YES;
  }
  else
  {
    NSString *defaultBookmarks = [[NSBundle mainBundle] pathForResource:@"bookmarks" ofType:@"plist"];
    if ([fM copyPath:defaultBookmarks toPath:bookmarkPath handler:nil]) {
        if ([self readPListBookmarks:bookmarkPath])
          return YES;
    }
  }

  // maybe just look for safari bookmarks here??
  
  // if we're here, we've had a problem
  NSRunAlertPanel(NSLocalizedString(@"CorruptedBookmarksAlert", @""),
                  NSLocalizedString(@"CorruptedBookmarksMsg", @""),
                  NSLocalizedString(@"OKButtonText", @""),
                  nil,
                  nil);
  return NO;
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
  while ((curChild = [rootFoldersEnum nextObject]))
  {
    if ([curChild isKindOfClass:[BookmarkFolder class]])
    {
      BookmarkFolder* bmFolder = (BookmarkFolder*)curChild;
      if ([bmFolder isToolbar])
      {
        toolbarFolder = bmFolder; // remember that we've seen it
        [bmFolder setIdentifier:kBookmarksToolbarFolderIdentifier];
      }
      else if (!menuFolder)
      {
        menuFolder = bmFolder;
        [bmFolder setIdentifier:kBookmarksMenuFolderIdentifier];
      }
      
      if (toolbarFolder && menuFolder)
        break;
    }
  }

  if (!menuFolder)
  {
    menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
    [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", @"")];
    [[self rootBookmarks] insertChild:menuFolder atIndex:kBookmarkMenuContainerIndex isMove:NO];
  }

  if (!toolbarFolder)
  {
    toolbarFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksToolbarFolderIdentifier] autorelease];
    [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", @"")];
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
  while ((curChild = [rootFoldersEnum nextObject]))
  {
    if ([curChild isKindOfClass:[BookmarkFolder class]])
    {
      BookmarkFolder* bmFolder = (BookmarkFolder*)curChild;
      if ([[bmFolder title] isEqualToString:@"BookmarksBar"])
      {
        toolbarFolder = bmFolder; // remember that we've seen it
        [bmFolder setIsToolbar:YES];
        [bmFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", @"")];
        [bmFolder setIdentifier:kBookmarksToolbarFolderIdentifier];
      }
      else if ([[bmFolder title] isEqualToString:@"BookmarksMenu"])
      {
        menuFolder = bmFolder;
        [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", @"")];
        [bmFolder setIdentifier:kBookmarksMenuFolderIdentifier];
      }
      
      if (toolbarFolder && menuFolder)
        break;
    }
  }

  if (!menuFolder)
  {
    menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
    [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", @"")];
    [[self rootBookmarks] insertChild:menuFolder atIndex:kBookmarkMenuContainerIndex isMove:NO];
  }

  if (!toolbarFolder)
  {
    toolbarFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksToolbarFolderIdentifier] autorelease];
    [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", @"")];
    [toolbarFolder setIsToolbar:YES];
    [[self rootBookmarks] insertChild:toolbarFolder atIndex:kToolbarContainerIndex isMove:NO];
  }

  return YES;
}

- (BOOL)readCaminoXMLBookmarks:(NSString *)pathToFile
{
  BookmarkFolder* menuFolder = [[[BookmarkFolder alloc] initWithIdentifier:kBookmarksMenuFolderIdentifier] autorelease];
  [menuFolder setTitle:NSLocalizedString(@"Bookmark Menu", @"")];
  [[self rootBookmarks] appendChild:menuFolder];

  BOOL readOK = [self importCaminoXMLFile:pathToFile intoFolder:menuFolder settingToolbarFolder:YES];

  // even if we didn't do a full read, try to fix up the toolbar folder
  BookmarkFolder* toolbarFolder = nil;
  NSEnumerator* bookmarksEnum = [[self rootBookmarks] objectEnumerator];
  BookmarkItem* curItem;
  while ((curItem = [bookmarksEnum nextObject]))
  {
    if ([curItem isKindOfClass:[BookmarkFolder class]])
    {
      BookmarkFolder* curFolder = (BookmarkFolder*)curItem;
      if ([curFolder isToolbar])
      {
        toolbarFolder = curFolder;
        break;
      }
    }
  }

  if (toolbarFolder)
  {
    [[toolbarFolder parent] moveChild:toolbarFolder toBookmarkFolder:[self rootBookmarks] atIndex:kToolbarContainerIndex];
    [toolbarFolder setTitle:NSLocalizedString(@"Bookmark Toolbar", @"")];
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

- (BOOL)importBookmarks:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder
{
  // I feel dirty doing it this way.  But we'll check file extension
  // to figure out how to handle this.
  NSUndoManager *undoManager = [self undoManager];
  [undoManager beginUndoGrouping];
  BOOL success = NO;
  NSString *extension =[pathToFile pathExtension];
  if ([extension isEqualToString:@""]) // we'll go out on a limb here
    success = [self readOperaFile:pathToFile intoFolder:aFolder];
  else if ([extension isEqualToString:@"html"] || [extension isEqualToString:@"htm"])
    success = [self importHTMLFile:pathToFile intoFolder:aFolder];
  else if ([extension isEqualToString:@"xml"])
    success = [self importCaminoXMLFile:pathToFile intoFolder:aFolder settingToolbarFolder:NO];
  else if ([extension isEqualToString:@"plist"] || !success)
    success = [self importPropertyListFile:pathToFile intoFolder:aFolder];
  // we don't know the extension, or we failed to load.  we'll take another
  // crack at it trying everything we know.
  if (!success) {
    success = [self readOperaFile:pathToFile intoFolder:aFolder];
    if (!success) {
      success = [self importHTMLFile:pathToFile intoFolder:aFolder];
      if (!success) {
        success = [self importCaminoXMLFile:pathToFile intoFolder:aFolder settingToolbarFolder:NO];
      }
    }
  }
  [[undoManager prepareWithInvocationTarget:[self rootBookmarks]] deleteChild:aFolder];
  [undoManager endUndoGrouping];
  [undoManager setActionName:NSLocalizedString(@"Import Bookmarks", @"")];
  return success;
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
    [NSNumber numberWithUnsignedInt:NSUTF8StringEncoding],utfdash8Key,
    [NSNumber numberWithUnsignedInt:NSMacOSRomanStringEncoding],xmacromanKey,
    [NSNumber numberWithUnsignedInt:NSShiftJISStringEncoding],shiftJisKey,
    [NSNumber numberWithUnsignedInt:[NSString defaultCStringEncoding]],xmacsystemKey,
    [NSNumber numberWithUnsignedInt:NSUTF8StringEncoding],operaUTF8Key,
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


-(BOOL)importHTMLFile:(NSString *)pathToFile intoFolder:(BookmarkFolder *)aFolder
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
  } else {
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
    if ((scanIndex+3) < [fileAsString length]) {
      tokenTag = [[NSString alloc] initWithString:[[fileAsString substringWithRange:NSMakeRange(scanIndex,3)] uppercaseString]];
      // now we pick out if it's something we want to save.
      // check in a "most likely thing first" order
      if ([tokenTag isEqualToString:@"<DT "]) {
        [fileScanner setScanLocation:(scanIndex+1)];
      }
      else if ([tokenTag isEqualToString:@"<P>"]) {
        [fileScanner setScanLocation:(scanIndex+1)];
      }
      else if ([tokenTag isEqualToString:@"<A "]) {
        // adding a new bookmark to end of currentArray.
        [fileScanner scanUpToString:@"</A>" intoString:&tokenString];
        tokenScanner = [[NSScanner alloc] initWithString:tokenString];
        [tokenScanner scanUpToString:@"href=\"" intoString:NULL];
        // might be a menu spacer.  check to make sure.
        if (![tokenScanner isAtEnd]) {
          [tokenScanner setScanLocation:([tokenScanner scanLocation]+6)];
          [tokenScanner scanUpToString:@"\"" intoString:&tempItem];
          if ([tokenScanner isAtEnd]) {
            // we scanned up to the </A> but didn't find a " character ending the HREF. This is probably
            // because we're scanning a bookmarklet that contains an embedded <A></A> so we're in the
            // middle of the string. Just bail and dont import this bookmark. The parser should be able
            // to recover on its own once it gets to the next "<A" token.
            [tokenScanner release];
            [tokenTag release];
            [fileScanner setScanLocation:([fileScanner scanLocation]+1)];
            continue;
          }
          currentItem = [currentArray addBookmark];
          [(Bookmark *)currentItem setUrl:[tempItem stringByRemovingAmpEscapes]];
          [tokenScanner scanUpToString:@">" intoString:&tempItem];
          if (![tokenScanner isAtEnd]) {     // protect against malformed files
            [currentItem setTitle:[[tokenString substringFromIndex:([tokenScanner scanLocation]+1)] stringByRemovingAmpEscapes]];
            justSetTitle = YES;
          }
          // see if we had a keyword
          if (isNetscape) {
            tempRange = [tempItem rangeOfString:@"SHORTCUTURL=\"" options: NSCaseInsensitiveSearch];
            if (tempRange.location != NSNotFound) {
              // throw everything to next " into keyword. A malformed bookmark might not have a closing " which
              // will throw things out of whack slightly, but it's better than crashing.
              keyRange = [tempItem rangeOfString:@"\"" options:0 range:NSMakeRange(tempRange.location+tempRange.length,[tempItem length]-(tempRange.location+tempRange.length))];
              if (keyRange.location != NSNotFound)
                [currentItem setKeyword:[tempItem substringWithRange:NSMakeRange(tempRange.location+tempRange.length,keyRange.location - (tempRange.location+tempRange.length))]];
            }
          }
        }
        [tokenScanner release];
        [fileScanner setScanLocation:([fileScanner scanLocation]+1)];
      }
      else if ([tokenTag isEqualToString:@"<DD"]) {
        // add a description to current item
        [fileScanner scanUpToString:@">" intoString:NULL];
        [fileScanner setScanLocation:([fileScanner scanLocation]+1)];
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
          [currentItem setTitle:[[tokenString substringFromIndex:([tokenScanner scanLocation]+1)] stringByRemovingAmpEscapes]];
          // check for group
          tempRange = [tempItem rangeOfString:@"FOLDER_GROUP=\"true\"" options: NSCaseInsensitiveSearch];
          if (tempRange.location != NSNotFound)
            [(BookmarkFolder *)currentItem setIsGroup:YES];          
        } else {
            // have to do this in chunks to handle omniweb 5
          [tokenScanner scanUpToString:@"<a" intoString:NULL];
          [tokenScanner scanUpToString:@">" intoString:NULL];
          [tokenScanner setScanLocation:([tokenScanner scanLocation]+1)];
          [tokenScanner scanUpToString:@"</a>" intoString:&tempItem];
          [currentItem setTitle:[tempItem stringByRemovingAmpEscapes]];
        }
        [tokenScanner release];
        [fileScanner setScanLocation:([fileScanner scanLocation]+1)];
      }
      else if ([tokenTag isEqualToString:@"<DL"]) {
        [fileScanner setScanLocation:(scanIndex+1)];
      }
      else if ([tokenTag isEqualToString:@"</D"]) {
        // note that we only scan for the first two characters of a tag
        // that is why this tag is "</D" and not "</DL"
        currentArray = (BookmarkFolder *)[currentArray parent];
        [fileScanner setScanLocation:(scanIndex+1)];
      }
      else if ([tokenTag isEqualToString:@"<H1"]) {
        [fileScanner scanUpToString:@">" intoString:NULL];
        [fileScanner scanUpToString:@"</H1>" intoString:NULL];
        [fileScanner setScanLocation:([fileScanner scanLocation]+1)];
      }
      else if ([tokenTag isEqualToString:@"<BO"]) {
        //omniweb bookmark marker, no doubt
        [fileScanner scanUpToString:@">" intoString:NULL];
        [fileScanner setScanLocation:([fileScanner scanLocation]+1)];
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
        [fileScanner setScanLocation:(scanIndex+1)];
      }
      else if ([tokenTag isEqualToString:@"</H"]) {
        // if it's not html, we'll include in previous text string
        tempItem = [[NSString alloc] initWithString:[fileAsString substringWithRange:NSMakeRange(scanIndex,1)]];
        if (([tempItem isEqualToString:@"</HT"]) || ([tempItem isEqualToString:@"</ht"]))
          [fileScanner scanUpToString:@">" intoString:NULL];
        else {
          [tempItem release];
          tempItem = [[NSString alloc] initWithString:[@"<" stringByAppendingString:[tokenString stringByRemovingAmpEscapes]]];
          if (justSetTitle)
            [currentItem setTitle:[[currentItem title] stringByAppendingString:tempItem]];
          else
            [currentItem setItemDescription:[[currentItem itemDescription] stringByAppendingString:tempItem]];
          [fileScanner setScanLocation:([fileScanner scanLocation]+1)];
        }
        [tempItem release];
      }
      // Firefox menu separator
      else if ([tokenTag isEqualToString:@"<HR"]) {
          currentItem = [currentArray addBookmark];
          [(Bookmark *)currentItem setIsSeparator:YES];
          [fileScanner setScanLocation:(scanIndex+1)];          
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
  for (int index = 0;index < count; index++)
  {
    CFXMLTreeRef subFileTree = CFTreeGetChildAtIndex(xmlFileTree,index);
    if (subFileTree)
    {
      CFXMLNodeRef bookmarkNode = CFXMLTreeGetNode(subFileTree);
      if (bookmarkNode)
      {
        switch (CFXMLNodeGetTypeCode(bookmarkNode))
        {
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

-(BOOL)importPropertyListFile:(NSString *)pathToFile intoFolder:aFolder
{
  NSDictionary* dict = [NSDictionary dictionaryWithContentsOfFile:pathToFile];
  // see if it's safari
  if ([dict objectForKey:@"WebBookmarkType"])
    return [aFolder readSafariDictionary:dict];

  return [aFolder readNativeDictionary:dict];
}

-(BOOL)readOperaFile:(NSString *)pathToFile intoFolder:aFolder
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

- (void) writeHTMLFile:(NSString *)pathToFile
{
  NSData *htmlData = [[[self rootBookmarks] writeHTML:0] dataUsingEncoding:NSUTF8StringEncoding];
  if (![htmlData writeToFile:[pathToFile stringByStandardizingPath] atomically:YES])
    NSLog(@"writeHTML: Failed to write file %@", pathToFile);
}

-(void) writeSafariFile:(NSString *)pathToFile
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
-(void)writePropertyListFile:(NSString *)pathToFile
{
  BookmarkFolder* rootBookmarks = [self rootBookmarks];
  if (!rootBookmarks)
    return;   // we never read anything

  NSDictionary* dict = [rootBookmarks writeNativeDictionary];
  NSString* stdPath = [pathToFile stringByStandardizingPath];
  NSString* backupFile = [NSString stringWithFormat:@"%@.new", stdPath];
  BOOL success = [dict writeToFile:backupFile atomically:YES];
  if (success) {
    NSFileManager* fm = [NSFileManager defaultManager];
    [fm removeFileAtPath:stdPath handler:nil];               // out with the old...
    [fm movePath:backupFile toPath:stdPath handler:nil];     //  ... in with the new
  }
  else
    NSLog(@"writePropertyList: Failed to write file %@", pathToFile);
}

@end
