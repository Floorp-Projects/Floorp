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

/*JFD
#include "msgpane.h"
#include "mimeenc.h"	// For base64/QP encoder
#include "mhtmlstm.h"
*/
#include "rosetta_mailnews.h"
#include "msgCore.h"
#include "prprf.h" /* should be defined into msgCore.h? */
#include "net.h" /* should be defined into msgCore.h? */
#include "intl_csi.h"
//#include "msgcom.h"
#include "nsFileStream.h"
#include "nsMsgMessageFlags.h"
#include "MsgCompGlue.h"
#include "nsIMsgSend.h"

class MSG_DeliverMimeAttachment;
class ParseOutgoingMessage;
class MailDB;

class nsMsgSendPart;
class nsMsgCompFields;

#define MIME_BUFFER_SIZE		4096
#define FCC_FAILURE				0
#define FCC_BLOCKING_SUCCESS	1
#define FCC_ASYNC_SUCCESS		2

#if 0 //JFD - We shouldn't use it anymore...
extern "C" void
msg_StartMessageDeliveryWithAttachments (MSG_Pane *pane,
										 void      *fe_data,
										 nsMsgCompFields *fields,
										 PRBool digest_p,
										 PRBool dont_deliver_p,
										 MSG_Deliver_Mode mode,
										 const char *attachment1_type,
										 const char *attachment1_body,
										 PRUint32 attachment1_body_length,
										 const struct MSG_AttachedFile *attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
										 void *relatedPart,
//#endif
										 void (*message_delivery_done_callback)
										      (MWContext *context,
											   void *fe_data,
											   int status,
											   const char *error_message),
										 const char *smtp
											   );
#endif //JFD

class nsMsgSendMimeDeliveryState : public nsIMsgSend
{
public:
	nsMsgSendMimeDeliveryState();
	virtual ~nsMsgSendMimeDeliveryState();

	/* this macro defines QueryInterface, AddRef and Release for this class */
	NS_DECL_ISUPPORTS

	NS_IMETHOD SendMessage(nsIMsgCompFields *fields, const char *smtp);

    void	StartMessageDelivery(MSG_Pane *pane,
									 void      *fe_data,
									 nsMsgCompFields *fields,
									 PRBool digest_p,
									 PRBool dont_deliver_p,
									 MSG_Deliver_Mode mode,
									 const char *attachment1_type,
									 const char *attachment1_body,
									 PRUint32 attachment1_body_length,
									 const struct MSG_AttachmentData
									   *attachments,
									 const struct MSG_AttachedFile
									   *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
									 nsMsgSendPart *relatedPart,
//#endif
									 void (*message_delivery_done_callback)
									      (MWContext *context,
										   void *fe_data,
										   int status,
										   const char *error_message));

  int	Init(MSG_Pane *pane,
			 void      *fe_data,
			 nsMsgCompFields *fields,
			 PRBool digest_p,
			 PRBool dont_deliver_p,
			 MSG_Deliver_Mode mode,
			 const char *attachment1_type,
			 const char *attachment1_body,
			 PRUint32 attachment1_body_length,
			 const struct MSG_AttachmentData *attachments,
			 const struct MSG_AttachedFile *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
			 nsMsgSendPart *relatedPart,
//#endif
			 void (*message_delivery_done_callback)
			 (MWContext *context,
			  void *fe_data,
			  int status,
			  const char *error_message));

  void	StartMessageDelivery();
  int	GatherMimeAttachments();
  void	DeliverMessage();
  void	QueueForLater();
  void  SaveAsDraft();
  void  SaveAsTemplate();
  PRBool IsSaveMode();

  HJ77514

  /* #### SendToMagicFolder(int flag) should be protected or private */
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

  int DoFcc();

  int HackAttachments(const struct MSG_AttachmentData *attachments,
					  const struct MSG_AttachedFile *preloaded_attachments);

  void	DeliverFileAsMail();
  void	DeliverFileAsNews();
  void	DeliverAsMailExit(URL_Struct *url, int status);
  void	DeliverAsNewsExit(URL_Struct *url, int status);
  void	Fail(int failure_code, char *error_msg);
  void	Clear();

  HJ58534
	  MWContext *GetContext() { return (m_pane ? m_pane->GetContext(): NULL); }

  int SetMimeHeader(MSG_HEADER_SET header, const char *value);
  void SetIMAPMessageUID(MessageKey key);

  MSG_Pane *m_pane;			/* Pane to use when loading the URLs */
  void *m_fe_data;			/* passed in and passed to callback */
  nsMsgCompFields *m_fields;

  PRBool m_dont_deliver_p;	/* If set, we just return the name of the file
							   we created, instead of actually delivering
							   this message. */

  MSG_Deliver_Mode m_deliver_mode; /* MSG_DeliverNow, MSG_QueueForLater,
									  MSG_SaveAsDraft, MSG_SaveAsTemplate
									*/

  PRBool m_attachments_only_p;	/* If set, then we don't construct a complete
								   MIME message; instead, we just retrieve the
								   attachments from the network, store them in
								   tmp files, and return a list of
								   MSG_AttachedFile structs which describe
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
  MSG_DeliverMimeAttachment* m_plaintext;

//#ifdef MSG_SEND_MULTIPART_RELATED
	// The multipart/related save object for HTML text.
  nsMsgSendPart *m_related_part;
//#endif

  // File where we stored our HTML so that we could make the plaintext form.
  char* m_html_filename;

  /* Subsequent attachments, if any.
   */
  PRInt32 m_attachment_count;
  PRInt32 m_attachment_pending_count;
  MSG_DeliverMimeAttachment *m_attachments;
  PRInt32 m_status; /* in case some attachments fail but not all */

  /* The caller's `exit' method. */
  void (*m_message_delivery_done_callback) (MWContext *context,
											void * fe_data, int status,
											const char * error_msg);

  /* The exit method used when downloading attachments only. */
  void (*m_attachments_done_callback) (MWContext *context,
									   void * fe_data, int status,
									   const char * error_msg,
									   struct MSG_AttachedFile *attachments);

  char *m_msg_file_name;				/* Our temporary file */
  nsOutputFileStream * m_msg_file;

  MSG_IMAPFolderInfoMail *m_imapFolderInfo;
  ParseOutgoingMessage *m_imapOutgoingParser;
  MailDB *m_imapLocalMailDB;
  

#if 0
  /*  char *headers;					/ * The headers of the message */

  /* Some state to control the thermometer during message delivery.
   */
  PRInt32 m_msg_size;					/* Size of the final message. */
  PRInt32 m_delivery_total_bytes;		/* How many bytes we will be delivering:
									   for example, if we're sending the
									   message to both mail and news, this
									   will be 2x the size of the message.
									 */
  PRInt32 m_delivery_bytes;			/* How many bytes we have delivered so far.
									 */
#endif /* 0 */
};

class MSG_DeliverMimeAttachment
{
public:

  MSG_DeliverMimeAttachment();
  ~MSG_DeliverMimeAttachment();

  void	UrlExit(URL_Struct *url, int status, MWContext *context);
  PRInt32	SnarfAttachment ();
  void  AnalyzeDataChunk (const char *chunk, PRInt32 chunkSize);
  void  AnalyzeSnarfedFile ();      /* Analyze a previously-snarfed file.
  									   (Currently only used for plaintext
  									   converted from HTML.) */
  int	PickEncoding (PRInt16 mail_csid);
  
  PRBool UseUUEncode_p(void);

  char *m_url_string;
  URL_Struct *m_url;
  PRBool m_done;

  nsMsgSendMimeDeliveryState		*m_mime_delivery_state;

  char *m_type;						/* The real type, once we know it. */
  char *m_override_type;			/* The type we should assume it to be
									   or 0, if we should get it from the
									   URL_Struct (from the server) */
  char *m_override_encoding;		/* Goes along with override_type */

  char *m_desired_type;				/* The type it should be converted to. */
  char *m_description;				/* For Content-Description header */
  char *m_x_mac_type, *m_x_mac_creator; /* Mac file type/creator. */
  char *m_real_name;				/* The name for the headers, if different
									   from the URL. */
  char *m_encoding;					/* The encoding, once we've decided. */
  PRBool m_already_encoded_p;		/* If we attach a document that is already
									   encoded, we just pass it through. */

  char *m_file_name;					/* The temp file to which we save it */
  XP_File m_file;

#ifdef XP_MAC
  char *m_ap_filename;				/* The temp file holds the appledouble
									   encoding of the file we want to post. */
#endif

  PRBool m_decrypted_p;	/* S/MIME -- when attaching a message that was
							   encrypted, it's necessary to decrypt it first
							   (since nobody but the original recipient can
							   read it -- if you forward it to someone in the
							   raw, it will be useless to them.)  This flag
							   indicates whether decryption occurred, so that
							   libmsg can issue appropriate warnings about
							   doing a cleartext forward of a message that was
							   originally encrypted.
							 */
  PRUint32 m_size;					/* Some state used while filtering it */
  PRUint32 m_unprintable_count;
  PRUint32 m_highbit_count;
  PRUint32 m_ctl_count;
  PRUint32 m_null_count;
  PRUint32 m_current_column;
  PRUint32 m_max_column;
  PRUint32 m_lines;

  MimeEncoderData *m_encoder_data;  /* Opaque state for base64/qp encoder. */

  PRBool m_graph_progress_started;

/*JFD
  PrintSetup m_print_setup;	*/		/* Used by HTML->Text and HTML->PS */
};

extern char * msg_generate_message_id (void);

// These routines should only be used by the nsMsgSendPart class.

extern PRBool mime_type_needs_charset (const char *type);
extern int mime_write_message_body(nsMsgSendMimeDeliveryState *state,
								   char *buf, PRInt32 size);
extern char* mime_get_stream_write_buffer(void);

#endif /*  __MSGSEND_H__ */
