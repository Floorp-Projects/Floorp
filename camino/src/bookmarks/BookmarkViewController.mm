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
*    Simon Fraser <sfraser@netscape.com>
*    Max Horn <max@quendi.de>
*    David Haas <haasd@cae.wisc.edu>
*    Simon Woodside <sbwoodside@yahoo.com>
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

#import "BookmarkViewController.h"
#import "NSString+Utils.h"
#import "NSArray+Utils.h"
#import "NSPasteboard+Utils.h"
#import "NSSplitView+Utils.h"
#import "BookmarkManager.h"
#import "BookmarkInfoController.h"
#import "BookmarkFolder.h"
#import "Bookmark.h"
#import "MainController.h"
#import "BrowserWindowController.h"
#import "PreferenceManager.h"
#import "ImageAndTextCell.h"
#import "ExtendedTableView.h"
#import "BookmarkOutlineView.h"
#import "HistoryDataSource.h"
#import "BookmarksClient.h"
#import "NetworkServices.h"
#import "UserDefaults.h"

#define kNoOpenAction 0
#define kOpenBookmarkAction 1
#define kOpenInNewTabAction 2
#define kOpenInNewWindowAction 3

#define kGetInfoContextMenuItemTag 9

// minimum sizes for the search panel
const long kMinContainerSplitWidth = 150;
const long kMinSearchPaneHeight = 80;

// The actual constant defined in 10.3.x and greater headers is NSTableViewSolidVerticalGridLineMask.
// In order to compile with 10.2.x, the value has just been extracted and put here.
// It is extremely unlikely that Apple will change it.
static unsigned int TableViewSolidVerticalGridLineMask = 1;

@interface BookmarkViewController (Private) <BookmarksClient, NetworkServicesClient>
-(void) setSearchResultArray:(NSArray *)anArray;
-(void) displayBookmarkInOutlineView:(BookmarkItem *)aBookmarkItem;
-(BOOL) doDrop:(id <NSDraggingInfo>)info intoFolder:(BookmarkFolder *)dropFolder index:(int)index;
@end

@implementation BookmarkViewController

- (id)init
{
  if (self = [super init]) {
    mCachedHref = nil;
    mRootBookmarks = nil;
    mActiveRootCollection = nil;
    mExpandedStatus = nil;
    mSearchResultArray = nil;
    mOpenActionFlag = 0;
    mSplittersRestored = NO;
    
    // wait for |-completeSetup| to be called to lazily complete our setup
    mSetupComplete = NO;
  }
  return self;
}

- (void)dealloc
{
  // save the splitter width of the conatiner view
  float width = [mContainersSplit leftWidth]; 
  [[NSUserDefaults standardUserDefaults] setFloat: width forKey:USER_DEFAULTS_CONTAINER_SPLITTER_WIDTH];

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mCachedHref release];
  [mExpandedStatus release];
  [mActiveRootCollection release];
  [mRootBookmarks release];
  [mSearchResultArray release];
  [super dealloc];
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

- (void)completeSetup
{
  [self setSearchResultArray:[NSMutableArray array]];
  // the standard table item doesn't handle text and icons. Replace it
  // with a custom cell that does.
  ImageAndTextCell* imageAndTextCell = [[[ImageAndTextCell alloc] init] autorelease];
  [imageAndTextCell setEditable: YES];
  [imageAndTextCell setWraps: NO];
  NSTableColumn* itemNameColumn = [mItemPane tableColumnWithIdentifier: @"title"];
  [itemNameColumn setDataCell:imageAndTextCell];
  NSTableColumn* containerNameColumn = [mContainerPane tableColumnWithIdentifier: @"title"];
  [containerNameColumn setDataCell:imageAndTextCell];
  NSTableColumn* searchNameColumn = [mSearchPane tableColumnWithIdentifier: @"title"];
  [searchNameColumn setDataCell:imageAndTextCell];

  // set up the table appearance for item and search views
  if ([mItemPane respondsToSelector:@selector(setUsesAlternatingRowBackgroundColors:)]) {
    [mItemPane setUsesAlternatingRowBackgroundColors:YES];
    [mSearchPane setUsesAlternatingRowBackgroundColors:YES];
    // if it responds to the above selector, then it will respond to this too...
    [mItemPane setGridStyleMask:TableViewSolidVerticalGridLineMask];
    [mSearchPane setGridStyleMask:TableViewSolidVerticalGridLineMask];
  }
  
  // set up the font on the item & search views to be smaller
  // also don't let the cells draw their backgrounds
  NSArray* columns = [mItemPane tableColumns];
  if (columns) {
    int numColumns = [columns count];
    NSFont* smallerFont = [NSFont systemFontOfSize:11];
    for (int i = 0; i < numColumns; i++) {
      [[[columns objectAtIndex:i] dataCell] setFont:smallerFont];
      [[[columns objectAtIndex:i] dataCell] setDrawsBackground:NO];
    }
  }
  columns = [mSearchPane tableColumns];
  if (columns) {
    int numColumns = [columns count];
    NSFont* smallerFont = [NSFont systemFontOfSize:11];
    for (int i = 0; i < numColumns; i++) {
      [[[columns objectAtIndex:i] dataCell] setFont:smallerFont];
      [[[columns objectAtIndex:i] dataCell] setDrawsBackground:NO];
    }
  }

  // Generic notifications for Bookmark Client
  NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
  [nc addObserver:self selector:@selector(bookmarkAdded:) name:BookmarkFolderAdditionNotification object:nil];
  [nc addObserver:self selector:@selector(bookmarkRemoved:) name:BookmarkFolderDeletionNotification object:nil];
  [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkItemChangedNotification object:nil];
  [nc addObserver:self selector:@selector(bookmarkChanged:) name:BookmarkIconChangedNotification object:nil];
  [nc addObserver:self selector:@selector(serviceResolved:) name:NetworkServicesResolutionSuccess object:nil];

  // register for notifications of when the BM manager starts up. Since it does it on a separate thread,
  // it can be created after we are and if we don't update ourselves, the bar will be blank. This
  // happens most notably when the app is launched with a 'odoc' or 'GURL' appleEvent.
  [nc addObserver:self selector:@selector(managerStarted:) name:[BookmarkManager managerStartedNotification] object:nil];

  // register for dragged types
  [mContainerPane registerForDraggedTypes:[NSArray arrayWithObjects:@"MozBookmarkType",@"MozURLType", NSURLPboardType, NSStringPboardType, nil]];

  [mSearchButton setEnabled:NO];
  [self ensureBookmarks];

  // these should be settable in the nib.  however, whenever
  // I try, they disappear as soon as I've saved.  Very annoying.
  [mItemPane setAutosaveName:@"BMOutlineView"];
  [mSearchPane setAutosaveName:@"BMSearchView"];
  [mContainerPane setAutosaveName:@"BMContainerView"];
  [mItemPane setAutosaveTableColumns:YES];
  [mSearchPane setAutosaveTableColumns:YES];
  [mContainerPane setAutosaveTableColumns:YES];

  // if we're on 10.2+, set the search field to be rounded
  if ([mSearchField respondsToSelector:@selector(setBezelStyle:)])
    [mSearchField setBezelStyle:NSTextFieldRoundedBezel];
  
  mSetupComplete = YES;
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
  if (!mRootBookmarks) {
    BookmarkFolder* manager = [[BookmarkManager sharedBookmarkManager] rootBookmarks];
    if (![manager count])     // not initialized yet, try again later (from start notifiation)
      return;

    mRootBookmarks = [manager retain];
    [mContainerPane setTarget:self];
    [mContainerPane setDeleteAction:@selector(deleteCollection:)];
    [mContainerPane reloadData];
    [mItemPane setTarget: self];
    [mItemPane setDoubleAction: @selector(openBookmark:)];
    [mItemPane setDeleteAction: @selector(deleteBookmarks:)];
    [mItemPane reloadData];
    [mSearchPane setTarget: self];
    [mSearchPane setDoubleAction: @selector(openBookmark:)];
    [self restoreFolderExpandedStates];
  }
}

-(void) setSearchResultArray:(NSArray *)anArray
{
  [anArray retain];
  [mSearchResultArray release];
  mSearchResultArray = anArray;
  [mSearchPane reloadData];
}

//
// IBActions
//

- (IBAction) setAsDockMenuFolder:(id)aSender
{
  int rowIndex = [mContainerPane selectedRow];
  if (rowIndex >= 0) {
    BookmarkFolder *aFolder = [mRootBookmarks objectAtIndex:[mContainerPane selectedRow]];
    [aFolder setIsDockMenu:YES];
  }
}

-(IBAction)addBookmark:(id)aSender
{
  [self addItem: aSender isFolder: NO URL:nil title:nil];
}

-(IBAction)addFolder:(id)aSender
{
  [self addItem: aSender isFolder: YES URL:nil title:nil];
}

-(IBAction)addCollection:(id)aSender
{
  BookmarkFolder *aFolder = [mRootBookmarks addBookmarkFolder];
  [aFolder setTitle:NSLocalizedString(@"NewBookmarkFolder",@"New Folder")];
  unsigned index = [mRootBookmarks indexOfObjectIdenticalTo:aFolder];
  [self selectContainer:index];
  [mContainerPane editColumn:0 row:index withEvent:nil select:YES];
}

-(IBAction) addSeparator:(id)aSender;
{
  Bookmark *aBookmark = [[Bookmark alloc] init];
  [aBookmark setIsSeparator:YES];
  [[[BookmarkManager sharedBookmarkManager] bookmarkMenuFolder] insertChild:aBookmark];
}

-(void)addItem:(id)aSender isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle
{
  // We ALWAYS use the selected item to determine the parent.
  BookmarkFolder *parentFolder = nil;
  BookmarkItem* item = nil;
  if ([mItemPane numberOfSelectedRows] == 1)
  {
    // There is only one selected row.  If it is a folder, use it as our parent.
    // Otherwise, use selected row's parent.
    int index = [mItemPane selectedRow];
    item = [mItemPane itemAtRow: index];
    if ([item isKindOfClass:[BookmarkFolder class]])
      parentFolder = item;
    else if ([item respondsToSelector:@selector(parent)])    // history items have no parent, for example
      parentFolder = [item parent];
  } else
    parentFolder = [self activeCollection];
  [self addItem:aSender withParent:parentFolder isFolder:aIsFolder URL:aURL title:aTitle];
}

-(void)addItem:(id)aSender withParent:(BookmarkFolder*)parent isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle
{
  NSString* titleString = aTitle;
  NSString* urlString   = aURL;

  if (!aIsFolder)
  {
    // If no URL and title were specified, get them from the current page.
    if (!aURL || !aTitle)
      [[mBrowserWindowController getBrowserWrapper] getTitle:&titleString andHref:&urlString];

    mCachedHref = [NSString stringWithString:urlString];
    [mCachedHref retain];

  } else {   // Folder
    mCachedHref = nil;
    titleString = NSLocalizedString(@"NewBookmarkFolder", @"");
  }

  NSTextField* textField  = [mBrowserWindowController getAddBookmarkTitle];
  NSString* bookmarkTitle = titleString;
  NSString* cleanedTitle  = [bookmarkTitle stringByReplacingCharactersInSet:[NSCharacterSet controlCharacterSet] withString:@" "];

  [textField setStringValue: cleanedTitle];

  [mBrowserWindowController cacheBookmarkVC: self];

  // Show/hide the bookmark all tabs checkbox as appropriate.
  NSTabView* tabView = [mBrowserWindowController getTabBrowser];
  id checkbox = [mBrowserWindowController getAddBookmarkCheckbox];
  BOOL hasSuperview = [checkbox superview] != nil;
  if (aIsFolder && hasSuperview) {
    // Just don't show it at all.
    [checkbox removeFromSuperview];
    [checkbox retain];
  }
  else if (!aIsFolder && !hasSuperview) {
    // Put it back in.
    [[[mBrowserWindowController getAddBookmarkSheetWindow] contentView] addSubview: checkbox];
    [checkbox autorelease];
  }

  // Enable the bookmark all tabs checkbox if appropriate.
  if (!aIsFolder)
    [[mBrowserWindowController getAddBookmarkCheckbox] setEnabled: ([tabView numberOfTabViewItems] > 1)];

  // Build up the folder list.
  NSPopUpButton* popup = [mBrowserWindowController getAddBookmarkFolder];
  [popup removeAllItems];
  [mRootBookmarks buildFlatFolderList:[popup menu] depth:1];

  int itemIndex = [popup indexOfItemWithRepresentedObject:parent];
  if (itemIndex != -1)
    [popup selectItemAtIndex:itemIndex];

  [popup synchronizeTitleAndSelectedItem];

  [NSApp beginSheet: [mBrowserWindowController getAddBookmarkSheetWindow]
     modalForWindow: [mBrowserWindowController window]
      modalDelegate: nil //self
     didEndSelector: nil //@selector(sheetDidEnd:)
        contextInfo: nil];
}

-(void)endAddBookmark: (int)aCode
{
  if (aCode == 0)
    return;

  BOOL isGroup = NO;
  id checkbox = [mBrowserWindowController getAddBookmarkCheckbox];
  if (([checkbox superview] != nil) && [checkbox isEnabled] && ([checkbox state] == NSOnState))
  {
    [mCachedHref release];
    mCachedHref = nil;
    isGroup = YES;
  }

  NSString* titleString = [[mBrowserWindowController getAddBookmarkTitle] stringValue];
  NSPopUpButton* popup = [mBrowserWindowController getAddBookmarkFolder];
  NSMenuItem* selectedItem = [popup selectedItem];
  BookmarkFolder *parentFolder = [selectedItem representedObject];

  if (isGroup)
  {
    BookmarkFolder *newGroup = [parentFolder addBookmarkFolder:titleString inPosition:[parentFolder count] isGroup:YES];
    id tabBrowser = [mBrowserWindowController getTabBrowser];
    int count     = [tabBrowser numberOfTabViewItems];
    for (int i = 0; i < count; i++)
    {
      BrowserWrapper* browserWrapper = (BrowserWrapper*)[[tabBrowser tabViewItemAtIndex: i] view];
      NSString* titleString = nil;
      NSString* hrefString = nil;
      [browserWrapper getTitle:&titleString andHref:&hrefString];
      [newGroup addBookmark:titleString url:hrefString inPosition:i isSeparator:NO];
    }
  }
  else
  {
    if (mCachedHref)
    {
      [parentFolder addBookmark:titleString url:mCachedHref inPosition:[parentFolder count] isSeparator:NO];
      [mCachedHref release];
      mCachedHref = nil;
    }
    else
      [parentFolder addBookmarkFolder:titleString inPosition:[parentFolder count] isGroup:NO];
  }

  // if we're NOT visible, set the selection to this bookmark's new parent folder
  // to ensure a selection exists. The next time we use the UI to add a bookmark, it will
  // use this parent as the suggested location in the UI. If the parent is a top-level
  // container (bookmark menu, toolbar bookmarks, etc), clear the item panel's selection entirely,
  // which will fall back to checking the selected container panel's selection. If 
  // the manager is visible, don't muck with the selection.
  if (![mBrowserWindowController bookmarksAreVisible:NO]) {
    [self displayBookmarkInOutlineView:parentFolder];
    long parentRow = [mItemPane rowForItem:parentFolder];   // will be -1 if top-level container
    if (parentRow >= 0)
      [mItemPane selectRow:parentRow byExtendingSelection:NO];
    else {
      // clear selection, next 'add bookmark' will subsequently use the container
      // panel's selection.
      NSEnumerator* iter = [mItemPane selectedRowEnumerator];
      NSNumber* row = nil;
      while ( (row = [iter nextObject]) )
        [mItemPane deselectRow:[row intValue]];
    }
  }
}

-(IBAction)deleteCollection:(id)aSender
{
  BookmarkManager *manager = [BookmarkManager sharedBookmarkManager];
  int index = [mContainerPane selectedRow];
  if (index < (int)[manager firstUserCollection])
    return;
  [self selectContainer:(index - 1)];
  [[manager rootBookmarks] deleteChild:[[manager rootBookmarks] objectAtIndex:index]];
}

-(IBAction)deleteBookmarks: (id)aSender
{
  int index = [mItemPane selectedRow];
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
    selRows = [mItemPane selectedRowEnumerator];
    while (allCollapsed && (doomedItem = [selRows nextObject])) {
      doomedItem = [mItemPane itemAtRow:[doomedItem intValue]];
      if ([mItemPane isItemExpanded:doomedItem]) {
        allCollapsed = NO;
        [mItemPane collapseItem:doomedItem];
      }
    }
  }

  // create array of items we need to delete. Deleting items out of of the
  // selection array is problematic for some reason.
  NSMutableArray *itemsToDelete = [[NSMutableArray alloc] init];
  selRows = [mItemPane selectedRowEnumerator];
  for (NSNumber* currIndex = [selRows nextObject];
       currIndex != nil;
       currIndex = [selRows nextObject]) {
    index = [currIndex intValue];
    BookmarkItem* item = [mItemPane itemAtRow: index];
    [itemsToDelete addObject: item];
  }

  // delete all bookmarks that are in our array
  int count = [itemsToDelete count];
  for (int i = 0; i < count; i++) {
    doomedItem = [itemsToDelete objectAtIndex:i];
    [[doomedItem parent] deleteChild:doomedItem];
  }
  [itemsToDelete release];

  // restore selection to location near last item deleted or last item
  int total = [mItemPane numberOfRows];
  if (index >= total)
    index = total - 1;
  [mItemPane selectRow: index byExtendingSelection: NO];
}

-(IBAction)openBookmark: (id)aSender
{
  id item = nil;
  if (aSender == mItemPane)  {
    int index = [mItemPane selectedRow];
    if (index == -1)
      return;
    item = [mItemPane itemAtRow: index];
  } else if (aSender == mSearchPane) {
    int index = [mSearchPane selectedRow];
    if (index == -1)
      return;
    item = [mSearchResultArray objectAtIndex:index];
  } else if ([aSender isKindOfClass:[BookmarkItem class]])
    item = aSender;

  if (!item)
    return;
  // see if it's a rendezvous item
  id parent = [item parent];
  if (![parent isKindOfClass:[BookmarkItem class]]) {
    [[NetworkServices sharedNetworkServices] attemptResolveService:[parent intValue] forSender:item];
    mOpenActionFlag = kOpenBookmarkAction;
    return;
  }

  // handling toggling of folders
  if ([item isKindOfClass:[BookmarkFolder class]])
  {
    if (![item isGroup])
    {
      if ([mItemPane isItemExpanded:item])
        [mItemPane collapseItem: item];
      else
        [mItemPane expandItem: item];
      return;
    }
  }

  // otherwise follow the standard bookmark opening behavior
  [[NSApp delegate] loadBookmark:item withWindowController:mBrowserWindowController openBehavior:eBookmarkOpenBehaviorDefault];
}

-(IBAction)openBookmarkInNewTab:(id)aSender
{
  id item = nil;

  if (![aSender isKindOfClass:[BookmarkItem class]]) {
    int index = [mItemPane selectedRow];
    if (index == -1)
      return;
    if ([mItemPane numberOfSelectedRows] == 1)
      item = [mItemPane itemAtRow:index];
  } else
    item = aSender;

  if (!item)
    return;
  // see if it's a rendezvous item
  id parent = [item parent];
  if (![parent isKindOfClass:[BookmarkItem class]]) {
    [[NetworkServices sharedNetworkServices] attemptResolveService:[parent intValue] forSender:item];
      mOpenActionFlag = kOpenInNewTabAction;
      return;
  }    

  // otherwise follow the standard bookmark opening behavior
  [[NSApp delegate] loadBookmark:item withWindowController:mBrowserWindowController openBehavior:eBookmarkOpenBehaviorNewTabDefault];
}

-(IBAction)openBookmarkInNewWindow:(id)aSender
{
  id item = nil;
  if (![aSender isKindOfClass:[BookmarkItem class]]) {
    int index = [mItemPane selectedRow];
    if (index == -1)
      return;
    if ([mItemPane numberOfSelectedRows] == 1)
      item = [mItemPane itemAtRow:index];
  } else
    item = aSender;
  if (!item)
    return;

  // see if it's a rendezvous item
  id parent = [item parent];
  if (![parent isKindOfClass:[BookmarkItem class]]) {
    [[NetworkServices sharedNetworkServices] attemptResolveService:[parent intValue] forSender:item];
    mOpenActionFlag = kOpenInNewWindowAction;
    return;
  }

  // otherwise follow the standard bookmark opening behavior
  [[NSApp delegate] loadBookmark:item withWindowController:mBrowserWindowController openBehavior:eBookmarkOpenBehaviorNewWindowDefault];
}

-(IBAction)showBookmarkInfo:(id)aSender
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController];
  BookmarkItem* item = nil;
  if ([[mItemPane window] firstResponder] == mSearchPane) {
    int index = [mSearchPane selectedRow];
    item = [mSearchResultArray objectAtIndex:index];
  } else {
    int index = [mItemPane selectedRow];
    item = [mItemPane itemAtRow: index];
  }
  [bic setBookmark:item];
  [bic showWindow:bic];
}

-(IBAction)startSearch:(id)aSender
{
  NSString *searchString = [mSearchField stringValue];
  if ([searchString length] > 0) {
    [self setSearchResultArray:[[BookmarkManager sharedBookmarkManager] searchBookmarksForString:searchString]];
    // display if it's hidden
    NSArray *subviews = [mItemSearchSplit subviews];
    NSRect bookmarkFrame = [[subviews objectAtIndex:0] frame];
    NSRect searchFrame = [[subviews objectAtIndex:1] frame];
    if (searchFrame.size.height < kMinSearchPaneHeight) {
      searchFrame.size.height = kMinSearchPaneHeight;
      bookmarkFrame.size.height -= kMinSearchPaneHeight;
      [[subviews objectAtIndex:0] setFrame:bookmarkFrame];
      [[subviews objectAtIndex:1] setFrame:searchFrame];
      [mItemSearchSplit adjustSubviews];
      [mItemSearchSplit setNeedsDisplay:YES];
    }
  }
}

-(IBAction) locateBookmark:(id)aSender
{
  int index = [mSearchPane selectedRow];
  if (index == -1)
    return;
  BookmarkItem *item = [mSearchResultArray objectAtIndex:index];
  [self displayBookmarkInOutlineView:item];
  [mItemPane selectRow:[mItemPane rowForItem:item] byExtendingSelection:NO];
}

-(void) displayBookmarkInOutlineView:(BookmarkItem *)aBookmarkItem
{
  BookmarkItem *parent = [aBookmarkItem parent];
  if (parent != mRootBookmarks)
    [self displayBookmarkInOutlineView:parent];
  else {
    int index = [mRootBookmarks indexOfObject:aBookmarkItem];
    [mContainerPane selectRow:index byExtendingSelection:NO];
    [self selectContainer:index];
    return;
  }
  [mItemPane expandItem:aBookmarkItem];
}


- (void) focus
{
  [[mItemPane window] makeFirstResponder:mItemPane];
  
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
    [mContainersSplit setLeftWidth:savedWidth];
    mSplittersRestored = YES;              // needed first time only
  }
}

- (void) setCanEditSelectedContainerContents:(BOOL)inCanEdit
{
  [mItemPane setAllowsEditing:inCanEdit];
  // update buttons
  [mAddBookmarkButton setEnabled:inCanEdit];
  [mAddFolderButton setEnabled:inCanEdit];
  [mInfoButton setEnabled:inCanEdit];
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

- (BOOL)isExpanded:(id)anItem
{
  NSMutableDictionary *dict = [self expandedStatusDictionary];
  NSNumber *aBool = [dict objectForKey:[NSNumber numberWithUnsignedInt:[anItem hash]]];
  if (aBool)
    return [aBool boolValue];
  return NO;
}

-(BOOL) isExpandedInView:(NSOutlineView *)aView
{
  return NO;
}

-(void) setItem:(BookmarkFolder *)anItem isExpanded:(BOOL)aBool
{
  NSMutableDictionary *dict = [self expandedStatusDictionary];
  [dict setObject:[NSNumber numberWithBool:aBool] forKey:[NSNumber numberWithUnsignedInt:[anItem hash]]];
}

- (void)restoreFolderExpandedStates
{
  int curRow = 0;
  while (curRow < [mItemPane numberOfRows])
  {
    id item = [mItemPane itemAtRow:curRow];
    if ([item isKindOfClass:[BookmarkFolder class]])
    {
      if ([self isExpanded:item])
        [mItemPane expandItem: item];
      else
        [mItemPane collapseItem: item];
    }
    curRow ++;
  }
}

-(NSMutableDictionary *)expandedStatusDictionary
{
  if (mExpandedStatus)
    return mExpandedStatus;
  mExpandedStatus = [[NSMutableDictionary alloc] initWithCapacity:20];
  return mExpandedStatus;
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

  if ([types containsObject: @"MozBookmarkType"])
  {
    NSArray *draggedItems = [NSArray pointerArrayFromDataArrayForMozBookmarkDrop:[[info draggingPasteboard] propertyListForType: @"MozBookmarkType"]];
    // added sequentially, so use reverse object enumerator to preserve order.
    NSEnumerator *enumerator = [draggedItems reverseObjectEnumerator];
    id aKid;
    while ((aKid = [enumerator nextObject])) {
      if (isCopy)
        [[aKid parent] copyChild:aKid toBookmarkFolder:dropFolder atIndex:index];
      else
        [[aKid parent] moveChild:aKid toBookmarkFolder:dropFolder atIndex:index];
    }
    return YES;
  }
  else if ([types containsObject: @"MozURLType"])
  {
    NSDictionary* proxy = [[info draggingPasteboard] propertyListForType: @"MozURLType"];
    [dropFolder addBookmark:[proxy objectForKey:@"title"] url:[proxy objectForKey:@"url"] inPosition:index isSeparator:NO];
    return YES;
  }
  else if ([types containsObject: NSURLPboardType])
  {
    NSURL*	urlData = [NSURL URLFromPasteboard:[info draggingPasteboard]];
    [dropFolder addBookmark:[urlData absoluteString] url:[urlData absoluteString] inPosition:index isSeparator:NO];
    return YES;
  }
  else if ([types containsObject: NSStringPboardType])
  {
    NSString* draggedText = [[info draggingPasteboard] stringForType:NSStringPboardType];
    NSURL *testURL = [NSURL URLWithString:draggedText];
    if (testURL) {
      [dropFolder addBookmark:draggedText url:draggedText inPosition:index isSeparator:NO];
      return YES;
    }
  }
  return NO;  
}


#pragma mark -
//
// table view things
//
- (void) selectContainer:(int)inRowIndex
{
  [mContainerPane selectRow:inRowIndex byExtendingSelection:NO];
  if ( inRowIndex == kHistoryContainerIndex ) {
    [mItemPane setDataSource:mHistorySource];
    [mItemPane setDelegate:mHistorySource];
    [mHistorySource loadLazily];
    [self setCanEditSelectedContainerContents:NO];
    [mItemPane setTarget:mHistorySource];
    [mItemPane setDoubleAction: @selector(openHistoryItem:)];
    [mItemPane setDeleteAction: @selector(deleteHistoryItems:)];
  } else {
    [mItemPane setDataSource:self];
    [mItemPane setDelegate:self];
    BookmarkFolder *activeCollection = [mRootBookmarks objectAtIndex:inRowIndex];
    [self setActiveCollection:activeCollection];
    [self restoreFolderExpandedStates];
    [mItemPane setTarget:self];
    [mItemPane setDoubleAction: @selector(openBookmark:)];
    if ([activeCollection isSmartFolder])
      [self setCanEditSelectedContainerContents:NO];
    else
      [self setCanEditSelectedContainerContents:YES];
    [mItemPane setDeleteAction: @selector(deleteBookmarks:)];
    if ( kBookmarkMenuContainerIndex == inRowIndex )
      [mAddSeparatorButton setEnabled:YES];
    else
      [mAddSeparatorButton setEnabled:NO];
  }
  [mItemPane reloadData];
}

- (void) selectLastContainer
{
  int curRow = [mContainerPane selectedRow];
  curRow = (curRow != -1) ? curRow : 0;
  [self selectContainer:curRow];
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
  if ( tableView == mContainerPane )
    return [mRootBookmarks count];
  else if ( tableView == mSearchPane )
    return [mSearchResultArray count];
  return 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
  id retValue = nil;
  id item = nil;
  if ( tableView == mContainerPane ) 
    item = [mRootBookmarks objectAtIndex:row];
  else if (tableView == mSearchPane )
    item = [mSearchResultArray objectAtIndex:row];
  NS_DURING
    retValue = [item valueForKey:[tableColumn identifier]];
  NS_HANDLER
    retValue = nil;
  NS_ENDHANDLER
  return retValue;
}

- (void)tableView:(NSTableView *)inTableView willDisplayCell:(id)inCell forTableColumn:(NSTableColumn *)inTableColumn row:(int)inRowIndex
{
  if ( inTableView == mContainerPane ) {
    BookmarkFolder *aFolder = [mRootBookmarks objectAtIndex:inRowIndex];
    [inCell setImage:[aFolder icon]];
  } else if (inTableView == mSearchPane && [[inTableColumn identifier] isEqualToString:@"title"]) {
    BookmarkItem *anItem = [mSearchResultArray objectAtIndex:inRowIndex];
    [inCell setImage:[anItem icon]];
  }
}

- (BOOL)tableView:(NSTableView *)aTableView shouldEditTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
  if (aTableView == mContainerPane) {
    if (rowIndex >= (int)[[BookmarkManager sharedBookmarkManager] firstUserCollection])
      return YES;
    return NO;
  }
  return NO;
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn row:(int)row;
{
  if (tableView == mContainerPane) {
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
  // Pack pointers to bookmark items into this array
  NSArray *pointerArray = [NSArray dataArrayFromPointerArrayForMozBookmarkDrop:itemArray];
  [itemArray release];
  [pboard declareTypes:[NSArray arrayWithObject:@"MozBookmarkType"] owner:self];
  [pboard setPropertyList:pointerArray forType:@"MozBookmarkType"];
  return YES;
}

//
// -tableView:validateDrop:proposedRow:proposedDropOperation:
//
// validate if the drop is allowed and what type it is (move, copy, etc). 
//
- (NSDragOperation)tableView:(NSTableView*)tv validateDrop:(id <NSDraggingInfo>)info proposedRow:(int)row proposedDropOperation:(NSTableViewDropOperation)op
{
  if (tv == mContainerPane) {
    NSArray* types = [[info draggingPasteboard] types];
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
      if ([types containsObject:@"MozBookmarkType"])
      {
        NSArray *draggedItems = [NSArray pointerArrayFromDataArrayForMozBookmarkDrop:[[info draggingPasteboard] propertyListForType: @"MozBookmarkType"]];
        BOOL isOK = [manager isDropValid:draggedItems toFolder:dropFolder];
        return (isOK) ? NSDragOperationGeneric: NSDragOperationNone;
      }
      else if ([types containsObject:@"MozURLType"] || [types containsObject:NSURLPboardType])
      {
        return (dropFolder == mRootBookmarks) ? NSDragOperationNone: NSDragOperationGeneric;
      }
      else if ([types containsObject:NSStringPboardType])
      {
        NSURL* testURL = [NSURL URLWithString:[[info draggingPasteboard] stringForType:NSStringPboardType]];
        if (testURL)
          return (dropFolder == mRootBookmarks) ? NSDragOperationNone: NSDragOperationGeneric;        
      }
    }
  }
  // nope
  return NSDragOperationNone;
}

- (BOOL)tableView:(NSTableView*)tv acceptDrop:(id <NSDraggingInfo>)info row:(int)row dropOperation:(NSTableViewDropOperation)op
{
  if (tv != mContainerPane)
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
  return [self doDrop:info intoFolder:dropFolder index:dropLocation];
}

- (BOOL)tableView:(NSTableView *)aTableView shouldSelectRow:(int)rowIndex
{
  if (aTableView == mContainerPane) 
    [self selectContainer:rowIndex];
  return YES;
}

-(void)tableViewSelectionDidChange:(NSNotification *)note
{
  if ([note object] == mSearchPane) {
    BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController];
    int index = [mSearchPane selectedRow];
    if (index != -1)
      [mSearchButton setEnabled:YES];
    else
      [mSearchButton setEnabled:NO];
    if ([[bic window] isVisible])
      [bic setBookmark:[mSearchResultArray objectAtIndex:index]];
  }
}

-(NSMenu *)tableView:(NSTableView *)aTableView contextMenuForRow:(int)rowIndex
{
  if (aTableView == mContainerPane) {
    NSMenu* contextMenu = nil;
    if (rowIndex >= 0) {
      contextMenu = [aTableView menu];
      int numItems = [contextMenu numberOfItems];
      while (numItems > 2)
        [contextMenu removeItemAtIndex:(--numItems)];
      if (rowIndex >= (int)[[BookmarkManager sharedBookmarkManager] firstUserCollection]) {
        [contextMenu addItem:[NSMenuItem separatorItem]];
        NSMenuItem *deleteItem = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"Delete",@"Delete") action:@selector(deleteCollection:) keyEquivalent:[NSString string]];
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
  if (item)
    return [item objectAtIndex:index];
  return [[self activeCollection] objectAtIndex:index];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
  if (!(item) || [item isKindOfClass:[BookmarkFolder class]])
    return YES;
  return NO;
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
  if (item)
    return [item count];
  return [[self activeCollection] count];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  id retValue = nil;
  NS_DURING
    retValue = [item valueForKey:[tableColumn identifier]];
  NS_HANDLER
    if ([item isKindOfClass:[BookmarkFolder class]] && [[tableColumn identifier] isEqualToString:@"url"]) {
      NSString* itemCountStr = [NSString stringWithFormat:NSLocalizedString(@"Contains Items", @"%u Items"),[item count]];
      NSDictionary* colorAttributes = [NSDictionary dictionaryWithObject:[NSColor disabledControlTextColor] forKey:NSForegroundColorAttributeName];
      retValue = [[[NSAttributedString alloc] initWithString:itemCountStr attributes:colorAttributes] autorelease];
    } else
      retValue = nil;
  NS_ENDHANDLER
  return retValue;
}

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

- (BOOL)outlineView:(NSOutlineView *)ov writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard
{
  int count = [items count];
  if ((count == 0) || [mActiveRootCollection isSmartFolder])
    return NO;

  // Pack pointers to bookmark items into this array.
  NSArray *pointerArray = [NSArray dataArrayFromPointerArrayForMozBookmarkDrop:items];
  if (count == 1) {
    id aBookmark = [items objectAtIndex:0];
    if ([aBookmark isKindOfClass:[Bookmark class]]) {
      [pboard declareURLPasteboardWithAdditionalTypes:[NSArray arrayWithObject:@"MozBookmarkType"] owner:self];
      [pboard setDataForURL:[aBookmark url] title:[aBookmark title]];
      [pboard setPropertyList:pointerArray forType:@"MozBookmarkType"];
      return YES;
    }
  }
  // it's either a folder or we've got more than 1 thing. ship it
  // out as MozBookmarkType
  [pboard declareTypes:[NSArray arrayWithObject:@"MozBookmarkType"] owner: self];
  [pboard setPropertyList: pointerArray forType:@"MozBookmarkType"];
  return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView*)ov validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index
{
  NSArray* types = [[info draggingPasteboard] types];

  //  if the index is -1, deny the drop
  if (index == NSOutlineViewDropOnItemIndex)
    return NSDragOperationNone;

  if ([types containsObject: @"MozBookmarkType"]) {
    NSArray *draggedItems = [NSArray pointerArrayFromDataArrayForMozBookmarkDrop:[[info draggingPasteboard] propertyListForType: @"MozBookmarkType"]];
    BookmarkFolder* parent = (item) ? item : [self activeCollection];
    BOOL isOK = [[BookmarkManager sharedBookmarkManager] isDropValid:draggedItems toFolder:parent];
    return (isOK) ? NSDragOperationGeneric : NSDragOperationNone;
  }

  if ([types containsObject: NSURLPboardType] || [types containsObject: @"MozURLType"] )
    return NSDragOperationGeneric;

  // see if we can turn a string into a URL.  If so, accept it. If not, punt.
  if ([types containsObject: NSStringPboardType]) {
    NSURL* testURL = [NSURL URLWithString:[[info draggingPasteboard] stringForType:NSStringPboardType]];
    if (testURL)
      return NSDragOperationGeneric;
  }
  return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView*)ov acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index
{
  BookmarkFolder *parent = (item) ? item : [self activeCollection];
  BOOL retVal = [self doDrop:info intoFolder:parent index:index];
  [ov deselectAll:self];
  return retVal;
}

- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)item
{
  if ([item isKindOfClass:[Bookmark class]]) {
    if ([[item description] length] > 0)
      return [NSString stringWithFormat:@"%@\n%@",[item url], [item description]];
    else
      return [item url];
  }
  else if ([item isKindOfClass:[BookmarkFolder class]]) {
    if ([[item description] length] > 0)
      return [item description];
    else
      return [item title];
  }
  else
    return nil;
}

- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItem:(id)item
{
  return [[BookmarkManager sharedBookmarkManager] contextMenuForItem:item fromView:outlineView target:self];
}

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren
{
  if (!item)
    [mItemPane reloadData];
  else
    [mItemPane reloadItem: item reloadChildren: aReloadChildren];
}

- (BOOL)haveSelectedRow
{
  return ([mItemPane selectedRow] != -1);
}

-(void)outlineViewSelectionDidChange: (NSNotification*) aNotification
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController];
  int index = [mItemPane selectedRow];
  if (index == -1)
  {
    [mInfoButton setEnabled:NO];
    [bic close];
  }
  else
  {
    id item = [mItemPane itemAtRow: index];
    [mInfoButton setEnabled:YES];
    if ([[bic window] isVisible])
      [bic setBookmark:item];
  }
}

/*
-(BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  int  index = [mItemPane selectedRow];
  BOOL haveSelection = (index != -1);
  BOOL multiSelection = ([mItemPane numberOfSelectedRows] > 1);
  BOOL isBookmark = NO;
  BOOL isToolbar = NO;
  BOOL isGroup = NO;

  id item = nil;

  if (haveSelection)
    item = [mItemPane itemAtRow: index];
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
  [self setItem:item isExpanded:YES];
}

- (void)outlineViewItemDidCollapse:(NSNotification *)notification
{
  id item = [[notification userInfo] objectForKey:@"NSObject"];
  [self setItem:item isExpanded:NO];
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
  BookmarkFolder *aFolder = [note object];
  if ((aFolder == [[BookmarkManager sharedBookmarkManager] rootBookmarks]))
  {
    [mContainerPane reloadData];
    BookmarkFolder *updatedFolder = [[note userInfo] objectForKey:BookmarkFolderChildKey];
    [self selectContainer:[aFolder indexOfObjectIdenticalTo:updatedFolder]];
    return;
  }
  else if (aFolder == mActiveRootCollection)
    aFolder = nil;
  [self reloadDataForItem:aFolder reloadChildren:YES];
}

- (void)bookmarkRemoved:(NSNotification *)note
{
  BookmarkFolder *aFolder = [note object];
  if ((aFolder == [[BookmarkManager sharedBookmarkManager] rootBookmarks])) {
    [mContainerPane reloadData];
    return;
  } else if (aFolder == mActiveRootCollection)
    aFolder = nil;
  [self reloadDataForItem:aFolder reloadChildren:YES];
}

- (void)bookmarkChanged:(NSNotification *)note
{
  [self reloadDataForItem:[note object] reloadChildren:NO];
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
  BOOL retVal = NO;
  // subview will be a NSScrollView, so we have to get the superview of the
  // search pane for comparison.
  if ( sender == mItemSearchSplit && subview == [mSearchPane superview] )
    retVal = YES;
  return retVal;
}


- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedCoord ofSubviewAt:(int)offset
{
  if ( sender == mContainersSplit )
    return kMinContainerSplitWidth;  // minimum size of collections pane
  else
    return proposedCoord;
}

@end
