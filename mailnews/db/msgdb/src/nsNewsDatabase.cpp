/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsNewsDatabase.h"
#include "nsNewsSummarySpec.h"
#include "nsMsgKeySet.h"
#include "nsCOMPtr.h"

nsNewsDatabase::nsNewsDatabase()
{
  // do nothing
}

nsNewsDatabase::~nsNewsDatabase()
{
  // do nothing.
  // todo:  figure out where to delete m_newsgroupSpec
}

nsresult nsNewsDatabase::MessageDBOpenUsingURL(const char * groupURL)
{
  return NS_OK;
}

NS_IMETHODIMP nsNewsDatabase::Open(nsFileSpec &newsgroupName, PRBool create, nsIMsgDatabase** pMessageDB, PRBool upgrading /*=PR_FALSE*/)
{
  nsNewsDatabase	        *newsDB;
  nsNewsSummarySpec	        summarySpec(newsgroupName);
  nsresult                  err = NS_OK;

#ifdef DEBUG_NEWS
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
  
  if (!newsDB) {
#ifdef DEBUG_NEWS
    printf("NS_ERROR_OUT_OF_MEMORY\n");
#endif
    return NS_ERROR_OUT_OF_MEMORY;
  }

  newsDB->m_newsgroupSpec = new nsFileSpec(newsgroupName);
  newsDB->AddRef();

  err = newsDB->OpenMDB((const char *) summarySpec, create);
  if (NS_SUCCEEDED(err)) {
#ifdef DEBUG_NEWS
    printf("newsDB->OpenMDB succeeded!\n");
#endif
	*pMessageDB = newsDB;
	if (newsDB) {
		GetDBCache()->AppendElement(newsDB);
	}
  }
  else {
#ifdef DEBUG_NEWS
    printf("newsDB->OpenMDB failed!\n");
#endif
    *pMessageDB = nsnull;
    if (newsDB) {
		delete newsDB;
	}
    newsDB = nsnull;
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

nsresult nsNewsDatabase::Commit(nsMsgDBCommitType commitType)
{
  return nsMsgDatabase::Commit(commitType);
}


PRUint32 nsNewsDatabase::GetCurVersion()
{
  return 1;
}

// methods to get and set docsets for ids.
NS_IMETHODIMP nsNewsDatabase::MarkHdrRead(nsIMsgDBHdr *msgHdr, PRBool bRead,
								nsIDBChangeListener *instigator)
{
	nsresult rv = NS_OK;
	nsMsgKey messageKey;
	rv = msgHdr->GetMessageKey(&messageKey);
	if (NS_FAILED(rv)) {
		return rv;
	}

	if (bRead)
		rv = m_newSet->Add(messageKey);
	else
		rv = m_newSet->Remove(messageKey);

	// give parent class chance to update data structures
	rv = nsMsgDatabase::MarkHdrRead(msgHdr, bRead, instigator);

	return rv;
}

NS_IMETHODIMP nsNewsDatabase::IsRead(nsMsgKey key, PRBool *pRead)
{
	NS_ASSERTION(pRead != NULL, "null out param in IsRead");
	if (pRead == NULL) 
		return NS_ERROR_NULL_POINTER;

	PRBool isRead = m_newSet->IsMember(key);
	*pRead = isRead;
	return 0;
}

PRBool nsNewsDatabase::IsArticleOffline(nsMsgKey key)
{
	return 0;
}

nsresult		nsNewsDatabase::MarkAllRead(nsMsgKeyArray *thoseMarked)
{
	return 0;
}
nsresult		nsNewsDatabase::AddHdrFromXOver(const char * line,  nsMsgKey *msgId)
{
	return 0;
}

NS_IMETHODIMP		nsNewsDatabase::AddHdrToDB(nsMsgHdr *newHdr, PRBool *newThread, PRBool notify)
{
	return 0;
}

NS_IMETHODIMP		nsNewsDatabase::ListNextUnread(ListContext **pContext, nsMsgHdr **pResult)
{
	return 0;
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

  nsCOMPtr<nsIEnumerator> hdrs;
  rv = EnumerateMessages(getter_AddRefs(hdrs));
  if (NS_FAILED(rv))
    return rv;

  hdrs->First(); 
  rv = hdrs->CurrentItem((nsISupports**)&pHeader);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsMsgDBEnumerator broken");
  if (NS_FAILED(rv)) 
    return rv;
  
  return pHeader->GetMessageKey(key);
}
 
nsresult		nsNewsDatabase::ExpireUpTo(nsMsgKey expireKey)
{
	return 0;
}
nsresult		nsNewsDatabase::ExpireRange(nsMsgKey startRange, nsMsgKey endRange)
{
	return 0;
}

nsNewsDatabase	*nsNewsDatabase::GetNewsDB() 
{
	return this;
}

PRBool	nsNewsDatabase::PurgeNeeded(MSG_PurgeInfo *hdrPurgeInfo, MSG_PurgeInfo *artPurgeInfo) { return PR_FALSE; };

PRBool	nsNewsDatabase::IsCategory() {
  return PR_FALSE;
}
nsresult nsNewsDatabase::SetOfflineRetrievalInfo(MSG_RetrieveArtInfo *)
{
  return NS_OK;
}
nsresult nsNewsDatabase::SetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo)
{
  return NS_OK;
}
nsresult nsNewsDatabase::SetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo)
{
  return NS_OK;
}
nsresult nsNewsDatabase::GetOfflineRetrievalInfo(MSG_RetrieveArtInfo *info)
{
  return NS_OK;
}
nsresult nsNewsDatabase::GetPurgeHeaderInfo(MSG_PurgeInfo *purgeInfo)
{
  return NS_OK;
}
nsresult nsNewsDatabase::GetPurgeArticleInfo(MSG_PurgeInfo *purgeInfo)
{
  return NS_OK;
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
/* static */ 
char *nsNewsDatabase::GetGroupNameFromURL(const char *url) 
{
  return nsnull;
}

// should we thread messages with common subjects that don't start with Re: together?
// I imagine we might have separate preferences for mail and news, so this is a virtual method.
PRBool	
nsNewsDatabase::ThreadBySubjectWithoutRe()
{
  return PR_TRUE;
}

#if 0
nsresult
nsNewsDatabase::CreateMsgHdr(nsIMdbRow* hdrRow, nsMsgKey key, nsIMsgDBHdr* *result)
{
  nsresult rv;
#ifdef DEBUG_NEWS
  printf("nsNewsDatabase::CreateMsgHdr()\n");
#endif

  NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
  
  if (NS_FAILED(rv)) return rv;
  
  char* msgURI;
  
  //Need to remove ".msf".  (msf = message summary file)
  nsFileSpec folderPath = path;
  char* leafName = folderPath.GetLeafName();
  nsString folderName(leafName);
  PL_strfree(leafName);
  if(folderName.Find(".msf") != -1)
	{
      nsString realFolderName;
      folderName.Left(realFolderName, folderName.Length() - 4);
      folderPath.SetLeafName((const nsString)realFolderName);
	}
  
  rv = nsBuildNewsMessageURI(folderPath, key, &msgURI);
  if (NS_FAILED(rv)) return rv;
  
  nsIRDFResource* res;
  rv = rdf->GetResource(msgURI, &res);
  PR_smprintf_free(msgURI);
  if (NS_FAILED(rv)) return rv;
  
  nsMsgHdr* msgHdr = (nsMsgHdr*)res;
  
  nsMsgHdr* msgHdr = new nsMsgHdr();
  msgHdr->Init(this, hdrRow);
  msgHdr->SetMessageKey(key);
  *result = msgHdr;
  
  return NS_OK;
}
#endif /* 0 */
