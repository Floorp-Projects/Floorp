/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsMsgCompUtils.h"
#include "nsIPref.h"
#include "prmem.h"
#include "nsMsgSend.h"
#include "nsIIOService.h"
#include "nsMailHeaders.h"
#include "nsMsgI18N.h"
#include "nsIMsgHeaderParser.h"
#include "nsIMimeURLUtils.h"
#include "nsINntpService.h"
#include "nsMsgNewsCID.h"
#include "nsMimeTypes.h"
#include "nsMsgComposeStringBundle.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID); 
static NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID); 
static NS_DEFINE_CID(kMimeURLUtilsCID, NS_IMIME_URLUTILS_CID);
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

//
// Hopefully, someone will write and XP call like this eventually!
//
#define     TPATH_LEN   1024

#ifdef WIN32
#include "windows.h"
#endif

static char *
GetTheTempDirectoryOnTheSystem(void)
{
  char *retPath = (char *)PR_Malloc(TPATH_LEN);
  if (!retPath)
    return nsnull;

  retPath[0] = '\0';
#ifdef WIN32
  if (getenv("TEMP"))
    PL_strncpy(retPath, getenv("TEMP"), TPATH_LEN);  // environment variable
  else if (getenv("TMP"))
    PL_strncpy(retPath, getenv("TMP"), TPATH_LEN);   // How about this environment variable?
  else
    GetWindowsDirectory(retPath, TPATH_LEN);
#endif 

  // RICHIE - should do something better here!

#if defined(XP_UNIX) || defined(XP_BEOS)
  char *tPath = getenv("TMPDIR");
  if (!tPath)
    PL_strncpy(retPath, "/tmp/", TPATH_LEN);
  else
    PL_strncpy(retPath, tPath, TPATH_LEN);
#endif

#ifdef XP_MAC
  PL_strncpy(retPath, "", TPATH_LEN);
#endif

  return retPath;
}

//
// Create a file spec for the a unique temp file
// on the local machine. Caller must free memory
//
nsFileSpec * 
nsMsgCreateTempFileSpec(char *tFileName)
{
  if ((!tFileName) || (!*tFileName))
    tFileName = "nsmail.tmp";

  // Age old question, where to store temp files....ugh!
  char  *tDir = GetTheTempDirectoryOnTheSystem();
  if (!tDir)
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N

  nsFileSpec *tmpSpec = new nsFileSpec(tDir);
  if (!tmpSpec)
  {
    PR_FREEIF(tDir);
    return (new nsFileSpec("mozmail.tmp"));  // No need to I18N
  }

  *tmpSpec += tFileName;
  tmpSpec->MakeUnique();

  PR_FREEIF(tDir);
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

  // Age old question, where to store temp files....ugh!
  char  *tDir = GetTheTempDirectoryOnTheSystem();
  if (!tDir)
    return "mozmail.tmp";  // No need to I18N

  nsFileSpec tmpSpec(tDir);
  tmpSpec += tFileName;
  tmpSpec.MakeUnique();

  PR_FREEIF(tDir);
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
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
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
	if (!from || !*from)
		return NS_MSG_NO_SENDER;
	else
		if ((!to || !*to) && (!cc || !*cc) &&
				(!bcc || !*bcc) && (!newsgroups || !*newsgroups))
#if 0
			return NS_MSG_NO_RECIPIENTS;
#else
			return NS_ERROR_INVALID_ARG;
#endif
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
char * 
mime_generate_headers (nsMsgCompFields *fields,
									     const char *charset,
									     nsMsgDeliverMode deliver_mode)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	int size = 0;
	char *buffer = 0, *buffer_tail = 0;
	PRBool isDraft = deliver_mode == nsMsgSaveAsDraft ||
					deliver_mode == nsMsgSaveAsTemplate ||
					deliver_mode == nsMsgQueueForLater;

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

	NS_ASSERTION (fields, "null fields");
	if (!fields)
		return NULL;

	/* Multiply by 3 here to make enough room for MimePartII conversion */
	pFrom = fields->GetFrom(); if (pFrom)						size += 3 * PL_strlen (pFrom);
	pReplyTo =fields->GetReplyTo(); if (pReplyTo)				size += 3 * PL_strlen (pReplyTo);
	pTo = fields->GetTo(); if (pTo)								size += 3 * PL_strlen (pTo);
	pCc = fields->GetCc(); if (pCc)								size += 3 * PL_strlen (pCc);
	pNewsGrp = fields->GetNewsgroups(); if (pNewsGrp)			size += 3 * PL_strlen (pNewsGrp);
	pFollow= fields->GetFollowupTo(); if (pFollow)				size += 3 * PL_strlen (pFollow);
	pSubject = fields->GetSubject(); if (pSubject)				size += 3 * PL_strlen (pSubject);
	pReference = fields->GetReferences(); if (pReference)		size += 3 * PL_strlen (pReference);
	pOrg= fields->GetOrganization(); if (pOrg)					size += 3 * PL_strlen (pOrg);
	pOtherHdr= fields->GetOtherRandomHeaders(); if (pOtherHdr)	size += 3 * PL_strlen (pOtherHdr);
	pPriority = fields->GetPriority();  if (pPriority)			size += 3 * PL_strlen (pPriority);
	pMessageID = fields->GetMessageId(); if (pMessageID)		size += PL_strlen (pMessageID);

	/* Add a bunch of slop for the static parts of the headers. */
	/* size += 2048; */
	size += 2560;

	buffer = (char *) PR_Malloc (size);
	if (!buffer)
		return 0; /* NS_ERROR_OUT_OF_MEMORY */
	
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
			(deliver_mode != nsMsgSaveAsDraft &&
			deliver_mode != nsMsgSaveAsTemplate))
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
		if (fields->GetBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK))
		  PUSH_STRING("uuencode=1");
		else
		  PUSH_STRING("uuencode=0");

		PUSH_NEWLINE ();
	}


	NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &rv); 
	if (NS_SUCCEEDED(rv) && pNetService)
	{
		PRUnichar * appInfo = nsnull;

		pNetService->GetAppCodeName(&appInfo);
		nsAutoString cStr(appInfo, eOneByte);
		if (!cStr.IsEmpty()) 
		{
			// PUSH_STRING ("X-Mailer: ");  // To be more standards compliant
			PUSH_STRING ("User-Agent: ");  
			PUSH_STRING(cStr.GetBuffer());
			nsCRT::free(appInfo);

			pNetService->GetAppVersion(&appInfo);
			cStr = appInfo;
			if (!cStr.IsEmpty()) 
			{
				PUSH_STRING (" ");
				PUSH_STRING(cStr.GetBuffer());
			}
			nsCRT::free(appInfo);
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
				return 0; /* NS_ERROR_OUT_OF_MEMORY */
			}
  	  		n2 = nsMsgStripLine(ptr);
			NS_ASSERTION(n2 == ptr, "n2 != ptr");	/* Otherwise, the PR_Free below is
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

    // Ok, if we are here, we need to decide the Newsgroup related headers
    // to write to the outgoing message. In ANY case, we need to write the
    // "Newsgroup" header which is the "proper" header as opposed to the
    // HEADER_X_MOZILLA_NEWSHOST which can contain the "news:" URL's.
    //
    // Since n2 can contain data in the form of:
    // "news://news.mozilla.org./netscape.test,news://news.mozilla.org./netscape.junk"
    // we need to turn that into: "netscape.test,netscape.junk"
    //
    NS_WITH_SERVICE(nsINntpService, nntpService, kNntpServiceCID, &rv); 
    char *newHeader = nsnull;
    rv = nntpService->ConvertNewsgroupsString(n2, &newHeader);
    if (NS_SUCCEEDED(rv) && nntpService) 
    {
      // caller frees the memory in newHeader
      // ConvertNewsgroupsString takes "news://news.mozilla.org./netscape.test,news://news.mozilla.org./netscape.junk"
      // and turns it into "netscape.test,netscape.junk"
      rv = nntpService->ConvertNewsgroupsString(n2, &newHeader);
      if (NS_FAILED(rv)) 
        return nsnull;
      else 
      {
#ifdef NS_DEBUG
        printf("SUCCESS:  %s -> %s\n",n2,newHeader);
#endif
      }
    }
    else 
      return nsnull;

    PUSH_STRING ("Newsgroups: ");
		PUSH_STRING (newHeader);
    PR_FREEIF(newHeader);
  	PUSH_NEWLINE ();

    // If we are here, we are NOT going to send this now. (i.e. it is a Draft, 
    // Send Later file, etc...). Because of that, we need to store what the user
    // typed in on the original composition window for use later when rebuilding
    // the headers
    if (deliver_mode != nsMsgDeliverNow) 
    {
      // This is going to be saved for later, that means we should just store
      // what the user typed into the "Newsgroup" line in the HEADER_X_MOZILLA_NEWSHOST
      // header for later use by "Send Unsent Messages", "Drafts" or "Templates"
      PUSH_STRING (HEADER_X_MOZILLA_NEWSHOST);
      PUSH_STRING (": ");
      PUSH_STRING(n2);
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
				return 0; /* NS_ERROR_OUT_OF_MEMORY */
			}
			n2 = nsMsgStripLine (ptr);
			NS_ASSERTION(n2 == ptr, "n2 != ptr");	/* Otherwise, the PR_Free below is
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
				  XP_MEMMOVE(trimAt, ptr, PL_strlen(ptr)+1); // including the
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
		abort ();

	/* realloc it smaller... */
	buffer = (char*)PR_REALLOC (buffer, buffer_tail - buffer + 1);

	return buffer;
}

static void
GenerateGlobalRandomBytes(unsigned char *buf, PRInt32 len)
{
  PRBool    firstTime = PR_TRUE;

  if (firstTime)
  {
   /* Seed the random-number generator with current time so that
    * the numbers will be different every time we run.    */
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
                  const char *content_id)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	PRInt32 buffer_size = 2048 + (base_url ? 2*PL_strlen(base_url) : 0);
	char *buffer = (char *) PR_Malloc (buffer_size);
	char *buffer_tail = buffer;

	if (! buffer)
		return 0; /* NS_ERROR_OUT_OF_MEMORY */

	NS_ASSERTION (encoding, "null encoding");

	PUSH_STRING ("Content-Type: ");
	PUSH_STRING (type);

	if (mime_type_needs_charset (type)) {

	  char charset_label[65];   // Content-Type: charset
    PL_strcpy(charset_label, charset);

		/* If the characters are all 7bit, then it's better (and true) to
		claim the charset to be US-  rather than Latin1.  Should we
		do this all the time, for all charsets?  I'm not sure.  But we
		should definitely do it for Latin1. */
		if (encoding &&
				!PL_strcasecmp (encoding, "7bit") &&
				!PL_strcasecmp (charset, "iso-8859-1"))
			PL_strcpy (charset_label, "us-ascii");

		// If charset is JIS and and type is HTML
		// then no charset to be specified (apply base64 instead)
		// in order to avoid mismatch META_TAG (bug#104255).
		if ((PL_strcasecmp(charset, "iso-2022-jp") != 0) ||
				(PL_strcasecmp(type, TEXT_HTML) != 0) ||
				(PL_strcasecmp(encoding, ENCODING_BASE64) != 0)) {
			PUSH_STRING ("; charset=");
			PUSH_STRING (charset_label);
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

	PRInt32 parmFolding = 0;
  if (NS_SUCCEEDED(rv) && prefs) 
    prefs->GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

#ifdef EMIT_NAME_IN_CONTENT_TYPE
	if (real_name && *real_name) {
		if (parmFolding == 0 || parmFolding == 1) {
			PUSH_STRING (";\r\n name=\"");
			PUSH_STRING (real_name);
			PUSH_STRING ("\"");
		}
		else // if (parmFolding == 2)
		{
			char *rfc2231Parm = RFC2231ParmFolding("name", charset,
												 nsMsgI18NGetAcceptLanguage(), real_name);
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

	if (real_name && *real_name) {
		char *period = PL_strrchr(real_name, '.');
		PRInt32 pref_content_disposition = 0;

    prefs->GetIntPref("mail.content_disposition_type", &pref_content_disposition);
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
			PUSH_STRING (real_name);
			PUSH_STRING ("\"" CRLF);
		}
		else // if (parmFolding == 2)
		{
			char *rfc2231Parm = RFC2231ParmFolding("filename", charset,
											 nsMsgI18NGetAcceptLanguage(), real_name);
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
  
	char *from;
  nsresult rv = identity->GetEmail(&from);
  if (NS_FAILED(rv)) return nsnull;

	GenerateGlobalRandomBytes((unsigned char *) &salt, sizeof(salt));
	if (from) {
		host = PL_strchr (from, '@');
		if (host) {
			const char *s;
			for (s = ++host; *s; s++)
				if (!XP_IS_ALPHA(*s) && !XP_IS_DIGIT(*s) &&
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
#define PR_MAX_FOLDING_LEN 75			// this is to gurantee the folded line will
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
		dupParm = PL_strdup(parmValue);

	if (!dupParm)
		return NULL;

	if (needEscape)
	{
		parmValueLen = PL_strlen(dupParm);
		parmNameLen = PL_strlen(parmName);
	}

	if (needEscape)
		parmNameLen += 5;		// *=__'__'___ or *[0]*=__'__'__ or *[1]*=___
	else
		parmNameLen += 5;		// *[0]="___";
	charsetLen = charset ? PL_strlen(charset) : 0;
	languageLen = language ? PL_strlen(language) : 0;

	if ((parmValueLen + parmNameLen + charsetLen + languageLen) <
		PR_MAX_FOLDING_LEN)
	{
		foldedParm = PL_strdup(parmName);
		if (needEscape)
		{
			StrAllocCat(foldedParm, "*=");
			if (charsetLen)
				StrAllocCat(foldedParm, charset);
			StrAllocCat(foldedParm, "'");
			if (languageLen)
				StrAllocCat(foldedParm, language);
			StrAllocCat(foldedParm, "'");
		}
		else
			StrAllocCat(foldedParm, "=\"");
		StrAllocCat(foldedParm, dupParm);
		if (!needEscape)
			StrAllocCat(foldedParm, "\"");
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
					StrAllocCat(foldedParm, "\r\n ");
				else
					StrAllocCat(foldedParm, ";\r\n ");
				StrAllocCat(foldedParm, parmName);
			}
			PR_snprintf(digits, sizeof(digits), "*%d", counter);
			StrAllocCat(foldedParm, digits);
			curLineLen += PL_strlen(digits);
			if (needEscape)
			{
				StrAllocCat(foldedParm, "*=");
				if (counter == 0)
				{
					if (charsetLen)
						StrAllocCat(foldedParm, charset);
					StrAllocCat(foldedParm, "'");
					if (languageLen)
						StrAllocCat(foldedParm, language);
					StrAllocCat (foldedParm, "'");
					curLineLen += charsetLen;
					curLineLen += languageLen;
				}
			}
			else
			{
				StrAllocCat(foldedParm, "=\"");
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
			StrAllocCat(foldedParm, start);
			if (!needEscape)
				StrAllocCat(foldedParm, "\"");

			parmValueLen -= (end-start);
			if (tmp)
				*end = tmp;
			start = end;
		}
	}

done:
	PR_FREEIF(dupParm);
	return foldedParm;
}

PRBool 
mime_7bit_data_p (const char *string, PRUint32 size)
{
	const unsigned char *s = (const unsigned char *) string;
	const unsigned char *end = s + size;
	if (s)
		for (; s < end; s++)
			if (*s > 0x7F)
				return PR_FALSE;
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
		nsIMsgHeaderParser * pHeader;
		nsresult rv = nsComponentManager::CreateInstance(kMsgHeaderParserCID, 
                                               NULL, 
                                               nsCOMTypeInfo<nsIMsgHeaderParser>::GetIID(), 
                                               (void **) &pHeader);
		if (NS_SUCCEEDED(rv)) {
			char *n;
			pHeader->ReformatHeaderAddresses(nsnull, string, &n);
			pHeader->Release();
			if (n)
				return n;
		}
	}

	old_size = PL_strlen (string);
	new_size = old_size;
	for (i = 0; i < old_size; i++)
		if (string[i] == CR || string[i] == LF)
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
		if (*in == CR || *in == LF) {
			if (*in == CR && in[1] == LF)
				in++;
			in++;
			*out++ = CR;
			*out++ = LF;
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
		"application/mac-binhex40",		/* APPLICATION_BINHEX */
		"application/pgp",				/* APPLICATION_PGP */
		"application/x-pgp-message",	/* APPLICATION_PGP2 */
		"application/postscript",		/* APPLICATION_POSTSCRIPT */
		"application/x-uuencode",		/* APPLICATION_UUENCODE */
		"application/x-uue",			/* APPLICATION_UUENCODE2 */
		"application/uue",				/* APPLICATION_UUENCODE4 */
		"application/uuencode",			/* APPLICATION_UUENCODE3 */
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
		"image/x-portable-pixmap",		/* IMAGE_PPM */
		"image/x-ppm",
		"image/x-xbitmap",				/* IMAGE_XBM */
		"image/x-xbm",					/* IMAGE_XBM2 */
		"image/xbm",					/* IMAGE_XBM3 */
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

//
// Generate headers for a form post to a mailto: URL.
// This lets the URL specify additional headers, but is careful to
// ignore headers which would be dangerous.  It may modify the URL
// (because of CC) so a new URL to actually post to is returned.
//
int 
nsMsgMIMEGenerateMailtoFormPostHeaders (const char *from,
                                        const char *old_post_url,
									                      const char * /*referer*/,
									                      char **new_post_url_return,
									                      char **headers_return)
{
  char *to = 0, *cc = 0, *body = 0, *search = 0;
  char *extra_headers = 0;
  char *s;
  PRBool subject_p = PR_FALSE;
  PRBool sign_p = PR_FALSE, encrypt_p = PR_FALSE;
  char *rest;
  int status = 0;
  nsMsgCompFields *fields = NULL;
  static const char *forbidden_headers[] = {
	"Apparently-To",
	"BCC",
	"Content-Encoding",
	HEADER_CONTENT_LENGTH,
	"Content-Transfer-Encoding",
	"Content-Type",
	"Date",
	"Distribution",
	"FCC",
	"Followup-To",
	"From",
	"Lines",
	"MIME-Version",
	"Message-ID",
	"Newsgroups",
	"Organization",
	"Reply-To",
	"Sender",
	HEADER_X_MOZILLA_STATUS,
	HEADER_X_MOZILLA_STATUS2,
	HEADER_X_MOZILLA_NEWSHOST,
	HEADER_X_UIDL,
	"XRef",
	0 };

  to = nsMsgParseURL (old_post_url, GET_PATH_PART);
  if (!to) {
	status = NS_ERROR_OUT_OF_MEMORY;
	goto FAIL;
  }

  if (!*to)
	{
	  status = NS_MSG_NO_RECIPIENTS; /* rb -1; */
	  goto FAIL;
	}

  search = nsMsgParseURL (old_post_url, GET_SEARCH_PART);

  rest = search;
  if (rest && *rest == '?')
	{
	  /* start past the '?' */
	  rest++;

    nsCRT::strtok(rest, "&", &rest);
	  while (rest && *rest)
		{
		  char *token = rest;
		  char *value = 0;
		  char *eq;

      nsCRT::strtok(0, "&", &rest);

		  eq = PL_strchr (token, '=');
		  if (eq)
			{
			  value = eq+1;
			  *eq = 0;
			}

		  if (!PL_strcasecmp (token, "subject"))
			subject_p = PR_TRUE;

		  if (value)
			/* Don't allow newlines or control characters in the value. */
			for (s = value; *s; s++)
			  if (*s < ' ' && *s != '\t')
				*s = ' ';

		  if (!PL_strcasecmp (token, "to"))
			{
			  if (to && *to)
				{
				  StrAllocCat (to, ", ");
				  StrAllocCat (to, value);
				}
			  else
				{
				  PR_FREEIF(to);
				  to = PL_strdup(value);
				}
			}
		  else if (!PL_strcasecmp (token, "cc"))
			{
			  if (cc && *cc)
				{
				  StrAllocCat (cc, ", ");
				  StrAllocCat (cc, value);
				}
			  else
				{
				  PR_FREEIF(cc);
				  cc = PL_strdup (value);
				}
			}
		  else if (!PL_strcasecmp (token, "body"))
			{
			  if (body && *body)
				{
				  StrAllocCat (body, "\n");
				  StrAllocCat (body, value);
				}
			  else
				{
				  PR_FREEIF(body);
				  body = PL_strdup (value);
				}
			}
		  else if (!PL_strcasecmp (token, "encrypt") ||
				   !PL_strcasecmp (token, "encrypted"))
			{
			  encrypt_p = (!PL_strcasecmp(value, "true") ||
						   !PL_strcasecmp(value, "yes"));
			}
		  else if (!PL_strcasecmp (token, "sign") ||
				   !PL_strcasecmp (token, "signed"))
			{
			  sign_p = (!PL_strcasecmp(value, "true") ||
						!PL_strcasecmp(value, "yes"));
			}
		  else
			{
			  const char **fh = forbidden_headers;
			  PRBool ok = PR_TRUE;
			  while (*fh)
				if (!PL_strcasecmp (token, *fh++))
				  {
					ok = PR_FALSE;
					break;
				  }
			  if (ok)
				{
				  PRBool upper_p = PR_FALSE;
				  char *s2;
				  for (s2 = token; *s2; s2++)
					{
					  if (*s2 >= 'A' && *s2 <= 'Z')
						upper_p = PR_TRUE;
					  else if (*s2 <= ' ' || *s2 >= '~' || *s2 == ':')
						goto NOT_OK;  /* bad character in header! */
					}
				  if (!upper_p && *token >= 'a' && *token <= 'z')
					*token -= ('a' - 'A');

				  StrAllocCat (extra_headers, token);
				  StrAllocCat (extra_headers, ": ");
				  if (value)
					StrAllocCat (extra_headers, value);
				  StrAllocCat (extra_headers, CRLF);
				NOT_OK: ;
				}
			}
		}
	}

  if (!subject_p)
	{
		nsresult rv = NS_OK;
	
		PRUnichar * sAppName = nsnull;
		NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &rv); 
		if (NS_SUCCEEDED(rv) && pNetService)
			pNetService->GetAppCodeName(&sAppName);

		nsCAutoString cstr(sAppName);

		/* If the URL didn't provide a subject, we will. */
		StrAllocCat (extra_headers, "Subject: Form posted from ");
		NS_ASSERTION (!cstr.IsEmpty(), "null AppCodeName");
		StrAllocCat (extra_headers, cstr);
		StrAllocCat (extra_headers, CRLF);
		nsCRT::free(sAppName);
	}

  /* Note: the `encrypt', `sign', and `body' parameters are currently
	 ignored in mailto form submissions. */

  *new_post_url_return = 0;

 /*JFD
	fields = MSG_CreateCompositionFields(from, 0, to, cc, 0, 0, 0, 0,
									   (char *)pCompPrefs.GetOrganization(), 0, 0,
									   extra_headers, 0, 0, 0,
									   encrypt_p, sign_p);
 */
  if (!fields)
  {
	  status = NS_ERROR_OUT_OF_MEMORY;
	  goto FAIL;
  }

  fields->SetDefaultBody(body);

  *headers_return = mime_generate_headers (fields, 0, nsMsgDeliverNow);
  if (*headers_return == 0)
	{
	  status = NS_ERROR_OUT_OF_MEMORY;
	  goto FAIL;
	}

  StrAllocCat ((*new_post_url_return), "mailto:");
  if (to)
	StrAllocCat ((*new_post_url_return), to);
  if (to && cc)
	StrAllocCat ((*new_post_url_return), ",");
  if (cc)
	StrAllocCat ((*new_post_url_return), cc);

 FAIL:
  PR_FREEIF (to);
  PR_FREEIF (cc);
  PR_FREEIF (body);
  PR_FREEIF (search);
  PR_FREEIF (extra_headers);
 /*JFD
 if (fields)
	  MSG_DestroyCompositionFields(fields);
*/

  return status;
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
		/* 	Put backslashes in front of existing backslashes, or double quote
			characters.
			If stripCRLFs is true, don't write out CRs or LFs. Otherwise,
			write out a backslash followed by the CR but not
			linear-white-space.
			We might already have quoted pair of "\ " or "\\t" skip it. 
			*/
		if (*s == '\\' || *s == '"' || 
			(!stripCRLFs && 
			 (*s == CR && (*(s+1) != LF || 
						   (*(s+1) == LF && (s+2) < end && !IS_SPACE(*(s+2)))))))
			*d++ = '\\';

		if (*s == CR)
		{
			if (stripCRLFs && *(s+1) == LF && (s+2) < end && IS_SPACE(*(s+2)))
				s += 2;			// skip CRLFLWSP
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
msg_pick_real_name (nsMsgAttachmentHandler *attachment, const char *charset)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  const char *s, *s2;
  char *s3;
  nsXPIDLCString url;

  if ( (attachment->m_real_name) && (*attachment->m_real_name))
  	return;

  attachment->mURL->GetSpec(getter_Copies(url));

  // Otherwise, extract a name from the URL.
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

  PRInt32 parmFolding = 0;
  if (NS_SUCCEEDED(rv) && prefs) 
    prefs->GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

  if (parmFolding == 0 || parmFolding == 1)
  {
	  /* Try to MIME-2 encode the filename... */
	  char *mime2Name = nsMsgI18NEncodeMimePartIIStr(attachment->m_real_name, charset, 
												nsMsgMIMEGetConformToStandard());
	  if (mime2Name && (mime2Name != attachment->m_real_name))
	  {
		  PR_Free(attachment->m_real_name);
		  attachment->m_real_name = mime2Name;
	  }
   
	  /* ... and then put backslashes before special characters (RFC 822 tells us
		 to). */

	  char *qtextName = NULL;
	  
	  qtextName = msg_make_filename_qtext(attachment->m_real_name, 
										  (parmFolding == 0 ? PR_TRUE : PR_FALSE));
		  
	  if (qtextName)
	  {
		  PR_Free(attachment->m_real_name);
		  attachment->m_real_name = qtextName;
	  }
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
		  if (L > L2 + 1 &&							/* long enough */
			  result[L - L2 - 1] == '.' &&			/* '.' in right place*/
			  !PL_strcasecmp(ext, result + (L - L2)))	/* ext matches */
			{
			  result[L - L2 - 1] = 0;		/* truncate at '.' and stop. */
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
  NS_WITH_SERVICE(nsIIOService, pNetService, kIOServiceCID, &rv); 
  if (NS_SUCCEEDED(rv) && pNetService)
	rv = pNetService->NewURI(aSpec, nsnull, aInstancePtrResult);
  return rv;
}

PRBool
nsMsgIsLocalFile(const char *url)
{
  if (PL_strncasecmp(url, "file:", 5) == 0) 
    return PR_TRUE;
  else
    return PR_FALSE;
}

char
*nsMsgGetLocalFileFromURL(char *url)
{
	char * finalPath;
	NS_ASSERTION(PL_strncasecmp(url, "file://", 7) == 0, "invalid url");
	finalPath = (char*)PR_Malloc(strlen(url));
	if (finalPath == NULL)
		return NULL;
	strcpy(finalPath, url+6+1);
	return finalPath;
}

char * 
nsMsgPlatformFileToURL (const char *name)
{
	char *prefix = "file:///";
	char *retVal = (char *)PR_Malloc(PL_strlen(name) + PL_strlen(prefix) + 1);
	if (retVal)
	{
		PL_strcpy(retVal, prefix);
		PL_strcat(retVal, name);
	}

  char *ptr = retVal;
  while (*ptr)
  {
    if (*ptr == '\\') *ptr = '/';
    if ( (*ptr == ':') && (ptr > (retVal+4)) )
      *ptr = '|';

    ++ptr;
  }
	return retVal;
}

char * 
nsMsgParseURL(const char *url, int part)
{
  nsCOMPtr<nsIMimeURLUtils>   utilPtr;
  char                        *retVal = nsnull;
  
  nsresult res = nsComponentManager::CreateInstance(kMimeURLUtilsCID, 
                                                    nsnull, nsCOMTypeInfo<nsIMimeURLUtils>::GetIID(), 
                                                    (void **) getter_AddRefs(utilPtr)); 
  if (NS_FAILED(res) || !utilPtr)
    return nsnull;
  
  res = utilPtr->ParseURL(url, part, &retVal);
  if (NS_FAILED(res))
    return nsnull;
  else
    return retVal;
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
      if (*cp != '\0')
      {
        returnString = PL_strdup(cp);
        PR_FREEIF(newFile);
        return returnString;
      }
    }
  }

  cp = nsnull;
  cp1 = nsnull;


  rv = aURL->GetSpec(getter_Copies(spec));
  if ( NS_SUCCEEDED(rv) && spec)
  {
    char *newSpec = PL_strdup(spec);
    if (!newSpec)
      return nsnull;

    char *cp = NULL, *cp1=NULL ;

    // strip '"' 
    cp = newSpec;
    while (*cp == '"') 
      cp++;
    if ((cp1 = PL_strchr(cp, '"')))
      *cp1 = 0;

    char *hostStr = nsMsgParseURL(cp, GET_HOST_PART);
    if (!hostStr)
      hostStr = cp;

    nsXPIDLCString protocol;
    if (NS_SUCCEEDED(aURL->GetScheme(getter_Copies(protocol))) && (protocol))
    {
      if (PL_strcasecmp(protocol, "http") == 0)
      {
        returnString = PR_smprintf("%s.html", hostStr);
        PR_FREEIF(hostStr);
      }
      else
        returnString = hostStr;
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
  if ((!aEmailAddress) || (!*aEmailAddress))
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
  if (domain != defaultDomain)
    PR_FREEIF(domain);

  return retVal;
}

char *
GetFolderURIFromUserPrefs(nsMsgDeliverMode   aMode,
                          PRBool             aNewsMessage)
{
  nsresult      rv = NS_OK;
  char          *uri = nsnull;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_FAILED(rv) || !prefs) 
    return nsnull;

  if (aMode == nsMsgQueueForLater)       // QueueForLater (Outbox)
  {
    rv = prefs->CopyCharPref("mail.default_sendlater_uri", &uri);
  }
  else if (aMode == nsMsgSaveAsDraft)    // SaveAsDraft (Drafts)
  {
    rv = prefs->CopyCharPref("mail.default_drafts_uri", &uri);
  }
  else if (aMode == nsMsgSaveAsTemplate) // SaveAsTemplate (Templates)
  {
    rv = prefs->CopyCharPref("mail.default_templates_uri", &uri);
  }
  else 
  {
    //
    // RICHIE SHERRY - Currently, I am always passing in PR_FALSE for 
    // the aNewsMessage .... need to do something more intelligent!!!
    //
    // This is an FCC operation for a mail message OR a news message 
    if (aNewsMessage)
      rv = prefs->CopyCharPref("mail.default_newsfcc_uri", &uri);
    else
      rv = prefs->CopyCharPref("mail.default_fcc_uri", &uri);
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

  if (aUrl == "")
    return nsnull;

  url = aUrl.ToNewCString();
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
  PR_FREEIF(url);
  return nsnull;
}
