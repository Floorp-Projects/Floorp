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

#import "RDFOutlineViewDataSource.h"
#import "CHBrowserService.h"

#include "nsCRT.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFResource.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsRDFCID.h"

#include "nsComponentManagerUtils.h"
#include "nsIServiceManagerUtils.h"

#include "nsXPIDLString.h"
#include "nsString.h"


@interface RDFOutlineViewItem(Private)

- (unsigned int)cacheVersion;
- (void)setNumChildren:(int)numChildren isExpandable:(BOOL)expandable cacheVersion:(unsigned int)version;
- (void)setChildren:(NSArray*)childArray;
- (BOOL)cacheValid:(unsigned int)version needChildren:(BOOL)needChildren;
- (BOOL)cachedChildren;

- (BOOL)isExpandable;
- (int)numChildren;
- (id)childAtIndex:(int)index;

@end


@implementation RDFOutlineViewItem

- (id)init
{
  if ((self = [super init]))
  {
    mCacheVersion = 0;
    mExpandable   = NO;
    mNumChildren  = 0;
  }
  return self;
}

- (void)dealloc
{
  [mChildNodes release];
  NS_IF_RELEASE(mResource);
  [super dealloc];
}

- (nsIRDFResource*)resource
{
  NS_IF_ADDREF(mResource);
  return mResource;
}

- (void)setResource:(nsIRDFResource*) aResource
{
  nsIRDFResource* oldResource = mResource;
  NS_IF_ADDREF(mResource = aResource);
  NS_IF_RELEASE(oldResource);
}

- (unsigned int)cacheVersion
{
  return mCacheVersion;
}

- (void)setNumChildren:(int)numChildren isExpandable:(BOOL)expandable cacheVersion:(unsigned int)version;
{
  mNumChildren  = numChildren;
  mExpandable   = expandable;
  mCacheVersion = version;
}

- (void)setChildren:(NSArray*)childArray
{
  // childArray can legally be nil here. If it is, we're clearing the cached children
  NSArray* oldChildren = mChildNodes;
  mChildNodes = childArray;
  [mChildNodes retain];
  [oldChildren release];
}

- (BOOL)cacheValid:(unsigned int)version needChildren:(BOOL)needChildren;
{
  return (mCacheVersion == version) && (needChildren ? (mChildNodes != nil) : 1);
}

- (BOOL)cachedChildren
{
  return (mChildNodes != nil);
}

- (BOOL)isExpandable
{
  return mExpandable;
}

- (int)numChildren
{
  return mNumChildren;
}

- (id)childAtIndex:(int)index
{
  if (mChildNodes)
    return [mChildNodes objectAtIndex:index];
  
  return nil;
}

@end

#pragma mark -

@interface RDFOutlineViewDataSource(Private)

- (void)registerForShutdownNotification;
- (void)cleanup;
- (void)updateItemProperties:(id)item enumerateChildren:(BOOL)doChildren;

@end

@implementation RDFOutlineViewDataSource

- (id)init
{
  if ((self = [super init]))
  {
    [self registerForShutdownNotification];
    mCacheVersion = 1;
  }
  return self;
}

- (void) dealloc
{
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [self cleanup];
  [super dealloc];
}

- (void)cleanup
{
  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mContainerUtils);
  NS_IF_RELEASE(mRDFService);
  
  NS_IF_RELEASE(mDataSource);
  NS_IF_RELEASE(mRootResource);
  
  [mDictionary release];
  mDictionary = nil;
}

- (void)cleanupDataSource
{
  [self cleanup];
}

//
// ensureDataSourceLoaded
//
// defer loading all this rdf junk until it's requested because it's slow
//
- (void) ensureDataSourceLoaded
{
  if ( !mContainer ) {
    nsCOMPtr<nsIRDFContainer> ctr = do_CreateInstance("@mozilla.org/rdf/container;1");
    NS_ADDREF(mContainer = ctr);
    
    nsCOMPtr<nsIRDFContainerUtils> ctrUtils = do_GetService("@mozilla.org/rdf/container-utils;1");
    NS_ADDREF(mContainerUtils = ctrUtils);
    
    nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1");
    NS_ADDREF(mRDFService = rdfService);
  
    mDictionary = [[NSMutableDictionary alloc] initWithCapacity: 30];
  
    mDataSource = nsnull;
    mRootResource = nsnull;
  }
}

- (void)registerForShutdownNotification
{
  [[NSNotificationCenter defaultCenter] addObserver:  self
                                        selector:     @selector(shutdown:)
                                        name:         XPCOMShutDownNotificationName
                                        object:       nil];
}

- (void)shutdown: (NSNotification*)aNotification
{
  [self cleanupDataSource];
}

- (nsIRDFDataSource*) dataSource
{
    NS_IF_ADDREF(mDataSource);
    return mDataSource;
}

- (nsIRDFResource*) rootResource
{
    NS_IF_ADDREF(mRootResource);
    return mRootResource;
}

- (void) setDataSource: (nsIRDFDataSource*) aDataSource
{
    nsIRDFDataSource* oldDataSource = mDataSource;
    NS_IF_ADDREF(mDataSource = aDataSource);
    NS_IF_RELEASE(oldDataSource);
}

- (void) setRootResource: (nsIRDFResource*) aResource
{
    nsIRDFResource* oldResource = mRootResource;
    NS_IF_ADDREF(mRootResource = aResource);
    NS_IF_RELEASE(oldResource);
}

//
// XXX - For now, we'll just say that none of our items are editable, as we aren't using any 
//       RDF datasources that are mutable.
//
- (BOOL) outlineView: (NSOutlineView*)aOutlineView shouldEditTableColumn: (NSTableColumn*) aTableColumn
                                                    item: (id) aItem
{
  return NO;
}

- (BOOL) outlineView: (NSOutlineView*)aOutlineView isItemExpandable:(id)aItem
{
  if (!mDataSource)
      return NO;
  
  if (!aItem)
      return YES; // The root is always open
  
  if (![aItem cacheValid:mCacheVersion needChildren:NO])
    [self updateItemProperties:aItem enumerateChildren:NO];
  
  return [aItem isExpandable];
}

- (id)outlineView:(NSOutlineView*)aOutlineView child:(int)aIndex ofItem:(id)aItem
{
  if (!mDataSource)
      return nil;
  
  if (!aItem)
  {
    nsCOMPtr<nsIRDFResource> rootResource = dont_AddRef([self rootResource]);
    aItem = [self getWrapperFor:rootResource];
  }
  
  if (![aItem cacheValid:mCacheVersion needChildren:YES])
    [self updateItemProperties:aItem enumerateChildren:YES];
    
  return [aItem childAtIndex:aIndex];
}
    
- (int)outlineView:(NSOutlineView*)aOutlineView numberOfChildrenOfItem:(id) aItem;
{
  if (!mDataSource)
      return 0;

  if (!aItem)
  {
    nsCOMPtr<nsIRDFResource> rootResource = dont_AddRef([self rootResource]);
    aItem = [self getWrapperFor:rootResource];
  }
  
  if (![aItem cacheValid:mCacheVersion needChildren:YES])
    [self updateItemProperties:aItem enumerateChildren:YES];

  return [aItem numChildren];
}

- (id)outlineView:(NSOutlineView*)aOutlineView objectValueForTableColumn:(NSTableColumn*)aTableColumn
                                                  byItem:(id)aItem
{
  if (!mDataSource || !aItem)
      return nil;
  
  // The table column's identifier is the RDF Resource URI of the property being displayed in
  // that column, e.g. "http://home.netscape.com/NC-rdf#Name"
  NSString* columnPropertyURI = [aTableColumn identifier];    
  NSString* propString = [self getPropertyString:columnPropertyURI forItem:aItem];

  return [self createCellContents:propString withColumn:columnPropertyURI byItem:aItem];
}

//
// createCellContents:withColumn:byItem
//
// Constructs a NSString from the given string data for this item in the given column.
// This should be overridden to do more fancy things, such as add an icon, etc.
//
- (id)createCellContents:(NSString*)inValue withColumn:(NSString*)inColumn byItem:(id)inItem
{
  return inValue;
}

//
// outlineView:tooltipForString
//
// returns the value of the Name property as the tooltip for the given item. Override to do 
// anything more complicated
//
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)inItem
{
  return [self getPropertyString:@"http://home.netscape.com/NC-rdf#Name" forItem:inItem];
}

- (void) reloadDataForItem: (id) aItem reloadChildren: (BOOL) aReloadChildren
{
  if (!aItem)
    [mOutlineView reloadData];
  else
    [mOutlineView reloadItem: aItem reloadChildren: aReloadChildren];
}

- (id)getWrapperFor:(nsIRDFResource*) aRDFResource
{
  const char* k;
  aRDFResource->GetValueConst(&k);
  NSString* key = [NSString stringWithCString:k];
  
  // see if we've created a wrapper already, if not, create a new wrapper object
  // and stash it in our dictionary
  RDFOutlineViewItem* item = [mDictionary objectForKey:key];
  if (!item)
  {
    item = [[[RDFOutlineViewItem alloc] init] autorelease];
    [item setResource: aRDFResource];
    [mDictionary setObject:item forKey:key];				// retains |item|
  }
  
  return item;
}

- (void)invalidateCachedItems
{
  mCacheVersion++;
}

- (void)updateItemProperties:(id)item enumerateChildren:(BOOL)doChildren
{
  BOOL isExpandable = NO;
  NSMutableArray* itemChildren = doChildren ? [[[NSMutableArray alloc] initWithCapacity:10] autorelease] : nil;
  
  nsCOMPtr<nsIRDFResource> itemResource = dont_AddRef([item resource]);
  
  PRBool isSeq = PR_FALSE;
  mContainerUtils->IsSeq(mDataSource, itemResource, &isSeq);
  if (isSeq)
    isExpandable = YES;

  nsCOMPtr<nsIRDFResource> childProperty;
  mRDFService->GetResource("http://home.netscape.com/NC-rdf#child", getter_AddRefs(childProperty));

  nsCOMPtr<nsISimpleEnumerator> childNodes;
  mDataSource->GetTargets(itemResource, childProperty, PR_TRUE, getter_AddRefs(childNodes));
  
  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(childNodes->HasMoreElements(&hasMore)) && hasMore)
  {
    nsCOMPtr<nsISupports> supp;
    childNodes->GetNext(getter_AddRefs(supp));

    nsCOMPtr<nsIRDFResource> childResource = do_QueryInterface(supp);
    if (childResource)
    {
      id childItem = [self getWrapperFor:childResource];
      if (childItem)
      {
        isExpandable = YES;
        if (!itemChildren) break;		// know enough already
        [itemChildren addObject:childItem];
      }
    }
  }
  
  // itemChildren will be nil here if we don't care about children, but that's OK.
  // the setChildren call will clear the cached child list.
	[item setNumChildren:[itemChildren count] isExpandable:isExpandable cacheVersion:mCacheVersion];
  [item setChildren:itemChildren];  
}

- (NSString*)getPropertyString:(NSString*)inPropertyURI forItem:(RDFOutlineViewItem*)inItem
{
  nsCOMPtr<nsIRDFResource> propertyResource;
  mRDFService->GetResource([inPropertyURI UTF8String], getter_AddRefs(propertyResource));
          
  nsCOMPtr<nsIRDFResource> resource = dont_AddRef([inItem resource]);
          
  nsCOMPtr<nsIRDFNode> valueNode;
  mDataSource->GetTarget(resource, propertyResource, PR_TRUE, getter_AddRefs(valueNode));
  if (!valueNode) {
#if DEBUG
      NSLog(@"ValueNode is null in RDF objectValueForTableColumn");
#endif
      return @"";
  }
  
  nsCOMPtr<nsIRDFLiteral> valueLiteral(do_QueryInterface(valueNode));
  if (!valueLiteral)
      return @"";

  const PRUnichar* value = NULL;
  valueLiteral->GetValueConst(&value);
  if (value)
    return [NSString stringWithCharacters:value length:nsCRT::strlen(value)];
   
  return @"";
}


@end
