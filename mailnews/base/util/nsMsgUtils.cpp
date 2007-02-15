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
#include "nsNativeCharsetUtils.h"
#include "nsCharTraits.h"
#include "prprf.h"
#include "nsNetCID.h"
#include "nsIIOService.h"
#include "nsIRDFService.h"
#include "nsIMimeConverter.h"
#include "nsMsgMimeCID.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsISupportsPrimitives.h"
#include "nsIPrefLocalizedString.h"
#include "nsIRelativeFilePref.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsISpamSettings.h"
#include "nsISignatureVerifier.h"
#include "nsICryptoHash.h"
#include "nsNativeCharsetUtils.h"
#include "nsIRssIncomingServer.h"
#include "nsIMsgFolder.h"
#include "nsIMsgMessageService.h"
#include "nsIMsgAccountManager.h"
#include "nsIOutputStream.h"

static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCNntpUrlCID, NS_NNTPURL_CID);

#define ILLEGAL_FOLDER_CHARS ";#"
#define ILLEGAL_FOLDER_CHARS_AS_FIRST_LETTER "." 
#define ILLEGAL_FOLDER_CHARS_AS_LAST_LETTER  ".~"

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

  if (protocol.Equals("file") && uriStr.Find("application/x-message-display") != kNotFound)
    protocol.Assign("mailbox");
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


// Where should this live? It's a utility used to convert a string priority,
//  e.g., "High, Low, Normal" to an enum.
// Perhaps we should have an interface that groups together all these
//  utilities...
nsresult NS_MsgGetPriorityFromString(
           const char * const priority,
           nsMsgPriorityValue & outPriority)
{
  if (!priority)
    return NS_ERROR_NULL_POINTER;

  // Note: Checking the values separately and _before_ the names,
  //        hoping for a much faster match;
  //       Only _drawback_, as "priority" handling is not truly specified:
  //        some softwares may have the number meanings reversed (1=Lowest) !?
  if (PL_strchr(priority, '1'))
    outPriority = nsMsgPriority::highest;
  else if (PL_strchr(priority, '2'))
    outPriority = nsMsgPriority::high;
  else if (PL_strchr(priority, '3'))
    outPriority = nsMsgPriority::normal;
  else if (PL_strchr(priority, '4'))
    outPriority = nsMsgPriority::low;
  else if (PL_strchr(priority, '5'))
    outPriority = nsMsgPriority::lowest;
  else if (PL_strcasestr(priority, "Highest"))
    outPriority = nsMsgPriority::highest;
       // Important: "High" must be tested after "Highest" !
  else if (PL_strcasestr(priority, "High") ||
           PL_strcasestr(priority, "Urgent"))
    outPriority = nsMsgPriority::high;
  else if (PL_strcasestr(priority, "Normal"))
    outPriority = nsMsgPriority::normal;
  else if (PL_strcasestr(priority, "Lowest"))
    outPriority = nsMsgPriority::lowest;
       // Important: "Low" must be tested after "Lowest" !
  else if (PL_strcasestr(priority, "Low") ||
           PL_strcasestr(priority, "Non-urgent"))
    outPriority = nsMsgPriority::low;
  else
    // "Default" case gets default value.
    outPriority = nsMsgPriority::Default;

  return NS_OK;
}

nsresult NS_MsgGetPriorityValueString(
           const nsMsgPriorityValue p,
           nsACString & outValueString)
{
  switch (p)
  {
    case nsMsgPriority::highest:
      outValueString.AssignLiteral("1");
      break;
    case nsMsgPriority::high:
      outValueString.AssignLiteral("2");
      break;
    case nsMsgPriority::normal:
      outValueString.AssignLiteral("3");
      break;
    case nsMsgPriority::low:
      outValueString.AssignLiteral("4");
      break;
    case nsMsgPriority::lowest:
      outValueString.AssignLiteral("5");
      break;
    case nsMsgPriority::none:
    case nsMsgPriority::notSet:
      // Note: '0' is a "fake" value; we expect to never be in this case.
      outValueString.AssignLiteral("0");
      break;
    default:
      NS_ASSERTION(PR_FALSE, "invalid priority value");
  }

  return NS_OK;
}

nsresult NS_MsgGetUntranslatedPriorityName(
           const nsMsgPriorityValue p,
           nsACString & outName)
{
  switch (p)
  {
    case nsMsgPriority::highest:
      outName.AssignLiteral("Highest");
      break;
    case nsMsgPriority::high:
      outName.AssignLiteral("High");
      break;
    case nsMsgPriority::normal:
      outName.AssignLiteral("Normal");
      break;
    case nsMsgPriority::low:
      outName.AssignLiteral("Low");
      break;
    case nsMsgPriority::lowest:
      outName.AssignLiteral("Lowest");
      break;
    case nsMsgPriority::none:
    case nsMsgPriority::notSet:
      // Note: 'None' is a "fake" value; we expect to never be in this case.
      outName.AssignLiteral("None");
      break;
    default:
      NS_ASSERTION(PR_FALSE, "invalid priority value");
  }

  return NS_OK;
}


/* this used to be XP_StringHash2 from xp_hash.c */
/* phong's linear congruential hash  */
static PRUint32 StringHash(const char *ubuf, PRInt32 len = -1)
{
  unsigned char * buf = (unsigned char*) ubuf;
  PRUint32 h=1;
  unsigned char *end = buf + (len == -1 ? strlen(ubuf) : len); 
  while(buf < end) {
    h = 0x63c63cd9*h + 0x9c39c33d + (int32)*buf;
    buf++;
  }
  return h;
}

inline PRUint32 StringHash(const nsAutoString& str)
{
    return StringHash(NS_REINTERPRET_CAST(const char*, str.get()),
                      str.Length() * 2);
}

// XXX : this may have other clients, in which case we'd better move it to
//       xpcom/io/nsNativeCharsetUtils with nsAString in place of nsAutoString
static PRBool ConvertibleToNative(const nsAutoString& str)
{
    nsCAutoString native;
    nsAutoString roundTripped;
    NS_CopyUnicodeToNative(str, native);
    NS_CopyNativeToUnicode(native, roundTripped);
    return str.Equals(roundTripped);
}

#if defined(XP_UNIX) || defined(XP_BEOS)
  const static PRUint32 MAX_LEN = 55;
#elif defined(XP_WIN32)
  const static PRUint32 MAX_LEN = 55;
#elif defined(XP_OS2)
  const static PRUint32 MAX_LEN = 55;
#else
  #error need_to_define_your_max_filename_length
#endif

nsresult NS_MsgHashIfNecessary(nsCAutoString &name)
{
  NS_NAMED_LITERAL_CSTRING (illegalChars, 
                            FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS ILLEGAL_FOLDER_CHARS);
  nsCAutoString str(name);

  // Given a filename, make it safe for filesystem
  // certain filenames require hashing because they 
  // are too long or contain illegal characters
  PRInt32 illegalCharacterIndex = str.FindCharInSet(illegalChars);

  // Need to check the first ('.') and last ('.' and '~') char
  if (illegalCharacterIndex == kNotFound) 
  {
	NS_NAMED_LITERAL_CSTRING (illegalFirstChars, ILLEGAL_FOLDER_CHARS_AS_FIRST_LETTER);
	NS_NAMED_LITERAL_CSTRING (illegalLastChars, ILLEGAL_FOLDER_CHARS_AS_LAST_LETTER);
	  
    PRInt32 lastIndex = str.Length() - 1;
    if(str.FindCharInSet(illegalFirstChars) == 0)
	  illegalCharacterIndex = 0;
	else if(str.RFindCharInSet(illegalLastChars) == lastIndex)
	  illegalCharacterIndex = lastIndex;
	else
	  illegalCharacterIndex = -1;
  }

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

// XXX : The number of UTF-16 2byte code units are half the number of
// bytes in legacy encodings for CJK strings and non-Latin1 in UTF-8.
// The ratio can be 1/3 for CJK strings in UTF-8. However, we can
// get away with using the same MAX_LEN for nsCString and nsString
// because MAX_LEN is defined rather conservatively in the first place.
nsresult NS_MsgHashIfNecessary(nsAutoString &name)
{
  PRInt32 illegalCharacterIndex = name.FindCharInSet(
                                  FILE_PATH_SEPARATOR FILE_ILLEGAL_CHARACTERS ILLEGAL_FOLDER_CHARS);

  // Need to check the first ('.') and last ('.' and '~') char
  if (illegalCharacterIndex == kNotFound) 
  {
	NS_NAMED_LITERAL_STRING (illegalFirstChars, ILLEGAL_FOLDER_CHARS_AS_FIRST_LETTER);
	NS_NAMED_LITERAL_STRING (illegalLastChars, ILLEGAL_FOLDER_CHARS_AS_LAST_LETTER);
	  
    PRInt32 lastIndex = name.Length() - 1;
    if(name.FindCharInSet(illegalFirstChars) == 0)
	  illegalCharacterIndex = 0;
	else if(name.RFindCharInSet(illegalLastChars) == lastIndex)
	  illegalCharacterIndex = lastIndex;
	else
	  illegalCharacterIndex = -1;
  }

  char hashedname[9];
  PRInt32 keptLength = -1;
  if (illegalCharacterIndex != kNotFound) 
      keptLength = illegalCharacterIndex;
  else if (!ConvertibleToNative(name))
      keptLength = 0;
  else if (name.Length() > MAX_LEN) 
  {
    keptLength = MAX_LEN-8; 
    // To avoid keeping only the high surrogate of a surrogate pair
    if (NS_IS_HIGH_SURROGATE(name.CharAt(keptLength-1)))
        --keptLength;
  }

  if (keptLength >= 0) {
    PR_snprintf(hashedname, 9, "%08lx", (unsigned long) StringHash(name));
    name.Truncate(keptLength);
    AppendASCIItoUTF16(hashedname, name);
  }

  return NS_OK;
}


nsresult NS_MsgCreatePathStringFromFolderURI(const char *aFolderURI,
                                             nsCString& aPathCString,
                                             PRBool aIsNewsFolder)
{
  // A file name has to be in native charset. Here we convert 
  // to UTF-16 and check for 'unsafe' characters before converting 
  // to native charset.
  NS_ENSURE_TRUE(IsUTF8(nsDependentCString(aFolderURI)), NS_ERROR_UNEXPECTED); 
  NS_ConvertUTF8toUTF16 oldPath(aFolderURI);

  nsAutoString pathPiece, path;

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
    if (!pathPiece.IsEmpty())
    {

      // add .sbd onto the previous path
      if (haveFirst)
      {
        path.AppendLiteral(".sbd/");
      }

      if (aIsNewsFolder)
      {
          nsCAutoString tmp;
          CopyUTF16toMUTF7(pathPiece, tmp); 
          CopyASCIItoUTF16(tmp, pathPiece);
      }
        
      NS_MsgHashIfNecessary(pathPiece);
      path += pathPiece;
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

  return NS_CopyUnicodeToNative(path, aPathCString);
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
    if (*tokPtr)
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

nsresult NS_MsgEscapeEncodeURLPath(const nsAString& aStr, nsAFlatCString& aResult)
{
  char *escapedString = nsEscape(NS_ConvertUTF16toUTF8(aStr).get(), url_Path); 
  if (!*escapedString)
    return NS_ERROR_OUT_OF_MEMORY;
  aResult.Adopt(escapedString);
  return NS_OK;
}

nsresult NS_MsgDecodeUnescapeURLPath(const nsACString& aPath,
                                     nsAString& aResult)
{
  nsCAutoString unescapedName;
  NS_UnescapeURL(PromiseFlatCString(aPath), 
                 esc_FileBaseName|esc_Forced|esc_AlwaysCopy,
                 unescapedName);
  CopyUTF8toUTF16(unescapedName, aResult);
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
nsresult EscapeFromSpaceLine(nsIOutputStream *outputStream, char *start, const char *end)
{
  nsresult rv;
  char *pChar;
  PRUint32 written;

  pChar = start;
  while (start < end)
  {
    while ((pChar < end) && (*pChar != nsCRT::CR) && (*(pChar+1) != nsCRT::LF))
      pChar++;

    if (pChar < end)
    {
      // Found a line so check if it's a qualified "From " line.
      if (IsAFromSpaceLine(start, pChar))
        rv = outputStream->Write(">", 1, &written);
      PRInt32 lineTerminatorCount = (*(pChar + 1) == nsCRT::LF) ? 2 : 1;
      rv = outputStream->Write(start, pChar - start + lineTerminatorCount, &written);
      NS_ENSURE_SUCCESS(rv,rv);
      pChar += lineTerminatorCount;
      start = pChar;
    }
    else if (start < end)
    {
      // Check and flush out the remaining data and we're done.
      if (IsAFromSpaceLine(start, end))
        rv = outputStream->Write(">", 1, &written);
      rv = outputStream->Write(start, end-start, &written);
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

// Warning, currently this routine only works for the Junk Folder
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
      // Hack to work around a localization bug with the Junk Folder.
      // Please see Bug #270261 for more information...
      nsXPIDLString localizedJunkName; 
      msgFolder->GetName(getter_Copies(localizedJunkName));

      // force the junk folder name to be Junk so it gets created on disk correctly...
      msgFolder->SetName(NS_LITERAL_STRING("Junk").get());

      rv = msgFolder->CreateStorageIfMissing(aListener);
      NS_ENSURE_SUCCESS(rv,rv);

      // now restore the localized folder name...
      msgFolder->SetName(localizedJunkName.get());

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

nsresult IsRSSArticle(nsIURI * aMsgURI, PRBool *aIsRSSArticle)
{
  nsresult rv;
  *aIsRSSArticle = PR_FALSE;

  nsCOMPtr<nsIMsgMessageUrl> msgUrl = do_QueryInterface(aMsgURI, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString resourceURI;
  msgUrl->GetUri(getter_Copies(resourceURI));
  
  // get the msg service for this URI
  nsCOMPtr<nsIMsgMessageService> msgService;
  rv = GetMessageServiceFromURI(resourceURI.get(), getter_AddRefs(msgService));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIMsgDBHdr> msgHdr;
  rv = msgService->MessageURIToMsgHdr(resourceURI, getter_AddRefs(msgHdr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMsgMailNewsUrl> mailnewsUrl = do_QueryInterface(aMsgURI, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // get the folder and the server from the msghdr
  nsCOMPtr<nsIRssIncomingServer> rssServer;
  nsCOMPtr<nsIMsgFolder> folder;
  rv = msgHdr->GetFolder(getter_AddRefs(folder));
  if (NS_SUCCEEDED(rv) && folder)
  {
    nsCOMPtr<nsIMsgIncomingServer> server;
    folder->GetServer(getter_AddRefs(server));
    rssServer = do_QueryInterface(server);

    if (rssServer)
      *aIsRSSArticle = PR_TRUE;
  }

  return rv;
}


// digest needs to be a pointer to a DIGEST_LENGTH (16) byte buffer
nsresult MSGCramMD5(const char *text, PRInt32 text_len, const char *key, PRInt32 key_len, unsigned char *digest)
{
  nsresult rv;

  nsCAutoString hash;
  nsCOMPtr<nsICryptoHash> hasher = do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);


  // this code adapted from http://www.cis.ohio-state.edu/cgi-bin/rfc/rfc2104.html

  char innerPad[65];    /* inner padding - key XORd with innerPad */
  char outerPad[65];    /* outer padding - key XORd with outerPad */
  int i;
  /* if key is longer than 64 bytes reset it to key=MD5(key) */
  if (key_len > 64) 
  {

    rv = hasher->Init(nsICryptoHash::MD5);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Update((const PRUint8*) key, key_len);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Finish(PR_FALSE, hash);
    NS_ENSURE_SUCCESS(rv, rv);

    key = hash.get();
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
  nsCAutoString result;
  rv = hasher->Init(nsICryptoHash::MD5); /* init context for 1st pass */
  rv = hasher->Update((const PRUint8*)innerPad, 64);       /* start with inner pad */
  rv = hasher->Update((const PRUint8*)text, text_len);     /* then text of datagram */
  rv = hasher->Finish(PR_FALSE, result);   /* finish up 1st pass */

  /*
   * perform outer MD5
   */
  hasher->Init(nsICryptoHash::MD5);       /* init context for 2nd pass */
  rv = hasher->Update((const PRUint8*)outerPad, 64);    /* start with outer pad */
  rv = hasher->Update((const PRUint8*)result.get(), 16);/* then results of 1st hash */
  rv = hasher->Finish(PR_FALSE, result);    /* finish up 2nd pass */

  if (result.Length() != DIGEST_LENGTH)
    return NS_ERROR_UNEXPECTED;

  memcpy(digest, result.get(), DIGEST_LENGTH);

  return rv;

}


// digest needs to be a pointer to a DIGEST_LENGTH (16) byte buffer
nsresult MSGApopMD5(const char *text, PRInt32 text_len, const char *password, PRInt32 password_len, unsigned char *digest)
{
  nsresult rv;
  nsCAutoString result;

  nsCOMPtr<nsICryptoHash> hasher = do_CreateInstance("@mozilla.org/security/hash;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = hasher->Init(nsICryptoHash::MD5);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = hasher->Update((const PRUint8*) text, text_len);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = hasher->Update((const PRUint8*) password, password_len);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = hasher->Finish(PR_FALSE, result);
  NS_ENSURE_SUCCESS(rv, rv);

  if (result.Length() != DIGEST_LENGTH)
    return NS_ERROR_UNEXPECTED;

  memcpy(digest, result.get(), DIGEST_LENGTH);
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

NS_MSG_BASE nsresult NS_GetUnicharPreferenceWithDefault(nsIPrefBranch *prefBranch,  //can be null, if so uses the root branch
                                                        const char *prefName,
                                                        const nsString& defValue,
                                                        nsString& prefValue)
{
    NS_ENSURE_ARG(prefName);

    nsCOMPtr<nsIPrefBranch> pbr;
    if(!prefBranch) {
        pbr = do_GetService(NS_PREFSERVICE_CONTRACTID);
        prefBranch = pbr;
    }
    nsCOMPtr<nsISupportsString> str;

    nsresult rv = prefBranch->GetComplexValue(prefName, NS_GET_IID(nsISupportsString), getter_AddRefs(str));
    if (NS_SUCCEEDED(rv))
        return str->GetData(prefValue);

    prefValue = defValue;
    return NS_OK;
}
 
NS_MSG_BASE nsresult NS_GetLocalizedUnicharPreferenceWithDefault(nsIPrefBranch *prefBranch,  //can be null, if so uses the root branch
                                                                 const char *prefName,
                                                                 const nsString& defValue,
                                                                 nsXPIDLString& prefValue)
{
    NS_ENSURE_ARG(prefName);

    nsCOMPtr<nsIPrefBranch> pbr;
    if(!prefBranch) {
        pbr = do_GetService(NS_PREFSERVICE_CONTRACTID);
        prefBranch = pbr;
    }

    nsCOMPtr<nsIPrefLocalizedString> str;

    nsresult rv = prefBranch->GetComplexValue(prefName, NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(str));
    if (NS_SUCCEEDED(rv))
        str->ToString(getter_Copies(prefValue));
    else
        prefValue = defValue;
    return NS_OK;
}

void PRTime2Seconds(PRTime prTime, PRUint32 *seconds)
{
  PRInt64 microSecondsPerSecond, intermediateResult;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
  LL_L2UI((*seconds), intermediateResult);
}

void PRTime2Seconds(PRTime prTime, PRInt32 *seconds)
{
  PRInt64 microSecondsPerSecond, intermediateResult;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
  LL_L2I((*seconds), intermediateResult);
}

void Seconds2PRTime(PRUint32 seconds, PRTime *prTime)
{
  PRInt64 microSecondsPerSecond, intermediateResult;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_UI2L(intermediateResult, seconds);
  LL_MUL((*prTime), intermediateResult, microSecondsPerSecond);
}

nsresult GetSummaryFileLocation(nsIFile* fileLocation, nsIFile** summaryLocation)
{
  nsIFile* newSummaryLocation;

  nsresult rv = fileLocation->Clone(&newSummaryLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString fileName;

  rv = newSummaryLocation->GetNativeLeafName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);

  fileName.Append(NS_LITERAL_CSTRING(SUMMARY_SUFFIX));

  rv = newSummaryLocation->SetNativeLeafName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);

  *summaryLocation = newSummaryLocation;
  return NS_OK;
}

nsresult GetSummaryFileLocation(nsIFileSpec* fileLocation, nsIFileSpec** summaryLocation)
{
  nsIFileSpec* newSummaryLocation;
  nsresult rv = NS_NewFileSpec(&newSummaryLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = newSummaryLocation->FromFileSpec(fileLocation);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString fileName;

  rv = newSummaryLocation->GetLeafName(getter_Copies(fileName));
  NS_ENSURE_SUCCESS(rv, rv);

  fileName.Append(NS_LITERAL_CSTRING(SUMMARY_SUFFIX));
  
  rv = newSummaryLocation->SetLeafName(fileName);
  NS_ENSURE_SUCCESS(rv, rv);

  *summaryLocation = newSummaryLocation;
  return NS_OK;
}

nsresult GetSummaryFileLocation(nsIFileSpec* fileLocation, nsFileSpec* summaryLocation)
{
  nsCOMPtr<nsIFileSpec> summaryIFile;
  nsresult rv = GetSummaryFileLocation(fileLocation, getter_AddRefs(summaryIFile));
  NS_ENSURE_SUCCESS(rv, rv);

  return summaryIFile->GetFileSpec(summaryLocation);
}

void MsgGenerateNowStr(nsACString &nowStr)
{
  char dateBuf[100];
  dateBuf[0] = '\0';
  PRExplodedTime exploded;
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &exploded);
  PR_FormatTimeUSEnglish(dateBuf, sizeof(dateBuf), "%a %b %d %H:%M:%S %Y", &exploded);
  nowStr.Assign(dateBuf);
}

void GetSummaryFileLocation(nsFileSpec& fileLocation, nsFileSpec* summaryLocation)
{
  nsXPIDLCString fileName;

  // First copy all the details across
  *summaryLocation = fileLocation;

  // Now work out the new file name.
  fileName.Adopt(fileLocation.GetLeafName());

  fileName.Append(NS_LITERAL_CSTRING(SUMMARY_SUFFIX));
  
  summaryLocation->SetLeafName(fileName);
}

// Gets a special directory and appends the supplied file name onto it.
nsresult GetSpecialDirectoryWithFileName(const char* specialDirName,
                                         const char* fileName,
                                         nsIFile** result)
{
  nsresult rv = NS_GetSpecialDirectory(specialDirName, result);
  NS_ENSURE_SUCCESS(rv, rv);

  return (*result)->AppendNative(nsDependentCString(fileName));
}

// XXX This function is provided temporarily whilst we are still working
// on bug 33451 to remove nsIFileSpec from mailnews.
nsresult GetSpecialDirectoryWithFileName(const char* specialDirName,
                                         const char* fileName,
                                         nsIFileSpec** result)
{
  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = GetSpecialDirectoryWithFileName(specialDirName, fileName,
                                                getter_AddRefs(tmpFile));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewFileSpecFromIFile(tmpFile, result);
}

// XXX This function is provided temporarily whilst we are still working
// on bug 33451 to remove nsIFileSpec from mailnews.
nsresult GetSpecialDirectoryWithFileName(const char* specialDirName,
                                         const char* fileName,
                                         nsFileSpec* result)
{


  nsCOMPtr<nsIFileSpec> tmpFile;
  nsresult rv = GetSpecialDirectoryWithFileName(specialDirName, fileName,
                                                getter_AddRefs(tmpFile));
  NS_ENSURE_SUCCESS(rv, rv);

  return tmpFile->GetFileSpec(result);
}

PRBool MsgFindKeyword(const nsACString &keyword, nsACString &keywords, nsACString::const_iterator &start, nsACString::const_iterator &end)
{
  keywords.BeginReading(start);
  keywords.EndReading(end);
  if (*start == ' ')
    start++;
  nsACString::const_iterator saveStart(start), saveEnd(end);
  while (PR_TRUE)
  {
    if (FindInReadable(keyword, start, end))
    {
      PRBool beginMatches = start == saveStart;
      PRBool endMatches = end == saveEnd;
      nsACString::const_iterator beforeStart(start);
      beforeStart--;
      // start and end point to the beginning and end of the match
      if (beginMatches && (end == saveEnd || *end == ' ')
        || (endMatches && *beforeStart == ' ')
        || *beforeStart == ' ' && *end == ' ')
      {
        if (*end == ' ')
          end++;
        if (*beforeStart == ' ' && endMatches)
          start--;
        return PR_TRUE;
      }
      else 
        start = end; // advance past bogus match.
    }
    else
      break;
  }
  return PR_FALSE;
}

PRBool MsgHostDomainIsTrusted(nsCString &host, nsCString &trustedMailDomains)
{
  const char *domain, *domainEnd, *end;
  PRUint32 hostLen, domainLen;
  PRBool domainIsTrusted = PR_FALSE;
  
  domain = trustedMailDomains.BeginReading();
  domainEnd = trustedMailDomains.EndReading(); 
  nsACString::const_iterator hostStart;
  
  host.BeginReading(hostStart);
  hostLen = host.Length();
  
  do {
    // skip any whitespace
    while (*domain == ' ' || *domain == '\t')
      ++domain;
    
    // find end of this domain in the string
    end = strchr(domain, ',');
    if (!end)
      end = domainEnd;
    
    // to see if the hostname is in the domain, check if the domain
    // matches the end of the hostname.
    domainLen = end - domain;
    if (domainLen && hostLen >= domainLen) {
      const char *hostTail = hostStart.get() + hostLen - domainLen;
      if (PL_strncasecmp(domain, hostTail, domainLen) == 0) 
      {
        // now, make sure either that the hostname is a direct match or
        // that the hostname begins with a dot.
        if (hostLen == domainLen || *hostTail == '.' || *(hostTail - 1) == '.')
        {
          domainIsTrusted = PR_TRUE;
          break;
        }
      }
    }
    
    domain = end + 1;
  } while (*end);
  return domainIsTrusted;
}

nsresult MsgMailboxGetURI(const char *nativepath, nsCString &mailboxUri)
{
  
  nsresult rv;
  
  nsCOMPtr<nsIMsgAccountManager> accountManager = 
    do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
  
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISupportsArray> serverArray;
  accountManager->GetAllServers(getter_AddRefs(serverArray));
  
  // do a char*->fileSpec->char* conversion to normalize the path
  nsFilePath filePath(nativepath);
  
  PRUint32 cnt;
  rv = serverArray->Count(&cnt);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 count = cnt;
  PRInt32 i;
  for (i=0; i<count; i++) 
  {
    
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryElementAt(serverArray, i);
    
    if (!server) continue;
    
    // get the path string, convert it to an nsFilePath
    nsCOMPtr<nsIFileSpec> nativeServerPath;
    rv = server->GetLocalPath(getter_AddRefs(nativeServerPath));
    if (NS_FAILED(rv)) continue;
    
    nsFileSpec spec;
    nativeServerPath->GetFileSpec(&spec);
    nsFilePath serverPath(spec);
    
    // check if filepath begins with serverPath
    PRInt32 len = PL_strlen(serverPath);
    if (PL_strncasecmp(serverPath, filePath, len) == 0) 
    {
      nsXPIDLCString serverURI;
      rv = server->GetServerURI(getter_Copies(serverURI));
      if (NS_FAILED(rv)) continue;
      
      // the relpath is just past the serverpath
      const char *relpath = nativepath + len;
      // skip past leading / if any
      while (*relpath == '/')
        relpath++;
      
      nsCAutoString pathStr(relpath);
      PRInt32 sbdIndex;
      while((sbdIndex = pathStr.Find(".sbd", PR_TRUE)) != -1)
        pathStr.Cut(sbdIndex, 4);
      
      mailboxUri = serverURI;
      mailboxUri.Append('/');
      mailboxUri.Append(pathStr);
      break;
    }
  }
  return mailboxUri.IsEmpty() ? NS_ERROR_FAILURE : NS_OK;
}

NS_MSG_BASE void MsgStripQuotedPrintable (unsigned char *src)
{
  // decode quoted printable text in place
  
  if (!*src)
    return;
  unsigned char *dest = src;
  int srcIdx = 0, destIdx = 0;
  
  while (src[srcIdx] != 0)
  {
    if (src[srcIdx] == '=')
    {
      unsigned char *token = &src[srcIdx];
      unsigned char c = 0;
      
      // decode the first quoted char
      if (token[1] >= '0' && token[1] <= '9')
        c = token[1] - '0';
      else if (token[1] >= 'A' && token[1] <= 'F')
        c = token[1] - ('A' - 10);
      else if (token[1] >= 'a' && token[1] <= 'f')
        c = token[1] - ('a' - 10);
      else
      {
        // first char after '=' isn't hex. check if it's a normal char
        // or a soft line break. If it's a soft line break, eat the
        // CR/LF/CRLF.
        if (src[srcIdx + 1] == nsCRT::CR || src[srcIdx + 1] == nsCRT::LF)
        {
          srcIdx++; // soft line break, ignore the '=';
          if (src[srcIdx] == nsCRT::CR || src[srcIdx] == nsCRT::LF)
          {
            srcIdx++;
            if (src[srcIdx] == nsCRT::LF)
              srcIdx++;
          }
        }
        else // normal char, copy it.
        {
          dest[destIdx++] = src[srcIdx++]; // aka token[0]
        }
        continue;
      }
      
      // decode the second quoted char
      c = (c << 4);
      if (token[2] >= '0' && token[2] <= '9')
        c += token[2] - '0';
      else if (token[2] >= 'A' && token[2] <= 'F')
        c += token[2] - ('A' - 10);
      else if (token[2] >= 'a' && token[2] <= 'f')
        c += token[2] - ('a' - 10);
      else
      {
        // second char after '=' isn't hex. copy the '=' as a normal char and keep going
        dest[destIdx++] = src[srcIdx++]; // aka token[0]
        continue;
      }
      
      // if we got here, we successfully decoded a quoted printable sequence,
      // so bump each pointer past it and move on to the next char;
      dest[destIdx++] = c; 
      srcIdx += 3;
      
    }
    else
      dest[destIdx++] = src[srcIdx++];
  }
  
  dest[destIdx] = src[srcIdx]; // null terminate
}  


