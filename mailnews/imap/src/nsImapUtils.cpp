/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsImapUtils.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIServiceManager.h"
#include "prsystem.h"
#include "nsEscape.h"
#include "nsIFileSpec.h"

// stuff for temporary root folder hack
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsIImapIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsImapCore.h"
#include "nsMsgUtils.h"
#include "nsIImapFlagAndUidState.h"

nsresult
nsImapURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult)
{
	nsresult rv;

	nsAutoString sbdSep;

	rv = nsGetMailFolderSeparator(sbdSep);
	if (NS_FAILED(rv)) 
		return rv;

	nsCAutoString uri(uriStr);
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
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
           do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if(NS_FAILED(rv)) return rv;

  char *unescapedUserName = ToNewCString(username);
  if (unescapedUserName)
  {
	  nsUnescape(unescapedUserName);
	  rv = accountManager->FindServer(unescapedUserName,
									  hostname.get(),
									  "imap",
									  getter_AddRefs(server));
	  PR_FREEIF(unescapedUserName);
  }
  else
	  rv = NS_ERROR_OUT_OF_MEMORY;

  if (NS_FAILED(rv)) return rv;
  
  if (server) {
    nsCOMPtr<nsIFileSpec> localPath;
    rv = server->GetLocalPath(getter_AddRefs(localPath));
    if (NS_FAILED(rv)) return rv;
    
    rv = localPath->GetFileSpec(&pathResult);
    if (NS_FAILED(rv)) return rv;
    
    pathResult.CreateDirectory();
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
          parentName.AppendWithConversion(sbdSep);
          pathResult += parentName.get();
		  // this fixes a strange purify warning.
          parentName = leafName.get();
          dirEnd = parentName.FindChar('/');
      }
      if (!leafName.IsEmpty()) {
        NS_MsgHashIfNecessary(leafName);
        pathResult += leafName.get();
      }
  }

	return NS_OK;
}

nsresult
nsImapURI2FullName(const char* rootURI, const char* hostname, const char* uriStr,
                   char **name)
{
    nsAutoString uri; uri.AssignWithConversion(uriStr);
    nsAutoString fullName;
    if (uri.Find(rootURI) != 0) return NS_ERROR_FAILURE;
    PRInt32 hostStart = uri.Find(hostname);
    if (hostStart <= 0) return NS_ERROR_FAILURE;
    uri.Right(fullName, uri.Length() - hostStart);
    uri = fullName;
    PRInt32 hostEnd = uri.FindChar('/');
    if (hostEnd <= 0) return NS_ERROR_FAILURE;
    uri.Right(fullName, uri.Length() - hostEnd - 1);
    if (fullName.IsEmpty()) return NS_ERROR_FAILURE;
    *name = ToNewCString(fullName);
    return NS_OK;
}

/* parses ImapMessageURI */
nsresult nsParseImapMessageURI(const char* uri, nsCString& folderURI, PRUint32 *key, char **part)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString uriStr(uri);
	PRInt32 keySeparator = uriStr.RFindChar('#');
	if(keySeparator != -1)
	{
    PRInt32 keyEndSeparator = uriStr.FindCharInSet("/?&", 
                                                   keySeparator); 
		nsAutoString folderPath;
		uriStr.Left(folderURI, keySeparator);
		folderURI.Cut(4, 8);	// cut out the _message part of imap_message:
		nsCAutoString keyStr;
    if (keyEndSeparator != -1)
        uriStr.Mid(keyStr, keySeparator+1, 
                   keyEndSeparator-(keySeparator+1));
    else
        uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
		PRInt32 errorCode;
		*key = keyStr.ToInteger(&errorCode);

    if (part && keyEndSeparator != -1)
    {
      PRInt32 partPos = uriStr.Find("part=", PR_FALSE, keyEndSeparator);
      if (partPos != -1)
      {
        nsCString partSubStr;
        uriStr.Right(partSubStr, uriStr.Length() - keyEndSeparator);
        *part = ToNewCString(partSubStr);

      }
    }
	}
	return NS_OK;

}

nsresult nsBuildImapMessageURI(const char *baseURI, PRUint32 key, nsCString& uri)
{
	
	uri.Append(baseURI);
	uri.Append('#');
	uri.AppendInt(key);
	return NS_OK;
}

nsresult nsCreateImapBaseMessageURI(const char *baseURI, char **baseMessageURI)
{
	if(!baseMessageURI)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString tailURI(baseURI);

	// chop off mailbox:/
	if (tailURI.Find(kImapRootURI) == 0)
		tailURI.Cut(0, PL_strlen(kImapRootURI));
	
	nsCAutoString baseURIStr(kImapMessageRootURI);
	baseURIStr += tailURI;

	*baseMessageURI = ToNewCString(baseURIStr);
	if(!*baseMessageURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}

// nsImapMailboxSpec stuff

NS_IMPL_ISUPPORTS1(nsImapMailboxSpec, nsIMailboxSpec)

nsImapMailboxSpec::nsImapMailboxSpec()
{
	folder_UIDVALIDITY = 0;
	number_of_messages = 0;
	number_of_unseen_messages = 0;
	number_of_recent_messages = 0;
	
	box_flags = 0;
	
	allocatedPathName = nsnull;
	unicharPathName = nsnull;
	hierarchySeparator = '\0';
	hostName = nsnull;
	
	folderSelected = PR_FALSE;
	discoveredFromLsub = PR_FALSE;

	onlineVerified = PR_FALSE;
	namespaceForFolder = nsnull;
	NS_INIT_REFCNT ();
}

nsImapMailboxSpec::~nsImapMailboxSpec()
{
	if (allocatedPathName)
		nsCRT::free(allocatedPathName);
	if (unicharPathName)
		nsCRT::free(unicharPathName);
	if (hostName)
		nsCRT::free(hostName);
}

NS_IMPL_GETSET(nsImapMailboxSpec, Folder_UIDVALIDITY, PRInt32, folder_UIDVALIDITY);
NS_IMPL_GETSET(nsImapMailboxSpec, Number_of_messages, PRInt32, number_of_messages);
NS_IMPL_GETSET(nsImapMailboxSpec, Number_of_unseen_messages, PRInt32, number_of_unseen_messages);
NS_IMPL_GETSET(nsImapMailboxSpec, Number_of_recent_messages, PRInt32, number_of_recent_messages);
NS_IMPL_GETSET(nsImapMailboxSpec, HierarchySeparator, char, hierarchySeparator);
NS_IMPL_GETSET(nsImapMailboxSpec, FolderSelected, PRBool, folderSelected);
NS_IMPL_GETSET(nsImapMailboxSpec, DiscoveredFromLsub, PRBool, discoveredFromLsub);
NS_IMPL_GETSET(nsImapMailboxSpec, OnlineVerified, PRBool, onlineVerified);
NS_IMPL_GETSET_STR(nsImapMailboxSpec, HostName, hostName);
NS_IMPL_GETSET_STR(nsImapMailboxSpec, AllocatedPathName, allocatedPathName);
NS_IMPL_GETSET(nsImapMailboxSpec, Box_flags, PRUint32, box_flags);
NS_IMPL_GETSET(nsImapMailboxSpec, NamespaceForFolder, nsIMAPNamespace *, namespaceForFolder);

NS_IMETHODIMP nsImapMailboxSpec::GetUnicharPathName(PRUnichar **aUnicharPathName)
{
	if (!aUnicharPathName)
		return NS_ERROR_NULL_POINTER;
	*aUnicharPathName = (unicharPathName) ? nsCRT::strdup(unicharPathName) : nsnull;
	return NS_OK;
}

NS_IMETHODIMP nsImapMailboxSpec::GetFlagState(nsIImapFlagAndUidState ** aFlagState)
{
	if (!aFlagState)
		return NS_ERROR_NULL_POINTER;
	*aFlagState = flagState;
	NS_IF_ADDREF(*aFlagState);
	return NS_OK;
}

NS_IMETHODIMP nsImapMailboxSpec::SetFlagState(nsIImapFlagAndUidState * aFlagState)
{
	flagState = aFlagState;
	return NS_OK;
}


NS_IMETHODIMP nsImapMailboxSpec::SetUnicharPathName(const PRUnichar *aUnicharPathName)
{
	PR_FREEIF(unicharPathName);
	unicharPathName= (aUnicharPathName) ? nsCRT::strdup(aUnicharPathName ) : nsnull;
	return (unicharPathName) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsImapMailboxSpec& nsImapMailboxSpec::operator=(const nsImapMailboxSpec& aCopy) 
{
  folder_UIDVALIDITY = aCopy.folder_UIDVALIDITY;
  number_of_messages = aCopy.number_of_messages;
  number_of_unseen_messages = aCopy.number_of_unseen_messages;
  number_of_recent_messages = aCopy.number_of_recent_messages;
	
  box_flags = aCopy.box_flags;
	
  allocatedPathName = (aCopy.allocatedPathName) ? nsCRT::strdup(aCopy.allocatedPathName) : nsnull;
  unicharPathName = (aCopy.unicharPathName) ? nsCRT::strdup(aCopy.unicharPathName) : nsnull;
  hierarchySeparator = aCopy.hierarchySeparator;
  hostName = nsCRT::strdup(aCopy.hostName);
	
  flagState = aCopy.flagState;
	
  folderSelected = aCopy.folderSelected;
  discoveredFromLsub = aCopy.discoveredFromLsub;

  onlineVerified = aCopy.onlineVerified;

  namespaceForFolder = aCopy.namespaceForFolder;
  
  return *this;
}

void AllocateImapUidString(PRUint32 *msgUids, PRUint32 msgCount, nsCString &returnString)
{
  PRUint32 startSequence = (msgCount > 0) ? msgUids[0] : 0xFFFFFFFF;
  PRUint32 curSequenceEnd = startSequence;
  PRUint32 total = msgCount;

  for (PRUint32 keyIndex=0; keyIndex < total; keyIndex++)
  {
    PRUint32 curKey = msgUids[keyIndex];
    PRUint32 nextKey = (keyIndex + 1 < total) ? msgUids[keyIndex + 1] : 0xFFFFFFFF;
    PRBool lastKey = (nextKey == 0xFFFFFFFF);

    if (lastKey)
      curSequenceEnd = curKey;
    if (nextKey == curSequenceEnd + 1 && !lastKey)
    {
      curSequenceEnd = nextKey;
      continue;
    }
    else if (curSequenceEnd > startSequence)
    {
      returnString.AppendInt(startSequence, 10);
      returnString += ':';
      returnString.AppendInt(curSequenceEnd, 10);
      if (!lastKey)
        returnString += ',';
//      sprintf(currentidString, "%ld:%ld,", startSequence, curSequenceEnd);
      startSequence = nextKey;
      curSequenceEnd = startSequence;
    }
    else
    {
      startSequence = nextKey;
      curSequenceEnd = startSequence;
      returnString.AppendInt(msgUids[keyIndex], 10);
      if (!lastKey)
        returnString += ',';
//      sprintf(currentidString, "%ld,", msgUids[keyIndex]);
    }
  }
}

