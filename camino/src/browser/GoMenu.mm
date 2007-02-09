/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
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

#import "NSString+Utils.h"
#import "NSDate+Utils.h"
#import "NSMenu+Utils.h"

#import "MainController.h"
#import "BrowserWindowController.h"
#import "ChimeraUIConstants.h"
#import "CHBrowserService.h"
#import "PreferenceManager.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIWebBrowser.h"
#include "nsISHistory.h"
#include "nsIWebNavigation.h"
#include "nsIHistoryEntry.h"
#include "nsCRT.h"

#import "HistoryItem.h"
#import "HistoryDataSource.h"

#import "GoMenu.h"



// the maximum number of history entry menuitems to display
static const int kMaxNumHistoryItems = 100;

// the maximum number of "today" items to show on the main menu
static const int kMaxTodayItems = 12;

// the maximum number of characters in a menu title before cropping it
static const unsigned int kMaxTitleLength = 50;

// this little class manages the singleton history data source, and takes
// care of shutting it down at XPCOM shutdown time.
@interface GoMenuHistoryDataSourceOwner : NSObject
{
  HistoryDataSource*    mHistoryDataSource;
}

+ (GoMenuHistoryDataSourceOwner*)sharedGoMenuHistoryDataSourceOwner;
+ (HistoryDataSource*)sharedHistoryDataSource;    // just a shortcut

- (HistoryDataSource*)historyDataSource;

@end


@implementation GoMenuHistoryDataSourceOwner

+ (GoMenuHistoryDataSourceOwner*)sharedGoMenuHistoryDataSourceOwner
{
  static GoMenuHistoryDataSourceOwner* sHistoryOwner = nil;
  if (!sHistoryOwner)
    sHistoryOwner = [[GoMenuHistoryDataSourceOwner alloc] init];
  
  return sHistoryOwner;
}

+ (HistoryDataSource*)sharedHistoryDataSource
{
  return [[GoMenuHistoryDataSourceOwner sharedGoMenuHistoryDataSourceOwner] historyDataSource];
}

- (id)init
{
  if ((self = [super init]))
  {
    // register for xpcom shutdown
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(xpcomShutdownNotification:)
                                                 name:XPCOMShutDownNotificationName
                                               object:nil];
  }
  return self;
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mHistoryDataSource release];
  [super dealloc];
}

- (void)xpcomShutdownNotification:(NSNotification*)inNotification
{
  [mHistoryDataSource release];
  mHistoryDataSource = nil;
}

- (HistoryDataSource*)historyDataSource
{
  if (!mHistoryDataSource)
  {
    mHistoryDataSource = [[HistoryDataSource alloc] init];
    [mHistoryDataSource setHistoryView:kHistoryViewByDate];
    [mHistoryDataSource setSortColumnIdentifier:@"last_visit"]; // always sort by last visit
    [mHistoryDataSource setSortDescending:YES];
  }
  return mHistoryDataSource;
}

@end // GoMenuHistoryDataSourceOwner


#pragma mark -

@interface HistoryMenu(Private)

+ (NSString*)menuItemTitleForHistoryItem:(HistoryItem*)inItem;

- (void)setupHistoryMenu;
- (void)rebuildHistoryItems;
- (void)historyChanged:(NSNotification*)inNotification;
- (void)menuWillDisplay:(NSNotification*)inNotification;
- (void)openHistoryItem:(id)sender;

@end

#pragma mark -

@implementation HistoryMenu

+ (NSString*)menuItemTitleForHistoryItem:(HistoryItem*)inItem
{
  NSString* itemTitle = [inItem title];
  if ([itemTitle length] == 0)
    itemTitle = [inItem url];

  return [itemTitle stringByTruncatingTo:kMaxTitleLength at:kTruncateAtMiddle];
}

- (id)initWithTitle:(NSString *)inTitle
{
  if ((self = [super initWithTitle:inTitle]))
  {
    mHistoryItemsDirty = YES;
    // we're making the assumption here that the Go menu only gets awakeFromNib,
    // and that other history submenus are allocated dynamically later on
    [self setupHistoryMenu];
  }
  return self;
}

- (void)awakeFromNib
{
  mHistoryItemsDirty = YES;
}

// this should only be called after app launch, when the data source is available
- (void)setupHistoryMenu
{
  // set ourselves up to listen for history changes
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(historyChanged:)
                                               name:kNotificationNameHistoryDataSourceChanged
                                             object:[GoMenuHistoryDataSourceOwner sharedHistoryDataSource]];

  // register for menu display
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(menuWillDisplay:)
                                               name:NSMenuWillDisplayNotification
                                             object:nil];
}

- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [mRootItem release];
  [mAdditionalItemsParent release];
  [super dealloc];
}

- (void)setRootHistoryItem:(HistoryItem*)inRootItem
{
  [mRootItem autorelease];
  mRootItem = [inRootItem retain];
}

- (HistoryItem*)rootItem
{
  return mRootItem;
}

- (void)setNumLeadingItemsToIgnore:(int)inIgnoreItems
{
  mNumIgnoreItems = inIgnoreItems;
}

- (int)numLeadingItemsToIgnore
{
  return mNumIgnoreItems;
}

- (void)setItemBeforeHistoryItems:(NSMenuItem*)inItem
{
  mItemBeforeHistoryItems = inItem;
}

- (NSMenuItem*)itemBeforeHistoryItems
{
  return mItemBeforeHistoryItems;
}

- (void)historyChanged:(NSNotification*)inNotification
{
  id rootChangedItem   =  [[inNotification userInfo] objectForKey:kNotificationHistoryDataSourceChangedUserInfoChangedItem];
  // We could optimize by only changing single menu items if itemOnlyChanged is true. Normally this will also be a visit
  // date change, which we can ignore.
  //BOOL itemOnlyChanged = [[[inNotification userInfo] objectForKey:kNotificationHistoryDataSourceChangedUserInfoChangedItemOnly] boolValue];
  
  // If rootChangedItem is nil, the whole history tree is being rebuilt.
  // We need to clear our root item, because it will become invalid. We'll set it again when we rebuild.
  if (!rootChangedItem)
  {
    [self setRootHistoryItem:nil];
    [mAdditionalItemsParent release];
    mAdditionalItemsParent = nil;
    mHistoryItemsDirty = YES;
  }
  else
  {
    mHistoryItemsDirty = mRootItem == rootChangedItem ||
                        [mRootItem isDescendentOfItem:rootChangedItem] ||
                        [rootChangedItem isDescendentOfItem:mRootItem];

    if (mAdditionalItemsParent)
      mHistoryItemsDirty |= (rootChangedItem == mAdditionalItemsParent ||
                            [rootChangedItem parentItem] == mAdditionalItemsParent);
  }
}

- (void)menuWillDisplay:(NSNotification*)inNotification
{
  if ([self isTargetOfMenuDisplayNotification:[inNotification object]])
  {
    [self menuWillBeDisplayed];
  }
}

- (void)rebuildHistoryItems
{
  if (!mHistoryItemsDirty)
    return;

  // remove everything after the "before" item
  [self removeItemsAfterItem:mItemBeforeHistoryItems];
  
  // now iterate through the history items
  NSEnumerator* childEnum = [[mRootItem children] objectEnumerator];

  // skip the first mNumIgnoreItems items
  for (int i = 0; i < mNumIgnoreItems; ++i)
    [childEnum nextObject];
  
  BOOL separatorPending = NO;
  
  HistoryItem* curChild;
  while ((curChild = [childEnum nextObject]))
  {
    NSMenuItem* newItem = nil;

    if ([curChild isKindOfClass:[HistorySiteItem class]])
    {
      newItem = [[[NSMenuItem alloc] initWithTitle:[HistoryMenu menuItemTitleForHistoryItem:curChild]
                                            action:@selector(openHistoryItem:)
                                     keyEquivalent:@""] autorelease];

      [newItem setImage:[curChild iconAllowingLoad:NO]];
      [newItem setTarget:self];
      [newItem setRepresentedObject:curChild];
      [self addItem:newItem];

      [self addCommandKeyAlternatesForMenuItem:newItem];
    }
    else if ([curChild isKindOfClass:[HistoryCategoryItem class]] && ([curChild numberOfChildren] > 0))
    {
      // The "today" category is special, because we hoist the 10 most recent items up to the parent menu.
      // This code assumes that we'll hit this first, by virtue of the last visit date sorting.
      if ([curChild respondsToSelector:@selector(isTodayCategory)] && [(HistoryDateCategoryItem*)curChild isTodayCategory])
      {
        // take note of the fact that we're showing children of this item
        [mAdditionalItemsParent autorelease];
        mAdditionalItemsParent = [curChild retain];

        // put the kMaxTodayItems most recent items into this menu (which is the go menu)
        NSMutableArray* menuTodayItems = [NSMutableArray arrayWithArray:[curChild children]];
        while ((int)[menuTodayItems count] > kMaxTodayItems)
          [menuTodayItems removeObjectAtIndex:kMaxTodayItems];

        NSEnumerator* todayItemsEnum = [menuTodayItems objectEnumerator];
        HistoryItem* curTodayItem;
        while ((curTodayItem = [todayItemsEnum nextObject]))
        {
          NSMenuItem* todayMenuItem = [[[NSMenuItem alloc] initWithTitle:[HistoryMenu menuItemTitleForHistoryItem:curTodayItem]
                                                                  action:@selector(openHistoryItem:)
                                                           keyEquivalent:@""] autorelease];

          [todayMenuItem setImage:[curTodayItem iconAllowingLoad:NO]];
          [todayMenuItem setTarget:self];
          [todayMenuItem setRepresentedObject:curTodayItem];
          [self addItem:todayMenuItem];

          [self addCommandKeyAlternatesForMenuItem:todayMenuItem];

          separatorPending = YES;
        }
        
        // and make a submenu for "earlier today"
        if ([curChild numberOfChildren] > kMaxTodayItems)
        {
          if (separatorPending)
          {
            [self addItem:[NSMenuItem separatorItem]];
            separatorPending = NO;
          }

          NSString* itemTitle = NSLocalizedString(@"GoMenuEarlierToday", @"");
          newItem = [[[NSMenuItem alloc] initWithTitle:itemTitle
                                                action:nil
                                         keyEquivalent:@""] autorelease];
          [newItem setImage:[curChild iconAllowingLoad:NO]];

          HistoryMenu* newSubmenu = [[HistoryMenu alloc] initWithTitle:itemTitle];
          [newSubmenu setRootHistoryItem:curChild];
          [newSubmenu setNumLeadingItemsToIgnore:kMaxTodayItems];

          [newItem setSubmenu:newSubmenu];
          [self addItem:newItem];
        }
      }
      else
      {
        if (separatorPending)
        {
          [self addItem:[NSMenuItem separatorItem]];
          separatorPending = NO;
        }

        // Not the "today" category, and has entries
        NSString* itemTitle = [HistoryMenu menuItemTitleForHistoryItem:curChild];
        newItem = [[[NSMenuItem alloc] initWithTitle:itemTitle
                                              action:nil
                                       keyEquivalent:@""] autorelease];
        [newItem setImage:[curChild iconAllowingLoad:NO]];

        HistoryMenu* newSubmenu = [[HistoryMenu alloc] initWithTitle:itemTitle];
        [newSubmenu setRootHistoryItem:curChild];
        
        [newItem setSubmenu:newSubmenu];
        [self addItem:newItem];
      }
    }
    
    // if we're not the Go menu, stop after kMaxNumHistoryItems items
    if (![self isKindOfClass:[GoMenu class]] && ([self numberOfItems] == kMaxNumHistoryItems))
      break;
  }
  
  [self addLastItems];
  
  mHistoryItemsDirty = NO;
}

- (void)addLastItems
{
  if ([self numberOfItems] >= kMaxNumHistoryItems)
  {
    // this will only be called for submenus
    [self addItem:[NSMenuItem separatorItem]];
    NSMenuItem* showMoreItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"ShowMoreHistoryMenuItem", @"")
                                                           action:@selector(showHistory:)
                                                    keyEquivalent:@""] autorelease];
    [showMoreItem setRepresentedObject:mRootItem];
    [self addItem:showMoreItem];
  }
}

- (void)menuWillBeDisplayed
{
  [self rebuildHistoryItems];
}

- (BOOL)validateMenuItem:(NSMenuItem*)aMenuItem
{
  BrowserWindowController* browserController = [(MainController *)[NSApp delegate] getMainWindowBrowserController];
  SEL action = [aMenuItem action];

  // disable history if a sheet is up
  if (browserController && [[browserController window] attachedSheet] &&
      (action == @selector(openHistoryItem:)))
  {
    return NO;
  }

  return YES;
}

- (void)openHistoryItem:(id)sender
{
  id repObject = [sender representedObject];
  if ([repObject isKindOfClass:[HistoryItem class]])
  {
    NSString* itemURL = [repObject url];
    
    // XXX share this logic with MainController and HistoryOutlineViewDelegate
    BrowserWindowController* bwc = [(MainController *)[NSApp delegate] getMainWindowBrowserController];
    if (bwc)
    {
      if ([sender keyEquivalentModifierMask] & NSCommandKeyMask)
      {
        BOOL backgroundLoad = [BrowserWindowController shouldLoadInBackground:sender];
        if ([[PreferenceManager sharedInstance] getBooleanPref:"browser.tabs.opentabfor.middleclick" withSuccess:NULL])
          [bwc openNewTabWithURL:itemURL referrer:nil loadInBackground:backgroundLoad allowPopups:NO setJumpback:NO];
        else
          [bwc openNewWindowWithURL:itemURL referrer:nil loadInBackground:backgroundLoad allowPopups:NO];
      }
      else
      {
        [bwc loadURL:itemURL];
      }
    }
    else
      [(MainController *)[NSApp delegate] openBrowserWindowWithURL:itemURL andReferrer:nil behind:nil allowPopups:NO];
  }
}

@end


#pragma mark -

@interface GoMenu(Private)

- (void)appLaunchFinished:(NSNotification*)inNotification;

@end

@implementation GoMenu


- (void)dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)awakeFromNib
{
  [super awakeFromNib];

  // listen for app launch completion
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(appLaunchFinished:)
                                               name:NSApplicationDidFinishLaunchingNotification
                                             object:nil];
}

- (void)appLaunchFinished:(NSNotification*)inNotification
{
  mAppLaunchDone = YES;
  // setup the history menu after a delay, so that other app launch stuff
  // finishes first
  [self performSelector:@selector(setupHistoryMenu) withObject:nil afterDelay:0];
}

- (void)menuWillBeDisplayed
{
  if (mAppLaunchDone)
  {
    // the root item is nil at launch, and if the history gets totally rebuilt
    if (!mRootItem)
    {
      HistoryDataSource* dataSource = [GoMenuHistoryDataSourceOwner sharedHistoryDataSource];
      [dataSource loadLazily];
      
      mRootItem = [[dataSource rootItem] retain];
    }
  }
  
  [super menuWillBeDisplayed];
}

- (void)addLastItems
{
  // at the bottom of the go menu, add a Clear History item
  if ([[mRootItem children] count] > 0)
    [self addItem:[NSMenuItem separatorItem]];

  NSMenuItem* clearHistoryItem = [[[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"ClearHistoryMenuItem", @"")
                                                             action:@selector(clearHistory:)
                                                      keyEquivalent:@""] autorelease];
  [self addItem:clearHistoryItem];
}

@end
