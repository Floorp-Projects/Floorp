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

// This file contains the deliver queued mail state machine
#include "rosetta.h"
#include "msg.h"

#include "xp_str.h"
#include "merrors.h"
#include "msgtpane.h"
#include "msgmast.h"
#include "maildb.h"
#include "msgfinfo.h"
#include "msgimap.h"
#include "msgprefs.h"
#include "msgurlq.h"
#include "mailhdr.h"
#include "prefapi.h"
#include "imaphost.h"

#include "msgdlqml.h"

extern "C"
{
#include "xpgetstr.h"
	extern int MK_MIME_ERROR_WRITING_FILE;
	/* extern int MK_MSG_CANT_COPY_TO_DRAFTS_FOLDER; */
	extern int MK_MSG_CANT_COPY_TO_QUEUE_FOLDER;
	extern int MK_MSG_CANT_COPY_TO_QUEUE_FOLDER_OLD;
	extern int MK_MSG_CANT_COPY_TO_SAME_FOLDER;
	extern int MK_MSG_CANT_CREATE_INBOX;
	extern int MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	extern int MK_MSG_FOLDER_SUMMARY_UNREADABLE;
	extern int MK_MSG_FOLDER_UNREADABLE;
	extern int MK_MSG_NON_MAIL_FILE_WRITE_QUESTION;
	extern int MK_MSG_NO_POP_HOST;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_UNABLE_TO_OPEN_FILE;
	extern int MK_MSG_COULDNT_OPEN_FCC_FILE;  
	extern int MK_MSG_COMPRESS_FOLDER_ETC;
	extern int MK_MSG_COMPRESS_FOLDER_DONE;
	extern int MK_MSG_CANT_OPEN;
	extern int MK_MSG_BOGUS_QUEUE_MSG_1;
	extern int MK_MSG_BOGUS_QUEUE_MSG_N;
	extern int MK_MSG_BOGUS_QUEUE_MSG_1_OLD;
	extern int MK_MSG_BOGUS_QUEUE_MSG_N_OLD;
	extern int MK_MSG_BOGUS_QUEUE_REASON;
	extern int MK_MSG_WHY_QUEUE_SPECIAL;
	extern int MK_MSG_WHY_QUEUE_SPECIAL_OLD;
	extern int MK_MSG_QUEUED_DELIVERY_FAILED;
	extern int MK_MSG_DELIVERING_MESSAGE_TO;
	extern int MK_MSG_DELIVERING_MESSAGE;

/* #### Why is this alegedly netlib-specific?   What else am I to do? */
extern char *NET_ExplainErrorDetails (int code, ...);

}

#define UNHEX(C) \
	  ((C >= '0' && C <= '9') ? C - '0' : \
	   ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
		((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))

MSG_DeliverQueuedMailState::MSG_DeliverQueuedMailState(const char *folderPath, MSG_Pane *pane)
{
	m_context = NULL;
	m_folderName = XP_STRDUP(folderPath);
	m_tmpName = NULL;
	m_infile = 0;
	m_outfile = 0;

	m_bcc = m_to = m_newsgroups = m_fcc = NULL;	
	m_newshost = NULL;
	m_flags = 0;
	m_status = 0;
	m_done = FALSE;

	m_awaitingDelivery = FALSE;
	m_deliveryFailed = FALSE;
	m_deliveryFailureCount = 0;
	m_bogusMessageCount = 0;
	m_unreadMessages = 0;
	m_totalMessages = 0;

	m_inhead = FALSE;
	m_position = 0;
	m_headersPosition = 0;
	m_flagsPosition = 0;

	m_obuffer = NULL;
	m_obufferSize = 0;

	m_ibuffer = NULL;
	m_ibufferSize = 0;
	m_ibufferFP = 0;

	m_headers = NULL;
	m_headersSize = 0;
	m_headersFP = 0;

	m_msgNumber = 0;
	m_folderSize = 0;
	m_bytesRead = 0;

	m_pane = pane;
	
	m_imapFolderInfo = NULL;
	m_localFolderInfo = NULL;

	m_msgKeys.Add(0);	// dummy message id in case we are appending to online Sent folder
}

MSG_DeliverQueuedMailState::~MSG_DeliverQueuedMailState()
{
}

/* static */ void MSG_DeliverQueuedMailState::PostLiteSelectExitFunction( URL_Struct *url,
										int status,
										MWContext *context)
{
	MSG_FolderInfoMail *folderInfo = (MSG_IMAPFolderInfoMail *)url->fe_data;
	if (folderInfo)
		url->msg_pane->SetLoadingImapFolder(folderInfo->GetIMAPFolderInfoMail());
	url->fe_data = NULL;
	ImapFolderSelectCompleteExitFunction(url, status, context);
	NET_FreeURLStruct(url);
}

int MSG_DeliverQueuedMailState::DeliverMoreQueued(URL_Struct* url)
{
	XP_StatStruct folderst;
	char* dbName;
	char *line;

	if (m_done)
		goto ALLDONE;

	/* If we're waiting for our post to a "mailto:" or "news:" URL to
	   complete, just return.  We'll be called again after the current
	   batch of stuff is serviced, and eventually this will go FALSE.

	   #### I think this means that, though we will be servicing user
	   events, netlib will be running full-tilt until the message is
	   delivered, calling this function as frequently as it can.
	   That's kind of bad...
	 */
	if (m_awaitingDelivery)
		return MK_WAITING_FOR_CONNECTION;

	if (m_obuffer == NULL) 
	{
		/* First time through. */

		if (XP_Stat (m_folderName, &folderst, xpMailFolder)) 
		{
			/* Can't stat the file; must not have any queued messages. */
			goto ALLDONE;
		}

		m_folderSize = folderst.st_size;
		if (m_folderSize)
			FE_GraphProgressInit(m_context, url, m_folderSize);

		m_obufferSize = 8192;
		while (!m_obuffer) 
		{
			m_obufferSize /= 2;
			m_obuffer = (char *) XP_ALLOC (m_obufferSize);
			if (!m_obuffer && m_obufferSize < 2) 
			{
				m_status = MK_OUT_OF_MEMORY;
				goto FAIL;
			}
		}

		m_infile = XP_FileOpen(m_folderName, xpMailFolder,
									XP_FILE_READ_BIN);
		if (!m_infile) goto ALLDONE;

		return MK_WAITING_FOR_CONNECTION;
	}

	if (m_infile)
	{
		/* We're in the middle of parsing the folder.  Do a line.
			 (It would be nice to use msg_LineBuffer() here, but because
			 of the goofy flow of control, that won't work.)
		 */
		line = XP_FileReadLine(m_obuffer, m_obufferSize,
								 m_infile);
		if (!line)
			/* Finally finished parsing.  Close and clean up. */
			goto ALLDONE;

		m_status = DeliverQueuedLine (line, XP_STRLEN(line));
		if (m_status < 0) 
			goto FAIL;

		return MK_WAITING_FOR_CONNECTION;
	}

ALLDONE:
	if (m_infile)
	{
		XP_FileClose(m_infile);
		m_infile = NULL;

		if (!m_inhead &&  m_headersFP > 0 &&  m_status >= 0)
		{
		  /* Ah, not really done now, are we? */
			m_status = DeliverQueuedMessage ();
			if (m_awaitingDelivery)
				return MK_WAITING_FOR_CONNECTION;
		}
	}

	/* Actually truncate the queue file if all messages in it have been
		 delivered successfully. */
	if (m_status >= 0 && m_deliveryFailureCount == 0 && m_bogusMessageCount == 0)
	{
	  /* If there is a summary file, it is now out of date.  Delete it,
		and recreate a new empty one.
	   */

		dbName = WH_FileName(m_folderName, xpMailFolderSummary);
		if (dbName)
		{
			MailDB *db =  (MailDB *) MessageDB::FindInCache(dbName);
			XP_FREEIF(dbName);
			if (db)
			{
				ListContext		*listContext = NULL;
				MsgERR			dbErr;
				DBMessageHdr	*pHeader;
				MSG_Pane		*threadPane = (m_pane ?
					m_pane->GetMaster()->FindThreadPaneNamed(m_folderName) :
					(MSG_Pane *)NULL);
					
				if (threadPane)
					threadPane->StartingUpdate(MSG_NotifyNone, 0, 0);
				dbErr = db->ListFirst(&listContext, &pHeader);
				while (dbErr == eSUCCESS && pHeader != NULL)
				{
					// can't delete hdr that iterator is on, so save it.
					DBMessageHdr *saveHdr = pHeader;
					dbErr = db->ListNext(listContext, &pHeader);

					if (dbErr == eDBEndOfList)
						db->ListDone(listContext);

					if (saveHdr)
						db->DeleteHeader(saveHdr, NULL, FALSE);
					delete saveHdr;
				}

				if (threadPane)
					threadPane->EndingUpdate(MSG_NotifyNone, 0, 0);
				// MessageDB::FindInCache() does not add refCount so don't close
				// db->Close();
				db = NULL;
				if (m_imapFolderInfo)
				{
					MSG_Pane *urlPane = NULL;

					if (!urlPane)
						m_pane->GetMaster()->FindPaneOfType(m_imapFolderInfo, MSG_THREADPANE);

					if (!urlPane)
						urlPane = m_pane;

					dbName = WH_FileName (m_imapFolderInfo->GetPathname(), xpMailFolderSummary);
					if (dbName)
						db = (MailDB *) MessageDB::FindInCache(dbName);
					XP_FREEIF(dbName);
					if (db && urlPane)
					{
						char *url = CreateImapMailboxLITESelectUrl(
							m_imapFolderInfo->GetHostName(),
							m_imapFolderInfo->GetOnlineName(),
							m_imapFolderInfo->GetOnlineHierarchySeparator());
						if (url)
						{
							URL_Struct *url_struct =
								NET_CreateURLStruct(url,
													NET_NORMAL_RELOAD);
							if (url_struct)
							{
								m_imapFolderInfo->SetFolderLoadingContext(urlPane->GetContext());
								urlPane->SetLoadingImapFolder(m_imapFolderInfo);
								url_struct->fe_data = (void *) m_imapFolderInfo;
								url_struct->internal_url = TRUE;
								url_struct->msg_pane = urlPane;
								urlPane->GetContext()->imapURLPane = urlPane;

								MSG_UrlQueue::AddUrlToPane (url_struct,
															PostLiteSelectExitFunction,
															urlPane, TRUE);

							}
							XP_FREEIF(url);
						}
						// MessageDB::FindInCache() does not add refCount so don't call close
						// db->Close();
					}
				}
			}
			else
			{
				XP_FileRemove(m_folderName, xpMailFolderSummary);
				// create emtpy, blank db
				MailDB::Open(m_folderName,TRUE, &db, FALSE);
				if (db)
					db->Close();
			}
		}
		XP_FileClose(XP_FileOpen(m_folderName, xpMailFolder,
							 XP_FILE_WRITE_BIN));

		if (m_pane && m_pane->GetMaster())
		{
			MailDB::SetFolderInfoValid(m_folderName, 0, 0);
			MSG_FolderInfoMail *mailFolder = m_pane->GetMaster()->FindMailFolder(m_folderName, FALSE);
			if (mailFolder)
				mailFolder->SummaryChanged();
		}
	}
	m_status = MK_CONNECTED;

	/* Warn about failed delivery.
	 */
	if (m_deliveryFailureCount > 0)
	{
		char *fmt, *buf;
		int L;
		if (m_deliveryFailureCount == 1)
			fmt = ("Delivery failed for 1 message.\n"
			   "\n"
			   "This message has been left in the Outbox folder.\n"
			   "Before it can be delivered, the error must be\n"
			   "corrected.");						/* #### i18n */
		else
			fmt = ("Delivery failed for %d messages.\n"
			   "\n"
			   "These messages have been left in the Outbox folder.\n"
			  "Before they can be delivered, the errors must be\n"
			  "corrected.");						/* #### i18n */

		L = XP_STRLEN(fmt) + 30;
		buf = (char *) XP_ALLOC (L);
		if (buf)
		{
			PR_snprintf(buf, L-1, fmt, m_deliveryFailureCount);
			FE_Alert(m_context, buf);
			XP_FREE(buf);
		}
	}

	/* Warn about bogus messages in the Outbox.
	 */
	else if (m_bogusMessageCount)
	{
		char *msg1, *msg2, *msg3;
		char *buf1, *buf2;

		if (m_bogusMessageCount == 1)
		{
			buf1 = NULL;
			msg1 = XP_GetString(MK_MSG_BOGUS_QUEUE_MSG_1);
			msg3 = XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL);
			const char *q = MSG_GetQueueFolderName();
			if (q)
			{
				if (!strcasecomp(q,QUEUE_FOLDER_NAME_OLD))
					msg1 = XP_GetString(MK_MSG_BOGUS_QUEUE_MSG_1_OLD);
					msg3 = XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL_OLD);
			}
		}
		else
		{
			const char *q = MSG_GetQueueFolderName();
			if (q)
			{
				if (!strcasecomp(q,QUEUE_FOLDER_NAME_OLD))
				{
					buf1 = PR_smprintf(XP_GetString(MK_MSG_BOGUS_QUEUE_MSG_N_OLD),
						m_bogusMessageCount);
						msg3 = XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL_OLD);
				}
				else
				{
					buf1 = PR_smprintf(XP_GetString(MK_MSG_BOGUS_QUEUE_MSG_N),
						 m_bogusMessageCount);
					msg3 = XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL);
				}

			}
			else
			{ 
				buf1 = PR_smprintf(XP_GetString(MK_MSG_BOGUS_QUEUE_MSG_N),
					 m_bogusMessageCount);
				msg3 = XP_GetString(MK_MSG_WHY_QUEUE_SPECIAL);
			}

			msg1 = buf1;
		}

		msg2 = XP_GetString(MK_MSG_BOGUS_QUEUE_REASON);

		if (msg1 && msg2 && msg3)
		{
			buf2 = PR_smprintf("%s%s%s", msg1, msg2, msg3);
			if (buf2)
			{
				FE_Alert(m_context, buf2);
				XP_FREE(buf2);
			}
		}

		if (buf1)
			XP_FREE(buf1);
	}

FAIL:
	return m_status;
}

int MSG_DeliverQueuedMailState::CloseDeliverQueuedSock(URL_Struct* url)
{
	if (m_infile) 
		XP_FileClose(m_infile);
	if (m_outfile) 
		XP_FileClose(m_outfile);
	FREEIF(m_to);
	FREEIF(m_bcc);
	FREEIF(m_newsgroups);
	FREEIF(m_newshost);
	FREEIF(m_ibuffer);
	FREEIF(m_obuffer);
	FREEIF(m_headers);
	FREEIF(m_folderName);
	if (m_tmpName)
	{
		XP_FileRemove(m_tmpName, xpFileToPost);
		XP_FREEIF(m_tmpName);
	}

	if (m_folderSize)
		FE_GraphProgressDestroy(m_context, url,
							m_folderSize, m_bytesRead);

	return 0;
}



int MSG_DeliverQueuedMailState::DeliverQueuedMessage ()
{
	XP_ASSERT (m_outfile && m_tmpName);
	if (!m_outfile || !m_tmpName) return -1;

	XP_FileClose (m_outfile);
	m_outfile = 0;

	if ((! (m_flags & MSG_FLAG_EXPUNGED)) &&
	  (m_flags & MSG_FLAG_QUEUED))
	{
		/* If this message is marked to be delivered, and does not have its
		 expunged bit set (meaning it has not yet been successfully delivered)
		 then deliver it now.
  		 */

		m_totalMessages++;
		if (! (m_flags & MSG_FLAG_READ))
			m_unreadMessages++;

#ifdef XP_UNIX
		m_status = msg_DeliverMessageExternally(m_context,
												   m_tmpName);
		if (m_status != 0)
		{
			if (m_status >= 0)
				m_status = DoFcc();
			else
				DeliverDone ();
		  return m_status;
		}
#endif /* XP_UNIX */

		DeliverAsNews ();
	}
  else
	{
	  /* Just to delete the temp file. */
		DeliverDone ();
	}

  return m_status;
}


int
MSG_DeliverQueuedMailState::DeliverDone ()
{
  int local_status = 0;
  if (m_status >= 0 &&
	  !m_deliveryFailed &&
	  m_flagsPosition > 0 &&
	  (m_flags & MSG_FLAG_QUEUED) &&
	  (! (m_flags & MSG_FLAG_EXPUNGED)))
	{
	  /* If delivery was successful, mark the message as expunged in the file
		 right now - we do this by opening the file a second time, updating it,
		 and closing it, to make sure that this informationn gets flushed to
		 disk as quickly as possible.  If we make it through the entire folder
		 and deliver them all, we will delete the whole file - but if we crash
		 in the middle, it's important that we not re-transmit all the already-
		 sent messages next time.

		 The message should have an X-Mozilla-Status header that is suitable
		 for us overwriting, but if it doesn't, then we will simply not mark
		 it as expunged - and should delivery fail, it will get sent again.
	   */
	  XP_File out;
	  char buf[50];
#if 0
	  XP_Bool summary_valid = FALSE;
	  XP_StatStruct st;
#endif

	  XP_Bool was_read = (m_flags & MSG_FLAG_READ) != 0;
	  m_flags |= MSG_FLAG_EXPUNGED;
	  m_flags &= (~MSG_FLAG_QUEUED);
	  /* it's pretty important that this take up the same space...
		 We can't write out a line terminator because at this point,
		 we don't know what the file had in it before.  (Safe to
		 assume LINEBREAK?) */
	  PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS_FORMAT, m_flags);

#if 0
	  XP_Stat (m_folderName, &st, xpURL); /* must succeed */
	  summary_valid = msg_IsSummaryValid (m_folderName, &st);
#endif

	  out = XP_FileOpen (m_folderName, xpMailFolder,
						 XP_FILE_UPDATE_BIN);
	  if (!out)
		{
		  local_status = m_status = MK_MSG_ERROR_WRITING_MAIL_FOLDER;
		  goto FAIL;
		}
	  XP_FileSeek (out, m_flagsPosition, SEEK_SET);
	  m_flagsPosition = 0; /* Just in case we get called again... */
	  m_status = XP_FileWrite (buf, XP_STRLEN(buf), out);
	  XP_FileClose (out);
	  if (m_status < (int) XP_STRLEN(buf)) {
		local_status = m_status = MK_MIME_ERROR_WRITING_FILE;
		goto FAIL;
	  }

	  XP_ASSERT(m_totalMessages > 0);
	  if (m_totalMessages > 0)
		m_totalMessages--;

	  if (!was_read)
		{
		  XP_ASSERT(m_unreadMessages > 0);
		  if (m_unreadMessages > 0)
			m_unreadMessages--;
		}
	}

 FAIL:
  /* Now we can delete the temp file too. */
  if (m_tmpName)
	{
	  int status2 = XP_FileRemove (m_tmpName, xpFileToPost);
	  if (local_status >= 0)
		local_status = m_status = status2;
	  XP_FREE (m_tmpName);
	  m_tmpName = 0;
	}

  m_awaitingDelivery = FALSE;

  if (m_deliveryFailed || m_status < 0)
	m_deliveryFailureCount++;
  m_deliveryFailed = FALSE;

  if (local_status < 0)
	/* If local_status is set, then it's an error that occurred in this
	   routine, not earlier -- so it wasn't a problem with delivery, but
	   rather, with the post-delivery stuff like updating the summary info.
	   (This would actually be a rather large problem, as now the message
	   has been sent, but we were unable to mark it as sent...)
	 */
	{
	  char *alert = NET_ExplainErrorDetails(local_status,0,0,0,0);
	  if (!alert) alert = XP_STRDUP("Unknown error!"); /* ####i18n */
	  if (alert)
		{
		  FE_Alert(m_context, alert);
		  XP_FREE(alert);
		}
	}

  return m_status;
}


int
MSG_DeliverQueuedMailState::DeliverAsNews ()
{
	URL_Struct *url;
	char *ng = m_newsgroups;
	char *host_and_port = m_newshost;
	XP_Bool xxx_p = FALSE;
	char *buf;

	if (ng) while (XP_IS_SPACE(*ng)) ng++;

	if (!ng || !*ng)
		return DeliverAsMail ();

	m_awaitingDelivery = TRUE;

	if (host_and_port && *host_and_port)
	{
		HG87355
	}
	if (host_and_port && !*host_and_port)
		host_and_port = 0;

	buf = (char *) XP_ALLOC (30 + (host_and_port ? XP_STRLEN(host_and_port) :0));
	if (!buf) return MK_OUT_OF_MEMORY;
	XP_STRCPY(buf, HG25525 "news:");
	if (host_and_port)
	{
		XP_STRCAT (buf, "//");
		XP_STRCAT (buf, host_and_port);
	}

	url = NET_CreateURLStruct (buf, NET_NORMAL_RELOAD);
	XP_FREE (buf);
	if (!url) return MK_OUT_OF_MEMORY;

	url->post_data = XP_STRDUP(m_tmpName);
	url->post_data_size = XP_STRLEN(url->post_data);
	url->post_data_is_file = TRUE;
	url->method = URL_POST_METHOD;
	url->fe_data = this;
	url->internal_url = TRUE;
	// We don't need to set msg_pane here. It's a good idea but little
	// risky for now.
	// url->msg_pane = m_pane;

	{
		char buf[100];
        const char *fmt = XP_GetString(MK_MSG_DELIVERING_MESSAGE_TO);
        
		PR_snprintf(buf, sizeof(buf)-1, fmt, (long) m_msgNumber, ng);
		FE_Progress(m_context, buf);
	}

  /* We can ignore the return value of NET_GetURL() because we have
	 handled the error in msg_deliver_queued_message_as_news_exit(). */
#if 0
	NET_GetURL (url, FO_CACHE_AND_PRESENT, m_context,
			  msg_deliver_queued_message_as_news_exit);
#else
	MSG_UrlQueue::AddUrlToPane (url, msg_deliver_queued_message_as_news_exit,
								m_pane, TRUE);
#endif
	return 0;
}


int
MSG_DeliverQueuedMailState::DeliverAsMail ()
{
	URL_Struct *url;
	char *to = m_to;
	char *bcc = m_bcc;
	char *buffer;
	if (to) while (XP_IS_SPACE(*to)) to++;
	if (bcc) while (XP_IS_SPACE(*bcc)) bcc++;

	if (!to || !*to)
		return DoFcc ();

	m_awaitingDelivery = TRUE;

#define EXTRA_BYTES 9
	/* 9 counts for "mailto:" + NULL terminated string + comma if we */
	/* have bcc */
	buffer = (char *) XP_ALLOC(EXTRA_BYTES + XP_STRLEN(to) +
							   (bcc ? XP_STRLEN(bcc)+1 : 0));
	if (!buffer) return MK_OUT_OF_MEMORY;
	XP_STRCPY(buffer, "mailto:");
	XP_STRCAT (buffer, to);
	if (bcc)
	{
		XP_STRCAT(buffer, ",");
		XP_STRCAT(buffer, bcc);
	}

	url = NET_CreateURLStruct (buffer, NET_NORMAL_RELOAD);
	XP_FREEIF (buffer);
	if (!url) return MK_OUT_OF_MEMORY;

	url->post_data = XP_STRDUP(m_tmpName);
	url->post_data_size = XP_STRLEN(url->post_data);
	url->post_data_is_file = TRUE;
	url->method = URL_POST_METHOD;
	url->fe_data = this;
	url->internal_url = TRUE;
	url->msg_pane = m_pane;

	{
		char buf[100];
        const char *fmt = XP_GetString(MK_MSG_DELIVERING_MESSAGE_TO);

		PR_snprintf(buf, sizeof(buf)-1, fmt, (long)m_msgNumber, to);
		FE_Progress(m_context, buf);
	}

  /* We can ignore the return value of NET_GetURL() because we have
	 handled the error in msg_deliver_queued_message_as_mail_exit(). */
#if 0
	NET_GetURL (url, FO_CACHE_AND_PRESENT, m_context,
			  msg_deliver_queued_message_as_mail_exit);
#else
	MSG_UrlQueue::AddUrlToPane (url, msg_deliver_queued_message_as_mail_exit,
								m_pane, TRUE);
#endif
	return 0;
}


void MSG_DeliverQueuedMailState::msg_deliver_queued_message_as_news_exit (URL_Struct *url, int status,
										 MWContext * /*context*/)
{
	MSG_DeliverQueuedMailState *state = (MSG_DeliverQueuedMailState *) url->fe_data;
	state->DeliverAsNewsExit(url, status);
}

void MSG_DeliverQueuedMailState::DeliverAsNewsExit(URL_Struct *url, int status)
{
	url->fe_data = 0;
	if (m_status >= 0)
		m_status = status;

	if (status < 0)
	{
		char *error_msg = 0;
		error_msg = url->error_msg;
		url->error_msg = 0;
		if (!error_msg)
			error_msg = NET_ExplainErrorDetails (status, 0, 0, 0, 0);
		Error (error_msg);
		FREEIF(error_msg);
	}

	NET_FreeURLStruct (url);

	if (m_status >= 0 && !m_deliveryFailed)
		m_status = DeliverAsMail ();
	else
		DeliverDone ();
}


void
MSG_DeliverQueuedMailState::msg_deliver_queued_message_as_mail_exit (URL_Struct *url, int status,
										 MWContext * /*context*/)
{
	MSG_DeliverQueuedMailState *state = (MSG_DeliverQueuedMailState *) url->fe_data;
	state->DeliverAsMailExit(url, status);
}

void MSG_DeliverQueuedMailState::DeliverAsMailExit(URL_Struct *url, int status)
{
	url->fe_data = 0;
	if (m_status >= 0)
		m_status = status;

	if (status < 0)
	{
		char *error_msg = 0;
		error_msg = url->error_msg;
		url->error_msg = 0;
		if (!error_msg)
			error_msg = NET_ExplainErrorDetails (status, 0, 0, 0, 0);
		Error (error_msg);
		FREEIF(error_msg);
	}

	NET_FreeURLStruct (url);

	if (m_status >= 0 && !m_deliveryFailed)
		m_status = DoFcc ();
	else
		DeliverDone ();
}

/* statuc */ void
MSG_DeliverQueuedMailState::PostAppendToIMAPSentFolder(
	URL_Struct *url,
	int status,
	MWContext * /* context */)
{
	MSG_DeliverQueuedMailState *state=
		(MSG_DeliverQueuedMailState*) url->fe_data;
	XP_ASSERT(state);

	if (state)
	{
		if (status < 0)
		{
			char *error_msg = 0;
			error_msg = url->error_msg;
			url->error_msg = 0;
			if (!error_msg)
				error_msg = NET_ExplainErrorDetails (status, 0, 0, 0,
													 0);
			state->Error (error_msg);
			FREEIF(error_msg);
		}
		else
		{
			if (state->m_imapFolderInfo)
			{
				if (!state->m_localFolderInfo)
					state->m_localFolderInfo =
						state->m_pane->GetMaster()->FindMailFolder(state->m_folderName, FALSE);

				if (state->m_localFolderInfo)
					state->m_localFolderInfo->UpdatePendingCounts(state->m_imapFolderInfo,
														   &state->m_msgKeys,
														   FALSE); 
				state->m_imapFolderInfo->SummaryChanged();
			}
		}
		state->m_status = state->DeliverDone();
	}
	NET_FreeURLStruct(url);
}

/* static */ void
MSG_DeliverQueuedMailState::PostCreateIMAPSentFolder(URL_Struct *url,
													 int status,
													 MWContext *context)
{
	MSG_DeliverQueuedMailState *state = 
		(MSG_DeliverQueuedMailState*) url->fe_data;
	XP_ASSERT(state);

	if (state)
	{
		if (status < 0)
		{
			char *error_msg = 0;
			error_msg = url->error_msg;
			url->error_msg = 0;
			if (!error_msg)
				error_msg = NET_ExplainErrorDetails (status, 0, 0, 0,
													 0);
			state->Error (error_msg);
			state->m_status = state->DeliverDone();
			FREEIF(error_msg);
		}
		else
		{
			MSG_Master::PostCreateImapFolderUrlExitFunc (url,
														 status,
														 context);
				
			char *folderName = IMAP_CreateOnlineSourceFolderNameFromUrl(
				url->address);
			char *hostName = NET_ParseURL(url->address, GET_HOST_PART);
			state->m_imapFolderInfo = 
				state->m_pane->GetMaster()->FindImapMailFolder(hostName, folderName, NULL, FALSE);
			FREEIF(hostName);
			XP_ASSERT(state->m_imapFolderInfo);

			if (state->m_imapFolderInfo)
			{
				state->m_imapFolderInfo->SetFlag(MSG_FOLDER_FLAG_SENTMAIL);
				state->AppendToIMAPSentFolder();
			}
			else
				state->m_status = state->DeliverDone();
		}
	}
	NET_FreeURLStruct(url);
}


void
MSG_DeliverQueuedMailState::PostListIMAPSentFolder (	URL_Struct *url,
														int status,
														MWContext *)
{
	MSG_DeliverQueuedMailState *state =
		(MSG_DeliverQueuedMailState*) url->fe_data;
	XP_ASSERT(state);

	if (state)
	{
		if (status < 0)
		{
			char *error_msg = 0;
			error_msg = url->error_msg;
			url->error_msg = 0;
			if (!error_msg)
				error_msg = NET_ExplainErrorDetails (status, 0, 0, 0,
													 0);
			state->Error (error_msg);
			state->m_status = state->DeliverDone();
			FREEIF(error_msg);
		}
		else
		{
			state->m_pane->SetIMAPListInProgress(FALSE);
			if (state->m_pane->IMAPListMailboxExist())
			{
				state->AppendToIMAPSentFolder();
			}
			else
			{
				char *buf = NULL;
				URL_Struct *url_struct = NULL;
				char *host = NET_ParseURL(state->m_fcc, GET_HOST_PART);
				char *name = NET_ParseURL(state->m_fcc, GET_PATH_PART);

				// *** Create Imap magic folder
				// *** then append message to the folder
				if (!name || !*name)
				{
					XP_FREEIF(name);
					name = PR_smprintf("/%s", SENT_FOLDER_NAME);
				}

				if (host && *host && name && *name)
				{
					buf = CreateImapMailboxCreateUrl
						(host, name+1, kOnlineHierarchySeparatorUnknown);
					if (buf)
					{
						url_struct = NET_CreateURLStruct(buf, NET_NORMAL_RELOAD);
						if (url_struct)
						{
							url_struct->fe_data = state;
							url_struct->internal_url = TRUE;
							url_struct->msg_pane = state->m_pane;
							state->m_pane->GetContext()->imapURLPane =
								state->m_pane; 
							MSG_UrlQueue::AddUrlToPane 
								(url_struct, PostCreateIMAPSentFolder,
								 state->m_pane, TRUE);
						}
						XP_FREEIF(buf);
					}
				}
				XP_FREEIF(host);
				XP_FREEIF(name);
			}
		}
	}
	NET_FreeURLStruct(url); 
}


void
MSG_DeliverQueuedMailState::ImapAppendAddBccHeadersIfNeeded( URL_Struct *url)
{
	XP_ASSERT(url);
	const char *bcc_headers = m_bcc;
	char *post_data = NULL;
	if (bcc_headers && *bcc_headers)
	{
		post_data = WH_TempName(xpFileToPost, "nsmail");
		if (post_data)
		{
			XP_File dstFile = XP_FileOpen(post_data, xpFileToPost, XP_FILE_WRITE_BIN);
			if (dstFile)
			{
				XP_File srcFile = XP_FileOpen(m_tmpName, xpFileToPost,
											  XP_FILE_READ_BIN);
				if (srcFile)
				{
					char *tmpBuffer = (char *)XP_ALLOC(4096);
					int bytesRead = 0;
					if (tmpBuffer)
					{
						XP_FileWrite("Bcc: ", 5, dstFile);
						XP_FileWrite(bcc_headers, XP_STRLEN(bcc_headers),
									 dstFile);
						XP_FileWrite(CRLF, XP_STRLEN(CRLF), dstFile);
						bytesRead = XP_FileRead(tmpBuffer, 4096, srcFile);
						while (bytesRead > 0)
						{
							XP_FileWrite(tmpBuffer, bytesRead, dstFile);
							bytesRead = XP_FileRead(tmpBuffer, 4096,
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
		post_data = XP_STRDUP(m_tmpName);
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
			(url, PostAppendToIMAPSentFolder, m_pane, TRUE);
	}
	else
	{
		NET_FreeURLStruct(url);
	}
}


void
MSG_DeliverQueuedMailState::AppendToIMAPSentFolder()
{
	char *host = NET_ParseURL(m_fcc, GET_HOST_PART);
	char *name = NET_ParseURL(m_fcc, GET_PATH_PART);
	char *owner = NET_ParseURL(m_fcc, GET_USERNAME_PART);

	if (!name || !*name)
	{
		XP_FREEIF(name);
		name = PR_smprintf("/%s", SENT_FOLDER_NAME);
	}
	if (!owner || !*owner)
	{
		MSG_IMAPHost *imapHost = m_pane->GetMaster()->GetIMAPHost(host);
		if (imapHost && imapHost->GetDefaultNamespacePrefixOfType(kPersonalNamespace))
			StrAllocCopy(owner, imapHost->GetUserName());
	}

	m_imapFolderInfo =
		m_pane->GetMaster()->FindImapMailFolder(host, name+1, owner, FALSE);
	
	char *urlBuf = NULL;
	URL_Struct *url = NULL;

	if (m_imapFolderInfo) 
	{
		MSG_Pane *urlPane = NULL;

		if (!urlPane)
			urlPane = m_pane;
		// *** append message to IMAP Send folder
		urlBuf =
			CreateImapAppendMessageFromFileUrl(
				m_imapFolderInfo->GetHostName(),
				m_imapFolderInfo->GetOnlineName(),
				m_imapFolderInfo->GetOnlineHierarchySeparator(),
				FALSE);
		if (urlBuf)
		{
			url = NET_CreateURLStruct (urlBuf, NET_NORMAL_RELOAD);
			if (url)
			{
				ImapAppendAddBccHeadersIfNeeded(url);
			}
			XP_FREEIF(urlBuf);
		}
	}
	else if (m_pane->IMAPListMailboxExist())
	{
		// m_pane->SetIMAPListMailboxExist(FALSE);
		// *** append message to not subscribed IMAP Send folder
		urlBuf =
			CreateImapAppendMessageFromFileUrl(host, name,
											   kOnlineHierarchySeparatorUnknown,
											   FALSE);
		if (urlBuf)
		{
			url = NET_CreateURLStruct (urlBuf, NET_NORMAL_RELOAD);
			if (url)
			{
				ImapAppendAddBccHeadersIfNeeded(url);
			}
			XP_FREEIF(urlBuf);
		}
	}
	else
	{
		// *** List not subscribed IMAP Sent Folder
		urlBuf =
			CreateImapListUrl(host, name, kOnlineHierarchySeparatorUnknown);
		if (urlBuf)
		{
			url = NET_CreateURLStruct(urlBuf, NET_NORMAL_RELOAD);
			if (url)
			{
				url->fe_data = this;
				url->internal_url = TRUE;
				url->msg_pane = m_pane;
				if (!m_context)
					SetContext(m_pane->GetContext());
				m_pane->GetContext()->imapURLPane = m_pane;
				m_pane->SetIMAPListInProgress(TRUE);
				MSG_UrlQueue::AddUrlToPane (url, PostListIMAPSentFolder,
											m_pane, TRUE);
			}
			XP_FREEIF(urlBuf);
		}
	}
	XP_FREEIF(host);
	XP_FREEIF(name);
	XP_FREEIF(owner);
}

int
MSG_DeliverQueuedMailState::DoFcc ()
{
  const char *bcc = m_bcc;
  const char *fcc = m_fcc;
  if (bcc) while (XP_IS_SPACE(*bcc)) bcc++;
  if (fcc) while (XP_IS_SPACE(*fcc)) fcc++;

  if (fcc && *fcc)
  {
	  // check if we are fccing to an an imap server
	  if (NET_URL_Type(fcc) == IMAP_TYPE_URL) 
	  {
		  if (m_pane->GetMaster()->GetPrefs()->GetMailServerIsIMAP4())
		  {
			  AppendToIMAPSentFolder();
			  return m_status;
		  }
		  else
			  return -1;
	  }
	  else
	  {
		  int status = msg_DoFCC (m_pane,
								  m_tmpName, xpFileToPost,
								  fcc, xpMailFolder,
								  bcc, fcc);
		  if (status < 0)
			  Error (/* #### slightly wrong error */
				  XP_GetString (MK_MSG_COULDNT_OPEN_FCC_FILE));
		  else
		  {
			  // this is normally done in the DeliverDoneCB, but that doesn't seem 
			  // to be used when delivering queued mail
			  MSG_FolderInfo *folder = m_pane->GetMaster()->FindMailFolder (fcc, FALSE /*create?*/);
			  if (folder)
				  folder->SummaryChanged();
		  }
	  }
  }

  m_status = DeliverDone ();
  return m_status;
}

void MSG_DeliverQueuedMailState::Error(const char *error_msg)
{
  XP_ASSERT( error_msg);

  m_deliveryFailed = TRUE;

  if (!error_msg)
	{
	  m_done = TRUE;
	}
  else
	{
	  char *s, *s2;
	  int L;
	  XP_Bool cont = FALSE;

	  s2 = XP_GetString(MK_MSG_QUEUED_DELIVERY_FAILED);			
			
	  L = XP_STRLEN(error_msg) + XP_STRLEN(s2) + 1;
	  s = (char *) XP_ALLOC(L);
	  if (!s)
		{
		  m_done = TRUE;
		  return;
		}
	  PR_snprintf (s, L, s2, error_msg);
	  cont = FE_Confirm (m_context, s);
	  XP_FREE(s);

	  if (cont)
		m_status = 0;
	  else
		m_done = TRUE;
	}
}

/* This function parses the headers, and also deletes from the header block
   any headers which should not be delivered in mail, regardless of whether
   they were present in the queue file.  Such headers include: BCC, FCC,
   Sender, X-Mozilla-Status, X-Mozilla-News-Host, and Content-Length.
   (Content-Length is for the disk file only, and must not be allowed to
   escape onto the network, since it depends on the local linebreak
   representation.  Arguably, we could allow Lines to escape, but it's not
   required by NNTP.)
 */
int32 MSG_DeliverQueuedMailState::HackHeaders()
{
	char *buf = m_headers;
	char *buf_end = buf + m_headersFP;

	FREEIF(m_to);
	FREEIF(m_bcc);
	FREEIF(m_newsgroups);
	FREEIF(m_newshost);
	m_flags = 0;

	if (m_pane)
		m_pane->SetRequestForReturnReceipt(FALSE);

	while (buf < buf_end)
	{
		XP_Bool prune_p = FALSE;
		XP_Bool do_flags_p = FALSE;
		XP_Bool do_return_receipt_p = FALSE;
		char *colon = XP_STRCHR (buf, ':');
		char *end;
		char *value = 0;
		char **header = 0;
		char *header_start = buf;

		if (! colon)
			break;

		end = colon;
		while (end > buf && (*end == ' ' || *end == '\t'))
			end--;

		switch (buf [0])
		{
		case 'B': case 'b':
		  if (!strncasecomp ("BCC", buf, end - buf))
			{
			  header = &m_bcc;
			  prune_p = TRUE;
			}
		  break;
		case 'C': case 'c':
		  if (!strncasecomp ("CC", buf, end - buf))
			header = &m_to;
		  else if (!strncasecomp (CONTENT_LENGTH, buf, end - buf))
			prune_p = TRUE;
		  break;
		case 'F': case 'f':
		  if (!strncasecomp ("FCC", buf, end - buf))
			{
			  header = &m_fcc;
			  prune_p = TRUE;
			}
		  break;
		case 'L': case 'l':
		  if (!strncasecomp ("Lines", buf, end - buf))
			prune_p = TRUE;
		  break;
		case 'N': case 'n':
		  if (!strncasecomp ("Newsgroups", buf, end - buf))
			header = &m_newsgroups;
		  break;
		case 'S': case 's':
		  if (!strncasecomp ("Sender", buf, end - buf))
			prune_p = TRUE;
		  break;
		case 'T': case 't':
		  if (!strncasecomp ("To", buf, end - buf))
			header = &m_to;
		  break;
		case 'X': case 'x':
		  if (X_MOZILLA_STATUS2_LEN == end - buf &&
			  !strncasecomp(X_MOZILLA_STATUS2, buf, end - buf))
			prune_p = TRUE;
		  else if (X_MOZILLA_STATUS_LEN == end - buf &&
			  !strncasecomp(X_MOZILLA_STATUS, buf, end - buf))
			prune_p = do_flags_p = TRUE;
		  else if (!strncasecomp(X_MOZILLA_DRAFT_INFO, buf, end - buf))
			prune_p = do_return_receipt_p = TRUE;
		  else if (!strncasecomp(X_MOZILLA_NEWSHOST, buf, end - buf))
			{
			  prune_p = TRUE;
			  header = &m_newshost;
			}
		  break;
		}

	  buf = colon + 1;
	  while (*buf == ' ' || *buf == '\t')
		buf++;

	  value = buf;

	SEARCH_NEWLINE:
	  while (*buf != 0 && *buf != CR && *buf != LF)
		buf++;

	  if (buf+1 >= buf_end)
		;
	  /* If "\r\n " or "\r\n\t" is next, that doesn't terminate the header. */
	  else if (buf+2 < buf_end &&
			   (buf[0] == CR  && buf[1] == LF) &&
			   (buf[2] == ' ' || buf[2] == '\t'))
		{
		  buf += 3;
		  goto SEARCH_NEWLINE;
		}
	  /* If "\r " or "\r\t" or "\n " or "\n\t" is next, that doesn't terminate
		 the header either. */
	  else if ((buf[0] == CR  || buf[0] == LF) &&
			   (buf[1] == ' ' || buf[1] == '\t'))
		{
		  buf += 2;
		  goto SEARCH_NEWLINE;
		}

	  if (header)
		{
		  int L = buf - value;
		  if (*header)
			{
			  char *newh = (char*) XP_REALLOC ((*header),
											   XP_STRLEN(*header) + L + 10);
			  if (!newh) return MK_OUT_OF_MEMORY;
			  *header = newh;
			  newh = (*header) + XP_STRLEN (*header);
			  *newh++ = ',';
			  *newh++ = ' ';
			  XP_MEMCPY (newh, value, L);
			  newh [L] = 0;
			}
		  else
			{
			  *header = (char *) XP_ALLOC(L+1);
			  if (!*header) return MK_OUT_OF_MEMORY;
			  XP_MEMCPY ((*header), value, L);
			  (*header)[L] = 0;
			}
		}
	  else if (do_flags_p)
		{
		  int i;
		  char *s = value;
		  XP_ASSERT(*s != ' ' && *s != '\t');
		  m_flags = 0;
		  for (i=0 ; i<4 ; i++) {
			m_flags = (m_flags << 4) | UNHEX(*s);
			s++;
		  }
		}
	  else if (do_return_receipt_p)
		{
		  int L = buf - value;
		  char *draftInfo = (char*) XP_ALLOC(L+1);
		  char *receipt = NULL;
		  if (!draftInfo) return MK_OUT_OF_MEMORY;
		  XP_MEMCPY(draftInfo, value, L);
		  *(draftInfo+L)=0;
		  receipt = XP_STRSTR(draftInfo, "receipt=");
		  if (receipt) 
			{
			  char *s = receipt+8;
			  int requestForReturnReceipt = 0;
			  sscanf(s, "%d", &requestForReturnReceipt);
			  if ((requestForReturnReceipt == 2 ||
				   requestForReturnReceipt == 3) && m_pane)
				m_pane->SetRequestForReturnReceipt(TRUE);
			}
		  XP_FREEIF(draftInfo);
		}

	  if (*buf == CR || *buf == LF)
		{
		  if (*buf == CR && buf[1] == LF)
			buf++;
		  buf++;
		}

	  if (prune_p)
		{
		  char *to = header_start;
		  char *from = buf;
		  while (from < buf_end)
			*to++ = *from++;
		  buf = header_start;
		  buf_end = to;
		  m_headersFP = buf_end - m_headers;
		}
	}

  /* Print a message if this message isn't already expunged. */
  if ((m_flags & MSG_FLAG_QUEUED) &&
	  (! (m_flags & MSG_FLAG_EXPUNGED)))
	{
	  char buf[100];
	  const char *fmt = XP_GetString(MK_MSG_DELIVERING_MESSAGE);

	  m_msgNumber++;
	  PR_snprintf(buf, sizeof(buf)-1, fmt, (long) m_msgNumber);
	  FE_Progress(m_context, buf);
	}
  else if ((! (m_flags & MSG_FLAG_QUEUED)) &&
		   (! (m_flags & MSG_FLAG_EXPUNGED)))
	{
	  /* Ugh!  a non-expunged message without its queued bit set!
		 How did that get in here?  Danger, danger. */
	  m_bogusMessageCount++;
	}

  /* Now m_headers contains only the headers we should ship.
	 Write them to the file;
   */
  m_headers [m_headersFP++] = CR;
  m_headers [m_headersFP++] = LF;
  if (XP_FileWrite (m_headers, m_headersFP,
					m_outfile) < m_headersFP) {
	return MK_MIME_ERROR_WRITING_FILE;
  }
  return 0;
}


#define msg_grow_headers(desired_size) \
  (((desired_size) >= m_headersSize) ? \
   msg_GrowBuffer ((desired_size), sizeof(char), 1024, \
				   &m_headers, &m_headersSize) \
   : 0)

int32
MSG_DeliverQueuedMailState::DeliverQueuedLine(char *line, uint32 length)
{
  uint32 flength = length;

  m_bytesRead += length;
  FE_SetProgressBarPercent(m_context,
						   ((m_bytesRead * 100)
							/ m_folderSize));

  /* convert existing newline to CRLF */
  if (length > 0 &&
	  (line[length-1] == CR ||
	   (line[length-1] == LF &&
		(length < 2 || line[length-2] != CR))))
	{
	  line[length-1] = CR;
	  line[length++] = LF;
	}

  if (line[0] == 'F' && msg_IsEnvelopeLine(line, length))
	{
	  XP_ASSERT (!m_inhead); /* else folder corrupted */

	  if (!m_inhead &&
		  m_headersFP > 0)
		{
		  m_status = DeliverQueuedMessage ();
		  if (m_status < 0) return m_status;
		}
	  m_headersFP = 0;
	  m_headersPosition = 0;
	  m_inhead = TRUE;
	}
  else if (m_inhead)
	{
	  if (m_headersPosition == 0)
		{
		  /* This line is the first line in a header block.
			 Remember its position. */
		  m_headersPosition = m_position;

		  /* Also, since we're now processing the headers, clear out the
			 slots which we will parse data into, so that the values that
			 were used the last time around do not persist.

			 We must do that here, and not in the previous clause of this
			 `else' (the "I've just seen a `From ' line clause") because
			 that clause happens before delivery of the previous message is
			 complete, whereas this clause happens after the previous msg
			 has been delivered.  If we did this up there, then only the
			 last message in the folder would ever be able to be both
			 mailed and posted (or fcc'ed.)
		   */
		  FREEIF(m_to);
		  FREEIF(m_bcc);
		  FREEIF(m_newsgroups);
		  FREEIF(m_newshost);
		  FREEIF(m_fcc);
		}

	  if (line[0] == CR || line[0] == LF || line[0] == 0)
		{
		  /* End of headers.  Now parse them; open the temp file;
			 and write the appropriate subset of the headers out. */
		  m_inhead = FALSE;

		  m_tmpName = WH_TempName (xpFileToPost, "nsqmail");
		  if (!m_tmpName)
			return MK_OUT_OF_MEMORY;
		  m_outfile = XP_FileOpen (m_tmpName, xpFileToPost,
										XP_FILE_WRITE_BIN);
		  if (!m_outfile)
			return MK_MIME_ERROR_WRITING_FILE;

		  m_status = HackHeaders ();
		  if (m_status < 0) return m_status;
		}
	  else
		{
		  /* Otherwise, this line belongs to a header.  So append it to the
			 header data. */

		  if (!strncasecomp (line, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN))
			/* Notice the position of the flags. */
			m_flagsPosition = m_position;
		  else if (m_headersFP == 0)
			m_flagsPosition = 0;

		  m_status = msg_grow_headers (length + m_headersFP + 10);
		  if (m_status < 0) return m_status;

		  XP_MEMCPY (m_headers + m_headersFP, line, length);
		  m_headersFP += length;
		}
	}
  else
	{
	  /* This is a body line.  Write it to the file. */
	  XP_ASSERT (m_outfile);
	  if (m_outfile)
		{
		  m_status = XP_FileWrite (line, length, m_outfile);
		  if (m_status < (int32) length) return MK_MIME_ERROR_WRITING_FILE;
		}
	}

  m_position += flength;
  return 0;
}

