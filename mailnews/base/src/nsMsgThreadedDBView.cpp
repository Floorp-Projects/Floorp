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
#include "nsMsgThreadedDBView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"
#include "nsIDBFolderInfo.h"
#include "nsIMsgSearchSession.h"

#define MSGHDR_CACHE_LOOK_AHEAD_SIZE  25    // Allocate this more to avoid reallocation on new mail.
#define MSGHDR_CACHE_MAX_SIZE         8192  // Max msghdr cache entries.
#define MSGHDR_CACHE_DEFAULT_SIZE     100

nsMsgThreadedDBView::nsMsgThreadedDBView()
{
  /* member initializers and constructor code */
  m_havePrevView = PR_FALSE;
}

nsMsgThreadedDBView::~nsMsgThreadedDBView()
{
  /* destructor code */
}

NS_IMETHODIMP nsMsgThreadedDBView::Open(nsIMsgFolder *folder, nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder, nsMsgViewFlagsTypeValue viewFlags, PRInt32 *pCount)
{
  nsresult rv = nsMsgDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!m_db)
    return NS_ERROR_NULL_POINTER;
  // Preset msg hdr cache size for performance reason.
  PRInt32 totalMessages, unreadMessages;
  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
  PersistFolderInfo(getter_AddRefs(dbFolderInfo));
  NS_ENSURE_SUCCESS(rv, rv);
  // save off sort type and order, view type and flags
  dbFolderInfo->GetNumUnreadMessages(&unreadMessages);
  dbFolderInfo->GetNumMessages(&totalMessages);
  if (m_viewFlags & nsMsgViewFlagsType::kUnreadOnly)
  { 
    // Set unread msg size + extra entries to avoid reallocation on new mail.
    totalMessages = (PRUint32)unreadMessages+MSGHDR_CACHE_LOOK_AHEAD_SIZE;  
  }
  else
  {
    if (totalMessages > MSGHDR_CACHE_MAX_SIZE) 
      totalMessages = MSGHDR_CACHE_MAX_SIZE;        // use max default
    else if (totalMessages > 0)
      totalMessages += MSGHDR_CACHE_LOOK_AHEAD_SIZE;// allocate extra entries to avoid reallocation on new mail.
  }
  // if total messages is 0, then we probably don't have any idea how many headers are in the db
  // so we have no business setting the cache size.
  if (totalMessages > 0)
    m_db->SetMsgHdrCacheSize((PRUint32)totalMessages);
  
  if (pCount)
    *pCount = 0;
  rv = InitThreadedView(pCount);

  // this is a hack, but we're trying to find a way to correct
  // incorrect total and unread msg counts w/o paying a big
  // performance penalty. So, if we're not threaded, just add
  // up the total and unread messages in the view and see if that
  // matches what the db totals say. Except ignored threads are
  // going to throw us off...hmm. Unless we just look at the
  // unread counts which is what mostly tweaks people anyway...
  PRInt32 unreadMsgsInView = 0;
  if (!(m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
  {
    for (PRInt32 i = 0; i < m_flags.GetSize(); i++)
    {
      if (! (m_flags.GetAt(i) & MSG_FLAG_READ))
        unreadMsgsInView++;
    }
    if (unreadMessages != unreadMsgsInView)
      m_db->SyncCounts();
  }
  m_db->SetMsgHdrCacheSize(MSGHDR_CACHE_DEFAULT_SIZE);

  return rv;
}

NS_IMETHODIMP nsMsgThreadedDBView::Close()
{
  return nsMsgDBView::Close();
}

nsresult nsMsgThreadedDBView::InitThreadedView(PRInt32 *pCount)
{
  nsresult rv;
  
  m_keys.RemoveAll();
  m_flags.RemoveAll();
  m_levels.RemoveAll(); 
  m_prevKeys.RemoveAll();
  m_prevFlags.RemoveAll();
  m_prevLevels.RemoveAll();
  m_havePrevView = PR_FALSE;
  nsresult getSortrv = NS_OK; // ### TODO m_db->GetSortInfo(&sortType, &sortOrder);
  
  // list all the ids into m_keys.
  nsMsgKey startMsg = 0; 
  do
  {
    const PRInt32 kIdChunkSize = 400;
    PRInt32			numListed = 0;
    nsMsgKey	idArray[kIdChunkSize];
    PRInt32		flagArray[kIdChunkSize];
    char		levelArray[kIdChunkSize];
    
    rv = ListThreadIds(&startMsg, (m_viewFlags & nsMsgViewFlagsType::kUnreadOnly) != 0, idArray, flagArray, 
      levelArray, kIdChunkSize, &numListed, nsnull);
    if (NS_SUCCEEDED(rv))
    {
      PRInt32 numAdded = AddKeys(idArray, flagArray, levelArray, m_sortType, numListed);
      if (pCount)
        *pCount += numAdded;
    }
    
  } while (NS_SUCCEEDED(rv) && startMsg != nsMsgKey_None);
  
  if (NS_SUCCEEDED(getSortrv))
  {
    rv = InitSort(m_sortType, m_sortOrder);
    SaveSortInfo(m_sortType, m_sortOrder);

  }
  return rv;
}

nsresult nsMsgThreadedDBView::SortThreads(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
  NS_PRECONDITION(m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay, "trying to sort unthreaded threads");

  PRUint32 numThreads = 0;
  // the idea here is that copy the current view,  then build up an m_keys and m_flags array of just the top level
  // messages in the view, and then call nsMsgDBView::Sort(sortType, sortOrder).
  // Then, we expand the threads in the result array that were expanded in the original view (perhaps by copying
  // from the original view, but more likely just be calling expand).
  for (PRUint32 i = 0; i < m_keys.GetSize(); i++)
  {
    if (m_flags[i] & MSG_VIEW_FLAG_ISTHREAD)
    {
      if (numThreads < i)
      {
        m_keys.SetAt(numThreads, m_keys[i]);
        m_flags[numThreads] = m_flags[i];
      }
      m_levels[numThreads] = 0;
      numThreads++;
    }
  }
  m_keys.SetSize(numThreads);
  m_flags.SetSize(numThreads);
  m_levels.SetSize(numThreads);
  //m_viewFlags &= ~nsMsgViewFlagsType::kThreadedDisplay;
  m_sortType = nsMsgViewSortType::byNone; // sort from scratch
  nsMsgDBView::Sort(sortType, sortOrder);
  m_viewFlags |= nsMsgViewFlagsType::kThreadedDisplay;
  DisableChangeUpdates();
  // Loop through the original array, for each thread that's expanded, find it in the new array
  // and expand the thread. We have to update MSG_VIEW_FLAG_HAS_CHILDREN because
  // we may be going from a flat sort, which doesn't maintain that flag,
  // to a threaded sort, which requires that flag.
  for (PRUint32 j = 0; j < m_keys.GetSize(); j++)
  {
    PRUint32 flags = m_flags[j];
    if ((flags & (MSG_VIEW_FLAG_HASCHILDREN | MSG_FLAG_ELIDED)) == MSG_VIEW_FLAG_HASCHILDREN)
    {
      PRUint32 numExpanded;
      m_flags[j] = flags | MSG_FLAG_ELIDED;
      ExpandByIndex(j, &numExpanded);
      j += numExpanded;
      if (numExpanded > 0)
        m_flags[j - numExpanded] = flags | MSG_VIEW_FLAG_HASCHILDREN;
    }
    else if (flags & MSG_VIEW_FLAG_ISTHREAD && ! (flags & MSG_VIEW_FLAG_HASCHILDREN))
    {
      nsCOMPtr <nsIMsgDBHdr> msgHdr;
      nsCOMPtr <nsIMsgThread> pThread;
      m_db->GetMsgHdrForKey(m_keys[j], getter_AddRefs(msgHdr));
      if (msgHdr)
      {
        m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(pThread));
        if (pThread)
        {
          PRUint32 numChildren;
          pThread->GetNumChildren(&numChildren);
          if (numChildren > 1)
            m_flags[j] = flags | MSG_VIEW_FLAG_HASCHILDREN | MSG_FLAG_ELIDED;
        }
      }
    }
  }
  EnableChangeUpdates();

  return NS_OK;
}

nsresult nsMsgThreadedDBView::AddKeys(nsMsgKey *pKeys, PRInt32 *pFlags, const char *pLevels, nsMsgViewSortTypeValue sortType, PRInt32 numKeysToAdd)

{
  PRInt32	numAdded = 0;
  // Allocate enough space first to avoid memory allocation/deallocation.
  m_keys.AllocateSpace(numKeysToAdd+m_keys.GetSize());
  m_flags.AllocateSpace(numKeysToAdd+m_flags.GetSize());
  m_levels.AllocateSpace(numKeysToAdd+m_levels.GetSize());
  for (PRInt32 i = 0; i < numKeysToAdd; i++)
  {
    PRInt32 threadFlag = pFlags[i];
    PRInt32 flag = threadFlag;
    
    // skip ignored threads.
    if ((threadFlag & MSG_FLAG_IGNORED) && !(m_viewFlags & nsMsgViewFlagsType::kShowIgnored))
      continue;
    // by default, make threads collapsed, unless we're in only viewing new msgs
    
    if (flag & MSG_VIEW_FLAG_HASCHILDREN)
      flag |= MSG_FLAG_ELIDED;
    // should this be persistent? Doesn't seem to need to be.
    flag |= MSG_VIEW_FLAG_ISTHREAD;
    m_keys.Add(pKeys[i]);
    m_flags.Add(flag);
    m_levels.Add(pLevels[i]);
    numAdded++;
    // we expand as we build the view, which allows us to insert at the end of the key array,
    // instead of the middle, and is much faster.
    if ((!(m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay) || m_viewFlags & nsMsgViewFlagsType::kExpandAll) && flag & MSG_FLAG_ELIDED)
       ExpandByIndex(m_keys.GetSize() - 1, NULL);
  }
  return numAdded;
}

NS_IMETHODIMP nsMsgThreadedDBView::Sort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
  nsresult rv;

  PRInt32 rowCountBeforeSort = GetSize();

  if (!rowCountBeforeSort) 
  {
    // still need to setup our flags even when no articles - bug 98183.
    m_sortType = sortType;
    if (sortType == nsMsgViewSortType::byThread && ! (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
      SetViewFlags(m_viewFlags | nsMsgViewFlagsType::kThreadedDisplay);
    SaveSortInfo(sortType, sortOrder);
    return NS_OK;
  }

  // sort threads by sort order
  PRBool sortThreads = m_viewFlags & (nsMsgViewFlagsType::kThreadedDisplay | nsMsgViewFlagsType::kGroupBySort);
  
  // if sort type is by thread, but we're not threaded, change sort type to byId
  if (sortType == nsMsgViewSortType::byThread && (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay) != 0)
    sortType = nsMsgViewSortType::byId;

  nsMsgKey preservedKey;
  nsMsgKeyArray preservedSelection;
  SaveAndClearSelection(&preservedKey, &preservedSelection);
  // if the client wants us to forget our cached id arrays, they
  // should build a new view. If this isn't good enough, we
  // need a method to do that.
  if (sortType != m_sortType || !m_sortValid || sortThreads)
  {
    SaveSortInfo(sortType, sortOrder);
    if (sortType == nsMsgViewSortType::byThread)  
    {
      m_sortType = sortType;
      m_viewFlags |= nsMsgViewFlagsType::kThreadedDisplay;
      m_viewFlags &= nsMsgViewFlagsType::kGroupBySort;
      if ( m_havePrevView)
      {
        // restore saved id array and flags array
        m_keys.RemoveAll();
        m_keys.InsertAt(0, &m_prevKeys);
        m_flags.RemoveAll();
        m_flags.InsertAt(0, &m_prevFlags);
        m_levels.RemoveAll();
        m_levels.InsertAt(0, &m_prevLevels);
        m_sortValid = PR_TRUE;
        
        // the sort may have changed the number of rows
        // before we restore the selection, tell the tree
        // do this before we call restore selection
        // this is safe when there is no selection.
        rv = AdjustRowCount(rowCountBeforeSort, GetSize());
        
        RestoreSelection(preservedKey, &preservedSelection);
        if (mTree) mTree->Invalidate();
        return NS_OK;
      }
      else
      {
        // set sort info in anticipation of what Init will do.
        InitThreadedView(nsnull);	// build up thread list.
        if (sortOrder != nsMsgViewSortOrder::ascending)
          Sort(sortType, sortOrder);
        
        // the sort may have changed the number of rows
        // before we update the selection, tell the tree
        // do this before we call restore selection
        // this is safe when there is no selection.
        rv = AdjustRowCount(rowCountBeforeSort, GetSize());
        
        RestoreSelection(preservedKey, &preservedSelection);
        if (mTree) mTree->Invalidate();
        return NS_OK;
      }
    }
    else if (sortType  != nsMsgViewSortType::byThread && (m_sortType == nsMsgViewSortType::byThread  || sortThreads)/* && !m_havePrevView*/)
    {
      if (sortThreads)
      {
        SortThreads(sortType, sortOrder);
        sortType = nsMsgViewSortType::byThread; // hack so base class won't do anything
      }
      else
      {
        // going from SortByThread to non-thread sort - must build new key, level,and flags arrays 
        m_prevKeys.RemoveAll();
        m_prevKeys.InsertAt(0, &m_keys);
        m_prevFlags.RemoveAll();
        m_prevFlags.InsertAt(0, &m_flags);
        m_prevLevels.RemoveAll();
        m_prevLevels.InsertAt(0, &m_levels);
        // do this before we sort, so that we'll use the cheap method
        // of expanding.
        m_viewFlags &= ~(nsMsgViewFlagsType::kThreadedDisplay | nsMsgViewFlagsType::kGroupBySort);
        ExpandAll();
        //			m_idArray.RemoveAll();
        //			m_flags.RemoveAll();
        m_havePrevView = PR_TRUE;
      }
    }
  }
  else if (m_sortOrder != sortOrder)// check for toggling the sort
  {
    nsMsgDBView::Sort(sortType, sortOrder);
  }
  if (!sortThreads)
  {
    // call the base class in case we're not sorting by thread
    rv = nsMsgDBView::Sort(sortType, sortOrder);
    SaveSortInfo(sortType, sortOrder);
  }
  // the sort may have changed the number of rows
  // before we restore the selection, tell the tree
  // do this before we call restore selection
  // this is safe when there is no selection.
  rv = AdjustRowCount(rowCountBeforeSort, GetSize());

  RestoreSelection(preservedKey, &preservedSelection);
  if (mTree) mTree->Invalidate();
  NS_ENSURE_SUCCESS(rv,rv);
  return NS_OK;
}

// list the ids of the top-level thread ids starting at id == startMsg. This actually returns
// the ids of the first message in each thread.
nsresult nsMsgThreadedDBView::ListThreadIds(nsMsgKey *startMsg, PRBool unreadOnly, nsMsgKey *pOutput, PRInt32 *pFlags, char *pLevels, 
									 PRInt32 numToList, PRInt32 *pNumListed, PRInt32 *pTotalHeaders)
{
  nsresult rv = NS_OK;
  // N.B..don't ret before assigning numListed to *pNumListed
  PRInt32	numListed = 0;
  
  if (*startMsg > 0)
  {
    NS_ASSERTION(m_threadEnumerator != nsnull, "where's our iterator?");	// for now, we'll just have to rely on the caller leaving
    // the iterator in the right place.
  }
  else
  {
    NS_ASSERTION(m_db, "no db");
    if (!m_db) return NS_ERROR_UNEXPECTED;
    rv = m_db->EnumerateThreads(getter_AddRefs(m_threadEnumerator));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  PRBool hasMore = PR_FALSE;
  
  nsCOMPtr <nsIMsgThread> threadHdr ;
  PRInt32	threadsRemoved = 0;
  for (numListed = 0; numListed < numToList
    && NS_SUCCEEDED(rv = m_threadEnumerator->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE);)
  {
    nsCOMPtr <nsISupports> supports;
    rv = m_threadEnumerator->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv))
    {
      threadHdr = nsnull;
      break;
    }
    threadHdr = do_QueryInterface(supports);
    if (!threadHdr)
      break;
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    PRUint32 numChildren;
    if (unreadOnly)
      threadHdr->GetNumUnreadChildren(&numChildren);
    else
      threadHdr->GetNumChildren(&numChildren);
    PRUint32 threadFlags;
    threadHdr->GetFlags(&threadFlags);
    if (numChildren != 0)	// not empty thread
    {
      PRInt32 unusedRootIndex;
      if (pTotalHeaders)
        *pTotalHeaders += numChildren;
      if (unreadOnly)
        rv = threadHdr->GetFirstUnreadChild(getter_AddRefs(msgHdr));
      else
        rv = threadHdr->GetRootHdr(&unusedRootIndex, getter_AddRefs(msgHdr));
      if (NS_SUCCEEDED(rv) && msgHdr != nsnull && WantsThisThread(threadHdr))
      {
        PRUint32 msgFlags;
        PRUint32 newMsgFlags;
        nsMsgKey msgKey;
        msgHdr->GetMessageKey(&msgKey);
        msgHdr->GetFlags(&msgFlags);
        // turn off high byte of msg flags - used for view flags.
        msgFlags &= ~MSG_VIEW_FLAGS;
        pOutput[numListed] = msgKey;
        pLevels[numListed] = 0;
        // turn off these flags on msg hdr - they belong in thread
        msgHdr->AndFlags(~(MSG_FLAG_WATCHED | MSG_FLAG_IGNORED), &newMsgFlags);
        AdjustReadFlag(msgHdr, &msgFlags);
        // try adding in MSG_VIEW_FLAG_ISTHREAD flag for unreadonly view.
        pFlags[numListed] = msgFlags | MSG_VIEW_FLAG_ISTHREAD | threadFlags;
        if (numChildren > 1)
          pFlags[numListed] |= MSG_VIEW_FLAG_HASCHILDREN;
        
        numListed++;
      }
      else
        NS_ASSERTION(NS_SUCCEEDED(rv) && msgHdr, "couldn't get header for some reason");
    }
    else if (threadsRemoved < 10 && !(threadFlags & (MSG_FLAG_WATCHED | MSG_FLAG_IGNORED)))
    {
      // ### remove thread.
      threadsRemoved++;	// don't want to remove all empty threads first time
      // around as it will choke preformance for upgrade.
#ifdef DEBUG_bienvenu
      printf("removing empty non-ignored non-watched thread\n");
#endif
    }
  }
  
  if (hasMore && threadHdr)
  {
    threadHdr->GetThreadKey(startMsg);
  }
  else
  {
    *startMsg = nsMsgKey_None;
    m_threadEnumerator = nsnull;
  }
  *pNumListed = numListed;
  return rv;
}

void	nsMsgThreadedDBView::OnExtraFlagChanged(nsMsgViewIndex index, PRUint32 extraFlag)
{
  if (IsValidIndex(index))
  {
    if (m_havePrevView)
    {
      nsMsgKey keyChanged = m_keys[index];
      nsMsgViewIndex prevViewIndex = m_prevKeys.FindIndex(keyChanged);
      if (prevViewIndex != nsMsgViewIndex_None)
      {
        PRUint32 prevFlag = m_prevFlags.GetAt(prevViewIndex);
        // don't want to change the elided bit, or has children or is thread
        if (prevFlag & MSG_FLAG_ELIDED)
          extraFlag |= MSG_FLAG_ELIDED;
        else
          extraFlag &= ~MSG_FLAG_ELIDED;
        if (prevFlag & MSG_VIEW_FLAG_ISTHREAD)
          extraFlag |= MSG_VIEW_FLAG_ISTHREAD;
        else
          extraFlag &= ~MSG_VIEW_FLAG_ISTHREAD;
        if (prevFlag & MSG_VIEW_FLAG_HASCHILDREN)
          extraFlag |= MSG_VIEW_FLAG_HASCHILDREN;
        else
          extraFlag &= ~MSG_VIEW_FLAG_HASCHILDREN;
        m_prevFlags.SetAt(prevViewIndex, extraFlag);	// will this be right?
      }
    }
  }
  // we don't really know what's changed, but to be on the safe side, set the sort invalid
  // so that reverse sort will pick it up.
  if (m_sortType == nsMsgViewSortType::byStatus || m_sortType == nsMsgViewSortType::byFlagged || 
    m_sortType == nsMsgViewSortType::byUnread || m_sortType == nsMsgViewSortType::byPriority)
    m_sortValid = PR_FALSE;
}

void nsMsgThreadedDBView::OnHeaderAddedOrDeleted()
{
  ClearPrevIdArray();
}

void nsMsgThreadedDBView::ClearPrevIdArray()
{
  m_prevKeys.RemoveAll();
  m_prevLevels.RemoveAll();
  m_prevFlags.RemoveAll();
  m_havePrevView = PR_FALSE;
}

nsresult nsMsgThreadedDBView::InitSort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
  if (sortType == nsMsgViewSortType::byThread)
  {
    nsMsgDBView::Sort(nsMsgViewSortType::byId, sortOrder); // sort top level threads by id.
    m_sortType = nsMsgViewSortType::byThread;
    m_viewFlags |= nsMsgViewFlagsType::kThreadedDisplay;
    m_viewFlags &= ~nsMsgViewFlagsType::kGroupBySort;
    SetViewFlags(m_viewFlags); // persist the view flags.
    //		m_db->SetSortInfo(m_sortType, sortOrder);
  }
//  else
//    m_viewFlags &= ~nsMsgViewFlagsType::kThreadedDisplay;
  
  // by default, the unread only view should have all threads expanded.
  if ((m_viewFlags & (nsMsgViewFlagsType::kUnreadOnly|nsMsgViewFlagsType::kExpandAll)) 
      && (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
    ExpandAll();
  if (! (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))
    ExpandAll(); // for now, expand all and do a flat sort.
  
  Sort(sortType, sortOrder);
  if (sortType != nsMsgViewSortType::byThread)	// forget prev view, since it has everything expanded.
    ClearPrevIdArray();
  return NS_OK;
}

nsresult nsMsgThreadedDBView::OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey aParentKey, PRBool ensureListed)
{
  nsresult rv = NS_OK;
  nsMsgKey newKey;
  newHdr->GetMessageKey(&newKey);

  // views can override this behaviour, which is to append to view.
  // This is the mail behaviour, but threaded views want
  // to insert in order...
  if (newHdr)
  {
    PRUint32 msgFlags;
    newHdr->GetFlags(&msgFlags);
    if ((m_viewFlags & nsMsgViewFlagsType::kUnreadOnly) && !ensureListed && (msgFlags & MSG_FLAG_READ))
      return NS_OK;
    // Currently, we only add the header in a threaded view if it's a thread.
    // We used to check if this was the first header in the thread, but that's
    // a bit harder in the unreadOnly view. But we'll catch it below.

    // for search view we don't support threaded display so just add it to the view.   
    if (!(m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)) // || msgHdr->GetMessageKey() == m_messageDB->GetKeyOfFirstMsgInThread(msgHdr->GetMessageKey()))
      rv = AddHdr(newHdr);
    else	// need to find the thread we added this to so we can change the hasnew flag
      // added message to existing thread, but not to view
    {	// Fix flags on thread header.
      PRInt32 threadCount;
      PRUint32 threadFlags;
      nsMsgViewIndex threadIndex = ThreadIndexOfMsg(newKey, nsMsgViewIndex_None, &threadCount, &threadFlags);
      if (threadIndex != nsMsgViewIndex_None)
      {
        PRUint32	flags = m_flags[threadIndex];
        if (!(flags & MSG_VIEW_FLAG_HASCHILDREN))
        {
          flags |= MSG_VIEW_FLAG_HASCHILDREN | MSG_VIEW_FLAG_ISTHREAD;
          if (!(m_viewFlags & nsMsgViewFlagsType::kUnreadOnly))
            flags |= MSG_FLAG_ELIDED;
          m_flags[threadIndex] = flags;
        }
        if (!(flags & MSG_FLAG_ELIDED))	// thread is expanded
        {								// insert child into thread
          // levels of other hdrs may have changed!
          PRUint32	newFlags = msgFlags;
          PRInt32 level = 0;
          nsMsgViewIndex insertIndex = threadIndex;
          if (aParentKey == nsMsgKey_None)
          {
            newFlags |= MSG_VIEW_FLAG_ISTHREAD | MSG_VIEW_FLAG_HASCHILDREN;
          }
          else
          {
            nsMsgViewIndex parentIndex = FindParentInThread(aParentKey, threadIndex);
            level = m_levels[parentIndex] + 1;
            insertIndex = GetInsertInfoForNewHdr(newHdr, parentIndex, level);
          }
          m_keys.InsertAt(insertIndex, newKey);
          m_flags.InsertAt(insertIndex, newFlags, 1);
          m_levels.InsertAt(insertIndex, level);

          // the call to NoteChange() has to happen after we add the key
          // as NoteChange() will call RowCountChanged() which will call our GetRowCount()
          NoteChange(insertIndex, 1, nsMsgViewNotificationCode::insertOrDelete);

          if (aParentKey == nsMsgKey_None)
          {
            // this header is the new king! try collapsing the existing thread,
            // removing it, installing this header as king, and expanding it.
            CollapseByIndex(threadIndex, nsnull);
            // call base class, so child won't get promoted.
            // nsMsgDBView::RemoveByIndex(threadIndex);	
            ExpandByIndex(threadIndex, nsnull);
          }
        }
        else if (aParentKey == nsMsgKey_None)
        {
          // if we have a collapsed thread which just got a new
          // top of thread, change the keys array.
          m_keys.SetAt(threadIndex, newKey);
        }
        // note change, to update the parent thread's unread and total counts
        NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
      }
      else // adding msg to thread that's not in view.
      {
        nsCOMPtr <nsIMsgThread> threadHdr;
        m_db->GetThreadContainingMsgHdr(newHdr, getter_AddRefs(threadHdr));
        if (threadHdr)
        {
          AddMsgToThreadNotInView(threadHdr, newHdr, ensureListed);
        }
      }
    }
  }
  else
    rv = NS_MSG_MESSAGE_NOT_FOUND;
  return rv;
}


NS_IMETHODIMP nsMsgThreadedDBView::OnParentChanged (nsMsgKey aKeyChanged, nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeListener *aInstigator)
{
  // we need to adjust the level of the hdr whose parent changed, and invalidate that row,
  // iff we're in threaded mode.
  if (PR_FALSE && m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)
  {
    nsMsgViewIndex childIndex = FindViewIndex(aKeyChanged);
    if (childIndex != nsMsgViewIndex_None)
    {
      nsMsgViewIndex parentIndex = FindViewIndex(newParent);
      PRInt32 newParentLevel = (parentIndex == nsMsgViewIndex_None) ? -1 : m_levels[parentIndex];
      nsMsgViewIndex oldParentIndex = FindViewIndex(oldParent);
      PRInt32 oldParentLevel = (oldParentIndex != nsMsgViewIndex_None || newParent == nsMsgKey_None) 
        ? m_levels[oldParentIndex] : -1 ;
      PRInt32 levelChanged = m_levels[childIndex];
      PRInt32 parentDelta = oldParentLevel - newParentLevel;
      m_levels[childIndex] = (newParent == nsMsgKey_None) ? 0 : newParentLevel + 1;
      if (parentDelta > 0)
      {
        for (nsMsgViewIndex viewIndex = childIndex + 1; viewIndex < GetSize() && m_levels[viewIndex] > levelChanged;  viewIndex++)
        {
          m_levels[viewIndex] = m_levels[viewIndex] - parentDelta;
          NoteChange(viewIndex, 1, nsMsgViewNotificationCode::changed);
        }
      }
      NoteChange(childIndex, 1, nsMsgViewNotificationCode::changed);
    }
  }
  return NS_OK;
}


nsMsgViewIndex nsMsgThreadedDBView::GetInsertInfoForNewHdr(nsIMsgDBHdr *newHdr, nsMsgViewIndex parentIndex, PRInt32 targetLevel)
{
  PRUint32 viewSize = GetSize();
  while (++parentIndex < viewSize)
  {
    // loop until we find a message at a level less than or equal to the parent level
    if (m_levels[parentIndex] < targetLevel)
      break;
  }
  return parentIndex;
}

nsresult nsMsgThreadedDBView::AddMsgToThreadNotInView(nsIMsgThread *threadHdr, nsIMsgDBHdr *msgHdr, PRBool ensureListed)
{
  nsresult rv = NS_OK;
  PRUint32 threadFlags;
  threadHdr->GetFlags(&threadFlags);
  if (!(threadFlags & MSG_FLAG_IGNORED))
    rv = AddHdr(msgHdr);
  return rv;
}

// This method just removes the specified line from the view. It does
// NOT delete it from the database.
nsresult nsMsgThreadedDBView::RemoveByIndex(nsMsgViewIndex index)
{
  nsresult rv = NS_OK;
  PRInt32	flags;
  
  if (!IsValidIndex(index))
    return NS_MSG_INVALID_DBVIEW_INDEX;
  
  OnHeaderAddedOrDeleted();
  
  flags = m_flags[index];
  
  if (! (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay)) 
    return nsMsgDBView::RemoveByIndex(index);
  
  nsCOMPtr <nsIMsgThread> threadHdr; 
  GetThreadContainingIndex(index, getter_AddRefs(threadHdr));
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 numThreadChildren = 0; // if we can't get thread, it's already deleted and thus has 0 children
  if (threadHdr)
    threadHdr->GetNumChildren(&numThreadChildren);
  // check if we're the top level msg in the thread, and we're not collapsed.
  if ((flags & MSG_VIEW_FLAG_ISTHREAD) && !(flags & MSG_FLAG_ELIDED) && (flags & MSG_VIEW_FLAG_HASCHILDREN))
  {
    // fix flags on thread header...Newly promoted message 
    // should have flags set correctly
    if (threadHdr)
    {
      nsMsgDBView::RemoveByIndex(index);
      nsCOMPtr <nsIMsgThread> nextThreadHdr;
      if (numThreadChildren > 0)
      {
        // unreadOnly
        nsCOMPtr <nsIMsgDBHdr> msgHdr;
        rv = threadHdr->GetChildHdrAt(0, getter_AddRefs(msgHdr));
        if (msgHdr != nsnull)
        {
          PRUint32 flag = 0;
          msgHdr->GetFlags(&flag);
          if (numThreadChildren > 1)
            flag |= MSG_VIEW_FLAG_ISTHREAD | MSG_VIEW_FLAG_HASCHILDREN;
          m_flags.SetAtGrow(index, flag);
          m_levels.SetAtGrow(index, 0);
        }
      }
    }
    return rv;
  }
  else if (!(flags & MSG_VIEW_FLAG_ISTHREAD))
  {
    // we're not deleting the top level msg, but top level msg might be only msg in thread now
    if (threadHdr && numThreadChildren == 1) 
    {
      nsMsgKey msgKey;
      rv = threadHdr->GetChildKeyAt(0, &msgKey);
      if (NS_SUCCEEDED(rv))
      {
        nsMsgViewIndex threadIndex = FindViewIndex(msgKey);
        if (threadIndex != nsMsgViewIndex_None)
        {
          PRUint32 flags = m_flags[threadIndex];
          flags &= ~(MSG_VIEW_FLAG_ISTHREAD | MSG_FLAG_ELIDED | MSG_VIEW_FLAG_HASCHILDREN);
          m_flags[threadIndex] = flags;
          NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
        }
      }
      
    }
    
    return nsMsgDBView::RemoveByIndex(index);
  }
  // deleting collapsed thread header is special case. Child will be promoted,
  // so just tell FE that line changed, not that it was deleted
  if (threadHdr && numThreadChildren > 0) // header has aleady been deleted from thread
  {
    // change the id array and flags array to reflect the child header.
    // If we're not deleting the header, we want the second header,
    // Otherwise, the first one (which just got promoted).
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    rv = threadHdr->GetChildHdrAt(0, getter_AddRefs(msgHdr));
    if (msgHdr != nsnull)
    {
      nsMsgKey msgKey;
      msgHdr->GetMessageKey(&msgKey);
      m_keys.SetAt(index, msgKey);
      
      PRUint32 flag = 0;
      msgHdr->GetFlags(&flag);
      //			CopyDBFlagsToExtraFlags(msgHdr->GetFlags(), &flag);
      //			if (msgHdr->GetArticleNum() == msgHdr->GetThreadId())
      flag |= MSG_VIEW_FLAG_ISTHREAD;
      
      if (numThreadChildren == 1)	// if only hdr in thread (with one about to be deleted)
        // adjust flags.
      {
        flag &=  ~MSG_VIEW_FLAG_HASCHILDREN;
        flag &= ~MSG_FLAG_ELIDED;
        // tell FE that thread header needs to be repainted.
        NoteChange(index, 1, nsMsgViewNotificationCode::changed);	
      }
      else
      {
        flag |= MSG_VIEW_FLAG_HASCHILDREN;
        flag |= MSG_FLAG_ELIDED;
      }
      m_flags[index] = flag;
      mIndicesToNoteChange.RemoveElement(index);
    }
    else
      NS_ASSERTION(PR_FALSE, "couldn't find thread child");	
    NoteChange(index, 1, nsMsgViewNotificationCode::changed);	
  }
  else
    rv = nsMsgDBView::RemoveByIndex(index);
  return rv;
}

NS_IMETHODIMP nsMsgThreadedDBView::GetViewType(nsMsgViewTypeValue *aViewType)
{
    NS_ENSURE_ARG_POINTER(aViewType);
    *aViewType = nsMsgViewType::eShowAllThreads; 
    return NS_OK;
}

NS_IMETHODIMP
nsMsgThreadedDBView::CloneDBView(nsIMessenger *aMessengerInstance, nsIMsgWindow *aMsgWindow, nsIMsgDBViewCommandUpdater *aCmdUpdater, nsIMsgDBView **_retval)
{
  nsMsgThreadedDBView* newMsgDBView;
  NS_NEWXPCOM(newMsgDBView, nsMsgThreadedDBView);

  if (!newMsgDBView)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = CopyDBView(newMsgDBView, aMessengerInstance, aMsgWindow, aCmdUpdater);
  NS_ENSURE_SUCCESS(rv,rv);

  NS_IF_ADDREF(*_retval = newMsgDBView);
  return NS_OK;
}

NS_IMETHODIMP
nsMsgThreadedDBView::GetSupportsThreading(PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = PR_TRUE;
  return NS_OK;
}
