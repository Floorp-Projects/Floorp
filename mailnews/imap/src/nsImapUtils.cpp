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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"
#include "nsImapUtils.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "prsystem.h"

// stuff for temporary root folder hack
#include "nsIMsgAccountManager.h"
#include "nsIMsgIncomingServer.h"
#include "nsIImapIncomingServer.h"
#include "nsMsgBaseCID.h"
#include "nsImapCore.h"
#include "nsMsgUtils.h"
#include "nsIImapFlagAndUidState.h"
#include "nsICharsetConverterManager.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

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
  NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                  NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  if(NS_FAILED(rv)) return rv;

  char *unescapedUserName = username.ToNewCString();
  if (unescapedUserName)
  {
	  nsUnescape(unescapedUserName);
	  rv = accountManager->FindServer(unescapedUserName,
									  hostname,
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
          pathResult += (const char *) parentName;
		  // this fixes a strange purify warning.
          parentName = (const char *) leafName;
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
    *name = fullName.ToNewCString();
    return NS_OK;
}

/* parses ImapMessageURI */
nsresult nsParseImapMessageURI(const char* uri, nsCString& folderURI, PRUint32 *key, char **part)
{
	if(!key)
		return NS_ERROR_NULL_POINTER;

	nsCAutoString uriStr(uri);
	PRInt32 keySeparator = uriStr.FindChar('#');
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
        *part = partSubStr.ToNewCString();

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

	*baseMessageURI = baseURIStr.ToNewCString();
	if(!*baseMessageURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}

// nsImapMailboxSpec stuff

NS_IMPL_ISUPPORTS(nsImapMailboxSpec, NS_GET_IID(nsIMailboxSpec));

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

// convert back and forth between imap utf7 and unicode.
char*
CreateUtf7ConvertedString(const char * aSourceString, 
                      PRBool aConvertToUtf7Imap)
{
  nsresult res;
  char *dstPtr = nsnull;
  PRInt32 dstLength = 0;
  char *convertedString = NULL;
  
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRUnichar *unichars = nsnull;
    PRInt32 unicharLength;

    if (!aConvertToUtf7Imap)
    {
      // convert utf7 to unicode
      nsIUnicodeDecoder* decoder = nsnull;

      res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
      if(NS_SUCCEEDED(res) && (nsnull != decoder)) 
      {
        PRInt32 srcLen = PL_strlen(aSourceString);
        res = decoder->GetMaxLength(aSourceString, srcLen, &unicharLength);
        // temporary buffer to hold unicode string
        unichars = new PRUnichar[unicharLength + 1];
        if (unichars == nsnull) 
        {
          res = NS_ERROR_OUT_OF_MEMORY;
        }
        else 
        {
          res = decoder->Convert(aSourceString, &srcLen, unichars, &unicharLength);
          unichars[unicharLength] = 0;
        }
        NS_IF_RELEASE(decoder);
        // convert the unicode to 8 bit ascii.
        nsString unicodeStr(unichars);
        convertedString = (char *) PR_Malloc(unicharLength + 1);
        if (convertedString)
          unicodeStr.ToCString(convertedString, unicharLength + 1, 0);
      }
    }
    else
    {
      // convert from 8 bit ascii string to modified utf7
      nsString unicodeStr; unicodeStr.AssignWithConversion(aSourceString);
      nsIUnicodeEncoder* encoder = nsnull;
      aCharset.AssignWithConversion("x-imap4-modified-utf7");
      res = ccm->GetUnicodeEncoder(&aCharset, &encoder);
      if(NS_SUCCEEDED(res) && (nsnull != encoder)) 
      {
        res = encoder->GetMaxLength(unicodeStr.GetUnicode(), unicodeStr.Length(), &dstLength);
        // allocale an output buffer
        dstPtr = (char *) PR_CALLOC(dstLength + 1);
        unicharLength = unicodeStr.Length();
        if (dstPtr == nsnull) 
        {
          res = NS_ERROR_OUT_OF_MEMORY;
        }
        else 
        {
          res = encoder->Convert(unicodeStr.GetUnicode(), &unicharLength, dstPtr, &dstLength);
          dstPtr[dstLength] = 0;
        }
      }
      NS_IF_RELEASE(encoder);
      nsString unicodeStr2; unicodeStr2.AssignWithConversion(dstPtr);
      convertedString = (char *) PR_Malloc(dstLength + 1);
      if (convertedString)
        unicodeStr2.ToCString(convertedString, dstLength + 1, 0);
        }
        delete [] unichars;
      }
    PR_FREEIF(dstPtr);
    return convertedString;
}

// convert back and forth between imap utf7 and unicode.
char*
CreateUtf7ConvertedStringFromUnicode(const PRUnichar * aSourceString)
{
  nsresult res;
  char *dstPtr = nsnull;
  PRInt32 dstLength = 0;
  
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRInt32 unicharLength;

      // convert from 8 bit ascii string to modified utf7
      nsString unicodeStr(aSourceString);
      nsIUnicodeEncoder* encoder = nsnull;
      aCharset.AssignWithConversion("x-imap4-modified-utf7");
      res = ccm->GetUnicodeEncoder(&aCharset, &encoder);
      if(NS_SUCCEEDED(res) && (nsnull != encoder)) 
      {
        res = encoder->GetMaxLength(unicodeStr.GetUnicode(), unicodeStr.Length(), &dstLength);
        // allocale an output buffer
        dstPtr = (char *) PR_CALLOC(dstLength + 1);
        unicharLength = unicodeStr.Length();
        if (dstPtr == nsnull) 
        {
          res = NS_ERROR_OUT_OF_MEMORY;
        }
        else 
        {
			// this should be enough of a finish buffer - utf7 isn't changing, and it'll always be '-'
		  char finishBuffer[20];
		  PRInt32 finishSize = sizeof(finishBuffer);

          res = encoder->Convert(unicodeStr.GetUnicode(), &unicharLength, dstPtr, &dstLength);
		  encoder->Finish(finishBuffer, &finishSize);
		  finishBuffer[finishSize] = '\0';
          dstPtr[dstLength] = 0;
		  strcat(dstPtr, finishBuffer);
        }
      }
      NS_IF_RELEASE(encoder);
        }
    return dstPtr;
}


nsresult CreateUnicodeStringFromUtf7(const char *aSourceString, PRUnichar **aUnicodeStr)
{
  if (!aUnicodeStr)
	  return NS_ERROR_NULL_POINTER;

  PRUnichar *convertedString = NULL;
  nsresult res;
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, kCharsetConverterManagerCID, &res); 

  if(NS_SUCCEEDED(res) && (nsnull != ccm))
  {
    nsString aCharset; aCharset.AssignWithConversion("x-imap4-modified-utf7");
    PRUnichar *unichars = nsnull;
    PRInt32 unicharLength;

    // convert utf7 to unicode
    nsIUnicodeDecoder* decoder = nsnull;

    res = ccm->GetUnicodeDecoder(&aCharset, &decoder);
    if(NS_SUCCEEDED(res) && (nsnull != decoder)) 
    {
      PRInt32 srcLen = PL_strlen(aSourceString);
      res = decoder->GetMaxLength(aSourceString, srcLen, &unicharLength);
      // temporary buffer to hold unicode string
      unichars = new PRUnichar[unicharLength + 1];
      if (unichars == nsnull) 
      {
        res = NS_ERROR_OUT_OF_MEMORY;
      }
      else 
      {
        res = decoder->Convert(aSourceString, &srcLen, unichars, &unicharLength);
        unichars[unicharLength] = 0;
      }
      NS_IF_RELEASE(decoder);
      nsString unicodeStr(unichars);
      convertedString = unicodeStr.ToNewUnicode();
	  delete [] unichars;
    }
  }
  *aUnicodeStr = convertedString;
  return (convertedString) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

