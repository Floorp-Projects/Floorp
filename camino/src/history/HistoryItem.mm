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
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *   Simon Woodside <sbwoodside@yahoo.com>
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

// these objects add an additional grand child cache
// why? because we flatten the list by skipping the children RDFItems
// and just returning the full list of grandchildren as our children

#import "HistoryItem.h"

#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"
#include "nsIBrowserHistory.h"
#include "nsIServiceManager.h"

#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsNetUtil.h"

#include "nsComponentManagerUtils.h"


@interface HistoryItem (Private)
- (HistoryItem*)newChild;
- (void)invalidateGrandChildCache;

@end

#pragma mark -

@implementation HistoryItem

- (id)init
{
  if( (self = [super init]) ) {
    mGrandChildNodes = nil;
  }
  return self;
}

- (void)dealloc
{
  [mGrandChildNodes autorelease];
  [super dealloc];
}

- (HistoryItem*)newChild;
{
  return [HistoryItem alloc];
}


- (void)deleteFromGecko;
{
  nsCOMPtr<nsIBrowserHistory> historyService = do_GetService("@mozilla.org/browser/global-history;2");
  if( !historyService )
    return;

  nsCOMPtr<nsIRDFDataSource> histDataSource = do_QueryInterface(historyService);
  if( !histDataSource )
    return;
  
  histDataSource->BeginUpdateBatch();
  if( ![self isExpandable] ) {
    NSString* urlString = [self url];
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), [urlString UTF8String]);
    if (uri)
      historyService->RemovePage(uri);
  }
  else {
    // delete a folder by iterating over each of its children and deleting them. There
    // should be a better way, but the history api's don't really support them. Expand
    // the folder before we delete it otherwise we don't get any child nodes to delete.
    //TODO what if the children have children?
    int numChilds = [self numChildren];
    for( int i = 0; i < numChilds; i++ ) {
      HistoryItem * deleteChild = [self childAtIndex:i];
      [deleteChild deleteFromGecko];
    }
  }
  histDataSource->EndUpdateBatch();
}

// Explicitly call to superclass
// to avoid recursive loops
- (bool)nativeIsExpandable;
{   return [super isExpandable]; }
- (int)nativeNumChildren;
{   return [super numChildren]; }
- (id)nativeChildAtIndex:(int)index;
{   return [super childAtIndex:index]; }
- (void)nativeBuildChildCache;
{   [super buildChildCache]; }

// should we return grandchildren nodes instead of the children nodes?
// this determines if we flatten the RDF at this "level"
- (bool)shouldUseGrandChildNodes;
{
  if( !kFlattenHistory )
    return NO;
  if( ![self nativeIsExpandable] )
    return NO;
  
  if( !mChildNodes ) [self nativeBuildChildCache];
  
  if ( [mChildNodes count] ) {
    HistoryItem * firstChild = [mChildNodes objectAtIndex:0];
    if( [firstChild nativeIsExpandable] ) {
      HistoryItem * grandChild = [firstChild nativeChildAtIndex:0];
      //is it a leaf node?
      if( ![grandChild nativeIsExpandable] )
        return YES;
    }
  }
  return NO;
}

- (HistoryItem*)childAtIndex:(int)index;
{
  if ( !kFlattenHistory )
    return [super childAtIndex:index];
  if ( ![self shouldUseGrandChildNodes] )
    return [super childAtIndex:index];
  if ( !mGrandChildNodes )
    [self buildGrandChildCache];
  HistoryItem* child = nil;
  if (index < [mGrandChildNodes count])
    child = [mGrandChildNodes objectAtIndex:index];
  return child;
}

- (int)numChildren;
{
  if( !kFlattenHistory )
    return [super numChildren];
  if( ![self shouldUseGrandChildNodes] )
    return [super numChildren];
  if( !mGrandChildNodes )
    [self buildGrandChildCache];
  int result = [mGrandChildNodes count];
  return result;
}

- (NSComparisonResult)historyItemDateCompare:(HistoryItem *)aItem;
{
  //backwards for reverse order
  NSComparisonResult result = [[aItem date] caseInsensitiveCompare:[self date]];
  return result;
}


- (void)buildGrandChildCache;
{
  if( !kFlattenHistory )
    return;
  if( ![self shouldUseGrandChildNodes] )
    return;
//  NSLog(@"Building grandChildNodes list for name=%@", [self name]);
  NSMutableArray * grandChildNodes = [[NSMutableArray alloc] init];
  int childCount = [mChildNodes count];
  for( int i=0; i < childCount; i++ ) {
    HistoryItem * curChild = [mChildNodes objectAtIndex:i];
    int grandChildCount = [curChild nativeNumChildren];
    for( int j=0; j < grandChildCount; j++ ) {
      HistoryItem * curGrandChild = [curChild nativeChildAtIndex:j];
      if (curGrandChild)
        [grandChildNodes addObject:curGrandChild];
    }
  }
  NSArray * sorted = [grandChildNodes sortedArrayUsingSelector:@selector(historyItemDateCompare:)];
  mGrandChildNodes = [sorted retain];
}

- (void)invalidateGrandChildCache;
{
  [mGrandChildNodes autorelease];
  mGrandChildNodes = nil;
}

// override superclass, because we need to also invalidate the
// grandchildren cache of our parent (the caller's grandparent)
- (void)deleteChildFromCache:(HistoryItem*)child;
{
  [super deleteChildFromCache:child];
  if( [self parent] )
    [(HistoryItem*)[self parent] invalidateGrandChildCache];
}


// RDF Properties
- (NSString*)name;
{
  return [self getStringForRDFPropertyURI:NC_NAME_KEY];
}

- (NSString*)url;
{
  return [self getStringForRDFPropertyURI:NC_URL_KEY];
}

- (NSString*)date;
{
  return [self getStringForRDFPropertyURI:NC_DATE_KEY];
}

@end
