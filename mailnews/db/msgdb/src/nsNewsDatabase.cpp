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

nsresult nsNewsDatabase::PrePopulate()
{
  nsIMessage       *msg;
  nsMsgHdr	       *newHdr = NULL;
  PRTime           resultTime, intermediateResult, microSecondsPerSecond;
  time_t           resDate;

  resultTime = PR_Now();

  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, resultTime, microSecondsPerSecond);
  LL_L2I(resDate, intermediateResult);
  
  nsresult rv = CreateNewHdr(1, &msg);
  if (NS_FAILED(rv)) return rv;
  newHdr = NS_STATIC_CAST(nsMsgHdr*, msg);          // closed system, cast ok
  newHdr->SetAuthor("bird@celtics.com (Larry Bird)");
  newHdr->SetSubject("Why the Lakers suck");
  newHdr->SetDate(resDate);
  newHdr->SetRecipients("riley@heat.com (Pat Riley)", PR_FALSE);
  AddNewHdrToDB (newHdr, PR_TRUE);
  printf("added header\n");
  newHdr->Release();
  
  rv = CreateNewHdr(2, &msg);
  if (NS_FAILED(rv)) return rv;
  newHdr = NS_STATIC_CAST(nsMsgHdr*, msg);          // closed system, cast ok
  newHdr->SetAuthor("shaq@brick.com (Shaquille O'Neal)");
  newHdr->SetSubject("Anyone here know how to shoot free throws?");
  newHdr->SetDate(resDate);
  AddNewHdrToDB (newHdr, PR_TRUE);
  printf("added header\n");
  newHdr->Release();
  
  rv = CreateNewHdr(3, &msg);
  if (NS_FAILED(rv)) return rv;
  newHdr = NS_STATIC_CAST(nsMsgHdr*, msg);          // closed system, cast ok
  newHdr->SetAuthor("dj@celtics.com (Dennis Johnson)");
  newHdr->SetSubject("Has anyone seen my jump shot?");
  newHdr->SetDate(resDate);
  AddNewHdrToDB (newHdr, PR_TRUE);
  printf("added header\n");
  newHdr->Release();
  
  rv = CreateNewHdr(4, &msg);
  if (NS_FAILED(rv)) return rv;
  newHdr = NS_STATIC_CAST(nsMsgHdr*, msg);          // closed system, cast ok
  newHdr->SetAuthor("sichting@celtics.com (Jerry Sichting)");
  newHdr->SetSubject("Tips for fighting 7' 4\" guys");
  newHdr->SetDate(resDate);
  AddNewHdrToDB (newHdr, PR_TRUE);
  printf("added header\n");
  newHdr->Release();
  return NS_OK;
}

/* static */ 
nsresult nsNewsDatabase::Open(nsFileSpec &newsgroupName, PRBool create, nsIMsgDatabase** pMessageDB, PRBool upgrading /*=PR_FALSE*/)
{
  printf("in nsNewsDatabase::Open()\n");
  nsNewsDatabase	*newsDB;
  nsresult          err = NS_OK;

  newsDB = new nsNewsDatabase();
  
  if (!newsDB) {
    printf("NS_ERROR_OUT_OF_MEMORY\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  newsDB->m_newsgroupSpec = new nsFileSpec(newsgroupName);
  newsDB->AddRef();

  /* sspitzer:  temporary work, don't panic */
  err = newsDB->OpenMDB("/tmp/mozillamaildb", create);
  if (NS_SUCCEEDED(err)) {
    printf("newsDB->OpenMDB succeeded!\n");
    newsDB->PrePopulate();
  }
  else {
    printf("newsDB->OpenMDB failed!\n");
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

  printf("nsNewsDatabase::CreateMsgHdr()\n");

  nsIRDFService *rdf;
  rv = nsServiceManager::GetService(kRDFServiceCID, 
                                    nsIRDFService::GetIID(), 
                                    (nsISupports**)&rdf);
  
  if (NS_FAILED(rv)) return rv;
  
  char* msgURI;
  
  //Need to remove ".msf".
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
  msgHdr->Init(this, hdrRow);
  msgHdr->SetMessageKey(key);
  *result = msgHdr;
  
  nsServiceManager::ReleaseService(kRDFServiceCID, rdf);
  
  return rv;
}
