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
 * Contributor(s): Paul Sandoz
 */

#include "nsAbBSDirectory.h"

#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"

#include "nsDirPrefs.h"
#include "nsAbBaseCID.h"
#include "nsMsgBaseCID.h"
#include "nsIAddressBook.h"
#include "nsAddrDatabase.h"
#include "nsIAddrBookSession.h"
#include "nsIAbMDBDirectory.h"
#include "nsHashtable.h"
#include "nsIAbUpgrader.h"
#include "nsIMessengerMigrator.h"

#include "prmem.h"
#include "prprf.h"

extern const char *kDirectoryDataSourceRoot;

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kMessengerMigratorCID, NS_MESSENGERMIGRATOR_CID);

nsAbBSDirectory::nsAbBSDirectory()
	: nsRDFResource(),
	  mInitialized(PR_FALSE),
	  mServers (13)
{
	NS_NewISupportsArray(getter_AddRefs(mSubDirectories));
}

nsAbBSDirectory::~nsAbBSDirectory()
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

NS_IMPL_ISUPPORTS_INHERITED(nsAbBSDirectory, nsRDFResource, nsIAbDirectory)


nsresult nsAbBSDirectory::AddDirectory(const char *uriName, nsIAbDirectory **childDir)
{
	if (!childDir || !uriName)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
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

nsresult nsAbBSDirectory::NotifyItemAdded(nsISupports *item)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemAdded(this, item);
	return NS_OK;
}

nsresult nsAbBSDirectory::NotifyItemDeleted(nsISupports *item)
{
	nsresult rv = NS_OK;
	NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemDeleted(this, item);

	return NS_OK;
}



NS_IMETHODIMP nsAbBSDirectory::GetChildNodes(nsIEnumerator* *result)
{
	if (!mInitialized) 
	{/*
    nsresult rv;
	  nsCOMPtr <nsIAbUpgrader> abUpgrader = do_GetService(NS_AB4xUPGRADER_CONTRACTID, &rv);
  	if (NS_SUCCEEDED(rv) && abUpgrader) 
    {
	    nsCOMPtr <nsIMessengerMigrator> migrator = do_GetService(kMessengerMigratorCID, &rv);
      if (NS_SUCCEEDED(rv) || migrator)
 	      migrator->UpgradePrefs();
    }*/

		if (!PL_strcmp(mURI, "abdirectory://") && GetDirList())
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
					nsAutoString name; name.AssignWithConversion(server->fileName);
					PRInt32 pos = name.Find("na2");
					if (pos >= 0) /* check: this is a 4.x file, remove when conversion is done */
						continue;

					char* uriStr = nsnull;
					uriStr = PR_smprintf("%s%s", kDirectoryDataSourceRoot, server->fileName);
					if (uriStr == nsnull) 
						return NS_ERROR_OUT_OF_MEMORY;

					nsCOMPtr<nsIAbDirectory> childDir;
					AddDirectory(uriStr, getter_AddRefs(childDir));
					nsCOMPtr<nsIAbMDBDirectory> dbchildDir(do_QueryInterface(childDir));

					if (uriStr)
						PR_smprintf_free(uriStr);
					if (childDir)
					{
						PRUnichar *unichars = nsnull;
						PRInt32 descLength = PL_strlen(server->description);
						INTL_ConvertToUnicode((const char *)server->description, 
							descLength, (void**)&unichars);
						childDir->SetDirName(unichars);
						PR_FREEIF(unichars);

						nsVoidKey key((void *)childDir);
						mServers.Put (&key, (void *)server);

					}
					nsresult rv = NS_OK;
					nsCOMPtr<nsIAddrDatabase>  listDatabase;  

					NS_WITH_SERVICE(nsIAddrBookSession, abSession, kAddrBookSessionCID, &rv); 
					if (NS_SUCCEEDED(rv))
					{
						nsFileSpec* dbPath;
						abSession->GetUserProfileDirectory(&dbPath);

						nsAutoString file; file.AssignWithConversion(server->fileName);
						(*dbPath) += file;

						NS_WITH_SERVICE(nsIAddrDatabase, addrDBFactory, kAddressBookDBCID, &rv);

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

NS_IMETHODIMP nsAbBSDirectory::CreateNewDirectory(PRUint32 prefCount, const char **prefName, const PRUnichar **prefValue)
{
	if (!*prefValue || !*prefName)
		return NS_ERROR_NULL_POINTER;
	
	PRUnichar *unicharDescription = nsnull;
	char *charFileName = nsnull;
	PRBool migrating = PR_FALSE;
	char *charMigrate = nsnull;
	nsresult rv;

	for (PRUint32 jk=0; jk < prefCount; jk++)
	{
		switch (tolower(prefName[jk][0]))
		{
		case 'd':
			if (!nsCRT::strcasecmp(prefName[jk], "description"))
			{
				unicharDescription = (PRUnichar *)prefValue[jk];
			}
			break;
		case 'f':
			if (!nsCRT::strcasecmp(prefName[jk], "fileName"))
			{
				nsString descString(prefValue[jk]);
				PRInt32 unicharLength = descString.Length();
				INTL_ConvertFromUnicode(prefValue[jk], unicharLength, &charFileName);
			}
			break;
		case 'm':
			if (!nsCRT::strcasecmp(prefName[jk], "migrating"))
			{
				nsString descString(prefValue[jk]);
				PRInt32 unicharLength = descString.Length();
				INTL_ConvertFromUnicode(prefValue[jk], unicharLength, &charMigrate);
				if (!nsCRT::strcasecmp(charMigrate, "true"))
				{
					migrating = PR_TRUE;
				}
			}
			break;			
		}
	}

	if (unicharDescription == nsnull)
	{
		rv = NS_ERROR_NULL_POINTER;
	}
	else
	{
    rv = CreateDirectoryPAB(unicharDescription, charFileName, migrating);
	}

	if (charFileName)
		nsMemory::Free(charFileName);
	if (charMigrate)
		nsMemory::Free(charMigrate);

	return rv;
}

nsresult nsAbBSDirectory::CreateDirectoryPAB(const PRUnichar *displayName, const char *fileName,  PRBool migrating)
{
  if (!displayName )
  {
    return NS_ERROR_NULL_POINTER;
  }

  DIR_Server * server = nsnull;
  DirectoryType dirType = PABDirectory;

  DIR_AddNewAddressBook(displayName, fileName, migrating, dirType, &server);

  char *uri = PR_smprintf("%s%s",kDirectoryDataSourceRoot, server->fileName);

  nsCOMPtr<nsIAbDirectory> newDir;
  nsresult rv = AddDirectory(uri, getter_AddRefs(newDir));

  if (uri)
    PR_smprintf_free(uri);
  if (NS_SUCCEEDED(rv) && newDir)
  {
    newDir->SetDirName((PRUnichar *)displayName);
    nsVoidKey key((void *)newDir);
    mServers.Put (&key, (void *)server);
    NotifyItemAdded(newDir);
    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbBSDirectory::CreateDirectoryByURI(const PRUnichar *displayName, const char *uri, PRBool migrating)
{
	if (!displayName || !uri)
		return NS_ERROR_NULL_POINTER;

	DIR_Server * server = nsnull;
	DirectoryType dirType = PABDirectory;
  const char* fileName = nsnull;
  if (PL_strstr(uri, kDirectoryDataSourceRoot)) // for abmdbdirectory://
  {
    fileName = &(uri[PL_strlen(kDirectoryDataSourceRoot)]);
    dirType = PABDirectory;
  }
  DIR_AddNewAddressBook(displayName, fileName, migrating, dirType, &server);

	nsCOMPtr<nsIAbDirectory> newDir;
	nsresult rv = AddDirectory(uri, getter_AddRefs(newDir));

	if (NS_SUCCEEDED(rv) && newDir)
	{
		newDir->SetDirName((PRUnichar *)displayName);
		nsVoidKey key((void *)newDir);
		mServers.Put (&key, (void *)server);
		NotifyItemAdded(newDir);
		return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsAbBSDirectory::DeleteDirectory(nsIAbDirectory *directory)
{
	nsresult rv = NS_OK;
	
	if (!directory)
		return NS_ERROR_FAILURE;

	nsCOMPtr<nsIAbMDBDirectory> dbdirectory(do_QueryInterface(directory, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

	DIR_Server *server = nsnull;
	nsVoidKey key((void *)directory);
	server = (DIR_Server* )mServers.Get (&key);

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
						nsCOMPtr<nsIAbMDBDirectory> dblistDir(do_QueryInterface(pSupport, &rv));
						if (listDir && dblistDir)
						{
							directory->DeleteDirectory(listDir);
							dblistDir->RemoveElementsFromAddressList();
						}
					}
					pAddressLists->RemoveElement(pSupport);
				}
			}
		}
		DIR_DeleteServerFromList(server);
		mServers.Remove(&key);
		dbdirectory->ClearDatabase();

		rv = mSubDirectories->RemoveElement(directory);
		NotifyItemDeleted(directory);
	}
	return rv;
}

NS_IMETHODIMP nsAbBSDirectory::HasDirectory(nsIAbDirectory *dir, PRBool *hasDir)
{
	if (!hasDir)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_ERROR_FAILURE;

	DIR_Server* dirServer = nsnull;
	nsVoidKey key((void *)dir);
	dirServer = (DIR_Server* )mServers.Get (&key);
	rv = DIR_ContainsServer(dirServer, hasDir);

	return rv;
}

