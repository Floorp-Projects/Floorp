/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsAbRDFResource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsAbBaseCID.h"
#include "prmem.h"

#include "nsAddrDatabase.h"
#include "nsIAddrBookSession.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

/* The definition is nsAddressBook.cpp */
extern const char *kDirectoryDataSourceRoot;

/* The definition is nsAddrDatabase.cpp */
extern const char *kMainPersonalAddressBook;


nsAbRDFResource::nsAbRDFResource(void)
{
	NS_INIT_REFCNT();

	mDatabase = null_nsCOMPtr();
}

nsAbRDFResource::~nsAbRDFResource(void)
{
	if (mDatabase)
	{
		mDatabase->RemoveListener(this);
		mDatabase->Close(PR_TRUE);
		mDatabase = null_nsCOMPtr();
	}
}

NS_IMPL_ISUPPORTS_INHERITED(nsAbRDFResource, nsRDFResource, nsIAddrDBListener)

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAbRDFResource::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbRDFResource::OnCardEntryChange
(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbRDFResource::OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator)
{
  return NS_OK;
}

nsresult nsAbRDFResource::GetAbDatabase()
{
	nsresult rv = NS_OK;
	if (!mDatabase && mURI)
	{
		nsFileSpec* dbPath = nsnull;

		NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
		if(NS_SUCCEEDED(rv))
			abSession->GetUserProfileDirectory(&dbPath);
		
		const char* file = nsnull;
		file = &(mURI[PL_strlen(kDirectoryDataSourceRoot)]);
		(*dbPath) += file;

		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(mDatabase), PR_TRUE);

		if (mDatabase)
			mDatabase->AddListener(this);

		return NS_OK;
	}
	if (!mDatabase)
		return NS_ERROR_NULL_POINTER;
	return NS_OK;
}
