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
#include "nsIMsgHdr.h"
#include "nsMsgUtils.h"
#include "nsString.h"
#include "nsFileSpec.h"
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
#include "xp_str.h"

static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);

#if defined(DEBUG_sspitzer_) || defined(DEBUG_seth_)
#define DEBUG_NS_MsgHashIfNecessary 1
#endif

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
  const PRUint32 MAX_LEN = 25;
#elif defined(XP_UNIX) || defined(XP_PC) || defined(XP_BEOS)
  const PRUint32 MAX_LEN = 55;
#else
#error need_to_define_your_max_filename_length
#endif
  nsCAutoString str(name);

#ifdef DEBUG_NS_MsgHashIfNecessary
  printf("in: %s\n",str.get());
#endif

  // Given a name, use either that name, if it fits on our
  // filesystem, or a hashified version of it, if the name is too
  // long to fit.
  char hashedname[MAX_LEN + 1];
  PRBool needshash = PL_strlen(str.get()) > MAX_LEN;
#if defined(XP_WIN16)  || defined(XP_OS2)
  if (!needshash) {
    needshash = PL_strchr(str.get(), '.') != nsnull ||
      PL_strchr(str.get(), ':') != nsnull;
  }
#endif
  PL_strncpy(hashedname, str.get(), MAX_LEN + 1);
  if (needshash) {
    PR_snprintf(hashedname + MAX_LEN - 8, 9, "%08lx",
                (unsigned long) StringHash(str.get()));
  }
  name = hashedname;
#ifdef DEBUG_NS_MsgHashIfNecessary
  printf("out: %s\n",hashedname);
#endif
  
  return NS_OK;
}

nsresult NS_MsgCreatePathStringFromFolderURI(const char *folderURI, nsCString& pathString)
{
	// A file name has to be in native charset, convert from UTF-8.
	nsCAutoString oldPath;
	const nsString fileCharset(nsMsgI18NFileSystemCharset());
	char *nativeString;
	nsresult rv = ConvertFromUnicode(fileCharset, nsAutoString(NS_ConvertUTF8toUCS2(folderURI)), &nativeString);
	if (NS_SUCCEEDED(rv))
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

  while (s < s_end && XP_IS_SPACE(*s))
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
		  while (s2 < (s_end-2) && XP_IS_DIGIT(*s2))
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
