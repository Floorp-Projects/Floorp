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

#ifndef __MSGSEND_H__
#define __MSGSEND_H__

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


//
// Necessary includes
//
#include "msgCore.h"
#include "prprf.h" 
#include "rosetta_mailnews.h"
#include "nsFileStream.h"
#include "nsMsgMessageFlags.h"
#include "MsgCompGlue.h"
#include "nsIMsgSend.h"
#include "nsIURL.h"
#include "nsMsgAttachmentHandler.h"
#include "nsMsgCompFields.h"
#include "nsMsgComposeBE.h"

#include "net.h" /* should be defined into msgCore.h? */
// RICHIE   #include "intl_csi.h"


//
// Some necessary defines...
//
#define TEN_K                 10240
#define MIME_BUFFER_SIZE		  4096
#define FCC_FAILURE				    0
#define FCC_BLOCKING_SUCCESS	1
#define FCC_ASYNC_SUCCESS		  2

//
// Utilities for string handling
//
#define PUSH_STRING(S) \
 do { PL_strcpy (buffer_tail, S); buffer_tail += PL_strlen (S); } while(0)
#define PUSH_NEWLINE() \
 do { *buffer_tail++ = CR; *buffer_tail++ = LF; *buffer_tail = '\0'; } while(0)




  //
  ////////////////////////////////////////////////////////////////////////////////
  // REWORK THIS STUF!!!!!!!!!
  // REWORK THIS STUF!!!!!!!!!
  // REWORK THIS STUF!!!!!!!!!
  ////////////////////////////////////////////////////////////////////////////////
  //



//
// Forward declarations...
//
class ParseOutgoingMessage;
class MailDB;
class nsMsgSendPart;

class nsMsgComposeAndSend : public nsIMsgSend
{
public:
  int	GatherMimeAttachments();
  void	DeliverMessage();
  void	QueueForLater();
  void  SaveAsDraft();
  void  SaveAsTemplate();
  PRBool IsSaveMode();

  HJ77514

  void    SendToMagicFolder (PRUint32 flag);
  int	  SendToImapMagicFolder(PRUint32 flag);
  void    ImapAppendAddBccHeadersIfNeeded(URL_Struct *url);
  static  void PostSendToImapMagicFolder (URL_Struct *url,
										  int status,
										  MWContext *context);
  static  void PostCreateImapMagicFolder (URL_Struct *url,
										  int status,
										  MWContext *context);
  static void PostListImapMailboxFolder (URL_Struct *url,
										 int status,
										 MWContext *context);
  static void PostSubscribeImapMailboxFolder (URL_Struct *url,
											  int status,
											  MWContext *context);
  PRUint32 GetFolderFlagAndDefaultName(const char **defaultName);
  /* caller needs to free the returned folder name */
  char *GetOnlineFolderName(PRUint32 flag, const char **pDefaultName);

  int InitImapOfflineDB(PRUint32 flag);
  int SaveAsOfflineOp();


  int HackAttachments(const struct nsMsgAttachmentData *attachments,
					  const struct nsMsgAttachedFile *preloaded_attachments);


  HJ58534

  int SetMimeHeader(MSG_HEADER_SET header, const char *value);
  void SetIMAPMessageUID(MessageKey key);

  MSG_Pane *m_pane;			/* Pane to use when loading the URLs */

  PRBool m_attachments_only_p;	/* If set, then we don't construct a complete
								   MIME message; instead, we just retrieve the
								   attachments from the network, store them in
								   tmp files, and return a list of
								   nsMsgAttachedFile structs which describe
								   them. */

  PRBool m_pre_snarfed_attachments_p;	/* If true, then the attachments were
										   loaded by msg_DownloadAttachments()
										   and therefore we shouldn't delete
										   the tmp files (but should leave
										   that to the caller.) */

  PRBool m_digest_p;			/* Whether to be multipart/digest instead of
								   multipart/mixed. */

  PRBool m_be_synchronous_p;	/* If true, we will load one URL after another,
								   rather than starting all URLs going at once
								   and letting them load in parallel.  This is
								   more efficient if (for example) all URLs are
								   known to be coming from the same news server
								   or mailbox: loading them in parallel would
								   cause multiple connections to the news
								   server to be opened, or would cause much
								   seek()ing.
								 */

  void *m_crypto_closure;		/* State used by composec.c */

  /* The first attachment, if any (typed in by the user.)
   */
  char *m_attachment1_type;
  char *m_attachment1_encoding;
  MimeEncoderData *m_attachment1_encoder_data;
  char *m_attachment1_body;
  PRUint32 m_attachment1_body_length;

  // The plaintext form of the first attachment, if needed.
  nsMsgAttachmentHandler* m_plaintext;

	// The multipart/related save object for HTML text.
  nsMsgSendPart *m_related_part;

  // File where we stored our HTML so that we could make the plaintext form.
  char* m_html_filename;

  /* Subsequent attachments, if any.
   */
  PRInt32 m_attachment_count;
  PRInt32 m_attachment_pending_count;
  nsMsgAttachmentHandler *m_attachments;
  PRInt32 m_status; /* in case some attachments fail but not all */

  /* The exit method used when downloading attachments only. */
  void (*m_attachments_done_callback) (
									   void * fe_data, int status,
									   const char * error_msg,
									   struct nsMsgAttachedFile *attachments);


  MSG_IMAPFolderInfoMail *m_imapFolderInfo;
  ParseOutgoingMessage *m_imapOutgoingParser;
  MailDB *m_imapLocalMailDB;
  
  //
  ////////////////////////////////////////////////////////////////////////////////
  // RICHIE - These are all calls that have been converted to the new world order!
  // Once all done, we will reorganize the layout of the class definition for readability.
  // Please bear with me.
  ////////////////////////////////////////////////////////////////////////////////
  //

  //
  // Define QueryInterface, AddRef and Release for this class 
  //
	NS_DECL_ISUPPORTS

  nsMsgComposeAndSend();
	virtual     ~nsMsgComposeAndSend();

  // Delivery and completion callback routines...
  void	      DeliverFileAsMail();
  void	      DeliverFileAsNews();
  void	      DeliverAsMailExit(nsIURL *aUrl, nsresult aExitCode);
  void	      DeliverAsNewsExit(nsIURL *aUrl, nsresult aExitCode);


  void	      Fail(nsresult failure_code, char *error_msg);
  nsresult    DoFcc();
  void	      Clear();

  // Message creation routines
  //
  
  // Init() will allow for either message creation without delivery or full
  // message creation and send operations
  //
  nsresult    Init(
			             nsMsgCompFields  *fields,
			             PRBool           digest_p,
			             PRBool           dont_deliver_p,
			             nsMsgDeliverMode mode,
			             const char       *attachment1_type,
			             const char       *attachment1_body,
			             PRUint32         attachment1_body_length,
			             const struct nsMsgAttachmentData   *attachments,
			             const struct nsMsgAttachedFile     *preloaded_attachments,
			             nsMsgSendPart    *relatedPart,
                   void             *fe_data);

  //
  // Setup the composition fields
  //
  nsresult    InitCompositionFields(nsMsgCompFields *fields);

  //
  ////////////////////////////////////////////////////////////////////////////////
  // RICHIE - These are partly new world, but still need work...still they are better
  // than before. Again...Please bear with me.
  ////////////////////////////////////////////////////////////////////////////////
  //
  nsresult    MimeDoFCC (
			                   nsFileSpec *input_file,  XP_FileType input_file_type,
			                   const char *output_name, XP_FileType output_file_type,
			                   nsMsgDeliverMode mode,
			                   const char *bcc_header,
			                   const char *fcc_header,
			                   const char *news_url);

  ////////////////////////////////////////////////////////////////////////////////
  // The current nsIMsgSend Interfaces exposed to the world!
  ////////////////////////////////////////////////////////////////////////////////
  NS_IMETHOD  CreateAndSendMessage(
 						              nsIMsgCompFields                  *fields,
						              PRBool                            digest_p,
						              PRBool                            dont_deliver_p,
						              nsMsgDeliverMode                  mode,
						              const char                        *attachment1_type,
						              const char                        *attachment1_body,
						              PRUint32                          attachment1_body_length,
						              const struct nsMsgAttachmentData  *attachments,
						              const struct nsMsgAttachedFile    *preloaded_attachments,
						              void                              *relatedPart,
                          nsMsgSendCompletionCallback       completionCallback,
                          void                              *tagData);

  //
  // All vars necessary for this implementation
  //
  nsMsgCompFields         *mCompFields;         // All needed composition fields (header, etc...)
  nsFileSpec              *mTempFileSpec;       // our temporary file
  
  nsOutputFileStream      *mOutputFile;         // the actual output file stream

  PRBool                  m_dont_deliver_p;     // If set, we just return the nsFileSpec of the file
							                                  // created, instead of actually delivering message.
  nsMsgDeliverMode        m_deliver_mode;       // nsMsgDeliverNow, nsMsgQueueForLater, nsMsgSaveAsDraft, 
                                                // nsMsgSaveAsTemplate

  // These are needed for callbacks to the FE...  
  nsMsgSendCompletionCallback   mSendCompleteCallback;  // Used for completion of actual send operations
  void                    *m_fe_data;			      // passed in and passed to callback 
  nsFileSpec              *mReturnFileSpec;     // a holder for file spec's to be returned to caller
};

// 
// These C routines should only be used by the nsMsgSendPart class.
//
extern int    mime_write_message_body(nsMsgComposeAndSend *state, char *buf, PRInt32 size);
extern char   *mime_get_stream_write_buffer(void);
extern int    mime_encoder_output_fn (const char *buf, PRInt32 size, void *closure);
extern PRBool UseQuotedPrintable(void);

#endif /*  __MSGSEND_H__ */
