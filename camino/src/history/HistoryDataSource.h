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
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *   Simon Woodside <sbwoodside@yahoo.com>
 *   Simon Fraser <smfr@smfr.org>
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

#import <AppKit/AppKit.h>

@class BrowserWindowController;
@class HistoryItem;
@class HistorySiteItem;

class nsIBrowserHistory;
class nsHistoryObserver;

extern NSString* const kHistoryViewByDate;      // grouped by last visit date
extern NSString* const kHistoryViewBySite;      // grouped by site
extern NSString* const kHistoryViewFlat;        // flat

// notification object is the data source.
extern NSString* const kNotificationNameHistoryDataSourceChanged;
// if not nil, this userInfo object is the changed item
extern NSString* const kNotificationHistoryDataSourceChangedUserInfoChangedItem;
// if true, this indicates that just the item changed, not its children (it's an NSNumber with bool)
extern NSString* const kNotificationHistoryDataSourceChangedUserInfoChangedItemOnly;

@class HistoryTreeBuilder;

@interface HistoryDataSource : NSObject
{
  nsIBrowserHistory*      mGlobalHistory;     // owned (would be an nsCOMPtr)
  nsHistoryObserver*      mHistoryObserver;   // owned
  
  NSString*               mCurrentViewIdentifier;
  
  NSString*               mSortColumn;
  BOOL                    mSortDescending;

  NSMutableArray*         mHistoryItems;              // this array owns all the history items
  NSMutableDictionary*    mHistoryItemsDictionary;    // history items indexed by id

  NSMutableArray*         mSearchResultsArray;

  // the tree builder encapsulates the logic to build and update different history views
  // (e.g. by date, by site etc), and supply the root object.  
  HistoryTreeBuilder*     mTreeBuilder;
 
  NSString*               mSearchString;
  int                     mSearchFieldTag;
  
  NSTimer*                mRefreshTimer;
  int                     mLastDayOfCommonEra;
}

- (void)setHistoryView:(NSString*)inView;
- (NSString*)historyView;

// XXX remove?
- (void)loadLazily;
- (HistoryItem*)rootItem;

// ideally sorting would be on the view, not the data source, but this keeps thing simpler
- (void)setSortColumnIdentifier:(NSString*)sortColumnIdentifier;
- (NSString*)sortColumnIdentifier;

- (BOOL)sortDescending;
- (void)setSortDescending:(BOOL)inDescending;

// quicksearch support
- (void)searchFor:(NSString*)searchString inFieldWithTag:(int)tag;
- (void)clearSearchResults;

- (void)removeItem:(HistorySiteItem*)item;

@end
