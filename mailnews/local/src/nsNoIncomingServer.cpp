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
 */

#include "msgCore.h" // pre-compiled headers
#include "nsNoIncomingServer.h"

#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIFileSpec.h"
#include "nsIPref.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsXPIDLString.h"


NS_IMPL_ISUPPORTS_INHERITED2(nsNoIncomingServer,
                            nsMsgIncomingServer,
                            nsINoIncomingServer,
			    nsILocalMailIncomingServer);

                            

nsNoIncomingServer::nsNoIncomingServer()
{    
    NS_INIT_REFCNT();
}

nsNoIncomingServer::~nsNoIncomingServer()
{
}

nsresult
nsNoIncomingServer::GetLocalStoreType(char **type)
{
    NS_ENSURE_ARG_POINTER(type);
    *type = nsCRT::strdup("mailbox");
    return NS_OK;
}

NS_IMETHODIMP 
nsNoIncomingServer::SetFlagsOnDefaultMailboxes()
{
	nsresult rv;

	nsCOMPtr<nsIFolder> rootFolder;
	rv = GetRootFolder(getter_AddRefs(rootFolder));
	if (NS_FAILED(rv)) return rv;
	if (!rootFolder) return NS_ERROR_FAILURE;

	nsCOMPtr <nsIFolder> folder;
	nsCOMPtr <nsIMsgFolder> msgFolder;

	// notice, no Inbox

	rv = rootFolder->FindSubFolder("Sent", getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
 	msgFolder = do_QueryInterface(folder);
	if (!msgFolder) return NS_ERROR_FAILURE;
	rv = msgFolder->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
	if (NS_FAILED(rv)) return rv;

	rv = rootFolder->FindSubFolder("Drafts", getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
 	msgFolder = do_QueryInterface(folder);
	if (!msgFolder) return NS_ERROR_FAILURE;
	rv = msgFolder->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
	if (NS_FAILED(rv)) return rv;
	
	rv = rootFolder->FindSubFolder("Templates", getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
 	msgFolder = do_QueryInterface(folder);
	if (!msgFolder) return NS_ERROR_FAILURE;
	rv = msgFolder->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
	if (NS_FAILED(rv)) return rv;

	rv = rootFolder->FindSubFolder("Trash", getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
 	msgFolder = do_QueryInterface(folder);
	if (!msgFolder) return NS_ERROR_FAILURE;
	rv = msgFolder->SetFlag(MSG_FOLDER_FLAG_TRASH);
	if (NS_FAILED(rv)) return rv;

	rv = rootFolder->FindSubFolder("Unsent Messages", getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
 	msgFolder = do_QueryInterface(folder);
	if (!msgFolder) return NS_ERROR_FAILURE;
	rv = msgFolder->SetFlag(MSG_FOLDER_FLAG_QUEUE);
	if (NS_FAILED(rv)) return rv;
	
	return NS_OK;
}	

NS_IMETHODIMP nsNoIncomingServer::CreateDefaultMailboxes(nsIFileSpec *path)
{
        nsresult rv;
        PRBool exists;
        if (!path) return NS_ERROR_NULL_POINTER;

        // todo, use a string bundle for these


		// notice, no Inbox
        rv = path->AppendRelativeUnixPath("Trash");
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

NS_IMETHODIMP nsNoIncomingServer::GetNewMail(nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIURI **aResult)
{
	// do nothing, there is no new mail for this incoming server, ever.
	return NS_OK;
}
