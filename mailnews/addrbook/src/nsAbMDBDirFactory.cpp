/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc.  Portions created by Sun are
 * Copyright (C) 2001 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Paul Sandoz   <paul.sandoz@sun.com> 
 *                   Csaba Borbola <csaba.borbola@sun.com>
 */

#include "nsAbMDBDirFactory.h"
#include "nsAbUtils.h"

#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFResource.h"

#include "nsIAbMDBDirectory.h"
#include "nsAbDirFactoryService.h"
#include "nsAbMDBDirFactory.h"
#include "nsIAddressBook.h"
#include "nsIAddrBookSession.h"
#include "nsIAddrDBListener.h"

#include "nsEnumeratorUtils.h"

#include "nsAbBaseCID.h"





extern const char* kDescriptionPropertyName;
extern const char* kURIPropertyName;

NS_IMPL_ISUPPORTS1(nsAbMDBDirFactory, nsIAbDirFactory);

nsAbMDBDirFactory::nsAbMDBDirFactory()
{
  NS_INIT_ISUPPORTS();
}

nsAbMDBDirFactory::~nsAbMDBDirFactory()
{
}


static nsresult RemoveMailListDBListeners (nsIAddrDatabase* database, nsIAbDirectory* directory)
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> pAddressLists;
    rv = directory->GetAddressLists(getter_AddRefs(pAddressLists));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 total;
    rv = pAddressLists->Count(&total);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 0; i < total; i++)
    {
        nsCOMPtr<nsISupports> pSupport;
        rv = pAddressLists->GetElementAt(i, getter_AddRefs(pSupport));
        if (NS_FAILED(rv))
            break;

        nsCOMPtr<nsIAbDirectory> listDir(do_QueryInterface(pSupport, &rv));
        if (NS_FAILED(rv))
            break;
        nsCOMPtr<nsIAddrDBListener> dbListener(do_QueryInterface(pSupport, &rv));
        if (NS_FAILED(rv))
            break;

        database->RemoveListener(dbListener);
    }

    return NS_OK;
}

/* nsISimpleEnumerator createDirectory (in unsigned long propertiesSize, [array, size_is (propertiesSize)] in string propertyNamesArray, [array, size_is (propertiesSize)] in wstring propertyValuesArray); */
NS_IMETHODIMP nsAbMDBDirFactory::CreateDirectory(
    PRUint32 propertiesSize,
    const char **propertyNamesArray,
    const PRUnichar **propertyValuesArray,
    nsISimpleEnumerator **_retval)
{
    if (!*propertyNamesArray || !*propertyValuesArray)
        return NS_ERROR_NULL_POINTER;
    
    if (propertiesSize == 0)
        return NS_ERROR_FAILURE;

    nsresult rv;

    // Create hash table from property arrays
    nsHashtable propertySet;
    rv = PropertyPtrArraysToHashtable::Convert (
            propertySet,
            propertiesSize,
            propertyNamesArray,
            propertyValuesArray);
    NS_ENSURE_SUCCESS (rv, rv);

    // Get description property
    nsCStringKey descriptionKey (kDescriptionPropertyName, -1, nsCStringKey::NEVER_OWN);
    const PRUnichar* description = NS_REINTERPRET_CAST(PRUnichar*, propertySet.Get (&descriptionKey));

    // Get uri property
    nsCStringKey URIKey (kURIPropertyName, -1, nsCStringKey::NEVER_OWN);
    const PRUnichar* URIUCS2 = NS_REINTERPRET_CAST(PRUnichar*, propertySet.Get (&URIKey));
    if (!URIUCS2)
        return NS_ERROR_FAILURE;
    NS_ConvertUCS2toUTF8 URIUTF8(URIUCS2);
    
	nsCOMPtr<nsIRDFService> rdf = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFResource> resource;
    rv = rdf->GetResource(URIUTF8.get (), getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    directory->SetDirName(description);


    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsFileSpec* dbPath;
    abSession->GetUserProfileDirectory(&dbPath);

    const char* fileName = nsnull;
    const char* uri = URIUTF8.get ();
    if (PL_strstr(uri, kMDBDirectoryRoot)) // for moz-abmdbdirectory://
        fileName = &(uri[PL_strlen(kMDBDirectoryRoot)]);

    nsAutoString file;
    file.AssignWithConversion(fileName);
    (*dbPath) += file;

    nsCOMPtr<nsIAddrDatabase>  addrDBFactory = do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAddrDatabase>  listDatabase;  
    rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(listDatabase), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = listDatabase->GetMailingListsFromDB(directory);
    NS_ENSURE_SUCCESS(rv, rv);

    /*
     * This is a hack because of the way
     * MDB databases reference MDB directories
     * after the mail lists have been created.
     *
     * To stop assertions when the listDatabase
     * goes out of scope it is necessary to remove
     * all mail lists which have been added as
     * listeners to the database
     */
    rv = RemoveMailListDBListeners (listDatabase, directory);
    NS_ENSURE_SUCCESS(rv, rv);

    nsSingletonEnumerator* cursor =    new nsSingletonEnumerator(directory);
    if(!cursor)
        return NS_ERROR_NULL_POINTER;
    *_retval = cursor;
    NS_IF_ADDREF(*_retval);

    return rv;
}



/* void deleteDirectory (in nsIAbDirectory directory); */
NS_IMETHODIMP nsAbMDBDirFactory::DeleteDirectory(nsIAbDirectory *directory)
{
    if (!directory)
        return NS_ERROR_NULL_POINTER;
    
    nsresult rv = NS_OK;

    nsCOMPtr<nsISupportsArray> pAddressLists;
    rv = directory->GetAddressLists(getter_AddRefs(pAddressLists));
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 total;
    rv = pAddressLists->Count(&total);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i = 0; i < total; i++)
    {
        nsCOMPtr<nsISupports> pSupport;
        rv = pAddressLists->GetElementAt(i, getter_AddRefs(pSupport));
        if (NS_FAILED(rv))
            break;

        nsCOMPtr<nsIAbDirectory> listDir(do_QueryInterface(pSupport, &rv));
        if (NS_FAILED(rv))
            break;

        nsCOMPtr<nsIAbMDBDirectory> dblistDir(do_QueryInterface(pSupport, &rv));
        if (NS_FAILED(rv))
            break;

        rv = directory->DeleteDirectory(listDir);
        if (NS_FAILED(rv))
            break;

        rv = dblistDir->RemoveElementsFromAddressList();
        if (NS_FAILED(rv))
            break;

        pAddressLists->RemoveElement(pSupport);
    }

    nsCOMPtr<nsIAbMDBDirectory> dbdirectory(do_QueryInterface(directory, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbdirectory->ClearDatabase ();
    return rv;
}

