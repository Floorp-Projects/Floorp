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
#include "prmem.h"

/* The definition is nsAddressBook.cpp */
extern const char *kDirectoryDataSourceRoot;

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAbCardCID, NS_ABCARDRESOURCE_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

nsAbDirectory::nsAbDirectory(void)
  :  nsAbRDFResource(),
     mInitialized(PR_FALSE)
{
	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));
}

nsAbDirectory::~nsAbDirectory(void)
{
	if (mSubDirectories)
	{
		PRUint32 count;
		nsresult rv = mSubDirectories->Count(&count);
		NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
		PRInt32 i;
		for (i = count - 1; i >= 0; i--)
			mSubDirectories->RemoveElementAt(i);
	}

}

NS_IMPL_ISUPPORTS_INHERITED(nsAbDirectory, nsAbRDFResource, nsIAbDirectory)

////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsAbDirectory::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbDirectory::OnCardEntryChange
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
			if (personCard)
			{
				personCard->CopyCard(card);
				if (mDatabase)
				{
					personCard->SetAbDatabase(mDatabase);
					nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(personCard, &rv));
					if (NS_FAILED(rv)) 
						return NS_ERROR_NULL_POINTER;
					mDatabase->AddListener(listener);
				}
				nsCOMPtr<nsISupports> cardSupports(do_QueryInterface(personCard));
				if (cardSupports)
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

NS_IMETHODIMP nsAbDirectory::GetChildNodes(nsIEnumerator* *result)
{
  if (!mInitialized) 
  {
    if (!PL_strcmp(mURI, kDirectoryDataSourceRoot) && GetDirList())
	{
		PRInt32 count = GetDirList()->Count();
		/* check: only show personal address book for now */
		/* not showing 4.x address book unitl we have the converting done */
		PRInt32 i;
		for (i = 0; i < count; i++)
		{
			DIR_Server *server = (DIR_Server *)GetDirList()->ElementAt(i);

			if (server->dirType == PABDirectory)
			{
				nsString name(server->fileName);
				PRInt32 pos = name.Find("na2");
				if (pos >= 0) /* check: this is a 4.x file, remove when conversion is done */
					continue;

				char* uriStr = nsnull;
				uriStr = PR_smprintf("%s%s", mURI, server->fileName);
				if (uriStr == nsnull) 
					return NS_ERROR_OUT_OF_MEMORY;

				nsCOMPtr<nsIAbDirectory> childDir;
				AddDirectory(uriStr, getter_AddRefs(childDir));
				if (uriStr)
					PR_smprintf_free(uriStr);

				if (childDir)
				{
					childDir->SetDirName(server->description);
					childDir->SetServer(server);
				}
			}
		}
	}
    mInitialized = PR_TRUE;
  }
  return mSubDirectories->Enumerate(result);
}

NS_IMETHODIMP nsAbDirectory::AddDirectory(const char *uriName, nsIAbDirectory **childDir)
{
	if (!childDir || !uriName)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);

	if(NS_FAILED(rv))
		return rv;
	
	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriName, getter_AddRefs(res));
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
		return rv;        

	mSubDirectories->AppendElement(directory);
	*childDir = directory;
	NS_IF_ADDREF(*childDir);
	 
	return rv;
}

NS_IMETHODIMP nsAbDirectory::GetChildCards(nsIEnumerator* *result)
{
	nsresult rv = GetAbDatabase();

	if (NS_SUCCEEDED(rv) && mDatabase)
	{
		rv = mDatabase->EnumerateCards(this, result);
	}
	return rv;
}

NS_IMETHODIMP nsAbDirectory::GetMailingList(nsIEnumerator **mailingList)
{
	nsresult rv = GetAbDatabase();

	if (NS_SUCCEEDED(rv) && mDatabase)
	{
		rv = mDatabase->EnumerateMailingLists(this, mailingList);
	}
	return rv;
}

NS_IMETHODIMP nsAbDirectory::CreateNewDirectory(const char *dirName, const char *fileName)
{
	if (!dirName)
		return NS_ERROR_NULL_POINTER;

	DIR_Server * server = nsnull;
	nsresult rv = DIR_AddNewAddressBook(dirName, fileName, &server);

	nsCOMPtr<nsIAbDirectory> newDir;
	char *uri = PR_smprintf("%s%s", kDirectoryDataSourceRoot, server->fileName);
	if (uri)
	{
		rv = AddDirectory(uri, getter_AddRefs(newDir));
		PR_smprintf_free(uri);
		if (NS_SUCCEEDED(rv) && newDir)
		{
			newDir->SetDirName(server->description);
			newDir->SetServer(server);

//			nsCOMPtr<nsISupports> dirSupports(do_QueryInterface(newDir, &rv));

//			if (NS_SUCCEEDED(rv))
				NotifyItemAdded(newDir);
			return rv;
		}
		else
			return NS_ERROR_NULL_POINTER;
	}
	return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbDirectory::AddChildCards(const char *uriName, nsIAbCard **childCard)
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

	*childCard = personCard;
	NS_ADDREF(*childCard);

	return rv;
}

NS_IMETHODIMP nsAbDirectory::DeleteCards(nsISupportsArray *cards)
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

nsresult nsAbDirectory::DeleteDirectoryCards(nsIAbDirectory* directory, DIR_Server *server)
{
	nsresult rv = NS_OK;
	nsFileSpec* dbPath = nsnull;
	nsCOMPtr<nsIAddrDatabase> database;


	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
		(*dbPath) += server->fileName;

		// close file before delete it
		NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_FALSE, getter_AddRefs(database), PR_TRUE);
	}

	/* delete cards */
	nsCOMPtr<nsISupportsArray> cardArray;
	nsCOMPtr<nsIEnumerator> cardChild;

	NS_NewISupportsArray(getter_AddRefs(cardArray));
	rv = directory->GetChildCards(getter_AddRefs(cardChild));

	if (NS_SUCCEEDED(rv) && cardChild)
	{
		nsCOMPtr<nsISupports> item;
		rv = cardChild->First();
		if (NS_SUCCEEDED(rv))
		{
			do 
			{
				cardChild->CurrentItem(getter_AddRefs(item));
				if (item)
				{
					nsCOMPtr<nsIAbCard> card;
					card = do_QueryInterface(item, &rv);
					if (card)
					{
						cardArray->AppendElement(card);
					}
				}
				rv = cardChild->Next();
			} while (NS_SUCCEEDED(rv));

			if (database)
			{
				PRUint32 cardCount;
				rv = cardArray->Count(&cardCount);
				if (NS_FAILED(rv)) return rv;
				for(PRUint32 i = 0; i < cardCount; i++)
				{
					nsISupports* cardSupports = cardArray->ElementAt(i);
					nsIAbCard* card = (nsIAbCard*)cardSupports;
					if (card)
					{
						database->DeleteCard(card, PR_TRUE);
					}
				}
			}
		}
	}
	return rv;
}

NS_IMETHODIMP nsAbDirectory::DeleteDirectories(nsISupportsArray *dierctories)
{
	nsresult rv = NS_ERROR_FAILURE;

	PRUint32 i, dirCount;
	rv = dierctories->Count(&dirCount);
	if (NS_FAILED(rv)) return rv;
	for (i = 0; i < dirCount; i++)
	{
		nsCOMPtr<nsISupports> dirSupports;
		nsCOMPtr<nsIAbDirectory> directory;
		dirSupports = getter_AddRefs(dierctories->ElementAt(i));
		directory = do_QueryInterface(dirSupports, &rv);
		if (NS_SUCCEEDED(rv) && directory)
		{
			DIR_Server *server = nsnull;
			rv = directory->GetServer(&server);
			if (server)
			{
				DeleteDirectoryCards(directory, server);

				DIR_DeleteServerFromList(server);

				rv = mSubDirectories->RemoveElement(directory);
				NotifyItemDeleted(directory);
			}
		}
	}

	return rv;
}

NS_IMETHODIMP nsAbDirectory::HasCard(nsIAbCard *cards, PRBool *hasCard)
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

NS_IMETHODIMP nsAbDirectory::HasDirectory(nsIAbDirectory *dir, PRBool *hasDir)
{
	if (!hasDir)
		return NS_ERROR_NULL_POINTER;

	DIR_Server* dirServer = nsnull;
	dir->GetServer(&dirServer);
	nsresult rv = DIR_ContainsServer(dirServer, hasDir);
	return rv;
}

nsresult nsAbDirectory::NotifyPropertyChanged(char *property, char* oldValue, char* newValue)
{
  return NS_OK;
}

nsresult nsAbDirectory::NotifyItemAdded(nsISupports *item)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemAdded(this, item);
	return NS_OK;
}

nsresult nsAbDirectory::NotifyItemDeleted(nsISupports *item)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemDeleted(this, item);

	return NS_OK;
}

