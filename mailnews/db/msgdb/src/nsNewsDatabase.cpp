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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsNewsDatabase.h"
#include "nsNewsSummarySpec.h"
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

NS_IMETHODIMP nsNewsDatabase::Open(nsIFileSpec *aNewsgroupName, PRBool create, PRBool upgrading, nsIMsgDatabase** pMessageDB)
{
  nsNewsDatabase	        *newsDB;

  if (!aNewsgroupName)
	return NS_ERROR_NULL_POINTER;

  nsFileSpec				newsgroupName;
  aNewsgroupName->GetFileSpec(&newsgroupName);

  nsNewsSummarySpec	        summarySpec(newsgroupName);
  nsresult                  err = NS_OK;
	PRBool			newFile = PR_FALSE;

#ifdef DEBUG_NEWS_DATABASE
  printf("nsNewsDatabase::Open(%s, %s, %p, %s) -> %s\n",
           (const char*)newsgroupName, create ? "TRUE":"FALSE",
           pMessageDB, upgrading ? "TRUE":"FALSE", (const char *)summarySpec);
#endif

  nsFileSpec dbPath(summarySpec);

  *pMessageDB = nsnull;

  newsDB = (nsNewsDatabase *) FindInCache(dbPath);
  if (newsDB) {
		*pMessageDB = newsDB;
		//FindInCache does the AddRef'ing
		//newsDB->AddRef();
		return(NS_OK);
  }

  newsDB = new nsNewsDatabase();
  newsDB->m_folder = m_folder;
  
  if (!newsDB) {
#ifdef DEBUG_NEWS_DATABASE
    printf("NS_ERROR_OUT_OF_MEMORY\n");
#endif
    return NS_ERROR_OUT_OF_MEMORY;
  }

  newsDB->AddRef();

	nsIDBFolderInfo	*folderInfo = nsnull;
  err = newsDB->OpenMDB((const char *) summarySpec, create);
  if (err == NS_OK)
  {
    newsDB->GetDBFolderInfo(&folderInfo);
    if (folderInfo == nsnull)
    {
      err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
    }
    else
    {
      // compare current version of db versus filed out version info.
      PRUint32 version;
      folderInfo->GetVersion(&version);
      if (newsDB->GetCurVersion() != version)
        err = NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE;
      NS_RELEASE(folderInfo);
    }
    if (err != NS_OK)
    {
      // this will make the db folder info release its ref to the mail db...
      NS_IF_RELEASE(newsDB->m_dbFolderInfo);
      newsDB->ForceClosed();
      if (err == NS_MSG_ERROR_FOLDER_SUMMARY_OUT_OF_DATE)
        summarySpec.Delete(PR_FALSE);
      
      newsDB = nsnull;
    }
  }
	if (err != NS_OK || newFile)
	{
		// if we couldn't open file, or we have a blank one, and we're supposed 
		// to upgrade, updgrade it.
		if (newFile && !upgrading)	// caller is upgrading, and we have empty summary file,
		{					// leave db around and open so caller can upgrade it.
			err = NS_MSG_ERROR_FOLDER_SUMMARY_MISSING;
		}
		else if (err != NS_OK)
		{
			*pMessageDB = nsnull;
      if (newsDB)
        newsDB->ForceClosed();
			delete newsDB;
      summarySpec.Delete(PR_FALSE);  // blow away the db if it's corrupt.
			newsDB = nsnull;
		}
	}
	if (err == NS_OK || err == NS_MSG_ERROR_FOLDER_SUMMARY_MISSING)
	{
		*pMessageDB = newsDB;
		if (newsDB)
			GetDBCache()->AppendElement(newsDB);

	}
  return err;
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
  PR_ASSERT(m_dbFolderInfo);
  if (!m_dbFolderInfo) {
    return NS_ERROR_FAILURE;
  }
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
  nsresult		rv;
  nsMsgHdr		*pHeader;

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
 
nsresult		nsNewsDatabase::ExpireUpTo(nsMsgKey expireKey)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}
nsresult		nsNewsDatabase::ExpireRange(nsMsgKey startRange, nsMsgKey endRange)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsNewsDatabase	*nsNewsDatabase::GetNewsDB() 
{
	return this;
}

// used to handle filters editing on open news groups.
//	static void				NotifyOpenDBsOfFilterChange(MSG_FolderInfo *folder);
void nsNewsDatabase::ClearFilterList()
{
  // filter was changed by user.
  return;
}
void nsNewsDatabase::OpenFilterList()
{
  return;
}
//void OnFolderFilterListChanged(MSG_FolderInfo *folder);
//caller needs to free

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
  nsMsgKey lowWater, highWater;
  GetLowWaterArticleNum(&lowWater);
  GetHighWaterArticleNum(&highWater);
	if (lowWater > 2)
		m_readSet->AddRange(1, lowWater - 1);
	nsresult err = nsMsgDatabase::MarkAllRead(thoseMarked);
	if (NS_SUCCEEDED(err) && 1 <= highWater)
		m_readSet->AddRange(1, highWater);	// mark everything read in newsrc.

	return err;
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
