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

#include "nsAbDirectory.h"	 
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsAbCard.h"
#include "nsAddrDatabase.h"
#include "nsIAbListener.h"
#include "nsIAddrBookSession.h"

#include "nsIFileSpec.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "mdb.h"
#include "prlog.h"
#include "prprf.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kAbCardCID, NS_ABCARDRESOURCE_CID);
static NS_DEFINE_CID(kAddressBookDB, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

nsABDirectory::nsABDirectory(void)
  :  nsAbRDFResource(),
     mInitialized(PR_FALSE), mCardInitialized(PR_FALSE),
     mCsid(0), mDepth(0), mPrefFlags(0)
{
	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));
}

nsABDirectory::~nsABDirectory(void)
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

}

NS_IMPL_ISUPPORTS_INHERITED(nsABDirectory, nsAbRDFResource, nsIAbDirectory)

////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP 
nsABDirectory::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsABDirectory::OnCardEntryChange
(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator)
{
	nsresult rv = NS_OK;
	if (abCode == AB_NotifyInserted && card)
	{ 
		NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

		if(NS_FAILED(rv))
			return rv;

		char* cardURI = nsnull;
		rv = card->GetCardURI(&cardURI);
		if (NS_FAILED(rv) || !cardURI)
			return NS_ERROR_NULL_POINTER;

		nsCOMPtr<nsIRDFResource> res;
		rv = rdf->GetResource(cardURI, getter_AddRefs(res));
		if(cardURI)
			PR_smprintf_free(cardURI);
		if (NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIAbCard> personCard = do_QueryInterface(res);
			personCard->CopyCard(card);
			nsCOMPtr<nsISupports> cardSupports(do_QueryInterface(personCard));
			if (cardSupports)
			{
				NotifyItemAdded(cardSupports);
			}
		}
	}
	else if (abCode == AB_NotifyDeleted && card)
	{
		nsCOMPtr<nsISupports> cardSupports(do_QueryInterface(card, &rv));
		if(NS_SUCCEEDED(rv))
			NotifyItemDeleted(cardSupports);
	}
	return NS_OK;
}

NS_IMETHODIMP 
nsABDirectory::AddUnique(nsISupports* element)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsABDirectory::ReplaceElement(nsISupports* element, nsISupports* newElement)
{
  PR_ASSERT(0);
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsABDirectory::GetChildNodes(nsIEnumerator* *result)
{
  if (!mInitialized) 
  {
    if (!PL_strcmp(mURI, "abdirectory:/") && GetDirList())
	{
		PRInt32 count = GetDirList()->Count();
		/* check: set count = 1 for personal addressbook only for now*/
		count = 1;
		PRInt32 i;
		for (i = 0; i < count; i++)
		{
			DIR_Server *server = (DIR_Server *)GetDirList()->ElementAt(i);
			nsCOMPtr<nsIAbDirectory> childDir;
			nsAutoString currentDirStr = "Pab";
			currentDirStr.Append(server->position, 10);
			AddSubDirectory(currentDirStr, getter_AddRefs(childDir));
		}
	}
    mInitialized = PR_TRUE;
  }
  return mSubDirectories->Enumerate(result);
}

nsresult nsABDirectory::AddSubDirectory(nsAutoString name, nsIAbDirectory **childDir)
{
	if(!childDir)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

	if(NS_FAILED(rv))
		return rv;
	
	nsAutoString uri;
	uri.Append(mURI);
	uri.Append("/");

	uri.Append(name);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriStr, getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;
	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;        
	delete[] uriStr;

	mSubDirectories->AppendElement(directory);
	*childDir = directory;
	NS_IF_ADDREF(*childDir);
	 
    (void)nsServiceManager::ReleaseService(kRDFServiceCID, rdf);

	return rv;
}

NS_IMETHODIMP nsABDirectory::GetChildCards(nsIEnumerator* *result)
{
	nsresult rv = GetAbDatabase();

	if (NS_SUCCEEDED(rv) && mDatabase)
	{
		rv = mDatabase->EnumerateCards(this, result);
	}
	return rv;
}

NS_IMETHODIMP nsABDirectory::AddChildCards(const char *uriName, nsIAbCard **childCard)
{
	if(!childCard)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

	if(NS_FAILED(rv))
		return rv;

	nsAutoString uri(uriName);
	char* uriStr = uri.ToNewCString();
	if (uriStr == nsnull) 
		return NS_ERROR_OUT_OF_MEMORY;

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriStr, getter_AddRefs(res));
	if (NS_FAILED(rv))
	{
		delete[] uriStr;
		return rv;
	}
	nsCOMPtr<nsIAbCard> personCard(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
	{
		rv = nsComponentManager::CreateInstance(kAbCardCID, nsnull, nsCOMTypeInfo<nsIAbCard>::GetIID(), getter_AddRefs(personCard));
		if (NS_FAILED(rv) || !personCard)
		{
			delete[] uriStr;
			return rv;
		}
	}
	delete[] uriStr;

	mSubDirectories->AppendElement(personCard);
	*childCard = personCard;
	NS_ADDREF(*childCard);

	return rv;
}

NS_IMETHODIMP nsABDirectory::GetDirPosition(PRUint32 *pos)
{
	if (pos)
	{
		*pos = mPos;
		return NS_OK;
	}
	return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsABDirectory::DeleteCards(nsISupportsArray *cards)
{
	nsresult rv = NS_OK;

	if (!mDatabase)
		rv = GetAbDatabase();

	if (NS_SUCCEEDED(rv) && mDatabase)
	{
		PRUint32 cardCount;
		rv = cards->Count(&cardCount);
		if (NS_FAILED(rv)) return rv;
		for(PRUint32 i = 0; i < cardCount; i++)
		{
			nsCOMPtr<nsISupports> cardSupports;
			nsCOMPtr<nsIAbCard> card;
			cardSupports = getter_AddRefs(cards->ElementAt(i));
			card = do_QueryInterface(cardSupports, &rv);
			if (card)
			{
				mDatabase->DeleteCard(card, PR_TRUE);
			}
		}
		mDatabase->Commit(kLargeCommit);
	}
	return rv;
}

NS_IMETHODIMP nsABDirectory::HasCard(nsIAbCard *cards, PRBool *hasCard)
{
	if(!hasCard)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	if (!mDatabase)
		rv = GetAbDatabase();

	if(NS_SUCCEEDED(rv) && mDatabase)
	{
		if(NS_SUCCEEDED(rv))
			rv = mDatabase->ContainsCard(cards, hasCard);
	}
	return rv;
}

NS_IMETHODIMP nsABDirectory::CreateCardFromDirectory(nsIAbCard* *result)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsABDirectory::NotifyPropertyChanged(char *property, char* oldValue, char* newValue)
{
  return NS_OK;
}

nsresult nsABDirectory::NotifyItemAdded(nsISupports *item)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemAdded(this, item);
	return NS_OK;
}

nsresult nsABDirectory::NotifyItemDeleted(nsISupports *item)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemDeleted(this, item);

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsABDirectory::GetName(char **name)
{
	if(!name)
		return NS_ERROR_NULL_POINTER;

	if (!PL_strcmp(mURI, "abdirectory://Pab1/Card1"))
		SetName("Person1");
	if (!PL_strcmp(mURI, "abdirectory://Pab1/Card2"))
		SetName("Person2");
	if (!PL_strcmp(mURI, "abdirectory://Pab2/Card1"))
		SetName("Person3");
	if (!PL_strcmp(mURI, "abdirectory://Pab2/Card2"))
		SetName("Person4");
	if (!PL_strcmp(mURI, "abdirectory://Pab3/Card1"))
		SetName("Person5");
	if (!PL_strcmp(mURI, "abdirectory://Pab3/Card2"))
		SetName("Person6");
	else if (GetDirList())
	{
		PRInt32 count = GetDirList()->Count();
		PRInt32 i;
		/* check: set count = 1 for personal addressbook only for now*/
		count = 1;
		for (i = 0; i < count; i++)
		{
			DIR_Server *server = (DIR_Server *)GetDirList()->ElementAt(i);
			nsCOMPtr<nsIAbDirectory> childDir;
			nsAutoString currentDirStr = "abdirectory://Pab";
			currentDirStr.Append(server->position, 10);
			char* dirUrl = currentDirStr.ToNewCString();
			if (dirUrl == nsnull) 
				return NS_ERROR_OUT_OF_MEMORY;
			if (!PL_strcmp(mURI, dirUrl))
			{
				SetName(server->description);
				mPos = server->position;
			}
			delete[] dirUrl;
		}
	}
	*name = mDirName.ToNewCString();
	return NS_OK;
}

NS_IMETHODIMP nsABDirectory::SetName(char * name)
{
	mDirName = name;
	return NS_OK;
}

NS_IMETHODIMP nsABDirectory::GetParent(nsIAbBase* *parent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectory::SetParent(nsIAbBase *parent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectory::GetChildNamed(const char* name, nsISupports* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsABDirectory::ContainsChildNamed(const char *name, PRBool* containsChild)
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

NS_IMETHODIMP nsABDirectory::FindParentOf(nsIAbDirectory * aDirectory, nsIAbDirectory ** aParent)
{
	if(!aParent)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	*aParent = nsnull;

	PRUint32 i, j, count;
	rv = mSubDirectories->Count(&count);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
	nsCOMPtr<nsISupports> supports;
	nsCOMPtr<nsIAbDirectory> child;

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

NS_IMETHODIMP nsABDirectory::IsParentOf(nsIAbDirectory *child, PRBool deep, PRBool *isParent)
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
		nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(supports, &rv));
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
	

