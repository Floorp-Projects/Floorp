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

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAddressBookDB, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

nsABCard::nsABCard(void)
  : nsRDFResource(), mListeners(nsnull),
    mInitialized(PR_FALSE), 
    mCsid(0), mDepth(0), mPrefFlags(0)
{
//  NS_INIT_REFCNT(); done by superclass

	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));

	//The rdf:addresscard datasource is going to be a listener to all nsIMsgFolders, so add
	//it as a listener
 /*   nsresult rv; 
    NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv); 
    if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFDataSource> datasource;
		rv = rdfService->GetDataSource("rdf:addresscard", getter_AddRefs(datasource));
		if(NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIAbListener> directoryListener(do_QueryInterface(datasource, &rv));
			if(NS_SUCCEEDED(rv))
			{
				AddAddrBookListener(directoryListener);
			}
		}
	}*/

}

nsABCard::~nsABCard(void)
{
	if(mSubDirectories)
	{
		PRUint32 count;
		nsresult rv = mSubDirectories->Count(&count);
		NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
		PRInt32 i;
		for (i = count - 1; i >= 0; i--)
			mSubDirectories->RemoveElementAt(i);
	}

	if(mDatabase)
		mDatabase->RemoveListener(this);

	if (mListeners) 
	{
		PRInt32 i;
		for (i = mListeners->Count() - 1; i >= 0; --i) 
			mListeners->RemoveElementAt(i);
		delete mListeners;
	}
}

NS_IMPL_ISUPPORTS_INHERITED(nsABCard, nsRDFResource, nsIAbCard)

////////////////////////////////////////////////////////////////////////////////

typedef PRBool
(*nsArrayFilter)(nsISupports* element, void* data);

#if 0
static nsresult
nsFilterBy(nsISupportsArray* array, nsArrayFilter filter, void* data,
           nsISupportsArray* *result)
{
  nsCOMPtr<nsISupportsArray> f;
  nsresult rv = NS_NewISupportsArray(getter_AddRefs(f));
  if (NS_FAILED(rv)) return rv;
  PRUint32 i, count;
  rv = array->Count(&count);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> element = getter_AddRefs(array->ElementAt(i));
    if (filter(element, data)) {
      rv = f->AppendElement(element);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }
  *result = f;
  return NS_OK;
}
#endif 

////////////////////////////////////////////////////////////////////////////////
/*
NS_IMETHODIMP nsABCard::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsABCard::OnCardEntryChange
(PRUint32 abCode, PRUint32 entryID, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsABCard::OnAnnouncerGoingAway(nsIAddrDBAnnouncer *instigator)
{
  return NS_OK;
}
*/

nsresult nsABCard::AddSubNode(nsAutoString name, nsIAbCard **childCard)
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

	mSubDirectories->AppendElement(card);
	*childCard = card;
	NS_IF_ADDREF(*childCard);

    (void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	return rv;
}

NS_IMETHODIMP nsABCard::ContainsChildNamed(const char *name, PRBool* containsChild)
{
	nsCOMPtr<nsISupports> child;
	
	if(containsChild)
	{
		*containsChild = PR_FALSE;
		if(NS_SUCCEEDED(GetChildNamed(name, getter_AddRefs(child))))
		{
			*containsChild = child != nsnull;
		}
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsABCard::FindParentOf(nsIAbCard * aDirectory, nsIAbCard ** aParent)
{
	if(!aParent)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	*aParent = nsnull;

	PRUint32 i, j, count;
	rv = mSubDirectories->Count(&count);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
	nsCOMPtr<nsISupports> supports;
	nsCOMPtr<nsIAbCard> child;
	for (i = 0; i < count && *aParent == NULL; i++)
	{
		supports = getter_AddRefs(mSubDirectories->ElementAt(i));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			if (aDirectory == child.get())
			{
				*aParent = this;
				NS_ADDREF(*aParent);
				return NS_OK;
			}
		}
	}
	for (j = 0; j < count && *aParent == NULL; j++)
	{
/*
		supports = getter_AddRefs(mSubDirectories->ElementAt(j));
		child = do_QueryInterface(supports, &rv);
		if(NS_SUCCEEDED(rv) && child)
		{
			rv = child->FindParentOf(aDirectory, aParent);
			if(NS_SUCCEEDED(rv))
				return rv;
		}
*/
	}

	return rv;
}

NS_IMETHODIMP nsABCard::IsParentOf(nsIAbCard *child, PRBool deep, PRBool *isParent)
{
	if(!isParent)
		return NS_ERROR_NULL_POINTER;
	
	nsresult rv = NS_OK;

	PRUint32 i, count;
	rv = mSubDirectories->Count(&count);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
 
	for (i = 0; i < count; i++)
	{
		nsCOMPtr<nsISupports> supports = getter_AddRefs(mSubDirectories->ElementAt(i));
		nsCOMPtr<nsIAbCard> directory(do_QueryInterface(supports, &rv));
		if(NS_SUCCEEDED(rv))
		{
			if (directory.get() == child )
				*isParent = PR_TRUE;
			else if(deep)
			{;
//				directory->IsParentOf(child, deep, isParent);
			}
		}
		if(*isParent)
			return NS_OK;
    }
	*isParent = PR_FALSE;
	return rv;
}


#ifdef HAVE_DB
NS_IMETHOD GetTotalPersonsInDB(PRUint32 *totalPersons) const;					// How many messages in database.
#endif
	
