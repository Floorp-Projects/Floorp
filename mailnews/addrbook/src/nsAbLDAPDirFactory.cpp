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
 * Created by: Paul Sandoz   <paul.sandoz@sun.com> 
 *
 * Contributor(s): Csaba Borbola <csaba.borbola@sun.com>
 */

#include "nsAbLDAPDirFactory.h"
#include "nsAbUtils.h"

#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsIRDFResource.h"

#include "nsIAbDirectory.h"
#include "nsAbLDAPDirectory.h"
#include "nsAbDirFactoryService.h"
#include "nsIAddressBook.h"

#include "nsEnumeratorUtils.h"

#include "nsAbBaseCID.h"




extern const char* kDescriptionPropertyName;
extern const char* kURIPropertyName;


NS_IMPL_ISUPPORTS1(nsAbLDAPDirFactory, nsIAbDirFactory);

nsAbLDAPDirFactory::nsAbLDAPDirFactory()
{
    NS_INIT_ISUPPORTS();
}

nsAbLDAPDirFactory::~nsAbLDAPDirFactory()
{
}

/* nsISimpleEnumerator createDirectory (in unsigned long propertiesSize, [array, size_is (propertiesSize)] in string propertyNamesArray, [array, size_is (propertiesSize)] in wstring propertyValuesArray); */
NS_IMETHODIMP nsAbLDAPDirFactory::CreateDirectory(
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
    const PRUnichar* description = NS_REINTERPRET_CAST(PRUnichar* ,propertySet.Get (&descriptionKey));

    // Get uri property
    nsCStringKey URIKey (kURIPropertyName, -1, nsCStringKey::NEVER_OWN);
    const PRUnichar* URIUCS2 = NS_REINTERPRET_CAST(PRUnichar* ,propertySet.Get (&URIKey));
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

    NS_IF_ADDREF(*_retval = new nsSingletonEnumerator(directory));

    return *_retval ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* void deleteDirectory (in nsIAbDirectory directory); */
NS_IMETHODIMP nsAbLDAPDirFactory::DeleteDirectory(nsIAbDirectory *directory)
{
    // No actual deletion - as the LDAP Address Book is not physically
    // created in the corresponding CreateDirectory() unlike the Personal
    // Address Books. But we still need to return NS_OK from here.
    return NS_OK;
}

