/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Seth Spitzer <sspitzer@netscape.com>
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
#include "nsNewsDatabase.h"
#include "nsMsgKeySet.h"
#include "nsCOMPtr.h"
#include "prlog.h"
#include "nsIFileSpec.h"

#if defined(DEBUG_sspitzer_) || defined(DEBUG_seth_)
#define DEBUG_NEWS_DATABASE 1
#endif

nsNewsDatabase::nsNewsDatabase()
{
  m_readSet = nsnull;
}

nsNewsDatabase::~nsNewsDatabase()
{
}

NS_IMPL_ADDREF_INHERITED(nsNewsDatabase, nsMsgDatabase)
NS_IMPL_RELEASE_INHERITED(nsNewsDatabase, nsMsgDatabase)  

NS_IMETHODIMP nsNewsDatabase::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (!aInstancePtr) return NS_ERROR_NULL_POINTER;
  *aInstancePtr = nsnull;   
  
  if (aIID.Equals(NS_GET_IID(nsINewsDatabase)))
  {
    *aInstancePtr = NS_STATIC_CAST(nsINewsDatabase *, this);
  }
  
  if(*aInstancePtr)
  {
    AddRef();
    return NS_OK;
  }     
  
  return nsMsgDatabase::QueryInterface(aIID, aInstancePtr);
}

nsresult nsNewsDatabase::Close(PRBool forceCommit)
{
  return nsMsgDatabase::Close(forceCommit);
}

nsresult nsNewsDatabase::ForceClosed()
{
  return nsMsgDatabase::ForceClosed();
}

nsresult nsNewsDatabase::Commit(nsMsgDBCommit commitType)
{
  if (m_dbFolderInfo && m_readSet)
  {
    // let's write out our idea of the read set so we can compare it with that of
    // the .rc file next time we start up.
    nsXPIDLCString readSet;
    m_readSet->Output(getter_Copies(readSet));
    m_dbFolderInfo->SetCharPtrProperty("readSet", readSet.get());
  }
  return nsMsgDatabase::Commit(commitType);
}


PRUint32 nsNewsDatabase::GetCurVersion()
{
  return kMsgDBVersion;
}

NS_IMETHODIMP nsNewsDatabase::IsRead(nsMsgKey key, PRBool *pRead)
{
  NS_ASSERTION(pRead, "null out param in IsRead");
  if (!pRead) return NS_ERROR_NULL_POINTER;

  NS_ASSERTION(m_readSet, "set is null!");
  if (!m_readSet) return NS_ERROR_FAILURE;
  
  *pRead = m_readSet->IsMember(key);
  return NS_OK;
}

nsresult nsNewsDatabase::IsHeaderRead(nsIMsgDBHdr *msgHdr, PRBool *pRead)
{
    nsresult rv;
    nsMsgKey messageKey;

    if (!msgHdr || !pRead) return NS_ERROR_NULL_POINTER;

    rv = msgHdr->GetMessageKey(&messageKey);
    if (NS_FAILED(rv)) return rv;

    rv = IsRead(messageKey,pRead);
    return rv;
}

// return highest article number we've seen.
NS_IMETHODIMP nsNewsDatabase::GetHighWaterArticleNum(nsMsgKey *key)
{
  NS_ASSERTION(m_dbFolderInfo, "null db folder info");
  if (!m_dbFolderInfo) 
    return NS_ERROR_FAILURE;
  return m_dbFolderInfo->GetHighWater(key);    
}

// return the key of the first article number we know about.
// Since the iterator iterates in id order, we can just grab the
// messagekey of the first header it returns.
// ### dmb
// This will not deal with the situation where we get holes in
// the headers we know about. Need to figure out how and when
// to solve that. This could happen if a transfer is interrupted.
// Do we need to keep track of known arts permanently?
NS_IMETHODIMP nsNewsDatabase::GetLowWaterArticleNum(nsMsgKey *key)
{
  nsresult  rv;
  nsMsgHdr  *pHeader;

  nsCOMPtr<nsISimpleEnumerator> hdrs;
  rv = EnumerateMessages(getter_AddRefs(hdrs));
  if (NS_FAILED(rv))
    return rv;

  rv = hdrs->GetNext((nsISupports**)&pHeader);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
  if (NS_FAILED(rv)) 
    return rv;
  
  return pHeader->GetMessageKey(key);
}
 
nsresult  nsNewsDatabase::ExpireUpTo(nsMsgKey expireKey)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
nsresult		nsNewsDatabase::ExpireRange(nsMsgKey startRange, nsMsgKey endRange)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// should we thread messages with common subjects that don't start with Re: together?
// I imagine we might have separate preferences for mail and news, so this is a virtual method.
PRBool	
nsNewsDatabase::ThreadBySubjectWithoutRe()
{
  return PR_TRUE;
}

NS_IMETHODIMP nsNewsDatabase::GetReadSet(nsMsgKeySet **pSet)
{
    if (!pSet) return NS_ERROR_NULL_POINTER;
    *pSet = m_readSet;
    return NS_OK;
}

NS_IMETHODIMP nsNewsDatabase::SetReadSet(nsMsgKeySet *pSet)
{
  m_readSet = pSet;

  if (m_readSet)
  {
    // compare this read set with the one in the db folder info.
    // If not equivalent, sync with this one.
    nsXPIDLCString dbReadSet;
    if (m_dbFolderInfo)
      m_dbFolderInfo->GetCharPtrProperty("readSet", getter_Copies(dbReadSet));
    nsXPIDLCString newsrcReadSet;
    m_readSet->Output(getter_Copies(newsrcReadSet));
    if (!dbReadSet.Equals(newsrcReadSet))
      SyncWithReadSet();
  }
  return NS_OK;
}
 
 
PRBool nsNewsDatabase::SetHdrReadFlag(nsIMsgDBHdr *msgHdr, PRBool bRead)
{
    nsresult rv;
    PRBool isRead;
    rv = IsHeaderRead(msgHdr, &isRead);
    
    if (isRead == bRead) 
    {
      // give the base class a chance to update m_flags.
      nsMsgDatabase::SetHdrReadFlag(msgHdr, bRead);
      return PR_FALSE;
    }
    else {
      nsMsgKey messageKey;

      // give the base class a chance to update m_flags.
      nsMsgDatabase::SetHdrReadFlag(msgHdr, bRead);
      rv = msgHdr->GetMessageKey(&messageKey);
      if (NS_FAILED(rv)) return PR_FALSE;

      NS_ASSERTION(m_readSet, "m_readSet is null");
      if (!m_readSet) return PR_FALSE;

      if (!bRead) {
#ifdef DEBUG_NEWS_DATABASE
        printf("remove %d from the set\n",messageKey);
#endif

        rv = m_readSet->Remove(messageKey);
        if (NS_FAILED(rv)) return PR_FALSE;

        rv = NotifyReadChanged(nsnull);
        if (NS_FAILED(rv)) return PR_FALSE;
      }
      else {
#ifdef DEBUG_NEWS_DATABASE
        printf("add %d to the set\n",messageKey);
#endif

        rv = m_readSet->Add(messageKey);
        if (NS_FAILED(rv)) return PR_FALSE;

        rv = NotifyReadChanged(nsnull);
        if (NS_FAILED(rv)) return PR_FALSE;
      }
    }
    return PR_TRUE;
}

NS_IMETHODIMP nsNewsDatabase::MarkAllRead(nsMsgKeyArray *thoseMarked)
{
  nsMsgKey lowWater = nsMsgKey_None, highWater;
  nsXPIDLCString knownArts;
  if (m_dbFolderInfo)
  {
    m_dbFolderInfo->GetKnownArtsSet(getter_Copies(knownArts));
    nsMsgKeySet *knownKeys = nsMsgKeySet::Create(knownArts);
    if (knownKeys)
      lowWater = knownKeys->GetFirstMember();

    delete knownKeys;
  }
  if (lowWater == nsMsgKey_None)
    GetLowWaterArticleNum(&lowWater);
  GetHighWaterArticleNum(&highWater);
  if (lowWater > 2)
    m_readSet->AddRange(1, lowWater - 1);
  nsresult err = nsMsgDatabase::MarkAllRead(thoseMarked);
  if (NS_SUCCEEDED(err) && 1 <= highWater)
    m_readSet->AddRange(1, highWater);	// mark everything read in newsrc.
  
  return err;
}

nsresult nsNewsDatabase::SyncWithReadSet()
{

  // The code below attempts to update the underlying nsMsgDatabase's idea
  // of read/unread flags to match the read set in the .newsrc file. It should
  // only be called when they don't match, e.g., we crashed after committing the
  // db but before writing out the .newsrc
  nsCOMPtr <nsISimpleEnumerator> hdrs;
  nsresult rv = EnumerateMessages(getter_AddRefs(hdrs));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE, readInNewsrc, isReadInDB, changed = PR_FALSE;
  nsCOMPtr <nsIMsgDBHdr> header;
  PRInt32 numMessages = 0, numUnreadMessages = 0;
  nsMsgKey messageKey;
  nsCOMPtr <nsIMsgThread> threadHdr;

  // Scan all messages in DB
  while (NS_SUCCEEDED(rv = hdrs->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE))
  {
      rv = hdrs->GetNext(getter_AddRefs(header));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = nsMsgDatabase::IsHeaderRead(header, &isReadInDB);
      NS_ENSURE_SUCCESS(rv, rv);
  
      header->GetMessageKey(&messageKey);
      IsRead(messageKey,&readInNewsrc);

      numMessages++;
      if (!readInNewsrc) 
        numUnreadMessages++;

      // If DB and readSet disagree on Read/Unread, fix DB
      if (readInNewsrc!=isReadInDB) 
      {
        MarkHdrRead(header, readInNewsrc, nsnull);
        changed = PR_TRUE;
      }
  }
  
  // Update FolderInfo Counters
  PRInt32 oldMessages, oldUnreadMessages;
  rv = m_dbFolderInfo->GetNumMessages(&oldMessages);
  if (NS_SUCCEEDED(rv) && oldMessages!=numMessages) 
  {
      changed = PR_TRUE;
      m_dbFolderInfo->ChangeNumMessages(numMessages-oldMessages);
  }
  rv = m_dbFolderInfo->GetNumUnreadMessages(&oldUnreadMessages);
  if (NS_SUCCEEDED(rv) && oldUnreadMessages!=numUnreadMessages) 
  {
      changed = PR_TRUE;
      m_dbFolderInfo->ChangeNumUnreadMessages(numUnreadMessages-oldUnreadMessages);
  }

  if (changed)
      Commit(nsMsgDBCommitType::kLargeCommit);

  return rv;
}

nsresult nsNewsDatabase::AdjustExpungedBytesOnDelete(nsIMsgDBHdr *msgHdr)
{
  PRUint32 msgFlags;
  msgHdr->GetFlags(&msgFlags);
  if (msgFlags & MSG_FLAG_OFFLINE && m_dbFolderInfo)
  {
    PRUint32 size = 0;
    (void)msgHdr->GetOfflineMessageSize(&size);
    return m_dbFolderInfo->ChangeExpungedBytes (size);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsNewsDatabase::GetDefaultViewFlags(nsMsgViewFlagsTypeValue *aDefaultViewFlags)
{
  NS_ENSURE_ARG_POINTER(aDefaultViewFlags);
  *aDefaultViewFlags = nsMsgViewFlagsType::kThreadedDisplay;
  return NS_OK;
}

NS_IMETHODIMP 
nsNewsDatabase::GetDefaultSortType(nsMsgViewSortTypeValue *aDefaultSortType)
{
  NS_ENSURE_ARG_POINTER(aDefaultSortType);
  *aDefaultSortType = nsMsgViewSortType::byThread;
  return NS_OK;
}
