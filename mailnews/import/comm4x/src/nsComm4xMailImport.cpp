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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Srilatha Moturi <srilatha@netscape.com>
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


/*

  Comm4xMail import mail interface

*/
#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif

#include "nscore.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIImportService.h"
#include "nsIComponentManager.h"
#include "nsComm4xMailImport.h"
#include "nsIMemory.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIImportService.h"
#include "nsIImportMailboxDescriptor.h"
#include "nsIImportGeneric.h"
#include "nsIImportFieldMap.h"
#include "nsIOutputStream.h"
#include "nsXPIDLString.h"
#include "nsTextFormatter.h"
#include "nsComm4xMailStringBundle.h"
#include "nsIStringBundle.h"
#include "nsReadableUtils.h"
#include "Comm4xMailDebugLog.h"

#include "nsDirectoryServiceDefs.h"

#include "nsIProxyObjectManager.h"
#include "nsProxiedService.h"

static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);

#define COMM4XMAIL_MSGS_URL   "chrome://messenger/locale/comm4xMailImportMsgs.properties"

PRLogModuleInfo *COMM4XLOGMODULE = nsnull;

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


nsComm4xMailImport::nsComm4xMailImport()
{
    // Init logging module.
    if (!COMM4XLOGMODULE)
      COMM4XLOGMODULE = PR_NewLogModule("IMPORT");

    IMPORT_LOG0("nsComm4xMailImport Module Created\n");

    nsCOMPtr <nsIStringBundleService> pBundleService;
    nsresult rv;

    m_pBundle = nsnull;

    pBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && (pBundleService)) 
           pBundleService->CreateBundle(COMM4XMAIL_MSGS_URL, getter_AddRefs(m_pBundle));
}


nsComm4xMailImport::~nsComm4xMailImport()
{

    IMPORT_LOG0("nsComm4xMailImport Module Deleted\n");

}



NS_IMPL_THREADSAFE_ISUPPORTS1(nsComm4xMailImport, nsIImportModule)


NS_IMETHODIMP nsComm4xMailImport::GetName(PRUnichar **name)
{
    NS_ENSURE_ARG_POINTER (name);
    nsresult rv = NS_ERROR_FAILURE;
    if (m_pBundle)
        rv = m_pBundle->GetStringFromID(COMM4XMAILIMPORT_NAME, name);
        return rv;
}

NS_IMETHODIMP nsComm4xMailImport::GetDescription(PRUnichar **name)
{
    NS_ENSURE_ARG_POINTER (name);
    nsresult rv = NS_ERROR_FAILURE;
    if (m_pBundle)
        rv = m_pBundle->GetStringFromID(COMM4XMAILIMPORT_DESCRIPTION, name);
    return rv;
}

NS_IMETHODIMP nsComm4xMailImport::GetSupports(char **supports)
{
    NS_ENSURE_ARG_POINTER (supports); 
    *supports = nsCRT::strdup(kComm4xMailSupportsString);
    return NS_OK;
}

NS_IMETHODIMP nsComm4xMailImport::GetSupportsUpgrade(PRBool *pUpgrade)
{
    NS_ENSURE_ARG_POINTER (pUpgrade); 
    *pUpgrade = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP nsComm4xMailImport::GetImportInterface(const char *pImportType, nsISupports **ppInterface)
{
    NS_ENSURE_ARG_POINTER (pImportType);
    NS_ENSURE_ARG_POINTER (ppInterface);
    *ppInterface = nsnull;
    nsresult    rv;
    
    if (!strcmp(pImportType, "mail")) {
        // create the nsIImportMail interface and return it!
        nsCOMPtr <nsIImportMail> pMail = do_CreateInstance(NS_COMM4XMAILIMPL_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr <nsIImportGeneric> pGeneric;
            nsCOMPtr<nsIImportService> impSvc(do_GetService(NS_IMPORTSERVICE_CONTRACTID, &rv));
            if (NS_SUCCEEDED(rv)) {
                rv = impSvc->CreateNewGenericMail(getter_AddRefs(pGeneric));
                if (NS_SUCCEEDED(rv)) {
                    pGeneric->SetData("mailInterface", pMail);
                    nsXPIDLString name;
                    rv = m_pBundle->GetStringFromID( COMM4XMAILIMPORT_NAME, getter_Copies(name));

                    nsCOMPtr<nsISupportsString> nameString (do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv));
                    NS_ENSURE_SUCCESS(rv,rv);
                    nameString->SetData(name);
                    pGeneric->SetData("name", nameString);
                    rv = pGeneric->QueryInterface(kISupportsIID, (void **)ppInterface);
                }
            }
        }
        return rv;
    }
            
    return NS_ERROR_NOT_AVAILABLE;
}

/////////////////////////////////////////////////////////////////////////////////

ImportComm4xMailImpl::ImportComm4xMailImpl()
{
    m_pBundleProxy = nsnull;
}

nsresult ImportComm4xMailImpl::Initialize()
{
    nsCOMPtr <nsIStringBundleService> pBundleService;
    nsresult rv;
    nsCOMPtr <nsIStringBundle>  pBundle;

    pBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv) && (pBundleService))
        pBundleService->CreateBundle(COMM4XMAIL_MSGS_URL, getter_AddRefs(pBundle));

    nsCOMPtr<nsIProxyObjectManager> proxyMgr(do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv));
    if (NS_SUCCEEDED(rv)) {
        rv = proxyMgr->GetProxyForObject( NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIStringBundle),
                                          pBundle, PROXY_SYNC | PROXY_ALWAYS, getter_AddRefs(m_pBundleProxy));
    }
    return rv;
}

ImportComm4xMailImpl::~ImportComm4xMailImpl()
{
}



NS_IMPL_THREADSAFE_ISUPPORTS1(ImportComm4xMailImpl, nsIImportMail)

NS_IMETHODIMP ImportComm4xMailImpl::GetDefaultLocation(nsIFileSpec **ppLoc, PRBool *found, PRBool *userVerify)
{
    NS_ENSURE_ARG_POINTER(found);
    NS_ENSURE_ARG_POINTER(ppLoc);
    NS_ENSURE_ARG_POINTER(userVerify);
    
    *ppLoc = nsnull;
    *found = PR_FALSE;
    *userVerify = PR_TRUE; 
    return NS_OK;  
}


NS_IMETHODIMP ImportComm4xMailImpl::FindMailboxes(nsIFileSpec *pLoc, nsISupportsArray **ppArray)
{
    NS_ENSURE_ARG_POINTER(pLoc);
    NS_ENSURE_ARG_POINTER(ppArray);

    PRBool exists = PR_FALSE;
    nsresult rv = pLoc->Exists(&exists);
    if (NS_FAILED(rv) || !exists)
        return NS_ERROR_FAILURE;

    // find mail boxes
    rv = m_mail.FindMailboxes(pLoc, ppArray);

    if (NS_FAILED(rv) && *ppArray) {
        NS_RELEASE(*ppArray);
        *ppArray = nsnull;
    }

    return rv;
}

void ImportComm4xMailImpl::ReportStatus( PRInt32 errorNum, nsString& name, nsString *pStream)
{
    if (!pStream) return;
    nsXPIDLString statusStr;
    const PRUnichar * fmtStr = name.get();
    nsresult rv = m_pBundleProxy->FormatStringFromID(errorNum, &fmtStr, 1, getter_Copies(statusStr));
    if (NS_SUCCEEDED (rv)) {
        pStream->Append (statusStr.get());
        pStream->Append( PRUnichar(nsCRT::LF));
    }
    
}

void ImportComm4xMailImpl::SetLogs(nsString& success, nsString& error, PRUnichar **pError, PRUnichar **pSuccess)
{
    if (pError)
        *pError = ToNewUnicode(error);
    if (pSuccess)
        *pSuccess = ToNewUnicode(success);
}

NS_IMETHODIMP ImportComm4xMailImpl::ImportMailbox(nsIImportMailboxDescriptor *pSource, 
                                                nsIFileSpec *pDestination, 
                                                PRUnichar **pErrorLog,
                                                PRUnichar **pSuccessLog,
                                                PRBool *fatalError)
{
    NS_PRECONDITION(pSource != nsnull, "null ptr");
    NS_PRECONDITION(pDestination != nsnull, "null ptr");
    NS_PRECONDITION(fatalError != nsnull, "null ptr");
    
    nsString    success;
    nsString    error;
    if (!pSource || !pDestination || !fatalError) {
        nsXPIDLString errorString;
        m_pBundleProxy->GetStringFromID(COMM4XMAILIMPORT_MAILBOX_BADPARAM, getter_Copies(errorString));
        error = errorString;
        if (fatalError)
            *fatalError = PR_TRUE;
        SetLogs(success, error, pErrorLog, pSuccessLog);
        return NS_ERROR_NULL_POINTER;
    }
      
    nsString    name;
    PRUnichar *    pName;
    if (NS_SUCCEEDED(pSource->GetDisplayName(&pName))) {
        name.Adopt(pName);
    }
    
    PRUint32 mailSize = 0;
    pSource->GetSize(&mailSize);
    if (mailSize == 0) {
        ReportStatus(COMM4XMAILIMPORT_MAILBOX_SUCCESS, name, &success);
        SetLogs(success, error, pErrorLog, pSuccessLog);
        return NS_OK;
    }
    
    PRUint32 index = 0;
    pSource->GetIdentifier(&index);
    nsresult rv = NS_OK;

    m_bytesDone = 0;

    // copy files from 4.x to here.
    nsCOMPtr <nsIFileSpec> inFile;
    if (NS_FAILED(pSource->GetFileSpec(getter_AddRefs(inFile)))) {
        ReportStatus(COMM4XMAILIMPORT_MAILBOX_CONVERTERROR, name, &error);
        SetLogs(success, error, pErrorLog, pSuccessLog);
        return NS_ERROR_FAILURE;
    }

    nsXPIDLCString pSrcPath, pDestPath;;
    inFile->GetNativePath(getter_Copies(pSrcPath));
    pDestination ->GetNativePath(getter_Copies(pDestPath));
    IMPORT_LOG2("ImportComm4xMailImpl::ImportMailbox: Copying folder from '%s' to '%s'.", pSrcPath.get(), pDestPath.get());

    nsCOMPtr <nsIFileSpec> parent;
    if (NS_FAILED (pDestination->GetParent(getter_AddRefs(parent))))
    {
        ReportStatus( COMM4XMAILIMPORT_MAILBOX_CONVERTERROR, name, &error);
        SetLogs( success, error, pErrorLog, pSuccessLog);
        return( NS_ERROR_FAILURE);
    }
    PRBool exists = PR_FALSE;
    pDestination->Exists(&exists);
    if (exists)
        rv = pDestination->Delete(PR_FALSE);
    rv = inFile->CopyToDir(parent);
      
    if (NS_SUCCEEDED(rv)) {
        m_bytesDone = mailSize;
        ReportStatus(COMM4XMAILIMPORT_MAILBOX_SUCCESS, name, &success);
    }
    else {
        ReportStatus(COMM4XMAILIMPORT_MAILBOX_CONVERTERROR, name, &error);
    }

    SetLogs(success, error, pErrorLog, pSuccessLog);

    return rv;
}


NS_IMETHODIMP ImportComm4xMailImpl::GetImportProgress(PRUint32 *pDoneSoFar) 
{ 
    NS_ENSURE_ARG_POINTER(pDoneSoFar);

    *pDoneSoFar = m_bytesDone;
    return NS_OK;
}

NS_IMETHODIMP ImportComm4xMailImpl::TranslateFolderName(const nsAString & aFolderName, nsAString & _retval)
{
  _retval = aFolderName; 
  return NS_OK;
}
