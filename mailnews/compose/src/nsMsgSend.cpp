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
#include "nsCOMPtr.h"
#include "msgCore.h"
#include "rosetta_mailnews.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsMsgSendPart.h"
#include "nsMsgSendFact.h"
#include "nsMsgBaseCID.h"
#include "nsMsgNewsCID.h"
#include "nsIMsgHeaderParser.h"
#include "nsINetService.h"
#include "nsISmtpService.h"  // for actually sending the message...
#include "nsINntpService.h"  // for actually posting the message...
#include "nsMsgCompPrefs.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgIdentity.h"
#include "nsMsgSend.h"
#include "nsEscape.h"
#include "nsIPref.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsMsgDeliveryListener.h"
#include "nsIMimeURLUtils.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgEncoders.h"
#include "nsMsgUtils.h"
#include "nsMsgI18N.h"

#include "nsMsgPrompts.h"

// RICHIE ////////////////////////////////////////////////////////////////////////
#define   MK_MSG_MIME_OBJECT_NOT_AVAILABLE    -9999
#define   MK_ATTACHMENT_LOAD_FAILED           -6666
// RICHIE ////////////////////////////////////////////////////////////////////////


/* use these macros to define a class IID for our component. Our object currently supports two interfaces 
   (nsISupports and nsIMsgCompose) so we want to define constants for these two interfaces */
static NS_DEFINE_IID(kIMsgSend, NS_IMSGSEND_IID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kNetServiceCID, NS_NETSERVICE_CID); 
static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kMimeURLUtilsCID, NS_IMIME_URLUTILS_CID);


#ifdef XP_MAC
#include "errors.h"
#include "m_cvstrm.h"

XP_BEGIN_PROTOS
extern OSErr my_FSSpecFromPathname(char* src_filename, FSSpec* fspec);
XP_END_PROTOS

static char* NET_GetLocalFileFromURL(char *url)
{
	char * finalPath;
	NS_ASSERTION(PL_strncasecmp(url, "file://", 7) == 0, "invalid url");
	finalPath = (char*)PR_Malloc(strlen(url));
	if (finalPath == NULL)
		return NULL;
	strcpy(finalPath, url+6+1);
	return finalPath;
}

static char* NET_GetURLFromLocalFile(char *filename)
{
    /*  file:///<path>0 */
	char * finalPath = (char*)PR_Malloc(strlen(filename) + 8 + 1);
	if (finalPath == NULL)
		return NULL;
	finalPath[0] = 0;
	strcat(finalPath, "file://");
	strcat(finalPath, filename);
	return finalPath;
}

#endif /* XP_MAC */

static const char* pSmtpServer = NULL;
static PRBool mime_use_quoted_printable_p = PR_TRUE;

//
// Ugh, we need to do this currently to access this boolean.
//
PRBool
UseQuotedPrintable(void)
{
  return mime_use_quoted_printable_p;
}

// 
// This function will be used by the factory to generate the 
// nsMsgComposeAndSend Object....
//
nsresult NS_NewMsgSend(const nsIID &aIID, void ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMsgComposeAndSend *pSend = new nsMsgComposeAndSend();
		if (pSend)
			return pSend->QueryInterface(kIMsgSend, aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}



static char *mime_mailto_stream_read_buffer = 0;
static char *mime_mailto_stream_write_buffer = 0;

static void mime_attachment_url_exit (URL_Struct *url, int status, MWContext *context);
static void mime_text_attachment_url_exit (PrintSetup *p);

/*JFD extern "C"*/ char * NET_ExplainErrorDetails (int code, ...);


// Returns a newly-allocated string containing the MIME type to be used for
// outgoing attachments of the given document type.  The way this is determined
// will be platform-specific, based on the real filename of the file (i.e. not the temp filename)
// and some Mac creator info.
// If there is no default specified in the prefs, then this returns NULL.
static char *msg_GetMissionControlledOutgoingMIMEType(const char *filename,
													  const char *x_mac_type,
													  const char *x_mac_creator)
{
	if (!filename)
		return NULL;

#ifdef XP_WIN
	char *whereDot = PL_strrchr(filename, '.');
	if (whereDot)
	{
		char *extension = whereDot + 1;
		if (extension)
		{
			char *mcOutgoingMimeType = NULL;
			char *prefString = PR_smprintf("mime.table.extension.%s.outgoing_default_type",extension);

			if (prefString)
			{
        nsresult rv;
        NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
        if (NS_SUCCEEDED(rv) && prefs) 
        {
          // RICHIE
          if (NS_SUCCEEDED(rv) && prefs)
  				  prefs->CopyCharPref(prefString, &mcOutgoingMimeType);
        }

				PR_Free(prefString);
			}
			return mcOutgoingMimeType;
		}
		else
			return NULL;	// no file extension
	}
	else
		return NULL;	// no file extension
	
#endif

	return NULL;
}



static void
mime_attachment_url_exit (URL_Struct *url, int status, MWContext *context)
{
  nsMsgAttachmentHandler *ma = (nsMsgAttachmentHandler *) url->fe_data;
  NS_ASSERTION(ma != NULL, "not-null mime attachment");
  if (ma != NULL)
	ma->UrlExit(url, status, context);
}



static void
mime_text_attachment_url_exit (PrintSetup *p)
{
#if 0 //JFD
  nsMsgAttachmentHandler *ma = (nsMsgAttachmentHandler *) p->carg;
  NS_ASSERTION (p->url == ma->m_url, "different url");
  ma->m_url->fe_data = ma;  /* grr */
  mime_attachment_url_exit (p->url, p->status,
							ma->m_mime_delivery_state->GetContext());
#endif //JFD
}


PRIVATE unsigned int
mime_attachment_stream_write_ready (void *stream)
{	
  return MAX_WRITE_READY;
}

PRIVATE int
mime_attachment_stream_write (void *stream, const char *block, PRInt32 length)
{
  nsMsgAttachmentHandler *ma = (nsMsgAttachmentHandler *) stream;
  /*
  const unsigned char *s;
  const unsigned char *end;
  */  

  if (ma->m_mime_delivery_state->m_status < 0)
	return ma->m_mime_delivery_state->m_status;

  ma->m_size += length;

  if (!ma->m_graph_progress_started)
	{
	  ma->m_graph_progress_started = PR_TRUE;
	  FE_GraphProgressInit (ma->m_mime_delivery_state->GetContext(), ma->m_url,
							ma->m_url->content_length);
	}

  FE_GraphProgress (ma->m_mime_delivery_state->GetContext(), ma->m_url,
					ma->m_size, length, ma->m_url->content_length);


  /* Copy out the content type and encoding if we haven't already.
   */
  if (!ma->m_type && ma->m_url->content_type)
	{
	  ma->m_type = PL_strdup (ma->m_url->content_type);

	  /* If the URL has an encoding, and it's not one of the "null" encodings,
		 then keep it. */
	  if (ma->m_url->content_encoding &&
		  PL_strcasecmp (ma->m_url->content_encoding, ENCODING_7BIT) &&
		  PL_strcasecmp (ma->m_url->content_encoding, ENCODING_8BIT) &&
		  PL_strcasecmp (ma->m_url->content_encoding, ENCODING_BINARY))
		{
		  PR_FREEIF (ma->m_encoding);
		  ma->m_encoding = PL_strdup (ma->m_url->content_encoding);
		  ma->m_already_encoded_p = PR_TRUE;
		}

	  /* Make sure there's a string in the type field.
		 Note that UNKNOWN_CONTENT_TYPE and APPLICATION_OCTET_STREAM are
		 different; "octet-stream" means that this document was *specified*
		 as an anonymous binary type; "unknown" means that we will guess
		 whether it is text or binary based on its contents.
	   */
	  if (!ma->m_type || !*ma->m_type) {
		PR_FREEIF(ma->m_type);
		ma->m_type = PL_strdup(UNKNOWN_CONTENT_TYPE);
	  }


#if defined(XP_WIN) || defined(XP_OS2)
	  /*  WinFE tends to spew out bogus internal "zz-" types for things
		 it doesn't know, so map those to the "real" unknown type.
	   */
	  if (ma->m_type && !PL_strncasecmp (ma->m_type, "zz-", 3)) {
		PR_FREEIF(ma->m_type);
		ma->m_type = PL_strdup(UNKNOWN_CONTENT_TYPE);
	  }
#endif /* XP_WIN */

	  /* There are some of "magnus" types in the default
		 mime.types file that SGI ships in /usr/local/lib/netscape/.  These
		 types are meaningless to the end user, and the server never returns
		 them, but they're getting attached to local .cgi files anyway.
		 Remove them.
	   */
	  if (ma->m_type && !PL_strncasecmp (ma->m_type, "magnus-internal/", 16)) {
		PR_FREEIF(ma->m_type);
		ma->m_type = PL_strdup (UNKNOWN_CONTENT_TYPE);
	  }


	  /* #### COMPLETE HORRIFIC KLUDGE
		 Unfortunately, the URL_Struct shares the `encoding' slot
		 amongst the Content-Encoding and Content-Transfer-Encoding headers.
		 Content-Transfer-Encoding is required to be one of the standard
		 MIME encodings (x- types are explicitly discourgaged.)  But
		 Content-Encoding can be anything (it's HTTP, not MIME.)

		 So, to prevent binary compressed data from getting dumped into the
		 mail stream, we special case some things here.  If the encoding is
		 "x-compress" or "x-gzip", then that must have come from a
		 Content-Encoding header, So change the type to application/x-compress
		 and allow it to be encoded in base64.

		 But what if it's something we don't know?  In that case, we just
		 dump it into the mail.  For Content-Transfer-Encodings, like for
		 example, x-uuencode, that's appropriate.  But for Content-Encodings,
		 like for example, x-some-brand-new-binary-compression-algorithm,
		 that's wrong.
	   */
	  if (ma->m_encoding &&
		  (!PL_strcasecmp (ma->m_encoding, ENCODING_COMPRESS) ||
		   !PL_strcasecmp (ma->m_encoding, ENCODING_COMPRESS2)))
		{
		  PR_FREEIF(ma->m_type);
		  ma->m_type = PL_strdup(APPLICATION_COMPRESS);
		  PR_FREEIF(ma->m_encoding);
		  ma->m_encoding = PL_strdup(ENCODING_BINARY);
		  ma->m_already_encoded_p = PR_FALSE;
		}
	  else if (ma->m_encoding &&
			   (!PL_strcasecmp (ma->m_encoding, ENCODING_GZIP) ||
				!PL_strcasecmp (ma->m_encoding, ENCODING_GZIP2)))
		{
		  PR_FREEIF(ma->m_type);
		  ma->m_type = PL_strdup (APPLICATION_GZIP);
		  PR_FREEIF(ma->m_encoding);
		  ma->m_encoding = PL_strdup (ENCODING_BINARY);
		  ma->m_already_encoded_p = PR_FALSE;
		}

	  /* If the caller has passed in an overriding type for this URL,
		 then ignore what the netlib guessed it to be.  This is so that
		 we can hand it a file:/some/tmp/file and tell it that it's of
		 type message/rfc822 without having to depend on that tmp file
		 having some particular extension.
	   */
	  if (ma->m_override_type)
		{
		  PR_FREEIF(ma->m_type);
		  ma->m_type = PL_strdup (ma->m_override_type);
		  if (ma->m_override_encoding) {
		    PR_FREEIF(ma->m_encoding);
			ma->m_encoding = PL_strdup (ma->m_override_encoding);
		  }
		}

	  char *mcType = msg_GetMissionControlledOutgoingMIMEType(ma->m_real_name, ma->m_x_mac_type, ma->m_x_mac_creator);	// returns an allocated string
	  if (mcType)
	  {
		  PR_FREEIF(ma->m_type);
		  ma->m_type = mcType;
	  }

	}

  /* Cumulatively examine the data that is passing through this stream,
	 building up a histogram that will be used when deciding which encoding
	 (if any) should be used.
   */
  ma->AnalyzeDataChunk(block, length); /* calling this instead of the previous 20ish lines */

  /* Write it to the file.
   */
  while (length > 0)
	{
	  PRInt32 l;
	  l = PR_Write (ma->m_file, block, length);
	  if (l < length)
		{
		  ma->m_mime_delivery_state->m_status = MK_MIME_ERROR_WRITING_FILE;
		  return ma->m_mime_delivery_state->m_status;
		}
	  block += l;
	  length -= l;
	}

  return 1;
}


PRIVATE void
mime_attachment_stream_complete (void *stream)
{	
  /* Nothing to do here - the URL exit method does our cleanup. */
}

PRIVATE void
mime_attachment_stream_abort (void *stream, int status)
{
  nsMsgAttachmentHandler *ma = (nsMsgAttachmentHandler *) stream;  

  if (ma->m_mime_delivery_state->m_status >= 0)
	ma->m_mime_delivery_state->m_status = status;

  /* Nothing else to do here - the URL exit method does our cleanup. */
}


#ifdef XP_OS2
//DSR040297 - OK, I know this looks pretty awful... but, the Visual Age compiler is mega type
//strict when it comes to function pointers & the like... So this must be extern & extern "C" to match.
XP_BEGIN_PROTOS
extern NET_StreamClass *
mime_make_attachment_stream (int /*format_out*/, void * /*closure*/,
							 URL_Struct *url, MWContext *context);
XP_END_PROTOS
extern NET_StreamClass *
#else
static NET_StreamClass *
#endif
mime_make_attachment_stream (int /*format_out*/, void * /*closure*/,
							 URL_Struct *url, MWContext *context)
{
  NET_StreamClass *stream;

  //  TRACEMSG(("Setting up attachment stream. Have URL: %s\n", url->address));

  stream = PR_NEW (NET_StreamClass);
  if (stream == NULL)
	return (NULL);

  memset (stream, 0, sizeof (NET_StreamClass));

/*JFD
  stream->name           = "attachment stream";
  stream->complete       = mime_attachment_stream_complete;
  stream->abort          = mime_attachment_stream_abort;
  stream->put_block      = mime_attachment_stream_write;
  stream->is_write_ready = mime_attachment_stream_write_ready;
  stream->data_object    = url->fe_data;
  stream->window_id      = context;
*/
  //  TRACEMSG(("Returning stream from mime_make_attachment_stream"));

  return stream;
}

char * mime_get_stream_write_buffer(void)
{
	if (!mime_mailto_stream_write_buffer)
		mime_mailto_stream_write_buffer = (char *) PR_Malloc(MIME_BUFFER_SIZE);
	return mime_mailto_stream_write_buffer;
}

int nsMsgComposeAndSend::InitImapOfflineDB(PRUint32 flag)
{
	int status = 0;
	char *folderName = msg_MagicFolderName(m_pane->GetMaster()->GetPrefs(), flag, &status);

	if (status < 0) 
		return status;
	else if (!folderName) 
		return MK_OUT_OF_MEMORY;
	else if (NET_URL_Type(folderName) == IMAP_TYPE_URL)
	{
		nsresult err = NS_OK;
		char *dummyEnvelope = msg_GetDummyEnvelope();
		NS_ASSERTION(!m_imapOutgoingParser && !m_imapLocalMailDB, "not-null ptr");
		err = MailDB::Open(m_msg_file_name, PR_TRUE, &m_imapLocalMailDB);
		if (err != NS_OK) 
		{
			if (m_imapLocalMailDB)
				m_imapLocalMailDB->Close();
			m_imapLocalMailDB = NULL;
			status = MK_OUT_OF_MEMORY;
			goto done;
		}
		m_imapOutgoingParser = new ParseOutgoingMessage;
		if (!m_imapOutgoingParser)
		{
			m_imapLocalMailDB->Close();
			m_imapLocalMailDB = NULL;
			status = MK_OUT_OF_MEMORY;
			goto done;
		}
		m_imapOutgoingParser->Clear();
/*JFD
		m_imapOutgoingParser->SetOutFile(m_msg_file);
*/
		m_imapOutgoingParser->SetMailDB(m_imapLocalMailDB);
		m_imapOutgoingParser->SetWriteToOutFile(PR_FALSE);
		m_imapOutgoingParser->Init(0);
		m_imapOutgoingParser->StartNewEnvelope(dummyEnvelope,
											   PL_strlen(dummyEnvelope));
	}

done:
	PR_FREEIF(folderName);
	return status;
}


/* All of the desired attachments have been written to individual temp files,
   and we know what's in them.  Now we need to make a final temp file of the
   actual mail message, containing all of the other files after having been
   encoded as appropriate.
 */
int nsMsgComposeAndSend::GatherMimeAttachments ()
{
	PRBool shouldDeleteDeliveryState = PR_TRUE;
	PRInt32 status, i;
	char *headers = 0;
	char *separator = 0;
	PRFileDesc  *in_file = 0;
	PRBool multipart_p = PR_FALSE;
	PRBool plaintext_is_mainbody_p = PR_FALSE; // only using text converted from HTML?
	char *buffer = 0;
	char *buffer_tail = 0;
	char* error_msg = 0;
 
  // to news is true if we have a m_field and we have a Newsgroup and it is not empty
	PRBool tonews = PR_FALSE;
	if (m_fields) {
		const char* pstrzNewsgroup = m_fields->GetNewsgroups();
		if (pstrzNewsgroup && *pstrzNewsgroup)
			tonews = PR_TRUE;
	}

	nsMsgI18NMessageSendToNews(tonews);			// hack to make Korean Mail/News work correctly 
											// Look at libi18n/doc_ccc.c for details 
											// temp solution for bug 30725

	nsMsgSendPart* toppart = NULL;			// The very top most container of the message
											// that we are going to send.

	nsMsgSendPart* mainbody = NULL;			// The leaf node that contains the text of the
											// message we're going to contain.

	nsMsgSendPart* maincontainer = NULL;	// The direct child of toppart that will
											// contain the mainbody.  If mainbody is
											// the same as toppart, then this is
											// also the same.  But if mainbody is
											// to end up somewhere inside of a
											// multipart/alternative or a
											// multipart/related, then this is that
											// multipart object.

	nsMsgSendPart* plainpart = NULL;		// If we converted HTML into plaintext,
											// the message or child containing the plaintext
											// goes here. (Need to use this to determine
											// what headers to append/set to the main 
											// message body.)
	char *hdrs = 0;
	PRBool maincontainerISrelatedpart = PR_FALSE;

	status = m_status;
	if (status < 0)
		goto FAIL;

	if (m_attachments_only_p)
	{
		/* If we get here, we shouldn't have the "generating a message" cb. */
		NS_ASSERTION(!m_dont_deliver_p && !m_message_delivery_done_callback, "Shouldn't be here");
		if (m_attachments_done_callback) {
			struct nsMsgAttachedFile *attachments;

			NS_ASSERTION(m_attachment_count > 0, "not more attachment");
			if (m_attachment_count <= 0) {
				m_attachments_done_callback (m_fe_data, 0, 0, 0);
				m_attachments_done_callback = 0;
				Clear();
				goto FAIL;
			}

			attachments = (struct nsMsgAttachedFile *)PR_Malloc((m_attachment_count + 1) * sizeof(*attachments));

			if (!attachments)
				goto FAILMEM;
			memset(attachments, 0, ((m_attachment_count + 1) * sizeof(*attachments)));
			for (i = 0; i < m_attachment_count; i++)
			{
				nsMsgAttachmentHandler *ma = &m_attachments[i];

#undef SNARF
#define SNARF(x,y) do { if((y) && *(y) && !(x)) { ((x) = (y)); ((y) = 0); }} \
				   while(0)
				/* Rather than copying the strings and dealing with allocation
				 failures, we'll just "move" them into the other struct (this
				 should be ok since this file uses PR_FREEIF when discarding
				 the mime_attachment objects.) */
				SNARF(attachments[i].orig_url, ma->m_url_string);
				SNARF(attachments[i].file_name, ma->m_file_name);
				SNARF(attachments[i].type, ma->m_type);
				SNARF(attachments[i].encoding, ma->m_encoding);
				SNARF(attachments[i].description, ma->m_description);
				SNARF(attachments[i].x_mac_type, ma->m_x_mac_type);
				SNARF(attachments[i].x_mac_creator, ma->m_x_mac_creator);
#undef SNARF
				attachments[i].size = ma->m_size;
				attachments[i].unprintable_count = ma->m_unprintable_count;
				attachments[i].highbit_count = ma->m_highbit_count;
				attachments[i].ctl_count = ma->m_ctl_count;
				attachments[i].null_count = ma->m_null_count;
				attachments[i].max_line_length = ma->m_max_column;
				HJ08239

				/* Doesn't really matter, but let's not lie about encoding
				   in case it does someday. */
				if (attachments[i].highbit_count > 0 && attachments[i].encoding &&
						!PL_strcasecmp(attachments[i].encoding, ENCODING_7BIT))
					attachments[i].encoding = ENCODING_8BIT;
			}

			m_attachments_done_callback(m_fe_data, 0, 0, attachments);
			PR_FREEIF(attachments);
			m_attachments_done_callback = 0;
			Clear();
		}
		goto FAIL;
	}


	/* If we get here, we're generating a message, so there shouldn't be an
	   attachments callback. */
	NS_ASSERTION(!m_attachments_done_callback, "shouldn't have an attachement callback!");

	if (!m_attachment1_type) {
		m_attachment1_type = PL_strdup(TEXT_PLAIN);
		if (!m_attachment1_type)
			goto FAILMEM;
	}

	/* If we have a text/html main part, and we need a plaintext attachment, then
	   we'll do so now.  This is an asynchronous thing, so we'll kick it off and
	   count on getting back here when it finishes. */

	if (m_plaintext == NULL &&
			(m_fields->GetForcePlainText() ||
			m_fields->GetUseMultipartAlternative()) &&
			m_attachment1_body && PL_strcmp(m_attachment1_type, TEXT_HTML) == 0)
	{
		m_html_filename = nsMsgCreateTempFileName("nsmail.tmp");
		if (!m_html_filename)
			goto FAILMEM;

		nsFileSpec aPath(m_html_filename);
		nsOutputFileStream tempfile(aPath);
		if (! tempfile.is_open()) {
			status = MK_UNABLE_TO_OPEN_TMP_FILE;
			goto FAIL;
		}

		status = tempfile.write(m_attachment1_body, m_attachment1_body_length);
		if (status < int(m_attachment1_body_length)) {
			if (status >= 0)
				status = MK_MIME_ERROR_WRITING_FILE;
			goto FAIL;
		}

		if (tempfile.failed()) goto FAIL;

		m_plaintext = new nsMsgAttachmentHandler;
		if (!m_plaintext)
			goto FAILMEM;
		m_plaintext->m_mime_delivery_state = this;
		char* temp = WH_FileName(m_html_filename, xpFileToPost);
		m_plaintext->m_url_string = XP_PlatformFileToURL(temp);
		PR_FREEIF(temp);
		if (!m_plaintext->m_url_string)
			goto FAILMEM;
		m_plaintext->m_url = NET_CreateURLStruct(m_plaintext->m_url_string, NET_DONT_RELOAD);
		if (!m_plaintext->m_url)
			goto FAILMEM;
		PR_FREEIF(m_plaintext->m_url->content_type);
		m_plaintext->m_url->content_type = PL_strdup(TEXT_HTML);
		PR_FREEIF(m_plaintext->m_type);
		m_plaintext->m_type = PL_strdup(TEXT_HTML);
		PR_FREEIF(m_plaintext->m_charset);
		m_plaintext->m_charset = PL_strdup(m_fields->GetCharacterSet());
		PR_FREEIF(m_plaintext->m_desired_type);
		m_plaintext->m_desired_type = PL_strdup(TEXT_PLAIN);
		m_attachment_pending_count ++;
		status = m_plaintext->SnarfAttachment();
		if (status < 0)
			goto FAIL;
		if (m_attachment_pending_count > 0)
			return 0;
	}
	  
	/* Kludge to avoid having to allocate memory on the toy computers... */
	buffer = mime_get_stream_write_buffer();
	if (! buffer)
		goto FAILMEM;

	buffer_tail = buffer;

	NS_ASSERTION (m_attachment_pending_count == 0, "m_attachment_pending_count != 0");

#ifdef UNREADY_CODE
	FE_Progress(GetContext(), XP_GetString(MK_MSG_ASSEMBLING_MSG));
#endif

	/* First, open the message file.
	*/
	m_msg_file_name = nsMsgCreateTempFileName("nsmail.tmp");
	if (! m_msg_file_name)
		goto FAILMEM;

	{ //those brackets are needed, please to not remove it else VC++6 will complain
		nsFileSpec msgPath(m_msg_file_name);
		m_msg_file = new nsOutputFileStream(msgPath);
	}
	if (! m_msg_file->is_open()) {
		status = MK_UNABLE_TO_OPEN_TMP_FILE;
#ifdef UNREADY_CODE
		error_msg = PR_smprintf(XP_GetString(status), m_msg_file_name);
		if (!error_msg)
			status = MK_OUT_OF_MEMORY;
#endif
		goto FAIL;
	}

	if (NET_IsOffline() && IsSaveMode()) {
		status = InitImapOfflineDB( m_deliver_mode == nsMsgSaveAsTemplate ?
								  MSG_FOLDER_FLAG_TEMPLATES :
								  MSG_FOLDER_FLAG_DRAFTS );
		if (status < 0) {
#ifdef UNREADY_CODE
			error_msg = PL_strdup (XP_GetString(status));
#endif
			goto FAIL;
		}
	}
	  
#ifdef GENERATE_MESSAGE_ID
	if (m_fields->GetMessageId() == NULL || *m_fields->GetMessageId() == 0)
		m_fields->SetMessageId(msg_generate_message_id (), NULL);
#endif /* GENERATE_MESSAGE_ID */

	mainbody = new nsMsgSendPart(this, m_fields->GetCharacterSet());
	if (!mainbody)
		goto FAILMEM;
  
	mainbody->SetMainPart(PR_TRUE);
	mainbody->SetType(m_attachment1_type ? m_attachment1_type : TEXT_PLAIN);

	NS_ASSERTION(mainbody->GetBuffer() == NULL, "not-null buffer");
	status = mainbody->SetBuffer(m_attachment1_body ? m_attachment1_body : " ");
	if (status < 0)
		goto FAIL;

	/*	### mwelch 
		Determine the encoding of the main message body before we free it.
		The proper way to do this should be to test whatever text is in mainbody
		just before writing it out, but that will require a fix that is less safe
		and takes more memory. */
	PR_FREEIF(m_attachment1_encoding);

  // Check if the part contains 7 bit only. Re-label charset if necessary.
  PRBool body_is_us_ascii;
  if (nsMsgI18Nstateful_charset(m_fields->GetCharacterSet())) {
    body_is_us_ascii = nsMsgI18N7bit_data_part(m_fields->GetCharacterSet(), m_attachment1_body, m_attachment1_body_length); 
  }
  else {
    body_is_us_ascii = mime_7bit_data_p (m_attachment1_body, m_attachment1_body_length);
  }

	if (nsMsgI18Nstateful_charset(m_fields->GetCharacterSet()) ||
		  body_is_us_ascii)
		m_attachment1_encoding = PL_strdup (ENCODING_7BIT);
	else if (mime_use_quoted_printable_p)
		m_attachment1_encoding = PL_strdup (ENCODING_QUOTED_PRINTABLE);
	else
		m_attachment1_encoding = PL_strdup (ENCODING_8BIT);

	PR_FREEIF (m_attachment1_body);

	maincontainer = mainbody;

	// If we were given a pre-saved collection of HTML and contained images,
	// then we want mainbody to point to the HTML lump therein.
	if (m_related_part)
	{
		// If m_related_part is of type text/html, set both maincontainer
		// and mainbody to point to it. If m_related_part is multipart/related,
		// however, set mainbody to be the first child within m_related_part.

		// To plug a memory leak, delete the original maincontainer/mainbody.
		// 
		// NOTE: We DO NOT want to do this to the HTML or multipart/related 
		// nsMsgSendParts because they are deleted at the end of message 
		// delivery by the TapeFileSystem object 
		// (MSG_MimeRelatedSaver / mhtmlstm.cpp).
		delete mainbody;

		// No matter what, maincontainer points to the outermost related part.
		maincontainer = m_related_part;
		maincontainerISrelatedpart = PR_TRUE;

		const char *relPartType = m_related_part->GetType();
		if (relPartType && !strcmp(relPartType, MULTIPART_RELATED))
			// outer shell is m/related,
			// mainbody wants to be the HTML lump within
			mainbody = m_related_part->GetChild(0);
		else
			// outer shell is text/html, 
			// so mainbody wants to be the same lump
			mainbody = maincontainer;

		mainbody->SetMainPart(PR_TRUE);
	}

	if (m_plaintext) {
		// OK.  We have a plaintext version of the main body that we want to
		// send instead of or with the text/html.  Shove it in.

		plainpart = new nsMsgSendPart(this, m_fields->GetCharacterSet());
		if (!plainpart)
			goto FAILMEM;
		status = plainpart->SetType(TEXT_PLAIN);
		if (status < 0)
			goto FAIL;
		status = plainpart->SetFile(m_plaintext->m_file_name, xpFileToPost);
		if (status < 0)
			goto FAIL;
		m_plaintext->AnalyzeSnarfedFile(); // look for 8 bit text, long lines, etc.
		m_plaintext->PickEncoding(m_fields->GetCharacterSet());
		hdrs = mime_generate_attachment_headers(m_plaintext->m_type,
											  m_plaintext->m_encoding,
											  m_plaintext->m_description,
											  m_plaintext->m_x_mac_type,
											  m_plaintext->m_x_mac_creator,
											  m_plaintext->m_real_name, 0,
											  m_digest_p,
											  m_plaintext,
											  m_fields->GetCharacterSet());
		if (!hdrs)
			goto FAILMEM;
		status = plainpart->SetOtherHeaders(hdrs);
		PR_Free(hdrs);
		hdrs = NULL;
		if (status < 0)
			goto FAIL;

		if (m_fields->GetUseMultipartAlternative()) {
			nsMsgSendPart* htmlpart = maincontainer;
			maincontainer = new nsMsgSendPart(this);
			if (!maincontainer)
				goto FAILMEM;
			status = maincontainer->SetType(MULTIPART_ALTERNATIVE);
			if (status < 0)
				goto FAIL;
			status = maincontainer->AddChild(plainpart);
			if (status < 0)
				goto FAIL;
			status = maincontainer->AddChild(htmlpart);
			if (status < 0)
				goto FAIL;

			// Create the encoder for the plaintext part here,
			// because we aren't the main part (attachment1).
			// (This, along with the rest of the routine, should really
			// be restructured so that no special treatment is given to
			// the main body text that came in. Best to put attachment1_text
			// etc. into a nsMsgSendPart, then reshuffle the parts. Sigh.)
			if (!PL_strcasecmp(m_plaintext->m_encoding, ENCODING_QUOTED_PRINTABLE))
			{
				MimeEncoderData *plaintext_enc = MIME_QPEncoderInit(mime_encoder_output_fn, this);
				if (!plaintext_enc)
				{
					status = MK_OUT_OF_MEMORY;
					goto FAIL;
				}
				plainpart->SetEncoderData(plaintext_enc);
			}
		}
		else {
			delete maincontainer; //### mwelch ??? doesn't this cause a crash?!
			if (maincontainerISrelatedpart)
				m_related_part = NULL;
			maincontainer = plainpart;
			mainbody = maincontainer;
			PR_FREEIF(m_attachment1_type);
			m_attachment1_type = PL_strdup(TEXT_PLAIN);
			if (!m_attachment1_type)
				goto FAILMEM;

			/* Override attachment1_encoding here. */
			PR_FREEIF(m_attachment1_encoding);
			m_attachment1_encoding = PL_strdup(m_plaintext->m_encoding);

			plaintext_is_mainbody_p = PR_TRUE; // converted plaintext is mainbody
		}
	}

	// ###tw  This used to be this more complicated thing, but for now, if it we
	// have any attachments, we generate multipart.
	// multipart_p = (m_attachment_count > 1 ||
	//				 (m_attachment_count == 1 &&
	//				  m_attachment1_body_length > 0));
	multipart_p = (m_attachment_count > 0);

	if (multipart_p) {
		toppart = new nsMsgSendPart(this);
		if (!toppart)
			goto FAILMEM;
		status = toppart->SetType(m_digest_p ? MULTIPART_DIGEST : MULTIPART_MIXED);
		if (status < 0)
			goto FAIL;
		status = toppart->AddChild(maincontainer);
		if (status < 0)
			goto FAIL;
		if (!m_crypto_closure) {
#ifdef UNREADY_CODE
      status = toppart->SetBuffer(XP_GetString (MK_MIME_MULTIPART_BLURB));
#else
// RICHIE
#define MIME_MULTIPART_BLURB     "This is a multi-part message in MIME format."

      status = toppart->SetBuffer(MIME_MULTIPART_BLURB);
#endif

			if (status < 0)
				goto FAIL;
		}
	}
	else
		toppart = maincontainer;

  /* Write out the message headers.
   */
	headers = mime_generate_headers (m_fields,
								   m_fields->GetCharacterSet(),
								   m_deliver_mode);
	if (!headers)
		goto FAILMEM;

	// ### mwelch
	// If we converted HTML into plaintext, the plaintext part (plainpart)
	// already has its content-type and content-transfer-encoding
	// ("other") headers set. 
	// 
	// In the specific case where such a plaintext part is the 
	// top level message part (iff an HTML message is being sent
	// as text only and no other attachments exist) we want to 
	// preserve the original plainpart headers, since they
	// contain accurate transfer encoding and Mac type/creator 
	// information.
	// 
	// So, in the above case we append the main message headers, 
	// otherwise we overwrite whatever headers may have existed.
	// 
	/* reordering of headers will happen in nsMsgSendPart::Write */
	if ((plainpart) && (plainpart == toppart))
		status = toppart->AppendOtherHeaders(headers);
	else
		status = toppart->SetOtherHeaders(headers);
	PR_Free(headers);
	headers = NULL;
	if (status < 0)
		goto FAIL;

	/* Set up the first part (user-typed.)  For now, do it even if the first
	* part is empty; we need to add things to skip it if this part is empty.
	* ###tw
	*/


	/* Set up encoder for the first part (message body.)
	*/
	NS_ASSERTION(!m_attachment1_encoder_data, "not-null m_attachment1_encoder_data");
	if (!PL_strcasecmp(m_attachment1_encoding, ENCODING_BASE64))
	{
		m_attachment1_encoder_data = MIME_B64EncoderInit(mime_encoder_output_fn, this);
		if (!m_attachment1_encoder_data) goto FAILMEM;
	}
	else
		if (!PL_strcasecmp(m_attachment1_encoding, ENCODING_QUOTED_PRINTABLE)) {
			m_attachment1_encoder_data =
			MIME_QPEncoderInit(mime_encoder_output_fn, this);
#if 0
			if (!m_attachment1_encoder_data)
				goto FAILMEM;
#endif
		}

	// ### mwelch
	// If we converted HTML into plaintext, the plaintext part
	// already has its type/encoding headers set. So, in the specific
	// case where such a plaintext part is the main message body
	// (iff an HTML message is being sent as text only)
	// we want to avoid generating type/encoding/digest headers;
	// in all other cases, generate such headers here.
	//
	// We really want to set up headers as a dictionary of some sort
	// so that we need not worry about duplicate header lines.
	//
	if ((!plainpart) || (plainpart != mainbody))
	{
		hdrs = mime_generate_attachment_headers (m_attachment1_type,
											   m_attachment1_encoding,
											   0, 0, 0, 0, 0,
											   m_digest_p,
											   NULL, /* no "ma"! */
											   m_fields->GetCharacterSet());
		if (!hdrs)
			goto FAILMEM;
		status = mainbody->AppendOtherHeaders(hdrs);
		if (status < 0)
			goto FAIL;
	}

	PR_FREEIF(hdrs);

	status = mainbody->SetEncoderData(m_attachment1_encoder_data);
	m_attachment1_encoder_data = NULL;
	if (status < 0)
		goto FAIL;


	/* Set up the subsequent parts.
	*/
	if (m_attachment_count > 0)
	{
		/* Kludge to avoid having to allocate memory on the toy computers... */
		if (! mime_mailto_stream_read_buffer)
			mime_mailto_stream_read_buffer = (char *) PR_Malloc (MIME_BUFFER_SIZE);
		buffer = mime_mailto_stream_read_buffer;
		if (! buffer)
			goto FAILMEM;
		buffer_tail = buffer;

		for (i = 0; i < m_attachment_count; i++)
		{
			nsMsgAttachmentHandler *ma = &m_attachments[i];
			hdrs = 0;

			nsMsgSendPart* part = NULL;

			// If at this point we *still* don't have an content-type, then
			// we're never going to get one.
			if (ma->m_type == NULL) {
				ma->m_type = PL_strdup(UNKNOWN_CONTENT_TYPE);
				if (ma->m_type == NULL)
					goto FAILMEM;
			}

			ma->PickEncoding (m_fields->GetCharacterSet());

			part = new nsMsgSendPart(this);
			if (!part)
				goto FAILMEM;
			status = toppart->AddChild(part);
			if (status < 0)
				goto FAIL;
			status = part->SetType(ma->m_type);
			if (status < 0)
				goto FAIL;

			hdrs = mime_generate_attachment_headers (ma->m_type, ma->m_encoding,
												   ma->m_description,
												   ma->m_x_mac_type,
												   ma->m_x_mac_creator,
												   ma->m_real_name,
												   ma->m_url_string,
												   m_digest_p,
												   ma,
												   m_fields->GetCharacterSet());
			if (!hdrs)
				goto FAILMEM;

			status = part->SetOtherHeaders(hdrs);
			PR_FREEIF(hdrs);
			if (status < 0)
				goto FAIL;
			status = part->SetFile(ma->m_file_name, xpFileToPost);
			if (status < 0)
				goto FAIL;
			if (ma->m_encoder_data) {
				status = part->SetEncoderData(ma->m_encoder_data);
				if (status < 0)
					goto FAIL;
				ma->m_encoder_data = NULL;
			}

			ma->m_current_column = 0;

			if (ma->m_type &&
					(!PL_strcasecmp (ma->m_type, MESSAGE_RFC822) ||
					!PL_strcasecmp (ma->m_type, MESSAGE_NEWS))) {
				status = part->SetStripSensitiveHeaders(PR_TRUE);
				if (status < 0)
					goto FAIL;
			}
		}
	}

	// OK, now actually write the structure we've carefully built up.
	status = toppart->Write();
	if (status < 0)
		goto FAIL;

	HJ45609

	if (m_msg_file) {
		/* If we don't do this check...ZERO length files can be sent */
		if (m_msg_file->failed()) {
			status = MK_MIME_ERROR_WRITING_FILE;
			goto FAIL;
		}

    m_msg_file->close();
		delete m_msg_file;
	}
	m_msg_file = NULL;

	if (m_imapOutgoingParser)
	{
		m_imapOutgoingParser->FinishHeader();
		NS_ASSERTION(m_imapOutgoingParser->m_newMsgHdr, "no new message header");
		m_imapOutgoingParser->m_newMsgHdr->OrFlags(MSG_FLAG_READ);
		m_imapOutgoingParser->m_newMsgHdr->SetMessageSize (m_imapOutgoingParser->m_bytes_written);
		m_imapOutgoingParser->m_newMsgHdr->SetMessageKey(0);
		m_imapLocalMailDB->AddHdrToDB(m_imapOutgoingParser->m_newMsgHdr, NULL, PR_TRUE);
	}

#ifdef UNREADY_CODE
	FE_Progress(GetContext(), XP_GetString(MK_MSG_ASSEMB_DONE_MSG));
#endif

	if (m_dont_deliver_p && m_message_delivery_done_callback)
	{
		m_message_delivery_done_callback (
											 m_fe_data, 0,
											 PL_strdup (m_msg_file_name));
		/* Need to ditch the file name here so that we don't delete the
		 file, since in this case, the FE needs the file itself. */
		PR_FREEIF (m_msg_file_name);
		m_msg_file_name = 0;
		m_message_delivery_done_callback = 0;
		Clear();
	}
	else {
		DeliverMessage();
		shouldDeleteDeliveryState = PR_FALSE;
	}

FAIL:
	if (toppart)
		delete toppart;
	toppart = NULL;
	mainbody = NULL;
	maincontainer = NULL;

	PR_FREEIF(headers);
	PR_FREEIF(separator);
	if (in_file) {
		PR_Close (in_file);
		in_file = NULL;
	}

	if (shouldDeleteDeliveryState)
	{
		if (status < 0) {
			m_status = status;
			Fail (status, error_msg);
		}
//JFD don't delete myself		delete this;
	}

	return status;

FAILMEM:
	status = MK_OUT_OF_MEMORY;
	goto FAIL;
}


#if defined(XP_MAC) && defined(DEBUG)
// Compiler runs out of registers for the debug build.
#pragma global_optimizer on
#pragma optimization_level 4
#endif // XP_MAC && DEBUG

# define FROB(X) \
	  if (X && *X) \
		{ \
		  if (*recipients) PL_strcat(recipients, ","); \
		  PL_strcat(recipients, X); \
	  }

HJ91531

#if defined(XP_MAC) && defined(DEBUG)
#pragma global_optimizer reset
#endif // XP_MAC && DEBUG


int
mime_write_message_body (nsMsgComposeAndSend *state,
						 char *buf, PRInt32 size)
{
	HJ62011
	{
		if (PRInt32(state->m_msg_file->write(buf, size)) < size) {
			return MK_MIME_ERROR_WRITING_FILE;
		} else {
			if (state->m_imapOutgoingParser) {
				state->m_imapOutgoingParser->ParseFolderLine(buf, size);
				state->m_imapOutgoingParser->m_bytes_written += size;
			}
			return 0;
		}
	}
}


int
mime_encoder_output_fn (const char *buf, PRInt32 size, void *closure)
{
  nsMsgComposeAndSend *state = (nsMsgComposeAndSend *) closure;
  return mime_write_message_body (state, (char *) buf, size);
}

int nsMsgComposeAndSend::HackAttachments(
						  const struct nsMsgAttachmentData *attachments,
						  const struct nsMsgAttachedFile *preloaded_attachments)
{
	INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(GetContext());
	if (preloaded_attachments)
		NS_ASSERTION(!attachments, "not-null attachments");
	if (attachments)
		NS_ASSERTION(!preloaded_attachments, "not-null preloaded attachments");

	if (preloaded_attachments && preloaded_attachments[0].orig_url) {
		/* These are attachments which have already been downloaded to tmp files.
		We merely need to point the internal attachment data at those tmp
		files.
		*/
		PRInt32 i;

		m_pre_snarfed_attachments_p = PR_TRUE;

		m_attachment_count = 0;
		while (preloaded_attachments[m_attachment_count].orig_url)
			m_attachment_count++;

		m_attachments = (nsMsgAttachmentHandler *)
		new nsMsgAttachmentHandler[m_attachment_count];

		if (! m_attachments)
			return MK_OUT_OF_MEMORY;

		for (i = 0; i < m_attachment_count; i++) {
			m_attachments[i].m_mime_delivery_state = this;
			/* These attachments are already "snarfed". */
			m_attachments[i].m_done = PR_TRUE;
			NS_ASSERTION (preloaded_attachments[i].orig_url, "null url");
			PR_FREEIF(m_attachments[i].m_url_string);
			m_attachments[i].m_url_string = PL_strdup (preloaded_attachments[i].orig_url);
			PR_FREEIF(m_attachments[i].m_type);
			m_attachments[i].m_type = PL_strdup (preloaded_attachments[i].type);
			PR_FREEIF(m_attachments[i].m_charset);
			m_attachments[i].m_charset = PL_strdup (m_fields->GetCharacterSet());
			PR_FREEIF(m_attachments[i].m_description);
			m_attachments[i].m_description = PL_strdup (preloaded_attachments[i].description);
			PR_FREEIF(m_attachments[i].m_real_name);
			m_attachments[i].m_real_name = PL_strdup (preloaded_attachments[i].real_name);
			PR_FREEIF(m_attachments[i].m_x_mac_type);
			m_attachments[i].m_x_mac_type = PL_strdup (preloaded_attachments[i].x_mac_type);
			PR_FREEIF(m_attachments[i].m_x_mac_creator);
			m_attachments[i].m_x_mac_creator = PL_strdup (preloaded_attachments[i].x_mac_creator);
			PR_FREEIF(m_attachments[i].m_encoding);
			m_attachments[i].m_encoding = PL_strdup (preloaded_attachments[i].encoding);
			PR_FREEIF(m_attachments[i].m_file_name);
			m_attachments[i].m_file_name = PL_strdup (preloaded_attachments[i].file_name);

			m_attachments[i].m_size = preloaded_attachments[i].size;
			m_attachments[i].m_unprintable_count =
			preloaded_attachments[i].unprintable_count;
			m_attachments[i].m_highbit_count =
			preloaded_attachments[i].highbit_count;
			m_attachments[i].m_ctl_count = preloaded_attachments[i].ctl_count;
			m_attachments[i].m_null_count =
			preloaded_attachments[i].null_count;
			m_attachments[i].m_max_column =
			preloaded_attachments[i].max_line_length;

			/* If the attachment has an encoding, and it's not one of
			the "null" encodings, then keep it. */
			if (m_attachments[i].m_encoding &&
					PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_7BIT) &&
					PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_8BIT) &&
					PL_strcasecmp (m_attachments[i].m_encoding, ENCODING_BINARY))
				m_attachments[i].m_already_encoded_p = PR_TRUE;

			msg_pick_real_name(&m_attachments[i], m_fields->GetCharacterSet());
		}
	}
	else 
		if (attachments && attachments[0].url) {
		/* These are attachments which have already been downloaded to tmp files.
		We merely need to point the internal attachment data at those tmp
		files.  We will delete the tmp files as we attach them.
		*/
		PRInt32 i;
		int mailbox_count = 0, news_count = 0;

		m_attachment_count = 0;
		while (attachments[m_attachment_count].url)
			m_attachment_count++;

		m_attachments = (nsMsgAttachmentHandler *)
		new nsMsgAttachmentHandler[m_attachment_count];

		if (! m_attachments)
			return MK_OUT_OF_MEMORY;

		for (i = 0; i < m_attachment_count; i++) {
			m_attachments[i].m_mime_delivery_state = this;
			NS_ASSERTION (attachments[i].url, "null url");
			PR_FREEIF(m_attachments[i].m_url_string);
			m_attachments[i].m_url_string = PL_strdup (attachments[i].url);
			PR_FREEIF(m_attachments[i].m_override_type);
			m_attachments[i].m_override_type = PL_strdup (attachments[i].real_type);
			PR_FREEIF(m_attachments[i].m_charset);
			m_attachments[i].m_charset = PL_strdup (m_fields->GetCharacterSet());
			PR_FREEIF(m_attachments[i].m_override_encoding);
			m_attachments[i].m_override_encoding = PL_strdup (attachments[i].real_encoding);
			PR_FREEIF(m_attachments[i].m_desired_type);
			m_attachments[i].m_desired_type = PL_strdup (attachments[i].desired_type);
			PR_FREEIF(m_attachments[i].m_description);
			m_attachments[i].m_description = PL_strdup (attachments[i].description);
			PR_FREEIF(m_attachments[i].m_real_name);
			m_attachments[i].m_real_name = PL_strdup (attachments[i].real_name);
			PR_FREEIF(m_attachments[i].m_x_mac_type);
			m_attachments[i].m_x_mac_type = PL_strdup (attachments[i].x_mac_type);
			PR_FREEIF(m_attachments[i].m_x_mac_creator);
			m_attachments[i].m_x_mac_creator = PL_strdup (attachments[i].x_mac_creator);
			PR_FREEIF(m_attachments[i].m_encoding);
			m_attachments[i].m_encoding = PL_strdup ("7bit");
			PR_FREEIF(m_attachments[i].m_url);
			m_attachments[i].m_url =
			NET_CreateURLStruct (m_attachments[i].m_url_string,
						 NET_DONT_RELOAD);

			// real name is set in the case of vcard so don't change it.
			// m_attachments[i].m_real_name = 0;

			/* Count up attachments which are going to come from mail folders
			and from NNTP servers. */
			if (PL_strncasecmp(m_attachments[i].m_url_string, "mailbox:",8) ||
					PL_strncasecmp(m_attachments[i].m_url_string, "IMAP:",5))
				mailbox_count++;
			else
				if (PL_strncasecmp(m_attachments[i].m_url_string, "news:",5) ||
						PL_strncasecmp(m_attachments[i].m_url_string, "snews:",6))
					news_count++;

			msg_pick_real_name(&m_attachments[i], m_fields->GetCharacterSet());
		}

		/* If there is more than one mailbox URL, or more than one NNTP url,
		do the load in serial rather than parallel, for efficiency.
		*/
		if (mailbox_count > 1 || news_count > 1)
			m_be_synchronous_p = PR_TRUE;

		m_attachment_pending_count = m_attachment_count;

		/* Start the URL attachments loading (eventually, an exit routine will
		call the done_callback). */

#ifdef UNREADY_CODE
		if (m_attachment_count == 1)
			FE_Progress(GetContext(), XP_GetString(MK_MSG_LOAD_ATTACHMNT));
		else
			FE_Progress(GetContext(), XP_GetString(MK_MSG_LOAD_ATTACHMNTS));
#endif

		for (i = 0; i < m_attachment_count; i++) {
			/* This only returns a failure code if NET_GetURL was not called
			(and thus no exit routine was or will be called.) */
			int status = m_attachments [i].SnarfAttachment();
			if (status < 0)
				return status;

			if (m_be_synchronous_p)
				break;
		}
	}

	if (m_attachment_pending_count <= 0)
		/* No attachments - finish now (this will call the done_callback). */
		GatherMimeAttachments ();

	return 0;
}


void nsMsgComposeAndSend::StartMessageDelivery(
						  MSG_Pane   *pane,
						  void       *fe_data,
						  nsMsgCompFields *fields,
						  PRBool digest_p,
						  PRBool dont_deliver_p,
						  nsMsgDeliverMode mode,
						  const char *attachment1_type,
						  const char *attachment1_body,
						  PRUint32 attachment1_body_length,
						  const struct nsMsgAttachmentData *attachments,
						  const struct nsMsgAttachedFile *preloaded_attachments,
						  nsMsgSendPart *relatedPart,
						  void (*message_delivery_done_callback)
						       (
								void *fe_data,
								int status,
								const char *error_message))
{
	int failure = 0;

	if (!attachment1_body || !*attachment1_body)
		attachment1_type = attachment1_body = 0;


	failure = Init(pane, fe_data, fields, 
					digest_p, dont_deliver_p, mode,
					attachment1_type, attachment1_body,
					attachment1_body_length,
					attachments, preloaded_attachments,
					relatedPart,
					message_delivery_done_callback);
	if (failure >= 0)
		return;

	char *err_msg = NET_ExplainErrorDetails (failure);

	if (pane)
		message_delivery_done_callback (fe_data, failure, err_msg);
	if (err_msg)
		PR_Free (err_msg);
}

int nsMsgComposeAndSend::SetMimeHeader(MSG_HEADER_SET header, const char *value)
{
	char * dupHeader = NULL;
	PRInt32	ret = MK_OUT_OF_MEMORY;

	if (header & (MSG_FROM_HEADER_MASK | MSG_TO_HEADER_MASK | MSG_REPLY_TO_HEADER_MASK | MSG_CC_HEADER_MASK | MSG_BCC_HEADER_MASK))
		dupHeader = mime_fix_addr_header(value);
	else if (header &  (MSG_NEWSGROUPS_HEADER_MASK| MSG_FOLLOWUP_TO_HEADER_MASK))
		dupHeader = mime_fix_news_header(value);
	else  if (header & (MSG_FCC_HEADER_MASK | MSG_ORGANIZATION_HEADER_MASK |  MSG_SUBJECT_HEADER_MASK | MSG_REFERENCES_HEADER_MASK | MSG_X_TEMPLATE_HEADER_MASK))
		dupHeader = mime_fix_header(value);
	else
		NS_ASSERTION(PR_FALSE, "invalid header");	// unhandled header mask - bad boy.

	if (dupHeader) {
		m_fields->SetHeader(header, dupHeader, &ret);
		PR_Free(dupHeader);
	}
	return ret;
}



int
nsMsgComposeAndSend::Init(
						  MSG_Pane *pane,
						  void      *fe_data,
						  nsMsgCompFields *fields,
						  PRBool digest_p,
						  PRBool dont_deliver_p,
						  nsMsgDeliverMode mode,

						  const char *attachment1_type,
						  const char *attachment1_body,
						  PRUint32 attachment1_body_length,
						  const struct nsMsgAttachmentData *attachments,
						  const struct nsMsgAttachedFile *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
						  nsMsgSendPart *relatedPart,
//#endif
						  void (*message_delivery_done_callback)
						       (void *fe_data,
								int status,
								const char *error_message))
{
	int failure = 0;
	const char* pStr = NULL;
	m_pane = pane;
	m_fe_data = fe_data;
	m_message_delivery_done_callback = message_delivery_done_callback;

	m_related_part = relatedPart;
	if (m_related_part)
		m_related_part->SetMimeDeliveryState(this);

	NS_ASSERTION (fields, "null fields");
	if (!fields)
		return MK_OUT_OF_MEMORY; /* rb -1; */

	if (m_fields)
		m_fields->Release();

	m_fields = new nsMsgCompFields;
	if (m_fields)
		m_fields->AddRef();
	else
		return MK_OUT_OF_MEMORY;

  PRBool strictly_mime = PR_FALSE;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
  if (NS_SUCCEEDED(rv) && prefs) {
    rv = prefs->GetBoolPref("mail.strictly_mime", &strictly_mime);
  }

  nsMsgMIMESetConformToStandard(strictly_mime);
	mime_use_quoted_printable_p = strictly_mime;

  m_fields->SetCharacterSet(fields->GetCharacterSet(), NULL);

	m_fields->SetOwner(pane);

#ifdef GENERATE_MESSAGE_ID
	pStr = fields->GetMessageId();
	if (pStr)
	{
		m_fields->SetMessageId((char *) pStr, NULL);
		/* Don't bother checking for out of memory; if it fails, then we'll just
		   let the server generate the message-id, and suffer with the
		   possibility of duplicate messages.*/
	}
#endif /* GENERATE_MESSAGE_ID */

	/* Strip whitespace from beginning and end of body. */
	if (attachment1_body)
	{
		/* BUG 115202 -- we used to strip out whitespaces from
		   beginning of body. We are not doing that anymore. */
		while (attachment1_body_length > 0 &&
			IS_SPACE (attachment1_body [attachment1_body_length - 1]))
		{
			attachment1_body_length--;
		}
		if (attachment1_body_length <= 0)
			attachment1_body = 0;

		if (attachment1_body)
		{
			char *newb = (char *) PR_Malloc (attachment1_body_length + 1);
			if (! newb)
				return MK_OUT_OF_MEMORY;
			memcpy (newb, attachment1_body, attachment1_body_length);
			newb [attachment1_body_length] = 0;
			m_attachment1_body = newb;
			m_attachment1_body_length = attachment1_body_length;
		}
	}

	pStr = fields->GetNewspostUrl();
	if (!pStr || !*pStr) {
		HJ41792
	}

	m_fields->SetNewspostUrl((char *) fields->GetNewspostUrl(), NULL);
	m_fields->SetDefaultBody((char *) fields->GetDefaultBody(), NULL);
	PR_FREEIF(m_attachment1_type);
	m_attachment1_type = PL_strdup (attachment1_type);
	PR_FREEIF(m_attachment1_encoding);
	m_attachment1_encoding = PL_strdup ("8bit");

	/* strip whitespace from and duplicate header fields. */
	SetMimeHeader(MSG_FROM_HEADER_MASK, fields->GetFrom());
	SetMimeHeader(MSG_REPLY_TO_HEADER_MASK, fields->GetReplyTo());
	SetMimeHeader(MSG_TO_HEADER_MASK, fields->GetTo());
	SetMimeHeader(MSG_CC_HEADER_MASK, fields->GetCc());
	SetMimeHeader(MSG_FCC_HEADER_MASK, fields->GetFcc());
	SetMimeHeader(MSG_BCC_HEADER_MASK, fields->GetBcc());
	SetMimeHeader(MSG_NEWSGROUPS_HEADER_MASK, fields->GetNewsgroups());
	SetMimeHeader(MSG_FOLLOWUP_TO_HEADER_MASK, fields->GetFollowupTo());
	SetMimeHeader(MSG_ORGANIZATION_HEADER_MASK, fields->GetOrganization());
	SetMimeHeader(MSG_SUBJECT_HEADER_MASK, fields->GetSubject());
	SetMimeHeader(MSG_REFERENCES_HEADER_MASK, fields->GetReferences());
	SetMimeHeader(MSG_X_TEMPLATE_HEADER_MASK, fields->GetTemplateName());

	pStr = fields->GetOtherRandomHeaders();
	if (pStr)
		m_fields->SetOtherRandomHeaders((char *) pStr, NULL);

	pStr = fields->GetPriority();
	if (pStr)
		m_fields->SetPriority((char *) pStr, NULL);

	int i, j = (int) MSG_LAST_BOOL_HEADER_MASK;
	for (i = 0; i < j; i++) {
		m_fields->SetBoolHeader((MSG_BOOL_HEADER_SET) i,
		fields->GetBoolHeader((MSG_BOOL_HEADER_SET) i), NULL);
	}

	m_fields->SetForcePlainText(fields->GetForcePlainText());
	m_fields->SetUseMultipartAlternative(fields->GetUseMultipartAlternative());

	if (pane && m_fields->GetReturnReceipt()) {
		if (m_fields->GetReturnReceiptType() == 1 ||
				m_fields->GetReturnReceiptType() == 3)
			pane->SetRequestForReturnReceipt(PR_TRUE);
		else
			pane->SetRequestForReturnReceipt(PR_FALSE);
	}

	/* Check the fields for legitimacy, and run the callback if they're not
	   ok.  */
	if (mode != nsMsgSaveAsDraft && mode != nsMsgSaveAsTemplate ) {
		failure = mime_sanity_check_fields (m_fields->GetFrom(), m_fields->GetReplyTo(),
										m_fields->GetTo(), m_fields->GetCc(),
										m_fields->GetBcc(), m_fields->GetFcc(),
										m_fields->GetNewsgroups(), m_fields->GetFollowupTo(),
										m_fields->GetSubject(), m_fields->GetReferences(),
										m_fields->GetOrganization(),
										m_fields->GetOtherRandomHeaders());
	}
	if (failure)
		return failure;

	m_digest_p = digest_p;
	m_dont_deliver_p = dont_deliver_p;
	m_deliver_mode = mode;
	// m_msg_file_name = nsMsgCreateTempFileName("nsmail.tmp");

	failure = HackAttachments(attachments, preloaded_attachments);
	return failure;
}

nsresult
MailDeliveryCallback(nsIURL *aUrl, nsresult aExitCode, void *tagData)
{
  nsresult rv = NS_OK;

  if (tagData)
  {
    nsMsgComposeAndSend *ptr = (nsMsgComposeAndSend *) tagData;
    ptr->DeliverAsMailExit(aUrl, aExitCode);
  }

  return rv;
}

extern "C" int
msg_DownloadAttachments (MSG_Pane *pane,
						 void *fe_data,
						 const struct nsMsgAttachmentData *attachments,
						 void (*attachments_done_callback)
						      (void *fe_data,
							   int status, const char *error_message,
							   struct nsMsgAttachedFile *attachments))
{
  nsMsgComposeAndSend *state = 0;
  int failure = 0;

  NS_ASSERTION(attachments && attachments[0].url, "invalid attachment parameter");
  /* if (!attachments || !attachments[0].url)
	{
	  failure = -1;
	  goto FAIL;
	} */ /* The only possible error above is out of memory and it is handled
			in MSG_CompositionPane::DownloadAttachments() */
  state = new nsMsgComposeAndSend;

  if (! state)
	{
	  failure = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  state->m_pane = pane;
  state->m_fe_data = fe_data;
  state->m_attachments_only_p = PR_TRUE;
  state->m_attachments_done_callback =
#ifdef XP_OS2
//DSR040297 - see comment above about 'Casting away extern "C"'
					(void(*)(void*,int,const char*,nsMsgAttachedFile*))
#endif
					attachments_done_callback;

  failure = state->HackAttachments(attachments, 0);
  if (failure >= 0)
	return 0;

 FAIL:
  NS_ASSERTION (failure, "Failed");

  /* in this case, our NET_GetURL exit routine has already freed
	 the state */
  if (failure != MK_ATTACHMENT_LOAD_FAILED)
	{
	  char *err_msg = NET_ExplainErrorDetails (failure);
	  attachments_done_callback (fe_data, failure, err_msg, 0);

	  if (err_msg) PR_Free (err_msg);
#ifdef XP_MAC
		// ### mwelch The MacFE wants this error thrown as an exception.
		//            This is because of the way that error recovery occurs
		//            inside the compose session object.
		if (failure == MK_INTERRUPTED)
			failure = userCanceledErr;
#endif
	}
  delete state;
  return failure;
}

HJ70669


void nsMsgComposeAndSend::DeliverMessage ()
{
	PRBool mail_p = ((m_fields->GetTo() && *m_fields->GetTo()) || 
					(m_fields->GetCc() && *m_fields->GetCc()) || 
					(m_fields->GetBcc() && *m_fields->GetBcc()));
	PRBool news_p = (m_fields->GetNewsgroups() && 
				    *(m_fields->GetNewsgroups()) ? PR_TRUE : PR_FALSE);

	if ( m_deliver_mode != nsMsgSaveAsDraft &&
			m_deliver_mode != nsMsgSaveAsTemplate )
		NS_ASSERTION(mail_p || news_p, "message without destination");

	if (m_deliver_mode == nsMsgQueueForLater) {
		QueueForLater();
		return;
	}
	else if (m_deliver_mode == nsMsgSaveAsDraft) {
		SaveAsDraft();
		return;
	}
	else if (m_deliver_mode == nsMsgSaveAsTemplate) {
		SaveAsTemplate();
		return;
	}

    
#if 0 //JFD - was XP_UNIX only
{
	int status = msg_DeliverMessageExternally(GetContext(), m_msg_file_name);
	if (status != 0) {
		if (status < 0)
			Fail (status, 0);
		else {
			/* The message has now been delivered successfully. */
			MWContext *context = GetContext();
			if (m_message_delivery_done_callback)
				m_message_delivery_done_callback (context, m_fe_data, 0, NULL);
			m_message_delivery_done_callback = 0;

			Clear();

			/* When attaching, even though the context has
			   active_url_count == 0, XFE_AllConnectionsComplete() **is**
			   called.  However, when not attaching, and not delivering right
			   away, we don't actually open any URLs, so we need to destroy
			   the window ourself.  Ugh!!
			 */
			if (m_attachment_count == 0)
				MSG_MailCompositionAllConnectionsComplete(MSG_FindPane(context, MSG_ANYPANE));
		}
		return;
	}
}
#endif /* 0 */

#ifdef MAIL_BEFORE_NEWS
	if (mail_p)
		DeliverFileAsMail ();   /* May call ...as_news() next. */
	else if (news_p)
		DeliverFileAsNews ();
#else  /* !MAIL_BEFORE_NEWS */
	if (news_p)
		DeliverFileAsNews ();   /* May call ...as_mail() next. */
	else if (mail_p)
		DeliverFileAsMail ();
#endif /* !MAIL_BEFORE_NEWS */
	else
		abort ();
}

void nsMsgComposeAndSend::DeliverFileAsMail ()
{
	char *buf, *buf2;

#ifdef UNREADY_CODE
	FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_MAIL));
#endif

	buf = (char *) PR_Malloc ((m_fields->GetTo() ? PL_strlen (m_fields->GetTo())  + 10 : 0) +
						   (m_fields->GetCc() ? PL_strlen (m_fields->GetCc())  + 10 : 0) +
						   (m_fields->GetBcc() ? PL_strlen (m_fields->GetBcc()) + 10 : 0) +
						   10);
	if (! buf) {
		Fail (MK_OUT_OF_MEMORY, 0);
		return;
	}

	PL_strcpy (buf, "");
	buf2 = buf + PL_strlen (buf);
	if (m_fields->GetTo())
		PL_strcat (buf2, m_fields->GetTo());
	if (m_fields->GetCc()) {
		if (*buf2) PL_strcat (buf2, ",");
			PL_strcat (buf2, m_fields->GetCc());
	}
	if (m_fields->GetBcc()) {
		if (*buf2) PL_strcat (buf2, ",");
			PL_strcat (buf2, m_fields->GetBcc());
	}

  nsFileSpec fileSpec (m_msg_file_name ? m_msg_file_name : "");	//need to convert to unix path
  nsFilePath filePath (fileSpec);
  
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsISmtpService, smtpService, kSmtpServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && smtpService)
  {
    nsMsgDeliveryListener *sendListener = new nsMsgDeliveryListener(MailDeliveryCallback, nsMailDelivery, this);
    if (!sendListener)
    {
      nsMsgDisplayMessageByString("Unable to create SMTP listener service. Send failed.");
      return;
    }

    rv = smtpService->SendMailMessage(filePath, buf, sendListener, nsnull);
  }
  
  PR_FREEIF(buf); // free the buf because we are done with it....
}


void nsMsgComposeAndSend::DeliverFileAsNews ()
{
  if (m_fields->GetNewsgroups() == nsnull) {
    return;
  }

  nsFileSpec fileSpec (m_msg_file_name ? m_msg_file_name : "");	//need to convert to unix path
  nsFilePath filePath (fileSpec);

  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsINntpService, nntpService, kNntpServiceCID, &rv);

  if (NS_SUCCEEDED(rv) && nntpService) {	
    rv = nntpService->PostMessage(filePath, m_fields->GetSubject(), m_fields->GetNewsgroups(), nsnull, nsnull);
  }
  
  return;
}

static void
mime_deliver_as_news_exit (URL_Struct *url, int status, MWContext * /*context*/)
{
  nsMsgComposeAndSend *state = (nsMsgComposeAndSend *) url->fe_data;
  state->DeliverAsNewsExit(url, status);
}

void
nsMsgComposeAndSend::DeliverAsNewsExit(URL_Struct *url, int status)
{
  char *error_msg = 0;
  
  url->fe_data = 0;
  if (status < 0 && url->error_msg)
    {
	  error_msg = url->error_msg;
	  url->error_msg = 0;
    }
  NET_FreeURLStruct (url);

  if (status < 0)
	{
	  Fail (status, error_msg);
	}
#ifndef MAIL_BEFORE_NEWS
  else if ((m_fields->GetTo() && *m_fields->GetTo()) || 
		   (m_fields->GetCc() && *m_fields->GetCc()) || 
		   (m_fields->GetBcc() && *m_fields->GetBcc()))
	{
	  /* If we're sending this news message to mail as well, start it now.
		 Completion and further errors will be handled there.
	   */
	  DeliverFileAsMail ();
	}
#endif /* !MAIL_BEFORE_NEWS */
  else
  {
	  status = 0;
	  /*rhp- The message has now been sent successfully! BUT if we fail on the copy 
       to Send folder operation we need to make it better known to the user 
       that ONLY the copy failed. 
     */
	  if (m_fields->GetFcc())
    {
      MSG_Pane *pane = NULL;
      int  retCode = DoFcc();
      if (retCode == FCC_FAILURE)
      {
        // If we hit here, the copy operation FAILED and we should at least tell the
        // user that it did fail but the send operation has already succeeded.
#ifdef UNREADY_CODE
        if (FE_Confirm(GetContext(), XP_GetString(MK_MSG_FAILED_COPY_OPERATION)))
		{
			status = MK_INTERRUPTED;
		}
#endif
      } 
      else if ( retCode == FCC_ASYNC_SUCCESS )
      {
        return;			// asyn imap append message operation; let url exit routine
						// deal with the clean up work
      }
    }

#ifdef UNREADY_CODE
	  FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_NEWS_DONE));
#endif

	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (m_fe_data, status, NULL);
	  m_message_delivery_done_callback = 0;
  	Clear();
//JFD Don't delete myself	  delete this;
	}
}


#ifdef XP_OS2
XP_BEGIN_PROTOS
extern PRInt32 msg_do_fcc_handle_line(char* line, PRInt32 length, void* closure);
XP_END_PROTOS
#else
static PRInt32 msg_do_fcc_handle_line(char* line, PRInt32 length, void* closure);
#endif

#ifdef XP_OS2
extern PRInt32
#else
static PRInt32
#endif
msg_do_fcc_handle_line(char* line, PRInt32 length, void* closure)
{
  ParseOutgoingMessage *outgoingParser = (ParseOutgoingMessage *) closure;
  PRInt32 err = 0;

  PRFileDesc *out = outgoingParser->GetOutFile();

  // if we have a DB, feed the line to the parser
  if (outgoingParser->GetMailDB() != NULL)
	{
	  if (outgoingParser->m_bytes_written == 0)
		err = outgoingParser->StartNewEnvelope(line, length);
	  else
		err = outgoingParser->ParseFolderLine(line, length);
	  if (err < 0)
		  return err;
	}

#ifdef MANGLE_INTERNAL_ENVELOPE_LINES
  /* Note: it is correct to mangle all lines beginning with "From ",
	 not just those that look like parsable message delimiters.
	 Other software expects this. */
  // If this really is the envelope, don't escape it. m_bytes_written will
  // be 0 in that case, because envelope is always first.
  if (outgoingParser->m_bytes_written > 0 && length >= 5 &&
	  line[0] == 'F' && !PL_strncmp(line, "From ", 5))
	{
	  if (PR_Write (out, ">", 1) < 1)
		  return MK_MIME_ERROR_WRITING_FILE;
	  outgoingParser->AdvanceOutPosition(1);
	}
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */

  /* #### if PR_Write is a performance problem, we can put in a
	 call to msg_ReBuffer() here... */
  if (PR_Write (out, line, length) < length)
	  return MK_MIME_ERROR_WRITING_FILE;
  outgoingParser->m_bytes_written += length;
  return 0;
}



char *
nsMsgComposeAndSend::GetOnlineFolderName(PRUint32 flag, const char
											   **pDefaultName)
{
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
	char *onlineFolderName = NULL;

	switch (flag)
	{
	case MSG_FOLDER_FLAG_DRAFTS:
		if (pDefaultName) *pDefaultName = DRAFTS_FOLDER_NAME;
    if (NS_SUCCEEDED(rv) && prefs) 
      prefs->CopyCharPref ("mail.default_drafts", &onlineFolderName);
		break;
	case MSG_FOLDER_FLAG_TEMPLATES:
		if (pDefaultName) *pDefaultName = TEMPLATES_FOLDER_NAME;
    if (NS_SUCCEEDED(rv) && prefs) 
      prefs->CopyCharPref("mail.default_templates", &onlineFolderName);
		break;
	case MSG_FOLDER_FLAG_SENTMAIL:
		if (pDefaultName) *pDefaultName = SENT_FOLDER_NAME;
		onlineFolderName = PL_strdup(m_fields->GetFcc());
		break;
	default:
		NS_ASSERTION(0, "unknow flag");
		break;
	}
	return onlineFolderName;
}

PRBool
nsMsgComposeAndSend::IsSaveMode()
{
	return (m_deliver_mode == nsMsgSaveAsDraft ||
			m_deliver_mode == nsMsgSaveAsTemplate ||
			m_deliver_mode == nsMsgSaveAs);
}


int
nsMsgComposeAndSend::SaveAsOfflineOp()
{
	NS_ASSERTION (m_imapOutgoingParser && 
			   m_imapLocalMailDB && m_imapFolderInfo, "invalid imap save destination");
	if (!m_imapOutgoingParser || !m_imapLocalMailDB || !m_imapFolderInfo)
	{
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
		return FCC_FAILURE;
	}

	int ret_code = FCC_BLOCKING_SUCCESS;
	nsresult err = NS_OK;
	MailDB *mailDB = NULL;
	MSG_IMAPFolderInfoMail *mailFolderInfo =
		m_imapFolderInfo->GetIMAPFolderInfoMail();
	NS_ASSERTION (mailFolderInfo, "null mail folder info");
	err = MailDB::Open (mailFolderInfo->GetPathname(), PR_FALSE, &mailDB);
	if (err == NS_OK)
	{
		MessageKey fakeId = mailDB->GetUnusedFakeId();
		MailMessageHdr *mailMsgHdr = NULL, *newMailMsgHdr = NULL;

		if (IsSaveMode())
		{
			// Do we have an actionInfo?
			MSG_PostDeliveryActionInfo *actionInfo =
				m_pane->GetPostDeliveryActionInfo(); 
			if (actionInfo)
			{
/*JFD
				NeoOfflineImapOperation *op = NULL;
				if (actionInfo->m_msgKeyArray.GetSize() > 0)
				{
					MessageKey msgKey = actionInfo->m_msgKeyArray.GetAt(0);
					if ((PRInt32) msgKey >= 0) // real message key
					{
						// we start with an existing draft and save it while offline 
						// delete the old message and create a new message header
						op = mailDB->GetOfflineOpForKey(msgKey, PR_TRUE);
						if (op)
						{
							op->SetImapFlagOperation(op->GetNewMessageFlags() |
													 kImapMsgDeletedFlag);
							op->unrefer();
						}
						actionInfo->m_msgKeyArray.RemoveAt(0);
						actionInfo->m_msgKeyArray.Add(fakeId);
						mailDB->DeleteMessage(msgKey);
					}
					else // faked UID; reuse it
					{
						fakeId = actionInfo->m_msgKeyArray.GetAt(0);
						mailDB->DeleteOfflineOp(fakeId);
						mailDB->DeleteMessage(fakeId);
					}
				}
				else
				{
					actionInfo->m_msgKeyArray.Add(fakeId);
				}
JFD */
			}
			else
			{
				// this is a new draft create a new actionInfo and a message
				// header 
/*JFD
				actionInfo = new MSG_PostDeliveryActionInfo(m_imapFolderInfo);
				actionInfo->m_flags |= MSG_FLAG_EXPUNGED;
				actionInfo->m_msgKeyArray.Add(fakeId);
				m_pane->SetPostDeliveryActionInfo(actionInfo);
*/
			}
		}
		newMailMsgHdr = new MailMessageHdr;

#if 0 //JFD
		mailMsgHdr = m_imapLocalMailDB->GetMailHdrForKey(0);
		if (mailMsgHdr)
		{
			if (newMailMsgHdr)
			{
				PRFileDesc  *fileId = PR_Open (m_msg_file_name, PR_RDONLY, 0);
				int iSize = 10240;
				char *ibuffer = NULL;
				
				while (!ibuffer && (iSize >= 512))
				{
					ibuffer = (char *) PR_Malloc(iSize);
					if (!ibuffer)
						iSize /= 2;
				}
				
				if (fileId && ibuffer)
				{
					PRInt32 numRead = 0;
					newMailMsgHdr->CopyFromMsgHdr(mailMsgHdr,
												  m_imapLocalMailDB->GetDB(),
												  mailDB->GetDB());

					newMailMsgHdr->SetMessageKey(fakeId);
					err = mailDB->AddHdrToDB(newMailMsgHdr, NULL, PR_TRUE);

					// now write the offline message
					numRead = PR_Read(fileId, ibuffer, iSize);
					while(numRead > 0)
					{
						newMailMsgHdr->AddToOfflineMessage(ibuffer, numRead,
														   mailDB->GetDB());
						numRead = PR_Read(fileId, ibuffer, iSize);
					}
					// now add the offline op to the database
					if (mailDB->GetMaster() == NULL)
						mailDB->SetMaster(m_pane->GetMaster());
					if (mailDB->GetFolderInfo() == NULL)
						mailDB->SetFolderInfo(mailFolderInfo);
					NS_ASSERTION(mailDB->GetMaster(), "null master");
					NeoOfflineImapOperation *op =
						mailDB->GetOfflineOpForKey(fakeId, PR_TRUE);
					if (op)
					{
						op->SetAppendMsgOperation(mailFolderInfo->GetOnlineName(),
												  m_deliver_mode ==
												  MSG_SaveAsDraft ?
												  kAppendDraft :
												  kAppendTemplate);
						op->unrefer();
						/* The message has now been queued successfully. */
						if (m_message_delivery_done_callback)
							m_message_delivery_done_callback (m_fe_data, 0, NULL);
						m_message_delivery_done_callback = 0;
						
						// Clear() clears the Fcc path
						Clear();
					}
					else
					{
						mailDB->RemoveHeaderFromDB (newMailMsgHdr);
						Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
						ret_code = FCC_FAILURE;
					}
				}
				else
				{
					delete newMailMsgHdr;
					Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
					ret_code = FCC_FAILURE;
				}
				if (fileId)
					PR_Close(fileId);
				PR_FREEIF(ibuffer);
			}
			else
			{
				Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
				ret_code = FCC_FAILURE;
			}
			mailMsgHdr->unrefer();
		}
		else
		{
			Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
			ret_code = FCC_FAILURE;
		}
#endif //JFD
	}
	else
	{
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
		ret_code = FCC_FAILURE;
	}
	if (mailDB)
		mailDB->Close();
	return ret_code;
}

void
nsMsgComposeAndSend::ImapAppendAddBccHeadersIfNeeded(URL_Struct *url)
{
	NS_ASSERTION(url, "null parameter");
	const char *bcc_headers = m_fields->GetBcc();
	char *post_data = NULL;
	if (bcc_headers && *bcc_headers)
	{
		post_data = nsMsgCreateTempFileName("nsmail.tmp");
		if (post_data)
		{
			PRFileDesc *dstFile = PR_Open(post_data, PR_WRONLY | PR_CREATE_FILE, 0);
			if (dstFile)
			{
				PRFileDesc *srcFile = PR_Open(m_msg_file_name, PR_RDONLY, 0);
				if (srcFile)
				{
					char *tmpBuffer = NULL;
					int bSize = TEN_K;
					
					while (!tmpBuffer && (bSize >= 512))
					{
						tmpBuffer = (char *)PR_Malloc(bSize);
						if (!tmpBuffer)
							bSize /= 2;
					}
					int bytesRead = 0;
					if (tmpBuffer)
					{
						PR_Write(dstFile, "Bcc: ", 5);
						PR_Write(dstFile, bcc_headers, PL_strlen(bcc_headers));
						PR_Write(dstFile, CRLF, PL_strlen(CRLF));
						bytesRead = PR_Read(srcFile, tmpBuffer, bSize);
						while (bytesRead > 0)
						{
							PR_Write(dstFile, tmpBuffer, bytesRead);
							bytesRead = PR_Read(srcFile, tmpBuffer, bSize);
						}
						PR_Free(tmpBuffer);
					}
					PR_Close(srcFile);
				}
				PR_Close(dstFile);
			}
		}
	}
	else
		post_data = PL_strdup(m_msg_file_name);

	if (post_data)
	{
		MSG_Pane *urlPane = NULL;

		if (!urlPane)
			urlPane = m_pane;

		url->post_data = post_data;
		url->post_data_size = PL_strlen(post_data);
		url->post_data_is_file = PR_TRUE;
		url->method = URL_POST_METHOD;
		url->fe_data = this;
		url->internal_url = PR_TRUE;
		url->msg_pane = urlPane;
		urlPane->GetContext()->imapURLPane = urlPane;
		
/*JFD
		MSG_UrlQueue::AddUrlToPane 
			(url, nsMsgComposeAndSend::PostSendToImapMagicFolder, urlPane,
			 PR_TRUE); 
*/		
	}
	else
	{
		NET_FreeURLStruct(url);
	}
}

/* Send the message to the magic folder, and runs the completion/failure
   callback.
 */
int
nsMsgComposeAndSend::SendToImapMagicFolder ( PRUint32 flag )
{
	char *onlineFolderName = NULL;
	const char *defaultName = "";
	char *name = NULL;
	char *host = NULL;
	char *owner = NULL;
	URL_Struct* url = NULL;
	char* buf = NULL;
	int ret_code = FCC_ASYNC_SUCCESS;

#if 0 //JFD
	if (!m_imapFolderInfo)
	{
		NS_ASSERTION (m_pane, "null pane");
		NS_ASSERTION (m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4(), "not a IMAP server");

		onlineFolderName = GetOnlineFolderName(flag, &defaultName);

		if (onlineFolderName && NET_URL_Type(onlineFolderName) == IMAP_TYPE_URL)
		{
			host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
			name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
			owner = NET_ParseURL(onlineFolderName, GET_USERNAME_PART);
			if (owner)
				nsUnEscape(owner);
			if (!name || !*name)
			{
				PR_FREEIF (name);	// in case of allocated empty string
				name = PR_smprintf("/%s", defaultName);
			}
			if (!owner || !*owner)
			{
/*JFD
				MSG_IMAPHost *imapHost = m_pane->GetMaster()->GetIMAPHost(host);
				if (imapHost) {
					PR_FREEIF(owner)
					owner = PL_strdup(imapHost->GetUserName());
				}
*/
			}
		}

		if (name && *name && host && *host)
		  m_imapFolderInfo = m_pane->GetMaster()->FindImapMailFolder(host,
																	 name+1,
																	 owner,
																	 PR_FALSE);
	}

	if (m_imapFolderInfo)
	{
		if (NET_IsOffline())
		{
			if (flag == MSG_FOLDER_FLAG_DRAFTS || flag == MSG_FOLDER_FLAG_TEMPLATES)
				ret_code = SaveAsOfflineOp();
			else
			{
				NS_ASSERTION(0, "shouldn't be here");	// shouldn't be here
				ret_code = FCC_FAILURE;
			}
		}
		else
		{
			buf = CreateImapAppendMessageFromFileUrl
				( m_imapFolderInfo->GetHostName(),
				  m_imapFolderInfo->GetOnlineName(),
				  m_imapFolderInfo->GetOnlineHierarchySeparator(),
				  IsSaveMode());

			if (buf)
			{
				url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
				if (url)
					// delivery state will be freed by the PostSendToImapMagicFolder
					ImapAppendAddBccHeadersIfNeeded(url);
				else
					ret_code = FCC_FAILURE;
				PR_FREEIF(buf);
			}
			else
				ret_code = FCC_FAILURE;
		}
	}
	else if (host && name && *host && *name && !NET_IsOffline())
	{
		if (m_pane->IMAPListMailboxExist())
		{
			m_pane->SetIMAPListMailboxExist(PR_FALSE);
			buf = CreateImapAppendMessageFromFileUrl
				(host,
				 name+1,
				 kOnlineHierarchySeparatorUnknown, 
				 IsSaveMode());
			if (buf)
			{
				url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
				if (url)
					// delivery state will be freed by the PostSendToImapMagicFolder
					ImapAppendAddBccHeadersIfNeeded(url);
				else
					ret_code = FCC_FAILURE;
				PR_FREEIF(buf);
			}
			else
				ret_code = FCC_FAILURE;
		}
		else
		{
			// Ok, we want to find a folder in the default personal namespace.
			// Call this to generate that folder name.
			TIMAPNamespace *ns = NULL;
			char *ourFolderName = IMAPNS_GenerateFullFolderNameWithDefaultNamespace(host, name+1, NULL /* owner */, kPersonalNamespace, &ns);
			NS_ASSERTION(ourFolderName, "null folder name");
			if (ourFolderName)
			{
				// Now, we need a hierarchy delimiter for this folder.  So find the
				// namespace that the folder will be put in, and get the real delimiter
				// from that.
				NS_ASSERTION(ns, "null name space");
				char delimiter = '/';	// a guess for now
				if (ns)
					delimiter = IMAPNS_GetDelimiterForNamespace(ns);


				buf = CreateImapListUrl(host, ourFolderName, delimiter);
				if (buf)
				{
					url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
					if (url)
					{
						url->fe_data = this;
						url->internal_url = PR_TRUE;
						url->msg_pane = m_pane;
						GetContext()->imapURLPane = m_pane;
						m_pane->SetIMAPListMailbox(name+1);
						m_pane->SetIMAPListMailboxExist(PR_FALSE);
						
						MSG_UrlQueue::AddUrlToPane
							(url, nsMsgComposeAndSend::PostListImapMailboxFolder, m_pane, PR_TRUE);
						
					}
					else
						ret_code = FCC_FAILURE;
					PR_FREEIF(buf);
				}
				else
					ret_code = FCC_FAILURE;
				PR_Free(ourFolderName);
			}
			else
				ret_code = FCC_FAILURE;
		}
	}
	else
	{
#ifdef UNREADY_CODE
		switch (m_deliver_mode)
		{
		case MSG_SaveAsDraft:
			FE_Alert(GetContext(),XP_GetString(MK_MSG_UNABLE_TO_SAVE_DRAFT));
			break;
		case MSG_SaveAsTemplate:
			FE_Alert(GetContext(), XP_GetString(MK_MSG_UNABLE_TO_SAVE_TEMPLATE));
			break;
		case MSG_DeliverNow:
		default:
			FE_Alert(GetContext(), XP_GetString(MK_MSG_COULDNT_OPEN_FCC_FILE));
			break;
		}
#endif
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);	/* -1 rb */
		ret_code = FCC_FAILURE;
	}
	
	PR_FREEIF(host);
	PR_FREEIF(name);
	PR_FREEIF(owner);
	PR_FREEIF(onlineFolderName);
#endif //JFD
	return ret_code;
}


void
nsMsgComposeAndSend::PostCreateImapMagicFolder (	URL_Struct *url,
														int status,
														MWContext * context)
{
#if 0 //JFD
	nsMsgComposeAndSend *state =
		(nsMsgComposeAndSend*) url->fe_data;
	NS_ASSERTION(state, "null delivery state");

	if (status < 0)
	{
		state->Fail (status, 0);
		// kill the delivery state
		delete state;
	}
	else
	{
		MSG_Master::PostCreateImapFolderUrlExitFunc (url, status, context);

		const char *defaultName;
		char *onlineFolderName = NULL;
		PRUint32 flag = 
			state->m_deliver_mode == MSG_SaveAsDraft ? MSG_FOLDER_FLAG_DRAFTS :
			state->m_deliver_mode == MSG_DeliverNow ? MSG_FOLDER_FLAG_SENTMAIL :
			MSG_FOLDER_FLAG_TEMPLATES;
		onlineFolderName = state->GetOnlineFolderName(flag, &defaultName);
		if (onlineFolderName)
		{
			NS_ASSERTION(onlineFolderName, "no online folder");

			char *host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
			char *name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
			char *owner = NULL;
			
			if (!name || !*name)
			{
				PR_FREEIF (name);	// in case of allocated empty string
				name = PR_smprintf("/%s", defaultName);
			}
			
			NS_ASSERTION(host && *host, "null host");
			MSG_IMAPHost *imapHost = 
				state->m_pane->GetMaster()->GetIMAPHost(host);
			if (imapHost)
				owner = PL_strdup(imapHost->GetUserName());
			
			state->m_imapFolderInfo = 
				state->m_pane->GetMaster()->FindImapMailFolder(host, name+1,
															   owner, PR_FALSE);
			NS_ASSERTION(state->m_imapFolderInfo, "null imap folder info");
			
			if (state->m_imapFolderInfo)
			{
				state->m_imapFolderInfo->SetFlag(flag);
				if (state->SendToImapMagicFolder(flag) != FCC_ASYNC_SUCCESS)
				{
					state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
					delete state;
				}
			}
			else
			{
				state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
				// kill the delivery state
				delete state;
			}
			PR_FREEIF(host);
			PR_FREEIF(name);
			PR_FREEIF(owner);
		}
		else
		{
			state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);
			// kill the delivery state
			delete state;
		}
		PR_FREEIF(onlineFolderName);
	}
	NET_FreeURLStruct(url); 
#endif //JFD
}


// returns folder flag and default folder name
PRUint32 nsMsgComposeAndSend::GetFolderFlagAndDefaultName(
	const char **defaultName)
{
	PRUint32 flag = 0;
	
	NS_ASSERTION(defaultName, "null parameter");

	switch (m_deliver_mode)
	{
	case nsMsgSaveAsDraft:
		*defaultName = DRAFTS_FOLDER_NAME;
		flag = MSG_FOLDER_FLAG_DRAFTS;
		break;
	case nsMsgSaveAsTemplate:
		*defaultName = TEMPLATES_FOLDER_NAME;
		flag = MSG_FOLDER_FLAG_TEMPLATES;
		break;
	case nsMsgDeliverNow:
		*defaultName = SENT_FOLDER_NAME;
		flag = MSG_FOLDER_FLAG_SENTMAIL;
		break;
	default:
		NS_ASSERTION(0, "unknow delivery mode");
		*defaultName = "";
		flag = 0;;
	}
	return flag;
}

/* static */ void 
nsMsgComposeAndSend::PostSubscribeImapMailboxFolder (
	URL_Struct *url,
	int status,
	MWContext *context )
{
	nsMsgComposeAndSend *state =
		(nsMsgComposeAndSend*) url->fe_data;
	NS_ASSERTION(state, "null state");
	
	if (status < 0)
	{
		state->Fail (status, 0);
		// kill the delivery state
		delete state;
	}
	else
	{
		const char *defaultName = "";
		PRUint32 flag = state->GetFolderFlagAndDefaultName(&defaultName);
		if (flag == 0)
		{
			state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0); /* -1 rb */
			// kill the delivery state
			delete state;
			NET_FreeURLStruct(url);
			return;
		}
		if (state->SendToImapMagicFolder(flag) != FCC_ASYNC_SUCCESS)
		{
			state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
			delete state;
		}
	}
	NET_FreeURLStruct(url);
}

void
nsMsgComposeAndSend::PostListImapMailboxFolder (	URL_Struct *url,
														int status,
														MWContext *context)
{
	nsMsgComposeAndSend *state =
		(nsMsgComposeAndSend*) url->fe_data;
	NS_ASSERTION(state, "null state");

	if (!state) return;

	if (status < 0)
	{
		/* rhp- This is to handle failed copy operation BUT only if we are trying to send the
		   message. If not, then this was not a Send operation and this prompt doesn't make sense. */
		if (state->m_deliver_mode == nsMsgDeliverNow)
		{
#ifdef UNREADY_CODE
			if (FE_Confirm(state->GetContext(), XP_GetString(MK_MSG_FAILED_COPY_OPERATION)))
			{        
				state->Fail (MK_INTERRUPTED, 0);
			}
			else
#endif
			{
		        // treated as successful close down the compose window
		        /* The message has now been queued successfully. */
				if (state->m_message_delivery_done_callback)
					state->m_message_delivery_done_callback 
						(state->m_fe_data, 0, NULL);
				state->m_message_delivery_done_callback = 0;
				
		        // Clear() clears the Fcc path
				state->Clear();
			}
		}
		else
		{
			state->Fail (status, 0);
		}
		// kill the delivery state
		delete state;
	}
	else
	{
		state->m_pane->SetIMAPListMailbox(NULL);
		if (state->m_pane->IMAPListMailboxExist())
		{
			char *host = NET_ParseURL(url->address, GET_HOST_PART);
			char *name = NET_ParseURL(url->address, GET_PATH_PART);

			if (host && *host && name && *name)
			{
				char *buf = 
					CreateIMAPSubscribeMailboxURL(host, name+1,
												  kOnlineHierarchySeparatorUnknown);
				if (buf)
				{
					URL_Struct *imapSubscribeUrl = 
						NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
					if (imapSubscribeUrl)
					{
						imapSubscribeUrl->fe_data = state;
						imapSubscribeUrl->internal_url = PR_TRUE;
						imapSubscribeUrl->msg_pane = state->m_pane;
						state->m_pane->GetContext()->imapURLPane = state->m_pane;
						
/*JFD
						MSG_UrlQueue::AddUrlToPane(imapSubscribeUrl,
												   PostSubscribeImapMailboxFolder,
												   state->m_pane, PR_TRUE);
*/
					}
					else
						// kill the delivery state
						delete state;
					PR_Free(buf);
				}
				else
					// kill the delivery state
					delete state;
			}
			else
				// kill the delivery state
				delete state;
			PR_FREEIF(host);
			PR_FREEIF(name);
		}
		else
		{
			const char *defaultName = "";
			PRUint32 flag = state->GetFolderFlagAndDefaultName (&defaultName);

			if (flag == 0)
			{
				state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0); /* -1 rb */
				// kill the delivery state
				delete state;
			}
			else
			{

				char *onlineFolderName =
					msg_MagicFolderName(state->m_pane->GetMaster()->GetPrefs(),
										flag, &status); 
				char *buf = NULL;
				char *host = NULL, *name = NULL /*, *owner = NULL */;
				MSG_IMAPHost *imapHost = NULL;

				if (status < 0)
				{
#ifdef UNREADY_CODE
					char *error_msg = XP_GetString(status);
					state->Fail(status, error_msg ? PL_strdup(error_msg) : 0);
#endif
					delete state;
				}
				else if (!onlineFolderName)
				{
					state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
					delete state;
				}
				else
				{
					NS_ASSERTION(NET_URL_Type(onlineFolderName) == IMAP_TYPE_URL, "wrong url type");
					host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
					name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
					// owner = NET_ParseURL(onlineFolderName, GET_USERNAME_PART);
			
					if (!name || !*name)
					{
						PR_FREEIF(name);
						name = PR_smprintf("/%s", defaultName);
					}
					
					imapHost = state->m_pane->GetMaster()->GetIMAPHost(host);
					
					// Now, we need a hierarchy delimiter for this folder.  So find it
					// from its namespace
					TIMAPNamespace *nsForFolder = NULL;
					NS_ASSERTION(imapHost, "null imap host");
					if (imapHost)
					{
						char *fullCanonicalName = 
							IMAPNS_GenerateFullFolderNameWithDefaultNamespace
							(imapHost->GetHostName(), name + 1, 
							 NULL /* owner */, kPersonalNamespace, &nsForFolder);
						if (fullCanonicalName)
						{
							PR_Free(name);
							name = fullCanonicalName;
						}
					}
					// *** Create Imap magic folder
					// *** then append message to the folder
					
					NS_ASSERTION(nsForFolder, "null ptr");
					char delimiter = '/';	// a guess for now
					if (nsForFolder)	// get the real one
						delimiter = IMAPNS_GetDelimiterForNamespace(nsForFolder);
			
					buf = CreateImapMailboxCreateUrl(host, *name == '/' ? name+1 : name,
													 delimiter);
					if (buf)
					{
						URL_Struct* url_struct = NULL;
						url_struct = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
						if (url_struct)
						{
							url_struct->fe_data = state;
							url_struct->internal_url = PR_TRUE;
							url_struct->msg_pane = state->m_pane;
							state->GetContext()->imapURLPane = state->m_pane;
							
/*JFD
							MSG_UrlQueue::AddUrlToPane (url_struct,
														PostCreateImapMagicFolder,
														state->m_pane, PR_TRUE);
*/
						}
						else
							delete state; // error, kill the delivery state
						PR_FREEIF(buf);
					}
					else
						delete state; // kill the delivery state
					PR_FREEIF(onlineFolderName);
					PR_FREEIF(host);
					PR_FREEIF(name);
					// PR_FREEIF(owner);
				}
			}
		}
	}
	NET_FreeURLStruct(url); 
}

void
nsMsgComposeAndSend::SetIMAPMessageUID(MessageKey key)
{
	NS_ASSERTION(m_pane && m_pane->GetPaneType() == MSG_COMPOSITIONPANE, "invalid msg pane");
	nsMsgCompose *composePane = (nsMsgCompose*) m_pane;
/*JFD
	composePane->SetIMAPMessageUID(key);
*/
}

void
nsMsgComposeAndSend::PostSendToImapMagicFolder (	URL_Struct *url,
														int status,
														MWContext *context)
{
#if 0 //JFD
	nsMsgComposeAndSend *state = (nsMsgComposeAndSend*) url->fe_data;
	nsMsgCompose *composePane = (nsMsgCompose*) state->m_pane;
	NS_ASSERTION(state && composePane, "null state or compose pane");

	MSG_PostDeliveryActionInfo *actionInfo =
		composePane->GetPostDeliveryActionInfo();
	MailDB *mailDB = NULL;
	IDArray *idArray = new IDArray;
	char *onlineFolderName = NULL;

	if (PL_strcmp(url->post_data, state->m_msg_file_name))
		PR_Delete(url->post_data);

	if (status < 0)
	{ 
		/* rhp- This is to handle failed copy operation BUT only if we are trying to
		   send the message. If not, then this was not a Send operation and this
		   prompt doesn't make sense. */
		if (state->m_deliver_mode == MSG_DeliverNow)
		{
#ifdef UNREADY_CODE
			if (FE_Confirm(state->GetContext(), 
						   XP_GetString(MK_MSG_FAILED_COPY_OPERATION)))
			{
				state->Fail (MK_INTERRUPTED, 0);
			}
			else
			{
#endif
				// treated as successful close down the compose window
				/* The message has now been queued successfully. */
				if (state->m_message_delivery_done_callback)
					state->m_message_delivery_done_callback 
						(state->GetContext(), state->m_fe_data, 0, NULL);
				state->m_message_delivery_done_callback = 0;
				
				// Clear() clears the Fcc path
				state->Clear();
			}
		}
		else
		{
			state->Fail (status, 0);
		}
	}
	else
	{
		NS_ASSERTION (composePane && composePane->GetPaneType() ==
				   MSG_COMPOSITIONPANE, "invalid compose pane"); 
		
		if (actionInfo  &&
			state->IsSaveMode() &&
			actionInfo->m_msgKeyArray.GetSize() > 1 &&
			actionInfo->m_flags & MSG_FLAG_EXPUNGED)
		{
			MSG_Pane *urlPane = actionInfo->m_folderInfo ?
				state->m_pane->GetMaster()->FindPaneOfType
				(actionInfo->m_folderInfo, MSG_THREADPANE) : NULL;
			
			composePane->DeleteIMAPOldUID(actionInfo, urlPane);
		}
		if (state->m_imapFolderInfo)
		{
			char *dbName = WH_FileName(state->m_imapFolderInfo->GetPathname(), 
									   xpMailFolderSummary);
			if (dbName)
				mailDB = (MailDB *) MessageDB::FindInCache(dbName);
			PR_FREEIF(dbName);
			MSG_Pane *urlPane = NULL;
			
			if (!urlPane)
				urlPane = state->m_pane->GetMaster()->FindPaneOfType
					(state->m_imapFolderInfo, MSG_THREADPANE);

			if (!urlPane)
				urlPane = state->m_pane->GetMaster()->FindPaneOfType
					(state->m_imapFolderInfo, MSG_MESSAGEPANE);
			
			if (!urlPane)
				urlPane = composePane;

			if (mailDB && urlPane)
			{
				char *url_string = CreateImapMailboxNoopUrl(
					state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetHostName(),
					state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetOnlineName(),
					state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetOnlineHierarchySeparator());

				if (url_string)
				{
					URL_Struct *url_struct =
						NET_CreateURLStruct(url_string,
											NET_NORMAL_RELOAD);
					NS_ASSERTION(urlPane, "null url pane");
					if (url_struct)
					{
						state->m_imapFolderInfo->SetFolderLoadingContext(urlPane->GetContext());
						urlPane->SetLoadingImapFolder(state->m_imapFolderInfo);
						url_struct->fe_data = (void *) state->m_imapFolderInfo;
						url_struct->internal_url = PR_TRUE;
						url_struct->msg_pane = urlPane;
						urlPane->GetContext()->imapURLPane = urlPane;

/*JFD
						MSG_UrlQueue::AddUrlToPane (url_struct, 
													MSG_Pane::PostNoopExitFunc,
													urlPane, PR_TRUE);
*/
					}
					PR_FREEIF(url_string);
				}
				// MessageDB::FindInCache() does not add refCount so don't call close
				// mailDB->Close();
			}
			else
			{
				idArray->Add(0);	// add dummy message key
				state->m_imapFolderInfo->UpdatePendingCounts(state->m_imapFolderInfo, 
															 idArray, PR_FALSE);
				state->m_imapFolderInfo->SummaryChanged();
				// make sure we close down the cached imap connection when we done
				// with save draft; this is save draft then send case what about the closing
				// down the compose window case?
				if (urlPane->GetPaneType() != MSG_THREADPANE && 
					state->m_imapFolderInfo->GetFlags() & MSG_FOLDER_FLAG_DRAFTS &&
					state->m_deliver_mode != MSG_SaveAsDraft)
					composePane->GetMaster()->ImapFolderClosed(state->m_imapFolderInfo);
			}
			/* The message has now been queued successfully. */
			if (state->m_message_delivery_done_callback)
				state->m_message_delivery_done_callback (state->m_fe_data, 0, NULL);
			state->m_message_delivery_done_callback = 0;
			
			// Clear() clears the Fcc path
			state->Clear();
		}
		else
		{
			state->Fail(-1, 0);	// something is wrong; maybe wrong password.
		}
	}
		
	PR_FREEIF(onlineFolderName);
	if (idArray)
		delete idArray;
	
	NET_FreeURLStruct(url);
	delete state;
#endif //JFD
}

/* Send the message to the magic folder, and runs the completion/failure
   callback.
 */
void
nsMsgComposeAndSend::SendToMagicFolder ( PRUint32 flag )
{
  char *name = 0;
  int status = 0;

  name = msg_MagicFolderName(m_pane->GetMaster()->GetPrefs(), flag, &status);
  if (status < 0)
  {
#ifdef UNREADY_CODE
	  char *error_msg = XP_GetString(status);
	  Fail (status, error_msg ? PL_strdup(error_msg) : 0);
//JFD don't delete myself	  delete this;
#endif
	  PR_FREEIF(name);
	  return;
  }
  else if (!name || *name == 0)
	{
	  PR_FREEIF(name);
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  else if (NET_URL_Type(name) == IMAP_TYPE_URL &&
	  m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4())
  {
	  if (SendToImapMagicFolder(flag) != FCC_ASYNC_SUCCESS)
	  {
		  Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
//JFD don't delete myself		  delete this;
	  }
	  PR_FREEIF(name);
	  return;
  }
  status = MimeDoFCC (
						  m_msg_file_name, xpFileToPost,
						  name, xpMailFolder,
						  m_deliver_mode, m_fields->GetBcc(), m_fields->GetFcc(),
						  (m_fields->GetNewsgroups() && *m_fields->GetNewsgroups()
						   ? m_fields->GetNewspostUrl() : 0));
  PR_FREEIF (name);

 FAIL:

  if (status < 0)
	{
	  Fail (status, 0);
	}
  else
	{
	  MWContext *context = GetContext();
#ifdef UNREADY_CODE
	  FE_Progress(context, XP_GetString(MK_MSG_QUEUED));
#endif
	  // Clear() clears the m_related_part now. So, we need to decide
	  // whether we need to make an extra all connections complete call or not.
	  PRBool callAllConnectionsComplete =
		  (!m_related_part || !m_related_part->GetNumChildren());

	  /* The message has now been queued successfully. */
	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (
										  m_fe_data, 0, NULL);
	  m_message_delivery_done_callback = 0;

	  Clear();

	  /* When attaching, even though the context has active_url_count == 0,
		 XFE_AllConnectionsComplete() **is** called.  However, when not
		 attaching, and not delivering right away, we don't actually open
		 any URLs, so we need to destroy the window ourself.  Ugh!!
	   */
	  /* Unfortunately, Multipart related message falls into the same category.
	   * If we are sending images within a html message, we'll be chaining URLs
	   * one after another. Which will get FE_AllConnectionsComplete() to be
	   * called from NET_ProcessNet(). To prevent from calling
	   * MSG_MailCompositionAllConnectionsComplete() twice and
	   * not crashing the browser we only call all connections complete
	   * when we are not an mhtml message or an mhtml message which does not 
	   * embed any images. This is really an ugly fix.
	   */
	  if (callAllConnectionsComplete && !XP_IsContextBusy(context))
		  MSG_MailCompositionAllConnectionsComplete(MSG_FindPane(context,
																 MSG_ANYPANE));
	}
//JFD don't delete myself  delete this;
}

/* Queues the message for later delivery, and runs the completion/failure
   callback.
 */
void nsMsgComposeAndSend::QueueForLater()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_QUEUE);
}


/* Save the message to the Drafts folder, and runs the completion/failure
   callback.
 */
void nsMsgComposeAndSend::SaveAsDraft()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_DRAFTS);
}

/* Save the message to the Template folder, and runs the completion/failure
   callback.
 */
void nsMsgComposeAndSend::SaveAsTemplate()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_TEMPLATES);
}






////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
// RICHIE - These are all calls that have been converted to the new world order!
// Once all done, we will reorganize the layout of this file and possibly separate
// classes into separate files for sanity sake.
// Please bear with me.
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
//

/* the following macro actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS(nsMsgComposeAndSend, nsIMsgSend::GetIID());

void 
nsMsgComposeAndSend::Fail(nsresult failure_code, char *error_msg)
{
  if (m_message_delivery_done_callback)
	{
	  m_message_delivery_done_callback (m_fe_data, failure_code, error_msg);
	}
  else if (m_attachments_done_callback)
	{
	  /* mime_free_message_state will take care of cleaning up the
		   attachment files and attachment structures */
	  m_attachments_done_callback (m_fe_data, failure_code, error_msg, 0);
	}

  // Now null out these callbacks since they are processed!
  m_message_delivery_done_callback = nsnull;
  m_attachments_done_callback = nsnull;
  
  Clear();
}

void
nsMsgComposeAndSend::DeliverAsMailExit(nsIURL *aUrl, nsresult aExitCode)
{
  if (NS_FAILED(aExitCode))
  {
    char    *eMsg = ComposeBEGetStringByID(aExitCode);
    Fail(aExitCode, eMsg);
    PR_FREEIF(eMsg);
    return;
  }

  // If we get here...the send was successful, but now we have FCC operations
  // to deal with. 

  // The message has now been sent successfully! BUT if we fail on the copy 
  // to FCC folder operation we need to make it known to the user that the FCC
  // failed. 
  //
  if (m_fields->GetFcc())
  {
    nsresult retCode = DoFcc();
    if (retCode == FCC_FAILURE)
    {
      // If we hit here, the copy operation FAILED and we should at least tell the
      // user that it did fail but the send operation has already succeeded.
      nsMsgDisplayMessageByID(MK_MSG_FAILED_COPY_OPERATION);
    } 
    else if ( retCode == FCC_ASYNC_SUCCESS )
    {
      return; // this is a async imap online append message operation; let
              // url exit function to deal with proper clean up work
    }
  }

  //
  // Finally, we are ready to do last cleanup stuff and move on with life.
  // No real reason to bother the user at this point.
  //      
  if (m_message_delivery_done_callback)
    m_message_delivery_done_callback (m_fe_data, aExitCode, NULL);

  m_message_delivery_done_callback = nsnull;
  Clear();
  delete this;
  return;
}

// 
// Now, the following error codes are returned:
//    #define FCC_FAILURE          0
//    #define FCC_BLOCKING_SUCCESS 1
//    #define FCC_ASYNC_SUCCESS    2
//
nsresult
nsMsgComposeAndSend::DoFcc()
{
  // Just return success if no FCC is necessary
  if (!m_fields->GetFcc() || !*m_fields->GetFcc())
  	return FCC_BLOCKING_SUCCESS;
  else 
  {
    PRInt32                     retType;
    nsCOMPtr<nsIMimeURLUtils>   utilPtr;

    // Check for an IMAP FCC request and do the right thing if it is 
    // an IMAP URL
    nsresult res = nsComponentManager::CreateInstance(kMimeURLUtilsCID, 
                      NULL, nsIMimeURLUtils::GetIID(), 
                      (void **) getter_AddRefs(utilPtr)); 
    if (NS_FAILED(res) || !utilPtr)
    {
      char *tmpMsg = ComposeBEGetStringByID(MK_MSG_MIME_OBJECT_NOT_AVAILABLE);
      Fail(res, tmpMsg);
      PR_FREEIF(tmpMsg);
    }

    utilPtr->URLType(m_fields->GetFcc(), &retType);
    if (retType == IMAP_TYPE_URL)
  	  return SendToImapMagicFolder(MSG_FOLDER_FLAG_SENTMAIL);
  }

  if (! (m_msg_file_name && *m_msg_file_name &&
		     m_fields->GetFcc() && *(m_fields->GetFcc())))
  {
    char *tMsg = ComposeBEGetStringByID(MK_MIME_ERROR_WRITING_FILE);
		Fail (MK_MIME_ERROR_WRITING_FILE, tMsg);
    PR_FREEIF(tMsg);
    return MK_MIME_ERROR_WRITING_FILE;
  }

  // Now, do the FCC operation...
  nsresult rv = MimeDoFCC (	m_msg_file_name, xpFileToPost,
							                  m_fields->GetFcc(), xpMailFolder,
                                nsMsgDeliverNow,
                                m_fields->GetBcc(),
							                  m_fields->GetFcc(), 0);
  if (rv < 0)
  {
    char *tMsg = ComposeBEGetStringByID(MK_MIME_ERROR_WRITING_FILE);
		Fail (-1, tMsg);
    PR_FREEIF(tMsg);
  }

  if (rv >= 0)
    return FCC_BLOCKING_SUCCESS;
  else
    return FCC_FAILURE;
}

void 
nsMsgComposeAndSend::Clear()
{
	PR_FREEIF (m_attachment1_type);
	PR_FREEIF (m_attachment1_encoding);
	PR_FREEIF (m_attachment1_body);

	if (m_fields) 
  {
		m_fields->Release();
		m_fields = NULL;
	}

	if (m_attachment1_encoder_data) 
  {
		MIME_EncoderDestroy(m_attachment1_encoder_data, PR_TRUE);
		m_attachment1_encoder_data = 0;
	}

	if (m_plaintext) 
  {
		if (m_plaintext->m_file)
			PR_Close(m_plaintext->m_file);
		PR_Delete(m_plaintext->m_file_name);
		PR_FREEIF(m_plaintext->m_file_name);
		delete m_plaintext;
		m_plaintext = NULL;
	}

	if (m_html_filename) 
  {
		PR_Delete(m_html_filename);
		PR_FREEIF(m_html_filename);
	}

	if (m_msg_file) 
  {
		delete m_msg_file;
		m_msg_file = 0;
		NS_ASSERTION (m_msg_file_name, "null file name");
	}

	if (m_imapOutgoingParser) 
  {
		delete m_imapOutgoingParser;
		m_imapOutgoingParser = NULL;
	}

	if (m_imapLocalMailDB) 
  {
		m_imapLocalMailDB->Close();
		m_imapLocalMailDB = NULL;
		PR_Delete(m_msg_file_name);
	}

	if (m_msg_file_name) 
  {
		PR_Delete(m_msg_file_name);
		PR_FREEIF (m_msg_file_name);
	}

	HJ82388

	if (m_attachments)
	{
		int i;
		for (i = 0; i < m_attachment_count; i++) 
    {
			if (m_attachments [i].m_encoder_data) 
      {
				MIME_EncoderDestroy(m_attachments[i].m_encoder_data, PR_TRUE);
				m_attachments [i].m_encoder_data = 0;
			}

			PR_FREEIF (m_attachments [i].m_url_string);
			if (m_attachments [i].m_url)
				NET_FreeURLStruct (m_attachments [i].m_url);
			PR_FREEIF (m_attachments [i].m_type);
			PR_FREEIF (m_attachments [i].m_override_type);
			PR_FREEIF (m_attachments [i].m_override_encoding);
			PR_FREEIF (m_attachments [i].m_desired_type);
			PR_FREEIF (m_attachments [i].m_description);
			PR_FREEIF (m_attachments [i].m_x_mac_type);
			PR_FREEIF (m_attachments [i].m_x_mac_creator);
			PR_FREEIF (m_attachments [i].m_real_name);
			PR_FREEIF (m_attachments [i].m_encoding);
			if (m_attachments [i].m_file)
				PR_Close (m_attachments [i].m_file);
			if (m_attachments [i].m_file_name) {
				if (!m_pre_snarfed_attachments_p)
					PR_Delete(m_attachments [i].m_file_name);
				PR_FREEIF (m_attachments [i].m_file_name);
			}

#ifdef XP_MAC
		  /* remove the appledoubled intermediate file after we done all.
		   */
			if (m_attachments [i].m_ap_filename) 
      {
				PR_Delete(m_attachments [i].m_ap_filename);
				PR_FREEIF (m_attachments [i].m_ap_filename);
			}
#endif /* XP_MAC */
		}

		delete[] m_attachments;
		m_attachment_count = m_attachment_pending_count = 0;
		m_attachments = 0;
	}
}

nsMsgComposeAndSend::nsMsgComposeAndSend()
{
	m_pane = NULL;		/* Pane to use when loading the URLs */
	m_fe_data = NULL;		/* passed in and passed to callback */
	m_fields = NULL;			/* Where to send the message once it's done */

	m_dont_deliver_p = PR_FALSE;
	m_deliver_mode = nsMsgDeliverNow;

	m_attachments_only_p = PR_FALSE;
	m_pre_snarfed_attachments_p = PR_FALSE;
	m_digest_p = PR_FALSE;
	m_be_synchronous_p = PR_FALSE;
	m_crypto_closure = NULL;
	m_attachment1_type = 0;
	m_attachment1_encoding = 0;
	m_attachment1_encoder_data = NULL;
	m_attachment1_body = 0;
	m_attachment1_body_length = 0;
	m_attachment_count = 0;
	m_attachment_pending_count = 0;
	m_attachments = NULL;
	m_status = 0;
	m_message_delivery_done_callback = NULL;
	m_attachments_done_callback = NULL;
	m_msg_file_name = NULL;
	m_msg_file = NULL;
	m_plaintext = NULL;
	m_html_filename = NULL;
	m_related_part = NULL;
	m_imapFolderInfo = NULL;
	m_imapOutgoingParser = NULL;
	m_imapLocalMailDB = NULL;

	NS_INIT_REFCNT();
}

nsMsgComposeAndSend::~nsMsgComposeAndSend()
{
	Clear();
}

// RICHIE
#include "nsMsgSendLater.h"

/* This is the main driving function of this module.  It generates a
   document of type message/rfc822, which contains the stuff provided.
   The first few arguments are the standard header fields that the
   generated document should have.

   `other_random_headers' is a string of additional headers that should
   be inserted beyond the standard ones.  If provided, it is just tacked
   on to the end of the header block, so it should have newlines at the
   end of each line, shouldn't have blank lines, multi-line headers
   should be properly continued, etc.

   `digest_p' says that most of the documents we are attaching are
   themselves messages, and so we should generate a multipart/digest
   container instead of multipart/mixed.  (It's a minor difference.)

   The full text of the first attachment is provided via `attachment1_type',
   `attachment1_body' and `attachment1_body_length'.  These may all be 0
   if all attachments are provided externally.

   Subsequent attachments are provided as URLs to load, described in the
   nsMsgAttachmentData structures.

   If `dont_deliver_p' is false, then we actually deliver the message to the
   SMTP and/or NNTP server, and the message_delivery_done_callback will be
   invoked with the status.

   If `dont_deliver_p' is true, then we just generate the message, we don't
   actually deliver it, and the message_delivery_done_callback will be called
   with the name of the generated file.  The callback is responsible for both
   freeing the file name string, and deleting the file when it is done with
   it.  If an error occurred, then `status' will be negative and
   `error_message' may be an error message to display.  If status is non-
   negative, then `error_message' contains the file name (this is kind of
   a kludge...)
 */
nsresult 
nsMsgComposeAndSend::SendMessage(
 						  nsIMsgCompFields                  *fields,
              const char                        *smtp,
						  PRBool                            digest_p,
						  PRBool                            dont_deliver_p,
						  PRInt32                           mode,
						  const char                        *attachment1_type,
						  const char                        *attachment1_body,
						  PRUint32                          attachment1_body_length,
						  const struct nsMsgAttachmentData  *attachments,
						  const struct nsMsgAttachedFile    *preloaded_attachments,
						  void                              *relatedPart,
						  void                              (*message_delivery_done_callback)(void *fe_data,
								                                                                  int status, const char *error_message))
{
	pSmtpServer = smtp;

  
// RICHIE - HACK HACK HACK
//nsIMsgIdentity *userIdentity = nsnull;
//SendUnsentMessages(userIdentity);
//return NS_OK;

  StartMessageDelivery(NULL, NULL, 
                          (nsMsgCompFields *)fields, 
                          digest_p, 
                          dont_deliver_p,
						              (nsMsgDeliverMode) mode,
						              attachment1_type,
						              attachment1_body,
						              attachment1_body_length,
						              attachments,
						              preloaded_attachments,
						              (nsMsgSendPart *)relatedPart,
						              NULL /*message_delivery_done_callback*/);    
	return NS_OK;
}


nsresult 
nsMsgComposeAndSend::MimeDoFCC (
			   const char *input_file_name,  XP_FileType input_file_type,
			   const char *output_name, XP_FileType output_file_type,
			   nsMsgDeliverMode mode,
			   const char *bcc_header,
			   const char *fcc_header,
			   const char *news_url)
{
  int status = 0;
  PRFileDesc  *in = 0;
  PRFileDesc  *out = 0;
  PRBool file_existed_p;
  XP_StatStruct st;
  char *ibuffer = 0;
  int ibuffer_size = TEN_K;
  char *obuffer = 0;
  PRInt32 obuffer_size = 0, obuffer_fp = 0;
  PRInt32 n;
  PRBool summaryWasValid = PR_FALSE;
  PRBool summaryIsValid = PR_FALSE;
  PRBool mark_as_read = PR_TRUE;
  ParseOutgoingMessage *outgoingParser = NULL;
  MailDB	*mail_db = NULL;
  nsresult err = NS_OK;
  char *envelope;
  char *output_file_name = NET_ParseURL(output_name, GET_PATH_PART);
  
  if (!output_file_name || !*output_file_name) { // must be real file path
	  PR_FREEIF(output_file_name);
	  output_file_name = PL_strdup(output_name);
  }

#ifdef UNREADY_CODE
  if (mode == MSG_QueueForLater)
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_QUEUEING));
  else if ( mode == MSG_SaveAsDraft )
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_SAVING_AS_DRAFT));
  else if ( mode == MSG_SaveAsTemplate )
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_SAVING_AS_TEMPLATE));
  else
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_WRITING_TO_FCC));
#endif

  ibuffer = NULL;
  while (!ibuffer && (ibuffer_size >= 1024))
  {
	  ibuffer = (char *) PR_Malloc (ibuffer_size);
	  if (!ibuffer)
		  ibuffer_size /= 2;
  }
  if (!ibuffer)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  file_existed_p = !XP_Stat (output_file_name, &st, output_file_type);

  if (file_existed_p)
	{
	  summaryWasValid = msg_IsSummaryValid(output_file_name, &st);
	  if (!msg_ConfirmMailFile (pane->GetContext(), output_file_name))
		{
		  PR_FREEIF(output_file_name);
		  return MK_INTERRUPTED; /* #### What a hack.  It turns out we already
									were testing for this result code and
									silently canceling the send if we ever got
									it (because it meant that the user hit the
									Stop button).  Returning it here has a
									similar effect -- the user was asked to
									confirm writing to the FCC folder, and he
									hit the Cancel button, so we now quietly
									do nothing. */

		}
	}
  else
	{
#ifdef UNREADY_CODE
	  pane->GetMaster()->FindMailFolder(output_file_name,
										PR_TRUE /*createIfMissing*/);

	  if (-1 == XP_Stat (output_file_name, &st, output_file_type))
		FE_Alert (pane->GetContext(), XP_GetString(MK_MSG_CANT_CREATE_FOLDER));
#endif
	}


  out = PR_Open (output_file_name, PR_APPEND, 493);
  if (!out)
	{
	  /* #### include file name in error message! */
    /* Need to determine what type of operation failed and set status accordingly. Used to always return
       MK_MSG_COULDNT_OPEN_FCC_FILE */
    switch (mode)
		{
		case nsMsgSaveAsDraft:
			status = MK_MSG_UNABLE_TO_SAVE_DRAFT;
			break;
		case nsMsgSaveAsTemplate:
			status = MK_MSG_UNABLE_TO_SAVE_TEMPLATE;
			break;
		case nsMsgDeliverNow:
		default:
			status = MK_MSG_COULDNT_OPEN_FCC_FILE;
			break;
		}
	  goto FAIL;
	}

  in = PR_Open (input_file_name, PR_RDONLY, 0);
  if (!in)
	{
	  status = MK_UNABLE_TO_OPEN_FILE; /* rb -1; */ /* How did this happen? */
	  goto FAIL;
	}


  // set up database and outbound message parser to keep db up to date.
  outgoingParser = new ParseOutgoingMessage;
  err = MailDB::Open(output_file_name, PR_FALSE, &mail_db);

  if (err != NS_OK)
	mail_db = NULL;

  if (!outgoingParser)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  outgoingParser->SetOutFile(out);
  outgoingParser->SetMailDB(mail_db);
  outgoingParser->SetWriteToOutFile(PR_FALSE);


  /* Write a BSD mailbox envelope line to the file.
	 If the file existed, preceed it by a linebreak: this file format wants a
	 *blank* line before all "From " lines except the first.  This assumes
	 that the previous message in the file was properly closed, that is, that
	 the last line in the file ended with a linebreak.
   */
  PR_Seek(out, 0, PR_SEEK_END);

  if (file_existed_p && st.st_size > 0)
	{
	  if (PR_Write (out, MSG_LINEBREAK, MSG_LINEBREAK_LEN) < MSG_LINEBREAK_LEN)
		{
		  status = MK_MIME_ERROR_WRITING_FILE;
		  goto FAIL;
		}
	}

  outgoingParser->Init(PR_Seek(out, 0, PR_SEEK_CUR));
  envelope = msg_GetDummyEnvelope();

  if (msg_do_fcc_handle_line (envelope, PL_strlen (envelope), outgoingParser)
	  < 0)
	{
	  status = MK_MIME_ERROR_WRITING_FILE;
	  goto FAIL;
	}

  /* Write out an X-Mozilla-Status header.

	 This is required for the queue file, so that we can overwrite it once
	 the messages have been delivered, and so that the MSG_FLAG_QUEUED bit
	 is set.

	 For FCC files, we don't necessarily need one, but we might as well put
	 one in so that it's marked as read already.
   */
  if (mode == nsMsgQueueForLater ||
	  mode == nsMsgSaveAsDraft ||
	  mode == nsMsgSaveAsTemplate ||
	  mark_as_read)
	{
	  char *buf = 0;
	  PRUint16 flags = 0;

	  mark_as_read = PR_TRUE;
	  flags |= MSG_FLAG_READ;
	  if (mode == nsMsgQueueForLater )
		flags |= MSG_FLAG_QUEUED;
	  buf = PR_smprintf(X_MOZILLA_STATUS_FORMAT MSG_LINEBREAK, flags);
	  if (buf)
	  {
		  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
		  PR_FREEIF(buf);
		  if (status < 0)
			goto FAIL;
	  }
	  
	  PRUint32 flags2 = 0;
	  if (mode == nsMsgSaveAsTemplate)
		  flags2 |= MSG_FLAG_TEMPLATE;
	  buf = PR_smprintf(X_MOZILLA_STATUS2_FORMAT MSG_LINEBREAK, flags2);
	  if (buf)
	  {
		  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
		  PR_FREEIF(buf);
		  if (status < 0)
			  goto FAIL;
	  }
	}


  /* Write out the FCC and BCC headers.
	 When writing to the Queue file, we *must* write the FCC and BCC
	 headers, or else that information would be lost.  Because, when actually
	 delivering the message (with "deliver now") we do FCC/BCC right away;
	 but when queueing for later delivery, we do FCC/BCC at delivery-time.

	 The question remains of whether FCC and BCC should be written into normal
	 BCC folders (like the Sent Mail folder.)

	 For FCC, there seems no point to do that; it's not information that one
	 would want to refer back to.

	 For BCC, the question isn't as clear.  On the one hand, if I send someone
	 a BCC'ed copy of the message, and save a copy of it for myself (with FCC)
	 I might want to be able to look at that message later and see the list of
	 people to whom I had BCC'ed it.

	 On the other hand, the contents of the BCC header is sensitive
	 information, and should perhaps not be stored at all.

	 Thus the consultation of the #define SAVE_BCC_IN_FCC_FILE.

	 (Note that, if there is a BCC header present in a message in some random
	 folder, and that message is forwarded to someone, then the attachment
	 code will strip out the BCC header before forwarding it.)
   */
  if ((mode == nsMsgQueueForLater ||
	   mode == nsMsgSaveAsDraft ||
	   mode == nsMsgSaveAsTemplate) &&
	  fcc_header && *fcc_header)
	{
	  PRInt32 L = PL_strlen(fcc_header) + 20;
	  char *buf = (char *) PR_Malloc (L);
	  if (!buf)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  PR_snprintf(buf, L-1, "FCC: %s" MSG_LINEBREAK, fcc_header);
	  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
	  if (status < 0)
		goto FAIL;
	}
  if (bcc_header && *bcc_header
#ifndef SAVE_BCC_IN_FCC_FILE
	  && (mode == MSG_QueueForLater ||
		  mode == MSG_SaveAsDraft ||
		  mode == MSG_SaveAsTemplate)
#endif
	  )
	{
	  PRInt32 L = PL_strlen(bcc_header) + 20;
	  char *buf = (char *) PR_Malloc (L);
	  if (!buf)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  PR_snprintf(buf, L-1, "BCC: %s" MSG_LINEBREAK, bcc_header);
	  status = msg_do_fcc_handle_line(buf, PL_strlen(buf), outgoingParser);
	  if (status < 0)
		goto FAIL;
	}


  /* Write out the X-Mozilla-News-Host header.
	 This is done only when writing to the queue file, not the FCC file.
	 We need this to complement the "Newsgroups" header for the case of
	 queueing a message for a non-default news host.

	 Convert a URL like "snews://host:123/" to the form "host:123/secure"
	 or "news://user@host:222" to simply "host:222".
   */
  if ((mode == nsMsgQueueForLater ||
	   mode == nsMsgSaveAsDraft ||
	   mode == nsMsgSaveAsTemplate) && news_url && *news_url)
	{
	  PRBool secure_p = (news_url[0] == 's' || news_url[0] == 'S');
	  char *orig_hap = NET_ParseURL (news_url, GET_HOST_PART);
	  char *host_and_port = orig_hap;
	  if (host_and_port)
		{
		  /* There may be authinfo at the front of the host - it could be of
			 the form "user:password@host:port", so take off everything before
			 the first at-sign.  We don't want to store authinfo in the queue
			 folder, I guess, but would want it to be re-prompted-for at
			 delivery-time.
		   */
		  char *at = PL_strchr (host_and_port, '@');
		  if (at)
			host_and_port = at + 1;
		}

	  if ((host_and_port && *host_and_port) || !secure_p)
		{
		  char *line = PR_smprintf(X_MOZILLA_NEWSHOST ": %s%s" MSG_LINEBREAK,
								   host_and_port ? host_and_port : "",
								   secure_p ? "/secure" : "");
		  PR_FREEIF(orig_hap);
		  orig_hap = 0;
		  if (!line)
			{
			  status = MK_OUT_OF_MEMORY;
			  goto FAIL;
			}
		  status = msg_do_fcc_handle_line(line, PL_strlen(line),
										  outgoingParser);
		  PR_FREEIF(line);

		  if (status < 0)
			goto FAIL;
		}
	  PR_FREEIF(orig_hap);
	  orig_hap = 0;
	}


  /* Read from the message file, and write to the FCC or Queue file.
	 There are two tricky parts: the first is that the message file
	 uses CRLF, and the FCC file should use LINEBREAK.  The second
	 is that the message file may have lines beginning with "From "
	 but the FCC file must have those lines mangled.

	 It's unfortunate that we end up writing the FCC file a line
	 at a time, but it's the easiest way...
   */

  while (1)
	{
	  n = PR_Read (in, ibuffer, ibuffer_size);
	  if (n == 0)
		break;
	  if (n < 0) /* read failed (not eof) */
		{
		  status = n;
		  goto FAIL;
		}

	  n = (PRInt32)msg_LineBuffer (ibuffer, n,
						  &obuffer, (PRUint32 *)&obuffer_size,
						  (PRUint32*)&obuffer_fp,
						  PR_TRUE, msg_do_fcc_handle_line,
						  outgoingParser);
	  if (n < 0) /* write failed */
		{
		  status = n;
		  goto FAIL;
		}
	}

  /* If there's still stuff in the buffer (meaning the last line had no
	 newline) push it out. */
  if (obuffer_fp > 0)
	msg_do_fcc_handle_line (obuffer, obuffer_fp, outgoingParser);

  /* Terminate with a final newline. */
  if (PR_Write (out, MSG_LINEBREAK, MSG_LINEBREAK_LEN) < MSG_LINEBREAK_LEN)
  {
	  status = MK_MIME_ERROR_WRITING_FILE;
  }
  else
	outgoingParser->AdvanceOutPosition(MSG_LINEBREAK_LEN);

  if (mail_db != NULL && outgoingParser != NULL &&
	  outgoingParser->m_newMsgHdr != NULL)
	{

	  outgoingParser->FinishHeader();
	  mail_db->AddHdrToDB(outgoingParser->m_newMsgHdr, NULL, PR_TRUE);
	  if (summaryWasValid)
		summaryIsValid = PR_TRUE;
	}

 FAIL:

  if (ibuffer)
	PR_Free (ibuffer);
  if (obuffer && obuffer != ibuffer)
	PR_Free (obuffer);

  if (in)
	PR_Close (in);

  if (out)
	{
	  if (status >= 0)
		{
		  PR_Close (out);
		  if (summaryIsValid) {
			msg_SetSummaryValid(output_file_name, 0, 0);
                  }
		}
	  else if (! file_existed_p)
		{
		  PR_Close (out);
		  PR_Delete(output_file_name);
		}
	  else
		{
		  PR_Close (out);
		  XP_FileTruncate (output_file_name, output_file_type, st.st_size);	/* restore original size */
		}
	}

  if (mail_db != NULL && status >= 0) {
	if ( mode == nsMsgSaveAsDraft || mode == nsMsgSaveAsTemplate )
	{
/*JFD
		MSG_PostDeliveryActionInfo *actionInfo =
		  pane->GetPostDeliveryActionInfo();
		if (actionInfo) {
		  if (actionInfo->m_flags & MSG_FLAG_EXPUNGED &&
			  actionInfo->m_msgKeyArray.GetSize() >= 1) {
			mail_db->DeleteMessage(actionInfo->m_msgKeyArray.GetAt(0));
			actionInfo->m_msgKeyArray.RemoveAt(0);
		  }
		}
		else {
		  actionInfo = new MSG_PostDeliveryActionInfo((MSG_FolderInfo*)
			pane->GetMaster()->
			FindMailFolder(output_file_name, PR_TRUE));
		  if (actionInfo) {
			actionInfo->m_flags |= MSG_FLAG_EXPUNGED;
			pane->SetPostDeliveryActionInfo(actionInfo);
		  }
		}
		if (outgoingParser->m_newMsgHdr && actionInfo)
		  actionInfo->m_msgKeyArray.Add(outgoingParser->m_newMsgHdr->GetMessageKey());
*/
	}
	mail_db->Close();
  }

  PR_FREEIF(output_file_name);

  delete outgoingParser;
  if (status < 0)
	{
	  /* Fail, and terminate. */
	  return status;
	}
  else
	{
	  /* Otherwise, continue on to _deliver_as_mail or _deliver_as_news
		 or mime_queue_for_later. */
	  return 0;
	}
}
