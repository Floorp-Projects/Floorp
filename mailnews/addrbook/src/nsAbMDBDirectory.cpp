/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsAbMDBDirectory.h"	 
#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsAbBaseCID.h"
#include "nsAddrDatabase.h"
#include "nsIAbMDBCard.h"
#include "nsIAbListener.h"
#include "nsIAddrBookSession.h"
#include "nsIAddressBook.h"

#include "nsAbMDBCardProperty.h"

#include "mdb.h"
#include "prlog.h"
#include "prprf.h"
#include "prmem.h"


static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAbCardCID, NS_ABMDBCARD_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);

nsAbMDBDirectory::nsAbMDBDirectory(void)
  :  nsAbMDBRDFResource(),
     mInitialized(PR_FALSE),
	 mIsMailingList(-1)
{
	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));
}

nsAbMDBDirectory::~nsAbMDBDirectory(void)
{
	if (mURI && PL_strcmp(mURI, kMDBDirectoryRoot))
	{
		nsresult rv = NS_OK;

		nsCOMPtr<nsIAddrDatabase> database;
		nsCOMPtr<nsIAddressBook> addressBook(do_GetService(kAddrBookCID, &rv)); 
		if (NS_SUCCEEDED(rv) && addressBook)
		{
			rv = addressBook->GetAbDatabaseFromURI(mURI, getter_AddRefs(database));
			if (NS_SUCCEEDED(rv) && database)
				database->RemoveListener(this);
		}
	}
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

NS_IMPL_ISUPPORTS_INHERITED2(nsAbMDBDirectory, nsAbMDBRDFResource, nsIAbDirectory, nsIAbMDBDirectory)

////////////////////////////////////////////////////////////////////////////////



// Not called
nsresult nsAbMDBDirectory::AddMailList(const char *uriName)
{
	if (!uriName)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
	NS_ENSURE_SUCCESS(rv, rv);
	
	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriName, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	mSubDirectories->AppendElement(directory);
	 
	return rv;
}

nsresult nsAbMDBDirectory::RemoveCardFromAddressList(const nsIAbCard* card)
{
	nsresult rv = NS_OK;
	PRUint32 listTotal;
	PRInt32 i, j;
	rv = m_AddressList->Count(&listTotal);
	for (i = listTotal - 1; i >= 0; i--)
	{						
		nsCOMPtr<nsISupports> pSupport = getter_AddRefs(m_AddressList->ElementAt(i));
		if (!pSupport)
			continue;

		nsCOMPtr<nsIAbDirectory> listDir(do_QueryInterface(pSupport, &rv));
		if (listDir)
		{
			nsISupportsArray* pAddressLists;
			listDir->GetAddressLists(&pAddressLists);
			if (pAddressLists)
			{	
				PRUint32 total;
				rv = pAddressLists->Count(&total);
				for (j = total - 1; j >= 0; j--)
				{
					nsCOMPtr<nsISupports> pSupport = getter_AddRefs(pAddressLists->ElementAt(j));
					nsCOMPtr<nsIAbCard> cardInList(do_QueryInterface(pSupport, &rv));
					if (card == cardInList.get())
						pAddressLists->RemoveElementAt(j);
				}
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::DeleteDirectory(nsIAbDirectory *directory)
{
	nsresult rv = NS_OK;
	
	if (!directory)
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIAbMDBDirectory> dbdirectory(do_QueryInterface(directory, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	nsXPIDLCString uri;
	rv = dbdirectory->GetDirUri(getter_Copies(uri));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAddrDatabase> database;
	nsCOMPtr<nsIAddressBook> addresBook(do_GetService(kAddrBookCID, &rv)); 
	if (NS_SUCCEEDED(rv))
	{
		rv = addresBook->GetAbDatabaseFromURI((const char *)uri, getter_AddRefs(database));				

		if (NS_SUCCEEDED(rv))
			rv = database->DeleteMailList(directory, PR_TRUE);

		if (NS_SUCCEEDED(rv))
			database->Commit(kLargeCommit);

		if (m_AddressList)
			m_AddressList->RemoveElement(directory);
		rv = mSubDirectories->RemoveElement(directory);

		NotifyItemDeleted(directory);
	}
	

	return rv;
}

nsresult nsAbMDBDirectory::NotifyPropertyChanged(char *property, PRUnichar* oldValue, PRUnichar* newValue)
{
	nsCOMPtr<nsISupports> supports;
	if(NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsISupports), getter_AddRefs(supports))))
	{
		//Notify listeners who listen to every folder
		nsresult rv;
		nsCOMPtr<nsIAddrBookSession> abSession = 
		         do_GetService(kAddrBookSessionCID, &rv); 
		if(NS_SUCCEEDED(rv))
			abSession->NotifyItemPropertyChanged(supports, property, oldValue, newValue);
	}

	return NS_OK;
}

nsresult nsAbMDBDirectory::NotifyItemAdded(nsISupports *item)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemAdded(this, item);
	return NS_OK;
}

nsresult nsAbMDBDirectory::NotifyItemDeleted(nsISupports *item)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemDeleted(this, item);

	return NS_OK;
}









// nsIAbMDBDirectory methods

NS_IMETHODIMP nsAbMDBDirectory::ClearDatabase()
{ 			
	if (mDatabase)
	{
		mDatabase->RemoveListener(this);
		mDatabase = null_nsCOMPtr(); 
	}
	return NS_OK; 
}

NS_IMETHODIMP nsAbMDBDirectory::RemoveElementsFromAddressList()
{
	if (m_AddressList)
	{
		PRUint32 count;
		nsresult rv = m_AddressList->Count(&count);
		NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
		PRInt32 i;
		for (i = count - 1; i >= 0; i--)
			m_AddressList->RemoveElementAt(i);
	}
	m_AddressList = null_nsCOMPtr();
	return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::RemoveEmailAddressAt(PRUint32 aIndex)
{
	if (m_AddressList)
	{
		return m_AddressList->RemoveElementAt(aIndex);
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbMDBDirectory::AddChildCards(const char *uriName, nsIAbCard **childCard)
{
	if(!childCard)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriName, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAbCard> personCard(do_QueryInterface(res, &rv));
	if (NS_FAILED(rv))
	{
		rv = nsComponentManager::CreateInstance(kAbCardCID, nsnull, NS_GET_IID(nsIAbCard), getter_AddRefs(personCard));
    NS_ENSURE_SUCCESS(rv, rv);
		if (!personCard)
			return NS_ERROR_NULL_POINTER;
	}

	*childCard = personCard;
	NS_ADDREF(*childCard);

	return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::AddDirectory(const char *uriName, nsIAbDirectory **childDir)
{
	if (!childDir || !uriName)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
	
	nsCOMPtr<nsIRDFResource> res;
	rv = rdf->GetResource(uriName, getter_AddRefs(res));
  NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(res, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	mSubDirectories->AppendElement(directory);
	*childDir = directory;
	NS_IF_ADDREF(*childDir);
	 
	return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::GetDirUri(char **uri)
{
	if (uri)
	{
		if (mURI)
			*uri = nsCRT::strdup(mURI);
		else
			*uri = nsCRT::strdup("");
		return NS_OK;
	}
	else
		return NS_RDF_NO_VALUE;
}

// nsIAbDirectory methods

NS_IMETHODIMP nsAbMDBDirectory::GetChildNodes(nsIEnumerator* *result)
{
	if (!mInitialized) 
	{
		mInitialized = PR_TRUE;
	}
	return mSubDirectories->Enumerate(result);
}

NS_IMETHODIMP nsAbMDBDirectory::GetChildCards(nsIEnumerator* *result)
{
	if (mURI && mIsMailingList == -1)
	{
		nsAutoString file; file.AssignWithConversion(&(mURI[PL_strlen(kMDBDirectoryRoot)]));
		PRInt32 pos = file.Find("/");
		if (pos != -1)
			mIsMailingList = 1;
		else
			mIsMailingList = 0;
	}

	nsresult rv = GetAbDatabase();

	if (NS_SUCCEEDED(rv) && mDatabase)
	{
		if (mIsMailingList == 0)
			rv = mDatabase->EnumerateCards(this, result);
		else if (mIsMailingList == 1)
			rv = mDatabase->EnumerateListAddresses(this, result);
	}

	return rv;
}


// Not called
nsresult nsAbMDBDirectory::DeleteDirectoryCards(nsIAbDirectory* directory, DIR_Server *server)
{
	if (!server->fileName)  // file name does not exist
		return NS_OK;
	if (PL_strlen(server->fileName) == 0)  // file name does not exist
		return NS_OK;

	nsresult rv = NS_OK;
	nsFileSpec* dbPath = nsnull;
	nsCOMPtr<nsIAddrDatabase> database;


	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->GetUserProfileDirectory(&dbPath);
	
	if (dbPath)
	{
		(*dbPath) += server->fileName;

		// close file before delete it
		nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
		         do_GetService(kAddressBookDBCID, &rv);

		if (NS_SUCCEEDED(rv) && addrDBFactory)
			rv = addrDBFactory->Open(dbPath, PR_FALSE, getter_AddRefs(database), PR_TRUE);

    delete dbPath;
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
        NS_ENSURE_SUCCESS(rv, rv);
				for(PRUint32 i = 0; i < cardCount; i++)
				{
					nsCOMPtr<nsISupports> cardSupports = getter_AddRefs(cardArray->ElementAt(i));
					nsCOMPtr<nsIAbCard> card = do_QueryInterface(cardSupports, &rv);
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

NS_IMETHODIMP nsAbMDBDirectory::DeleteCards(nsISupportsArray *cards)
{
	nsresult rv = NS_OK;

	if (!mDatabase)
		rv = GetAbDatabase();

	if (NS_SUCCEEDED(rv) && mDatabase)
	{
		PRUint32 cardCount;
		PRUint32 i;
		rv = cards->Count(&cardCount);
    NS_ENSURE_SUCCESS(rv, rv);
		for (i = 0; i < cardCount; i++)
		{
			nsCOMPtr<nsISupports> cardSupports;
			nsCOMPtr<nsIAbCard> card;
			nsCOMPtr<nsIAbMDBCard> dbcard;
			cardSupports = getter_AddRefs(cards->ElementAt(i));
			card = do_QueryInterface(cardSupports, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
			dbcard = do_QueryInterface(card, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
			if (card)
			{
				if (IsMailingList())
				{
					mDatabase->DeleteCardFromMailList(this, card, PR_TRUE);

					PRUint32 cardTotal;
					PRInt32 i;
					rv = m_AddressList->Count(&cardTotal);
					for (i = cardTotal - 1; i >= 0; i--)
					{						
						nsCOMPtr<nsISupports> pSupport = getter_AddRefs(m_AddressList->ElementAt(i));
						if (!pSupport)
							continue;

						nsCOMPtr<nsIAbMDBCard> dbarrayCard(do_QueryInterface(pSupport, &rv));
						if (dbarrayCard)
						{
							PRUint32 tableID, rowID, cardTableID, cardRowID; 
							dbarrayCard->GetDbTableID(&tableID);
							dbarrayCard->GetDbRowID(&rowID);
							dbcard->GetDbTableID(&cardTableID);
							dbcard->GetDbRowID(&cardRowID);
							if (tableID == cardTableID && rowID == cardRowID)
								m_AddressList->RemoveElementAt(i);
						}
					}
				}
				else
				{
					mDatabase->DeleteCard(card, PR_TRUE);
					PRBool bIsMailList = PR_FALSE;
					card->GetIsMailList(&bIsMailList);
					if (bIsMailList)
					{
						//to do, get mailing list dir side uri and notify rdf to remove it
						PRUint32 rowID;
						dbcard->GetDbRowID(&rowID);
						char *listUri = PR_smprintf("%s/MailList%ld", mURI, rowID);
						if (listUri)
						{
							nsresult rv = NS_OK;
							nsCOMPtr<nsIRDFService> rdfService = 
							         do_GetService(kRDFServiceCID, &rv);

							if(NS_SUCCEEDED(rv))
								{
								nsCOMPtr<nsIRDFResource> listResource;
								rv = rdfService->GetResource(listUri, getter_AddRefs(listResource));
								nsCOMPtr<nsIAbDirectory> listDir = do_QueryInterface(listResource, &rv);
								if(NS_SUCCEEDED(rv))
									{
									if (m_AddressList)
										m_AddressList->RemoveElement(listDir);
									rv = mSubDirectories->RemoveElement(listDir);

									if (listDir)
										NotifyItemDeleted(listDir);
									PR_smprintf_free(listUri);
									}
								else 
									{
									PR_smprintf_free(listUri);
									return rv;
									}
								}
							else
								{
								PR_smprintf_free(listUri);
								return rv;
								}
						}
					}
					else
					{ 
						RemoveCardFromAddressList(card);
					}
				}
			}
		}
		mDatabase->Commit(kLargeCommit);
	}
	return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::HasCard(nsIAbCard *cards, PRBool *hasCard)
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

NS_IMETHODIMP nsAbMDBDirectory::HasDirectory(nsIAbDirectory *dir, PRBool *hasDir)
{
	if (!hasDir)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_ERROR_FAILURE;

	nsCOMPtr<nsIAbMDBDirectory> dbdir(do_QueryInterface(dir, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
	
	PRBool bIsMailingList  = PR_FALSE;
	dir->GetIsMailList(&bIsMailingList);
	if (bIsMailingList)
	{
		nsXPIDLCString uri;
		rv = dbdir->GetDirUri(getter_Copies(uri));
    NS_ENSURE_SUCCESS(rv, rv);
		nsCOMPtr<nsIAddrDatabase> database;
		nsCOMPtr<nsIAddressBook> addresBook(do_GetService(kAddrBookCID, &rv)); 
		if (NS_SUCCEEDED(rv))
		{
			rv = addresBook->GetAbDatabaseFromURI((const char *)uri, getter_AddRefs(database));				
		}
		if(NS_SUCCEEDED(rv) && database)
		{
			if(NS_SUCCEEDED(rv))
				rv = database->ContainsMailList(dir, hasDir);
		}
	}

	return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::CreateNewDirectory(PRUint32 prefCount, const char **prefName, const PRUnichar **prefValue)
{
	if (!*prefName || !*prefValue)
		return NS_ERROR_NULL_POINTER;

	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbMDBDirectory::CreateDirectoryByURI(const PRUnichar *dirName, const char *uri, PRBool migrating)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAbMDBDirectory::AddMailList(nsIAbDirectory *list)
{
	nsresult rv = NS_OK;
	if (!mDatabase)
		rv = GetAbDatabase();

	if (NS_FAILED(rv) || !mDatabase)
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIAbMDBDirectory> dblist(do_QueryInterface(list, &rv));
	if (NS_FAILED(rv))
	{
		nsAbMDBDirProperty* dblistproperty = new nsAbMDBDirProperty ();
		NS_ADDREF(dblistproperty);
		nsCOMPtr<nsIAbDirectory> newlist = getter_AddRefs(NS_STATIC_CAST(nsIAbDirectory*, dblistproperty));
		newlist->CopyMailList(list);
		list = newlist;
		dblist = do_QueryInterface(list);
		rv = NS_OK;
	}

	mDatabase->CreateMailListAndAddToDB(list, PR_TRUE);
	mDatabase->Commit(kLargeCommit);

	PRUint32 dbRowID;
	dblist->GetDbRowID(&dbRowID);
	char *listUri = PR_smprintf("%s/MailList%ld", mURI, dbRowID);

	nsCOMPtr<nsIAbDirectory> newList;
	rv = AddDirectory(listUri, getter_AddRefs(newList));
	nsCOMPtr<nsIAbMDBDirectory> dbnewList(do_QueryInterface(newList));
	if (NS_SUCCEEDED(rv) && newList)
	{
		nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(newList, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
		mDatabase->AddListener(listener);

		dbnewList->CopyDBMailList (dblist);
		AddMailListToDirectory(newList);
		NotifyItemAdded(newList);
	}

	return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::AddCard(nsIAbCard* card, nsIAbCard **_retval)
{
	nsresult rv = NS_OK;
	if (!mDatabase)
		rv = GetAbDatabase();

	if (NS_FAILED(rv) || !mDatabase)
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &rv));
	if (NS_FAILED(rv) || !dbcard)
	{
		nsAbMDBCardProperty* dbcardproperty = new nsAbMDBCardProperty ();
		NS_ADDREF(dbcardproperty);
		nsCOMPtr<nsIAbCard> newcard = getter_AddRefs(NS_STATIC_CAST(nsIAbCard*, dbcardproperty));
		newcard->Copy (card);
		card = newcard;
		dbcard = do_QueryInterface(card);
		rv = NS_OK;
	}
	
	dbcard->SetAbDatabase (mDatabase);
	if (mIsMailingList == 1)
		mDatabase->CreateNewListCardAndAddToDB(m_dbRowID, card, PR_TRUE);
	else
		mDatabase->CreateNewCardAndAddToDB(card, PR_TRUE);
	mDatabase->Commit(kLargeCommit);

	// Return the RDF resource instance of the card
	// which was added to the database
	//
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	nsXPIDLCString uri;
	rv = dbcard->GetCardURI(getter_Copies (uri));

	nsCOMPtr<nsIRDFResource> resource;
	rv = rdf->GetResource(uri, getter_AddRefs(resource));
	
	nsCOMPtr<nsIAbCard> cardInstance (do_QueryInterface (resource, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	*_retval = cardInstance;
	NS_ADDREF(*_retval);

	return rv;
}

NS_IMETHODIMP nsAbMDBDirectory::DropCard(nsIAbCard* card, nsIAbCard **_retval)
{
	nsresult rv = NS_OK;

	if (mURI && mIsMailingList == -1)
	{
		nsAutoString file; file.AssignWithConversion(&(mURI[PL_strlen(kMDBDirectoryRoot)]));
		PRInt32 pos = file.Find("/");
		if (pos != -1)
			mIsMailingList = 1;
		else
			mIsMailingList = 0;
	}
	if (!mDatabase)
		rv = GetAbDatabase();

	if (NS_FAILED(rv) || !mDatabase)
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &rv));
	if (NS_FAILED(rv))
	{
		nsAbMDBCardProperty* dbcardproperty = new nsAbMDBCardProperty ();
		NS_ADDREF(dbcardproperty);
		nsCOMPtr<nsIAbCard> newcard = getter_AddRefs (NS_STATIC_CAST(nsIAbCard*, dbcardproperty));
		newcard->Copy (card);
		card = newcard;
		dbcard = do_QueryInterface (card);
		rv = NS_OK;
	}

	dbcard->SetAbDatabase (mDatabase);
	if (mIsMailingList == 1)
		mDatabase->CreateNewListCardAndAddToDB(m_dbRowID, card, PR_TRUE);
	else
		mDatabase->CreateNewCardAndAddToDB(card, PR_TRUE);
	mDatabase->Commit(kLargeCommit);


	// Return the RDF resource instance of the card
	// which was added to the database
	//
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	nsXPIDLCString uri;
	rv = dbcard->GetCardURI(getter_Copies (uri));

	nsCOMPtr<nsIRDFResource> resource;
	rv = rdf->GetResource(uri, getter_AddRefs(resource));
	
	nsCOMPtr<nsIAbCard> cardInstance (do_QueryInterface (resource, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	*_retval = cardInstance;
	NS_ADDREF(*_retval);

	return NS_OK;
}







// nsIAddrDBListener methods

NS_IMETHODIMP nsAbMDBDirectory::OnCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator)
{
  return NS_OK;
}

NS_IMETHODIMP nsAbMDBDirectory::OnCardEntryChange
(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator)
{
	nsresult rv = NS_OK;
	if (abCode == AB_NotifyInserted && card)
	{ 
		nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

		nsCOMPtr<nsIAbMDBCard> dbcard(do_QueryInterface(card, &rv));
    NS_ENSURE_SUCCESS(rv, rv);
		if (!dbcard)
			return NS_ERROR_FAILURE;

    nsXPIDLCString cardURI;
		rv = dbcard->GetCardURI(getter_Copies(cardURI));
		if (!cardURI)
			return NS_ERROR_NULL_POINTER;
		
		nsCOMPtr<nsIRDFResource> res;
		rv = rdf->GetResource(cardURI, getter_AddRefs(res));
		if (NS_SUCCEEDED(rv))
		{
			nsCOMPtr<nsIAbMDBCard> dbpersonCard = do_QueryInterface(res);
			if (dbpersonCard)
			{
				dbpersonCard->CopyCard(dbcard);
				if (mDatabase)
				{
					dbpersonCard->SetAbDatabase(mDatabase);
					nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(dbpersonCard, &rv));
					NS_ENSURE_SUCCESS(rv, rv);
					mDatabase->AddListener(listener);
				}
				nsCOMPtr<nsISupports> cardSupports(do_QueryInterface(dbpersonCard));
				if (cardSupports)
				{
					NotifyItemAdded(cardSupports);
				}
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

NS_IMETHODIMP nsAbMDBDirectory::OnListEntryChange
(PRUint32 abCode, nsIAbDirectory *list, nsIAddrDBListener *instigator)
{
	nsresult rv = NS_OK;
	
	if (abCode == AB_NotifyPropertyChanged && list)
	{
		PRBool bIsMailList = PR_FALSE;
		list->GetIsMailList(&bIsMailList);
		
		PRUint32 rowID;
		nsCOMPtr<nsIAbMDBDirectory> dblist(do_QueryInterface(list, &rv));
		if(NS_SUCCEEDED(rv))
		{
			dblist->GetDbRowID(&rowID);

			if (bIsMailList && m_dbRowID == rowID)
			{
				nsXPIDLString pListName;
				list->GetListName(getter_Copies(pListName));
				if (pListName)
					NotifyPropertyChanged("DirName", nsnull, 
									  NS_CONST_CAST(PRUnichar*, (const PRUnichar*)pListName));
			}
		}

	}
	return rv;
}

