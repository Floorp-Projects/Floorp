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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsMsgCompUtils.h"
#include "nsIPref.h"
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
#include "nsSpecialSystemDirectory.h"
#include "nsIDocumentEncoder.h"    // for editor output flags
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsMsgPrompts.h"
#include "nsMsgUtils.h"
#include "nsMsgSimulateError.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID); 
static NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kHTTPHandlerCID, NS_HTTPPROTOCOLHANDLER_CID);

//
// Hopefully, someone will write and XP call like this eventually!
//
#define     TPATH_LEN   1024

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
//
nsFileSpec * 
nsMsgCreateTempFileSpec(char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  nsFileSpec *tmpSpec = new nsFileSpec(nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory));

  if (!tmpSpec)
    return nsnull;

  *tmpSpec += tFileName;
  tmpSpec->MakeUnique();

  return tmpSpec;
}

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
// returned
//
char * 
nsMsgCreateTempFileName(char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  nsFileSpec tmpSpec = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory); 
  tmpSpec += tFileName;
  tmpSpec.MakeUnique();

  char *tString = (char *)PL_strdup(tmpSpec.GetNativePathCString());
  if (!tString)
    return PL_strdup("mozmail.tmp");  // No need to I18N
  else
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
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
    if (NS_SUCCEEDED(rv) && prefs) {
      rv = prefs->GetBoolPref("mail.strictly_mime_headers", &mime_headers_use_quoted_printable_p);
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

char * 
mime_generate_headers (nsMsgCompFields *fields,
                       const char *charset,
                       nsMsgDeliverMode deliver_mode, nsIPrompt * aPrompt, PRInt32 *status)
{
  nsresult rv;
  *status = 0;

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
  if (NS_FAILED(rv)) {
  *status = rv;
    return nsnull;
  }

  int size = 0;
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

  nsCAutoString headerBuf;    // accumulate header strings for charset conversion check
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

  const char *pBcc = fields->GetBcc();
  if (pBcc)
    headerBuf.Append(pBcc);
  // alert the user if headers contain characters out of charset range (e.g. multilingual data)
  if (!nsMsgI18Ncheck_data_in_charset_range(charset, NS_ConvertUTF8toUCS2(headerBuf.get()).get())) {
    PRBool proceedTheSend;
    rv = nsMsgAskBooleanQuestionByID(aPrompt, NS_MSG_MULTILINGUAL_SEND, &proceedTheSend);
    if (!proceedTheSend) {
      *status = NS_ERROR_ABORT;
      return nsnull;
    }
  }

  /* Add a bunch of slop for the static parts of the headers. */
  /* size += 2048; */
  size += 2560;

  buffer = (char *) PR_Malloc (size);
  if (!buffer)
    return nsnull; /* NS_ERROR_OUT_OF_MEMORY */
  
  buffer_tail = buffer;

  if (pMessageID && *pMessageID) {
    char *convbuf = NULL;
    PUSH_STRING ("Message-ID: ");
    PUSH_STRING (pMessageID);
    PUSH_NEWLINE ();
    /* MDN request header requires to have MessageID header presented
    * in the message in order to
    * coorelate the MDN reports to the original message. Here will be
    * the right place
    */
    if (fields->GetReturnReceipt() && 
      (fields->GetReturnReceiptType() == 2 ||
      fields->GetReturnReceiptType() == 3) && 
      (deliver_mode != nsIMsgSend::nsMsgSaveAsDraft &&
      deliver_mode != nsIMsgSend::nsMsgSaveAsTemplate))
    {
      PRInt32 receipt_header_type = 0;

      if (prefs) 
        prefs->GetIntPref("mail.receipt.request_header_type", &receipt_header_type);

      // 0 = MDN Disposition-Notification-To: ; 1 = Return-Receipt-To: ; 2 =
      // both MDN DNT & RRT headers
      if (receipt_header_type == 1) {
        RRT_HEADER:
        PUSH_STRING ("Return-Receipt-To: ");
        convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pFrom, charset, nsMsgMIMEGetConformToStandard());
        if (convbuf) {    /* MIME-PartII conversion */
          PUSH_STRING (convbuf);
          PR_FREEIF(convbuf);
        }
        else
          PUSH_STRING (pFrom);
        PUSH_NEWLINE ();
      }
      else  {
        PUSH_STRING ("Disposition-Notification-To: ");
        convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pFrom, charset,
                        nsMsgMIMEGetConformToStandard());
        if (convbuf) {     /* MIME-PartII conversion */
          PUSH_STRING (convbuf);
          PR_FREEIF(convbuf);
        }
        else
          PUSH_STRING (pFrom);
        PUSH_NEWLINE ();
        if (receipt_header_type == 2)
          goto RRT_HEADER;
      }
    }
#ifdef SUPPORT_X_TEMPLATE_NAME
    if (deliver_mode == MSG_SaveAsTemplate) {
      PUSH_STRING ("X-Template: ");
      char * pStr = fields->GetTemplateName();
      if (pStr) {
        convbuf = nsMsgI18NEncodeMimePartIIStr((char *)
                   pStr,
                   charset,
                   nsMsgMIMEGetConformToStandard());
        if (convbuf) {
          PUSH_STRING (convbuf);
          PR_FREEIF(convbuf);
        }
        else
          PUSH_STRING(pStr);
      }
      PUSH_NEWLINE ();
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

  if (pFrom && *pFrom) {
    char *convbuf;
    PUSH_STRING ("From: ");
    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pFrom, charset,
                    nsMsgMIMEGetConformToStandard());

    if (convbuf) {    /* MIME-PartII conversion */
      PUSH_STRING (convbuf);
      PR_Free(convbuf);
    }
    else
      PUSH_STRING (pFrom);
    PUSH_NEWLINE ();
  }

  if (pReplyTo && *pReplyTo)
  {
    char *convbuf;
    PUSH_STRING ("Reply-To: ");
    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pReplyTo, charset,
                    nsMsgMIMEGetConformToStandard());
    if (convbuf) {     /* MIME-PartII conversion */
      PUSH_STRING (convbuf);
      PR_Free(convbuf);
    }
    else
      PUSH_STRING (pReplyTo);
    PUSH_NEWLINE ();
  }

  if (pOrg && *pOrg)
  {
    char *convbuf;
    PUSH_STRING ("Organization: ");
    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pOrg, charset,
                    nsMsgMIMEGetConformToStandard());
    if (convbuf) {     /* MIME-PartII conversion */
      PUSH_STRING (convbuf);
      PR_Free(convbuf);
    }
    else
      PUSH_STRING (pOrg);
    PUSH_NEWLINE ();
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
      char *type = PR_smprintf("%d", (int) fields->GetReturnReceiptType());
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


  nsCOMPtr<nsIHttpProtocolHandler> pHTTPHandler = 
           do_GetService(kHTTPHandlerCID, &rv); 
  if (NS_SUCCEEDED(rv) && pHTTPHandler)
  {
        nsXPIDLCString userAgentString;
        pHTTPHandler->GetUserAgent(getter_Copies(userAgentString));

    if (userAgentString)
    {
      // PUSH_STRING ("X-Mailer: ");  // To be more standards compliant
      PUSH_STRING ("User-Agent: ");  
      PUSH_STRING(userAgentString.get());
      PUSH_NEWLINE ();
    }
  }

  /* for Netscape Server, Accept-Language data sent in Mail header */
  char *acceptlang = nsMsgI18NGetAcceptLanguage();
  if( (acceptlang != NULL) && ( *acceptlang != '\0') ){
    PUSH_STRING( "X-Accept-Language: " );
    PUSH_STRING( acceptlang );
    PUSH_NEWLINE();
  }

  PUSH_STRING ("MIME-Version: 1.0" CRLF);

  if (pNewsGrp && *pNewsGrp) {
    /* turn whitespace into a comma list
    */
    char *ptr, *ptr2;
    char *n2;
    char *convbuf;

    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pNewsGrp, charset,
                    nsMsgMIMEGetConformToStandard());
    if (convbuf)
        n2 = nsMsgStripLine (convbuf);
    else {
      ptr = PL_strdup(pNewsGrp);
      if (!ptr) {
        PR_FREEIF(buffer);
        return nsnull; /* NS_ERROR_OUT_OF_MEMORY */
      }
          n2 = nsMsgStripLine(ptr);
      NS_ASSERTION(n2 == ptr, "n2 != ptr"); /* Otherwise, the PR_Free below is
                             gonna choke badly. */
    }

    for(ptr=n2; *ptr != '\0'; ptr++) {
      /* find first non white space */
      while(!IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
        ptr++;

      if(*ptr == '\0')
        break;

      if(*ptr != ',')
        *ptr = ',';

      /* find next non white space */
      ptr2 = ptr+1;
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

    PUSH_STRING ("Newsgroups: ");
    PUSH_STRING (newsgroupsHeaderVal.get());
    PUSH_NEWLINE ();

    // If we are here, we are NOT going to send this now. (i.e. it is a Draft, 
    // Send Later file, etc...). Because of that, we need to store what the user
    // typed in on the original composition window for use later when rebuilding
    // the headers
    if (deliver_mode != nsIMsgSend::nsMsgDeliverNow)
    {
      // This is going to be saved for later, that means we should just store
      // what the user typed into the "Newsgroup" line in the HEADER_X_MOZILLA_NEWSHOST
      // header for later use by "Send Unsent Messages", "Drafts" or "Templates"
      PUSH_STRING (HEADER_X_MOZILLA_NEWSHOST);
      PUSH_STRING (": ");
      PUSH_STRING (newshostHeaderVal.get());
      PUSH_NEWLINE ();
    }

    PR_FREEIF(n2);
  }

  /* #### shamelessly duplicated from above */
  if (pFollow && *pFollow) {
    /* turn whitespace into a comma list
    */
    char *ptr, *ptr2;
    char *n2;
    char *convbuf;

    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pFollow, charset,
                    nsMsgMIMEGetConformToStandard());
    if (convbuf)
      n2 = nsMsgStripLine (convbuf);
    else {
      ptr = PL_strdup(pFollow);
      if (!ptr) {
        PR_FREEIF(buffer);
        return nsnull; /* NS_ERROR_OUT_OF_MEMORY */
      }
      n2 = nsMsgStripLine (ptr);
      NS_ASSERTION(n2 == ptr, "n2 != ptr"); /* Otherwise, the PR_Free below is
                             gonna choke badly. */
    }

    for (ptr=n2; *ptr != '\0'; ptr++) {
      /* find first non white space */
      while(!IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
        ptr++;

      if(*ptr == '\0')
        break;

      if(*ptr != ',')
        *ptr = ',';

      /* find next non white space */
      ptr2 = ptr+1;
      while(IS_SPACE(*ptr2))
        ptr2++;

      if(ptr2 != ptr+1)
        PL_strcpy(ptr+1, ptr2);
    }

    PUSH_STRING ("Followup-To: ");
    PUSH_STRING (n2);
    PR_Free (n2);
    PUSH_NEWLINE ();
  }

  if (pTo && *pTo) {
    char *convbuf;
    PUSH_STRING ("To: ");
    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pTo, charset,
                    nsMsgMIMEGetConformToStandard());
    if (convbuf) {     /* MIME-PartII conversion */
      PUSH_STRING (convbuf);
      PR_Free(convbuf);
    }
    else
      PUSH_STRING (pTo);

    PUSH_NEWLINE ();
  }
 
  if (pCc && *pCc) {
    char *convbuf;
    PUSH_STRING ("CC: ");
    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pCc, charset,
                    nsMsgMIMEGetConformToStandard());
    if (convbuf) {   /* MIME-PartII conversion */
      PUSH_STRING (convbuf);
      PR_Free(convbuf);
    }
    else
      PUSH_STRING (pCc);
    PUSH_NEWLINE ();
  }

  if (pSubject && *pSubject) {

    char *convbuf;
    PUSH_STRING ("Subject: ");
    convbuf = nsMsgI18NEncodeMimePartIIStr((char *)pSubject, charset,
                    nsMsgMIMEGetConformToStandard());
    if (convbuf) {  /* MIME-PartII conversion */
      PUSH_STRING (convbuf);
      PR_Free(convbuf);
    }
    else
      PUSH_STRING (pSubject);
    PUSH_NEWLINE ();
  }
  
  if (pPriority && *pPriority)
    if (!PL_strcasestr(pPriority, "normal")) {
      PUSH_STRING ("X-Priority: ");
      /* Important: do not change the order of the 
      * following if statements
      */
      if (PL_strcasestr (pPriority, "highest"))
        PUSH_STRING("1 (");
      else if (PL_strcasestr(pPriority, "high"))
        PUSH_STRING("2 (");
      else if (PL_strcasestr(pPriority, "lowest"))
        PUSH_STRING("5 (");
      else if (PL_strcasestr(pPriority, "low"))
        PUSH_STRING("4 (");

      PUSH_STRING (pPriority);
      PUSH_STRING(")");
      PUSH_NEWLINE ();
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
          nsCRT::memmove(trimAt, ptr, PL_strlen(ptr)+1); // including the
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
  }

  if (pOtherHdr && *pOtherHdr) {
    /* Assume they already have the right newlines and continuations
     and so on. */
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

char * 
mime_generate_attachment_headers (const char *type, const char *encoding,
                  const char *description,
                  const char *x_mac_type,
                  const char *x_mac_creator,
                  const char *real_name,
                  const char *base_url,
                  PRBool /*digest_p*/,
                  nsMsgAttachmentHandler * /*ma*/,
                  const char *charset,
                  const char *content_id, 
                  PRBool      aBodyDocument)
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 

  PRInt32 buffer_size = 2048 + (base_url ? 2*PL_strlen(base_url) : 0);
  char *buffer = (char *) PR_Malloc (buffer_size);
  char *buffer_tail = buffer;

  if (! buffer)
    return 0; /* NS_ERROR_OUT_OF_MEMORY */

  NS_ASSERTION (encoding, "null encoding");

  PRInt32 parmFolding = 0;
  if (NS_SUCCEEDED(rv) && prefs) 
    prefs->GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

  /* Let's encode the real name */
  char *encodedRealName = nsnull;
  if (real_name)
  {
    encodedRealName = nsMsgI18NEncodeMimePartIIStr(real_name,
      NS_ConvertUCS2toUTF8(nsMsgI18NFileSystemCharset()).get(), nsMsgMIMEGetConformToStandard());
    if (!encodedRealName || !*encodedRealName)
    {
      /* Let's try one more time using UTF8 */
        encodedRealName = nsMsgI18NEncodeMimePartIIStr(real_name, "UTF-8", nsMsgMIMEGetConformToStandard());
      if (!encodedRealName || !*encodedRealName)
      {
        PR_FREEIF(encodedRealName);
        encodedRealName = PL_strdup(real_name);
      }
    }

    /* ... and then put backslashes before special characters (RFC 822 tells us to). */
    char *qtextName = msg_make_filename_qtext(encodedRealName, (parmFolding == 0 ? PR_TRUE : PR_FALSE));    
    if (qtextName)
    {
      PR_FREEIF(encodedRealName);
      encodedRealName = qtextName;
    }
  }

  PUSH_STRING ("Content-Type: ");
  PUSH_STRING (type);

  if (mime_type_needs_charset (type)) 
  {
    
    char charset_label[65] = "";   // Content-Type: charset
    PL_strcpy(charset_label, charset);
    
    /* If the characters are all 7bit, then it's better (and true) to
    claim the charset to be US-  rather than Latin1.  Should we
    do this all the time, for all charsets?  I'm not sure.  But we
    should definitely do it for Latin1. */
    if (encoding &&
                  !PL_strcasecmp (encoding, "7bit") &&
                  !PL_strcasecmp (charset, "iso-8859-1"))
      PL_strcpy (charset_label, "us-ascii");
    
    // If charset is multibyte then no charset to be specified (apply base64 instead).
    // The list of file types match with PickEncoding() where we put base64 label.
    if ( ((charset && !nsMsgI18Nmultibyte_charset(charset)) ||
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
      PUSH_STRING ("; charset=");
      PUSH_STRING (charset_label);
    }
  }

  // Only do this if we are in the body of a message
  if (aBodyDocument)
  {
    // Add format=flowed as in RFC 2646 if we are using that
    if(type && !PL_strcasecmp(type, "text/plain"))
    {
      if(UseFormatFlowed(charset))
        PUSH_STRING ("; format=flowed");
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
    PUSH_STRING ("; x-mac-type=\"");
    PUSH_STRING (x_mac_type);
    PUSH_STRING ("\"");
  }

  if (x_mac_creator && *x_mac_creator) {
    PUSH_STRING ("; x-mac-creator=\"");
    PUSH_STRING (x_mac_creator);
    PUSH_STRING ("\"");
  }

#ifdef EMIT_NAME_IN_CONTENT_TYPE
  if (encodedRealName && *encodedRealName) {
    if (parmFolding == 0 || parmFolding == 1) {
      PUSH_STRING (";\r\n name=\"");
      PUSH_STRING (encodedRealName);
      PUSH_STRING ("\"");
    }
    else // if (parmFolding == 2)
    {
      char *rfc2231Parm = RFC2231ParmFolding("name", charset,
                         nsMsgI18NGetAcceptLanguage(), encodedRealName);
      if (rfc2231Parm) {
        PUSH_STRING(";\r\n ");
        PUSH_STRING(rfc2231Parm);
        PR_Free(rfc2231Parm);
      }
    }
  }
#endif /* EMIT_NAME_IN_CONTENT_TYPE */

  PUSH_NEWLINE ();

  PUSH_STRING ("Content-Transfer-Encoding: ");
  PUSH_STRING (encoding);
  PUSH_NEWLINE ();

  if (description && *description) {
    char *s = mime_fix_header (description);
    if (s) {
      PUSH_STRING ("Content-Description: ");
      PUSH_STRING (s);
      PUSH_NEWLINE ();
      PR_Free(s);
    }
  }

  if ( (content_id) && (*content_id) )
  {
    PUSH_STRING ("Content-ID: <");
    PUSH_STRING (content_id);
    PUSH_STRING (">");
    PUSH_NEWLINE ();
  }

  if (encodedRealName && *encodedRealName) {
    char *period = PL_strrchr(encodedRealName, '.');
    PRInt32 pref_content_disposition = 0;

        rv = prefs->GetIntPref("mail.content_disposition_type", &pref_content_disposition);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get mail.content_disposition_type");

    PUSH_STRING ("Content-Disposition: ");

    if (pref_content_disposition == 1)
      PUSH_STRING ("attachment");
    else
      if (pref_content_disposition == 2 && 
          (!PL_strcasecmp(type, TEXT_PLAIN) || 
          (period && !PL_strcasecmp(period, ".txt"))))
        PUSH_STRING("attachment");

      /* If this document is an anonymous binary file or a vcard, 
      then always show it as an attachment, never inline. */
      else
        if (!PL_strcasecmp(type, APPLICATION_OCTET_STREAM) || 
            !PL_strcasecmp(type, TEXT_VCARD) ||
            !PL_strcasecmp(type, APPLICATION_DIRECTORY)) /* text/x-vcard synonym */
          PUSH_STRING ("attachment");
        else
          PUSH_STRING ("inline");

    if (parmFolding == 0 || parmFolding == 1) {
      PUSH_STRING (";\r\n filename=\"");
      PUSH_STRING (encodedRealName);
      PUSH_STRING ("\"" CRLF);
    }
    else // if (parmFolding == 2)
    {
      char *rfc2231Parm = RFC2231ParmFolding("filename", charset,
                       nsMsgI18NGetAcceptLanguage(), encodedRealName);
      if (rfc2231Parm) {
        PUSH_STRING(";\r\n ");
        PUSH_STRING(rfc2231Parm);
        PUSH_NEWLINE ();
        PR_Free(rfc2231Parm);
      }
    }
  }
  else
    if (type &&
        (!PL_strcasecmp (type, MESSAGE_RFC822) ||
        !PL_strcasecmp (type, MESSAGE_NEWS)))
      PUSH_STRING ("Content-Disposition: inline" CRLF);
  
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
    if (NS_SUCCEEDED(rv) && prefs) 
      prefs->GetBoolPref("mail.use_content_location_on_send", &useContentLocation);

    if (useContentLocation)
      PUSH_STRING ("Content-Location: \"");
    else
      PUSH_STRING ("Content-Base: \"");
    /* rhp - Pref for Content-Location usage */

/* rhp: this is to work with the Content-Location stuff */
CONTENT_LOC_HACK:

    while (*s != 0 && *s != '#')
    {
      const char *ot = buffer_tail;

      /* URLs must be wrapped at 40 characters or less. */
      if (col >= 38) {
        PUSH_STRING(CRLF "\t");
        col = 0;
      }

      if (*s == ' ')
        PUSH_STRING("%20");
      else if (*s == '\t')
        PUSH_STRING("%09");
      else if (*s == '\n')
        PUSH_STRING("%0A");
      else if (*s == '\r')
        PUSH_STRING("%0D");
      else {
        *buffer_tail++ = *s;
        *buffer_tail = '\0';
      }
      s++;
      col += (buffer_tail - ot);
    }
    PUSH_STRING ("\"" CRLF);

    /* rhp: this is to try to get around this fun problem with Content-Location */
    if (!useContentLocation) {
      PUSH_STRING ("Content-Location: \"");
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
  buffer = (char*) PR_REALLOC (buffer, buffer_tail - buffer + 1);

  PR_FREEIF(encodedRealName);
  return buffer;
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
  
  nsXPIDLCString from;
  nsresult rv = identity->GetEmail(getter_Copies(from));
  if (NS_FAILED(rv)) return nsnull;

  GenerateGlobalRandomBytes((unsigned char *) &salt, sizeof(salt));
  if (from) {
    host = PL_strchr (from, '@');
    if (host) {
      const char *s;
      for (s = ++host; *s; s++)
        if (!nsCRT::IsAsciiAlpha(*s) && !nsCRT::IsAsciiDigit(*s) &&
            *s != '-' && *s != '_' && *s != '.') {
          host = 0;
          break;
        }
    }
  }

  if (! host)
  /* If we couldn't find a valid host name to use, we can't generate a
     valid message ID, so bail, and let NNTP and SMTP generate them. */
    return 0;

  return PR_smprintf("<%lX.%lX@%s>",
           (unsigned long) now, (unsigned long) salt, host);
}

char *
RFC2231ParmFolding(const char *parmName, const char *charset, 
           const char *language, const char *parmValue)
{
#define PR_MAX_FOLDING_LEN 75     // this is to gurantee the folded line will
                                  // never be greater than 78 = 75 + CRLFLWSP
  char *foldedParm = NULL;
  char *dupParm = NULL;
  PRInt32 parmNameLen = 0;
  PRInt32 parmValueLen = 0;
  PRInt32 charsetLen = 0;
  PRInt32 languageLen = 0;
  PRBool needEscape = PR_FALSE;

  NS_ASSERTION(parmName && *parmName && parmValue && *parmValue, "null parameters");
  if (!parmName || !*parmName || !parmValue || !*parmValue)
    return NULL;
  if ((charset && *charset && PL_strcasecmp(charset, "us-ascii") != 0) || 
    (language && *language && PL_strcasecmp(language, "en") != 0 &&
     PL_strcasecmp(language, "en-us") != 0))
    needEscape = PR_TRUE;

  if (needEscape)
    dupParm = nsEscape(parmValue, url_Path);
  else 
    dupParm = nsCRT::strdup(parmValue);

  if (!dupParm)
    return NULL;

  if (needEscape)
  {
    parmValueLen = PL_strlen(dupParm);
    parmNameLen = PL_strlen(parmName);
  }

  if (needEscape)
    parmNameLen += 5;   // *=__'__'___ or *[0]*=__'__'__ or *[1]*=___
  else
    parmNameLen += 5;   // *[0]="___";
  charsetLen = charset ? PL_strlen(charset) : 0;
  languageLen = language ? PL_strlen(language) : 0;

  if ((parmValueLen + parmNameLen + charsetLen + languageLen) <
    PR_MAX_FOLDING_LEN)
  {
    foldedParm = PL_strdup(parmName);
    if (needEscape)
    {
      NS_MsgSACat(&foldedParm, "*=");
      if (charsetLen)
        NS_MsgSACat(&foldedParm, charset);
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
        if (needEscape)
          NS_MsgSACat(&foldedParm, "\r\n ");
        else
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
            NS_MsgSACat(&foldedParm, charset);
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
  nsCRT::free(dupParm);
  return foldedParm;
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
    nsCOMPtr<nsIMsgHeaderParser> pHeader = do_GetService(kMsgHeaderParserCID, &rv);
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
    "application/x-pgp-message",  /* APPLICATION_PGP2 */
    "application/postscript",   /* APPLICATION_POSTSCRIPT */
    "application/x-uuencode",   /* APPLICATION_UUENCODE */
    "application/x-uue",      /* APPLICATION_UUENCODE2 */
    "application/uue",        /* APPLICATION_UUENCODE4 */
    "application/uuencode",     /* APPLICATION_UUENCODE3 */
    "application/sgml",
    "application/x-csh",
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
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
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
  attachment->mURL->GetSpec(getter_Copies(url));

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

  PRInt32 parmFolding = 0;
  if (NS_SUCCEEDED(rv) && prefs) 
    prefs->GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

  if (parmFolding == 0 || parmFolding == 1)
    if (!proposedName || !(*proposedName))
  {
    /* Convert to unicode */
    nsAutoString uStr;
    rv = ConvertToUnicode(nsMsgI18NFileSystemCharset(), attachment->m_real_name, uStr);
    if (NS_FAILED(rv)) 
      uStr.AssignWithConversion(attachment->m_real_name);

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
  nsCOMPtr<nsIIOService> pNetService(do_GetService(kIOServiceCID, &rv)); 
  if (NS_SUCCEEDED(rv) && pNetService)
  {
    if (PL_strstr(aSpec, "://") == nsnull)
    {
      nsAutoString newSpec; newSpec.AssignWithConversion("http://");
      newSpec.AppendWithConversion(aSpec);
      nsCAutoString newspecC;
      newspecC.AssignWithConversion(newSpec);
		  rv = pNetService->NewURI(newspecC.get(), nsnull, aInstancePtrResult);
  	}
  	else
		  rv = pNetService->NewURI(aSpec, nsnull, aInstancePtrResult);
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
  char          *retVal = nsnull;
  nsresult      rv;

  rv = nsMsgNewURL(&workURI, url);
  if (NS_FAILED(rv) || !workURI)
    return nsnull;
  
  rv = workURI->GetHost(&retVal);
  NS_IF_RELEASE(workURI);
  if (NS_FAILED(rv))
    return nsnull;
  else
    return retVal;
}

char *
GetFolderNameFromURLString(char *url)
{
  char *rightSlash = nsnull;

  if (!url)
    return nsnull;

  rightSlash  = PL_strrchr(url, '/');
  if (!rightSlash)
    rightSlash = PL_strrchr(url, '\\');
   
  if (rightSlash)
  {
    char *retVal = PL_strdup(rightSlash + 1);
    return retVal;
  }

  return nsnull;
}

char *
GenerateFileNameFromURI(nsIURI *aURL)
{
  nsresult    rv; 
  nsXPIDLCString file;
  nsXPIDLCString spec;;
  char        *returnString;
  char        *cp = nsnull;
  char        *cp1 = nsnull;

  rv = aURL->GetPath(getter_Copies(file));
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


  rv = aURL->GetSpec(getter_Copies(spec));
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
  char              *domain = nsnull;
  char              *defaultDomain = "@netscape.com";

  nsCRT::memset(rand_buf1, 0, randLen-1);
  nsCRT::memset(rand_buf2, 0, randLen-1);

  GenerateGlobalRandomBytes(rand_buf1, randLen);
  GenerateGlobalRandomBytes(rand_buf2, randLen);

  // Find the @domain.com string...
  if (aEmailAddress && *aEmailAddress)
    domain = PL_strchr(aEmailAddress, '@');

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
GetFolderURIFromUserPrefs(nsMsgDeliverMode   aMode,
                          nsIMsgIdentity* identity)
{
  nsresult      rv = NS_OK;
  char          *uri = nsnull;

  if (!identity) return nsnull;

  if (aMode == nsIMsgSend::nsMsgQueueForLater)       // QueueForLater (Outbox)
  {
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv)); 
    if (NS_FAILED(rv) || !prefs) 
      return nsnull;
    rv = prefs->CopyCharPref("mail.default_sendlater_uri", &uri);
   
    if (NS_FAILED(rv) || !uri) {
  uri = PR_smprintf("%s", ANY_SERVER);
  rv = NS_OK;
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
  }
  else if (aMode == nsIMsgSend::nsMsgSaveAsDraft)    // SaveAsDraft (Drafts)
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
// Find an extension in a URL
char *
nsMsgGetExtensionFromFileURL(nsString aUrl)
{
  char *url = nsnull;
  char *rightDot = nsnull;
  char *rightSlash = nsnull;

  if (aUrl.IsEmpty())
    return nsnull;

  url = ToNewCString(aUrl);
  if (!url)
    goto ERROR_OUT;

  rightDot = PL_strrchr(url, '.');
  if (!rightDot)
    goto ERROR_OUT;

  rightSlash  = PL_strrchr(url, '/');
  if (!rightSlash)
    rightSlash = PL_strrchr(url, '\\');
   
  if (!rightSlash)
    goto ERROR_OUT;

  if (rightDot > rightSlash)
  {
    if (rightDot+1 == '\0')
      goto ERROR_OUT;

    char *retVal = PL_strdup(rightDot + 1);
    PR_FREEIF(url);
    return retVal;
  }

ERROR_OUT:
  if (url)
  nsMemory::Free(url);
  return nsnull;
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
  nsresult    rv;
  nsString    convertedText;
  nsCOMPtr<nsIParser> parser;

  if (aConBuf.IsEmpty())
    return NS_OK;

  rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, 
                                          NS_GET_IID(nsIParser), getter_AddRefs(parser));
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

    textSink->Initialize(&convertedText, converterFlags, wrapWidth);

    parser->SetContentSink(sink);

    nsAutoString contentType; contentType = NS_LITERAL_STRING("text/html");
    parser->Parse(aConBuf, 0, contentType, PR_FALSE, PR_TRUE);
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
 * format=flowed is specifed. (See bug 26734 in Bugzilla)
 */
PRBool UseFormatFlowed(const char *charset)
{
  // Add format=flowed as in RFC 2646 unless asked to not do that.
  PRBool sendFlowed = PR_TRUE;
  PRBool disableForCertainCharsets = PR_TRUE;
  nsresult rv;

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
  if (NS_FAILED(rv))
    return PR_FALSE;

  if(prefs)
  {
    
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
  }
  else
  {
    // No prefs service. Be careful. Don't use format=flowed.
    return PR_FALSE;
  }

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

