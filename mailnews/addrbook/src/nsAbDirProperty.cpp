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

static NS_DEFINE_CID(kAbCardCID, NS_ABCARDRESOURCE_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRESSBOOKDB_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);

nsAbDirProperty::nsAbDirProperty(void)
  : m_DirName(nsnull), m_LastModifiedDate(nsnull),
	m_DbPath(nsnull), m_Server(nsnull)
{
	NS_INIT_REFCNT();
}

nsAbDirProperty::~nsAbDirProperty(void)
{
	PR_FREEIF(m_DirName);
	PR_FREEIF(m_LastModifiedDate);
	PR_FREEIF(m_DbPath);
	// m_Server will free with the list
}

NS_IMPL_ADDREF(nsAbDirProperty)
NS_IMPL_RELEASE(nsAbDirProperty)

NS_IMETHODIMP nsAbDirProperty::QueryInterface(REFNSIID aIID, void** aResult)
{   
    if (aResult == NULL)  
        return NS_ERROR_NULL_POINTER;  

    if (aIID.Equals(nsCOMTypeInfo<nsIAbDirectory>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
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

NS_IMETHODIMP nsAbDirProperty::GetDirName(char **name)
{
	if (name)
	{
		if (m_DirName)
			*name = PL_strdup(m_DirName);
		else
			*name = PL_strdup("");
		return NS_OK;
	}
	else
		return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbDirProperty::SetDirName(char * name)
{
	if (name)
	{
		PR_FREEIF(m_DirName);
		m_DirName = PL_strdup(name);
	}
	return NS_OK;
}

NS_IMETHODIMP nsAbDirProperty::GetLastModifiedDate(char * *aLastModifiedDate)
{
	if (aLastModifiedDate)
	{
		if (m_DirName)
			*aLastModifiedDate = PL_strdup(m_LastModifiedDate);
		else
			*aLastModifiedDate = PL_strdup("");
		return NS_OK;
	}
	else
		return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP nsAbDirProperty::SetLastModifiedDate(char * aLastModifiedDate)
{
	if (aLastModifiedDate)
	{
		PR_FREEIF(m_LastModifiedDate);
		m_LastModifiedDate = PL_strdup(aLastModifiedDate);
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
