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
* The Original Code is mozilla mailnews.
*
* The Initial Developer of the Original Code is
* Seth Spitzer <sspitzer@mozilla.org>.
* Portions created by the Initial Developer are Copyright (C) 2004
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsRssService.h"
#include "nsIRssIncomingServer.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsAppDirectoryServiceDefs.h"

nsRssService::nsRssService()
{
}

nsRssService::~nsRssService()
{
}

NS_IMPL_ISUPPORTS2(nsRssService,
                   nsIRssService,
                   nsIMsgProtocolInfo)
                   
NS_IMETHODIMP nsRssService::GetDefaultLocalPath(nsIFileSpec * *aDefaultLocalPath)
{
    NS_ENSURE_ARG_POINTER(aDefaultLocalPath);
    *aDefaultLocalPath = nsnull;
    
    nsCOMPtr<nsILocalFile> localFile;
    nsCOMPtr<nsIProperties> dirService(do_GetService("@mozilla.org/file/directory_service;1"));
    if (!dirService) return NS_ERROR_FAILURE;
    dirService->Get(NS_APP_MAIL_50_DIR, NS_GET_IID(nsILocalFile), getter_AddRefs(localFile));
    if (!localFile) return NS_ERROR_FAILURE;

    PRBool exists;
    nsresult rv = localFile->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpecFromIFile(localFile, getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    
    NS_IF_ADDREF(*aDefaultLocalPath = outSpec);
    return NS_OK;

}

NS_IMETHODIMP nsRssService::SetDefaultLocalPath(nsIFileSpec * aDefaultLocalPath)
{
    NS_ASSERTION(0,"foo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRssService::GetServerIID(nsIID * *aServerIID)
{
    NS_ASSERTION(0,"foo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRssService::GetRequiresUsername(PRBool *aRequiresUsername)
{
    NS_ASSERTION(0,"foo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRssService::GetPreflightPrettyNameWithEmailAddress(PRBool *aPreflightPrettyNameWithEmailAddress)
{
    NS_ASSERTION(0,"foo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRssService::GetCanDelete(PRBool *aCanDelete)
{
    NS_ENSURE_ARG_POINTER(aCanDelete);
    *aCanDelete = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsRssService::GetCanLoginAtStartUp(PRBool *aCanLoginAtStartUp)
{
    NS_ENSURE_ARG_POINTER(aCanLoginAtStartUp);
    *aCanLoginAtStartUp = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsRssService::GetCanDuplicate(PRBool *aCanDuplicate)
{
    NS_ENSURE_ARG_POINTER(aCanDuplicate);
    *aCanDuplicate = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsRssService::GetDefaultServerPort(PRBool isSecure, PRInt32 *_retval)
{
    NS_ASSERTION(0,"foo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRssService::GetCanGetMessages(PRBool *aCanGetMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetMessages);
    *aCanGetMessages = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsRssService::GetCanGetIncomingMessages(PRBool *aCanGetIncomingMessages)
{
    NS_ENSURE_ARG_POINTER(aCanGetIncomingMessages);
    *aCanGetIncomingMessages = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsRssService::GetDefaultDoBiff(PRBool *aDefaultDoBiff)
{
    NS_ENSURE_ARG_POINTER(aDefaultDoBiff);
    // by default, do biff for RSS feeds
    *aDefaultDoBiff = PR_TRUE;    
    return NS_OK;
}

NS_IMETHODIMP nsRssService::GetShowComposeMsgLink(PRBool *aShowComposeMsgLink)
{
    NS_ENSURE_ARG_POINTER(aShowComposeMsgLink);
    *aShowComposeMsgLink = PR_FALSE;    
    return NS_OK;
}

NS_IMETHODIMP nsRssService::GetNeedToBuildSpecialFolderURIs(PRBool *aNeedToBuildSpecialFolderURIs)
{
    NS_ASSERTION(0,"foo");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsRssService::GetSpecialFoldersDeletionAllowed(PRBool *aSpecialFoldersDeletionAllowed)
{
    NS_ASSERTION(0,"foo");
    return NS_ERROR_NOT_IMPLEMENTED;
}