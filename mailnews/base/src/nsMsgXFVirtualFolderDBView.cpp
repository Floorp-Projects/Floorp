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
 * David Bienvenu.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Bienvenu <bienvenu@nventure.com>
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

#include "msgCore.h"
#include "nsMsgXFVirtualFolderDBView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"
#include "nsQuickSort.h"
#include "nsIDBFolderInfo.h"
#include "nsXPIDLString.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgCopyService.h"
#include "nsICopyMsgStreamListener.h"
#include "nsMsgUtils.h"
#include "nsITreeColumns.h"
#include "nsIMsgSearchSession.h"
#include "nsMsgDBCID.h"

nsMsgXFVirtualFolderDBView::nsMsgXFVirtualFolderDBView()
{
  mSuppressMsgDisplay = PR_FALSE;
  m_numUnread = m_numTotal = 0;
}

nsMsgXFVirtualFolderDBView::~nsMsgXFVirtualFolderDBView()
{	
}

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
  m_viewFolder = folder;
  return nsMsgSearchDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
}

void nsMsgXFVirtualFolderDBView::RemovePendingDBListeners()
{
  nsresult rv;
  PRInt32 scopeCount;
  nsCOMPtr <nsIMsgSearchSession> searchSession = do_QueryReferent(m_searchSession);
  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
  searchSession->CountSearchScopes(&scopeCount);
  for (PRInt32 i = 0; i < scopeCount; i++)
  {
    nsMsgSearchScopeValue scopeId;
    nsCOMPtr<nsIMsgFolder> searchFolder;
    searchSession->GetNthSearchScope(i, &scopeId, getter_AddRefs(searchFolder));
    if (searchFolder && msgDBService)
      msgDBService->UnregisterPendingListener(this);
  }
}

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::Close()
{
  RemovePendingDBListeners();
  return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::CloneDBView(nsIMessenger *aMessengerInstance, nsIMsgWindow *aMsgWindow, 
                                        nsIMsgDBViewCommandUpdater *aCmdUpdater, nsIMsgDBView **_retval)
{
  nsMsgXFVirtualFolderDBView* newMsgDBView;
  NS_NEWXPCOM(newMsgDBView, nsMsgXFVirtualFolderDBView);

  if (!newMsgDBView)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = CopyDBView(newMsgDBView, aMessengerInstance, aMsgWindow, aCmdUpdater);
  NS_ENSURE_SUCCESS(rv,rv);

  NS_IF_ADDREF(*_retval = newMsgDBView);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::CopyDBView(nsMsgDBView *aNewMsgDBView, nsIMessenger *aMessengerInstance, 
                                       nsIMsgWindow *aMsgWindow, nsIMsgDBViewCommandUpdater *aCmdUpdater)
{
  nsMsgSearchDBView::CopyDBView(aNewMsgDBView, aMessengerInstance, aMsgWindow, aCmdUpdater);

  nsMsgXFVirtualFolderDBView* newMsgDBView = (nsMsgXFVirtualFolderDBView *) aNewMsgDBView;

  newMsgDBView->m_viewFolder = m_viewFolder;
  newMsgDBView->m_numUnread = m_numUnread;
  newMsgDBView->m_numTotal = m_numTotal;
  newMsgDBView->m_searchSession = m_searchSession;
  return NS_OK;
}


NS_IMETHODIMP nsMsgXFVirtualFolderDBView::GetViewType(nsMsgViewTypeValue *aViewType)
{
    NS_ENSURE_ARG_POINTER(aViewType);
    *aViewType = nsMsgViewType::eShowVirtualFolderResults; 
    return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::SetSearchSession(nsIMsgSearchSession *aSession)
{
  m_searchSession = do_GetWeakReference(aSession);
  return NS_OK;
}

nsresult nsMsgXFVirtualFolderDBView::OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey aParentKey, PRBool /*ensureListed*/)
{
  if (newHdr)
  {
    PRBool match=PR_FALSE;
    nsCOMPtr <nsIMsgSearchSession> searchSession = do_QueryReferent(m_searchSession);
    if (searchSession)
      searchSession->MatchHdr(newHdr, m_db, &match);
    if (match)
    {
      nsCOMPtr <nsIMsgFolder> folder;
      newHdr->GetFolder(getter_AddRefs(folder));
      // set this to null so we don't think we're in the middle of a full-blown
      // search and start deleting invalid hits.
      m_curFolderGettingHits = nsnull;
      OnSearchHit(newHdr, folder); 
    }
  }
  return NS_OK;
}

nsresult nsMsgXFVirtualFolderDBView::InsertHdrFromFolder(nsIMsgDBHdr *msgHdr, nsISupports *folder)
{
  nsMsgViewIndex insertIndex = GetInsertIndex(msgHdr);
  if (insertIndex == nsMsgViewIndex_None)
    return AddHdrFromFolder(msgHdr, folder);

  nsMsgKey msgKey;
  PRUint32 msgFlags;
  msgHdr->GetMessageKey(&msgKey);
  msgHdr->GetFlags(&msgFlags);
  m_keys.InsertAt(insertIndex, msgKey);
  m_flags.InsertAt(insertIndex, msgFlags);
  m_folders->InsertElementAt(folder, insertIndex);
  m_levels.InsertAt((PRInt32) insertIndex, (PRUint8) 0);
    
  // the call to NoteChange() has to happen after we add the key
  // as NoteChange() will call RowCountChanged() which will call our GetRowCount()
  NoteChange(insertIndex, 1, nsMsgViewNotificationCode::insertOrDelete);
  return NS_OK;
}

void nsMsgXFVirtualFolderDBView::UpdateCacheAndViewForFolder(nsIMsgFolder *folder, nsMsgKey *newHits, PRUint32 numNewHits)
{
  nsCOMPtr <nsIMsgDatabase> db;
  nsresult rv = folder->GetMsgDatabase(nsnull, getter_AddRefs(db));
  if (NS_SUCCEEDED(rv) && db)
  {
    nsXPIDLCString searchUri;
    m_viewFolder->GetURI(getter_Copies(searchUri));
    PRUint32 numBadHits;
    nsMsgKey *badHits;
    rv = db->RefreshCache(searchUri, numNewHits, newHits,
                     &numBadHits, &badHits);
    if (NS_SUCCEEDED(rv))
    {
      for (PRUint32 badHitIndex = 0; badHitIndex < numBadHits; badHitIndex++)
      {
        // of course, this isn't quite right
        nsMsgViewIndex staleHitIndex = FindKey(badHits[badHitIndex], PR_TRUE);
        if (staleHitIndex != nsMsgViewIndex_None)
          RemoveByIndex(staleHitIndex);
      }
      delete [] badHits;
    }
  }
}

void nsMsgXFVirtualFolderDBView::UpdateCacheAndViewForPrevSearchedFolders(nsIMsgFolder *curSearchFolder)
{
  // Handle the most recent folder with hits, if any.
  if (m_curFolderGettingHits)
  {
    PRUint32 count = m_hdrHits.Count();
    nsMsgKeyArray newHits;
    for (PRUint32 i = 0; i < count; i++)
    {
      nsMsgKey key;
      m_hdrHits[i]->GetMessageKey(&key);
      newHits.Add(key);
    }
    UpdateCacheAndViewForFolder(m_curFolderGettingHits, newHits.GetArray(), newHits.GetSize());
  }

  while (m_foldersWithNonVerifiedCachedHits.Count() > 0)
  {
    // this new folder has cached hits.
    if (m_foldersWithNonVerifiedCachedHits[0] == curSearchFolder)
    {
      m_curFolderHasCachedHits = PR_TRUE;
      break;
    }
    else if (m_foldersWithNonVerifiedCachedHits[0] != m_curFolderGettingHits)
    {
      // this must be a folder that had cached hits but no hits with 
      // the current search. So all cached hits need to be removed. 
      UpdateCacheAndViewForFolder(m_foldersWithNonVerifiedCachedHits[0], 0, nsnull);
    }
    m_foldersWithNonVerifiedCachedHits.RemoveObjectAt(0);
  }
}
NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(aMsgHdr);
  NS_ENSURE_ARG(folder);

  nsCOMPtr <nsISupports> supports = do_QueryInterface(folder);
  nsCOMPtr<nsIMsgDatabase> dbToUse;
  nsCOMPtr<nsIDBFolderInfo> folderInfo;
  folder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));
  
  if (m_curFolderGettingHits != folder)
  {
    m_curFolderHasCachedHits = PR_FALSE;
    // since we've gotten a hit for a new folder, the searches for 
    // any previous folders are done, so deal with stale cached hits
    // for those folders now.

    UpdateCacheAndViewForPrevSearchedFolders(folder);
    m_curFolderGettingHits = folder;
    m_hdrHits.Clear();
    m_curFolderStartKeyIndex = m_keys.GetSize();
  }
  PRBool hdrInCache = PR_FALSE;
  nsXPIDLCString searchUri;
  m_viewFolder->GetURI(getter_Copies(searchUri));
  dbToUse->HdrIsInCache(searchUri, aMsgHdr, &hdrInCache);
  if (!m_curFolderHasCachedHits || !hdrInCache)
  {
    if (m_sortValid)
      InsertHdrFromFolder(aMsgHdr, supports);
    else
      AddHdrFromFolder(aMsgHdr, supports);
  }
  m_hdrHits.AppendObject(aMsgHdr);

  m_numTotal++;
  PRUint32 msgFlags;
  aMsgHdr->GetFlags(&msgFlags);
  if (!(msgFlags & MSG_FLAG_READ))
    m_numUnread++;

  return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnSearchDone(nsresult status)
{
  // handle any non verified hits we haven't handled yet.
  UpdateCacheAndViewForPrevSearchedFolders(nsnull);

  //we want to set imap delete model once the search is over because setting next
  //message after deletion will happen before deleting the message and search scope
  //can change with every search.
  mDeleteModel = nsMsgImapDeleteModels::MoveToTrash;  //set to default in case it is non-imap folder
  nsCOMPtr <nsIMsgFolder> curFolder = do_QueryElementAt(m_folders, 0);
  if (curFolder)   
    GetImapDeleteModel(curFolder);
  nsCOMPtr <nsIMsgDatabase> virtDatabase;
  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;

  nsresult rv = m_viewFolder->GetDBFolderInfoAndDB(getter_AddRefs(dbFolderInfo), getter_AddRefs(virtDatabase));
  NS_ENSURE_SUCCESS(rv, rv);
  dbFolderInfo->SetNumUnreadMessages(m_numUnread);
  dbFolderInfo->SetNumMessages(m_numTotal);
  m_viewFolder->UpdateSummaryTotals(true); // force update from db.
  virtDatabase->Commit(nsMsgDBCommitType::kLargeCommit);
  if (!m_sortValid && m_sortType != nsMsgViewSortType::byThread)
  {
    m_sortValid = PR_FALSE;       //sort the results 
    Sort(m_sortType, m_sortOrder);
  }
  m_foldersWithNonVerifiedCachedHits.Clear();
  m_curFolderGettingHits = nsnull;
  return rv;
}


NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnNewSearch()
{
  PRInt32 oldSize = GetSize();

  RemovePendingDBListeners();

  m_folders->Clear();
  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();

  // needs to happen after we remove the keys, since RowCountChanged() will call our GetRowCount()
  if (mTree) 
    mTree->RowCountChanged(0, -oldSize);

  // to use the search results cache, we'll need to iterate over the scopes in the
  // search session, calling getNthSearchScope for i = 0; i < searchSession.countSearchScopes; i++
  // and for each folder, then open the db and pull out the cached hits, add them to the view.
  // For each hit in a new folder, we'll then clean up the stale hits from the previous folder(s).
  
  PRInt32 scopeCount;
  nsCOMPtr <nsIMsgSearchSession> searchSession = do_QueryReferent(m_searchSession);
  nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID);
  searchSession->CountSearchScopes(&scopeCount);
  for (PRInt32 i = 0; i < scopeCount; i++)
  {
    nsMsgSearchScopeValue scopeId;
    nsCOMPtr<nsIMsgFolder> searchFolder;
    searchSession->GetNthSearchScope(i, &scopeId, getter_AddRefs(searchFolder));
    if (searchFolder)
    {
      nsCOMPtr<nsISimpleEnumerator> cachedHits;
      nsCOMPtr<nsIMsgDatabase> searchDB;
      nsXPIDLCString searchUri;
      m_viewFolder->GetURI(getter_Copies(searchUri));
      nsresult rv = searchFolder->GetMsgDatabase(nsnull, getter_AddRefs(searchDB));
      if (NS_SUCCEEDED(rv) && searchDB)
      {
        if (msgDBService)
          msgDBService->RegisterPendingListener(searchFolder, this);

        searchDB->GetCachedHits(searchUri, getter_AddRefs(cachedHits));
        PRBool hasMore;
        if (cachedHits)
        {
          cachedHits->HasMoreElements(&hasMore);
          if (hasMore)
          {
            nsMsgKey prevKey = nsMsgKey_None;
            m_foldersWithNonVerifiedCachedHits.AppendObject(searchFolder);
            while (hasMore)
            {
              nsCOMPtr <nsIMsgDBHdr> pHeader;
              nsresult rv = cachedHits->GetNext(getter_AddRefs(pHeader));
              NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
              if (pHeader && NS_SUCCEEDED(rv))
              {
                nsMsgKey msgKey;
                pHeader->GetMessageKey(&msgKey);
                NS_ASSERTION(prevKey == nsMsgKey_None || msgKey > prevKey, "cached Hits not sorted");
                prevKey = msgKey;
                AddHdrFromFolder(pHeader, searchFolder); // need to QI to nsISupports?
              }
              else
                break;
              cachedHits->HasMoreElements(&hasMore);
            }
          }
        }
      }
    }
  }

  m_curFolderStartKeyIndex = 0;
  m_curFolderGettingHits = nsnull;
  m_curFolderHasCachedHits = PR_FALSE;

  // if we have cached hits, sort them.
  if (GetSize() > 0)
  {
    if (m_sortType != nsMsgViewSortType::byThread)
    {
      m_sortValid = PR_FALSE;       //sort the results 
      Sort(m_sortType, m_sortOrder);
    }
  }
//    mSearchResults->Clear();
    return NS_OK;
}


NS_IMETHODIMP nsMsgXFVirtualFolderDBView::DoCommand(nsMsgViewCommandTypeValue command)
{
    return nsMsgSearchDBView::DoCommand(command);
}



NS_IMETHODIMP nsMsgXFVirtualFolderDBView::GetMsgFolder(nsIMsgFolder **aMsgFolder)
{
  NS_ENSURE_ARG_POINTER(aMsgFolder);
  NS_IF_ADDREF(*aMsgFolder = m_viewFolder);
  return NS_OK;
}
