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

#include "msgCore.h" // pre-compiled headers

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"

#include "nsIFileSpec.h"
#include "nsXPIDLString.h"

#include "nsNoIncomingServer.h"

#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgMailSession.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgAccountManager.h"
#include "nsIPop3IncomingServer.h"

NS_IMPL_ISUPPORTS_INHERITED2(nsNoIncomingServer,
                            nsMsgIncomingServer,
                            nsINoIncomingServer,
			    nsILocalMailIncomingServer)

                            

nsNoIncomingServer::nsNoIncomingServer()
{    
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
nsNoIncomingServer::GetAccountManagerChrome(nsAString& aResult)
{
  aResult.AssignLiteral("am-serverwithnoidentities.xul");
  return NS_OK;
}

NS_IMETHODIMP 
nsNoIncomingServer::SetFlagsOnDefaultMailboxes()
{
  nsCOMPtr<nsIMsgFolder> rootFolder;
  nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgLocalMailFolder> localFolder =
      do_QueryInterface(rootFolder, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 mailboxFlags = MSG_FOLDER_FLAG_SENTMAIL |
                          MSG_FOLDER_FLAG_DRAFTS |
                          MSG_FOLDER_FLAG_TEMPLATES |
                          MSG_FOLDER_FLAG_TRASH |
                          MSG_FOLDER_FLAG_JUNK |
                          MSG_FOLDER_FLAG_QUEUE;
 
  // "none" may have a inbox if it is getting deferred to.
  PRBool isDeferredTo;
  if (NS_SUCCEEDED(GetIsDeferredTo(&isDeferredTo)) && isDeferredTo)
    mailboxFlags |= MSG_FOLDER_FLAG_INBOX;

  localFolder->SetFlagsOnDefaultMailboxes(mailboxFlags);

  return NS_OK;
}	

NS_IMETHODIMP nsNoIncomingServer::CopyDefaultMessages(const char *folderNameOnDisk, nsIFileSpec *parentDir)
{
  nsresult rv;
  PRBool exists;
  if (!folderNameOnDisk || !parentDir) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Get defaults directory for messenger files. MailSession service appends 'messenger' to the 
  // the app defaults folder and returns it. Locale will be added to the path, if there is one.
  nsCOMPtr<nsIFile> defaultMessagesFile;
  rv = mailSession->GetDataFilesDir("messenger", getter_AddRefs(defaultMessagesFile));
  NS_ENSURE_SUCCESS(rv,rv);
  
  // check if bin/defaults/messenger/<folderNameOnDisk> 
  // (or bin/defaults/messenger/<locale>/<folderNameOnDisk> if we had a locale provide) exists.
  // it doesn't have to exist.  if it doesn't, return
  rv = defaultMessagesFile->AppendNative(nsDependentCString(folderNameOnDisk));
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
    rv = testDir->AppendNative(nsDependentCString(folderNameOnDisk));
    if (NS_FAILED(rv)) return rv;
    rv = testDir->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
  }
  
  // if it exists add to the end, else copy
  if (exists) 
  {
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
    rv = defaultMessagesFile->CopyTo(localParentDir, EmptyString());
    if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}


NS_IMETHODIMP nsNoIncomingServer::CreateDefaultMailboxes(nsIFileSpec *path)
{
  nsresult rv;
  if (!path) 
    return NS_ERROR_NULL_POINTER;
  
  // notice, no Inbox, unless we're deferred to...
   // need to have a leaf to start with
  rv = path->AppendRelativeUnixPath("Trash"); 
  PRBool isDeferredTo;
  if (NS_SUCCEEDED(GetIsDeferredTo(&isDeferredTo)) && isDeferredTo)
    CreateLocalFolder(path, "Inbox");
  CreateLocalFolder(path, "Trash");
  if (NS_FAILED(rv)) return rv;
  
  rv = CreateLocalFolder(path, "Sent");
  if (NS_FAILED(rv)) return rv;
  rv = CreateLocalFolder(path, "Drafts");
  if (NS_FAILED(rv)) return rv;
  
  // copy the default templates into the Templates folder
  nsCOMPtr<nsIFileSpec> parentDir;
  rv = path->GetParent(getter_AddRefs(parentDir));
  if (NS_FAILED(rv)) return rv;
  rv = CopyDefaultMessages("Templates",parentDir);
  if (NS_FAILED(rv)) return rv;
  
  rv = CreateLocalFolder(path, "Drafts");
  if (NS_FAILED(rv)) return rv;
  
  (void ) CreateLocalFolder(path, "Unsent Messages");
  return NS_OK;
}

NS_IMETHODIMP
nsNoIncomingServer::GetNewMail(nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIMsgFolder *aInbox, nsIURI **aResult)
{
  nsCOMPtr <nsISupportsArray> deferredServers;
  nsresult rv = GetDeferredServers(this, getter_AddRefs(deferredServers));
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 count;
  deferredServers->Count(&count);
  if (count > 0)
  {
    nsCOMPtr <nsIPop3IncomingServer> firstServer(do_QueryElementAt(deferredServers, 0));
    if (firstServer)
    {
      rv = firstServer->DownloadMailFromServers(deferredServers, aMsgWindow,
                              aInbox, 
                              aUrlListener);
    }
  }
  // listener might be counting on us to send a notification.
  else if (aUrlListener)
    aUrlListener->OnStopRunningUrl(nsnull, NS_OK);
  return rv;
}


NS_IMETHODIMP
nsNoIncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
  NS_ENSURE_ARG_POINTER(canSearchMessages);
  *canSearchMessages = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP 
nsNoIncomingServer::GetServerRequiresPasswordForBiff(PRBool *aServerRequiresPasswordForBiff)
{
  NS_ENSURE_ARG_POINTER(aServerRequiresPasswordForBiff);
  *aServerRequiresPasswordForBiff = PR_FALSE;  // for local folders, we don't require a password
  return NS_OK;
}


