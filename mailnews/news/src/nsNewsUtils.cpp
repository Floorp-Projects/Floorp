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

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsresult
nsGetNewsRoot(const char *hostname, nsFileSpec &result)
{
  nsresult rv = NS_OK;

  // retrieve the AccountManager
  NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIMsgAccountManager> accountManager;
  rv = session->GetAccountManager(getter_AddRefs(accountManager));
  if (NS_FAILED(rv)) return rv;                  
  
  // find all news hosts matching the given hostname
  nsCOMPtr<nsISupportsArray> hosts;
  rv = accountManager->FindServersByHostname(hostname,
                                            nsINntpIncomingServer::GetIID(),
                                            getter_AddRefs(hosts));
  if (NS_FAILED(rv)) return rv; 

  // use enumeration function to find the first nntp server
  nsISupports *serverSupports = hosts->ElementAt(0);

#ifdef DEBUG_NEWS
  if (hosts->Count() <= 0)
    fprintf(stderr, "Augh, no nntp server named %s?\n", hostname);
  if (!serverSupports)
    fprintf(stderr, "Huh, serverSupports returned nsnull\n");
#endif

  // if there are no nntp servers, how did we get here?
  if (! serverSupports) return NS_ERROR_UNEXPECTED;

  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(serverSupports);

  // this had better be a nsIMsgIncomingServer, otherwise
  // FindServersByHostname is broken or we got some weird object back
  PR_ASSERT(server);

  // now ask the server what it's root is
  char *localPath;
  rv = server->GetLocalPath(&localPath);
  if (NS_SUCCEEDED(rv)) {
    result = localPath;
    PL_strfree(localPath);
  }
  
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
nsNewsURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult)
{
  nsresult rv;
  
  nsAutoString sep = PR_GetDirectorySeparator();

  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;

  // verify that rootURI starts with "news:/" or "news_message:/"
  if ((PL_strcmp(rootURI, kNewsRootURI) != 0) && 
      (PL_strcmp(rootURI, kNewsMessageRootURI) != 0)) {
    pathResult = nsnull;
    return NS_ERROR_FAILURE;
	}

  // the server name is the first component of the path, so extract it out
  PRInt32 hostStart = 0;

  hostStart = uri.Find('/');
  if (hostStart <= 0) return NS_ERROR_FAILURE;

  // skip past all //
  while (uri[hostStart]=='/') hostStart++;
  
  // cut news://hostname/newsgroup -> hostname/newsgroup
  nsAutoString hostname (eOneByte);
  uri.Right(hostname, uri.Length() - hostStart);

  PRInt32 hostEnd = hostname.Find('/');

  // newsgroup comes after the hostname, after the '/'
  nsAutoString newsgroup (eOneByte);
 
  if (hostEnd != -1) {
    hostname.Right(newsgroup, hostname.Length() - hostEnd - 1);
  }

  // cut off first '/' and everything following it
  // hostname/newsgroup -> hostname
  if (hostEnd >0) {
    hostname.Truncate(hostEnd);
  }

  rv = nsGetNewsRoot(hostname.GetBuffer(), pathResult);

  if (NS_FAILED(rv)) {
    pathResult = nsnull;
#ifdef DEBUG_NEWS
    printf("nsGetNewsRoot failed!\n");
#endif
    return rv;
  }

  // create pathResult if it doesn't exist
  // at this point, pathResult should be something like
  // .../News, ...\News, ...:News)
  if (!pathResult.Exists())
    pathResult.CreateDir();
              
  nsAutoString alteredHost ((const char *) "host-", eOneByte);
  alteredHost += hostname;
  
  // can't do pathResult += "host-"; pathresult += hostname; 
  // because += on a nsFileSpec inserts a separator
  // so we'd end up with host-/hostname and not host-hostname
  pathResult += alteredHost;

  // create pathResult if it doesn't exist
  // at this point, pathResult should be something like
  // ../News/host-<hostname>, ...\News\host-<hostname>, ...:News:host-<hostname>
  if (!pathResult.Exists())
    pathResult.CreateDir();
  
  if (newsgroup != "") {
    pathResult += newsgroup;
  }

#ifdef DEBUG_NEWS
  printf("nsGetNewsRoot(%s) = %s\n\tnewsgroup = %s\n",
         hostname.GetBuffer(), (const char*)pathResult,
         newsgroup.GetBuffer();
#endif

  return NS_OK;
}

nsresult
nsNewsURI2Name(const char* rootURI, const char* uriStr, nsString& name)
{
  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;
  PRInt32 pos = uri.RFind("/");
  PRInt32 length = uri.Length();
  PRInt32 count = length - (pos + 1);
  return uri.Right(name, count);
}

/* parses NewsMessageURI */
nsresult 
nsParseNewsMessageURI(const char* uri, nsString& folderURI, PRUint32 *key)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsAutoString uriStr = uri;
	PRInt32 keySeparator = uriStr.Find('#');
	if(keySeparator != -1)
	{
		nsAutoString folderPath;
		uriStr.Left(folderURI, keySeparator);

		nsAutoString keyStr;
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

  nsAutoString tailURI(baseURI, eOneByte);

  // chop off news:/
  if (tailURI.Find(kNewsRootURI) == 0)
    tailURI.Cut(0, kNewsRootURILen);
  
	*uri = PR_smprintf("%s%s#%d", kNewsMessageRootURI, tailURI.GetBuffer(), key);
  
	return NS_OK;
}
