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

#include "nsAbDirProperty.h"	 
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

/* The definition is nsAddressBook.cpp */
extern const char *kDirectoryDataSourceRoot;


static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kAbCardCID, NS_ABCARD_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);

nsAbDirProperty::nsAbDirProperty(void)
  : m_DirName(""), m_LastModifiedDate(0),
	m_DbPath(nsnull), m_Server(nsnull)
{
	NS_INIT_REFCNT();

	m_ListName = "";
	m_ListNickName = "";
	m_Description = "";
	m_bIsMailList = PR_FALSE;
}

nsAbDirProperty::~nsAbDirProperty(void)
{
	PR_FREEIF(m_DbPath);
}

NS_IMPL_ADDREF(nsAbDirProperty)
NS_IMPL_RELEASE(nsAbDirProperty)

NS_IMETHODIMP nsAbDirProperty::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(NS_GET_IID(nsIAbDirectory)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsIAbDirectory*, this);   
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}   

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAbDirProperty::GetDirFilePath(char **dbPath)
{
	if (m_Server && m_Server->fileName)
	{
		nsresult rv = NS_OK;
		nsFileSpec* dbFile = nsnull;

		NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
		if(NS_SUCCEEDED(rv))
			abSession->GetUserProfileDirectory(&dbFile);
		
		(*dbFile) += m_Server->fileName;
		char* file = PL_strdup(dbFile->GetCString());
		*dbPath = file;
		
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsAbDirProperty::GetDirName(PRUnichar **aDirName)
{
	if (aDirName)
	{
		*aDirName = m_DirName.ToNewUnicode();
		if (!(*aDirName)) 
			return NS_ERROR_OUT_OF_MEMORY;
		else
			return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbDirProperty::SetDirName(const PRUnichar * aDirName)
{
	if (aDirName)
		m_DirName = aDirName;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::GetLastModifiedDate(PRUint32 *aLastModifiedDate)
{
	if (aLastModifiedDate)
	{
		*aLastModifiedDate = m_LastModifiedDate;
		return NS_OK;
	}
	else
		return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbDirProperty::SetLastModifiedDate(PRUint32 aLastModifiedDate)
{
	if (aLastModifiedDate)
	{
		m_LastModifiedDate = aLastModifiedDate;
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::GetServer(DIR_Server * *aServer)
{
	if (aServer)
	{
		*aServer = m_Server;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsAbDirProperty::SetServer(DIR_Server * aServer)
{
	m_Server = aServer;
	return NS_OK;
}

NS_IMETHODIMP
nsAbDirProperty::GetChildNodes(nsIEnumerator **childList)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::GetChildCards(nsIEnumerator **childCards)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::AddChildCards(const char *uriName, nsIAbCard **childCard)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::AddDirectory(const char *uriName, nsIAbDirectory **childDir)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::DeleteDirectories(nsISupportsArray *dierctories)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::DeleteCards(nsISupportsArray *cards)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::HasCard(nsIAbCard *cards, PRBool *hasCard)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::HasDirectory(nsIAbDirectory *dir, PRBool *hasDir)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::CreateNewDirectory(const PRUnichar *dirName, const char *fileName, PRBool migrating)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::CreateNewMailingList(const char* uri, nsIAbDirectory *list)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::GetDirUri(char **uri)
{ return NS_OK; }

nsresult nsAbDirProperty::GetAttributeName(PRUnichar **aName, nsString& value)
{
	if (aName)
	{
		*aName = value.ToNewUnicode();
		if (!(*aName)) 
			return NS_ERROR_OUT_OF_MEMORY;
		else
			return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;

}

nsresult nsAbDirProperty::SetAttributeName(const PRUnichar *aName, nsString& arrtibute)
{
	if (aName)
		arrtibute = aName;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::GetListName(PRUnichar * *aListName)
{ return GetAttributeName(aListName, m_ListName); }

NS_IMETHODIMP nsAbDirProperty::SetListName(const PRUnichar * aListName)
{ return SetAttributeName(aListName, m_ListName); }

NS_IMETHODIMP nsAbDirProperty::GetListNickName(PRUnichar * *aListNickName)
{ return GetAttributeName(aListNickName, m_ListNickName); }

NS_IMETHODIMP nsAbDirProperty::SetListNickName(const PRUnichar * aListNickName)
{ return SetAttributeName(aListNickName, m_ListNickName); }

NS_IMETHODIMP nsAbDirProperty::GetDescription(PRUnichar * *aDescription)
{ return GetAttributeName(aDescription, m_Description); }

NS_IMETHODIMP nsAbDirProperty::SetDescription(const PRUnichar * aDescription)
{ return SetAttributeName(aDescription, m_Description); }

NS_IMETHODIMP nsAbDirProperty::GetDbRowID(PRUint32 *aDbRowID)
{
	*aDbRowID = m_dbRowID;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::SetDbRowID(PRUint32 aDbRowID)
{
	m_dbRowID = aDbRowID;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::GetIsMailList(PRBool *aIsMailList)
{
	*aIsMailList = m_bIsMailList;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::SetIsMailList(PRBool aIsMailList)
{
	m_bIsMailList = aIsMailList;
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::GetAddressLists(nsISupportsArray * *aAddressLists)
{
	if (!m_AddressList)
		NS_NewISupportsArray(getter_AddRefs(m_AddressList));

	*aAddressLists = m_AddressList;
	NS_ADDREF(*aAddressLists);
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::SetAddressLists(nsISupportsArray * aAddressLists)
{
	m_AddressList = aAddressLists;
	return NS_OK;
}

/* add mailing list to the parent directory */
NS_IMETHODIMP nsAbDirProperty::AddMailListToDirectory(nsIAbDirectory *mailList)
{
	if (!m_AddressList)
		NS_NewISupportsArray(getter_AddRefs(m_AddressList));
	m_AddressList->AppendElement(mailList);
	return NS_OK;
}

/* add addresses to the mailing list */
NS_IMETHODIMP nsAbDirProperty::AddAddressToList(nsIAbCard *card)
{
	if (!m_AddressList)
		NS_NewISupportsArray(getter_AddRefs(m_AddressList));
	m_AddressList->AppendElement(card);
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::AddMailListToDatabase(const char *uri)
{
	nsresult rv = NS_OK;

	nsCOMPtr<nsIAddrDatabase>  listDatabase;  

	NS_WITH_SERVICE(nsIAddressBook, addresBook, kAddrBookCID, &rv); 
	if (NS_SUCCEEDED(rv))
		rv = addresBook->GetAbDatabaseFromURI(uri, getter_AddRefs(listDatabase));

	if (listDatabase)
	{
		listDatabase->CreateMailListAndAddToDB(this, PR_TRUE);
		listDatabase->Commit(kLargeCommit);
		listDatabase = null_nsCOMPtr();

		NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
		if(NS_FAILED(rv))
			return rv;
		nsCOMPtr<nsIRDFResource> parentResource;
		char *parentUri = PR_smprintf("%s", kDirectoryDataSourceRoot);
		rv = rdfService->GetResource(parentUri, getter_AddRefs(parentResource));
		nsCOMPtr<nsIAbDirectory> parentDir = do_QueryInterface(parentResource);
		if (!parentDir)
			return NS_ERROR_NULL_POINTER;
		if (parentUri)
			PR_smprintf_free(parentUri);

		char *listUri = PR_smprintf("%s/MailList%ld", uri, m_dbRowID);
		if (listUri)
		{
			parentDir->CreateNewMailingList(listUri, this);
			PR_smprintf_free(listUri);
			return NS_OK;
		}
		else
			return NS_ERROR_FAILURE;
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbDirProperty::EditMailListToDatabase(const char *uri)
{
	nsresult rv = NS_OK;

	nsCOMPtr<nsIAddrDatabase>  listDatabase;  

	NS_WITH_SERVICE(nsIAddressBook, addresBook, kAddrBookCID, &rv); 
	if (NS_SUCCEEDED(rv))
		rv = addresBook->GetAbDatabaseFromURI(uri, getter_AddRefs(listDatabase));

	if (listDatabase)
	{
		listDatabase->EditMailList(this, PR_TRUE);
		listDatabase->Commit(kLargeCommit);
		listDatabase = null_nsCOMPtr();

		//notify RDF property change
		return NS_OK;

	}
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsAbDirProperty::CopyMailList(nsIAbDirectory* srcList)
{
	PRUnichar *str = nsnull;
	srcList->GetListName(&str);
	SetListName(str);
	PR_FREEIF(str);
	srcList->GetListNickName(&str);
	SetListNickName(str);
	PR_FREEIF(str);
	srcList->GetDescription(&str);
	SetDescription(str);
	PR_FREEIF(str);

	SetIsMailList(PR_TRUE);

	nsISupportsArray* pAddressLists;
	srcList->GetAddressLists(&pAddressLists);
	NS_IF_ADDREF(pAddressLists);
	SetAddressLists(pAddressLists);

	PRUint32 rowID;
	srcList->GetDbRowID(&rowID);
	SetDbRowID(rowID);

	return NS_OK;
}
