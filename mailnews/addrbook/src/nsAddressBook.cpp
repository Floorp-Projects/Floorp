/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIAddressBook.h"
#include "nsAddressBook.h"
#include "nsAbBaseCID.h"

#include "prmem.h"

#include "nsCOMPtr.h"
#include "nsIDOMXULTreeElement.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFResource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"

static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddressBookDB, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);


static nsresult ConvertDOMListToResourceArray(nsIDOMNodeList *nodeList, nsISupportsArray **resourceArray)
{
	nsresult rv = NS_OK;
	PRUint32 listLength;
	nsIDOMNode *node;
	nsIDOMXULTreeElement *xulElement;
	nsIRDFResource *resource;

	if(!resourceArray)
		return NS_ERROR_NULL_POINTER;

	if(NS_FAILED(rv = nodeList->GetLength(&listLength)))
		return rv;

	if(NS_FAILED(NS_NewISupportsArray(resourceArray)))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	for(PRUint32 i = 0; i < listLength; i++)
	{
		if(NS_FAILED(nodeList->Item(i, &node)))
			return rv;

		if(NS_SUCCEEDED(rv = node->QueryInterface(nsCOMTypeInfo<nsIDOMXULElement>::GetIID(), (void**)&xulElement)))
		{
			if(NS_SUCCEEDED(rv = xulElement->GetResource(&resource)))
			{
				(*resourceArray)->AppendElement(resource);
				NS_RELEASE(resource);
			}
			NS_RELEASE(xulElement);
		}
		NS_RELEASE(node);
		
	}

	return rv;
}

//
// nsAddressBook
//
nsAddressBook::nsAddressBook()
{
	NS_INIT_REFCNT();
}

nsAddressBook::~nsAddressBook()
{
}

NS_IMPL_ISUPPORTS(nsAddressBook, nsIAddressBook::GetIID());
/*
NS_IMPL_ADDREF(nsAddressBook)
NS_IMPL_RELEASE(nsAddressBook)

NS_IMETHODIMP nsAddressBook::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsCOMTypeInfo<nsIAddressBook>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIAbCard>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIAddressBook*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   
*/
//
// nsIAddressBook
//

//NS_IMETHODIMP nsAddressBook::NewCard(nsIRDFCompositeDataSource *database, nsIDOMXULElement *parentFolderElement,
//						nsIAbCard *card)
NS_IMETHODIMP nsAddressBook::NewCard()
{
	nsresult rv = NS_OK;
/*	nsCOMPtr<nsIRDFResource> folderResource;
	nsCOMPtr<nsISupportsArray> nameArray, dirArray;

	if(!parentFolderElement || !card)
		return NS_ERROR_NULL_POINTER;

	rv = parentFolderElement->GetResource(getter_AddRefs(folderResource));
	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(nameArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	rv = NS_NewISupportsArray(getter_AddRefs(dirArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	dirArray->AppendElement(folderResource);

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_SUCCEEDED(rv))
	{
		nsString nameStr"person";
		nsCOMPtr<nsIRDFLiteral> nameLiteral;

		rdfService->GetLiteral(nameStr.GetUnicode(), getter_AddRefs(nameLiteral));
		nameArray->AppendElement(nameLiteral);
		rv = DoCommand(database, NC_RDF_NEWABCARD, dirArray, nameArray);
	}*/
	return rv;
}

NS_IMETHODIMP nsAddressBook::DeleteCards
(nsIDOMXULTreeElement *tree, nsIDOMXULElement *srcDirectory, nsIDOMNodeList *nodeList)
{
	nsresult rv;

	if(!tree || !srcDirectory || !nodeList)
		return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIRDFCompositeDataSource> database;
	nsCOMPtr<nsISupportsArray> resourceArray, dirArray;
	nsCOMPtr<nsIRDFResource> resource;

	rv = srcDirectory->GetResource(getter_AddRefs(resource));

	if(NS_FAILED(rv))
		return rv;

	rv = tree->GetDatabase(getter_AddRefs(database));
	if(NS_FAILED(rv))
		return rv;

	rv = ConvertDOMListToResourceArray(nodeList, getter_AddRefs(resourceArray));
	if(NS_FAILED(rv))
		return rv;

	rv = NS_NewISupportsArray(getter_AddRefs(dirArray));
	if(NS_FAILED(rv))
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}

	dirArray->AppendElement(resource);
	
	rv = DoCommand(database, NC_RDF_DELETE, dirArray, resourceArray);

	return rv;
}

nsresult nsAddressBook::DoCommand(nsIRDFCompositeDataSource* db, char *command,
						nsISupportsArray *srcArray, nsISupportsArray *argumentArray)
{

	nsresult rv;

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIRDFResource> commandResource;
	rv = rdfService->GetResource(command, getter_AddRefs(commandResource));
	if(NS_SUCCEEDED(rv))
	{
		rv = db->DoCommand(srcArray, commandResource, argumentArray);
	}

	return rv;

}

