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

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsresult
nsGetImapRoot(const char* hostname, nsFileSpec &result)
{
    nsresult rv = NS_OK; 

	  NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv); 
    if (NS_FAILED(rv)) return rv;

	  nsCOMPtr<nsIMsgAccountManager> accountManager;
	  rv = session->GetAccountManager(getter_AddRefs(accountManager));
    if(NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupportsArray> servers;
    rv = accountManager->FindServersByHostname(hostname,
                                               nsIImapIncomingServer::GetIID(),
                                               getter_AddRefs(servers));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIMsgIncomingServer>
        server(do_QueryInterface(servers->ElementAt(0)));

    char *localPath = nsnull;

    if (server)
				rv = server->GetLocalPath(&localPath);

    result = localPath;
    return rv;
}

nsresult
nsImapURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult)
{
	nsresult rv;
	nsAutoString sep;

	sep += PR_GetDirectorySeparator();

	nsAutoString sbdSep;
	/* sspitzer: is this ok for mail and news? */
	rv = nsGetMailFolderSeparator(sbdSep);
	if (NS_FAILED(rv)) 
		return rv;

	nsAutoString uri = uriStr;
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

	hostStart = uri.Find('/');
	if (hostStart <= 0) return NS_ERROR_FAILURE;

	// skip past all //
	while (uri[hostStart]=='/') hostStart++;

	// cut imap://hostname/folder -> hostname/folder
	nsAutoString hostname;
	uri.Right(hostname, uri.Length() - hostStart);

	PRInt32 hostEnd = hostname.Find('/');

	nsAutoString folder;
	// folder comes after the hostname, after the '/'

	// cut off first '/' and everything following it
	// hostname/folder -> hostname
	if (hostEnd > 0) 
	{
		hostname.Right(folder, hostname.Length() - hostEnd - 1);
		hostname.Truncate(hostEnd);
	}
	char *hostchar = hostname.ToNewCString();

	rv = nsGetImapRoot(hostchar, pathResult);

	delete[] hostchar;

	if (NS_FAILED(rv)) 
	{
		pathResult = nsnull;
		return rv;
	}

	if (folder != "")
	  pathResult += folder;

	return NS_OK;
}

nsresult
nsImapURI2Name(const char* rootURI, char* uriStr, nsString& name)
{
  nsAutoString uri = uriStr;
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;
  PRInt32 pos = uri.RFind("/");
  PRInt32 length = uri.Length();
  PRInt32 count = length - (pos + 1);
  return uri.Right(name, count);
}

/* parses ImapMessageURI */
nsresult nsParseImapMessageURI(const char* uri, nsString& folderURI, PRUint32 *key)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsAutoString uriStr = uri;
	PRInt32 keySeparator = uriStr.Find('#');
	if(keySeparator != -1)
	{
		nsAutoString folderPath;
		uriStr.Left(folderURI, keySeparator);
		folderURI.Cut(4, 8);	// cut out the _message part of imap_message:
		nsAutoString keyStr;
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
