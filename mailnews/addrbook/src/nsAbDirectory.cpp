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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIAddressBook.h"

#include "mdb.h"
#include "prlog.h"
#include "prprf.h"
#include "prmem.h"


static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kAbCardCID, NS_ABCARD_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);

nsAbDirectory::nsAbDirectory(void)
  :  nsAbRDFResource(),
     mInitialized(PR_FALSE),
	 mIsMailingList(-1)
{
	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));
}

nsAbDirectory::~nsAbDirectory(void)
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

NS_IMPL_ISUPPORTS_INHERITED1(nsAbDirectory, nsAbRDFResource, nsIAbDirectory)

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
		nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &rv));
		NS_ENSURE_SUCCESS(rv, rv);

		char* cardURI = nsnull;
		rv = card->GetCardURI(&cardURI);
    NS_ENSURE_SUCCESS(rv, rv);
		if (!cardURI)
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
					NS_ENSURE_SUCCESS(rv, rv);
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

NS_IMETHODIMP nsAbDirectory::OnListEntryChange
(PRUint32 abCode, nsIAbDirectory *list, nsIAddrDBListener *instigator)
{
	if (abCode == AB_NotifyPropertyChanged && list)
	{
		PRBool bIsMailList = PR_FALSE;
		list->GetIsMailList(&bIsMailList);
		
		PRUint32 rowID;
		list->GetDbRowID(&rowID);

		if (bIsMailList && m_dbRowID == rowID)
		{
			nsXPIDLString pListName;
			list->GetListName(getter_Copies(pListName));
			if (pListName)
				NotifyPropertyChanged("DirName", nsnull, 
									  NS_CONST_CAST(PRUnichar*, (const PRUnichar*)pListName));
		}

	}
	return NS_OK;
}

NS_IMETHODIMP nsAbDirectory::GetChildNodes(nsIEnumerator* *result)
{
	if (!mInitialized) 
	{
		if (!PL_strcmp(mURI, kMDBDirectoryRoot) && GetDirList())
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
					nsString name; name.AssignWithConversion(server->fileName);
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
						PRUnichar *unichars = nsnull;
						PRInt32 descLength = PL_strlen(server->description);
						INTL_ConvertToUnicode((const char *)server->description, 
							descLength, (void**)&unichars);
						childDir->SetDirName(unichars);
						childDir->SetServer(server);
						PR_FREEIF(unichars);
					}
					nsresult rv = NS_OK;
					nsCOMPtr<nsIAddrDatabase>  listDatabase;  

					nsCOMPtr<nsIAddrBookSession> abSession = 
					         do_GetService(kAddrBookSessionCID, &rv); 
					if (NS_SUCCEEDED(rv))
					{
						nsFileSpec* dbPath;
						abSession->GetUserProfileDirectory(&dbPath);

						nsAutoString file; file.AssignWithConversion(server->fileName);
						(*dbPath) += file;

						nsCOMPtr<nsIAddrDatabase> addrDBFactory = 
						         do_GetService(kAddressBookDBCID, &rv);

						if (NS_SUCCEEDED(rv) && addrDBFactory)
							rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(listDatabase), PR_TRUE);
						if (NS_SUCCEEDED(rv) && listDatabase)
						{
							listDatabase->GetMailingListsFromDB(childDir);
						}

              delete dbPath;
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

nsresult nsAbDirectory::AddMailList(const char *uriName)
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

NS_IMETHODIMP nsAbDirectory::GetChildCards(nsIEnumerator* *result)
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

NS_IMETHODIMP nsAbDirectory::CreateNewDirectory(const PRUnichar *dirName, const char *fileName, PRBool migrating)
{
	if (!dirName)
		return NS_ERROR_NULL_POINTER;

	DIR_Server * server = nsnull;
	nsresult rv = DIR_AddNewAddressBook(dirName, fileName, PABDirectory, &server);

	nsCOMPtr<nsIAbDirectory> newDir;
	char *uri = PR_smprintf("%s%s", kMDBDirectoryRoot, server->fileName);
	if (uri)
	{
		nsCOMPtr<nsIAddrDatabase>  database;  
		nsCOMPtr<nsIAddressBook> addresBook(do_GetService(kAddrBookCID, &rv)); 
		if (NS_SUCCEEDED(rv))
			rv = addresBook->GetAbDatabaseFromURI(uri, getter_AddRefs(database));

		rv = AddDirectory(uri, getter_AddRefs(newDir));
		PR_smprintf_free(uri);
		if (NS_SUCCEEDED(rv) && newDir)
		{
			newDir->SetDirName((PRUnichar *)dirName);
			newDir->SetServer(server);

			NotifyItemAdded(newDir);
			return rv;
		}
		else
			return NS_ERROR_NULL_POINTER;
	}
	return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbDirectory::CreateNewMailingList(const char* uri, nsIAbDirectory *list)
{
	if (!uri)
		return NS_ERROR_NULL_POINTER;
	
	nsCOMPtr<nsIAbDirectory> newList;
	nsresult rv = AddDirectory(uri, getter_AddRefs(newList));
	if (NS_SUCCEEDED(rv) && newList)
	{
		nsCOMPtr<nsIAddrDatabase>  listDatabase;  
		nsCOMPtr<nsIAddressBook> addresBook(do_GetService(kAddrBookCID, &rv)); 
		if (NS_SUCCEEDED(rv))
			rv = addresBook->GetAbDatabaseFromURI(uri, getter_AddRefs(listDatabase));
		if (listDatabase)
		{
			nsCOMPtr<nsIAddrDBListener> listener(do_QueryInterface(newList, &rv));
			NS_ENSURE_SUCCESS(rv, rv);
			listDatabase->AddListener(listener);
		}
		newList->CopyMailList(list);
		AddMailListToDirectory(newList);
		NotifyItemAdded(newList);
		return rv;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbDirectory::AddChildCards(const char *uriName, nsIAbCard **childCard)
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
		{
			return NS_ERROR_NULL_POINTER;
		}
	}

	*childCard = personCard;
	NS_ADDREF(*childCard);

	return rv;
}

NS_IMETHODIMP nsAbDirectory::RemoveElementsFromAddressList()
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
	m_AddressList = nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirectory::RemoveEmailAddressAt(PRUint32 aIndex)
{
	if (m_AddressList)
	{
		return m_AddressList->RemoveElementAt(aIndex);
	}
	else
		return NS_ERROR_FAILURE;
}

nsresult nsAbDirectory::RemoveCardFromAddressList(const nsIAbCard* card)
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
          pSupport = getter_AddRefs(pAddressLists->ElementAt(j));
					nsCOMPtr<nsIAbCard> cardInList(do_QueryInterface(pSupport, &rv));
					if (card == cardInList.get())
						pAddressLists->RemoveElementAt(j);
				}
			}
		}
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbDirectory::DeleteCards(nsISupportsArray *cards)
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
			cardSupports = getter_AddRefs(cards->ElementAt(i));
			card = do_QueryInterface(cardSupports, &rv);
			if (card)
			{
				if (IsMailingList())
				{
					mDatabase->DeleteCardFromMailList(this, card, PR_TRUE);

					PRUint32 cardTotal;
					rv = m_AddressList->Count(&cardTotal);
					for (PRInt32 k = cardTotal - 1; k >= 0; k--)
					{						
						nsCOMPtr<nsISupports> pSupport = getter_AddRefs(m_AddressList->ElementAt(k));
						if (!pSupport)
							continue;

						nsCOMPtr<nsIAbCard> arrayCard(do_QueryInterface(pSupport, &rv));
						if (arrayCard)
						{
							PRUint32 tableID, rowID, cardTableID, cardRowID; 
							arrayCard->GetDbTableID(&tableID);
							arrayCard->GetDbRowID(&rowID);
							card->GetDbTableID(&cardTableID);
							card->GetDbRowID(&cardRowID);
							if (tableID == cardTableID && rowID == cardRowID)
								m_AddressList->RemoveElementAt(k);
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
						card->GetDbRowID(&rowID);
						char *listUri = PR_smprintf("%s/MailList%ld", mURI, rowID);
						if (listUri)
						{
							rv = NS_OK;
							nsCOMPtr<nsIRDFService> rdfService = 
							         do_GetService(kRDFServiceCID, &rv);
							NS_ENSURE_SUCCESS(rv, rv);
							nsCOMPtr<nsIRDFResource> listResource;
							rv = rdfService->GetResource(listUri, getter_AddRefs(listResource));
							nsCOMPtr<nsIAbDirectory> listDir = do_QueryInterface(listResource);
							if (m_AddressList)
								m_AddressList->RemoveElement(listDir);
							rv = mSubDirectories->RemoveElement(listDir);
							if (listDir)
								NotifyItemDeleted(listDir);
							PR_smprintf_free(listUri);
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

nsresult nsAbDirectory::DeleteDirectoryCards(nsIAbDirectory* directory, DIR_Server *server)
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

NS_IMETHODIMP nsAbDirectory::DeleteDirectory(nsIAbDirectory *directory)
{
	nsresult rv = NS_OK;
	
	if (!directory)
		return NS_ERROR_FAILURE;

	DIR_Server *server = nsnull;
	rv = directory->GetServer(&server);
	if (server)
	{	//it's an address book		
		nsISupportsArray* pAddressLists;
		directory->GetAddressLists(&pAddressLists);
		if (pAddressLists)
		{	//remove mailing list node
			PRUint32 total;
			rv = pAddressLists->Count(&total);
			if (total)
			{
				PRInt32 i;
				for (i = total - 1; i >= 0; i--)
				{
					nsCOMPtr<nsISupports> pSupport = getter_AddRefs(pAddressLists->ElementAt(i));
					if (pSupport)
					{
						nsCOMPtr<nsIAbDirectory> listDir(do_QueryInterface(pSupport, &rv));
						if (listDir)
						{
							directory->DeleteDirectory(listDir);
							listDir->RemoveElementsFromAddressList();
						}
					}
					pAddressLists->RemoveElement(pSupport);
				}
			}
		}
		DIR_DeleteServerFromList(server);
		directory->ClearDatabase();

		rv = mSubDirectories->RemoveElement(directory);
		NotifyItemDeleted(directory);
	}
	else
	{	//it's a mailing list
		rv = NS_OK;

		nsXPIDLCString uri;
		rv = directory->GetDirUri(getter_Copies(uri));
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

	nsresult rv = NS_ERROR_FAILURE;
	PRBool bIsMailingList  = PR_FALSE;
	dir->GetIsMailList(&bIsMailingList);
	if (bIsMailingList)
	{
		nsXPIDLCString uri;
		rv = dir->GetDirUri(getter_Copies(uri));
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
	else
	{
		DIR_Server* dirServer = nsnull;
		dir->GetServer(&dirServer);
		rv = DIR_ContainsServer(dirServer, hasDir);
	}
	return rv;
}

nsresult nsAbDirectory::NotifyPropertyChanged(char *property, PRUnichar* oldValue, PRUnichar* newValue)
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

nsresult nsAbDirectory::NotifyItemAdded(nsISupports *item)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemAdded(this, item);
	return NS_OK;
}

nsresult nsAbDirectory::NotifyItemDeleted(nsISupports *item)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemDeleted(this, item);

	return NS_OK;
}

NS_IMETHODIMP nsAbDirectory::GetDirUri(char **uri)
{
	if (uri)
	{
		if (mURI)
			*uri =  nsCRT::strdup(mURI);
		else
			*uri =  nsCRT::strdup("");
		return NS_OK;
	}
	else
		return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbDirectory::ClearDatabase()
{ 			
	if (mDatabase)
	{
		mDatabase->RemoveListener(this);
		mDatabase = nsnull; 
	}
	return NS_OK; 
}
