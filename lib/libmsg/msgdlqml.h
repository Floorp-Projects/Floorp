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
#ifndef MSG_DELIVER_QUEUED_MAIL_H
#define MSG_DELIVER_QUEUED_MAIL_H

class MSG_DeliverQueuedMailState
{
public:
			MSG_DeliverQueuedMailState(const char *folderPath, MSG_Pane *pane);
	virtual	~MSG_DeliverQueuedMailState();
	void	SetContext(MWContext *context) {m_context = context;}
	int		DeliverMoreQueued(URL_Struct* url);
	int		CloseDeliverQueuedSock(URL_Struct* url);
	int		DeliverDone();
	/* IMAP online Sent Folder support */ 
	
	void    AppendToIMAPSentFolder();
	void    ImapAppendAddBccHeadersIfNeeded(URL_Struct *url);
	void    CreateIMAPSentFolder();
	static  void PostCreateIMAPSentFolder(URL_Struct *url,
										  int status,
										  MWContext *context);
	static  void PostAppendToIMAPSentFolder(URL_Struct *url,
											int status,
											MWContext *context);
	static void PostListIMAPSentFolder(URL_Struct *url,
									   int status,
									   MWContext *context);
	static void PostLiteSelectExitFunction(URL_Struct *url,
									   int status,
									   MWContext *context);
		
protected:
	int		DeliverQueuedMessage ();
	int		DeliverAsNews();
	int		DeliverAsMail();
	int		DoFcc();
	int32	HackHeaders();
	int32	DeliverQueuedLine(char *line, uint32 length);

static void msg_deliver_queued_message_as_news_exit(URL_Struct *url,
													int status,
													MWContext *context);
static void msg_deliver_queued_message_as_mail_exit(URL_Struct *url,
													int status,
													MWContext *context);

	void	DeliverAsMailExit(URL_Struct *url, int status);
	void	DeliverAsNewsExit(URL_Struct *url, int status);

	void	Error(const char *error_msg);

protected:
  MSG_Pane *m_pane;
  MWContext *m_context;
  char *m_folderName;			/* The full path of the queue folder. */
  char *m_tmpName;				/* The full path of the tmp file. */
  XP_File m_infile;				/* The queue file we're parsing. */
  XP_File m_outfile;			/* A tmp file for each message in turn. */

  char *m_bcc, *m_to, *m_newsgroups, *m_fcc;	/* The parsed mail and news recipients */
  char *m_newshost;				/* Optional host on which the groups reside. */
  uint16 m_flags;					/* Flags from the X-Mozilla-Status header */
  int m_status;
  XP_Bool m_done;

  XP_Bool m_awaitingDelivery;	/* This is set while we are waiting for
								   another URL to finish - it is used to
								   make intervening calls to
								   MSG_FinishDeliverQueued() to no-op.
								 */
  XP_Bool m_deliveryFailed;		/* Set when delivery of one message failed,
								   but we want to continue on with the next.
								 */
  int m_deliveryFailureCount;	/* So that we can tell whether it's ok to
								   zero the file at the end.
								 */
  int m_bogusMessageCount;		/* If there are messages in the queue file
								   which oughtn't be there, this counts them.
								 */

  int32 m_unreadMessages;	/* As we walk through the folder, we regenerate */
  int32 m_totalMessages;		/* the summary info. */

  XP_Bool m_inhead;
  uint32 m_position;
  uint32 m_headersPosition;
  uint32 m_flagsPosition;

  char *m_obuffer;
  uint32 m_obufferSize;

  char *m_ibuffer;
  uint32 m_ibufferSize;
  uint32 m_ibufferFP;

  char *m_headers;
  uint32 m_headersSize;
  uint32 m_headersFP;

  /* Thermo stuff
   */
  int32 m_msgNumber;				/* The ordinal of the message in the file
								   (not counting expunged messages.) */
  int32 m_folderSize;			/* How big the queue folder is. */
  int32 m_bytesRead;				/* How many bytes into the file we are. */

	/* IMAP online Sent Folder support */
  MSG_FolderInfoMail *m_localFolderInfo;
  MSG_IMAPFolderInfoMail *m_imapFolderInfo;
  IDArray m_msgKeys;
};

#endif
