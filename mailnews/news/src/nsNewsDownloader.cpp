/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#include "msgCore.h"
#include "nntpCore.h"
#include "nsXPIDLString.h"
#include "nsIMsgNewsFolder.h"
#include "nsIStringBundle.h"
#include "nsNewsDownloader.h"
#include "nsINntpService.h"
#include "nsMsgNewsCID.h"
#include "nsIMsgSearchSession.h"
#include "nsIMsgSearchTerm.h"
#include "nsIMsgSearchValidityManager.h"
#include "nsRDFCID.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgFolderFlags.h"
#include "nsIRequestObserver.h"
// This file contains the news article download state machine.

static NS_DEFINE_CID(kNntpServiceCID,	NS_NNTPSERVICE_CID);


// if pIds is not null, download the articles whose id's are passed in. Otherwise,
// which articles to download is determined by nsNewsDownloader object,
// or subclasses thereof. News can download marked objects, for example.
nsresult nsNewsDownloader::DownloadArticles(nsIMsgWindow *window, nsIMsgFolder *folder, nsMsgKeyArray *pIds)
{
  if (pIds != nsnull)
    m_keysToDownload.InsertAt(0, pIds);
  
  if (m_keysToDownload.GetSize() > 0)
    m_downloadFromKeys = PR_TRUE;
  
  m_folder = folder;
  m_window = window;
  
  PRBool headersToDownload = GetNextHdrToRetrieve();
  // should we have a special error code for failure here?
  return (headersToDownload) ? DownloadNext(PR_TRUE) : NS_ERROR_FAILURE;
}

/* Saving news messages
 */

NS_IMPL_ISUPPORTS2(nsNewsDownloader, nsIUrlListener, nsIMsgSearchNotify)

nsNewsDownloader::nsNewsDownloader(nsIMsgWindow *window, nsIMsgDatabase *msgDB, nsIUrlListener *listener)
{
  m_numwrote = 0;
  m_downloadFromKeys = PR_FALSE;
  m_newsDB = msgDB;
  m_abort = PR_FALSE;
  m_listener = listener;
  m_window = window;
  // not the perfect place for this, but I think it will work.
  if (m_window)
    m_window->SetStopped(PR_FALSE);
  NS_INIT_REFCNT();
}

nsNewsDownloader::~nsNewsDownloader()
{
	if (m_listener)
	  m_listener->OnStopRunningUrl(/* don't have a url */nsnull, m_status);
	if (m_newsDB)
	{
		m_newsDB->Commit(nsMsgDBCommitType::kLargeCommit);
		m_newsDB = nsnull;
	}
}

NS_IMETHODIMP nsNewsDownloader::OnStartRunningUrl(nsIURI* url)
{
  return NS_OK;
}

NS_IMETHODIMP nsNewsDownloader::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  PRBool stopped = PR_FALSE;
  if (m_window)
    m_window->GetStopped(&stopped);
  if (stopped)
    exitCode = NS_BINDING_ABORTED;

 nsresult rv = exitCode;
  if (NS_SUCCEEDED(exitCode) || exitCode == NS_MSG_NEWS_ARTICLE_NOT_FOUND)
    rv = DownloadNext(PR_FALSE);

  return rv;
}

nsresult nsNewsDownloader::DownloadNext(PRBool firstTimeP)
{
  nsresult rv;
  if (!firstTimeP)
  {
    PRBool moreHeaders = GetNextHdrToRetrieve();
    if (!moreHeaders)
    {
      if (m_listener)
        m_listener->OnStopRunningUrl(nsnull, NS_OK);
      return NS_OK;
    }
  }
  StartDownload();
  m_wroteAnyP = PR_FALSE;
  nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID,&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return nntpService->FetchMessage(m_folder, m_keyToDownload, m_window, nsnull, this, nsnull);
}

PRBool DownloadNewsArticlesToOfflineStore::GetNextHdrToRetrieve()
{
  nsresult rv;

  if (m_downloadFromKeys)
    return nsNewsDownloader::GetNextHdrToRetrieve();

  if (m_headerEnumerator == nsnull)
    rv = m_newsDB->EnumerateMessages(getter_AddRefs(m_headerEnumerator));

  PRBool hasMore = PR_FALSE;

  while (NS_SUCCEEDED(rv = m_headerEnumerator->HasMoreElements(&hasMore)) && hasMore) 
  {
    nsCOMPtr <nsISupports> supports;
    rv = m_headerEnumerator->GetNext(getter_AddRefs(supports));
    m_newsHeader = do_QueryInterface(supports);
    NS_ENSURE_SUCCESS(rv,rv);
    PRUint32 hdrFlags;
    m_newsHeader->GetFlags(&hdrFlags);
    if (hdrFlags & MSG_FLAG_MARKED)
    {
      m_newsHeader->GetMessageKey(&m_keyToDownload);
      break;
    }
    else
    {
      m_newsHeader = nsnull;
    }
  }
  return hasMore;
}

void nsNewsDownloader::Abort() {}
void nsNewsDownloader::Complete() {}

PRBool nsNewsDownloader::GetNextHdrToRetrieve()
{
  nsresult rv;
  if (m_downloadFromKeys)
  {
    if (m_numwrote >= (PRInt32) m_keysToDownload.GetSize())
      return PR_FALSE;
    m_keyToDownload = m_keysToDownload.GetAt(m_numwrote++);
#ifdef DEBUG_bienvenu
    //		XP_Trace("downloading %ld index = %ld\n", m_keyToDownload, m_numwrote);
#endif
    nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(m_folder);
    //		PRInt32 stringID = (newsFolder ? MK_MSG_RETRIEVING_ARTICLE_OF : MK_MSG_RETRIEVING_MESSAGE_OF);
    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleService->CreateBundle(NEWS_MSGS_URL, getter_AddRefs(bundle));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsAutoString firstStr;
    firstStr.AppendInt(m_numwrote);
    nsAutoString totalStr;
    totalStr.AppendInt(m_keysToDownload.GetSize());
    nsXPIDLString prettiestName;
    nsXPIDLString statusString;
    
    m_folder->GetPrettiestName(getter_Copies(prettiestName));
    
    const PRUnichar *formatStrings[3] = { firstStr.get(), totalStr.get(), (const PRUnichar *) prettiestName };
    rv = bundle->FormatStringFromName(NS_LITERAL_STRING("downloadingArticlesForOffline").get(), formatStrings, 3, getter_Copies(statusString));
    NS_ENSURE_SUCCESS(rv, rv);
    // ### TODO set status string on window?
    PRInt32 percent;
    percent = (100 * m_numwrote) / (PRInt32) m_keysToDownload.GetSize();
    // FE_SetProgressBarPercent (m_context, percent);
    ShowProgress(statusString, percent);
    return PR_TRUE;
  }
  NS_ASSERTION(PR_FALSE, "shouldn't get here if we're not downloading from keys.");
  return PR_FALSE;	// shouldn't get here if we're not downloading from keys.
}

nsresult nsNewsDownloader::ShowProgress(const PRUnichar *progressString, PRInt32 percent)
{
  if (!m_statusFeedback)
  {
    if (m_window)
      m_window->GetStatusFeedback(getter_AddRefs(m_statusFeedback));
  }
  if (m_statusFeedback)
  {
    m_statusFeedback->ShowStatusString(progressString);
    m_statusFeedback->ShowProgress(percent);
  }
  return NS_OK;
}

NS_IMETHODIMP DownloadNewsArticlesToOfflineStore::OnStartRunningUrl(nsIURI* url)
{
  return NS_OK;
}


NS_IMETHODIMP DownloadNewsArticlesToOfflineStore::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
	m_status = exitCode;
	if (m_newsHeader != nsnull)
	{
#ifdef DEBUG_bienvenu
//		XP_Trace("finished retrieving %ld\n", m_newsHeader->GetMessageKey());
#endif
		if (m_newsDB)
		{
      nsMsgKey msgKey;
      m_newsHeader->GetMessageKey(&msgKey);
			m_newsDB->MarkMarked(msgKey, PR_FALSE, nsnull);
		}
	}	
	m_newsHeader = nsnull;
	return nsNewsDownloader::OnStopRunningUrl(url, exitCode);
}

int DownloadNewsArticlesToOfflineStore::FinishDownload()
{
	return 0;
}


NS_IMETHODIMP nsNewsDownloader::OnSearchHit(nsIMsgDBHdr *header, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(header);

  
  PRUint32 msgFlags;
  header->GetFlags(&msgFlags);
	// only need to download articles we don't already have...
	if (! (msgFlags & MSG_FLAG_OFFLINE))
	{
    nsMsgKey key;
    header->GetMessageKey(&key);
		m_keysToDownload.Add(key);
#ifdef HAVE_PORT
		char *statusTemplate = XP_GetString (MK_MSG_ARTICLES_TO_RETRIEVE);
		char *statusString = PR_smprintf (statusTemplate,  (long) newsArticleState->m_keysToDownload.GetSize());
		if (statusString)
		{
			FE_Progress (newsArticleState->m_context, statusString);
			XP_FREE(statusString);
		}
#endif
	}
  return NS_OK;
}

NS_IMETHODIMP nsNewsDownloader::OnSearchDone(nsresult status)
{
  if (m_keysToDownload.GetSize() == 0)
  {
    if (m_listener)
      return m_listener->OnStopRunningUrl(nsnull, NS_OK);
  }
  nsresult rv = DownloadArticles(m_window, m_folder, &m_keysToDownload);
  if (!NS_SUCCEEDED(rv))
    if (m_listener)
      m_listener->OnStopRunningUrl(nsnull, rv);

  return rv;
}
NS_IMETHODIMP nsNewsDownloader::OnNewSearch()
{
  return NS_OK;
}

int DownloadNewsArticlesToOfflineStore::StartDownload()
{
	m_newsDB->GetMsgHdrForKey(m_keyToDownload, getter_AddRefs(m_newsHeader));
	return 0;
}

DownloadNewsArticlesToOfflineStore::DownloadNewsArticlesToOfflineStore(nsIMsgWindow *window, nsIMsgDatabase *db, nsIUrlListener *listener)
	: nsNewsDownloader(window, db, listener)
{
	m_newsDB = db;
}

DownloadNewsArticlesToOfflineStore::~DownloadNewsArticlesToOfflineStore()
{
}

DownloadMatchingNewsArticlesToNewsDB::DownloadMatchingNewsArticlesToNewsDB
	(nsIMsgWindow *window, nsIMsgFolder *folder, nsIMsgDatabase *newsDB,
	 nsIUrlListener *listener) :
	 DownloadNewsArticlesToOfflineStore(window, newsDB, listener)
{
	m_window = window;
	m_folder = folder;
	m_newsDB = newsDB;
	m_downloadFromKeys = PR_TRUE;	// search term matching means downloadFromKeys.
}

DownloadMatchingNewsArticlesToNewsDB::~DownloadMatchingNewsArticlesToNewsDB()
{
}

static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

NS_IMPL_ISUPPORTS1(nsMsgDownloadAllNewsgroups, nsIUrlListener)


nsMsgDownloadAllNewsgroups::nsMsgDownloadAllNewsgroups(nsIMsgWindow *window, nsIUrlListener *listener)
{
  NS_INIT_REFCNT();
  m_window = window;
  m_listener = listener;
  m_downloaderForGroup = new DownloadMatchingNewsArticlesToNewsDB(window, nsnull, nsnull, this);
}

nsMsgDownloadAllNewsgroups::~nsMsgDownloadAllNewsgroups()
{
}

NS_IMETHODIMP nsMsgDownloadAllNewsgroups::OnStartRunningUrl(nsIURI* url)
{
    return NS_OK;
}

NS_IMETHODIMP
nsMsgDownloadAllNewsgroups::OnStopRunningUrl(nsIURI* url, nsresult exitCode)
{
  nsresult rv = exitCode;
  if (NS_SUCCEEDED(exitCode) || exitCode == NS_MSG_NEWS_ARTICLE_NOT_FOUND)
    rv = ProcessNextGroup();
  else if (m_listener)  // notify main observer.
    m_listener->OnStopRunningUrl(url, exitCode);
   
  return rv;
}

// leaves m_currentServer at the next nntp "server" that
// might have folders to download for offline use. If no more servers,
// m_currentServer will be left at nsnull.
// Also, sets up m_serverEnumerator to enumerate over the server
// If no servers found, m_serverEnumerator will be left at null,
nsresult nsMsgDownloadAllNewsgroups::AdvanceToNextServer(PRBool *done)
{
  nsresult rv;

  NS_ENSURE_ARG(done);

  *done = PR_TRUE;
  if (!m_allServers)
  {
    nsCOMPtr<nsIMsgAccountManager> accountManager = 
             do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
    NS_ASSERTION(accountManager && NS_SUCCEEDED(rv), "couldn't get account mgr");
    if (!accountManager || NS_FAILED(rv)) return rv;

    rv = accountManager->GetAllServers(getter_AddRefs(m_allServers));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  PRUint32 serverIndex = (m_currentServer) ? m_allServers->IndexOf(m_currentServer) + 1 : 0;
  m_currentServer = nsnull;
  PRUint32 numServers; 
  m_allServers->Count(&numServers);
  nsCOMPtr <nsIFolder> rootFolder;

  while (serverIndex < numServers)
  {
    nsCOMPtr <nsISupports> serverSupports = getter_AddRefs(m_allServers->ElementAt(serverIndex));
    serverIndex++;

    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(serverSupports);
    nsCOMPtr <nsINntpIncomingServer> newsServer = do_QueryInterface(server);
    if (!newsServer) // we're only looking for news servers
      continue;
    if (server)
    {
      m_currentServer = server;
      server->GetRootFolder(getter_AddRefs(rootFolder));
      if (rootFolder)
      {
        NS_NewISupportsArray(getter_AddRefs(m_allFolders));
        rv = rootFolder->ListDescendents(m_allFolders);
        if (NS_SUCCEEDED(rv))
          m_allFolders->Enumerate(getter_AddRefs(m_serverEnumerator));
        if (NS_SUCCEEDED(rv) && m_serverEnumerator)
        {
          rv = m_serverEnumerator->First();
          if (NS_SUCCEEDED(rv))
          {
            *done = PR_FALSE;
            break;
          }
        }
      }
    }
  }
  return rv;
}

nsresult nsMsgDownloadAllNewsgroups::AdvanceToNextGroup(PRBool *done)
{
  nsresult rv;
  NS_ENSURE_ARG(done);
  *done = PR_TRUE;

  if (m_currentFolder)
  {
    nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(m_currentFolder);
    if (newsFolder)
      newsFolder->SetSaveArticleOffline(PR_FALSE);
  
    m_currentFolder = nsnull;
  }

  *done = PR_FALSE;

  if (!m_currentServer)
     rv = AdvanceToNextServer(done);
  else
    rv = m_serverEnumerator->Next();
  if (!NS_SUCCEEDED(rv))
    rv = AdvanceToNextServer(done);

  if (NS_SUCCEEDED(rv) && !*done && m_serverEnumerator)
  {
    nsCOMPtr <nsISupports> supports;
    rv = m_serverEnumerator->CurrentItem(getter_AddRefs(supports));
    m_currentFolder = do_QueryInterface(supports);
    *done = PR_FALSE;
  }
  return rv;
}

nsresult DownloadMatchingNewsArticlesToNewsDB::RunSearch(nsIMsgFolder *folder, nsIMsgDatabase *newsDB, nsIMsgSearchSession *searchSession)
{
  m_folder = folder;
  m_newsDB = newsDB;
  m_searchSession = searchSession;

  m_keysToDownload.RemoveAll();
  nsresult rv;
  NS_ENSURE_ARG(searchSession);
  NS_ENSURE_ARG(folder);

  searchSession->RegisterListener(this);
  rv = searchSession->AddScopeTerm(nsMsgSearchScope::localNews, folder);
	return searchSession->Search(m_window);
}

nsresult nsMsgDownloadAllNewsgroups::ProcessNextGroup()
{
  nsresult rv = NS_OK;
  PRBool done = PR_FALSE;

  while (NS_SUCCEEDED(rv) && !done)
  {
    rv = AdvanceToNextGroup(&done); 
    if (m_currentFolder)
    {
      PRUint32 folderFlags;
      m_currentFolder->GetFlags(&folderFlags);
      if (folderFlags & MSG_FOLDER_FLAG_OFFLINE)
        break;
    }
  }
  if (!NS_SUCCEEDED(rv) || done)
  {
    if (m_listener)
      return m_listener->OnStopRunningUrl(nsnull, NS_OK);
  }

  nsCOMPtr <nsIMsgDatabase> db;
  nsCOMPtr <nsISupportsArray> termList;
  nsCOMPtr <nsIMsgDownloadSettings> downloadSettings;
  m_currentFolder->GetMsgDatabase(m_window, getter_AddRefs(db));
  rv = m_currentFolder->GetDownloadSettings(getter_AddRefs(downloadSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(m_currentFolder);
  if (newsFolder)
    newsFolder->SetSaveArticleOffline(PR_TRUE);

  if (!m_termList)
  {
    rv = NS_NewISupportsArray(getter_AddRefs(m_termList));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr <nsIMsgSearchSession> searchSession = do_CreateInstance(NS_MSGSEARCHSESSION_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool downloadByDate, downloadUnreadOnly;
  PRUint32 ageLimitOfMsgsToDownload;

  downloadSettings->GetDownloadByDate(&downloadByDate);
	downloadSettings->GetDownloadUnreadOnly(&downloadUnreadOnly);
	downloadSettings->GetAgeLimitOfMsgsToDownload(&ageLimitOfMsgsToDownload);

  nsCOMPtr <nsIMsgSearchTerm> term;
	nsCOMPtr <nsIMsgSearchValue>		value;

  rv = searchSession->CreateTerm(getter_AddRefs(term));
  NS_ENSURE_SUCCESS(rv, rv);
  term->GetValue(getter_AddRefs(value));

  if (downloadUnreadOnly)
  {
    value->SetAttrib(nsMsgSearchAttrib::MsgStatus);
    value->SetStatus(MSG_FLAG_READ);
    searchSession->AddSearchTerm(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, value, PR_TRUE, nsnull);
  }
	if (downloadByDate)
	{
    value->SetAttrib(nsMsgSearchAttrib::AgeInDays);
		value->SetAge(ageLimitOfMsgsToDownload);
		searchSession->AddSearchTerm(nsMsgSearchAttrib::AgeInDays, nsMsgSearchOp::IsLessThan, value, nsMsgSearchBooleanOp::BooleanAND, nsnull);
	}
  value->SetAttrib(nsMsgSearchAttrib::MsgStatus);
	value->SetStatus(MSG_FLAG_OFFLINE);
  searchSession->AddSearchTerm(nsMsgSearchAttrib::MsgStatus, nsMsgSearchOp::Isnt, value, nsMsgSearchBooleanOp::BooleanAND, nsnull);

  m_downloaderForGroup->RunSearch(m_currentFolder, db, searchSession);
  return rv;
}
