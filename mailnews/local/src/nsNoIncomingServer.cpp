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
#include "nsIChromeRegistry.h" 
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"

static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);

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

NS_IMETHODIMP nsNoIncomingServer::CopyDefaultMessages(const char *folderNameOnDisk, nsIFileSpec *parentDir)
{
	nsresult rv;
    PRBool exists;
	if (!folderNameOnDisk || !parentDir) return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIFile> defaultMessagesFile;
	rv = NS_GetSpecialDirectory(NS_APP_DEFAULTS_50_DIR, getter_AddRefs(defaultMessagesFile));
	if (NS_FAILED(rv)) return rv;

	// bin/defaults better exist
    rv = defaultMessagesFile->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) return NS_ERROR_FAILURE;

	// bin/defaults/messenger doesn't have to exist
    // if not, return.
	rv = defaultMessagesFile->Append("messenger");
	if (NS_FAILED(rv)) return rv;
    rv = defaultMessagesFile->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) return NS_OK;

	// test if there is a locale provider
	// this code stolen from nsMsgServiceProvider.cpp
  nsCOMPtr<nsIChromeRegistry> chromeRegistry = do_GetService(kChromeRegistryCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsXPIDLString lc_name;
    nsAutoString tmpstr; tmpstr.AssignWithConversion("global-region");
    rv = chromeRegistry->GetSelectedLocale(tmpstr.get(), getter_Copies(lc_name));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString localeStr(lc_name);
      if (!localeStr.IsEmpty()) {
        nsCOMPtr<nsIFile> tmpdataFilesDir;
        rv = defaultMessagesFile->Clone(getter_AddRefs(tmpdataFilesDir));
        NS_ENSURE_SUCCESS(rv,rv);
        rv = tmpdataFilesDir->AppendUnicode(lc_name);
        NS_ENSURE_SUCCESS(rv,rv);
        rv = tmpdataFilesDir->Exists(&exists);
        NS_ENSURE_SUCCESS(rv,rv);
        if (exists && (const PRUnichar *)lc_name) {
            // use locale provider instead
            rv = defaultMessagesFile->AppendUnicode(lc_name);            
  	        NS_ENSURE_SUCCESS(rv,rv);
        }
      }
    }
  }

	// check if bin/defaults/messenger/<folderNameOnDisk> 
	// (or bin/defaults/messenger/<locale>/<folderNameOnDisk> if we had a locale provide) exists.
	// it doesn't have to exist.  if it doesn't, return
	rv = defaultMessagesFile->Append(folderNameOnDisk);
	if (NS_FAILED(rv)) return rv;
    rv = defaultMessagesFile->Exists(&exists);
	if (NS_FAILED(rv)) return rv;
	if (!exists) return NS_OK;
	
	// Make an nsILocalFile of the parentDir
	nsFileSpec parentDirSpec;
	nsCOMPtr<nsILocalFile> localParentDir;
	
	rv = parentDir->GetFileSpec(&parentDirSpec);
	if (NS_FAILED(rv)) return rv;
	rv = NS_FileSpecToIFile(&parentDirSpec, getter_AddRefs(localParentDir));
	if (NS_FAILED(rv)) return rv;

	// check if parentDir/<folderNameOnDisk> exists
  {
    nsCOMPtr<nsIFile> testDir;
    rv = localParentDir->Clone(getter_AddRefs(testDir));
    if (NS_FAILED(rv)) return rv;
    rv = testDir->Append(folderNameOnDisk);
    if (NS_FAILED(rv)) return rv;
    rv = testDir->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
  }

	// if it exists add to the end, else copy
	if (exists) {
#ifdef DEBUG_sspitzer
		printf("append default %s\n",folderNameOnDisk);
#endif
		// todo for bug #1181
		// open folderFile, seek to end
		// read defaultMessagesFile, write to folderFile
	}
	else {
#ifdef DEBUG_sspitzer
		printf("copy default %s\n",folderNameOnDisk);
#endif
		rv = defaultMessagesFile->CopyTo(localParentDir, nsnull);
		if (NS_FAILED(rv)) return rv;
	}
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

		// copy the default templates into the Templates folder
		nsCOMPtr<nsIFileSpec> parentDir;
		rv = path->GetParent(getter_AddRefs(parentDir));
		if (NS_FAILED(rv)) return rv;
		rv = CopyDefaultMessages("Templates",parentDir);
        if (NS_FAILED(rv)) return rv;

		// we may not have had any default templates.  if so
		// we still want to create the Templates folder
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

        return NS_OK;
}

NS_IMETHODIMP nsNoIncomingServer::GetNewMail(nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIMsgFolder *aInbox, nsIURI **aResult)
{
  // listener might be counting on us to send a notification.
  if (aUrlListener)
    aUrlListener->OnStopRunningUrl(nsnull, NS_OK);
  // do nothing, there is no new mail for this incoming server, ever.
  return NS_OK;
}

// the "none" server does not support filters, because
// it doesn't support incoming messages!
NS_IMETHODIMP
nsNoIncomingServer::GetFilterList(nsIMsgFilterList **aResult)
{
    *aResult = nsnull;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNoIncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
    NS_ENSURE_ARG_POINTER(canSearchMessages);
    *canSearchMessages = PR_TRUE;
    return NS_OK;
}

