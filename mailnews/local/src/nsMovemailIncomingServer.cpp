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
 * Adam D. Moss <adam@gimp.org>
 */

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

//#include "nsCOMPtr.h"
//#include "nsIPref.h"

#include "nsXPIDLString.h"

#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIFileSpec.h"
#include "nsIPref.h"

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
    NS_INIT_REFCNT();
}

nsMovemailIncomingServer::~nsMovemailIncomingServer()
{
}

/*NS_IMPL_SERVERPREF_BOOL(nsMovemailIncomingServer,
                        AdamIsASillyhead,
                        "adam_is_a_sillyhead")*/

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
    nsresult rv;
    
    nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgLocalMailFolder> localFolder =
        do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    localFolder->SetFlagsOnDefaultMailboxes(MSG_FOLDER_FLAG_INBOX |
                                            MSG_FOLDER_FLAG_SENTMAIL |
                                            MSG_FOLDER_FLAG_DRAFTS |
                                            MSG_FOLDER_FLAG_TEMPLATES |
                                            MSG_FOLDER_FLAG_TRASH |
                                            // hmm?
                                            MSG_FOLDER_FLAG_QUEUE);
    return NS_OK;
}

NS_IMETHODIMP nsMovemailIncomingServer::CreateDefaultMailboxes(nsIFileSpec *path)
{
        nsresult rv;
        PRBool exists;
        if (!path) return NS_ERROR_NULL_POINTER;

        // todo, use a string bundle for this

        rv =path->AppendRelativeUnixPath("Inbox");
        if (NS_FAILED(rv)) return rv;
        rv = path->Exists(&exists);
        if (!exists) {
                rv = path->Touch();
                if (NS_FAILED(rv)) return rv;
        }

        rv =path->SetLeafName("Trash");
        if (NS_FAILED(rv)) return rv;
        rv = path->Exists(&exists);
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
    
    NS_WITH_SERVICE(nsIMovemailService, movemailService,
                    kCMovemailServiceCID, &rv);
    
    if (NS_FAILED(rv)) return rv;
    
    rv = movemailService->GetNewMail(aMsgWindow, aUrlListener,
                                     aMsgFolder, this, aResult);

    return rv;
}        


