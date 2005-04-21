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

#import "GoMenu.h"
#import "MainController.h"
#import "BrowserWindowController.h"
#import "ChimeraUIConstants.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsIBrowserHistory.h"
#include "nsIHistoryItems.h"
#include "nsISimpleEnumerator.h"
#include "nsIServiceManager.h"

// the maximum number of history entry menuitems to display
//static const int kMaxItems = 15;
// the maximum number of characters in a menu title before cropping it
static const unsigned int kMaxTitleLength = 80;


//
// HistoryMenuItem
//
// An object that passes some data between the menu items and the history data source. This
// item lives longer than the lifetime of the tracking of the menu.
//

@interface HistoryMenuItem : NSObject
{
@private
  NSString* mURL;    // strong
  NSString* mTitle;  // strong
  PRTime    mLastVisit;
}
-(id)initWithURL:(NSString*)inURL title:(NSString*)inTitle lastVisit:(PRTime)inLastVisit;
-(NSString*)url;
-(NSString*)title;
@end

@implementation HistoryMenuItem

-(id)initWithURL:(NSString*)inURL title:(NSString*)inTitle lastVisit:(PRTime)inLastVisit
{
  if ((self = [super init])) {
    mURL = [inURL retain];
    mTitle = [inTitle retain];
    mLastVisit = inLastVisit;
  }
  return self;
}

- (void)dealloc
{
  [mURL release];
  [mTitle release];
}

-(NSString*)url
{
  return mURL;
}

-(NSString*)title
{
  return mTitle;
}

- (NSComparisonResult)compare:(HistoryMenuItem *)inItem
{
  if (inItem->mLastVisit > mLastVisit)
    return NSOrderedAscending;
  if (inItem->mLastVisit < mLastVisit)
    return NSOrderedDescending;
  return NSOrderedSame;
}

@end

#pragma mark -

@implementation GoMenu

- (void)dealloc
{
  [mBuckets autorelease];
  [super dealloc];
}

- (void) update
{
  [self emptyHistoryItems];
  [self addHistoryItems];

  [super update];
}

- (void) historyItemAction:(id)sender
{
  NSString* urlToLoad = [[sender representedObject] url];
  BrowserWindowController* bwc = [(MainController *)[NSApp delegate] getMainWindowBrowserController];
  if (bwc)
    [bwc loadURL:urlToLoad referrer:nil activate:YES allowPopups:NO];
  else
    [(MainController *)[NSApp delegate] openNewWindowOrTabWithURL:urlToLoad andReferrer:nil];
}

- (void) emptyHistoryItems
{ 
  // remove every history item after the insertion point
  int insertionIndex = [self indexOfItemWithTag:kDividerTag];
  for (int i = [self numberOfItems]-1; i > insertionIndex ; --i) {
    [self removeItemAtIndex:i];
  }
  [mBuckets autorelease];
}

- (void) addHistoryItems
{
  // first grab all the items in the history and put them into buckets based on
  // the last visit date. we'll then sort each bucket. each bucket becomes a menu.
  // XXX if there is just one menu and there are less than, say, 10 items, show them inline.
  // XXX optimize by building once, then saving the results and observing additions/removals
  
  // |mBuckets| is a dictionary of arrays. Each array represents a menu of all the
  // websites for a given day in sorted order. The key for the array is the corresponding
  // NSDate for that day. This lives until the menu is rebuilt so we don't have any
  // lifetime issues.
  mBuckets = [[NSMutableDictionary dictionary] retain];

  nsCOMPtr<nsIBrowserHistory> globalHist = do_GetService("@mozilla.org/browser/global-history;2");
  nsCOMPtr<nsIHistoryItems> history = do_QueryInterface(globalHist);
  if (history) {
    // walk the list of history items placing them into buckets by day.
    nsCOMPtr<nsISimpleEnumerator> items;
    history->GetItemEnumerator(getter_AddRefs(items));
    PRBool hasMore = PR_FALSE;
    while (NS_SUCCEEDED(items->HasMoreElements(&hasMore)) && hasMore) {
      nsCOMPtr<nsISupports> supp;
      if (NS_FAILED(items->GetNext(getter_AddRefs(supp))))
        break;
      nsCOMPtr<nsIHistoryItem> historyItem = do_QueryInterface(supp);

      // the key for the elements of |mBuckets| is the date with the hours/minutes/seconds
      // stripped off so that all visits that fall on the same day will map down to the same hash key.
      // we then map back to a date object so it can be used to display a pretty date and sorted below.
      PRTime lastVisit;
      historyItem->GetLastVisitDate(&lastVisit);
      NSDate* dateKey = [NSDate dateWithString:
                          [[NSDate dateWithPRTime:lastVisit] descriptionWithCalendarFormat:@"%Y-%m-%d 00:00:00 +0000"
                                                                timeZone:nil locale:nil]];
      NSMutableArray* bucket = [mBuckets objectForKey:dateKey];
      if (!bucket) {
        bucket = [NSMutableArray array];
        [mBuckets setObject:bucket forKey:dateKey];
      }
      
      // create the HistoryMenuItem which we store until the next time the menu is built. The ordering
      // of the array isn't really useful for us, we'll have to sort it later.
      nsCString url;
      historyItem->GetURL(url);
      NSString* urlStr = [NSString stringWith_nsACString:url];
      nsString title;
      historyItem->GetTitle(title);
      NSString* titleStr = [NSString stringWith_nsAString:title];
      HistoryMenuItem* menuObject = [[[HistoryMenuItem alloc] initWithURL:urlStr title:titleStr lastVisit:lastVisit] autorelease];
      [bucket addObject:menuObject];
    }
  }
  
  // Now that the buckets of days are built, we need to build up the actual menu. First sort the keys then
  // walk them in order, building the top level items and their submenus
  
  NSArray* sortedKeys = [[mBuckets allKeys] sortedArrayUsingSelector:@selector(compare:)];
  NSEnumerator* e = [sortedKeys reverseObjectEnumerator];  // most recent at the top
  NSDate* dateKey = nil;
  while ((dateKey = [e nextObject])) {
    // we sort the items in the bucket which sorts them by the full last visit date. Since we want the
    // most recent (highest time) at the top, we need to use a reverse enumerator below as well.
    NSMutableArray* bucket = [[mBuckets objectForKey:dateKey] sortedArrayUsingSelector:@selector(compare:)];
    
    // create a toplevel menu with the date for |dateKey|
    NSString* menuTitle = [dateKey descriptionWithCalendarFormat:NSLocalizedString(@"HistoryMenuDateFormat",nil) timeZone:nil locale:nil];
    NSMenuItem* newItem = [self addItemWithTitle:menuTitle action:nil keyEquivalent:@""];

    // build the child menu with all the history urls for this day We tie back to the url for the action by
    // setting the represented object on the menu item (it's a weak ref, but means that the
    // |menuObject| must outlive the duration of the time the menu is being tracked, which it 
    // certainly is).
    NSMenu* childMenu = [[[NSMenu allocWithZone:[NSMenu menuZone]] initWithTitle:@""] autorelease];
    NSEnumerator* e2 = [bucket reverseObjectEnumerator];
    HistoryMenuItem* menuObject = nil;
    while ((menuObject = [e2 nextObject])) {
      NSString* itemTitle = [[menuObject title] stringByTruncatingTo:kMaxTitleLength at:kTruncateAtMiddle];
      NSMenuItem* childMenuItem = [childMenu addItemWithTitle:itemTitle action:@selector(historyItemAction:) keyEquivalent:@""];
      [childMenuItem setRepresentedObject:menuObject];
      [childMenuItem setTarget:self];
    }
    
    [self setSubmenu:childMenu forItem:newItem];
  }  
}

@end
