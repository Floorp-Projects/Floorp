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
#include "nsMsgCopy.h"
#include "nsIPref.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgAttachmentHandler.h"
#include "nsMsgSend.h"
#include "nsMsgCompUtils.h"
#include "nsIPref.h"
#include "nsMsgEncoders.h"
#include "nsMsgI18N.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

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

nsMsgAttachmentHandler::nsMsgAttachmentHandler()
{
	m_url_string = NULL;
	m_url = NULL;
	m_done = PR_FALSE;
	m_type = NULL;
  m_charset = NULL;
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
	m_already_encoded_p = PR_FALSE;
	m_file_name = NULL;
	m_file = 0;
#ifdef XP_MAC
	m_ap_filename = NULL;
#endif
	m_decrypted_p = PR_FALSE;
	m_size = 0;
	m_unprintable_count = 0;
	m_highbit_count = 0;
	m_ctl_count = 0;
	m_null_count = 0;
	m_current_column = 0;
	m_max_column = 0;
	m_lines = 0;

	m_encoder_data = NULL;

	m_graph_progress_started = PR_FALSE;
}

nsMsgAttachmentHandler::~nsMsgAttachmentHandler()
{
}


PRBool nsMsgAttachmentHandler::UseUUEncode_p(void)
{
printf("RICHIE: commenting out GetCompBoolHeader() call for now...\n");
	PRBool returnVal = (m_mime_delivery_state) && 
	  					(m_mime_delivery_state->m_pane) 
                                                &&
                                                PR_FALSE;

//	  					((nsMsgCompose*)(m_mime_delivery_state->m_pane))->
//				  		GetCompBoolHeader(MSG_UUENCODE_BINARY_BOOL_HEADER_MASK);
	
	return returnVal;
}

static void 
msg_escape_file_name (URL_Struct *m_url) 
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);   
  (void)prefs;
  
  NS_ASSERTION (m_url->address && !PL_strncasecmp(m_url->address, "file:", 5), "Invalid URL type");
	if (!m_url->address || PL_strncasecmp(m_url->address, "file:", 5))
		return;

	char * new_address = nsEscape(PL_strchr(m_url->address, ':') + 1, url_Path);
	NS_ASSERTION(new_address, "null ptr");
	if (!new_address)
		return;

	PR_FREEIF(m_url->address);
	m_url->address = PR_smprintf("file:%s", new_address);
	PR_FREEIF(new_address);
}

PRInt32
nsMsgAttachmentHandler::SnarfAttachment ()
{
  PRInt32 status = 0;
  NS_ASSERTION (! m_done, "Already done");

  m_file_name = nsMsgCreateTempFileName("nsmail.tmp");
  if (! m_file_name)
	return (MK_OUT_OF_MEMORY);

  m_file = PR_Open (m_file_name, PR_CREATE_FILE | PR_RDWR, 493);
  if (! m_file)
	return MK_UNABLE_TO_OPEN_TMP_FILE; /* #### how do we pass file name? */

  m_url->fe_data = this;

  /* #### m_type is still unknown at this point.
	 We *really* need to find a way to make the textfe not blow
	 up on documents that are not text/html!!
   */
   
#ifdef XP_MAC
  if (NET_IsLocalFileURL(m_url->address) &&	// do we need to add IMAP: to this list? NET_IsLocalFileURL returns PR_FALSE always for IMAP - DMB
	  (PL_strncasecmp(m_url->address, "mailbox:", 8) != 0))
	{
	  /* convert the apple file to AppleDouble first, and then patch the
		 address in the url.
	   */
	  char* src_filename = NET_GetLocalFileFromURL (m_url->address);

		// ### mwelch Only use appledouble if we aren't uuencoding.
	  if(/*JFD isMacFile(src_filename) && */ (! UseUUEncode_p()))
		{

		  char	*separator, tmp[128];
		  NET_StreamClass *ad_encode_stream;

		  separator = mime_make_separator("ad");
		  if (!separator)
			return MK_OUT_OF_MEMORY;
						
		  m_ap_filename  = nsMsgCreateTempFileName("nsmail.tmp");
		  ad_encode_stream = (NET_StreamClass *)		/* need a prototype */
NULL; /* JFD			fe_MakeAppleDoubleEncodeStream (FO_CACHE_AND_MAIL_TO,
											(void*)NULL,
											m_url,
											m_mime_delivery_state->GetContext(),
											src_filename,
											m_ap_filename,
											separator,
											m_real_name);
JFD */

		  if (ad_encode_stream == NULL)
			{
			  PR_FREEIF(separator);
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

		  PR_Free(ad_encode_stream);

		  if (status < 0)
			{
			  PR_FREEIF(separator);
			  return status;
			}

		  PR_Free(m_url->address);
		  {
		    char * temp = WH_FileName(m_ap_filename, xpFileToPost );
		    m_url->address = XP_PlatformFileToURL(temp); // jrm 97/02/08
			if (temp)
				PR_Free(temp);
		  }
		  /* and also patch the types.
		   */

		  PR_snprintf(tmp, sizeof(tmp), MULTIPART_APPLEDOUBLE ";\r\n boundary=\"%s\"",
					 separator);

		  PR_FREEIF(separator);

		PR_FREEIF (m_type);
		m_type = PL_strdup(tmp);
		}
	  else
		{
//JFD			if (isMacFile(src_filename))
			{
				// The only time we want to send just the data fork of a two-fork
				// Mac file is if uuencoding has been requested.
				NS_ASSERTION(UseUUEncode_p(), "not UseUUEncode_p");
				if (!((nsMsgCompose *) m_mime_delivery_state->m_pane)->m_confirmed_uuencode_p)
				{
#ifdef UNREADY_CODE
					PRBool confirmed = FE_Confirm(m_mime_delivery_state->m_pane->GetContext(), 
													XP_GetString(MK_MSG_MAC_PROMPT_UUENCODE)); 
#else
				PRBool confirmed = PR_TRUE;
#endif

					// only want to do this once
					((nsMsgCompose *) m_mime_delivery_state->m_pane)->m_confirmed_uuencode_p = PR_TRUE;
					
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

/*JFD
		  my_FSSpecFromPathname(src_filename, &fsSpec);
*/
		  if (FSpGetFInfo (&fsSpec, &info) == noErr)
			{
			  PR_snprintf(filetype, sizeof(filetype), "%X", info.fdType);
			  PR_FREEIF(m_x_mac_type);
			  m_x_mac_type    = PL_strdup(filetype);

			  PR_snprintf(filetype, sizeof(filetype), "%X", info.fdCreator);
			  PR_FREEIF(m_x_mac_creator);
			  m_x_mac_creator = PL_strdup(filetype);
			  if (m_type == NULL ||
				  !PL_strcasecmp (m_type, TEXT_PLAIN))
				{
# define TEXT_TYPE	0x54455854  /* the characters 'T' 'E' 'X' 'T' */
# define text_TYPE	0x74657874  /* the characters 't' 'e' 'x' 't' */

				  if (info.fdType != TEXT_TYPE && info.fdType != text_TYPE)
					{
/*JFD
					  FE_FileType(m_url->address, &useDefault,
								  &macType, &macEncoding);
*/

					  PR_FREEIF(m_type);
					  m_type = macType;
					}
				}
			}

		  /* don't bother to set the types if we failed in getting the file
			 info. */
		}
	  PR_FREEIF(src_filename);
	  src_filename = 0;
	}
#else

  /* if we are attaching a local file make sure the file name are escaped
   * properly
   */
  if (NET_IsLocalFileURL(m_url->address) &&
	  PL_strncasecmp (m_url->address, "file:", 5) == 0) 
  {
	  msg_escape_file_name(m_url);
  }

#endif /* XP_MAC */

  if (m_desired_type &&
	  !PL_strcasecmp (m_desired_type, TEXT_PLAIN) )
	{
	  /* Conversion to plain text desired.
	   */
/*JFD
	  m_print_setup.url = m_url;
	  m_print_setup.carg = this;
	  m_print_setup.completion = mime_text_attachment_url_exit;
	  m_print_setup.filename = NULL;
	  m_print_setup.out = m_file;
	  m_print_setup.eol = CRLF;
JFD */
	  PRInt32 width = 72;
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
    if (NS_SUCCEEDED(rv) && prefs) 
  	  prefs->GetIntPref("mailnews.wraplength", &width);
	  if (width == 0) width = 72;
	  else if (width < 10) width = 10;
	  else if (width > 30000) width = 30000;

	  if (m_mime_delivery_state->m_pane->GetPaneType() == MSG_COMPOSITIONPANE)
	  {
		  int lineWidth = ((nsMsgCompose *) m_mime_delivery_state->m_pane)
			  ->GetLineWidth();
		  if (lineWidth > width)
			  width = lineWidth;
	  }
/*JFD
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
JFD */
	  if (m_type) PR_Free (m_type);
	  m_type = m_desired_type;
	  m_desired_type = 0;
	  if (m_encoding) PR_Free (m_encoding);
	  m_encoding = 0;
	}
/* this used to be XP_UNIX? */
#if 0
  else if (m_desired_type &&
		   !PL_strcasecmp (m_desired_type, APPLICATION_POSTSCRIPT) )
	{
      SHIST_SavedData saved_data;

      /* Make sure layout saves the current state of form elements. */
      LO_SaveFormData(m_mime_delivery_state->GetContext());

      /* Hold on to the saved data. */
      memcpy(&saved_data, &m_url->savedData, sizeof(SHIST_SavedData));

	  /* Conversion to postscript desired.
	   */
/*JFD
	  XFE_InitializePrintSetup (&m_print_setup);
	  m_print_setup.url = m_url;
	  m_print_setup.carg = this;
	  m_print_setup.completion = mime_text_attachment_url_exit;
	  m_print_setup.filename = NULL;
	  m_print_setup.out = m_file;
	  m_print_setup.eol = CRLF;
      memset (&m_url->savedData, 0, sizeof (SHIST_SavedData));
	  XL_TranslatePostscript (m_mime_delivery_state->GetContext(),
							  m_url, &saved_data,
							  &m_print_setup);
JFD*/
	  if (m_type) PR_Free (m_type);
	  m_type = m_desired_type;
	  m_desired_type = 0;
	  if (m_encoding) PR_Free (m_encoding);
	  m_encoding = 0;
	}
#endif /* XP_UNIX */
  else
	{
	  m_url->allow_content_change = PR_FALSE;	// don't use modified content

      if (NET_URL_Type (m_url->address) == NEWS_TYPE_URL && !m_url->msg_pane)
		  m_url->msg_pane = m_mime_delivery_state->m_pane;

/*JFD
		  MSG_UrlQueue::AddUrlToPane(m_url, mime_attachment_url_exit,
								 m_mime_delivery_state->m_pane, PR_TRUE,
								 FO_CACHE_AND_MAIL_TO);
JFD*/
	  return 0;
	}
  return status;
}

void nsMsgAttachmentHandler::UrlExit(URL_Struct *url, int status,
										MWContext *context)
{
  char *error_msg = url->error_msg;
  url->error_msg = 0;
  url->fe_data = 0;

  NS_ASSERTION(m_mime_delivery_state != NULL, "not-null m_mime_delivery_state");
  NS_ASSERTION(m_mime_delivery_state->GetContext() != NULL, "not-null context");
  NS_ASSERTION(m_url != NULL, "not-null m_url");

  if (m_graph_progress_started)
	{
	  m_graph_progress_started = PR_FALSE;
	  FE_GraphProgressDestroy (m_mime_delivery_state->GetContext(), m_url,
							   m_url->content_length, m_size);
	}

  if (status < 0)
	/* If any of the attachment URLs fail, kill them all. */
	NET_InterruptWindow (context);

  /* Close the file, but don't delete it (or the file name.) */
  PR_Close (m_file);
  m_file = 0;
  NET_FreeURLStruct (m_url);
  /* I'm pretty sure m_url == url */
  m_url = 0;
  url = 0;



  if (status < 0) {
	  if (m_mime_delivery_state->m_status >= 0)
		m_mime_delivery_state->m_status = status;
	  PR_Delete(m_file_name);
	  PR_FREEIF(m_file_name);
  }

  m_done = PR_TRUE;

  NS_ASSERTION (m_mime_delivery_state->m_attachment_pending_count > 0, "no more pending attachment");
  m_mime_delivery_state->m_attachment_pending_count--;

  if (status >= 0 && m_mime_delivery_state->m_be_synchronous_p)
	{
	  /* Find the next attachment which has not yet been loaded,
		 if any, and start it going.
	   */
	  PRInt32 i;
	  nsMsgAttachmentHandler *next = 0;
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
	  if (status < 0 && error_msg)
		FE_Alert (context, error_msg);
	  */
		
	  if (status < 0)
		{
		  m_mime_delivery_state->Fail(status, error_msg);
		  error_msg = 0;
		}
	}
  PR_FREEIF (error_msg);
}

void
nsMsgAttachmentHandler::AnalyzeDataChunk(const char *chunk, PRInt32 length)
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
nsMsgAttachmentHandler::AnalyzeSnarfedFile(void)
{
	char chunk[256];
	PRFileDesc  *fileHdl = NULL;
	PRInt32 numRead = 0;
	
	if (m_file_name && *m_file_name)
	{
		fileHdl = PR_Open(m_file_name, PR_RDONLY, 0);
		if (fileHdl)
		{
			do
			{
				numRead = PR_Read(fileHdl, chunk, 256);
				if (numRead > 0)
					AnalyzeDataChunk(chunk, numRead);
			}
			while (numRead > 0);
			PR_Close(fileHdl);
		}
	}
}

/* Given a content-type and some info about the contents of the document,
   decide what encoding it should have.
 */
int
nsMsgAttachmentHandler::PickEncoding (const char *charset)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

  // use the boolean so we only have to test for uuencode vs base64 once
  PRBool needsB64 = PR_FALSE;
  PRBool forceB64 = PR_FALSE;

  if (m_already_encoded_p)
	goto DONE;

  /* Allow users to override our percentage-wise guess on whether
	 the file is text or binary */
  if (NS_SUCCEEDED(rv) && prefs) 
    prefs->GetBoolPref ("mail.file_attach_binary", &forceB64);

  if (forceB64 || mime_type_requires_b64_p (m_type))
	{
	  /* If the content-type is "image/" or something else known to be binary,
		 always use base64 (so that we don't get confused by newline
		 conversions.)
	   */
		needsB64 = PR_TRUE;
	}
  else
	{
	  /* Otherwise, we need to pick an encoding based on the contents of
		 the document.
	   */

	  PRBool encode_p;

	  if (m_max_column > 900)
		encode_p = PR_TRUE;
	  else if (UseQuotedPrintable() && m_unprintable_count)
		encode_p = PR_TRUE;

	  else if (m_null_count)	/* If there are nulls, we must always encode,
								   because sendmail will blow up. */
		encode_p = PR_TRUE;
#if 0
	  else if (m_ctl_count)	/* Should we encode if random other control
								   characters are present?  Probably... */
		encode_p = PR_TRUE;
#endif
	  else
		encode_p = PR_FALSE;

	  /* MIME requires a special case that these types never be encoded.
	   */
	  if (!PL_strncasecmp (m_type, "message", 7) ||
		  !PL_strncasecmp (m_type, "multipart", 9))
		{
		  encode_p = PR_FALSE;
		  if (m_desired_type && !PL_strcasecmp (m_desired_type, TEXT_PLAIN))
			{
			  PR_Free (m_desired_type);
			  m_desired_type = 0;
			}
		}
	  /* If the Mail charset is ISO_2022_JP we force it to use Base64 for attachments (bug#104255). 
		Use 7 bit for other STATFUL charsets ( e.g. ISO_2022_KR). */
	  if ((PL_strcasecmp(charset, "iso-2022-jp") == 0) &&
		  (PL_strcasecmp(m_type, TEXT_HTML) == 0))
         needsB64 = PR_TRUE;
	  else if((nsMsgI18Nstateful_charset(charset)) &&
         ((PL_strcasecmp(m_type, TEXT_HTML) == 0) ||
          (PL_strcasecmp(m_type, TEXT_MDL) == 0) ||
          (PL_strcasecmp(m_type, TEXT_PLAIN) == 0) ||
          (PL_strcasecmp(m_type, TEXT_RICHTEXT) == 0) ||
          (PL_strcasecmp(m_type, TEXT_ENRICHED) == 0) ||
          (PL_strcasecmp(m_type, TEXT_VCARD) == 0) ||
          (PL_strcasecmp(m_type, APPLICATION_DIRECTORY) == 0) || /* text/x-vcard synonym */
          (PL_strcasecmp(m_type, TEXT_CSS) == 0) ||
          (PL_strcasecmp(m_type, TEXT_JSSS) == 0) ||
          (PL_strcasecmp(m_type, MESSAGE_RFC822) == 0) ||
          (PL_strcasecmp(m_type, MESSAGE_NEWS) == 0)))
      {
		PR_FREEIF(m_encoding);
        m_encoding = PL_strdup (ENCODING_7BIT);
	  }
	  else if (encode_p &&
               m_size > 500 &&
               m_unprintable_count > (m_size / 10))
		/* If the document contains more than 10% unprintable characters,
		   then that seems like a good candidate for base64 instead of
		   quoted-printable.
		 */
		  needsB64 = PR_TRUE;
	  else if (encode_p) {
		PR_FREEIF(m_encoding);
		m_encoding = PL_strdup (ENCODING_QUOTED_PRINTABLE);
	  }
	  else if (m_highbit_count > 0) {
		PR_FREEIF(m_encoding);
		m_encoding = PL_strdup (ENCODING_8BIT);
	  }
	  else {
		PR_FREEIF(m_encoding);
		m_encoding = PL_strdup (ENCODING_7BIT);
	  }
	}

  if (needsB64)
  {
	  /* 
		 ### mwelch We might have to uuencode instead of 
		            base64 the binary data.
	  */
	  PR_FREEIF(m_encoding);
	  if (UseUUEncode_p())
		  m_encoding = PL_strdup (ENCODING_UUENCODE);
	  else
		  m_encoding = PL_strdup (ENCODING_BASE64);
  }

  /* Now that we've picked an encoding, initialize the filter.
   */
  NS_ASSERTION(!m_encoder_data, "not-null m_encoder_data");
  if (!PL_strcasecmp(m_encoding, ENCODING_BASE64))
	{
	  m_encoder_data = MIME_B64EncoderInit(mime_encoder_output_fn,
										  m_mime_delivery_state);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else if (!PL_strcasecmp(m_encoding, ENCODING_UUENCODE))
	{
		char *tailName = NULL;
		
		if (m_url_string)
		{
		  	tailName = PL_strrchr(m_url_string, '/');
			if (tailName) {
				char * tmp = tailName;
		  		tailName = PL_strdup(tailName+1);
				PR_FREEIF(tmp);
			}
		}
		
		if (m_url && !tailName)
		{
			tailName = PL_strrchr(m_url->address, '/');
			if (tailName) {
				char * tmp = tailName;
				tailName = PL_strdup(tailName+1);
				PR_FREEIF(tmp);
			}
		}

		  m_encoder_data = MIME_UUEncoderInit((char *)(tailName ? tailName : ""),
	  									  mime_encoder_output_fn,
										  m_mime_delivery_state);
	  PR_FREEIF(tailName);
	  if (!m_encoder_data) return MK_OUT_OF_MEMORY;
	}
  else if (!PL_strcasecmp(m_encoding, ENCODING_QUOTED_PRINTABLE))
	{
	  m_encoder_data = MIME_QPEncoderInit(mime_encoder_output_fn,
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
  if (!m_type || !*m_type || !PL_strcasecmp(m_type, UNKNOWN_CONTENT_TYPE))
	{
	  PR_FREEIF(m_type);
	  if (m_already_encoded_p)
		m_type = PL_strdup (APPLICATION_OCTET_STREAM);
	  else if (m_encoding &&
			   (!PL_strcasecmp(m_encoding, ENCODING_BASE64) ||
				!PL_strcasecmp(m_encoding, ENCODING_UUENCODE)))
		m_type = PL_strdup (APPLICATION_OCTET_STREAM);
	  else
		m_type = PL_strdup (TEXT_PLAIN);
	}
  return 0;
}

