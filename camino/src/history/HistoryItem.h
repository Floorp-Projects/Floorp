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

class nsIHistoryItem;
@class HistoryDataSource;

// HistoryItem is the base class for every object in the history outliner

@interface HistoryItem : NSObject
{
  HistoryItem*        mParentItem;    // our parent item (not retained)
  HistoryDataSource*  mDataSource;    // the data source that owns us (not retained)
}

- (id)initWithDataSource:(HistoryDataSource*)inDataSource;
- (HistoryDataSource*)dataSource;

- (NSString*)title;
- (BOOL)isSiteItem;
- (NSImage*)icon;
- (NSImage*)iconAllowingLoad:(BOOL)inAllowLoad;

- (NSString*)url;
- (NSDate*)firstVisit;
- (NSDate*)lastVisit;
- (NSString*)hostname;
- (NSString*)identifier;

- (void)setParentItem:(HistoryItem*)inParent;
- (HistoryItem*)parentItem;
- (BOOL)isDescendentOfItem:(HistoryItem*)inItem;

- (NSMutableArray*)children;
- (int)numberOfChildren;
- (HistoryItem*)childAtIndex:(int)inIndex;

- (BOOL)isSiteItem;

// we put sort comparators on the base class for convenience
- (NSComparisonResult)compareURL:(HistoryItem *)aItem sortDescending:(NSNumber*)inDescending;
- (NSComparisonResult)compareTitle:(HistoryItem *)aItem sortDescending:(NSNumber*)inDescending;
- (NSComparisonResult)compareFirstVisitDate:(HistoryItem *)aItem sortDescending:(NSNumber*)inDescending;
- (NSComparisonResult)compareLastVisitDate:(HistoryItem *)aItem sortDescending:(NSNumber*)inDescending;
- (NSComparisonResult)compareHostname:(HistoryItem *)aItem sortDescending:(NSNumber*)inDescending;

@end

// a history category item (a folder in the outliner)
@interface HistoryCategoryItem : HistoryItem
{
  NSString*         mUUIDString;  // used to identify folders for outliner state saving
  NSString*         mTitle;
  NSMutableArray*   mChildren;    // array of HistoryItems (may be heterogeneous)
}

- (id)initWithDataSource:(HistoryDataSource*)inDataSource title:(NSString*)title childCapacity:(int)capacity;
- (NSString*)title;
- (NSString*)identifier;    // return UUID for this folder

- (void)addChild:(HistoryItem*)inChild;
- (void)removeChild:(HistoryItem*)inChild;
- (void)addChildren:(NSArray*)inChildren;

@end


// a history site category item (a folder in the outliner)
@interface HistorySiteCategoryItem : HistoryCategoryItem
{
  NSString*         mSite;    // not user-visible; used for state tracking
}

- (id)initWithDataSource:(HistoryDataSource*)inDataSource site:(NSString*)site title:(NSString*)title childCapacity:(int)capacity;
- (NSString*)site;

@end

// a history site category item (a folder in the outliner)
@interface HistoryDateCategoryItem : HistoryCategoryItem
{
  NSDate*           mStartDate;
  int               mAgeInDays;   // -1 is used for "distant past"
}

- (id)initWithDataSource:(HistoryDataSource*)inDataSource startDate:(NSDate*)startDate ageInDays:(int)days title:(NSString*)title childCapacity:(int)capacity;
- (NSDate*)startDate;
- (BOOL)isTodayCategory;

@end

// a specific history entry
@interface HistorySiteItem : HistoryItem
{
  NSString*         mItemIdentifier;
  NSString*         mURL;
  NSString*         mTitle;
  NSString*         mHostname;
  NSDate*           mFirstVisitDate;
  NSDate*           mLastVisitDate;
  
  NSImage*          mSiteIcon;
  BOOL              mAttemptedIconLoad;
}

- (id)initWithDataSource:(HistoryDataSource*)inDataSource historyItem:(nsIHistoryItem*)inItem;
// return YES if anything changed
- (BOOL)updateWith_nsIHistoryItem:(nsIHistoryItem*)inItem;

- (BOOL)matchesString:(NSString*)searchString inFieldWithTag:(int)tag;

- (void)setSiteIcon:(NSImage*)inImage;

@end
