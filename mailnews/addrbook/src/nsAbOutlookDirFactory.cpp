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
 * Contributor(s): 
 *
 * Created by Cyrille Moureaux <Cyrille.Moureaux@sun.com>
 */
#include "nsAbOutlookDirFactory.h"
#include "nsMapiAddressBook.h"
#include "nsIAbDirectory.h"

#include "nsIRDFService.h"
#include "nsIRDFResource.h"
#include "nsRDFResource.h"
#include "nsEnumeratorUtils.h"

#include "nsAbBaseCID.h"

#include "prprf.h"

#include "nslog.h"

NS_IMPL_LOG(nsAbOutlookDirFactoryLog)

#define PRINTF NS_LOG_PRINTF(nsAbOutlookDirFactoryLog)
#define FLUSH  NS_LOG_FLUSH(nsAbOutlookDirFactoryLog)


// In case someone is wondering WHY I have to undefine CreateDirectory,
// it's because the windows files winbase.h and wininet.h define this
// to CreateDirectoryA/W (for reasons best left unknown) and with the
// MAPI stuff, I end up including this, which wreaks havoc on my symbol
// table.
#ifdef CreateDirectory 
#  undef CreateDirectory
#endif // CreateDirectory

NS_IMPL_ISUPPORTS1(nsAbOutlookDirFactory, nsIAbDirFactory)

nsAbOutlookDirFactory::nsAbOutlookDirFactory(void)
{
    NS_INIT_ISUPPORTS() ;
}

nsAbOutlookDirFactory::~nsAbOutlookDirFactory(void)
{
}

extern const char *kOutlookDirectoryScheme ;

NS_IMETHODIMP nsAbOutlookDirFactory::CreateDirectory(PRUint32 aNbProperties, 
                                                     const char **aPropertyNames, 
                                                     const PRUnichar **aPropertyValues, 
                                                     nsISimpleEnumerator **aDirectories)
{
    if (aPropertyNames == nsnull ||
        aPropertyValues == nsnull || 
        aDirectories == nsnull) { return NS_ERROR_NULL_POINTER ; }
    *aDirectories = nsnull ;
    nsresult retCode = NS_OK ;
    nsMapiAddressBook mapiAddBook ;
    nsMapiEntryArray folders ;
    ULONG nbFolders = 0 ;
    nsCOMPtr<nsISupportsArray> directories ;
    
    retCode = NS_NewISupportsArray(getter_AddRefs(directories)) ;
    NS_ENSURE_SUCCESS(retCode, retCode) ;
    if (!mapiAddBook.GetFolders(folders)) {
        return NS_ERROR_FAILURE ;
    }
    nsCOMPtr<nsIRDFService> rdf = do_GetService (NS_RDF_CONTRACTID "/rdf-service;1", &retCode);
    NS_ENSURE_SUCCESS(retCode, retCode);
    nsCAutoString entryId ;
    nsCAutoString uri ;
    nsCOMPtr<nsIRDFResource> resource ;
    
    for (ULONG i = 0 ; i < folders.mNbEntries ; ++ i) {
        if (mapiAddBook.EntryToString(folders.mEntries [i], entryId)) {
            uri.Assign(kOutlookDirectoryScheme) ;
            uri.Append(entryId) ;
            retCode = rdf->GetResource(uri, getter_AddRefs(resource)) ;
            NS_ENSURE_SUCCESS(retCode, retCode) ;
            directories->AppendElement(resource) ;
        }
    }
    return NS_NewArrayEnumerator(aDirectories, directories) ;
}

// No actual deletion, since you cannot create the address books from Mozilla.
NS_IMETHODIMP nsAbOutlookDirFactory::DeleteDirectory(nsIAbDirectory *aDirectory)
{
    return NS_OK ;
}

