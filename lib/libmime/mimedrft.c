/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "xp.h"
#include "libi18n.h"
#include "xp_time.h"
#include "msgcom.h"
#include "mimeobj.h"
#include "mimemsg.h"
#include "mimeenc.h" /* jrm - needed to make it build 97/02/21 */

#define HEADER_NNTP_POSTING_HOST             "NNTP-Posting-Host"

extern int MK_UNABLE_TO_OPEN_TMP_FILE;
extern int MK_MIME_ERROR_WRITING_FILE;


int 
mime_decompose_file_init_fn ( void *stream_closure,
							  MimeHeaders *headers );


int 
mime_decompose_file_output_fn ( char *buf,
								int32 size,
								void *stream_closure );

int
mime_decompose_file_close_fn ( void *stream_closure );

/* This struct is the state we used in MIME_ToDraftConverter() */
struct mime_draft_data {
  URL_Struct *url;                         /* original url */
  int format_out;                          /* intended output format; 
											  should be FO_OPEN_DRAFT */
  MWContext *context;
  NET_StreamClass *stream;                 /* not used for now */
  MimeObject *obj;                         /* The root */
  MimeDisplayOptions *options;             /* data for communicating with libmime.a */
  MimeHeaders *headers;                    /* copy of outer most mime header */
  int attachments_count;                   /* how many attachments we have */
  MSG_AttachedFile *attachments;           /* attachments */
  MSG_AttachedFile *messageBody;           /* message body */
  MSG_AttachedFile *curAttachment;		   /* temp */
  char *tmp_file_name;                     /* current opened file for output */
  XP_File tmp_file;                        /* output file handle */
  MimeDecoderData *decoder_data;
  int16	mailcsid;							/* get it from CHARSET of Content-Type and convert to csid */
};


static int
dummy_file_write( char *buf, int32 size, void *fileHandle )
{
  return XP_FileWrite(buf, size, (XP_File) fileHandle);
}

static int
mime_parse_stream_write ( NET_StreamClass *stream,
			  const char *buf,
			  int32 size )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;  
  XP_ASSERT ( mdd );

  if ( !mdd || !mdd->obj ) return -1;

  return mdd->obj->class->parse_buffer ((char *) buf, size, mdd->obj);
}

static unsigned int
mime_parse_stream_write_ready ( NET_StreamClass *stream )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;  
  XP_ASSERT (mdd);

  if (!mdd) return MAX_WRITE_READY;
  if (mdd->stream)
    return mdd->stream->is_write_ready ( mdd->stream );
  else
    return MAX_WRITE_READY;
}

static void
mime_free_attachments ( MSG_AttachedFile *attachments,
						int count )
{
  int i;
  MSG_AttachedFile *cur = attachments;

  XP_ASSERT ( attachments && count > 0 );
  if ( !attachments || count <= 0 ) return;

  for ( i = 0; i < count; i++, cur++ ) {
	FREEIF ( cur->orig_url );
	FREEIF ( cur->type );
	FREEIF ( cur->encoding );
	FREEIF ( cur->description );
	FREEIF ( cur->x_mac_type );
	FREEIF ( cur->x_mac_creator );
	if ( cur->file_name ) {
	  XP_FileRemove( cur->file_name, xpFileToPost );
	  FREEIF ( cur->file_name );
	}
  }
  XP_FREE ( attachments );
}

static int
mime_draft_process_attachments ( struct mime_draft_data *mdd,
								 MSG_Pane *cpane )
{
  struct MSG_AttachmentData *attachData = NULL, *tmp = NULL;
  struct MSG_AttachedFile *tmpFile = NULL;
  int i;

  XP_ASSERT ( mdd->attachments_count && mdd->attachments );

  if ( !mdd->attachments || !mdd->attachments_count )
	return -1;

  attachData = XP_ALLOC( ( (mdd->attachments_count+1) * sizeof (MSG_AttachmentData) ) );
  if ( !attachData ) return MK_OUT_OF_MEMORY;

  XP_MEMSET ( attachData, 0, (mdd->attachments_count+1) * sizeof (MSG_AttachmentData) );

  tmpFile = mdd->attachments;
  tmp = attachData;

  for ( i=0; i < mdd->attachments_count; i++, tmp++, tmpFile++ ) {
	if (tmpFile->type) {
		if (strcasecomp ( tmpFile->type, "text/x-vcard") == 0)
			StrAllocCopy (tmp->real_name, tmpFile->description);
	}
	if ( tmpFile->orig_url ) {
	  StrAllocCopy ( tmp->url, tmpFile->orig_url );
	  if (!tmp->real_name)
	  StrAllocCopy ( tmp->real_name, tmpFile->orig_url );
	}
	if ( tmpFile->type ) {
	  StrAllocCopy ( tmp->desired_type, tmpFile->type );
	  StrAllocCopy ( tmp->real_type, tmpFile->type );
	}
	if ( tmpFile->encoding ) {
	  StrAllocCopy ( tmp->real_encoding, tmpFile->encoding );
	}
	if ( tmpFile->description ) {
	  StrAllocCopy ( tmp->description, tmpFile->description );
	}
	if ( tmpFile->x_mac_type ) {
	  StrAllocCopy ( tmp->x_mac_type, tmpFile->x_mac_type );
	}
	if ( tmpFile->x_mac_creator ) {
	  StrAllocCopy ( tmp->x_mac_creator, tmpFile->x_mac_creator );
	}
  }

  MSG_SetPreloadedAttachments ( cpane, MSG_GetContext (cpane), attachData,
								mdd->attachments, mdd->attachments_count );

  XP_FREE (attachData);

  return 0;

}

static void
mime_parse_stream_complete (NET_StreamClass *stream)
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;
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

  XP_Bool encrypt_p = FALSE;	/* #### how do we determine this? */
  XP_Bool sign_p = FALSE;		/* #### how do we determine this? */
  
  XP_ASSERT (mdd);

  if (!mdd) return;

  if (mdd->obj) {
    int status;

    status = mdd->obj->class->parse_eof ( mdd->obj, FALSE );
    mdd->obj->class->parse_end( mdd->obj, status < 0 ? TRUE : FALSE );
    
	encrypt_p = mdd->options->decrypt_p;
	sign_p = mdd->options->signed_p;

    XP_ASSERT ( mdd->options == mdd->obj->options );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) {
      FREEIF (mdd->options->part_to_load);
      XP_FREE(mdd->options);
      mdd->options = 0;
    }
    if (mdd->stream) {
      mdd->stream->complete (mdd->stream);
      XP_FREE( mdd->stream );
      mdd->stream = 0;
    }
  }

  /* time to bring up the compose windows with all the info gathered */

  if ( mdd->headers ) {
	char *newString = NULL;

	repl = MimeHeaders_get(mdd->headers, HEADER_REPLY_TO, FALSE, FALSE);
	subj = MimeHeaders_get(mdd->headers, HEADER_SUBJECT,  FALSE, FALSE);
	to   = MimeHeaders_get(mdd->headers, HEADER_TO,       FALSE, TRUE);
	cc   = MimeHeaders_get(mdd->headers, HEADER_CC,       FALSE, TRUE);
	bcc   = MimeHeaders_get(mdd->headers, HEADER_BCC,       FALSE, TRUE);

	/* These headers should not be RFC-1522-decoded. */
	grps = MimeHeaders_get(mdd->headers, HEADER_NEWSGROUPS,  FALSE, TRUE);
	foll = MimeHeaders_get(mdd->headers, HEADER_FOLLOWUP_TO, FALSE, TRUE);
	id   = MimeHeaders_get(mdd->headers, HEADER_MESSAGE_ID,  FALSE, FALSE);
	refs = MimeHeaders_get(mdd->headers, HEADER_REFERENCES,  FALSE, TRUE);
	priority = MimeHeaders_get(mdd->headers, HEADER_X_PRIORITY, FALSE, FALSE);

	host = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_NEWSHOST, FALSE, FALSE);
	if (!host)
	  host = MimeHeaders_get(mdd->headers, HEADER_NNTP_POSTING_HOST, FALSE, FALSE);

	{
		if (repl) 
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							 repl, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != repl) 
			{
				FREEIF (repl);
				repl = newString;
			}
		}
		if (subj)
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							subj, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != repl) 
			{
				FREEIF (subj);
				subj = newString;
			}
		}
		if (to)
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							to, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != repl) 
			{
				FREEIF (to);
				to = newString;
			}
		}
		if (cc)
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							cc, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != cc) 
			{
				FREEIF (cc);
				cc = newString;
			}
		}
		if (bcc)
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							bcc, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != bcc) 
			{
				FREEIF (bcc);
				bcc = newString;
			}
		}
		if (grps)
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							grps, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != grps) 
			{
				FREEIF (grps);
				grps = newString;
			}
		}
		if (foll)
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							foll, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != foll) 
			{
				FREEIF (foll);
				foll = newString;
			}
		}
		if (host)
		{
			newString = (char *)IntlDecodeMimePartIIStr(
							host, INTL_DocToWinCharSetID(mdd->mailcsid), FALSE);
			if (newString && newString != host) 
			{
				FREEIF (host);
				repl = newString;
			}
		}
	}

	if (host) {
		char *secure = NULL;

		secure = strcasestr(host, "secure");
		if (secure) {
			*secure = 0;
			news_host = PR_smprintf ("snews://%s", host);
		}
		else {
			news_host = PR_smprintf ("news://%s", host);
		}
	}
	  

	fields = MSG_CreateCompositionFields( from, repl, to, cc, bcc, fcc, grps, foll,
										  org, subj, refs, 0, priority, 0, news_host,
										  encrypt_p, sign_p);

	draftInfo = MimeHeaders_get(mdd->headers, HEADER_X_MOZILLA_DRAFT_INFO, FALSE, FALSE);
	if (draftInfo && fields) {
		char *parm = 0;
		parm = MimeHeaders_get_parameter(draftInfo, "vcard");
		if (parm && !XP_STRCMP(parm, "1"))
			MSG_SetCompFieldsBoolHeader(fields, MSG_ATTACH_VCARD_BOOL_HEADER_MASK, TRUE);
		else
			MSG_SetCompFieldsBoolHeader(fields, MSG_ATTACH_VCARD_BOOL_HEADER_MASK, FALSE);
		FREEIF(parm);
		parm = MimeHeaders_get_parameter(draftInfo, "receipt");
		if (parm && !XP_STRCMP(parm, "0"))
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_RETURN_RECEIPT_BOOL_HEADER_MASK,
										FALSE); 
		else
		{
			int receiptType = 0;
			MSG_SetCompFieldsBoolHeader(fields,
										MSG_RETURN_RECEIPT_BOOL_HEADER_MASK,
										TRUE); 
			sscanf(parm, "%d", &receiptType);
			MSG_SetCompFieldsReceiptType(fields, (int32) receiptType);
		}
		FREEIF(parm);
		parm = MimeHeaders_get_parameter(draftInfo, "uuencode");
		if (parm && !XP_STRCMP(parm, "1"))
			MSG_SetCompFieldsBoolHeader(fields, MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, TRUE);
		else
			MSG_SetCompFieldsBoolHeader(fields, MSG_UUENCODE_BINARY_BOOL_HEADER_MASK, FALSE);
		FREEIF(parm);
		parm = MimeHeaders_get_parameter(draftInfo, "html");
		if (parm)
			sscanf(parm, "%d", &htmlAction);
		FREEIF(parm);
		parm = MimeHeaders_get_parameter(draftInfo, "linewidth");
		if (parm)
			sscanf(parm, "%d", &lineWidth);
		FREEIF(parm);

	}

	if (mdd->messageBody) {
	  char *body;
	  XP_StatStruct st;
	  XP_File file;
	  MSG_EditorType editorType = MSG_DEFAULT;

	  st.st_size = 0;
	  XP_Stat (mdd->messageBody->file_name, &st, xpFileToPost);
	  body = XP_ALLOC (st.st_size + 1);
	  XP_MEMSET (body, 0, st.st_size+1);

	  file = XP_FileOpen (mdd->messageBody->file_name, xpFileToPost, XP_FILE_READ_BIN);
	  XP_FileRead (body, st.st_size+1, file);
	  XP_FileClose(file);

	  if (mdd->messageBody->type && *mdd->messageBody->type)
	  {
		  if( XP_STRSTR(mdd->messageBody->type, "text/html") != NULL )
			  editorType = MSG_HTML_EDITOR;
		  else if ( XP_STRSTR(mdd->messageBody->type, "text/plain") != NULL )
			  editorType = MSG_PLAINTEXT_EDITOR;
	  }

	  {
		CCCDataObject conv = INTL_CreateCharCodeConverter();
		if(conv) {
			if (INTL_GetCharCodeConverter(mdd->mailcsid, INTL_DocToWinCharSetID(mdd->mailcsid), conv)) {
				char *newBody = NULL;
				newBody = (char *)INTL_CallCharCodeConverter(
					conv, (unsigned char *) body, (int32) st.st_size+1);
				if (newBody) {
				/* CharCodeConverter return the char* to the orginal string
				   we don't want to free body in that case */
					if( newBody != body)
						FREEIF(body);
					body = newBody;
				}
			}
			INTL_DestroyCharCodeConverter(conv);
		}
	  }

	  cpane = FE_CreateCompositionPane (mdd->context, fields, body, editorType);

	  XP_FREE (body);
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
										    MSG_GetMailEncryptionPreference(), 
											MSG_GetMailSigningPreference());
	  if (fields)
		cpane = FE_CreateCompositionPane(mdd->context, fields, NULL, MSG_DEFAULT);
  }

  if (cpane) {
	  if ( mdd->url->fe_data && (mdd->format_out != FO_CMDLINE_ATTACHMENTS) ) 
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
	/* do not call mime_free_attachments just use FREEIF()  */
    FREEIF ( mdd->attachments );

  if (fields)
	  MSG_DestroyCompositionFields(fields);

  XP_FREE (mdd);

  FREEIF(host);
  FREEIF(to_and_cc);
  FREEIF(re_subject);
  FREEIF(new_refs);
  FREEIF(from);
  FREEIF(repl);
  FREEIF(subj);
  FREEIF(id);
  FREEIF(refs);
  FREEIF(to);
  FREEIF(cc);
  FREEIF(grps);
  FREEIF(foll);
  FREEIF(priority);
  FREEIF(draftInfo);

}

static void
mime_parse_stream_abort (NET_StreamClass *stream, int status )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream->data_object;  
  XP_ASSERT (mdd);

  if (!mdd) return;
  
  if (mdd->obj) {
    int status;

    if ( !mdd->obj->closed_p )
      status = mdd->obj->class->parse_eof ( mdd->obj, TRUE );
    if ( !mdd->obj->parsed_p )
      mdd->obj->class->parse_end( mdd->obj, TRUE );
    
    XP_ASSERT ( mdd->options == mdd->obj->options );
    mime_free (mdd->obj);
    mdd->obj = 0;
    if (mdd->options) {
      FREEIF (mdd->options->part_to_load);
      XP_FREE(mdd->options);
      mdd->options = 0;
    }
    if (mdd->stream) {
      mdd->stream->abort (mdd->stream, status);
      XP_FREE( mdd->stream );
      mdd->stream = 0;
    }
  }

  if ( mdd->headers )
    MimeHeaders_free (mdd->headers);
  if (mdd->attachments)
    mime_free_attachments( mdd->attachments, mdd->attachments_count );

  XP_FREE (mdd);
}

static int
make_mime_headers_copy ( void *closure,
						 MimeHeaders *headers )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) closure;

  XP_ASSERT ( mdd && headers );
  
  if ( !mdd || ! headers ) return 0;

  XP_ASSERT ( mdd->headers == NULL );

  mdd->headers = MimeHeaders_copy ( headers );

  mdd->options->done_parsing_outer_headers = TRUE;

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
  XP_Bool needURL = FALSE;
  XP_Bool creatingMsgBody = FALSE;
  
  XP_ASSERT (mdd && headers);
  if (!mdd || !headers) return -1;

  if ( !mdd->options->is_multipart_msg ) {
	if (mdd->options->decompose_init_count) {
	  mdd->options->decompose_init_count++;
	  XP_ASSERT(mdd->curAttachment);
	  if (mdd->curAttachment) {
  		char *ct = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, TRUE, FALSE);
		if (ct)
			StrAllocCopy(mdd->curAttachment->type, ct);
		FREEIF(ct);
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
	contentType = MimeHeaders_get(headers, HEADER_CONTENT_TYPE, FALSE, FALSE);
	if (contentType) {
		charset = MimeHeaders_get_parameter(contentType, "charset");
		mdd->mailcsid = INTL_CharSetNameToID(charset);
		FREEIF(charset);
		FREEIF(contentType);
	}

	mdd->messageBody = XP_NEW_ZAP (MSG_AttachedFile);
	XP_ASSERT (mdd->messageBody);
	if (!mdd->messageBody) 
	  return MK_OUT_OF_MEMORY;
	newAttachment = mdd->messageBody;
	creatingMsgBody = TRUE;
  }
  else {
	/* always allocate one more extra; don't ask me why */
	needURL = TRUE;
	if ( nAttachments ) {
	  XP_ASSERT (mdd->attachments);
		
	  attachments = XP_REALLOC (mdd->attachments, 
								sizeof (MSG_AttachedFile) * 
								(nAttachments + 2));
	  if (!attachments)
		return MK_OUT_OF_MEMORY;
	  mdd->attachments = attachments;
	  mdd->attachments_count++;
	}
	else {
	  XP_ASSERT (!mdd->attachments);
	  
	  attachments = XP_ALLOC ( sizeof (MSG_AttachedFile) * 2);
	  if (!attachments) 
		return MK_OUT_OF_MEMORY;
	  mdd->attachments_count++;
	  mdd->attachments = attachments;
	}

	newAttachment = attachments + nAttachments;
	XP_MEMSET ( newAttachment, 0, sizeof (MSG_AttachedFile) * 2 );
  }

  newAttachment->orig_url = MimeHeaders_get_name ( headers );

  if (!newAttachment->orig_url) {
	parm_value = MimeHeaders_get( headers, HEADER_CONTENT_BASE, FALSE, FALSE );
	if (parm_value) {
	  char *cp = NULL, *cp1=NULL ;
	  NET_UnEscape(parm_value);
	  /* strip '"' */
	  cp = parm_value;
	  while (*cp == '"') cp++;
	  if ((cp1 = XP_STRCHR(cp, '"')))
		*cp1 = 0;
	  StrAllocCopy(newAttachment->orig_url, cp);
	  FREEIF(parm_value);
	}
  }

  mdd->curAttachment = newAttachment;

  newAttachment->type =  MimeHeaders_get ( headers, HEADER_CONTENT_TYPE,
										   TRUE, FALSE );
  
  /* This is to handle the degenerated Apple Double attachment.
   */
  parm_value = MimeHeaders_get( headers, HEADER_CONTENT_TYPE,
										   FALSE, FALSE );
  if (parm_value) {
	  char *boundary = NULL;
	  char *tmp_value = NULL;
	  boundary = MimeHeaders_get_parameter(parm_value, "boundary");
	  if (boundary)
		  tmp_value = PR_smprintf("; boundary=\"%s\"", boundary);
	  if (tmp_value)
		  StrAllocCat(newAttachment->type, tmp_value);
	  newAttachment->x_mac_type = 
		  MimeHeaders_get_parameter(parm_value, "x-mac-type");
	  newAttachment->x_mac_creator = 
		  MimeHeaders_get_parameter(parm_value, "x-mac-creator");
	  FREEIF(parm_value);
	  FREEIF(boundary);
	  FREEIF(tmp_value);
  }
  newAttachment->encoding = MimeHeaders_get ( headers, 
											  HEADER_CONTENT_TRANSFER_ENCODING,
											  FALSE, FALSE );
  newAttachment->description = MimeHeaders_get( headers, 
												HEADER_CONTENT_DESCRIPTION,
												FALSE, FALSE );
  mdd->tmp_file_name = WH_TempName (xpFileToPost, "nsmail");

  if (!mdd->tmp_file_name)
	return MK_OUT_OF_MEMORY;

  StrAllocCopy (newAttachment->file_name, mdd->tmp_file_name);
  mdd->tmp_file = XP_FileOpen ( mdd->tmp_file_name, 
								xpFileToPost, XP_FILE_WRITE_BIN );
  if (!mdd->tmp_file)
	return MK_UNABLE_TO_OPEN_TMP_FILE;

  /* if need an URL and we don't have one, let's fake one */
  if (needURL && !newAttachment->orig_url) {
	newAttachment->orig_url = PR_smprintf ("file://%s", mdd->tmp_file_name);
  }

  if (creatingMsgBody) {
	MimeDecoderData *(*fn) (int (*) (const char*, int32, void*), void*) = 0;

	/* Initialize a decoder if necessary.
	 */
	if (!newAttachment->encoding || mdd->options->decrypt_p)
	;
	else if (!strcasecomp(newAttachment->encoding, ENCODING_BASE64))
	  fn = &MimeB64DecoderInit;
	else if (!strcasecomp(newAttachment->encoding, ENCODING_QUOTED_PRINTABLE))
	  fn = &MimeQPDecoderInit;
	else if (!strcasecomp(newAttachment->encoding, ENCODING_UUENCODE) ||
			 !strcasecomp(newAttachment->encoding, ENCODING_UUENCODE2) ||
			 !strcasecomp(newAttachment->encoding, ENCODING_UUENCODE3) ||
			 !strcasecomp(newAttachment->encoding, ENCODING_UUENCODE4))
	  fn = &MimeUUDecoderInit;

	if (fn) {
	  mdd->decoder_data =
		fn (/* The (int (*) ...) cast is to turn the `void' argument
			   into `MimeObject'. */
			((int (*) (const char *, int32, void *))
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
								int32 size,
								void *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  int ret = 0;
  
  XP_ASSERT (mdd && buf);
  if (!mdd || !buf) return -1;
  if (!size) return 0;

  /****** doesn't seem to be needed *****
  XP_ASSERT (mdd->tmp_file && mdd->tmp_file_name); 
  */
  if ( !mdd->tmp_file ) 
	return 0;

  if (mdd->decoder_data) {
	ret = MimeDecoderWrite(mdd->decoder_data, buf, size);
	if (ret == -1) return -1;
  }
  else {
    ret = XP_FileWrite(buf, size, mdd->tmp_file);
	if (ret < size)
	  return MK_MIME_ERROR_WRITING_FILE;
  }
  
  return 0;
}

int
mime_decompose_file_close_fn ( void *stream_closure )
{
  struct mime_draft_data *mdd = (struct mime_draft_data *) stream_closure;
  
  /* relax the rule in case we encountered invalid encrypted situation
    XP_ASSERT (mdd && mdd->tmp_file); 
   */
  if ( !mdd || !mdd->tmp_file )
	return -1;

  if ( !mdd->options->is_multipart_msg ) {
	if ( --mdd->options->decompose_init_count > 0 )
	  return 0;
  }

  if (mdd->decoder_data) {
	MimeDecoderDestroy(mdd->decoder_data, FALSE);
	mdd->decoder_data = 0;
  }

  XP_FileClose ( mdd->tmp_file );
  mdd->tmp_file = 0;
  FREEIF ( mdd->tmp_file_name );
  mdd->tmp_file_name = 0;

  return 0;
}

extern NET_StreamClass *
MIME_ToDraftConverter ( int format_out,
			void *closure,
			URL_Struct *url,
			MWContext *context )
{
  int status = 0;
  NET_StreamClass * stream = NULL;
  struct mime_draft_data *mdd = NULL;
  MimeObject *obj;

  XP_ASSERT (url && context);

  if ( !url || !context ) return NULL;

  mdd = XP_NEW_ZAP (struct mime_draft_data);
  if (!mdd) return 0;

  mdd->url = url;
  mdd->context = context;
  mdd->format_out = format_out;

  mdd->options = XP_NEW_ZAP ( MimeDisplayOptions );
  if ( !mdd->options ) {
    XP_FREE (mdd);
    return 0;
  }
  mdd->options->passwd_prompt_fn_arg = context;
  mdd->options->decompose_file_p = TRUE;	/* new field in MimeDisplayOptions */
  mdd->options->url = url->address;
  mdd->options->stream_closure = mdd;
  mdd->options->html_closure = mdd;
  mdd->options->decompose_headers_info_fn = make_mime_headers_copy;
  mdd->options->decompose_file_init_fn = mime_decompose_file_init_fn;
  mdd->options->decompose_file_output_fn = mime_decompose_file_output_fn;
  mdd->options->decompose_file_close_fn = mime_decompose_file_close_fn;
#ifdef FO_MAIL_MESSAGE_TO
  /* If we're attaching a message (for forwarding) then we must eradicate all
	 traces of encryption from it, since forwarding someone else a message
	 that wasn't encrypted for them doesn't work.  We have to decrypt it
	 before sending it.
   */
  mdd->options->decrypt_p = TRUE;
#endif /* FO_MAIL_MESSAGE_TO */

  obj = mime_new ( (MimeObjectClass *) &mimeMessageClass,
		   (MimeHeaders *) NULL,
		   MESSAGE_RFC822 );
  if ( !obj ) {
    FREEIF( mdd->options->part_to_load );
    XP_FREE ( mdd->options );
    XP_FREE ( mdd );
    return 0;
  }
  
  obj->options = mdd->options;
  mdd->obj = obj;

  stream = XP_NEW_ZAP ( NET_StreamClass );
  if ( !stream ) {
    FREEIF( mdd->options->part_to_load );
    XP_FREE ( mdd->options );
    XP_FREE ( mdd );
    XP_FREE ( obj );
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
    XP_FREE ( stream );
    FREEIF( mdd->options->part_to_load );
    XP_FREE ( mdd->options );
    XP_FREE ( mdd );
    XP_FREE ( obj );
    return 0;
  }

  return stream;
}
