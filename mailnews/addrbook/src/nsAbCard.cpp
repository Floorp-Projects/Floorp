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

#include "nsAbCard.h"	 
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "prmem.h"	 
#include "prlog.h"	 
#include "nsAddrDatabase.h"
#include "nsIAddrBookSession.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

nsAbCard::nsAbCard(void)
  : nsAbRDFResource(), mListeners(nsnull)
{
}

nsAbCard::~nsAbCard(void)
{
	if (mCardDatabase)
	{
		nsIAddrDBListener* listener = this;
		mCardDatabase->RemoveListener(listener);

		mCardDatabase->Close(PR_TRUE);
		mCardDatabase = null_nsCOMPtr();
	}

	if (mListeners) 
	{
		PRInt32 i;
		for (i = mListeners->Count() - 1; i >= 0; --i) 
			mListeners->RemoveElementAt(i);
		delete mListeners;
	}
}

NS_IMPL_ISUPPORTS_INHERITED(nsAbCard, nsAbRDFResource, nsIAbCard)

////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsAbCard::OnCardEntryChange
(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator)
{
	if (abCode == AB_NotifyPropertyChanged && card)
	{
		PRUint32 tableID;
		PRUint32 rowID;

		card->GetDbTableID(&tableID);
		card->GetDbRowID(&rowID);
		if (m_dbTableID == tableID && m_dbRowID == rowID)
		{
			char* pNewStr = nsnull;
			card->GetDisplayName(&pNewStr);
			if (pNewStr)
				NotifyPropertyChanged("DisplayName", nsnull, pNewStr);
			PR_FREEIF(pNewStr);

			card->GetPrimaryEmail(&pNewStr);
			if (pNewStr)
				NotifyPropertyChanged("PrimaryEmail", nsnull, pNewStr);
			PR_FREEIF(pNewStr);

			card->GetWorkPhone(&pNewStr);
			if (pNewStr)
				NotifyPropertyChanged("WorkPhone", nsnull, pNewStr);
			PR_FREEIF(pNewStr);
		}
	}
	return NS_OK;
}

nsresult nsAbCard::NotifyPropertyChanged(char *property, char* oldValue, char* newValue)
{
	nsCOMPtr<nsISupports> supports;
	if(NS_SUCCEEDED(QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), getter_AddRefs(supports))))
	{
		//Notify listeners who listen to every folder
		nsresult rv;
		NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
		if(NS_SUCCEEDED(rv))
			abSession->NotifyItemPropertyChanged(supports, property, oldValue, newValue);
	}

	return NS_OK;
}

nsresult nsAbCard::AddSubNode(nsAutoString name, nsIAbCard **childCard)
{
	if(!childCard)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

	if(NS_FAILED(rv))
		return rv;

	nsAutoString uri;
	uri.Append(mURI);
	uri.Append('/');

	uri.Append(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriStr, getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;
	nsCOMPtr<nsIAbCard> card(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;        
	delete[] uriStr;

	*childCard = card;
	NS_IF_ADDREF(*childCard);

	return rv;
}

