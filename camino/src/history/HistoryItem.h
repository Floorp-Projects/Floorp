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

// HistoryItem is the base class for every object in the history outliner

@interface HistoryItem : NSObject
{
}

- (NSString*)title;
- (BOOL)isSiteItem;
- (NSImage*)icon;

- (NSString*)url;
- (NSDate*)firstVisit;
- (NSDate*)lastVisit;
- (NSString*)hostname;
- (NSString*)identifier;

- (NSMutableArray*)children;
- (int)numberOfChildren;
- (HistoryItem*)childAtIndex:(int)inIndex;

- (BOOL)isSiteItem;

// we put sort comparators on the base class for convenience
- (NSComparisonResult)compareURL:(HistoryItem *)aItem;
- (NSComparisonResult)compareTitle:(HistoryItem *)aItem;
- (NSComparisonResult)compareFirstVisitDate:(HistoryItem *)aItem;
- (NSComparisonResult)compareLastVisitDate:(HistoryItem *)aItem;
- (NSComparisonResult)compareHostname:(HistoryItem *)aItem;

@end

// a history category item (a folder in the outliner)
@interface HistoryCategoryItem : HistoryItem
{
  NSString*         mUUIDString;  // used to identify folders for outliner state saving
  NSString*         mTitle;
  NSDate*           mStartDate;
  NSMutableArray*   mChildren;    // array of HistoryItems (may be heterogeneous)
}

- (id)initWithTitle:(NSString*)title childCapacity:(int)capacity;
- (NSString*)title;
- (NSString*)identifier;    // return UUID for this folder
// start date only valid when grouping by date
- (NSDate*)startDate;
- (void)setStartDate:(NSDate*)date;

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
}

- (id)initWith_nsIHistoryItem:(nsIHistoryItem*)inItem;
// return YES if anything changed
- (BOOL)updateWith_nsIHistoryItem:(nsIHistoryItem*)inItem;

- (BOOL)matchesString:(NSString*)searchString inFieldWithTag:(int)tag;

@end
