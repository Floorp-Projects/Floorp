/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Paul Sandoz   <paul.sandoz@sun.com>
 *   Csaba Borbola <csaba.borbola@sun.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIPrefService.h"
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
#include "nsArrayEnumerator.h"

nsAbBSDirectory::nsAbBSDirectory()
: nsRDFResource(),
mInitialized(PR_FALSE),
mServers (13)
{
}

nsAbBSDirectory::~nsAbBSDirectory()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsAbBSDirectory, nsRDFResource, nsIAbDirectory)

nsresult nsAbBSDirectory::NotifyItemAdded(nsISupports *item)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIAddrBookSession> abSession = 
    do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv); 
  if(NS_SUCCEEDED(rv))
    abSession->NotifyDirectoryItemAdded(this, item);
  return NS_OK;
}

nsresult nsAbBSDirectory::NotifyItemDeleted(nsISupports *item)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIAddrBookSession> abSession = 
    do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
  
  if(NS_SUCCEEDED(rv))
    abSession->NotifyDirectoryDeleted(this, item);
  
  return NS_OK;
}

nsresult nsAbBSDirectory::CreateDirectoriesFromFactory(
                                                       nsIAbDirectoryProperties *aProperties,
                                                       DIR_Server *aServer,
                                                       PRBool aNotify)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aProperties);
  
  // Get the directory factory service
  nsCOMPtr<nsIAbDirFactoryService> dirFactoryService = 
    do_GetService(NS_ABDIRFACTORYSERVICE_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS (rv, rv);
		
  // Get the directory factory from the URI
  nsXPIDLCString uri;
  rv = aProperties->GetURI(getter_Copies(uri));
  NS_ENSURE_SUCCESS(rv,rv);
  
  nsCOMPtr<nsIAbDirFactory> dirFactory;
  rv = dirFactoryService->GetDirFactory(uri.get(), getter_AddRefs(dirFactory));
  NS_ENSURE_SUCCESS (rv, rv);
  
  // Create the directories
  nsCOMPtr<nsISimpleEnumerator> newDirEnumerator;
  rv = dirFactory->CreateDirectory(aProperties, getter_AddRefs(newDirEnumerator));
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
    
    // Define a relationship between the preference
    // entry and the directory
    nsVoidKey key((void *)childDir);
    mServers.Put (&key, (void *)aServer);
    
    mSubDirectories.AppendObject(childDir);
    
    // Inform the listener, i.e. the RDF directory data
    // source that a new address book has been added
    if (aNotify)
      NotifyItemAdded(childDir);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsAbBSDirectory::GetChildNodes(nsISimpleEnumerator* *aResult)
{
  if (!mInitialized) 
  {
    nsresult rv;
    nsCOMPtr<nsIAbDirFactoryService> dirFactoryService = 
      do_GetService(NS_ABDIRFACTORYSERVICE_CONTRACTID,&rv);
    NS_ENSURE_SUCCESS (rv, rv);
    
    if (!DIR_GetDirectories())
      return NS_ERROR_FAILURE;
    
    PRInt32 count = DIR_GetDirectories()->Count();
    for (PRInt32 i = 0; i < count; i++)
    {
      DIR_Server *server = (DIR_Server *)(DIR_GetDirectories()->ElementAt(i));
      
      // if this is a 4.x, local .na2 addressbook (PABDirectory)
      // we must skip it.
      // mozilla can't handle 4.x .na2 addressbooks
      // note, the filename might be na2 for 4.x LDAP directories
      // (we used the .na2 file for replication), and we don't want to skip
      // those.  see bug #127007
      PRUint32 fileNameLen = strlen(server->fileName);
      if (((fileNameLen > kABFileName_PreviousSuffixLen) && 
        strcmp(server->fileName + fileNameLen - kABFileName_PreviousSuffixLen, kABFileName_PreviousSuffix) == 0) &&
        (server->dirType == PABDirectory))
        continue;
      
      nsCOMPtr <nsIAbDirectoryProperties> properties;
      properties = do_CreateInstance(NS_ABDIRECTORYPROPERTIES_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv,rv);
      
      NS_ConvertUTF8toUCS2 description (server->description);
      rv = properties->SetDescription(description);
      NS_ENSURE_SUCCESS(rv,rv);
      
      rv = properties->SetFileName(server->fileName);
      NS_ENSURE_SUCCESS(rv,rv);
      
      // Set the uri property
      nsCAutoString URI (server->uri);
      // This is in case the uri is never set
      // in the nsDirPref.cpp code.
      if (!server->uri)
        URI = NS_LITERAL_CSTRING(kMDBDirectoryRoot) + nsDependentCString(server->fileName);
      
      /*
      * Check that we are not converting from a
      * a 4.x address book file e.g. pab.na2
      * check if the URI ends with ".na2"
      */
      if (StringEndsWith(URI, NS_LITERAL_CSTRING(kABFileName_PreviousSuffix))) 
        URI.ReplaceSubstring(URI.get() + kMDBDirectoryRootLen, server->fileName);
      
      rv = properties->SetPrefName(server->prefName);
      NS_ENSURE_SUCCESS(rv,rv);
      
      rv = properties->SetURI(URI.get());
      NS_ENSURE_SUCCESS(rv,rv);
      
      // Create the directories
      rv = CreateDirectoriesFromFactory(properties,
        server, PR_FALSE /* notify */);
    }
    
    mInitialized = PR_TRUE;
    // sort directories by position...
  }
  return NS_NewArrayEnumerator(aResult, mSubDirectories);
}

NS_IMETHODIMP nsAbBSDirectory::CreateNewDirectory(nsIAbDirectoryProperties *aProperties)
{
/*
* TODO
* This procedure is still MDB specific
* due to the dependence on the current
* nsDirPref.cpp code
*
	 */
  NS_ENSURE_ARG_POINTER(aProperties);
  nsresult rv;
  
  nsAutoString description;
  nsXPIDLCString fileName;
  nsXPIDLCString uri;
  nsXPIDLCString authDn;
  
  rv = aProperties->GetDescription(description);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = aProperties->GetFileName(getter_Copies(fileName));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = aProperties->GetURI(getter_Copies(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint32 dirType;
  rv = aProperties->GetDirType(&dirType);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint32 maxHits;
  rv = aProperties->GetMaxHits(&maxHits);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = aProperties->GetAuthDn(getter_Copies(authDn));
  NS_ENSURE_SUCCESS(rv, rv);
  
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
  rv = DIR_AddNewAddressBook(description.get(),
    (fileName.Length ()) ? fileName.get () : nsnull,
    PR_FALSE /* is_migrating */, uri.get(),
    maxHits, authDn,
    (DirectoryType)dirType, 
    &server);
  NS_ENSURE_SUCCESS (rv, rv);
  
  // Update the file name property
  rv = aProperties->SetFileName(server->fileName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (dirType != LDAPDirectory) {
    // Add the URI property
    nsCAutoString URI(NS_LITERAL_CSTRING(kMDBDirectoryRoot) + nsDependentCString(server->fileName));
    rv = aProperties->SetURI(URI.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  rv = aProperties->SetPrefName(server->prefName);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = CreateDirectoriesFromFactory(aProperties, server, PR_TRUE /* notify */);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsAbBSDirectory::CreateDirectoryByURI(const PRUnichar *aDisplayName, const char *aURI, PRBool migrating)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aDisplayName);

  nsresult rv = NS_OK;

  const char* fileName = nsnull;
  if (StringBeginsWith(nsDependentCString(aURI), NS_LITERAL_CSTRING(kMDBDirectoryRoot)))
    fileName = aURI + kMDBDirectoryRootLen;

  DIR_Server * server = nsnull;
  rv = DIR_AddNewAddressBook(aDisplayName, fileName, migrating, aURI, 0, nsnull, PABDirectory, &server);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIAbDirectoryProperties> properties;
  properties = do_CreateInstance(NS_ABDIRECTORYPROPERTIES_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = properties->SetDescription(nsDependentString(aDisplayName));
  NS_ENSURE_SUCCESS(rv,rv);
	
  rv = properties->SetFileName(server->fileName);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = properties->SetURI(aURI);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = properties->SetPrefName(server->prefName);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = properties->SetDirType(server->dirType);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = CreateDirectoriesFromFactory(properties, server, PR_TRUE /* notify */);
  NS_ENSURE_SUCCESS(rv,rv);
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
	nsresult rv;
	
	NS_ENSURE_ARG_POINTER(directory);

	// if addressbook is not launched yet mSevers will not be initialized
	// calling GetChildNodes will initialize mServers
	if (!mInitialized) {
		nsCOMPtr<nsISimpleEnumerator> subDirectories;
		rv = GetChildNodes(getter_AddRefs(subDirectories));
		NS_ENSURE_SUCCESS(rv, rv);
	}

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

		rv = mSubDirectories.RemoveObject(d);
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

NS_IMETHODIMP nsAbBSDirectory::ModifyDirectory(nsIAbDirectory *directory, nsIAbDirectoryProperties *aProperties)
{
  nsresult rv;

  NS_ENSURE_ARG_POINTER(directory);
  NS_ENSURE_ARG_POINTER(aProperties);

  // if addressbook is not launched yet mSevers will not be initialized
  // calling GetChildNodes will initialize mServers
  if (!mInitialized) {
    nsCOMPtr<nsISimpleEnumerator> subDirectories;
    rv = GetChildNodes(getter_AddRefs(subDirectories));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  DIR_Server *server = nsnull;
  nsVoidKey key((void *)directory);
  server = (DIR_Server* )mServers.Get (&key);

  if (!server)
    return NS_ERROR_FAILURE;
  GetDirectories getDirectories (server);
  mServers.Enumerate (GetDirectories_getDirectory, (void *)&getDirectories);

  nsAutoString description;
  nsXPIDLCString uri;
  nsXPIDLCString authDn;
  PRUint32 maxHits, palmSyncTimeStamp;
  PRInt32 palmCategoryId;

  rv = aProperties->GetDescription(description);
  NS_ENSURE_SUCCESS(rv, rv);
 
  NS_ConvertUTF8toUCS2 oldValue(server->description);
  nsCRT::free(server->description);
  NS_ConvertUCS2toUTF8 utf8str(description.get());
  server->description = ToNewCString(utf8str);

  rv = aProperties->GetURI(getter_Copies(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCRT::free(server->uri);
  server->uri = ToNewCString(uri);

  rv = aProperties->GetMaxHits(&maxHits);
  NS_ENSURE_SUCCESS(rv, rv);
  server->maxHits = maxHits;
  
  rv=aProperties->GetAuthDn(getter_Copies(authDn));
  NS_ENSURE_SUCCESS(rv, rv);
  server->authDn = ToNewCString(authDn);

  rv = aProperties->GetSyncTimeStamp(&palmSyncTimeStamp);
  NS_ENSURE_SUCCESS(rv, rv);
  server->PalmSyncTimeStamp = palmSyncTimeStamp;

  rv = aProperties->GetCategoryId(&palmCategoryId);
  NS_ENSURE_SUCCESS(rv, rv);
  server->PalmCategoryId = palmCategoryId;
  
  DIR_SavePrefsForOneServer(server);

  if (!oldValue.Equals(description)) {
    nsCOMPtr<nsIAbDirectory> modifiedDir;
    getDirectories.directories->GetElementAt (0, getter_AddRefs(modifiedDir));

    nsCOMPtr<nsIAddrBookSession> abSession = 
              do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);

    if (NS_SUCCEEDED(rv))
      abSession->NotifyItemPropertyChanged(modifiedDir, "DirName", oldValue.get(), description.get());
  }

  // Save modified address book into pref file.
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
  NS_ENSURE_SUCCESS(rv, rv);
  return prefService->SavePrefFile(nsnull);
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

