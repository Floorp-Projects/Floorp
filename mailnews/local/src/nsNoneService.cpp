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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Seth Spitzer <sspitzer@netscape.com>
 */

#include "msgCore.h"    // precompiled header...

#include "nsNoneService.h"
#include "nsINoIncomingServer.h"
#include "nsINoneService.h"
#include "nsIMsgProtocolInfo.h"
#include "nsIMsgMailSession.h"

#include "nsIPref.h"

#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"

#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"

#define PREF_MAIL_ROOT_NONE "mail.root.none"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsNoneService::nsNoneService()
{
    NS_INIT_REFCNT();
}

nsNoneService::~nsNoneService()
{}

NS_IMPL_ISUPPORTS2(nsNoneService, nsINoneService, nsIMsgProtocolInfo); 

NS_IMETHODIMP
nsNoneService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->SetFilePref(PREF_MAIL_ROOT_NONE, aPath, PR_FALSE /* set default */);
    return rv;
}     

NS_IMETHODIMP
nsNoneService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    PRBool havePref;
    nsCOMPtr<nsILocalFile> prefLocal;
    nsCOMPtr<nsIFile> localFile;
    rv = prefs->GetFileXPref(PREF_MAIL_ROOT_NONE, getter_AddRefs(prefLocal));
    if (NS_SUCCEEDED(rv)) {
        localFile = prefLocal;
        havePref = PR_TRUE;
    }
    if (!localFile) {
        rv = NS_GetSpecialDirectory(NS_APP_MAIL_50_DIR, getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
        havePref = PR_FALSE;
    }
    
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) {
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
    }
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsXPIDLCString pathBuf;
    rv = localFile->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpec(getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    outSpec->SetNativePath(pathBuf);
    
    if (!havePref || !exists)
        rv = SetDefaultLocalPath(outSpec);
        
    *aResult = outSpec;
    NS_IF_ADDREF(*aResult);
    return rv;
}
    

NS_IMETHODIMP
nsNoneService::GetServerIID(nsIID* *aServerIID)
{
    *aServerIID = new nsIID(NS_GET_IID(nsINoIncomingServer));
    return NS_OK;
}

NS_IMETHODIMP
nsNoneService::GetRequiresUsername(PRBool *aRequiresUsername)
{
        NS_ENSURE_ARG_POINTER(aRequiresUsername);
        *aRequiresUsername = PR_TRUE;
        return NS_OK;
}

NS_IMETHODIMP
nsNoneService::GetPreflightPrettyNameWithEmailAddress(PRBool *aPreflightPrettyNameWithEmailAddress)
{
        NS_ENSURE_ARG_POINTER(aPreflightPrettyNameWithEmailAddress);
        *aPreflightPrettyNameWithEmailAddress = PR_TRUE;
        return NS_OK;
}

NS_IMETHODIMP
nsNoneService::GetCanDelete(PRBool *aCanDelete)
{
        NS_ENSURE_ARG_POINTER(aCanDelete);
        *aCanDelete = PR_FALSE;
        return NS_OK;
}  

NS_IMETHODIMP
nsNoneService::GetCanDuplicate(PRBool *aCanDuplicate)
{
        NS_ENSURE_ARG_POINTER(aCanDuplicate);
        *aCanDuplicate = PR_FALSE;
        return NS_OK;
}  

NS_IMETHODIMP
nsNoneService::GetDefaultCopiesAndFoldersPrefsToServer(PRBool *aDefaultCopiesAndFoldersPrefsToServer)
{
    NS_ENSURE_ARG_POINTER(aDefaultCopiesAndFoldersPrefsToServer);
    // when a "none" server is created (like "Local Folders")
	// the copies and folder prefs for the associated identity
    // point to folders on this server.
    *aDefaultCopiesAndFoldersPrefsToServer = PR_TRUE;
    return NS_OK;
}   

NS_IMETHODIMP
nsNoneService::GetDefaultServerPort(PRInt32 *aDefaultPort)
{
    NS_ASSERTION(0, "This should probably never be called!");
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = -1;
    return NS_OK;
}
