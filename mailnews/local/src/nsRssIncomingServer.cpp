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

#include "nsIRssService.h"
#include "nsRssIncomingServer.h"
#include "nsMsgFolderFlags.h"
#include "nsINewsBlogFeedDownloader.h"

#include "nsIMsgLocalMailFolder.h"
#include "nsIDBFolderInfo.h"

NS_IMPL_ISUPPORTS_INHERITED2(nsRssIncomingServer,
                             nsMsgIncomingServer,
                             nsIRssIncomingServer,
                             nsILocalMailIncomingServer)

nsRssIncomingServer::nsRssIncomingServer() 
{
  m_canHaveFilters = PR_TRUE;
}

nsRssIncomingServer::~nsRssIncomingServer()
{
}

NS_IMETHODIMP nsRssIncomingServer::CreateDefaultMailboxes(nsIFileSpec *path)
{
    NS_ENSURE_ARG_POINTER(path);
    
    // for RSS, all we have is Trash
    // XXX or should we be use Local Folders/Trash?
    nsresult rv = path->AppendRelativeUnixPath("Trash");
    if (NS_FAILED(rv)) 
        return rv;
    
    PRBool exists;
    rv = path->Exists(&exists);
    if (!exists) 
    {
        rv = path->Touch();
        if (NS_FAILED(rv)) 
            return rv;
    }
    
    return NS_OK;
}

NS_IMETHODIMP nsRssIncomingServer::SetFlagsOnDefaultMailboxes()
{
    nsCOMPtr<nsIMsgFolder> rootFolder;
    nsresult rv = GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIMsgLocalMailFolder> localFolder =
        do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    localFolder->SetFlagsOnDefaultMailboxes(MSG_FOLDER_FLAG_TRASH);
    return NS_OK;
}

NS_IMETHODIMP nsRssIncomingServer::GetNewMail(nsIMsgWindow *aMsgWindow, nsIUrlListener *aUrlListener, nsIMsgFolder *aFolder, nsIURI **_retval)
{
  NS_ENSURE_ARG_POINTER(aFolder);
  
  PRBool valid = PR_FALSE;
  nsCOMPtr <nsIMsgDatabase> db;
  nsresult rv;
  nsCOMPtr <nsINewsBlogFeedDownloader> rssDownloader = do_GetService("@mozilla.org/newsblog-feed-downloader;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aFolder->GetMsgDatabase(aMsgWindow, getter_AddRefs(db));
  if (NS_SUCCEEDED(rv) && db)
  {
    rv = db->GetSummaryValid(&valid);
    NS_ASSERTION(valid, "db is invalid");
    if (valid)
    {
      nsCOMPtr <nsIDBFolderInfo> folderInfo;
      rv = db->GetDBFolderInfo(getter_AddRefs(folderInfo));
      if (folderInfo)
      {
        nsXPIDLCString url;
        folderInfo->GetCharPtrProperty("feedUrl", getter_Copies(url));

        rv = rssDownloader->DownloadFeed(url.get(), 
          aFolder, PR_FALSE, "Slashdot", aUrlListener, aMsgWindow);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsRssIncomingServer::GetLocalStoreType(char **type)
{
    NS_ENSURE_ARG_POINTER(type);
    *type = strdup("mailbox");
    return NS_OK;
}
