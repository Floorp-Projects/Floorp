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

// These objects cache their children to avoid excessive lookups through
// the gecko API, which can be slow

#import "RDFItem.h"

#import "NSString+Utils.h"

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

#include "nsString.h"


@interface RDFItem (Private)
- (unsigned int)cacheVersion;
- (BOOL)cachedChildren;
- (void)buildChildCache;
- (NSString*)getTextForNode:(nsIRDFNode *)aNode;
@end

#pragma mark -

@implementation RDFItem

- (id)init
{
  if ((self = [super init])) {
    nsCOMPtr<nsIRDFContainer> ctr = do_CreateInstance("@mozilla.org/rdf/container;1");
    NS_ADDREF(mRDFContainer = ctr);

    nsCOMPtr<nsIRDFContainerUtils> ctrUtils = do_GetService("@mozilla.org/rdf/container-utils;1");
    NS_ADDREF(mRDFContainerUtils = ctrUtils);

    nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1");
    NS_ADDREF(mRDFService = rdfService);

    mRDFDataSource = nsnull;
    mRDFResource = nsnull;

    mPropertyCache = [[[NSMutableDictionary alloc] init] retain];
  }
  return self;
}

- (id)initWithRDFResource:(nsIRDFResource*)aRDFResource RDFDataSource:(nsIRDFDataSource*)aRDFDataSource parent:(RDFItem*)newparent;
{
  if( (self = [self init]) ) {
    NS_IF_ADDREF(mRDFResource = aRDFResource);
    mRDFDataSource = aRDFDataSource;
    mParent = newparent;
  }
  return self;
}

- (void)dealloc
{
  [mChildNodes autorelease];
  [mPropertyCache autorelease];
  NS_IF_RELEASE(mRDFResource);
  [super dealloc];
}

- (RDFItem*)newChild;
{
  return [RDFItem alloc];
}

- (void)invalidateCache;
{
  if( mChildNodes ) {
    //invalidate children first (in case they have children)
    [mChildNodes makeObjectsPerformSelector:@selector(invalidateCache)];
    [mChildNodes autorelease];
    mChildNodes = nil;
  }
}

- (BOOL)cachedChildren
{
  return (mChildNodes != nil);
}

- (BOOL)isExpandable
{
  if( mChildNodes )
    return YES;  // cached already

  // Query gecko for the answer
  PRBool isSeq = PR_FALSE;
  mRDFContainerUtils->IsSeq(mRDFDataSource, mRDFResource, &isSeq);
  if (isSeq)
    return YES;

  nsCOMPtr<nsIRDFResource> childRDFProperty;
  mRDFService->GetResource(nsDependentCString("http://home.netscape.com/NC-rdf#child"),
                           getter_AddRefs(childRDFProperty));
  nsCOMPtr<nsISimpleEnumerator> childNodesEnum;
  mRDFDataSource->GetTargets(mRDFResource, childRDFProperty, PR_TRUE, getter_AddRefs(childNodesEnum));

  PRBool hasMore = PR_FALSE;
  if( NS_SUCCEEDED(childNodesEnum->HasMoreElements(&hasMore)) && hasMore ) {
    nsCOMPtr<nsISupports> supp;
    childNodesEnum->GetNext(getter_AddRefs(supp));
    nsCOMPtr<nsIRDFResource> childResource = do_QueryInterface(supp);
    if (childResource)
      return YES;
  }
  return NO;
}


- (int)numChildren;
{
  if( mChildNodes )
    return [mChildNodes count];

  // Since gecko RDF enumerator->HasMoreElements
  // is so slow, we may as well build the cache now
  // because the extra time it takes is tiny
  [self buildChildCache];
  int numChilds = [mChildNodes count];
  return numChilds;
}

// 0- based
- (RDFItem*)childAtIndex:(int)index;
{
  RDFItem* child = nil;
  if (!mChildNodes)
    [self buildChildCache];
  if (index < [mChildNodes count])
    child = [mChildNodes objectAtIndex:index];
  return child;
}

- (void)buildChildCache;
{
  NSMutableArray * childNodes = [[[NSMutableArray alloc] init] retain];
  
  [self invalidateCache];

  nsCOMPtr<nsIRDFResource> childRDFProperty;
  mRDFService->GetResource(nsDependentCString("http://home.netscape.com/NC-rdf#child"),
                           getter_AddRefs(childRDFProperty));
  nsCOMPtr<nsISimpleEnumerator> childNodesEnum;
  mRDFDataSource->GetTargets(mRDFResource, childRDFProperty, PR_TRUE, getter_AddRefs(childNodesEnum));

  PRBool hasMore = PR_FALSE;
  // this call to HasMoreElements can take more than 10 ms !!
  // maybe not any more though
  while( NS_SUCCEEDED(childNodesEnum->HasMoreElements(&hasMore)) && hasMore ) {
    nsCOMPtr<nsISupports> supp;
    childNodesEnum->GetNext(getter_AddRefs(supp));
    nsCOMPtr<nsIRDFResource> childResource = do_QueryInterface(supp);
    if (childResource) {
      RDFItem * childItem = [self newChild];
      [childItem initWithRDFResource:childResource RDFDataSource:mRDFDataSource parent:self];
      if (childItem)
        [childNodes addObject:childItem];
    }
  }
  mChildNodes = [childNodes retain];
}

- (void)deleteChildFromCache:(RDFItem*)child;
{
  [mChildNodes removeObject:child];
}

- (RDFItem*)parent;
{
  return mParent;
}


#pragma mark -

- (NSString*)getStringForRDFPropertyURI:(NSString*)aPropertyURI;
{
  NSString * cached = [mPropertyCache objectForKey:aPropertyURI];
  if( cached )
    return cached;

  nsCOMPtr<nsIRDFResource> myRDFProperty;
  mRDFService->GetResource(nsDependentCString([aPropertyURI UTF8String]),
                           getter_AddRefs(myRDFProperty));
  nsCOMPtr<nsIRDFNode> resultNode;
  nsresult rv;
  rv = mRDFDataSource->GetTarget(mRDFResource, myRDFProperty, PR_TRUE, getter_AddRefs(resultNode));
  if( NS_FAILED(rv) )
    return @"mRDFDataSource->GetTarget NS_FAILED";
  if (!resultNode) {
//    NSLog(@"resultNode is null in getStringForRDFPropertyURI, aPropertyURI= %@", aPropertyURI);
    return @"";
  }
  NSString * result = [self getTextForNode:resultNode];
  [mPropertyCache setObject:result forKey:aPropertyURI];
  return result;
}

- (NSString *)description;
{
  return [self getStringForRDFPropertyURI:@"http://home.netscape.com/NC-rdf#Name"];
}

- (NSString*)getTextForNode:(nsIRDFNode *)aNode;
{
  nsString aResult;
  NSString * resultString = nil;

  nsresult        rv;
  nsIRDFResource  *resource;
  nsIRDFLiteral   *literal;
  nsIRDFDate      *dateLiteral;
  nsIRDFInt       *intLiteral;
  if (! aNode) {
    aResult.Truncate();
    rv = NS_OK;
  }
  else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFResource), (void**) &resource))) {
    const char  *p = nsnull;
    if (NS_SUCCEEDED(rv = resource->GetValueConst( &p )) && (p)) {
      aResult.AssignWithConversion(p);
    }
    NS_RELEASE(resource);
  }
  else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFDate), (void**) &dateLiteral))) {
    PRInt64     theDate, million;
    if (NS_SUCCEEDED(rv = dateLiteral->GetValue( &theDate ))) {
      LL_I2L(million, PR_USEC_PER_SEC);
      LL_DIV(theDate, theDate, million);          // convert from microseconds (PRTime) to seconds
      PRInt32     now32;
      LL_L2I(now32, theDate);
      double interval = (double)now32;
      NSDate * date = [NSDate dateWithTimeIntervalSince1970:interval];
      resultString = [date description];
    }
    NS_RELEASE(dateLiteral);
  }
  else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFInt), (void**) &intLiteral)))
  {
    PRInt32     theInt;
    aResult.Truncate();
    if (NS_SUCCEEDED(rv = intLiteral->GetValue( &theInt ))) {
      aResult.AppendInt(theInt, 10);
    }
    NS_RELEASE(intLiteral);
  }
  else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFLiteral), (void**) &literal))) {
    const PRUnichar     *p = nsnull;
    if (NS_SUCCEEDED(rv = literal->GetValueConst( &p )) && (p)) {
      aResult = p;
    }
    NS_RELEASE(literal);
  }
  else {
    NS_ERROR("not a resource or a literal");
    rv = NS_ERROR_UNEXPECTED;
    return @"Not a resource or a literal";
  }
  if( !resultString )
    resultString = [NSString stringWith_nsAString:aResult];
  return resultString;
}

@end
