/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#include "nsCOMPtr.h"
#include "modmimee.h"
#include "libi18n.h"
#include "mimeobj.h"
#include "modlmime.h"
#include "mimei.h"
#include "mimebuf.h"
#include "mimemoz2.h"
#include "mimemsg.h"
#include "nsMimeTypes.h"

#include "prmem.h"
#include "plstr.h"
#include "prio.h"
#include "nsIPref.h"
#include "msgCore.h"
#include "nsCRT.h"
#include "nsIMsgSend.h"
#include "nsFileStream.h"
#include "nsMimeStringResources.h"
#include "nsIIOService.h"
#include "comi18n.h"
#include "nsIMsgCompFields.h"
#include "nsMsgCompCID.h"
#include "nsIMsgComposeService.h"
#include "nsMsgI18N.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIMsgMessageService.h"
#include "nsMsgUtils.h"
#include "nsXPIDLString.h"
#include "nsIMIMEService.h"
#include "nsIMIMEInfo.h"

//
// Header strings...
//
#define HEADER_NNTP_POSTING_HOST      "NNTP-Posting-Host"
#define MIME_HEADER_TABLE             "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>"
#define HEADER_START_JUNK             "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"
#define HEADER_MIDDLE_JUNK            ": </TH><TD>"
#define HEADER_END_JUNK               "</TD></TR>"

//
// Forward declarations...
//
extern "C" char     *MIME_StripContinuations(char *original);
int                 mime_decompose_file_init_fn ( void *stream_closure, MimeHeaders *headers );
int                 mime_decompose_file_output_fn ( char *buf, PRInt32 size, void *stream_closure );
int                 mime_decompose_file_close_fn ( void *stream_closure );
extern int          MimeHeaders_build_heads_list(MimeHeaders *hdrs);

static nsString& mime_decode_string(const char* str , 
                                    PRBool eatContinuations = PR_TRUE);

// CID's
static NS_DEFINE_CID(kCMsgComposeServiceCID,  NS_MSGCOMPOSESERVICE_CID);       
static NS_DEFINE_CID(kMimeServiceCID, NS_MIMESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// THIS SHOULD ALL MOVE TO ANOTHER FILE AFTER LANDING!
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// Define CIDs...
static NS_DEFINE_CID(kMsgCompFieldsCID,       NS_MSGCOMPFIELDS_CID); 
static NS_DEFINE_CID(kPrefCID,                NS_PREF_CID);

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
    tFileName = "nsmime.tmp";

  nsFileSpec *tmpSpec = new nsFileSpec(nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory));
  if (!tmpSpec)
    return nsnull;
  
  *tmpSpec += tFileName;
  tmpSpec->MakeUnique();

  return tmpSpec;
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// END OF - THIS SHOULD ALL MOVE TO ANOTHER FILE AFTER LANDING!
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

typedef enum {
	nsMsg_RETURN_RECEIPT_BOOL_HEADER_MASK = 0,
	nsMsg_ENCRYPTED_BOOL_HEADER_MASK,
	nsMsg_SIGNED_BOOL_HEADER_MASK,
	nsMsg_UUENCODE_BINARY_BOOL_HEADER_MASK,
	nsMsg_ATTACH_VCARD_BOOL_HEADER_MASK,
	nsMsg_LAST_BOOL_HEADER_MASK			// last boolean header mask; must be the last one 
										              // DON'T remove.
} nsMsgBoolHeaderSet;

extern "C" void
mime_dump_attachments ( nsMsgAttachmentData *attachData )
{
  PRInt32     i = 0;
  struct nsMsgAttachmentData  *tmp = attachData;

  while ( (tmp) && (tmp->real_name) )
  {
	  printf("Real Name         : %s\n", tmp->real_name);

    if ( tmp->url ) 
    {
      char *spec = nsnull;

      tmp->url->GetSpec(&spec);
  	  printf("URL               : %s\n", spec);
    }

	  printf("Desired Type      : %s\n", tmp->desired_type);
    printf("Real Type         : %s\n", tmp->real_type);
	  printf("Real Encoding     : %s\n", tmp->real_encoding); 
    printf("Description       : %s\n", tmp->description);
    printf("Mac Type          : %s\n", tmp->x_mac_type);
    printf("Mac Creator       : %s\n", tmp->x_mac_creator);
    i++;
    tmp++;
  }
}

nsresult
CreateTheComposeWindow(nsIMsgCompFields     *compFields,
                       nsMsgAttachmentData  *attachmentList,
                       MSG_ComposeType		composeType,
                       MSG_ComposeFormat    composeFormat,
                       nsIMsgIdentity *		identity)
{
nsresult            rv;
MSG_ComposeFormat   format = nsIMsgCompFormat::Default;

#ifdef NS_DEBUG
printf("RICHIE: Need to do something with the attachment data!!!!\n");
mime_dump_attachments ( attachmentList );
#endif

  nsMsgAttachmentData *curAttachment = attachmentList;
  if (curAttachment)
  {
	nsString attachments;
	char* spec = nsnull;

	while (curAttachment && curAttachment->real_name)
	  {
		rv = curAttachment->url->GetSpec(&spec);
		if (NS_SUCCEEDED(rv) && spec)
		  {
			if (attachments.Length())
			  attachments.AppendWithConversion(',');
			attachments.AppendWithConversion(spec);
			nsCRT::free(spec);
			spec = nsnull;
		  }
		curAttachment++;
	  }
	if (attachments.Length())
	  compFields->SetAttachments(attachments.GetUnicode());
  }

  NS_WITH_SERVICE(nsIMsgComposeService, msgComposeService,
				  kCMsgComposeServiceCID, &rv); 
  if ((NS_FAILED(rv)) || (!msgComposeService))
    return rv; 
  
  if (identity && composeType == nsIMsgCompType::ForwardInline)
  {
  	PRBool composeHtml = PR_FALSE;
  	identity->GetComposeHtml(&composeHtml);
  	if (composeHtml)
		format = nsIMsgCompFormat::HTML;
	else
	{
		format = nsIMsgCompFormat::PlainText;
		/* do we we need to convert the HTML body to plain text? */
		if (composeFormat == nsIMsgCompFormat::HTML)
			compFields->ConvertBodyToPlainText();
	}
  }
  else
    format = composeFormat;
    
  rv = msgComposeService->OpenComposeWindowWithCompFields(nsnull, /* default chrome */
                                                          composeType, format, compFields, identity);
  return rv;
}

static nsString& mime_decode_string(const char* str, 
                                    PRBool eatContinuations)
{
    static nsString decodedString;
    nsString encodedCharset;
    nsMsgI18NDecodeMimePartIIStr(NS_ConvertASCIItoUCS2(str), encodedCharset,
                                 decodedString, eatContinuations);
    return decodedString;
}

nsIMsgCompFields * 
CreateCompositionFields(const char        *from,
									      const char        *reply_to,
									      const char        *to,
									      const char        *cc,
									      const char        *bcc,
									      const char        *fcc,
									      const char        *newsgroups,
									      const char        *followup_to,
									      const char        *organization,
									      const char        *subject,
									      const char        *references,
									      const char        *other_random_headers,
									      const char        *priority,
									      const char        *attachment,
                        const char        *newspost_url,
                        PRBool            xlate_p,
                        PRBool            sign_p,
                        char              *charset)
{
  nsIMsgCompFields *cFields = nsnull;

  // Create the compose fields...
  nsresult rv = nsComponentManager::CreateInstance(kMsgCompFieldsCID, NULL, 
                                                   NS_GET_IID(nsIMsgCompFields), 
                                                   (void **) &cFields);   
  if (NS_FAILED(rv) || !cFields) 
    return nsnull;
  NS_ADDREF(cFields);

  // Now set all of the passed in stuff...
  cFields->SetCharacterSet(NS_ConvertASCIItoUCS2(charset).GetUnicode());
  cFields->SetFrom(mime_decode_string(from).GetUnicode());
  cFields->SetSubject(mime_decode_string(subject).GetUnicode());
  cFields->SetReplyTo(mime_decode_string(reply_to).GetUnicode());
  cFields->SetTo(mime_decode_string(to).GetUnicode());
  cFields->SetCc(mime_decode_string(cc).GetUnicode());
  cFields->SetBcc(mime_decode_string(bcc).GetUnicode());
  cFields->SetFcc(mime_decode_string(fcc).GetUnicode());
  cFields->SetNewsgroups(mime_decode_string(newsgroups).GetUnicode());
  cFields->SetFollowupTo(mime_decode_string(followup_to).GetUnicode());
  cFields->SetOrganization(mime_decode_string(organization).GetUnicode());
  cFields->SetReferences(mime_decode_string(references).GetUnicode());
  cFields->SetOtherRandomHeaders(mime_decode_string(other_random_headers).GetUnicode());
  cFields->SetPriority(mime_decode_string(priority).GetUnicode());
  cFields->SetAttachments(mime_decode_string(attachment).GetUnicode());
  cFields->SetNewspostUrl(mime_decode_string(newspost_url).GetUnicode());

  return cFields;
}

// RICHIE:
// Not sure what this preference is in the new world.
PRBool
GetMailXlateionPreference(void)
{
  nsresult res;
  PRBool   xlate = PR_FALSE; 

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->GetBoolPref("mail.unknown", &xlate);
  
  return xlate;
}

// RICHIE:
// Not sure what this preference is in the new world.
PRBool
GetMailSigningPreference(void)
{
  nsresult  res;
  PRBool    signit = PR_FALSE;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
  if (NS_SUCCEEDED(res) && prefs)
    res = prefs->GetBoolPref("mail.unknown", &signit);

  return signit;
}
  
static int
dummy_file_write( char *buf, PRInt32 size, void *fileHandle )
{
  if (!fileHandle)
    return NS_ERROR_FAILURE;

  nsOutputFileStream  *tStream = (nsOutputFileStream *) fileHandle;
  return tStream->write(buf, size);
}

static int PR_CALLBACK
mime_parse_stream_write ( nsMIMESession *stream, const char *buf, PRInt32 size )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;  
  NS_ASSERTION ( mdd, "null mime draft data!" );

  if ( !mdd || !mdd->obj ) 
    return -1;

  return mdd->obj->clazz->parse_buffer ((char *) buf, size, mdd->obj);
}

static void
mime_free_attach_data ( nsMsgAttachmentData *attachData, int cleanupCount)
{
  PRInt32     i;
  struct nsMsgAttachmentData  *tmp = attachData;

  for ( i=0; i < cleanupCount; i++, tmp++ ) 
  {
	  if ( tmp->url ) 
      delete tmp->url;
    PR_FREEIF(tmp->real_name);

	  PR_FREEIF(tmp->desired_type);
    PR_FREEIF(tmp->real_type);
	  PR_FREEIF(tmp->real_encoding); 
    PR_FREEIF(tmp->description);
    PR_FREEIF(tmp->x_mac_type);
    PR_FREEIF(tmp->x_mac_creator);
  }
}

static void
mime_free_attachments ( nsMsgAttachedFile *attachments, int count )
{
  int               i;
  nsMsgAttachedFile *cur = attachments;

  NS_ASSERTION ( attachments && count > 0, "freeing attachments but there aren't any");
  if ( !attachments || count <= 0 ) 
    return;

  for ( i = 0; i < count; i++, cur++ ) 
  {
	  if ( cur->orig_url )
      NS_RELEASE(cur->orig_url);

	  PR_FREEIF ( cur->type );
	  PR_FREEIF ( cur->encoding );
	  PR_FREEIF ( cur->description );
	  PR_FREEIF ( cur->x_mac_type );
	  PR_FREEIF ( cur->x_mac_creator );
	  if ( cur->file_spec ) 
    {
      cur->file_spec->Delete(PR_FALSE);
	    delete ( cur->file_spec );
	  }
  }

  PR_FREEIF( attachments );
}

static nsMsgAttachmentData *
mime_draft_process_attachments(mime_draft_data *mdd) 
{
  nsMsgAttachmentData         *attachData = NULL, *tmp = NULL;
  nsMsgAttachedFile           *tmpFile = NULL;
  int                         i;
  PRInt32                     cleanupCount = 0;

  NS_ASSERTION ( mdd->attachments_count && mdd->attachments, "processing attachments but there aren't any" );

  if ( !mdd->attachments || !mdd->attachments_count )
  	return nsnull;

  attachData = (nsMsgAttachmentData *) PR_MALLOC( ( (mdd->attachments_count+1) * sizeof (nsMsgAttachmentData) ) );
  if ( !attachData ) 
    return nsnull;

  nsCRT::memset ( attachData, 0, (mdd->attachments_count+1) * sizeof (nsMsgAttachmentData) );

  tmpFile = mdd->attachments;
  tmp = attachData;

  for ( i=0; i < mdd->attachments_count; i++, tmp++, tmpFile++ ) 
  {
	  if (tmpFile->type) 
    {
		  if (nsCRT::strcasecmp ( tmpFile->type, "text/x-vcard") == 0)
			  mime_SACopy (&(tmp->real_name), tmpFile->description);
	  }

	  if ( tmpFile->orig_url ) 
    {
      char  *tmpSpec = nsnull;
      if (NS_FAILED(tmpFile->orig_url->GetSpec(&tmpSpec)))
      {
        cleanupCount = i;
        goto FAIL;
      }

      if (NS_FAILED(nsMimeNewURI(&(tmp->url), tmpSpec, nsnull)))
      {
        PR_FREEIF(tmpSpec);
        cleanupCount = i;
        goto FAIL; 
      }

      NS_ADDREF(tmp->url);
	    if (!tmp->real_name)
  	    mime_SACopy ( &(tmp->real_name), tmpSpec );
	  }

	  if ( tmpFile->type ) 
    {
	    mime_SACopy ( &(tmp->desired_type), tmpFile->type );
	    mime_SACopy ( &(tmp->real_type), tmpFile->type );
	  }

	  if ( tmpFile->encoding ) 
    {
	    mime_SACopy ( &(tmp->real_encoding), tmpFile->encoding );
	  }

	  if ( tmpFile->description ) 
    {
	    mime_SACopy ( &(tmp->description), tmpFile->description );
	  }

	  if ( tmpFile->x_mac_type ) 
    {
	    mime_SACopy ( &(tmp->x_mac_type), tmpFile->x_mac_type );
	  }

	  if ( tmpFile->x_mac_creator ) 
    {
	    mime_SACopy ( &(tmp->x_mac_creator), tmpFile->x_mac_creator );
	  }
  }

  return (attachData);

FAIL:
  mime_free_attach_data (attachData, cleanupCount);
  PR_FREEIF(attachData);
  return nsnull;
}

static void 
mime_fix_up_html_address( char **addr)
{
	//
  // We need to replace paired <> they are treated as HTML tag 
  //
	if (addr && *addr && PL_strchr(*addr, '<') && PL_strchr(*addr, '>'))
	{
		char *lt = NULL;
		PRInt32 newLen = 0;
		do 
		{
			newLen = nsCRT::strlen(*addr) + 3 + 1;
			*addr = (char *) PR_REALLOC(*addr, newLen);
			NS_ASSERTION (*addr, "out of memory fixing up html address");
			lt = PL_strchr(*addr, '<');
			NS_ASSERTION(lt, "couldn't find < char in address");
      nsCRT::memmove(lt+4, lt+1, newLen - 4 - (lt - *addr));
			*lt++ = '&';
			*lt++ = 'l';
			*lt++ = 't';
			*lt = ';';
		} while (PL_strchr(*addr, '<'));
	}
}

static void  
mime_intl_mimepart_2_str(char **str, char *mcharset)
{
  //
  // Converting these header values for UTF-8 for proper display
  // in the text input widgets.
  //
	if (str && *str)
	{
    // Now do conversion to UTF-8???
    char  *newStr = NULL;
    PRInt32 newStrLen;
    PRInt32 res = MIME_ConvertCharset(PR_TRUE, mcharset, "UTF-8", *str, nsCRT::strlen(*str), 
                                    &newStr, &newStrLen, NULL);
		if ( (NS_SUCCEEDED(res)) && (newStr && newStr != *str))
		{
			PR_FREEIF(*str);
			*str = newStr;
		}
		else
		{
			MIME_StripContinuations(*str);
		}
	}
}

static void 
mime_intl_insert_message_header_1(char        **body, 
                                  char        **hdr_value,
											            char        *hdr_str, 
											            const char  *html_hdr_str,
											            char        *mailcharset,
											            PRBool      htmlEdit)
{
	if (!body || !hdr_value || !hdr_str)
		return;

	if (htmlEdit)
	{
		mime_SACat(body, HEADER_START_JUNK);
	}
	else
	{
		mime_SACat(body, MSG_LINEBREAK);
	}
	if (!html_hdr_str)
		html_hdr_str = hdr_str;
	mime_SACat(body, html_hdr_str);
	if (htmlEdit)
	{
		mime_SACat(body, HEADER_MIDDLE_JUNK);
	}
	else
		mime_SACat(body, ": ");

  // MIME decode header and convert to UTF-8
  nsAutoString ucs2(mime_decode_string(*hdr_value));
  char* utf8 = ucs2.ToNewUTF8String();
  if (NULL != utf8) {
    mime_SACat(body, utf8);
    Recycle(utf8);
  }
  else 
    mime_SACat(body, *hdr_value); // raw MIME encoded string

	if (htmlEdit)
		mime_SACat(body, HEADER_END_JUNK);
}

static void 
mime_insert_all_headers(char            **body,
									      MimeHeaders     *headers,
									      MSG_ComposeFormat composeFormat,
									      char            *mailcharset)
{
	PRBool htmlEdit = (composeFormat == nsIMsgCompFormat::HTML);
	char *newBody = NULL;
	char *html_tag = PL_strcasestr(*body, "<HTML>");
	int i;

	if (!headers->done_p)
	{
		MimeHeaders_build_heads_list(headers);
		headers->done_p = PR_TRUE;
	}

	if (htmlEdit)
	{
		if (html_tag)
		{
			*html_tag = 0;
			mime_SACopy(&(newBody), *body);
			*html_tag = '<';
			mime_SACat(&newBody, 
					 "<HTML> <BR><BR>-------- Original Message --------");
		}
		else
		{
			mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
		}
		mime_SACat(&newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 MSG_LINEBREAK MSG_LINEBREAK "-------- Original Message --------");
	}

	for (i = 0; i < headers->heads_size; i++)
	{
	  char *head = headers->heads[i];
	  char *end = (i == headers->heads_size-1
				   ? headers->all_headers + headers->all_headers_fp
				   : headers->heads[i+1]);
	  char *colon, *ocolon;
	  char *contents;
	  char *name = 0;
	  char *c2 = 0;

	  // Hack for BSD Mailbox delimiter. 
	  if (i == 0 && head[0] == 'F' && !nsCRT::strncmp(head, "From ", 5))
		{
		  colon = head + 4;
		  contents = colon + 1;
		}
	  else
		{
		  /* Find the colon. */
		  for (colon = head; colon < end; colon++)
			if (*colon == ':') break;

		  if (colon >= end) continue;   /* junk */

		  /* Back up over whitespace before the colon. */
		  ocolon = colon;
		  for (; colon > head && nsCRT::IsAsciiSpace(colon[-1]); colon--)
			;

		  contents = ocolon + 1;
		}

	  /* Skip over whitespace after colon. */
	  while (contents <= end && nsCRT::IsAsciiSpace(*contents))
		contents++;

	  /* Take off trailing whitespace... */
	  while (end > contents && nsCRT::IsAsciiSpace(end[-1]))
		end--;

	  name = (char *)PR_MALLOC(colon - head + 1);
	  if (!name) 
      return /* MIME_OUT_OF_MEMORY */;
	  nsCRT::memcpy(name, head, colon - head);
	  name[colon - head] = 0;

	  c2 = (char *)PR_MALLOC(end - contents + 1);
	  if (!c2)
		{
		  PR_Free(name);
		  return /* MIME_OUT_OF_MEMORY */;
		}
	  nsCRT::memcpy(c2, contents, end - contents);
	  c2[end - contents] = 0;
	  
	  if (htmlEdit) mime_fix_up_html_address(&c2);
		  
	  mime_intl_insert_message_header_1(&newBody, &c2, name, name, mailcharset,
										htmlEdit); 
	  PR_Free(name);
	  PR_Free(c2);
	}

	if (htmlEdit)
	{
		mime_SACat(&newBody, "</TABLE>");
		mime_SACat(&newBody, MSG_LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(&newBody, html_tag+6);
		else
			mime_SACat(&newBody, *body);
	}
	else
	{
		mime_SACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
		mime_SACat(&newBody, *body);
	}

	if (newBody)
	{
		PR_FREEIF(*body);
		*body = newBody;
	}
}

char *
MimeGetNamedString(PRInt32 id)
{
  static char   retString[256];

  retString[0] = '\0';
  char *tString = MimeGetStringByID(id);
  if (tString)
    PL_strncpy(retString, tString, sizeof(retString)); 

  return retString;
}

static void 
mime_insert_normal_headers(char             **body,
									         MimeHeaders      *headers,
									         MSG_ComposeFormat  composeFormat,
									         char             *mailcharset)
{
	char *newBody = NULL;
	char *subject = MimeHeaders_get(headers, HEADER_SUBJECT, PR_FALSE, PR_FALSE);
	char *resent_comments = MimeHeaders_get(headers, HEADER_RESENT_COMMENTS,
											PR_FALSE, PR_FALSE);
	char *resent_date = MimeHeaders_get(headers, HEADER_RESENT_DATE, PR_FALSE,
										PR_TRUE); 
	char *resent_from = MimeHeaders_get(headers, HEADER_RESENT_FROM, PR_FALSE,
										PR_TRUE);
	char *resent_to = MimeHeaders_get(headers, HEADER_RESENT_TO, PR_FALSE, PR_TRUE);
	char *resent_cc = MimeHeaders_get(headers, HEADER_RESENT_CC, PR_FALSE, PR_TRUE);
	char *date = MimeHeaders_get(headers, HEADER_DATE, PR_FALSE, PR_TRUE);
	char *from = MimeHeaders_get(headers, HEADER_FROM, PR_FALSE, PR_TRUE);
	char *reply_to = MimeHeaders_get(headers, HEADER_REPLY_TO, PR_FALSE, PR_TRUE);
	char *organization = MimeHeaders_get(headers, HEADER_ORGANIZATION,
										 PR_FALSE, PR_FALSE);
	char *to = MimeHeaders_get(headers, HEADER_TO, PR_FALSE, PR_TRUE);
	char *cc = MimeHeaders_get(headers, HEADER_CC, PR_FALSE, PR_TRUE);
	char *bcc = MimeHeaders_get(headers, HEADER_BCC, PR_FALSE, PR_TRUE);
	char *newsgroups = MimeHeaders_get(headers, HEADER_NEWSGROUPS, PR_FALSE,
									   PR_TRUE);
	char *followup_to = MimeHeaders_get(headers, HEADER_FOLLOWUP_TO, PR_FALSE,
										PR_TRUE);
	char *references = MimeHeaders_get(headers, HEADER_REFERENCES, PR_FALSE, PR_TRUE);
	const char *html_tag = PL_strcasestr(*body, "<HTML>");
	PRBool htmlEdit = composeFormat == nsIMsgCompFormat::HTML;
	
	if (!from)
		from = MimeHeaders_get(headers, HEADER_SENDER, PR_FALSE, PR_TRUE);
	if (!resent_from)
		resent_from = MimeHeaders_get(headers, HEADER_RESENT_SENDER, PR_FALSE,
									  PR_TRUE); 
	
	if (htmlEdit)
	{
		mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
		mime_SACat(&newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 MSG_LINEBREAK MSG_LINEBREAK "-------- Original Message --------");
	}
	if (subject)
		mime_intl_insert_message_header_1(&newBody, &subject, HEADER_SUBJECT,
										  MimeGetNamedString(MIME_MHTML_SUBJECT),
											mailcharset, htmlEdit);
	if (resent_comments)
		mime_intl_insert_message_header_1(&newBody, &resent_comments,
										  HEADER_RESENT_COMMENTS, 
										  MimeGetNamedString(MIME_MHTML_RESENT_COMMENTS),
											mailcharset, htmlEdit);
	if (resent_date)
		mime_intl_insert_message_header_1(&newBody, &resent_date,
										  HEADER_RESENT_DATE,
										  MimeGetNamedString(MIME_MHTML_RESENT_DATE),
											mailcharset, htmlEdit);
	if (resent_from)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_from);
		mime_intl_insert_message_header_1(&newBody, &resent_from,
										  HEADER_RESENT_FROM,
										  MimeGetNamedString(MIME_MHTML_RESENT_FROM),
											mailcharset, htmlEdit);
	}
	if (resent_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_to);
		mime_intl_insert_message_header_1(&newBody, &resent_to,
										  HEADER_RESENT_TO,
										  MimeGetNamedString(MIME_MHTML_RESENT_TO),
											mailcharset, htmlEdit);
	}
	if (resent_cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_cc);
		mime_intl_insert_message_header_1(&newBody, &resent_cc,
										  HEADER_RESENT_CC,
										  MimeGetNamedString(MIME_MHTML_RESENT_CC),
											mailcharset, htmlEdit);
	}
	if (date)
		mime_intl_insert_message_header_1(&newBody, &date, HEADER_DATE,
										  MimeGetNamedString(MIME_MHTML_DATE),
										  mailcharset, htmlEdit);
	if (from)
	{
		if (htmlEdit) mime_fix_up_html_address(&from);
		mime_intl_insert_message_header_1(&newBody, &from, HEADER_FROM,
										  MimeGetNamedString(MIME_MHTML_FROM),
										  mailcharset, htmlEdit);
	}
	if (reply_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&reply_to);
		mime_intl_insert_message_header_1(&newBody, &reply_to, HEADER_REPLY_TO,
										  MimeGetNamedString(MIME_MHTML_REPLY_TO),
										  mailcharset, htmlEdit);
	}
	if (organization)
		mime_intl_insert_message_header_1(&newBody, &organization,
										  HEADER_ORGANIZATION,
										  MimeGetNamedString(MIME_MHTML_ORGANIZATION),
										  mailcharset, htmlEdit);
	if (to)
	{
		if (htmlEdit) mime_fix_up_html_address(&to);
		mime_intl_insert_message_header_1(&newBody, &to, HEADER_TO,
										  MimeGetNamedString(MIME_MHTML_TO),
										  mailcharset, htmlEdit);
	}
	if (cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&cc);
		mime_intl_insert_message_header_1(&newBody, &cc, HEADER_CC,
										  MimeGetNamedString(MIME_MHTML_CC),
										  mailcharset, htmlEdit);
	}
	if (bcc)
	{
		if (htmlEdit) mime_fix_up_html_address(&bcc);
		mime_intl_insert_message_header_1(&newBody, &bcc, HEADER_BCC,
										  MimeGetNamedString(MIME_MHTML_BCC),
										  mailcharset, htmlEdit);
	}
	if (newsgroups)
		mime_intl_insert_message_header_1(&newBody, &newsgroups, HEADER_NEWSGROUPS,
										  MimeGetNamedString(MIME_MHTML_NEWSGROUPS),
										  mailcharset, htmlEdit);
	if (followup_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&followup_to);
		mime_intl_insert_message_header_1(&newBody, &followup_to,
										  HEADER_FOLLOWUP_TO,
										  MimeGetNamedString(MIME_MHTML_FOLLOWUP_TO),
										  mailcharset, htmlEdit);
	}
	if (references)
	{
		if (htmlEdit) mime_fix_up_html_address(&references);
		mime_intl_insert_message_header_1(&newBody, &references,
										  HEADER_REFERENCES,
										  MimeGetNamedString(MIME_MHTML_REFERENCES),
										  mailcharset, htmlEdit);
	}
	if (htmlEdit)
	{
		mime_SACat(&newBody, "</TABLE>");
		mime_SACat(&newBody, MSG_LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(&newBody, html_tag+6);
		else
			mime_SACat(&newBody, *body);
	}
	else
	{
		mime_SACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
		mime_SACat(&newBody, *body);
	}
	if (newBody)
	{
		PR_Free(*body);
		*body = newBody;
	}
	PR_FREEIF(subject);
	PR_FREEIF(resent_comments);
	PR_FREEIF(resent_date);
	PR_FREEIF(resent_from);
	PR_FREEIF(resent_to);
	PR_FREEIF(resent_cc);
	PR_FREEIF(date);
	PR_FREEIF(from);
	PR_FREEIF(reply_to);
	PR_FREEIF(organization);
	PR_FREEIF(to);
	PR_FREEIF(cc);
	PR_FREEIF(bcc);
	PR_FREEIF(newsgroups);
	PR_FREEIF(followup_to);
	PR_FREEIF(references);
}

static void 
mime_insert_micro_headers(char            **body,
									        MimeHeaders     *headers,
									        MSG_ComposeFormat composeFormat,
									        char            *mailcharset)
{
	char *newBody = NULL;
	char *subject = MimeHeaders_get(headers, HEADER_SUBJECT, PR_FALSE, PR_FALSE);
	char *from = MimeHeaders_get(headers, HEADER_FROM, PR_FALSE, PR_TRUE);
	char *resent_from = MimeHeaders_get(headers, HEADER_RESENT_FROM, PR_FALSE,
										PR_TRUE); 
	char *date = MimeHeaders_get(headers, HEADER_DATE, PR_FALSE, PR_TRUE);
	char *to = MimeHeaders_get(headers, HEADER_TO, PR_FALSE, PR_TRUE);
	char *cc = MimeHeaders_get(headers, HEADER_CC, PR_FALSE, PR_TRUE);
	char *bcc = MimeHeaders_get(headers, HEADER_BCC, PR_FALSE, PR_TRUE);
	char *newsgroups = MimeHeaders_get(headers, HEADER_NEWSGROUPS, PR_FALSE,
									   PR_TRUE);
	const char *html_tag = PL_strcasestr(*body, "<HTML>");
	PRBool htmlEdit = composeFormat == nsIMsgCompFormat::HTML;
	
	if (!from)
		from = MimeHeaders_get(headers, HEADER_SENDER, PR_FALSE, PR_TRUE);
	if (!resent_from)
		resent_from = MimeHeaders_get(headers, HEADER_RESENT_SENDER, PR_FALSE,
									  PR_TRUE); 
	if (!date)
		date = MimeHeaders_get(headers, HEADER_RESENT_DATE, PR_FALSE, PR_TRUE);
	
	if (htmlEdit)
	{
		mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
	    mime_SACat(&newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 MSG_LINEBREAK MSG_LINEBREAK "-------- Original Message --------");
	}
	if (from)
	{
		if (htmlEdit) 
      mime_fix_up_html_address(&from);
		mime_intl_insert_message_header_1(&newBody, &from, HEADER_FROM,
										MimeGetNamedString(MIME_MHTML_FROM),
										mailcharset, htmlEdit);
	}
	if (subject)
		mime_intl_insert_message_header_1(&newBody, &subject, HEADER_SUBJECT,
										MimeGetNamedString(MIME_MHTML_SUBJECT),
											mailcharset, htmlEdit);
/*
	if (date)
		mime_intl_insert_message_header_1(&newBody, &date, HEADER_DATE,
										MimeGetNamedString(MIME_MHTML_DATE),
										mailcharset, htmlEdit);
*/
	if (resent_from)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_from);
		mime_intl_insert_message_header_1(&newBody, &resent_from,
										HEADER_RESENT_FROM,
										MimeGetNamedString(MIME_MHTML_RESENT_FROM),
										mailcharset, htmlEdit);
	}
	if (to)
	{
		if (htmlEdit) mime_fix_up_html_address(&to);
		mime_intl_insert_message_header_1(&newBody, &to, HEADER_TO,
										MimeGetNamedString(MIME_MHTML_TO),
										mailcharset, htmlEdit);
	}
	if (cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&cc);
		mime_intl_insert_message_header_1(&newBody, &cc, HEADER_CC,
										MimeGetNamedString(MIME_MHTML_CC),
										mailcharset, htmlEdit);
	}
	if (bcc)
	{
		if (htmlEdit) mime_fix_up_html_address(&bcc);
		mime_intl_insert_message_header_1(&newBody, &bcc, HEADER_BCC,
										MimeGetNamedString(MIME_MHTML_BCC),
										mailcharset, htmlEdit);
	}
	if (newsgroups)
		mime_intl_insert_message_header_1(&newBody, &newsgroups, HEADER_NEWSGROUPS,
										MimeGetNamedString(MIME_MHTML_NEWSGROUPS),
										mailcharset, htmlEdit);
	if (htmlEdit)
	{
		mime_SACat(&newBody, "</TABLE>");
		mime_SACat(&newBody, MSG_LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(&newBody, html_tag+6);
		else
			mime_SACat(&newBody, *body);
	}
	else
	{
		mime_SACat(&newBody, MSG_LINEBREAK MSG_LINEBREAK);
		mime_SACat(&newBody, *body);
	}
	if (newBody)
	{
		PR_Free(*body);
		*body = newBody;
	}
	PR_FREEIF(subject);
	PR_FREEIF(from);
	PR_FREEIF(resent_from);
	PR_FREEIF(date);
	PR_FREEIF(to);
	PR_FREEIF(cc);
	PR_FREEIF(bcc);
	PR_FREEIF(newsgroups);

}

static void 
mime_insert_forwarded_message_headers(char            **body, 
												              MimeHeaders     *headers,
												              MSG_ComposeFormat composeFormat,
												              char            *mailcharset)
{
	if (body && *body && headers)
	{
		PRInt32     show_headers = 0;
    nsresult    res;
    
    // convert body from mail charset to UTF-8
    char *utf8 = NULL;
    nsAutoString ucs2;
    if (NS_SUCCEEDED(nsMsgI18NConvertToUnicode(nsCAutoString(mailcharset), nsCAutoString(*body), ucs2))) {
      utf8 = ucs2.ToNewUTF8String();
      if (NULL != utf8) {
        PR_Free(*body);
        *body = utf8;
      }
    }

    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &res); 
    if (NS_SUCCEEDED(res) && prefs)
      res = prefs->GetIntPref("mail.show_headers", &show_headers);

		switch (show_headers)
		{
		case 0:
			mime_insert_micro_headers(body, headers, composeFormat, mailcharset);
			break;
		default:
		case 1:
			mime_insert_normal_headers(body, headers, composeFormat, mailcharset);
			break;
		case 2:
			mime_insert_all_headers(body, headers, composeFormat, mailcharset);
			break;
		}
	}
}

static void PR_CALLBACK
mime_parse_stream_complete (nsMIMESession *stream)
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;
  nsIMsgCompFields *fields = NULL;
  int htmlAction = 0;
  int lineWidth = 0;
  
  char *host = 0;
  char *news_host = 0;
  char *to_and_cc = 0;
  char *re_subject = 0;
  char *new_refs = 0;
  char *from = 0;
  char *repl = 0;
  char *subj = 0;
  char *id = 0;
  char *refs = 0;
  char *to = 0;
  char *cc = 0;
  char *bcc = 0; 
  char *fcc = 0; 
  char *org = 0; 
  char *grps = 0;
  char *foll = 0;
  char *priority = 0;
  char *draftInfo = 0;
  
  PRBool xlate_p = PR_FALSE;	/* #### how do we determine this? */
  PRBool sign_p = PR_FALSE;		/* #### how do we determine this? */
  PRBool forward_inline = PR_FALSE;
  
  NS_ASSERTION (mdd, "null mime draft data");
  
  if (!mdd) return;
  
  if (mdd->obj) 
  {
    int status;
    
    status = mdd->obj->clazz->parse_eof ( mdd->obj, PR_FALSE );
    mdd->obj->clazz->parse_end( mdd->obj, status < 0 ? PR_TRUE : PR_FALSE );
    
    xlate_p = mdd->options->dexlate_p;
    sign_p = mdd->options->signed_p;
    
    // RICHIE
    // We need to figure out how to pass the forwarded flag along with this
    // operation.
    
    //forward_inline = (mdd->format_out != FO_CMDLINE_ATTACHMENTS);    
    forward_inline = mdd->forwardInline;
    if (forward_inline)
    {
#ifdef MOZ_SECURITY
      HG88890
#endif
    }
    
    NS_ASSERTION ( mdd->options == mdd->obj->options, "mime draft options not same as obj->options" );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) 
    {
      // mscott: aren't we leaking a bunch of trings here like the charset strings and such?
      delete mdd->options;
      mdd->options = 0;
    }
    if (mdd->stream) 
    {
      mdd->stream->complete ((nsMIMESession *)mdd->stream->data_object);
      PR_Free( mdd->stream );
      mdd->stream = 0;
    }
  }
  
  //
  // Now, process the attachments that we have gathered from the message
  // on disk
  //
  nsMsgAttachmentData     *newAttachData = nsnull;
  if (mdd && (mdd->attachments_count > 0))
    newAttachData = mime_draft_process_attachments(mdd);
  
  //
  // time to bring up the compose windows with all the info gathered 
  //
  if ( mdd->headers )
  {
    subj = MimeHeaders_get(mdd->headers, HEADER_SUBJECT,  PR_FALSE, PR_FALSE);
    if (forward_inline)
    {
      if (subj)
      {
        char *newSubj = PR_smprintf("[Fwd: %s]", subj);
        if (newSubj)
        {
          PR_Free(subj);
          subj = newSubj;
        }
      }
    }	
    else
    {
      repl = MimeHeaders_get(mdd->headers, HEADER_REPLY_TO, PR_FALSE, PR_FALSE);
      to   = MimeHeaders_get(mdd->headers, HEADER_TO,       PR_FALSE, PR_TRUE);
      cc   = MimeHeaders_get(mdd->headers, HEADER_CC,       PR_FALSE, PR_TRUE);
      bcc   = MimeHeaders_get(mdd->headers, HEADER_BCC,       PR_FALSE, PR_TRUE);
      
      /* These headers should not be RFC-1522-decoded. */
      grps = MimeHeaders_get(mdd->headers, HEADER_NEWSGROUPS,  PR_FALSE, PR_TRUE);
      foll = MimeHeaders_get(mdd->headers, HEADER_FOLLOWUP_TO, PR_FALSE, PR_TRUE);
      
      host = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_NEWSHOST, PR_FALSE, PR_FALSE);
      if (!host)
        host = MimeHeaders_get(mdd->headers, HEADER_NNTP_POSTING_HOST, PR_FALSE, PR_FALSE);
      
      id   = MimeHeaders_get(mdd->headers, HEADER_MESSAGE_ID,  PR_FALSE, PR_FALSE);
      refs = MimeHeaders_get(mdd->headers, HEADER_REFERENCES,  PR_FALSE, PR_TRUE);
      priority = MimeHeaders_get(mdd->headers, HEADER_X_PRIORITY, PR_FALSE, PR_FALSE);
      
      mime_intl_mimepart_2_str(&repl, mdd->mailcharset);
      mime_intl_mimepart_2_str(&to, mdd->mailcharset);
      mime_intl_mimepart_2_str(&cc, mdd->mailcharset);
      mime_intl_mimepart_2_str(&bcc, mdd->mailcharset);
      mime_intl_mimepart_2_str(&grps, mdd->mailcharset);
      mime_intl_mimepart_2_str(&foll, mdd->mailcharset);
      mime_intl_mimepart_2_str(&host, mdd->mailcharset);
      
      if (host) 
      {
        char *secure = NULL;
        
        secure = PL_strcasestr(host, "secure");
        if (secure) 
        {
          *secure = 0;
          news_host = PR_smprintf ("snews://%s", host);
        }
        else 
        {
          news_host = PR_smprintf ("news://%s", host);
        }
      }
    }
    
    mime_intl_mimepart_2_str(&subj, mdd->mailcharset);
    
    fields = CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
      org, subj, refs, 0, priority, 0, news_host,
      xlate_p, sign_p, mdd->mailcharset);
    
    draftInfo = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_DRAFT_INFO, PR_FALSE, PR_FALSE);
    if (draftInfo && fields && !forward_inline) 
    {
      char *parm = 0;
      parm = MimeHeaders_get_parameter(draftInfo, "vcard", NULL, NULL);
      if (parm && !nsCRT::strcmp(parm, "1"))
        fields->SetBoolHeader(nsMsg_ATTACH_VCARD_BOOL_HEADER_MASK, PR_TRUE);
      else
        fields->SetBoolHeader(nsMsg_ATTACH_VCARD_BOOL_HEADER_MASK, PR_FALSE);
      
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "receipt", NULL, NULL);
      if (parm && !nsCRT::strcmp(parm, "0"))
        fields->SetBoolHeader(nsMsg_RETURN_RECEIPT_BOOL_HEADER_MASK, PR_FALSE); 
      else
      {
        int receiptType = 0;
        fields->SetBoolHeader(nsMsg_RETURN_RECEIPT_BOOL_HEADER_MASK, PR_TRUE); 
        sscanf(parm, "%d", &receiptType);
        fields->SetReturnReceipt((PRInt32) receiptType);
      }
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "uuencode", NULL, NULL);
      if (parm && !nsCRT::strcmp(parm, "1"))
        fields->SetBoolHeader(nsMsg_UUENCODE_BINARY_BOOL_HEADER_MASK, PR_TRUE);
      else
        fields->SetBoolHeader(nsMsg_UUENCODE_BINARY_BOOL_HEADER_MASK, PR_FALSE);
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "html", NULL, NULL);
      if (parm)
        sscanf(parm, "%d", &htmlAction);
      PR_FREEIF(parm);
      parm = MimeHeaders_get_parameter(draftInfo, "linewidth", NULL, NULL);
      if (parm)
        sscanf(parm, "%d", &lineWidth);
      PR_FREEIF(parm);
      
    }
    
    if (mdd->messageBody) 
    {
      char *body;
      PRUint32 bodyLen = 0;
      MSG_ComposeFormat composeFormat = nsIMsgCompFormat::Default;
      
      bodyLen = mdd->messageBody->file_spec->GetFileSize();
      body = (char *)PR_MALLOC (bodyLen + 1);
      if (body)
      {
        nsCRT::memset (body, 0, bodyLen+1);
        
        nsInputFileStream inputFile(*(mdd->messageBody->file_spec));
        if (inputFile.is_open())
          inputFile.read(body, bodyLen);
        
        inputFile.close();
        
        if (mdd->messageBody->type && *mdd->messageBody->type)
        {
          if( PL_strcasestr(mdd->messageBody->type, "text/html") != NULL )
            composeFormat = nsIMsgCompFormat::HTML;
          else if ( ( PL_strcasestr(mdd->messageBody->type, "text/plain") != NULL ) ||
            ( PL_strcasecmp(mdd->messageBody->type, "text") == 0 ) )
            composeFormat = nsIMsgCompFormat::PlainText;
        }
        else
        {
          composeFormat = nsIMsgCompFormat::PlainText;
        }
        
        // Since we have body text, then we should set the compose fields with
        // this data.      
        //       if (composeFormat == nsIMsgCompFormat::PlainText)
        //         fields->SetTheForcePlainText(PR_TRUE);
        
        if (forward_inline)
        {
          if (mdd->identity)
          {
            PRBool bFormat;
            mdd->identity->GetComposeHtml(&bFormat);
            if (bFormat)
            {
              if (body && composeFormat == nsIMsgCompFormat::PlainText)
              {
                char* newbody = (char *)PR_MALLOC (bodyLen + 12); //+11 chars for <pre> & </pre> tags
                if (newbody)
                {
                  *newbody = 0;
                  PL_strcat(newbody, "<PRE>");
                  PL_strcat(newbody, body);
                  PL_strcat(newbody, "</PRE>");
                  PR_Free(body);
                  body = newbody;
                }

                composeFormat = nsIMsgCompFormat::HTML;
              }
            }
          }
          
          mime_insert_forwarded_message_headers(&body, mdd->headers, composeFormat,
            mdd->mailcharset);
        }
        // setting the charset while we are creating the composition fields
        //fields->SetCharacterSet(NS_ConvertASCIItoUCS2(mdd->mailcharset));
        
        // Ok, if we are here, then we should look at the charset and convert
        // to UTF-8...
        //
        if (!forward_inline)
        {
          char *bodyCharset = MimeHeaders_get_parameter (mdd->messageBody->type, "charset", NULL, NULL);
          if (bodyCharset)
          {
            // Now do conversion to UTF-8 for output
            char  *convertedString = nsnull;
            PRInt32 convertedStringLen;
            PRInt32 res = MIME_ConvertCharset(PR_FALSE, bodyCharset, "UTF-8", body, nsCRT::strlen(body), 
                                              &convertedString, &convertedStringLen, NULL);
            if (res == 0)
            {
              PR_FREEIF(body);
              body = convertedString;
            }  

            PR_FREEIF(bodyCharset);
          }
        }

        // convert from UTF-8 to UCS2
        nsString ucs2;
        if (NS_SUCCEEDED(nsMsgI18NConvertToUnicode(nsCAutoString("UTF-8"), nsCAutoString(body), ucs2)))
          fields->SetBody(ucs2.GetUnicode());
        else
          fields->SetBody(NS_ConvertASCIItoUCS2(body).GetUnicode());
        
        PR_FREEIF(body);
      } // end if (messageBody)
      
      //
      // At this point, we need to create a message compose window or editor
      // window via XP-COM with the information that we have retrieved from 
      // the message store.
      //
      if (mdd->format_out == nsMimeOutput::nsMimeMessageEditorTemplate)
      {
#ifdef NS_DEBUG
        printf("RICHIE: Time to create the EDITOR with this template - HAS a body!!!!\n");
#endif
        CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::Template, composeFormat, mdd->identity);
      }
      else
      {
#ifdef NS_DEBUG
        printf("Time to create the composition window WITH a body!!!!\n");
#endif
        if (mdd->forwardInline)
          CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::ForwardInline, composeFormat, mdd->identity);
        else
        {
          nsString urlStr; urlStr.AssignWithConversion(mdd->url_name);
          fields->SetDraftId(urlStr.GetUnicode());
          CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::Draft, composeFormat, mdd->identity);
        }
      }
      
      PR_FREEIF(body);
      mime_free_attachments (mdd->messageBody, 1);
    }
    else
    {
      //
      // At this point, we need to create a message compose window via
      // XP-COM with the information that we have retrieved from the message store.
      //
      if (mdd->format_out == nsMimeOutput::nsMimeMessageEditorTemplate)
      {
#ifdef NS_DEBUG
        printf("RICHIE: Time to create the EDITOR with this template - NO body!!!!\n");
#endif
        CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::Template, nsIMsgCompFormat::Default, mdd->identity);
      }
      else
      {
#ifdef NS_DEBUG
        printf("Time to create the composition window WITHOUT a body!!!!\n");
#endif
        if (mdd->forwardInline)
          CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::ForwardInline, nsIMsgCompFormat::Default, mdd->identity);
        else
        {
          nsString urlStr; urlStr.AssignWithConversion(mdd->url_name);
          fields->SetDraftId(urlStr.GetUnicode());
          CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::Draft, nsIMsgCompFormat::Default, mdd->identity);
        }
      }
    }    
  }
  else
  {
    fields = CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
      org, subj, refs, 0, priority, 0, news_host,
      GetMailXlateionPreference(), 
      GetMailSigningPreference(),
      mdd->mailcharset);
    if (fields)
      CreateTheComposeWindow(fields, newAttachData, nsIMsgCompType::New, nsIMsgCompFormat::Default, mdd->identity);
  }
  
  if ( mdd->headers )
    MimeHeaders_free ( mdd->headers );
  
  // 
  // Free the original attachment structure...
  // Make sure we only cleanup the local copy of the memory and not kill 
  // files we need on disk
  //
  if (mdd->attachments)
  {
    int               i;
    nsMsgAttachedFile *cur = mdd->attachments;
    
    for ( i = 0; i < mdd->attachments_count; i++, cur++ ) 
    {
      if ( cur->file_spec ) 
      {
        delete ( cur->file_spec );
        cur->file_spec = nsnull;
      }
    }
    
    mime_free_attachments( mdd->attachments, mdd->attachments_count );
  }
  
  PR_FREEIF(mdd);
  if (fields)
    NS_RELEASE(fields);
  
  // Release the prefs service
  MimeObject *obj = (mdd ? mdd->obj : 0);  
  if ( (obj) && (obj->options) && (obj->options->prefs) )
    nsServiceManager::ReleaseService(kPrefCID, obj->options->prefs);
  
  PR_Free (mdd);
  
  PR_FREEIF(host);
  PR_FREEIF(to_and_cc);
  PR_FREEIF(re_subject);
  PR_FREEIF(new_refs);
  PR_FREEIF(from);
  PR_FREEIF(repl);
  PR_FREEIF(subj);
  PR_FREEIF(id);
  PR_FREEIF(refs);
  PR_FREEIF(to);
  PR_FREEIF(cc);
  PR_FREEIF(grps);
  PR_FREEIF(foll);
  PR_FREEIF(priority);
  PR_FREEIF(draftInfo);  
}

static void PR_CALLBACK
mime_parse_stream_abort (nsMIMESession *stream, int status )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;  
  NS_ASSERTION (mdd, "null mime draft data");

  if (!mdd) 
    return;
  
  if (mdd->obj) 
  {
    int status=0;

    if ( !mdd->obj->closed_p )
      status = mdd->obj->clazz->parse_eof ( mdd->obj, PR_TRUE );
    if ( !mdd->obj->parsed_p )
      mdd->obj->clazz->parse_end( mdd->obj, PR_TRUE );
    
    NS_ASSERTION ( mdd->options == mdd->obj->options, "draft display options not same as mime obj" );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) 
    {
      delete mdd->options;
      mdd->options = 0;
    }

    if (mdd->stream) 
    {
      mdd->stream->abort ((nsMIMESession *)mdd->stream->data_object, status);
      PR_Free( mdd->stream );
      mdd->stream = 0;
    }
  }

  if ( mdd->headers )
    MimeHeaders_free (mdd->headers);


  if (mdd->attachments)
    mime_free_attachments( mdd->attachments, mdd->attachments_count );

  PR_Free (mdd);
}

static int
make_mime_headers_copy ( void *closure, MimeHeaders *headers )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) closure;

  NS_ASSERTION ( mdd && headers, "null mime draft data and/or headers" );
  
  if ( !mdd || ! headers ) 
    return 0;

  NS_ASSERTION ( mdd->headers == NULL , "non null mime draft data headers");

  mdd->headers = MimeHeaders_copy ( headers );
  mdd->options->done_parsing_outer_headers = PR_TRUE;

  return 0;
}

int 
mime_decompose_file_init_fn ( void *stream_closure, MimeHeaders *headers )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  nsMsgAttachedFile *attachments = 0, *newAttachment = 0;
  int nAttachments = 0;
  //char *hdr_value = NULL;
  char *parm_value = NULL;
  PRBool needURL = PR_FALSE;
  PRBool creatingMsgBody = PR_TRUE;
  PRBool bodyPart = PR_FALSE;
  
  NS_ASSERTION (mdd && headers, "null mime draft data and/or headers");
  if (!mdd || !headers) 
    return -1;

  if ( !mdd->options->is_multipart_msg ) 
  {
    if (mdd->options->decompose_init_count) 
    {
      mdd->options->decompose_init_count++;
      NS_ASSERTION(mdd->curAttachment, "missing attachment in mime_decompose_file_init_fn");
      if (mdd->curAttachment) {
        char *ct = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, PR_TRUE, PR_FALSE);
        if (ct)
          mime_SACopy(&(mdd->curAttachment->type), ct);
        PR_FREEIF(ct);
      }
      return 0;
    }
    else {
      mdd->options->decompose_init_count++;
    }
  }

  nAttachments = mdd->attachments_count;

  if (!nAttachments && !mdd->messageBody) 
  {
    // if we've been told to use an override charset then do so....otherwise use the charset
    // inside the message header...
    if (mdd->options && mdd->options->override_charset)
        mdd->mailcharset = nsCRT::strdup(mdd->options->override_charset);
    else
    {
      char *contentType = NULL;
      contentType = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
      if (contentType) 
      {
        mdd->mailcharset = MimeHeaders_get_parameter(contentType, "charset", NULL, NULL);
        PR_FREEIF(contentType);
      }
    }
    
    mdd->messageBody = PR_NEWZAP (nsMsgAttachedFile);
    NS_ASSERTION (mdd->messageBody, "missing messageBody in mime_decompose_file_init_fn");
    if (!mdd->messageBody) 
      return MIME_OUT_OF_MEMORY;
    newAttachment = mdd->messageBody;
    creatingMsgBody = PR_TRUE;
    bodyPart = PR_TRUE;
  }
  else 
  {
    /* always allocate one more extra; don't ask me why */
    needURL = PR_TRUE;
    if ( nAttachments ) 
    {
      NS_ASSERTION (mdd->attachments, "no attachments");
      
      attachments = (nsMsgAttachedFile *)PR_REALLOC(mdd->attachments, 
								                                    sizeof (nsMsgAttachedFile) * 
                                                    (nAttachments + 2));
    if (!attachments)
        return MIME_OUT_OF_MEMORY;
      mdd->attachments = attachments;
      mdd->attachments_count++;
    }
    else {
      NS_ASSERTION (!mdd->attachments, "have attachments but count is 0");
      
      attachments = (nsMsgAttachedFile *) PR_MALLOC ( sizeof (nsMsgAttachedFile) * 2);
      if (!attachments) 
        return MIME_OUT_OF_MEMORY;
      mdd->attachments_count++;
      mdd->attachments = attachments;
    }
    
    newAttachment = attachments + nAttachments;
    nsCRT::memset ( newAttachment, 0, sizeof (nsMsgAttachedFile) * 2 );
  }

  char *workURLSpec = nsnull;
  char *contLoc = nsnull;

  newAttachment->real_name = MimeHeaders_get_name ( headers );
  contLoc = MimeHeaders_get( headers, HEADER_CONTENT_LOCATION, PR_FALSE, PR_FALSE );
  if (!contLoc)
      contLoc = MimeHeaders_get( headers, HEADER_CONTENT_BASE, PR_FALSE, PR_FALSE );

  if ( (!contLoc) && (newAttachment->real_name) )
    workURLSpec = nsCRT::strdup(newAttachment->real_name);
  if ( (contLoc) && (!workURLSpec) )
    workURLSpec = nsCRT::strdup(contLoc);

  PR_FREEIF(contLoc);

  mdd->curAttachment = newAttachment;  
  newAttachment->type =  MimeHeaders_get ( headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE );
  
  //
  // This is to handle the degenerated Apple Double attachment.
  //
  parm_value = MimeHeaders_get( headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE );
  if (parm_value) 
  {
    char *boundary = NULL;
    char *tmp_value = NULL;
    boundary = MimeHeaders_get_parameter(parm_value, "boundary", NULL, NULL);
    if (boundary)
      tmp_value = PR_smprintf("; boundary=\"%s\"", boundary);
    if (tmp_value)
      mime_SACat(&(newAttachment->type), tmp_value);
    newAttachment->x_mac_type = MimeHeaders_get_parameter(parm_value, "x-mac-type", NULL, NULL);
    newAttachment->x_mac_creator = MimeHeaders_get_parameter(parm_value, "x-mac-creator", NULL, NULL);
    PR_FREEIF(parm_value);
    PR_FREEIF(boundary);
    PR_FREEIF(tmp_value);
  }
  newAttachment->encoding = MimeHeaders_get ( headers, HEADER_CONTENT_TRANSFER_ENCODING,
                                              PR_FALSE, PR_FALSE );
  newAttachment->description = MimeHeaders_get( headers, HEADER_CONTENT_DESCRIPTION,
                                                PR_FALSE, PR_FALSE );
  // 
  // If we came up empty for description or the orig URL, we should do something about it.
  //
  if  ( ( (!newAttachment->description) || (!*newAttachment->description) ) && (workURLSpec) )
    newAttachment->description = nsCRT::strdup(workURLSpec);

  nsFileSpec *tmpSpec = nsnull;
  if (newAttachment->real_name && *newAttachment->real_name)
  {
  	//Need some convertion to native file system character set
  	nsAutoString outStr;
  	char * fileName = nsnull;
  	nsresult rv = ConvertToUnicode(NS_ConvertASCIItoUCS2(msgCompHeaderInternalCharset()), newAttachment->real_name, outStr);
  	if (NS_SUCCEEDED(rv))
  	{
  		rv = ConvertFromUnicode(nsMsgI18NFileSystemCharset(), outStr, &fileName);
  		if (NS_FAILED(rv))
  			fileName = PL_strdup(newAttachment->real_name);
   	}
   	else
  		fileName = PL_strdup(newAttachment->real_name);

    tmpSpec = nsMsgCreateTempFileSpec(fileName);
  	PR_FREEIF(fileName);
  }
  else
  {
    // if we didn't get a real name for the attachment, then we should
    // ask the mime service what extension this content type should use and use nsmail.that file extension
    // if that doesn't work then just use "nsmail.tmp"

    nsCAutoString  newAttachName ("nsmail");
    PRBool extensionAdded = PR_FALSE;
    // the content type may contain a charset. i.e. text/html; ISO-2022-JP...we want to strip off the charset
    // before we ask the mime service for a mime info for this content type.
    nsCAutoString contentType (newAttachment->type);
    PRInt32 pos = contentType.FindCharInSet(";");
    if (pos > 0)
      contentType.Truncate(pos);
    nsresult  rv = NS_OK;
    NS_WITH_SERVICE(nsIMIMEService, mimeFinder, kMimeServiceCID, &rv); 
    if (NS_SUCCEEDED(rv) && mimeFinder) 
    {
      nsCOMPtr<nsIMIMEInfo> mimeInfo = nsnull;
      rv = mimeFinder->GetFromMIMEType(contentType, getter_AddRefs(mimeInfo));
      if (NS_SUCCEEDED(rv) && mimeInfo) 
      {
        nsXPIDLCString fileExtension;

        if ( (NS_SUCCEEDED(mimeInfo->FirstExtension(getter_Copies(fileExtension)))) && fileExtension)
        {
          newAttachName.Append(".");
          newAttachName.Append(fileExtension);
          extensionAdded = PR_TRUE;
        }
      }        
    }

    if (!extensionAdded)
    {
      newAttachName.Append(".tmp");
    }

    tmpSpec = nsMsgCreateTempFileSpec(newAttachName);
  }

  // This needs to be done so the attachment structure has a handle 
  // on the temp file for this attachment...
  // if ( (tmpSpec) && (!bodyPart) )
  if (tmpSpec)
  {
      nsFileURL fileURL(*tmpSpec);
      const char * tempSpecStr = fileURL.GetURLString();

      nsMimeNewURI(&(newAttachment->orig_url), tempSpecStr, nsnull);
      NS_IF_ADDREF(newAttachment->orig_url);
  }

  PR_FREEIF(workURLSpec);
  if (!tmpSpec)
    return MIME_OUT_OF_MEMORY;

  NS_NewFileSpecWithSpec(*tmpSpec, &mdd->tmpFileSpec);
	if (!mdd->tmpFileSpec)
    return MIME_OUT_OF_MEMORY;
  
  newAttachment->file_spec = tmpSpec;

  mdd->tmpFileStream = new nsOutputFileStream(mdd->tmpFileSpec);
  if (!mdd->tmpFileStream)
    return MIME_UNABLE_TO_OPEN_TMP_FILE;
    
  // For now, we are always going to decode all of the attachments 
  // for the message. This way, we have native data 
  if (creatingMsgBody) 
  {
    MimeDecoderData *(*fn) (int (*) (const char*, PRInt32, void*), void*) = 0;
    
    //
    // Initialize a decoder if necessary.
    //
    if (!newAttachment->encoding || mdd->options->dexlate_p)
      ;
    else if (!nsCRT::strcasecmp(newAttachment->encoding, ENCODING_BASE64))
      fn = &MimeB64DecoderInit;
    else if (!nsCRT::strcasecmp(newAttachment->encoding, ENCODING_QUOTED_PRINTABLE))
      fn = &MimeQPDecoderInit;
    else if (!nsCRT::strcasecmp(newAttachment->encoding, ENCODING_UUENCODE) ||
             !nsCRT::strcasecmp(newAttachment->encoding, ENCODING_UUENCODE2) ||
             !nsCRT::strcasecmp(newAttachment->encoding, ENCODING_UUENCODE3) ||
             !nsCRT::strcasecmp(newAttachment->encoding, ENCODING_UUENCODE4))
      fn = &MimeUUDecoderInit;
    
    if (fn) 
    {
      mdd->decoder_data = fn (/* The (int (*) ...) cast is to turn the `void' argument into `MimeObject'. */
                              ((int (*) (const char *, PRInt32, void *))
                              dummy_file_write), mdd->tmpFileStream);
      if (!mdd->decoder_data)
        return MIME_OUT_OF_MEMORY;
    }
  }

  return 0;
}

int 
mime_decompose_file_output_fn (char     *buf,
								               PRInt32  size,
								               void     *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  int ret = 0;
  
  NS_ASSERTION (mdd && buf, "missing mime draft data and/or buf");
  if (!mdd || !buf) return -1;
  if (!size) return 0;
  
  if ( !mdd->tmpFileStream ) 
    return 0;
  
  if (mdd->decoder_data) {
    ret = MimeDecoderWrite(mdd->decoder_data, buf, size);
    if (ret == -1) return -1;
  }
  else 
  {
    ret = mdd->tmpFileStream->write(buf, size);
    if (ret < size)
      return MIME_ERROR_WRITING_FILE;
  }
  
  return 0;
}

int
mime_decompose_file_close_fn ( void *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  
  if ( !mdd || !mdd->tmpFileStream )
    return -1;
  
  if ( !mdd->options->is_multipart_msg ) 
  {
    if ( --mdd->options->decompose_init_count > 0 )
      return 0;
  }
  
  if (mdd->decoder_data) {
    MimeDecoderDestroy(mdd->decoder_data, PR_FALSE);
    mdd->decoder_data = 0;
  }

  if (mdd->tmpFileStream->GetIStream())
    mdd->tmpFileStream->close();
  delete mdd->tmpFileStream;
  delete mdd->tmpFileSpec;
  mdd->tmpFileSpec = nsnull;
  
  return 0;
}

extern "C" void  *
mime_bridge_create_draft_stream(
                          nsIMimeEmitter      *newEmitter,
                          nsStreamConverter   *newPluginObj2,
                          nsIURI              *uri,
                          nsMimeOutputType    format_out)
{
  int                     status = 0;
  nsMIMESession           *stream = NULL;
  struct mime_draft_data  *mdd = NULL;
  MimeObject              *obj;

  if ( !uri ) 
    return nsnull;

  mdd = PR_NEWZAP(struct mime_draft_data);
  if (!mdd) 
    return nsnull;

  // first, convert the rdf msg uri into a url that represents the message...
  char *turl;
  if (NS_FAILED(uri->GetSpec(&turl)))
    return nsnull;

  nsIMsgMessageService * msgService = nsnull;
  nsresult rv = GetMessageServiceFromURI(turl, &msgService);
  if (NS_FAILED(rv)) 
    return nsnull;

  nsCOMPtr<nsIURI> aURL;
  rv = msgService->GetUrlForUri(turl, getter_AddRefs(aURL), nsnull);
  if (NS_FAILED(rv)) 
    return nsnull;

  nsXPIDLCString urlString;
  if (NS_SUCCEEDED(aURL->GetSpec(getter_Copies(urlString))))
  {
    mdd->url_name = nsCRT::strdup(urlString);
    if (!(mdd->url_name))
    {
      PR_FREEIF(mdd);
      return nsnull;
    }
  }

  ReleaseMessageServiceFromURI(turl, msgService);

  newPluginObj2->GetForwardInline(&mdd->forwardInline);
  newPluginObj2->GetIdentity(getter_AddRefs(mdd->identity));
  mdd->format_out = format_out;
  mdd->options = new  MimeDisplayOptions ;
  if ( !mdd->options ) 
  {
    PR_FREEIF(mdd->url_name);
    PR_FREEIF(mdd);
    return nsnull;
  }

  mdd->options->url = nsCRT::strdup(mdd->url_name);
  mdd->options->format_out = format_out;     // output format
  mdd->options->decompose_file_p = PR_TRUE;	/* new field in MimeDisplayOptions */
  mdd->options->stream_closure = mdd;
  mdd->options->html_closure = mdd;
  mdd->options->decompose_headers_info_fn = make_mime_headers_copy;
  mdd->options->decompose_file_init_fn = mime_decompose_file_init_fn;
  mdd->options->decompose_file_output_fn = mime_decompose_file_output_fn;
  mdd->options->decompose_file_close_fn = mime_decompose_file_close_fn;

  rv = nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), (nsISupports**)&(mdd->options->prefs));
  if (! (mdd->options->prefs && NS_SUCCEEDED(rv)))
	{
    PR_FREEIF(mdd);
    return nsnull;
  }

#ifdef FO_MAIL_MESSAGE_TO
  /* If we're attaching a message (for forwarding) then we must eradicate all
	 traces of xlateion from it, since forwarding someone else a message
	 that wasn't xlated for them doesn't work.  We have to dexlate it
	 before sending it.
   */
// RICHIE  mdd->options->dexlate_p = PR_TRUE;
#endif /* FO_MAIL_MESSAGE_TO */

  obj = mime_new ( (MimeObjectClass *) &mimeMessageClass, (MimeHeaders *) NULL, MESSAGE_RFC822 );
  if ( !obj ) 
  {
    PR_FREEIF(mdd->url_name);
    delete mdd->options;
    PR_FREEIF(mdd );
    return nsnull;
  }
  
  obj->options = mdd->options;
  mdd->obj = obj;

  stream = PR_NEWZAP ( nsMIMESession );
  if ( !stream ) 
  {
    PR_FREEIF(mdd->url_name);
    delete mdd->options;
    PR_FREEIF ( mdd );
    PR_FREEIF ( obj );
    return nsnull;
  }

  stream->name = "MIME To Draft Converter Stream";
  stream->complete = mime_parse_stream_complete;
  stream->abort = mime_parse_stream_abort;
  stream->put_block = mime_parse_stream_write;
  stream->data_object = mdd;

  status = obj->clazz->initialize ( obj );
  if ( status >= 0 )
    status = obj->clazz->parse_begin ( obj );
  if ( status < 0 ) 
  {
    PR_FREEIF(mdd->url_name);
    PR_FREEIF ( stream );
    delete mdd->options;
    PR_FREEIF ( mdd );
    PR_FREEIF ( obj );
    return nsnull;
  }

  return stream;
}
