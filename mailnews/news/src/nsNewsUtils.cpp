/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsNewsUtils.h"
#include "nsIServiceManager.h"
#include "prsystem.h"
#include "nsCOMPtr.h"

#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsINntpIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsMsgUtils.h"

#ifdef DEBUG_seth
#define DEBUG_NEWS 1
#endif

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

static nsresult
nsGetNewsServer(const char* username, const char *hostname,
                nsIMsgIncomingServer** aResult)
{
#ifdef DEBUG_NEWS
  printf("nsGetNewsServer(%s,%s,??)\n",username,hostname);
#endif
  nsresult rv = NS_OK;

  // retrieve the AccountManager
  NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = session->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) return rv;                  
  
  // find the news host
  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = accountManager->FindServer(username,
                                  hostname,
                                  "nntp",
                                  getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv; 

  *aResult = server;
  NS_ADDREF(*aResult);
  
  return rv;
}

// copy-and-paste from nsGetMailboxHostName()
// we could probably combine them in a common place to save
// space.
nsresult nsGetNewsHostName(const char *rootURI, const char *uriStr, char **hostName)
{
  if(!hostName)
	  return NS_ERROR_NULL_POINTER;

  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;

  // start parsing the uriStr
  const char* curPos = uriStr;
  
  // skip past schema 
  while (*curPos != ':') curPos++;
  curPos++;
  while (*curPos == '/') curPos++;

  char *atPos = PL_strchr(curPos, '@');
  if (atPos) curPos = atPos+1;
  
  char *slashPos = PL_strchr(curPos, '/');
  int length;

  // if there are no more /'s then we just copy the rest of the string
  if (slashPos)
    length = (slashPos - curPos) + 1;
  else
    length = PL_strlen(curPos) + 1;

  *hostName = new char[length];
  if(!*hostName)
	  return NS_ERROR_OUT_OF_MEMORY;

  PL_strncpyz(*hostName, curPos, length);

  return NS_OK;
}

nsresult
nsGetNewsUsername(const char *rootURI, const char *uriStr, char **userName)
{
  const char *curPos = uriStr;
  while (*curPos != ':') curPos++;
  curPos++;
  while (*curPos == '/') curPos++;

  char *atPos = PL_strchr(curPos, '@');
  
  if (atPos) {
    *userName = PL_strndup(curPos, (atPos - curPos));
  } else {
    *userName = PL_strdup("");
  }

  return NS_OK;
}

nsresult
nsNewsURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult)
{
#ifdef DEBUG_NEWS
  printf("nsNewsURI2Path(%s,%s,??)\n",rootURI,uriStr);
#endif
  nsresult rv = NS_OK;
  
  pathResult = nsnull;
  nsAutoString sep = PR_GetDirectorySeparator();

  nsCAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;

  // verify that rootURI starts with "news:/" or "news_message:/"
  if ((PL_strcmp(rootURI, kNewsRootURI) != 0) && 
      (PL_strcmp(rootURI, kNewsMessageRootURI) != 0)) {
    return NS_ERROR_FAILURE;
	}

  // the server name is the first component of the path, so extract it out
  PRInt32 hostStart = 0;

  hostStart = uri.FindChar('/');
  if (hostStart <= 0) return NS_ERROR_FAILURE;

  // skip past all //
  while (uri.CharAt(hostStart) =='/') hostStart++;
  
  // cut 
  // news://[username@]hostname -> username@hostname
  // and
  // news://[username@]hostname/newsgroup -> username@hostname/newsgroup
  nsCAutoString hostname;
  uri.Right(hostname, uri.Length() - hostStart);

  PRInt32 hostEnd = hostname.FindChar('/');

  PRInt32 atPos = hostname.FindChar('@');
  nsCAutoString username;

  username = "";
  // we have a username here
  // XXX do this right, this doesn't work -alecf@netscape.com
  if (atPos != -1) {
    hostname.Left(username, atPos);	
#ifdef DEBUG_NEWS
    printf("username = %s\n",username.GetBuffer());
#endif
  }

  // newsgroup comes after the hostname, after the '/'
  nsCAutoString newsgroup;
  nsCAutoString exacthostname;

  if (hostEnd != -1) {
    hostname.Right(newsgroup, hostname.Length() - hostEnd - 1);
  }

  // cut off first '/' and everything following it
  // hostname/newsgroup -> hostname
  if (hostEnd > 0) {
    hostname.Truncate(hostEnd);
  }
 
  // if we had a username, remove it to get the exact hostname
  if (atPos != -1) {
    hostname.Right(exacthostname, hostname.Length() - atPos - 1);
  }
  else {
    exacthostname = hostname.GetBuffer();
#ifdef DEBUG_NEWS
    printf("exacthostname = %s, hostname = %s\n",exacthostname.GetBuffer(),hostname.GetBuffer());
#endif
  } 

  nsCOMPtr<nsIMsgIncomingServer> server;
  rv = nsGetNewsServer(username.GetBuffer(),
                       exacthostname.GetBuffer(), getter_AddRefs(server));
  if (NS_FAILED(rv)) return rv;

  // now ask the server what it's root is
  char *localPath = nsnull;
  rv = server->GetLocalPath(&localPath);
  if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_NEWS
  printf("local path = %s\n", localPath);
#endif
  pathResult = localPath;

  // mismatched free?
  if (localPath) PL_strfree(localPath);

  if (!pathResult.Exists())
    pathResult.CreateDir();
  
  if (!newsgroup.IsEmpty()) {
    NS_MsgHashIfNecessary(newsgroup);
    pathResult += (const char *) newsgroup;
  }

  return NS_OK;
}

/* parses NewsMessageURI */
nsresult 
nsParseNewsMessageURI(const char* uri, nsCString& messageUriWithoutKey, PRUint32 *key)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString uriStr = uri;
	PRInt32 keySeparator = uriStr.FindChar('#');
	if(keySeparator != -1)
	{
		uriStr.Left(messageUriWithoutKey, keySeparator);

		nsCAutoString keyStr;
		uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
		PRInt32 errorCode;
		*key = keyStr.ToInteger(&errorCode);

		return errorCode;
	}
	return NS_ERROR_FAILURE;

}

nsresult nsBuildNewsMessageURI(const char *baseURI, PRUint32 key, char** uri)
{
	if(!uri)
		return NS_ERROR_NULL_POINTER;
  // need to convert news://hostname/.. to news_message://hostname/..

  nsCString tailURI(baseURI);

  // chop off news:/
#if 0
  if (tailURI.Find(kNewsRootURI) == 0)
    tailURI.Cut(0, kNewsRootURILen);
#else
  PRInt32 strOffset = tailURI.Find(":/");
  if (strOffset != -1)
    tailURI.Cut(0, strOffset+2);
#endif
  
	*uri = PR_smprintf("%s%s#%d", kNewsMessageRootURI, tailURI.GetBuffer(), key);
  
	return NS_OK;
}
