/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Sandoz <paul.sandoz@sun.com> 
 *                   Csaba Borbola <csaba.borbola@sun.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

NS_IMPL_ISUPPORTS1(nsAbMDBDirFactory, nsIAbDirFactory);

nsAbMDBDirFactory::nsAbMDBDirFactory()
{
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

NS_IMETHODIMP nsAbMDBDirFactory::CreateDirectory(nsIAbDirectoryProperties *aProperties,
    nsISimpleEnumerator **_retval)
{
    NS_ENSURE_ARG_POINTER(aProperties);
    NS_ENSURE_ARG_POINTER(_retval);

    nsresult rv;

    nsXPIDLCString uri;
    nsAutoString description;

    rv = aProperties->GetDescription(description);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aProperties->GetURI(getter_Copies(uri));
    NS_ENSURE_SUCCESS(rv, rv);
    
	nsCOMPtr<nsIRDFService> rdf = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFResource> resource;
    rv = rdf->GetResource(uri.get(), getter_AddRefs(resource));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAbDirectory> directory(do_QueryInterface(resource, &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = directory->SetDirName(description.get());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAddrBookSession> abSession = do_GetService(NS_ADDRBOOKSESSION_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsFileSpec* dbPath;
    rv = abSession->GetUserProfileDirectory(&dbPath);

    nsCOMPtr<nsIAddrDatabase>  listDatabase;  
    if (dbPath)
    {
      nsCAutoString fileName;
      nsDependentCString uriStr(uri);
      
      if (Substring(uriStr, 0, kMDBDirectoryRootLen).Equals(kMDBDirectoryRoot))
          fileName = Substring(uriStr, kMDBDirectoryRootLen, uriStr.Length() - kMDBDirectoryRootLen);

      (*dbPath) += fileName.get();

      nsCOMPtr<nsIAddrDatabase>  addrDBFactory = do_GetService(NS_ADDRDATABASE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = addrDBFactory->Open(dbPath, PR_TRUE, getter_AddRefs(listDatabase), PR_TRUE);
      delete dbPath;
    }
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
    
    NS_IF_ADDREF(*_retval = cursor);
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

