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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsMsgThread.h"
#include "nsMsgDatabase.h"
#include "nsCOMPtr.h"
#include "MailNewsTypes2.h"

NS_IMPL_ISUPPORTS1(nsMsgThread, nsMsgThread)


nsMsgThread::nsMsgThread()
{
  
  MOZ_COUNT_CTOR(nsMsgThread);
  Init();
}
nsMsgThread::nsMsgThread(nsMsgDatabase *db, nsIMdbTable *table)
{
  MOZ_COUNT_CTOR(nsMsgThread);
  Init();
  m_mdbTable = table;
  m_mdbDB = db;
  if (db)
    db->AddRef();
  
  if (table && db)
  {
    table->GetMetaRow(db->GetEnv(), nsnull, nsnull, &m_metaRow);
    InitCachedValues();
  }
}

void nsMsgThread::Init()
{
  m_threadKey = nsMsgKey_None; 
  m_threadRootKey = nsMsgKey_None;
  m_numChildren = 0;		
  m_numUnreadChildren = 0;	
  m_flags = 0;
  m_mdbTable = nsnull;
  m_mdbDB = nsnull;
  m_metaRow = nsnull;
  m_newestMsgDate = 0;
  m_cachedValuesInitialized = PR_FALSE;
}


nsMsgThread::~nsMsgThread()
{
  MOZ_COUNT_DTOR(nsMsgThread);
  if (m_mdbTable)
    m_mdbTable->Release();
  if (m_mdbDB)
    m_mdbDB->Release();
  if (m_metaRow)
    m_metaRow->Release();
}

nsresult nsMsgThread::InitCachedValues()
{
  nsresult err = NS_OK;
  
  if (!m_mdbDB || !m_metaRow)
    return NS_ERROR_NULL_POINTER;
  
  if (!m_cachedValuesInitialized)
  {
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadFlagsColumnToken, &m_flags);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, &m_numChildren);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadIdColumnToken, &m_threadKey);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, &m_numUnreadChildren);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadRootKeyColumnToken, &m_threadRootKey, nsMsgKey_None);
    err = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadNewestMsgDateColumnToken, &m_newestMsgDate, 0);
    // fix num children if it's wrong. this doesn't work - some DB's have a bogus thread table
    // that is full of bogus headers - don't know why.
    PRUint32 rowCount = 0;
    m_mdbTable->GetCount(m_mdbDB->GetEnv(), &rowCount);
    //    NS_ASSERTION(m_numChildren <= rowCount, "num children wrong - fixing");
    if (m_numChildren > rowCount)
      ChangeChildCount((PRInt32) rowCount - (PRInt32) m_numChildren);
    if ((PRInt32) m_numUnreadChildren < 0)
      ChangeUnreadChildCount(- (PRInt32) m_numUnreadChildren);
    if (NS_SUCCEEDED(err))
      m_cachedValuesInitialized = PR_TRUE;
  }
  return err;
}

NS_IMETHODIMP		nsMsgThread::SetThreadKey(nsMsgKey threadKey)
{
  m_threadKey = threadKey;
  // by definition, the initial thread key is also the thread root key.
  SetThreadRootKey(threadKey);
  // gotta set column in meta row here.
  return m_mdbDB->UInt32ToRowCellColumn(
                    m_metaRow, m_mdbDB->m_threadIdColumnToken, threadKey);
}

NS_IMETHODIMP	nsMsgThread::GetThreadKey(nsMsgKey *result)
{
  if (result)
  {
    nsresult res = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadIdColumnToken, &m_threadKey);
    *result = m_threadKey;
    return res;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgThread::GetFlags(PRUint32 *result)
{
  if (result)
  {
    nsresult res = m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadFlagsColumnToken, &m_flags);
    *result = m_flags;
    return res;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMsgThread::SetFlags(PRUint32 flags)
{
  m_flags = flags;
  return m_mdbDB->UInt32ToRowCellColumn(
                    m_metaRow, m_mdbDB->m_threadFlagsColumnToken, m_flags);
}

NS_IMETHODIMP nsMsgThread::SetSubject(const char *subject)
{
  return m_mdbDB->CharPtrToRowCellColumn(m_metaRow, m_mdbDB->m_threadSubjectColumnToken, subject);
}

NS_IMETHODIMP nsMsgThread::GetSubject(char **result)
{
  if (!result)
    return NS_ERROR_NULL_POINTER;
  return m_mdbDB->RowCellColumnToCharPtr(
                    m_metaRow, m_mdbDB->m_threadSubjectColumnToken, result);
}

NS_IMETHODIMP nsMsgThread::GetNumChildren(PRUint32 *result)
{
  if (result)
  {
    *result = m_numChildren;
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}


NS_IMETHODIMP nsMsgThread::GetNumUnreadChildren (PRUint32 *result)
{
  if (result)
  {
    *result = m_numUnreadChildren;
    return NS_OK;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

nsresult nsMsgThread::RerootThread(nsIMsgDBHdr *newParentOfOldRoot, nsIMsgDBHdr *oldRoot, nsIDBChangeAnnouncer *announcer)
{
  nsCOMPtr <nsIMsgDBHdr> ancestorHdr = newParentOfOldRoot;
  nsMsgKey newRoot;
  newParentOfOldRoot->GetMessageKey(&newRoot);
  mdb_pos outPos;

  nsMsgKey newHdrAncestor;
  ancestorHdr->GetMessageKey(&newRoot);
  nsresult rv = NS_OK;
  // loop trying to find the oldest ancestor of this msg
  // that is a parent of the root. The oldest ancestor will
  // become the root of the thread.
  do 
  {
    ancestorHdr->GetThreadParent(&newHdrAncestor);
    if (newHdrAncestor != nsMsgKey_None && newHdrAncestor != m_threadRootKey && newHdrAncestor != newRoot)
    {
      newRoot = newHdrAncestor;
      rv = m_mdbDB->GetMsgHdrForKey(newRoot, getter_AddRefs(ancestorHdr));
    }
  }
  while (NS_SUCCEEDED(rv) && ancestorHdr && newHdrAncestor != nsMsgKey_None && newHdrAncestor != m_threadRootKey
    && newHdrAncestor != newRoot);
  SetThreadRootKey(newRoot);
  ReparentNonReferenceChildrenOf(oldRoot, newRoot, announcer);
  if (ancestorHdr)
  {
    nsIMsgHdr *msgHdr = ancestorHdr;
    nsMsgHdr* rootMsgHdr = NS_STATIC_CAST(nsMsgHdr*, msgHdr);          // closed system, cast ok
    nsIMdbRow *newRootHdrRow = rootMsgHdr->GetMDBRow();
    // move the  root hdr to pos 0.
    m_mdbTable->MoveRow(m_mdbDB->GetEnv(), newRootHdrRow, -1, 0, &outPos);
    ancestorHdr->SetThreadParent(nsMsgKey_None);
  }
  return rv;
}

NS_IMETHODIMP nsMsgThread::AddChild(nsIMsgDBHdr *child, nsIMsgDBHdr *inReplyTo, PRBool threadInThread, 
                                    nsIDBChangeAnnouncer *announcer)
{
  nsresult ret = NS_OK;
  nsMsgHdr* hdr = NS_STATIC_CAST(nsMsgHdr*, child);          // closed system, cast ok
  PRUint32 newHdrFlags = 0;
  PRUint32 msgDate;
  nsMsgKey newHdrKey = 0;
  PRBool parentKeyNeedsSetting = PR_TRUE;
  
  nsIMdbRow *hdrRow = hdr->GetMDBRow();
  hdr->GetRawFlags(&newHdrFlags);
  hdr->GetMessageKey(&newHdrKey);
  hdr->GetDateInSeconds(&msgDate);
  if (msgDate > m_newestMsgDate)
    SetNewestMsgDate(msgDate);

  if (newHdrFlags & MSG_FLAG_IGNORED)
    SetFlags(m_flags | MSG_FLAG_IGNORED);

  if (newHdrFlags & MSG_FLAG_WATCHED)
    SetFlags(m_flags | MSG_FLAG_WATCHED);

  child->AndFlags(~(MSG_FLAG_WATCHED | MSG_FLAG_IGNORED), &newHdrFlags);
  PRUint32 numChildren;
  PRUint32 childIndex = 0;
  
  // get the num children before we add the new header.
  GetNumChildren(&numChildren);
  
  // if this is an empty thread, set the root key to this header's key
  if (numChildren == 0)
    SetThreadRootKey(newHdrKey);
  
  if (m_mdbTable)
  {
    m_mdbTable->AddRow(m_mdbDB->GetEnv(), hdrRow);
    ChangeChildCount(1);
    if (! (newHdrFlags & MSG_FLAG_READ))
      ChangeUnreadChildCount(1);
  }
  if (inReplyTo)
  {
    nsMsgKey parentKey;
    inReplyTo->GetMessageKey(&parentKey);
    child->SetThreadParent(parentKey);
    parentKeyNeedsSetting = PR_FALSE;
  }
  // check if this header is a parent of one of the messages in this thread
  
  PRBool hdrMoved = PR_FALSE;
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  for (childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsMsgKey msgKey;
    
    ret = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
    if (NS_SUCCEEDED(ret) && curHdr)
    {
      if (hdr->IsParentOf(curHdr))
      {
        nsMsgKey oldThreadParent;
        mdb_pos outPos;
        // move this hdr before the current header.
        if (!hdrMoved)
        {
          m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, childIndex, &outPos);
          hdrMoved = PR_TRUE;
          curHdr->GetThreadParent(&oldThreadParent);
          curHdr->GetMessageKey(&msgKey);
          nsCOMPtr <nsIMsgDBHdr> curParent;
          m_mdbDB->GetMsgHdrForKey(oldThreadParent, getter_AddRefs(curParent));
          if (curParent && hdr->IsAncestorOf(curParent))
          {
            nsMsgKey curParentKey;
            curParent->GetMessageKey(&curParentKey);
            if (curParentKey == m_threadRootKey)
            {
              m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, 0, &outPos);
              RerootThread(child, curParent, announcer);
              parentKeyNeedsSetting = PR_FALSE;
            }
          }
          else if (msgKey == m_threadRootKey)
          {
            RerootThread(child, curHdr, announcer);
            parentKeyNeedsSetting = PR_FALSE;
          }
        }
        curHdr->SetThreadParent(newHdrKey);
        if (msgKey == newHdrKey)
          parentKeyNeedsSetting = PR_FALSE;
        
        // OK, this is a reparenting - need to send notification
        if (announcer)
          announcer->NotifyParentChangedAll(msgKey, oldThreadParent, newHdrKey, nsnull);
#ifdef DEBUG_bienvenu1
        if (newHdrKey != m_threadKey)
          printf("adding second level child\n");
#endif
      }
    }
  }
  // If this header is not a reply to a header in the thread, and isn't a parent
  // check to see if it starts with Re: - if not, and the first header does start
  // with re, should we make this header the top level header?
  // If it's date is less (or it's ID?), then yes.
  if (numChildren > 0 && !(newHdrFlags & MSG_FLAG_HAS_RE) && !inReplyTo)
  {
    PRTime newHdrDate;
    PRTime topLevelHdrDate;
    
    nsCOMPtr <nsIMsgDBHdr> topLevelHdr;
    ret = GetRootHdr(nsnull, getter_AddRefs(topLevelHdr));
    if (NS_SUCCEEDED(ret) && topLevelHdr)
    {
      child->GetDate(&newHdrDate);
      topLevelHdr->GetDate(&topLevelHdrDate);
      if (LL_CMP(newHdrDate, <, topLevelHdrDate))
      {
        RerootThread(child, topLevelHdr, announcer);
        mdb_pos outPos;
        m_mdbTable->MoveRow(m_mdbDB->GetEnv(), hdrRow, -1, 0, &outPos);
        topLevelHdr->SetThreadParent(newHdrKey);
        parentKeyNeedsSetting = PR_FALSE;
        // ### need to get ancestor of new hdr here too.
        SetThreadRootKey(newHdrKey);
        child->SetThreadParent(nsMsgKey_None);
        // argh, here we'd need to adjust all the headers that listed 
        // the demoted header as their thread parent, but only because
        // of subject threading. Adjust them to point to the new parent,
        // that is.
        ReparentNonReferenceChildrenOf(topLevelHdr, newHdrKey, announcer);
      }
    }
  }
  // OK, check to see if we added this header, and didn't parent it.
  
  if (numChildren > 0 && parentKeyNeedsSetting)
    child->SetThreadParent(m_threadRootKey);
  
  // do this after we've put the new hdr in the thread
  if (m_flags & MSG_FLAG_IGNORED && m_mdbDB)
    m_mdbDB->MarkHdrRead(child, PR_TRUE, nsnull);
  
#ifdef DEBUG_bienvenu1
  nsMsgDatabase *msgDB = NS_STATIC_CAST(nsMsgDatabase*, m_mdbDB);
  msgDB->DumpThread(m_threadRootKey);
#endif
  return ret;
}

nsresult nsMsgThread::ReparentNonReferenceChildrenOf(nsIMsgDBHdr *oldTopLevelHdr, nsMsgKey newParentKey,
                                                            nsIDBChangeAnnouncer *announcer)
{
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  PRUint32 numChildren;
  PRUint32 childIndex = 0;
  
  GetNumChildren(&numChildren);
  for (childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsMsgKey oldTopLevelHdrKey;
    
    oldTopLevelHdr->GetMessageKey(&oldTopLevelHdrKey);
    nsresult ret = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
    if (NS_SUCCEEDED(ret) && curHdr)
    {
      nsMsgKey oldThreadParent, curHdrKey;
      nsIMsgDBHdr *curMsgHdr = curHdr;
      nsMsgHdr* oldTopLevelMsgHdr = NS_STATIC_CAST(nsMsgHdr*, oldTopLevelHdr);      // closed system, cast ok
      curHdr->GetThreadParent(&oldThreadParent);
      curHdr->GetMessageKey(&curHdrKey);
      if (oldThreadParent == oldTopLevelHdrKey && curHdrKey != newParentKey && !oldTopLevelMsgHdr->IsParentOf(curHdr))
      {
        curHdr->GetThreadParent(&oldThreadParent);
        curHdr->SetThreadParent(newParentKey);
        // OK, this is a reparenting - need to send notification
        if (announcer)
          announcer->NotifyParentChangedAll(curHdrKey, oldThreadParent, newParentKey, nsnull);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsMsgThread::GetChildKeyAt(PRInt32 aIndex, nsMsgKey *result)
{
  nsresult ret;

  mdbOid oid;
  ret = m_mdbTable->PosToOid( m_mdbDB->GetEnv(), aIndex, &oid);
  if (NS_SUCCEEDED(ret))
    *result = oid.mOid_Id;

  return ret;
}

NS_IMETHODIMP nsMsgThread::GetChildAt(PRInt32 aIndex, nsIMsgDBHdr **result)
{
  nsresult ret;

  mdbOid oid;
  ret = m_mdbTable->PosToOid( m_mdbDB->GetEnv(), aIndex, &oid);
  if (NS_SUCCEEDED(ret))
  {
    nsIMdbRow *hdrRow = nsnull;
    //do I have to release hdrRow?
    ret = m_mdbTable->PosToRow(m_mdbDB->GetEnv(), aIndex, &hdrRow); 
    if(NS_SUCCEEDED(ret) && hdrRow)
    {
      ret = m_mdbDB->CreateMsgHdr(hdrRow,  oid.mOid_Id , result);
    }
  }
  
  return (NS_SUCCEEDED(ret)) ? NS_OK : NS_MSG_MESSAGE_NOT_FOUND;
}


NS_IMETHODIMP nsMsgThread::GetChild(nsMsgKey msgKey, nsIMsgDBHdr **result)
{
  nsresult ret;

  mdb_bool	hasOid;
  mdbOid		rowObjectId;
  
  if (!result || !m_mdbTable)
    return NS_ERROR_NULL_POINTER;
  
  *result = NULL;
  rowObjectId.mOid_Id = msgKey;
  rowObjectId.mOid_Scope = m_mdbDB->m_hdrRowScopeToken;
  ret = m_mdbTable->HasOid(m_mdbDB->GetEnv(), &rowObjectId, &hasOid);
  if (NS_SUCCEEDED(ret) && hasOid && m_mdbDB && m_mdbDB->m_mdbStore)
  {
    nsIMdbRow *hdrRow = nsnull;
    ret = m_mdbDB->m_mdbStore->GetRow(m_mdbDB->GetEnv(), &rowObjectId,  &hdrRow);
    if (NS_SUCCEEDED(ret) && hdrRow && m_mdbDB)
    {
      ret = m_mdbDB->CreateMsgHdr(hdrRow,  msgKey, result);
    }
  }

  return ret;
}


NS_IMETHODIMP nsMsgThread::GetChildHdrAt(PRInt32 aIndex, nsIMsgDBHdr **result)
{
  nsresult ret;

  nsIMdbRow* resultRow;
  mdb_pos pos = aIndex - 1;
  
  if (!result)
    return NS_ERROR_NULL_POINTER;
  
  *result = nsnull;
  // mork doesn't seem to handle this correctly, so deal with going off
  // the end here.
  if (aIndex > (PRInt32) m_numChildren)
    return NS_OK;
  
  nsIMdbTableRowCursor *rowCursor;
  ret = m_mdbTable->GetTableRowCursor(m_mdbDB->GetEnv(), pos, &rowCursor);
  if (NS_FAILED(ret))
    return ret;
  
  ret = rowCursor->NextRow(m_mdbDB->GetEnv(), &resultRow, &pos);
  NS_RELEASE(rowCursor);
  if (NS_FAILED(ret) || !resultRow) 
    return ret;
  
  //Get key from row
  mdbOid outOid;
  nsMsgKey key=0;
  if (resultRow->GetOid(m_mdbDB->GetEnv(), &outOid) == NS_OK)
    key = outOid.mOid_Id;
  
  return m_mdbDB->CreateMsgHdr(resultRow, key, result);
}


NS_IMETHODIMP nsMsgThread::RemoveChildAt(PRInt32 aIndex)
{
  return NS_OK;
}


nsresult nsMsgThread::RemoveChild(nsMsgKey msgKey)
{
  nsresult ret;

  mdbOid		rowObjectId;
  rowObjectId.mOid_Id = msgKey;
  rowObjectId.mOid_Scope = m_mdbDB->m_hdrRowScopeToken;
  ret = m_mdbTable->CutOid(m_mdbDB->GetEnv(), &rowObjectId);
  // if this thread is empty, remove it from the all threads table.
  if (m_numChildren == 0 && m_mdbDB->m_mdbAllThreadsTable)
  {
    mdbOid rowID;
    rowID.mOid_Id = m_threadKey;
    rowID.mOid_Scope = m_mdbDB->m_threadRowScopeToken;
    
    m_mdbDB->m_mdbAllThreadsTable->CutOid(m_mdbDB->GetEnv(), &rowID);
  }
#if 0 // this seems to cause problems
  if (m_numChildren == 0 && m_metaRow && m_mdbDB)
    m_metaRow->CutAllColumns(m_mdbDB->GetEnv());
#endif
  
  return ret;
}

NS_IMETHODIMP nsMsgThread::RemoveChildHdr(nsIMsgDBHdr *child, nsIDBChangeAnnouncer *announcer)
{
  PRUint32 flags;
  nsMsgKey key;
  nsMsgKey threadParent;
  
  if (!child)
    return NS_ERROR_NULL_POINTER;
  
  child->GetFlags(&flags);
  child->GetMessageKey(&key);
  
  child->GetThreadParent(&threadParent);
  ReparentChildrenOf(key, threadParent, announcer);
  
  // if this was the newest msg, clear the newest msg date so we'll recalc.
  PRUint32 date;
  child->GetDateInSeconds(&date);
  if (date == m_newestMsgDate)
    SetNewestMsgDate(0);

 if (!(flags & MSG_FLAG_READ))
    ChangeUnreadChildCount(-1);
  ChangeChildCount(-1);
  return RemoveChild(key);
}

nsresult nsMsgThread::ReparentChildrenOf(nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeAnnouncer *announcer)
{
  nsresult rv = NS_OK;
  
  PRUint32 numChildren;
  PRUint32 childIndex = 0;
  
  GetNumChildren(&numChildren);
  
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  if (numChildren > 0)
  {
    for (childIndex = 0; childIndex < numChildren; childIndex++)
    {
      rv = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
      if (NS_SUCCEEDED(rv) && curHdr)
      {
        nsMsgKey threadParent;
        
        curHdr->GetThreadParent(&threadParent);
        if (threadParent == oldParent)
        {
          nsMsgKey curKey;
          
          curHdr->SetThreadParent(newParent);
          curHdr->GetMessageKey(&curKey);
          if (announcer)
            announcer->NotifyParentChangedAll(curKey, oldParent, newParent, nsnull);
          // if the old parent was the root of the thread, then only the first child gets 
          // promoted to root, and other children become children of the new root.
          if (newParent == nsMsgKey_None)
          {
            SetThreadRootKey(curKey);
            newParent = curKey;
          }
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgThread::MarkChildRead(PRBool bRead)
{
  ChangeUnreadChildCount(bRead ? -1 : 1);
  return NS_OK;
}

class nsMsgThreadEnumerator : public nsISimpleEnumerator {
public:
  NS_DECL_ISUPPORTS
    
  // nsISimpleEnumerator methods:
  NS_DECL_NSISIMPLEENUMERATOR
    
  // nsMsgThreadEnumerator methods:
  typedef nsresult (*nsMsgThreadEnumeratorFilter)(nsIMsgDBHdr* hdr, void* closure);
  
  nsMsgThreadEnumerator(nsMsgThread *thread, nsMsgKey startKey,
  nsMsgThreadEnumeratorFilter filter, void* closure);
  PRInt32 MsgKeyFirstChildIndex(nsMsgKey inMsgKey);
  virtual ~nsMsgThreadEnumerator();
  
protected:
  
  nsresult                Prefetch();
  
  nsIMdbTableRowCursor*   mRowCursor;
  nsCOMPtr <nsIMsgDBHdr>  mResultHdr;
  nsMsgThread*	          mThread;
  nsMsgKey                mThreadParentKey;
  nsMsgKey                mFirstMsgKey;
  PRInt32                 mChildIndex;
  PRBool                  mDone;
  PRBool                  mNeedToPrefetch;
  nsMsgThreadEnumeratorFilter     mFilter;
  void*                   mClosure;
  PRBool                  mFoundChildren;
};

nsMsgThreadEnumerator::nsMsgThreadEnumerator(nsMsgThread *thread, nsMsgKey startKey,
                                             nsMsgThreadEnumeratorFilter filter, void* closure)
                                             : mRowCursor(nsnull), mDone(PR_FALSE),
                                             mFilter(filter), mClosure(closure), mFoundChildren(PR_FALSE)
{
  mThreadParentKey = startKey;
  mChildIndex = 0;
  mThread = thread;
  mNeedToPrefetch = PR_TRUE;
  mFirstMsgKey = nsMsgKey_None;
  
  nsresult rv = mThread->GetRootHdr(nsnull, getter_AddRefs(mResultHdr));
  
  if (NS_SUCCEEDED(rv) && mResultHdr)
    mResultHdr->GetMessageKey(&mFirstMsgKey);
  
  PRUint32 numChildren;
  mThread->GetNumChildren(&numChildren);
  
  if (mThreadParentKey != nsMsgKey_None)
  {
    nsMsgKey msgKey = nsMsgKey_None;
    PRUint32 childIndex = 0;
    
    
    for (childIndex = 0; childIndex < numChildren; childIndex++)
    {
      rv = mThread->GetChildHdrAt(childIndex, getter_AddRefs(mResultHdr));
      if (NS_SUCCEEDED(rv) && mResultHdr)
      {
        mResultHdr->GetMessageKey(&msgKey);
        
        if (msgKey == startKey)
        {
          mChildIndex = MsgKeyFirstChildIndex(msgKey);
          mDone = (mChildIndex < 0);
          break;
        }
        
        if (mDone)
          break;
        
      }
      else
        NS_ASSERTION(PR_FALSE, "couldn't get child from thread");
    }
  }
  
#ifdef DEBUG_bienvenu1
  nsCOMPtr <nsIMsgDBHdr> child;
  for (PRUint32 childIndex = 0; childIndex < numChildren; childIndex++)
  {
    rv = mThread->GetChildHdrAt(childIndex, getter_AddRefs(child));
    if (NS_SUCCEEDED(rv) && child)
    {
      nsMsgKey threadParent;
      nsMsgKey msgKey;
      // we're only doing one level of threading, so check if caller is
      // asking for children of the first message in the thread or not.
      // if not, we will tell him there are no children.
      child->GetMessageKey(&msgKey);
      child->GetThreadParent(&threadParent);
      
      printf("index = %ld key = %ld parent = %lx\n", childIndex, msgKey, threadParent);
    }
  }
#endif
  NS_ADDREF(thread);
}

nsMsgThreadEnumerator::~nsMsgThreadEnumerator()
{
    NS_RELEASE(mThread);
}

NS_IMPL_ISUPPORTS1(nsMsgThreadEnumerator, nsISimpleEnumerator)


PRInt32 nsMsgThreadEnumerator::MsgKeyFirstChildIndex(nsMsgKey inMsgKey)
{
  //	if (msgKey != mThreadParentKey)
  //		mDone = PR_TRUE;
  // look through rest of thread looking for a child of this message.
  // If the inMsgKey is the first message in the thread, then all children
  // without parents are considered to be children of inMsgKey.
  // Otherwise, only true children qualify.
  PRUint32 numChildren;
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  PRInt32 firstChildIndex = -1;
  
  mThread->GetNumChildren(&numChildren);
  
  // if this is the first message in the thread, just check if there's more than
  // one message in the thread.
  //	if (inMsgKey == mThread->m_threadRootKey)
  //		return (numChildren > 1) ? 1 : -1;
  
  for (PRUint32 curChildIndex = 0; curChildIndex < numChildren; curChildIndex++)
  {
    nsresult rv = mThread->GetChildHdrAt(curChildIndex, getter_AddRefs(curHdr));
    if (NS_SUCCEEDED(rv) && curHdr)
    {
      nsMsgKey parentKey;
      
      curHdr->GetThreadParent(&parentKey);
      if (parentKey == inMsgKey)
      {
        firstChildIndex = curChildIndex;
        break;
      }
    }
  }
#ifdef DEBUG_bienvenu1
  printf("first child index of %ld = %ld\n", inMsgKey, firstChildIndex);
#endif
  return firstChildIndex;
}

NS_IMETHODIMP nsMsgThreadEnumerator::GetNext(nsISupports **aItem)
{
  if (!aItem)
    return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;
  
  if (mNeedToPrefetch)
    rv = Prefetch();
  
  if (NS_SUCCEEDED(rv) && mResultHdr) 
  {
    *aItem = mResultHdr;
    NS_ADDREF(*aItem);
    mNeedToPrefetch = PR_TRUE;
  }
  return rv;
}

nsresult nsMsgThreadEnumerator::Prefetch()
{
  nsresult rv=NS_OK;          // XXX or should this default to an error?
  mResultHdr = nsnull;
  if (mThreadParentKey == nsMsgKey_None)
  {
    rv = mThread->GetRootHdr(&mChildIndex, getter_AddRefs(mResultHdr));
    NS_ASSERTION(NS_SUCCEEDED(rv) && mResultHdr, "better be able to get root hdr");
    mChildIndex = 0; // since root can be anywhere, set mChildIndex to 0.
  }
  else if (!mDone)
  {
    PRUint32 numChildren;
    mThread->GetNumChildren(&numChildren);
    
    while (mChildIndex < (PRInt32) numChildren)
    {
      rv  = mThread->GetChildHdrAt(mChildIndex++, getter_AddRefs(mResultHdr));
      if (NS_SUCCEEDED(rv) && mResultHdr)
      {
        nsMsgKey parentKey;
        nsMsgKey curKey;
        
        if (mFilter && NS_FAILED(mFilter(mResultHdr, mClosure))) {
          mResultHdr = nsnull;
          continue;
        }
        
        mResultHdr->GetThreadParent(&parentKey);
        mResultHdr->GetMessageKey(&curKey);
        // if the parent is the same as the msg we're enumerating over,
        // or the parentKey isn't set, and we're iterating over the top
        // level message in the thread, then leave mResultHdr set to cur msg.
        if (parentKey == mThreadParentKey || 
          (parentKey == nsMsgKey_None 
          && mThreadParentKey == mFirstMsgKey && curKey != mThreadParentKey))
          break;
        mResultHdr = nsnull;
      }
      else
        NS_ASSERTION(PR_FALSE, "better be able to get child");
    }
    if (!mResultHdr && mThreadParentKey == mFirstMsgKey && !mFoundChildren && numChildren > 1)
    {
      mThread->ReparentMsgsWithInvalidParent(numChildren, mThreadParentKey);
    }
  }
  if (!mResultHdr) 
  {
    mDone = PR_TRUE;
    return NS_ERROR_FAILURE;
  }
  if (NS_FAILED(rv)) 
  {
    mDone = PR_TRUE;
    return rv;
  }
  else
    mNeedToPrefetch = PR_FALSE;
  mFoundChildren = PR_TRUE;

#ifdef DEBUG_bienvenu1
	nsMsgKey debugMsgKey;
	mResultHdr->GetMessageKey(&debugMsgKey);
	printf("next for %ld = %ld\n", mThreadParentKey, debugMsgKey);
#endif

    return rv;
}

NS_IMETHODIMP nsMsgThreadEnumerator::HasMoreElements(PRBool *aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  if (mNeedToPrefetch)
    Prefetch();
  *aResult = !mDone;
  return NS_OK;
}

NS_IMETHODIMP nsMsgThread::EnumerateMessages(nsMsgKey parentKey, nsISimpleEnumerator* *result)
{
    nsMsgThreadEnumerator* e = new nsMsgThreadEnumerator(this, parentKey, nsnull, nsnull);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;

    return NS_OK;
}

nsresult nsMsgThread::ReparentMsgsWithInvalidParent(PRUint32 numChildren, nsMsgKey threadParentKey)
{
  nsresult ret = NS_OK;
  // run through looking for messages that don't have a correct parent, 
  // i.e., a parent that's in the thread!
  for (PRInt32 childIndex = 0; childIndex < (PRInt32) numChildren; childIndex++)
  {
    nsCOMPtr <nsIMsgDBHdr> curChild;
    ret  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
    if (NS_SUCCEEDED(ret) && curChild)
    {
      nsMsgKey parentKey;
      nsCOMPtr <nsIMsgDBHdr> parent;
      
      curChild->GetThreadParent(&parentKey);
      
      if (parentKey != nsMsgKey_None)
      {
        GetChild(parentKey, getter_AddRefs(parent));
        if (!parent)
          curChild->SetThreadParent(threadParentKey);
      }
    }
  }
  return ret;
}

NS_IMETHODIMP nsMsgThread::GetRootHdr(PRInt32 *resultIndex, nsIMsgDBHdr **result)
{
  if (!result)
    return NS_ERROR_NULL_POINTER;
  
  *result = nsnull;
  
  if (m_threadRootKey != nsMsgKey_None)
  {
    nsresult ret = GetChildHdrForKey(m_threadRootKey, result, resultIndex);
    if (NS_SUCCEEDED(ret) && *result)
      return ret;
    else
    {
#ifdef DEBUG_bienvenu
      printf("need to reset thread root key\n");
#endif
      PRUint32 numChildren;
      nsMsgKey threadParentKey = nsMsgKey_None;
      GetNumChildren(&numChildren);
      
      for (PRInt32 childIndex = 0; childIndex < (PRInt32) numChildren; childIndex++)
      {
        nsCOMPtr <nsIMsgDBHdr> curChild;
        ret  = GetChildHdrAt(childIndex, getter_AddRefs(curChild));
        if (NS_SUCCEEDED(ret) && curChild)
        {
          nsMsgKey parentKey;
          
          curChild->GetThreadParent(&parentKey);
          if (parentKey == nsMsgKey_None)
          {
            NS_ASSERTION(!(*result), "two top level msgs, not good");
            curChild->GetMessageKey(&threadParentKey);
            SetThreadRootKey(threadParentKey);
            if (resultIndex)
              *resultIndex = childIndex;
            *result = curChild;
            NS_ADDREF(*result);
            ReparentMsgsWithInvalidParent(numChildren, threadParentKey);
            //            return NS_OK;
          }
        }
      }
      if (*result)
      {
        return NS_OK;
      }
    }
    // if we can't get the thread root key, we'll just get the first hdr.
    // there's a bug where sometimes we weren't resetting the thread root key 
    // when removing the thread root key.
  }
  if (resultIndex)
    *resultIndex = 0;
  return GetChildHdrAt(0, result);
}

nsresult nsMsgThread::ChangeChildCount(PRInt32 delta)
{
  nsresult ret;

  PRUint32 childCount = 0;
  m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);
  
  NS_ASSERTION(childCount != 0 || delta > 0, "child count gone negative");
  childCount += delta;
  
  NS_ASSERTION((PRInt32) childCount >= 0, "child count gone to 0 or below");
  if ((PRInt32) childCount < 0)	// force child count to >= 0
    childCount = 0;
  
  ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadChildrenColumnToken, childCount);
  m_numChildren = childCount;
  return ret;
}

nsresult nsMsgThread::ChangeUnreadChildCount(PRInt32 delta)
{
  nsresult ret;

  PRUint32 childCount = 0;
  m_mdbDB->RowCellColumnToUInt32(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
  childCount += delta;
  if ((PRInt32) childCount < 0)
  {
#ifdef DEBUG_bienvenu1
    NS_ASSERTION(PR_FALSE, "negative unread child count");
#endif
    childCount = 0;
  }
  ret = m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadUnreadChildrenColumnToken, childCount);
  m_numUnreadChildren = childCount;
  return ret;
}

nsresult nsMsgThread::SetThreadRootKey(nsMsgKey threadRootKey)
{
  m_threadRootKey = threadRootKey;
  return m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadRootKeyColumnToken, threadRootKey);
}

nsresult nsMsgThread::GetChildHdrForKey(nsMsgKey desiredKey, nsIMsgDBHdr **result, PRInt32 *resultIndex)
{
  PRUint32 numChildren;
  PRUint32 childIndex = 0;
  nsresult rv = NS_OK;        // XXX or should this default to an error?
  
  if (!result)
    return NS_ERROR_NULL_POINTER;
  
  GetNumChildren(&numChildren);
  
  if ((PRInt32) numChildren < 0)
    numChildren = 0;
  
  for (childIndex = 0; childIndex < numChildren; childIndex++)
  {
    rv = GetChildHdrAt(childIndex, result);
    if (NS_SUCCEEDED(rv) && *result)
    {
      nsMsgKey msgKey;
      // we're only doing one level of threading, so check if caller is
      // asking for children of the first message in the thread or not.
      // if not, we will tell him there are no children.
      (*result)->GetMessageKey(&msgKey);
      
      if (msgKey == desiredKey)
      {
        nsMsgKey threadKey;
        (*result)->GetThreadId(&threadKey);
        if (threadKey != m_threadKey) // this msg isn't in this thread
        {
          PRUint32 msgSize;
          (*result)->GetMessageSize(&msgSize);
          if (msgSize == 0) // this is a phantom message - let's get rid of it.
            RemoveChild(msgKey);
          rv = NS_ERROR_UNEXPECTED;
        }
        break;
      }
      NS_RELEASE(*result);
    }
  }
  if (resultIndex)
    *resultIndex = childIndex;
  
  return rv;
}

NS_IMETHODIMP nsMsgThread::GetFirstUnreadChild(nsIMsgDBHdr **result)
{
  NS_ENSURE_ARG(result);
  PRUint32 numChildren;
  nsresult rv = NS_OK;
  
  GetNumChildren(&numChildren);
  
  if ((PRInt32) numChildren < 0)
    numChildren = 0;
  
  for (PRUint32 childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsCOMPtr <nsIMsgDBHdr> child;
    rv = GetChildHdrAt(childIndex, getter_AddRefs(child));
    if (NS_SUCCEEDED(rv) && child)
    {
      nsMsgKey msgKey;
      child->GetMessageKey(&msgKey);
      
      PRBool isRead;
      rv = m_mdbDB->IsRead(msgKey, &isRead);
      if (NS_SUCCEEDED(rv) && !isRead)
      {
        *result = child;
        NS_ADDREF(*result);
        break;
      }
    }
  }
  
  return rv;
}

NS_IMETHODIMP nsMsgThread::GetNewestMsgDate(PRUint32 *aResult) 
{
  // if this hasn't been set, figure it out by enumerating the msgs in the thread.
  if (!m_newestMsgDate)
  {
    PRUint32 numChildren;
    nsresult rv = NS_OK;
  
    GetNumChildren(&numChildren);
  
    if ((PRInt32) numChildren < 0)
      numChildren = 0;
  
    for (PRUint32 childIndex = 0; childIndex < numChildren; childIndex++)
    {
      nsCOMPtr <nsIMsgDBHdr> child;
      rv = GetChildHdrAt(childIndex, getter_AddRefs(child));
      if (NS_SUCCEEDED(rv) && child)
      {
        PRUint32 msgDate;
        child->GetDateInSeconds(&msgDate);
        if (msgDate > m_newestMsgDate)
          m_newestMsgDate = msgDate;
      }
    }
  
  }
  *aResult = m_newestMsgDate;
  return NS_OK;
}


NS_IMETHODIMP nsMsgThread::SetNewestMsgDate(PRUint32 aNewestMsgDate) 
{
  m_newestMsgDate = aNewestMsgDate;
  return m_mdbDB->UInt32ToRowCellColumn(m_metaRow, m_mdbDB->m_threadNewestMsgDateColumnToken, aNewestMsgDate);
}

