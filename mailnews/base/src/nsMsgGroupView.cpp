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
#include "nsMsgGroupView.h"
#include "nsIMsgHdr.h"
#include "nsIMsgThread.h"
#include "nsIDBFolderInfo.h"
#include "nsIMsgSearchSession.h"
#include "nsMsgGroupThread.h"
#include "nsITreeColumns.h"

#define MSGHDR_CACHE_LOOK_AHEAD_SIZE  25    // Allocate this more to avoid reallocation on new mail.
#define MSGHDR_CACHE_MAX_SIZE         8192  // Max msghdr cache entries.
#define MSGHDR_CACHE_DEFAULT_SIZE     100

nsMsgGroupView::nsMsgGroupView()
{
}

nsMsgGroupView::~nsMsgGroupView()
{
}

NS_IMETHODIMP nsMsgGroupView::Open(nsIMsgFolder *aFolder, nsMsgViewSortTypeValue aSortType, nsMsgViewSortOrderValue aSortOrder, nsMsgViewFlagsTypeValue aViewFlags, PRInt32 *aCount)
{
  nsCOMPtr <nsISimpleEnumerator> headers;
  nsresult rv = aFolder->GetMsgDatabase(nsnull, getter_AddRefs(m_db));
  NS_ENSURE_SUCCESS(rv, rv);
  m_db->AddListener(this);

  m_viewFlags = aViewFlags;
  m_sortOrder = aSortOrder;
  m_sortType = aSortType;

  nsCOMPtr <nsIDBFolderInfo> dbFolderInfo;
  PersistFolderInfo(getter_AddRefs(dbFolderInfo));

  rv = m_db->EnumerateMessages(getter_AddRefs(headers));
  NS_ENSURE_SUCCESS(rv, rv);
  m_folder = aFolder;
  return OpenWithHdrs(headers, aSortType, aSortOrder, aViewFlags, aCount);
}

/* static */PRIntn PR_CALLBACK ReleaseThread (nsHashKey *aKey, void *thread, void *closure)
{
  nsMsgGroupThread *groupThread = (nsMsgGroupThread *) thread;
  groupThread->Release();
  return kHashEnumerateNext;
}


NS_IMETHODIMP nsMsgGroupView::Close()
{
  // enumerate m_groupsTable releasing the thread objects.
  m_groupsTable.Enumerate(ReleaseThread);
  return nsMsgThreadedDBView::Close();
}

nsHashKey *nsMsgGroupView::AllocHashKeyForHdr(nsIMsgDBHdr *msgHdr)
{
  static nsXPIDLCString cStringKey;
  static nsXPIDLString stringKey;
  switch (m_sortType)
  {
    case nsMsgViewSortType::bySubject:
      (void) msgHdr->GetSubject(getter_Copies(cStringKey));
      return new nsCStringKey(cStringKey.get());
      break;
    case nsMsgViewSortType::byAuthor:
      (void) msgHdr->GetAuthor(getter_Copies(cStringKey));
      return new nsCStringKey(cStringKey.get());
    case nsMsgViewSortType::byAccount:
      {
        nsCOMPtr <nsIMsgDatabase> dbToUse = m_db;
  
        if (!dbToUse) // probably search view
          GetDBForViewIndex(0, getter_AddRefs(dbToUse));

        (void) FetchAccount(msgHdr, getter_Copies(stringKey));
        return new nsStringKey (stringKey.get());

      }
      break;
    case nsMsgViewSortType::byLabel:
      {
        nsMsgLabelValue label;
        msgHdr->GetLabel(&label);
        return new nsPRUint32Key(label);
      }
      break;
    case nsMsgViewSortType::byPriority:
      {
        nsMsgPriorityValue priority;
        msgHdr->GetPriority(&priority);
        return new nsPRUint32Key(priority);
      }
      break;
    case nsMsgViewSortType::byStatus:
      {
        PRUint32 status = 0;

        GetStatusSortValue(msgHdr, &status);
        return new nsPRUint32Key(status);
      }
    case nsMsgViewSortType::byDate:
    {
      PRUint32 ageBucket;
      PRTime dateOfMsg;
	    
      nsresult rv = msgHdr->GetDate(&dateOfMsg);

      PRTime currentTime = PR_Now();
      PRExplodedTime explodedCurrentTime;
      PR_ExplodeTime(currentTime, PR_LocalTimeParameters, &explodedCurrentTime);
      PRExplodedTime explodedMsgTime;
      PR_ExplodeTime(dateOfMsg, PR_LocalTimeParameters, &explodedMsgTime);

      if (explodedCurrentTime.tm_year == explodedMsgTime.tm_year &&
          explodedCurrentTime.tm_month == explodedMsgTime.tm_month &&
          explodedCurrentTime.tm_mday == explodedMsgTime.tm_mday)
      {
        // same day...
        ageBucket = 1;
      } 
      // figure out how many days ago this msg arrived
      else if (LL_CMP(currentTime, >, dateOfMsg))
      {
        // some constants for calculation
        static PRInt64 microSecondsPerSecond;
        static PRInt64 secondsPerDay;
        static PRInt64 microSecondsPerDay;
        static PRInt64 microSecondsPer6Days;
        static PRInt64 microSecondsPer13Days;

        static PRBool bGotConstants = PR_FALSE;
        if ( !bGotConstants )
        {
          // seeds
          LL_I2L  ( microSecondsPerSecond,  PR_USEC_PER_SEC );
          LL_UI2L ( secondsPerDay,          60 * 60 * 24 );
    
          // derivees
          LL_MUL( microSecondsPerDay,   secondsPerDay,      microSecondsPerSecond );
          LL_MUL( microSecondsPer6Days, microSecondsPerDay, 6 );
          LL_MUL( microSecondsPer13Days, microSecondsPerDay, 13 );

          bGotConstants = PR_TRUE;
        }

        // the most recent midnight, counting from current time
        PRInt64 todaysMicroSeconds, mostRecentMidnight;
        LL_MOD( todaysMicroSeconds, currentTime, microSecondsPerDay );
        LL_SUB( mostRecentMidnight, currentTime, todaysMicroSeconds );
        PRInt64 yesterday;
        LL_SUB( yesterday, mostRecentMidnight, microSecondsPerDay );
        // most recent midnight minus 6 days
        PRInt64 mostRecentWeek;
        LL_SUB( mostRecentWeek, mostRecentMidnight, microSecondsPer6Days );

        // was the message sent yesterday?
        if ( LL_CMP( dateOfMsg, >=, yesterday ) )
        { // yes ....
          ageBucket = 2;
        }
        else if (LL_CMP(dateOfMsg, >=, mostRecentWeek))
        {
          ageBucket = 3;
        }
        else
        {
          PRInt64 lastTwoWeeks;
          LL_SUB( lastTwoWeeks, mostRecentMidnight, microSecondsPer13Days);
          ageBucket = LL_CMP(dateOfMsg, >=, lastTwoWeeks) ? 4 : 5;
        }
      }
      return new nsPRUint32Key(ageBucket);
    }
    default:
      NS_ASSERTION(PR_FALSE, "no hash key for this type");
    }
  return nsnull;
}

nsMsgGroupThread *nsMsgGroupView::AddHdrToThread(nsIMsgDBHdr *msgHdr)
{
  nsMsgKey msgKey;
  PRUint32 msgFlags;
  msgHdr->GetMessageKey(&msgKey);
  msgHdr->GetFlags(&msgFlags);
  nsHashKey *hashKey = AllocHashKeyForHdr(msgHdr);
//  if (m_sortType == nsMsgViewSortType::byDate)
//    msgKey = ((nsPRUint32Key *) hashKey)->GetValue();
  nsMsgGroupThread *foundThread = nsnull;
  foundThread = (nsMsgGroupThread *) m_groupsTable.Get(hashKey);
  PRBool newThread = !foundThread;
  nsMsgViewIndex viewIndexOfThread;
  if (!foundThread)
  {
    foundThread = new nsMsgGroupThread(m_db);
    m_groupsTable.Put(hashKey, foundThread);
    foundThread->AddRef();
    if (m_sortType == nsMsgViewSortType::byDate)
    {
      foundThread->m_dummy = PR_TRUE;
      msgFlags |=  MSG_VIEW_FLAG_DUMMY | MSG_VIEW_FLAG_HASCHILDREN;
    }

    nsMsgViewIndex insertIndex = GetInsertIndex(msgHdr);
    if (insertIndex == nsMsgViewIndex_None)
      insertIndex = m_keys.GetSize();
    m_keys.InsertAt(insertIndex, msgKey);
    m_flags.InsertAt(insertIndex, msgFlags | MSG_VIEW_FLAG_ISTHREAD | MSG_FLAG_ELIDED);
    m_levels.InsertAt(insertIndex, 0, 1);
    // if grouped by date, insert dummy header for "age"
    if (m_sortType == nsMsgViewSortType::byDate)
    {
      foundThread->m_keys.InsertAt(0, msgKey/* nsMsgKey_None */);
      foundThread->m_threadKey = ((nsPRUint32Key *) hashKey)->GetValue();
    }
  }
  else
  {
    viewIndexOfThread = GetIndexOfFirstDisplayedKeyInThread(foundThread);
  }
  delete hashKey;
  if (foundThread)
    foundThread->AddChild(msgHdr, nsnull, PR_FALSE, nsnull /* announcer */);
  // check if new hdr became thread root
  if (!newThread && foundThread->m_keys[0] == msgKey)
    m_keys.SetAt(viewIndexOfThread, msgKey);

  return foundThread;
}

NS_IMETHODIMP nsMsgGroupView::OpenWithHdrs(nsISimpleEnumerator *aHeaders, nsMsgViewSortTypeValue aSortType, 
                                        nsMsgViewSortOrderValue aSortOrder, nsMsgViewFlagsTypeValue aViewFlags, 
                                        PRInt32 *aCount)
{
  nsresult rv = NS_OK;

  if (aSortType == nsMsgViewSortType::byThread || aSortType == nsMsgViewSortType::byId
    || aSortType == nsMsgViewSortType::byNone)
    return NS_ERROR_INVALID_ARG;

  m_sortType = aSortType;
  m_sortOrder = aSortOrder;
  m_viewFlags = aViewFlags | nsMsgViewFlagsType::kThreadedDisplay | nsMsgViewFlagsType::kGroupBySort;

  PRBool hasMore;
  nsCOMPtr <nsISupports> supports;
  nsCOMPtr <nsIMsgDBHdr> msgHdr;
  while (NS_SUCCEEDED(rv) && NS_SUCCEEDED(rv = aHeaders->HasMoreElements(&hasMore)) && hasMore)
  {
    nsXPIDLCString cStringKey;
    nsXPIDLString stringKey;
    rv = aHeaders->GetNext(getter_AddRefs(supports));
    if (NS_SUCCEEDED(rv) && supports)
    {
      msgHdr = do_QueryInterface(supports);
      AddHdrToThread(msgHdr);
    }
  }
  *aCount = m_keys.GetSize();
  PRUint32 viewFlag = (m_sortType == nsMsgViewSortType::byDate) ? MSG_VIEW_FLAG_DUMMY : 0;
  //go through the view updating the flags for threads with more than one message...
  for (PRUint32 viewIndex = 0; viewIndex < m_keys.GetSize(); viewIndex++)
  {
    nsCOMPtr <nsIMsgThread> thread;
    GetThreadContainingIndex(viewIndex, getter_AddRefs(thread));
    if (thread)
    {
      PRUint32 numChildren;
      thread->GetNumChildren(&numChildren);
      if (numChildren > 1 || viewFlag)
        OrExtraFlag(viewIndex, viewFlag | MSG_VIEW_FLAG_HASCHILDREN);
    }
  }
  return rv;
}

nsresult nsMsgGroupView::OnNewHeader(nsIMsgDBHdr *newHdr, nsMsgKey aParentKey, PRBool /*ensureListed*/)
{
  nsMsgGroupThread *thread = AddHdrToThread(newHdr); 
  if (thread)
  {
    nsMsgKey msgKey;
    PRUint32 msgFlags;
    newHdr->GetMessageKey(&msgKey);
    newHdr->GetFlags(&msgFlags);

    nsMsgViewIndex threadIndex = ThreadIndexOfMsg(msgKey);
    // may need to fix thread counts
    if (threadIndex != nsMsgViewIndex_None)
    {
      if (! (m_flags[threadIndex] & MSG_FLAG_ELIDED))
      {
        PRUint32 msgIndexInThread = thread->m_keys.IndexOf(msgKey);
        m_keys.InsertAt(threadIndex + msgIndexInThread, msgKey);
        m_flags.InsertAt(threadIndex + msgIndexInThread, msgFlags);
        if (msgIndexInThread > 0)
        {
          m_levels.InsertAt(threadIndex + msgIndexInThread, 1);
        }
        else // insert new header at level 0, and bump old level 0 to 1
        {
          m_levels.InsertAt(threadIndex, 0, 1);
          m_levels.SetAt(threadIndex + 1, 1);
        }
      }
      NoteChange(threadIndex, 1, nsMsgViewNotificationCode::changed);
    }
  }
  // if thread is expanded, we need to add hdr to view...
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupView::OnHdrChange(nsIMsgDBHdr *aHdrChanged, PRUint32 aOldFlags, 
                                      PRUint32 aNewFlags, nsIDBChangeListener *aInstigator)
{
  nsCOMPtr <nsIMsgThread> thread;

  nsresult rv = GetThreadContainingMsgHdr(aHdrChanged, getter_AddRefs(thread));
  NS_ENSURE_SUCCESS(rv, rv);
  PRUint32 deltaFlags = (aOldFlags ^ aNewFlags);
  if (deltaFlags & MSG_FLAG_READ)
    thread->MarkChildRead(aNewFlags & MSG_FLAG_READ);

  return nsMsgDBView::OnHdrChange(aHdrChanged, aOldFlags, aNewFlags, aInstigator);
}

NS_IMETHODIMP nsMsgGroupView::OnHdrDeleted(nsIMsgDBHdr *aHdrDeleted, nsMsgKey aParentKey, PRInt32 aFlags, 
                            nsIDBChangeListener *aInstigator)
{
  nsCOMPtr <nsIMsgThread> thread;

  nsresult rv = GetThreadContainingMsgHdr(aHdrDeleted, getter_AddRefs(thread));
  NS_ENSURE_SUCCESS(rv, rv);
  nsMsgViewIndex viewIndexOfThread = GetIndexOfFirstDisplayedKeyInThread(thread);
  nsHashKey *hashKey = AllocHashKeyForHdr(aHdrDeleted);
  thread->RemoveChildHdr(aHdrDeleted, nsnull);

  nsMsgGroupThread *groupThread = NS_STATIC_CAST(nsMsgGroupThread *, (nsIMsgThread *) thread);
  rv = nsMsgDBView::OnHdrDeleted(aHdrDeleted, aParentKey, aFlags, aInstigator);
  if (groupThread->m_dummy && !groupThread->NumRealChildren())
  {
    thread->RemoveChildAt(0); // get rid of dummy
    nsMsgDBView::RemoveByIndex(viewIndexOfThread);
    NoteChange(viewIndexOfThread, -1, nsMsgViewNotificationCode::insertOrDelete); // an example where view is not the listener - D&D messages
  }
  if (!groupThread->m_keys.GetSize())
  {
    nsHashKey *hashKey = AllocHashKeyForHdr(aHdrDeleted);
    m_groupsTable.Remove(hashKey);
    delete hashKey;
  }
  return rv;
}

NS_IMETHODIMP nsMsgGroupView::GetCellProperties(PRInt32 aRow, nsITreeColumn *aCol, nsISupportsArray *aProperties)
{
  if (m_flags[aRow] & MSG_VIEW_FLAG_DUMMY)
    return aProperties->AppendElement(kDummyMsgAtom);
  return nsMsgDBView::GetCellProperties(aRow, aCol, aProperties);
}

NS_IMETHODIMP nsMsgGroupView::GetCellText(PRInt32 aRow, nsITreeColumn* aCol, nsAString& aValue)
{
  const PRUnichar* colID;
  aCol->GetIdConst(&colID);
  if (m_flags[aRow] & MSG_VIEW_FLAG_DUMMY && colID[0] != 'u')
  {
    nsCOMPtr <nsIMsgDBHdr> msgHdr;
    nsresult rv = GetMsgHdrForViewIndex(aRow, getter_AddRefs(msgHdr));
    nsHashKey *hashKey = AllocHashKeyForHdr(msgHdr);
    nsMsgGroupThread *groupThread = (nsMsgGroupThread *) m_groupsTable.Get(hashKey);
    if (colID[0] == 'd')
    {
      aValue.SetCapacity(0);
      // ### need to localize these...
      // need to get the thread for this row, and use the thread id to switch on.
      switch (((nsPRUint32Key *)hashKey)->GetValue())
      {
      case 1:
        aValue.Assign(NS_LITERAL_STRING("today"));
        break;
      case 2:
        aValue.Assign(NS_LITERAL_STRING("yesterday"));
        break;
      case 3:
        aValue.Assign(NS_LITERAL_STRING("last week"));
        break;
      case 4:
        aValue.Assign(NS_LITERAL_STRING("two weeks ago"));
        break;
      case 5:
        aValue.Assign(NS_LITERAL_STRING("older..."));
        break;
      default:
        NS_ASSERTION(PR_FALSE, "bad age thread");
        break;
      }
    }
    else if (colID[0] == 't')
    {
      nsAutoString formattedCountString;
      PRUint32 numChildren = (groupThread) ? groupThread->NumRealChildren() : 0;
      formattedCountString.AppendInt(numChildren);
      aValue.Assign(formattedCountString);
    }
    return NS_OK;
  }
  // XXX fix me by making Fetch* take an nsAString& parameter
  nsXPIDLString valueText;
  nsCOMPtr <nsIMsgThread> thread;

  return nsMsgDBView::GetCellText(aRow, aCol, aValue);
}

NS_IMETHODIMP nsMsgGroupView::LoadMessageByViewIndex(nsMsgViewIndex aViewIndex)
{

  if (m_flags[aViewIndex] & MSG_VIEW_FLAG_DUMMY)
  {
    // if we used to have one item selected, and now we have more than one, we should clear the message pane.
    nsCOMPtr <nsIMsgMessagePaneController> controller;
    if (mMsgWindow && NS_SUCCEEDED(mMsgWindow->GetMessagePaneController(getter_AddRefs(controller))) && controller)
      controller->ClearMsgPane();
    return NS_OK;
  }
  else
    return nsMsgDBView::LoadMessageByViewIndex(aViewIndex);
}

nsresult nsMsgGroupView::GetThreadContainingMsgHdr(nsIMsgDBHdr *msgHdr, nsIMsgThread **pThread)
{
  nsHashKey *hashKey = AllocHashKeyForHdr(msgHdr);
  nsMsgGroupThread *groupThread = (nsMsgGroupThread *) m_groupsTable.Get(hashKey);
  
  if (groupThread)
    groupThread->QueryInterface(NS_GET_IID(nsIMsgThread), (void **) pThread);
  delete hashKey;
  return (*pThread) ? NS_OK : NS_ERROR_FAILURE;
}

PRInt32 nsMsgGroupView::FindLevelInThread(nsIMsgDBHdr *msgHdr, nsMsgViewIndex startOfThread, nsMsgViewIndex viewIndex)
{
  return (startOfThread == viewIndex) ? 0 : 1;
}


nsMsgViewIndex nsMsgGroupView::ThreadIndexOfMsg(nsMsgKey msgKey, 
                                            nsMsgViewIndex msgIndex /* = nsMsgViewIndex_None */,
                                            PRInt32 *pThreadCount /* = NULL */,
                                            PRUint32 *pFlags /* = NULL */)
{
  if (msgIndex != nsMsgViewIndex_None && m_sortType == nsMsgViewSortType::byDate)
  {
    // this case is all we care about at this point.
    if (m_flags[msgIndex] & MSG_VIEW_FLAG_ISTHREAD)
      return msgIndex;
  }
  return nsMsgDBView::ThreadIndexOfMsg(msgKey, msgIndex, pThreadCount, pFlags);
}
