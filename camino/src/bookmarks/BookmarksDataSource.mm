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
 *			Max Horn <max@quendi.de>
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

#import "NSString+Utils.h"

#import "BookmarksDataSource.h"
#import "BookmarkInfoController.h"
#import "SiteIconProvider.h"

#include "nsCOMPtr.h"
#include "nsIContent.h"

#include "nsVoidArray.h"

#import "BookmarksService.h"

@interface BookmarksDataSource(Private)

- (void)refreshChildrenOfItem:(nsIContent*)item;

@end

const int kBookmarksRootItemTag = -2;

@implementation BookmarksDataSource

-(id) init
{
  if ( (self = [super init]) )
  {
    mCachedHref = nil;
  }
  return self;
}

- (void)dealloc
{
//  NSLog(@"BookmarksDataSource dealloc");
  [super dealloc];
}

-(void) awakeFromNib
{
  // make sure these are disabled at the start since the outliner
  // starts off with no selection.
  [mEditBookmarkButton setEnabled:NO];
  [mDeleteBookmarkButton setEnabled:NO];
}

-(void) windowClosing
{
	BookmarksManager* bmManager = [BookmarksManager sharedBookmarksManagerDontAlloc];
  [bmManager removeBookmarksClient:self];
}

-(void) ensureBookmarks
{
  if (mRegisteredClient)
    return;
  
  BookmarksManager* bmManager = [BookmarksManager sharedBookmarksManager];
  [bmManager addBookmarksClient:self];
  mRegisteredClient = YES;
  
  [mOutlineView setTarget: self];
  [mOutlineView setDoubleAction: @selector(openBookmark:)];
  [mOutlineView setDeleteAction: @selector(deleteBookmarks:)];
  [mOutlineView reloadData];
  [self restoreFolderExpandedStates];
}

-(IBAction)addBookmark:(id)aSender
{
  [self addBookmark: aSender useSelection: YES isFolder: NO URL:nil title:nil];
}

-(IBAction)addFolder:(id)aSender
{
  [self addBookmark: aSender useSelection: YES isFolder: YES URL:nil title:nil];
}

-(void)addBookmark:(id)aSender useSelection:(BOOL)aUseSel isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle
{
  // We use the selected item to determine the parent only if aUseSel is YES.
  BookmarkItem* item = nil;
  if (aUseSel && ([mOutlineView numberOfSelectedRows] == 1))
  {
    // There is only one selected row.  If it is a folder, use it as our parent.
    // Otherwise, use our parent,
    int index = [mOutlineView selectedRow];
    item = [mOutlineView itemAtRow: index];
    if (![mOutlineView isExpandable: item]) {
      // We can't be used as the parent.  Try our parent.
      nsIContent* content = [item contentNode];
      if (!content)
        return;

      nsCOMPtr<nsIContent> parentContent = content->GetParent();
      nsCOMPtr<nsIContent> root;
      BookmarksService::GetRootContent(getter_AddRefs(root));
      
      // The root has no item, so we don't need to do a lookup unless we
      // aren't the root.
      if (parentContent != root) {
        PRUint32 contentID;
        parentContent->GetContentID(&contentID);
        item = BookmarksService::GetWrapperFor(contentID);
      }
    }
  }

  [self addBookmark:aSender withParent:item isFolder:aIsFolder URL:aURL title:aTitle];
}

-(void)addBookmark:(id)aSender withParent:(BookmarkItem*)bmItem isFolder:(BOOL)aIsFolder URL:(NSString*)aURL title:(NSString*)aTitle
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
  
  [mBrowserWindowController cacheBookmarkDS: self];

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
  [popup addItemWithTitle: NSLocalizedString(@"BookmarksRootName", @"")];
  [[popup lastItem] setTag:kBookmarksRootItemTag];
  
  BookmarksManager* bmManager = [BookmarksManager sharedBookmarksManager];
  [bmManager buildFlatFolderList:[popup menu] fromRoot:NULL];
  
  int itemIndex = [popup indexOfItemWithTag:[bmItem intContentID]];
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

  BookmarksManager* bmManager = [BookmarksManager sharedBookmarksManager];
  NSString* titleString = [[mBrowserWindowController getAddBookmarkTitle] stringValue];

  // Figure out the parent element.
  nsCOMPtr<nsIContent> parentContent;
  
  NSPopUpButton* popup = [mBrowserWindowController getAddBookmarkFolder];
  NSMenuItem* selectedItem = [popup selectedItem];
  int tag = [selectedItem tag];
  if (tag != kBookmarksRootItemTag)
  {
    BookmarkItem* item = BookmarksService::GetWrapperFor(tag);
    // Get the content node.
    parentContent = [item contentNode];
  }
  
  if (isGroup)
  {
    id tabBrowser = [mBrowserWindowController getTabBrowser];
    int count     = [tabBrowser numberOfTabViewItems];

    NSMutableArray* itemsArray = [[NSMutableArray alloc] initWithCapacity:count];

    for (int i = 0; i < count; i++)
    {
      BrowserWrapper* browserWrapper = (BrowserWrapper*)[[tabBrowser tabViewItemAtIndex: i] view];
      
      NSString* titleString = nil;
      NSString* hrefString = nil;
      [browserWrapper getTitle:&titleString andHref:&hrefString];

      NSDictionary* itemDict = [NSDictionary dictionaryWithObjectsAndKeys:
                                                titleString, @"title",
                                                 hrefString, @"href",
                                                             nil];
      [itemsArray addObject:itemDict];
    }
    
    [bmManager addNewBookmarkGroup:titleString items:itemsArray withParent:parentContent];
  }
  else
  {
    if (mCachedHref)
    {
      [bmManager addNewBookmark:mCachedHref title:titleString withParent:parentContent];
      [mCachedHref release];
      mCachedHref = nil;
    }
    else
      [bmManager addNewBookmarkFolder:titleString withParent:parentContent];
  }
}

-(IBAction)deleteBookmarks: (id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;

  // first, see how many items are selected
  BOOL haveBookmarks = NO;
  
  NSEnumerator* testSelRows = [mOutlineView selectedRowEnumerator];
  for (NSNumber* currIndex = [testSelRows nextObject];
      currIndex != nil;
      currIndex = [testSelRows nextObject])
  {
    index = [currIndex intValue];
    BookmarkItem* item = [mOutlineView itemAtRow: index];
    if ([mOutlineView isExpandable: item]) {
      // dumb check to see if we're deleting an empty folder. Should really
      // recurse down
      if ([self outlineView:mOutlineView numberOfChildrenOfItem: item] > 0)
        haveBookmarks = YES;
    } else
      haveBookmarks = YES;
  }

  // ideally, we should count the number of doomed bookmarks and tell the user
  if (haveBookmarks) {
    NSString *alert         = NSLocalizedString(@"DeteleBookmarksAlert",@"");
    NSString *message       = NSLocalizedString(@"DeteleBookmarksMsg",@"");
    NSString *okButton      = NSLocalizedString(@"DeteleBookmarksOKButton",@"");
    NSString *cancelButton  = NSLocalizedString(@"DeteleBookmarksCancelButton",@"");
    if (NSRunAlertPanel(alert, message, okButton, cancelButton, nil) != NSAlertDefaultReturn)
      return;
  }
  
  // The alert panel was the key window.  As soon as we dismissed it, Cocoa will
  // pick a new one for us.  Ideally, it'll be the window we were using when
  // we clicked the delete button.  However, if by chance the BookmarkInfoController
  // is visible, it will become the key window since it's a panel.  If we then delete
  // the bookmark and try to close the window before we've setup a new bookmark,
  // we'll trigger the windowDidResignKey message, which will try to update the bookmark
  // we just deleted, and things will crash.  So, we'll trigger windowDidResignKey now
  // and avoid the unpleasentness of a crash log.

  if (![[mBrowserWindowController window] isKeyWindow])
    [[mBrowserWindowController window] makeKeyWindow];
  
  // we'll run into problems if a parent item and one if its children are both selected.
  // A cheap way of having to avoid scanning the list to remove children is to have the
  // outliner collapse all items that are being deleted. This will cull the selection
  // for us and eliminate any children that happened to be selected.
  NSEnumerator* selRows = [mOutlineView selectedRowEnumerator];
  for (NSNumber* currIndex = [selRows nextObject];
      currIndex != nil;
      currIndex = [selRows nextObject]) {
    index = [currIndex intValue];
    BookmarkItem* item = [mOutlineView itemAtRow: index];
    [mOutlineView collapseItem: item];
  }

  // create array of items we need to delete. Deleting items out of of the
  // selection array is problematic for some reason.
  NSMutableArray* itemsToDelete = [[[NSMutableArray alloc] init] autorelease];
  selRows = [mOutlineView selectedRowEnumerator];
  for (NSNumber* currIndex = [selRows nextObject];
      currIndex != nil;
      currIndex = [selRows nextObject]) {
    index = [currIndex intValue];
    BookmarkItem* item = [mOutlineView itemAtRow: index];
    [itemsToDelete addObject: item];
  }

  // delete all bookmarks that are in our array
  int count = [itemsToDelete count];
  for (int i = 0; i < count; i++) {
    BookmarkItem* item = [itemsToDelete objectAtIndex: i];
    [self deleteBookmark: item];
  }

  // restore selection to location near last item deleted or last item
  int total = [mOutlineView numberOfRows];
  if (index >= total)
    index = total - 1;
  [mOutlineView selectRow: index byExtendingSelection: NO];
  // lame, but makes sure we catch all delete events in Info Panel
  [[NSNotificationCenter defaultCenter] postNotificationName:@"NSOutlineViewSelectionDidChangeNotification" object:mOutlineView];
}

-(void)deleteBookmark:(id)aItem
{
  [aItem remove];
}

-(IBAction)openBookmark: (id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;

  id item = [mOutlineView itemAtRow: index];
  if (!item)
    return;

  if ([item isGroup])
  {
    NSArray* groupURLs = [[BookmarksManager sharedBookmarksManager] getBookmarkGroupURIs:item];
    [mBrowserWindowController openTabGroup:groupURLs replaceExistingTabs:YES];
  }
  else if ([mOutlineView isExpandable: item])
  {
    if ([mOutlineView isItemExpanded: item])
      [mOutlineView collapseItem: item];
    else
      [mOutlineView expandItem: item];
  }
  else
  {
    // if the command key is down, follow the command-click pref
    if (([[NSApp currentEvent] modifierFlags] & NSCommandKeyMask) &&
        [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL])
    {
      BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
      [mBrowserWindowController openNewTabWithURL:[item url] referrer:nil loadInBackground: loadInBackground];
    }
    else
      [[mBrowserWindowController getBrowserWrapper] loadURI:[item url] referrer:nil flags:NSLoadFlagsNone activate:YES];
  }
}

-(IBAction)openBookmarkInNewTab:(id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;

  if ([mOutlineView numberOfSelectedRows] == 1)
  {
    BookmarkItem* item = [mOutlineView itemAtRow: index];
    BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];
    [mBrowserWindowController openNewTabWithURL:[item url] referrer:nil loadInBackground: loadInBackground];
  }
}

-(IBAction)openBookmarkInNewWindow:(id)aSender
{
  int index = [mOutlineView selectedRow];
  if (index == -1)
    return;
  if ([mOutlineView numberOfSelectedRows] == 1)
  {
    BookmarkItem* item = [mOutlineView itemAtRow: index];

    BOOL loadInBackground = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.loadInBackground" withSuccess:NULL];

    if ([item isGroup]) 
      [mBrowserWindowController openNewWindowWithGroup:[item contentNode] loadInBackground:loadInBackground];
    else
      [mBrowserWindowController openNewWindowWithURL:[item url] referrer: nil loadInBackground: loadInBackground];
  }
}

-(IBAction)showBookmarkInfo:(id)aSender
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController]; 

  int index = [mOutlineView selectedRow];
  BookmarkItem* item = [mOutlineView itemAtRow: index];
  [bic setBookmark:item];

  [bic showWindow:bic];
}

- (void)restoreFolderExpandedStates
{
  int curRow = 0;
  
  while (curRow < [mOutlineView numberOfRows])
  {
    id item = [mOutlineView itemAtRow:curRow];

    if (item)
    {
      if ([item isExpanded])
        [mOutlineView expandItem: item];
      else
        [mOutlineView collapseItem: item];
    }
  
    curRow ++;
  }
}

- (void)refreshChildrenOfItem:(nsIContent*)item
{
  BookmarkItem* bmItem = nil;

  nsCOMPtr<nsIContent> parent;
  if (item)
    parent = item->GetParent();
  if (parent)		// we're not the root
    bmItem = [[BookmarksManager sharedBookmarksManager] getWrapperForContent:item];

  [self reloadDataForItem:bmItem reloadChildren:YES];
}

#pragma mark -

// BookmarksClient protocol

- (void)bookmarkAdded:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
  [self refreshChildrenOfItem:container];
}

- (void)bookmarkRemoved:(nsIContent*)bookmark inContainer:(nsIContent*)container isChangedRoot:(BOOL)isRoot
{
  [self refreshChildrenOfItem:container];
}

- (void)bookmarkChanged:(nsIContent*)bookmark
{
  BookmarkItem* item = [[BookmarksManager sharedBookmarksManager] getWrapperForContent:bookmark];
  [self reloadDataForItem:item reloadChildren:NO];
}

- (void)specialFolder:(EBookmarksFolderType)folderType changedTo:(nsIContent*)newFolderContent
{
  // change the icons
  
  
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
  return NO;	//([[tableColumn identifier] isEqualToString:@"name"]);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
  if (!mRegisteredClient) return nil;
  
  nsCOMPtr<nsIContent> content;
  if (!item)
    BookmarksService::GetRootContent(getter_AddRefs(content));
  else
    content = [item contentNode];
  
  nsIContent *child=content->GetChildAt(index);
  if ( child )
    return BookmarksService::GetWrapperFor(child);
  
  return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
  if (!mRegisteredClient) return NO;
  
  if (!item)
    return YES; // The root node is always open.
  
  return [item isFolder];
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
  if (!mRegisteredClient) return 0;
  
  nsCOMPtr<nsIContent> content;
  if (!item)
    BookmarksService::GetRootContent(getter_AddRefs(content));
  else 
    content = [item contentNode];
  
  PRInt32 childCount = 0;
  if ( content )
	childCount=content->GetChildCount();
  
  return childCount;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  if (!mRegisteredClient) return nil;

  id retValue = nil;

  NSString *columnName = [tableColumn identifier];
  if ([columnName isEqualToString: @"Name"]) {
    // Return data as an NSString. We'll deal with the icon later.
    retValue = [item name];
  }
  else if ([columnName isEqualToString: @"URL"]) {
    if ([item isFolder]) {
#if 0
      int numKids = [item getNumberOfChildren];
      NSString* itemCountStr = [NSString stringWithFormat:NSLocalizedString(@"Contains Items", @"%d Items"),
                                  numKids];
      NSMutableDictionary* colorAttributes = nil;   //XXXX fill in color attribute
      retValue = [[NSAttributedString alloc] initWithString:itemCountStr attributes:colorAttributes];
#endif
    }
    else
      retValue = [item url];
  }
  return retValue;
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
  // set the image on the name column. the url column doesn't have an image.
  if ([[tableColumn identifier] isEqualToString: @"Name"]) {
    NSImage* image = [[BookmarksManager sharedBookmarksManager] createIconForBookmarkItem:item useSiteIcon:YES];
    [cell setImage:image];
  }
}

- (void)outlineView:(NSOutlineView *)outlineView setObjectValue:(id)object forTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
  // object is really an NSString, even though objectValueForTableColumn returns NSAttributedStrings.
  BookmarkItem* bmItem = (BookmarkItem*)item;
  NSString *columnName = [tableColumn identifier];
  if ( [columnName isEqualTo:@"Name"] )
  {
    [bmItem setName:object];
    [bmItem itemChanged:YES];
  }
  else if ( [columnName isEqualTo:@"URL"] ) {
    [bmItem setUrl:object];
    [bmItem itemChanged:YES];
  }
}

- (BOOL)outlineView:(NSOutlineView *)ov writeItems:(NSArray*)items toPasteboard:(NSPasteboard*)pboard 
{
  if (!mRegisteredClient) return NO;
  
#ifdef FILTER_DESCENDANT_ON_DRAG
  NSArray *toDrag = BookmarksService::FilterOutDescendantsForDrag(items);
#else
  NSArray *toDrag = items;
#endif
  int count = [toDrag count];
  if (count > 0) {
    // Create Pasteboard Data
    NSMutableArray *draggedIDs = [NSMutableArray arrayWithCapacity: count];
    
    for (int i = 0; i < count; i++)
      [draggedIDs addObject: [[toDrag objectAtIndex: i] contentID]];
    
    if (count == 1) {
      // if we have just one item, we add some more flavours
      [pboard declareTypes: [NSArray arrayWithObjects:
          @"MozBookmarkType", NSURLPboardType, NSStringPboardType, nil] owner: self];
      [pboard setPropertyList: draggedIDs forType: @"MozBookmarkType"];

      NSString* itemURL = [[toDrag objectAtIndex: 0] url];
      [pboard setString:itemURL forType: NSStringPboardType];
      [[NSURL URLWithString:itemURL] writeToPasteboard: pboard];
      // maybe construct the @"MozURLType" type here also
    }
    else {
      // multiple bookmarks. Arrays of strings or NSURLs seem to
      // confuse receivers. Not sure what the correct way is.
      [pboard declareTypes: [NSArray arrayWithObject: @"MozBookmarkType"] owner: self];
      [pboard setPropertyList: draggedIDs    forType: @"MozBookmarkType"];
    }

    return YES;
  }

  return NO;
}


- (NSDragOperation)outlineView:(NSOutlineView*)ov validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(int)index 
{
  if (!mRegisteredClient) return NSDragOperationNone;
  
  NSArray* types = [[info draggingPasteboard] types];
  BOOL    isCopy = ([info draggingSourceOperationMask] == NSDragOperationCopy);

  //  if the index is -1, deny the drop
  if (index == NSOutlineViewDropOnItemIndex)
    return NSDragOperationNone;

  if ([types containsObject: @"MozBookmarkType"])
  {
    NSArray *draggedIDs = [[info draggingPasteboard] propertyListForType: @"MozBookmarkType"];

    BookmarkItem* parent = (item) ? item : BookmarksService::GetRootItem();
    return (BookmarksService::IsBookmarkDropValid(parent, index, draggedIDs, isCopy)) ? NSDragOperationGeneric : NSDragOperationNone;
  }
  
  if ([types containsObject: @"MozURLType"])
    return NSDragOperationGeneric;
  
  if ([types containsObject: NSStringPboardType])
    return NSDragOperationGeneric;

  if ([types containsObject: NSURLPboardType])
    return NSDragOperationGeneric;

  return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView*)ov acceptDrop:(id <NSDraggingInfo>)info item:(id)item childIndex:(int)index
{
  if (!mRegisteredClient) return NO;

  NSArray*      types  = [[info draggingPasteboard] types];
  BookmarkItem* parent = (item) ? item : BookmarksService::GetRootItem();
  BOOL          isCopy = ([info draggingSourceOperationMask] == NSDragOperationCopy);
    
  BookmarkItem* beforeItem = [self outlineView:ov child:index ofItem:item];
  if ([types containsObject: @"MozBookmarkType"])
  {
    NSArray *draggedItems = [[info draggingPasteboard] propertyListForType: @"MozBookmarkType"];
    return BookmarksService::PerformBookmarkDrop(parent, beforeItem, index, draggedItems, isCopy);
  }
  else if ([types containsObject: @"MozURLType"])
  {
    NSDictionary* proxy = [[info draggingPasteboard] propertyListForType: @"MozURLType"];    
    return BookmarksService::PerformProxyDrop(parent, beforeItem, proxy);
  }
  else if ([types containsObject: NSStringPboardType])
  {
    NSString* draggedText = [[info draggingPasteboard] stringForType:NSStringPboardType];
    return BookmarksService::PerformURLDrop(parent, beforeItem, draggedText, draggedText);
  }
  else if ([types containsObject: NSURLPboardType])
  {
    NSURL*	urlData = [NSURL URLFromPasteboard:[info draggingPasteboard]];
    return BookmarksService::PerformURLDrop(parent, beforeItem, [urlData absoluteString], [urlData absoluteString]);
  }
  
  return NO;
}

- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)item
{
  if (!mRegisteredClient) return @"";

  NSString* descStr = nil;
  NSString* hrefStr = nil;

  if ([[item descriptionString] length] > 0)
    descStr = [item descriptionString];

  // Only description for folders
  if ([item isFolder])
    return descStr;
  
  // Extract the URL from the item
  if ([[item url] length] > 0)
    hrefStr = [item url];

  if (!hrefStr)
    return descStr;
  
  if (!descStr)
    return hrefStr;

  // Display both URL and description
  return [NSString stringWithFormat:@"%@\n%@", hrefStr, descStr];
}

/*
- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItem:(id)item
{
 // TODO - return (custom?) context menu for item here.
 // Note that according to HIG, there should never be disabled items in
 // a context menu - instead, items that do not apply should be removed.
 // We could nicely do that here.
}
*/

- (void)reloadDataForItem:(id)item reloadChildren: (BOOL)aReloadChildren
{
  if (!item)
    [mOutlineView reloadData];
  else
    [mOutlineView reloadItem: item reloadChildren: aReloadChildren];
}

- (BOOL)haveSelectedRow
{
  return ([mOutlineView selectedRow] != -1);
}

-(void)outlineViewSelectionDidChange: (NSNotification*) aNotification
{
  BookmarkInfoController *bic = [BookmarkInfoController sharedBookmarkInfoController]; 
  int index = [mOutlineView selectedRow];
  if (index == -1)
  {
    [mEditBookmarkButton setEnabled:NO];
    [mDeleteBookmarkButton setEnabled:NO];
    [bic close];
  }
  else
  {
    BookmarkItem* item = [mOutlineView itemAtRow: index];

    [mEditBookmarkButton setEnabled:YES];
    [mDeleteBookmarkButton setEnabled:![item isToobarRoot]];
    if ([[bic window] isVisible]) 
      [bic setBookmark:[mOutlineView itemAtRow:index]];
  }
}

-(BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  int  index = [mOutlineView selectedRow];
  BOOL haveSelection = (index != -1);
  BOOL isBookmark = NO;
  BOOL isGroup = NO;
  
  BookmarkItem* item = nil;

  if (haveSelection)
  {
    item = [mOutlineView itemAtRow: index];
    isBookmark = ([mOutlineView isExpandable:item] == NO);
    isGroup    = [item isGroup];
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
    return haveSelection && ![item isToobarRoot];		// should deal with multiple selections

  if (([aMenuItem action] == @selector(addFolder:)))
    return YES;

  return YES;
}

- (void)outlineViewItemWillExpand:(NSNotification *)notification
{
  BookmarkItem* item = [[notification userInfo] objectForKey:[[[notification userInfo] allKeys] objectAtIndex: 0]];
  [item setIsExpanded:YES];
}

- (void)outlineViewItemWillCollapse:(NSNotification *)notification
{
  BookmarkItem* item = [[notification userInfo] objectForKey:[[[notification userInfo] allKeys] objectAtIndex: 0]];
  [item setIsExpanded:NO];
}

@end

