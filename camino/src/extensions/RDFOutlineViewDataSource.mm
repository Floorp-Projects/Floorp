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

#import "CHRDFOutlineViewDataSource.h"

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



@implementation CHRDFOutlineViewDataSource

- (void) ensureDataSourceLoaded
{
    nsCOMPtr<nsIRDFContainer> ctr = do_CreateInstance("@mozilla.org/rdf/container;1");
    NS_ADDREF(mContainer = ctr);
        
    nsCOMPtr<nsIRDFContainerUtils> ctrUtils = do_GetService("@mozilla.org/rdf/container-utils;1");
    NS_ADDREF(mContainerUtils = ctrUtils);
        
    nsCOMPtr<nsIRDFService> rdfService = do_GetService("@mozilla.org/rdf/rdf-service;1");
    NS_ADDREF(mRDFService = rdfService);
        
    mDataSource = nsnull;
    mRootResource = nsnull;

    mDictionary = [[NSMutableDictionary alloc] initWithCapacity: 30];
}

- (void) dealloc
{
    NS_IF_RELEASE(mContainer);
    NS_IF_RELEASE(mContainerUtils);
    NS_IF_RELEASE(mRDFService);
    
    NS_IF_RELEASE(mDataSource);
    NS_IF_RELEASE(mRootResource);
    
    [mDictionary release];
    
    [super dealloc];
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
- (BOOL) outlineView: (NSOutlineView*) aOutlineView shouldEditTableColumn: (NSTableColumn*) aTableColumn
                                                    item: (id) aItem
{
    return NO;
}

- (BOOL) outlineView: (NSOutlineView*) aOutlineView isItemExpandable: (id) aItem
{
    if (!mDataSource)
        return NO;
    
    if (!aItem)
        return YES; // The root is always open
    
    nsCOMPtr<nsIRDFResource> itemResource = dont_AddRef([aItem resource]);
    
    PRBool isSeq = PR_FALSE;
    mContainerUtils->IsSeq(mDataSource, itemResource, &isSeq);
    if (isSeq)
        return YES;
    
    nsCOMPtr<nsIRDFResource> childProperty;
    mRDFService->GetResource("http://home.netscape.com/NC-rdf#child", getter_AddRefs(childProperty));
    
    nsCOMPtr<nsIRDFNode> childNode;
    mDataSource->GetTarget(itemResource, childProperty, PR_TRUE, getter_AddRefs(childNode));
    
    return childNode != nsnull;
}

- (id) outlineView: (NSOutlineView*) aOutlineView child: (int) aIndex
                                                  ofItem: (id) aItem
{
    if (!mDataSource)
        return nil;
    
    nsCOMPtr<nsIRDFResource> resource = !aItem ? dont_AddRef([self rootResource]) : dont_AddRef([aItem resource]);
    
    nsCOMPtr<nsIRDFResource> ordinalResource;
    mContainerUtils->IndexToOrdinalResource(aIndex + 1, getter_AddRefs(ordinalResource));
    
    nsCOMPtr<nsIRDFNode> childNode;
    mDataSource->GetTarget(resource, ordinalResource, PR_TRUE, getter_AddRefs(childNode));
    if (childNode) {
        // Yay. A regular container. We don't need to count, we can go directly to 
        // our object. 
        nsCOMPtr<nsIRDFResource> childResource(do_QueryInterface(childNode));
        if (childResource) 
            return [self MakeWrapperFor:childResource];
    }
    else
    {
        // Oh well, not a regular container. We need to count, dagnabbit. 
        nsCOMPtr<nsIRDFResource> childProperty;
        mRDFService->GetResource("http://home.netscape.com/NC-rdf#child", getter_AddRefs(childProperty));

        nsCOMPtr<nsISimpleEnumerator> childNodes;
        mDataSource->GetTargets(resource, childProperty, PR_TRUE, getter_AddRefs(childNodes));
        
        nsCOMPtr<nsISupports> supp;
        PRInt32 count = 0;

        PRBool hasMore = PR_FALSE;
        while (NS_SUCCEEDED(childNodes->HasMoreElements(&hasMore)) && hasMore)
        {
            childNodes->GetNext(getter_AddRefs(supp));
            if (count == aIndex)
                break;
            count ++;
        }

        nsCOMPtr<nsIRDFResource> childResource(do_QueryInterface(supp));
        if (childResource) {
            return [self MakeWrapperFor:childResource];
        }
    }

    return nil;
}
    
- (int) outlineView: (NSOutlineView*) aOutlineView numberOfChildrenOfItem: (id) aItem;
{
    if (!mDataSource)
        return 0;
    
    nsCOMPtr<nsIRDFResource> resource = dont_AddRef(aItem ? [aItem resource] : [self rootResource]);
        
    // XXX just assume NC:child is the only containment arc for now
    nsCOMPtr<nsIRDFResource> childProperty;
    mRDFService->GetResource("http://home.netscape.com/NC-rdf#child", getter_AddRefs(childProperty));
    
    nsCOMPtr<nsISimpleEnumerator> childNodes;
    mDataSource->GetTargets(resource, childProperty, PR_TRUE, getter_AddRefs(childNodes));
    
    PRBool hasMore = PR_FALSE;
    PRInt32 count = 0;
        
    while (NS_SUCCEEDED(childNodes->HasMoreElements(&hasMore)) && hasMore)
    {
        nsCOMPtr<nsISupports> supp;
        childNodes->GetNext(getter_AddRefs(supp));
        count ++;
    }
    
    if (count == 0) {
        nsresult rv = mContainer->Init(mDataSource, resource);
        if (NS_FAILED(rv))
            return 0;
        
        mContainer->GetCount(&count);
    }
    
    return count;
}

- (id) outlineView: (NSOutlineView*) aOutlineView objectValueForTableColumn: (NSTableColumn*) aTableColumn
                                                  byItem: (id) aItem
{
    if (!mDataSource || !aItem)
        return nil;
    
    // The table column's identifier is the RDF Resource URI of the property being displayed in
    // that column, e.g. "http://home.netscape.com/NC-rdf#Name"
    NSString* columnPropertyURI = [aTableColumn identifier];
    
    nsCOMPtr<nsIRDFResource> propertyResource;
    mRDFService->GetResource([columnPropertyURI cString], getter_AddRefs(propertyResource));
            
    nsCOMPtr<nsIRDFResource> resource = dont_AddRef([aItem resource]);
            
    nsCOMPtr<nsIRDFNode> valueNode;
    mDataSource->GetTarget(resource, propertyResource, PR_TRUE, getter_AddRefs(valueNode));
    if (!valueNode) {
        NSLog(@"ValueNode is null!");
        return nil;
    }
    
    nsCOMPtr<nsIRDFLiteral> valueLiteral(do_QueryInterface(valueNode));
    if (!valueLiteral)
        return nil;
    
    nsXPIDLString literalValue;
    valueLiteral->GetValue(getter_Copies(literalValue));

    return [NSString stringWithCharacters: literalValue.get() length:literalValue.Length()];
}

- (void) outlineView: (NSOutlineView*) aOutlineView setObjectValue: (id) aObject
                                                    forTableColumn: (NSTableColumn*) aTableColumn
                                                    byItem: (id) aItem
{

}

- (void) reloadDataForItem: (id) aItem reloadChildren: (BOOL) aReloadChildren
{
    if (!aItem)
        [mOutlineView reloadData];
    else
        [mOutlineView reloadItem: aItem reloadChildren: aReloadChildren];
}

- (id) MakeWrapperFor: (nsIRDFResource*) aRDFResource
{
    RDFOutlineViewItem* item = [[[RDFOutlineViewItem alloc] init] autorelease];
    [item setResource: aRDFResource];
    // keep a copy around
    const char* resourceValue;
    aRDFResource->GetValueConst(&resourceValue);
    
    [mDictionary setObject:item forKey:[NSString stringWithCString:resourceValue]];
    return item;
}


@end

@implementation RDFOutlineViewItem

- (void) dealloc
{
    NS_IF_RELEASE(mResource);
    [super dealloc];
}

- (nsIRDFResource*) resource
{
    NS_IF_ADDREF(mResource);
    return mResource;
}

- (void) setResource: (nsIRDFResource*) aResource
{
    nsIRDFResource* oldResource = mResource;
    NS_IF_ADDREF(mResource = aResource);
    NS_IF_RELEASE(oldResource);
}

@end
