/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam D. Moss <adam@gimp.org>
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

#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIFileSpec.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMovemailService.h"
#include "msgCore.h" // pre-compiled headers
#include "nsMovemailIncomingServer.h"


static NS_DEFINE_CID(kCMovemailServiceCID, NS_MOVEMAILSERVICE_CID);


NS_IMPL_ISUPPORTS_INHERITED2(nsMovemailIncomingServer,
                             nsMsgIncomingServer,
                             nsIMovemailIncomingServer,
                             nsILocalMailIncomingServer)

                            

nsMovemailIncomingServer::nsMovemailIncomingServer()
{    
    m_canHaveFilters = PR_TRUE;
}

nsMovemailIncomingServer::~nsMovemailIncomingServer()
{
}

NS_IMETHODIMP
nsMovemailIncomingServer::GetIsSecureServer(PRBool *aIsSecureServer)
{
    NS_ENSURE_ARG_POINTER(aIsSecureServer);
    *aIsSecureServer = PR_FALSE;
    return NS_OK;
}

nsresult
nsMovemailIncomingServer::GetLocalStoreType(char **type)
{
    NS_ENSURE_ARG_POINTER(type);
    *type = nsCRT::strdup("mailbox");
    return NS_OK;
}

NS_IMETHODIMP
nsMovemailIncomingServer::SetFlagsOnDefaultMailboxes()
{
    nsCOMPtr<nsIMsgFolder> rootFolder;
    nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgLocalMailFolder> localFolder =
        do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    localFolder->SetFlagsOnDefaultMailboxes(MSG_FOLDER_FLAG_INBOX |
                                            MSG_FOLDER_FLAG_SENTMAIL |
                                            MSG_FOLDER_FLAG_DRAFTS |
                                            MSG_FOLDER_FLAG_TEMPLATES |
                                            MSG_FOLDER_FLAG_TRASH |
                                            MSG_FOLDER_FLAG_JUNK |
                                            // hmm?
                                            MSG_FOLDER_FLAG_QUEUE);
    return NS_OK;
}

NS_IMETHODIMP nsMovemailIncomingServer::CreateDefaultMailboxes(nsIFileSpec *path)
{
    NS_ENSURE_ARG_POINTER(path);

    nsresult rv = path->AppendRelativeUnixPath("Inbox");
    if (NS_FAILED(rv)) return rv;
    PRBool exists;
    rv = path->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
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

    rv = path->SetLeafName("Unsent Messages");
    if (NS_FAILED(rv)) return rv;
    rv = path->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) {
        rv = path->Touch();
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}


NS_IMETHODIMP
nsMovemailIncomingServer::GetNewMail(nsIMsgWindow *aMsgWindow,
                                     nsIUrlListener *aUrlListener,
                                     nsIMsgFolder *aMsgFolder,
                                     nsIURI **aResult)
{
    nsresult rv;
    
    nsCOMPtr<nsIMovemailService> movemailService = 
             do_GetService(kCMovemailServiceCID, &rv);
    
    if (NS_FAILED(rv)) return rv;
    
    rv = movemailService->GetNewMail(aMsgWindow, aUrlListener,
                                     aMsgFolder, this, aResult);

    return rv;
}        

NS_IMETHODIMP
nsMovemailIncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
    NS_ENSURE_ARG_POINTER(canSearchMessages);
    *canSearchMessages = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP 
nsMovemailIncomingServer::GetAccountManagerChrome(AString& aResult)
{
    aResult = ToNewUnicodeString("am-serverwithnoidentities.xul");
    return NS_OK;
}
