/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsNetCID.h"

// stuff for temporary root folder hack
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsIImapIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsImapCore.h"
#include "nsMsgUtils.h"
#include "nsImapFlagAndUidState.h"
#include "nsISupportsObsolete.h"

nsresult
nsImapURI2Path(const char* rootURI, const char* uriStr, nsFileSpec& pathResult)
{
  nsresult rv;
  
  nsAutoString sbdSep;
  nsCOMPtr<nsIURL> url;
 
  rv = nsGetMailFolderSeparator(sbdSep);
  if (NS_FAILED(rv)) 
    return rv;
  
  url = do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCAutoString uri(uriStr);
  if (uri.Find(rootURI) != 0)     // if doesn't start with rootURI
    return NS_ERROR_FAILURE;
  
  if ((PL_strcmp(rootURI, kImapRootURI) != 0) &&
    (PL_strcmp(rootURI, kImapMessageRootURI) != 0)) 
  {
    pathResult = nsnull;
    rv = NS_ERROR_FAILURE; 
  }
  
  // Set our url to the string given
  rv = url->SetSpec(nsDependentCString(uriStr));
  if (NS_FAILED(rv)) return rv;

  // Set the folder to the url path
  nsCAutoString folder;
  rv = url->GetPath(folder);
  // can't have leading '/' in path
  if (folder.CharAt(0) == '/')
    folder.Cut(0, 1);
  // Now find the server from the URL
  nsCOMPtr<nsIMsgIncomingServer> server;
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if(NS_FAILED(rv)) return rv;
  
  rv = accountManager->FindServerByURI(url, PR_FALSE,
    getter_AddRefs(server));

  if (NS_FAILED(rv)) return rv;
  
  if (server) 
  {
    nsCOMPtr<nsIFileSpec> localPath;
    rv = server->GetLocalPath(getter_AddRefs(localPath));
    if (NS_FAILED(rv)) return rv;
    
    rv = localPath->GetFileSpec(&pathResult);
    if (NS_FAILED(rv)) return rv;
    
    // This forces the creation of the parent server directory
    // so that we don't get imapservername.sbd instead
    // when the host directory has been deleted. See bug 210683
    nsFileSpec tempPath(pathResult.GetNativePathCString(), PR_TRUE); 
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
    
    // FIXME : This would break with multibyte encodings such as 
    // Shift_JIS, Big5 because '0x5c' in the second byte of a multibyte
    // character would be mistaken for '/'. 
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
    if (!leafName.IsEmpty()) 
    {
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
    if (uri.Find(rootURI) != 0)
      return NS_ERROR_FAILURE;
    uri.Right(fullName, uri.Length() - strlen(rootURI));
    uri = fullName;
    PRInt32 hostStart = uri.Find(hostname);
    if (hostStart <= 0) 
      return NS_ERROR_FAILURE;
    uri.Right(fullName, uri.Length() - hostStart);
    uri = fullName;
    PRInt32 hostEnd = uri.FindChar('/');
    if (hostEnd <= 0) 
      return NS_ERROR_FAILURE;
    uri.Right(fullName, uri.Length() - hostEnd - 1);
    if (fullName.IsEmpty())
      return NS_ERROR_FAILURE;
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
    folderURI.Cut(4, 8);	// cut out the _message part of imap-message:
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
  supportedUserFlags = 0;
  
  allocatedPathName = nsnull;
  unicharPathName = nsnull;
  hierarchySeparator = '\0';
  hostName = nsnull;
  
  folderSelected = PR_FALSE;
  discoveredFromLsub = PR_FALSE;
  
  onlineVerified = PR_FALSE;
  namespaceForFolder = nsnull;
}

nsImapMailboxSpec::~nsImapMailboxSpec()
{
  nsCRT::free(allocatedPathName);
  nsCRT::free(unicharPathName);
  nsCRT::free(hostName);
}

NS_IMPL_GETSET(nsImapMailboxSpec, Folder_UIDVALIDITY, PRInt32, folder_UIDVALIDITY)
NS_IMPL_GETSET(nsImapMailboxSpec, NumMessages, PRInt32, number_of_messages)
NS_IMPL_GETSET(nsImapMailboxSpec, NumUnseenMessages, PRInt32, number_of_unseen_messages)
NS_IMPL_GETSET(nsImapMailboxSpec, NumRecentMessages, PRInt32, number_of_recent_messages)
NS_IMPL_GETSET(nsImapMailboxSpec, HierarchySeparator, char, hierarchySeparator)
NS_IMPL_GETSET(nsImapMailboxSpec, FolderSelected, PRBool, folderSelected)
NS_IMPL_GETSET(nsImapMailboxSpec, DiscoveredFromLsub, PRBool, discoveredFromLsub)
NS_IMPL_GETSET(nsImapMailboxSpec, OnlineVerified, PRBool, onlineVerified)
NS_IMPL_GETSET_STR(nsImapMailboxSpec, HostName, hostName)
NS_IMPL_GETSET_STR(nsImapMailboxSpec, AllocatedPathName, allocatedPathName)
NS_IMPL_GETSET(nsImapMailboxSpec, SupportedUserFlags, PRUint32, supportedUserFlags)
NS_IMPL_GETSET(nsImapMailboxSpec, Box_flags, PRUint32, box_flags)
NS_IMPL_GETSET(nsImapMailboxSpec, NamespaceForFolder, nsIMAPNamespace *, namespaceForFolder)

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
  PR_Free(unicharPathName);
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
  supportedUserFlags = aCopy.supportedUserFlags;
  
  allocatedPathName = (aCopy.allocatedPathName) ? strdup(aCopy.allocatedPathName) : nsnull;
  unicharPathName = (aCopy.unicharPathName) ? nsCRT::strdup(aCopy.unicharPathName) : nsnull;
  hierarchySeparator = aCopy.hierarchySeparator;
  hostName = strdup(aCopy.hostName);
	
  flagState = aCopy.flagState;
	
  folderSelected = aCopy.folderSelected;
  discoveredFromLsub = aCopy.discoveredFromLsub;

  onlineVerified = aCopy.onlineVerified;

  namespaceForFolder = aCopy.namespaceForFolder;
  
  return *this;
}

// use the flagState to determine if the gaps in the msgUids correspond to gaps in the mailbox,
// in which case we can still use ranges. If flagState is null, we won't do this.
void AllocateImapUidString(PRUint32 *msgUids, PRUint32 &msgCount, 
                           nsImapFlagAndUidState *flagState, nsCString &returnString)
{
  PRUint32 startSequence = (msgCount > 0) ? msgUids[0] : 0xFFFFFFFF;
  PRUint32 curSequenceEnd = startSequence;
  PRUint32 total = msgCount;
  PRInt32  curFlagStateIndex = -1;

  for (PRUint32 keyIndex=0; keyIndex < total; keyIndex++)
  {
    PRUint32 curKey = msgUids[keyIndex];
    PRUint32 nextKey = (keyIndex + 1 < total) ? msgUids[keyIndex + 1] : 0xFFFFFFFF;
    PRBool lastKey = (nextKey == 0xFFFFFFFF);

    if (lastKey)
      curSequenceEnd = curKey;

    if (!lastKey)
    {
      if (nextKey == curSequenceEnd + 1)
      {
        curSequenceEnd = nextKey;
        curFlagStateIndex++;
        continue;
      }
      if (flagState)
      {
        if (curFlagStateIndex == -1)
        {
          PRBool foundIt;
          flagState->GetMessageFlagsFromUID(curSequenceEnd, &foundIt, &curFlagStateIndex);
          NS_ASSERTION(foundIt, "flag state missing key");
        }
        curFlagStateIndex++;
        PRUint32 nextUidInFlagState;
        nsresult rv = flagState->GetUidOfMessage(curFlagStateIndex, &nextUidInFlagState);
        if (NS_SUCCEEDED(rv) && nextUidInFlagState == nextKey)
        {
          curSequenceEnd = nextKey;
          continue;
        }
      }
    }
    if (curSequenceEnd > startSequence)
    {
      returnString.AppendInt(startSequence);
      returnString += ':';
      returnString.AppendInt(curSequenceEnd);
      startSequence = nextKey;
      curSequenceEnd = startSequence;
      curFlagStateIndex = -1;
    }
    else
    {
      startSequence = nextKey;
      curSequenceEnd = startSequence;
      returnString.AppendInt(msgUids[keyIndex]);
      curFlagStateIndex = -1;
    }
    // check if we've generated too long a string - if there's no flag state,
    // it means we just need to go ahead and generate a too long string
    // because the calling code won't handle breaking up the strings.
    if (flagState && returnString.Length() > 950) 
    {
      msgCount = keyIndex;
      break;
    }
    // If we are not the last item then we need to add the comma 
    // but it's important we do it here, after the length check 
    if (!lastKey) 
      returnString += ','; 
  }
}

void ParseUidString(const char *uidString, nsMsgKeyArray &keys)
{
  // This is in the form <id>,<id>, or <id1>:<id2>
  char curChar = *uidString;
  PRBool isRange = PR_FALSE;
  int32 curToken;
  int32 saveStartToken=0;

  for (const char *curCharPtr = uidString; curChar && *curCharPtr;)
  {
    const char *currentKeyToken = curCharPtr;
    curChar = *curCharPtr;
    while (curChar != ':' && curChar != ',' && curChar != '\0')
      curChar = *curCharPtr++;

    // we don't need to null terminate currentKeyToken because atoi 
    // stops at non-numeric chars.
    curToken = atoi(currentKeyToken); 
    if (isRange)
    {
      while (saveStartToken < curToken)
        keys.Add(saveStartToken++);
    }
    keys.Add(curToken);
    isRange = (curChar == ':');
    if (isRange)
      saveStartToken = curToken + 1;
  }
}

