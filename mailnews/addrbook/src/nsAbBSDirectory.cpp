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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s): Paul Sandoz   <paul.sandoz@sun.com> 
 *		   Csaba Borbola <csaba.borbola@sun.com>
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

#include "nsAbBSDirectory.h"
#include "nsAbUtils.h"

#include "nsRDFCID.h"
#include "nsIRDFService.h"

#include "nsDirPrefs.h"
#include "nsAbBaseCID.h"
#include "nsMsgBaseCID.h"
#include "nsIAddressBook.h"
#include "nsAddrDatabase.h"
#include "nsIAddrBookSession.h"
#include "nsIAbMDBDirectory.h"
#include "nsIAbUpgrader.h"
#include "nsIMessengerMigrator.h"
#include "nsAbDirFactoryService.h"
#include "nsAbMDBDirFactory.h"

#include "prmem.h"
#include "prprf.h"



static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kAddrBookCID, NS_ADDRESSBOOK_CID);
static NS_DEFINE_CID(kAddressBookDBCID, NS_ADDRDATABASE_CID);
static NS_DEFINE_CID(kAddrBookSessionCID, NS_ADDRBOOKSESSION_CID);
static NS_DEFINE_CID(kMessengerMigratorCID, NS_MESSENGERMIGRATOR_CID);

const char* kDescriptionPropertyName	= "description";
const char* kFileNamePropertyName	= "filename";
const char* kURIPropertyName		= "uri";
const char* kMigratingPropertyName	= "migrating";

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

nsresult nsAbBSDirectory::NotifyItemAdded(nsISupports *item)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemAdded(this, item);
	return NS_OK;
}

nsresult nsAbBSDirectory::NotifyItemDeleted(nsISupports *item)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsIAddrBookSession> abSession = 
	         do_GetService(kAddrBookSessionCID, &rv); 
	if(NS_SUCCEEDED(rv))
		abSession->NotifyDirectoryItemDeleted(this, item);

	return NS_OK;
}

nsresult nsAbBSDirectory::CreateDirectoriesFromFactory(
	const char* URI,
	DIR_Server* server,
	PRUint32 propertiesSize,
	const char** propertyNameArray,
	const PRUnichar** propertyValueArray,
	PRBool notify)
{
	nsresult rv;

	// Get the directory factory service
	nsCOMPtr<nsIAbDirFactoryService> dirFactoryService = 
			do_GetService(NS_ABDIRFACTORYSERVICE_CONTRACTID,&rv);
	NS_ENSURE_SUCCESS (rv, rv);
		
	// Get the directory factory from the URI
	nsCOMPtr<nsIAbDirFactory> dirFactory;
	rv = dirFactoryService->GetDirFactory (URI, getter_AddRefs(dirFactory));
	NS_ENSURE_SUCCESS (rv, rv);

	// Create the directories
	nsCOMPtr<nsISimpleEnumerator> newDirEnumerator;
	rv = dirFactory->CreateDirectory(propertiesSize,
		propertyNameArray,
		propertyValueArray,
		getter_AddRefs(newDirEnumerator));
	NS_ENSURE_SUCCESS (rv, rv);

	// Enumerate through the directories adding them
	// to the sub directories array
	PRBool hasMore;
	while (NS_SUCCEEDED(newDirEnumerator->HasMoreElements(&hasMore)) && hasMore)
	{
		nsCOMPtr<nsISupports> newDirSupports;
		rv = newDirEnumerator->GetNext(getter_AddRefs(newDirSupports));
		if(NS_FAILED(rv))
			continue;
			
		nsCOMPtr<nsIAbDirectory> childDir = do_QueryInterface(newDirSupports, &rv); 
		if(NS_FAILED(rv))
			continue;

		// Define realtion ship between the preference
		// entry and the directory
		nsVoidKey key((void *)childDir);
		mServers.Put (&key, (void *)server);

		mSubDirectories->AppendElement(childDir);

		// Inform the listener, i.e. the RDF directory data
		// source that a new address book has been added
		if (notify == PR_TRUE)
			NotifyItemAdded(childDir);
	}

	return NS_OK;
}

NS_IMETHODIMP nsAbBSDirectory::GetChildNodes(nsIEnumerator* *result)
{
	if (!mInitialized) 
	{
		nsresult rv;
	    nsCOMPtr<nsIAbDirFactoryService> dirFactoryService = 
			do_GetService(NS_ABDIRFACTORYSERVICE_CONTRACTID,&rv);
		NS_ENSURE_SUCCESS (rv, rv);

		if (!GetDirList())
			return NS_ERROR_FAILURE;
		
		PRInt32 count = GetDirList()->Count();
		for (PRInt32 i = 0; i < count; i++)
		{
			DIR_Server *server = (DIR_Server *)GetDirList()->ElementAt(i);

			NS_ConvertUTF8toUCS2 fileName (server->fileName);
			PRInt32 pos = fileName.Find("na2");
			if (pos >= 0) // check: this is a 4.x file, remove when conversion is done
				continue;
			
			nsHashtable propertySet;

			// Set the description property
			nsCStringKey descriptionKey (kDescriptionPropertyName, -1, nsCStringKey::NEVER_OWN);
			NS_ConvertUTF8toUCS2 description (server->description);
			propertySet.Put (&descriptionKey, (void* )description.get ());
			
			// Set the file name property
			nsCStringKey fileNameKey (kFileNamePropertyName, -1, nsCStringKey::NEVER_OWN);
			propertySet.Put (&fileNameKey, (void* )fileName.get ());
			
			// Set the uri property
			nsCStringKey URIKey (kURIPropertyName, -1, nsCStringKey::NEVER_OWN);
			nsCAutoString URIUTF8 (server->uri);
			// This is in case the uri is never set
			// in the nsDirPref.cpp code.
			if (!server->uri)
			{
				URIUTF8 = kMDBDirectoryRoot;
				URIUTF8.Append (server->fileName);
			}

            /*
             * Check that we are not converting from a
             * a 4.x address book file e.g. pab.na2
            */
            nsCAutoString uriName (URIUTF8.get());
            pos = uriName.Find("na2");
            if (pos >= 0) 
            {
                const char* tempFileName = nsnull;
                const char* uri = URIUTF8.get ();
                if (PL_strstr(uri, kMDBDirectoryRoot)) // for moz-abmdbdirectory://
                {
                    tempFileName = &(uri[PL_strlen(kMDBDirectoryRoot)]);
                    uriName.ReplaceSubstring (tempFileName,server->fileName);
                }
            }

			NS_ConvertUTF8toUCS2 URIUCS2 (uriName.get ());
			propertySet.Put (&URIKey, (void* )URIUCS2.get ());
			
			// Convert the hastable (property set) to pointer
			// arrays, using the array guards
			CharPtrArrayGuard factoryPropertyNames (PR_FALSE);
			PRUnicharPtrArrayGuard factoryPropertyValues (PR_FALSE);
			HashtableToPropertyPtrArrays::Convert (propertySet,
				factoryPropertyNames.GetSizeAddr (),
				factoryPropertyNames.GetArrayAddr (),
				factoryPropertyValues.GetArrayAddr ());	
			*(factoryPropertyNames.GetSizeAddr ()) = factoryPropertyNames.GetSize ();

			// Create the directories
			rv = CreateDirectoriesFromFactory (uriName.get (),
				server,
				factoryPropertyNames.GetSize (),
				factoryPropertyNames.GetArray(),
				factoryPropertyValues.GetArray(),
				PR_FALSE);
		}

		mInitialized = PR_TRUE;
	}
	return mSubDirectories->Enumerate(result);
}

nsresult nsAbBSDirectory::CreateNewDirectory(nsHashtable &propertySet, const char *uri, DIR_Server* server)
{
	nsresult rv;

	// Convert the hastable (property set) to pointer
	// arrays, using the array guards
	// This is done because the original hashtable
	// has been modified (added the uri and changed
	// the file name)
	CharPtrArrayGuard factoryPropertyNames (PR_FALSE);
	PRUnicharPtrArrayGuard factoryPropertyValues (PR_FALSE);
	HashtableToPropertyPtrArrays::Convert (propertySet,
		factoryPropertyNames.GetSizeAddr (),
		factoryPropertyNames.GetArrayAddr (),
		factoryPropertyValues.GetArrayAddr ());	
	*(factoryPropertyNames.GetSizeAddr ()) = factoryPropertyNames.GetSize ();

	// Create the directories
	rv = CreateDirectoriesFromFactory (uri,
		server,
		factoryPropertyNames.GetSize (),
		factoryPropertyNames.GetArray(),
		factoryPropertyValues.GetArray());

	return rv;
}

NS_IMETHODIMP nsAbBSDirectory::CreateNewDirectory(PRUint32 propCount, const char **propName, const PRUnichar **propValue)
{
	/*
	 * TODO
	 * This procedure is still MDB specific
	 * due to the dependence on the current
	 * nsDirPref.cpp code
	 *
	 */

	if (!propValue || !propName)
		return NS_ERROR_NULL_POINTER;

	if (propCount == 0)
		return NS_ERROR_FAILURE;

	nsresult rv;

	// Create hash table from property arrays
	nsHashtable propertySet;
	rv = PropertyPtrArraysToHashtable::Convert (propertySet, propCount, propName, propValue);
	NS_ENSURE_SUCCESS (rv, rv);

	// Get description property
	nsCStringKey descriptionKey (kDescriptionPropertyName, -1, nsCStringKey::NEVER_OWN);
	const PRUnichar* description = (PRUnichar* )propertySet.Get (&descriptionKey);

	// Get file name property
	nsCStringKey fileNameKey (kFileNamePropertyName, -1, nsCStringKey::NEVER_OWN);
	const PRUnichar* fileName = (PRUnichar* )propertySet.Get (&fileNameKey);
	NS_ConvertUCS2toUTF8 fileNameUTF8(fileName);

	// Get migrating property
	nsCStringKey migratingKey (kMigratingPropertyName, -1, nsCStringKey::NEVER_OWN);
	const PRUnichar* migrating = (PRUnichar* )propertySet.Get (&migratingKey);
	PRBool is_migrating = PR_FALSE;
	if (migrating && nsCRT::strcmp (migrating, "true"))
		is_migrating = PR_TRUE;

	/*
	 * The creation of the address book in the preferences
	 * is very MDB implementation specific.
	 * If the fileName attribute is null then it will
	 * create an appropriate file name.
	 * Somehow have to resolve this issue so that it
	 * is more general.
	 *
	 */
	DIR_Server* server = nsnull;
	rv = DIR_AddNewAddressBook(description,
			(fileNameUTF8.Length ()) ? fileNameUTF8.get () : nsnull,
			is_migrating,
			PABDirectory,
			&server);
	NS_ENSURE_SUCCESS (rv, rv);


	// Update the file name property
	NS_ConvertUTF8toUCS2 fileNameUCS2(server->fileName);
	propertySet.Put (&fileNameKey, (void* )fileNameUCS2.get ());
	
	// Add the URI property
	nsAutoString URI;
	URI.AssignWithConversion (kMDBDirectoryRoot);
	URI.Append (fileNameUCS2);
	nsCStringKey URIKey (kURIPropertyName, -1, nsCStringKey::NEVER_OWN);
	propertySet.Put (&URIKey, (void* )URI.get ());

	// Get the directory factory from the URI
	NS_ConvertUCS2toUTF8 URIUTF8(URI);

	rv = CreateNewDirectory (propertySet, URIUTF8.get (), server);

	return rv;
}

NS_IMETHODIMP nsAbBSDirectory::CreateDirectoryByURI(const PRUnichar *displayName, const char *uri, PRBool migrating)
{
	if (!displayName || !uri)
		return NS_ERROR_NULL_POINTER;

	nsresult rv = NS_OK;

	const char* fileName = nsnull;
	if (PL_strstr(uri, kMDBDirectoryRoot)) // for moz-abmdbdirectory://
		fileName = &(uri[nsCRT::strlen(kMDBDirectoryRoot)]);

	DIR_Server * server = nsnull;
	rv = DIR_AddNewAddressBook(displayName, fileName, migrating, PABDirectory, &server);
	NS_ENSURE_SUCCESS(rv,rv);


	nsHashtable propertySet;

	// Set the description property
	nsCStringKey descriptionKey (kDescriptionPropertyName, -1, nsCStringKey::NEVER_OWN);
	propertySet.Put (&descriptionKey, (void* )displayName);
	
	// Set the file name property
	nsCStringKey fileNameKey (kFileNamePropertyName, -1, nsCStringKey::NEVER_OWN);
	NS_ConvertUTF8toUCS2 fileNameUCS2 (server->fileName);
	propertySet.Put (&fileNameKey, (void* )fileNameUCS2.get ());
	
	// Set the uri property
	nsCStringKey URIKey (kURIPropertyName, -1, nsCStringKey::NEVER_OWN);
	NS_ConvertUTF8toUCS2 URIUCS2 (uri);
	propertySet.Put (&URIKey, (void* )URIUCS2.get ());

	rv = CreateNewDirectory (propertySet, uri, server); 

	return rv;
}



struct GetDirectories
{
		GetDirectories (DIR_Server* aServer) :
				mServer (aServer)
		{
			NS_NewISupportsArray(getter_AddRefs(directories));
		}

		nsCOMPtr<nsISupportsArray> directories;
		DIR_Server* mServer;
};

PRBool PR_CALLBACK GetDirectories_getDirectory (nsHashKey *aKey, void *aData, void* closure)
{
	GetDirectories* getDirectories = (GetDirectories* )closure;

	DIR_Server* server = (DIR_Server*) aData;
	if (server == getDirectories->mServer)
	{
			nsVoidKey* voidKey = (nsVoidKey* )aKey;
			nsIAbDirectory* directory = (nsIAbDirectory* )voidKey->GetValue ();
			getDirectories->directories->AppendElement (directory);
	}

	return PR_TRUE;
}

NS_IMETHODIMP nsAbBSDirectory::DeleteDirectory(nsIAbDirectory *directory)
{
	nsresult rv = NS_OK;
	
	if (!directory)
		return NS_ERROR_NULL_POINTER;

	DIR_Server *server = nsnull;
	nsVoidKey key((void *)directory);
	server = (DIR_Server* )mServers.Get (&key);

	if (!server)
		return NS_ERROR_FAILURE;

	GetDirectories getDirectories (server);
	mServers.Enumerate (GetDirectories_getDirectory, (void *)&getDirectories);

	DIR_DeleteServerFromList(server);
	
	nsCOMPtr<nsIAbDirFactoryService> dirFactoryService = 
			do_GetService(NS_ABDIRFACTORYSERVICE_CONTRACTID,&rv);
	NS_ENSURE_SUCCESS (rv, rv);

	PRUint32 count;
	rv = getDirectories.directories->Count (&count);
	NS_ENSURE_SUCCESS(rv, rv);

	for (PRUint32 i = 0; i < count; i++)
	{
		nsCOMPtr<nsIAbDirectory> d;
		getDirectories.directories->GetElementAt (i, getter_AddRefs(d));

		nsVoidKey k((void *)d);
		mServers.Remove(&k);

		rv = mSubDirectories->RemoveElement(d);
		NotifyItemDeleted(d);

		nsCOMPtr<nsIRDFResource> resource (do_QueryInterface (d, &rv));
		const char* uri;
		resource->GetValueConst (&uri);

		nsCOMPtr<nsIAbDirFactory> dirFactory;
		rv = dirFactoryService->GetDirFactory (uri, getter_AddRefs(dirFactory));
		if (NS_FAILED(rv))
				continue;

		rv = dirFactory->DeleteDirectory(d);
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

