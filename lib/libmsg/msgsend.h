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

#ifndef __MSGSEND_H__
#define __MSGSEND_H__

#include "msgpane.h"
#include "mimeenc.h"	/* For base64/QP encoder */
#include "mhtmlstm.h"
#include "rosetta.h"

class MSG_DeliverMimeAttachment;
class ParseOutgoingMessage;
class MailDB;

#define MIME_BUFFER_SIZE 4096

class MSG_SendMimeDeliveryState
{
public:
  MSG_SendMimeDeliveryState();
  ~MSG_SendMimeDeliveryState();

  static void	StartMessageDelivery(MSG_Pane *pane,
									 void      *fe_data,
									 MSG_CompositionFields *fields,
									 XP_Bool digest_p,
									 XP_Bool dont_deliver_p,
									 MSG_Deliver_Mode mode,
									 const char *attachment1_type,
									 const char *attachment1_body,
									 uint32 attachment1_body_length,
									 const struct MSG_AttachmentData
									   *attachments,
									 const struct MSG_AttachedFile
									   *preloaded_attachments,
//#ifdef MSG_SEND_MULTIPART_RELATED
									 MSG_SendPart *relatedPart,
//#endif
									 void (*message_delivery_done_callback)
									      (MWContext *context,
										   void *fe_data,
										   int status,
										   const char *error_message));

  int	Init(MSG_Pane *pane,
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
			  const char *error_message));

  void	StartMessageDelivery();
  int	GatherMimeAttachments();
  void	DeliverMessage();
  void	QueueForLater();
  void  SaveAsDraft();
  void  SaveAsTemplate();

  /* #### SendToMagicFolder(int flag) should be protected or private */
  void    SendToMagicFolder (uint32 flag);
  void	  SendToImapMagicFolder(uint32 flag);
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
  /* caller needs to free the returned folder name */
  char *GetOnlineFolderName(uint32 flag, const char **pDefaultName);

  int InitImapOfflineDB(uint32 flag);
  void SaveAsOfflineOp();

  XP_Bool DoFcc();

  int HackAttachments(const struct MSG_AttachmentData *attachments,
					  const struct MSG_AttachedFile *preloaded_attachments);

  void	DeliverFileAsMail();
  void	DeliverFileAsNews();
  void	DeliverAsMailExit(URL_Struct *url, int status);
  void	DeliverAsNewsExit(URL_Struct *url, int status);
  void	Fail(int failure_code, char *error_msg);
  void	Clear();

  // Callback from msgsendp.cpp into msgsend.cpp.
  HG89377
  MWContext *GetContext() { return m_pane->GetContext(); }

  int SetMimeHeader(MSG_HEADER_SET header, const char *value);
  void SetIMAPMessageUID(MessageKey key);

  MSG_Pane *m_pane;			/* Pane to use when loading the URLs */
  void *m_fe_data;			/* passed in and passed to callback */
  MSG_CompositionFields *m_fields;

  XP_Bool m_dont_deliver_p;	/* If set, we just return the name of the file
							   we created, instead of actually delivering
							   this message. */

  MSG_Deliver_Mode m_deliver_mode; /* MSG_DeliverNow, MSG_QueueForLater,
									  MSG_SaveAsDraft, MSG_SaveAsTemplate
									*/

  XP_Bool m_attachments_only_p;	/* If set, then we don't construct a complete
								   MIME message; instead, we just retrieve the
								   attachments from the network, store them in
								   tmp files, and return a list of
								   MSG_AttachedFile structs which describe
								   them. */

  XP_Bool m_pre_snarfed_attachments_p;	/* If true, then the attachments were
										   loaded by msg_DownloadAttachments()
										   and therefore we shouldn't delete
										   the tmp files (but should leave
										   that to the caller.) */

  XP_Bool m_digest_p;			/* Whether to be multipart/digest instead of
								   multipart/mixed. */

  XP_Bool m_be_synchronous_p;	/* If true, we will load one URL after another,
								   rather than starting all URLs going at once
								   and letting them load in parallel.  This is
								   more efficient if (for example) all URLs are
								   known to be coming from the same news server
								   or mailbox: loading them in parallel would
								   cause multiple connections to the news
								   server to be opened, or would cause much
								   seek()ing.
								 */

  HG83623

  /* The first attachment, if any (typed in by the user.)
   */
  char *m_attachment1_type;
  char *m_attachment1_encoding;
  MimeEncoderData *m_attachment1_encoder_data;
  char *m_attachment1_body;
  uint32 m_attachment1_body_length;

  // The plaintext form of the first attachment, if needed.
  MSG_DeliverMimeAttachment* m_plaintext;

//#ifdef MSG_SEND_MULTIPART_RELATED
	// The multipart/related save object for HTML text.
  MSG_SendPart *m_related_part;
//#endif

  // File where we stored our HTML so that we could make the plaintext form.
  char* m_html_filename;

  /* Subsequent attachments, if any.
   */
  int32 m_attachment_count;
  int32 m_attachment_pending_count;
  MSG_DeliverMimeAttachment *m_attachments;
  int32 m_status; /* in case some attachments fail but not all */

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
  XP_File m_msg_file;

  MSG_IMAPFolderInfoMail *m_imapFolderInfo;
  ParseOutgoingMessage *m_imapOutgoingParser;
  MailDB *m_imapLocalMailDB;
  

#if 0
  /*  char *headers;					/ * The headers of the message */

  /* Some state to control the thermometer during message delivery.
   */
  int32 m_msg_size;					/* Size of the final message. */
  int32 m_delivery_total_bytes;		/* How many bytes we will be delivering:
									   for example, if we're sending the
									   message to both mail and news, this
									   will be 2x the size of the message.
									 */
  int32 m_delivery_bytes;			/* How many bytes we have delivered so far.
									 */
#endif /* 0 */
};

class MSG_DeliverMimeAttachment
{
public:

  MSG_DeliverMimeAttachment();
  ~MSG_DeliverMimeAttachment();

  void	UrlExit(URL_Struct *url, int status, MWContext *context);
  int32	SnarfAttachment ();
  void  AnalyzeDataChunk (const char *chunk, int32 chunkSize);
  void  AnalyzeSnarfedFile ();      /* Analyze a previously-snarfed file.
  									   (Currently only used for plaintext
  									   converted from HTML.) */
  int	PickEncoding (int16 mail_csid);
  
  XP_Bool UseUUEncode_p(void);

  char *m_url_string;
  URL_Struct *m_url;
  XP_Bool m_done;

  MSG_SendMimeDeliveryState		*m_mime_delivery_state;

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
  XP_Bool m_already_encoded_p;		/* If we attach a document that is already
									   encoded, we just pass it through. */

  char *m_file_name;					/* The temp file to which we save it */
  XP_File m_file;

#ifdef XP_MAC
  char *m_ap_filename;				/* The temp file holds the appledouble
									   encoding of the file we want to post. */
#endif

  HG93873
  uint32 m_size;					/* Some state used while filtering it */
  uint32 m_unprintable_count;
  uint32 m_highbit_count;
  uint32 m_ctl_count;
  uint32 m_null_count;
  uint32 m_current_column;
  uint32 m_max_column;
  uint32 m_lines;

  MimeEncoderData *m_encoder_data;  /* Opaque state for base64/qp encoder. */

  XP_Bool m_graph_progress_started;

  PrintSetup m_print_setup;			/* Used by HTML->Text and HTML->PS */
};
// These routines should only be used by the MSG_SendPart class.

extern XP_Bool mime_type_needs_charset (const char *type);
extern int mime_write_message_body(MSG_SendMimeDeliveryState *state,
								   char *buf, int32 size);
extern char* mime_get_stream_write_buffer(void);

#endif /*  __MSGSEND_H__ */
