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
#include "nsIMsgHdr.h"
#include "nsMsgUtils.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsFileSpec.h"
#include "nsEscape.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIImapUrl.h"
#include "nsIMailboxUrl.h"
#include "nsINntpUrl.h"
#include "nsMsgNewsCID.h"
#include "nsMsgLocalCID.h"
#include "nsMsgBaseCID.h"
#include "nsMsgImapCID.h"
#include "nsMsgI18N.h"
#include "prprf.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsIRDFService.h"
#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsISpamSettings.h"


static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);

#define NS_PASSWORDMANAGER_CATEGORY "passwordmanager"
static PRBool gInitPasswordManager = PR_FALSE;

nsresult GetMessageServiceContractIDForURI(const char *uri, nsCString &contractID)
{
  nsresult rv = NS_OK;
  //Find protocol
  nsCAutoString uriStr(uri);
  PRInt32 pos = uriStr.FindChar(':');
  if(pos == kNotFound)
    return NS_ERROR_FAILURE;

  nsCAutoString protocol;
  uriStr.Left(protocol, pos);

  //Build message service contractid
  contractID = "@mozilla.org/messenger/messageservice;1?type=";
  contractID += protocol.get();

  return rv;
}

nsresult GetMessageServiceFromURI(const char *uri, nsIMsgMessageService **aMessageService)
{
  nsresult rv;

  nsCAutoString contractID;
  rv = GetMessageServiceContractIDForURI(uri, contractID);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgMessageService> msgService = do_GetService(contractID.get(), &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  NS_IF_ADDREF(*aMessageService = msgService);
  return rv;
}

nsresult GetMsgDBHdrFromURI(const char *uri, nsIMsgDBHdr **msgHdr)
{
  nsCOMPtr <nsIMsgMessageService> msgMessageService;
  nsresult rv = GetMessageServiceFromURI(uri, getter_AddRefs(msgMessageService));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!msgMessageService) return NS_ERROR_FAILURE;

  return msgMessageService->MessageURIToMsgHdr(uri, msgHdr);
}

nsresult CreateStartupUrl(char *uri, nsIURI** aUrl)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  if (!uri || !*uri || !aUrl) return rv;
  *aUrl = nsnull;
  
  // XXX fix this, so that base doesn't depend on imap, local or news.
  // we can't do NS_NewURI(uri, aUrl), because these are imap-message://, mailbox-message://, news-message:// uris.
  // I think we should do something like GetMessageServiceFromURI() to get the service, and then have the service create the 
  // appropriate nsI*Url, and then QI to nsIURI, and return it.
  // see bug #110689
  if (PL_strncasecmp(uri, "imap", 4) == 0)
  {
    nsCOMPtr<nsIImapUrl> imapUrl = do_CreateInstance(kImapUrlCID, &rv);
    
    if (NS_SUCCEEDED(rv) && imapUrl)
      rv = imapUrl->QueryInterface(NS_GET_IID(nsIURI),
      (void**) aUrl);
  }
  else if (PL_strncasecmp(uri, "mailbox", 7) == 0)
  {
    nsCOMPtr<nsIMailboxUrl> mailboxUrl = do_CreateInstance(kCMailboxUrl, &rv);
    if (NS_SUCCEEDED(rv) && mailboxUrl)
      rv = mailboxUrl->QueryInterface(NS_GET_IID(nsIURI),
      (void**) aUrl);
  }
  else if (PL_strncasecmp(uri, "news", 4) == 0)
  {
    nsCOMPtr<nsINntpUrl> nntpUrl = do_CreateInstance(kCNntpUrlCID, &rv);
    if (NS_SUCCEEDED(rv) && nntpUrl)
      rv = nntpUrl->QueryInterface(NS_GET_IID(nsIURI),
      (void**) aUrl);
  }
  if (*aUrl)
    (*aUrl)->SetSpec(nsDependentCString(uri));
  return rv;
}


// Where should this live? It's a utility used to convert a string priority, e.g., "High, Low, Normal" to an enum.
// Perhaps we should have an interface that groups together all these utilities...
nsresult NS_MsgGetPriorityFromString(const char *priority, nsMsgPriorityValue *outPriority)
{
	if (!outPriority)
		return NS_ERROR_NULL_POINTER;

	nsMsgPriorityValue retPriority = nsMsgPriority::normal;

	if (PL_strcasestr(priority, "Normal") != NULL)
		retPriority = nsMsgPriority::normal;
	else if (PL_strcasestr(priority, "Lowest") != NULL)
		retPriority = nsMsgPriority::lowest;
	else if (PL_strcasestr(priority, "Highest") != NULL)
		retPriority = nsMsgPriority::highest;
	else if (PL_strcasestr(priority, "High") != NULL || 
			 PL_strcasestr(priority, "Urgent") != NULL)
		retPriority = nsMsgPriority::high;
	else if (PL_strcasestr(priority, "Low") != NULL ||
			 PL_strcasestr(priority, "Non-urgent") != NULL)
		retPriority = nsMsgPriority::low;
	else if (PL_strcasestr(priority, "1") != NULL)
		retPriority = nsMsgPriority::highest;
	else if (PL_strcasestr(priority, "2") != NULL)
		retPriority = nsMsgPriority::high;
	else if (PL_strcasestr(priority, "3") != NULL)
		retPriority = nsMsgPriority::normal;
	else if (PL_strcasestr(priority, "4") != NULL)
		retPriority = nsMsgPriority::low;
	else if (PL_strcasestr(priority, "5") != NULL)
	    retPriority = nsMsgPriority::lowest;
	else
		retPriority = nsMsgPriority::normal;
	*outPriority = retPriority;
	return NS_OK;
		//return nsMsgNoPriority;
}


nsresult NS_MsgGetUntranslatedPriorityName (nsMsgPriorityValue p, nsString *outName)
{
	if (!outName)
		return NS_ERROR_NULL_POINTER;
	switch (p)
	{
	case nsMsgPriority::notSet:
	case nsMsgPriority::none:
		outName->Assign(NS_LITERAL_STRING("None"));
		break;
	case nsMsgPriority::lowest:
		outName->Assign(NS_LITERAL_STRING("Lowest"));
		break;
	case nsMsgPriority::low:
		outName->Assign(NS_LITERAL_STRING("Low"));
		break;
	case nsMsgPriority::normal:
		outName->Assign(NS_LITERAL_STRING("Normal"));
		break;
	case nsMsgPriority::high:
		outName->Assign(NS_LITERAL_STRING("High"));
		break;
	case nsMsgPriority::highest:
		outName->Assign(NS_LITERAL_STRING("Highest"));
		break;
	default:
		NS_ASSERTION(PR_FALSE, "invalid priority value");
	}
	return NS_OK;
}

/* this used to be XP_StringHash2 from xp_hash.c */
/* phong's linear congruential hash  */
static PRUint32 StringHash(const char *ubuf)
{
  unsigned char * buf = (unsigned char*) ubuf;
  PRUint32 h=1;
  while(*buf) {
    h = 0x63c63cd9*h + 0x9c39c33d + (int32)*buf;
    buf++;
  }
  return h;
}

nsresult NS_MsgHashIfNecessary(nsCAutoString &name)
{
#if defined(XP_MAC)
  const PRUint32 MAX_LEN = 25;
#elif defined(XP_UNIX) || defined(XP_BEOS)
  const PRUint32 MAX_LEN = 55;
#elif defined(XP_WIN32)
  const PRUint32 MAX_LEN = 55;
#elif defined(XP_OS2)
  const PRUint32 MAX_LEN = 55;
#else
  #error need_to_define_your_max_filename_length
#endif
  nsCAutoString illegalChars(FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS);
  nsCAutoString str(name);

  // Given a filename, make it safe for filesystem
  // certain filenames require hashing because they 
  // are too long or contain illegal characters
  PRInt32 illegalCharacterIndex = str.FindCharInSet(illegalChars);
  char hashedname[MAX_LEN + 1];
  if (illegalCharacterIndex == kNotFound) 
  {
    // no illegal chars, it's just too long
    // keep the initial part of the string, but hash to make it fit
    if (str.Length() > MAX_LEN) 
    {
      PL_strncpy(hashedname, str.get(), MAX_LEN + 1);
      PR_snprintf(hashedname + MAX_LEN - 8, 9, "%08lx",
                (unsigned long) StringHash(str.get()));
      name = hashedname;
    }
  }
  else 
  {
      // found illegal chars, hash the whole thing
      // if we do substitution, then hash, two strings
      // could hash to the same value.
      // for example, on mac:  "foo__bar", "foo:_bar", "foo::bar"
      // would map to "foo_bar".  this way, all three will map to
      // different values
      PR_snprintf(hashedname, 9, "%08lx",
                (unsigned long) StringHash(str.get()));
      name = hashedname;
  }
  
  return NS_OK;
}

nsresult NS_MsgCreatePathStringFromFolderURI(const char *folderURI, nsCString& pathString)
{
	// A file name has to be in native charset, convert from UTF-8.
	nsCAutoString oldPath;
  if (nsCRT::IsAscii(folderURI))
    oldPath.Assign(folderURI);
  else
  {
    char *nativeString = nsnull;
    nsresult rv = ConvertFromUnicode(nsMsgI18NFileSystemCharset(), 
                                     nsAutoString(NS_ConvertUTF8toUCS2(folderURI)), &nativeString);
    if (NS_SUCCEEDED(rv) && nativeString && nativeString[0])
      oldPath.Assign(nativeString);
    else
      oldPath.Assign(folderURI);
    PR_FREEIF(nativeString);
  }

	nsCAutoString pathPiece;

	PRInt32 startSlashPos = oldPath.FindChar('/');
	PRInt32 endSlashPos = (startSlashPos >= 0) 
		? oldPath.FindChar('/', startSlashPos + 1) - 1 : oldPath.Length() - 1;
	if (endSlashPos < 0)
		endSlashPos = oldPath.Length();
    // trick to make sure we only add the path to the first n-1 folders
    PRBool haveFirst=PR_FALSE;
    while (startSlashPos != -1) {
	  oldPath.Mid(pathPiece, startSlashPos + 1, endSlashPos - startSlashPos);
      // skip leading '/' (and other // style things)
      if (pathPiece.Length() > 0) {

        // add .sbd onto the previous path
        if (haveFirst) {
          pathString+=".sbd";
          pathString += "/";
        }
        
        NS_MsgHashIfNecessary(pathPiece);
        pathString += pathPiece;
        haveFirst=PR_TRUE;
      }
	  // look for the next slash
      startSlashPos = endSlashPos + 1;

	  endSlashPos = (startSlashPos >= 0) 
			? oldPath.FindChar('/', startSlashPos + 1)  - 1: oldPath.Length() - 1;
	  if (endSlashPos < 0)
			endSlashPos = oldPath.Length();

      if (startSlashPos >= endSlashPos)
		  break;
    }

	return NS_OK;
}

/* Given a string and a length, removes any "Re:" strings from the front.
   It also deals with that dumbass "Re[2]:" thing that some losing mailers do.

   Returns PR_TRUE if it made a change, PR_FALSE otherwise.

   The string is not altered: the pointer to its head is merely advanced,
   and the length correspondingly decreased.
 */
PRBool NS_MsgStripRE(const char **stringP, PRUint32 *lengthP, char **modifiedSubject)
{
  const char *s, *s_end;
  const char *last;
  PRUint32 L;
  PRBool result = PR_FALSE;
  NS_ASSERTION(stringP, "bad null param");
  if (!stringP) return PR_FALSE;

  // decode the string
  nsXPIDLCString decodedString;
  nsCOMPtr<nsIMimeConverter> mimeConverter;
  nsresult rv;
  // we cannot strip "Re:" for MIME encoded subject without modifying the original
  if (modifiedSubject && strstr(*stringP, "=?"))
  {
    mimeConverter = do_GetService(NS_MIME_CONVERTER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      rv = mimeConverter->DecodeMimeHeader(*stringP, getter_Copies(decodedString));
  }

  s = decodedString ? decodedString.get() : *stringP;
  L = lengthP ? *lengthP : strlen(s);

  s_end = s + L;
  last = s;

 AGAIN:

  while (s < s_end && IS_SPACE(*s))
	s++;

  if (s < (s_end-2) &&
	  (s[0] == 'r' || s[0] == 'R') &&
	  (s[1] == 'e' || s[1] == 'E'))
	{
	  if (s[2] == ':')
		{
		  s = s+3;			/* Skip over "Re:" */
		  result = PR_TRUE;	/* Yes, we stripped it. */
		  goto AGAIN;		/* Skip whitespace and try again. */
		}
	  else if (s[2] == '[' || s[2] == '(')
		{
		  const char *s2 = s+3;		/* Skip over "Re[" */

		  /* Skip forward over digits after the "[". */
		  while (s2 < (s_end-2) && IS_DIGIT(*s2))
			s2++;

		  /* Now ensure that the following thing is "]:"
			 Only if it is do we alter `s'.
		   */
		  if ((s2[0] == ']' || s2[0] == ')') && s2[1] == ':')
			{
			  s = s2+2;			/* Skip over "]:" */
			  result = PR_TRUE;	/* Yes, we stripped it. */
			  goto AGAIN;		/* Skip whitespace and try again. */
			}
		}
	}

  if (decodedString)
  {
    // encode the string back if any modification is made
    if (s != decodedString.get())
    {
      // extract between "=?" and "?"
      // e.g. =?ISO-2022-JP?
      const char *p1 = strstr(*stringP, "=?");
      if (p1) 
      {
        p1 += sizeof("=?")-1;         // skip "=?"
        const char *p2 = strchr(p1, '?');   // then search for '?' 
        if (p2) 
        {
          char charset[kMAX_CSNAME] = "";
          if (kMAX_CSNAME >= (p2 - p1))
            strncpy(charset, p1, p2 - p1);
          rv = mimeConverter->EncodeMimePartIIStr_UTF8(s, PR_FALSE, charset, sizeof("Subject:"), 
                                                       kMIME_ENCODED_WORD_SIZE, modifiedSubject);
          if (NS_SUCCEEDED(rv)) 
            return result;
        }
      }
    }
    else 
      s = *stringP;   // no modification, set the original encoded string
  }


  /* Decrease length by difference between current ptr and original ptr.
	 Then store the current ptr back into the caller. */
  if (lengthP) 
	  *lengthP -= (s - (*stringP));
  *stringP = s;

  return result;
}

/*	Very similar to strdup except it free's too
 */
char * NS_MsgSACopy (char **destination, const char *source)
{
  if(*destination)
  {
    PR_Free(*destination);
    *destination = 0;
  }
  if (! source)
    *destination = nsnull;
  else 
  {
    *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
    if (*destination == nsnull) 
      return(nsnull);
    
    PL_strcpy (*destination, source);
  }
  return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
char * NS_MsgSACat (char **destination, const char *source)
{
  if (source && *source)
    if (*destination)
    {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
      if (*destination == nsnull)
        return(nsnull);

      PL_strcpy (*destination + length, source);
    }
    else
    {
      *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
      if (*destination == nsnull)
        return(nsnull);

      PL_strcpy (*destination, source);
    }
  return *destination;
}

nsresult NS_MsgEscapeEncodeURLPath(const PRUnichar *str, char **result)
{
  NS_ENSURE_ARG_POINTER(str);
  NS_ENSURE_ARG_POINTER(result);

  *result = nsEscape(NS_ConvertUCS2toUTF8(str).get(), url_Path);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

nsresult NS_MsgDecodeUnescapeURLPath(const char *path, PRUnichar **result)
{
  NS_ENSURE_ARG_POINTER(path);
  NS_ENSURE_ARG_POINTER(result);

  char *unescapedName = nsCRT::strdup(path);
  if (!unescapedName) return NS_ERROR_OUT_OF_MEMORY;
  nsUnescape(unescapedName);
  nsAutoString resultStr;
  resultStr = NS_ConvertUTF8toUCS2(unescapedName);
  *result = ToNewUnicode(resultStr);
  if (!*result) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

PRBool WeAreOffline()
{
  nsresult rv = NS_OK;
  PRBool offline = PR_FALSE;

  nsCOMPtr <nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && ioService)
    ioService->GetOffline(&offline);

  return offline;
}

nsresult GetExistingFolder(const char *aFolderURI, nsIMsgFolder **aFolder)
{
  NS_ENSURE_ARG_POINTER(aFolderURI);
  NS_ENSURE_ARG_POINTER(aFolder);

  *aFolder = nsnull;

  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf(do_GetService("@mozilla.org/rdf/rdf-service;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFResource> resource;
  rv = rdf->GetResource(aFolderURI, getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIMsgFolder> thisFolder;
  thisFolder = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Parent doesn't exist means that this folder doesn't exist.
  nsCOMPtr<nsIFolder> parentFolder;
  rv = thisFolder->GetParent(getter_AddRefs(parentFolder));
  if (NS_SUCCEEDED(rv) && parentFolder)
    NS_ADDREF(*aFolder = thisFolder);
  return rv;
}

PRBool IsAFromSpaceLine(char *start, const char *end)
{
  nsresult rv = PR_FALSE;
  while ((start < end) && (*start == '>'))
    start++;
  // If the leading '>'s are followed by an 'F' then we have a possible case here.
  if ( (*start == 'F') && (end-start > 4) && !strncmp(start, "From ", 5) )
    rv = PR_TRUE;
  return rv;
}

//
// This function finds all lines starting with "From " or "From " preceeding
// with one or more '>' (ie, ">From", ">>From", etc) in the input buffer
// (between 'start' and 'end') and prefix them with a ">" .
//
nsresult EscapeFromSpaceLine(nsIFileSpec *pDst, char *start, const char *end)
{
  nsresult rv;
  char *pChar;
  PRInt32 written;

  pChar = start;
  while (start < end)
  {
    while ((pChar < end) && (*pChar != nsCRT::CR) && (*(pChar+1) != nsCRT::LF))
      pChar++;

    if (pChar < end)
    {
      // Found a line so check if it's a qualified "From " line.
      if (IsAFromSpaceLine(start, pChar))
        rv = pDst->Write(">", 1, &written);

      rv = pDst->Write(start, pChar-start+2, &written);
      NS_ENSURE_SUCCESS(rv,rv);
      pChar += 2;
      start = pChar;
    }
    else if (start < end)
    {
      // Check and flush out the remaining data and we're done.
      if (IsAFromSpaceLine(start, end))
        rv = pDst->Write(">", 1, &written);
      rv = pDst->Write(start, end-start, &written);
      NS_ENSURE_SUCCESS(rv,rv);
      break;
    }
  }
  return NS_OK;
}

nsresult CreateServicesForPasswordManager()
{
  if (!gInitPasswordManager) 
  {
     // Initialize the password manager category
    gInitPasswordManager = PR_TRUE;
    return NS_CreateServicesFromCategory(NS_PASSWORDMANAGER_CATEGORY,
                                  nsnull,
                                  NS_PASSWORDMANAGER_CATEGORY);
  }
  return NS_OK;
}
  
nsresult IsRFC822HeaderFieldName(const char *aHdr, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aHdr);
  NS_ENSURE_ARG_POINTER(aResult);
  PRUint32 length = strlen(aHdr);
  for(PRUint32 i=0; i<length; i++)
  {
    char c = aHdr[i];
    if ( c < '!' || c == ':' || c > '~') 
    {
      *aResult = PR_FALSE;
      return NS_OK;
    }
  }
  *aResult = PR_TRUE;
  return NS_OK;
}

nsresult
GetOrCreateFolder(const nsACString &aURI, nsIUrlListener *aListener)
{
  nsresult rv;
  nsCOMPtr <nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // get the corresponding RDF resource
  // RDF will create the folder resource if it doesn't already exist
  nsCOMPtr<nsIRDFResource> resource;
  rv = rdf->GetResource(nsCAutoString(aURI).get(), getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr <nsIMsgFolder> folderResource;
  folderResource = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // don't check validity of folder - caller will handle creating it
  nsCOMPtr<nsIMsgIncomingServer> server; 
  // make sure that folder hierarchy is built so that legitimate parent-child relationship is established
  rv = folderResource->GetServer(getter_AddRefs(server));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!server)
    return NS_ERROR_UNEXPECTED;

  nsCOMPtr <nsIMsgFolder> msgFolder;
  rv = server->GetMsgFolderFromURI(folderResource, nsCAutoString(aURI).get(), getter_AddRefs(msgFolder));
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIFolder> parent;
  rv = msgFolder->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv) || !parent)
  {
    nsCOMPtr <nsIFileSpec> folderPath;
    // for local folders, path is to the berkeley mailbox. 
    // for imap folders, path needs to have .msf appended to the name
    msgFolder->GetPath(getter_AddRefs(folderPath));

    nsXPIDLCString type;
    rv = server->GetType(getter_Copies(type));
    NS_ENSURE_SUCCESS(rv,rv);

    PRBool isImapFolder = type.Equals("imap");
    // if we can't get the path from the folder, then try to create the storage.
    // for imap, it doesn't matter if the .msf file exists - it still might not
    // exist on the server, so we should try to create it
    PRBool exists = PR_FALSE;
    if (!isImapFolder && folderPath)
      folderPath->Exists(&exists);
    if (!exists)
    {
      rv = msgFolder->CreateStorageIfMissing(aListener);
      NS_ENSURE_SUCCESS(rv,rv);

      // XXX TODO
      // JUNK MAIL RELATED
      // ugh, I hate this hack
      // we have to do this (for now)
      // because imap and local are different (one creates folder asynch, the other synch)
      // one will notify the listener, one will not.
      // I blame nsMsgCopy.  
      // we should look into making it so no matter what the folder type
      // we always call the listener
      // this code should move into local folder's version of CreateStorageIfMissing()
      if (!isImapFolder && aListener) {
        rv = aListener->OnStartRunningUrl(nsnull);
        NS_ENSURE_SUCCESS(rv,rv);
        
        rv = aListener->OnStopRunningUrl(nsnull, NS_OK);
        NS_ENSURE_SUCCESS(rv,rv);
      }
    }
  }
  return NS_OK;
}

