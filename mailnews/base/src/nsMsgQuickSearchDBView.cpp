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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Navin Gupta <naving@netscape.com> (Original Author)
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
#include "nsMsgQuickSearchDBView.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgHdr.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgImapMailFolder.h"
#include "nsImapCore.h"
#include "nsIMsgHdr.h"

nsMsgQuickSearchDBView::nsMsgQuickSearchDBView()
{
}

nsMsgQuickSearchDBView::~nsMsgQuickSearchDBView()
{	
 /* destructor code */
}

NS_IMPL_ISUPPORTS_INHERITED2(nsMsgQuickSearchDBView, nsMsgDBView, nsIMsgDBView, nsIMsgSearchNotify)

NS_IMETHODIMP nsMsgQuickSearchDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
  nsresult rv = nsMsgDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!m_db)
    return NS_ERROR_NULL_POINTER;
  if (pCount)
    *pCount = 0;
  m_viewFolder = nsnull;
  return InitThreadedView(pCount);
}

NS_IMETHODIMP nsMsgQuickSearchDBView::DoCommand(nsMsgViewCommandTypeValue aCommand)
{
  if (aCommand == nsMsgViewCommandType::markAllRead)
  {
    nsresult rv = NS_OK;
    m_folder->EnableNotifications(nsIMsgFolder::allMessageCountNotifications, PR_FALSE, PR_TRUE /*dbBatching*/);

    for (PRInt32 i=0;NS_SUCCEEDED(rv) && i < GetSize();i++)
    {
      nsCOMPtr<nsIMsgDBHdr> msgHdr;
      m_db->GetMsgHdrForKey(m_keys[i],getter_AddRefs(msgHdr)); 
      rv = m_db->MarkHdrRead(msgHdr, PR_TRUE, nsnull);
    }

    m_folder->EnableNotifications(nsIMsgFolder::allMessageCountNotifications, PR_TRUE, PR_TRUE /*dbBatching*/);

    nsCOMPtr<nsIMsgImapMailFolder> imapFolder = do_QueryInterface(m_folder);
    if (NS_SUCCEEDED(rv) && imapFolder)
      rv = imapFolder->StoreImapFlags(kImapMsgSeenFlag, PR_TRUE, m_keys.GetArray(), 
                                      m_keys.GetSize(), nsnull);

    m_db->SetSummaryValid(PR_TRUE);
    m_db->Commit(nsMsgDBCommitType::kLargeCommit);
    return rv;
  }
  else
    return nsMsgDBView::DoCommand(aCommand);
}

NS_IMETHODIMP nsMsgQuickSearchDBView::GetViewType(nsMsgViewTypeValue *aViewType)
{
    NS_ENSURE_ARG_POINTER(aViewType);
    *aViewType = nsMsgViewType::eShowQuickSearchResults; 
    return NS_OK;
}

nsresult nsMsgQuickSearchDBView::OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey aParentKey, PRBool ensureListed)
{
  if (newHdr)
  {
    PRBool match=PR_FALSE;
    nsCOMPtr <nsIMsgSearchSession> searchSession = do_QueryReferent(m_searchSession);
    if (searchSession)
      searchSession->MatchHdr(newHdr, m_db, &match);
    if (match)
      nsMsgThreadedDBView::OnNewHeader(newHdr, aParentKey, ensureListed); // do not add a new message if there isn't a match.
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgQuickSearchDBView::OnHdrChange(nsIMsgDBHdr *aHdrChanged, PRUint32 aOldFlags, 
                                       PRUint32 aNewFlags, nsIDBChangeListener *aInstigator)
{
  nsresult rv = nsMsgDBView::OnHdrChange(aHdrChanged, aOldFlags, aNewFlags, aInstigator);
  // flags haven't really changed - check if the message is newly classified as junk 
  if ((aOldFlags == aNewFlags) && (aOldFlags & MSG_FLAG_NEW)) 
  {
    if (aHdrChanged)
    {
      nsXPIDLCString junkScoreStr;
      (void) aHdrChanged->GetStringProperty("junkscore", getter_Copies(junkScoreStr));
      if (atoi(junkScoreStr.get()) > 50)
      {
        nsXPIDLCString originStr;
        (void) aHdrChanged->GetStringProperty("junkscoreorigin", 
                                       getter_Copies(originStr));

        // if this was classified by the plugin, see if we're supposed to
        // show junk mail
        if (originStr.get()[0] == 'p') 
        {
          PRBool match=PR_FALSE;
          nsCOMPtr <nsIMsgSearchSession> searchSession = do_QueryReferent(m_searchSession);
          if (searchSession)
            searchSession->MatchHdr(aHdrChanged, m_db, &match);
          if (!match)
          {
            // remove hdr from view
            nsMsgViewIndex deletedIndex = FindHdr(aHdrChanged);
            if (deletedIndex != nsMsgViewIndex_None)
              RemoveByIndex(deletedIndex);
          }
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMsgQuickSearchDBView::GetSearchSession(nsIMsgSearchSession* *aSession)
{
  NS_ASSERTION(PR_FALSE, "GetSearchSession method is not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMsgQuickSearchDBView::SetSearchSession(nsIMsgSearchSession *aSession)
{
  m_searchSession = do_GetWeakReference(aSession);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgQuickSearchDBView::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(aMsgHdr);
  if (!m_db)
    return NS_ERROR_NULL_POINTER;
  return AddHdr(aMsgHdr); 
}

NS_IMETHODIMP
nsMsgQuickSearchDBView::OnSearchDone(nsresult status)
{
  if (m_sortType != nsMsgViewSortType::byThread)//we do not find levels for the results.
  {
    m_sortValid = PR_FALSE;       //sort the results 
    Sort(m_sortType, m_sortOrder);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsMsgQuickSearchDBView::OnNewSearch()
{
  PRInt32 oldSize = GetSize();

  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();

  // this needs to happen after we remove all the keys, since RowCountChanged() will call our GetRowCount()
  if (mTree)
    mTree->RowCountChanged(0, -oldSize);

  return NS_OK;
}

nsresult nsMsgQuickSearchDBView::GetFirstMessageHdrToDisplayInThread(nsIMsgThread *threadHdr, nsIMsgDBHdr **result)
{
  PRUint32 numChildren;
  nsresult rv = NS_OK;
  PRUint8 minLevel = 0xff;
  nsMsgKey threadRootKey;

  threadHdr->GetNumChildren(&numChildren);
  threadHdr->GetThreadKey(&threadRootKey);
  if ((PRInt32) numChildren < 0)
    numChildren = 0;

  nsCOMPtr <nsIMsgDBHdr> retHdr;

  // iterate over thread, finding mgsHdr in view with the lowest level.
  for (PRUint32 childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsCOMPtr <nsIMsgDBHdr> child;
    rv = threadHdr->GetChildHdrAt(childIndex, getter_AddRefs(child));
    if (NS_SUCCEEDED(rv) && child)
    {
      nsMsgKey msgKey;
      child->GetMessageKey(&msgKey);

      // this works because we've already sorted m_keys by id.
      nsMsgViewIndex keyIndex = m_origKeys.IndexOfSorted(msgKey);
      if (keyIndex != kNotFound)
      {
        // this is the root, so it's the best we're going to do.
        if (msgKey == threadRootKey)
        {
          retHdr = child;
          break;
        }
        PRUint8 level = 0;
        nsMsgKey parentId;
        child->GetThreadParent(&parentId);
        nsCOMPtr <nsIMsgDBHdr> parent;
        // count number of ancestors - that's our level
        while (parentId != nsMsgKey_None)
        {
          rv = m_db->GetMsgHdrForKey(parentId, getter_AddRefs(parent));
          if (parent)
          {
            parent->GetThreadParent(&parentId);
            level++;
          }
        }
        if (level < minLevel)
        {
          minLevel = level;
          retHdr = child;
        }
      }
    }
  }
  NS_IF_ADDREF(*result = retHdr);
  return NS_OK; 
}

nsresult nsMsgQuickSearchDBView::SortThreads(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
  // we don't handle grouping in quick search views yet.
  if (m_viewFlags & nsMsgViewFlagsType::kGroupBySort)
    return NS_OK;

  // iterate over the messages in the view, getting the thread id's
  // sort m_keys so we can quickly find if a key is in the view. 
  m_keys.QuickSort();
  // array of the threads' root hdr keys.
  nsMsgKeyArray threadRootIds;
  nsCOMPtr <nsIMsgDBHdr> rootHdr;
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  nsCOMPtr <nsIMsgThread> threadHdr;
  for (PRUint32 i = 0; i < m_keys.GetSize(); i++)
  {
    GetMsgHdrForViewIndex(i, getter_AddRefs(msgHdr));
    m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(threadHdr));
    if (threadHdr)
    {
      nsMsgKey rootKey;
      threadHdr->GetChildKeyAt(0, &rootKey);
      nsMsgViewIndex threadRootIndex = threadRootIds.IndexOfSorted(rootKey);
      // if we already have that id in top level threads, ignore this msg.
      if (threadRootIndex != kNotFound)
        continue;
      // it would be nice if GetInsertIndexHelper always found the hdr, but it doesn't.
      threadHdr->GetChildHdrAt(0, getter_AddRefs(rootHdr));
      threadRootIndex = GetInsertIndexHelper(rootHdr, &threadRootIds, nsMsgViewSortOrder::ascending, nsMsgViewSortType::byId);
      threadRootIds.InsertAt(threadRootIndex, rootKey);
    }
  }
  m_origKeys.CopyArray(m_keys);
  // need to sort the top level threads now by sort order, if it's not by id.
  if (sortType != nsMsgViewSortType::byId)
  {
    m_keys.CopyArray(threadRootIds);
    nsMsgDBView::Sort(sortType, sortOrder);
    threadRootIds.CopyArray(m_keys);
  }
  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();
  // now we've build up the list of thread ids - need to build the view
  // from that. So for each thread id, we need to list the messages in the thread.
  PRUint32 numThreads = threadRootIds.GetSize();
  for (PRUint32 threadIndex = 0; threadIndex < numThreads; threadIndex++)
  {
    m_db->GetMsgHdrForKey(threadRootIds[threadIndex], getter_AddRefs(rootHdr));
    if (rootHdr)
    {
      nsCOMPtr <nsIMsgDBHdr> displayRootHdr;
      m_db->GetThreadContainingMsgHdr(rootHdr, getter_AddRefs(threadHdr));
      if (threadHdr)
      {
        nsMsgKey rootKey;
        PRUint32 rootFlags;
        GetFirstMessageHdrToDisplayInThread(threadHdr, getter_AddRefs(displayRootHdr));
        displayRootHdr->GetMessageKey(&rootKey);
        displayRootHdr->GetFlags(&rootFlags);
        rootFlags |= MSG_VIEW_FLAG_ISTHREAD;
        m_keys.Add(rootKey);
        m_flags.Add(rootFlags);
        m_levels.Add(0);

        nsMsgViewIndex startOfThreadViewIndex = m_keys.GetSize() - 1;
        PRUint32 numListed;
        ListIdsInThread(threadHdr, startOfThreadViewIndex, &numListed);
      }
    }
  }
  NS_ASSERTION(m_origKeys.GetSize() == m_keys.GetSize(), "problem threading quick search");
  return NS_OK;
}

nsresult  nsMsgQuickSearchDBView::ListIdsInThread(nsIMsgThread *threadHdr, nsMsgViewIndex startOfThreadViewIndex, PRUint32 *pNumListed)
{
  PRUint32 numChildren;
  threadHdr->GetNumChildren(&numChildren);
  PRUint32 i;
  PRUint32 viewIndex = startOfThreadViewIndex + 1;
  nsCOMPtr <nsIMsgDBHdr> rootHdr;
  nsMsgKey rootKey;
  PRUint32 rootFlags = m_flags[startOfThreadViewIndex];
  *pNumListed = 0;
  GetMsgHdrForViewIndex(startOfThreadViewIndex, getter_AddRefs(rootHdr));
  rootHdr->GetMessageKey(&rootKey);
  for (i = 0; i < numChildren; i++)
  {
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    threadHdr->GetChildHdrAt(i, getter_AddRefs(msgHdr));
    if (msgHdr != nsnull)
    {
      nsMsgKey msgKey;
      msgHdr->GetMessageKey(&msgKey);
      if (msgKey != rootKey)
      {
        nsMsgViewIndex threadRootIndex = m_origKeys.IndexOfSorted(msgKey);
        // if this hdr is in the original view, add it to new view.
        if (threadRootIndex != kNotFound)
        {
          PRUint32 childFlags;
          msgHdr->GetFlags(&childFlags);
          PRUint8 levelToAdd;
          m_keys.InsertAt(viewIndex, msgKey);
          m_flags.InsertAt(viewIndex, childFlags);
          if (! (rootFlags & MSG_VIEW_FLAG_HASCHILDREN))
          {
            rootFlags |= MSG_VIEW_FLAG_HASCHILDREN;
            m_flags.SetAt(startOfThreadViewIndex, rootFlags);
          }
          levelToAdd = FindLevelInThread(msgHdr, startOfThreadViewIndex, viewIndex);
          m_levels.InsertAt(viewIndex, levelToAdd);
          viewIndex++;
          (*pNumListed)++;
        }
      }
    }
  }
  return NS_OK;
}

nsresult nsMsgQuickSearchDBView::ExpansionDelta(nsMsgViewIndex index, PRInt32 *expansionDelta)
{
  *expansionDelta = 0;
  if ( index > ((nsMsgViewIndex) m_keys.GetSize()))
    return NS_MSG_MESSAGE_NOT_FOUND;

  char	flags = m_flags[index];

  if (!(m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
    return NS_OK;

  // The client can pass in the key of any message
  // in a thread and get the expansion delta for the thread.

  PRInt32 numChildren = CountExpandedThread(index);

  *expansionDelta = (flags & MSG_FLAG_ELIDED) ? 
                    numChildren - 1 : - (PRInt32) (numChildren - 1);
  return NS_OK;
}
