/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "libi18n.h"
#include "msgcom.h"
#include "mimeobj.h"
#include "mimemsg.h"
#include "mimeenc.h" /* jrm - needed to make it build 97/02/21 */
#include "prefapi.h"
#include "mimebuf.h"

#include "prmem.h"
#include "plstr.h"
#include "prio.h"
#include "nsIPref.h"
#include "msgCore.h"
#include "nsCRT.h"


#define HEADER_NNTP_POSTING_HOST             "NNTP-Posting-Host"
#define MIME_HEADER_TABLE "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>"
#define HEADER_START_JUNK  "<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>"
#define HEADER_MIDDLE_JUNK ": </TH><TD>"
#define HEADER_END_JUNK    "</TD></TR>"

extern "C" int MK_MIMEHTML_DISP_SUBJECT;
extern "C" int MK_MIMEHTML_DISP_RESENT_COMMENTS;
extern "C" int MK_MIMEHTML_DISP_RESENT_DATE;
extern "C" int MK_MIMEHTML_DISP_RESENT_SENDER;
extern "C" int MK_MIMEHTML_DISP_RESENT_FROM;
extern "C" int MK_MIMEHTML_DISP_RESENT_TO;
extern "C" int MK_MIMEHTML_DISP_RESENT_CC;
extern "C" int MK_MIMEHTML_DISP_DATE;
extern "C" int MK_MIMEHTML_DISP_SENDER;
extern "C" int MK_MIMEHTML_DISP_FROM;
extern "C" int MK_MIMEHTML_DISP_REPLY_TO;
extern "C" int MK_MIMEHTML_DISP_ORGANIZATION;
extern "C" int MK_MIMEHTML_DISP_TO;
extern "C" int MK_MIMEHTML_DISP_CC;
extern "C" int MK_MIMEHTML_DISP_BCC;
extern "C" int MK_MIMEHTML_DISP_NEWSGROUPS;
extern "C" int MK_MIMEHTML_DISP_FOLLOWUP_TO;
extern "C" int MK_MIMEHTML_DISP_REFERENCES;
extern "C" int MK_MIMEHTML_DISP_NAME;
extern "C" int MK_MIMEHTML_DISP_TYPE;
extern "C" int MK_MIMEHTML_DISP_ENCODING;
extern "C" int MK_MIMEHTML_DISP_DESCRIPTION;
extern "C" int MK_UNABLE_TO_OPEN_TMP_FILE;
extern "C" int MK_MIME_ERROR_WRITING_FILE;

extern "C" char *
MIME_StripContinuations(char *original);

int 
mime_decompose_file_init_fn ( void *stream_closure,
							  MimeHeaders *headers );


int 
mime_decompose_file_output_fn ( char *buf,
								PRInt32 size,
								void *stream_closure );

int
mime_decompose_file_close_fn ( void *stream_closure );

extern int
MimeHeaders_build_heads_list(MimeHeaders *hdrs);

/* This struct is the state we used in MIME_ToDraftConverter() */
struct mime_draft_data {
  URL_Struct *url;                         /* original url */
  int format_out;                          /* intended output format; 
											  should be FO_OPEN_DRAFT */
  MWContext *context;
  nsMIMESession *stream;                 /* not used for now */
  MimeObject *obj;                         /* The root */
  MimeDisplayOptions *options;             /* data for communicating with libmime.a */
  MimeHeaders *headers;                    /* copy of outer most mime header */
  int attachments_count;                   /* how many attachments we have */
  MSG_AttachedFile *attachments;           /* attachments */
  MSG_AttachedFile *messageBody;           /* message body */
  MSG_AttachedFile *curAttachment;		   /* temp */
  char *tmp_file_name;                     /* current opened file for output */
  PRFileDesc *tmp_file;                    /* output file handle */
  MimeDecoderData *decoder_data;
  PRInt16	mailcsid;							/* get it from CHARSET of Content-Type and convert to csid */
};


static int
dummy_file_write( char *buf, PRInt32 size, void *fileHandle )
{
  return PR_Write((PRFileDesc *) fileHandle, buf, size);
}

static int
mime_parse_stream_write ( void *stream,
			  const char *buf,
			  PRInt32 size )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream;  
  PR_ASSERT ( mdd );

  if ( !mdd || !mdd->obj ) return -1;

  return mdd->obj->class->parse_buffer ((char *) buf, size, mdd->obj);
}

static unsigned int
mime_parse_stream_write_ready ( void *stream )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream;  
  PR_ASSERT (mdd);

  if (!mdd) return MAX_WRITE_READY;
  if (mdd->stream)
    return mdd->stream->is_write_ready ( mdd->stream->data_object );
  else
    return MAX_WRITE_READY;
}

static void
mime_free_attachments ( MSG_AttachedFile *attachments,
						int count )
{
  int i;
  MSG_AttachedFile *cur = attachments;

  PR_ASSERT ( attachments && count > 0 );
  if ( !attachments || count <= 0 ) return;

  for ( i = 0; i < count; i++, cur++ ) {
	PR_FREEIF ( cur->orig_url );
	PR_FREEIF ( cur->type );
	PR_FREEIF ( cur->encoding );
	PR_FREEIF ( cur->description );
	PR_FREEIF ( cur->x_mac_type );
	PR_FREEIF ( cur->x_mac_creator );
	if ( cur->file_name ) {
	  PR_Delete( cur->file_name );
	  PR_FREEIF ( cur->file_name );
	}
  }
  PR_Free ( attachments );
}

static int
mime_draft_process_attachments ( struct mime_draft_data *mdd,
								 MSG_Pane *cpane )
{
  struct MSG_AttachmentData *attachData = NULL, *tmp = NULL;
  struct MSG_AttachedFile *tmpFile = NULL;
  int i;

  PR_ASSERT ( mdd->attachments_count && mdd->attachments );

  if ( !mdd->attachments || !mdd->attachments_count )
	return -1;

  attachData = PR_MALLOC( ( (mdd->attachments_count+1) * sizeof (MSG_AttachmentData) ) );
  if ( !attachData ) return MK_OUT_OF_MEMORY;

  memset ( attachData, 0, (mdd->attachments_count+1) * sizeof (MSG_AttachmentData) );

  tmpFile = mdd->attachments;
  tmp = attachData;

  for ( i=0; i < mdd->attachments_count; i++, tmp++, tmpFile++ ) {
	if (tmpFile->type) {
		if (PL_strcasecmp ( tmpFile->type, "text/x-vcard") == 0)
			mime_SACopy (&(tmp->real_name), tmpFile->description);
	}
	if ( tmpFile->orig_url ) {
	  mime_SACopy ( &(tmp->url), tmpFile->orig_url );
	  if (!tmp->real_name)
	  mime_SACopy ( &(tmp->real_name), tmpFile->orig_url );
	}
	if ( tmpFile->type ) {
	  mime_SACopy ( &(tmp->desired_type), tmpFile->type );
	  mime_SACopy ( &(tmp->real_type), tmpFile->type );
	}
	if ( tmpFile->encoding ) {
	  mime_SACopy ( &(tmp->real_encoding), tmpFile->encoding );
	}
	if ( tmpFile->description ) {
	  mime_SACopy ( &(tmp->description), tmpFile->description );
	}
	if ( tmpFile->x_mac_type ) {
	  mime_SACopy ( &(tmp->x_mac_type), tmpFile->x_mac_type );
	}
	if ( tmpFile->x_mac_creator ) {
	  mime_SACopy ( &(tmp->x_mac_creator), tmpFile->x_mac_creator );
	}
  }

  MSG_SetPreloadedAttachments ( cpane, MSG_GetContext (cpane), attachData,
								mdd->attachments, mdd->attachments_count );

  PR_Free (attachData);

  return 0;

}

static void mime_fix_up_html_address( char **addr)
{
	/* We need to replace paired <> they are treated as HTML tag */
	if (addr && *addr && 
		PL_strchr(*addr, '<') && PL_strchr(*addr, '>'))
	{
		char *lt = NULL;
		PRInt32 newLen = 0;
		do 
		{
			newLen = PL_strlen(*addr) + 3 + 1;
			*addr = (char *) PR_REALLOC(*addr, newLen);
			PR_ASSERT (*addr);
			lt = PL_strchr(*addr, '<');
			PR_ASSERT(lt);
      nsCRT::memmove(lt+4, lt+1, newLen - 4 - (lt - *addr));
			*lt++ = '&';
			*lt++ = 'l';
			*lt++ = 't';
			*lt = ';';
		} while (PL_strchr(*addr, '<'));
	}
}

static void  mime_intl_mimepart_2_str(char **str, PRInt16 mcsid)
{
	if (str && *str)
	{
		char *newStr = (char *) IntlDecodeMimePartIIStr
			(*str, INTL_DocToWinCharSetID(mcsid), PR_FALSE);
		if (newStr && newStr != *str)
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

static void mime_intl_insert_message_header_1(char **body, char **hdr_value,
											  char *hdr_str, 
											  const char *html_hdr_str,
											  PRInt16 mailcsid,
											  PRBool htmlEdit)
{
	if (!body || !hdr_value || !hdr_str)
		return;
	mime_intl_mimepart_2_str(hdr_value, mailcsid);
	if (htmlEdit)
	{
		mime_SACat(*body, HEADER_START_JUNK);
		/* mime_SACat(*body, LINEBREAK "<BR><B>"); */
	}
	else
	{
		mime_SACat(*body, LINEBREAK);
	}
	if (!html_hdr_str)
		html_hdr_str = hdr_str;
	mime_SACat(*body, html_hdr_str);
	if (htmlEdit)
	{
		mime_SACat(*body, HEADER_MIDDLE_JUNK);
		/* mime_SACat(*body, ":</B> "); */
	}
	else
		mime_SACat(*body, ": ");
	mime_SACat(*body, *hdr_value);
	if (htmlEdit)
		mime_SACat(*body, HEADER_END_JUNK);
}

static void mime_intl_insert_message_header(char **body, char**hdr_value,
											char *hdr_str, 
											int html_hdr_id,
											PRInt16 mailcsid,
											PRBool htmlEdit)
{
	const char *newName = NULL;
	newName = XP_GetStringForHTML(html_hdr_id, mailcsid, hdr_str);
	mime_intl_insert_message_header_1(body, hdr_value, hdr_str, newName,
									  mailcsid, htmlEdit);
}

static void mime_insert_all_headers(char **body,
									MimeHeaders *headers,
									MSG_EditorType editorType,
									PRInt16 mailcsid)
{
	PRBool htmlEdit = editorType == MSG_HTML_EDITOR;
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
			mime_SACat(newBody, 
					 "<HTML> <BR><BR>-------- Original Message --------");
		}
		else
		{
			mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
		}
		mime_SACat(newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 LINEBREAK LINEBREAK "-------- Original Message --------");
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

	  /* Hack for BSD Mailbox delimiter. */
	  if (i == 0 && head[0] == 'F' && !PL_strncmp(head, "From ", 5))
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
		  for (; colon > head && IS_SPACE(colon[-1]); colon--)
			;

		  contents = ocolon + 1;
		}

	  /* Skip over whitespace after colon. */
	  while (contents <= end && IS_SPACE(*contents))
		contents++;

	  /* Take off trailing whitespace... */
	  while (end > contents && IS_SPACE(end[-1]))
		end--;

	  name = PR_MALLOC(colon - head + 1);
	  if (!name) return /* MK_OUT_OF_MEMORY */;
	  nsCRT::memcpy(name, head, colon - head);
	  name[colon - head] = 0;

	  c2 = PR_MALLOC(end - contents + 1);
	  if (!c2)
		{
		  PR_Free(name);
		  return /* MK_OUT_OF_MEMORY */;
		}
	  nsCRT::memcpy(c2, contents, end - contents);
	  c2[end - contents] = 0;
	  
	  if (htmlEdit) mime_fix_up_html_address(&c2);
		  
	  mime_intl_insert_message_header_1(&newBody, &c2, name, name, mailcsid,
										htmlEdit); 
	  PR_Free(name);
	  PR_Free(c2);
	}

	if (htmlEdit)
	{
		mime_SACat(newBody, "</TABLE>");
		mime_SACat(newBody, LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(newBody, html_tag+6);
		else
			mime_SACat(newBody, *body);
	}
	else
	{
		mime_SACat(newBody, LINEBREAK LINEBREAK);
		mime_SACat(newBody, *body);
	}

	if (newBody)
	{
		PR_Free(*body);
		*body = newBody;
	}
}

static void mime_insert_normal_headers(char **body,
									   MimeHeaders *headers,
									   MSG_EditorType editorType,
									   PRInt16 mailcsid)
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
	PRBool htmlEdit = editorType == MSG_HTML_EDITOR;
	
	if (!from)
		from = MimeHeaders_get(headers, HEADER_SENDER, PR_FALSE, PR_TRUE);
	if (!resent_from)
		resent_from = MimeHeaders_get(headers, HEADER_RESENT_SENDER, PR_FALSE,
									  PR_TRUE); 
	
	if (htmlEdit)
	{
		mime_SACopy(&(newBody), 
					 "<HTML> <BR><BR>-------- Original Message --------");
		mime_SACat(newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 LINEBREAK LINEBREAK "-------- Original Message --------");
	}
	if (subject)
		mime_intl_insert_message_header(&newBody, &subject, HEADER_SUBJECT,
										MK_MIMEHTML_DISP_SUBJECT,
											mailcsid, htmlEdit);
	if (resent_comments)
		mime_intl_insert_message_header(&newBody, &resent_comments,
										HEADER_RESENT_COMMENTS, 
										MK_MIMEHTML_DISP_RESENT_COMMENTS,
											mailcsid, htmlEdit);
	if (resent_date)
		mime_intl_insert_message_header(&newBody, &resent_date,
										HEADER_RESENT_DATE,
										MK_MIMEHTML_DISP_RESENT_DATE,
											mailcsid, htmlEdit);
	if (resent_from)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_from);
		mime_intl_insert_message_header(&newBody, &resent_from,
										HEADER_RESENT_FROM,
										MK_MIMEHTML_DISP_RESENT_FROM,
											mailcsid, htmlEdit);
	}
	if (resent_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_to);
		mime_intl_insert_message_header(&newBody, &resent_to,
										HEADER_RESENT_TO,
										MK_MIMEHTML_DISP_RESENT_TO,
											mailcsid, htmlEdit);
	}
	if (resent_cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_cc);
		mime_intl_insert_message_header(&newBody, &resent_cc,
										HEADER_RESENT_CC,
										MK_MIMEHTML_DISP_RESENT_CC,
											mailcsid, htmlEdit);
	}
	if (date)
		mime_intl_insert_message_header(&newBody, &date, HEADER_DATE,
										MK_MIMEHTML_DISP_DATE,
										mailcsid, htmlEdit);
	if (from)
	{
		if (htmlEdit) mime_fix_up_html_address(&from);
		mime_intl_insert_message_header(&newBody, &from, HEADER_FROM,
										MK_MIMEHTML_DISP_FROM,
										mailcsid, htmlEdit);
	}
	if (reply_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&reply_to);
		mime_intl_insert_message_header(&newBody, &reply_to, HEADER_REPLY_TO,
										MK_MIMEHTML_DISP_REPLY_TO,
										mailcsid, htmlEdit);
	}
	if (organization)
		mime_intl_insert_message_header(&newBody, &organization,
										HEADER_ORGANIZATION,
										MK_MIMEHTML_DISP_ORGANIZATION,
										mailcsid, htmlEdit);
	if (to)
	{
		if (htmlEdit) mime_fix_up_html_address(&to);
		mime_intl_insert_message_header(&newBody, &to, HEADER_TO,
										MK_MIMEHTML_DISP_TO,
										mailcsid, htmlEdit);
	}
	if (cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&cc);
		mime_intl_insert_message_header(&newBody, &cc, HEADER_CC,
										MK_MIMEHTML_DISP_CC,
										mailcsid, htmlEdit);
	}
	if (bcc)
	{
		if (htmlEdit) mime_fix_up_html_address(&bcc);
		mime_intl_insert_message_header(&newBody, &bcc, HEADER_BCC,
										MK_MIMEHTML_DISP_BCC,
										mailcsid, htmlEdit);
	}
	if (newsgroups)
		mime_intl_insert_message_header(&newBody, &newsgroups, HEADER_NEWSGROUPS,
										MK_MIMEHTML_DISP_NEWSGROUPS,
										mailcsid, htmlEdit);
	if (followup_to)
	{
		if (htmlEdit) mime_fix_up_html_address(&followup_to);
		mime_intl_insert_message_header(&newBody, &followup_to,
										HEADER_FOLLOWUP_TO,
										MK_MIMEHTML_DISP_FOLLOWUP_TO,
										mailcsid, htmlEdit);
	}
	if (references)
	{
		if (htmlEdit) mime_fix_up_html_address(&references);
		mime_intl_insert_message_header(&newBody, &references,
										HEADER_REFERENCES,
										MK_MIMEHTML_DISP_REFERENCES,
										mailcsid, htmlEdit);
	}
	if (htmlEdit)
	{
		mime_SACat(newBody, "</TABLE>");
		mime_SACat(newBody, LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(newBody, html_tag+6);
		else
			mime_SACat(newBody, *body);
	}
	else
	{
		mime_SACat(newBody, LINEBREAK LINEBREAK);
		mime_SACat(newBody, *body);
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

static void mime_insert_micro_headers(char **body,
									 MimeHeaders *headers,
									 MSG_EditorType editorType,
									 PRInt16 mailcsid)
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
	PRBool htmlEdit = editorType == MSG_HTML_EDITOR;
	
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
	    mime_SACat(newBody, MIME_HEADER_TABLE);
	}
	else
	{
		mime_SACopy(&(newBody), 
					 LINEBREAK LINEBREAK "-------- Original Message --------");
	}
	if (from)
	{
		if (htmlEdit) mime_fix_up_html_address(&from);
		mime_intl_insert_message_header(&newBody, &from, HEADER_FROM,
										MK_MIMEHTML_DISP_FROM,
										mailcsid, htmlEdit);
	}
	if (subject)
		mime_intl_insert_message_header(&newBody, &subject, HEADER_SUBJECT,
										MK_MIMEHTML_DISP_SUBJECT,
											mailcsid, htmlEdit);
/*
	if (date)
		mime_intl_insert_message_header(&newBody, &date, HEADER_DATE,
										MK_MIMEHTML_DISP_DATE,
										mailcsid, htmlEdit);
*/
	if (resent_from)
	{
		if (htmlEdit) mime_fix_up_html_address(&resent_from);
		mime_intl_insert_message_header(&newBody, &resent_from,
										HEADER_RESENT_FROM,
										MK_MIMEHTML_DISP_RESENT_FROM,
										mailcsid, htmlEdit);
	}
	if (to)
	{
		if (htmlEdit) mime_fix_up_html_address(&to);
		mime_intl_insert_message_header(&newBody, &to, HEADER_TO,
										MK_MIMEHTML_DISP_TO,
										mailcsid, htmlEdit);
	}
	if (cc)
	{
		if (htmlEdit) mime_fix_up_html_address(&cc);
		mime_intl_insert_message_header(&newBody, &cc, HEADER_CC,
										MK_MIMEHTML_DISP_CC,
										mailcsid, htmlEdit);
	}
	if (bcc)
	{
		if (htmlEdit) mime_fix_up_html_address(&bcc);
		mime_intl_insert_message_header(&newBody, &bcc, HEADER_BCC,
										MK_MIMEHTML_DISP_BCC,
										mailcsid, htmlEdit);
	}
	if (newsgroups)
		mime_intl_insert_message_header(&newBody, &newsgroups, HEADER_NEWSGROUPS,
										MK_MIMEHTML_DISP_NEWSGROUPS,
										mailcsid, htmlEdit);
	if (htmlEdit)
	{
		mime_SACat(newBody, "</TABLE>");
		mime_SACat(newBody, LINEBREAK "<BR><BR>");
		if (html_tag)
			mime_SACat(newBody, html_tag+6);
		else
			mime_SACat(newBody, *body);
	}
	else
	{
		mime_SACat(newBody, LINEBREAK LINEBREAK);
		mime_SACat(newBody, *body);
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

static void mime_insert_forwarded_message_headers(char **body, 
												  MimeHeaders *headers,
												  MSG_EditorType editorType,
												  PRInt16 mailcsid)
{
	if (body && *body && headers)
	{
		PRInt32 show_headers = 0;
		
		PREF_GetIntPref("mail.show_headers", &show_headers);

		switch (show_headers)
		{
		case 0:
			mime_insert_micro_headers(body, headers, editorType, mailcsid);
			break;
		default:
		case 1:
			mime_insert_normal_headers(body, headers, editorType, mailcsid);
			break;
		case 2:
			mime_insert_all_headers(body, headers, editorType, mailcsid);
			break;
		}
	}
}

static void
mime_parse_stream_complete (void *stream)
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream;
  MSG_Pane * cpane = NULL;
  MSG_CompositionFields *fields = NULL;
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
  
  PR_ASSERT (mdd);

  if (!mdd) return;

  if (mdd->obj) {
    int status;

    status = mdd->obj->class->parse_eof ( mdd->obj, PR_FALSE );
    mdd->obj->class->parse_end( mdd->obj, status < 0 ? PR_TRUE : PR_FALSE );
    
	xlate_p = mdd->options->dexlate_p;
	sign_p = mdd->options->signed_p;

	forward_inline = ( mdd->url->fe_data && 
					   mdd->format_out != FO_CMDLINE_ATTACHMENTS &&
					   MSG_GetActionInfoFlags(mdd->url->fe_data) &
					   MSG_FLAG_FORWARDED );

	if (forward_inline)
	{
#ifdef MOZ_SECURITY
      HG88890
#endif
	}

    PR_ASSERT ( mdd->options == mdd->obj->options );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) {
      PR_FREEIF (mdd->options->part_to_load);
      PR_Free(mdd->options);
      mdd->options = 0;
    }
    if (mdd->stream) {
      mdd->stream->complete (mdd->stream->data_object);
      PR_Free( mdd->stream );
      mdd->stream = 0;
    }
  }

  /* time to bring up the compose windows with all the info gathered */

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

		mime_intl_mimepart_2_str(&repl, mdd->mailcsid);
		mime_intl_mimepart_2_str(&to, mdd->mailcsid);
		mime_intl_mimepart_2_str(&cc, mdd->mailcsid);
		mime_intl_mimepart_2_str(&bcc, mdd->mailcsid);
		mime_intl_mimepart_2_str(&grps, mdd->mailcsid);
		mime_intl_mimepart_2_str(&foll, mdd->mailcsid);
		mime_intl_mimepart_2_str(&host, mdd->mailcsid);

		if (host) {
			char *secure = NULL;
			
			secure = PL_strcasestr(host, "secure");
			if (secure) {
				*secure = 0;
				news_host = PR_smprintf ("snews://%s", host);
			}
			else {
			news_host = PR_smprintf ("news://%s", host);
			}
		}
	}

	mime_intl_mimepart_2_str(&subj, mdd->mailcsid);

	fields = MSG_CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
										  org, subj, refs, 0, priority, 0, news_host,
										  xlate_p, sign_p);

	draftInfo = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_DRAFT_INFO, PR_FALSE, PR_FALSE);
	if (draftInfo && fields && !forward_inline) {
		char *parm = 0;
		parm = MimeHeaders_get_parameter(draftInfo, "vcard", NULL, NULL);
		if (parm && !PL_strcmp(parm, "1"))
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_ATTACH_VCARD_BOOL_HEADER_MASK,
										PR_TRUE);
		else
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_ATTACH_VCARD_BOOL_HEADER_MASK,
										PR_FALSE);
		PR_FREEIF(parm);
		parm = MimeHeaders_get_parameter(draftInfo, "receipt", NULL, NULL);
		if (parm && !PL_strcmp(parm, "0"))
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_RETURN_RECEIPT_BOOL_HEADER_MASK,
										PR_FALSE); 
		else
		{
			int receiptType = 0;
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_RETURN_RECEIPT_BOOL_HEADER_MASK,
										PR_TRUE); 
			sscanf(parm, "%d", &receiptType);
			MSG_SetCompFieldsReceiptType(fields, (PRInt32) receiptType);
		}
		PR_FREEIF(parm);
		parm = MimeHeaders_get_parameter(draftInfo, "uuencode", NULL, NULL);
		if (parm && !PL_strcmp(parm, "1"))
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_UUENCODE_BINARY_BOOL_HEADER_MASK,
										PR_TRUE);
		else
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_UUENCODE_BINARY_BOOL_HEADER_MASK,
										PR_FALSE);
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

	if (mdd->messageBody) {
		char *body;
		XP_StatStruct st;
		PRUint32 bodyLen = 0;
		PRFileDesc *file;
		MSG_EditorType editorType = MSG_DEFAULT;

		st.st_size = 0;
		XP_Stat (mdd->messageBody->file_name, &st, xpFileToPost);
		bodyLen = st.st_size;
		body = PR_MALLOC (bodyLen + 1);
		if (body)
		{
			memset (body, 0, bodyLen+1);
			
			file = PR_Open (mdd->messageBody->file_name, PR_RDONLY, 0);
			PR_Read (file, body, bodyLen);
			PR_Close(file);
			
			if (mdd->messageBody->type && *mdd->messageBody->type)
			{
				if( PL_strcasestr(mdd->messageBody->type, "text/html") != NULL )
					editorType = MSG_HTML_EDITOR;
				else if ( PL_strcasestr(mdd->messageBody->type, "text/plain") != NULL )
					editorType = MSG_PLAINTEXT_EDITOR;
			}
			else
			{
				editorType = MSG_PLAINTEXT_EDITOR;
			}
		  
			if (forward_inline)
			{
				mime_insert_forwarded_message_headers(&body, mdd->headers,
													  editorType,
													  mdd->mailcsid);
				bodyLen = PL_strlen(body);
			}

			{
				CCCDataObject conv = INTL_CreateCharCodeConverter();
				if(conv) {
					if (INTL_GetCharCodeConverter(mdd->mailcsid,
							 INTL_DocToWinCharSetID(mdd->mailcsid), conv))
					{
						char *newBody = NULL;
						newBody = (char *)INTL_CallCharCodeConverter(
							conv, (unsigned char *) body, (PRInt32) bodyLen);
						if (newBody) {
							/* CharCodeConverter return the char* to the orginal string
							   we don't want to free body in that case */
							if( newBody != body)
								PR_FREEIF(body);
							body = newBody;
						}
					}
					INTL_DestroyCharCodeConverter(conv);
				}
			}
		}
		  
		cpane = FE_CreateCompositionPane (mdd->context, fields, body, editorType);
		
		PR_Free (body);
		mime_free_attachments (mdd->messageBody, 1);
	}
	else
	{
		cpane = FE_CreateCompositionPane(mdd->context, fields, NULL, MSG_DEFAULT);
	}
	
	if (cpane)
	{
		/* clear the message body in case someone store the signature string in it */
		MSG_SetCompBody(cpane, "");
		MSG_SetHTMLAction(cpane, (MSG_HTMLComposeAction) htmlAction);
		if (lineWidth > 0)
			MSG_SetLineWidth(cpane, lineWidth);
		
		if ( mdd->attachments_count) 
			mime_draft_process_attachments (mdd, cpane);
	}
  }
  else
  {
	  fields = MSG_CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
											org, subj, refs, 0, priority, 0, news_host,
											MSG_GetMailXlateionPreference(), 
											MSG_GetMailSigningPreference());
	  if (fields)
		  cpane = FE_CreateCompositionPane(mdd->context, fields, NULL, MSG_DEFAULT);
  }
	
  if (cpane && mdd->url->fe_data) 
  {
	  if ( mdd->format_out != FO_CMDLINE_ATTACHMENTS ) 
	  {
		  MSG_SetPostDeliveryActionInfo (cpane, mdd->url->fe_data);
	  }
	  else
	  {
		  MSG_SetAttachmentList ( cpane, (MSG_AttachmentData*)(mdd->url->fe_data));
	  }
  }
  
  
  if ( mdd->headers )
	  MimeHeaders_free ( mdd->headers );
  
  if (mdd->attachments)
	  /* do not call mime_free_attachments just use PR_FREEIF()  */
	  PR_FREEIF ( mdd->attachments );
  
  if (fields)
	  MSG_DestroyCompositionFields(fields);
  
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

static void
mime_parse_stream_abort (void *stream, int status )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream;  
  PR_ASSERT (mdd);

  if (!mdd) return;
  
  if (mdd->obj) {
    int status;

    if ( !mdd->obj->closed_p )
      status = mdd->obj->class->parse_eof ( mdd->obj, PR_TRUE );
    if ( !mdd->obj->parsed_p )
      mdd->obj->class->parse_end( mdd->obj, PR_TRUE );
    
    PR_ASSERT ( mdd->options == mdd->obj->options );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) {
      PR_FREEIF (mdd->options->part_to_load);
      PR_Free(mdd->options);
      mdd->options = 0;
    }
    if (mdd->stream) {
      mdd->stream->abort (mdd->stream->data_object, status);
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
make_mime_headers_copy ( void *closure,
						 MimeHeaders *headers )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) closure;

  PR_ASSERT ( mdd && headers );
  
  if ( !mdd || ! headers ) return 0;

  PR_ASSERT ( mdd->headers == NULL );

  mdd->headers = MimeHeaders_copy ( headers );

  mdd->options->done_parsing_outer_headers = PR_TRUE;

  return 0;
}

int 
mime_decompose_file_init_fn ( void *stream_closure,
								MimeHeaders *headers )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  MSG_AttachedFile *attachments = 0, *newAttachment = 0;
  int nAttachments = 0;
  char *hdr_value = NULL, *parm_value = NULL;
  PRBool needURL = PR_FALSE;
  PRBool creatingMsgBody = PR_FALSE;
  
  PR_ASSERT (mdd && headers);
  if (!mdd || !headers) return -1;

  if ( !mdd->options->is_multipart_msg ) {
	if (mdd->options->decompose_init_count) {
	  mdd->options->decompose_init_count++;
	  PR_ASSERT(mdd->curAttachment);
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

  if (!nAttachments && !mdd->messageBody) {
	char *charset = NULL, *contentType = NULL;
	contentType = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, PR_FALSE, PR_FALSE);
	if (contentType) {
		charset = MimeHeaders_get_parameter(contentType, "charset", NULL, NULL);
		mdd->mailcsid = INTL_CharSetNameToID(charset);
		PR_FREEIF(charset);
		PR_FREEIF(contentType);
	}

	mdd->messageBody = PR_NEWZAP (MSG_AttachedFile);
	PR_ASSERT (mdd->messageBody);
	if (!mdd->messageBody) 
	  return MK_OUT_OF_MEMORY;
	newAttachment = mdd->messageBody;
	creatingMsgBody = PR_TRUE;
  }
  else {
	/* always allocate one more extra; don't ask me why */
	needURL = PR_TRUE;
	if ( nAttachments ) {
	  PR_ASSERT (mdd->attachments);
		
	  attachments = PR_REALLOC (mdd->attachments, 
								sizeof (MSG_AttachedFile) * 
								(nAttachments + 2));
	  if (!attachments)
		return MK_OUT_OF_MEMORY;
	  mdd->attachments = attachments;
	  mdd->attachments_count++;
	}
	else {
	  PR_ASSERT (!mdd->attachments);
	  
	  attachments = PR_MALLOC ( sizeof (MSG_AttachedFile) * 2);
	  if (!attachments) 
		return MK_OUT_OF_MEMORY;
	  mdd->attachments_count++;
	  mdd->attachments = attachments;
	}

	newAttachment = attachments + nAttachments;
	memset ( newAttachment, 0, sizeof (MSG_AttachedFile) * 2 );
  }

  newAttachment->orig_url = MimeHeaders_get_name ( headers );

  if (!newAttachment->orig_url) {
	parm_value = MimeHeaders_get( headers, HEADER_CONTENT_BASE, PR_FALSE, PR_FALSE );
	if (parm_value) {
	  char *cp = NULL, *cp1=NULL ;
	  nsUnescape(parm_value);
	  /* strip '"' */
	  cp = parm_value;
	  while (*cp == '"') cp++;
	  if ((cp1 = PL_strchr(cp, '"')))
		*cp1 = 0;
	  mime_SACopy(&(newAttachment->orig_url), cp);
	  PR_FREEIF(parm_value);
	}
  }

  mdd->curAttachment = newAttachment;

  newAttachment->type =  MimeHeaders_get ( headers, HEADER_CONTENT_TYPE,
										   PR_TRUE, PR_FALSE );
  
  /* This is to handle the degenerated Apple Double attachment.
   */
  parm_value = MimeHeaders_get( headers, HEADER_CONTENT_TYPE,
										   PR_FALSE, PR_FALSE );
  if (parm_value) {
	  char *boundary = NULL;
	  char *tmp_value = NULL;
	  boundary = MimeHeaders_get_parameter(parm_value, "boundary", NULL, NULL);
	  if (boundary)
		  tmp_value = PR_smprintf("; boundary=\"%s\"", boundary);
	  if (tmp_value)
		  mime_SACat(newAttachment->type, tmp_value);
	  newAttachment->x_mac_type = 
		  MimeHeaders_get_parameter(parm_value, "x-mac-type", NULL, NULL);
	  newAttachment->x_mac_creator = 
		  MimeHeaders_get_parameter(parm_value, "x-mac-creator", NULL, NULL);
	  PR_FREEIF(parm_value);
	  PR_FREEIF(boundary);
	  PR_FREEIF(tmp_value);
  }
  newAttachment->encoding = MimeHeaders_get ( headers, 
											  HEADER_CONTENT_TRANSFER_ENCODING,
											  PR_FALSE, PR_FALSE );
  newAttachment->description = MimeHeaders_get( headers, 
												HEADER_CONTENT_DESCRIPTION,
												PR_FALSE, PR_FALSE );
  mdd->tmp_file_name = GetOSTempFile("nsmail");

  if (!mdd->tmp_file_name)
	return MK_OUT_OF_MEMORY;

  mime_SACopy (&(newAttachment->file_name), mdd->tmp_file_name);
  mdd->tmp_file = PR_Open ( mdd->tmp_file_name, PR_RDWR | PR_CREATE_FILE, 493 );
  if (!mdd->tmp_file)
	return MK_UNABLE_TO_OPEN_TMP_FILE;

  /* if need an URL and we don't have one, let's fake one */
  if (needURL && !newAttachment->orig_url) {
	newAttachment->orig_url = PR_smprintf ("file://%s", mdd->tmp_file_name);
  }

  if (creatingMsgBody) {
	MimeDecoderData *(*fn) (int (*) (const char*, PRInt32, void*), void*) = 0;

	/* Initialize a decoder if necessary.
	 */
	if (!newAttachment->encoding || mdd->options->dexlate_p)
	;
	else if (!PL_strcasecmp(newAttachment->encoding, ENCODING_BASE64))
	  fn = &MimeB64DecoderInit;
	else if (!PL_strcasecmp(newAttachment->encoding, ENCODING_QUOTED_PRINTABLE))
	  fn = &MimeQPDecoderInit;
	else if (!PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE) ||
			 !PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE2) ||
			 !PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE3) ||
			 !PL_strcasecmp(newAttachment->encoding, ENCODING_UUENCODE4))
	  fn = &MimeUUDecoderInit;

	if (fn) {
	  mdd->decoder_data =
		fn (/* The (int (*) ...) cast is to turn the `void' argument
			   into `MimeObject'. */
			((int (*) (const char *, PRInt32, void *))
			 dummy_file_write),
			mdd->tmp_file);
	  
	  if (!mdd->decoder_data)
		return MK_OUT_OF_MEMORY;
	}
  }
  return 0;
}

int 
mime_decompose_file_output_fn ( char *buf,
								PRInt32 size,
								void *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  int ret = 0;
  
  PR_ASSERT (mdd && buf);
  if (!mdd || !buf) return -1;
  if (!size) return 0;

  /****** doesn't seem to be needed *****
  PR_ASSERT (mdd->tmp_file && mdd->tmp_file_name); 
  */
  if ( !mdd->tmp_file ) 
	return 0;

  if (mdd->decoder_data) {
	ret = MimeDecoderWrite(mdd->decoder_data, buf, size);
	if (ret == -1) return -1;
  }
  else {
    ret = PR_Write(mdd->tmp_file, buf, size);
	if (ret < size)
	  return MK_MIME_ERROR_WRITING_FILE;
  }
  
  return 0;
}

int
mime_decompose_file_close_fn ( void *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  
  /* relax the rule in case we encountered invalid xlated situation
    PR_ASSERT (mdd && mdd->tmp_file); 
   */
  if ( !mdd || !mdd->tmp_file )
	return -1;

  if ( !mdd->options->is_multipart_msg ) {
	if ( --mdd->options->decompose_init_count > 0 )
	  return 0;
  }

  if (mdd->decoder_data) {
	MimeDecoderDestroy(mdd->decoder_data, PR_FALSE);
	mdd->decoder_data = 0;
  }

  PR_Close ( mdd->tmp_file );
  mdd->tmp_file = 0;
  PR_FREEIF ( mdd->tmp_file_name );
  mdd->tmp_file_name = 0;

  return 0;
}

extern nsMIMESession *
MIME_ToDraftConverter ( int format_out,
			void *closure,
			URL_Struct *url,
			MWContext *context )
{
  int status = 0;
  nsMIMESession * stream = NULL;
  struct mime_draft_data *mdd = NULL;
  MimeObject *obj;

  PR_ASSERT (url && context);

  if ( !url || !context ) return NULL;

  mdd = PR_NEWZAP (struct mime_draft_data);
  if (!mdd) return 0;

  mdd->url = url;
  mdd->context = context;
  mdd->format_out = format_out;

  mdd->options = PR_NEWZAP  ( MimeDisplayOptions );
  if ( !mdd->options ) {
    PR_Free (mdd);
    return 0;
  }
  mdd->options->passwd_prompt_fn_arg = context;
  mdd->options->decompose_file_p = PR_TRUE;	/* new field in MimeDisplayOptions */
  mdd->options->url = url->address;
  mdd->options->stream_closure = mdd;
  mdd->options->html_closure = mdd;
  mdd->options->decompose_headers_info_fn = make_mime_headers_copy;
  mdd->options->decompose_file_init_fn = mime_decompose_file_init_fn;
  mdd->options->decompose_file_output_fn = mime_decompose_file_output_fn;
  mdd->options->decompose_file_close_fn = mime_decompose_file_close_fn;
#ifdef FO_MAIL_MESSAGE_TO
  /* If we're attaching a message (for forwarding) then we must eradicate all
	 traces of xlateion from it, since forwarding someone else a message
	 that wasn't xlated for them doesn't work.  We have to dexlate it
	 before sending it.
   */
  mdd->options->dexlate_p = PR_TRUE;
#endif /* FO_MAIL_MESSAGE_TO */

  obj = mime_new ( (MimeObjectClass *) &mimeMessageClass,
		   (MimeHeaders *) NULL,
		   MESSAGE_RFC822 );
  if ( !obj ) {
    PR_FREEIF( mdd->options->part_to_load );
    PR_Free ( mdd->options );
    PR_Free ( mdd );
    return 0;
  }
  
  obj->options = mdd->options;
  mdd->obj = obj;

  stream = PR_NEWZAP ( nsMIMESession );
  if ( !stream ) {
    PR_FREEIF( mdd->options->part_to_load );
    PR_Free ( mdd->options );
    PR_Free ( mdd );
    PR_Free ( obj );
    return 0;
  }

  stream->name = "MIME To Draft Converter Stream";
  stream->complete = mime_parse_stream_complete;
  stream->abort = mime_parse_stream_abort;
  stream->put_block = mime_parse_stream_write;
  stream->is_write_ready = mime_parse_stream_write_ready;
  stream->data_object = mdd;
  stream->window_id = context;

  status = obj->class->initialize ( obj );
  if ( status >= 0 )
    status = obj->class->parse_begin ( obj );
  if ( status < 0 ) {
    PR_Free ( stream );
    PR_FREEIF( mdd->options->part_to_load );
    PR_Free ( mdd->options );
    PR_Free ( mdd );
    PR_Free ( obj );
    return 0;
  }

  return stream;
}
