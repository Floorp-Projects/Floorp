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

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

#include "nsCOMPtr.h"
#include "nsIPref.h"

#include "nsXPIDLString.h"
#include "nsIStringBundle.h"

#include "nsIPop3IncomingServer.h"
#include "nsPop3IncomingServer.h"
#include "nsIPop3Service.h"
#include "nsMsgBaseCID.h"
#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIFileSpec.h"
#include "nsPop3Protocol.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgLocalMailFolder.h"

static NS_DEFINE_CID(kCPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define INBOX_NAME "Inbox"

nsrefcnt nsPop3IncomingServer::gInstanceCount	= 0;

PRUnichar *nsPop3IncomingServer::kInboxName = 0;
PRUnichar *nsPop3IncomingServer::kTrashName = 0;
PRUnichar *nsPop3IncomingServer::kSentName = 0;
PRUnichar *nsPop3IncomingServer::kDraftsName = 0;
PRUnichar *nsPop3IncomingServer::kTemplatesName = 0;
PRUnichar *nsPop3IncomingServer::kUnsentName = 0;

NS_IMPL_ISUPPORTS_INHERITED2(nsPop3IncomingServer,
                             nsMsgIncomingServer,
                             nsIPop3IncomingServer,
			     nsILocalMailIncomingServer)

nsPop3IncomingServer::nsPop3IncomingServer()
{    
    NS_INIT_REFCNT();
    m_capabilityFlags = 
        POP3_AUTH_LOGIN_UNDEFINED |
        POP3_XSENDER_UNDEFINED |
        POP3_GURL_UNDEFINED |
        POP3_UIDL_UNDEFINED |
        POP3_TOP_UNDEFINED |
        POP3_XTND_XLST_UNDEFINED;

    if (gInstanceCount++ == 0) {
        initializeStrings();

    }

	m_canHaveFilters = PR_TRUE;
}

nsPop3IncomingServer::~nsPop3IncomingServer()
{
    if (--gInstanceCount == 0) {
        CRTFREEIF(kInboxName);
        CRTFREEIF(kTrashName);
        CRTFREEIF(kSentName);
        CRTFREEIF(kDraftsName);
        CRTFREEIF(kTemplatesName);
        CRTFREEIF(kUnsentName);
    }
}

nsresult
nsPop3IncomingServer::initializeStrings()
{
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
        do_GetService(kStringBundleServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle("chrome://messenger/locale/messenger.properties",
                                     getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, rv);
    
    bundle->GetStringFromName(NS_LITERAL_STRING("inboxFolderName").get(),
                              &kInboxName);
    bundle->GetStringFromName(NS_LITERAL_STRING("trashFolderName").get(),
                              &kTrashName);
    bundle->GetStringFromName(NS_LITERAL_STRING("sentFolderName").get(),
                              &kSentName);
    bundle->GetStringFromName(NS_LITERAL_STRING("draftsFolderName").get(),
                              &kDraftsName);
    bundle->GetStringFromName(NS_LITERAL_STRING("templatesFolderName").get(),
                              &kTemplatesName);
    bundle->GetStringFromName(NS_LITERAL_STRING("unsentFolderName").get(),
                              &kUnsentName);
    return NS_OK;
}


NS_IMPL_SERVERPREF_BOOL(nsPop3IncomingServer,
                        LeaveMessagesOnServer,
                        "leave_on_server")

NS_IMPL_SERVERPREF_BOOL(nsPop3IncomingServer,
                        DeleteMailLeftOnServer,
                        "delete_mail_left_on_server")

NS_IMPL_SERVERPREF_BOOL(nsPop3IncomingServer,
                        AuthLogin,
                        "auth_login")

NS_IMPL_SERVERPREF_BOOL(nsPop3IncomingServer,
                        DotFix,
                        "dot_fix")

nsresult 
nsPop3IncomingServer::GetPop3CapabilityFlags(PRUint32 *flags)
{
    *flags = m_capabilityFlags;
    return NS_OK;
}

nsresult
nsPop3IncomingServer::SetPop3CapabilityFlags(PRUint32 flags)
{
    m_capabilityFlags = flags;
    return NS_OK;
}

nsresult
nsPop3IncomingServer::GetLocalStoreType(char **type)
{
    NS_ENSURE_ARG_POINTER(type);
    *type = nsCRT::strdup("mailbox");
    return NS_OK;
}

NS_IMETHODIMP nsPop3IncomingServer::PerformBiff()
{
	nsresult rv;

	nsCOMPtr<nsIPop3Service> pop3Service(do_GetService(kCPop3ServiceCID, &rv));
    if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIMsgFolder> inbox;
	nsCOMPtr<nsIFolder> rootFolder;
	rv = GetRootFolder(getter_AddRefs(rootFolder));
	if(NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder);
		if(rootMsgFolder)
		{
			PRUint32 numFolders;
			rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1,
                                                   &numFolders,
                                                   getter_AddRefs(inbox));
            if (NS_FAILED(rv) || numFolders != 1) return rv;
		}
	}

	//Biff just needs to give status in one of the windows. so do it in topmost window.
	nsCOMPtr<nsIMsgMailSession> mailSession = 
	         do_GetService(kCMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return rv;

	
	nsCOMPtr<nsIMsgWindow> msgWindow;

	rv = mailSession->GetTopmostMsgWindow(getter_AddRefs(msgWindow));
	if(NS_SUCCEEDED(rv))
    {
        PRBool downloadOnBiff = PR_FALSE;
        rv = GetDownloadOnBiff(&downloadOnBiff);
        if (downloadOnBiff)
        {
            nsCOMPtr <nsIMsgLocalMailFolder> localInbox = do_QueryInterface(inbox, &rv);
            PRBool parsingInbox;
            if (localInbox && NS_SUCCEEDED(rv))
            {
              rv = localInbox->GetParsingInbox(&parsingInbox);
              NS_ENSURE_SUCCESS(rv,rv);
              if (!parsingInbox)
                rv = pop3Service->GetNewMail(msgWindow, nsnull, inbox, this, nsnull);
              else
                rv = localInbox->SetCheckForNewMessagesAfterParsing(PR_TRUE);
            }
        }
        else
            rv = pop3Service->CheckForNewMail(msgWindow, nsnull, inbox, this,
                                              nsnull);
    }

	return NS_OK;
}

NS_IMETHODIMP
nsPop3IncomingServer::SetFlagsOnDefaultMailboxes()
{
    nsresult rv;
    
    nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgLocalMailFolder> localFolder =
        do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // pop3 gets an inbox, but no queue (unsent messages)
    localFolder->SetFlagsOnDefaultMailboxes(MSG_FOLDER_FLAG_INBOX |
                                            MSG_FOLDER_FLAG_SENTMAIL |
                                            MSG_FOLDER_FLAG_DRAFTS |
                                            MSG_FOLDER_FLAG_TEMPLATES |
                                            MSG_FOLDER_FLAG_TRASH);
    return NS_OK;
}
    

NS_IMETHODIMP nsPop3IncomingServer::CreateDefaultMailboxes(nsIFileSpec *path)
{
	nsresult rv;
	PRBool exists;
	if (!path) return NS_ERROR_NULL_POINTER;

	// todo, use a string bundle for this
        rv =path->AppendRelativeUnixPath(INBOX_NAME);
	if (NS_FAILED(rv)) return rv;
	rv = path->Exists(&exists);
	if (!exists) {
		rv = path->Touch();
		if (NS_FAILED(rv)) return rv;
	}
	
	rv = path->SetLeafName("Trash");
	if (NS_FAILED(rv)) return rv;
	rv = path->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) {
		rv = path->Touch();
		if (NS_FAILED(rv)) return rv;
	}

	rv = path->SetLeafName("Sent");
	if (NS_FAILED(rv)) return rv;
	rv = path->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) {
		rv = path->Touch();
		if (NS_FAILED(rv)) return rv;
	}
	
	rv = path->SetLeafName("Drafts");
	if (NS_FAILED(rv)) return rv;
	rv = path->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) {
		rv = path->Touch();
		if (NS_FAILED(rv)) return rv;
	}
	
	rv = path->SetLeafName("Templates");
	if (NS_FAILED(rv)) return rv;
	rv = path->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) {
		rv = path->Touch();
		if (NS_FAILED(rv)) return rv;
	}
	
	return NS_OK;
}

NS_IMETHODIMP nsPop3IncomingServer::GetNewMail(nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIMsgFolder *inbox, nsIURI **aResult)
{
	nsresult rv;

	nsCOMPtr<nsIPop3Service> pop3Service = do_GetService(kCPop3ServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = pop3Service->GetNewMail(aMsgWindow, aUrlListener, inbox, this, aResult);
	return rv;
}

NS_IMETHODIMP
nsPop3IncomingServer::GetDownloadMessagesAtStartup(PRBool *getMessagesAtStartup)
{
    // GetMessagese is not automatically done for pop servers at startup.
    // We need to trigger that action. Return true.
    *getMessagesAtStartup = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsPop3IncomingServer::GetCanBeDefaultServer(PRBool *canBeDefaultServer)
{
    *canBeDefaultServer = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsPop3IncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
    NS_ENSURE_ARG_POINTER(canSearchMessages);
    *canSearchMessages = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsPop3IncomingServer::GetOfflineSupportLevel(PRInt32 *aSupportLevel)
{
    NS_ENSURE_ARG_POINTER(aSupportLevel);
    nsresult rv;
    
    rv = GetIntValue("offline_support_level", aSupportLevel);
    if (*aSupportLevel != OFFLINE_SUPPORT_LEVEL_UNDEFINED) return rv;
    
    // set default value
    *aSupportLevel = OFFLINE_SUPPORT_LEVEL_NONE;
    return NS_OK;
}
