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

NS_IMETHODIMP nsMsgXFVirtualFolderDBView::Close()
{
  PRInt32 count = m_dbToUseList.Count();
  
  for(PRInt32 i = 0; i < count; i++)
    m_dbToUseList[i]->RemoveListener(this);

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
      OnSearchHit(newHdr, folder); 
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(aMsgHdr);
  NS_ENSURE_ARG(folder);

  nsCOMPtr <nsISupports> supports = do_QueryInterface(folder);
  if (m_folders->IndexOf(supports) < 0 ) //do this just for new folder
  {
    nsCOMPtr<nsIMsgDatabase> dbToUse;
    nsCOMPtr<nsIDBFolderInfo> folderInfo;
    folder->GetDBFolderInfoAndDB(getter_AddRefs(folderInfo), getter_AddRefs(dbToUse));
    if (dbToUse)
    {
      dbToUse->AddListener(this);
      m_dbToUseList.AppendObject(dbToUse);
    }
  }

  m_folders->AppendElement(supports);
  nsMsgKey msgKey;
  PRUint32 msgFlags;
  aMsgHdr->GetMessageKey(&msgKey);
  aMsgHdr->GetFlags(&msgFlags);
  m_keys.Add(msgKey);
  m_levels.Add(0);
  m_flags.Add(msgFlags);

  // this needs to be called after we add the key, since RowCountChanged() will call our GetRowCount()
  if (mTree)
    mTree->RowCountChanged(GetSize() - 1, 1);

  m_numTotal++;
  if (!(msgFlags & MSG_FLAG_READ))
    m_numUnread++;

  return NS_OK;
}

NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnSearchDone(nsresult status)
{
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
  return rv;
}


NS_IMETHODIMP
nsMsgXFVirtualFolderDBView::OnNewSearch()
{
  PRInt32 oldSize = GetSize();

  PRInt32 count = m_dbToUseList.Count();
  for(PRInt32 j = 0; j < count; j++) 
    m_dbToUseList[j]->RemoveListener(this);

  m_dbToUseList.Clear();

  m_folders->Clear();
  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();

  // needs to happen after we remove the keys, since RowCountChanged() will call our GetRowCount()
  if (mTree) 
    mTree->RowCountChanged(0, -oldSize);

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
