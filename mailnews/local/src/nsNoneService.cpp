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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Seth Spitzer <sspitzer@netscape.com>
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

#include "msgCore.h"    // precompiled header...

#include "nsNoneService.h"
#include "nsINoIncomingServer.h"
#include "nsINoneService.h"
#include "nsIMsgProtocolInfo.h"
#include "nsIMsgMailSession.h"

#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsMsgUtils.h"

#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"

#define PREF_MAIL_ROOT_NONE "mail.root.none"        // old - for backward compatibility only
#define PREF_MAIL_ROOT_NONE_REL "mail.root.none-rel"

nsNoneService::nsNoneService()
{
}

nsNoneService::~nsNoneService()
{}

NS_IMPL_ISUPPORTS2(nsNoneService, nsINoneService, nsIMsgProtocolInfo)

NS_IMETHODIMP
nsNoneService::SetDefaultLocalPath(nsIFileSpec *aPath)
{
    NS_ENSURE_ARG(aPath);
    nsresult rv;
    
    nsFileSpec spec;
    rv = aPath->GetFileSpec(&spec);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsILocalFile> localFile;
    NS_FileSpecToIFile(&spec, getter_AddRefs(localFile));
    if (!localFile) return NS_ERROR_FAILURE;
    
    rv = NS_SetPersistentFile(PREF_MAIL_ROOT_NONE_REL, PREF_MAIL_ROOT_NONE, localFile);

    return rv;
}     

NS_IMETHODIMP
nsNoneService::GetDefaultLocalPath(nsIFileSpec ** aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
    
    nsresult rv;
    PRBool havePref;
    nsCOMPtr<nsILocalFile> localFile;    
    rv = NS_GetPersistentFile(PREF_MAIL_ROOT_NONE_REL,
                              PREF_MAIL_ROOT_NONE,
                              NS_APP_MAIL_50_DIR,
                              havePref,
                              getter_AddRefs(localFile));
    if (NS_FAILED(rv)) return rv;
    
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
        if (NS_FAILED(rv)) return rv;
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpecFromIFile(localFile, getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    
    if (!havePref || !exists) {
        rv = NS_SetPersistentFile(PREF_MAIL_ROOT_NONE_REL, PREF_MAIL_ROOT_NONE, localFile);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to set root dir pref.");
    }
        
    *aResult = outSpec;
    NS_IF_ADDREF(*aResult);
    return NS_OK;


    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = nsnull;
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
nsNoneService::GetCanLoginAtStartUp(PRBool *aCanLoginAtStartUp)
{
        NS_ENSURE_ARG_POINTER(aCanLoginAtStartUp);
        *aCanLoginAtStartUp = PR_FALSE;
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
nsNoneService::GetCanGetMessages(PRBool *aCanGetMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetMessages);
    *aCanGetMessages = PR_FALSE;
    return NS_OK;
}  

NS_IMETHODIMP
nsNoneService::GetCanGetIncomingMessages(PRBool *aCanGetIncomingMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetIncomingMessages);
    *aCanGetIncomingMessages = PR_FALSE;
    return NS_OK;
} 

NS_IMETHODIMP 
nsNoneService::GetDefaultDoBiff(PRBool *aDoBiff)
{
    NS_ENSURE_ARG_POINTER(aDoBiff);
    // by default, don't do biff for "none" servers
    *aDoBiff = PR_FALSE;    
    return NS_OK;
}

NS_IMETHODIMP
nsNoneService::GetDefaultServerPort(PRBool isSecure, PRInt32 *aDefaultPort)
{
    NS_ASSERTION(0, "This should probably never be called!");
    NS_ENSURE_ARG_POINTER(aDefaultPort);
    *aDefaultPort = -1;
    return NS_OK;
}

NS_IMETHODIMP 
nsNoneService::GetShowComposeMsgLink(PRBool *showComposeMsgLink)
{
    NS_ENSURE_ARG_POINTER(showComposeMsgLink);
    *showComposeMsgLink = PR_FALSE;    
    return NS_OK;
}

NS_IMETHODIMP
nsNoneService::GetNeedToBuildSpecialFolderURIs(PRBool *needToBuildSpecialFolderURIs)
{
    NS_ENSURE_ARG_POINTER(needToBuildSpecialFolderURIs);
    *needToBuildSpecialFolderURIs = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsNoneService::GetSpecialFoldersDeletionAllowed(PRBool *specialFoldersDeletionAllowed)
{
    NS_ENSURE_ARG_POINTER(specialFoldersDeletionAllowed);
    *specialFoldersDeletionAllowed = PR_TRUE;
    return NS_OK;
}

