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
#include "nsImapUtils.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "prsystem.h"

// stuff for temporary root folder hack
#include "nsIMsgMailSession.h"
#include "nsIMsgIncomingServer.h"
#include "nsIImapIncomingServer.h"
#include "nsMsgBaseCID.h"

#include "nsMsgUtils.h"

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsresult
nsGetImapServer(const char* username, const char* hostname,
                nsIMsgIncomingServer ** aResult)
{
    nsresult rv = NS_OK; 

	NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv); 
    if (NS_FAILED(rv)) return rv;

    
	nsCOMPtr<nsIMsgAccountManager> accountManager;
	rv = session->GetAccountManager(getter_AddRefs(accountManager));
    if(NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = accountManager->FindServer(username,
                                    hostname,
                                    "imap",
                                    getter_AddRefs(server));
    if (NS_FAILED(rv)) return rv;

    *aResult = server;
    NS_IF_ADDREF(*aResult);

    return rv;
}

nsresult
nsImapURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult)
{
	nsresult rv;

	nsAutoString sbdSep;

	rv = nsGetMailFolderSeparator(sbdSep);
	if (NS_FAILED(rv)) 
		return rv;

	nsCAutoString uri = uriStr;
	if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
		return NS_ERROR_FAILURE;

	if ((PL_strcmp(rootURI, kImapRootURI) != 0) &&
		   (PL_strcmp(rootURI, kImapMessageRootURI) != 0)) 
	{
		pathResult = nsnull;
		rv = NS_ERROR_FAILURE; 
	}

	// the server name is the first component of the path, so extract it out
	PRInt32 hostStart;

	hostStart = uri.FindChar('/');
	if (hostStart <= 0) return NS_ERROR_FAILURE;

	// skip past all //
	while (uri.CharAt(hostStart) =='/') hostStart++;

	// cut imap://[userid@]hostname/folder -> [userid@]hostname/folder
	nsCAutoString hostname;
	uri.Right(hostname, uri.Length() - hostStart);

	nsCAutoString username;

	PRInt32 atPos = hostname.FindChar('@');
	if (atPos != -1) {
		hostname.Left(username, atPos);
		hostname.Cut(0, atPos+1);
	}
  
	nsCAutoString folder;
	// folder comes after the hostname, after the '/'


	// cut off first '/' and everything following it
	// hostname/folder -> hostname
	PRInt32 hostEnd = hostname.FindChar('/');
	if (hostEnd > 0) 
	{
		hostname.Right(folder, hostname.Length() - hostEnd - 1);
		hostname.Truncate(hostEnd);
	}

	nsCOMPtr<nsIMsgIncomingServer> server;
	rv = nsGetImapServer((const char *) username,
						 (const char *) hostname,
                       getter_AddRefs(server));
  
  if (NS_FAILED(rv)) return rv;
  
  char *localPath = nsnull;
  if (server) {
    rv = server->GetLocalPath(&localPath);
    
    if (NS_SUCCEEDED(rv)) {
      pathResult = localPath;
      pathResult.CreateDirectory();
      PL_strfree(localPath);
    }
  }

	if (NS_FAILED(rv)) 
	{
		pathResult = nsnull;
		return rv;
	}

  if (!folder.IsEmpty())
  {
      nsCAutoString parentName = folder;
      nsCAutoString leafName = folder;
      PRInt32 dirEnd = parentName.FindChar('/');

      while(dirEnd > 0)
      {
          parentName.Right(leafName, parentName.Length() - dirEnd -1);
          parentName.Truncate(dirEnd);
          NS_MsgHashIfNecessary(parentName);
          parentName += sbdSep;
          pathResult += (const char *) parentName;
          parentName = leafName;
          dirEnd = parentName.FindChar('/');
      }
      if (!leafName.IsEmpty()) {
        NS_MsgHashIfNecessary(leafName);
        pathResult += (const char *) leafName;
      }
  }

	return NS_OK;
}

nsresult
nsImapURI2FullName(const char* rootURI, const char* hostname, char* uriStr,
                   nsString& name)
{
    nsAutoString uri = uriStr;
    nsAutoString fullName;
    if (uri.Find(rootURI) != 0) return NS_ERROR_FAILURE;
    PRInt32 hostStart = uri.Find(hostname);
    if (hostStart <= 0) return NS_ERROR_FAILURE;
    uri.Right(fullName, uri.Length() - hostStart);
    uri = fullName;
    PRInt32 hostEnd = uri.FindChar('/');
    if (hostEnd <= 0) return NS_ERROR_FAILURE;
    uri.Right(fullName, uri.Length() - hostEnd - 1);
    if (fullName == "") return NS_ERROR_FAILURE;
    name = fullName;
    return NS_OK;
}

nsresult
nsImapURI2UserName(const char* rootURI, const char* uriStr,
                   nsString& username)
{
    nsAutoString uri = uriStr;
    if (uri.Find(rootURI) != 0) return NS_ERROR_FAILURE;
    PRInt32 userStart = PL_strlen(rootURI);
    while (uri[userStart] == '/') userStart++;
    uri.Cut(0, userStart);
    PRInt32 userEnd = uri.FindChar('@');
    if (userEnd < 1)
        return NS_ERROR_FAILURE;
    uri.SetLength(userEnd);
    username = uri;
    return NS_OK;
}

nsresult
nsImapURI2HostName(const char* rootURI, const char* uriStr, 
                   nsString& hostname)
{
    nsAutoString uri = uriStr;
    if (uri.Find(rootURI) != 0) return NS_ERROR_FAILURE;
    PRInt32 hostStart = PL_strlen(rootURI);
    while (uri[hostStart] == '/') hostStart++;
    uri.Cut(0, hostStart);
    hostStart = uri.FindChar('@'); // skip username
    if (hostStart > 0)
        uri.Cut(0, hostStart+1);
    PRInt32 hostEnd = uri.FindChar('/');
    if (hostEnd > 0)
        uri.SetLength(hostEnd);
    hostname = uri;
    return NS_OK;
}

nsresult
nsURI2ProtocolType(const char* uriStr, nsString& type)
{
    nsAutoString uri = uriStr;
    PRInt32 typeEnd = uri.FindChar(':');
    if (typeEnd < 1)
        return NS_ERROR_FAILURE;
    uri.SetLength(typeEnd);
    type = uri;
    return NS_OK;
}

/* parses ImapMessageURI */
nsresult nsParseImapMessageURI(const char* uri, nsCString& folderURI, PRUint32 *key)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString uriStr = uri;
	PRInt32 keySeparator = uriStr.FindChar('#');
	if(keySeparator != -1)
	{
		nsAutoString folderPath;
		uriStr.Left(folderURI, keySeparator);
		folderURI.Cut(4, 8);	// cut out the _message part of imap_message:
		nsCAutoString keyStr;
		uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
		PRInt32 errorCode;
		*key = keyStr.ToInteger(&errorCode);

		return errorCode;
	}
	return NS_ERROR_FAILURE;

}

nsresult nsBuildImapMessageURI(const char *baseURI, PRUint32 key, char** uri)
{
	
	if(!uri)
		return NS_ERROR_NULL_POINTER;

	nsAutoString tailURI(baseURI);

	if (tailURI.Find(kImapRootURI) == 0)
		tailURI.Cut(0, PL_strlen(kImapRootURI));

	char *tail = tailURI.ToNewCString();

	*uri = PR_smprintf("%s%s#%d", kImapMessageRootURI, tail, key);
	delete[] tail;

	return NS_OK;
}
