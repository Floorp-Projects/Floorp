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
/*   compose.c --- generation and delivery of MIME objects.
 */

#include "rosetta.h"
#include "msg.h"
#include "ntypes.h"
#include "structs.h"
#include "xlate.h"		/* Text and PostScript converters */
#include "merrors.h"
#include "gui.h"		/* for XP_AppCodeName */
#include "mime.h"
#include "xp_time.h"	/* For XP_LocalZoneOffset() */
#include "libi18n.h"
#include "xpgetstr.h"
#include "prtime.h"
#include "prtypes.h"
#include "msgcom.h"
#include "msgsend.h"
#include "msgsendp.h"
#include "maildb.h"
#include "mailhdr.h"
#include "msgprefs.h"
#include "msgmast.h"
#include "msgcpane.h"
#ifndef NO_SECURITY
#include  HG02902
#endif
#include "grpinfo.h"
#include "msgcflds.h"
#include "prefapi.h"
#include "abdefn.h"
#include "prsembst.h"
#include "secrng.h"	/* for RNG_GenerateGlobalRandomBytes() */
#include "addrbook.h"
#include "imaphost.h"
#include "imapoff.h"
#ifdef XP_MAC
#include "errors.h"
#endif
#include "intl_csi.h"
#include "msgimap.h"
#include "msgurlq.h"

#ifdef XP_MAC
#pragma warn_unusedarg off
#endif // XP_MAC

extern "C"
{
	extern int MK_MSG_ASSEMBLING_MSG;
	extern int MK_MSG_ASSEMB_DONE_MSG;
	extern int MK_MSG_LOAD_ATTACHMNT;
	extern int MK_MSG_LOAD_ATTACHMNTS;
	extern int MK_MSG_DELIV_MAIL;
	extern int MK_MSG_DELIV_MAIL_DONE;
	extern int MK_MSG_DELIV_NEWS;
	extern int MK_MSG_DELIV_NEWS_DONE;
	extern int MK_MSG_QUEUEING;
	extern int MK_MSG_WRITING_TO_FCC;
	extern int MK_MSG_QUEUED;
	extern int MK_MIME_ERROR_WRITING_FILE;
	extern int MK_MIME_MULTIPART_BLURB;
	extern int MK_MIME_NO_RECIPIENTS;
	extern int MK_MIME_NO_SENDER;
	extern int MK_MSG_COULDNT_OPEN_FCC_FILE;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_TMP_FILE;
	extern int MK_MSG_CANT_CREATE_FOLDER;
	extern int MK_MSG_SAVE_DRAFT;
	extern int MK_ADDR_BOOK_CARD;
	extern int MK_MSG_MAC_PROMPT_UUENCODE;
	extern int MK_MSG_SAVING_AS_DRAFT;
	extern int MK_MSG_SAVING_AS_TEMPLATE;
	extern int MK_MSG_UNABLE_TO_SAVE_DRAFT;
	extern int MK_MSG_UNABLE_TO_SAVE_TEMPLATE;
	extern int MK_UNABLE_TO_OPEN_FILE;
	extern int MK_IMAP_UNABLE_TO_SAVE_MESSAGE;
	extern int MK_IMAP_NO_ONLINE_FOLDER;
}

#ifdef XP_MAC
#include "m_cvstrm.h"
#endif

#define TEN_K 10240
#define MK_ATTACHMENT_LOAD_FAILED -666

/* Asynchronous mailing of messages with attached URLs.

   - If there are any attachments, start their URLs going, and write each
     of them to a temp file.

   - While writing to their files, examine the data going by and decide
     what kind of encoding, if any, they need.  Also remember their content
     types.

   - Once that URLs has been saved to a temp file (or, if there were no
     attachments) generate a final temp file, of the actual message:

	 -  Generate a string of the headers.
	 -  Open the final temp file.
	 -  Write the headers.
     -  Examine the first part, and decide whether to encode it.
	 -  Write the first part to the file, possibly encoded.
	 -  Write the second and subsequent parts to the file, possibly encoded.
        (Open the first temp file and copy it to the final temp file, and so
		on, through an encoding filter.)

     - Delete the attachment temp file(s) as we finish with them.
	 - Close the final temp file.
	 - Open the news: url.
	 - Send the final temp file to NNTP.
	   If there's an error, run the callback with "failure" status.
	 - If mail succeeded, open the mailto: url.
	 - Send the final temp file to SMTP.
	   If there's an error, run the callback with "failure" status.
	 - Otherwise, run the callback with "success" status.
	 - Free everything, delete the final temp file.

  The theory behind the encoding logic:
  =====================================

  If the document is of type text/html, and the user has asked to attach it
  as source or postscript, it will be run through the appropriate converter
  (which will result in a document of type text/plain.)

  An attachment will be encoded if:

   - it is of a non-text type (in which case we will use base64); or
   - The "use QP" option has been selected and high-bit characters exist; or
   - any NULLs exist in the document; or
   - any line is longer than 900 bytes.

   - If we are encoding, and more than 10% of the document consists of
     non-ASCII characters, then we always use base64 instead of QP.

  We eschew quoted-printable in favor of base64 for documents which are likely
  to always be binary (images, sound) because, on the off chance that a GIF
  file (for example) might contain primarily bytes in the ASCII range, using
  the quoted-printable representation might cause corruption due to the
  translation of CR or LF to CRLF.  So, when we don't know that the document
  has "lines", we don't use quoted-printable.
 */


/* It's better to send a message as news before sending it as mail, because
   the NNTP server is more likely to reject the article (for any number of
   reasons) than the SMTP server is. */
#undef MAIL_BEFORE_NEWS

/* Generating a message ID here is a good because it means that if a message
   is sent to both mail and news, it will have the same ID in both places. */
#define GENERATE_MESSAGE_ID


/* For maximal compatibility, it helps to emit both
      Content-Type: <type>; name="<original-file-name>"
   as well as
      Content-Disposition: inline; filename="<original-file-name>"

  The lossage here is, RFC1341 defined the "name" parameter to Content-Type,
  but then RFC1521 deprecated it in anticipation of RFC1806, which defines
  Content-Type and the "filename" parameter.  But, RFC1521 is "Standards Track"
  while RFC1806 is still "Experimental."  So, it's probably best to just
  implement both.
 */
#define EMIT_NAME_IN_CONTENT_TYPE


/* Whether the contents of the BCC header should be preserved in the FCC'ed
   copy of a message.  See comments below, in mime_do_fcc_1().
 */
#define SAVE_BCC_IN_FCC_FILE


/* When attaching an HTML document, one must indicate the original URL of
   that document, if the receiver is to have any chance of being able to
   retreive and display the inline images, or to click on any links in the
   HTML.

   The way we have done this in the past is by inserting a <BASE> tag as the
   first line of all HTML documents we attach.  (This is kind of bad in that
   we're actually modifying the document, and it really isn't our place to
   do that.)

   The sanctioned *new* way of doing this is to insert a Content-Base header
   field on the attachment.  This is (will be) a part of the forthcoming MHTML
   spec.

   If GENERATE_CONTENT_BASE, we generate a Content-Base header.

   We used to have a MANGLE_HTML_ATTACHMENTS_WITH_BASE_TAG symbol that we
   defined, which added a BASE tag to the bodies.  We stopped doing this in
   4.0.  */
#define GENERATE_CONTENT_BASE


static XP_Bool mime_use_quoted_printable_p = TRUE;
static XP_Bool mime_headers_use_quoted_printable_p = FALSE;

#ifdef XP_MAC

XP_BEGIN_PROTOS
extern OSErr my_FSSpecFromPathname(char* src_filename, FSSpec* fspec);
extern char * mime_make_separator(const char *prefix);
HG89984
XP_END_PROTOS

static char* NET_GetLocalFileFromURL(char *url)
{
	char * finalPath;
	XP_ASSERT(strncasecomp(url, "file://", 7) == 0);
	finalPath = (char*)XP_ALLOC(strlen(url));
	if (finalPath == NULL)
		return NULL;
	strcpy(finalPath, url+6+1);
	return finalPath;
}

static char* NET_GetURLFromLocalFile(char *filename)
{
    /*  file:///<path>0 */
	char * finalPath = (char*)XP_ALLOC(strlen(filename) + 8 + 1);
	if (finalPath == NULL)
		return NULL;
	finalPath[0] = 0;
	strcat(finalPath, "file://");
	strcat(finalPath, filename);
	return finalPath;
}

#endif /* XP_MAC */

void
MIME_ConformToStandard (XP_Bool conform_p)
{
  /* 
   * If we are conforming to mime standard no matter what we set
   * for the headers preference when generating mime headers we should 
   * also conform to the standard. Otherwise, depends the preference
   * we set. For now, the headers preference is not accessible from UI.
   */
  if (conform_p)
	mime_headers_use_quoted_printable_p = TRUE;
  else
	PREF_GetBoolPref("mail.strictly_mime_headers", 
					 &mime_headers_use_quoted_printable_p);
  mime_use_quoted_printable_p = conform_p;
}

MSG_SendMimeDeliveryState::MSG_SendMimeDeliveryState()
{
  m_pane = NULL;		/* Pane to use when loading the URLs */
  m_fe_data = NULL;		/* passed in and passed to callback */
  m_fields = NULL;			/* Where to send the message once it's done */

  m_dont_deliver_p = FALSE;
  m_deliver_mode = MSG_DeliverNow;

  m_attachments_only_p = FALSE;
  m_pre_snarfed_attachments_p = FALSE;
  m_digest_p = FALSE;
  m_be_synchronous_p = FALSE;
  HG54689
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
  m_msg_file = 0;
  m_plaintext = NULL;
  m_html_filename = NULL;

//#ifdef MSG_SEND_MULTIPART_RELATED
  m_related_part = NULL;
//#endif
  m_imapFolderInfo = NULL;
  m_imapOutgoingParser = NULL;
  m_imapLocalMailDB = NULL;
}

MSG_SendMimeDeliveryState::~MSG_SendMimeDeliveryState()
{
//#ifdef MSG_SEND_MULTIPART_RELATED
#if 0
  if (m_related_part)
  {
	  // m_related_part (if it exists) gets deleted when 
	  // m_related_saver gets torched.
	  delete m_related_part;
	  m_related_part = NULL;
  }
#endif
//#endif
  FREEIF(m_msg_file_name);
}

static char *mime_mailto_stream_read_buffer = 0;
static char *mime_mailto_stream_write_buffer = 0;

static void mime_attachment_url_exit (URL_Struct *url, int status,
									  MWContext *context);
static void mime_text_attachment_url_exit (PrintSetup *p);
static int mime_sanity_check_fields (const char *from,
									 const char *reply_to,
									 const char *to,
									 const char *cc,
									 const char *bcc,
									 const char *fcc,
									 const char *newsgroups,
									 const char *followup_to,
									 const char *subject,
									 const char *references,
									 const char *organization,
									 const char *other_random_headers);
static char *mime_generate_headers (MSG_CompositionFields *fields,
									int csid, 
									MSG_Deliver_Mode deliver_mode);
static char *mime_generate_attachment_headers (const char *type,
											   const char *encoding,
											   const char *description,
											   const char *x_mac_type,
											   const char *x_mac_creator,
											   const char *real_name,
											   const char *base_url,
											   XP_Bool digest_p,
											   MSG_DeliverMimeAttachment *ma,
											   int16 mail_csid);
static char *RFC2231ParmFolding(const char *parmName, const char *charset,
								const char *language, const char *parmValue);
#if 0
static XP_Bool mime_type_conversion_possible (const char *from_type,
											  const char *to_type);
#endif

#ifdef XP_UNIX
extern "C" void XFE_InitializePrintSetup (PrintSetup *p);
#endif /* XP_UNIX */

extern "C" char * NET_ExplainErrorDetails (int code, ...);

MSG_DeliverMimeAttachment::MSG_DeliverMimeAttachment()
{
  m_url_string = NULL;
  m_url = NULL;
  m_done = FALSE;
  m_type = NULL;
  m_override_type = NULL;
  m_override_encoding = NULL;
  m_desired_type = NULL;
  m_description = NULL;
  m_x_mac_type = NULL;
  m_x_mac_creator = NULL;
  m_encoding = NULL;
  m_real_name = NULL;
  m_mime_delivery_state = NULL;
  m_encoding = NULL;
  m_already_encoded_p = FALSE;
  m_file_name = NULL;
  m_file = 0;
#ifdef XP_MAC
  m_ap_filename = NULL;
#endif
  HG54897
  m_size = 0;
  m_unprintable_count = 0;
  m_highbit_count = 0;
  m_ctl_count = 0;
  m_null_count = 0;
  m_current_column = 0;
  m_max_column = 0;
  m_lines = 0;

  m_encoder_data = NULL;

  XP_MEMSET(&m_print_setup, 0, sizeof(m_print_setup));
  m_graph_progress_started = FALSE;
}

MSG_DeliverMimeAttachment::~MSG_DeliverMimeAttachment()
{
}


extern "C" char *
mime_make_separator(const char *prefix)
{
  unsigned char rand_buf[13]; 
  RNG_GenerateGlobalRandomBytes((void *) rand_buf, 12);
  return PR_smprintf("------------%s"
					 "%02X%02X%02X%02X"
					 "%02X%02X%02X%02X"
					 "%02X%02X%02X%02X",
					 prefix,
					 rand_buf[0], rand_buf[1], rand_buf[2], rand_buf[3],
					 rand_buf[4], rand_buf[5], rand_buf[6], rand_buf[7],
					 rand_buf[8], rand_buf[9], rand_buf[10], rand_buf[11]);
}

XP_Bool
MSG_DeliverMimeAttachment::UseUUEncode_p(void)
{
	XP_Bool returnVal = (m_mime_delivery_state) && 
	  					(m_mime_delivery_state->m_pane) &&
	  					((MSG_CompositionPane*)(m_mime_delivery_state->m_pane))->
				  		GetCompBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK);
	
	return returnVal;
}

static void msg_escape_file_name (URL_Struct *m_url) 
{
    XP_ASSERT (m_url->address && !XP_STRNCASECMP(m_url->address, "file:", 5));
	if (!m_url->address ||
		XP_STRNCASECMP(m_url->address, "file:", 5)) return;

	char * new_address = NET_Escape(XP_STRCHR(m_url->address, ':')+1, 
									URL_PATH);
	XP_ASSERT(new_address);
	if (!new_address) return;

	XP_FREEIF(m_url->address);
	m_url->address = PR_smprintf("file:%s", new_address);
	XP_FREEIF(new_address);
}

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
	char *whereDot = XP_STRRCHR(filename, '.');
	if (whereDot)
	{
		char *extension = whereDot + 1;
		if (extension)
		{
			char *mcOutgoingMimeType = NULL;
			char *prefString = PR_smprintf("mime.table.extension.%s.outgoing_default_type",extension);

			if (prefString)
			{
				PREF_CopyCharPref(prefString, &mcOutgoingMimeType);
				XP_FREE(prefString);
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


static char *
RFC2231ParmFolding(const char *parmName, const char *charset, 
				   const char *language, const char *parmValue)
{
#define MAX_FOLDING_LEN 75			// this is to gurantee the folded line will
								// never be greater than 78 = 75 + CRLFLWSP
	char *foldedParm = NULL;
	char *dupParm = NULL;
	int32 parmNameLen = 0;
	int32 parmValueLen = 0;
	int32 charsetLen = 0;
	int32 languageLen = 0;
	XP_Bool needEscape = FALSE;

	XP_ASSERT(parmName && *parmName && parmValue && *parmValue);
	if (!parmName || !*parmName || !parmValue || !*parmValue)
		return NULL;
	if ((charset && *charset) || (language && *language))
		needEscape = TRUE;

	if (needEscape)
		dupParm = NET_Escape(parmValue, URL_PATH);
	else 
		dupParm = XP_STRDUP(parmValue);

	if (!dupParm)
		return NULL;

	parmValueLen = XP_STRLEN(dupParm);
	parmNameLen = XP_STRLEN(parmName);
	if (needEscape)
		parmNameLen += 5;		// *=__'__'___ or *[0]*=__'__'__ or *[1]*=___
	else
		parmNameLen += 5;		// *[0]="___";
	charsetLen = charset ? XP_STRLEN(charset) : 0;
	languageLen = language ? XP_STRLEN(language) : 0;

	if ((parmValueLen + parmNameLen + charsetLen + languageLen) <
		MAX_FOLDING_LEN)
	{
		StrAllocCopy(foldedParm, parmName);
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
			if (counter == 0)
			{
				StrAllocCopy (foldedParm, parmName);
			}
			else
			{
				if (needEscape)
					StrAllocCat(foldedParm, "\r\n ");
				else
					StrAllocCat(foldedParm, ";\r\n ");
				StrAllocCat(foldedParm, parmName);
			}
			XP_SPRINTF(digits, "*%d", counter);
			StrAllocCat(foldedParm, digits);
			curLineLen += XP_STRLEN(digits);
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
			if (parmValueLen <= MAX_FOLDING_LEN - curLineLen)
				end = start + parmValueLen;
			else
				end = start + (MAX_FOLDING_LEN - curLineLen);

			tmp = 0;
			if (*end && needEscape)
			{
				// check to see if we are in the middle of escaped char
				if (*end == '%')
				{
					tmp = '%'; *end = NULL;
				}
				else if (end-1 > start && *(end-1) == '%')
				{
					end -= 1; tmp = '%'; *end = NULL;
				}
				else if (end-2 > start && *(end-2) == '%')
				{
					end -= 2; tmp = '%'; *end = NULL;
				}
				else
				{
					tmp = *end; *end = NULL;
				}
			}
			else
			{
				tmp = *end; *end = NULL;
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
	XP_FREEIF(dupParm);
	return foldedParm;
}

int32
MSG_DeliverMimeAttachment::SnarfAttachment ()
{
  int32 status = 0;
  XP_ASSERT (! m_done);

  m_file_name = WH_TempName (xpFileToPost, "nsmail");
  if (! m_file_name)
	return (MK_OUT_OF_MEMORY);

  m_file = XP_FileOpen (m_file_name, xpFileToPost, XP_FILE_WRITE_BIN);
  if (! m_file)
	return MK_UNABLE_TO_OPEN_TMP_FILE; /* #### how do we pass file name? */

  m_url->fe_data = this;

  /* #### m_type is still unknown at this point.
	 We need to find a way to make the textfe not blow
	 up on documents that are not text/html!
   */
   
#ifdef XP_MAC
  if (NET_IsLocalFileURL(m_url->address) &&	// do we need to add IMAP: to this list? NET_IsLocalFileURL returns FALSE always for IMAP - DMB
	  (strncasecomp(m_url->address, "mailbox:", 8) != 0))
	{
	  /* convert the apple file to AppleDouble first, and then patch the
		 address in the url.
	   */
	  char* src_filename = NET_GetLocalFileFromURL (m_url->address);

		// ### mwelch Only use appledouble if we aren't uuencoding.
	  if(isMacFile(src_filename) && (! UseUUEncode_p()))
		{

		  char	*separator, tmp[128];
		  NET_StreamClass *ad_encode_stream;

		  separator = mime_make_separator("ad");
		  if (!separator)
			return MK_OUT_OF_MEMORY;
						
		  m_ap_filename  = WH_TempName (xpFileToPost, "nsmail");

		  ad_encode_stream = (NET_StreamClass *)		/* need a prototype */
			fe_MakeAppleDoubleEncodeStream (FO_CACHE_AND_MAIL_TO,
											NULL,
											m_url,
											m_mime_delivery_state->GetContext(),
											src_filename,
											m_ap_filename,
											separator);

		  if (ad_encode_stream == NULL)
			{
			  FREEIF(separator);
			  return MK_OUT_OF_MEMORY;
			}

		  do {
			status = (*ad_encode_stream->put_block)
			  ((NET_StreamClass *)ad_encode_stream->data_object, NULL, 1024);
		  } while (status == noErr);

		  if (status >= 0)
			ad_encode_stream->complete ((NET_StreamClass *)ad_encode_stream->data_object);
		  else
			ad_encode_stream->abort ((NET_StreamClass *)ad_encode_stream->data_object, status);

		  XP_FREE(ad_encode_stream);

		  if (status < 0)
			{
			  FREEIF(separator);
			  return status;
			}

		  XP_FREE(m_url->address);
		  {
		    char * temp = WH_FileName(m_ap_filename, xpFileToPost );
		    m_url->address = XP_PlatformFileToURL(temp); // jrm 97/02/08
			if (temp)
				XP_FREE(temp);
		  }
		  /* and also patch the types.
		   */
		  if (m_type)
			XP_FREE (m_type);

		  XP_SPRINTF(tmp, MULTIPART_APPLEDOUBLE ";\r\n boundary=\"%s\"",
					 separator);

		  FREEIF(separator);

		m_type = XP_STRDUP(tmp);
		}
	  else
		{
			if (isMacFile(src_filename))
			{
				// The only time we want to send just the data fork of a two-fork
				// Mac file is if uuencoding has been requested.
				XP_ASSERT(UseUUEncode_p());
				if (!((MSG_CompositionPane *) m_mime_delivery_state->m_pane)->m_confirmed_uuencode_p)
				{
					XP_Bool confirmed = FE_Confirm(m_mime_delivery_state->m_pane->GetContext(), 
													XP_GetString(MK_MSG_MAC_PROMPT_UUENCODE)); 

					// only want to do this once
					((MSG_CompositionPane *) m_mime_delivery_state->m_pane)->m_confirmed_uuencode_p = TRUE;
					
					if (! confirmed) // cancelled
						return MK_INTERRUPTED;
				}
			}
		  /* make sure the file type and create are set.	*/
		  char filetype[32];
		  FSSpec fsSpec;
		  FInfo info;
		  Bool 	useDefault;
		  char	*macType, *macEncoding;

		  my_FSSpecFromPathname(src_filename, &fsSpec);
		  if (FSpGetFInfo (&fsSpec, &info) == noErr)
			{
			  XP_SPRINTF(filetype, "%X", info.fdType);
			  m_x_mac_type    = XP_STRDUP(filetype);

			  XP_SPRINTF(filetype, "%X", info.fdCreator);
			  m_x_mac_creator = XP_STRDUP(filetype);
			  if (m_type == NULL ||
				  !strcasecomp (m_type, TEXT_PLAIN))
				{
# define TEXT_TYPE	0x54455854  /* the characters 'T' 'E' 'X' 'T' */
# define text_TYPE	0x74657874  /* the characters 't' 'e' 'x' 't' */

				  if (info.fdType != TEXT_TYPE && info.fdType != text_TYPE)
					{
					  FE_FileType(m_url->address, &useDefault,
								  &macType, &macEncoding);

					  FREEIF(m_type);
					  m_type = macType;
					}
				}
			}

		  /* don't bother to set the types if we failed in getting the file
			 info. */
		}
	  FREEIF(src_filename);
	  src_filename = 0;
	}
#else

  /* if we are attaching a local file make sure the file name are escaped
   * properly
   */
  if (NET_IsLocalFileURL(m_url->address) &&
	  XP_STRNCASECMP (m_url->address, "file:", 5) == 0) 
  {
	  msg_escape_file_name(m_url);
  }

#endif /* XP_MAC */

  if (m_desired_type &&
	  !strcasecomp (m_desired_type, TEXT_PLAIN) /* #### &&
	  mime_type_conversion_possible (m_type, m_desired_type) */ )
	{
	  /* Conversion to plain text desired.
	   */
	  m_print_setup.url = m_url;
	  m_print_setup.carg = this;
	  m_print_setup.completion = mime_text_attachment_url_exit;
	  m_print_setup.filename = NULL;
	  m_print_setup.out = m_file;
	  m_print_setup.eol = CRLF;

	  int32 width = 72;
	  PREF_GetIntPref("mailnews.wraplength", &width);
	  if (width == 0) width = 72;
	  else if (width < 10) width = 10;
	  else if (width > 30000) width = 30000;

	  if (m_mime_delivery_state->m_pane->GetPaneType() == MSG_COMPOSITIONPANE)
	  {
		  int lineWidth = ((MSG_CompositionPane *) m_mime_delivery_state->m_pane)
			  ->GetLineWidth();
		  if (lineWidth > width)
			  width = lineWidth;
	  }
	  m_print_setup.width = width;

	  m_url->savedData.FormList = 0;
#ifdef _USRDLL
	  if (! NDLLXL_TranslateText (m_mime_delivery_state->GetContext(), m_url,
							&m_print_setup))
		return MK_ATTACHMENT_LOAD_FAILED;
#else
	  if (! XL_TranslateText (m_mime_delivery_state->GetContext(), m_url,
						&m_print_setup))
		return MK_ATTACHMENT_LOAD_FAILED;
#endif
	  if (m_type) XP_FREE (m_type);
	  m_type = m_desired_type;
	  m_desired_type = 0;
	  if (m_encoding) XP_FREE (m_encoding);
	  m_encoding = 0;
	}
#ifdef XP_UNIX
  else if (m_desired_type &&
		   !strcasecomp (m_desired_type, APPLICATION_POSTSCRIPT) /* #### &&
		   mime_type_conversion_possible (m_type, m_desired_type) */ )
	{
      SHIST_SavedData saved_data;

      /* Make sure layout saves the current state of form elements. */
      LO_SaveFormData(m_mime_delivery_state->GetContext());

      /* Hold on to the saved data. */
      XP_MEMCPY(&saved_data, &m_url->savedData, sizeof(SHIST_SavedData));

	  /* Conversion to postscript desired.
	   */
	  XFE_InitializePrintSetup (&m_print_setup);
	  m_print_setup.url = m_url;
	  m_print_setup.carg = this;
	  m_print_setup.completion = mime_text_attachment_url_exit;
	  m_print_setup.filename = NULL;
	  m_print_setup.out = m_file;
	  m_print_setup.eol = CRLF;
      XP_MEMSET (&m_url->savedData, 0, sizeof (SHIST_SavedData));
	  XL_TranslatePostscript (m_mime_delivery_state->GetContext(),
							  m_url, &saved_data,
							  &m_print_setup);
	  if (m_type) XP_FREE (m_type);
	  m_type = m_desired_type;
	  m_desired_type = 0;
	  if (m_encoding) XP_FREE (m_encoding);
	  m_encoding = 0;
	}
#endif /* XP_UNIX */
  else
	{
	  int get_url_status;

	  /* In this case, ignore the status, as that will be handled by
		 the exit routine. */

	  /* jwz && tj -> we're assuming that it is safe to return the result
		 of this call as our status result.

		 A negative result means that the exit routine was run, either
		 because the operation completed quickly or failed.
	   */
	  m_url->allow_content_change = FALSE;	// don't use modified content
	  get_url_status = NET_GetURL (m_url, FO_CACHE_AND_MAIL_TO,
								   m_mime_delivery_state->GetContext(),
								   mime_attachment_url_exit);

	  if (get_url_status < 0)
		return MK_ATTACHMENT_LOAD_FAILED;
	  else
		return 0;
	}
  return status;
}


static void
mime_attachment_url_exit (URL_Struct *url, int status, MWContext *context)
{
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) url->fe_data;
  XP_ASSERT(ma != NULL);
  if (ma != NULL)
	ma->UrlExit(url, status, context);
}


void MSG_DeliverMimeAttachment::UrlExit(URL_Struct *url, int status,
										MWContext *context)
{
  char *error_msg = url->error_msg;
  url->error_msg = 0;
  url->fe_data = 0;

  XP_ASSERT(m_mime_delivery_state != NULL);
  XP_ASSERT(m_mime_delivery_state->GetContext() != NULL);
  XP_ASSERT(m_url != NULL);

  if (m_graph_progress_started)
	{
	  m_graph_progress_started = FALSE;
	  FE_GraphProgressDestroy (m_mime_delivery_state->GetContext(), m_url,
							   m_url->content_length, m_size);
	}

  if (status < 0)
	/* If any of the attachment URLs fail, kill them all. */
	NET_InterruptWindow (context);

  /* Close the file, but don't delete it (or the file name.) */
  XP_FileClose (m_file);
  m_file = 0;
  NET_FreeURLStruct (m_url);
  /* I'm pretty sure m_url == url */
  m_url = 0;
  url = 0;



  if (status < 0) {
	  if (m_mime_delivery_state->m_status >= 0)
		m_mime_delivery_state->m_status = status;
	  XP_FileRemove(m_file_name, xpFileToPost);
	  XP_FREEIF(m_file_name);
  }

  m_done = TRUE;

  XP_ASSERT (m_mime_delivery_state->m_attachment_pending_count > 0);
  m_mime_delivery_state->m_attachment_pending_count--;

  if (status >= 0 && m_mime_delivery_state->m_be_synchronous_p)
	{
	  /* Find the next attachment which has not yet been loaded,
		 if any, and start it going.
	   */
	  int32 i;
	  MSG_DeliverMimeAttachment *next = 0;
	  for (i = 0; i < m_mime_delivery_state->m_attachment_count; i++)
		if (!m_mime_delivery_state->m_attachments[i].m_done)
		  {
			next = &m_mime_delivery_state->m_attachments[i];
			break;
		  }
	  if (next)
		{
		  int status = next->SnarfAttachment ();
		  if (status < 0)
			{
			  m_mime_delivery_state->Fail(status, 0);
			  return;
			}
		}
	}

  if (m_mime_delivery_state->m_attachment_pending_count == 0)
	{
	  /* If this is the last attachment, then either complete the
		 delivery (if successful) or report the error by calling
		 the exit routine and terminating the delivery.
	   */
	  if (status < 0)
		{
		  m_mime_delivery_state->Fail(status, error_msg);
		  error_msg = 0;
		}
	  else
		{
		  m_mime_delivery_state->GatherMimeAttachments ();
		}
	}
  else
	{
	  /* If this is not the last attachment, but it got an error,
		 then report that error and continue (we won't actually
		 abort the delivery until all the other pending URLs have
		 caught up with the NET_InterruptWindow() we did up above.)
	   */
	  if (status < 0 && error_msg)
		FE_Alert (context, error_msg);
	}
  FREEIF (error_msg);
}

void
MSG_DeliverMimeAttachment::AnalyzeDataChunk(const char *chunk, int32 length)
{
  unsigned char *s = (unsigned char *) chunk;
  unsigned char *end = s + length;
  for (; s < end; s++)
	{
	  if (*s > 126)
		{
		  m_highbit_count++;
		  m_unprintable_count++;
		}
	  else if (*s < ' ' && *s != '\t' && *s != CR && *s != LF)
		{
		  m_unprintable_count++;
		  m_ctl_count++;
		  if (*s == 0)
			m_null_count++;
		}

	  if (*s == CR || *s == LF)
		{
		  if (s+1 < end && s[0] == CR && s[1] == LF)
			s++;
		  if (m_max_column < m_current_column)
			m_max_column = m_current_column;
		  m_current_column = 0;
		  m_lines++;
		}
	  else
		{
		  m_current_column++;
		}
	}
}

void
MSG_DeliverMimeAttachment::AnalyzeSnarfedFile(void)
{
	char chunk[256];
	XP_File fileHdl = NULL;
	int32 numRead = 0;
	
	if (m_file_name && *m_file_name)
	{
		fileHdl = XP_FileOpen(m_file_name, xpFileToPost, XP_FILE_READ_BIN);
		if (fileHdl)
		{
			do
			{
				numRead = XP_FileRead(chunk, 256, fileHdl);
				if (numRead > 0)
					AnalyzeDataChunk(chunk, numRead);
			}
			while (numRead > 0);
			XP_FileClose(fileHdl);
		}
	}
}

static void
mime_text_attachment_url_exit (PrintSetup *p)
{
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) p->carg;
  XP_ASSERT (p->url == ma->m_url);
  ma->m_url->fe_data = ma;  /* grr */
  mime_attachment_url_exit (p->url, p->status,
							ma->m_mime_delivery_state->GetContext());
}


PRIVATE unsigned int
mime_attachment_stream_write_ready (NET_StreamClass *stream)
{	
  return MAX_WRITE_READY;
}

PRIVATE int
mime_attachment_stream_write (NET_StreamClass *stream, const char *block, int32 length)
{
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) stream->data_object;
  /*
  const unsigned char *s;
  const unsigned char *end;
  */  

  if (ma->m_mime_delivery_state->m_status < 0)
	return ma->m_mime_delivery_state->m_status;

  ma->m_size += length;

  if (!ma->m_graph_progress_started)
	{
	  ma->m_graph_progress_started = TRUE;
	  FE_GraphProgressInit (ma->m_mime_delivery_state->GetContext(), ma->m_url,
							ma->m_url->content_length);
	}

  FE_GraphProgress (ma->m_mime_delivery_state->GetContext(), ma->m_url,
					ma->m_size, length, ma->m_url->content_length);


  /* Copy out the content type and encoding if we haven't already.
   */
  if (!ma->m_type && ma->m_url->content_type)
	{
	  ma->m_type = XP_STRDUP (ma->m_url->content_type);

	  /* If the URL has an encoding, and it's not one of the "null" encodings,
		 then keep it. */
	  if (ma->m_url->content_encoding &&
		  strcasecomp (ma->m_url->content_encoding, ENCODING_7BIT) &&
		  strcasecomp (ma->m_url->content_encoding, ENCODING_8BIT) &&
		  strcasecomp (ma->m_url->content_encoding, ENCODING_BINARY))
		{
		  if (ma->m_encoding) XP_FREE (ma->m_encoding);
		  ma->m_encoding = XP_STRDUP (ma->m_url->content_encoding);
		  ma->m_already_encoded_p = TRUE;
		}

	  /* Make sure there's a string in the type field.
		 Note that UNKNOWN_CONTENT_TYPE and APPLICATION_OCTET_STREAM are
		 different; "octet-stream" means that this document was *specified*
		 as an anonymous binary type; "unknown" means that we will guess
		 whether it is text or binary based on its contents.
	   */
	  if (!ma->m_type || !*ma->m_type)
		StrAllocCopy (ma->m_type, UNKNOWN_CONTENT_TYPE);


#if defined(XP_WIN) || defined(XP_OS2)
	  /*  WinFE tends to spew out bogus internal "zz-" types for things
		 it doesn't know, so map those to the "real" unknown type.
	   */
	  if (ma->m_type && !strncasecomp (ma->m_type, "zz-", 3))
		StrAllocCopy (ma->m_type, UNKNOWN_CONTENT_TYPE);
#endif /* XP_WIN */

	  /* There are some of "magnus" types in the default
		 mime.types file that some platforms ship in /usr/local/lib/netscape/.  These
		 types are meaningless to the end user, and the server never returns
		 them, but they're getting attached to local .cgi files anyway.
		 Remove them.
	   */
	  if (ma->m_type && !strncasecomp (ma->m_type, "magnus-internal/", 16))
		StrAllocCopy (ma->m_type, UNKNOWN_CONTENT_TYPE);


	  /* kludge.
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
		  (!strcasecomp (ma->m_encoding, ENCODING_COMPRESS) ||
		   !strcasecomp (ma->m_encoding, ENCODING_COMPRESS2)))
		{
		  StrAllocCopy (ma->m_type, APPLICATION_COMPRESS);
		  StrAllocCopy (ma->m_encoding, ENCODING_BINARY);
		  ma->m_already_encoded_p = FALSE;
		}
	  else if (ma->m_encoding &&
			   (!strcasecomp (ma->m_encoding, ENCODING_GZIP) ||
				!strcasecomp (ma->m_encoding, ENCODING_GZIP2)))
		{
		  StrAllocCopy (ma->m_type, APPLICATION_GZIP);
		  StrAllocCopy (ma->m_encoding, ENCODING_BINARY);
		  ma->m_already_encoded_p = FALSE;
		}

	  /* If the caller has passed in an overriding type for this URL,
		 then ignore what the netlib guessed it to be.  This is so that
		 we can hand it a file:/some/tmp/file and tell it that it's of
		 type message/rfc822 without having to depend on that tmp file
		 having some particular extension.
	   */
	  if (ma->m_override_type)
		{
		  StrAllocCopy (ma->m_type, ma->m_override_type);
		  if (ma->m_override_encoding)
			StrAllocCopy (ma->m_encoding, ma->m_override_encoding);
		}

	  char *mcType = msg_GetMissionControlledOutgoingMIMEType(ma->m_real_name, ma->m_x_mac_type, ma->m_x_mac_creator);	// returns an allocated string
	  if (mcType)
	  {
		  FREEIF(ma->m_type);
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
	  int32 l;
	  l = XP_FileWrite (block, length, ma->m_file);
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
mime_attachment_stream_complete (NET_StreamClass *stream)
{	
  /* Nothing to do here - the URL exit method does our cleanup. */
}

PRIVATE void
mime_attachment_stream_abort (NET_StreamClass *stream, int status)
{
  MSG_DeliverMimeAttachment *ma = (MSG_DeliverMimeAttachment *) stream->data_object;  

  if (ma->m_mime_delivery_state->m_status >= 0)
	ma->m_mime_delivery_state->m_status = status;

  /* Nothing else to do here - the URL exit method does our cleanup. */
}


#ifdef XP_OS2
//DSR040297 - This looks pretty bad, but some compilers are very type
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

  TRACEMSG(("Setting up attachment stream. Have URL: %s\n", url->address));

  stream = XP_NEW (NET_StreamClass);
  if (stream == NULL)
	return (NULL);

  XP_MEMSET (stream, 0, sizeof (NET_StreamClass));

  stream->name           = "attachment stream";
  stream->complete       = mime_attachment_stream_complete;
  stream->abort          = mime_attachment_stream_abort;
  stream->put_block      = mime_attachment_stream_write;
  stream->is_write_ready = mime_attachment_stream_write_ready;
  stream->data_object    = url->fe_data;
  stream->window_id      = context;

  TRACEMSG(("Returning stream from mime_make_attachment_stream"));

  return stream;
}

HG55451

void
MSG_RegisterConverters (void)
{
  NET_RegisterContentTypeConverter ("*", FO_MAIL_TO,
									NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_TO,
									NULL, mime_make_attachment_stream);

  /* FO_MAIL_MESSAGE_TO is treated the same as FO_MAIL_TO -- this format_out
	 just means that libmime has already gotten its hands on this document
	 (which happens to be of type message/rfc822 or message/news) and has
	 altered it in some way (for example, has decrypted it.) */
  NET_RegisterContentTypeConverter ("*", FO_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);

  /* Attachment of mail and news messages happens slightly differently:
	 Rather than FO_MAIL_TO going in to mime_make_attachment_stream, it
	 goes into MIME_MessageConverter, which will then open a later stream
	 with FO_MAIL_MESSAGE_TO -- which is how it eventually gets into
	 mime_make_attachment_stream, after having gone through libmime.
   */
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_MAIL_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_MAIL_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_CACHE_AND_MAIL_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_CACHE_AND_MAIL_TO,
									NULL, MIME_MessageConverter);

  /* for saving news messages */
  NET_RegisterContentTypeConverter ("*", FO_APPEND_TO_FOLDER,
                                    NULL, msg_MakeAppendToFolderStream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_APPEND_TO_FOLDER,
                                    NULL, msg_MakeAppendToFolderStream);

  /* Decoders from mimehtml.c for message/rfc822 */
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_PRESENT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_PRINT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_EMBED,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_QUOTE_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_QUOTE_HTML_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_SAVE_AS,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_SAVE_AS_TEXT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_INTERNAL_IMAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_FONT,
									NULL, MIME_MessageConverter);
  
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_CMDLINE_ATTACHMENTS,
									NULL, MIME_ToDraftConverter);

  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_OPEN_DRAFT,
									NULL, MIME_ToDraftConverter);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_SAVE_AS_POSTSCRIPT,
									NULL, MIME_MessageConverter);
#endif /* XP_UNIX */

  /* Decoders from mimehtml.c for message/news (same as message/rfc822) */
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_PRESENT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_PRINT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_EMBED,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_QUOTE_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_QUOTE_HTML_MESSAGE,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_SAVE_AS,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_SAVE_AS_TEXT,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_INTERNAL_IMAGE,
									NULL, MIME_MessageConverter);

  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_OPEN_DRAFT,
									NULL, MIME_ToDraftConverter);
#ifdef XP_UNIX
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_SAVE_AS_POSTSCRIPT,
									NULL, MIME_MessageConverter);
#endif /* XP_UNIX */

  /* Decoders from mimehtml.c for text/richtext and text/enriched */
  NET_RegisterContentTypeConverter (TEXT_RICHTEXT, FO_PRESENT,
									NULL, MIME_RichtextConverter);
  NET_RegisterContentTypeConverter (TEXT_RICHTEXT, FO_PRINT,
									NULL, MIME_RichtextConverter);
  NET_RegisterContentTypeConverter (TEXT_ENRICHED, FO_PRESENT,
									NULL, MIME_EnrichedTextConverter);
  NET_RegisterContentTypeConverter (TEXT_ENRICHED, FO_PRINT,
									NULL, MIME_EnrichedTextConverter);

  /* Decoders from mimevcrd.c for text/x-vcard and text/enriched */
  NET_RegisterContentTypeConverter (TEXT_VCARD, FO_PRESENT,
									NULL, MIME_VCardConverter);
  NET_RegisterContentTypeConverter (TEXT_VCARD, FO_PRINT,
									NULL, MIME_VCardConverter);

  /* Decoders from mimejul.c for text/calendar */
#ifdef MOZ_CALENDAR
  NET_RegisterContentTypeConverter (TEXT_CALENDAR, FO_PRESENT,
									NULL, MIME_JulianConverter);
  NET_RegisterContentTypeConverter (TEXT_CALENDAR, FO_PRINT,
									NULL, MIME_JulianConverter);
#endif


}


static XP_Bool
mime_7bit_data_p (const char *string, uint32 size)
{
  const unsigned char *s = (const unsigned char *) string;
  const unsigned char *end = s + size;
  if (s)
	for (; s < end; s++)
	  if (*s > 0x7F)
		return FALSE;
  return TRUE;
}


static int
mime_sanity_check_fields (const char *from,
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
  if (from) while (XP_IS_SPACE (*from)) from++;
  if (reply_to) while (XP_IS_SPACE (*reply_to)) reply_to++;
  if (to) while (XP_IS_SPACE (*to)) to++;
  if (cc) while (XP_IS_SPACE (*cc)) cc++;
  if (bcc) while (XP_IS_SPACE (*bcc)) bcc++;
  if (fcc) while (XP_IS_SPACE (*fcc)) fcc++;
  if (newsgroups) while (XP_IS_SPACE (*newsgroups)) newsgroups++;
  if (followup_to) while (XP_IS_SPACE (*followup_to)) followup_to++;

  /* #### check other_random_headers for newline conventions */

  if (!from || !*from)
	return MK_MIME_NO_SENDER;
  else if ((!to || !*to) &&
		   (!cc || !*cc) &&
		   (!bcc || !*bcc) &&
		   (!newsgroups || !*newsgroups))
	return MK_MIME_NO_RECIPIENTS;
  else
	return 0;
}


/* Strips whitespace, and expands newlines into newline-tab for use in
   mail headers.  Returns a new string or 0 (if it would have been empty.)
   If addr_p is true, the addresses will be parsed and reemitted as
   rfc822 mailboxes.
 */
static char *
mime_fix_header_1 (const char *string,
				   XP_Bool addr_p, XP_Bool news_p)
{
  char *new_string;
  const char *in;
  char *out;
  int32 i, old_size, new_size;
  if (!string || !*string)
	return 0;

  if (addr_p)
	{
	  char *n = MSG_ReformatRFC822Addresses (string);
	  if (n) return n;
	}

  old_size = XP_STRLEN (string);
  new_size = old_size;
  for (i = 0; i < old_size; i++)
	if (string[i] == CR || string[i] == LF)
	  new_size += 2;

  new_string = (char *) XP_ALLOC (new_size + 1);
  if (! new_string)
	return 0;

  in  = string;
  out = new_string;

  /* strip leading whitespace. */
  while (XP_IS_SPACE (*in))
	in++;

  /* replace CR, LF, or CRLF with CRLF-TAB. */
  while (*in)
	{
	  if (*in == CR || *in == LF)
		{
		  if (*in == CR && in[1] == LF)
			in++;
		  in++;
		  *out++ = CR;
		  *out++ = LF;
		  *out++ = '\t';
		}
	  else if (news_p && *in == ',')
		{
		  *out++ = *in++;
		  /* skip over all whitespace after a comma. */
		  while (XP_IS_SPACE (*in))
			in++;
		}
	  else
		{
		  *out++ = *in++;
		}
	}
  *out = 0;

  /* strip trailing whitespace. */
  while (out > in && XP_IS_SPACE (out[-1]))
	*out-- = 0;

  /* If we ended up throwing it all away, use 0 instead of "". */
  if (!*new_string)
	{
	  XP_FREE (new_string);
	  new_string = 0;
	}

  return new_string;
}


static char *
mime_fix_header (const char *string)
{
  return mime_fix_header_1 (string, FALSE, FALSE);
}

static char *
mime_fix_addr_header (const char *string)
{
  return mime_fix_header_1 (string, TRUE, FALSE);
}

static char *
mime_fix_news_header (const char *string)
{
  return mime_fix_header_1 (string, FALSE, TRUE);
}


#if 0
static XP_Bool
mime_type_conversion_possible (const char *from_type, const char *to_type)
{
  if (! to_type)
	return TRUE;

  if (! from_type)
	return FALSE;

  if (!strcasecomp (from_type, to_type))
	/* Don't run text/plain through the text->html converter. */
	return FALSE;

  if ((!strcasecomp (from_type, INTERNAL_PARSER) ||
	   !strcasecomp (from_type, TEXT_HTML) ||
	   !strcasecomp (from_type, TEXT_MDL)) &&
	  !strcasecomp (to_type, TEXT_PLAIN))
	/* Don't run UNKNOWN_CONTENT_TYPE through the text->html converter
	   (treat it as text/plain already.) */
	return TRUE;

#ifdef XP_UNIX
  if ((!strcasecomp (from_type, INTERNAL_PARSER) ||
	   !strcasecomp (from_type, TEXT_PLAIN) ||
	   !strcasecomp (from_type, TEXT_HTML) ||
	   !strcasecomp (from_type, TEXT_MDL) ||
	   !strcasecomp (from_type, IMAGE_GIF) ||
	   !strcasecomp (from_type, IMAGE_JPG) ||
	   !strcasecomp (from_type, IMAGE_PJPG) ||
	   !strcasecomp (from_type, IMAGE_XBM) ||
	   !strcasecomp (from_type, IMAGE_XBM2) ||
	   !strcasecomp (from_type, IMAGE_XBM3) ||
	   /* always treat unknown content types as text/plain */
	   !strcasecomp (from_type, UNKNOWN_CONTENT_TYPE)
	   ) &&
	  !strcasecomp (to_type, APPLICATION_POSTSCRIPT))
	return TRUE;
#endif /* XP_UNIX */

  return FALSE;
}
#endif


static XP_Bool
mime_type_requires_b64_p (const char *type)
{
  if (!type || !strcasecomp (type, UNKNOWN_CONTENT_TYPE))
	/* Unknown types don't necessarily require encoding.  (Note that
	   "unknown" and "application/octet-stream" aren't the same.) */
	return FALSE;

  else if (!strncasecomp (type, "image/", 6) ||
		   !strncasecomp (type, "audio/", 6) ||
		   !strncasecomp (type, "video/", 6) ||
		   !strncasecomp (type, "application/", 12))
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
		if (!strcasecomp (type, *s))
		  return FALSE;

	  /* All others must be assumed to be binary formats, and need Base64. */
	  return TRUE;
	}

  else
	return FALSE;
}

#ifdef XP_OS2
XP_BEGIN_PROTOS
extern int mime_encoder_output_fn (const char *buf, int32 size, void *closure);
XP_END_PROTOS
#else
static int mime_encoder_output_fn (const char *buf, int32 size, void *closure);
#endif

/* Given a content-type and some info about the contents of the document,
   decide what encoding it should have.
 */
int
MSG_DeliverMimeAttachment::PickEncoding (int16 mail_csid)
{
  // use the boolean so we only have to test for uuencode vs base64 once
  XP_Bool needsB64 = FALSE;

  if (m_already_encoded_p)
	goto DONE;

  if (mime_type_requires_b64_p (m_type))
	{
	  /* If the content-type is "image/" or something else known to be binary,
		 always use base64 (so that we don't get confused by newline
		 conversions.)
	   */
		needsB64 = TRUE;
	}
  else
	{
	  /* Otherwise, we need to pick an encoding based on the contents of
		 the document.
	   */

	  XP_Bool encode_p;

	  if (m_max_column > 900)
		encode_p = TRUE;
	  else if (mime_use_quoted_printable_p && m_unprintable_count)
		encode_p = TRUE;

	  else if (m_null_count)	/* If there are nulls, we must always encode,
								   because sendmail will blow up. */
		encode_p = TRUE;
#if 0
	  else if (m_ctl_count)	/* Should we encode if random other control
								   characters are present?  Probably... */
		encode_p = TRUE;
#endif
	  else
		encode_p = FALSE;

	  /* MIME requires a special case that these types never be encoded.
	   */
	  if (!strncasecomp (m_type, "message", 7) ||
		  !strncasecomp (m_type, "multipart", 9))
		{
		  encode_p = FALSE;
		  if (m_desired_type && !strcasecomp (m_desired_type, TEXT_PLAIN))
			{
			  XP_FREE (m_desired_type);
			  m_desired_type = 0;
			}
		}
	  /* If the Mail csid is CS_JIS we force it to use Base64 for attachments (bug#104255). 
		Use 7 bit for other STATFUL ( e.g. CS_2022_KR) csid. */
	  if ((mail_csid == CS_JIS) &&
		  (strcasecomp(m_type, TEXT_HTML) == 0))
         needsB64 = TRUE;
	  else if((mail_csid & STATEFUL) &&
         ((strcasecomp(m_type, TEXT_HTML) == 0) ||
          (strcasecomp(m_type, TEXT_MDL) == 0) ||
          (strcasecomp(m_type, TEXT_PLAIN) == 0) ||
          (strcasecomp(m_type, TEXT_RICHTEXT) == 0) ||
          (strcasecomp(m_type, TEXT_ENRICHED) == 0) ||
          (strcasecomp(m_type, TEXT_VCARD) == 0) ||
          (strcasecomp(m_type, TEXT_CSS) == 0) ||
          (strcasecomp(m_type, TEXT_JSSS) == 0) ||
          (strcasecomp(m_type, MESSAGE_RFC822) == 0) ||
          (strcasecomp(m_type, MESSAGE_NEWS) == 0)))
      {
        needsB64 = TRUE;
     }
	  else if (encode_p &&
			   m_size > 500 &&
			   m_unprintable_count > (m_size / 10))
		/* If the document contains more than 10% unprintable characters,
		   then that seems like a good candidate for base64 instead of
		   quoted-printable.
		 */
		  needsB64 = TRUE;
	  else if (encode_p)
		StrAllocCopy (m_encoding, ENCODING_QUOTED_PRINTABLE);
	  else if (m_highbit_count > 0)
		StrAllocCopy (m_encoding, ENCODING_8BIT);
	  else
		StrAllocCopy (m_encoding, ENCODING_7BIT);
	}

  if (needsB64)
  {
	  /* 
		 ### mwelch We might have to uuencode instead of 
		            base64 the binary data.
	  */
	  if (UseUUEncode_p())
		  StrAllocCopy (m_encoding, ENCODING_UUENCODE);
	  else
		  StrAllocCopy (m_encoding, ENCODING_BASE64);
  }

  /* Now that we've picked an encoding, initialize the filter.
   */
  XP_ASSERT(!m_encoder_data);
  if (!strcasecomp(m_encoding, ENCODING_BASE64))
	{
	  m_encoder_data = MimeB64EncoderInit(mime_encoder_output_fn,
										  m_mime_delivery_state);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else if (!strcasecomp(m_encoding, ENCODING_UUENCODE))
	{
		char *tailName = NULL;
		
		if (m_url_string)
		{
		  	tailName = XP_STRRCHR(m_url_string, '/');
		  	if (tailName)
		  		tailName = XP_STRDUP(tailName+1);
		}
		
		if (m_url && !tailName)
		{
			tailName = XP_STRRCHR(m_url->address, '/');
			if (tailName)
				tailName = XP_STRDUP(tailName+1);
		}

	  m_encoder_data = MimeUUEncoderInit(tailName ? tailName : "",
	  									  mime_encoder_output_fn,
										  m_mime_delivery_state);
	  XP_FREEIF(tailName);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else if (!strcasecomp(m_encoding, ENCODING_QUOTED_PRINTABLE))
	{
	  m_encoder_data = MimeQPEncoderInit(mime_encoder_output_fn,
										 m_mime_delivery_state);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else
	{
	  m_encoder_data = 0;
	}


  /* Do some cleanup for documents with unknown content type.

	 There are two issues: how they look to MIME users, and how they look to
	 non-MIME users.

	 If the user attaches a "README" file, which has unknown type because it
	 has no extension, we still need to send it with no encoding, so that it
	 is readable to non-MIME users.

	 But if the user attaches some random binary file, then base64 encoding
	 will have been chosen for it (above), and in this case, it won't be
	 immediately readable by non-MIME users.  However, if we type it as
	 text/plain instead of application/octet-stream, it will show up inline
	 in a MIME viewer, which will probably be ugly, and may possibly have
	 bad charset things happen as well.

	 So, the heuristic we use is, if the type is unknown, then the type is
	 set to application/octet-stream for data which needs base64 (binary data)
	 and is set to text/plain for data which didn't need base64 (unencoded or
	 lightly encoded data.)
   */
 DONE:
  if (!m_type || !*m_type || !strcasecomp(m_type, UNKNOWN_CONTENT_TYPE))
	{
	  if (m_already_encoded_p)
		StrAllocCopy (m_type, APPLICATION_OCTET_STREAM);
	  else if (m_encoding &&
			   (!strcasecomp(m_encoding, ENCODING_BASE64) ||
				!strcasecomp(m_encoding, ENCODING_UUENCODE)))
		StrAllocCopy (m_type, APPLICATION_OCTET_STREAM);
	  else
		StrAllocCopy (m_type, TEXT_PLAIN);
	}
  return 0;
}


/* Some types should have a "charset=" parameter, and some shouldn't.
   This is what decides. */
XP_Bool
mime_type_needs_charset (const char *type)
{
  /* Only text types should have charset. */
  if (!type || !*type)
	return FALSE;
  else if (!strncasecomp (type, "text", 4))
	return TRUE;
  else
	return FALSE;
}


char*
mime_get_stream_write_buffer(void)
{
	if (!mime_mailto_stream_write_buffer) {
		mime_mailto_stream_write_buffer = (char *) XP_ALLOC(MIME_BUFFER_SIZE);
	}
	return mime_mailto_stream_write_buffer;
}

int MSG_SendMimeDeliveryState::InitImapOfflineDB(uint32 flag)
{
	int status = 0;
	char *folderName = msg_MagicFolderName(m_pane->GetMaster()->GetPrefs(), flag, &status);

	if (status < 0) 
		return status;
	else if (!folderName) 
		return MK_OUT_OF_MEMORY;
	else if (NET_URL_Type(folderName) == IMAP_TYPE_URL)
	{
		MsgERR err = eSUCCESS;
		char *dummyEnvelope = msg_GetDummyEnvelope();
		XP_ASSERT(!m_imapOutgoingParser && !m_imapLocalMailDB);
		MailDB::Open(m_msg_file_name, TRUE, &m_imapLocalMailDB);
		if (err != eSUCCESS) 
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
		m_imapOutgoingParser->SetOutFile(m_msg_file);
		m_imapOutgoingParser->SetMailDB(m_imapLocalMailDB);
		m_imapOutgoingParser->SetWriteToOutFile(FALSE);
		m_imapOutgoingParser->Init(0);
		m_imapOutgoingParser->StartNewEnvelope(dummyEnvelope,
											   XP_STRLEN(dummyEnvelope));
	}

done:
	XP_FREEIF(folderName);
	return status;
}
#define PUSH_STRING(S) \
 do { XP_STRCPY (buffer_tail, S); buffer_tail += XP_STRLEN (S); } while(0)
#define PUSH_NEWLINE() \
 do { *buffer_tail++ = CR; *buffer_tail++ = LF; *buffer_tail = '\0'; } while(0)


/* All of the desired attachments have been written to individual temp files,
   and we know what's in them.  Now we need to make a final temp file of the
   actual mail message, containing all of the other files after having been
   encoded as appropriate.
 */
int MSG_SendMimeDeliveryState::GatherMimeAttachments ()
{
  int32 status, i;
  char *headers = 0;
  char *separator = 0;
  XP_File in_file = 0;
  XP_Bool multipart_p = FALSE;
  XP_Bool plaintext_is_mainbody_p = FALSE; // only using text converted from HTML?
  char *buffer = 0;
  char *buffer_tail = 0;
  char* error_msg = 0;
  int16 win_csid	 = INTL_DefaultWinCharSetID(GetContext());
  XP_Bool tonews =  (m_fields && m_fields->GetNewsgroups() && *m_fields->GetNewsgroups()) ;
	// to news is true if we have a m_field and we have a Newsgroup and it is not empty
  INTL_MessageSendToNews(tonews);	// hack to make Korean Mail/News work correctly 
					// Look at libi18n/doc_ccc.c for details 
					// temp solution for bug 30725
  int16 mail_csid 	 = tonews ?
		INTL_DefaultNewsCharSetID(win_csid) :
		INTL_DefaultMailCharSetID(win_csid);

  MSG_SendPart* toppart = NULL;	// The very top most container of the message
								// that we are going to send.
  MSG_SendPart* mainbody = NULL;// The leaf node that contains the text of the
								// message we're going to contain.
  MSG_SendPart* maincontainer = NULL;// The direct child of toppart that will
									 // contain the mainbody.  If mainbody is
									 // the same as toppart, then this is
									 // also the same.  But if mainbody is
									 // to end up somewhere inside of a
									 // multipart/alternative or a
									 // multipart/related, then this is that
									 // multipart object.

  MSG_SendPart* plainpart = NULL;	// If we converted HTML into plaintext,
  									// the message or child containing the plaintext
  									// goes here. (Need to use this to determine
  									// what headers to append/set to the main 
  									// message body.)
  char *hdrs = 0;
  XP_Bool maincontainerISrelatedpart = FALSE;

  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(GetContext());


  status = m_status;
  if (status < 0)
	goto FAIL;

  if (m_attachments_only_p)
	{
	  /* If we get here, we shouldn't have the "generating a message" cb. */
	  XP_ASSERT(!m_dont_deliver_p &&
				!m_message_delivery_done_callback);
	  if (m_attachments_done_callback)
		{
		  struct MSG_AttachedFile *attachments;

		  XP_ASSERT(m_attachment_count > 0);
		  if (m_attachment_count <= 0)
			{
			  m_attachments_done_callback (GetContext(),
												m_fe_data, 0, 0,
												0);
			  m_attachments_done_callback = 0;
			  Clear();
			  goto FAIL;
			}

		  attachments = (struct MSG_AttachedFile *)
			XP_ALLOC((m_attachment_count + 1) * sizeof(*attachments));

		  if (!attachments) goto FAILMEM;
		  XP_MEMSET(attachments, 0, ((m_attachment_count + 1)
									 * sizeof(*attachments)));
		  for (i = 0; i < m_attachment_count; i++)
			{
			  MSG_DeliverMimeAttachment *ma = &m_attachments[i];

#undef SNARF
#define SNARF(x,y) do { if((y) && *(y) && !(x)) { ((x) = (y)); ((y) = 0); }} \
				   while(0)
			  /* Rather than copying the strings and dealing with allocation
				 failures, we'll just "move" them into the other struct (this
				 should be ok since this file uses FREEIF when discarding
				 the mime_attachment objects.) */
			  SNARF(attachments[i].orig_url, ma->m_url_string);
			  SNARF(attachments[i].file_name, ma->m_file_name);
			  SNARF(attachments[i].type, ma->m_type);
			  SNARF(attachments[i].encoding, ma->m_encoding);
			  SNARF(attachments[i].description, ma->m_description);
			  SNARF(attachments[i].x_mac_type, ma->m_x_mac_type);
			  SNARF(attachments[i].x_mac_creator, ma->m_x_mac_creator);
#undef SNARF
			  HG96484
			  attachments[i].size = ma->m_size;
			  attachments[i].unprintable_count = ma->m_unprintable_count;
			  attachments[i].highbit_count = ma->m_highbit_count;
			  attachments[i].ctl_count = ma->m_ctl_count;
			  attachments[i].null_count = ma->m_null_count;
			  attachments[i].max_line_length = ma->m_max_column;

			  /* Doesn't really matter, but let's not lie about encoding
				 in case it does someday. */
			  if (attachments[i].highbit_count > 0 &&
				  attachments[i].encoding &&
				  !strcasecomp(attachments[i].encoding, ENCODING_7BIT))
				StrAllocCopy (attachments[i].encoding, ENCODING_8BIT);
			}

		  /* The callback is expected to free the attachments list and all
			 the strings in it.   It's also expected to delete the files!
		   */
		  m_attachments_done_callback (GetContext(),
											m_fe_data, 0, 0,
											attachments);
		  m_attachments_done_callback = 0;
		  Clear();
		}
	  goto FAIL;
	}


  /* If we get here, we're generating a message, so there shouldn't be an
	 attachments callback. */
  XP_ASSERT(!m_attachments_done_callback);


  if (!m_attachment1_type) {
	  m_attachment1_type = XP_STRDUP(TEXT_PLAIN);
	  if (!m_attachment1_type) goto FAILMEM;
  }

  // If we have a text/html main part, and we need a plaintext attachment, then
  // we'll do so now.  This is an asynchronous thing, so we'll kick it off and
  // count on getting back here when it finishes.

  if (m_plaintext == NULL &&
	  (m_fields->GetForcePlainText() ||
	   m_fields->GetUseMultipartAlternative()) &&
	    m_attachment1_body && XP_STRCMP(m_attachment1_type, TEXT_HTML) == 0) {
	  m_html_filename = WH_TempName (xpFileToPost, "nsmail");
	  if (!m_html_filename) goto FAILMEM;
	  in_file = XP_FileOpen(m_html_filename, xpFileToPost, XP_FILE_WRITE_BIN);
	  if (!in_file) {
		  status = MK_UNABLE_TO_OPEN_TMP_FILE;
		  goto FAIL;
	  }
	  status = XP_FileWrite(m_attachment1_body, m_attachment1_body_length,
							in_file);
	  if (status < int(m_attachment1_body_length)) {
		  if (status >= 0) {
			  status = MK_MIME_ERROR_WRITING_FILE;
		  }
		  goto FAIL;
	  }
	  status = XP_FileClose(in_file);
	  in_file = NULL;
	  if (status < 0) goto FAIL;

	  m_plaintext = new MSG_DeliverMimeAttachment;
	  if (!m_plaintext) goto FAILMEM;
	  m_plaintext->m_mime_delivery_state = this;
	  char* temp = WH_FileName(m_html_filename, xpFileToPost);
	  m_plaintext->m_url_string = XP_PlatformFileToURL(temp);
	  if (temp) XP_FREE(temp);
	  if (!m_plaintext->m_url_string) goto FAILMEM;
	  m_plaintext->m_url =
		  NET_CreateURLStruct (m_plaintext->m_url_string, NET_DONT_RELOAD);
	  if (!m_plaintext->m_url) goto FAILMEM;
	  StrAllocCopy(m_plaintext->m_url->content_type, TEXT_HTML);
	  StrAllocCopy(m_plaintext->m_type, TEXT_HTML);
	  StrAllocCopy(m_plaintext->m_desired_type, TEXT_PLAIN);
	  m_attachment_pending_count++;
	  status = m_plaintext->SnarfAttachment();
	  if (status < 0) goto FAIL;
	  if (m_attachment_pending_count > 0) return 0;
  }
	  

  /* hack to avoid having to allocate memory... */
  buffer = mime_get_stream_write_buffer();
  if (! buffer) goto FAILMEM;

  buffer_tail = buffer;

  XP_ASSERT (m_attachment_pending_count == 0);

  FE_Progress(GetContext(), XP_GetString(MK_MSG_ASSEMBLING_MSG));

  /* First, open the message file.
   */
  m_msg_file_name = WH_TempName (xpFileToPost, "nsmail");
  if (! m_msg_file_name) goto FAILMEM;
  m_msg_file = XP_FileOpen (m_msg_file_name, xpFileToPost,
								 XP_FILE_WRITE_BIN);
  if (! m_msg_file)
	{
	  status = MK_UNABLE_TO_OPEN_TMP_FILE;
	  error_msg = PR_smprintf(XP_GetString(status), m_msg_file_name);
	  if (!error_msg) status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  if (NET_IsOffline() && (m_deliver_mode == MSG_SaveAsDraft || 
						  m_deliver_mode == MSG_SaveAsTemplate))
  {
	  status = InitImapOfflineDB( m_deliver_mode == MSG_SaveAsDraft ?
								  MSG_FOLDER_FLAG_DRAFTS :
								  MSG_FOLDER_FLAG_TEMPLATES );
	  if (status < 0)
	  {
		  StrAllocCopy (error_msg, XP_GetString(status));
		  goto FAIL;
	  }
  }
	  
#ifdef GENERATE_MESSAGE_ID
  if (m_fields->GetMessageId() == NULL) {
	m_fields->SetMessageId(msg_generate_message_id ());
  }
#endif /* GENERATE_MESSAGE_ID */

  mainbody = new MSG_SendPart(this, mail_csid);
  if (!mainbody) goto FAILMEM;
  
  mainbody->SetMainPart(TRUE);
  mainbody->SetType(m_attachment1_type ? m_attachment1_type : TEXT_PLAIN);

  XP_ASSERT(mainbody->GetBuffer() == NULL);
  status = mainbody->SetBuffer(m_attachment1_body ? m_attachment1_body : " ");
  if (status < 0) goto FAIL;

	/*	### mwelch 
		Determine the encoding of the main message body before we free it.
		The proper way to do this should be to test whatever text is in mainbody
		just before writing it out, but that will require a fix that is less safe
		and takes more memory. */
  if ((mail_csid & STATEFUL) || /* CS_JIS or CS_2022_KR */
		  mime_7bit_data_p (m_attachment1_body,
						m_attachment1_body_length))
	StrAllocCopy (m_attachment1_encoding, ENCODING_7BIT);
  else if (mime_use_quoted_printable_p)
	StrAllocCopy (m_attachment1_encoding, ENCODING_QUOTED_PRINTABLE);
  else
	StrAllocCopy (m_attachment1_encoding, ENCODING_8BIT);

  FREEIF (m_attachment1_body);

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
	  // MSG_SendParts because they are deleted at the end of message 
	  // delivery by the TapeFileSystem object 
	  // (MSG_MimeRelatedSaver / mhtmlstm.cpp).
	  delete mainbody;

	  // No matter what, maincontainer points to the outermost related part.
	  maincontainer = m_related_part;
	  maincontainerISrelatedpart = TRUE;

	  const char *relPartType = m_related_part->GetType();
	  if (relPartType && !strcmp(relPartType, MULTIPART_RELATED))
		  // outer shell is m/related,
		  // mainbody wants to be the HTML lump within
		  mainbody = m_related_part->GetChild(0);
	  else
		  // outer shell is text/html, 
		  // so mainbody wants to be the same lump
		  mainbody = maincontainer;

	  mainbody->SetMainPart(TRUE);
  }

  if (m_plaintext) {
	  // OK.  We have a plaintext version of the main body that we want to
	  // send instead of or with the text/html.  Put it in.
	  
	  plainpart = new MSG_SendPart(this, mail_csid);
	  if (!plainpart) goto FAILMEM;
	  status = plainpart->SetType(TEXT_PLAIN);
	  if (status < 0) goto FAIL;
	  status = plainpart->SetFile(m_plaintext->m_file_name, xpFileToPost);
	  if (status < 0) goto FAIL;
	  m_plaintext->AnalyzeSnarfedFile(); // look for 8 bit text, long lines, etc.
	  m_plaintext->PickEncoding(mail_csid);
	  hdrs = mime_generate_attachment_headers(m_plaintext->m_type,
											  m_plaintext->m_encoding,
											  m_plaintext->m_description,
											  m_plaintext->m_x_mac_type,
											  m_plaintext->m_x_mac_creator,
											  m_plaintext->m_real_name, 0,
											  m_digest_p,
											  m_plaintext,
											  mail_csid);
	  if (!hdrs) goto FAILMEM;
	  status = plainpart->SetOtherHeaders(hdrs);
	  XP_FREE(hdrs);
	  hdrs = NULL;
	  if (status < 0) goto FAIL;

	  

	  if (m_fields->GetUseMultipartAlternative()) {
		  MSG_SendPart* htmlpart = maincontainer;
		  maincontainer = new MSG_SendPart(this);
		  if (!maincontainer) goto FAILMEM;
		  status = maincontainer->SetType(MULTIPART_ALTERNATIVE);
		  if (status < 0) goto FAIL;
		  status = maincontainer->AddChild(plainpart);
		  if (status < 0) goto FAIL;
		  status = maincontainer->AddChild(htmlpart);
		  if (status < 0) goto FAIL;
		  
		  // Create the encoder for the plaintext part here,
		  // because we aren't the main part (attachment1).
		  // (This, along with the rest of the routine, should really
		  // be restructured so that no special treatment is given to
		  // the main body text that came in. Best to put attachment1_text
		  // etc. into a MSG_SendPart, then reshuffle the parts. Sigh.)
		  if (!XP_STRCASECMP(m_plaintext->m_encoding, ENCODING_QUOTED_PRINTABLE))
		  {
		  	 MimeEncoderData *plaintext_enc = MimeQPEncoderInit(mime_encoder_output_fn, this);
		  	 if (!plaintext_enc)
		  	 {
		  	 	status = MK_OUT_OF_MEMORY;
		  	 	goto FAIL;
		  	 }
		  	 plainpart->SetEncoderData(plaintext_enc);
		  }
	  } else {
		  delete maincontainer; //### mwelch - does this cause a crash?!
		  if (maincontainerISrelatedpart)
			  m_related_part = NULL;
		  maincontainer = plainpart;
		  mainbody = maincontainer;
		  FREEIF(m_attachment1_type);
		  m_attachment1_type = XP_STRDUP(TEXT_PLAIN);
		  if (!m_attachment1_type) goto FAILMEM;
		  
		  /* Override attachment1_encoding here. */
	 	  FREEIF(m_attachment1_encoding);
	 	  StrAllocCopy(m_attachment1_encoding, m_plaintext->m_encoding);

		  plaintext_is_mainbody_p = TRUE; // converted plaintext is mainbody
	  }
  }

  // ###tw  This used to be this more complicated thing, but for now, if it we
  // have any attachments, we generate multipart.
  //multipart_p = (m_attachment_count > 1 ||
  //				 (m_attachment_count == 1 &&
  //				  m_attachment1_body_length > 0));
  multipart_p = (m_attachment_count > 0);

  if (multipart_p) {
	toppart = new MSG_SendPart(this);
	if (!toppart) goto FAILMEM;
	status = toppart->SetType(m_digest_p ? MULTIPART_DIGEST : MULTIPART_MIXED);
	if (status < 0) goto FAIL;
	status = toppart->AddChild(maincontainer);
	if (status < 0) goto FAIL;
    HG36459
    { 
	  status = toppart->SetBuffer(XP_GetString (MK_MIME_MULTIPART_BLURB));
	  if (status < 0) goto FAIL;
	}
  } else {
	toppart = maincontainer;
  }


  /* Write out the message headers.
   */
  headers = mime_generate_headers (m_fields,
								   INTL_GetCSIWinCSID(c),
								   m_deliver_mode);
  if (!headers) goto FAILMEM;

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
  /* reordering of headers will happen in MSG_SendPart::Write */
  if ((plainpart) && (plainpart == toppart))
  	status = toppart->AppendOtherHeaders(headers);
  else
	status = toppart->SetOtherHeaders(headers);
  XP_FREE(headers);
  headers = NULL;
  if (status < 0) goto FAIL;


  /* Set up the first part (user-typed.)  For now, do it even if the first
   * part is empty; we need to add things to skip it if this part is empty.
   * ###tw
   */


  /* Set up encoder for the first part (message body.)
   */
  XP_ASSERT(!m_attachment1_encoder_data);
  if (!strcasecomp(m_attachment1_encoding, ENCODING_BASE64))
	{
	  m_attachment1_encoder_data =
		MimeB64EncoderInit(mime_encoder_output_fn, this);
	  if (!m_attachment1_encoder_data) goto FAILMEM;
	}
  else if (!strcasecomp(m_attachment1_encoding,
						ENCODING_QUOTED_PRINTABLE))
	{
	  m_attachment1_encoder_data =
		MimeQPEncoderInit(mime_encoder_output_fn, this);
	  if (!m_attachment1_encoder_data) goto FAILMEM;
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
											   mail_csid);
	  if (!hdrs) goto FAILMEM;
	  status = mainbody->AppendOtherHeaders(hdrs);
	  if (status < 0) goto FAIL;
  }

  FREEIF(hdrs);

  status = mainbody->SetEncoderData(m_attachment1_encoder_data);
  m_attachment1_encoder_data = NULL;
  if (status < 0) goto FAIL;


  /* Set up the subsequent parts.
   */
  if (m_attachment_count > 0)
	{
	  char *buffer;

	  /* Hack to avoid having to allocate memory... */
	  if (! mime_mailto_stream_read_buffer)
		mime_mailto_stream_read_buffer = (char *) XP_ALLOC (MIME_BUFFER_SIZE);
	  buffer = mime_mailto_stream_read_buffer;
	  if (! buffer) goto FAILMEM;
	  buffer_tail = buffer;

	  for (i = 0; i < m_attachment_count; i++)
		{
		  MSG_DeliverMimeAttachment *ma = &m_attachments[i];
		  char *hdrs = 0;

		  MSG_SendPart* part = NULL;

		  // If at this point we *still* don't have an content-type, then
		  // we're never going to get one.
		  if (ma->m_type == NULL) {
			  ma->m_type = XP_STRDUP(UNKNOWN_CONTENT_TYPE);
			  if (ma->m_type == NULL) goto FAILMEM;
		  }

		  ma->PickEncoding (mail_csid);

		  part = new MSG_SendPart(this);
		  if (!part) goto FAILMEM;
		  status = toppart->AddChild(part);
		  if (status < 0) goto FAIL;
		  status = part->SetType(ma->m_type);
		  if (status < 0) goto FAIL;

		  hdrs = mime_generate_attachment_headers (ma->m_type, ma->m_encoding,
												   ma->m_description,
												   ma->m_x_mac_type,
												   ma->m_x_mac_creator,
												   ma->m_real_name,
												   ma->m_url_string,
												   m_digest_p,
												   ma,
												   mail_csid);
		  if (!hdrs) goto FAILMEM;

		  status = part->SetOtherHeaders(hdrs);
		  FREEIF(hdrs);
		  if (status < 0) goto FAIL;
		  status = part->SetFile(ma->m_file_name, xpFileToPost);
		  if (status < 0) goto FAIL;
		  if (ma->m_encoder_data) {
			status = part->SetEncoderData(ma->m_encoder_data);
			if (status < 0) goto FAIL;
			ma->m_encoder_data = NULL;
		  }

		  ma->m_current_column = 0;

		  if (ma->m_type &&
			  (!strcasecomp (ma->m_type, MESSAGE_RFC822) ||
			   !strcasecomp (ma->m_type, MESSAGE_NEWS))) {
			status = part->SetStripSensitiveHeaders(TRUE);
			if (status < 0) goto FAIL;
		  }

		}
	}

  // OK, now actually write the structure we've carefully built up.
  status = toppart->Write();
  if (status < 0) goto FAIL;

  HG75442

  if (m_msg_file)
	XP_FileClose (m_msg_file);
  m_msg_file = 0;

  if (m_imapOutgoingParser)
  {
 	  m_imapOutgoingParser->FinishHeader();
	  XP_ASSERT(m_imapOutgoingParser->m_newMsgHdr);
 	  m_imapOutgoingParser->m_newMsgHdr->OrFlags(kIsRead);
 	  m_imapOutgoingParser->m_newMsgHdr->SetMessageSize
 		  (m_imapOutgoingParser->m_bytes_written);
	  m_imapOutgoingParser->m_newMsgHdr->SetMessageKey(0);
 	  m_imapLocalMailDB->AddHdrToDB(m_imapOutgoingParser->m_newMsgHdr,
 								 NULL, TRUE);
  }

  FE_Progress(GetContext(), XP_GetString(MK_MSG_ASSEMB_DONE_MSG));

  if (m_dont_deliver_p &&
	  m_message_delivery_done_callback)
	{
	  m_message_delivery_done_callback (GetContext(),
											 m_fe_data, 0,
											 XP_STRDUP (m_msg_file_name));
	  /* Need to ditch the file name here so that we don't delete the
		 file, since in this case, the FE needs the file itself. */
	  FREEIF (m_msg_file_name);
	  m_msg_file_name = 0;
	  m_message_delivery_done_callback = 0;
	  Clear();
	}
  else
	{
	  DeliverMessage();
	}

  // Get rid of all the encoder data and temporary files.
  for (i = 0; i < m_attachment_count; i++) {
	MSG_DeliverMimeAttachment *ma = &m_attachments[i];
	if (ma->m_encoder_data) {
	  status = MimeEncoderDestroy(ma->m_encoder_data, FALSE);
	  ma->m_encoder_data = 0;
	  if (status < 0) goto FAIL;
	}

	if (!m_pre_snarfed_attachments_p) {
	  if (ma->m_file) {
		  XP_FileClose(ma->m_file);
		  ma->m_file = 0;
	  }
	  XP_FileRemove(ma->m_file_name, xpFileToPost);
	}
	XP_FREE (ma->m_file_name);
	ma->m_file_name = 0;
  }



 FAIL:
  if (toppart)
	delete toppart;
  toppart = NULL;
  mainbody = NULL;
  maincontainer = NULL;

  /* Close off encoder for the first part (message body.) */
  if (m_attachment1_encoder_data)
	{
	  status = MimeEncoderDestroy(m_attachment1_encoder_data, FALSE);
	  m_attachment1_encoder_data = 0;
	  if (status < 0) goto FAIL;
	}
  if (headers) XP_FREE (headers);
  if (separator) XP_FREE (separator);
  if (in_file) {
	XP_FileClose (in_file);
	in_file = NULL;
  }

  HG59731

  if (status < 0)
	{
	  m_status = status;
	  Fail (status, error_msg);
	}
  /* If status is >= 0, then the the next event coming up is posting to
	 a "mailto:" or "news:" URL; the message_delivery_done_callback will
	 be called from the exit routine of that URL. */


  if (m_plaintext) {
	  if (m_plaintext->m_file)
		  XP_FileClose(m_plaintext->m_file);
	  XP_FileRemove(m_plaintext->m_file_name, xpFileToPost);
	  XP_FREE(m_plaintext->m_file_name);
	  m_plaintext->m_file_name = NULL;
	  delete m_plaintext;
	  m_plaintext = NULL;
  }

  if (m_html_filename) {
	  XP_FileRemove(m_html_filename, xpFileToPost);
	  XP_FREE(m_html_filename);
	  m_html_filename = NULL;
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

static void FROB (const char *src, char *dest)
{
    if (src && *src && dest)
    {
        if (*dest) XP_STRCAT(dest, ",");
        XP_STRCAT(dest, src);
    }
}

HG53784

#if defined(XP_MAC) && defined(DEBUG)
#pragma global_optimizer reset
#endif // XP_MAC && DEBUG


int
mime_write_message_body (MSG_SendMimeDeliveryState *state,
						 char *buf, int32 size)
{
    HG56898
	{
	  if (int32(XP_FileWrite (buf, size, state->m_msg_file)) < size)
	  {
		return MK_MIME_ERROR_WRITING_FILE;
	  }
	  else
	  {
	    if (state->m_imapOutgoingParser)
		{
			state->m_imapOutgoingParser->ParseFolderLine(buf, size);
			state->m_imapOutgoingParser->m_bytes_written += size;
		}
		return 0;
	  }
	}
}


#ifdef XP_OS2
extern int
#else
static int
#endif
mime_encoder_output_fn (const char *buf, int32 size, void *closure)
{
  MSG_SendMimeDeliveryState *state = (MSG_SendMimeDeliveryState *) closure;
  return mime_write_message_body (state, (char *) buf, size);
}


char *
msg_generate_message_id (void)
{
  time_t now = XP_TIME();
  uint32 salt = 0;
  const char *host = 0;
  const char *from = FE_UsersMailAddress ();

  RNG_GenerateGlobalRandomBytes((void *) &salt, sizeof(salt));

  if (from)
	{
	  host = XP_STRCHR (from, '@');
	  if (host)
	    {
	      const char *s;
	      for (s = ++host; *s; s++)
		    if (!XP_IS_ALPHA(*s) && !XP_IS_DIGIT(*s) &&
			    *s != '-' && *s != '_' && *s != '.')
		      {
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

static char *
mime_generate_headers (MSG_CompositionFields *fields,
					   int csid,
					   MSG_Deliver_Mode deliver_mode)
{
  int size = 0;
  char *buffer = 0, *buffer_tail = 0;
  XP_Bool isDraft = deliver_mode == MSG_SaveAsDraft ||
					deliver_mode == MSG_SaveAsTemplate ||
					deliver_mode == MSG_QueueForLater;

  XP_ASSERT (fields);
  if (!fields)
	  return NULL;

  /* Multiply by 3 here to make enough room for MimePartII conversion */
  if (fields->GetFrom())                 size += 3 * XP_STRLEN (fields->GetFrom());
  if (fields->GetReplyTo())             size += 3 * XP_STRLEN (fields->GetReplyTo());
  if (fields->GetTo())                   size += 3 * XP_STRLEN (fields->GetTo());
  if (fields->GetCc())                   size += 3 * XP_STRLEN (fields->GetCc());
  if (fields->GetNewsgroups())           size += 3 * XP_STRLEN (fields->GetNewsgroups());
  if (fields->GetFollowupTo())          size += 3 * XP_STRLEN (fields->GetFollowupTo());
  if (fields->GetSubject())              size += 3 * XP_STRLEN (fields->GetSubject());
  if (fields->GetReferences())           size += 3 * XP_STRLEN (fields->GetReferences());
  if (fields->GetOrganization())         size += 3 * XP_STRLEN (fields->GetOrganization());
  if (fields->GetOtherRandomHeaders()) size += 3 * XP_STRLEN (fields->GetOtherRandomHeaders());
  if (fields->GetPriority())             size += 3 * XP_STRLEN (fields->GetPriority());
#ifdef GENERATE_MESSAGE_ID
  if (fields->GetMessageId())           size += XP_STRLEN (fields->GetMessageId());
#endif /* GENERATE_MESSAGE_ID */

  /* Add a bunch of space for the static parts of the headers. */
  /* size += 2048; */
  size += 2560;

  buffer = (char *) XP_ALLOC (size);
  if (!buffer)
	return 0; /* MK_OUT_OF_MEMORY */

  buffer_tail = buffer;

#ifdef GENERATE_MESSAGE_ID
  if (fields->GetMessageId() && *fields->GetMessageId())
	{
	  char *convbuf = NULL;
	  PUSH_STRING ("Message-ID: ");
	  PUSH_STRING (fields->GetMessageId());
	  PUSH_NEWLINE ();
	  /* MDN request header requires to have MessageID header presented
	   * in the message in order to
	   * coorelate the MDN reports to the original message. Here will be
	   * the right place
	   */
	  if (fields->GetReturnReceipt() && 
		  (fields->GetReturnReceiptType() == 2 ||
		   fields->GetReturnReceiptType() == 3) && 
		  (deliver_mode != MSG_SaveAsDraft &&
					   deliver_mode != MSG_SaveAsTemplate))
	  {
		  int32 receipt_header_type = 0;

		  PREF_GetIntPref("mail.receipt.request_header_type",
						  &receipt_header_type);
		  // 0 = MDN Disposition-Notification-To: ; 1 = Return-Receipt-To: ; 2 =
		  // both MDN DNT & RRT headers
		  if (receipt_header_type == 1)
		  {
		  RRT_HEADER:
			  PUSH_STRING ("Return-Receipt-To: ");
			  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetFrom(), csid,
												mime_headers_use_quoted_printable_p);
			  if (convbuf)     /* MIME-PartII conversion */
			  {
				  PUSH_STRING (convbuf);
				  XP_FREEIF(convbuf);
			  }
			  else
				  PUSH_STRING (fields->GetFrom());
			  PUSH_NEWLINE ();
		  }
		  else 
		  {
			  PUSH_STRING ("Disposition-Notification-To: ");
			  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetFrom(), csid,
												mime_headers_use_quoted_printable_p);
			  if (convbuf)     /* MIME-PartII conversion */
			  {
				  PUSH_STRING (convbuf);
				  XP_FREEIF(convbuf);
			  }
			  else
				  PUSH_STRING (fields->GetFrom());
			  PUSH_NEWLINE ();
			  if (receipt_header_type == 2)
				  goto RRT_HEADER;
		  }
	  }
#ifdef SUPPORT_X_TEMPLATE_NAME
	  if (deliver_mode == MSG_SaveAsTemplate)
	  {
		  PUSH_STRING ("X-Template: ");
		  if (fields->GetTemplateName())
		  {
			  convbuf = IntlEncodeMimePartIIStr((char *)
									 fields->GetTemplateName(),
									 csid,
									 mime_headers_use_quoted_printable_p);
			  if (convbuf)
			  {
				  PUSH_STRING (convbuf);
				  XP_FREEIF(convbuf);
			  }
			  else
			  {
				  PUSH_STRING(fields->GetTemplateName());
			  }
		  }
		  PUSH_NEWLINE ();
	  }
#endif /* SUPPORT_X_TEMPLATE_NAME */
	}
#endif /* GENERATE_MESSAGE_ID */

  {
#if 0
  /* Use strftime() to format the date, then figure out what our local
	 GMT offset it, and append that (since strftime() can't do that.)
	 Generate four digit years as per RFC 1123 (superceding RFC 822.)
   */
	time_t now = time ((time_t *) 0);
	int gmtoffset = XP_LocalZoneOffset();
	strftime (buffer_tail, 100, "Date: %a, %d %b %Y %H:%M:%S ",
			  localtime (&now));
#else
	int gmtoffset = XP_LocalZoneOffset();
#ifndef NSPR20
    PRTime now;
    PR_ExplodeTime(&now, PR_Now());
#else
    PRExplodedTime now;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &now);
#endif /* NSPR20 */

	/* Use PR_FormatTimeUSEnglish() to format the date in US English format,
	   then figure out what our local GMT offset is, and append it (since
	   PR_FormatTimeUSEnglish() can't do that.) Generate four digit years as
	   per RFC 1123 (superceding RFC 822.)
	 */
	PR_FormatTimeUSEnglish(buffer_tail, 100,
						   "Date: %a, %d %b %Y %H:%M:%S ",
						   &now);
#endif

	buffer_tail += XP_STRLEN (buffer_tail);
	PR_snprintf(buffer_tail, buffer + size - buffer_tail,
				"%c%02d%02d" CRLF,
				(gmtoffset >= 0 ? '+' : '-'),
				((gmtoffset >= 0 ? gmtoffset : -gmtoffset) / 60),
				((gmtoffset >= 0 ? gmtoffset : -gmtoffset) % 60));
	buffer_tail += XP_STRLEN (buffer_tail);
  }

  if (fields->GetFrom() && *fields->GetFrom())
	{
	  char *convbuf;
  	  PUSH_STRING ("From: ");
	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetFrom(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)     /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (fields->GetFrom());
      PUSH_NEWLINE ();
	}

  if (fields->GetReplyTo() && *fields->GetReplyTo())
	{
	  char *convbuf;
	  PUSH_STRING ("Reply-To: ");
	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetReplyTo(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)     /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (fields->GetReplyTo());
	  PUSH_NEWLINE ();
	}

  if (fields->GetOrganization() && *fields->GetOrganization())
	{
	  char *convbuf;
	  PUSH_STRING ("Organization: ");
	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetOrganization(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)     /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (fields->GetOrganization());
	  PUSH_NEWLINE ();
	}

  // X-Sender tag
  if (fields->GetOwner())
  {
	XP_Bool bUseXSender = FALSE;
	
	PREF_GetBoolPref("mail.use_x_sender", &bUseXSender);
	if (bUseXSender) {
	  char *convbuf;
	  char tmpBuffer[256];
	  int bufSize = 256;
	  
	  *tmpBuffer = 0;

  	  PUSH_STRING ("X-Sender: ");

	  PUSH_STRING("\"");

	  PREF_GetCharPref("mail.identity.username", tmpBuffer, &bufSize);
	  convbuf = IntlEncodeMimePartIIStr((char *)tmpBuffer, csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)     /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (tmpBuffer);

	  PUSH_STRING("\" <");

	  PREF_GetCharPref("mail.pop_name", tmpBuffer, &bufSize);
	  convbuf = IntlEncodeMimePartIIStr((char *)tmpBuffer, csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)     /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (tmpBuffer);

	  PUSH_STRING ("@");

	  PREF_GetCharPref("network.hosts.pop_server", tmpBuffer, &bufSize);
	  convbuf = IntlEncodeMimePartIIStr((char *)tmpBuffer, csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)     /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (tmpBuffer);

	  PUSH_STRING(">");

	  convbuf = IntlEncodeMimePartIIStr((char *)tmpBuffer, csid,
										mime_headers_use_quoted_printable_p);

	  if (!fields->GetOwner()->GetMaster()->IsUserAuthenticated())
		  PUSH_STRING (" (Unverified)");
      PUSH_NEWLINE ();
	}
  }
  // X-Mozilla-Draft-Info
  if (isDraft) {
	  char *htmlAction = 0;
	  char *lineWidth = 0; // force plain text hard line break info

	  PUSH_STRING(X_MOZILLA_DRAFT_INFO);
	  PUSH_STRING(": internal/draft; ");
	  if (fields->GetAttachVCard()) {
		  PUSH_STRING("vcard=1");
	  }
	  else {
		  PUSH_STRING("vcard=0");
	  }
	  PUSH_STRING("; ");
	  if (fields->GetReturnReceipt()) {
		  char *type = PR_smprintf("%d", (int) fields->GetReturnReceiptType());
		  if (type)
		  {
			  PUSH_STRING("receipt=");
			  PUSH_STRING(type);
			  XP_FREEIF(type);
		  }
	  }
	  else {
		  PUSH_STRING("receipt=0");
	  }
	  PUSH_STRING("; ");
	  if (fields->GetBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK)) {
		  PUSH_STRING("uuencode=1");
	  }
	  else {
		  PUSH_STRING("uuencode=0");
	  }

	  htmlAction = PR_smprintf("html=%d", 
		  ((MSG_CompositionPane*)fields->GetOwner())->GetHTMLAction());
	  if (htmlAction)
	  {
	  	PUSH_STRING("; ");
	  	PUSH_STRING(htmlAction);
	  	FREEIF(htmlAction);
	  }

	  lineWidth = PR_smprintf("; linewidth=%d",
		  ((MSG_CompositionPane*)fields->GetOwner())->GetLineWidth());
	  if (lineWidth)
	  {
		  PUSH_STRING(lineWidth);
		  FREEIF(lineWidth);
	  }
	  PUSH_NEWLINE ();
  }

  PUSH_STRING ("X-Mailer: ");
  PUSH_STRING (XP_AppCodeName);
  PUSH_STRING (" ");
  PUSH_STRING (XP_AppVersion);
  PUSH_NEWLINE ();

  /* for Netscape Server, Accept-Language data sent in Mail header */
  char *acceptlang = INTL_GetAcceptLanguage();
  if( (acceptlang != NULL) && ( *acceptlang != '\0') ){
	  PUSH_STRING( "X-Accept-Language: " );
	  PUSH_STRING( acceptlang );
	  PUSH_NEWLINE();
  }

  PUSH_STRING ("MIME-Version: 1.0" CRLF);

  if (fields->GetNewsgroups() && *fields->GetNewsgroups())
	{
      /* turn whitespace into a comma list
       */
	  char *ptr, *ptr2;
	  char *n2;
	  char *convbuf;

	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetNewsgroups(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)
	  	n2 = XP_StripLine (convbuf);
	  else {
		ptr = XP_STRDUP(fields->GetNewsgroups());
		if (!ptr)
		  {
			FREEIF(buffer);
			return 0; /* MK_OUT_OF_MEMORY */
		  }
  	  	n2 = XP_StripLine(ptr);
		XP_ASSERT(n2 == ptr);	/* Otherwise, the XP_FREE below is
								   gonna choke badly. */
	  }

      for(ptr=n2; *ptr != '\0'; ptr++)
        {
          /* find first non white space */
          while(!XP_IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
              ptr++;

          if(*ptr == '\0')
              break;

          if(*ptr != ',')
              *ptr = ',';

          /* find next non white space */
          ptr2 = ptr+1;
          while(XP_IS_SPACE(*ptr2))
              ptr2++;

          if(ptr2 != ptr+1)
              XP_STRCPY(ptr+1, ptr2);
        }

	  PUSH_STRING ("Newsgroups: ");
	  PUSH_STRING (n2);
	  XP_FREE (n2);
	  PUSH_NEWLINE ();
	}

  /* #### shamelessly duplicated from above */
  if (fields->GetFollowupTo() && *fields->GetFollowupTo())
	{
      /* turn whitespace into a comma list
       */
	  char *ptr, *ptr2;
	  char *n2;
	  char *convbuf;

	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetFollowupTo(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)
	  	n2 = XP_StripLine (convbuf);
	  else {
		ptr = XP_STRDUP(fields->GetFollowupTo());
		if (!ptr)
		  {
			FREEIF(buffer);
			return 0; /* MK_OUT_OF_MEMORY */
		  }
  	  	n2 = XP_StripLine (ptr);
		XP_ASSERT(n2 == ptr);	/* Otherwise, the XP_FREE below is
								   gonna choke badly. */
	  }

      for(ptr=n2; *ptr != '\0'; ptr++)
        {
          /* find first non white space */
          while(!XP_IS_SPACE(*ptr) && *ptr != ',' && *ptr != '\0')
              ptr++;

          if(*ptr == '\0')
              break;

          if(*ptr != ',')
              *ptr = ',';

          /* find next non white space */
          ptr2 = ptr+1;
          while(XP_IS_SPACE(*ptr2))
              ptr2++;

          if(ptr2 != ptr+1)
              XP_STRCPY(ptr+1, ptr2);
        }

	  PUSH_STRING ("Followup-To: ");
	  PUSH_STRING (n2);
	  XP_FREE (n2);
	  PUSH_NEWLINE ();
	}

  if (fields->GetTo() && *fields->GetTo())
	{
	  char *convbuf;
	  PUSH_STRING ("To: ");
	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetTo(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)     /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (fields->GetTo());

	  PUSH_NEWLINE ();
	}
  if (fields->GetCc() && *fields->GetCc())
	{
	  char *convbuf;
	  PUSH_STRING ("CC: ");
	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetCc(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)   /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (fields->GetCc());
	  PUSH_NEWLINE ();
	}
  if (fields->GetSubject() && *fields->GetSubject())
	{
	  char *convbuf;
	  PUSH_STRING ("Subject: ");
	  convbuf = IntlEncodeMimePartIIStr((char *)fields->GetSubject(), csid,
										mime_headers_use_quoted_printable_p);
	  if (convbuf)  /* MIME-PartII conversion */
	    {
	  	  PUSH_STRING (convbuf);
	  	  XP_FREE(convbuf);
	    }
	  else
	    PUSH_STRING (fields->GetSubject());
	  PUSH_NEWLINE ();
	}
  if (fields->GetPriority() && *(fields->GetPriority()))
	{
	  char *priority = (char *) fields->GetPriority();

	  if (!strcasestr(priority, "normal"))
	  {
		  PUSH_STRING ("X-Priority: ");
		  /* Important: do not change the order of the 
		   * following if statements
		   */
		  if (strcasestr (priority, "highest"))
			  PUSH_STRING("1 (");
		  else if (strcasestr(priority, "high"))
			  PUSH_STRING("2 (");
		  else if (strcasestr(priority, "lowest"))
			  PUSH_STRING("5 (");
		  else if (strcasestr(priority, "low"))
			  PUSH_STRING("4 (");

		  PUSH_STRING (priority);
		  PUSH_STRING(")");
		  PUSH_NEWLINE ();
	  }
	}
  if (fields->GetReferences() && *fields->GetReferences())
	{
	  PUSH_STRING ("References: ");
	  PUSH_STRING (fields->GetReferences());
	  PUSH_NEWLINE ();
	}

  if (fields->GetOtherRandomHeaders() && *fields->GetOtherRandomHeaders())
	{
	  /* Assume they already have the right newlines and continuations
		 and so on. */
	  PUSH_STRING (fields->GetOtherRandomHeaders());
	}

  if (buffer_tail > buffer + size - 1)
	abort ();

  /* realloc it smaller... */
  buffer = (char*)XP_REALLOC (buffer, buffer_tail - buffer + 1);

  return buffer;
}


/* Generate headers for a form post to a mailto: URL.
   This lets the URL specify additional headers, but is careful to
   ignore headers which would be dangerous.  It may modify the URL
   (because of CC) so a new URL to actually post to is returned.
 */
int
MIME_GenerateMailtoFormPostHeaders (const char *old_post_url,
									const char * /*referer*/,
									char **new_post_url_return,
									char **headers_return)
{
  char *from = 0, *to = 0, *cc = 0, *body = 0, *search = 0;
  char *extra_headers = 0;
  char *s;
  XP_Bool subject_p = FALSE;
  XP_Bool sign_p = FALSE;
  HG29292
  char *rest;
  int status = 0;
  MSG_CompositionFields *fields = NULL;
  static const char *forbidden_headers[] = {
	"Apparently-To",
	"BCC",
	"Content-Encoding",
	CONTENT_LENGTH,
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
	X_MOZILLA_STATUS,
	X_MOZILLA_STATUS2,
	X_MOZILLA_NEWSHOST,
	X_UIDL,
	"XRef",
	0 };

  from = MIME_MakeFromField ();
  if (!from) {
	status = MK_OUT_OF_MEMORY;
	goto FAIL;
  }

  to = NET_ParseURL (old_post_url, GET_PATH_PART);
  if (!to) {
	status = MK_OUT_OF_MEMORY;
	goto FAIL;
  }

  if (!*to)
	{
	  status = MK_MIME_NO_RECIPIENTS; /* rb -1; */
	  goto FAIL;
	}

  search = NET_ParseURL (old_post_url, GET_SEARCH_PART);

  rest = search;
  if (rest && *rest == '?')
	{
	  /* start past the '?' */
	  rest++;
	  rest = XP_STRTOK (rest, "&");
	  while (rest && *rest)
		{
		  char *token = rest;
		  char *value = 0;
		  char *eq;

		  rest = XP_STRTOK (0, "&");

		  eq = XP_STRCHR (token, '=');
		  if (eq)
			{
			  value = eq+1;
			  *eq = 0;
			}

		  if (!strcasecomp (token, "subject"))
			subject_p = TRUE;

		  if (value)
			/* Don't allow newlines or control characters in the value. */
			for (s = value; *s; s++)
			  if (*s < ' ' && *s != '\t')
				*s = ' ';

		  if (!strcasecomp (token, "to"))
			{
			  if (to && *to)
				{
				  StrAllocCat (to, ", ");
				  StrAllocCat (to, value);
				}
			  else
				{
				  StrAllocCopy (to, value);
				}
			}
		  else if (!strcasecomp (token, "cc"))
			{
			  if (cc && *cc)
				{
				  StrAllocCat (cc, ", ");
				  StrAllocCat (cc, value);
				}
			  else
				{
				  StrAllocCopy (cc, value);
				}
			}
		  else if (!strcasecomp (token, "body"))
			{
			  if (body && *body)
				{
				  StrAllocCat (body, "\n");
				  StrAllocCat (body, value);
				}
			  else
				{
				  StrAllocCopy (body, value);
				}
			}
		  HG28926
		  else if (!strcasecomp (token, "sign") ||
				   !strcasecomp (token, "signed"))
			{
			  sign_p = (!strcasecomp(value, "true") ||
						!strcasecomp(value, "yes"));
			}
		  else
			{
			  const char **fh = forbidden_headers;
			  XP_Bool ok = TRUE;
			  while (*fh)
				if (!strcasecomp (token, *fh++))
				  {
					ok = FALSE;
					break;
				  }
			  if (ok)
				{
				  XP_Bool upper_p = FALSE;
				  char *s;
				  for (s = token; *s; s++)
					{
					  if (*s >= 'A' && *s <= 'Z')
						upper_p = TRUE;
					  else if (*s <= ' ' || *s >= '~' || *s == ':')
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
	  /* If the URL didn't provide a subject, we will. */
	  StrAllocCat (extra_headers, "Subject: Form posted from ");
	  XP_ASSERT (XP_AppCodeName);
	  StrAllocCat (extra_headers, XP_AppCodeName);
	  StrAllocCat (extra_headers, CRLF);
	}

  /* Note: the `sign', and `body' parameters are currently
	 ignored in mailto form submissions. */

  *new_post_url_return = 0;

  fields = MSG_CreateCompositionFields(from, 0, to, cc, 0, 0, 0, 0,
									   FE_UsersOrganization(), 0, 0,
									   extra_headers, 0, 0, 0
									   HG15448);
  if (!fields)
  {
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
  }

  fields->SetDefaultBody(body);

  *headers_return = mime_generate_headers (fields, 0, MSG_DeliverNow);
  if (*headers_return == 0)
	{
	  status = MK_OUT_OF_MEMORY;
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
  FREEIF (from);
  FREEIF (to);
  FREEIF (cc);
  FREEIF (body);
  FREEIF (search);
  FREEIF (extra_headers);
  if (fields)
	  MSG_DestroyCompositionFields(fields);

  return status;
}



static char *
mime_generate_attachment_headers (const char *type, const char *encoding,
								  const char *description,
								  const char *x_mac_type,
								  const char *x_mac_creator,
								  const char *real_name,
								  const char *base_url,
								  XP_Bool /*digest_p*/,
								  MSG_DeliverMimeAttachment * /*ma*/,
								  int16 mail_csid)
{
  char *buffer = (char *) XP_ALLOC (2048);
  char *buffer_tail = buffer;
  char charset[30];

  if (! buffer)
	return 0; /* MK_OUT_OF_MEMORY */

  XP_ASSERT (encoding);
  charset[0] = 0;

  PUSH_STRING ("Content-Type: ");
  PUSH_STRING (type);

  if (mime_type_needs_charset (type))
	{

	  /* push 7bit encoding out based on current default codeset */
	  INTL_CharSetIDToName (mail_csid, charset);

	  /* If the characters are all 7bit, then it's better (and true) to
		 claim the charset to be US-ASCII rather than Latin1.  Should we
		 do this all the time, for all charsets?  I'm not sure.  But we
		 should definitely do it for Latin1. */
	  if (encoding &&
		  !strcasecomp (encoding, "7bit") &&
		  !strcasecomp (charset, "iso-8859-1"))
		XP_STRCPY (charset, "us-ascii");

	  // If csid is JIS and and type is HTML
	  // then no charset to be specified (apply base64 instead)
	  // in order to avoid mismatch META_TAG (bug#104255).
	  if ((mail_csid != CS_JIS) ||
	     (strcasecomp(type, TEXT_HTML) != 0) ||
		 (strcasecomp(encoding, ENCODING_BASE64) != 0))
		{
		PUSH_STRING ("; charset=");
		PUSH_STRING (charset);
		}
	}

  if (x_mac_type && *x_mac_type)
	{
	  PUSH_STRING ("; x-mac-type=\"");
	  PUSH_STRING (x_mac_type);
	  PUSH_STRING ("\"");
	}
  if (x_mac_creator && *x_mac_creator)
	{
	  PUSH_STRING ("; x-mac-creator=\"");
	  PUSH_STRING (x_mac_creator);
	  PUSH_STRING ("\"");
	}

  int32 parmFolding = 0;
  PREF_GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

#ifdef EMIT_NAME_IN_CONTENT_TYPE
  if (real_name && *real_name)
	{
	  if (parmFolding == 0 || parmFolding == 1)
	  {
		  PUSH_STRING (";\r\n name=\"");
		  PUSH_STRING (real_name);
		  PUSH_STRING ("\"");
	  }
	  else // if (parmFolding == 2)
	  {
		  char *rfc2231Parm = RFC2231ParmFolding("name", charset,
												 INTL_GetAcceptLanguage(), real_name);
		  if (rfc2231Parm)
		  {
			  PUSH_STRING(";\r\n ");
			  PUSH_STRING(rfc2231Parm);
			  XP_FREE(rfc2231Parm);
		  }
	  }
	}
#endif /* EMIT_NAME_IN_CONTENT_TYPE */

  PUSH_NEWLINE ();

  PUSH_STRING ("Content-Transfer-Encoding: ");
  PUSH_STRING (encoding);
  PUSH_NEWLINE ();

  if (description && *description)
	{
	  char *s = mime_fix_header (description);
	  if (s)
		{
		  PUSH_STRING ("Content-Description: ");
		  PUSH_STRING (s);
		  PUSH_NEWLINE ();
		  XP_FREE(s);
		}
	}

  if (real_name && *real_name)
	{
	  char *period = XP_STRRCHR(real_name, '.');
	  int32 pref_content_disposition = 0;

	  PREF_GetIntPref("mail.content_disposition_type", 
					  &pref_content_disposition);

	  PUSH_STRING ("Content-Disposition: ");
	 
	  if (pref_content_disposition == 1)
		PUSH_STRING ("attachment");
	  else if (pref_content_disposition == 2 && 
			   (!strcasecomp(type, TEXT_PLAIN) || 
				(period && !strcasecomp(period, ".txt"))))
		PUSH_STRING("attachment");
	  /* If this document is an anonymous binary file or a vcard, 
		 then always show it as an attachment, never inline. */
	  else if (!strcasecomp(type, APPLICATION_OCTET_STREAM) || 
		   !strcasecomp(type, vCardMimeFormat))
	    PUSH_STRING ("attachment");
	  else
		PUSH_STRING ("inline");

	  if (parmFolding == 0 || parmFolding == 1)
	  {
		  PUSH_STRING (";\r\n filename=\"");
		  PUSH_STRING (real_name);
		  PUSH_STRING ("\"" CRLF);
	  }
	  else // if (parmFolding == 2)
	  {
		  char *rfc2231Parm = RFC2231ParmFolding("name", charset,
												 INTL_GetAcceptLanguage(), real_name);
		  if (rfc2231Parm)
		  {
			  PUSH_STRING(";\r\n ");
			  PUSH_STRING(rfc2231Parm);
			  PUSH_NEWLINE ();
			  XP_FREE(rfc2231Parm);
		  }
	  }
	}
  else if (type &&
	       (!strcasecomp (type, MESSAGE_RFC822) ||
			!strcasecomp (type, MESSAGE_NEWS)))
	{
	  PUSH_STRING ("Content-Disposition: inline" CRLF);
	}
  
#ifdef GENERATE_CONTENT_BASE
  /* If this is an HTML document, and we know the URL it originally
	 came from, write out a Content-Base header. */
  if (type &&
	  (!strcasecomp (type, TEXT_HTML) ||
	   !strcasecomp (type, TEXT_MDL)) &&
	  base_url && *base_url)
	{
	  int32 col = 0;
	  const char *s = base_url;
	  const char *colon = XP_STRCHR (s, ':');
    XP_Bool useContentLocation = FALSE;   /* rhp - add this  */

	  if (!colon) goto GIVE_UP_ON_CONTENT_BASE;  /* malformed URL? */

	  /* Don't emit a content-base that points to (or into) a news or
		 mail message. */
	  if (!strncasecomp (s, "news:", 5) ||
		  !strncasecomp (s, "snews:", 6) ||
		  !strncasecomp (s, "IMAP:", 5) ||
		  !strncasecomp (s, "mailbox:", 8))
		goto GIVE_UP_ON_CONTENT_BASE;

    /* rhp - Put in a pref for using Content-Location instead of Content-Base.
             This will get tweaked to default to true in 5.0
     */
  	PREF_GetBoolPref("mail.use_content_location_on_send", &useContentLocation);
    
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
		  if (col >= 38)
			{
			  PUSH_STRING(CRLF "\t");
			  col = 0;
			}

		  if (*s == ' ')       PUSH_STRING("%20");
		  else if (*s == '\t') PUSH_STRING("%09");
		  else if (*s == '\n') PUSH_STRING("%0A");
		  else if (*s == '\r') PUSH_STRING("%0D");
		  else
			{
			  *buffer_tail++ = *s;
			  *buffer_tail = '\0';
			}
		  s++;
		  col += (buffer_tail - ot);
		}
	  PUSH_STRING ("\"" CRLF);

    /* rhp: this is to try to get around this fun problem with Content-Location */
    if (!useContentLocation)
    {
      PUSH_STRING ("Content-Location: \"");
      s = base_url;
      col = 0;
      useContentLocation = TRUE;
      goto CONTENT_LOC_HACK;
    }
    /* rhp: this is to try to get around this fun problem with Content-Location */

	GIVE_UP_ON_CONTENT_BASE:
	  ;
	}
#endif /* GENERATE_CONTENT_BASE */


  /* realloc it smaller... */
  buffer = (char*) XP_REALLOC (buffer, buffer_tail - buffer + 1);

  return buffer;
}


void MSG_SendMimeDeliveryState::Fail (int failure_code, char *error_msg)
{
  if (m_message_delivery_done_callback)
	{
	  if (failure_code < 0 && !error_msg)
		error_msg = NET_ExplainErrorDetails(failure_code, 0, 0, 0, 0);
	  m_message_delivery_done_callback (GetContext(), m_fe_data,
										failure_code, error_msg);
	  FREEIF(error_msg);		/* #### Is there a memory leak here?  Shouldn't
								   this free be outside the if? */
	}
  else if (m_attachments_done_callback)
	{
	  if (failure_code < 0 && !error_msg)
		error_msg = NET_ExplainErrorDetails(failure_code, 0, 0, 0, 0);

	  /* mime_free_message_state will take care of cleaning up the
		 attachment files and attachment structures */
	  m_attachments_done_callback (GetContext(),
								   m_fe_data, failure_code,
								   error_msg, 0);

	  FREEIF(error_msg);		/* #### Is there a memory leak here?  Shouldn't
								   this free be outside the if? */
	}

  m_message_delivery_done_callback = 0;
  m_attachments_done_callback = 0;

  Clear();
}

/* Given a string, convert it to 'qtext' (quoted text) for RFC822 header purposes. */
static char *
msg_make_filename_qtext(const char *srcText, XP_Bool stripCRLFs)
{
	/* newString can be at most twice the original string (every char quoted). */
	char *newString = (char *) XP_ALLOC(XP_STRLEN(srcText)*2 + 1);
	if (!newString) return NULL;

	const char *s = srcText;
	const char *end = srcText + XP_STRLEN(srcText);
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
						   (*(s+1) == LF && (s+2) < end && !XP_IS_SPACE(*(s+2)))))))
			*d++ = '\\';

		if (*s == CR)
		{
			if (stripCRLFs && *(s+1) == LF && (s+2) < end && XP_IS_SPACE(*(s+2)))
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
static void
msg_pick_real_name (MSG_DeliverMimeAttachment *attachment, int16 csid)
{
  const char *s, *s2;
  char *s3;
  char *url;

  if (attachment->m_real_name)
	return;

  url = attachment->m_url_string;

  /* Perhaps the MIME parser knows a better name than the URL itself?
	 This can happen when one attaches a MIME part from one message
	 directly into another message.
	 
	 ### mwelch Note that this function simply duplicates and returns an existing
	 			MIME header, so we don't need to process it. */
  attachment->m_real_name =
	MimeGuessURLContentName(attachment->m_mime_delivery_state->GetContext(),
							url);
  if (attachment->m_real_name)
	return;

  /* Otherwise, extract a name from the URL. */

  s = url;
  s2 = XP_STRCHR (s, ':');
  if (s2) s = s2 + 1;
  /* If we know the URL doesn't have a sensible file name in it,
	 don't bother emitting a content-disposition. */
  if (!strncasecomp (url, "news:", 5) ||
	  !strncasecomp (url, "snews:", 6) ||
	  !strncasecomp (url, "IMAP:", 5) ||
	  !strncasecomp (url, "mailbox:", 8))
	return;

  /* Take the part of the file name after the last / or \ */
  s2 = XP_STRRCHR (s, '/');
  if (s2) s = s2+1;
  s2 = XP_STRRCHR (s, '\\');
  
  if (csid & MULTIBYTE)
  {
	  // We don't want to truncate the file name in case of the double
	  // byte file name
	  while ( s2 != NULL && 
			  s2 > s &&
			  INTL_IsLeadByte(csid, *(s2-1)))
	  {
		  s3 = (char *) s2;
		  *s3 = 0;
		  s2 = XP_STRRCHR(s, '\\');
		  *s3 = '\\';
	  }
  }
	  
  if (s2) s = s2+1;
  /* Copy it into the attachment struct. */
  StrAllocCopy (attachment->m_real_name, s);
  /* Now trim off any named anchors or search data. */
  s3 = XP_STRCHR (attachment->m_real_name, '?');
  if (s3) *s3 = 0;
  s3 = XP_STRCHR (attachment->m_real_name, '#');
  if (s3) *s3 = 0;

  /* Now lose the %XX  */
  NET_UnEscape (attachment->m_real_name);

  int32 parmFolding = 0;
  PREF_GetIntPref("mail.strictly_mime.parm_folding", &parmFolding);

  if (parmFolding == 0 || parmFolding == 1)
  {
	  /* Try to MIME-2 encode the filename... */
	  char *mime2Name = IntlEncodeMimePartIIStr(attachment->m_real_name, csid, 
												mime_headers_use_quoted_printable_p);
	  if (mime2Name && (mime2Name != attachment->m_real_name))
	  {
		  XP_FREE(attachment->m_real_name);
		  attachment->m_real_name = mime2Name;
	  }
   
	  /* ... and then put backslashes before special characters (RFC 822 tells us
		 to). */

	  char *qtextName = NULL;
	  
	  qtextName = msg_make_filename_qtext(attachment->m_real_name, 
										  (parmFolding == 0 ? TRUE : FALSE));
		  
	  if (qtextName)
	  {
		  XP_FREE(attachment->m_real_name);
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
	  int32 L = XP_STRLEN(result);
	  const char **exts = 0;

	  /* #### hack
		 I'd like to ask the mime.types file, "what extensions correspond
		 to obj->encoding (which happens to be "x-uuencode") but doing that
		 in a non-sphagetti way would require brain surgery.  So, since
		 currently uuencode is the only content-transfer-encoding which we
		 understand which traditionally has an extension, we just special-
		 case it here!  

		 Note that it's special-cased in a similar way in libmime/mimei.c.
	   */
	  if (!strcasecomp(attachment->m_encoding, ENCODING_UUENCODE) ||
		  !strcasecomp(attachment->m_encoding, ENCODING_UUENCODE2) ||
		  !strcasecomp(attachment->m_encoding, ENCODING_UUENCODE3) ||
		  !strcasecomp(attachment->m_encoding, ENCODING_UUENCODE4))
		{
		  static const char *uue_exts[] = { "uu", "uue", 0 };
		  exts = uue_exts;
		}

	  while (exts && *exts)
		{
		  const char *ext = *exts;
		  int32 L2 = XP_STRLEN(ext);
		  if (L > L2 + 1 &&							/* long enough */
			  result[L - L2 - 1] == '.' &&			/* '.' in right place*/
			  !strcasecomp(ext, result + (L - L2)))	/* ext matches */
			{
			  result[L - L2 - 1] = 0;		/* truncate at '.' and stop. */
			  break;
			}
		  exts++;
		}
	}
}


int
MSG_SendMimeDeliveryState::HackAttachments(
						  const struct MSG_AttachmentData *attachments,
						  const struct MSG_AttachedFile *preloaded_attachments)
{
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(GetContext());
  if (preloaded_attachments) XP_ASSERT(!attachments);
  if (attachments) XP_ASSERT(!preloaded_attachments);

  if (preloaded_attachments && preloaded_attachments[0].orig_url)
	{
	  /* These are attachments which have already been downloaded to tmp files.
		 We merely need to point the internal attachment data at those tmp
		 files.
	   */
	  int32 i;

	  m_pre_snarfed_attachments_p = TRUE;

	  m_attachment_count = 0;
	  while (preloaded_attachments[m_attachment_count].orig_url)
		m_attachment_count++;

	  m_attachments = (MSG_DeliverMimeAttachment *)
		new MSG_DeliverMimeAttachment[m_attachment_count];

	  if (! m_attachments)
		return MK_OUT_OF_MEMORY;

	  for (i = 0; i < m_attachment_count; i++)
		{
		  m_attachments[i].m_mime_delivery_state = this;
		  /* These attachments are already "snarfed". */
		  m_attachments[i].m_done = TRUE;
		  XP_ASSERT (preloaded_attachments[i].orig_url);
		  StrAllocCopy (m_attachments[i].m_url_string,
						preloaded_attachments[i].orig_url);
		  StrAllocCopy (m_attachments[i].m_type,
						preloaded_attachments[i].type);
		  StrAllocCopy (m_attachments[i].m_description,
						preloaded_attachments[i].description);
		  StrAllocCopy (m_attachments[i].m_real_name,
						preloaded_attachments[i].real_name);
		  StrAllocCopy (m_attachments[i].m_x_mac_type,
						preloaded_attachments[i].x_mac_type);
		  StrAllocCopy (m_attachments[i].m_x_mac_creator,
						preloaded_attachments[i].x_mac_creator);
		  StrAllocCopy (m_attachments[i].m_encoding,
						preloaded_attachments[i].encoding);
		  StrAllocCopy (m_attachments[i].m_file_name,
						preloaded_attachments[i].file_name);

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
			  strcasecomp (m_attachments[i].m_encoding, ENCODING_7BIT) &&
			  strcasecomp (m_attachments[i].m_encoding, ENCODING_8BIT) &&
			  strcasecomp (m_attachments[i].m_encoding, ENCODING_BINARY))
			m_attachments[i].m_already_encoded_p = TRUE;

		  msg_pick_real_name(&m_attachments[i], INTL_GetCSIWinCSID(c));
		}
	}
  else if (attachments && attachments[0].url)
	{
	  /* These are attachments which have already been downloaded to tmp files.
		 We merely need to point the internal attachment data at those tmp
		 files.  We will delete the tmp files as we attach them.
	   */
	  int32 i;
	  int mailbox_count = 0, news_count = 0;

	  m_attachment_count = 0;
	  while (attachments[m_attachment_count].url)
		m_attachment_count++;

	  m_attachments = (MSG_DeliverMimeAttachment *)
		new MSG_DeliverMimeAttachment[m_attachment_count];

	  if (! m_attachments)
		return MK_OUT_OF_MEMORY;

	  for (i = 0; i < m_attachment_count; i++)
		{
		  m_attachments[i].m_mime_delivery_state = this;
		  XP_ASSERT (attachments[i].url);
		  StrAllocCopy (m_attachments[i].m_url_string,
						attachments[i].url);
		  StrAllocCopy (m_attachments[i].m_override_type,
						attachments[i].real_type);
		  StrAllocCopy (m_attachments[i].m_override_encoding,
						attachments[i].real_encoding);
		  StrAllocCopy (m_attachments[i].m_desired_type,
						attachments[i].desired_type);
		  StrAllocCopy (m_attachments[i].m_description,
						attachments[i].description);
		  StrAllocCopy (m_attachments[i].m_real_name,
						attachments[i].real_name);
		  StrAllocCopy (m_attachments[i].m_x_mac_type,
						attachments[i].x_mac_type);
		  StrAllocCopy (m_attachments[i].m_x_mac_creator,
						attachments[i].x_mac_creator);
		  StrAllocCopy (m_attachments[i].m_encoding, "7bit");
		  m_attachments[i].m_url =
			NET_CreateURLStruct (m_attachments[i].m_url_string,
								 NET_DONT_RELOAD);

		  // real name is set in the case of vcard so don't change it.
		  // m_attachments[i].m_real_name = 0;

		  /* Count up attachments which are going to come from mail folders
			 and from NNTP servers. */
		  if (strncasecomp(m_attachments[i].m_url_string, "mailbox:",8) ||
			  strncasecomp(m_attachments[i].m_url_string, "IMAP:",5))
			mailbox_count++;
		  else if (strncasecomp(m_attachments[i].m_url_string, "news:",5) ||
				   strncasecomp(m_attachments[i].m_url_string, "snews:",6))
			news_count++;

		  msg_pick_real_name(&m_attachments[i], INTL_GetCSIWinCSID(c));
		}

	  /* If there is more than one mailbox URL, or more than one NNTP url,
		 do the load in serial rather than parallel, for efficiency.
	   */
	  if (mailbox_count > 1 || news_count > 1)
		m_be_synchronous_p = TRUE;

	  m_attachment_pending_count = m_attachment_count;

	  /* Start the URL attachments loading (eventually, an exit routine will
		 call the done_callback). */

	  if (m_attachment_count == 1)
		FE_Progress(GetContext(), XP_GetString(MK_MSG_LOAD_ATTACHMNT));
	  else
		FE_Progress(GetContext(), XP_GetString(MK_MSG_LOAD_ATTACHMNTS));

	  for (i = 0; i < m_attachment_count; i++)
		{
		  /* This only returns a failure code if NET_GetURL was not called
			 (and thus no exit routine was or will be called.) */
		  int status = m_attachments [i].SnarfAttachment ();
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


void
MSG_SendMimeDeliveryState::StartMessageDelivery(
						  MSG_Pane   *pane,
						  void       *fe_data,
						  MSG_CompositionFields *fields,
						  XP_Bool digest_p,
						  XP_Bool dont_deliver_p,
						  MSG_Deliver_Mode mode,
						  const char *attachment1_type,
						  const char *attachment1_body,
						  uint32 attachment1_body_length,
						  const struct MSG_AttachmentData *attachments,
						  const struct MSG_AttachedFile *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
						  MSG_SendPart *relatedPart,
//#endif
						  void (*message_delivery_done_callback)
						       (MWContext *context,
								void *fe_data,
								int status,
								const char *error_message))
{
  int failure = 0;
  MSG_SendMimeDeliveryState	*state;

  if (!attachment1_body || !*attachment1_body)
	attachment1_type = attachment1_body = 0;

  state = new MSG_SendMimeDeliveryState;
  if (! state)
	{
	  failure = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}

  failure = state->Init(pane, fe_data, fields, 
						digest_p, dont_deliver_p, mode,
						attachment1_type, attachment1_body,
						attachment1_body_length,
						attachments, preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
						relatedPart,
//#endif
						message_delivery_done_callback);
  if (failure >= 0)
	return;

FAIL:
  char *err_msg = NET_ExplainErrorDetails (failure);
  message_delivery_done_callback (pane->GetContext(), fe_data, failure,
								  err_msg);
  if (err_msg) XP_FREE (err_msg);
  delete state;
}

int MSG_SendMimeDeliveryState::SetMimeHeader(MSG_HEADER_SET header, const char *value)
{
	char	*dupHeader = NULL;
	int		ret = MK_OUT_OF_MEMORY;

	if (header & (MSG_FROM_HEADER_MASK | MSG_TO_HEADER_MASK | MSG_REPLY_TO_HEADER_MASK | MSG_CC_HEADER_MASK | MSG_BCC_HEADER_MASK))
		dupHeader = mime_fix_addr_header(value);
	else if (header &  (MSG_NEWSGROUPS_HEADER_MASK| MSG_FOLLOWUP_TO_HEADER_MASK))
		dupHeader = mime_fix_news_header(value);
	else  if (header & (MSG_FCC_HEADER_MASK | MSG_ORGANIZATION_HEADER_MASK |  MSG_SUBJECT_HEADER_MASK | MSG_REFERENCES_HEADER_MASK | MSG_X_TEMPLATE_HEADER_MASK))
		dupHeader = mime_fix_header(value);
	else
		XP_ASSERT(FALSE);	// unhandled header mask

	if (dupHeader)
	{
		ret = m_fields->SetHeader(header, dupHeader);
		XP_FREE(dupHeader);
	}
	return ret;
}



int
MSG_SendMimeDeliveryState::Init(
						  MSG_Pane *pane,
						  void      *fe_data,
						  MSG_CompositionFields *fields,
						  XP_Bool digest_p,
						  XP_Bool dont_deliver_p,
						  MSG_Deliver_Mode mode,

						  const char *attachment1_type,
						  const char *attachment1_body,
						  uint32 attachment1_body_length,
						  const struct MSG_AttachmentData *attachments,
						  const struct MSG_AttachedFile *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
						  MSG_SendPart *relatedPart,
//#endif
						  void (*message_delivery_done_callback)
						       (MWContext *context,
								void *fe_data,
								int status,
								const char *error_message))
{
  int failure = 0;
  m_pane = pane;
  m_fe_data = fe_data;
  m_message_delivery_done_callback = message_delivery_done_callback;

//#ifdef MSG_SEND_MULTIPART_RELATED
  m_related_part = relatedPart;
  if (m_related_part)
	  m_related_part->SetMimeDeliveryState(this);
//#endif

  XP_ASSERT (fields);
  if (!fields) return MK_OUT_OF_MEMORY; /* rb -1; */

  if (m_fields)
  {
	  delete m_fields;
  }

  m_fields = new MSG_CompositionFields;
  if (!m_fields)
	  return MK_OUT_OF_MEMORY;

  m_fields->SetOwner(pane);

#ifdef GENERATE_MESSAGE_ID
  if (fields->GetMessageId())
	{
	  m_fields->SetMessageId(XP_STRDUP(fields->GetMessageId()));
	  /* Don't bother checking for out of memory; if it fails, then we'll just
		 let the server generate the message-id, and suffer with the
		 possibility of duplicate messages.*/
	}
#endif /* GENERATE_MESSAGE_ID */

  /* Strip whitespace from beginning and end of body. */
  if (attachment1_body)
	{
	  while (attachment1_body_length > 0 &&
			 XP_IS_SPACE (*attachment1_body))
		{
		  attachment1_body++;
		  attachment1_body_length--;
		}
	  while (attachment1_body_length > 0 &&
			 XP_IS_SPACE (attachment1_body [attachment1_body_length - 1]))
		{
		  attachment1_body_length--;
		}
	  if (attachment1_body_length <= 0)
		attachment1_body = 0;

	  if (attachment1_body)
		{
		  char *newb = (char *) XP_ALLOC (attachment1_body_length + 1);
		  if (! newb)
			{
			  return MK_OUT_OF_MEMORY;
			}
		  XP_MEMCPY (newb, attachment1_body, attachment1_body_length);
		  newb [attachment1_body_length] = 0;
		  m_attachment1_body = newb;
		  m_attachment1_body_length = attachment1_body_length;
		}
	}

  if (!fields->GetNewspostUrl() || !*fields->GetNewspostUrl())
	fields->SetNewspostUrl("news:");

  m_fields->SetNewspostUrl(fields->GetNewspostUrl());
  m_fields->SetDefaultBody(fields->GetDefaultBody());
  StrAllocCopy (m_attachment1_type, attachment1_type);
  StrAllocCopy (m_attachment1_encoding, "8bit");

  /* strip whitespace from and duplicate header fields. */

  SetMimeHeader(MSG_FROM_HEADER_MASK, fields->GetFrom());

  SetMimeHeader(MSG_REPLY_TO_HEADER_MASK, fields->GetReplyTo());
  SetMimeHeader(MSG_TO_HEADER_MASK, (fields->GetTo()));
  SetMimeHeader(MSG_CC_HEADER_MASK, (fields->GetCc()));
  SetMimeHeader(MSG_FCC_HEADER_MASK, (fields->GetFcc()));
  SetMimeHeader(MSG_BCC_HEADER_MASK, (fields->GetBcc()));
  SetMimeHeader(MSG_NEWSGROUPS_HEADER_MASK, (fields->GetNewsgroups()));
  SetMimeHeader(MSG_FOLLOWUP_TO_HEADER_MASK, (fields->GetFollowupTo()));
  SetMimeHeader(MSG_ORGANIZATION_HEADER_MASK, (fields->GetOrganization()));
  SetMimeHeader(MSG_SUBJECT_HEADER_MASK, (fields->GetSubject()));
  SetMimeHeader(MSG_REFERENCES_HEADER_MASK, (fields->GetReferences()));
  SetMimeHeader(MSG_X_TEMPLATE_HEADER_MASK, (fields->GetTemplateName()));

  if (fields->GetOtherRandomHeaders())
	m_fields->SetOtherRandomHeaders(fields->GetOtherRandomHeaders());

  if (fields->GetPriority())
	m_fields->SetPriority(fields->GetPriority());

  int i, j = (int) MSG_LAST_BOOL_HEADER_MASK;
  for (i = 0; i < j; i++)
	  m_fields->SetBoolHeader((MSG_BOOL_HEADER_SET) i, 
							  fields->GetBoolHeader((MSG_BOOL_HEADER_SET) i));
#if 0
  m_fields->SetReturnReceipt(fields->GetReturnReceipt());
  HG29822
  m_fields->SetSigned(fields->GetSigned());
  m_fields->SetAttachVCard(fields->GetAttachVCard());
#endif
  m_fields->SetForcePlainText(fields->GetForcePlainText());
  m_fields->SetUseMultipartAlternative(fields->GetUseMultipartAlternative());

  if (pane && m_fields->GetReturnReceipt())
  {
	  if (m_fields->GetReturnReceiptType() == 1 ||
		  m_fields->GetReturnReceiptType() == 3)
		  pane->SetRequestForReturnReceipt(TRUE);
	  else
		  pane->SetRequestForReturnReceipt(FALSE);
  }

  /* Check the fields for legitimacy, and run the callback if they're not
	 ok.  */
  if ( mode != MSG_SaveAsDraft && mode != MSG_SaveAsTemplate )
	failure = mime_sanity_check_fields (m_fields->GetFrom(), m_fields->GetReplyTo(),
										m_fields->GetTo(), m_fields->GetCc(),
										m_fields->GetBcc(), m_fields->GetFcc(),
										m_fields->GetNewsgroups(), m_fields->GetFollowupTo(),
										m_fields->GetSubject(), m_fields->GetReferences(),
										m_fields->GetOrganization(),
										m_fields->GetOtherRandomHeaders());
  if (failure)
	return failure;

  m_digest_p = digest_p;
  m_dont_deliver_p = dont_deliver_p;
  m_deliver_mode = mode;
  // m_msg_file_name = WH_TempName (xpFileToPost, "nsmail");

  failure = HackAttachments(attachments, preloaded_attachments);
  return failure;
}



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
   MSG_AttachmentData structures.

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
extern "C" void
MSG_StartMessageDelivery (MSG_Pane *pane,
						  void      *fe_data,
						  MSG_CompositionFields *fields,
						  XP_Bool digest_p,
						  XP_Bool dont_deliver_p,
						  const char *attachment1_type,
						  const char *attachment1_body,
						  uint32 attachment1_body_length,
						  const struct MSG_AttachmentData *attachments,
						  void *relatedPart,
						  void (*message_delivery_done_callback)
						       (MWContext *context,
								void *fe_data,
								int status,
								const char *error_message))
{
  MSG_SendMimeDeliveryState::StartMessageDelivery(pane, fe_data, fields,
							 digest_p, dont_deliver_p,
						     MSG_DeliverNow, /* ####??? */
							 attachment1_type,
							 attachment1_body, attachment1_body_length,
							 attachments, 0, 
						     (MSG_SendPart *) relatedPart,
#ifdef XP_OS2
//DSR040297 - Casting away extern "C"
//DSR040297 - Note: this simple little cast switches the function pointers from extern "C" 
//to non-extern "C" pointer (aka a C++ function). Don't try to change the static method, I've
//tried it and it only gets uglier. This (for what its worth) is at least only a few casts in
//spots where it makes sense.
						     (void (*) (MWContext *, void *, int, const char *))
#endif
						     message_delivery_done_callback);
}


extern "C" void
msg_StartMessageDeliveryWithAttachments (MSG_Pane *pane,
										 void      *fe_data,
										 MSG_CompositionFields *fields,
										 XP_Bool digest_p,
										 XP_Bool dont_deliver_p,
										 MSG_Deliver_Mode mode,
										 const char *attachment1_type,
										 const char *attachment1_body,
										 uint32 attachment1_body_length,
										 const struct MSG_AttachedFile
										   *attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
										 void *relatedPart,
//#endif
										 
										 void (*message_delivery_done_callback)
										      (MWContext *context,
											   void *fe_data,
											   int status,
											   const char *error_message))
{
  MSG_SendMimeDeliveryState::StartMessageDelivery(pane, fe_data, fields,
							  digest_p, dont_deliver_p, mode,
							  attachment1_type, attachment1_body,
							  attachment1_body_length,
							  0, attachments,
							  (MSG_SendPart *) relatedPart,
#ifdef XP_OS2
//DSR040297 - see comment above about 'Casting away extern "C"'
							  (void (*) (MWContext *, void *, int, const char *))
#endif
							  message_delivery_done_callback);
}


extern "C" int
msg_DownloadAttachments (MSG_Pane *pane,
						 void *fe_data,
						 const struct MSG_AttachmentData *attachments,
						 void (*attachments_done_callback)
						      (MWContext *context,
							   void *fe_data,
							   int status, const char *error_message,
							   struct MSG_AttachedFile *attachments))
{
  MSG_SendMimeDeliveryState *state = 0;
  int failure = 0;

  XP_ASSERT(attachments && attachments[0].url);
  /* if (!attachments || !attachments[0].url)
	{
	  failure = -1;
	  goto FAIL;
	} */ /* The only possible error above is out of memory and it is handled
			in MSG_CompositionPane::DownloadAttachments() */
  state = new MSG_SendMimeDeliveryState;

  if (! state)
	{
	  failure = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  state->m_pane = pane;
  state->m_fe_data = fe_data;
  state->m_attachments_only_p = TRUE;
  state->m_attachments_done_callback =
#ifdef XP_OS2
//DSR040297 - see comment above about 'Casting away extern "C"'
					(void(*)(MWContext*,void*,int,const char*,MSG_AttachedFile*))
#endif
					attachments_done_callback;

  failure = state->HackAttachments(attachments, 0);
  if (failure >= 0)
	return 0;

 FAIL:
  XP_ASSERT (failure);

  /* in this case, our NET_GetURL exit routine has already freed
	 the state */
  if (failure != MK_ATTACHMENT_LOAD_FAILED)
	{
	  char *err_msg = NET_ExplainErrorDetails (failure);
	  attachments_done_callback (state->GetContext(), fe_data, failure,
								 err_msg, 0);
	  if (state) delete state;

	  if (err_msg) XP_FREE (err_msg);
#ifdef XP_MAC
		// ### mwelch The MacFE wants this error thrown as an exception.
		//            This is because of the way that error recovery occurs
		//            inside the compose session object.
		if (failure == MK_INTERRUPTED)
			failure = userCanceledErr;
#endif
	}
	return failure;
}


void MSG_SendMimeDeliveryState::Clear()
{
  if (m_fields) {
	  delete m_fields;
	  m_fields = NULL;
  }
  if (m_attachment1_type)	 	XP_FREE (m_attachment1_type);
  if (m_attachment1_encoding)	XP_FREE (m_attachment1_encoding);
  if (m_attachment1_body)		XP_FREE (m_attachment1_body);

  if (m_attachment1_encoder_data)
	{
	  MimeEncoderDestroy(m_attachment1_encoder_data, TRUE);
	  m_attachment1_encoder_data = 0;
	}

/*  if (m_headers)				XP_FREE (m_headers); */

  if (m_msg_file)
	{
	  XP_FileClose (m_msg_file);
	  m_msg_file = 0;
	  XP_ASSERT (m_msg_file_name);
	}


  if (m_imapOutgoingParser)
	{
		delete m_imapOutgoingParser;
		m_imapOutgoingParser = NULL;
	}

  if(m_imapLocalMailDB)
	{
		m_imapLocalMailDB->Close();
		m_imapLocalMailDB = NULL;
		XP_FileRemove (m_msg_file_name, xpMailFolderSummary);
	}

  if (m_msg_file_name)
	{
	  XP_FileRemove (m_msg_file_name, xpFileToPost);
	  XP_FREE (m_msg_file_name);
	  m_msg_file_name = 0;
	}

  if (m_attachments)
	{
	  int i;
	  for (i = 0; i < m_attachment_count; i++)
		{
		  if (m_attachments [i].m_encoder_data)
			{
			  MimeEncoderDestroy(m_attachments [i].m_encoder_data,
								 TRUE);
			  m_attachments [i].m_encoder_data = 0;
			}

		  FREEIF (m_attachments [i].m_url_string);
		  if (m_attachments [i].m_url)
			NET_FreeURLStruct (m_attachments [i].m_url);
		  FREEIF (m_attachments [i].m_type);
		  FREEIF (m_attachments [i].m_override_type);
		  FREEIF (m_attachments [i].m_override_encoding);
		  FREEIF (m_attachments [i].m_desired_type);
		  FREEIF (m_attachments [i].m_description);
		  FREEIF (m_attachments [i].m_x_mac_type);
		  FREEIF (m_attachments [i].m_x_mac_creator);
		  FREEIF (m_attachments [i].m_real_name);
		  FREEIF (m_attachments [i].m_encoding);
		  if (m_attachments [i].m_file)
			XP_FileClose (m_attachments [i].m_file);
		  if (m_attachments [i].m_file_name)
			{
			  if (!m_pre_snarfed_attachments_p)
				XP_FileRemove (m_attachments [i].m_file_name, xpFileToPost);
			  XP_FREE (m_attachments [i].m_file_name);
			}
#ifdef XP_MAC
		  /* remove the appledoubled intermediate file after we done all.
		   */
		  if (m_attachments [i].m_ap_filename)
			{
			  XP_FileRemove (m_attachments [i].m_ap_filename, xpFileToPost);
			  XP_FREE (m_attachments [i].m_ap_filename);
			}
#endif /* XP_MAC */
		}
	  delete[] m_attachments;
	  m_attachment_count = m_attachment_pending_count = 0;
	  m_attachments = 0;
	}
}


void
MSG_SendMimeDeliveryState::DeliverMessage ()
{
  XP_Bool mail_p = ((m_fields->GetTo() && *m_fields->GetTo()) || 
					(m_fields->GetCc() && *m_fields->GetCc()) || 
					(m_fields->GetBcc() && *m_fields->GetBcc()));
  XP_Bool news_p = (m_fields->GetNewsgroups() && 
				    *(m_fields->GetNewsgroups()) ? TRUE : FALSE);

  if ( m_deliver_mode != MSG_SaveAsDraft &&
	   m_deliver_mode != MSG_SaveAsTemplate )
	XP_ASSERT(mail_p || news_p);

#if 0
  /* Figure out how many bytes we're actually going to be writing, total.
   */
  m_delivery_bytes = 0;
  m_delivery_total_bytes = 0;

  if (m_fcc && *m_fcc)
	m_delivery_total_bytes += m_msg_size;

  if (m_queue_for_later_p)
	m_delivery_total_bytes += m_msg_size;
  else
	{
	  if (mail_p)
		m_delivery_total_bytes += m_msg_size;
	  if (news_p)
		m_delivery_total_bytes += m_msg_size;
	}
#endif /* 0 */

  if (m_deliver_mode == MSG_QueueForLater)
	{
	  QueueForLater();
	  return;
	}

  else if (m_deliver_mode == MSG_SaveAsDraft)
	{
	  SaveAsDraft();
	  return;
	}
  else if (m_deliver_mode == MSG_SaveAsTemplate)
  {
	  SaveAsTemplate();
	  return;
  }
/*
  if (m_fields->GetFcc())
	if (!DoFcc())
	  return;
*/
#ifdef XP_UNIX
  {
	int status = msg_DeliverMessageExternally(GetContext(), m_msg_file_name);
	if (status != 0)
	  {
		if (status < 0)
		  Fail (status, 0);
		else
		  {
			/* The message has now been delivered successfully. */
			MWContext *context = GetContext();
			if (m_message_delivery_done_callback)
			  m_message_delivery_done_callback (context,
												m_fe_data, 0, NULL);
			m_message_delivery_done_callback = 0;

			Clear();

			/* When attaching, even though the context has
			   active_url_count == 0, XFE_AllConnectionsComplete() **is**
			   called.  However, when not attaching, and not delivering right
			   away, we don't actually open any URLs, so we need to destroy
			   the window ourself.  Ugh!!
			 */
			if (m_attachment_count == 0)
			  MSG_MailCompositionAllConnectionsComplete(MSG_FindPane(context,
																 MSG_ANYPANE));
		  }
		return;
	  }
  }
#endif /* XP_UNIX */


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


#if 0
void
MSG_SendMimeDeliveryState::DeliveryThermo (int32 increment)
{
  int32 percent;
  m_delivery_bytes += increment;
  XP_ASSERT(m_delivery_total_bytes > 0);
  if (m_delivery_total_bytes <= 0) return;
  percent = 100 * (((double) m_delivery_bytes) /
				   ((double) m_delivery_total_bytes));
  FE_SetProgressBarPercent (GetContext(), percent);
#if 0
  FE_GraphProgress (GetContext(), 0,
					m_delivery_bytes, 0,
					m_delivery_total_bytes);
#endif /* 0 */
}
#endif /* 0 */


static void mime_deliver_as_mail_exit (URL_Struct *, int status, MWContext *);
static void mime_deliver_as_news_exit (URL_Struct *url, int status,
									   MWContext *);

void
MSG_SendMimeDeliveryState::DeliverFileAsMail ()
{
  char *buf, *buf2;
  URL_Struct *url;

  FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_MAIL));

  buf = (char *) XP_ALLOC ((m_fields->GetTo() ? XP_STRLEN (m_fields->GetTo())  + 10 : 0) +
						   (m_fields->GetCc() ? XP_STRLEN (m_fields->GetCc())  + 10 : 0) +
						   (m_fields->GetBcc() ? XP_STRLEN (m_fields->GetBcc()) + 10 : 0) +
						   10);
  if (! buf)
	{
	  Fail (MK_OUT_OF_MEMORY, 0);
	  return;
	}
  XP_STRCPY (buf, "mailto:");
  buf2 = buf + XP_STRLEN (buf);
  if (m_fields->GetTo())
	{
	  XP_STRCAT (buf2, m_fields->GetTo());
	}
  if (m_fields->GetCc())
	{
	  if (*buf2) XP_STRCAT (buf2, ",");
	  XP_STRCAT (buf2, m_fields->GetCc());
	}
  if (m_fields->GetBcc())
	{
	  if (*buf2) XP_STRCAT (buf2, ",");
	  XP_STRCAT (buf2, m_fields->GetBcc());
	}

  url = NET_CreateURLStruct (buf, NET_DONT_RELOAD);
  XP_FREE (buf);
  if (! url)
	{
	  Fail (MK_OUT_OF_MEMORY, 0);
	  return;
	}

  /* put the filename of the message into the post data field and set a flag
	 in the URL struct to specify that it is a file
   */
  url->post_data = XP_STRDUP(m_msg_file_name);
  url->post_data_size = XP_STRLEN(url->post_data);
  url->post_data_is_file = TRUE;
  url->method = URL_POST_METHOD;
  url->fe_data = this;
  url->internal_url = TRUE;

  url->msg_pane = m_pane;

  /* We can ignore the return value of NET_GetURL() because we have
	 handled the error in mime_deliver_as_mail_exit(). */
  
  MSG_UrlQueue::AddUrlToPane(url, mime_deliver_as_mail_exit, m_pane, TRUE);
}


void
MSG_SendMimeDeliveryState::DeliverFileAsNews ()
{
  URL_Struct *url = NET_CreateURLStruct (m_fields->GetNewspostUrl(), NET_DONT_RELOAD);
  if (! url)
	{
	  Fail (MK_OUT_OF_MEMORY, 0);
	  return;
	}

  FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_NEWS));

  /*  put the filename of the message into the post data field and set a flag
	  in the URL struct to specify that it is a file.
   */
 
  url->post_data = XP_STRDUP(m_msg_file_name);
  url->post_data_size = XP_STRLEN(url->post_data);
  url->post_data_is_file = TRUE;
  url->method = URL_POST_METHOD;

  url->fe_data = this;
  url->internal_url = TRUE;

  url->msg_pane = m_pane;

  /* We can ignore the return value of NET_GetURL() because we have
	 handled the error in mime_deliver_as_news_exit(). */

  MSG_UrlQueue::AddUrlToPane (url, mime_deliver_as_news_exit, m_pane, TRUE);
}

static void
mime_deliver_as_mail_exit (URL_Struct *url, int status,
						   MWContext * /*context*/)
{
  MSG_SendMimeDeliveryState *state =
	(MSG_SendMimeDeliveryState *) url->fe_data;

  state->DeliverAsMailExit(url, status);
}

void
MSG_SendMimeDeliveryState::DeliverAsMailExit(URL_Struct *url, int status)
{
  char *error_msg = 0;

  url->fe_data = 0;
  if (status < 0 && url->error_msg)
    {
	  error_msg = url->error_msg;
	  url->error_msg = 0;
    }
//  NET_FreeURLStruct (url);

  if (status < 0)
	{
	  Fail (status, error_msg);
	}
#ifdef MAIL_BEFORE_NEWS
  else if (m_newsgroups)
	{
	  /* If we're sending this mail message to news as well, start it now.
		 Completion and further errors will be handled there.
	   */
	  DeliverFileAsNews ();
	}
#endif /* MAIL_BEFORE_NEWS */
  else
	{
	  /* The message has now been sent successfully! */
	  if (m_fields->GetFcc())
		if (!DoFcc())
		  return;

	  FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_MAIL_DONE));
	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (GetContext(),
										  m_fe_data, 0, NULL);
	  m_message_delivery_done_callback = 0;
	  Clear();
	  delete this;
	}
}

static void
mime_deliver_as_news_exit (URL_Struct *url, int status,
						   MWContext * /*context*/)
{
  MSG_SendMimeDeliveryState *state =
	(MSG_SendMimeDeliveryState *) url->fe_data;
  state->DeliverAsNewsExit(url, status);
}

void
MSG_SendMimeDeliveryState::DeliverAsNewsExit(URL_Struct *url, int status)
{
  char *error_msg = 0;

  url->fe_data = 0;
  if (status < 0 && url->error_msg)
    {
	  error_msg = url->error_msg;
	  url->error_msg = 0;
    }
//  NET_FreeURLStruct (url);

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
	  /* The message has now been sent successfully! */
	  if (m_fields->GetFcc())
		if (!DoFcc())
		  return;

	  FE_Progress (GetContext(), XP_GetString(MK_MSG_DELIV_NEWS_DONE));
	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (GetContext(),
										  m_fe_data, 0, NULL);
	  m_message_delivery_done_callback = 0;
	  Clear();
	  delete this;
	}
}


#ifdef XP_OS2
XP_BEGIN_PROTOS
extern int32 msg_do_fcc_handle_line(char* line, uint32 length, void* closure);
XP_END_PROTOS
#else
static int32 msg_do_fcc_handle_line(char* line, uint32 length, void* closure);
#endif

static int
mime_do_fcc_1 (MSG_Pane *pane,
			   const char *input_file_name,  XP_FileType input_file_type,
			   const char *output_name, XP_FileType output_file_type,
			   MSG_Deliver_Mode mode,
			   const char *bcc_header,
			   const char *fcc_header,
			   const char *news_url)
{
  int status = 0;
  XP_File in = 0;
  XP_File out = 0;
  XP_Bool file_existed_p;
  XP_StatStruct st;
  char *ibuffer = 0;
  int ibuffer_size = TEN_K;
  char *obuffer = 0;
  int32 obuffer_size = 0, obuffer_fp = 0;
  int32 n;
  XP_Bool summaryWasValid = FALSE;
  XP_Bool summaryIsValid = FALSE;
  XP_Bool mark_as_read = TRUE;
  ParseOutgoingMessage *outgoingParser = NULL;
  MailDB	*mail_db = NULL;
  MsgERR err = eSUCCESS;
  char *envelope;
  char *output_file_name = NET_ParseURL(output_name, GET_PATH_PART);
  
  if (!output_file_name || !*output_file_name) // must be real file path
	  StrAllocCopy(output_file_name, output_name);

  if (mode == MSG_QueueForLater)
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_QUEUEING));
  else if ( mode == MSG_SaveAsDraft )
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_SAVING_AS_DRAFT));
  else if ( mode == MSG_SaveAsTemplate )
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_SAVING_AS_TEMPLATE));
  else
	FE_Progress (pane->GetContext(), XP_GetString(MK_MSG_WRITING_TO_FCC));


  ibuffer = NULL;
  while (!ibuffer && (ibuffer_size >= 1024))
  {
	  ibuffer = (char *) XP_ALLOC (ibuffer_size);
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
		  XP_FREEIF(output_file_name);
		  return MK_INTERRUPTED; /* #### hack.  It turns out we already
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
	  pane->GetMaster()->FindMailFolder(output_file_name,
										TRUE /*createIfMissing*/);

	  if (-1 == XP_Stat (output_file_name, &st, output_file_type))
		FE_Alert (pane->GetContext(), XP_GetString(MK_MSG_CANT_CREATE_FOLDER));
	}


  out = XP_FileOpen (output_file_name, output_file_type, XP_FILE_APPEND_BIN);
  if (!out)
	{
	  /* #### include file name in error message! */
	  status = MK_MSG_COULDNT_OPEN_FCC_FILE;
	  goto FAIL;
	}

  in = XP_FileOpen (input_file_name, input_file_type, XP_FILE_READ_BIN);
  if (!in)
	{
	  status = MK_UNABLE_TO_OPEN_FILE; /* rb -1; */ /* How did this happen? */
	  goto FAIL;
	}


  // set up database and outbound message parser to keep db up to date.
  outgoingParser = new ParseOutgoingMessage;
  err = MailDB::Open(output_file_name, FALSE, &mail_db);

  if (err != eSUCCESS)
	mail_db = NULL;

  if (!outgoingParser)
	{
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  outgoingParser->SetOutFile(out);
  outgoingParser->SetMailDB(mail_db);
  outgoingParser->SetWriteToOutFile(FALSE);


  /* Write a BSD mailbox envelope line to the file.
	 If the file existed, preceed it by a linebreak: this file format wants a
	 *blank* line before all "From " lines except the first.  This assumes
	 that the previous message in the file was properly closed, that is, that
	 the last line in the file ended with a linebreak.
   */
  XP_FileSeek(out, 0, SEEK_END);

  if (file_existed_p && st.st_size > 0)
	{
	  if (XP_FileWrite (LINEBREAK, LINEBREAK_LEN, out) < LINEBREAK_LEN)
		{
		  status = MK_MIME_ERROR_WRITING_FILE;
		  goto FAIL;
		}
	}

  outgoingParser->Init(XP_FileTell(out));
  envelope = msg_GetDummyEnvelope();

  if (msg_do_fcc_handle_line (envelope, XP_STRLEN (envelope), outgoingParser)
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
  if (mode == MSG_QueueForLater ||
	  mode == MSG_SaveAsDraft ||
	  mode == MSG_SaveAsTemplate ||
	  mark_as_read)
	{
	  char *buf = 0;
	  uint16 flags = 0;

	  mark_as_read = TRUE;
	  flags |= MSG_FLAG_READ;
	  if (mode == MSG_QueueForLater )
		flags |= MSG_FLAG_QUEUED;
	  buf = PR_smprintf(X_MOZILLA_STATUS_FORMAT LINEBREAK, flags);
	  if (buf)
	  {
		  status = msg_do_fcc_handle_line(buf, XP_STRLEN(buf), outgoingParser);
		  XP_FREEIF(buf);
		  if (status < 0)
			goto FAIL;
	  }
	  
	  uint32 flags2 = 0;
	  if (mode == MSG_SaveAsTemplate)
		  flags2 |= MSG_FLAG_TEMPLATE;
	  buf = PR_smprintf(X_MOZILLA_STATUS2_FORMAT LINEBREAK, flags2);
	  if (buf)
	  {
		  status = msg_do_fcc_handle_line(buf, XP_STRLEN(buf), outgoingParser);
		  XP_FREEIF(buf);
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
  if ((mode == MSG_QueueForLater ||
	   mode == MSG_SaveAsDraft ||
	   mode == MSG_SaveAsTemplate) &&
	  fcc_header && *fcc_header)
	{
	  int32 L = XP_STRLEN(fcc_header) + 20;
	  char *buf = (char *) XP_ALLOC (L);
	  if (!buf)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  PR_snprintf(buf, L-1, "FCC: %s" LINEBREAK, fcc_header);
	  status = msg_do_fcc_handle_line(buf, XP_STRLEN(buf), outgoingParser);
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
	  int32 L = XP_STRLEN(bcc_header) + 20;
	  char *buf = (char *) XP_ALLOC (L);
	  if (!buf)
		{
		  status = MK_OUT_OF_MEMORY;
		  goto FAIL;
		}
	  PR_snprintf(buf, L-1, "BCC: %s" LINEBREAK, bcc_header);
	  status = msg_do_fcc_handle_line(buf, XP_STRLEN(buf), outgoingParser);
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
  if ((mode == MSG_QueueForLater ||
	   mode == MSG_SaveAsDraft ||
	   mode == MSG_SaveAsTemplate) && news_url && *news_url)
	{
	  XP_Bool secure_p = (news_url[0] == 's' || news_url[0] == 'S');
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
		  char *at = XP_STRCHR (host_and_port, '@');
		  if (at)
			host_and_port = at + 1;
		}

	  if ((host_and_port && *host_and_port) || !secure_p)
		{
		  char *line = PR_smprintf(X_MOZILLA_NEWSHOST ": %s%s" LINEBREAK,
								   host_and_port ? host_and_port : "",
								   secure_p ? "/secure" : "");
		  FREEIF(orig_hap);
		  orig_hap = 0;
		  if (!line)
			{
			  status = MK_OUT_OF_MEMORY;
			  goto FAIL;
			}
		  status = msg_do_fcc_handle_line(line, XP_STRLEN(line),
										  outgoingParser);
		  FREEIF(line);

		  if (status < 0)
			goto FAIL;
		}
	  FREEIF(orig_hap);
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
	  n = XP_FileRead (ibuffer, ibuffer_size, in);
	  if (n == 0)
		break;
	  if (n < 0) /* read failed (not eof) */
		{
		  status = n;
		  goto FAIL;
		}

	  n = msg_LineBuffer (ibuffer, n,
						  &obuffer, (uint32 *)&obuffer_size,
						  (uint32*)&obuffer_fp,
						  TRUE, msg_do_fcc_handle_line,
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
  if (XP_FileWrite (LINEBREAK, LINEBREAK_LEN, out) < LINEBREAK_LEN)
  {
	  status = MK_MIME_ERROR_WRITING_FILE;
  }
  else
	outgoingParser->AdvanceOutPosition(LINEBREAK_LEN);

  if (mail_db != NULL && outgoingParser != NULL &&
	  outgoingParser->m_newMsgHdr != NULL)
	{

	  outgoingParser->FinishHeader();
	  mail_db->AddHdrToDB(outgoingParser->m_newMsgHdr, NULL, TRUE);
	  if (summaryWasValid)
		summaryIsValid = TRUE;
	}

 FAIL:

  if (ibuffer)
	XP_FREE (ibuffer);
  if (obuffer && obuffer != ibuffer)
	XP_FREE (obuffer);

  if (in)
	XP_FileClose (in);

  if (out)
	{
	  if (status >= 0)
		{
		  XP_FileClose (out);
		  if (summaryIsValid)
			msg_SetSummaryValid(output_file_name, 0, 0);
		}
	  else if (! file_existed_p)
		{
		  XP_FileClose (out);
		  XP_FileRemove (output_file_name, output_file_type);
		}
	  else
		{
		  XP_FileClose (out);
		  XP_FileTruncate (output_file_name, output_file_type, st.st_size);	/* restore original size */
		}
	}

  if (mail_db != NULL && status >= 0) {
	if ( mode == MSG_SaveAsDraft ) {
		MSG_PostDeliveryActionInfo *actionInfo =
		  pane->GetPostDeliveryActionInfo();
		if (actionInfo) {
		  if (actionInfo->m_flags & MSG_FLAG_EXPUNGED) {
			XP_ASSERT(actionInfo->m_msgKeyArray.GetSize()== 1);
			mail_db->DeleteMessage(actionInfo->m_msgKeyArray.GetAt(0));
			actionInfo->m_msgKeyArray.RemoveAt(0);
		  }
		}
		else {
		  actionInfo = new MSG_PostDeliveryActionInfo((MSG_FolderInfo*)
			pane->GetMaster()->
			FindMailFolder(output_file_name, TRUE));
		  if (actionInfo) {
			actionInfo->m_flags |= MSG_FLAG_EXPUNGED;
			pane->SetPostDeliveryActionInfo(actionInfo);
		  }
		}
		if (outgoingParser->m_newMsgHdr && actionInfo)
		  actionInfo->m_msgKeyArray.Add(outgoingParser->m_newMsgHdr->GetMessageKey());
	}
	mail_db->Close();
  }

  FREEIF(output_file_name);

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

#ifdef XP_OS2
extern int32
#else
static int32
#endif
msg_do_fcc_handle_line(char* line, uint32 length, void* closure)
{
  ParseOutgoingMessage *outgoingParser = (ParseOutgoingMessage *) closure;
  int32 err = 0;

  XP_File out = outgoingParser->GetOutFile();

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
	  line[0] == 'F' && !XP_STRNCMP(line, "From ", 5))
	{
	  if (XP_FileWrite (">", 1, out) < 1)
		  return MK_MIME_ERROR_WRITING_FILE;
	  outgoingParser->AdvanceOutPosition(1);
	}
#endif /* MANGLE_INTERNAL_ENVELOPE_LINES */

  /* #### if XP_FileWrite is a performance problem, we can put in a
	 call to msg_ReBuffer() here... */
  if (XP_FileWrite (line, length, out) < length)
	  return MK_MIME_ERROR_WRITING_FILE;
  outgoingParser->m_bytes_written += length;
  return 0;
}


extern "C" int
msg_DoFCC (MSG_Pane *pane,
		   const char *input_file,  XP_FileType input_file_type,
		   const char *output_file, XP_FileType output_file_type,
		   const char *bcc_header_value,
		   const char *fcc_header_value)
{
  XP_ASSERT(pane &&
			input_file && *input_file &&
			output_file && *output_file);
  if (! (pane &&
		 input_file && *input_file &&
		 output_file && *output_file))
	return MK_MIME_ERROR_WRITING_FILE;
  return mime_do_fcc_1 (pane,
						input_file, input_file_type,
						output_file, output_file_type,
						MSG_DeliverNow, bcc_header_value,
						fcc_header_value,
						0);
}


/* Returns false if an error happened. */
XP_Bool
MSG_SendMimeDeliveryState::DoFcc()
{
  if (!m_fields->GetFcc() || !*m_fields->GetFcc())
	return TRUE;
  else if (NET_URL_Type(m_fields->GetFcc()) == IMAP_TYPE_URL &&
		   m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4())
  {
	  SendToImapMagicFolder(MSG_FOLDER_FLAG_SENTMAIL);
	  return FALSE;
  }
  else
	{
	  int status = msg_DoFCC (m_pane,
							  m_msg_file_name, xpFileToPost,
							  m_fields->GetFcc(), xpMailFolder,
							  m_fields->GetBcc(),
							  m_fields->GetFcc());
	  if (status < 0)
		Fail (status, 0);
	  return (status >= 0);
	}
}

char *
MSG_SendMimeDeliveryState::GetOnlineFolderName(uint32 flag, const char
											   **pDefaultName)
{
	char *onlineFolderName = NULL;

	switch (flag)
	{
	case MSG_FOLDER_FLAG_DRAFTS:
		if (pDefaultName) *pDefaultName = DRAFTS_FOLDER_NAME;
		PREF_CopyCharPref ("mail.default_drafts", &onlineFolderName);
		break;
	case MSG_FOLDER_FLAG_TEMPLATES:
		if (pDefaultName) *pDefaultName = TEMPLATES_FOLDER_NAME;
		PREF_CopyCharPref("mail.default_templates", &onlineFolderName);
		break;
	case MSG_FOLDER_FLAG_SENTMAIL:
		if (pDefaultName) *pDefaultName = SENT_FOLDER_NAME;
		onlineFolderName = XP_STRDUP(m_fields->GetFcc());
		break;
	default:
		XP_ASSERT(0);
		break;
	}
	return onlineFolderName;
}

void
MSG_SendMimeDeliveryState::SaveAsOfflineOp()
{
	XP_ASSERT (m_imapOutgoingParser && 
			   m_imapLocalMailDB && m_imapFolderInfo);
	if (!m_imapOutgoingParser || !m_imapLocalMailDB || !m_imapFolderInfo)
	{
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
		return;
	}

	MsgERR err = eSUCCESS;
	MailDB *mailDB = NULL;
	MSG_IMAPFolderInfoMail *mailFolderInfo =
		m_imapFolderInfo->GetIMAPFolderInfoMail();
	XP_ASSERT (mailFolderInfo);
	err = MailDB::Open (mailFolderInfo->GetPathname(), FALSE, &mailDB);
	if (err == eSUCCESS)
	{
		MessageKey fakeId = mailDB->GetUnusedFakeId();
		MailMessageHdr *mailMsgHdr = NULL, *newMailMsgHdr = NULL;

		if (m_deliver_mode == MSG_SaveAsDraft)
		{
			// Do we have an actionInfo?
			MSG_PostDeliveryActionInfo *actionInfo =
				m_pane->GetPostDeliveryActionInfo(); 
			if (actionInfo)
			{
				DBOfflineImapOperation *op = NULL;
				MessageKey msgKey = actionInfo->m_msgKeyArray.GetAt(0);
				if ((int32) msgKey >= 0) // real message key
				{
					// we start with an existing draft and save it while offline 
					// delete the old message and create a new message header
					op = mailDB->GetOfflineOpForKey(msgKey, TRUE);
					if (op)
					{
						op->SetImapFlagOperation(op->GetNewMessageFlags() |
												 kImapMsgDeletedFlag);
						delete op;
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
				// this is a new draft create a new actionInfo and a message
				// header 
				actionInfo = new MSG_PostDeliveryActionInfo(m_imapFolderInfo);
				actionInfo->m_flags |= MSG_FLAG_EXPUNGED;
				actionInfo->m_msgKeyArray.Add(fakeId);
				m_pane->SetPostDeliveryActionInfo(actionInfo);
			}
		}
		newMailMsgHdr = new MailMessageHdr;

		mailMsgHdr = m_imapLocalMailDB->GetMailHdrForKey(0);
		if (mailMsgHdr)
		{
			if (newMailMsgHdr)
			{
				XP_File fileId = XP_FileOpen (m_msg_file_name, xpFileToPost,
											  XP_FILE_READ_BIN);
				int iSize = 10240;
				char *ibuffer = NULL;
				
				while (!ibuffer && (iSize >= 512))
				{
					ibuffer = (char *) XP_ALLOC(iSize);
					if (!ibuffer)
						iSize /= 2;
				}
				
				if (fileId && ibuffer)
				{
					int32 numRead = 0;
					newMailMsgHdr->CopyFromMsgHdr(mailMsgHdr,
												  m_imapLocalMailDB->GetDB(),
												  mailDB->GetDB());

					newMailMsgHdr->SetMessageKey(fakeId);
					err = mailDB->AddHdrToDB(newMailMsgHdr, NULL, TRUE);

					// now write the offline message
					numRead = XP_FileRead(ibuffer, iSize, fileId);
					while(numRead > 0)
					{
						newMailMsgHdr->AddToOfflineMessage(ibuffer, numRead,
														   mailDB->GetDB());
						numRead = XP_FileRead(ibuffer, iSize, fileId);
					}
					// now add the offline op to the database
					DBOfflineImapOperation *op =
						mailDB->GetOfflineOpForKey(fakeId, TRUE);
					if (op)
					{
						op->SetAppendMsgOperation(mailFolderInfo->GetOnlineName(),
												  m_deliver_mode ==
												  MSG_SaveAsDraft ?
												  kAppendDraft :
												  kAppendTemplate);
						delete op;
						/* The message has now been queued successfully. */
						if (m_message_delivery_done_callback)
							m_message_delivery_done_callback (GetContext(),
															  m_fe_data, 0, NULL);
						m_message_delivery_done_callback = 0;
						
						// Clear() clears the Fcc path
						Clear();
					}
					else
					{
						mailDB->RemoveHeaderFromDB (newMailMsgHdr);
						Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
					}
				}
				else
				{
					Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
				}
				if (fileId)
					XP_FileClose(fileId);
				XP_FREEIF(ibuffer);
				delete newMailMsgHdr;
			}
			else
			{
				Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
			}
			delete mailMsgHdr;
		}
		else
		{
			Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
		}
	}
	else
	{
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);
	}
	if (mailDB)
		mailDB->Close();
}

void
MSG_SendMimeDeliveryState::ImapAppendAddBccHeadersIfNeeded(URL_Struct *url)
{
	XP_ASSERT(url);
	const char *bcc_headers = m_fields->GetBcc();
	char *post_data = NULL;
	if (bcc_headers && *bcc_headers)
	{
		post_data = WH_TempName(xpFileToPost, "nsmail");
		if (post_data)
		{
			XP_File dstFile = XP_FileOpen(post_data, xpFileToPost, XP_FILE_WRITE_BIN);
			if (dstFile)
			{
				XP_File srcFile = XP_FileOpen(m_msg_file_name, xpFileToPost,
											  XP_FILE_READ_BIN);
				if (srcFile)
				{
					char *tmpBuffer = NULL;
					int bSize = TEN_K;
					
					while (!tmpBuffer && (bSize >= 512))
					{
						tmpBuffer = (char *)XP_ALLOC(bSize);
						if (!tmpBuffer)
							bSize /= 2;
					}
					int bytesRead = 0;
					if (tmpBuffer)
					{
						XP_FileWrite("Bcc: ", 5, dstFile);
						XP_FileWrite(bcc_headers, XP_STRLEN(bcc_headers),
									 dstFile);
						XP_FileWrite(CRLF, XP_STRLEN(CRLF), dstFile);
						bytesRead = XP_FileRead(tmpBuffer, bSize, srcFile);
						while (bytesRead > 0)
						{
							XP_FileWrite(tmpBuffer, bytesRead, dstFile);
							bytesRead = XP_FileRead(tmpBuffer, bSize,
													srcFile);
						}
						XP_FREE(tmpBuffer);
					}
					XP_FileClose(srcFile);
				}
				XP_FileClose(dstFile);
			}
		}
	}
	else
	{
		post_data = XP_STRDUP(m_msg_file_name);
	}

	if (post_data)
	{
		url->post_data = post_data;
		url->post_data_size = XP_STRLEN(post_data);
		url->post_data_is_file = TRUE;
		url->method = URL_POST_METHOD;
		url->fe_data = this;
		url->internal_url = TRUE;
		url->msg_pane = m_pane;
		m_pane->GetContext()->imapURLPane = m_pane;
		
		MSG_UrlQueue::AddUrlToPane 
			(url, MSG_SendMimeDeliveryState::PostSendToImapMagicFolder, m_pane, TRUE);
		
	}
	else
	{
		NET_FreeURLStruct(url);
	}
}

/* Send the message to the magic folder, and runs the completion/failure
   callback.
 */
void
MSG_SendMimeDeliveryState::SendToImapMagicFolder ( uint32 flag )
{
	char *onlineFolderName = NULL;
	const char *defaultName = "";
	char *name = NULL;
	char *host = NULL;
	char *owner = NULL;
	URL_Struct* url = NULL;
	char* buf = NULL;

	if (!m_imapFolderInfo)
	{
		XP_ASSERT (m_pane);
		XP_ASSERT (m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4());

		onlineFolderName = GetOnlineFolderName(flag, &defaultName);

		if (onlineFolderName && NET_URL_Type(onlineFolderName) == IMAP_TYPE_URL)
		{
			host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
			name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
			owner = NET_ParseURL(onlineFolderName, GET_USERNAME_PART);
			if (!name || !*name)
			{
				XP_FREEIF (name);	// in case of allocated empty string
				name = PR_smprintf("/%s", defaultName);
			}
			if (!owner || !*owner)
			{
				MSG_IMAPHost *imapHost = m_pane->GetMaster()->GetIMAPHost(host);
				if (imapHost && imapHost->GetDefaultNamespacePrefixOfType(kPersonalNamespace))
					StrAllocCopy(owner, imapHost->GetUserName());
			}
		}

		if (name && *name && host && *host)
		  m_imapFolderInfo = m_pane->GetMaster()->FindImapMailFolder(host,
																	 name+1,
																	 owner,
																	 FALSE);
	}

	if (m_imapFolderInfo)
	{
		if (NET_IsOffline())
		{
			if (flag == MSG_FOLDER_FLAG_DRAFTS || flag == MSG_FOLDER_FLAG_TEMPLATES)
				SaveAsOfflineOp();
			else
				XP_ASSERT(0);	// shouldn't be here
		}
		else
		{
			buf = CreateImapAppendMessageFromFileUrl( m_imapFolderInfo->GetHostName(),
													  m_imapFolderInfo->GetOnlineName(),
													  m_imapFolderInfo->GetOnlineHierarchySeparator(),
													  m_deliver_mode == MSG_SaveAsDraft);
			if (buf)
			{
				url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
				if (url)
				{
					ImapAppendAddBccHeadersIfNeeded(url);
				}
				XP_FREEIF(buf);
			}
		}
	}
	else if (host && name && *host && *name && !NET_IsOffline())
	{
		if (m_pane->IMAPListMailboxExist())
		{
			m_pane->SetIMAPListMailboxExist(FALSE);
			buf = CreateImapAppendMessageFromFileUrl(host,
													 name+1,
													 kOnlineHierarchySeparatorUnknown, 
													 m_deliver_mode == MSG_SaveAsDraft);
			if (buf)
			{
				url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
				if (url)
				{
					ImapAppendAddBccHeadersIfNeeded(url);
				}
				XP_FREEIF(buf);
			}
		}
		else
		{
			buf = CreateImapListUrl(host,
									name+1, kOnlineHierarchySeparatorUnknown);
			if (buf)
			{
				url = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
				if (url)
				{
					url->fe_data = this;
					url->internal_url = TRUE;
					url->msg_pane = m_pane;
					GetContext()->imapURLPane = m_pane;
					m_pane->SetIMAPListInProgress(TRUE);
					
					MSG_UrlQueue::AddUrlToPane
						(url, MSG_SendMimeDeliveryState::PostListImapMailboxFolder, m_pane, TRUE);
					
				}
				XP_FREEIF(buf);
			}
		}
	}
	else
	{
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
		Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0);	/* -1 rb */
	}
	
	XP_FREEIF(host);
	XP_FREEIF(name);
	XP_FREEIF(owner);
	XP_FREEIF(onlineFolderName);
}


void
MSG_SendMimeDeliveryState::PostCreateImapMagicFolder (	URL_Struct *url,
														int status,
														MWContext * context)
{
	MSG_SendMimeDeliveryState *state =
		(MSG_SendMimeDeliveryState*) url->fe_data;
	XP_ASSERT(state);

	if (status < 0)
	{
		state->Fail (status, 0);
	}
	else
	{
		MSG_Master::PostCreateImapFolderUrlExitFunc (url, status, context);
		char *host = NET_ParseURL(url->address, GET_HOST_PART);
		char *name = NET_ParseURL(url->address, GET_PATH_PART);
		char *owner = NET_ParseURL(url->address, GET_USERNAME_PART);
		
		state->m_imapFolderInfo = 
			state->m_pane->GetMaster()->FindImapMailFolder(host, name+1,
														   owner, FALSE);
		XP_ASSERT(state->m_imapFolderInfo);

		if (state->m_imapFolderInfo)
		{
			if (state->m_deliver_mode == MSG_SaveAsDraft)
			{
				state->m_imapFolderInfo->SetFlag(MSG_FOLDER_FLAG_DRAFTS);
				state->SendToImapMagicFolder(MSG_FOLDER_FLAG_DRAFTS);
			}
			else if (state->m_deliver_mode == MSG_DeliverNow)
			{
				state->m_imapFolderInfo->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
				state->SendToImapMagicFolder(MSG_FOLDER_FLAG_SENTMAIL);
			}
			else if (state->m_deliver_mode == MSG_SaveAsTemplate)
			{
				state->m_imapFolderInfo->SetFlag(MSG_FOLDER_FLAG_TEMPLATES);
				state->SendToImapMagicFolder(MSG_FOLDER_FLAG_TEMPLATES);
			}
			else
				XP_ASSERT(FALSE);
		}
		else
		{
			state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
		}
		XP_FREEIF(host);
		XP_FREEIF(name);
		XP_FREEIF(owner);
	}
	NET_FreeURLStruct(url); 
}


void
MSG_SendMimeDeliveryState::PostListImapMailboxFolder (	URL_Struct *url,
														int status,
														MWContext *context)
{
	MSG_SendMimeDeliveryState *state =
		(MSG_SendMimeDeliveryState*) url->fe_data;
	XP_ASSERT(state);
	const char *defaultName = "";

	if (status < 0)
	{
		state->Fail (status, 0);
	}
	else
	{
		uint32 flag;

		switch (state->m_deliver_mode)
		{
		case MSG_SaveAsDraft:
			defaultName = DRAFTS_FOLDER_NAME;
			flag = MSG_FOLDER_FLAG_DRAFTS;
			break;
		case MSG_SaveAsTemplate:
			defaultName = TEMPLATES_FOLDER_NAME;
			flag = MSG_FOLDER_FLAG_TEMPLATES;
			break;
		case MSG_DeliverNow:
			defaultName = SENT_FOLDER_NAME;
			flag = MSG_FOLDER_FLAG_SENTMAIL;
			break;
		default:
			XP_ASSERT(0);
			state->Fail(MK_IMAP_UNABLE_TO_SAVE_MESSAGE, 0); /* -1 rb */
			return;
			break;
		}

		state->m_pane->SetIMAPListInProgress(FALSE);
		if (state->m_pane->IMAPListMailboxExist())
		{
			state->SendToImapMagicFolder(flag);
		}
		else
		{
			char *onlineFolderName = msg_MagicFolderName(state->m_pane->GetMaster()->GetPrefs(), flag, &status);
			char *buf = NULL;
			char *host = NULL, *name = NULL /*, *owner = NULL */;
			MSG_IMAPHost *imapHost = NULL;

			if (status < 0)
			{
				char *error_msg = XP_GetString(status);
				state->Fail(status, error_msg ? XP_STRDUP(error_msg) : 0);
				return;
			}
			else if (!onlineFolderName)
			{
				state->Fail(MK_IMAP_NO_ONLINE_FOLDER, 0);	/* -1 rb */
				return;
			}

			XP_ASSERT(NET_URL_Type(onlineFolderName) == IMAP_TYPE_URL);

			host = NET_ParseURL(onlineFolderName, GET_HOST_PART);
			name = NET_ParseURL(onlineFolderName, GET_PATH_PART);
			// owner = NET_ParseURL(onlineFolderName, GET_USERNAME_PART);
			
			if (!name || !*name)
			{
				XP_FREEIF(name);
				name = PR_smprintf("/%s", defaultName);
			}

			imapHost = state->m_pane->GetMaster()->GetIMAPHost(host);

			XP_ASSERT(imapHost);
			if (imapHost->GetDefaultNamespacePrefixOfType(kPersonalNamespace))
			{
				char *nameWithPrefix = PR_smprintf("%s%s",
												   imapHost->GetDefaultNamespacePrefixOfType(kPersonalNamespace),
												   name+1);
				if (nameWithPrefix)
				{
					XP_FREE(name);
					name = nameWithPrefix;
				}
			}
			// *** Create Imap magic folder
			// *** then append message to the folder
			buf = CreateImapMailboxCreateUrl(host, *name == '/' ? name+1 : name,
					  kOnlineHierarchySeparatorUnknown);
			if (buf)
			{
			  URL_Struct* url_struct = NULL;
			  url_struct = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
			  if (url_struct)
			  {
				  url_struct->fe_data = state;
				  url_struct->internal_url = TRUE;
				  url_struct->msg_pane = state->m_pane;
				  state->GetContext()->imapURLPane = state->m_pane;

				  MSG_UrlQueue::AddUrlToPane (url_struct,
					  MSG_SendMimeDeliveryState::PostCreateImapMagicFolder, state->m_pane, TRUE);
			  }
			  XP_FREEIF(buf);
			}
			XP_FREEIF(onlineFolderName);
			XP_FREEIF(host);
			XP_FREEIF(name);
			// XP_FREEIF(owner);
		}
	}
	NET_FreeURLStruct(url); 
}

void
MSG_SendMimeDeliveryState::SetIMAPMessageUID(MessageKey key)
{
	XP_ASSERT(m_pane && m_pane->GetPaneType() == MSG_COMPOSITIONPANE);
	MSG_CompositionPane *composePane = (MSG_CompositionPane*) m_pane;
	composePane->SetIMAPMessageUID(key);
}

void
MSG_SendMimeDeliveryState::PostSendToImapMagicFolder (	URL_Struct *url,
														int status,
														MWContext *context)
{
	MSG_SendMimeDeliveryState *state = (MSG_SendMimeDeliveryState*) url->fe_data;
	MSG_CompositionPane *composePane = (MSG_CompositionPane*) state->m_pane;
	XP_ASSERT(state && composePane);

	MSG_PostDeliveryActionInfo *actionInfo =
		composePane->GetPostDeliveryActionInfo();
	MailDB *mailDB = NULL;
	IDArray *idArray = new IDArray;
	char *onlineFolderName = NULL;

	if (XP_STRCMP(url->post_data, state->m_msg_file_name))
		XP_FileRemove(url->post_data, xpFileToPost);

	if (status < 0)
	{
		state->Fail (status, 0);
	}
	else
	{
		/* The message has now been queued successfully. */
		if (state->m_message_delivery_done_callback)
			state->m_message_delivery_done_callback (state->GetContext(),
													 state->m_fe_data, 0, NULL);
		state->m_message_delivery_done_callback = 0;
		
		// Clear() clears the Fcc path
		state->Clear();
	}
	
	XP_ASSERT (composePane && composePane->GetPaneType() ==
			   MSG_COMPOSITIONPANE); 

	if (actionInfo  &&
		state->m_deliver_mode == MSG_SaveAsDraft &&
		actionInfo->m_msgKeyArray.GetSize() > 1 &&
		actionInfo->m_flags & MSG_FLAG_EXPUNGED)
	{
		MSG_Pane *urlPane = NULL;

		if (!urlPane)
			actionInfo->m_folderInfo ?
			state->m_pane->GetMaster()->FindPaneOfType
			               (actionInfo->m_folderInfo, MSG_THREADPANE) : NULL;

		composePane->DeleteIMAPOldDraftUID(actionInfo, urlPane);
	}
	if (state->m_imapFolderInfo)
	{
		char *dbName = WH_FileName(state->m_imapFolderInfo->GetPathname(), xpMailFolderSummary);
		if (dbName)
			mailDB = (MailDB *) MessageDB::FindInCache(dbName);
		XP_FREEIF(dbName);
		MSG_Pane *urlPane = NULL;

		if (!urlPane)
			urlPane = state->m_pane->GetMaster()->FindPaneOfType
				            (state->m_imapFolderInfo, MSG_THREADPANE);

		if (!urlPane)
			urlPane = composePane;

		if (mailDB && urlPane)
		{
			char *url_string = CreateImapMailboxLITESelectUrl(
				state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetHostName(),
				state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetOnlineName(),
				state->m_imapFolderInfo->GetIMAPFolderInfoMail()->GetOnlineHierarchySeparator());

			if (url_string)
			{
				URL_Struct *url_struct =
					NET_CreateURLStruct(url_string,
										NET_NORMAL_RELOAD);
				XP_ASSERT(urlPane);
				if (url_struct)
				{
					state->m_imapFolderInfo->SetFolderLoadingContext(urlPane->GetContext());
					urlPane->SetLoadingImapFolder(state->m_imapFolderInfo);
					url_struct->fe_data = (void *) state->m_imapFolderInfo;
					url_struct->internal_url = TRUE;
					url_struct->msg_pane = urlPane;
					urlPane->GetContext()->imapURLPane = urlPane;

					MSG_UrlQueue::AddUrlToPane (url_struct, 
												MSG_Pane::PostLiteSelectExitFunc,
												urlPane, TRUE);
				}
				XP_FREEIF(url_string);
			}
			// MessageDB::FindInCache() does not add refCount so don't call close
			// mailDB->Close();
		}
		else
		{
			idArray->Add(0);	// add dummy message key
			state->m_imapFolderInfo->UpdatePendingCounts(state->m_imapFolderInfo, 
												idArray, FALSE);
			state->m_imapFolderInfo->SummaryChanged();
			// make sure we close down the cached imap connection when we done
			// with save draft; this is save draft then send case what about the closing
			// down the compose window case?
			if (urlPane->GetPaneType() != MSG_THREADPANE && 
				state->m_imapFolderInfo->GetFlags() & MSG_FOLDER_FLAG_DRAFTS &&
				state->m_deliver_mode != MSG_SaveAsDraft)
				composePane->GetMaster()->ImapFolderClosed(state->m_imapFolderInfo);
		}
	}

	XP_FREEIF(onlineFolderName);
	if (idArray)
		delete idArray;

	NET_FreeURLStruct(url);
	delete state;
}

/* Send the message to the magic folder, and runs the completion/failure
   callback.
 */
void
MSG_SendMimeDeliveryState::SendToMagicFolder ( uint32 flag )
{
  char *name = 0;
  int status = 0;

  name = msg_MagicFolderName(m_pane->GetMaster()->GetPrefs(), flag, &status);
  if (status < 0)
  {
	  char *error_msg = XP_GetString(status);
	  Fail (status, error_msg ? XP_STRDUP(error_msg) : 0);
	  return;
  }
  else if (!name || *name == 0)
	{
	  XP_FREEIF(name);
	  status = MK_OUT_OF_MEMORY;
	  goto FAIL;
	}
  else if (NET_URL_Type(name) == IMAP_TYPE_URL &&
	  m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4())
  {
	  SendToImapMagicFolder(flag);
	  return;
  }
  status = mime_do_fcc_1 (m_pane,
						  m_msg_file_name, xpFileToPost,
						  name, xpMailFolder,
						  m_deliver_mode, m_fields->GetBcc(), m_fields->GetFcc(),
						  (m_fields->GetNewsgroups() && *m_fields->GetNewsgroups()
						   ? m_fields->GetNewspostUrl() : 0));
  XP_FREEIF (name);
  if (status < 0)
	goto FAIL;

 FAIL:

  if (status < 0)
	{
	  Fail (status, 0);
	}
  else
	{
	  MWContext *context = GetContext();
	  FE_Progress(context, XP_GetString(MK_MSG_QUEUED));

	  /* The message has now been queued successfully. */
	  if (m_message_delivery_done_callback)
		m_message_delivery_done_callback (context,
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
	   * embed any images. This could be done better.
	   */
	  if (m_attachment_count == 0)
		  if (!m_related_part || !m_related_part->GetNumChildren())
			  MSG_MailCompositionAllConnectionsComplete(MSG_FindPane(context,
																 MSG_ANYPANE));
	}
}

/* Queues the message for later delivery, and runs the completion/failure
   callback.
 */
void MSG_SendMimeDeliveryState::QueueForLater()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_QUEUE);
}


/* Save the message to the Drafts folder, and runs the completion/failure
   callback.
 */
void MSG_SendMimeDeliveryState::SaveAsDraft()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_DRAFTS);
}

/* Save the message to the Template folder, and runs the completion/failure
   callback.
 */
void MSG_SendMimeDeliveryState::SaveAsTemplate()
{
  SendToMagicFolder (MSG_FOLDER_FLAG_TEMPLATES);
}

#ifdef _USRDLL

PUBLIC void
NET_RegisterDLLContentConverters()
{

  NET_RegisterContentTypeConverter ("*", FO_MAIL_TO,
										NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_TO,
										NULL, mime_make_attachment_stream);

  /* #### What is this function for?
	 Is this right?  I've cloned this stuff from MSG_RegisterConverters()
	 above.  --jwz */

  NET_RegisterContentTypeConverter ("*", FO_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);
  NET_RegisterContentTypeConverter ("*", FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, mime_make_attachment_stream);

  NET_RegisterContentTypeConverter (MESSAGE_RFC822, FO_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS, FO_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_RFC822,
									FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
  NET_RegisterContentTypeConverter (MESSAGE_NEWS,
									FO_CACHE_AND_MAIL_MESSAGE_TO,
									NULL, MIME_MessageConverter);
}

#endif
