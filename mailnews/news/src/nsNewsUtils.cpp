/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
*/

#include "msgCore.h"
#include "nntpCore.h"
#include "nsNewsUtils.h"
#include "nsIServiceManager.h"
#include "prsystem.h"
#include "nsCOMPtr.h"
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsINntpIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsMsgUtils.h"

static nsresult
nsGetNewsServer(const char* username, const char *hostname,
                nsIMsgIncomingServer** aResult)
{
#ifdef DEBUG_NEWS
  printf("nsGetNewsServer(%s,%s,??)\n",username,hostname);
#endif
  nsresult rv = NS_OK;

  // retrieve the AccountManager
  NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                  NS_MSGACCOUNTMANAGER_PROGID, &rv);
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


nsresult
nsNewsURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult)
{
#ifdef DEBUG_NEWS
  printf("nsNewsURI2Path(%s,%s,??)\n",rootURI,uriStr);
#endif
  nsresult rv = NS_OK;
  
  pathResult = nsnull;
  nsAutoString sep(PRUnichar(PR_GetDirectorySeparator()));

  nsCAutoString uri(uriStr);
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
  nsCOMPtr<nsIFileSpec> localPath;
  rv = server->GetLocalPath(getter_AddRefs(localPath));
  if (NS_FAILED(rv)) return rv;

  localPath->GetFileSpec(&pathResult);

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

	nsCAutoString uriStr(uri);
	PRInt32 keySeparator = uriStr.FindChar('#');
	if(keySeparator != -1)
	{
    PRInt32 keyEndSeparator = uriStr.FindCharInSet("?&", 
                                                   keySeparator); 

		uriStr.Left(messageUriWithoutKey, keySeparator);

		nsCAutoString keyStr;
    if (keyEndSeparator != -1)
        uriStr.Mid(keyStr, keySeparator+1, 
                   keyEndSeparator-(keySeparator+1));
    else
        uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
		PRInt32 errorCode;
		*key = keyStr.ToInteger(&errorCode);

		return errorCode;
	}
	return NS_ERROR_FAILURE;

}

nsresult nsBuildNewsMessageURI(const char *baseURI, PRUint32 key, nsCString& uri)
{
	uri.Append(baseURI);
	uri.Append('#');

  uri.AppendInt(key);
	return NS_OK;
}

nsresult nsCreateNewsBaseMessageURI(const char *baseURI, char **baseMessageURI)
{
	if(!baseMessageURI)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString tailURI(baseURI);

	// chop off mailbox:/
	if (tailURI.Find(kNewsRootURI) == 0)
		tailURI.Cut(0, PL_strlen(kNewsRootURI));
	
	nsCAutoString baseURIStr(kNewsMessageRootURI);
	baseURIStr += tailURI;

	*baseMessageURI = baseURIStr.ToNewCString();
	if(!*baseMessageURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}
