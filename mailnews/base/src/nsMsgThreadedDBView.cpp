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
#include "nsMsgThreadedDBView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"

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
	nsresult rv;
	rv = nsMsgDBView::Open(folder, sortType, sortOrder, viewFlags, pCount);
    NS_ENSURE_SUCCESS(rv, rv);

	if (pCount)
		*pCount = 0;
	return InitThreadedView(pCount);
}

NS_IMETHODIMP nsMsgThreadedDBView::Close()
{
  return nsMsgDBView::Close();
}

NS_IMETHODIMP nsMsgThreadedDBView::ReloadFolderAfterQuickSearch()
{
  mIsSearchView = PR_FALSE;
  m_sortValid = PR_FALSE;  //force a sort
  nsresult rv = NS_OK;
  if (m_preSearchKeys.GetSize())
  {
    // restore saved id array and flags array
    m_keys.RemoveAll();
    m_flags.RemoveAll();
    m_levels.RemoveAll();
    m_keys.InsertAt(0, &m_preSearchKeys);
    m_flags.InsertAt(0, &m_preSearchFlags);
    m_levels.InsertAt(0, &m_preSearchLevels);
    ClearPreSearchInfo();
    ClearPrevIdArray();  // previous cached info about non threaded display is not useful
    InitSort(m_sortType, m_sortOrder);
  }
  else
  {
    rv=InitThreadedView(nsnull);
  }
  return rv;
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
	}
	return rv;
}

nsresult nsMsgThreadedDBView::AddKeys(nsMsgKey *pKeys, PRInt32 *pFlags, const char *pLevels, nsMsgViewSortTypeValue sortType, PRInt32 numKeysToAdd)

{
	PRInt32	numAdded = 0;
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
		if ((/*m_viewFlags & nsMsgViewFlagsType::kUnreadOnly || */(sortType != nsMsgViewSortType::byThread)) && flag & MSG_FLAG_ELIDED)
		{
			ExpandByIndex(m_keys.GetSize() - 1, NULL);
		}
	}
	return numAdded;
}

NS_IMETHODIMP nsMsgThreadedDBView::Sort(nsMsgViewSortTypeValue sortType, nsMsgViewSortOrderValue sortOrder)
{
  nsresult rv;
  nsMsgKeyArray preservedSelection;
  SaveSelection(&preservedSelection);
  
  PRInt32 rowCountBeforeSort = GetSize();

  if (!rowCountBeforeSort)
    return NS_OK;
  
  // if the client wants us to forget our cached id arrays, they
  // should build a new view. If this isn't good enough, we
  // need a method to do that.
  if (sortType != m_sortType || !m_sortValid )
  {
    if (sortType == nsMsgViewSortType::byThread)  
    {
      SaveSortInfo(sortType, sortOrder);
      m_sortType = sortType;
      m_viewFlags |= nsMsgViewFlagsType::kThreadedDisplay;
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
        // before we restore the selection, tell the outliner
        // do this before we call restore selection
        // this is safe when there is no selection.
        rv = AdjustRowCount(rowCountBeforeSort, GetSize());
        
        RestoreSelection(&preservedSelection);
        if (mOutliner) mOutliner->Invalidate();
        return NS_OK;
      }
      else
      {
        // set sort info in anticipation of what Init will do.
        InitThreadedView(nsnull);	// build up thread list.
        if (sortOrder != nsMsgViewSortOrder::ascending)
          Sort(sortType, sortOrder);
        
        // the sort may have changed the number of rows
        // before we update the selection, tell the outliner
        // do this before we call restore selection
        // this is safe when there is no selection.
        rv = AdjustRowCount(rowCountBeforeSort, GetSize());
        
        RestoreSelection(&preservedSelection);
        if (mOutliner) mOutliner->Invalidate();
        return NS_OK;
      }
    }
    else if (sortType  != nsMsgViewSortType::byThread && m_sortType == nsMsgViewSortType::byThread /* && !m_havePrevView*/)
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
      m_viewFlags &= ~nsMsgViewFlagsType::kThreadedDisplay;
      ExpandAll();
      //			m_idArray.RemoveAll();
      //			m_flags.RemoveAll();
      m_havePrevView = PR_TRUE;
    }
  }
  // call the base class in case we're not sorting by thread
  rv = nsMsgDBView::Sort(sortType, sortOrder);
  SaveSortInfo(sortType, sortOrder);
  // the sort may have changed the number of rows
  // before we restore the selection, tell the outliner
  // do this before we call restore selection
  // this is safe when there is no selection.
  rv = AdjustRowCount(rowCountBeforeSort, GetSize());

  RestoreSelection(&preservedSelection);
  if (mOutliner) mOutliner->Invalidate();
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
    if (!NS_SUCCEEDED(rv))
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
        // DMB TODO - This will do for now...Until we decide how to
        // handle thread flags vs. message flags, if we do decide
        // to make them different.
        msgHdr->OrFlags(threadFlags & (MSG_FLAG_WATCHED | MSG_FLAG_IGNORED), &newMsgFlags);
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

nsresult	nsMsgThreadedDBView::ExpandAll()
{
	nsresult rv = NS_OK;
	// go through expanding in place 
	for (PRUint32 i = 0; i < m_keys.GetSize(); i++)
	{
		PRUint32	numExpanded;
		PRUint32	flags = m_flags[i];
		if (flags & MSG_VIEW_FLAG_HASCHILDREN && (flags & MSG_FLAG_ELIDED))
		{
			rv = ExpandByIndex(i, &numExpanded);
			i += numExpanded;
			NS_ENSURE_SUCCESS(rv, rv);
		}
	}
	return rv;
}

void	nsMsgThreadedDBView::OnExtraFlagChanged(nsMsgViewIndex index, PRUint32 extraFlag)
{
	if (IsValidIndex(index) && m_havePrevView)
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
	// we don't really know what's changed, but to be on the safe side, set the sort invalid
	// so that reverse sort will pick it up.
	if (m_sortType == nsMsgViewSortType::byStatus || m_sortType == nsMsgViewSortType::byFlagged || 
      m_sortType == nsMsgViewSortType::byUnread || m_sortType == nsMsgViewSortType::byPriority)
		m_sortValid = PR_FALSE;
}

void nsMsgThreadedDBView::OnHeaderAddedOrDeleted()
{
	ClearPrevIdArray();
  ClearPreSearchInfo();
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
    //		m_db->SetSortInfo(m_sortType, sortOrder);
  }
  else
    m_viewFlags &= ~nsMsgViewFlagsType::kThreadedDisplay;
  
  // by default, the unread only view should have all threads expanded.
  if ((m_viewFlags & nsMsgViewFlagsType::kUnreadOnly) && m_sortType == nsMsgViewSortType::byThread)
    ExpandAll();
  if (sortType != nsMsgViewSortType::byThread)
    ExpandAll(); // for now, expand all and do a flat sort.
  
  Sort(sortType, sortOrder);
  if (sortType != nsMsgViewSortType::byThread)	// forget prev view, since it has everything expanded.
    ClearPrevIdArray();
  return NS_OK;
}

nsresult nsMsgThreadedDBView::OnNewHeader(nsMsgKey newKey, nsMsgKey aParentKey, PRBool ensureListed)
{
  nsresult	rv = NS_OK;
  if (mIsSearchView)
  {
    OnHeaderAddedOrDeleted();  //db has changed and we are not adding hdr to view so clear the cached info..
    return NS_OK; // do not add a new message to search view.
  }
  // views can override this behaviour, which is to append to view.
  // This is the mail behaviour, but threaded views want
  // to insert in order...
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  rv = m_db->GetMsgHdrForKey(newKey, getter_AddRefs(msgHdr));
  if (NS_SUCCEEDED(rv) && msgHdr != nsnull)
  {
    PRUint32 msgFlags;
    msgHdr->GetFlags(&msgFlags);
    if ((m_viewFlags & nsMsgViewFlagsType::kUnreadOnly) && !ensureListed && (msgFlags & MSG_FLAG_READ))
      return NS_OK;
    // Currently, we only add the header in a threaded view if it's a thread.
    // We used to check if this was the first header in the thread, but that's
    // a bit harder in the unreadOnly view. But we'll catch it below.
    if (! (m_viewFlags & nsMsgViewFlagsType::kThreadedDisplay))// || msgHdr->GetMessageKey() == m_messageDB->GetKeyOfFirstMsgInThread(msgHdr->GetMessageKey()))
      rv = AddHdr(msgHdr);
    else	// need to find the thread we added this to so we can change the hasnew flag
      // added message to existing thread, but not to view
    {		// Fix flags on thread header.
      PRInt32 threadCount;
      PRUint32 threadFlags;
      nsMsgViewIndex threadIndex = ThreadIndexOfMsg(newKey, nsMsgViewIndex_None, &threadCount, &threadFlags);
      if (threadIndex != nsMsgViewIndex_None)
      {
        // check if this is now the new thread hdr
        PRUint32	flags = m_flags[threadIndex];
        // if we have a collapsed thread which just got a new
        // top of thread, change the keys array.
        PRInt32 level = FindLevelInThread(msgHdr, threadIndex);
        if ((flags & MSG_FLAG_ELIDED)
          && (!(m_viewFlags & nsMsgViewFlagsType::kUnreadOnly) || !(msgFlags & MSG_FLAG_READ)))
        {
          if (level == 0) {
            nsMsgKey msgKey;
            msgHdr->GetMessageKey(&msgKey);
            m_keys.SetAt(threadIndex, msgKey);
          }
          // note change, to update the parent thread's unread and total counts
          NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
        }
        if (!(flags & MSG_VIEW_FLAG_HASCHILDREN))
        {
          flags |= MSG_VIEW_FLAG_HASCHILDREN | MSG_VIEW_FLAG_ISTHREAD;
          if (!(m_viewFlags & nsMsgViewFlagsType::kUnreadOnly))
            flags |= MSG_FLAG_ELIDED;
          m_flags[threadIndex] = flags;
          NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
        }
        if (!(flags & MSG_FLAG_ELIDED))	// thread is expanded
        {								// insert child into thread
          // levels of other hdrs may have changed!
          PRUint32	newFlags = msgFlags;
          nsMsgViewIndex insertIndex = GetInsertInfoForNewHdr(msgHdr, threadIndex, level);
          // this header is the new king! try collapsing the existing thread,
          // removing it, installing this header as king, and expanding it.
          if (level == 0)	
          {
            CollapseByIndex(threadIndex, nsnull);
            // call base class, so child won't get promoted.
            nsMsgDBView::RemoveByIndex(threadIndex);	
            newFlags |= MSG_VIEW_FLAG_ISTHREAD | MSG_VIEW_FLAG_HASCHILDREN | MSG_FLAG_ELIDED;
          }
          m_keys.InsertAt(insertIndex, newKey);
          m_flags.InsertAt(insertIndex, newFlags, 1);
          m_levels.InsertAt(insertIndex, level);
          NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
          NoteChange(insertIndex, 1, nsMsgViewNotificationCode::insertOrDelete);
          if (level == 0)
            ExpandByIndex(threadIndex, nsnull);
        }
      }
      else // adding msg to thread that's not in view.
      {
        nsCOMPtr <nsIMsgThread> threadHdr;
        m_db->GetThreadContainingMsgHdr(msgHdr, getter_AddRefs(threadHdr));
        if (threadHdr)
        {
          AddMsgToThreadNotInView(threadHdr, msgHdr, ensureListed);
        }
      }
    }
  }
  else
    rv = NS_MSG_MESSAGE_NOT_FOUND;
  return rv;
}

nsMsgViewIndex nsMsgThreadedDBView::GetInsertInfoForNewHdr(nsIMsgDBHdr *newHdr, nsMsgViewIndex threadIndex, PRInt32 targetLevel)
{
  nsMsgKey threadParent;
  newHdr->GetThreadParent(&threadParent);
  nsMsgViewIndex parentIndex = m_keys.FindIndex(threadParent, threadIndex);
  PRInt32 viewSize = GetSize();
  nsMsgViewIndex insertIndex = parentIndex + 1;
  if (parentIndex != nsMsgViewIndex_None)
  {
    PRInt32 parentLevel = m_levels[parentIndex];
    NS_ASSERTION(targetLevel == parentLevel + 1, "levels are screwed up");
    while ((PRInt32) insertIndex < viewSize)
    {
      // loop until we find a message at a level less than or equal to the parent level
      if (m_levels[insertIndex] <= parentLevel)
        break;
      insertIndex++;
    }
  }
  return insertIndex;
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
  PRUint32 numThreadChildren;
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
nsMsgThreadedDBView::OnSearchHit(nsIMsgDBHdr* aMsgHdr, nsIMsgFolder *folder)
{
  NS_ENSURE_ARG(aMsgHdr);
  NS_ENSURE_ARG(folder);
  nsMsgKey msgKey;
  PRUint32 msgFlags;
  aMsgHdr->GetMessageKey(&msgKey);
  aMsgHdr->GetFlags(&msgFlags);
  m_keys.Add(msgKey);
  m_levels.Add(0);
  m_flags.Add(msgFlags);
  if (mOutliner)
    mOutliner->RowCountChanged(m_keys.GetSize() - 1, 1);

  return NS_OK;
}

NS_IMETHODIMP
nsMsgThreadedDBView::OnSearchDone(nsresult status)
{  
  return NS_OK;
}


NS_IMETHODIMP
nsMsgThreadedDBView::OnNewSearch()
{
  if (!mIsSearchView)
  {
    SavePreSearchInfo();  //save the folder view to reload it later. 
  }
  if (mOutliner)
  {
    PRInt32 viewSize = m_keys.GetSize();
    mOutliner->RowCountChanged(0, -viewSize); // all rows gone.
  }
  m_keys.RemoveAll();
  m_levels.RemoveAll();
  m_flags.RemoveAll();
  ClearPrevIdArray(); // previous cached info about non threaded display is not useful
  mIsSearchView = PR_TRUE;  
  return NS_OK;
}

void nsMsgThreadedDBView::SavePreSearchInfo()
{
  ClearPreSearchInfo();
  m_preSearchKeys.InsertAt(0, &m_keys);
  m_preSearchLevels.InsertAt(0, &m_levels);
  m_preSearchFlags.InsertAt(0, &m_flags);
}

void nsMsgThreadedDBView::ClearPreSearchInfo()
{
  m_preSearchKeys.RemoveAll();
  m_preSearchLevels.RemoveAll();
  m_preSearchFlags.RemoveAll();
}
