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
 *   Simon Fraser <smfr@smfr.org>
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
#import "NSDate+Utils.h"

#import "HistoryItem.h"

#import "nsString.h"
#import "nsIHistoryItems.h"


// search field tags, used in search field context menu item tags
enum
{
  eHistorySearchFieldAll = 1,
  eHistorySearchFieldTitle,
  eHistorySearchFieldURL
};


@implementation HistoryItem

- (id)init
{
  if ((self = [super init]))
  {
  }
  return self;
}

- (void)dealloc
{
  [super dealloc];
}

- (NSString*)title
{
  return @"";   // subclasses override
}

- (BOOL)isSiteItem
{
  return NO;
}

- (NSImage*)icon
{
  return nil;
}

- (NSString*)url
{
  return @"";
}

- (NSDate*)firstVisit
{
  return nil;
}

- (NSDate*)lastVisit
{
  return nil;
}

- (NSString*)hostname
{
  return @"";
}

- (NSString*)identifier
{
  return @"";
}

- (NSMutableArray*)children
{
  return nil;
}

- (int)numberOfChildren
{
  return 0;
}

- (HistoryItem*)childAtIndex:(int)inIndex
{
  return nil;
}

- (NSComparisonResult)compareURL:(HistoryItem *)aItem
{
  return NSOrderedSame;
}

- (NSComparisonResult)compareTitle:(HistoryItem *)aItem
{
  return NSOrderedSame;
}

- (NSComparisonResult)compareFirstVisitDate:(HistoryItem *)aItem
{
  return NSOrderedSame;
}

- (NSComparisonResult)compareLastVisitDate:(HistoryItem *)aItem
{
  return NSOrderedSame;
}

- (NSComparisonResult)compareHostname:(HistoryItem *)aItem
{
  return NSOrderedSame;
}

@end

#pragma mark -

@implementation HistoryCategoryItem

- (id)initWithTitle:(NSString*)title childCapacity:(int)capacity
{
  if ((self = [super init]))
  {
    mTitle = [title retain];
    mChildren = [[NSMutableArray alloc] initWithCapacity:capacity];
  }
  return self;
}

- (void)dealloc
{
  [mTitle release];
  [mChildren release];
  [super dealloc];
}

- (NSString*)title
{
  return mTitle;
}

- (NSDate*)startDate
{
  return mStartDate;
}

- (void)setStartDate:(NSDate*)date
{
  NSDate* oldDate = mStartDate;
  mStartDate = [date retain];
  [oldDate release];
}

- (NSString*)url
{
  return @"";
}

- (NSDate*)firstVisit
{
  return nil;
}

- (NSDate*)lastVisit
{
  return nil;
}

- (NSString*)hostname
{
  return @"";
}

- (NSImage*)icon
{
  return [NSImage imageNamed:@"folder"];
}

- (NSMutableArray*)children
{
  return mChildren;
}

- (int)numberOfChildren
{
  return [mChildren count];
}

- (HistoryItem*)childAtIndex:(int)inIndex
{
  if (inIndex >= 0 && inIndex < [mChildren count])
    return [mChildren objectAtIndex:inIndex];
    
  return nil;
}

- (NSComparisonResult)compareURL:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistorySiteItem class]])
    return NSOrderedAscending;

  // sort on title
  return [self compareTitle:aItem];
}

- (NSComparisonResult)compareTitle:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistorySiteItem class]])
    return NSOrderedAscending;

  // if we have dates, they override other sorts
  if ([self startDate] && [aItem startDate])
    return [[self startDate] compare:[aItem startDate]];
  else
    return [mTitle compare:[aItem title] options:NSCaseInsensitiveSearch];
}

- (NSComparisonResult)compareFirstVisitDate:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistorySiteItem class]])
    return NSOrderedAscending;

  // sort on title
  return [self compareTitle:aItem];
}

- (NSComparisonResult)compareLastVisitDate:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistorySiteItem class]])
    return NSOrderedAscending;

  // sort on title
  return [self compareTitle:aItem];
}

- (NSComparisonResult)compareHostname:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistorySiteItem class]])
    return NSOrderedAscending;

  // sort on title
  return [self compareTitle:aItem];
}

@end

#pragma mark -

@implementation HistorySiteItem

- (id)initWith_nsIHistoryItem:(nsIHistoryItem*)inItem
{
  if ((self = [super init]))
  {
    nsCString identifier;
    if (NS_SUCCEEDED(inItem->GetID(identifier)))
      mItemIdentifier = [[NSString stringWith_nsACString:identifier] retain];

    nsCString url;
    if (NS_SUCCEEDED(inItem->GetURL(url)))
      mURL = [[NSString stringWith_nsACString:url] retain];
    
    nsString title;
    if (NS_SUCCEEDED(inItem->GetTitle(title)))
      mTitle = [[NSString stringWith_nsAString:title] retain];

    nsCString hostname;
    if (NS_SUCCEEDED(inItem->GetHostname(hostname)))
      mHostname = [[NSString stringWith_nsACString:hostname] retain];

    PRTime firstVisit;
    if (NS_SUCCEEDED(inItem->GetFirstVisitDate(&firstVisit)))
      mFirstVisitDate = [[NSDate dateWithPRTime:firstVisit] retain];

    PRTime lastVisit;
    if (NS_SUCCEEDED(inItem->GetLastVisitDate(&lastVisit)))
      mLastVisitDate = [[NSDate dateWithPRTime:lastVisit] retain];
  }
  return self;
}

- (BOOL)updateWith_nsIHistoryItem:(nsIHistoryItem*)inItem
{
  // only the title and last visit date can change
  BOOL somethingChanged = NO;

  nsString title;
  if (NS_SUCCEEDED(inItem->GetTitle(title)))
  {
    NSString* newTitle = [NSString stringWith_nsAString:title];
    if (!mTitle || ![mTitle isEqualToString:newTitle])
    {
      [mTitle release];
      mTitle = [newTitle retain];
      somethingChanged = YES;
    }
  }
  
  PRTime lastVisit;
  if (NS_SUCCEEDED(inItem->GetLastVisitDate(&lastVisit)))
  {
    NSDate* newDate = [NSDate dateWithPRTime:lastVisit];
    if (![mLastVisitDate isEqual:newDate])
    {
      [mLastVisitDate release];
      mLastVisitDate = [newDate retain];
      somethingChanged = YES;
    }
  }
  
  return somethingChanged;
}

- (void)dealloc
{
  [mItemIdentifier release];
  [mURL release];
  [mTitle release];
  [mHostname release];
  [mFirstVisitDate release];
  [mLastVisitDate release];
  [super dealloc];
}

- (NSString*)url
{
  return mURL;
}

- (NSString*)title
{
  return mTitle;
}

- (NSDate*)firstVisit
{
  return mFirstVisitDate;
}

- (NSDate*)lastVisit
{
  return mLastVisitDate;
}

- (NSString*)hostname
{
  return mHostname;
}

- (NSString*)identifier
{
  return mItemIdentifier;
}

- (BOOL)isSiteItem
{
  return YES;
}

- (NSImage*)icon
{
  // XXX todo: site icons
  return [NSImage imageNamed:@"smallbookmark"];
}

- (NSComparisonResult)compareURL:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistoryCategoryItem class]])
    return NSOrderedDescending;

  return [mURL compare:[aItem url] options:NSCaseInsensitiveSearch];
}

- (NSComparisonResult)compareTitle:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistoryCategoryItem class]])
    return NSOrderedDescending;

  return [mTitle compare:[aItem title] options:NSCaseInsensitiveSearch];
}

- (NSComparisonResult)compareFirstVisitDate:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistoryCategoryItem class]])
    return NSOrderedDescending;

  return [mFirstVisitDate compare:[aItem firstVisit]];
}

- (NSComparisonResult)compareLastVisitDate:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistoryCategoryItem class]])
    return NSOrderedDescending;

  return [mLastVisitDate compare:[aItem lastVisit]];
}

- (NSComparisonResult)compareHostname:(HistoryItem *)aItem
{
  // sort categories before sites
  if ([aItem isKindOfClass:[HistoryCategoryItem class]])
    return NSOrderedDescending;

  return [mHostname compare:[aItem hostname] options:NSCaseInsensitiveSearch];
}

- (BOOL)matchesString:(NSString*)searchString inFieldWithTag:(int)tag
{
  switch (tag)
  {
    case eHistorySearchFieldAll:
      return (([[self title] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound) ||
              ([[self url]   rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound));

    case eHistorySearchFieldTitle:
      return ([[self title] rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound);

    case eHistorySearchFieldURL:
      return ([[self url]   rangeOfString:searchString options:NSCaseInsensitiveSearch].location != NSNotFound);
  }

  return NO;    
}


@end
