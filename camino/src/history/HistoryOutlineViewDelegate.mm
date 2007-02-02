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
 *   Simon Fraser <sfraser@netscape.com>
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

#import "NSMenu+Utils.h"
#import "NSPasteboard+Utils.h"

#import "HistoryItem.h"
#import "HistoryDataSource.h"

#import "ExtendedOutlineView.h"
#import "PreferenceManager.h"
#import "BrowserWindowController.h"
#import "BookmarkViewController.h"
#import "MainController.h"

#import "HistoryOutlineViewDelegate.h"

// sort popup menu items
enum
{
  kGroupingItemsTagMask     = (1 << 16),
  kGroupByDateItemTag       = 1,
  kGroupBySiteItemTag       = 2,
  kNoGroupingItemTag        = 3,
  
  kSortByItemsTagMask       = (1 << 17),
  kSortBySiteItemTag        = 1,
  kSortByURLItemTag         = 2,
  kSortByFirstVisitItemTag  = 3,
  kSortByLastVisitItemTag   = 4,
  
  kSortOrderItemsTagMask    = (1 << 18),
  kSortAscendingItemTag     = 1,
  kSortDescendingItemTag    = 2
};

static NSString* const kExpandedHistoryStatesDefaultsKey = @"history_expand_state";

@interface HistoryOutlineViewDelegate(Private)

- (void)historyChanged:(NSNotification *)notification;
- (HistoryDataSource*)historyDataSource;
- (void)recursiveDeleteItem:(HistoryItem*)item;
- (void)saveViewToPrefs;
- (void)updateSortMenuState;
- (BOOL)anyHistorySiteItemsSelected;

- (int)tagForSortColumnIdentifier:(NSString*)identifier;
- (NSString*)sortColumnIdentifierForTag:(int)tag;

- (int)tagForGrouping:(NSString*)grouping;
- (NSString*)groupingForTag:(int)tag;

- (int)tagForSortOrder:(BOOL)sortDescending;

- (NSMutableDictionary *)expandedStateDictionary;
- (void)saveExpandedStateDictionary;
- (BOOL)hasExpandedState:(HistoryItem*)inItem;
- (void)setStateOfItem:(HistoryItem*)inItem toExpanded:(BOOL)inExpanded;
- (void)restoreFolderExpandedStates;


@end

@implementation HistoryOutlineViewDelegate

- (void)dealloc
{
  [self saveExpandedStateDictionary];

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [mExpandedStates release];

  [super dealloc];
}

- (void)awakeFromNib
{
  // register for history change notifications. note that we only observe our data
  // source, as there may be more than one.
  [[NSNotificationCenter defaultCenter] addObserver:self
                                        selector:@selector(historyChanged:)
                                        name:kNotificationNameHistoryDataSourceChanged object:[self historyDataSource]];
}

- (void)setBrowserWindowController:(BrowserWindowController*)bwController
{
  mBrowserWindowController = bwController;
}

- (void)historyViewMadeVisible:(BOOL)visible
{
  if (visible)
  {
    if (!mHistoryLoaded)
    {
      NSString* historyView = [[NSUserDefaults standardUserDefaults] stringForKey:@"History View"];
      if (historyView)
        [[self historyDataSource] setHistoryView:historyView];

      [[self historyDataSource] loadLazily];
      
      if (![mHistoryOutlineView sortColumnIdentifier])
      {
        // these forward to the data source
        [mHistoryOutlineView setSortColumnIdentifier:@"last_visit"];
        [mHistoryOutlineView setSortDescending:YES];
      }
            
      mHistoryLoaded = YES;
    }
    
    mUpdatesDisabled = NO;
    [mHistoryOutlineView reloadData];
    [self restoreFolderExpandedStates];
    [self updateSortMenuState];
  }
  else
  {
    mUpdatesDisabled = YES;
  }
}

- (void)searchFor:(NSString*)searchString inFieldWithTag:(int)tag
{
  [[self historyDataSource] searchFor:searchString inFieldWithTag:tag];
}

- (void)clearSearchResults
{
  [mBookmarksViewController resetSearchField];
  [[self historyDataSource] clearSearchResults];
}

#pragma mark -

- (IBAction)openHistoryItem:(id)sender
{  
  NSArray* selectedHistoryItems = [mHistoryOutlineView selectedItems];
  if ([selectedHistoryItems count] == 0) return;
  
  // only do expand/collapse if just one item is selected
  id firstItem;
  if (([selectedHistoryItems count] == 1) &&
      (firstItem = [selectedHistoryItems objectAtIndex:0]) &&
      [mHistoryOutlineView isExpandable:firstItem])
  {
    if ([mHistoryOutlineView isItemExpanded: firstItem])
      [mHistoryOutlineView collapseItem:firstItem];
    else
      [mHistoryOutlineView expandItem:firstItem];

    return;
  }

  BOOL loadInBackground = [BrowserWindowController shouldLoadInBackground:sender];
  BOOL openInTabs       = [[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL];
  BOOL cmdKeyDown       = (([[NSApp currentEvent] modifierFlags] & NSCommandKeyMask) != 0);
  
  NSEnumerator* itemEnum = [selectedHistoryItems objectEnumerator];
  id curItem;
  while ((curItem = [itemEnum nextObject]))
  {
    // The history view obeys the app preference for cmd-click -> open in new window or tab
    if (![curItem isSiteItem]) continue;

    NSString* url = [curItem url];
    if (cmdKeyDown)
    {
      if (openInTabs)
        [mBrowserWindowController openNewTabWithURL:url referrer:nil loadInBackground:loadInBackground allowPopups:NO setJumpback:YES];
      else
        [mBrowserWindowController openNewWindowWithURL:url referrer: nil loadInBackground:loadInBackground allowPopups:NO];
    }
    else
      [[mBrowserWindowController getBrowserWrapper] loadURI:url referrer:nil flags:NSLoadFlagsNone focusContent:YES allowPopups:NO];
  }
}

- (IBAction)deleteHistoryItems:(id)sender
{
  // If just 1 row was selected, keep it so the user can delete again immediately
  BOOL clearSelectionWhenDone = ([mHistoryOutlineView numberOfSelectedRows] > 1);
  
  // make a list of doomed items first so the rows don't change under us
  NSArray* doomedItems = [mHistoryOutlineView selectedItems];
  if ([doomedItems count] == 0) return;

  // to avoid potentially many updates, disabled auto updating
  mUpdatesDisabled = YES;
  
  // catch exceptions to make sure we turn updating back on
  NS_DURING
    NSEnumerator* itemsEnum = [doomedItems objectEnumerator];
    HistoryItem* curItem;
    while ((curItem = [itemsEnum nextObject]))
    {
      [self recursiveDeleteItem:curItem];
    }
  NS_HANDLER
  NS_ENDHANDLER
  
  if (clearSelectionWhenDone)
    [mHistoryOutlineView deselectAll:self];

  mUpdatesDisabled = NO;
  [mHistoryOutlineView reloadData];
  [self restoreFolderExpandedStates];
}


// called from context menu, assumes represented object has been set
- (IBAction)openHistoryItemInNewWindow:(id)aSender
{
  NSArray* itemsArray = [mHistoryOutlineView selectedItems];

  BOOL backgroundLoad = [BrowserWindowController shouldLoadInBackground:aSender];

  NSEnumerator* itemsEnum = [itemsArray objectEnumerator];
  HistoryItem* curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    if ([curItem isKindOfClass:[HistorySiteItem class]])
      [mBrowserWindowController openNewWindowWithURL:[curItem url] referrer:nil loadInBackground:backgroundLoad allowPopups:NO];
  }
}

// called from context menu, assumes represented object has been set
- (IBAction)openHistoryItemInNewTab:(id)aSender
{
  NSArray* itemsArray = [mHistoryOutlineView selectedItems];

  BOOL backgroundLoad = [BrowserWindowController shouldLoadInBackground:aSender];

  NSEnumerator* itemsEnum = [itemsArray objectEnumerator];
  HistoryItem* curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    if ([curItem isKindOfClass:[HistorySiteItem class]])
      [mBrowserWindowController openNewTabWithURL:[curItem url] referrer:nil loadInBackground:backgroundLoad allowPopups:NO setJumpback:YES];
  }
}

- (IBAction)openHistoryItemsInTabsInNewWindow:(id)aSender
{
  NSArray* itemsArray = [mHistoryOutlineView selectedItems];

  // make url array
  NSMutableArray* urlArray = [NSMutableArray arrayWithCapacity:[itemsArray count]];

  NSEnumerator* itemsEnum = [itemsArray objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject])) {
    if ([curItem isKindOfClass:[HistorySiteItem class]])
      [urlArray addObject:[curItem url]];
  }

  // make new window
  BOOL loadInBackground = [BrowserWindowController shouldLoadInBackground:aSender];
  NSWindow* behindWindow = nil;
  if (loadInBackground)
    behindWindow = [mBrowserWindowController window];

  [[NSApp delegate] openBrowserWindowWithURLs:urlArray behind:behindWindow allowPopups:NO];
}

- (IBAction)copyURLs:(id)aSender
{
  [self copyHistoryURLs:[mHistoryOutlineView selectedItems] toPasteboard:[NSPasteboard generalPasteboard]];
}


#pragma mark -

- (IBAction)groupByDate:(id)sender
{
  [self clearSearchResults];
  [[self historyDataSource] setHistoryView:kHistoryViewByDate];
  [self updateSortMenuState];
  [self saveViewToPrefs];
}

- (IBAction)groupBySite:(id)sender
{
  [self clearSearchResults];
  [[self historyDataSource] setHistoryView:kHistoryViewBySite];
  [self updateSortMenuState];
  [self saveViewToPrefs];
}

- (IBAction)setNoGrouping:(id)sender
{
  [self clearSearchResults];
  [[self historyDataSource] setHistoryView:kHistoryViewFlat];
  [self updateSortMenuState];
  [self saveViewToPrefs];
}

- (IBAction)sortBy:(id)sender
{
  [mHistoryOutlineView setSortColumnIdentifier:[self sortColumnIdentifierForTag:[sender tagRemovingMask:kSortByItemsTagMask]]];
  [self updateSortMenuState];
}

- (IBAction)sortAscending:(id)sender
{
  [mHistoryOutlineView setSortDescending:NO];
  [self updateSortMenuState];
}

- (IBAction)sortDescending:(id)sender
{
  [mHistoryOutlineView setSortDescending:YES];
  [self updateSortMenuState];
}

#pragma mark -

// maybe we can push some of this logic down into the ExtendedOutlineView
- (void)outlineView:(NSOutlineView *)outlineView didClickTableColumn:(NSTableColumn *)tableColumn
{
  if ([[mHistoryOutlineView sortColumnIdentifier] isEqualToString:[tableColumn identifier]])
  {
    [mHistoryOutlineView setSortDescending:![mHistoryOutlineView sortDescending]];
  }
  else
  {
    [mHistoryOutlineView setSortColumnIdentifier:[tableColumn identifier]];
  }

  [self updateSortMenuState];
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(NSCell *)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
  if ([[tableColumn identifier] isEqualToString:@"title"])
    [cell setImage:[item iconAllowingLoad:YES]];
}

#if 0
// implementing this makes NSOutlineView updates much slower (because of all the hover region maintenance)
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)item
{
  return nil;
}
#endif

- (NSMenu *)outlineView:(NSOutlineView *)outlineView contextMenuForItems:(NSArray*)items
{
  unsigned int numSiteItems = 0;

  NSEnumerator* itemsEnum = [items objectEnumerator];
  id curItem;
  while ((curItem = [itemsEnum nextObject]))
  {
    if ([curItem isKindOfClass:[HistorySiteItem class]])
      ++numSiteItems;
  }
  
  if (numSiteItems == 0)
    return [outlineView menu];

  NSMenu*     contextMenu = [[[NSMenu alloc] initWithTitle:@"notitle"] autorelease];
  NSMenuItem* menuItem = nil;
  NSMenuItem* shiftMenuItem = nil;
  NSString*   menuTitle = nil;

  if (numSiteItems > 1)
    menuTitle = NSLocalizedString(@"Open in New Windows", @"");
  else
    menuTitle = NSLocalizedString(@"Open in New Window", @"");
  menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openHistoryItemInNewWindow:) keyEquivalent:@""] autorelease];
  [menuItem setTarget:self];
  [contextMenu addItem:menuItem];

  shiftMenuItem = [NSMenuItem alternateMenuItemWithTitle:menuTitle
                                                  action:@selector(openHistoryItemInNewWindow:)
                                                  target:self
                                               modifiers:([menuItem keyEquivalentModifierMask] | NSShiftKeyMask)];
  [contextMenu addItem:shiftMenuItem];

  if (numSiteItems > 1)
    menuTitle = NSLocalizedString(@"Open in New Tabs", @"");
  else
    menuTitle = NSLocalizedString(@"Open in New Tab", @"");
  menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openHistoryItemInNewTab:) keyEquivalent:@""] autorelease];
  [menuItem setTarget:self];
  [contextMenu addItem:menuItem];

  shiftMenuItem = [NSMenuItem alternateMenuItemWithTitle:menuTitle
                                                  action:@selector(openHistoryItemInNewTab:)
                                                  target:self
                                               modifiers:([menuItem keyEquivalentModifierMask] | NSShiftKeyMask)];
  [contextMenu addItem:shiftMenuItem];

  if (numSiteItems > 1)
  {
    menuTitle = NSLocalizedString(@"Open in Tabs in New Window", @"");
    menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(openHistoryItemsInTabsInNewWindow:) keyEquivalent:@""] autorelease];
    [menuItem setTarget:self];
    [contextMenu addItem:menuItem];

    shiftMenuItem = [NSMenuItem alternateMenuItemWithTitle:menuTitle
                                                    action:@selector(openHistoryItemsInTabsInNewWindow:)
                                                    target:self
                                                 modifiers:([menuItem keyEquivalentModifierMask] | NSShiftKeyMask)];
    [contextMenu addItem:shiftMenuItem];
  }

  // space
  [contextMenu addItem:[NSMenuItem separatorItem]];

  // delete
  menuTitle = NSLocalizedString(@"Delete", @"");
  menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(deleteHistoryItems:) keyEquivalent:@""] autorelease];
  [menuItem setTarget:self];
  [contextMenu addItem:menuItem];

  // space
  [contextMenu addItem:[NSMenuItem separatorItem]];

  if (numSiteItems > 1)
    menuTitle = NSLocalizedString(@"Copy URLs to Clipboard", @"");
  else
    menuTitle = NSLocalizedString(@"Copy URL to Clipboard", @"");
  menuItem = [[[NSMenuItem alloc] initWithTitle:menuTitle action:@selector(copyURLs:) keyEquivalent:@""] autorelease];
  [menuItem setTarget:self];
  [contextMenu addItem:menuItem];

  return contextMenu;
}

- (unsigned int)outlineView:(NSOutlineView *)outlineView draggingSourceOperationMaskForLocal:(BOOL)localFlag
{
  if (localFlag)
    return NSDragOperationGeneric;

  return (NSDragOperationGeneric | NSDragOperationCopy | NSDragOperationLink);
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

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem
{
  // Window actions are disabled while a sheet is showing
  if ([[mBrowserWindowController window] attachedSheet])
    return NO;  

  SEL action = [menuItem action];

  if (action == @selector(openHistoryItem:) ||
      action == @selector(openHistoryItemInNewWindow:) ||
      action == @selector(openHistoryItemInNewTab:) ||
      action == @selector(openHistoryItemsInTabsInNewWindow:) ||
      action == @selector(deleteHistoryItems:) ||
      action == @selector(copyURLs:))
  {
    return [self anyHistorySiteItemsSelected];
  }

  return YES;
}

- (void)historyChanged:(NSNotification *)notification
{
  if (mUpdatesDisabled) return;

  id rootChangedItem   = [[notification userInfo] objectForKey:kNotificationHistoryDataSourceChangedUserInfoChangedItem];
  BOOL itemOnlyChanged = [[[notification userInfo] objectForKey:kNotificationHistoryDataSourceChangedUserInfoChangedItemOnly] boolValue];

  if (rootChangedItem)
    [mHistoryOutlineView reloadItem:rootChangedItem reloadChildren:!itemOnlyChanged];
  else
  {
    [mHistoryOutlineView reloadData];
    [self restoreFolderExpandedStates];
  }
}

- (void)recursiveDeleteItem:(HistoryItem*)item
{
  if ([item isKindOfClass:[HistorySiteItem class]])
  {
    [[self historyDataSource] removeItem:(HistorySiteItem*)item];
  }
  else if ([item isKindOfClass:[HistoryCategoryItem class]])
  {
    // clone the child list to avoid it changing under us
    NSArray* itemChildren = [NSArray arrayWithArray:[item children]];
    
    NSEnumerator* childEnum = [itemChildren objectEnumerator];
    HistoryItem* childItem;
    while ((childItem = [childEnum nextObject]))
    {
      [self recursiveDeleteItem:childItem];
    }
  }
}

//
// Copy a set of history URLs to the specified pasteboard.
// We don't care about item titles here, nor do we care about format.
//
- (void) copyHistoryURLs:(NSArray*)historyItemsToCopy toPasteboard:(NSPasteboard*)aPasteboard
{
  // handle URLs, and nothing else, for simplicity.
  [aPasteboard declareTypes:[NSArray arrayWithObject:kCorePasteboardFlavorType_url] owner:self];

  NSMutableArray* urlList = [NSMutableArray array];
  NSEnumerator* historyItemsEnum = [historyItemsToCopy objectEnumerator];
  HistoryItem* curItem;
  while (curItem = [historyItemsEnum nextObject])
  {
    if ([curItem isKindOfClass:[HistorySiteItem class]]) {
      [urlList addObject:[(HistorySiteItem*)curItem url]];
    }
  }
  [aPasteboard setURLs:urlList withTitles:nil];
}


- (HistoryDataSource*)historyDataSource
{
  return (HistoryDataSource*)[mHistoryOutlineView dataSource];
}

- (void)saveViewToPrefs
{
  NSString* historyView = [[self historyDataSource] historyView];
  [[NSUserDefaults standardUserDefaults] setObject:historyView forKey:@"History View"];
}

- (BOOL)anyHistorySiteItemsSelected
{
  NSIndexSet* selectedIndexes = [mHistoryOutlineView selectedRowIndexes];
  unsigned int currentRow = [selectedIndexes firstIndex];

  while (currentRow != NSNotFound) {
    if ([[mHistoryOutlineView itemAtRow:currentRow] isKindOfClass:[HistorySiteItem class]])
      return YES;

    currentRow = [selectedIndexes indexGreaterThanIndex:currentRow];
  }

  return NO;
}

- (void)updateSortMenuState
{
  HistoryDataSource* dataSource = [self historyDataSource];
  
  // grouping items
  [mHistorySortMenu checkItemWithTag:[self tagForGrouping:[dataSource historyView]] inGroupWithMask:kGroupingItemsTagMask];

  // sorting items
  [mHistorySortMenu checkItemWithTag:[self tagForSortColumnIdentifier:[dataSource sortColumnIdentifier]] inGroupWithMask:kSortByItemsTagMask];

  // ascending/descending
  [mHistorySortMenu checkItemWithTag:[self tagForSortOrder:[dataSource sortDescending]] inGroupWithMask:kSortOrderItemsTagMask];
}


- (int)tagForSortColumnIdentifier:(NSString*)identifier
{
  if ([identifier isEqualToString:@"last_visit"])
    return kSortByLastVisitItemTag;

  if ([identifier isEqualToString:@"first_visit"])
    return kSortByFirstVisitItemTag;

  if ([identifier isEqualToString:@"title"])
    return kSortBySiteItemTag;

  if ([identifier isEqualToString:@"url"])
    return kSortByURLItemTag;
    
  return 0;
}

// tag should be unmasked
- (NSString*)sortColumnIdentifierForTag:(int)tag
{
  switch (tag)
  {
    case kSortBySiteItemTag:         return @"title";
    case kSortByURLItemTag:          return @"url";
    case kSortByFirstVisitItemTag:   return @"first_visit";
    default:
    case kSortByLastVisitItemTag:    return @"last_visit";
  }
  return @""; // quiet warning
}

- (int)tagForGrouping:(NSString*)grouping
{
  if ([grouping isEqualToString:kHistoryViewByDate])
    return kGroupByDateItemTag;
  else if ([grouping isEqualToString:kHistoryViewBySite])
    return kGroupBySiteItemTag;
  else
    return kNoGroupingItemTag;
}

// tag should be unmasked
- (NSString*)groupingForTag:(int)tag
{
  switch (tag)
  {
    default:
    case kGroupByDateItemTag:   return kHistoryViewByDate;
    case kGroupBySiteItemTag:   return kHistoryViewBySite;
    case kNoGroupingItemTag:    return kHistoryViewFlat;
  }
  return @""; // quiet warning
}

- (int)tagForSortOrder:(BOOL)sortDescending
{
  return (sortDescending) ? kSortDescendingItemTag : kSortAscendingItemTag;
}

#pragma mark -

- (NSMutableDictionary *)expandedStateDictionary
{
  if (!mExpandedStates)
  {
    mExpandedStates = [[[NSUserDefaults standardUserDefaults] dictionaryForKey:kExpandedHistoryStatesDefaultsKey] mutableCopy];
    if (!mExpandedStates)
      mExpandedStates = [[NSMutableDictionary alloc] initWithCapacity:20];
  }
  return mExpandedStates;
}

- (void)saveExpandedStateDictionary
{
  if (mExpandedStates)
    [[NSUserDefaults standardUserDefaults] setObject:mExpandedStates forKey:kExpandedHistoryStatesDefaultsKey];
}

- (BOOL)hasExpandedState:(HistoryItem*)inItem
{
  NSMutableDictionary *dict = [self expandedStateDictionary];
  return [[dict objectForKey:[inItem identifier]] boolValue];
}

- (void)setStateOfItem:(HistoryItem*)inItem toExpanded:(BOOL)inExpanded
{
  NSMutableDictionary *dict = [self expandedStateDictionary];
  if (inExpanded)
    [dict setObject:[NSNumber numberWithBool:YES] forKey:[inItem identifier]];
  else
    [dict removeObjectForKey:[inItem identifier]];
}

- (void)restoreFolderExpandedStates
{
  int curRow = 0;
  while (curRow < [mHistoryOutlineView numberOfRows])
  {
    id item = [mHistoryOutlineView itemAtRow:curRow];
    if ([item isKindOfClass:[HistoryCategoryItem class]])
    {
      if ([self hasExpandedState:item])
        [mHistoryOutlineView expandItem: item];
      else
        [mHistoryOutlineView collapseItem: item];
    }
    curRow ++;
  }
}

@end
