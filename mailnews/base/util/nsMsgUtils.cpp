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
#include "nsIPrefService.h"
#include "nsIRelativeFilePref.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsISpamSettings.h"
#include "nsISignatureVerifier.h"

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

nsresult CreateStartupUrl(const char *uri, nsIURI** aUrl)
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
		outName->AssignLiteral("None");
		break;
	case nsMsgPriority::lowest:
		outName->AssignLiteral("Lowest");
		break;
	case nsMsgPriority::low:
		outName->AssignLiteral("Low");
		break;
	case nsMsgPriority::normal:
		outName->AssignLiteral("Normal");
		break;
	case nsMsgPriority::high:
		outName->AssignLiteral("High");
		break;
	case nsMsgPriority::highest:
		outName->AssignLiteral("Highest");
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
  else {
    nsresult rv = nsMsgI18NCopyUTF16ToNative(NS_ConvertUTF8toUTF16(folderURI),
                                             oldPath);
    if (NS_FAILED(rv))
      oldPath.Assign(folderURI);
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
      if (!pathPiece.IsEmpty()) {

        // add .sbd onto the previous path
        if (haveFirst) {
          pathString += ".sbd/";
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

   If mailnews.localizedRe is set, it will also remove localized "Re:" strings.

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

  // get localizedRe pref
  nsresult rv;
  nsXPIDLCString localizedRe;
  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_SUCCEEDED(rv))
    prefBranch->GetCharPref("mailnews.localizedRe", getter_Copies(localizedRe));
    
  // hardcoded "Re" so that noone can configure Mozilla standards incompatible 
  nsCAutoString checkString("Re,RE,re,rE");
  if (!localizedRe.IsEmpty()) 
    checkString.Append(NS_LITERAL_CSTRING(",") + localizedRe);

  // decode the string
  nsXPIDLCString decodedString;
  nsCOMPtr<nsIMimeConverter> mimeConverter;
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

  const char *tokPtr = checkString.get();
  while (*tokPtr)
  {
    //tokenize the comma separated list
    PRSize tokenLength = 0;
    while (*tokPtr && *tokPtr != ',') {
      tokenLength++; 
      tokPtr++;
    }
    //check if the beginning of s is the actual token
    if (tokenLength && !strncmp(s, tokPtr - tokenLength, tokenLength))
    {
      if (s[tokenLength] == ':')
      {
        s = s + tokenLength + 1; /* Skip over "Re:" */
        result = PR_TRUE;        /* Yes, we stripped it. */
        goto AGAIN;              /* Skip whitespace and try again. */
      }
      else if (s[tokenLength] == '[' || s[tokenLength] == '(')
      {
        const char *s2 = s + tokenLength + 1; /* Skip over "Re[" */
        
        /* Skip forward over digits after the "[". */
        while (s2 < (s_end - 2) && IS_DIGIT(*s2))
          s2++;

        /* Now ensure that the following thing is "]:"
           Only if it is do we alter `s'. */
        if ((s2[0] == ']' || s2[0] == ')') && s2[1] == ':')
        {
          s = s2 + 2;       /* Skip over "]:" */
          result = PR_TRUE; /* Yes, we stripped it. */
          goto AGAIN;       /* Skip whitespace and try again. */
        }
      }
    } 
    tokPtr++;
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

nsresult NS_MsgEscapeEncodeURLPath(const nsAString& str, nsAFlatCString& result)
{
  char *escapedString = nsEscape(NS_ConvertUTF16toUTF8(str).get(), url_Path); 
  if (!*escapedString)
    return NS_ERROR_OUT_OF_MEMORY;
  result.Adopt(escapedString);
  return NS_OK;
}

nsresult NS_MsgDecodeUnescapeURLPath(const nsASingleFragmentCString& path, nsAString& result)
{
  nsCAutoString unescapedName;
  NS_UnescapeURL(path, esc_FileBaseName|esc_Forced|esc_AlwaysCopy,
                 unescapedName);
  CopyUTF8toUTF16(unescapedName, result);
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
  rv = rdf->GetResource(nsDependentCString(aFolderURI), getter_AddRefs(resource));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr <nsIMsgFolder> thisFolder;
  thisFolder = do_QueryInterface(resource, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Parent doesn't exist means that this folder doesn't exist.
  nsCOMPtr<nsIMsgFolder> parentFolder;
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
  rv = rdf->GetResource(aURI, getter_AddRefs(resource));
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

  nsCOMPtr <nsIMsgFolder> parent;
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
  else {
    // if the folder exists, we should set the junk flag on it
    // which is what the listener will do
    if (aListener) {
      rv = aListener->OnStartRunningUrl(nsnull);
      NS_ENSURE_SUCCESS(rv,rv);
      
      rv = aListener->OnStopRunningUrl(nsnull, NS_OK);
      NS_ENSURE_SUCCESS(rv,rv);
    }
  }

  return NS_OK;
}

// digest needs to be a pointer to a DIGEST_LENGTH (16) byte buffer
nsresult MSGCramMD5(const char *text, PRInt32 text_len, const char *key, PRInt32 key_len, unsigned char *digest)
{
  nsresult rv;
  unsigned char result[DIGEST_LENGTH];
  unsigned char *presult = result;

  nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);


  // this code adapted from http://www.cis.ohio-state.edu/cgi-bin/rfc/rfc2104.html

  char innerPad[65];    /* inner padding - key XORd with innerPad */
  char outerPad[65];    /* outer padding - key XORd with outerPad */
  int i;
  /* if key is longer than 64 bytes reset it to key=MD5(key) */
  if (key_len > 64) 
  {

    HASHContextStr      *tctx;
    PRUint32 resultLen;

    rv = verifier->HashBegin(nsISignatureVerifier::MD5, &tctx);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = verifier->HashUpdate(tctx, key, key_len);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = verifier->HashEnd(tctx, &presult, &resultLen, DIGEST_LENGTH);
    NS_ENSURE_SUCCESS(rv, rv);

    key = (const char *) result;
    key_len = DIGEST_LENGTH;
  }

  /*
   * the HMAC_MD5 transform looks like:
   *
   * MD5(K XOR outerPad, MD5(K XOR innerPad, text))
   *
   * where K is an n byte key
   * innerPad is the byte 0x36 repeated 64 times
   * outerPad is the byte 0x5c repeated 64 times
   * and text is the data being protected
   */

  /* start out by storing key in pads */
  memset(innerPad, 0, sizeof innerPad);
  memset(outerPad, 0, sizeof outerPad);
  memcpy(innerPad, key,  key_len);
  memcpy(outerPad, key, key_len);

  /* XOR key with innerPad and outerPad values */
  for (i=0; i<64; i++)
  {
    innerPad[i] ^= 0x36;
    outerPad[i] ^= 0x5c;
  }
  /*
   * perform inner MD5
   */
  HASHContextStr      *context;
  PRUint32 resultLen;

  rv = verifier->HashBegin(nsISignatureVerifier::MD5, &context); /* init context for 1st pass */
  rv = verifier->HashUpdate(context, innerPad, 64);      /* start with inner pad */
  rv = verifier->HashUpdate(context, text, text_len); /* then text of datagram */
  rv = verifier->HashEnd(context, &presult, &resultLen, DIGEST_LENGTH);          /* finish up 1st pass */
  /*
   * perform outer MD5
   */
  verifier->HashBegin(nsISignatureVerifier::MD5, &context);  /* init context for 2nd pass */
  rv = verifier->HashUpdate(context, outerPad, 64);     /* start with outer pad */
  rv = verifier->HashUpdate(context, (const char *) result, 16);     /* then results of 1st hash */
  rv = verifier->HashEnd(context, &presult, &resultLen, DIGEST_LENGTH);  /* finish up 2nd pass */
  memcpy(digest, result, DIGEST_LENGTH);
  return rv;

}


// digest needs to be a pointer to a DIGEST_LENGTH (16) byte buffer
nsresult MSGApopMD5(const char *text, PRInt32 text_len, const char *password, PRInt32 password_len, unsigned char *digest)
{
  nsresult rv;
  unsigned char result[DIGEST_LENGTH];
  unsigned char *presult = result;

  nsCOMPtr<nsISignatureVerifier> verifier = do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);


  HASHContextStr      *context;
  PRUint32 resultLen;

  rv = verifier->HashBegin(nsISignatureVerifier::MD5, &context);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = verifier->HashUpdate(context, text, text_len);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = verifier->HashUpdate(context, password, password_len);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = verifier->HashEnd(context, &presult, &resultLen, DIGEST_LENGTH);
  NS_ENSURE_SUCCESS(rv, rv);
  memcpy(digest, result, DIGEST_LENGTH);
  return rv;
}

NS_MSG_BASE nsresult NS_GetPersistentFile(const char *relPrefName,
                                          const char *absPrefName,
                                          const char *dirServiceProp,
                                          PRBool& gotRelPref,
                                          nsILocalFile **aFile)
{
    NS_ENSURE_ARG_POINTER(aFile);
    *aFile = nsnull;
    NS_ENSURE_ARG(relPrefName);
    NS_ENSURE_ARG(absPrefName);
    gotRelPref = PR_FALSE;
    
    nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (!prefService) return NS_ERROR_FAILURE;
    nsCOMPtr<nsIPrefBranch> mainBranch;
    prefService->GetBranch(nsnull, getter_AddRefs(mainBranch));
    if (!mainBranch) return NS_ERROR_FAILURE;

    nsCOMPtr<nsILocalFile> localFile;
    
    // Get the relative first    
    nsCOMPtr<nsIRelativeFilePref> relFilePref;
    mainBranch->GetComplexValue(relPrefName,
                                NS_GET_IID(nsIRelativeFilePref), getter_AddRefs(relFilePref));
    if (relFilePref) {
        relFilePref->GetFile(getter_AddRefs(localFile));
        NS_ASSERTION(localFile, "An nsIRelativeFilePref has no file.");
        if (localFile)
            gotRelPref = PR_TRUE;
    }    
    
    // If not, get the old absolute
    if (!localFile) {
        mainBranch->GetComplexValue(absPrefName,
                                    NS_GET_IID(nsILocalFile), getter_AddRefs(localFile));
                                        
        // If not, and given a dirServiceProp, use directory service.
        if (!localFile && dirServiceProp) {
            nsCOMPtr<nsIProperties> dirService(do_GetService("@mozilla.org/file/directory_service;1"));
            if (!dirService) return NS_ERROR_FAILURE;
            dirService->Get(dirServiceProp, NS_GET_IID(nsILocalFile), getter_AddRefs(localFile));
            if (!localFile) return NS_ERROR_FAILURE;
        }
    }
    
    if (localFile) {
        *aFile = localFile;
        NS_ADDREF(*aFile);
        return NS_OK;
    }
    
    return NS_ERROR_FAILURE;
}

NS_MSG_BASE nsresult NS_SetPersistentFile(const char *relPrefName,
                                          const char *absPrefName,
                                          nsILocalFile *aFile)
{
    NS_ENSURE_ARG(relPrefName);
    NS_ENSURE_ARG(absPrefName);
    NS_ENSURE_ARG(aFile);
    
    nsresult rv;
    nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (!prefService) return NS_ERROR_FAILURE;
    nsCOMPtr<nsIPrefBranch> mainBranch;
    prefService->GetBranch(nsnull, getter_AddRefs(mainBranch));
    if (!mainBranch) return NS_ERROR_FAILURE;

    // Write the absolute for backwards compatibilty's sake.
    // Or, if aPath is on a different drive than the profile dir.
    rv = mainBranch->SetComplexValue(absPrefName, NS_GET_IID(nsILocalFile), aFile);
    
    // Write the relative path.
    nsCOMPtr<nsIRelativeFilePref> relFilePref;
    NS_NewRelativeFilePref(aFile, nsDependentCString(NS_APP_USER_PROFILE_50_DIR), getter_AddRefs(relFilePref));
    if (relFilePref) {
        nsresult rv2 = mainBranch->SetComplexValue(relPrefName, NS_GET_IID(nsIRelativeFilePref), relFilePref);
        if (NS_FAILED(rv2) && NS_SUCCEEDED(rv))
            mainBranch->ClearUserPref(relPrefName);
    }

    return rv;
}

