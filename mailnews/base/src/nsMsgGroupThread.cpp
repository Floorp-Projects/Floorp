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
#include "nsMsgGroupThread.h"

NS_IMPL_ISUPPORTS1(nsMsgGroupThread, nsIMsgThread);

nsMsgGroupThread::nsMsgGroupThread()
{
  Init();
}
nsMsgGroupThread::nsMsgGroupThread(nsIMsgDatabase *db)
{
  m_db = db;
  Init();
}

void nsMsgGroupThread::Init()
{
  m_threadKey = nsMsgKey_None; 
  m_threadRootKey = nsMsgKey_None;
  m_numUnreadChildren = 0;	
  m_flags = 0;
  m_newestMsgDate = 0;
  m_dummy = PR_FALSE;
}


nsMsgGroupThread::~nsMsgGroupThread()
{
}


NS_IMETHODIMP nsMsgGroupThread::SetThreadKey(nsMsgKey threadKey)
{
  m_threadKey = threadKey;
  // by definition, the initial thread key is also the thread root key.
  m_threadRootKey = threadKey;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetThreadKey(nsMsgKey *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = m_threadKey;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetFlags(PRUint32 *aFlags)
{
  NS_ENSURE_ARG_POINTER(aFlags);
  *aFlags = m_flags;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::SetFlags(PRUint32 aFlags)
{
  m_flags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::SetSubject(const char *subject)
{
  NS_ASSERTION(PR_FALSE, "shouldn't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgGroupThread::GetSubject(char **result)
{
  NS_ASSERTION(PR_FALSE, "shouldn't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgGroupThread::GetNumChildren(PRUint32 *aNumChildren)
{
  NS_ENSURE_ARG_POINTER(aNumChildren);
  *aNumChildren = m_keys.GetSize(); // - ((m_dummy) ? 1 : 0);
  return NS_OK;
}

PRUint32 nsMsgGroupThread::NumRealChildren()
{
  return m_keys.GetSize() - ((m_dummy) ? 1 : 0);
}

NS_IMETHODIMP nsMsgGroupThread::GetNumUnreadChildren (PRUint32 *aNumUnreadChildren)
{
  NS_ENSURE_ARG_POINTER(aNumUnreadChildren);
  *aNumUnreadChildren = m_numUnreadChildren;
  return NS_OK;
}
#if 0
nsresult nsMsgGroupThread::RerootThread(nsIMsgDBHdr *newParentOfOldRoot, nsIMsgDBHdr *oldRoot, nsIDBChangeAnnouncer *announcer)
{
  nsCOMPtr <nsIMsgDBHdr> ancestorHdr = newParentOfOldRoot;
  nsMsgKey newRoot;
  newParentOfOldRoot->GetMessageKey(&newRoot);

  nsMsgKey newHdrAncestor;
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
      rv = m_db->GetMsgHdrForKey(newRoot, getter_AddRefs(ancestorHdr));
    }
  }
  while (NS_SUCCEEDED(rv) && ancestorHdr && newHdrAncestor != nsMsgKey_None && newHdrAncestor != m_threadRootKey
    && newHdrAncestor != newRoot);
  m_threadRootKey = newRoot;
//  ReparentNonReferenceChildrenOf(oldRoot, newRoot, announcer);
  if (ancestorHdr)
  {
    // move the  root hdr to pos 0 by removing it and adding it at 0.
    m_keys.RemoveElement(newRoot);
    m_keys.InsertAt(0, newRoot);
    ancestorHdr->SetThreadParent(nsMsgKey_None);
  }
  return rv;
}
#endif
nsresult nsMsgGroupThread::AddMsgHdrInDateOrder(nsIMsgDBHdr *child)
{
  nsresult ret = NS_OK;
  PRBool keyAdded = PR_FALSE;
  nsMsgKey newHdrKey;
  child->GetMessageKey(&newHdrKey);
  // since we're sorted by date, we could do a binary search for the 
  // insert point. Or, we could start at the end...
  if (m_keys.GetSize() > 0)
  {
    nsCOMPtr <nsIMsgDBHdr> topLevelHdr;
    PRTime newHdrDate;
    PRTime topLevelHdrDate;
    PRBool done = PR_FALSE;

    child->GetDate(&newHdrDate);
    nsCOMPtr <nsIMsgDBHdr> curHdr;
    for (PRUint32 childIndex = m_keys.GetSize() - 1; !done && !keyAdded; childIndex--)
    {
      PRTime curHdrDate;
      
      ret = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
      if (NS_SUCCEEDED(ret) && curHdr)
      {
        curHdr->GetDate(&curHdrDate);
        if (LL_CMP(newHdrDate, >, curHdrDate))
        {
          m_keys.InsertAt(childIndex + 1, newHdrKey);
          keyAdded = PR_TRUE;
          break;
        }
      }
      done = !childIndex;
    }
  }
  if (!keyAdded)
  {
    m_keys.InsertAt(0, newHdrKey);
    m_threadRootKey = newHdrKey;
  }
  return ret;
}

// if threadInThread is false, we're doing a non-thread grouping (e.g., by age or sender)
NS_IMETHODIMP nsMsgGroupThread::AddChild(nsIMsgDBHdr *child, nsIMsgDBHdr *inReplyTo, PRBool threadInThread, 
                                    nsIDBChangeAnnouncer *announcer)
{
  nsresult ret = NS_OK;
  PRUint32 newHdrFlags = 0;
  PRUint32 msgDate;
  nsMsgKey newHdrKey = 0;
  PRBool parentKeyNeedsSetting = PR_TRUE;
  PRBool hdrAddedToThread = PR_FALSE;
  
  child->GetFlags(&newHdrFlags);
  child->GetMessageKey(&newHdrKey);
  child->GetDateInSeconds(&msgDate);
  if (msgDate > m_newestMsgDate)
    SetNewestMsgDate(msgDate);

  if (threadInThread && newHdrFlags & MSG_FLAG_IGNORED)
    SetFlags(m_flags | MSG_FLAG_IGNORED);

  if (threadInThread && newHdrFlags & MSG_FLAG_WATCHED)
    SetFlags(m_flags | MSG_FLAG_WATCHED);

  child->AndFlags(~(MSG_FLAG_WATCHED | MSG_FLAG_IGNORED), &newHdrFlags);
  PRUint32 numChildren;
  PRUint32 childIndex = 0;
  
  // get the num children before we add the new header.
  GetNumChildren(&numChildren);
  
  // if this is an empty thread, set the root key to this header's key
  if (numChildren == 0)
    m_threadRootKey = newHdrKey;
  
  if (! (newHdrFlags & MSG_FLAG_READ))
    ChangeUnreadChildCount(1);

  if (!threadInThread)
    return AddMsgHdrInDateOrder(child);
#if 0
  // we can't diddle the msg hdr structures for threading, because that will
  // affect the persistent db. We'll need to store the threading info in the thread object
  // somehow...perhaps by using a levels array.
  if (inReplyTo)
  {
    nsMsgKey parentKey;
    inReplyTo->GetMessageKey(&parentKey);
    child->SetThreadParent(parentKey);
    parentKeyNeedsSetting = PR_FALSE;
  }
  // check if this header is a parent of one of the messages in this thread
  
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  for (childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsMsgKey msgKey;
    
    ret = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
    if (NS_SUCCEEDED(ret) && curHdr)
    {
      if (child->IsParentOf(curHdr))
      {
        nsMsgKey oldThreadParent;
        // move this hdr before the current header.
        m_keys.InsertAt(childIndex, newHdrKey);
        hdrAddedToThread = PR_TRUE;
        curHdr->GetThreadParent(&oldThreadParent);
        curHdr->GetMessageKey(&msgKey);
        if (msgKey == m_threadRootKey)
        {
          RerootThread(child, curHdr, announcer);
          parentKeyNeedsSetting = PR_FALSE;
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
        // If this hdr was the root, then the new hdr is the root (or its ancestor, if it has any)
        break;
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
        m_keys.Remove(newHdrKey);
        m_keys.InsertAt(0, newHdrKey);
        hdrAddedToThread = PR_TRUE;
        topLevelHdr->SetThreadParent(newHdrKey);
        parentKeyNeedsSetting = PR_FALSE;
        // ### need to get ancestor of new hdr here too.
        m_threadRootKey = newHdrKey;
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
  
  if (!hdrAddedToThread)
    m_keys.Add(newHdrKey);
#endif
  return ret;
}

nsresult nsMsgGroupThread::ReparentNonReferenceChildrenOf(nsIMsgDBHdr *topLevelHdr, nsMsgKey newParentKey,
                                                            nsIDBChangeAnnouncer *announcer)
{
#if 0
  nsCOMPtr <nsIMsgDBHdr> curHdr;
  PRUint32 numChildren;
  PRUint32 childIndex = 0;
  
  GetNumChildren(&numChildren);
  for (childIndex = 0; childIndex < numChildren; childIndex++)
  {
    nsMsgKey msgKey;
    
    topLevelHdr->GetMessageKey(&msgKey);
    nsresult ret = GetChildHdrAt(childIndex, getter_AddRefs(curHdr));
    if (NS_SUCCEEDED(ret) && curHdr)
    {
      nsMsgKey oldThreadParent, curHdrKey;
      nsIMsgDBHdr *curMsgHdr = curHdr;
      curHdr->GetThreadParent(&oldThreadParent);
      curHdr->GetMessageKey(&curHdrKey);
      if (oldThreadParent == msgKey && curHdrKey != newParentKey && topLevelMsgHdr->IsParentOf(curHdr))
      {
        curHdr->GetThreadParent(&oldThreadParent);
        curHdr->SetThreadParent(newParentKey);
        // OK, this is a reparenting - need to send notification
        if (announcer)
          announcer->NotifyParentChangedAll(curHdrKey, oldThreadParent, newParentKey, nsnull);
      }
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetChildKeyAt(PRInt32 aIndex, nsMsgKey *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  if (aIndex >= m_keys.GetSize())
    return NS_ERROR_INVALID_ARG;
  *aResult = m_keys[aIndex];
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::GetChildAt(PRInt32 aIndex, nsIMsgDBHdr **aResult)
{
  if (aIndex >= m_keys.GetSize())
    return NS_MSG_MESSAGE_NOT_FOUND;
  return m_db->GetMsgHdrForKey(m_keys[aIndex], aResult);
}


NS_IMETHODIMP nsMsgGroupThread::GetChild(nsMsgKey msgKey, nsIMsgDBHdr **aResult)
{
  PRUint32 childIndex = m_keys.IndexOf(msgKey);
  return (childIndex != kNotFound) ? GetChildAt(childIndex, aResult) : NS_MSG_MESSAGE_NOT_FOUND;
}


NS_IMETHODIMP nsMsgGroupThread::GetChildHdrAt(PRInt32 aIndex, nsIMsgDBHdr **aResult)
{
  return GetChildAt(aIndex, aResult);
}


NS_IMETHODIMP nsMsgGroupThread::RemoveChildAt(PRInt32 aIndex)
{
  return NS_OK;
}


nsresult nsMsgGroupThread::RemoveChild(nsMsgKey msgKey)
{
  PRUint32 childIndex = m_keys.IndexOf(msgKey);
  if (childIndex != kNotFound)
    m_keys.RemoveAt(childIndex);
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::RemoveChildHdr(nsIMsgDBHdr *child, nsIDBChangeAnnouncer *announcer)
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
  return RemoveChild(key);
}

nsresult nsMsgGroupThread::ReparentChildrenOf(nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeAnnouncer *announcer)
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
            m_threadRootKey = curKey;
            newParent = curKey;
          }
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsMsgGroupThread::MarkChildRead(PRBool bRead)
{
  ChangeUnreadChildCount(bRead ? -1 : 1);
  return NS_OK;
}

// this could be moved into utils, because I think it's the same as the db impl.
class nsMsgGroupThreadEnumerator : public nsISimpleEnumerator {
public:
  NS_DECL_ISUPPORTS
    
  // nsISimpleEnumerator methods:
  NS_DECL_NSISIMPLEENUMERATOR
    
  // nsMsgGroupThreadEnumerator methods:
  typedef nsresult (*nsMsgGroupThreadEnumeratorFilter)(nsIMsgDBHdr* hdr, void* closure);
  
  nsMsgGroupThreadEnumerator(nsMsgGroupThread *thread, nsMsgKey startKey,
  nsMsgGroupThreadEnumeratorFilter filter, void* closure);
  PRInt32 MsgKeyFirstChildIndex(nsMsgKey inMsgKey);
  virtual ~nsMsgGroupThreadEnumerator();
  
protected:
  
  nsresult                Prefetch();
  
  nsCOMPtr <nsIMsgDBHdr>  mResultHdr;
  nsMsgGroupThread*       mThread;
  nsMsgKey                mThreadParentKey;
  nsMsgKey                mFirstMsgKey;
  PRInt32                 mChildIndex;
  PRBool                  mDone;
  PRBool                  mNeedToPrefetch;
  nsMsgGroupThreadEnumeratorFilter     mFilter;
  void*                   mClosure;
  PRBool                  mFoundChildren;
};

nsMsgGroupThreadEnumerator::nsMsgGroupThreadEnumerator(nsMsgGroupThread *thread, nsMsgKey startKey,
                                             nsMsgGroupThreadEnumeratorFilter filter, void* closure)
                                             : mDone(PR_FALSE),
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

nsMsgGroupThreadEnumerator::~nsMsgGroupThreadEnumerator()
{
    NS_RELEASE(mThread);
}

NS_IMPL_ISUPPORTS1(nsMsgGroupThreadEnumerator, nsISimpleEnumerator)


PRInt32 nsMsgGroupThreadEnumerator::MsgKeyFirstChildIndex(nsMsgKey inMsgKey)
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

NS_IMETHODIMP nsMsgGroupThreadEnumerator::GetNext(nsISupports **aItem)
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

nsresult nsMsgGroupThreadEnumerator::Prefetch()
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
//      mThread->ReparentMsgsWithInvalidParent(numChildren, mThreadParentKey);
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

NS_IMETHODIMP nsMsgGroupThreadEnumerator::HasMoreElements(PRBool *aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  if (mNeedToPrefetch)
    Prefetch();
  *aResult = !mDone;
  return NS_OK;
}

NS_IMETHODIMP nsMsgGroupThread::EnumerateMessages(nsMsgKey parentKey, nsISimpleEnumerator* *result)
{
    nsMsgGroupThreadEnumerator* e = new nsMsgGroupThreadEnumerator(this, parentKey, nsnull, nsnull);
    if (e == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(e);
    *result = e;

    return NS_OK;
}
#if 0
nsresult nsMsgGroupThread::ReparentMsgsWithInvalidParent(PRUint32 numChildren, nsMsgKey threadParentKey)
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
#endif
NS_IMETHODIMP nsMsgGroupThread::GetRootHdr(PRInt32 *resultIndex, nsIMsgDBHdr **result)
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
      printf("need to reset thread root key\n");
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
            m_threadRootKey = threadParentKey;
            if (resultIndex)
              *resultIndex = childIndex;
            *result = curChild;
            NS_ADDREF(*result);
//            ReparentMsgsWithInvalidParent(numChildren, threadParentKey);
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

nsresult nsMsgGroupThread::ChangeUnreadChildCount(PRInt32 delta)
{
  m_numUnreadChildren += delta;
  return NS_OK;
}

nsresult nsMsgGroupThread::GetChildHdrForKey(nsMsgKey desiredKey, nsIMsgDBHdr **result, PRInt32 *resultIndex)
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
        break;
      NS_RELEASE(*result);
    }
  }
  if (resultIndex)
    *resultIndex = childIndex;
  
  return rv;
}

NS_IMETHODIMP nsMsgGroupThread::GetFirstUnreadChild(nsIMsgDBHdr **result)
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
      rv = m_db->IsRead(msgKey, &isRead);
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

NS_IMETHODIMP nsMsgGroupThread::GetNewestMsgDate(PRUint32 *aResult) 
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


NS_IMETHODIMP nsMsgGroupThread::SetNewestMsgDate(PRUint32 aNewestMsgDate) 
{
  m_newestMsgDate = aNewestMsgDate;
  return NS_OK;
}

nsMsgXFGroupThread::nsMsgXFGroupThread()
{
}

nsMsgXFGroupThread::~nsMsgXFGroupThread()
{
}

NS_IMETHODIMP nsMsgXFGroupThread::GetNumChildren(PRUint32 *aNumChildren)
{
  NS_ENSURE_ARG_POINTER(aNumChildren);
  *aNumChildren = m_hdrs.Count();
  return NS_OK;
}

NS_IMETHODIMP nsMsgXFGroupThread::GetChildKeyAt(PRInt32 aIndex, nsMsgKey *aResult)
{
  NS_ASSERTION(PR_FALSE, "shouldn't call this");
  return NS_ERROR_NOT_IMPLEMENTED;
}

