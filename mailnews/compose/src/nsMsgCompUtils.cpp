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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsMsgCompUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "prmem.h"
#include "nsEscape.h"
#include "nsIFileSpec.h"
#include "nsMsgSend.h"
#include "nsIIOService.h"
#include "nsIHttpProtocolHandler.h"
#include "nsMailHeaders.h"
#include "nsMsgI18N.h"
#include "nsIMsgHeaderParser.h"
#include "nsINntpService.h"
#include "nsMimeTypes.h"
#include "nsMsgComposeStringBundle.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIDocumentEncoder.h"    // for editor output flags
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsMsgPrompts.h"
#include "nsMsgUtils.h"
#include "nsMsgSimulateError.h"

#include "nsIMsgCompUtils.h"
#include "nsIMsgMdnGenerator.h"

#ifdef MOZ_THUNDERBIRD
#include "nsIXULAppInfo.h"
#include "nsXULAppAPI.h"
#endif

NS_IMPL_ISUPPORTS1(nsMsgCompUtils, nsIMsgCompUtils)

nsMsgCompUtils::nsMsgCompUtils()
{
}

nsMsgCompUtils::~nsMsgCompUtils()
{
}

NS_IMETHODIMP nsMsgCompUtils::MimeMakeSeparator(const char *prefix,
                                                char **_retval)
{
  NS_ENSURE_ARG_POINTER(prefix);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mime_make_separator(prefix);
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompUtils::MsgGenerateMessageId(nsIMsgIdentity *identity,
                                                    char **_retval)
{
  NS_ENSURE_ARG_POINTER(identity);
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = msg_generate_message_id(identity);
  return NS_OK;
}

NS_IMETHODIMP nsMsgCompUtils::GetMsgMimeConformToStandard(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsMsgMIMEGetConformToStandard();
  return NS_OK;
}

//
// Hopefully, someone will write and XP call like this eventually!
//
#define     TPATH_LEN   1024

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
//
nsFileSpec * 
nsMsgCreateTempFileSpec(const char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  nsFileSpec *tmpSpec = new nsFileSpec;
  
  if (NS_FAILED(GetSpecialDirectoryWithFileName(NS_OS_TEMP_DIR,
                                                tFileName,
                                                tmpSpec)))
  {
    delete tmpSpec;
    return nsnull;
  }

  tmpSpec->MakeUnique();

  return tmpSpec;
}

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
// returned
//
char * 
nsMsgCreateTempFileName(const char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  nsCOMPtr<nsIFile> tmpFile;

  nsresult rv = GetSpecialDirectoryWithFileName(NS_OS_TEMP_DIR,
                                                tFileName,
                                                getter_AddRefs(tmpFile));
  if (NS_FAILED(rv))
    return nsnull;

  rv = tmpFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 00600);
  if (NS_FAILED(rv))
    return nsnull;

  nsXPIDLCString tempString;
  rv = tmpFile->GetNativePath(tempString);
  if (NS_FAILED(rv))
    return nsnull;
  
  char *tString = (char *)PL_strdup(tempString.get());
  if (!tString)
    return PL_strdup("mozmail.tmp");  // No need to I18N

  return tString;
}

static PRBool mime_headers_use_quoted_printable_p = PR_FALSE;

PRBool
nsMsgMIMEGetConformToStandard (void)
{
  return mime_headers_use_quoted_printable_p;
}

void
nsMsgMIMESetConformToStandard (PRBool conform_p)
{
  /* 
  * If we are conforming to mime standard no matter what we set
  * for the headers preference when generating mime headers we should 
  * also conform to the standard. Otherwise, depends the preference
  * we set. For now, the headers preference is not accessible from UI.
  */
  if (conform_p)
    mime_headers_use_quoted_printable_p = PR_TRUE;
  else {
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
    if (NS_SUCCEEDED(rv)) {
      prefs->GetBoolPref("mail.strictly_mime_headers", &mime_headers_use_quoted_printable_p);
    }
  }
}

nsresult mime_sanity_check_fields (
          const char *from,
          const char *reply_to,
          const char *to,
          const char *cc,
          const char *bcc,
          const char *fcc,
          const char *newsgroups,
          const char *followup_to,
          const char * /*subject*/,
          const char * /*references*/,
          const char * /*organization*/,
          const char * /*other_random_headers*/)
{
  if (from)
    while (IS_SPACE (*from))
      from++;
  if (reply_to)
    while (IS_SPACE (*reply_to))
      reply_to++;
  if (to)
    while (IS_SPACE (*to))
      to++;
  if (cc)
    while (IS_SPACE (*cc))
      cc++;
  if (bcc)
    while (IS_SPACE (*bcc))
      bcc++;
  if (fcc)
    while (IS_SPACE (*fcc))
      fcc++;
  if (newsgroups)
    while (IS_SPACE (*newsgroups))
      newsgroups++;
  if (followup_to)
    while (IS_SPACE (*followup_to))
      followup_to++;

  /* #### sanity check other_random_headers for newline conventions */
  if (!from || !*from || CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_6))
    return NS_MSG_NO_SENDER;
  else
    if (((!to || !*to) && (!cc || !*cc) &&
        (!bcc || !*bcc) && (!newsgroups || !*newsgroups)) ||
        CHECK_SIMULATED_ERROR(SIMULATED_SEND_ERROR_7))
      return NS_MSG_NO_RECIPIENTS;
  else
    return NS_OK;
}

static char *
nsMsgStripLine (char * string)
{
  char * ptr;
  
  /* remove leading blanks */
  while(*string=='\t' || *string==' ' || *string=='\r' || *string=='\n')
    string++;    
  
  for(ptr=string; *ptr; ptr++)
    ;   /* NULL BODY; Find end of string */
  
  /* remove trailing blanks */
  for(ptr--; ptr >= string; ptr--) 
  {
    if(*ptr=='\t' || *ptr==' ' || *ptr=='\r' || *ptr=='\n') 
      *ptr = '\0'; 
    else 
      break;
  }
  
  return string;
}

//
// Generate the message headers for the new RFC822 message
//
#define UA_PREF_PREFIX "general.useragent."

#define ENCODE_AND_PUSH(name, structured, body, charset, usemime) \
  { \
    PUSH_STRING((name)); \
    convbuf = nsMsgI18NEncodeMimePartIIStr((body), (structured), (charset), strlen(name), (usemime)); \
    if (convbuf) { \
      PUSH_STRING (convbuf); \
      PR_FREEIF(convbuf); \
    } \
    else \
      PUSH_STRING((body)); \
    PUSH_NEWLINE (); \
  }

char * 
mime_generate_headers (nsMsgCompFields *fields,
                       const char *charset,
                       nsMsgDeliverMode deliver_mode, nsIPrompt * aPrompt, PRInt32 *status)
{
  nsresult rv;
  *status = 0;

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
  if (NS_FAILED(rv)) {
    *status = rv;
    return nsnull;
  }

  PRBool usemime = nsMsgMIMEGetConformToStandard();
  PRInt32 size = 0;
  char *buffer = 0, *buffer_tail = 0;
  PRBool isDraft =
  deliver_mode == nsIMsgSend::nsMsgSaveAsDraft ||
  deliver_mode == nsIMsgSend::nsMsgSaveAsTemplate ||
  deliver_mode == nsIMsgSend::nsMsgQueueForLater;

  const char* pFrom;
  const char* pTo;
  const char* pCc;
  const char* pMessageID;
  const char* pReplyTo;
  const char* pOrg;
  const char* pNewsGrp;
  const char* pFollow;
  const char* pSubject;
  const char* pPriority;
  const char* pReference;
  const char* pOtherHdr;
  char *convbuf;

  PRBool hasDisclosedRecipient = PR_FALSE;

  nsCAutoString headerBuf;    // accumulate header strings to get length
  headerBuf.Truncate(0);

  NS_ASSERTION (fields, "null fields");
  if (!fields)
    return nsnull;

  pFrom = fields->GetFrom(); 
  if (pFrom)
    headerBuf.Append(pFrom);
  pReplyTo =fields->GetReplyTo(); 
  if (pReplyTo)
    headerBuf.Append(pReplyTo);
  pTo = fields->GetTo();
  if (pTo)
    headerBuf.Append(pTo);
  pCc = fields->GetCc(); 
  if (pCc)
    headerBuf.Append(pCc);
  pNewsGrp = fields->GetNewsgroups(); if (pNewsGrp)     size += 3 * PL_strlen (pNewsGrp);
  pFollow= fields->GetFollowupTo(); if (pFollow)        size += 3 * PL_strlen (pFollow);
  pSubject = fields->GetSubject(); 
  if (pSubject)
    headerBuf.Append(pSubject);
  pReference = fields->GetReferences(); if (pReference)   size += 3 * PL_strlen (pReference);
  pOrg= fields->GetOrganization(); 
  if (pOrg)
    headerBuf.Append(pOrg);
  pOtherHdr= fields->GetOtherRandomHeaders(); if (pOtherHdr)  size += 3 * PL_strlen (pOtherHdr);
  pPriority = fields->GetPriority();  if (pPriority)      size += 3 * PL_strlen (pPriority);
  pMessageID = fields->GetMessageId(); if (pMessageID)    size += PL_strlen (pMessageID);

  /* Multiply by 3 here to make enough room for MimePartII conversion */
  size += 3 * headerBuf.Length();

  /* Add a bunch of slop for the static parts of the headers. */
  /* size += 2048; */
  size += 2560;

  buffer = (char *) PR_Malloc (size);
  if (!buffer)
    return nsnull; /* NS_ERROR_OUT_OF_MEMORY */
  
  buffer_tail = buffer;

  if (pMessageID && *pMessageID) {
    PUSH_STRING ("Message-ID: ");
    PUSH_STRING (pMessageID);
    PUSH_NEWLINE ();
    /* MDN request header requires to have MessageID header presented
    * in the message in order to
    * coorelate the MDN reports to the original message. Here will be
    * the right place
    */

    if (fields->GetReturnReceipt() && 
      (deliver_mode != nsIMsgSend::nsMsgSaveAsDraft &&
      deliver_mode != nsIMsgSend::nsMsgSaveAsTemplate))
    {
        PRInt32 receipt_header_type = nsIMsgMdnGenerator::eDntType;
        fields->GetReceiptHeaderType(&receipt_header_type);

      // nsIMsgMdnGenerator::eDntType = MDN Disposition-Notification-To: ;
      // nsIMsgMdnGenerator::eRrtType = Return-Receipt-To: ;
      // nsIMsgMdnGenerator::eDntRrtType = both MDN DNT and RRT headers .
      if (receipt_header_type != nsIMsgMdnGenerator::eRrtType)
        ENCODE_AND_PUSH(
	  "Disposition-Notification-To: ", PR_TRUE, pFrom, charset, usemime);
      if (receipt_header_type != nsIMsgMdnGenerator::eDntType)
        ENCODE_AND_PUSH(
	  "Return-Receipt-To: ", PR_TRUE, pFrom, charset, usemime);
    }

#ifdef SUPPORT_X_TEMPLATE_NAME
    if (deliver_mode == MSG_SaveAsTemplate) {
      const char *pStr = fields->GetTemplateName();
      pStr = pStr ? pStr : "";
      ENCODE_AND_PUSH("X-Template: ", PR_FALSE, pStr, charset, usemime);
    }
#endif /* SUPPORT_X_TEMPLATE_NAME */
  }

  PRExplodedTime now;
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
  int gmtoffset = (now.tm_params.tp_gmt_offset + now.tm_params.tp_dst_offset) / 60;

  /* Use PR_FormatTimeUSEnglish() to format the date in US English format,
     then figure out what our local GMT offset is, and append it (since
     PR_FormatTimeUSEnglish() can't do that.) Generate four digit years as
     per RFC 1123 (superceding RFC 822.)
   */
  PR_FormatTimeUSEnglish(buffer_tail, 100,
               "Date: %a, %d %b %Y %H:%M:%S ",
               &now);

  buffer_tail += PL_strlen (buffer_tail);
  PR_snprintf(buffer_tail, buffer + size - buffer_tail,
        "%c%02d%02d" CRLF,
        (gmtoffset >= 0 ? '+' : '-'),
        ((gmtoffset >= 0 ? gmtoffset : -gmtoffset) / 60),
        ((gmtoffset >= 0 ? gmtoffset : -gmtoffset) % 60));
  buffer_tail += PL_strlen (buffer_tail);

  if (pFrom && *pFrom) 
  {
    ENCODE_AND_PUSH("From: ", PR_TRUE, pFrom, charset, usemime);
  }

  if (pReplyTo && *pReplyTo)
  {
    ENCODE_AND_PUSH("Reply-To: ", PR_TRUE, pReplyTo, charset, usemime);
  }

  if (pOrg && *pOrg)
  {
    ENCODE_AND_PUSH("Organization: ", PR_FALSE, pOrg, charset, usemime);
  }

  // X-Mozilla-Draft-Info
  if (isDraft) 
  {
    PUSH_STRING(HEADER_X_MOZILLA_DRAFT_INFO);
    PUSH_STRING(": internal/draft; ");
    if (fields->GetAttachVCard())
      PUSH_STRING("vcard=1");
    else
      PUSH_STRING("vcard=0");
    PUSH_STRING("; ");
    if (fields->GetReturnReceipt()) {
      // slight change compared to 4.x; we used to use receipt= to tell
      // whether the draft/template has request for either MDN or DNS or both
      // return receipt; since the DNS is out of the picture we now use the
      // header type + 1 to tell whether user has requested the return receipt
      PRInt32 headerType = 0;
      fields->GetReceiptHeaderType(&headerType);
      char *type = PR_smprintf("%d", (int)headerType + 1);
      if (type)
      {
        PUSH_STRING("receipt=");
        PUSH_STRING(type);
        PR_FREEIF(type);
      }
    }
    else
      PUSH_STRING("receipt=0");
    PUSH_STRING("; ");
    if (fields->GetUuEncodeAttachments())
      PUSH_STRING("uuencode=1");
    else
      PUSH_STRING("uuencode=0");

    PUSH_NEWLINE ();
  }


  nsCOMPtr<nsIHttpProtocolHandler> pHTTPHandler = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http", &rv); 
  if (NS_SUCCEEDED(rv) && pHTTPHandler)
  {
    nsCAutoString userAgentString;
#ifdef MOZ_THUNDERBIRD

    nsXPIDLCString userAgentOverride;
    prefs->GetCharPref("general.useragent.override", getter_Copies(userAgentOverride));

    // allow a user to override the default UA
    if (!userAgentOverride)
    {
      nsCOMPtr<nsIXULAppInfo> xulAppInfo (do_GetService(XULAPPINFO_SERVICE_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv)) 
      {
        xulAppInfo->GetName(userAgentString);

	      nsCAutoString productSub;
	      pHTTPHandler->GetProductSub(productSub);

	      nsCAutoString platform;
	      pHTTPHandler->GetPlatform(platform);

	      userAgentString += ' ';
	      userAgentString += NS_STRINGIFY(MOZ_APP_VERSION);
	      userAgentString += " (";
	      userAgentString += platform;
	      userAgentString += "/";
	      userAgentString += productSub;
	      userAgentString += ")";
      }
    }
    else
      userAgentString = userAgentOverride;
#else
      pHTTPHandler->GetUserAgent(userAgentString);
#endif

    if (!userAgentString.IsEmpty())
    {
      PUSH_STRING ("User-Agent: ");  
      PUSH_STRING(userAgentString.get());
      PUSH_NEWLINE ();
    }
  }

  PUSH_STRING ("MIME-Version: 1.0" CRLF);

  if (pNewsGrp && *pNewsGrp) {
    /* turn whitespace into a comma list
    */
    char *duppedNewsGrp = PL_strdup(pNewsGrp);
    if (!duppedNewsGrp) {
      PR_FREEIF(buffer);
      return nsnull; /* NS_ERROR_OUT_OF_MEMORY */
    }
    char *n2 = nsMsgStripLine(duppedNewsGrp);

    for(char *ptr = n2; *ptr != '\0'; ptr++) {
      /* find first non white space */
      while(!IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
        ptr++;

      if(*ptr == '\0')
        break;

      if(*ptr != ',')
        *ptr = ',';

      /* find next non white space */
      char *ptr2 = ptr+1;
      while(IS_SPACE(*ptr2))
        ptr2++;

      if(ptr2 != ptr+1)
        PL_strcpy(ptr+1, ptr2);
    }

    // we need to decide the Newsgroup related headers
    // to write to the outgoing message. In ANY case, we need to write the
    // "Newsgroup" header which is the "proper" header as opposed to the
    // HEADER_X_MOZILLA_NEWSHOST which can contain the "news:" URL's.
    //
    // Since n2 can contain data in the form of:
    // "news://news.mozilla.org/netscape.test,news://news.mozilla.org/netscape.junk"
    // we need to turn that into: "netscape.test,netscape.junk"
    //
    nsCOMPtr <nsINntpService> nntpService = do_GetService("@mozilla.org/messenger/nntpservice;1");
    if (NS_FAILED(rv) || !nntpService) {
      *status = NS_ERROR_FAILURE;
      return nsnull;
    }

    nsXPIDLCString newsgroupsHeaderVal;
    nsXPIDLCString newshostHeaderVal;
    rv = nntpService->GenerateNewsHeaderValsForPosting(n2, getter_Copies(newsgroupsHeaderVal), getter_Copies(newshostHeaderVal));
    if (NS_FAILED(rv)) {
      *status = rv;
      return nsnull;
    }

    // fixme:the newsgroups header had better be encoded as the server-side
    // character encoding, but this |charset| might be different from it.
    ENCODE_AND_PUSH("Newsgroups: ", PR_FALSE, newsgroupsHeaderVal.get(),
                    charset, PR_FALSE);

    // If we are here, we are NOT going to send this now. (i.e. it is a Draft, 
    // Send Later file, etc...). Because of that, we need to store what the user
    // typed in on the original composition window for use later when rebuilding
    // the headers
    if (deliver_mode != nsIMsgSend::nsMsgDeliverNow && deliver_mode != nsIMsgSend::nsMsgSendUnsent)
    {
      // This is going to be saved for later, that means we should just store
      // what the user typed into the "Newsgroup" line in the HEADER_X_MOZILLA_NEWSHOST
      // header for later use by "Send Unsent Messages", "Drafts" or "Templates"
      PUSH_STRING (HEADER_X_MOZILLA_NEWSHOST);
      PUSH_STRING (": ");
      PUSH_STRING (newshostHeaderVal.get());
      PUSH_NEWLINE ();
    }

    PR_FREEIF(duppedNewsGrp);
    hasDisclosedRecipient = PR_TRUE;
  }

  /* #### shamelessly duplicated from above */
  if (pFollow && *pFollow) {
    /* turn whitespace into a comma list
    */
    char *duppedFollowup = PL_strdup(pFollow);
    if (!duppedFollowup) {
      PR_FREEIF(buffer);
      return nsnull; /* NS_ERROR_OUT_OF_MEMORY */
    }
    char *n2 = nsMsgStripLine (duppedFollowup);

    for (char *ptr = n2; *ptr != '\0'; ptr++) {
      /* find first non white space */
      while(!IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
        ptr++;

      if(*ptr == '\0')
        break;

      if(*ptr != ',')
        *ptr = ',';

      /* find next non white space */
      char *ptr2 = ptr+1;
      while(IS_SPACE(*ptr2))
        ptr2++;

      if(ptr2 != ptr+1)
        PL_strcpy(ptr+1, ptr2);
    }

    PUSH_STRING ("Followup-To: ");
    PUSH_STRING (n2);
    PR_Free (duppedFollowup);
    PUSH_NEWLINE ();
  }

  if (pTo && *pTo) {
    ENCODE_AND_PUSH("To: ", PR_TRUE, pTo, charset, usemime);
    hasDisclosedRecipient = PR_TRUE;
  }
 
  if (pCc && *pCc) {
    ENCODE_AND_PUSH("CC: ", PR_TRUE, pCc, charset, usemime);
    hasDisclosedRecipient = PR_TRUE;
  }

  // If we don't have disclosed recipient (only Bcc), address the message to
  // undisclosed-recipients to prevent problem with some servers

  // If we are saving the message as a draft, don't bother inserting the undisclosed recipients field. We'll take care of that when we
  // really send the message.
  if (!hasDisclosedRecipient && !isDraft) {
    PRBool bAddUndisclosedRecipients = PR_TRUE;
    prefs->GetBoolPref("mail.compose.add_undisclosed_recipients", &bAddUndisclosedRecipients);
    if (bAddUndisclosedRecipients) {
      const char* pBcc = fields->GetBcc(); //Do not free me!
      if (pBcc && *pBcc) {
        nsCOMPtr<nsIStringBundleService> stringService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIStringBundle> composeStringBundle;
          rv = stringService->CreateBundle("chrome://messenger/locale/messengercompose/composeMsgs.properties", getter_AddRefs(composeStringBundle));
          if (NS_SUCCEEDED(rv)) {
            nsXPIDLString undisclosedRecipients;
            rv = composeStringBundle->GetStringFromID(NS_MSG_UNDISCLOSED_RECIPIENTS, getter_Copies(undisclosedRecipients));
            if (NS_SUCCEEDED(rv) && !undisclosedRecipients.IsEmpty()){
              char * cstr = ToNewCString(undisclosedRecipients);
              if (cstr) {
                PUSH_STRING("To: ");
                PUSH_STRING(cstr);
                PUSH_STRING(":;");
                PUSH_NEWLINE ();
              }
              PR_Free(cstr);
            }
          }
        }
      }
    }
  }

  if (pSubject && *pSubject) {
    ENCODE_AND_PUSH("Subject: ", PR_FALSE, pSubject, charset, usemime);
  }
  
  // Skip no or empty priority.
  if (pPriority && *pPriority) {
    nsMsgPriorityValue priorityValue;

    NS_MsgGetPriorityFromString(pPriority, priorityValue);

    // Skip default priority.
    if (priorityValue != nsMsgPriority::Default) {
      nsCAutoString priorityName;
      nsCAutoString priorityValueString;

      NS_MsgGetPriorityValueString(priorityValue, priorityValueString);
      NS_MsgGetUntranslatedPriorityName(priorityValue, priorityName);

      // Output format: [X-Priority: <pValue> (<pName>)]
      PUSH_STRING("X-Priority: ");
      PUSH_STRING(priorityValueString.get());
      PUSH_STRING(" (");
      PUSH_STRING(priorityName.get());
      PUSH_STRING(")");
      PUSH_NEWLINE();
    }
  }

  if (pReference && *pReference) {
    PUSH_STRING ("References: ");
    if (PL_strlen(pReference) >= 986) {
      char *references = PL_strdup(pReference);
      char *trimAt = PL_strchr(references+1, '<');
      char *ptr;
      // per sfraser, RFC 1036 - a message header line should not exceed
      // 998 characters including the header identifier
      // retiring the earliest reference one after the other
      // but keep the first one for proper threading
      while (references && PL_strlen(references) >= 986 && trimAt) {
        ptr = PL_strchr(trimAt+1, '<');
        if (ptr)
          memmove(trimAt, ptr, PL_strlen(ptr)+1); // including the
        else
          break;
      }
      NS_ASSERTION(references, "null references");
      if (references) {
        PUSH_STRING (references);
        PR_Free(references);
      }
    }
    else
      PUSH_STRING (pReference);
    PUSH_NEWLINE ();
    {
      const char *lastRef = PL_strrchr(pReference, '<');

      if (lastRef) {
        PUSH_STRING ("In-Reply-To: ");
        PUSH_STRING (lastRef);
        PUSH_NEWLINE ();
      }
    }
  }

  if (pOtherHdr && *pOtherHdr) {
    /* Assume they already have the right newlines and continuations
     and so on.  for these headers, the PUSH_NEWLINE() happens in addressingWidgetOverlay.js */
    PUSH_STRING (pOtherHdr);
  }

  if (buffer_tail > buffer + size - 1)
    return nsnull;

  /* realloc it smaller... */
  buffer = (char*)PR_REALLOC (buffer, buffer_tail - buffer + 1);

  return buffer;
}

static void
GenerateGlobalRandomBytes(unsigned char *buf, PRInt32 len)
{
  static PRBool    firstTime = PR_TRUE;
  
  if (firstTime)
  {
    // Seed the random-number generator with current time so that
    // the numbers will be different every time we run.
    PRInt32 aTime;
    LL_L2I(aTime, PR_Now());
    srand( (unsigned)aTime );
    firstTime = PR_FALSE;
  }
  
  for( PRInt32 i = 0; i < len; i++ )
    buf[i] = rand() % 10;
}
   
char 
*mime_make_separator(const char *prefix)
{
  unsigned char rand_buf[13]; 
  GenerateGlobalRandomBytes(rand_buf, 12);

  return PR_smprintf("------------%s"
           "%02X%02X%02X%02X"
           "%02X%02X%02X%02X"
           "%02X%02X%02X%02X",
           prefix,
           rand_buf[0], rand_buf[1], rand_buf[2], rand_buf[3],
           rand_buf[4], rand_buf[5], rand_buf[6], rand_buf[7],
           rand_buf[8], rand_buf[9], rand_buf[10], rand_buf[11]);
}

static char *
RFC2231ParmFolding(const char *parmName, const nsAFlatCString& charset,
                   const char *language, const nsAFlatString& parmValue);

static char *
RFC2047ParmFolding(const nsAFlatCString& aCharset,
                   const nsAFlatCString& aFileName, PRInt32 aParmFolding);

char * 
mime_generate_attachment_headers (const char *type,
                  const char *type_param,
                  const char *encoding,
                  const char *description,
                  const char *x_mac_type,
                  const char *x_mac_creator,
                  const char *real_name,
                  const char *base_url,
                  PRBool /*digest_p*/,
                  nsMsgAttachmentHandler * /*ma*/,
                  const char *attachmentCharset,
                  const char *bodyCharset,
                  PRBool bodyIsAsciiOnly,
                  const char *content_id, 
                  PRBool      aBodyDocument)
{
  NS_ASSERTION (encoding, "null encoding");

  PRInt32 parmFolding = 2; // RFC 2231-compliant
  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
  if (prefs)
    prefs->GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

  /* Let's encode the real name */
  char *encodedRealName = nsnull;
  nsXPIDLCString charset;   // actual charset used for MIME encode
  nsAutoString realName;
  if (real_name)
  {
    // first try main body's charset to encode the file name, 
    // then try local file system charset if fails
    CopyUTF8toUTF16(real_name, realName);
    if (bodyCharset && *bodyCharset &&
        nsMsgI18Ncheck_data_in_charset_range(bodyCharset, realName.get()))
      charset.Assign(bodyCharset);
    else
    {
      charset.Assign(nsMsgI18NFileSystemCharset());
      if (!nsMsgI18Ncheck_data_in_charset_range(charset.get(), realName.get()))
        charset.Assign("UTF-8"); // set to UTF-8 if fails again
    }

    if (parmFolding == 2 || parmFolding == 3 || parmFolding == 4) {
      encodedRealName = RFC2231ParmFolding("filename", charset, nsnull, 
                                           realName);
      // somehow RFC2231ParamFolding failed. fall back to RFC 2047     
      if (!encodedRealName || !*encodedRealName) {
        PR_FREEIF(encodedRealName);
        parmFolding = 0; 
      }
    }

    // Not RFC 2231 style encoding (it's not standard-compliant)
    if (parmFolding == 0 || parmFolding == 1) {
      encodedRealName =
        RFC2047ParmFolding(charset, nsDependentCString(real_name), parmFolding);
    }
  }

  nsCString buf;  // very likely to be longer than 64 characters
  buf.Append("Content-Type: ");
  buf.Append(type);
  if (type_param && *type_param)
  {
    if (*type_param != ';')
      buf.Append("; ");
    buf.Append(type_param);
  }

  if (mime_type_needs_charset (type)) 
  {
    
    char charset_label[65] = "";   // Content-Type: charset
    if (attachmentCharset)
    {
      PL_strncpy(charset_label, attachmentCharset, sizeof(charset_label)-1);
      charset_label[sizeof(charset_label)-1] = '\0';
    }
    
    /* If the characters are all 7bit, arguably it's better to 
    claim the charset to be US-ASCII. However, it causes
    a major 'interoperability problem' with MS OE, which makes it hard
    to sell Mozilla/TB to people most of whose correspondents use
    MS OE. MS OE turns all non-ASCII characters to question marks 
    in replies to messages labeled as US-ASCII if users select 'send as is'
    with MIME turned on. (with MIME turned off, this happens without
    any warning.) To avoid this, we use the label 'US-ASCII' only when
    it's explicitly requested by setting the hidden pref.
    'mail.label_ascii_only_mail_as_us_ascii'. (bug 247958) */
    PRBool labelAsciiAsAscii = PR_FALSE;
    if (prefs)
      prefs->GetBoolPref("mail.label_ascii_only_mail_as_us_ascii",
                         &labelAsciiAsAscii);
    if (labelAsciiAsAscii && encoding &&
        !PL_strcasecmp (encoding, "7bit") && bodyIsAsciiOnly)
      PL_strcpy (charset_label, "us-ascii");
    
    // If charset is multibyte then no charset to be specified (apply base64 instead).
    // The list of file types match with PickEncoding() where we put base64 label.
    if ( ((attachmentCharset && !nsMsgI18Nmultibyte_charset(attachmentCharset)) ||
         ((PL_strcasecmp(type, TEXT_HTML) == 0) ||
         (PL_strcasecmp(type, TEXT_MDL) == 0) ||
         (PL_strcasecmp(type, TEXT_PLAIN) == 0) ||
         (PL_strcasecmp(type, TEXT_RICHTEXT) == 0) ||
         (PL_strcasecmp(type, TEXT_ENRICHED) == 0) ||
         (PL_strcasecmp(type, TEXT_VCARD) == 0) ||
         (PL_strcasecmp(type, APPLICATION_DIRECTORY) == 0) || /* text/x-vcard synonym */
         (PL_strcasecmp(type, TEXT_CSS) == 0) ||
         (PL_strcasecmp(type, TEXT_JSSS) == 0)) ||
         (PL_strcasecmp(encoding, ENCODING_BASE64) != 0)) &&
         (*charset_label))
    {
      buf.Append("; charset=");
      buf.Append(charset_label);
    }
  }

  // Only do this if we are in the body of a message
  if (aBodyDocument)
  {
    // Add format=flowed as in RFC 2646 if we are using that
    if(type && !PL_strcasecmp(type, "text/plain"))
    {
      if(UseFormatFlowed(bodyCharset))
        buf.Append("; format=flowed");
      // else
      // {
      // Don't add a markup. Could use 
      //        PUSH_STRING ("; format=fixed");
      // but it is equivalent to nothing at all and we do want
      // to save bandwidth. Don't we?
      // }
    }
  }    

  if (x_mac_type && *x_mac_type) {
    buf.Append("; x-mac-type=\"");
    buf.Append(x_mac_type);
    buf.Append("\"");
  }

  if (x_mac_creator && *x_mac_creator) {
    buf.Append("; x-mac-creator=\"");
    buf.Append(x_mac_creator);
    buf.Append("\"");
  }

#ifdef EMIT_NAME_IN_CONTENT_TYPE
  if (encodedRealName && *encodedRealName) {
    // Note that we don't need to output the name field if the name encoding is
    // RFC 2231. If the MUA knows the RFC 2231, it should know the RFC 2183 too.
    if (parmFolding != 2) {
      char *nameValue = nsnull;
      if (parmFolding == 3 || parmFolding == 4)
        nameValue = RFC2047ParmFolding(charset, nsDependentCString(real_name),
                                       parmFolding);
      if (!nameValue || !*nameValue) {
        PR_FREEIF(nameValue);
        nameValue = encodedRealName;
      }
      buf.Append(";\r\n name=\"");
      buf.Append(nameValue);
      buf.Append("\"");
      if (nameValue != encodedRealName)
        PR_FREEIF(nameValue);
    }
  }
#endif /* EMIT_NAME_IN_CONTENT_TYPE */
  buf.Append(CRLF);

  buf.Append("Content-Transfer-Encoding: ");
  buf.Append(encoding);
  buf.Append(CRLF);

  if (description && *description) {
    char *s = mime_fix_header (description);
    if (s) {
      buf.Append("Content-Description: ");
      buf.Append(s);
      buf.Append(CRLF);
      PR_Free(s);
    }
  }

  if ( (content_id) && (*content_id) )
  {
    buf.Append("Content-ID: <");
    buf.Append(content_id);
    buf.Append(">");  
    buf.Append(CRLF);
  }

  if (encodedRealName && *encodedRealName) {
    char *period = PL_strrchr(encodedRealName, '.');
    PRInt32 pref_content_disposition = 0;

    if (prefs) {
      nsresult rv = prefs->GetIntPref("mail.content_disposition_type",
                                      &pref_content_disposition);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get mail.content_disposition_type");
    }

    buf.Append("Content-Disposition: ");

    if (pref_content_disposition == 1)
      buf.Append("attachment");
    else
      if (pref_content_disposition == 2 && 
          (!PL_strcasecmp(type, TEXT_PLAIN) || 
          (period && !PL_strcasecmp(period, ".txt"))))
        buf.Append("attachment");

      /* If this document is an anonymous binary file or a vcard, 
      then always show it as an attachment, never inline. */
      else
        if (!PL_strcasecmp(type, APPLICATION_OCTET_STREAM) || 
            !PL_strcasecmp(type, TEXT_VCARD) ||
            !PL_strcasecmp(type, APPLICATION_DIRECTORY)) /* text/x-vcard synonym */
          buf.Append("attachment");
        else
          buf.Append("inline");

    if (parmFolding == 0 || parmFolding == 1) {
      buf.Append(";\r\n filename=\"");
      buf.Append(encodedRealName);
      buf.Append("\"" CRLF);
    }
    else // if (parmFolding == 2 || parmFolding == 3 || parmFolding == 4)
    {
      buf.Append(";\r\n ");
      buf.Append(encodedRealName);
      buf.Append(CRLF);
    }
  }
  else
    if (type &&
        (!PL_strcasecmp (type, MESSAGE_RFC822) ||
        !PL_strcasecmp (type, MESSAGE_NEWS)))
      buf.Append("Content-Disposition: inline" CRLF);
  
#ifdef GENERATE_CONTENT_BASE
  /* If this is an HTML document, and we know the URL it originally
     came from, write out a Content-Base header. */
  if (type &&
      (!PL_strcasecmp (type, TEXT_HTML) ||
      !PL_strcasecmp (type, TEXT_MDL)) &&
      base_url && *base_url)
  {
    PRInt32 col = 0;
    const char *s = base_url;
    const char *colon = PL_strchr (s, ':');
    PRBool useContentLocation = PR_FALSE;   /* rhp - add this  */

    if (!colon)
      goto GIVE_UP_ON_CONTENT_BASE;  /* malformed URL? */

    /* Don't emit a content-base that points to (or into) a news or
       mail message. */
    if (!PL_strncasecmp (s, "news:", 5) ||
        !PL_strncasecmp (s, "snews:", 6) ||
        !PL_strncasecmp (s, "IMAP:", 5) ||
        !PL_strncasecmp (s, "file:", 5) ||    /* rhp: fix targets from included HTML files */
        !PL_strncasecmp (s, "mailbox:", 8))
      goto GIVE_UP_ON_CONTENT_BASE;

    /* rhp - Put in a pref for using Content-Location instead of Content-Base.
           This will get tweaked to default to true in 5.0
    */
    if (prefs) 
      prefs->GetBoolPref("mail.use_content_location_on_send", &useContentLocation);

    if (useContentLocation)
      buf.Append("Content-Location: \"");
    else
      buf.Append("Content-Base: \"");
    /* rhp - Pref for Content-Location usage */

/* rhp: this is to work with the Content-Location stuff */
CONTENT_LOC_HACK:

    while (*s != 0 && *s != '#')
    {
      PRUint32 ot=buf.Length();
      char tmp[]="\x00\x00";
      /* URLs must be wrapped at 40 characters or less. */
      if (col >= 38) {
        buf.Append(CRLF "\t");
        col = 0;
      }

      if (*s == ' ')
        buf.Append("%20");
      else if (*s == '\t')
        buf.Append("%09");
      else if (*s == '\n')
        buf.Append("%0A");
      else if (*s == '\r')
        buf.Append("%0D");
      else {
	      tmp[0]=*s;
	      buf.Append(tmp);
      }
      s++;
      col += (buf.Length() - ot);
    }
    buf.Append("\"" CRLF);

    /* rhp: this is to try to get around this fun problem with Content-Location */
    if (!useContentLocation) {
      buf.Append("Content-Location: \"");
      s = base_url;
      col = 0;
      useContentLocation = PR_TRUE;
      goto CONTENT_LOC_HACK;
    }
    /* rhp: this is to try to get around this fun problem with Content-Location */

GIVE_UP_ON_CONTENT_BASE:
    ;
  }
#endif /* GENERATE_CONTENT_BASE */

  /* realloc it smaller... */

#ifdef DEBUG_jungshik
  printf ("header=%s\n", buf.get());
#endif
  PR_Free(encodedRealName);
  return PL_strdup(buf.get());
}

static PRBool isValidHost( const char* host )
{
  if ( host )
    for (const char *s = host; *s; ++s)
      if  (  !nsCRT::IsAsciiAlpha(*s)
         && !nsCRT::IsAsciiDigit(*s)
         && *s != '-'
         && *s != '_'
         && *s != '.'
         )
      {
       host = nsnull;
       break;
      }

  return nsnull != host;
}

char *
msg_generate_message_id (nsIMsgIdentity *identity)
{
  PRUint32 now;
  PRTime prNow = PR_Now();
  PRInt64 microSecondsPerSecond, intermediateResult;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, prNow, microSecondsPerSecond);
    LL_L2UI(now, intermediateResult);

  PRUint32 salt = 0;
  const char *host = 0;
  
  nsXPIDLCString forcedFQDN;
  nsXPIDLCString from;
  nsresult rv = NS_OK;

  rv = identity->GetCharAttribute("FQDN", getter_Copies(forcedFQDN));

  if (NS_SUCCEEDED(rv) && forcedFQDN)
    host = forcedFQDN.get();

  if (!isValidHost(host))
  {
    nsresult rv = identity->GetEmail(getter_Copies(from));
    if (NS_SUCCEEDED(rv) && from)
      host = strchr(from,'@');

    // No '@'? Munged address, anti-spam?
    // see bug #197203
    if (host)
      ++host;
  }

  if (!isValidHost(host))
  /* If we couldn't find a valid host name to use, we can't generate a
     valid message ID, so bail, and let NNTP and SMTP generate them. */
    return 0;

  GenerateGlobalRandomBytes((unsigned char *) &salt, sizeof(salt));
  return PR_smprintf("<%lX.%lX@%s>",
           (unsigned long) now, (unsigned long) salt, host);
}


inline static PRBool is7bitCharset(const nsAFlatCString& charset)
{
  // charset name is canonical (no worry about case-sensitivity)
  return charset.EqualsLiteral("HZ-GB-2312") ||
         Substring(charset, 0, 8).EqualsLiteral("ISO-2022-");
}

#define PR_MAX_FOLDING_LEN 75     // this is to gurantee the folded line will
                                  // never be greater than 78 = 75 + CRLFLWSP
/*static */ char *
RFC2231ParmFolding(const char *parmName, const nsAFlatCString& charset,
                   const char *language, const nsAFlatString& parmValue)
{
  NS_ENSURE_TRUE(parmName && *parmName && !parmValue.IsEmpty(), nsnull);

  PRBool needEscape;
  char *dupParm = nsnull; 

  if (!IsASCII(parmValue) || is7bitCharset(charset)) {
    needEscape = PR_TRUE;
    nsCAutoString nativeParmValue; 
    ConvertFromUnicode(charset.get(), parmValue, nativeParmValue);
    dupParm = nsEscape(nativeParmValue.get(), url_All);
  }
  else {
    needEscape = PR_FALSE;
    dupParm = 
      msg_make_filename_qtext(NS_LossyConvertUTF16toASCII(parmValue).get(),
                              PR_TRUE);
  }

  if (!dupParm)
    return nsnull; 

  PRInt32 parmNameLen = PL_strlen(parmName);
  PRInt32 parmValueLen = PL_strlen(dupParm);

  if (needEscape)
    parmNameLen += 5;   // *=__'__'___ or *[0]*=__'__'__ or *[1]*=___
  else
    parmNameLen += 5;   // *[0]="___";

  PRInt32 languageLen = language ?  PL_strlen(language) : 0;
  PRInt32 charsetLen = charset.Length();
  char *foldedParm = nsnull; 

  if ((parmValueLen + parmNameLen + charsetLen + languageLen) <
      PR_MAX_FOLDING_LEN)
  {
    foldedParm = PL_strdup(parmName);
    if (needEscape)
    {
      NS_MsgSACat(&foldedParm, "*=");
      if (charsetLen)
        NS_MsgSACat(&foldedParm, charset.get());
      NS_MsgSACat(&foldedParm, "'");
      if (languageLen)
        NS_MsgSACat(&foldedParm, language);
      NS_MsgSACat(&foldedParm, "'");
    }
    else
      NS_MsgSACat(&foldedParm, "=\"");
    NS_MsgSACat(&foldedParm, dupParm);
    if (!needEscape)
      NS_MsgSACat(&foldedParm, "\"");
    goto done;
  }
  else
  {
    int curLineLen = 0;
    int counter = 0;
    char digits[32];
    char *start = dupParm;
    char *end = NULL;
    char tmp = 0;

    while (parmValueLen > 0)
    {
      curLineLen = 0;
      if (counter == 0) {
        PR_FREEIF(foldedParm)
        foldedParm = PL_strdup(parmName);
      }
      else {
        NS_MsgSACat(&foldedParm, ";\r\n ");
        NS_MsgSACat(&foldedParm, parmName);
      }
      PR_snprintf(digits, sizeof(digits), "*%d", counter);
      NS_MsgSACat(&foldedParm, digits);
      curLineLen += PL_strlen(digits);
      if (needEscape)
      {
        NS_MsgSACat(&foldedParm, "*=");
        if (counter == 0)
        {
          if (charsetLen)
            NS_MsgSACat(&foldedParm, charset.get());
          NS_MsgSACat(&foldedParm, "'");
          if (languageLen)
            NS_MsgSACat(&foldedParm, language);
          NS_MsgSACat(&foldedParm, "'");
          curLineLen += charsetLen;
          curLineLen += languageLen;
        }
      }
      else
      {
        NS_MsgSACat(&foldedParm, "=\"");
      }
      counter++;
      curLineLen += parmNameLen;
      if (parmValueLen <= PR_MAX_FOLDING_LEN - curLineLen)
        end = start + parmValueLen;
      else
        end = start + (PR_MAX_FOLDING_LEN - curLineLen);

      tmp = 0;
      if (*end && needEscape)
      {
        // check to see if we are in the middle of escaped char
        if (*end == '%')
        {
          tmp = '%'; *end = nsnull;
        }
        else if (end-1 > start && *(end-1) == '%')
        {
          end -= 1; tmp = '%'; *end = nsnull;
        }
        else if (end-2 > start && *(end-2) == '%')
        {
          end -= 2; tmp = '%'; *end = nsnull;
        }
        else
        {
          tmp = *end; *end = nsnull;
        }
      }
      else
      {
        // XXX should check if we are in the middle of escaped char (RFC 822)
        tmp = *end; *end = nsnull;
      }
      NS_MsgSACat(&foldedParm, start);
      if (!needEscape)
        NS_MsgSACat(&foldedParm, "\"");

      parmValueLen -= (end-start);
      if (tmp)
        *end = tmp;
      start = end;
    }
  }

done:
  if (needEscape) 
    nsMemory::Free(dupParm);
  else 
    PR_Free(dupParm);
  return foldedParm;
}

/*static */ char *
RFC2047ParmFolding(const nsAFlatCString& aCharset,
                   const nsAFlatCString& aFileName, PRInt32 aParmFolding)
{
  PRBool usemime = nsMsgMIMEGetConformToStandard();
  char *encodedRealName =
    nsMsgI18NEncodeMimePartIIStr(aFileName.get(), PR_FALSE, aCharset.get(),
                                 0, usemime);

  if (!encodedRealName || !*encodedRealName) {
    PR_FREEIF(encodedRealName);
    encodedRealName = (char *) PR_Malloc(aFileName.Length() + 1);
    if (encodedRealName)
      PL_strcpy(encodedRealName, aFileName.get());
  }

  // Now put backslashes before special characters per RFC 822 
  char *qtextName =
    msg_make_filename_qtext(encodedRealName,
      aParmFolding == 0 || aParmFolding == 3 ? PR_TRUE : PR_FALSE);
  if (qtextName) {
    PR_FREEIF(encodedRealName);
    encodedRealName = qtextName;
  }
  return encodedRealName;
}

PRBool 
mime_7bit_data_p (const char *string, PRUint32 size)
{
  if ((!string) || (!*string))
    return PR_TRUE;

  char *ptr = (char *)string;
  for (PRUint32 i=0; i<size; i++)
  {
    if ((unsigned char) ptr[i] > 0x7F)
      return PR_FALSE;
  }
  return PR_TRUE;
}

/* Strips whitespace, and expands newlines into newline-tab for use in
   mail headers.  Returns a new string or 0 (if it would have been empty.)
   If addr_p is true, the addresses will be parsed and reemitted as
   rfc822 mailboxes.
 */
char * 
mime_fix_header_1 (const char *string, PRBool addr_p, PRBool news_p)
{
  char *new_string;
  const char *in;
  char *out;
  PRInt32 i, old_size, new_size;

  if (!string || !*string)
    return 0;

  if (addr_p) {
    nsresult rv = NS_OK;
    nsCOMPtr<nsIMsgHeaderParser> pHeader = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      char *n;
      pHeader->ReformatHeaderAddresses(nsnull, string, &n);
      if (n)        return n;
    }
  }

  old_size = PL_strlen (string);
  new_size = old_size;
  for (i = 0; i < old_size; i++)
    if (string[i] == nsCRT::CR || string[i] == nsCRT::LF)
      new_size += 2;

  new_string = (char *) PR_Malloc (new_size + 1);
  if (! new_string)
    return 0;

  in  = string;
  out = new_string;

  /* strip leading whitespace. */
  while (IS_SPACE (*in))
    in++;

  /* replace CR, LF, or CRLF with CRLF-TAB. */
  while (*in) {
    if (*in == nsCRT::CR || *in == nsCRT::LF) {
      if (*in == nsCRT::CR && in[1] == nsCRT::LF)
        in++;
      in++;
      *out++ = nsCRT::CR;
      *out++ = nsCRT::LF;
      *out++ = '\t';
    }
    else
      if (news_p && *in == ',') {
        *out++ = *in++;
        /* skip over all whitespace after a comma. */
        while (IS_SPACE (*in))
          in++;
      }
      else
        *out++ = *in++;
  }
  *out = 0;

  /* strip trailing whitespace. */
  while (out > in && IS_SPACE (out[-1]))
    *out-- = 0;

  /* If we ended up throwing it all away, use 0 instead of "". */
  if (!*new_string) {
    PR_Free (new_string);
    new_string = 0;
  }

  return new_string;
}

char * 
mime_fix_header (const char *string)
{
  return mime_fix_header_1 (string, PR_FALSE, PR_FALSE);
}

char * 
mime_fix_addr_header (const char *string)
{
  return mime_fix_header_1 (string, PR_TRUE, PR_FALSE);
}

char * 
mime_fix_news_header (const char *string)
{
  return mime_fix_header_1 (string, PR_FALSE, PR_TRUE);
}

PRBool
mime_type_requires_b64_p (const char *type)
{
  if (!type || !PL_strcasecmp (type, UNKNOWN_CONTENT_TYPE))
  /* Unknown types don't necessarily require encoding.  (Note that
     "unknown" and "application/octet-stream" aren't the same.) */
  return PR_FALSE;

  else if (!PL_strncasecmp (type, "image/", 6) ||
       !PL_strncasecmp (type, "audio/", 6) ||
       !PL_strncasecmp (type, "video/", 6) ||
       !PL_strncasecmp (type, "application/", 12))
  {
    /* The following types are application/ or image/ types that are actually
     known to contain textual data (meaning line-based, not binary, where
     CRLF conversion is desired rather than disasterous.)  So, if the type
     is any of these, it does not *require* base64, and if we do need to
     encode it for other reasons, we'll probably use quoted-printable.
     But, if it's not one of these types, then we assume that any subtypes
     of the non-"text/" types are binary data, where CRLF conversion would
     corrupt it, so we use base64 right off the bat.

     The reason it's desirable to ship these as text instead of just using
     base64 all the time is mainly to preserve the readability of them for
     non-MIME users: if I mail a /bin/sh script to someone, it might not
     need to be encoded at all, so we should leave it readable if we can.

     This list of types was derived from the comp.mail.mime FAQ, section
     10.2.2, "List of known unregistered MIME types" on 2-Feb-96.
     */
    static const char *app_and_image_types_which_are_really_text[] = {
    "application/mac-binhex40",   /* APPLICATION_BINHEX */
    "application/pgp",        /* APPLICATION_PGP */
    "application/pgp-keys",
    "application/x-pgp-message",  /* APPLICATION_PGP2 */
    "application/postscript",   /* APPLICATION_POSTSCRIPT */
    "application/x-uuencode",   /* APPLICATION_UUENCODE */
    "application/x-uue",      /* APPLICATION_UUENCODE2 */
    "application/uue",        /* APPLICATION_UUENCODE4 */
    "application/uuencode",     /* APPLICATION_UUENCODE3 */
    "application/sgml",
    "application/x-csh",
    "application/javascript",
    "application/ecmascript",
    "application/x-javascript",
    "application/x-latex",
    "application/x-macbinhex40",
    "application/x-ns-proxy-autoconfig",
    "application/x-www-form-urlencoded",
    "application/x-perl",
    "application/x-sh",
    "application/x-shar",
    "application/x-tcl",
    "application/x-tex",
    "application/x-texinfo",
    "application/x-troff",
    "application/x-troff-man",
    "application/x-troff-me",
    "application/x-troff-ms",
    "application/x-troff-ms",
    "application/x-wais-source",
    "image/x-bitmap",
    "image/x-pbm",
    "image/x-pgm",
    "image/x-portable-anymap",
    "image/x-portable-bitmap",
    "image/x-portable-graymap",
    "image/x-portable-pixmap",    /* IMAGE_PPM */
    "image/x-ppm",
    "image/x-xbitmap",        /* IMAGE_XBM */
    "image/x-xbm",          /* IMAGE_XBM2 */
    "image/xbm",          /* IMAGE_XBM3 */
    "image/x-xpixmap",
    "image/x-xpm",
    0 };
    const char **s;
    for (s = app_and_image_types_which_are_really_text; *s; s++)
    if (!PL_strcasecmp (type, *s))
      return PR_FALSE;

    /* All others must be assumed to be binary formats, and need Base64. */
    return PR_TRUE;
  }

  else
  return PR_FALSE;
}

//
// Some types should have a "charset=" parameter, and some shouldn't.
// This is what decides. 
//
PRBool 
mime_type_needs_charset (const char *type)
{
  /* Only text types should have charset. */
  if (!type || !*type)
    return PR_FALSE;
  else 
    if (!PL_strncasecmp (type, "text", 4))
      return PR_TRUE;
    else
      return PR_FALSE;
}

/* Given a string, convert it to 'qtext' (quoted text) for RFC822 header purposes. */
char *
msg_make_filename_qtext(const char *srcText, PRBool stripCRLFs)
{
  /* newString can be at most twice the original string (every char quoted). */
  char *newString = (char *) PR_Malloc(PL_strlen(srcText)*2 + 1);
  if (!newString) return NULL;

  const char *s = srcText;
  const char *end = srcText + PL_strlen(srcText);
  char *d = newString;

  while(*s)
  {
    /*  Put backslashes in front of existing backslashes, or double quote
      characters.
      If stripCRLFs is true, don't write out CRs or LFs. Otherwise,
      write out a backslash followed by the CR but not
      linear-white-space.
      We might already have quoted pair of "\ " or "\\t" skip it. 
      */
    if (*s == '\\' || *s == '"' || 
      (!stripCRLFs && 
       (*s == nsCRT::CR && (*(s+1) != nsCRT::LF || 
               (*(s+1) == nsCRT::LF && (s+2) < end && !IS_SPACE(*(s+2)))))))
      *d++ = '\\';

    if (*s == nsCRT::CR)
    {
      if (stripCRLFs && *(s+1) == nsCRT::LF && (s+2) < end && IS_SPACE(*(s+2)))
        s += 2;     // skip CRLFLWSP
    }
    else
    {
      *d++ = *s;
    }
    s++;
  }
  *d = 0;

  return newString;
}

/* Rip apart the URL and extract a reasonable value for the `real_name' slot.
 */
void
msg_pick_real_name (nsMsgAttachmentHandler *attachment, const PRUnichar *proposedName, const char *charset)
{
  const char *s, *s2;
  char *s3;

  if ( (attachment->m_real_name) && (*attachment->m_real_name))
    return;

  if (proposedName && *proposedName)
  {
    attachment->m_real_name = ToNewUTF8String(nsAutoString(proposedName));
  }
  else //Let's extract the name from the URL
  {
    nsXPIDLCString url;
  attachment->mURL->GetSpec(url);

  s = url;
  s2 = PL_strchr (s, ':');
  if (s2) s = s2 + 1;
  /* If we know the URL doesn't have a sensible file name in it,
   don't bother emitting a content-disposition. */
  if (!PL_strncasecmp (url, "news:", 5) ||
    !PL_strncasecmp (url, "snews:", 6) ||
    !PL_strncasecmp (url, "IMAP:", 5) ||
    !PL_strncasecmp (url, "mailbox:", 8))
    return;

  /* Take the part of the file name after the last / or \ */
  s2 = PL_strrchr (s, '/');
  if (s2) s = s2+1;
  s2 = PL_strrchr (s, '\\');
  
  if (s2) s = s2+1;
  /* Copy it into the attachment struct. */
  PR_FREEIF(attachment->m_real_name);
  attachment->m_real_name = PL_strdup (s);
  /* Now trim off any named anchors or search data. */
  s3 = PL_strchr (attachment->m_real_name, '?');
  if (s3) *s3 = 0;
  s3 = PL_strchr (attachment->m_real_name, '#');
  if (s3) *s3 = 0;

  /* Now lose the %XX crap. */
  nsUnescape (attachment->m_real_name);
  }

  /* Now a special case for attaching uuencoded files...

   If we attach a file "foo.txt.uu", we will send it out with
   Content-Type: text/plain; Content-Transfer-Encoding: x-uuencode.
   When saving such a file, a mail reader will generally decode it first
   (thus removing the uuencoding.)  So, let's make life a little easier by
   removing the indication of uuencoding from the file name itself.  (This
   will presumably make the file name in the Content-Disposition header be
   the same as the file name in the "begin" line of the uuencoded data.)

   However, since there are mailers out there (including earlier versions of
   Mozilla) that will use "foo.txt.uu" as the file name, we still need to
   cope with that; the code which copes with that is in the MIME parser, in
   libmime/mimei.c.
   */
  if (attachment->m_already_encoded_p &&
    attachment->m_encoding)
  {
    char *result = attachment->m_real_name;
    PRInt32 L = PL_strlen(result);
    const char **exts = 0;

    /* #### TOTAL KLUDGE.
     I'd like to ask the mime.types file, "what extensions correspond
     to obj->encoding (which happens to be "x-uuencode") but doing that
     in a non-sphagetti way would require brain surgery.  So, since
     currently uuencode is the only content-transfer-encoding which we
     understand which traditionally has an extension, we just special-
     case it here!  

     Note that it's special-cased in a similar way in libmime/mimei.c.
     */
    if (!PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE) ||
      !PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE2) ||
      !PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE3) ||
      !PL_strcasecmp(attachment->m_encoding, ENCODING_UUENCODE4))
    {
      static const char *uue_exts[] = { "uu", "uue", 0 };
      exts = uue_exts;
    }

    while (exts && *exts)
    {
      const char *ext = *exts;
      PRInt32 L2 = PL_strlen(ext);
      if (L > L2 + 1 &&             /* long enough */
        result[L - L2 - 1] == '.' &&      /* '.' in right place*/
        !PL_strcasecmp(ext, result + (L - L2))) /* ext matches */
      {
        result[L - L2 - 1] = 0;   /* truncate at '.' and stop. */
        break;
      }
      exts++;
    }
  }
}

// Utility to create a nsIURI object...
nsresult 
nsMsgNewURL(nsIURI** aInstancePtrResult, const char * aSpec)
{ 
  nsresult rv = NS_OK;
  if (nsnull == aInstancePtrResult) 
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsIIOService> pNetService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv)); 
  if (NS_SUCCEEDED(rv) && pNetService)
  {
    if (PL_strstr(aSpec, "://") == nsnull && strncmp(aSpec, "data:", 5))
    {
      //XXXjag Temporary fix for bug 139362 until the real problem(bug 70083) get fixed
      nsCAutoString uri(NS_LITERAL_CSTRING("http://") + nsDependentCString(aSpec));
      rv = pNetService->NewURI(uri, nsnull, nsnull, aInstancePtrResult);
    }
    else
      rv = pNetService->NewURI(nsDependentCString(aSpec), nsnull, nsnull, aInstancePtrResult);
  }
  return rv;
}

PRBool
nsMsgIsLocalFile(const char *url)
{
  /*
    A url is considered as a local file if it's start with file://
    But on Window, we need to filter UNC file url because there
    are not really local file. Those start with file:////
  */
  if (PL_strncasecmp(url, "file://", 7) == 0)
  {
#ifdef XP_WIN
    if (PL_strncasecmp(url, "file:////", 9) == 0)
      return PR_FALSE;
#endif
    return PR_TRUE;
  }
  else
    return PR_FALSE;
}

char
*nsMsgGetLocalFileFromURL(const char *url)
{
#ifdef XP_MAC
  nsFileURL fileURL(url);
  const char * nativePath = fileURL.GetFileSpec().GetNativePathCString();
  return nsCRT::strdup (nativePath);
#else
  char * finalPath;
  NS_ASSERTION(PL_strncasecmp(url, "file://", 7) == 0, "invalid url");
  finalPath = (char*)PR_Malloc(strlen(url));
  if (finalPath == NULL)
    return NULL;
  strcpy(finalPath, url+6+1);
  return finalPath;
#endif
}

char *
nsMsgPlatformFileToURL (nsFileSpec  aFileSpec)
{
  nsFileURL   tURL(aFileSpec);
  const char  *tPtr = nsnull;

  tPtr = tURL.GetURLString();
  if (tPtr)
    return PL_strdup(tPtr);
  else
    return nsnull;
}

char * 
nsMsgParseURLHost(const char *url)
{
  nsIURI        *workURI = nsnull;
  nsresult      rv;

  rv = nsMsgNewURL(&workURI, url);
  if (NS_FAILED(rv) || !workURI)
    return nsnull;
  
  nsCAutoString host;
  rv = workURI->GetHost(host);
  NS_IF_RELEASE(workURI);
  if (NS_FAILED(rv))
    return nsnull;

  return ToNewCString(host);
}

char *
GenerateFileNameFromURI(nsIURI *aURL)
{
  nsresult    rv; 
  nsXPIDLCString file;
  nsXPIDLCString spec;
  char        *returnString;
  char        *cp = nsnull;
  char        *cp1 = nsnull;

  rv = aURL->GetPath(file);
  if ( NS_SUCCEEDED(rv) && file)
  {
    char *newFile = PL_strdup(file);
    if (!newFile)
      return nsnull;

    // strip '/'
    cp = PL_strrchr(newFile, '/');
    if (cp)
      ++cp;
    else
      cp = newFile;

    if (*cp)
    {
      if ((cp1 = PL_strchr(cp, '/'))) *cp1 = 0;  
      if ((cp1 = PL_strchr(cp, '?'))) *cp1 = 0;  
      if ((cp1 = PL_strchr(cp, '>'))) *cp1 = 0;  
      if (*cp != '\0')
      {
        returnString = PL_strdup(cp);
        PR_FREEIF(newFile);
        return returnString;
      }
    }
    else
      return nsnull;
  }

  cp = nsnull;
  cp1 = nsnull;


  rv = aURL->GetSpec(spec);
  if ( NS_SUCCEEDED(rv) && spec)
  {
    char *newSpec = PL_strdup(spec);
    if (!newSpec)
      return nsnull;

    char *cp2 = NULL, *cp3=NULL ;

    // strip '"' 
    cp2 = newSpec;
    while (*cp2 == '"') 
      cp2++;
    if ((cp3 = PL_strchr(cp2, '"')))
      *cp3 = 0;

    char *hostStr = nsMsgParseURLHost(cp2);
    if (!hostStr)
      hostStr = PL_strdup(cp2);

    PRBool isHTTP = PR_FALSE;
    if (NS_SUCCEEDED(aURL->SchemeIs("http", &isHTTP)) && isHTTP)
    {
        returnString = PR_smprintf("%s.html", hostStr);
        PR_FREEIF(hostStr);
    }
    else
      returnString = hostStr;

    PR_FREEIF(newSpec);
    return returnString;
  }

  return nsnull;
}

//
// This routine will generate a content id for use in a mail part.
// It will take the part number passed in as well as the email 
// address. If the email address is null or invalid, we will simply
// use netscape.com for the interesting part. The content ID's will
// look like the following:
//
//      Content-ID: <part1.36DF1DCE.73B5A330@netscape.com>
//
char *
mime_gen_content_id(PRUint32 aPartNum, const char *aEmailAddress)
{
  PRInt32           randLen = 5;
  unsigned char     rand_buf1[5]; 
  unsigned char     rand_buf2[5]; 
  const char        *domain = nsnull;
  const char        *defaultDomain = "@netscape.com";

  memset(rand_buf1, 0, randLen-1);
  memset(rand_buf2, 0, randLen-1);

  GenerateGlobalRandomBytes(rand_buf1, randLen);
  GenerateGlobalRandomBytes(rand_buf2, randLen);

  // Find the @domain.com string...
  if (aEmailAddress && *aEmailAddress)
    domain = NS_CONST_CAST(const char*, PL_strchr(aEmailAddress, '@'));

  if (!domain)
    domain = defaultDomain;

  char *retVal = PR_smprintf("part%d."
                              "%02X%02X%02X%02X"
                              "."
                              "%02X%02X%02X%02X"
                              "%s",
                              aPartNum,
                              rand_buf1[0], rand_buf1[1], rand_buf1[2], rand_buf1[3],
                              rand_buf2[0], rand_buf2[1], rand_buf2[2], rand_buf2[3],
                              domain);

  return retVal;
}

char *
GetFolderURIFromUserPrefs(nsMsgDeliverMode aMode, nsIMsgIdentity* identity)
{
  nsresult rv;
  char *uri = nsnull;
  
  if (aMode == nsIMsgSend::nsMsgQueueForLater)       // QueueForLater (Outbox)
  {
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv)); 
    if (NS_FAILED(rv)) 
      return nsnull;
    rv = prefs->GetCharPref("mail.default_sendlater_uri", &uri);   
    if (NS_FAILED(rv) || !uri) 
    {
      uri = PR_smprintf("%s", ANY_SERVER);
    }
    else
    {
      // check if uri is unescaped, and if so, escape it and reset the pef.
      if (PL_strchr(uri, ' ') != nsnull)
      {
        nsCAutoString uriStr(uri);
        uriStr.ReplaceSubstring(" ", "%20");
        PR_Free(uri);
        uri = PL_strdup(uriStr.get());
        prefs->SetCharPref("mail.default_sendlater_uri", uriStr.get());
      }
    }
    return uri;
  }

  if (!identity)
    return nsnull;

  if (aMode == nsIMsgSend::nsMsgSaveAsDraft)    // SaveAsDraft (Drafts)
  {
    rv = identity->GetDraftFolder(&uri);
  }
  else if (aMode == nsIMsgSend::nsMsgSaveAsTemplate) // SaveAsTemplate (Templates)
  {
    rv = identity->GetStationeryFolder(&uri);
  }
  else 
  {
    PRBool doFcc = PR_FALSE;
    rv = identity->GetDoFcc(&doFcc);
    if (doFcc)
      rv = identity->GetFccFolder(&uri);
    else
      uri = PL_strdup("");
  }
  return uri;
}

//
// Convert an nsString buffer to plain text...
//
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIHTMLToTextSink.h"
#include "nsIContentSink.h"
#include "nsICharsetConverterManager.h"

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID); 

/**
 * Converts a buffer to plain text. Some conversions may
 * or may not work with certain end charsets which is why we
 * need that as an argument to the function. If charset is
 * unknown or deemed of no importance NULL could be passed.
 */
nsresult
ConvertBufToPlainText(nsString &aConBuf, PRBool formatflowed /* = PR_FALSE */)
{
  if (aConBuf.IsEmpty())
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &rv);
  if (NS_SUCCEEDED(rv) && parser)
  {
    PRUint32 converterFlags = 0;
    PRUint32 wrapWidth = 72;
    
    converterFlags |= nsIDocumentEncoder::OutputFormatted;

    /*
    */
    if(formatflowed)
      converterFlags |= nsIDocumentEncoder::OutputFormatFlowed;
    
    nsCOMPtr<nsIContentSink> sink;

    sink = do_CreateInstance(NS_PLAINTEXTSINK_CONTRACTID);
    NS_ENSURE_TRUE(sink, NS_ERROR_FAILURE);

    nsCOMPtr<nsIHTMLToTextSink> textSink(do_QueryInterface(sink));
    NS_ENSURE_TRUE(textSink, NS_ERROR_FAILURE);

    nsString convertedText;
    textSink->Initialize(&convertedText, converterFlags, wrapWidth);

    parser->SetContentSink(sink);

    parser->Parse(aConBuf, 0, NS_LITERAL_CSTRING("text/html"), PR_TRUE);
    //
    // Now if we get here, we need to get from ASCII text to 
    // UTF-8 format or there is a problem downstream...
    //
    if (NS_SUCCEEDED(rv))
    {
      aConBuf = convertedText;
    }
  }

  return rv;
}

// Simple parser to parse Subject. 
// It only supports the case when the description is within one line. 
char * 
nsMsgParseSubjectFromFile(nsFileSpec* fileSpec) 
{ 
  nsIFileSpec   *tmpFileSpec = nsnull;
  char          *subject = nsnull;
  char          buffer[1024];
  char          *ptr = &buffer[0];

  NS_NewFileSpecWithSpec(*fileSpec, &tmpFileSpec);
  if (!tmpFileSpec)
    return nsnull;

  if (NS_FAILED(tmpFileSpec->OpenStreamForReading()))
    return nsnull;

  PRBool eof = PR_FALSE;

  while ( NS_SUCCEEDED(tmpFileSpec->Eof(&eof)) && (!eof) )
  {
    PRBool wasTruncated = PR_FALSE;

    if (NS_FAILED(tmpFileSpec->ReadLine(&ptr, sizeof(buffer), &wasTruncated)))
      break;

    if (wasTruncated)
      continue;

    if (*buffer == nsCRT::CR || *buffer == nsCRT::LF || *buffer == 0) 
      break; 

    if ( !PL_strncasecmp(buffer, "Subject: ", 9) )
    { 
      subject = nsCRT::strdup(buffer + 9);
      break;
    } 
  } 

  tmpFileSpec->CloseStream();
  return subject; 
}

/**
 * Check if we should use format=flowed (RFC 2646) for a mail.
 *
 * We will use format=flowed unless prefs tells us not to do
 * or if a charset which are known to have problems with
 * format=flowed is specified. (See bug 26734 in Bugzilla)
 */
PRBool UseFormatFlowed(const char *charset)
{
  // Add format=flowed as in RFC 2646 unless asked to not do that.
  PRBool sendFlowed = PR_TRUE;
  PRBool disableForCertainCharsets = PR_TRUE;
  nsresult rv;

  nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return PR_FALSE;

  rv = prefs->GetBoolPref("mailnews.send_plaintext_flowed", &sendFlowed);
  if (NS_SUCCEEDED(rv) && !sendFlowed)
    return PR_FALSE;

  // If we shouldn't care about charset, then we are finished
  // checking and can go on using format=flowed
  if(!charset)
    return PR_TRUE;
  rv = prefs->GetBoolPref("mailnews.disable_format_flowed_for_cjk",
                          &disableForCertainCharsets);
  if (NS_SUCCEEDED(rv) && !disableForCertainCharsets)
    return PR_TRUE;

  // Just the check for charset left.

  // This is a raw check and might include charsets which could
  // use format=flowed and might exclude charsets which couldn't
  // use format=flowed.
  //
  // The problem is the SPACE format=flowed inserts at the end of
  // the line. Not all charsets like that.
  if( nsCRT::strcasecmp(charset, "UTF-8") &&
      nsMsgI18Nmultibyte_charset(charset))
    return PR_FALSE;

  return PR_TRUE;
}
