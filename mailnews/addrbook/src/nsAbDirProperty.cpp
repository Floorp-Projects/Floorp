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

#include "mdb.h"
#include "prlog.h"
#include "prprf.h"
#include "prmem.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kAbCardCID, NS_ABCARD_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

nsAbDirProperty::nsAbDirProperty(void)
  : m_DirName(""), m_LastModifiedDate(0),
	m_DbPath(nsnull), m_Server(nsnull)
{
	NS_INIT_REFCNT();
}

nsAbDirProperty::~nsAbDirProperty(void)
{
	PR_FREEIF(m_DbPath);
	// m_Server will free with the list
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
nsAbDirProperty::GetMailingList(nsIEnumerator **mailingList)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::CreateNewDirectory(const PRUnichar *dirName, const char *fileName, PRBool migrating)
{ return NS_OK; }

NS_IMETHODIMP
nsAbDirProperty::GetDirUri(char **uri)
{ return NS_OK; }

