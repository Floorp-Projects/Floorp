/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the Mozilla browser.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation. Portions created by Netscape are
* Copyright (C) 2002 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*   Joe Hewitt <hewitt@netscape.com> (Original Author)
*/

#import "NSString+Utils.h"

#import "GoMenu.h"
#import "MainController.h"
#import "BrowserWindowController.h"
#import "BrowserWrapper.h"
#import "CHBrowserView.h"
#import "ChimeraUIConstants.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIWebBrowser.h"
#include "nsISHistory.h"
#include "nsIWebNavigation.h"
#include "nsIHistoryEntry.h"
#include "nsCRT.h"

// the maximum number of history entry menuitems to display
static const int kMaxItems = 15;
// the maximum number of characters in a menu title before cropping it
static const unsigned int kMaxTitleLength = 80;

@implementation GoMenu

- (void) update
{
  [self emptyHistoryItems];
  [self addHistoryItems];

  [super update];
}

- (nsIWebNavigation*) currentWebNavigation
{
  // get controller for current window
  BrowserWindowController *controller = [(MainController *)[NSApp delegate] getMainWindowBrowserController];
  if (!controller) return nsnull;
  return [controller currentWebNavigation];
}

- (void) historyItemAction:(id)sender
{
  // get web navigation for current browser
  nsIWebNavigation* webNav = [self currentWebNavigation];
  if (!webNav) return;
  
  // browse to the history entry for the menuitem that was selected
  PRInt32 historyIndex = ([sender tag] - 1) - kDividerTag;
  webNav->GotoIndex(historyIndex);
}

- (void) emptyHistoryItems
{
  // remove every history item after the insertion point
  int insertionIndex = [self indexOfItemWithTag:kDividerTag];
  for (int i = [self numberOfItems]-1; i > insertionIndex ; --i) {
    [self removeItemAtIndex:i];
  }
}

- (void) addHistoryItems
{
  // get session history for current browser
  nsIWebNavigation* webNav = [self currentWebNavigation];
  if (!webNav) return;
  nsCOMPtr<nsISHistory> sessionHistory;
  webNav->GetSessionHistory(getter_AddRefs(sessionHistory));
  
  PRInt32 count;
  sessionHistory->GetCount(&count);
  PRInt32 currentIndex;
  sessionHistory->GetIndex(&currentIndex);

  // determine the range of items to display
  int rangeStart, rangeEnd;
  int above = kMaxItems/2;
  int below = (kMaxItems-above)-1;
  if (count <= kMaxItems) {
    // if the whole history fits within our menu, fit range to show all
    rangeStart = count-1;
    rangeEnd = 0;
  } else {
    // if the whole history is too large for menu, try to put current index as close to 
    // the middle of the list as possible, so the user can see both back and forward in session
    rangeStart = currentIndex + above;
    rangeEnd = currentIndex - below;
    if (rangeStart >= count-1) {
      rangeEnd -= (rangeStart-count)+1; // shift overflow to the end
      rangeStart = count-1;
    } else if (rangeEnd < 0) {
      rangeStart -= rangeEnd; // shift overflow to the start
      rangeEnd = 0;
    }
  }

  // create a new menu item for each history entry (up to MAX_MENUITEM entries)
  for (PRInt32 i = rangeStart; i >= rangeEnd; --i) {
    nsCOMPtr<nsIHistoryEntry> entry;
    sessionHistory->GetEntryAtIndex(i, PR_FALSE, getter_AddRefs(entry));

    nsXPIDLString textStr;
    entry->GetTitle(getter_Copies(textStr));
    NSString* title = [[NSString stringWith_nsAString: textStr] stringByTruncatingTo:kMaxTitleLength at:kTruncateAtMiddle];    
    NSMenuItem *newItem = [self addItemWithTitle:title action:@selector(historyItemAction:) keyEquivalent:@""];
    [newItem setTarget:self];
    [newItem setTag:kDividerTag+1+i];
    if (currentIndex == i)
      [newItem setState:NSOnState];
  }
}

@end
