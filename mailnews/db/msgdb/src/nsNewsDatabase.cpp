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

#include "nsRDFCID.h"
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsNewsDatabase::nsNewsDatabase()
{
  // do nothing
}

nsNewsDatabase::~nsNewsDatabase()
{
  // do nothing
}

nsresult nsNewsDatabase::MessageDBOpenUsingURL(const char * groupURL)
{
  return NS_OK;
}

/* static */ 
nsresult nsNewsDatabase::Open(nsFileSpec &newsgroupName, PRBool create, nsIMsgDatabase** pMessageDB, PRBool upgrading /*=PR_FALSE*/)
{
  nsNewsDatabase	        *newsDB;
  nsNewsSummarySpec	        summarySpec(newsgroupName);
  nsresult                  err = NS_OK;

#ifdef DEBUG
    printf("nsNewsDatabase::Open(%s, %s, %p, %s) -> %s\n",
           (const char*)newsgroupName, create ? "TRUE":"FALSE",
           pMessageDB, upgrading ? "TRUE":"FALSE", (const char*)newsgroupName);
#endif

  *pMessageDB = nsnull;

  newsDB = new nsNewsDatabase();
  
  if (!newsDB) {
#ifdef DEBUG
    printf("NS_ERROR_OUT_OF_MEMORY\n");
#endif
    return NS_ERROR_OUT_OF_MEMORY;
  }

  newsDB->m_newsgroupSpec = new nsFileSpec(newsgroupName);
  newsDB->AddRef();

  err = newsDB->OpenMDB((const char *) summarySpec, create);
  if (NS_SUCCEEDED(err)) {
#ifdef DEBUG
    printf("newsDB->OpenMDB succeeded!\n");
#endif
    *pMessageDB = newsDB;
  }
#ifdef DEBUG
  else {
    printf("newsDB->OpenMDB failed!\n");
    *pMessageDB = nsnull;
    delete newsDB;
    newsDB = nsnull;
  }
#endif

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
NS_IMETHODIMP nsNewsDatabase::MarkHdrRead(nsIMessage *msgHdr, PRBool bRead,
								nsIDBChangeListener *instigator)
{
    nsresult		err = NS_OK;
#if 0
	nsMsgKey messageKey = msgHdr->GetMessageKey();

	if (bRead)
		err = m_set->Add(messageKey);
	else
		err = m_set->Remove(messageKey);
#endif
	// give parent class chance to update data structures
	nsMsgDatabase::MarkHdrRead(msgHdr, bRead, instigator);

//	return (err >= 0) ? 0 : NS_ERROR_OUT_OF_MEMORY;
	return err;
}

NS_IMETHODIMP nsNewsDatabase::IsRead(nsMsgKey key, PRBool *pRead)
{
	NS_ASSERTION(pRead != NULL, "null out param in IsRead");
	if (pRead == NULL) 
		return NS_ERROR_NULL_POINTER;
#if 0
	PRBool isRead = m_set->IsMember(messageKey);
	*pRead = isRead;
#endif
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
	nsMsgKey		nsNewsDatabase::GetHighwaterArticleNum()
	{
		return 0;
	}
	nsMsgKey		nsNewsDatabase::GetLowWaterArticleNum()
	{
		return 0;
	}

	nsresult		nsNewsDatabase::ExpireUpTo(nsMsgKey expireKey)
	{
		return 0;
	}
	nsresult		nsNewsDatabase::ExpireRange(nsMsgKey startRange, nsMsgKey endRange)
	{
		return 0;
	}

nsNewsSet				*nsNewsDatabase::GetNewsArtSet() 
{
	return m_set;
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

nsresult
nsNewsDatabase::CreateMsgHdr(nsIMdbRow* hdrRow, nsFileSpec& path, nsMsgKey key, nsIMessage* *result,
							PRBool getKeyFromHeader)
{
  nsresult rv;

#ifdef DEBUG
  printf("nsNewsDatabase::CreateMsgHdr()\n");
#endif

  nsIRDFService *rdf;
  rv = nsServiceManager::GetService(kRDFServiceCID, 
                                    nsIRDFService::GetIID(), 
                                    (nsISupports**)&rdf);
  
  if (NS_FAILED(rv)) return rv;
  
  char* msgURI;
  
  //Need to remove ".nsf".
  nsFileSpec folderPath = path;
  char* leafName = folderPath.GetLeafName();
  nsString folderName(leafName);
  PL_strfree(leafName);
  if(folderName.Find(".nsf") != -1)
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
  msgHdr->Init(this, hdrRow);
  msgHdr->SetMessageKey(key);
  *result = msgHdr;
  
  nsServiceManager::ReleaseService(kRDFServiceCID, rdf);
  
  return rv;
}
