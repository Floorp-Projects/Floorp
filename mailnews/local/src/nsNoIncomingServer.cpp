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

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

#include "nsIFileSpec.h"
#include "nsIPref.h"
#include "nsXPIDLString.h"

#include "nsNoIncomingServer.h"

#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsFileLocations.h"
#include "nsIFileLocator.h"

static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

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
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgLocalMailFolder> localFolder =
        do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // "none" doesn't have an inbox, but it does have a queue (unsent messages)
    localFolder->SetFlagsOnDefaultMailboxes(MSG_FOLDER_FLAG_SENTMAIL |
                                            MSG_FOLDER_FLAG_DRAFTS |
                                            MSG_FOLDER_FLAG_TEMPLATES |
                                            MSG_FOLDER_FLAG_TRASH |
                                            MSG_FOLDER_FLAG_QUEUE);
    return NS_OK;
}	

nsresult nsNoIncomingServer::CopyDefaultMessages(const char *defaultFolderName, nsIFileSpec *path)
{
	nsresult rv;
    PRBool exists;
	if (!defaultFolderName || !path) return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIFileLocator> locator = do_GetService(kFileLocatorCID, &rv);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIFileSpec> defaultMessagesFile;
	rv = locator->GetFileLocation(nsSpecialFileSpec::App_DefaultsFolder50, getter_AddRefs(defaultMessagesFile));
	if (NS_FAILED(rv)) return rv;

	// bin/defaults better exist
    rv = defaultMessagesFile->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) return NS_ERROR_FAILURE;

	// bin/defaults/messenger doesn't have to exist
	rv = defaultMessagesFile->AppendRelativeUnixPath("messenger");
	if (NS_FAILED(rv)) return rv;
    rv = defaultMessagesFile->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) return NS_OK;

	// bin/defaults/messenger/<defaultFolderName> doesn't have to exist
	rv = defaultMessagesFile->AppendRelativeUnixPath(defaultFolderName);
	if (NS_FAILED(rv)) return rv;
    rv = defaultMessagesFile->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) return NS_OK;

	nsCOMPtr<nsIFileSpec> parentDir;
	rv = path->GetParent(getter_AddRefs(parentDir));
	if (NS_FAILED(rv)) return rv;

	rv = defaultMessagesFile->CopyToDir(parentDir);
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

		// if they exist, use the default templates
		rv = CopyDefaultMessages("Templates",path);
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

        return NS_OK;
}

NS_IMETHODIMP nsNoIncomingServer::GetNewMail(nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIURI **aResult)
{
	// do nothing, there is no new mail for this incoming server, ever.
	return NS_OK;
}
