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

static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);

nsresult GetMessageServiceContractIDForURI(const char *uri, nsString &contractID)
{

	nsresult rv = NS_OK;
	//Find protocol
	nsAutoString uriStr; uriStr.AssignWithConversion(uri);
	PRInt32 pos = uriStr.FindChar(':');
	if(pos == -1)
		return NS_ERROR_FAILURE;
	nsString protocol;
	uriStr.Left(protocol, pos);

	//Build message service contractid
	contractID.AssignWithConversion("@mozilla.org/messenger/messageservice;1?type=");
	contractID += protocol;

	return rv;
}

nsresult GetMessageServiceFromURI(const char *uri, nsIMsgMessageService **messageService)
{

	nsAutoString contractID;
	nsresult rv;

	rv = GetMessageServiceContractIDForURI(uri, contractID);

	if(NS_SUCCEEDED(rv))
	{
    nsCAutoString contractidC;
    contractidC.AssignWithConversion(contractID);
		rv = nsServiceManager::GetService((const char *) contractidC, NS_GET_IID(nsIMsgMessageService),
		           (nsISupports**)messageService, nsnull);
	}

	return rv;
}


nsresult ReleaseMessageServiceFromURI(const char *uri, nsIMsgMessageService *messageService)
{
	nsAutoString contractID;
	nsresult rv;

	rv = GetMessageServiceContractIDForURI(uri, contractID);
	if(NS_SUCCEEDED(rv))
  {
    nsCAutoString contractidC;
    contractidC.AssignWithConversion(contractID);
		rv = nsServiceManager::ReleaseService(contractidC, messageService);
  }
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
    if (PL_strncasecmp(uri, "imap", 4) == 0)
    {
        nsCOMPtr<nsIImapUrl> imapUrl;
        rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
                                                NS_GET_IID(nsIImapUrl),
                                                getter_AddRefs(imapUrl));
        if (NS_SUCCEEDED(rv) && imapUrl)
            rv = imapUrl->QueryInterface(NS_GET_IID(nsIURI),
                                         (void**) aUrl);
    }
    else if (PL_strncasecmp(uri, "mailbox", 7) == 0)
    {
        nsCOMPtr<nsIMailboxUrl> mailboxUrl;
        rv = nsComponentManager::CreateInstance(kCMailboxUrl, nsnull,
                                                NS_GET_IID(nsIMailboxUrl),
                                                getter_AddRefs(mailboxUrl));
        if (NS_SUCCEEDED(rv) && mailboxUrl)
            rv = mailboxUrl->QueryInterface(NS_GET_IID(nsIURI),
                                            (void**) aUrl);
    }
    else if (PL_strncasecmp(uri, "news", 4) == 0)
    {
        nsCOMPtr<nsINntpUrl> nntpUrl;
        rv = nsComponentManager::CreateInstance(kCNntpUrlCID, nsnull,
                                                NS_GET_IID(nsINntpUrl),
                                                getter_AddRefs(nntpUrl));
        if (NS_SUCCEEDED(rv) && nntpUrl)
            rv = nntpUrl->QueryInterface(NS_GET_IID(nsIURI),
                                         (void**) aUrl);
    }
    if (*aUrl)
        (*aUrl)->SetSpec(uri);
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
		outName->AssignWithConversion("None");
		break;
	case nsMsgPriority::lowest:
		outName->AssignWithConversion("Lowest");
		break;
	case nsMsgPriority::low:
		outName->AssignWithConversion("Low");
		break;
	case nsMsgPriority::normal:
		outName->AssignWithConversion("Normal");
		break;
	case nsMsgPriority::high:
		outName->AssignWithConversion("High");
		break;
	case nsMsgPriority::highest:
		outName->AssignWithConversion("Highest");
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
  nsCAutoString illegalChars(":");
  const PRUint32 MAX_LEN = 25;
#elif defined(XP_UNIX) || defined(XP_BEOS)
  nsCAutoString illegalChars;  // is this correct for BEOS?
  const PRUint32 MAX_LEN = 55;
#elif defined(XP_WIN32)
  nsCAutoString illegalChars("\"/\\[]:;=,|?<>*$");
  const PRUint32 MAX_LEN = 55;
#elif defined(XP_OS2)
  nsCAutoString illegalChars("\"/\\[]:;=,|?<>*$. ");
  const PRUint32 MAX_LEN = 8;
#else
#error need_to_define_your_max_filename_length
#endif
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
	const nsString fileCharset(nsMsgI18NFileSystemCharset());
	char *nativeString = nsnull;
	nsresult rv = ConvertFromUnicode(fileCharset, nsAutoString(NS_ConvertUTF8toUCS2(folderURI)), &nativeString);
	if (NS_SUCCEEDED(rv) && nativeString && nativeString[0])
		oldPath.Assign(nativeString);
	else
		oldPath.Assign(folderURI);
	PR_FREEIF(nativeString);

	nsCAutoString pathPiece;

	PRInt32 startSlashPos = oldPath.FindChar('/');
	PRInt32 endSlashPos = (startSlashPos >= 0) 
		? oldPath.FindChar('/', PR_FALSE, startSlashPos + 1) - 1 : oldPath.Length() - 1;
	if (endSlashPos == -1)
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
			? oldPath.FindChar('/', PR_FALSE, startSlashPos + 1)  - 1: oldPath.Length() - 1;
	  if (endSlashPos == -1)
			endSlashPos = oldPath.Length();

      if (startSlashPos == endSlashPos)
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
PRBool NS_MsgStripRE(const char **stringP, PRUint32 *lengthP)
{
  const char *s, *s_end;
  const char *last;
  PRUint32 L;
  PRBool result = PR_FALSE;
  NS_ASSERTION(stringP, "bad null param");
  if (!stringP) return PR_FALSE;
  s = *stringP;
  L = lengthP ? *lengthP : nsCRT::strlen(s);

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
  *result = resultStr.ToNewUnicode();
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
