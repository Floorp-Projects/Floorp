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

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_CID(kRDFServiceCID,	NS_RDFSERVICE_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kAddressBookDB, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);


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

    if (aIID.Equals(nsIAddressBook::GetIID()) ||
        aIID.Equals(nsIAbCard::GetIID()) ||
        aIID.Equals(::nsISupports::GetIID())) {
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
	nsCOMPtr<nsISupportsArray> nameArray, folderArray;

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

	rv = NS_NewISupportsArray(getter_AddRefs(folderArray));
	if(NS_FAILED(rv))
		return NS_ERROR_OUT_OF_MEMORY;

	folderArray->AppendElement(folderResource);

    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
	if(NS_SUCCEEDED(rv))
	{
		nsString nameStr"person";
		nsCOMPtr<nsIRDFLiteral> nameLiteral;

		rdfService->GetLiteral(nameStr.GetUnicode(), getter_AddRefs(nameLiteral));
		nameArray->AppendElement(nameLiteral);
		rv = DoCommand(database, NC_RDF_NEWABCARD, folderArray, nameArray);
	}*/
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
        // ** jt - temporary solution for pickybacking the undo manager into
        // the nsISupportArray
//        if (mTxnMgr)
//           srcArray->InsertElementAt(mTxnMgr, 0);
		rv = db->DoCommand(srcArray, commandResource, argumentArray);
	}

	return rv;

}

