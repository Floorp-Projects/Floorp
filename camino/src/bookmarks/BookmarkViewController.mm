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
 *   Josh Aas <josha@mac.com>
 *   Bruce Davidson <Bruce.Davidson@ipl.com>
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

#import "NSString+Utils.h"
#import "NSPasteboard+Utils.h"
#import "NSSplitView+Utils.h"
#import "NSView+Utils.h"

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
#import "SearchTextField.h"
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



#define kNoOpenAction 0
#define kOpenBookmarkAction 1
#define kOpenInNewTabAction 2
#define kOpenInNewWindowAction 3

#define kGetInfoContextMenuItemTag 9

static NSString* const kExpandedBookmarksStatesDefaultsKey = @"bookmarks_expand_state";

// minimum sizes for the search panel
const long kMinContainerSplitWidth = 150;
const long kMinSearchPaneHeight = 80;

// The actual constant defined in 10.3.x and greater headers is NSTableViewSolidVerticalGridLineMask.
// In order to compile with 10.2.x, the value has just been extracted and put here.
// It is extremely unlikely that Apple will change it.
static const unsigned int TableViewSolidVerticalGridLineMask = 1;

static const int kDisabledQuicksearchPopupItemTag = 9999;

#pragma mark -

@interface BookmarkViewController (Private) <BookmarksClient, NetworkServicesClient>

- (void)ensureNibLoaded;
- (void)completeSetup;
- (void)setupAppearanceOfTableView:(NSTableView*)tableView;

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren;

- (void)setSearchResultArray:(NSArray *)anArray;
- (void)displayBookmarkInOutlineView:(BookmarkItem *)aBookmarkItem;
- (BOOL)doDrop:(id <NSDraggingInfo>)info intoFolder:(BookmarkFolder *)dropFolder index:(int)index;

- (void)searchStringChanged:(NSString*)searchString;
- (void)searchFor:(NSString*)searchString inFieldWithTag:(int)tag;
- (void)clearSearchResults;

- (void)selectContainer:(int)inRowIndex;

- (void)selectItems:(NSArray*)items expandingContainers:(BOOL)expandContainers scrollIntoView:(BOOL)scroll;
- (void)selectItem:(BookmarkItem*)item expandingContainers:(BOOL)expandContainers scrollIntoView:(BOOL)scroll byExtendingSelection:(BOOL)extendSelection;

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

- (NSDragOperation)preferredDragOperationForSourceMask:(NSDragOperation)srcMask;

-(void)pasteBookmarks:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder *)dropFolder index:(int)index copying:(BOOL)isCopy;
-(void)pasteBookmarksFromURLsAndTitles:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder*)dropFolder index:(int)index;
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
  if ((self = [super init]))
  {
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
// only if we think we've fully initalized ourselves.
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
  [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkIconChangedNotification object:nil];
  [nc addObserver:self selector:@selector(serviceResolved:) name:NetworkServicesResolutionSuccess object:nil];

  // get notified when the action button pops up, to set its menu
  [nc addObserver:self selector:@selector(actionButtonWillDisplay:) name:PopupMenuButtonWillDisplayMenu object:mActionButton];

  // register for notifications of when the BM manager starts up. Since it does it on a separate thread,
  // it can be created after we are and if we don't update ourselves, the bar will be blank. This
  // happens most notably when the app is launched with a 'odoc' or 'GURL' appleEvent.
  [nc addObserver:self selector:@selector(managerStarted:) name:kBookmarkManagerStartedNotification object:nil];
  
  // register for dragged types
  [mContainersTableView registerForDraggedTypes:[NSArray arrayWithObjects:kCaminoBookmarkListPBoardType, kWebURLsWithTitlesPboardType, NSURLPboardType, NSStringPboardType, nil]];

  [self ensureBookmarks];

  // these should be settable in the nib.  however, whenever
  // I try, they disappear as soon as I've saved.  Very annoying.
  [mContainersTableView setAutosaveName:@"BMContainerView"];
  [mContainersTableView setAutosaveTableColumns:YES];

  [mBookmarksOutlineView setAutosaveName:@"BookmarksOutlineView"];
  [mBookmarksOutlineView setAutosaveTableColumns:YES];

  [mHistoryOutlineView setAutosaveName:@"HistoryOutlineView"];
  [mHistoryOutlineView setAutosaveTableColumns:YES];
  [mHistoryOutlineView setAutosaveTableSort:YES];

  [[mSearchField cell] setControlSize:NSSmallControlSize];
  
  mSetupComplete = YES;
}

- (void)setupAppearanceOfTableView:(NSTableView*)tableView
{
  // the standard table item doesn't handle text and icons. Replace it
  // with a custom cell that does.
  ImageAndTextCell* imageAndTextCell = [[[ImageAndTextCell alloc] init] autorelease];
  [imageAndTextCell setEditable: YES];
  [imageAndTextCell setWraps: NO];

  NSTableColumn* itemNameColumn = [tableView tableColumnWithIdentifier: @"title"];
  [itemNameColumn setDataCell:imageAndTextCell];

  if ([tableView respondsToSelector:@selector(setUsesAlternatingRowBackgroundColors:)]) {
    [tableView setUsesAlternatingRowBackgroundColors:YES];
    // if it responds to the above selector, then it will respond to this too...
    [tableView setGridStyleMask:TableViewSolidVerticalGridLineMask];
  }
  
  // set up the font on the item & search views to be smaller
  // also don't let the cells draw their backgrounds
  NSArray* columns = [tableView tableColumns];
  if (columns)
  {
    int numColumns = [columns count];
    NSFont* smallerFont = [NSFont systemFontOfSize:11];
    for (int i = 0; i < numColumns; i++)
    {
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
-(void)ensureBookmarks
{
  if (!mRootBookmarks)
  {
    BookmarkFolder* manager = [[BookmarkManager sharedBookmarkManager] rootBookmarks];
    if (![manager count])     // not initialized yet, try again later (from start notifiation)
      return;

    mRootBookmarks = [manager retain];

    [mContainersTableView setTarget:self];
    [mContainersTableView setDeleteAction:@selector(deleteCollection:)];
    [mContainersTableView reloadData];

    [mBookmarksOutlineView setTarget: self];
    [mBookmarksOutlineView setDoubleAction: @selector(openBookmark:)];
    [mBookmarksOutlineView setDeleteAction: @selector(deleteBookmarks:)];
    [mBookmarksOutlineView reloadData];
  }
}

-(void) setSearchResultArray:(NSArray *)anArray
{
  [anArray retain];
  [mSearchResultArray release];
  mSearchResultArray = anArray;
}

//
// IBActions
//

- (IBAction) setAsDockMenuFolder:(id)aSender
{
  int rowIndex = [mContainersTableView selectedRow];
  if (rowIndex >= 0) {
    BookmarkFolder *aFolder = [mRootBookmarks objectAtIndex:[mContainersTableView selectedRow]];
    [aFolder setIsDockMenu:YES];
  }
}

-(IBAction)addCollection:(id)aSender
{
  BookmarkFolder *aFolder = [mRootBookmarks addBookmarkFolder];
  [aFolder setTitle:NSLocalizedString(@"NewBookmarkFolder",@"New Folder")];
  unsigned int index = [mRootBookmarks indexOfObjectIdenticalTo:aFolder];
  [self selectContainer:index];
  [mContainersTableView editColumn:0 row:index withEvent:nil select:YES];
}

-(IBAction)addBookmarkSeparator:(id)aSender
{
  Bookmark *aBookmark = [[Bookmark alloc] init];
  [aBookmark setIsSeparator:YES];

  int index;
  BookmarkFolder *parentFolder = [self selectedItemFolderAndIndex:&index];

  [parentFolder insertChild:aBookmark atIndex:index isMove:NO];  
  [self selectItem:aBookmark expandingContainers:YES scrollIntoView:YES byExtendingSelection:NO];
  [aBookmark release];
}

-(IBAction)addBookmarkFolder:(id)aSender
{
  AddBookmarkDialogController* addBookmarkController = [AddBookmarkDialogController sharedAddBookmarkDialogController];

  int itemIndex;
  BookmarkFolder* parentFolder = [self selectedItemFolderAndIndex:&itemIndex];
  [addBookmarkController setDefaultParentFolder:parentFolder andIndex:itemIndex];
  [addBookmarkController setBookmarkViewController:self];

  [addBookmarkController showDialogWithLocationsAndTitles:nil isFolder:YES onWindow:[mBookmarksEditingView window]];
}

-(IBAction)deleteCollection:(id)aSender
{
  BookmarkManager *manager = [BookmarkManager sharedBookmarkManager];
  int index = [mContainersTableView selectedRow];
  if (index < (int)[manager firstUserCollection])
    return;
  [self selectContainer:(index - 1)];
  [[manager rootBookmarks] deleteChild:[[manager rootBookmarks] objectAtIndex:index]];
}

-(IBAction)deleteBookmarks: (id)aSender
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

  // create array of items we need to delete.
  NSArray* itemsToDelete = [mBookmarksOutlineView selectedItems];

  // delete all bookmarks that are in our array
  int count = [itemsToDelete count];
  for (int i = 0; i < count; i++) {
    doomedItem = [itemsToDelete objectAtIndex:i];
    [[doomedItem parent] deleteChild:doomedItem];
  }

  // restore selection to location near last item deleted or last item
  int total = [mBookmarksOutlineView numberOfRows];
  if (index >= total)
    index = total - 1;
  [mBookmarksOutlineView selectRow: index byExtendingSelection: NO];
}

-(IBAction)openBookmark: (id)aSender
{
  NSArray* items = nil;
  if ([aSender isKindOfClass:[BookmarkItem class]])
    items = [NSArray arrayWithObject:aSender];
  else
    items = [mBookmarksOutlineView selectedItems];

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    // see if it's a rendezvous item
    id parent = [curItem parent];
    if (![parent isKindOfClass:[BookmarkItem class]])
    {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[parent intValue] forSender:curItem];
      mOpenActionFlag = kOpenBookmarkAction;
    }
    else if ([curItem isKindOfClass:[BookmarkFolder class]])
    {
      if (![curItem isGroup])
      {
        if ([mBookmarksOutlineView isItemExpanded:curItem])
          [mBookmarksOutlineView collapseItem: curItem];
        else
          [mBookmarksOutlineView expandItem: curItem];
      }
    }
    else
    {
      // otherwise follow the standard bookmark opening behavior
      [[NSApp delegate] loadBookmark:curItem withWindowController:mBrowserWindowController openBehavior:eBookmarkOpenBehaviorDefault];
    }
  }
}

-(IBAction)openBookmarkInNewTab:(id)aSender
{
  NSArray* items = nil;
  if ([aSender isKindOfClass:[BookmarkItem class]])
    items = [NSArray arrayWithObject:aSender];
  else
    items = [mBookmarksOutlineView selectedItems];

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    // see if it's a rendezvous item
    id parent = [curItem parent];
    if (![parent isKindOfClass:[BookmarkItem class]])
    {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[parent intValue] forSender:curItem];
      mOpenActionFlag = kOpenInNewTabAction;
    }
    else
    {
      // otherwise follow the standard bookmark opening behavior
      [[NSApp delegate] loadBookmark:curItem withWindowController:mBrowserWindowController openBehavior:eBookmarkOpenBehaviorNewTabDefault];
    }
  }
}

-(IBAction)openBookmarksInTabsInNewWindow:(id)aSender
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
  while ((curItem = [itemsEnum nextObject]))
  {
    // see if it's a rendezvous item (this won't open in the new window, because we suck)
    id parent = [curItem parent];
    if (![parent isKindOfClass:[BookmarkItem class]])
    {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[parent intValue] forSender:curItem];
      mOpenActionFlag = kOpenInNewTabAction;
    }
    else
    {
      if ([curItem isKindOfClass:[Bookmark class]])
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

-(IBAction)openBookmarkInNewWindow:(id)aSender
{
  NSArray* items = nil;
  if ([aSender isKindOfClass:[BookmarkItem class]])
    items = [NSArray arrayWithObject:aSender];
  else
    items = [mBookmarksOutlineView selectedItems];

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    // see if it's a rendezvous item
    id parent = [curItem parent];
    if (![parent isKindOfClass:[BookmarkItem class]])
    {
      [[NetworkServices sharedNetworkServices] attemptResolveService:[parent intValue] forSender:curItem];
      mOpenActionFlag = kOpenInNewWindowAction;
    }
    else
    {
      // otherwise follow the standard bookmark opening behavior
      [[NSApp delegate] loadBookmark:curItem withWindowController:mBrowserWindowController openBehavior:eBookmarkOpenBehaviorNewWindowDefault];
    }
  }
}

-(IBAction)showBookmarkInfo:(id)aSender
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController];
  BookmarkItem* item = nil;

  int index = [mBookmarksOutlineView selectedRow];
  item = [mBookmarksOutlineView itemAtRow: index];

  [bic setBookmark:item];
  [bic showWindow:bic];
}

-(IBAction) locateBookmark:(id)aSender
{
  // XXX use to go from quicksort to tree view?
#if 0
  int index = [mSearchPane selectedRow];
  if (index == -1)
    return;
#endif 
  BookmarkItem *item = nil; //  = [mSearchResultArray objectAtIndex:index];
  [self displayBookmarkInOutlineView:item];
  [mBookmarksOutlineView selectRow:[mBookmarksOutlineView rowForItem:item] byExtendingSelection:NO];
}

-(IBAction) cut:(id)aSender
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
// If no items are selected in the view : at the end of the bookmark menu folder
// If a folder is selected: at the end of that folder
// If a bookmark is selected: immediately after that bookmark, under the same parent
// XXX: At the moment if multiple items are selected we only examine the first one
//
-(IBAction) paste:(id)aSender
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
    } else if ([item isKindOfClass:[Bookmark class]]) {
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
  if ([types containsObject: kCaminoBookmarkListPBoardType])
    [self pasteBookmarks:[NSPasteboard generalPasteboard] intoFolder:pasteDestinationFolder index:pasteDestinationIndex copying:YES];
  else if ([[NSPasteboard generalPasteboard] containsURLData])
    [self pasteBookmarksFromURLsAndTitles:[NSPasteboard generalPasteboard] intoFolder:pasteDestinationFolder index:pasteDestinationIndex];
}

-(IBAction) delete:(id)aSender
{
  [self deleteBookmarks:aSender];
}

-(IBAction)quicksearchPopupChanged:(id)aSender
{
  // do the search again (we'll pick up the new popup item tag)
  NSString* currentText = [mSearchField stringValue];
  [self searchStringChanged:currentText];
}

- (void)resetSearchField
{
  [mSearchField selectPopupMenuItem:[[mSearchField popupMenu] itemWithTag:1]];   // select the "all" item
  [mSearchField setStringValue:@""];
}

-(void)setBrowserWindowController:(BrowserWindowController*)bwController
{
  // don't retain
  mBrowserWindowController = bwController;
}

-(void) displayBookmarkInOutlineView:(BookmarkItem *)aBookmarkItem
{
  if (!aBookmarkItem) return;   // avoid recursion
  BookmarkItem *parent = [aBookmarkItem parent];
  if (parent != mRootBookmarks)
    [self displayBookmarkInOutlineView:parent];
  else {
    int index = [mRootBookmarks indexOfObject:aBookmarkItem];
    [mContainersTableView selectRow:index byExtendingSelection:NO];
    [self selectContainer:index];
    return;
  }
  [mBookmarksOutlineView expandItem:aBookmarkItem];
}

-(NSView*)bookmarksEditingView
{
  return mBookmarksEditingView;
}

- (void)focus
{
  [[mBookmarksOutlineView window] makeFirstResponder:mBookmarksOutlineView];
  
  // restore splitters to their saved positions. We have to do this here
  // (rather than in |-completeSetup| because only at this point is the
  // manager view resized correctly. If we did it earlier, it would resize again
  // to stretch proportionally to the size of the browser window, destroying 
  // the width we just set.
  if (!mSplittersRestored)
  {
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

- (void) setCanEditSelectedContainerContents:(BOOL)inCanEdit
{
  [mBookmarksOutlineView setAllowsEditing:inCanEdit];
  // XXX update buttons
//  [mAddBookmarkButton setEnabled:inCanEdit];
//  [mAddFolderButton setEnabled:inCanEdit];
  // if editable and something is selected, then enable get info button, otherwise disable it
  //[mInfoButton setEnabled:(inCanEdit && ([mBookmarksOutlineView numberOfSelectedRows] == 1))];
}

-(void) setActiveCollection:(BookmarkFolder *)aFolder
{
  [aFolder retain];
  [mActiveRootCollection release];
  mActiveRootCollection = aFolder;
}

-(BookmarkFolder *)activeCollection
{
  return mActiveRootCollection;
}

-(BookmarkFolder *)selectedItemFolderAndIndex:(int*)outIndex
{
  BookmarkFolder *parentFolder = nil;
  *outIndex = 0;

  if ([mBookmarksOutlineView numberOfSelectedRows] == 1)
  {
    int row = [mBookmarksOutlineView selectedRow];
    BookmarkItem *item = [mBookmarksOutlineView itemAtRow:row];
    // if it's a folder, use it
    if ([item isKindOfClass:[BookmarkFolder class]])
    {
      *outIndex = (int)[(BookmarkFolder*)item count];
      return item;
    }
    
    // otherwise use its parent
    if ([item respondsToSelector:@selector(parent)])    // when would it not?
    {
      parentFolder = [item parent];
      *outIndex = [parentFolder indexOfObject:item] + 1;
    }
  }

  if (!parentFolder)
  {
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

-(void)revealItem:(BookmarkItem*)item scrollIntoView:(BOOL)inScroll selecting:(BOOL)inSelectItem byExtendingSelection:(BOOL)inExtendSelection
{
  BookmarkManager* bmManager = [BookmarkManager sharedBookmarkManager];
  
  if ([item hasAncestor:[bmManager bookmarkMenuFolder]])
    [self selectContainer:kBookmarkMenuContainerIndex];
  else if ([item hasAncestor:[bmManager toolbarFolder]])
    [self selectContainer:kToolbarContainerIndex];
  else if ([item parent] == [bmManager rootBookmarks])
  {
    int itemRow = [[bmManager rootBookmarks] indexOfObject:item];
    [mContainersTableView selectRow:itemRow byExtendingSelection:NO];
    return;
  }

  [self expandAllParentsOfItem:item];
  
  int itemRow = [mBookmarksOutlineView rowForItem:item];
  if (itemRow == -1) return;

  if (inSelectItem)
    [mBookmarksOutlineView selectRow:itemRow byExtendingSelection:NO];
  [mBookmarksOutlineView scrollRowToVisible:itemRow];
}

- (void)expandAllParentsOfItem:(BookmarkItem*)inItem
{
  // make an array of parents
  NSMutableArray* parentArray = [[NSMutableArray alloc] initWithCapacity:10];
  
  id curItem = [inItem parent];
  while (curItem)
  {
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
  while (curRow < [mBookmarksOutlineView numberOfRows])
  {
    id item = [mBookmarksOutlineView itemAtRow:curRow];
    if ([item isKindOfClass:[BookmarkFolder class]])
    {
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
  if (!mExpandedStates)
  {
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
}

-(void)pasteBookmarks:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder *)dropFolder index:(int)index copying:(BOOL)isCopy
{
  NSArray* mozBookmarkList = [BookmarkManager bookmarkItemsFromSerializableArray:[aPasteboard propertyListForType: kCaminoBookmarkListPBoardType]];

  NSMutableArray* newBookmarks = [[NSMutableArray alloc] initWithCapacity:[mozBookmarkList count]];
  if (!isCopy)
    [newBookmarks addObjectsFromArray:mozBookmarkList];
  
  // turn off updates to avoid lots of reloadData with multiple items
  mBookmarkUpdatesDisabled = YES;
  
  // make sure we re-enable updates
  NS_DURING
    NSEnumerator *enumerator = [mozBookmarkList objectEnumerator];

    id aKid;
    while ((aKid = [enumerator nextObject]))
    {
      if (isCopy)
      {
        BookmarkItem* newItem = [[aKid parent] copyChild:aKid toBookmarkFolder:dropFolder atIndex:index];
        [newBookmarks addObject:newItem];
        ++index;
      }
      else
      {
        // need to be careful to adjust index as we insert items to avoid
        // inserting in reverse order
        if ([aKid parent] == (id)dropFolder)
        {
          int kidIndex = [dropFolder indexOfObject:aKid];
          [[aKid parent] moveChild:aKid toBookmarkFolder:dropFolder atIndex:index];
          if (kidIndex > index)
            ++index;
        }
        else
        {
          [[aKid parent] moveChild:aKid toBookmarkFolder:dropFolder atIndex:index];
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

-(void)pasteBookmarksFromURLsAndTitles:(NSPasteboard*)aPasteboard intoFolder:(BookmarkFolder *)dropFolder index:(int)index
{
  NSArray* urls = nil;
  NSArray* titles = nil;
  
  [aPasteboard getURLs:&urls andTitles:&titles];

  // turn off updates to avoid lots of reloadData with multiple items
  mBookmarkUpdatesDisabled = YES;
  
  NSMutableArray* newBookmarks = [NSMutableArray arrayWithCapacity:[urls count]];
  // make sure we re-enable updates
  NS_DURING
    for ( unsigned int i = 0; i < [urls count]; ++i ) {
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

//
// -doDrop:intoFolder:index:
//
// called when a drop occurs on a table or outline to do the actual work based on the
// data types present in the drag info.
//
-(BOOL) doDrop:(id <NSDraggingInfo>)info intoFolder:(BookmarkFolder *)dropFolder index:(int)index
{
  NSArray* types  = [[info draggingPasteboard] types];
  BOOL isCopy = ([info draggingSourceOperationMask] == NSDragOperationCopy);

  if ([types containsObject: kCaminoBookmarkListPBoardType])
  {
    [self pasteBookmarks:[info draggingPasteboard] intoFolder:dropFolder index:index copying:isCopy];
    return YES;
  }
  
  if ([[info draggingPasteboard] containsURLData])
  {
    [self pasteBookmarksFromURLsAndTitles:[info draggingPasteboard] intoFolder:dropFolder index:index];
    return YES;
  }
  return NO;  
}

// Choose a single drag operation to return based on a provided mask and the operations that table view/outline view support.
-(NSDragOperation) preferredDragOperationForSourceMask:(NSDragOperation)srcMask
{
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
- (void) copyBookmarks:(NSArray*)bookmarkItemsToCopy toPasteboard:(NSPasteboard*)aPasteboard
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
  for ( unsigned int i = 0; i < [bookmarkItemsToCopy count]; ++i ) {
    BookmarkItem* bookmarkItem = [bookmarkItemsToCopy objectAtIndex:i];
    if ([bookmarkItem isKindOfClass:[Bookmark class]]) {
      Bookmark* bookmark = (Bookmark*) bookmarkItem;
      [urlList addObject:[bookmark url]];
      [titleList addObject:[bookmark title]];
    }
  }
  [aPasteboard setURLs:urlList withTitles:titleList];
}


-(BOOL) canPasteFromPasteboard:(NSPasteboard*)aPasteboard
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

- (void)selectContainer:(int)inRowIndex
{
  [mContainersTableView selectRow:inRowIndex byExtendingSelection:NO];

  // reset the search
  [self resetSearchField];

  if (inRowIndex == kHistoryContainerIndex)
  {
    [self setActiveOutlineView:mHistoryOutlineView];

    [mHistoryOutlineViewDelegate clearSearchResults];
    [mHistoryOutlineViewDelegate historyViewMadeVisible:YES];
    
    [mActionButton setMenu:mActionMenuHistory];
    [mSortButton   setMenu:mSortMenuHistory];
    [mSearchField  setPopupMenu:mQuickSearchMenuHistory];
    [mSearchField  selectPopupMenuItem:[[mSearchField popupMenu] itemWithTag:1]];   // select the "all" item
    
    [[mBookmarksEditingView window] setTitle:NSLocalizedString(@"HistoryWindowTitle", @"")];
  } 
  else
  {
    [self setActiveOutlineView:mBookmarksOutlineView];

    [mHistoryOutlineViewDelegate historyViewMadeVisible:NO];

    BookmarkFolder *activeCollection = [mRootBookmarks objectAtIndex:inRowIndex];
    [self setActiveCollection:activeCollection];
    [self clearSearchResults];

    if ([activeCollection isSmartFolder])
      [self setCanEditSelectedContainerContents:NO];
    else
      [self setCanEditSelectedContainerContents:YES];

    // if its a smart folder, but not the history folder
    if ([[self activeCollection] isSmartFolder])
      [mBookmarksOutlineView setDeleteAction:nil];
    else
      [mBookmarksOutlineView setDeleteAction:@selector(deleteBookmarks:)];

    [mActionButton setMenu:mActionMenuBookmarks];
    [mSortButton   setMenu:mSortMenuBookmarks];
    [mSearchField  setPopupMenu:mQuickSearchMenuBookmarks];
    [mSearchField  selectPopupMenuItem:[[mSearchField popupMenu] itemWithTag:1]];   // select the "all" item

    [[mBookmarksEditingView window] setTitle:NSLocalizedString(@"BookmarksWindowTitle", @"")];

    // this reload ensures that we display the newly selected activeCollection 
    [mBookmarksOutlineView reloadData];
    // after we've reloaded data, restore twisty states
    [self restoreFolderExpandedStates];
  }
}

- (void)selectLastContainer
{
  int curRow = [mContainersTableView selectedRow];
  curRow = (curRow != -1) ? curRow : 0;
  [self selectContainer:curRow];
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
  if ( tableView == mContainersTableView )
    return [mRootBookmarks count];

  return 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
  id retValue = nil;
  id item = nil;

  if ( tableView == mContainersTableView ) 
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
  if ( inTableView == mContainersTableView ) {
    BookmarkFolder *aFolder = [mRootBookmarks objectAtIndex:inRowIndex];
    [inCell setImage:[aFolder icon]];
  }
}

- (BOOL)tableView:(NSTableView *)aTableView shouldEditTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  if (aTableView == mContainersTableView) {
    if (rowIndex >= (int)[[BookmarkManager sharedBookmarkManager] firstUserCollection])
      return YES;
    return NO;
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
  NSMutableArray *itemArray = [[NSMutableArray alloc] initWithCapacity:count];
  BookmarkManager *manager = [BookmarkManager sharedBookmarkManager];
  NSEnumerator *enumerator = [rows objectEnumerator];
  unsigned firstUserCollection = [manager firstUserCollection];
  int rowVal;
  id aRow;
  while ((aRow = [enumerator nextObject])) {
    rowVal = [aRow intValue];
    if (rowVal >= (int)firstUserCollection)
      [itemArray addObject:[mRootBookmarks objectAtIndex:rowVal]];
  }
  if ([itemArray count] == 0) {
    [itemArray release];
    return NO;
  }
  
  [self copyBookmarks:itemArray toPasteboard:pboard];
  [itemArray release];
  return YES;
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
    NSDragOperation dragOp = [self preferredDragOperationForSourceMask:[info draggingSourceOperationMask]];
    // figure out where we want to drop. |dropFolder| will either be a container or
    // the top-level bookmarks root if we're to create a new container.
    BookmarkManager *manager = [BookmarkManager sharedBookmarkManager];
    BookmarkFolder *dropFolder = nil;
    if ((row < 2) && (op == NSTableViewDropOn))
      dropFolder = [mRootBookmarks objectAtIndex:row];
    else if (row >= (int)[manager firstUserCollection]) {
      if (op == NSTableViewDropAbove)
        dropFolder = mRootBookmarks;
      else
        dropFolder = [mRootBookmarks objectAtIndex:row];
    }
    if (dropFolder) {
      // special check if we're moving pointers around
      if ([types containsObject:kCaminoBookmarkListPBoardType])
      {
        NSArray *draggedItems = [BookmarkManager bookmarkItemsFromSerializableArray:[[info draggingPasteboard] propertyListForType: kCaminoBookmarkListPBoardType]];
        BOOL isOK = [manager isDropValid:draggedItems toFolder:dropFolder];
        return (isOK) ? dragOp : NSDragOperationNone;
      }
      else if ([[info draggingPasteboard] containsURLData])
      {
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
  [self selectContainer:[mContainersTableView selectedRow]];
  return result;
}

-(void)tableViewSelectionDidChange:(NSNotification *)note
{
  NSTableView *aView = [note object];
  if (aView == mContainersTableView) {
    [self selectContainer:[aView selectedRow]];
  }
}

-(NSMenu *)tableView:(NSTableView *)aTableView contextMenuForRow:(int)rowIndex
{
  if (aTableView == mContainersTableView)
  {
    NSMenu *contextMenu = [[[aTableView menu] copy] autorelease];
    if ([aTableView numberOfSelectedRows] > 0)
    {
      [contextMenu addItem:[NSMenuItem separatorItem]];
      NSMenuItem *useAsDockItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Use as Dock Menu", @"Use as Dock Menu")
                                                             action:@selector(setAsDockMenuFolder:)
                                                      keyEquivalent:@""];
      [useAsDockItem setTarget:self];
      [contextMenu addItem:useAsDockItem];
      [useAsDockItem release];
      if (rowIndex >= (int)[[BookmarkManager sharedBookmarkManager] firstUserCollection])
      {
        NSMenuItem *deleteItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Delete", @"Delete")
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
  if (outlineView == mBookmarksOutlineView)
  {
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
  if ([[tableColumn identifier] isEqualToString: @"title"])
    [cell setImage:[item icon]];
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
  if ((count == 0) || [mActiveRootCollection isSmartFolder])
    return NO;

  // Pack pointers to bookmark items into this array.
  [self copyBookmarks:items toPasteboard:pboard];
  return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView*)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
  NSArray* types = [[info draggingPasteboard] types];
  NSDragOperation dragOp = [self preferredDragOperationForSourceMask:[info draggingSourceOperationMask]];

  //  if the index is -1, deny the drop
  if (index == NSOutlineViewDropOnItemIndex)
    return NSDragOperationNone;

  if ([types containsObject: kCaminoBookmarkListPBoardType]) {
    NSArray *draggedItems = [BookmarkManager bookmarkItemsFromSerializableArray:[[info draggingPasteboard] propertyListForType: kCaminoBookmarkListPBoardType]];
    BookmarkFolder* parent = (item) ? item : [self itemTreeRootContainer];
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
      return [NSString stringWithFormat:@"%@\n%@",[item url], [item itemDescription]];
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
  return [[BookmarkManager sharedBookmarkManager] contextMenuForItems:items fromView:outlineView target:self];
}

- (BOOL)outlineView:(NSOutlineView*)inOutlineView columnHasIcon:(NSTableColumn*)inColumn
{
  BOOL hasIcon = NO;
  if ([[inColumn identifier] isEqualToString:@"title"])
    hasIcon = YES;
  return hasIcon;
}

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren
{
  if (mBookmarkUpdatesDisabled)
    return;

  if (!item)
    [mBookmarksOutlineView reloadData];
  else
    [mBookmarksOutlineView reloadItem: item reloadChildren: aReloadChildren];
}

- (int)numberOfSelectedRows
{
  return [mBookmarksOutlineView numberOfSelectedRows];
}

- (BOOL)haveSelectedRow
{
  return ([mBookmarksOutlineView selectedRow] != -1);
}

-(void)outlineViewSelectionDidChange: (NSNotification*) aNotification
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController];
  if ([mBookmarksOutlineView numberOfSelectedRows] == 1) {
    //[mInfoButton setEnabled:YES];
    if ([[bic window] isVisible]) {
      [bic setBookmark:[mBookmarksOutlineView itemAtRow:[mBookmarksOutlineView selectedRow]]];
    }
  }
  else {
    //[mInfoButton setEnabled:NO];
    [bic close];
  }
}

- (BOOL)validateMenuItem:(id <NSMenuItem>)menuItem
{
  SEL action = [menuItem action];

  if ([self activeOutlineView] == mBookmarksOutlineView)
  {
    if (action == @selector(addBookmarkSeparator:))
    {
      BookmarkFolder *activeCollection = [self activeCollection];
      return (![activeCollection isRoot] && ![activeCollection isSmartFolder]);
    }

    BookmarkItem* selItem = [mBookmarksOutlineView itemAtRow:[mBookmarksOutlineView selectedRow]];

    if (action == @selector(openBookmark:))
      return (selItem != nil);

    if (action == @selector(openBookmarkInNewTab:))
      return (selItem != nil);

    if (action == @selector(openBookmarkInNewWindow:))
      return (selItem != nil);

    if (action == @selector(deleteBookmarks:))
      return (selItem != nil);

    if (action == @selector(showBookmarkInfo:))
      return (selItem != nil);
  }
  else    // history visible
  {
    if (action == @selector(addBookmark:))
      return NO;

    if (action == @selector(addFolder:))
      return NO;

    if (action == @selector(addBookmarkSeparator:))
      return NO;
  
  }
  return YES;
}


/*
-(BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  int  index = [mBookmarksOutlineView selectedRow];
  BOOL haveSelection = (index != -1);
  BOOL multiSelection = ([mBookmarksOutlineView numberOfSelectedRows] > 1);
  BOOL isBookmark = NO;
  BOOL isToolbar = NO;
  BOOL isGroup = NO;

  id item = nil;

  if (haveSelection)
    item = [mBookmarksOutlineView itemAtRow: index];
  if ([item isKindOfClass:[Bookmark class]])
    isBookmark = YES;
  else if ([item isKindOfClass:[BookmarkFolder class]]) {
    isGroup = [item isGroup];
    isToolbar = [item isToolbar];
  }

  // Bookmarks and Bookmark Groups can be opened in a new window
  if (([aMenuItem action] == @selector(openBookmarkInNewWindow:)))
    return (isBookmark || isGroup);

  // Only Bookmarks can be opened in new tabs
  if (([aMenuItem action] == @selector(openBookmarkInNewTab:)))
    return isBookmark && [mBrowserWindowController newTabsAllowed];

  if (([aMenuItem action] == @selector(showBookmarkInfo:)))
    return haveSelection;

  if (([aMenuItem action] == @selector(deleteBookmarks:)))
    return (multiSelection || (haveSelection && !isToolbar));

  if (([aMenuItem action] == @selector(addFolder:)))
    return YES;

  return YES;
}
*/

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

// called when the user typed into the quicksearch field, or edits an item inline
- (void)controlTextDidChange:(NSNotification *)aNotification
{
  if ([aNotification object] == mSearchField)
  {
    NSString* currentText = [mSearchField stringValue];
    [self searchStringChanged:currentText];
  }
}

- (void)searchStringChanged:(NSString*)searchString
{
  if ([searchString length] == 0)
  {
    [self clearSearchResults];
    [[self activeOutlineView] reloadData];
  }
  else
  {
    [self searchFor:searchString inFieldWithTag:[[mSearchField selectedPopupMenuItem] tag]];
    [[self activeOutlineView] reloadData];
  }
}

- (void)searchFor:(NSString*)searchString inFieldWithTag:(int)tag
{
  if ([self activeOutlineView] == mHistoryOutlineView)
    [mHistoryOutlineViewDelegate searchFor:searchString inFieldWithTag:tag];
  else
  {
    BookmarkFolder* searchRoot = [self activeCollection];
    NSArray* searchResults = [[BookmarkManager sharedBookmarkManager] searchBookmarksContainer:searchRoot forString:searchString inFieldWithTag:tag];
    [self setSearchResultArray:searchResults];
  }
}

- (void)clearSearchResults
{
  if ([self activeOutlineView] == mHistoryOutlineView)
  {
    [mHistoryOutlineViewDelegate clearSearchResults];
  }
  else
  {
    [mSearchResultArray release];
    mSearchResultArray = nil;
  }
}

- (void)selectItems:(NSArray*)items expandingContainers:(BOOL)expandContainers scrollIntoView:(BOOL)scroll
{
  NSEnumerator* itemsEnum = [items objectEnumerator];
  [mBookmarksOutlineView deselectAll:nil];
  BookmarkItem* item;
  while ((item = [itemsEnum nextObject]))
  {
    [self selectItem:item expandingContainers:expandContainers scrollIntoView:scroll byExtendingSelection:YES];
  }
}

// share code with revealItem:selecting:
- (void)selectItem:(BookmarkItem*)item expandingContainers:(BOOL)expandContainers scrollIntoView:(BOOL)scroll byExtendingSelection:(BOOL)extendSelection
{
  [self revealItem:item scrollIntoView:scroll selecting:YES byExtendingSelection:extendSelection];
#if 0
  int itemRow = [mBookmarksOutlineView rowForItem:item];
  if (itemRow == -1)
  {
    if (!expandContainers)
      return;
    
    // expanding isn't implemented yet
    return;
  }

  [mBookmarksOutlineView selectRow:itemRow byExtendingSelection:extendSelection];
  if (scroll)
    [mBookmarksOutlineView scrollRowToVisible:itemRow];
#endif
}

- (id)itemTreeRootContainer
{
  if (mSearchResultArray)
    return mSearchResultArray;
  
  return [self activeCollection];
}

- (void)setActiveOutlineView:(NSOutlineView*)outlineView
{
  if (outlineView == mBookmarksOutlineView)
  {
    [mOutlinerHostView swapFirstSubview:mBookmarksHostView];
    [mAddCollectionButton setNextKeyView:mBookmarksOutlineView];
    [mBookmarksOutlineView setNextKeyView:mAddButton];
  }
  else
  {
    [mOutlinerHostView swapFirstSubview:mHistoryHostView];
    [mAddCollectionButton setNextKeyView:mHistoryOutlineView];
    [mHistoryOutlineView setNextKeyView:mAddButton];
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
  if ([self activeOutlineView] == mHistoryOutlineView)
  {
    NSArray* selectedItems = [mHistoryOutlineView selectedItems];
    if ([selectedItems count] > 0)
      actionMenu = [mHistoryOutlineViewDelegate outlineView:mHistoryOutlineView contextMenuForItems:selectedItems];
    else
      actionMenu = mActionMenuHistory;
  }
  else
  {
    NSArray* selectedBMs = [mBookmarksOutlineView selectedItems];
    if ([selectedBMs count] > 0)
      actionMenu = [[BookmarkManager sharedBookmarkManager] contextMenuForItems:selectedBMs fromView:mBookmarksOutlineView target:self];
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
  if (mOpenActionFlag == kNoOpenAction)
    return;
  NSDictionary *dict = [note userInfo];
  id aClient = [dict objectForKey:NetworkServicesClientKey];
  if ([aClient isKindOfClass:[Bookmark class]]) {
    switch (mOpenActionFlag) {
      case (kOpenBookmarkAction):
        [[NSRunLoop currentRunLoop] performSelector:@selector(openBookmark:) target:self argument:aClient order:10 modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
        break;
      case (kOpenInNewTabAction):
        [[NSRunLoop currentRunLoop] performSelector:@selector(openBookmarkInNewTab:) target:self argument:aClient order:10 modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
        break;
      case (kOpenInNewWindowAction):
        [[NSRunLoop currentRunLoop] performSelector:@selector(openBookmarkInNewWindow:) target:self argument:aClient order:10 modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
        break;
      default:
        break;
    }
    mOpenActionFlag = kNoOpenAction;
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
  if ((addedItem == [[BookmarkManager sharedBookmarkManager] rootBookmarks]))
  {
    [mContainersTableView reloadData];
    BookmarkFolder *updatedFolder = [[note userInfo] objectForKey:BookmarkFolderChildKey];
    [self selectContainer:[(BookmarkFolder*)addedItem indexOfObjectIdenticalTo:updatedFolder]];
    return;
  }
  
  if (addedItem == mActiveRootCollection)
    addedItem = nil;

  [self reloadDataForItem:addedItem reloadChildren:YES];
}

- (void)bookmarkRemoved:(NSNotification *)note
{
  BookmarkItem* removedItem = [note object];

  if (removedItem == mItemToReveal)
  {
    [mItemToReveal autorelease];
    mItemToReveal = nil;
  }
  
  if ((removedItem == [[BookmarkManager sharedBookmarkManager] rootBookmarks]))
  {
    [mContainersTableView reloadData];
    return;
  }
  
  if (removedItem == mActiveRootCollection)
    removedItem = nil;

  [self reloadDataForItem:removedItem reloadChildren:YES];
}

- (void)bookmarkChanged:(NSNotification *)note
{
  [self reloadDataForItem:[note object] reloadChildren:NO];
}

- (void)bookmarksViewDidMoveToWindow:(NSWindow*)inWindow
{
  // we're leaving the window, so...
  if (!inWindow)
  {
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
  if ( sender == mContainersSplit )
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
    [self selectContainer:kHistoryContainerIndex];
  else
    [self selectContainer:kBookmarkMenuContainerIndex];

  if (mItemToReveal)
  {
    [self revealItem:mItemToReveal scrollIntoView:YES selecting:YES byExtendingSelection:NO];
    [mItemToReveal release];
    mItemToReveal = nil;
  }

  return mBookmarksEditingView;
}

- (void)contentView:(NSView*)inView usedForURL:(NSString*)inURL
{
  if (inView == mBookmarksEditingView)
  {
    [self focus];
  }
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

