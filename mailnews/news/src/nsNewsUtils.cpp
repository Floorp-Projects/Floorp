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

// stuff for temporary root folder hack
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsINntpIncomingServer.h"

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

#ifdef DEBUG_sspitzer_
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
  if (NS_SUCCEEDED(rv)) 
    result = localPath;

  return rv;
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
  PRInt32 hostStart;

  hostStart = uri.Find('/');
  if (hostStart <= 0) return NS_ERROR_FAILURE;

  // skip past all //
  while (uri[hostStart]=='/') hostStart++;
  
  // cut news://hostname/newsgroup -> hostname/newsgroup
  nsAutoString hostname;
  uri.Right(hostname, uri.Length() - hostStart);

  PRInt32 hostEnd = hostname.Find('/');

  // newsgroup comes after the hostname, after the '/'
  nsAutoString newsgroup = "";
 
  if (hostEnd != -1) {
    hostname.Right(newsgroup, hostname.Length() - hostEnd - 1);
  }

  // cut off first '/' and everything following it
  // hostname/newsgroup -> hostname
  if (hostEnd >0) {
    hostname.Truncate(hostEnd);
  }

  char *hostchar = hostname.ToNewCString();
  rv = nsGetNewsRoot(hostchar, pathResult);
  delete[] hostchar;

  if (NS_FAILED(rv)) {
    pathResult = nsnull;
#ifdef DEBUG_sspitzer
    printf("nsGetNewsRoot failed!\n");
#endif
    return rv;
  }

  nsAutoString alteredHost = "host-";
  alteredHost += hostname;

  // can't do pathResult += "host-"; pathresult += hostname; 
  // because += on a nsFileSpec inserts a separator
  // so we'd end up with host-/hostname and not host-hostname
  pathResult += alteredHost;

  if (newsgroup != "") {
    pathResult += newsgroup;
  }

#ifdef DEBUG_sspitzer
  printf("nsGetNewsRoot(%s) = %s\n\tnewsgroup = %s\n",
         hostname.ToNewCString(), (const char*)pathResult,
         newsgroup.ToNewCString());
#endif

  return NS_OK;
}

nsresult
nsNewsURI2Name(const char* rootURI, char* uriStr, nsString& name)
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

  nsAutoString tailURI(baseURI);

  // chop off news:/
  if (tailURI.Find(kNewsRootURI) == 0)
    tailURI.Cut(0, PL_strlen(kNewsRootURI));
  
  const char *tail = tailURI.ToNewCString();
    
	*uri = PR_smprintf("%s%s#%d", kNewsMessageRootURI, tail, key);
  
	return NS_OK;
}
