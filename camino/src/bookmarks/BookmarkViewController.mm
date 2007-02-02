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
 *   Simon Fraser <smfr@smfr.org>
 *   Max Horn <max@quendi.de>
 *   David Haas <haasd@cae.wisc.edu>
 *   Simon Woodside <sbwoodside@yahoo.com>
 *   Josh Aas <josh@mozilla.com>
 *   Bruce Davidson <Bruce.Davidson@ipl.com>
 *   Håkan Waara <hwaara@gmail.com>
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

#import "NSArray+Utils.h"
#import "NSString+Utils.h"
#import "NSPasteboard+Utils.h"
#import "NSSplitView+Utils.h"
#import "NSView+Utils.h"
#import "NSMenu+Utils.h"

#import "BookmarkManager.h"
#import "BookmarkInfoController.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "AddBookmarkDialogController.h"

#import "MainController.h"

#import "BrowserWindowController.h"
#import "BrowserTabView.h"
#import "PreferenceManager.h"
#import "ImageAndTextCell.h"
#import "ExtendedTableView.h"
#import "ExtendedOutlineView.h"
#import "BookmarkOutlineView.h"
#import "PopupMenuButton.h"

#import "HistoryOutlineViewDelegate.h"
#import "HistoryDataSource.h"
#import "HistoryItem.h"

#import "BookmarksClient.h"
#import "NetworkServices.h"
#import "UserDefaults.h"


enum {
  eNoOpenAction = 0,
  eOpenBookmarkAction = 1,
  eOpenInNewTabAction = 2,
  eOpenInNewWindowAction = 3
};

const int kSearchAllTag = 1;

static NSString* const kExpandedBookmarksStatesDefaultsKey = @"bookmarks_expand_state";
static NSString* const kBookmarksSelectedContainerDefaultsKey = @"bookmarks_selected_container";
static NSString* const kBookmarksSelectedContainerIdentifierKey = @"identifier";
static NSString* const kBookmarksSelectedContainerUUIDKey       = @"uuid";

const long kMinContainerSplitWidth = 150;
const int kMinSeparatorWidth = 16;
const int kOutlineViewLeftMargin = 19; // determined empirically, since it doesn't seem to be in the API

#pragma mark -

@interface BookmarkViewController (Private) <BookmarksClient, NetworkServicesClient>

- (void)ensureNibLoaded;
- (void)completeSetup;
- (void)setupAppearanceOfTableView:(NSTableView*)tableView;

- (void)restoreSplitters;

- (void)reloadDataForItem:(id)item reloadChildren:(BOOL)aReloadChildren;

- (void)setSearchResultArray:(NSArray *)anArray;
- (void)displayBookmarkInOutlineView:(BookmarkItem *)aBookmarkItem;
- (BOOL)doDrop:(id <NSDraggingInfo>)info intoFolder:(BookmarkFolder *)dropFolder index:(int)index;

- (void)setSearchFilterTag:(int)tag;
- (void)searchFor:(NSString*)searchString inFieldWithTag:(int)tag;
- (void)clearSearchResults;

- (void)selectContainerFolder:(BookmarkFolder*)inFolder;
- (BookmarkFolder*)selectedContainerFolder;

- (void)selectItems:(NSArray*)items expandingContainers:(BOOL)expandContainers scrollIntoView:(BOOL)scroll;
- (BookmarkItem*)selectedBookmarkItem;

- (SEL)sortSelectorFromItemTag:(int)inTag;

- (id)itemTreeRootContainer;   // something that responds to NSArray-like selectors

- (NSOutlineView*)activeOutlineView;    // return the outline view of the visible tab
- (void)setActiveOutlineView:(NSOutlineView*)outlineView;

- (NSMutableDictionary *)expandedStateDictionary;
- (void)restoreFolderExpandedStates;
- (void)saveExpandedStateDictionary;

- (BOOL)hasExpandedState:(id)anItem;
- (void)setStateOfItem:(BookmarkFolder *)anItem toExpanded:(BOOL)aBool;

- (void)expandAllParentsOfItem:(BookmarkItem*)inItem;

- (void)actionButtonWillDisplay:(NSNotification *)notification;

- (NSDragOperation)preferredDragOperationForInfo:(id <NSDraggingInfo>)info;

- (void)pasteBookmarks:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder *)dropFolder index:(int)index copying:(BOOL)isCopy;
- (void)pasteBookmarksFromURLsAndTitles:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder*)dropFolder index:(int)index;

@end



#pragma mark -

@implementation BookmarkViewController


+ (NSAttributedString*)greyStringWithItemCount:(int)itemCount
{
  NSString* itemCountStr = [NSString stringWithFormat:NSLocalizedString(@"Contains Items", @"%u Items"), itemCount];
  NSDictionary* colorAttributes = [NSDictionary dictionaryWithObject:[NSColor disabledControlTextColor] forKey:NSForegroundColorAttributeName];
  return [[[NSAttributedString alloc] initWithString:itemCountStr attributes:colorAttributes] autorelease];
}

- (id)initWithBrowserWindowController:(BrowserWindowController*)bwController
{
  if ((self = [super init])) {
    mBrowserWindowController = bwController;  // not retained

    // wait for |-completeSetup| to be called to lazily complete our setup
    mSetupComplete = NO;

    // we'll delay loading the nib until we need to show UI
    // (important because we get created for every new tab)
  }
  return self;
}

- (void)dealloc
{
  // we know this is still alive, because we release the last ref below
  [mBookmarksEditingView setDelegate:nil];

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // balance the extra retains
  [mBookmarksHostView release];
  [mHistoryHostView release];

  // release the views
  // Note: we have to be careful only to release top-level items in the nib,
  // not any random subview we might have an outlet to.
  [mBookmarksEditingView release];
  [mBookmarksHostView release];
  [mHistoryHostView release];

  [mActionMenuBookmarks release];
  [mActionMenuHistory release];

  [mSortMenuBookmarks release];
  [mSortMenuHistory release];

  [mQuickSearchMenuBookmarks release];
  [mQuickSearchMenuHistory release];

  [mHistoryOutlineViewDelegate release];

  // release data
  [mItemToReveal release];
  [mExpandedStates release];
  [mActiveRootCollection release];
  [mRootBookmarks release];
  [mSearchResultArray release];

  [mHistoryDataSource release];

  [mSeparatorImage release];

  [super dealloc];
}

// called when our nib has loaded
- (void)awakeFromNib
{
  [mBookmarksEditingView setDelegate:self];

  // retain views that we remove from the hierarchy
  [mBookmarksHostView retain];
  [mHistoryHostView retain];

  [self completeSetup];
}

//
// - managerStarted:
//
// Notification callback from the bookmark manager. Reload all the table data, but
// only if we think we've fully initialized ourselves.
//
- (void)managerStarted:(NSNotification*)inNotify
{
  if (mSetupComplete)
    [self ensureBookmarks];
}

- (void)ensureNibLoaded
{
  if (!mBookmarksEditingView)
    [NSBundle loadNibNamed:@"BookmarksEditing" owner:self];
}

- (void)completeSetup
{
  // set up the table appearance for item and search views
  [self setupAppearanceOfTableView:mContainersTableView];
  [self setupAppearanceOfTableView:mBookmarksOutlineView];
  [self setupAppearanceOfTableView:mHistoryOutlineView];

  [mBookmarksOutlineView setAutoresizesOutlineColumn:NO];
  [mHistoryOutlineView setAutoresizesOutlineColumn:NO];

  // set up history outliner
  mHistoryDataSource = [[HistoryDataSource alloc] init];
  [mHistoryOutlineView setDataSource:mHistoryDataSource];
  [mHistoryOutlineViewDelegate setBrowserWindowController:mBrowserWindowController];
  [mHistoryOutlineView setTarget:mHistoryOutlineViewDelegate];
  [mHistoryOutlineView setDoubleAction:@selector(openHistoryItem:)];
  [mHistoryOutlineView setDeleteAction:@selector(deleteHistoryItems:)];

  // Generic notifications for Bookmark Client
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self selector:@selector(bookmarkAdded:)   name:BookmarkFolderAdditionNotification object:nil];
  [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
  [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:nil];
  [nc addObserver:self selector:@selector(serviceResolved:) name:NetworkServicesResolutionSuccess object:nil];

  // get notified when the action button pops up, to set its menu
  [nc addObserver:self selector:@selector(actionButtonWillDisplay:) name:PopupMenuButtonWillDisplayMenu object:mActionButton];

  // register for notifications of when the BM manager starts up. Since it does it on a separate thread,
  // it can be created after we are and if we don't update ourselves, the bar will be blank. This
  // happens most notably when the app is launched with a 'odoc' or 'GURL' appleEvent.
  [nc addObserver:self selector:@selector(managerStarted:) name:kBookmarkManagerStartedNotification object:nil];

  // register for dragged types
  [mContainersTableView registerForDraggedTypes:[NSArray arrayWithObjects:kCaminoBookmarkListPBoardType,
                                                                          kWebURLsWithTitlesPboardType,
                                                                          NSURLPboardType,
                                                                          NSStringPboardType,
                                                                          nil]];

  [self ensureBookmarks];

  // set a formatter on the keyword column
  BookmarkKeywordFormatter* keywordFormatter = [[[BookmarkKeywordFormatter alloc] init] autorelease];
  [[[mBookmarksOutlineView tableColumnWithIdentifier:@"keyword"] dataCell] setFormatter:keywordFormatter];

  // these should be settable in the nib.  however, whenever
  // I try, they disappear as soon as I've saved.  Very annoying.
  [mContainersTableView setAutosaveName:@"BMContainerView"];
  [mContainersTableView setAutosaveTableColumns:YES];

  [mBookmarksOutlineView setAutosaveName:@"BookmarksOutlineView"];
  [mBookmarksOutlineView setAutosaveTableColumns:YES];

  [mHistoryOutlineView setAutosaveName:@"HistoryOutlineView"];
  [mHistoryOutlineView setAutosaveTableColumns:YES];
  [mHistoryOutlineView setAutosaveTableSort:YES];

  mSeparatorImage = [[NSImage imageNamed:@"bm_horizontal_separator"] retain];
  [mSeparatorImage setScalesWhenResized:YES];

  mSetupComplete = YES;
}

- (void)setupAppearanceOfTableView:(NSTableView*)tableView
{
  // the standard table item doesn't handle text and icons. Replace it
  // with a custom cell that does.
  ImageAndTextCell* imageAndTextCell = [[[ImageAndTextCell alloc] init] autorelease];
  [imageAndTextCell setEditable:YES];
  [imageAndTextCell setWraps:NO];

  NSTableColumn* itemNameColumn = [tableView tableColumnWithIdentifier:@"title"];
  [itemNameColumn setDataCell:imageAndTextCell];

  [tableView setUsesAlternatingRowBackgroundColors:YES];
  [tableView setGridStyleMask:NSTableViewSolidVerticalGridLineMask];

  // set up the font on the item & search views to be smaller
  // also don't let the cells draw their backgrounds
  NSArray* columns = [tableView tableColumns];
  if (columns) {
    int numColumns = [columns count];
    NSFont* smallerFont = [NSFont systemFontOfSize:11];
    for (int i = 0; i < numColumns; i++) {
      [[[columns objectAtIndex:i] dataCell] setFont:smallerFont];
      [[[columns objectAtIndex:i] dataCell] setDrawsBackground:NO];
    }
  }
}

//
// ensureBookmarks
//
// Setup the connections for the bookmark manager and tell the tables to reload their
// data. This routine may be called more than once safely. Note that if the bookmark manager
// has not yet been fully initialized by the time we get here, bail until we hear back later.
//
- (void)ensureBookmarks
{
  if (!mRootBookmarks) {
    BookmarkFolder* manager = [[BookmarkManager sharedBookmarkManager] rootBookmarks];
    if (![manager count])     // not initialized yet, try again later (from start notifiation)
      return;

    mRootBookmarks = [manager retain];

    [mContainersTableView setTarget:self];
    [mContainersTableView setDeleteAction:@selector(deleteCollection:)];
    [mContainersTableView reloadData];

    [mBookmarksOutlineView setTarget:self];
    [mBookmarksOutlineView setDoubleAction:@selector(openBookmark:)];
    [mBookmarksOutlineView setDeleteAction:@selector(deleteBookmarks:)];
    [mBookmarksOutlineView reloadData];
  }
}

- (void)setSearchResultArray:(NSArray *)anArray
{
  [anArray retain];
  [mSearchResultArray release];
  mSearchResultArray = anArray;
}

//
// IBActions
//

- (IBAction)toggleIsDockMenuFolder:(id)aSender
{
  BookmarkFolder* aFolder = [self selectedContainerFolder];
  [aFolder toggleIsDockMenu:aSender];
}

- (IBAction)addCollection:(id)aSender
{
  BookmarkFolder *aFolder = [mRootBookmarks addBookmarkFolder];
  [aFolder setTitle:NSLocalizedString(@"NewBookmarkFolder", nil)];
  [self selectContainerFolder:aFolder];
  int newFolderIndex = [[BookmarkManager sharedBookmarkManager] indexOfContainer:aFolder];
  [mContainersTableView editColumn:0 row:newFolderIndex withEvent:nil select:YES];
}

- (IBAction)addBookmarkSeparator:(id)aSender
{
  Bookmark *aBookmark = [[Bookmark alloc] init];
  [aBookmark setIsSeparator:YES];

  int index;
  BookmarkFolder *parentFolder = [self selectedItemFolderAndIndex:&index];

  [parentFolder insertChild:aBookmark atIndex:index isMove:NO];

  [self revealItem:aBookmark scrollIntoView:YES selecting:YES byExtendingSelection:NO];
  [aBookmark release];
}

- (IBAction)addBookmarkFolder:(id)aSender
{
  AddBookmarkDialogController* addBookmarkController = [AddBookmarkDialogController sharedAddBookmarkDialogController];

  int itemIndex;
  BookmarkFolder* parentFolder = [self selectedItemFolderAndIndex:&itemIndex];
  [addBookmarkController setDefaultParentFolder:parentFolder andIndex:itemIndex];
  [addBookmarkController setBookmarkViewController:self];

  [addBookmarkController showDialogWithLocationsAndTitles:nil isFolder:YES onWindow:[mBookmarksEditingView window]];
}

- (IBAction)deleteCollection:(id)aSender
{
  BookmarkManager* manager = [BookmarkManager sharedBookmarkManager];
  int index = [mContainersTableView selectedRow];

  BookmarkFolder* selectedContainer = [self selectedContainerFolder];
  if (![manager isUserCollection:selectedContainer])
    return;

  [self selectContainerFolder:[manager containerAtIndex:index - 1]];
  [[manager rootBookmarks] deleteChild:selectedContainer];
}

- (IBAction)deleteBookmarks:(id)aSender
{
  int index = [mBookmarksOutlineView selectedRow];
  if (index == -1)
    return;

  // A cheap way of having to avoid scanning the list to remove children is to have the
  // outliner collapse all items that are being deleted. This will cull the selection
  // for us and eliminate any children that happened to be selected.
  BOOL allCollapsed = NO;
  id doomedItem;
  NSEnumerator* selRows;
  while (!allCollapsed) {
    allCollapsed = YES;
    selRows = [mBookmarksOutlineView selectedRowEnumerator];
    while (allCollapsed && (doomedItem = [selRows nextObject])) {
      doomedItem = [mBookmarksOutlineView itemAtRow:[doomedItem intValue]];
      if ([mBookmarksOutlineView isItemExpanded:doomedItem]) {
        allCollapsed = NO;
        [mBookmarksOutlineView collapseItem:doomedItem];
      }
    }
  }

  [[BookmarkManager sharedBookmarkManager] startSuppressingChangeNotifications];

  // all the parents of the children we need to notify, some may overlap, but in general
  // that's pretty uncommon, so this is good enough.
  NSMutableSet *parentsToNotify = [NSMutableSet set];

  // delete all bookmarks that are in our array
  NSEnumerator *e = [[mBookmarksOutlineView selectedItems] objectEnumerator];
  BookmarkItem *doomedBookmark = nil;

  while ((doomedBookmark = [e nextObject])) {
    BookmarkFolder *currentParent = [doomedBookmark parent];
    [parentsToNotify addObject:currentParent];
    [currentParent deleteChild:doomedBookmark];
  }

  [[BookmarkManager sharedBookmarkManager] stopSuppressingChangeNotifications];

  // notify observers that the parents have changed
  e = [parentsToNotify objectEnumerator];
  BookmarkFolder *currentParent = nil;
  while ((currentParent = [e nextObject]))
    [currentParent notifyChildrenChanged];

  // restore selection to location near last item deleted or last item
  int total = [mBookmarksOutlineView numberOfRows];
  if (index >= total)
    index = total - 1;
  [mBookmarksOutlineView selectRow:index byExtendingSelection:NO];
}

- (IBAction)openBookmark:(id)aSender
{
  NSArray* items = nil;
  if ([aSender isKindOfClass:[BookmarkItem class]])
    items = [NSArray arrayWithObject:aSender];
  else
    items = [mBookmarksOutlineView selectedItems];

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject])) {
    // see if it's a rendezvous item
    if ([curItem isKindOfClass:[RendezvousBookmark class]] && ![curItem resolved]) {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[(RendezvousBookmark*)curItem serviceID] forSender:curItem];
      mOpenActionFlag = eOpenBookmarkAction;
    }
    else if ([curItem isKindOfClass:[BookmarkFolder class]] && ![curItem isGroup]) {
      if ([mBookmarksOutlineView isItemExpanded:curItem])
        [mBookmarksOutlineView collapseItem:curItem];
      else
        [mBookmarksOutlineView expandItem:curItem];
    }
    else if (![curItem isSeparator]) {
      // otherwise follow the standard bookmark opening behavior
      BOOL shiftKeyDown = ([[NSApp currentEvent] modifierFlags] & NSShiftKeyMask) != 0;
      EBookmarkOpenBehavior behavior = eBookmarkOpenBehavior_Preferred;
      // if command is down, open in new tab/window
      if (([[NSApp currentEvent] modifierFlags] & NSCommandKeyMask) != 0)
        behavior = eBookmarkOpenBehavior_NewPreferred;

      [[NSApp delegate] loadBookmark:curItem withBWC:mBrowserWindowController openBehavior:behavior reverseBgToggle:shiftKeyDown];
    }
  }
}

- (IBAction)openBookmarkInNewTab:(id)aSender
{
  NSArray* items = nil;
  if ([aSender isKindOfClass:[BookmarkItem class]])
    items = [NSArray arrayWithObject:aSender];
  else
    items = [mBookmarksOutlineView selectedItems];

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject])) {
    // see if it's a rendezvous item
    if ([curItem isKindOfClass:[RendezvousBookmark class]] && ![curItem resolved]) {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[(RendezvousBookmark*)curItem serviceID] forSender:curItem];
      mOpenActionFlag = eOpenInNewTabAction;
    }
    else if (![curItem isSeparator]) {
      // otherwise follow the standard bookmark opening behavior
      BOOL reverseBackgroundPref = NO;
      if ([aSender isAlternate])
        reverseBackgroundPref = ([aSender keyEquivalentModifierMask] & NSShiftKeyMask) != 0;

      [[NSApp delegate] loadBookmark:curItem
                             withBWC:mBrowserWindowController
                        openBehavior:eBookmarkOpenBehavior_NewTab
                     reverseBgToggle:reverseBackgroundPref];
    }
  }
}

- (IBAction)openBookmarksInTabsInNewWindow:(id)aSender
{
  NSArray* items = nil;
  if ([aSender isKindOfClass:[BookmarkItem class]])
    items = [NSArray arrayWithObject:aSender];
  else
    items = [mBookmarksOutlineView selectedItems];

  // make url array
  NSMutableArray* urlArray = [NSMutableArray arrayWithCapacity:[items count]];

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject])) {
    // see if it's a rendezvous item (this won't open in the new window, because we suck)
    if ([curItem isKindOfClass:[RendezvousBookmark class]] && ![curItem resolved]) {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[(RendezvousBookmark*)curItem serviceID] forSender:curItem];
      mOpenActionFlag = eOpenInNewTabAction;
    }
    else {
      if ([curItem isKindOfClass:[Bookmark class]] && ![curItem isSeparator])
        [urlArray addObject:[curItem url]];
      else if ([curItem isKindOfClass:[BookmarkFolder class]])
        [urlArray addObjectsFromArray:[curItem childURLs]];
    }
  }

  // make new window
  BOOL loadNewTabsInBackgroundPref = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];

  NSWindow* behindWindow = nil;
  if (loadNewTabsInBackgroundPref)
    behindWindow = [mBrowserWindowController window];

  [[NSApp delegate] openBrowserWindowWithURLs:urlArray behind:behindWindow allowPopups:NO];
}

- (IBAction)openBookmarkInNewWindow:(id)aSender
{
  NSArray* items = nil;
  if ([aSender isKindOfClass:[BookmarkItem class]])
    items = [NSArray arrayWithObject:aSender];
  else
    items = [mBookmarksOutlineView selectedItems];

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject])) {
    // see if it's a rendezvous item
    if ([curItem isKindOfClass:[RendezvousBookmark class]] && ![curItem resolved]) {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[(RendezvousBookmark*)curItem serviceID] forSender:curItem];
      mOpenActionFlag = eOpenInNewWindowAction;
    }
    else if (![curItem isSeparator]) {
      // otherwise follow the standard bookmark opening behavior
      BOOL reverseBackgroundPref = NO;
      if ([aSender isAlternate])
        reverseBackgroundPref = ([aSender keyEquivalentModifierMask] & NSShiftKeyMask) != 0;

      [[NSApp delegate] loadBookmark:curItem
                             withBWC:mBrowserWindowController
                        openBehavior:eBookmarkOpenBehavior_NewWindow
                     reverseBgToggle:reverseBackgroundPref];
    }
  }
}

- (IBAction)showBookmarkInfo:(id)aSender
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController];
  BookmarkItem* item = [self selectedBookmarkItem];

  [bic setBookmark:item];
  [bic showWindow:bic];
}

- (IBAction)revealBookmark:(id)aSender
{
  [self revealItem:[self selectedBookmarkItem] scrollIntoView:YES selecting:YES byExtendingSelection:NO];
}

- (IBAction)cut:(id)aSender
{
  // XXX write me. We'll need to write to the pasteboard something other than an array of UUIDs,
  // because we need to rip the bookmark items out of the tree.

}

- (IBAction)copy:(id)aSender
{
  [self copyBookmarks:[mBookmarksOutlineView selectedItems] toPasteboard:[NSPasteboard generalPasteboard]];
}


//
// Paste bookmark(s) from the general pasteboard into the user's bookmarks file
// We use the view to work out where to paste the bookmark
// If no items are selected in the view: at the end of the bookmark menu folder
// If a folder is selected: at the end of that folder
// If a bookmark is selected: immediately after that bookmark, under the same parent
// XXX: At the moment if multiple items are selected we only examine the first one
//
- (IBAction)paste:(id)aSender
{
  NSArray* types = [[NSPasteboard generalPasteboard] types];

  int pasteDestinationIndex = 0;
  BookmarkFolder* pasteDestinationFolder = nil;

  // Work out what the selected item is and therefore where to paste the bookmark(s)
  NSEnumerator* selRows = [mBookmarksOutlineView selectedRowEnumerator];
  id curSelectedRow = [selRows nextObject];

  if (curSelectedRow) {
    BookmarkItem* item = [mBookmarksOutlineView itemAtRow:[curSelectedRow intValue]];
    if ([item isKindOfClass:[BookmarkFolder class]]) {
      pasteDestinationFolder = (BookmarkFolder*) item;
      pasteDestinationIndex = [pasteDestinationFolder count];
    }
    else if ([item isKindOfClass:[Bookmark class]]) {
      pasteDestinationFolder = (BookmarkFolder*) [item parent];
      pasteDestinationIndex = [pasteDestinationFolder indexOfObject:item] + 1;
    }
  }

  // If we don't have a destination use the end of the current collection, if possible
  if (!pasteDestinationFolder) {
    BookmarkFolder* destFolder = [self activeCollection];
    if ([destFolder isSmartFolder])
      destFolder = [[BookmarkManager sharedBookmarkManager] bookmarkMenuFolder];

    pasteDestinationFolder = destFolder;
    pasteDestinationIndex = [pasteDestinationFolder count];
  }

  // Do the actual copy based on the type available on the clipboard
  if ([types containsObject:kCaminoBookmarkListPBoardType])
    [self pasteBookmarks:[NSPasteboard generalPasteboard] intoFolder:pasteDestinationFolder index:pasteDestinationIndex copying:YES];
  else if ([[NSPasteboard generalPasteboard] containsURLData])
    [self pasteBookmarksFromURLsAndTitles:[NSPasteboard generalPasteboard] intoFolder:pasteDestinationFolder index:pasteDestinationIndex];
}

- (IBAction)delete:(id)aSender
{
  [self deleteBookmarks:aSender];
}

//
// the logic of what to sort here is somewhat subtle.
//
// If a single folder is selected, we sort its children.
// If > 1 items are selected, we just re-order them.
// If the option key is down, we sort deep
//
- (IBAction)arrange:(id)aSender
{
  BookmarkFolder* activeCollection = [self activeCollection];
  if ([activeCollection isRoot] || [activeCollection isSmartFolder])
    return;

  int tag = [aSender tag];

  BOOL reverseSort  = ((tag & kArrangeBookmarksDescendingMask) != 0);
  SEL  sortSelector = [self sortSelectorFromItemTag:tag];
  if (!sortSelector)
    return;     // all UI items that call this should have the appropriate tags set

  // sort deep if the option key is down
  BOOL sortDeep = (([[NSApp currentEvent] modifierFlags] & NSAlternateKeyMask) != 0);

  NSArray* bmItems = [mBookmarksOutlineView selectedItems];
  if ([bmItems count] == 0) {
    // if nothing is selected, sort the whole container
    bmItems = [NSArray arrayWithObject:[self activeCollection]];
  }

  // if the items don't have a common parent, bail.
  if (![[BookmarkManager sharedBookmarkManager] itemsShareCommonParent:bmItems])
    return;

  // first arrange the items at the top level
  if ([bmItems count] > 1) {
    BookmarkFolder* itemsParent = (BookmarkFolder*) [[bmItems firstObject] parent];
    [itemsParent arrangeChildItems:bmItems usingSelector:sortSelector reverseSort:reverseSort];
  }

  // now sort the children if a single folder is selected,
  // or sort deep if we are doing so
  if ([bmItems count] == 1 || sortDeep) {
    NSEnumerator* itemsEnum = [bmItems objectEnumerator];
    BookmarkItem* curItem;
    while ((curItem = [itemsEnum nextObject])) {
      if ([curItem isKindOfClass:[BookmarkFolder class]]) {
        BookmarkFolder* curFolder = (BookmarkFolder*)curItem;
        [curFolder sortChildrenUsingSelector:sortSelector reverseSort:reverseSort sortDeep:sortDeep undoable:YES];
      }
    }
  }

  // reselect them
  [self selectItems:bmItems expandingContainers:NO scrollIntoView:YES];
}

- (IBAction)copyURLs:(id)aSender
{
  [[BookmarkManager sharedBookmarkManager] copyBookmarksURLs:[mBookmarksOutlineView selectedItems] toPasteboard:[NSPasteboard generalPasteboard]];
}

- (void)resetSearchField
{
  [self setSearchFilterTag:kSearchAllTag];
  [[mSearchField cell] setStringValue:@""];
  [[BookmarkManager sharedBookmarkManager] setSearchActive:NO]; // ensure the manager knows we aren't searching any more
}

- (void)setBrowserWindowController:(BrowserWindowController*)bwController
{
  // don't retain
  mBrowserWindowController = bwController;
}

// XXX unused
- (void)displayBookmarkInOutlineView:(BookmarkItem *)aBookmarkItem
{
#if 0
  if (!aBookmarkItem) return;   // avoid recursion
  BookmarkFolder *parent = [aBookmarkItem parent];
  if (parent != mRootBookmarks)
    [self displayBookmarkInOutlineView:parent];
  else {
    [self selectContainerFolder:aBookmarkItem];
    return;
  }
  [mBookmarksOutlineView expandItem:aBookmarkItem];
#endif
}

- (NSView*)bookmarksEditingView
{
  return mBookmarksEditingView;
}

- (void)restoreSplitters
{
  // restore splitters to their saved positions. We have to do this here
  // (rather than in |-completeSetup| because only at this point is the
  // manager view resized correctly. If we did it earlier, it would resize again
  // to stretch proportionally to the size of the browser window, destroying
  // the width we just set.
  if (!mSplittersRestored) {
    const float kDefaultSplitWidth = kMinContainerSplitWidth;
    float savedWidth = [[NSUserDefaults standardUserDefaults] floatForKey:USER_DEFAULTS_CONTAINER_SPLITTER_WIDTH];
    if (savedWidth < kDefaultSplitWidth)
      savedWidth = kDefaultSplitWidth;

    float maxWidth = NSWidth([mBookmarksEditingView frame]) - 100;
    if (savedWidth > maxWidth)
      savedWidth = maxWidth;

    [mContainersSplit setLeftWidth:savedWidth];
    mSplittersRestored = YES;              // needed first time only
  }
}

- (void)setCanEditSelectedContainerContents:(BOOL)inCanEdit
{
  [mBookmarksOutlineView setAllowsEditing:inCanEdit];
  [mAddButton setEnabled:inCanEdit];
  [mSortButton setEnabled:inCanEdit];
}

- (void)setActiveCollection:(BookmarkFolder *)aFolder
{
  [aFolder retain];
  [mActiveRootCollection release];
  mActiveRootCollection = aFolder;
}

- (BookmarkFolder *)activeCollection
{
  return mActiveRootCollection;
}

- (BookmarkFolder *)selectedItemFolderAndIndex:(int*)outIndex
{
  BookmarkFolder *parentFolder = nil;
  *outIndex = 0;

  if ([mBookmarksOutlineView numberOfSelectedRows] == 1) {
    BookmarkItem *item = [self selectedBookmarkItem];
    // if it's a folder, use it
    if ([item isKindOfClass:[BookmarkFolder class]]) {
      BookmarkFolder* selectedFolder = (BookmarkFolder*) item;
      *outIndex = [selectedFolder count];
      return selectedFolder;
    }

    // otherwise use its parent
    if ([item respondsToSelector:@selector(parent)]) {  // when would it not?
      parentFolder = [item parent];
      *outIndex = [parentFolder indexOfObject:item] + 1;
    }
  }

  if (!parentFolder) {
    parentFolder = [self activeCollection];
    *outIndex = [parentFolder count];
  }
  return parentFolder;
}

- (void)setItemToRevealOnLoad:(BookmarkItem*)inItem
{
  [mItemToReveal autorelease];
  mItemToReveal = [inItem retain];
}

- (void)revealItem:(BookmarkItem*)item scrollIntoView:(BOOL)inScroll selecting:(BOOL)inSelectItem byExtendingSelection:(BOOL)inExtendSelection
{
  BookmarkManager* bmManager = [BookmarkManager sharedBookmarkManager];

  BookmarkFolder* menuContainer    = [bmManager bookmarkMenuFolder];
  BookmarkFolder* toolbarContainer = [bmManager toolbarFolder];
  if ([item hasAncestor:menuContainer])
    [self selectContainerFolder:menuContainer];
  else if ([item hasAncestor:toolbarContainer])
    [self selectContainerFolder:toolbarContainer];
  else {
    // walk up to the child of the root, which should be a container
    id curParent = item;
    while (curParent && [curParent respondsToSelector:@selector(parent)] && (BookmarkFolder*)[curParent parent] != [bmManager rootBookmarks])
      curParent = [curParent parent];

    if (curParent)
      [self selectContainerFolder:(BookmarkFolder*)curParent];
  }

  [self expandAllParentsOfItem:item];

  int itemRow = [mBookmarksOutlineView rowForItem:item];
  if (itemRow == -1) return;

  if (inSelectItem)
    [mBookmarksOutlineView selectRow:itemRow byExtendingSelection:inExtendSelection];

  if (inScroll)
    [mBookmarksOutlineView scrollRowToVisible:itemRow];
}

- (void)expandAllParentsOfItem:(BookmarkItem*)inItem
{
  // make an array of parents
  NSMutableArray* parentArray = [[NSMutableArray alloc] initWithCapacity:10];

  id curItem = [inItem parent];
  while (curItem) {
    if (![curItem respondsToSelector:@selector(parent)])
      break;

    [parentArray addObject:curItem];
    curItem = [curItem parent];
  }

  // now expand from the top down (as required by the outline view)
  NSEnumerator* parentsEnum = [parentArray reverseObjectEnumerator];
  while ((curItem = [parentsEnum nextObject]))
    [mBookmarksOutlineView expandItem:curItem];

  [parentArray release];
}

- (BOOL)hasExpandedState:(id)anItem
{
  NSMutableDictionary *dict = [self expandedStateDictionary];
  return [[dict objectForKey:[anItem UUID]] boolValue];
}

- (void)setStateOfItem:(BookmarkFolder *)anItem toExpanded:(BOOL)aBool
{
  NSMutableDictionary *dict = [self expandedStateDictionary];
  if (aBool)
    [dict setObject:[NSNumber numberWithBool:YES] forKey:[anItem UUID]];
  else
    [dict removeObjectForKey:[anItem UUID]];
}

- (void)restoreFolderExpandedStates
{
  int curRow = 0;
  while (curRow < [mBookmarksOutlineView numberOfRows]) {
    id item = [mBookmarksOutlineView itemAtRow:curRow];
    if ([item isKindOfClass:[BookmarkFolder class]]) {
      if ([self hasExpandedState:item])
        [mBookmarksOutlineView expandItem:item];
      else
        [mBookmarksOutlineView collapseItem:item];
    }
    curRow ++;
  }
}

- (NSMutableDictionary *)expandedStateDictionary
{
  if (!mExpandedStates) {
    mExpandedStates = [[[NSUserDefaults standardUserDefaults] dictionaryForKey:kExpandedBookmarksStatesDefaultsKey] mutableCopy];
    if (!mExpandedStates)
      mExpandedStates = [[NSMutableDictionary alloc] initWithCapacity:20];
  }
  return mExpandedStates;
}

- (void)saveExpandedStateDictionary
{
  if (mExpandedStates)
    [[NSUserDefaults standardUserDefaults] setObject:mExpandedStates forKey:kExpandedBookmarksStatesDefaultsKey];

  NSDictionary* collectionStateDict = nil;
  BookmarkFolder* collectionFolder = [self activeCollection];
  if ([[collectionFolder identifier] length] > 0)   // if it has an identifier, use that
    collectionStateDict = [NSDictionary dictionaryWithObject:[collectionFolder identifier] forKey:kBookmarksSelectedContainerIdentifierKey];
  else    // otherwise use UUID
    collectionStateDict = [NSDictionary dictionaryWithObject:[collectionFolder UUID] forKey:kBookmarksSelectedContainerUUIDKey];

  [[NSUserDefaults standardUserDefaults] setObject:collectionStateDict forKey:kBookmarksSelectedContainerDefaultsKey];
}

- (void)pasteBookmarks:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder *)dropFolder index:(int)index copying:(BOOL)isCopy
{
  NSArray* mozBookmarkList = [BookmarkManager bookmarkItemsFromSerializableArray:[aPasteboard propertyListForType:kCaminoBookmarkListPBoardType]];

  NSMutableArray* newBookmarks = [[NSMutableArray alloc] initWithCapacity:[mozBookmarkList count]];
  if (!isCopy)
    [newBookmarks addObjectsFromArray:mozBookmarkList];

  // turn off updates to avoid lots of reloadData with multiple items
  mBookmarkUpdatesDisabled = YES;

  // make sure we re-enable updates
  NS_DURING
    NSEnumerator *enumerator = [mozBookmarkList objectEnumerator];

    id aKid;
    while ((aKid = [enumerator nextObject])) {
      if (isCopy) {
        BookmarkItem* newItem = [(BookmarkFolder*)[aKid parent] copyChild:aKid toBookmarkFolder:dropFolder atIndex:index];
        [newBookmarks addObject:newItem];
        ++index;
      }
      else {
        // need to be careful to adjust index as we insert items to avoid
        // inserting in reverse order
        if ([aKid parent] == (id)dropFolder) {
          int kidIndex = [dropFolder indexOfObject:aKid];
          [(BookmarkFolder*)[aKid parent] moveChild:aKid toBookmarkFolder:dropFolder atIndex:index];
          if (kidIndex > index)
            ++index;
        }
        else {
          [(BookmarkFolder*)[aKid parent] moveChild:aKid toBookmarkFolder:dropFolder atIndex:index];
          ++index;
        }
      }
    }
  NS_HANDLER
  NS_ENDHANDLER

  mBookmarkUpdatesDisabled = NO;
  [self reloadDataForItem:nil reloadChildren:YES];
  [self selectItems:newBookmarks expandingContainers:YES scrollIntoView:YES];
  [newBookmarks release];
}

- (void)pasteBookmarksFromURLsAndTitles:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder *)dropFolder index:(int)index
{
  NSArray* urls = nil;
  NSArray* titles = nil;

  [aPasteboard getURLs:&urls andTitles:&titles];

  // turn off updates to avoid lots of reloadData with multiple items
  mBookmarkUpdatesDisabled = YES;

  NSMutableArray* newBookmarks = [NSMutableArray arrayWithCapacity:[urls count]];
  // make sure we re-enable updates
  NS_DURING
    for (unsigned int i = 0; i < [urls count]; ++i) {
      NSString* title = [titles objectAtIndex:i];
      if ([title length] == 0)
        title = [urls objectAtIndex:i];

      [newBookmarks addObject:[dropFolder addBookmark:title url:[urls objectAtIndex:i] inPosition:(index + i) isSeparator:NO]];
    }
  NS_HANDLER
  NS_ENDHANDLER

  mBookmarkUpdatesDisabled = NO;
  [self reloadDataForItem:nil reloadChildren:YES];
  [self selectItems:newBookmarks expandingContainers:NO scrollIntoView:YES];
}

- (unsigned int)outlineView:(NSOutlineView *)outlineView draggingSourceOperationMaskForLocal:(BOOL)localFlag
{
  if (outlineView == mBookmarksOutlineView) {
    if (localFlag)
      return (NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationMove);

    return (NSDragOperationCopy | NSDragOperationLink | NSDragOperationDelete | NSDragOperationGeneric);
  }

  return NSDragOperationGeneric;
}

//
// -doDrop:intoFolder:index:
//
// called when a drop occurs on a table or outline to do the actual work based on the
// data types present in the drag info.
//
- (BOOL)doDrop:(id <NSDraggingInfo>)info intoFolder:(BookmarkFolder *)dropFolder index:(int)index
{
  NSArray* types  = [[info draggingPasteboard] types];
  BOOL isCopy = ([info draggingSourceOperationMask] == NSDragOperationCopy);

  if ([types containsObject:kCaminoBookmarkListPBoardType]) {
    [self pasteBookmarks:[info draggingPasteboard] intoFolder:dropFolder index:index copying:isCopy];
    return YES;
  }

  if ([[info draggingPasteboard] containsURLData]) {
    [self pasteBookmarksFromURLsAndTitles:[info draggingPasteboard] intoFolder:dropFolder index:index];
    return YES;
  }
  return NO;
}

// Choose a single drag operation to return based on the dragging info and the operations that table view/outline view support.
- (NSDragOperation)preferredDragOperationForInfo:(id <NSDraggingInfo>)info
{
  // If the drag came from another app, force copies (work around Safari bug)
  if (![info draggingSource])
    return NSDragOperationCopy;

  NSDragOperation srcMask = [info draggingSourceOperationMask];
  if (srcMask & NSDragOperationMove)
    return NSDragOperationMove;
  // only copy if the modifier key was held down - the OS will clear any other drag op flags
  if (srcMask == NSDragOperationCopy)
    return NSDragOperationCopy;
  if (srcMask & NSDragOperationGeneric)
    return NSDragOperationGeneric;
  return NSDragOperationNone;
}

//
// Copy a set of bookmarks (an NSArray containing the BookmarkItem and BookmarkFolder objects)
// to the specified pasteboard, in all the available formats
//
- (void)copyBookmarks:(NSArray*)bookmarkItemsToCopy toPasteboard:(NSPasteboard*)aPasteboard
{
  // Copy these items to the general pasteboard as an internal list so we can
  // paste back to ourselves with no information loss
  NSArray *bookmarkUUIDArray = [BookmarkManager serializableArrayWithBookmarkItems:bookmarkItemsToCopy];
  [aPasteboard declareTypes:[NSArray arrayWithObject:kCaminoBookmarkListPBoardType] owner:self];
  [aPasteboard setPropertyList:bookmarkUUIDArray forType:kCaminoBookmarkListPBoardType];

  // Now add copies in formats useful to other applications. Our pasteboard
  // category takes care of working out what formats to write.
  NSMutableArray* urlList = [NSMutableArray array];
  NSMutableArray* titleList = [NSMutableArray array];
  NSEnumerator* bookmarkItemsEnum = [bookmarkItemsToCopy objectEnumerator];
  BookmarkItem* curItem;
  while (curItem = [bookmarkItemsEnum nextObject]) {
    if ([curItem isKindOfClass:[Bookmark class]]) {
      [urlList addObject:[(Bookmark*)curItem url]];
      [titleList addObject:[(Bookmark*)curItem title]];
    }
  }
  [aPasteboard setURLs:urlList withTitles:titleList];
}

- (BOOL)canPasteFromPasteboard:(NSPasteboard*)aPasteboard
{
    return [[aPasteboard types] containsObject:kCaminoBookmarkListPBoardType]
        || [aPasteboard containsURLData];
}

#pragma mark -
//
// table view things
//
- (int)containerCount
{
  return [mRootBookmarks count];
}

- (void)selectContainerFolder:(BookmarkFolder*)inFolder
{
  BookmarkManager* bmManager = [BookmarkManager sharedBookmarkManager];

  unsigned folderIndex = [bmManager indexOfContainer:inFolder];
  if (folderIndex == NSNotFound)
    return;

  [mContainersTableView selectRow:folderIndex byExtendingSelection:NO];

  // reset the search
  [self resetSearchField];

  if (inFolder == [bmManager historyFolder]) {
    [self setActiveOutlineView:mHistoryOutlineView];

    [mHistoryOutlineViewDelegate clearSearchResults];
    [mHistoryOutlineViewDelegate historyViewMadeVisible:YES];

    [mAddButton    setEnabled:NO];
    [mActionButton setMenu:mActionMenuHistory];
    [mSortButton   setEnabled:YES];
    [mSortButton   setMenu:mSortMenuHistory];
    [[mSearchField cell] setSearchMenuTemplate:mQuickSearchMenuHistory];
    [self setSearchFilterTag:kSearchAllTag];

    [[mBookmarksEditingView window] setTitle:NSLocalizedString(@"HistoryWindowTitle", nil)];
  }
  else {
    [self setActiveOutlineView:mBookmarksOutlineView];

    [mHistoryOutlineViewDelegate historyViewMadeVisible:NO];

    [self setActiveCollection:inFolder];
    [self clearSearchResults];

    if ([inFolder isSmartFolder])
      [self setCanEditSelectedContainerContents:NO];
    else
      [self setCanEditSelectedContainerContents:YES];

    // if it's a smart folder, but not the history folder
    if ([inFolder isSmartFolder])
      [mBookmarksOutlineView setDeleteAction:nil];
    else
      [mBookmarksOutlineView setDeleteAction:@selector(deleteBookmarks:)];

    [mActionButton setMenu:mActionMenuBookmarks];
    [mSortButton   setMenu:mSortMenuBookmarks];
    [[mSearchField cell] setSearchMenuTemplate:mQuickSearchMenuBookmarks];
    [self setSearchFilterTag:kSearchAllTag];

    [[mBookmarksEditingView window] setTitle:NSLocalizedString(@"BookmarksWindowTitle", nil)];

    // this reload ensures that we display the newly selected activeCollection
    [mBookmarksOutlineView reloadData];
    // after we've reloaded data, restore twisty states
    [self restoreFolderExpandedStates];
  }
}

- (BookmarkFolder*)selectedContainerFolder
{
  int selectedRow = [mContainersTableView selectedRow];
  id selectedItem = [mRootBookmarks objectAtIndex:selectedRow];
  if ([selectedItem isKindOfClass:[BookmarkFolder class]])
    return (BookmarkFolder*)selectedItem;

  return nil;
}

- (void)selectLastContainer
{
  BookmarkFolder* lastContainer = [[[[BookmarkManager sharedBookmarkManager] rootBookmarks] childArray] lastObject];
  [self selectContainerFolder:lastContainer];
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
  if (tableView == mContainersTableView)
    return [mRootBookmarks count];

  return 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
  id retValue = nil;
  id item = nil;

  if (tableView == mContainersTableView)
    item = [mRootBookmarks objectAtIndex:row];

  NS_DURING
    retValue = [item valueForKey:[tableColumn identifier]];
  NS_HANDLER
    retValue = nil;
  NS_ENDHANDLER
  return retValue;
}

- (void)tableView:(NSTableView *)inTableView willDisplayCell:(id)inCell forTableColumn:(NSTableColumn *)inTableColumn row:(int)inRowIndex
{
  if (inTableView == mContainersTableView) {
    BookmarkFolder *aFolder = [mRootBookmarks objectAtIndex:inRowIndex];
    [inCell setImage:[aFolder icon]];
  }
}

- (BOOL)tableView:(NSTableView *)aTableView shouldEditTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  if (aTableView == mContainersTableView) {
    BookmarkFolder* aFolder = [mRootBookmarks objectAtIndex:rowIndex];
    return [[BookmarkManager sharedBookmarkManager] isUserCollection:aFolder];
  }
  return NO;
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
  if (tableView == mContainersTableView) {
    BookmarkFolder *aFolder = [mRootBookmarks objectAtIndex:row];
    [aFolder setTitle:object];
  }
}

- (BOOL)tableView:(NSTableView *)tv writeRows:(NSArray*)rows toPasteboard:(NSPasteboard*)pboard
{
  int count = [rows count];
  if (count == 0)
    return NO;

  NSMutableArray* itemArray = [[NSMutableArray alloc] initWithCapacity:count];
  BookmarkManager* manager = [BookmarkManager sharedBookmarkManager];

  NSEnumerator* enumerator = [rows objectEnumerator];
  id curRow;
  while ((curRow = [enumerator nextObject])) {
    int rowVal = [curRow intValue];
    BookmarkFolder* collectionFolder = [mRootBookmarks objectAtIndex:rowVal];
    if ([manager isUserCollection:collectionFolder])
      [itemArray addObject:collectionFolder];
  }

  BOOL haveCopyableItems = ([itemArray count] > 0);
  if (haveCopyableItems)
    [self copyBookmarks:itemArray toPasteboard:pboard];

  [itemArray release];
  return haveCopyableItems;
}

//
// -tableView:validateDrop:proposedRow:proposedDropOperation:
//
// validate if the drop is allowed and what type it is (move, copy, etc).
//
- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op
{
  if (tv == mContainersTableView) {
    NSArray* types = [[info draggingPasteboard] types];
    NSDragOperation dragOp = [self preferredDragOperationForInfo:info];
    // figure out where we want to drop. |dropFolder| will either be a container or
    // the top-level bookmarks root if we're to create a new container.
    BookmarkManager* manager = [BookmarkManager sharedBookmarkManager];
    BookmarkFolder* dropFolder = nil;

    if (op == NSTableViewDropOn) {
      BookmarkFolder* destFolder = [mRootBookmarks objectAtIndex:row];
      // only use this if it's a modifiable folder
      if (![destFolder isSmartFolder])
        dropFolder = destFolder;
    }
    else if (op == NSTableViewDropAbove) {
      // disallow drops above the first user collection (this assumes that the last smart
      // folder is the address book folder)
      int firstUserCollectionRow = [manager indexOfContainer:[manager addressBookFolder]] + 1;
      if (row >= firstUserCollectionRow)
        dropFolder = mRootBookmarks;
    }

    if (dropFolder) {
      // special check if we're moving pointers around
      if ([types containsObject:kCaminoBookmarkListPBoardType]) {
        NSArray* draggedItems = [BookmarkManager bookmarkItemsFromSerializableArray:[[info draggingPasteboard] propertyListForType:kCaminoBookmarkListPBoardType]];
        BOOL isOK = [manager isDropValid:draggedItems toFolder:dropFolder];
        return (isOK) ? dragOp : NSDragOperationNone;
      }
      else if ([[info draggingPasteboard] containsURLData]) {
        return (dropFolder == mRootBookmarks) ? NSDragOperationNone : dragOp;
      }
    }
  }
  // nope
  return NSDragOperationNone;
}

- (BOOL)tableView:(NSTableView*)tv acceptDrop:(id <NSDraggingInfo>)info row:(int)row dropOperation:(NSTableViewDropOperation)op
{
  if (tv != mContainersTableView)
    return NO;
  // get info
  BookmarkFolder *dropFolder;
  int dropLocation = row;
  if (op == NSTableViewDropAbove)
    dropFolder = mRootBookmarks;
  else {
    dropFolder = [mRootBookmarks objectAtIndex:row];
    dropLocation = [dropFolder count];
  }
  BOOL result = [self doDrop:info intoFolder:dropFolder index:dropLocation];
  [self selectContainerFolder:[self selectedContainerFolder]];
  return result;
}

- (void)tableViewSelectionDidChange:(NSNotification *)note
{
  NSTableView *aView = [note object];
  if (aView == mContainersTableView) {

    [self selectContainerFolder:[self selectedContainerFolder]];
  }
}

- (NSMenu *)tableView:(NSTableView *)aTableView contextMenuForRow:(int)rowIndex
{
  if (aTableView == mContainersTableView) {
    NSMenu* contextMenu = [[[aTableView menu] copy] autorelease];
    if ([aTableView numberOfSelectedRows] > 0) {
      BookmarkFolder* aFolder = [mRootBookmarks objectAtIndex:rowIndex];

      [contextMenu addItem:[NSMenuItem separatorItem]];
      NSMenuItem* useAsDockItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Use as Dock Menu", nil)
                                                             action:@selector(toggleIsDockMenuFolder:)
                                                      keyEquivalent:@""];
      [useAsDockItem setTarget:self];
      if ([aFolder isDockMenu])
        [useAsDockItem setState:NSOnState];
      [contextMenu addItem:useAsDockItem];
      [useAsDockItem release];

      if ([[BookmarkManager sharedBookmarkManager] isUserCollection:aFolder]) {
        NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Delete", nil)
                                                            action:@selector(deleteCollection:)
                                                     keyEquivalent:@""];
        [deleteItem setTarget:self];
        [contextMenu addItem:deleteItem];
        [deleteItem release];
      }
    }
    return contextMenu;
  }
  return nil;
}

#pragma mark -

- (void)outlineView:(NSOutlineView *)outlineView didClickTableColumn:(NSTableColumn *)tableColumn
{
  if (outlineView == mBookmarksOutlineView) {
    // XXX impl bookmarks sorting
  }
}

//
// outlineView:shouldEditTableColumn:item: (delegate method)
//
// Called by the outliner to determine whether or not we should allow the
// user to edit this item. We always return NO, because we invoke the
// edit methods manually.
//
- (BOOL)outlineView:(NSOutlineView *)outlineView shouldEditTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
  return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
  if (item) // it's a BookmarkFolder
    return [item objectAtIndex:index];
  return [[self itemTreeRootContainer] objectAtIndex:index];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
  if (!(item) || [item isKindOfClass:[BookmarkFolder class]])
    return YES;
  return NO;
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
  if (item) // it's a BookmarkFolder
    return [item count];
  return [[self itemTreeRootContainer] count];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  id retValue = nil;
  NS_DURING
    retValue = [item valueForKey:[tableColumn identifier]];
  NS_HANDLER
    if ([item isKindOfClass:[BookmarkFolder class]] && [[tableColumn identifier] isEqualToString:@"url"])
      retValue = [BookmarkViewController greyStringWithItemCount:[item count]];
    else
      retValue = nil;
  NS_ENDHANDLER
  return retValue;
}

// this is a delegate, not a data source method
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
  // set the image on the name column. the url column doesn't have an image.
  if ([[tableColumn identifier] isEqualToString:@"title"]) {
    if ([item respondsToSelector:@selector(isSeparator)] && [item isSeparator]) {
      [cell setTitle:@""];
      float fullWidth = [tableColumn width] - kOutlineViewLeftMargin -
                        [outlineView indentationPerLevel]*[outlineView levelForItem:item];
      NSSize imageSize = [mSeparatorImage size];
      imageSize.width = (fullWidth > kMinSeparatorWidth) ? fullWidth : kMinSeparatorWidth;
      [mSeparatorImage setSize:imageSize];
      [cell setImage:mSeparatorImage];
    }
    else {
      [cell setImage:[item icon]];
    }
  }
}

- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  NS_DURING
    [item takeValue:object forKey:[tableColumn identifier]];
  NS_HANDLER
    return;
  NS_ENDHANDLER
}

- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard
{
  int count = [items count];
  if ((count == 0) || [mActiveRootCollection isSmartFolder]) // XXX why deny drags from smart folders?
    return NO;

  // Pack pointers to bookmark items into this array.
  [self copyBookmarks:items toPasteboard:pboard];
  return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView*)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
  //  if the index is -1, deny the drop
  if (index == NSOutlineViewDropOnItemIndex)
    return NSDragOperationNone;

  // if the proposed item's parent is a smart folder, deny the drop
  BookmarkFolder* parent = (item) ? item : [self itemTreeRootContainer];
  if ([parent isSmartFolder])
    return NSDragOperationNone;

  NSArray* types = [[info draggingPasteboard] types];
  NSDragOperation dragOp = [self preferredDragOperationForInfo:info];

  if ([types containsObject:kCaminoBookmarkListPBoardType]) {
    NSArray *draggedItems = [BookmarkManager bookmarkItemsFromSerializableArray:[[info draggingPasteboard] propertyListForType:kCaminoBookmarkListPBoardType]];
    BOOL isOK = [[BookmarkManager sharedBookmarkManager] isDropValid:draggedItems toFolder:parent];
    return (isOK) ? dragOp : NSDragOperationNone;
  }

  if ([[info draggingPasteboard] containsURLData])
    return dragOp;

  return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index
{
  BookmarkFolder *parent = (item) ? item : [self itemTreeRootContainer];
  BOOL retVal = [self doDrop:info intoFolder:parent index:index];
  return retVal;
}

// implementing this makes NSOutlineView updates much slower (because of all the hover region maintenance)
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)item
{
  if ([item isKindOfClass:[Bookmark class]]) {
    if ([[item itemDescription] length] > 0)
      return [NSString stringWithFormat:@"%@\n%@", [item url], [item itemDescription]];
    else
      return [item url];
  }
  else if ([item isKindOfClass:[BookmarkFolder class]]) {
    if ([[item itemDescription] length] > 0)
      return [item itemDescription];
    else
      return [item title];
  }
  else
    return nil;
}

- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItems:(NSArray*)items
{
  return [[BookmarkManager sharedBookmarkManager] contextMenuForItems:items fromView:(BookmarkOutlineView*)outlineView target:self];
}

- (BOOL)outlineView:(NSOutlineView*)inOutlineView columnHasIcon:(NSTableColumn*)inColumn
{
  BOOL hasIcon = NO;
  if ([[inColumn identifier] isEqualToString:@"title"])
    hasIcon = YES;
  return hasIcon;
}

- (void)reloadDataForItem:(id)item reloadChildren:(BOOL)aReloadChildren
{
  if (mBookmarkUpdatesDisabled)
    return;

  if (!item || (item == mActiveRootCollection))
    [mBookmarksOutlineView reloadData];
  else
    [mBookmarksOutlineView reloadItem:item reloadChildren:aReloadChildren];
}

- (int)numberOfSelectedRows
{
  return [mBookmarksOutlineView numberOfSelectedRows];
}

- (BOOL)haveSelectedRow
{
  return ([mBookmarksOutlineView selectedRow] != -1);
}

- (void)outlineViewSelectionDidChange:(NSNotification*)aNotification
{
  BookmarkInfoController* bic = [BookmarkInfoController existingSharedBookmarkInfoController];
  if ([[bic window] isVisible]) {
    if ([mBookmarksOutlineView numberOfSelectedRows] == 1)
        [bic setBookmark:[self selectedBookmarkItem]];
    else
        [bic setBookmark:nil];
  }
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
  // Window actions are disabled while a sheet is showing
  if ([[mBrowserWindowController window] attachedSheet])
    return NO;

  SEL action = [menuItem action];

  if ([self activeOutlineView] == mBookmarksOutlineView) {
    if (action == @selector(addBookmarkSeparator:)) {
      BookmarkFolder *activeCollection = [self activeCollection];
      return (![activeCollection isRoot] && ![activeCollection isSmartFolder]);
    }

    BookmarkItem* selItem = [self selectedBookmarkItem];

    if ((action == @selector(openBookmark:)) ||
        (action == @selector(openBookmarkInNewTab:)) ||
        (action == @selector(openBookmarkInNewWindow:)) ||
        (action == @selector(deleteBookmarks:)) ||
        (action == @selector(showBookmarkInfo:)))
    {
      return (selItem != nil);
    }

    // getInfo: is passed here from BrowserWindowController
    if (action == @selector(getInfo:))
      return ((selItem != nil) && ![selItem isSeparator]);

    if (action == @selector(arrange:)) {
      BookmarkFolder* activeCollection = [self activeCollection];
      if ([activeCollection isRoot] || [activeCollection isSmartFolder])
        return NO;

      NSArray* selectedBMs = [mBookmarksOutlineView selectedItems];
      return ([selectedBMs count] == 0) ||
             ([selectedBMs count] == 1 && [[selectedBMs firstObject] isKindOfClass:[BookmarkFolder class]]) ||
             (([selectedBMs count] > 1) && [[BookmarkManager sharedBookmarkManager] itemsShareCommonParent:selectedBMs]);
    }
  }
  else {  // history visible
    if (action == @selector(addBookmark:))
      return NO;

    if (action == @selector(addFolder:))
      return NO;

    if (action == @selector(addBookmarkSeparator:))
      return NO;

  }
  return YES;
}

- (void)outlineViewItemDidExpand:(NSNotification *)notification
{
  id item = [[notification userInfo] objectForKey:@"NSObject"];
  [self setStateOfItem:item toExpanded:YES];
}

- (void)outlineViewItemDidCollapse:(NSNotification *)notification
{
  id item = [[notification userInfo] objectForKey:@"NSObject"];
  [self setStateOfItem:item toExpanded:NO];
}

#pragma mark -

- (IBAction)searchStringChanged:(id)aSender
{
  NSString* searchString = [[mSearchField cell] stringValue];
  if ([searchString length] == 0) {
    [self clearSearchResults];
    [[self activeOutlineView] reloadData];
    [[BookmarkManager sharedBookmarkManager] setSearchActive:NO];
  }
  else {
    [self searchFor:searchString inFieldWithTag:mSearchTag];
    [[self activeOutlineView] reloadData];
    [[BookmarkManager sharedBookmarkManager] setSearchActive:YES];
  }
}

- (IBAction)quicksearchPopupChanged:(id)aSender
{
  [self setSearchFilterTag:[aSender tag]];
  // do the search again with the new filter
  [self searchStringChanged:aSender];
}

- (void)setSearchFilterTag:(int)tag
{
  mSearchTag = tag;
  NSMenu* menuTemplate = [[mSearchField cell] searchMenuTemplate];
  [menuTemplate checkItemWithTag:mSearchTag uncheckingOtherItems:YES];
  [[mSearchField cell] setPlaceholderString:[[menuTemplate itemWithTag:mSearchTag] title]];
  [[mSearchField cell] setSearchMenuTemplate:menuTemplate];
}

- (void)searchFor:(NSString*)searchString inFieldWithTag:(int)tag
{
  if ([self activeOutlineView] == mHistoryOutlineView)
    [mHistoryOutlineViewDelegate searchFor:searchString inFieldWithTag:tag];
  else {
    BookmarkFolder* searchRoot = [self activeCollection];
    NSArray* searchResults = [[BookmarkManager sharedBookmarkManager] searchBookmarksContainer:searchRoot
                                                                                     forString:searchString
                                                                                inFieldWithTag:tag];
    [self setSearchResultArray:searchResults];
  }
}

- (void)clearSearchResults
{
  if ([self activeOutlineView] == mHistoryOutlineView) {
    [mHistoryOutlineViewDelegate clearSearchResults];
  }
  else {
    [mSearchResultArray release];
    mSearchResultArray = nil;
  }
}

- (void)selectItems:(NSArray*)items expandingContainers:(BOOL)expandContainers scrollIntoView:(BOOL)scroll
{
  NSEnumerator* itemsEnum = [items objectEnumerator];
  [mBookmarksOutlineView deselectAll:nil];
  BookmarkItem* item;
  while ((item = [itemsEnum nextObject])) {
    [self revealItem:item scrollIntoView:scroll selecting:YES byExtendingSelection:YES];
  }
}

- (BookmarkItem*)selectedBookmarkItem
{
  int index = [mBookmarksOutlineView selectedRow];
  if (index == -1) return nil;

  return (BookmarkItem*)[mBookmarksOutlineView itemAtRow:index];
}

- (SEL)sortSelectorFromItemTag:(int)inTag
{
  switch (inTag & kArrangeBookmarksFieldMask) {
    default:
      NSLog(@"Unknown sort tag mask");
      // fall through
    case kArrangeBookmarksByLocationMask:
      return @selector(compareURL:sortDescending:);

    case kArrangeBookmarksByTitleMask:
      return @selector(compareTitle:sortDescending:);

    case kArrangeBookmarksByKeywordMask:
      return @selector(compareKeyword:sortDescending:);

    case kArrangeBookmarksByDescriptionMask:
      return @selector(compareDescription:sortDescending:);

    case kArrangeBookmarksByLastVisitMask:
      return @selector(compareLastVisitDate:sortDescending:);

    case kArrangeBookmarksByVisitCountMask:
      return @selector(compareVisitCount:sortDescending:);

    case kArrangeBookmarksByTypeMask:
      return @selector(compareType:sortDescending:);
  }

  return NULL;  // keep compiler quiet
}


- (id)itemTreeRootContainer
{
  if (mSearchResultArray)
    return mSearchResultArray;

  return [self activeCollection];
}

- (void)setActiveOutlineView:(NSOutlineView*)outlineView
{
  if (outlineView == mBookmarksOutlineView) {
    [mOutlinerHostView swapFirstSubview:mBookmarksHostView];
    [mContainersTableView setNextKeyView:mBookmarksOutlineView];
    [mBookmarksOutlineView setNextKeyView:mAddCollectionButton];
  }
  else {
    [mOutlinerHostView swapFirstSubview:mHistoryHostView];
    [mContainersTableView setNextKeyView:mHistoryOutlineView];

    // we're setting this explicitly, because doing it from the nib
    // makes the shift-tab case not work; appkit bug?
    [mHistoryOutlineView setNextKeyView:mAddCollectionButton];
  }
}

- (NSOutlineView*)activeOutlineView
{
  if ([mOutlinerHostView firstSubview] == mBookmarksHostView)
    return mBookmarksOutlineView;

  if ([mOutlinerHostView firstSubview] == mHistoryHostView)
    return mHistoryOutlineView;

  return nil;
}

- (void)actionButtonWillDisplay:(NSNotification *)notification
{
  NSMenu* actionMenu = nil;
  if ([self activeOutlineView] == mHistoryOutlineView) {
    NSArray* selectedItems = [mHistoryOutlineView selectedItems];
    if ([selectedItems count] > 0)
      actionMenu = [mHistoryOutlineViewDelegate outlineView:mHistoryOutlineView contextMenuForItems:selectedItems];
    else
      actionMenu = mActionMenuHistory;
  }
  else {
    NSArray* selectedBMs = [mBookmarksOutlineView selectedItems];
    if ([selectedBMs count] > 0) {
      actionMenu = [[BookmarkManager sharedBookmarkManager] contextMenuForItems:selectedBMs
                                                                       fromView:mBookmarksOutlineView
                                                                         target:self];
      // remove the arrange stuff, because it's on the sort button too
      int arrangeSeparatorIndex = [actionMenu indexOfItemWithTag:kBookmarksContextMenuArrangeSeparatorTag];
      if (arrangeSeparatorIndex != -1)
        [actionMenu removeItemsFromIndex:arrangeSeparatorIndex];
    }
    else
      actionMenu = mActionMenuBookmarks;
  }

  [mActionButton setMenu:actionMenu];
}

#pragma mark -

//
// Network services protocol
//
- (void)availableServicesChanged:(NSNotification *)note
{
}

//
// we've got to to a delayed call here in case we received
// the note before the bookmark was updated
//
- (void)serviceResolved:(NSNotification *)note
{
  if (mOpenActionFlag == eNoOpenAction)
    return;
  NSDictionary *dict = [note userInfo];
  id aClient = [dict objectForKey:NetworkServicesClientKey];
  if ([aClient isKindOfClass:[Bookmark class]]) {
    switch (mOpenActionFlag) {
      case (eOpenBookmarkAction):
        [self performSelector:@selector(openBookmark:) withObject:aClient afterDelay:0];
        break;

      case (eOpenInNewTabAction):
        [self performSelector:@selector(openBookmarkInNewTab:) withObject:aClient afterDelay:0];
        break;

      case (eOpenInNewWindowAction):
        [self performSelector:@selector(openBookmarkInNewWindow:) withObject:aClient afterDelay:0];
        break;

      default:
        break;
    }
    mOpenActionFlag = eNoOpenAction;
  }
}

- (void)serviceResolutionFailed:(NSNotification *)note
{
}


#pragma mark -
//
// BookmarksClient protocol
//
- (void)bookmarkAdded:(NSNotification *)note
{
  BookmarkItem* addedItem = [note object];
  if ((addedItem == [[BookmarkManager sharedBookmarkManager] rootBookmarks])) {
    [mContainersTableView reloadData];
    BookmarkFolder* updatedFolder = [[note userInfo] objectForKey:BookmarkFolderChildKey];
    [self selectContainerFolder:updatedFolder];
    return;
  }

  if (addedItem == mActiveRootCollection)
    addedItem = nil;

  [self reloadDataForItem:addedItem reloadChildren:YES];
}

- (void)bookmarkRemoved:(NSNotification *)note
{
  BookmarkItem* removedItem = [note object];

  if (removedItem == mItemToReveal) {
    [mItemToReveal autorelease];
    mItemToReveal = nil;
  }

  if ((removedItem == [[BookmarkManager sharedBookmarkManager] rootBookmarks])) {
    [mContainersTableView reloadData];
    return;
  }

  if (removedItem == mActiveRootCollection)
    removedItem = nil;

  [self reloadDataForItem:removedItem reloadChildren:YES];
}

- (void)bookmarkChanged:(NSNotification *)note
{
  const unsigned int kVisibleAttributeChangedFlags = (kBookmarkItemTitleChangedMask |
                                                      kBookmarkItemIconChangedMask |
                                                      kBookmarkItemURLChangedMask |
                                                      kBookmarkItemKeywordChangedMask |
                                                      kBookmarkItemDescriptionChangedMask |
                                                      kBookmarkItemLastVisitChangedMask |
                                                      kBookmarkItemStatusChangedMask);

  BOOL reloadItem     = [BookmarkItem bookmarkChangedNotificationUserInfo:[note userInfo]
                                                            containsFlags:kVisibleAttributeChangedFlags];
  BOOL reloadChildren = [BookmarkItem bookmarkChangedNotificationUserInfo:[note userInfo]
                                                            containsFlags:kBookmarkItemChildrenChangedMask];

  if (reloadItem || reloadChildren)
    [self reloadDataForItem:[note object] reloadChildren:reloadChildren];
}

- (void)bookmarksViewDidMoveToWindow:(NSWindow*)inWindow
{
  // we're leaving the window, so...
  if (!inWindow) {
    // save the splitter width
    float containerWidth = [mContainersSplit leftWidth];
    [[NSUserDefaults standardUserDefaults] setFloat:containerWidth forKey:USER_DEFAULTS_CONTAINER_SPLITTER_WIDTH];

    // save the expanded state
    [self saveExpandedStateDictionary];
  }

}

#pragma mark -

//
// - splitView:canCollapseSubview:
//
// Called when appkit wants to ask if it can collapse a subview. The only subview
// of our splits that we allow to be hidden is the search panel.
//
- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview
{
  return NO;
}

- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedCoord ofSubviewAt:(int)offset
{
  if (sender == mContainersSplit)
    return kMinContainerSplitWidth;  // minimum size of collections pane

  return proposedCoord;
}

- (void)splitViewDidResizeSubviews:(NSNotification *)aNotification
{
}


#pragma mark -

// ContentViewProvider protocol

- (NSView*)provideContentViewForURL:(NSString*)inURL
{
  [self ensureNibLoaded];

  if ([[inURL lowercaseString] isEqualToString:@"about:history"])
    [self selectContainerFolder:[[BookmarkManager sharedBookmarkManager] historyFolder]];
  else {
    BookmarkManager* bookmarkManager = [BookmarkManager sharedBookmarkManager];
    BookmarkFolder* folderToSelect = [bookmarkManager bookmarkMenuFolder];

    // fetch the last-viewed container
    NSDictionary* selectedContainerInfo = [[NSUserDefaults standardUserDefaults] dictionaryForKey:kBookmarksSelectedContainerDefaultsKey];
    if (selectedContainerInfo) {
      NSString* containerId = nil;
      BookmarkFolder* theFolder = nil;
      if ((containerId = [selectedContainerInfo objectForKey:kBookmarksSelectedContainerIdentifierKey]))
        theFolder = [bookmarkManager rootBookmarkFolderWithIdentifier:containerId];
      else if ((containerId = [selectedContainerInfo objectForKey:kBookmarksSelectedContainerUUIDKey])) {
        theFolder = (BookmarkFolder*)[bookmarkManager itemWithUUID:containerId];
        // make sure it's (still) a container
        if ([theFolder parent] != [bookmarkManager rootBookmarks])
          theFolder = nil;
      }
      if (theFolder)
        folderToSelect = theFolder;
    }

    [self selectContainerFolder:folderToSelect];
  }

  if (mItemToReveal) {
    [self revealItem:mItemToReveal scrollIntoView:YES selecting:YES byExtendingSelection:NO];
    [mItemToReveal release];
    mItemToReveal = nil;
  }

  return mBookmarksEditingView;
}

- (void)contentView:(NSView*)inView usedForURL:(NSString*)inURL
{
  if (inView == mBookmarksEditingView) {
    [self restoreSplitters];

    [[NSApp delegate] closeFindDialog];

    // Set the initial focus to the Search field unless a row is already selected;
    // e.g., if setItemToRevealOnLoad: is used.
    // For more info about focus, see the header.
    if ([mBookmarksOutlineView selectedRow] == -1)
      [self focusSearchField];
    else
      [[mBookmarksEditingView window] makeFirstResponder:mBookmarksOutlineView];
  }
}

- (void)focusSearchField
{
  [[mBookmarksEditingView window] makeFirstResponder:mSearchField];
}

@end

#pragma mark -

@implementation BookmarksEditingView

- (id)delegate
{
  return mDelegate;
}

- (void)setDelegate:(id)inDelegate
{
  mDelegate = inDelegate;
}

- (void)viewDidMoveToWindow
{
  if ([mDelegate respondsToSelector:@selector(bookmarksViewDidMoveToWindow:)])
    [mDelegate bookmarksViewDidMoveToWindow:[self window]];
}

@end

