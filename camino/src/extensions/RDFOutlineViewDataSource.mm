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

#import "NSString+Utils.h"

#import "RDFOutlineViewDataSource.h"
#import "CHBrowserService.h"
#import "ExtendedOutlineView.h"
#import "RDFItem.h"

#include "nsCRT.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFResource.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsRDFCID.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "nsXPIDLString.h"
#include "nsString.h"


@interface RDFOutlineViewDataSource(Private)
- (void)cleanup;
@end

#pragma mark -

@implementation RDFOutlineViewDataSource

- (id)init
{
  if ((self = [super init]))
  {
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(shutdown:)
                                                 name:XPCOMShutDownNotificationName object:nil];
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
  NS_IF_RELEASE(mRDFContainer);
  NS_IF_RELEASE(mRDFContainerUtils);
  NS_IF_RELEASE(mRDFService);
  
  NS_IF_RELEASE(mRDFDataSource);
}

- (void)cleanupDataSource
{
  [self cleanup];
}

- (RDFItem *)rootRDFItem;
{
  return mRootRDFItem;
}

- (void)setRootRDFItem:(RDFItem*)item;
{
  [mRootRDFItem autorelease];
  mRootRDFItem = [item retain];
}


//
// loadLazily
//
// defer loading all this rdf junk until it's requested because it's slow
// TODO measure how slow it really is
//
- (void) loadLazily
{
  if ( !mRDFContainer ) {
    nsCOMPtr<nsIRDFContainer> ctr = do_CreateInstance("@mozilla.org/rdf/container;1");
    NS_ADDREF(mRDFContainer = ctr);
    
    nsCOMPtr<nsIRDFContainerUtils> ctrUtils = do_GetService("@mozilla.org/rdf/container-utils;1");
    NS_ADDREF(mRDFContainerUtils = ctrUtils);
    
    nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1");
    NS_ADDREF(mRDFService = rdfService);
  
    mRDFDataSource = nsnull;
  }
}

- (void)shutdown:(NSNotification*)aNotification
{
  [self cleanupDataSource];
}


#pragma mark -

// Implementation of NSOutlineViewDataSource protocol

// XXX - For now, we'll just say that none of our items are editable, as we aren't using any 
//       RDF datasources that are mutable.

- (BOOL)outlineView:(NSOutlineView*)aOutlineView shouldEditTableColumn:(NSTableColumn*)aTableColumn item:(id)aItem
{
  return NO;
}

- (BOOL)outlineView:(NSOutlineView*)aOutlineView isItemExpandable:(id)aItem
{
  if (!mRDFDataSource)
      return NO;
  if (!aItem)
      return YES; // The root is always open
  if( [aItem isKindOfClass:[RDFItem class]] ) {
    RDFItem * rdfItem = aItem;
    return [rdfItem isExpandable];
  }
  return NO;
}

- (id)outlineView:(NSOutlineView*)aOutlineView child:(int)aIndex ofItem:(id)aItem
{
  if (!mRDFDataSource)
      return nil;
  if (!aItem) {
    aItem = [self rootRDFItem];
  }
  if( [aItem isKindOfClass:[RDFItem class]] ) {
    RDFItem * rdfItem = aItem;
    return [rdfItem childAtIndex:aIndex];
  }
  return nil;
}
    
- (int)outlineView:(NSOutlineView*)aOutlineView numberOfChildrenOfItem:(id)aItem;
{
  if (!mRDFDataSource)
      return 0;
  if (!aItem) {
    aItem = [self rootRDFItem];
  }
  if( [aItem isKindOfClass:[RDFItem class]] ) {
    RDFItem * rdfItem = aItem;
    return [rdfItem numChildren];
  }
  return 0;
}

// TODO move to subclass
- (id)outlineView:(NSOutlineView*)aOutlineView objectValueForTableColumn:(NSTableColumn*)aTableColumn byItem:(id)aItem
{
  if (!mRDFDataSource || !aItem)
      return nil;
  // The table column's identifier is the last part of the RDF Resource URI of the property
  // being displayed in that column, e.g. "http://home.netscape.com/NC-rdf#Name"
  NSString *resourceName;
  if( [[aTableColumn identifier] isEqualToString:@"title"] )
    resourceName = @"Name";
  else if( [[aTableColumn identifier] isEqualToString:@"url"] )
    resourceName = @"URL";
  else
    return nil;
  NSString * RDFURI = @"http://home.netscape.com/NC-rdf#";
  NSString * RDFpropertyURI = [RDFURI stringByAppendingString:resourceName];
  NSString * propertyString = [aItem getStringForRDFPropertyURI:RDFpropertyURI];
  return propertyString;
}

// returns the value of the Name property as the tooltip for the given item. Override to do
// anything more complicated
- (NSString *)outlineView:(NSOutlineView *)outlineView tooltipStringForItem:(id)anItem
{
  return [anItem getStringForRDFPropertyURI:@"http://home.netscape.com/NC-rdf#Name"];
}



#pragma mark -


- (void)reloadDataForItem:(id)aItem reloadChildren:(BOOL)aReloadChildren
{
  if (!aItem)
    [mOutlineView reloadData];
  else
    [mOutlineView reloadItem:aItem reloadChildren:aReloadChildren];
}

- (void)invalidateCachedItems
{
  [[self rootRDFItem] invalidateCache];
}

@end
