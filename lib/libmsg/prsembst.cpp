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

#include "rosetta.h"
#include "msg.h"
#include "xp.h"
#include "prsembst.h"
#include "mailhdr.h"
#include "maildb.h"
#include "msgfpane.h"
#include "msgfinfo.h"
#include "xp_time.h"
#include HG03067
#include "msgdbvw.h"
#include "grpinfo.h"
#include "msg_filt.h"
#include "msg_srch.h"
#include "pmsgsrch.h"
#include "pmsgfilt.h"
#include "xplocale.h"
#include "msgprefs.h"
#include "msgurlq.h"
#include "jsmsg.h"
#include "libi18n.h"
#include "msgimap.h"
#include "imaphost.h"
#include "msgmdn.h"
#include "prefapi.h"

extern "C"
{
#include "xpgetstr.h"
	extern int MK_MSG_FOLDER_UNREADABLE;
	extern int MK_OUT_OF_MEMORY;
	extern int MK_MSG_NON_MAIL_FILE_READ_QUESTION;
	extern int MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	extern int MK_MSG_FOLDER_UNREADABLE;
	extern int MK_MSG_REPARSE_FOLDER;
}

ParseMailboxState::ParseMailboxState(const char *mailboxName)
{
	m_mailDB = NULL;
	m_msgDBView = NULL;
	m_mailboxName = XP_STRDUP(mailboxName);
	m_mailMaster = NULL;
	m_folder = NULL;
	m_pane = NULL;
	m_obuffer = NULL;
	m_obuffer_size = 0;
	m_ibuffer = NULL;
	m_ibuffer_size = 0;
	m_ibuffer_fp = 0;
	m_graph_progress_total = 0;
	m_graph_progress_received = 0;
	m_updateAsWeGo = FALSE;
	m_ignoreNonMailFolder = FALSE;
	m_isRealMailFolder = TRUE;
	m_file = 0;
	m_context = NULL;
	// OK, it's bad to allocate one of these in the constructor.
	// But this needs to be a pointer so that we can replace it
	// with a different parser as required.
	m_parseMsgState = new ParseMailMessageState;
}

ParseMailboxState::~ParseMailboxState()
{
	XP_FREE(m_mailboxName);
	// make sure we don't have the folder locked.
	if (m_folder && m_folder->TestSemaphore(this))
		m_folder->ReleaseSemaphore(this);
	if (m_parseMsgState)
		delete m_parseMsgState;
}

void ParseMailboxState::SetMailMessageParseState(ParseMailMessageState *mailMessageState)
{
	// delete old one.
	if (m_parseMsgState)
		delete m_parseMsgState;

	m_parseMsgState = mailMessageState;
}

void ParseMailboxState::UpdateStatusText ()
{
	char *leafName = XP_STRRCHR (m_mailboxName, '/');
	if (!leafName)
		leafName = m_mailboxName;
	else
		leafName++;
	NET_UnEscape(leafName);
	char *upgrading = XP_GetString (MK_MSG_REPARSE_FOLDER);
	int progressLength = XP_STRLEN(upgrading) + XP_STRLEN(leafName) + 1;
	char *progress = new char [progressLength];
	PR_snprintf (progress, progressLength, upgrading, leafName); 
	FE_Progress (m_context, progress);
	delete [] progress;
}

void ParseMailboxState::UpdateProgressPercent ()
{
	XP_ASSERT(m_context != NULL);
	XP_ASSERT(m_graph_progress_total != 0);
	if ((m_context) && (m_graph_progress_total != 0))
	{
		MSG_SetPercentProgress(m_context, m_graph_progress_received, m_graph_progress_total);
	}
}

int  ParseMailboxState::BeginOpenFolderSock(const char *folder_name,
							  const char * /*message_id*/ , int32 /* msgnum */,
							  void **folder_ptr)
{
	XP_StatStruct folderst;
	// get the semaphore for the folder.
	int	status = (m_folder) ? m_folder->AcquireSemaphore(this) : 0;
	if (status)	
	{
#ifdef DEBUG_bienvenu 
		XP_Trace ("ParseMailboxState::BeginOpenFolderSock: failed to acquire semaphore for %s", folder_name);
		XP_ASSERT(FALSE);
#endif
		return status;
	}

	if (XP_Stat (folder_name, &folderst, xpMailFolder)) 
	{
#ifdef DEBUG_bienvenu
		XP_Trace ("ParseMailboxState::BeginOpenFolderSock: couldn't stat %s", folder_name);
#endif
		return MK_MSG_FOLDER_UNREADABLE;
	}
	m_file = XP_FileOpen(folder_name, xpMailFolder, XP_FILE_READ_BIN);
	if (!m_file) {
#ifdef DEBUG_bienvenu
		XP_Trace("ParseMailboxState::BeginOpenFolderSock: couldn't open %s", folder_name);
#endif
		return MK_MSG_FOLDER_UNREADABLE;
	}

	HG82220
	// assign this so libnet will call CloseFolderSock.
	if (folder_ptr != NULL)
		*folder_ptr = this;
	/*	The folder file is now open, and netlib will call us as it reads
		chunks of it.   Set up the buffers, etc. */

	status = BeginParsingFolder(0);
	if (status < 0)
	{
#ifdef DEBUG_bienvenu
		XP_Trace ("ParseMailboxState::BeginOpenFolderSock: BeginParsing %s returned %d", folder_name, status);
#endif
		return status;
	}
	m_graph_progress_total = folderst.st_size;

	UpdateStatusText ();

#ifdef DEBUG_bienvenu
	XP_Trace ("ParseMailboxState::BeginOpenFolderSock: returned WAITING_FOR_CONNECTION");
#endif
	return(MK_WAITING_FOR_CONNECTION);
}

int ParseMailboxState::BeginParsingFolder(int32 startPos)
{
	m_obuffer_size = 10240;
	m_parsingDone = FALSE;
	m_obuffer = (char *) XP_ALLOC (m_obuffer_size);
	if (! m_obuffer)
	{
		return MK_OUT_OF_MEMORY;
	}

	m_parseMsgState->Init(startPos);
	return 0;
}

int ParseMailboxState::ParseBlock(const char *block, int32 length)
{
	return msg_LineBuffer (block, length, &m_ibuffer, &m_ibuffer_size,  &m_ibuffer_fp, FALSE,
#ifdef XP_OS2
					(int32 (_Optlink*) (char*,uint32,void*))
#endif
					   LineBufferCallback, this);
}

/* This function works on what MSG_BeginOpenFolder
 * starts.  This function can return MK_WAITING_FOR_CONNECTION
 * as many times as it needs and will be called again
 * after yeilding to user events until it returns
 * MK_CONNECTED or a negative error code.
 */
int ParseMailboxState::ParseMoreFolderSock(const char* folder_name,
							   const char* /* message_id */,
							   int32 /* msgnum */, void** /* folder_ptr */)
{
	int status;
	if (! m_file)
	{
#ifdef MBOX_DEBUG
		fprintf(real_stderr, "MSG_FinishOpenFolderSock: no file??\n");
#endif
		return MK_MSG_FOLDER_UNREADABLE;
	}

	/* Read the next chunk of data from the file. */
	status = XP_FileRead (m_obuffer, m_obuffer_size, m_file);
#ifdef MBOX_DEBUG
	fprintf(real_stderr, "ParseMoreFolderSock: parsed %d of %s\n",
			status, folder_name);
#endif

	if (status > 0 &&
		m_graph_progress_total > 0 &&
		m_graph_progress_received == 0)
	{
		/* This is the first block from the file.  Check to see if this
		   looks like a mail file. */
		const char *s = m_obuffer;
		const char *end = s + m_obuffer_size;
		while (s < end && XP_IS_SPACE(*s))
			s++;
		if ((end - s) < 20 || !msg_IsEnvelopeLine(s, end - s))
		{
			char buf[500];
			PR_snprintf (buf, sizeof(buf),
						 XP_GetString(MK_MSG_NON_MAIL_FILE_READ_QUESTION),
						 folder_name);
			m_isRealMailFolder = FALSE;
			if (m_ignoreNonMailFolder)
				return MK_CONNECTED;
			else if (!FE_Confirm (m_context, buf))
				return -1; /* #### NOT_A_MAIL_FILE */
		}
	}

	if (m_graph_progress_total > 0)
	{
		if (status > 0)
		  m_graph_progress_received += status;
		MSG_SetPercentProgress (m_context, m_graph_progress_received, m_graph_progress_total);
	}

	if (status < 0)
	{
		return status;
	}
	else if (status == 0)
	{
		HG22067
		DoneParsingFolder();
		m_parsingDone = TRUE;
		return MK_CONNECTED;
	}
	else
	{
		status = ParseBlock(m_obuffer, status);
		if (status < 0)
		{
			return status;
		}
	}
	return(MK_WAITING_FOR_CONNECTION);
}

void ParseMailboxState::CloseFolderSock(const char* /*folder_name*/, const char* /*message_id*/,
						   int32 /*msgnum*/, void* /*folder_ptr*/)

{
	if (m_file)
		XP_FileClose(m_file);

	FREEIF (m_ibuffer);
	FREEIF (m_obuffer);
	m_obuffer_size = 0;
	m_ibuffer_size = 0;
	if (m_msgDBView != NULL && m_parsingDone && !m_updateAsWeGo)
	{
		uint32 viewCount;

		m_msgDBView->NoteStartChange(0, 0, MSG_NotifyAll);
		m_msgDBView->Init(&viewCount);
		m_msgDBView->Sort(m_msgDBView->GetSortType(), m_msgDBView->GetSortOrder());
		m_msgDBView->NoteEndChange(0, 0, MSG_NotifyAll);
		if (m_pane)
			FE_PaneChanged(m_pane, FALSE, MSG_PaneNotifyFolderLoaded, (uint32) m_folder);
	}
	if (!m_parsingDone)
	{
		if (m_mailDB != NULL)
		{
			m_mailDB->Close();
			m_mailDB = NULL;
		}

		// If we've failed to create a summary file, don't leave the DB lying around
		if (m_parseMsgState)
			XP_FileRemove (m_mailboxName, xpMailFolderSummary);
	}
}


void ParseMailboxState::DoneParsingFolder()
{
	if (m_ibuffer_fp > 0) 
	{
		m_parseMsgState->ParseFolderLine(m_ibuffer, m_ibuffer_fp);
		m_ibuffer_fp = 0;
	}
	PublishMsgHeader();

	if (m_mailDB != NULL)	// finished parsing, so flush db folder info 
		UpdateDBFolderInfo();

	if (m_folder != NULL)
		m_folder->SummaryChanged();

	FreeBuffers();
}

void ParseMailboxState::FreeBuffers()
{
	/* We're done reading the folder - we don't need these things
	 any more. */
	FREEIF (m_ibuffer);
	m_ibuffer_size = 0;
	FREEIF (m_obuffer);
	m_obuffer_size = 0;
}

void ParseMailboxState::UpdateDBFolderInfo()
{
	UpdateDBFolderInfo(m_mailDB, m_mailboxName);
}

// update folder info in db so we know not to reparse.
void ParseMailboxState::UpdateDBFolderInfo(MailDB *mailDB, const char *mailboxName)
{
	XP_StatStruct folderst;
	DBFolderInfo *folderInfo = mailDB->m_dbFolderInfo;

	if (!XP_Stat (mailboxName, &folderst, xpMailFolder))
	{
		folderInfo->m_folderDate = folderst.st_mtime;
		folderInfo->m_folderSize = folderst.st_size;
		folderInfo->m_parsedThru = folderst.st_size;
//		folderInfo->setDirty();	DMB TODO
	}
	mailDB->Commit();
//	m_mailDB->Close();
}

// By default, do nothing
void ParseMailboxState::FolderTypeSpecificTweakMsgHeader(MailMessageHdr * /* tweakMe */)
{
}

// Tell the world about the message header (add to db, and view, if any)
int32 ParseMailboxState::PublishMsgHeader()
{
	m_parseMsgState->FinishHeader();
	if (m_parseMsgState->m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_parseMsgState->m_newMsgHdr);
		
		if (m_parseMsgState->m_newMsgHdr->GetFlags() & kExpunged)
		{
			DBFolderInfo *folderInfo = m_mailDB->m_dbFolderInfo;
			folderInfo->m_expunged_bytes += m_parseMsgState->m_newMsgHdr->GetByteLength();
			if (m_parseMsgState->m_newMsgHdr)
			{
				delete m_parseMsgState->m_newMsgHdr;
				m_parseMsgState->m_newMsgHdr = NULL;
			}
		}
		else if (m_mailDB != NULL)
		{
			m_mailDB->AddHdrToDB(m_parseMsgState->m_newMsgHdr, NULL, m_updateAsWeGo);
			delete m_parseMsgState->m_newMsgHdr;
			m_parseMsgState->m_newMsgHdr = NULL;
		}
		else
			XP_ASSERT(FALSE);	// should have a DB, no?
	}
	else if (m_mailDB)
	{
		DBFolderInfo *folderInfo = m_mailDB->m_dbFolderInfo;
		folderInfo->m_expunged_bytes += m_parseMsgState->m_position - m_parseMsgState->m_envelope_pos;
	}
	return 0;
}

void ParseMailboxState::AbortNewHeader()
{
	if (m_parseMsgState->m_newMsgHdr && m_mailDB)
	{
		delete m_parseMsgState->m_newMsgHdr;
		m_parseMsgState->m_newMsgHdr = NULL;
	}
}

ParseMailMessageState *ParseMailboxState::GetMsgState()
{
	return m_parseMsgState;
}

/* static */
int32 ParseMailboxState::LineBufferCallback(char *line, uint32 lineLength,
									  void *closure)
{
	ParseMailboxState *parseState = (ParseMailboxState *) closure;

	return parseState->ParseFolderLine(line, lineLength);
}

int32 ParseMailboxState::ParseFolderLine(const char *line, uint32 lineLength)
{
	int status = 0;

	if (m_mailDB && m_mailDB->GetDB())
	{
		m_parseMsgState->SetMailDB(m_mailDB);
	}
	// mailbox parser needs to do special stuff when it finds an envelope
	// after parsing a message body. So do that.
	if (line[0] == 'F' && msg_IsEnvelopeLine(line, lineLength))
	{
		// **** This used to be
		// XP_ASSERT (m_parseMsgState->m_state == MBOX_PARSE_BODY);
		// **** I am not sure this is a right thing to do. This happens when
		// going online, downloading a message while playing back append
		// draft/template offline operation. We are mixing MBOX_PARSE_BODY &&
		// MBOX_PARSE_HEADERS state.  **** jt

		XP_ASSERT (m_parseMsgState->m_state == MBOX_PARSE_BODY ||
				   m_parseMsgState->m_state == MBOX_PARSE_HEADERS); /* else folder corrupted */
		PublishMsgHeader();
		m_parseMsgState->Clear();
		status = m_parseMsgState->StartNewEnvelope(line, lineLength);
		if (status < 0)
			return status;
	}
	// otherwise, the message parser can handle it completely.
	else if (m_mailDB != NULL)	// if no DB, do we need to parse at all?
		return m_parseMsgState->ParseFolderLine(line, lineLength);

	return 0;

}

ParseMailMessageState::ParseMailMessageState()
{
	m_envelope = NULL;
	m_headers = NULL;
	m_headers_size = 0;
	m_envelope_size = 0;
	m_mailDB = NULL;
	m_position = 0;
	m_IgnoreXMozillaStatus = FALSE;
	m_state = MBOX_PARSE_BODY;
	Clear();
}

ParseMailMessageState::~ParseMailMessageState()
{
	FREEIF(m_envelope);
	FREEIF(m_headers);
	ClearAggregateHeader (m_toList);
	ClearAggregateHeader (m_ccList);
}

void ParseMailMessageState::Init(uint32 fileposition)
{
	m_state = MBOX_PARSE_BODY;
	m_position = fileposition;
	m_newMsgHdr = NULL;
	HG98330
}

void ParseMailMessageState::Clear()
{
	m_headers_fp = 0;
	m_envelope_fp = 0;
	m_headers_size = 0;
	m_message_id.length = 0;
	m_references.length = 0;
	m_date.length = 0;
	m_from.length = 0;
	m_sender.length = 0;
	m_newsgroups.length = 0;
	m_subject.length = 0;
	m_status.length = 0;
	m_mozstatus.length = 0;
	m_mozstatus2.length = 0;
	m_envelope_from.length = 0;
	m_envelope_date.length = 0;
	m_priority.length = 0;
	m_mdn_dnt.length = 0;
	m_return_path.length = 0;
	m_mdn_original_recipient.length = 0;
	m_body_lines = 0;
	m_newMsgHdr = NULL;
	m_envelope_pos = 0;
	ClearAggregateHeader (m_toList);
	ClearAggregateHeader (m_ccList);
}

int ParseMailMessageState::GrowHeaders(uint32 desired_size)
{
  return (((desired_size) >= m_headers_size) ? 
   msg_GrowBuffer ((desired_size), sizeof(char), 1024, 
				   &m_headers, &m_headers_size) 
   : 0);
}

int ParseMailMessageState::GrowEnvelope(uint32 desired_size) 
{
  return (((desired_size) >= m_envelope_size) ? 
   msg_GrowBuffer ((desired_size), sizeof(char), 255, 
				   &m_envelope, &m_envelope_size)
   : 0);
}

int32 ParseMailMessageState::ParseFolderLine(const char *line, uint32 lineLength)
{
	int status = 0;

	if (m_state == MBOX_PARSE_HEADERS)
	{
		if (EMPTY_MESSAGE_LINE(line))
		{
		  /* End of headers.  Now parse them. */
			status = ParseHeaders();
			if (status < 0)
				return status;

			 status = FinalizeHeaders();
			if (status < 0)
				return status;
			 m_state = MBOX_PARSE_BODY;
		}
		else
		{
		  /* Otherwise, this line belongs to a header.  So append it to the
			 header data, and stay in MBOX `MIME_PARSE_HEADERS' state.
		   */
			status = GrowHeaders (lineLength + m_headers_fp + 1);
			if (status < 0) return status;
			XP_MEMCPY (m_headers + m_headers_fp, line, lineLength);
			m_headers_fp += lineLength;
		}
	}
	else if ( m_state == MBOX_PARSE_BODY)
	{
		m_body_lines++;
	}

	m_position += lineLength;

	return 0;
}

void ParseMailMessageState::SetMailDB(MailDB *mailDB)
{
	m_mailDB = mailDB;
}

// We've found the start of the next message, so finish this one off.
void ParseMailMessageState::FinishHeader()
{
	if (m_newMsgHdr)
	{
		  m_newMsgHdr->SetMessageKey(m_envelope_pos);
		  m_newMsgHdr->SetByteLength(m_position - m_envelope_pos);
		  m_newMsgHdr->SetMessageSize(m_position - m_envelope_pos);	// dmb - no longer number of lines.
		  m_newMsgHdr->SetLineCount(m_body_lines);
	}
}

struct message_header *ParseMailMessageState::GetNextHeaderInAggregate (XPPtrArray &list)
{
	// When parsing a message with multiple To or CC header lines, we're storing each line in a 
	// list, where the list represents the "aggregate" total of all the header. Here we get a new
	// line for the list

	struct message_header *header = (struct message_header*) XP_CALLOC (1, sizeof(struct message_header));
	list.Add (header);
	return header;
}

void ParseMailMessageState::GetAggregateHeader (XPPtrArray &list, struct message_header *outHeader)
{
	// When parsing a message with multiple To or CC header lines, we're storing each line in a 
	// list, where the list represents the "aggregate" total of all the header. Here we combine
	// all the lines together, as though they were really all found on the same line

	struct message_header *header = NULL;
	int length = 0;
	int i;

	// Count up the bytes required to allocate the aggregated header
	for (i = 0; i < list.GetSize(); i++)
	{
		header = (struct message_header*) list.GetAt(i);
		length += (header->length + 1); //+ for ","
		XP_ASSERT(header->length == XP_STRLEN(header->value));
	}

	if (length > 0)
	{
		char *value = (char*) XP_ALLOC (length + 1); //+1 for null term
		if (value)
		{
			// Catenate all the To lines together, separated by commas
			value[0] = '\0';
			int size = list.GetSize();
			for (i = 0; i < size; i++)
			{
				header = (struct message_header*) list.GetAt(i);
				XP_STRCAT (value, header->value);
				if (i + 1 < size)
					XP_STRCAT (value, ",");
			}
			outHeader->length = length;
			outHeader->value = value;
		}
	}
	else
	{
		outHeader->length = 0;
		outHeader->value = NULL;
	}
}

void ParseMailMessageState::ClearAggregateHeader (XPPtrArray &list)
{
	// Reset the aggregate headers. Free only the message_header struct since 
	// we don't own the value pointer

	for (int i = 0; i < list.GetSize(); i++)
		XP_FREE ((struct message_header*) list.GetAt(i));
	list.RemoveAll();
}

// We've found a new envelope to parse.
int ParseMailMessageState::StartNewEnvelope(const char *line, uint32 lineLength)
{
	m_envelope_pos = m_position;
	m_state = MBOX_PARSE_HEADERS;
	m_position += lineLength;
	m_headerstartpos = m_position;
	return ParseEnvelope (line, lineLength);
}

/* largely taken from mimehtml.c, which does similar parsing, sigh...
 */
int ParseMailMessageState::ParseHeaders ()
{
  char *buf = m_headers;
  char *buf_end = buf + m_headers_fp;
  while (buf < buf_end)
	{
	  char *colon = XP_STRCHR (buf, ':');
	  char *end;
	  char *value = 0;
	  struct message_header *header = 0;

	  if (! colon)
		break;

	  end = colon;
	  while (end > buf && (*end == ' ' || *end == '\t'))
		end--;

	  switch (buf [0])
		{
		case 'C': case 'c':
		  if (!strncasecomp ("CC", buf, end - buf))
			header = GetNextHeaderInAggregate(m_ccList);
		  break;
		case 'D': case 'd':
		  if (!strncasecomp ("Date", buf, end - buf))
			header = &m_date;
		  else if (!strncasecomp("Disposition-Notification-To", buf, end - buf))
			header = &m_mdn_dnt;
		  break;
		case 'F': case 'f':
		  if (!strncasecomp ("From", buf, end - buf))
			header = &m_from;
		  break;
		case 'M': case 'm':
		  if (!strncasecomp ("Message-ID", buf, end - buf))
			header = &m_message_id;
		  break;
		case 'N': case 'n':
		  if (!strncasecomp ("Newsgroups", buf, end - buf))
			header = &m_newsgroups;
		  break;
		case 'O': case 'o':
			if (!strncasecomp ("Original-Recipient", buf, end - buf))
				header = &m_mdn_original_recipient;
			break;
		case 'R': case 'r':
		  if (!strncasecomp ("References", buf, end - buf))
			header = &m_references;
		  else if (!strncasecomp ("Return-Path", buf, end - buf))
			  header = &m_return_path;
		   // treat conventional Return-Receipt-To as MDN
		   // Disposition-Notification-To
		  else if (!strncasecomp ("Return-Receipt-To", buf, end - buf))
			  header = &m_mdn_dnt;
		  break;
		case 'S': case 's':
		  if (!strncasecomp ("Subject", buf, end - buf))
			header = &m_subject;
		  else if (!strncasecomp ("Sender", buf, end - buf))
			header = &m_sender;
		  else if (!strncasecomp ("Status", buf, end - buf))
			header = &m_status;
		  break;
		case 'T': case 't':
		  if (!strncasecomp ("To", buf, end - buf))
			header = GetNextHeaderInAggregate(m_toList);
		  break;
		case 'X':
		  if (X_MOZILLA_STATUS2_LEN == end - buf &&
			  !strncasecomp(X_MOZILLA_STATUS2, buf, end - buf) &&
			  !m_IgnoreXMozillaStatus)
			  header = &m_mozstatus2;
		  else if ( X_MOZILLA_STATUS_LEN == end - buf &&
			  !strncasecomp(X_MOZILLA_STATUS, buf, end - buf) && !m_IgnoreXMozillaStatus)
			header = &m_mozstatus;
		  // we could very well care what the priority header was when we 
		  // remember its value. If so, need to remember it here. Also, 
		  // different priority headers can appear in the same message, 
		  // but we only rememeber the last one that we see.
		  else if (!strncasecomp("X-Priority", buf, end - buf)
			  || !strncasecomp("Priority", buf, end - buf))
			  header = &m_priority;
		  break;
		}

	  buf = colon + 1;
	  while (*buf == ' ' || *buf == '\t')
		buf++;

	  value = buf;
	  if (header)
        header->value = value;

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
		header->length = buf - header->value;

	  if (*buf == CR || *buf == LF)
		{
		  char *last = buf;
		  if (*buf == CR && buf[1] == LF)
			buf++;
		  buf++;
		  *last = 0;	/* short-circuit const, and null-terminate header. */
		}

	  if (header)
		{
		  /* More const short-circuitry... */
		  /* strip leading whitespace */
		  while (XP_IS_SPACE (*header->value))
			header->value++, header->length--;
		  /* strip trailing whitespace */
		  while (header->length > 0 &&
				 XP_IS_SPACE (header->value [header->length - 1]))
			((char *) header->value) [--header->length] = 0;
		}
	}
  return 0;
}

int ParseMailMessageState::ParseEnvelope (const char *line, uint32 line_size)
{
	const char *end;
	char *s;
	int status = 0;

	status = GrowEnvelope (line_size + 1);
	if (status < 0) return status;
	XP_MEMCPY (m_envelope, line, line_size);
	m_envelope_fp = line_size;
	m_envelope [line_size] = 0;
	end = m_envelope + line_size;
	s = m_envelope + 5;

	while (s < end && XP_IS_SPACE (*s))
		s++;
	m_envelope_from.value = s;
	while (s < end && !XP_IS_SPACE (*s))
		s++;
	m_envelope_from.length = s - m_envelope_from.value;

	while (s < end && XP_IS_SPACE (*s))
		s++;
	m_envelope_date.value = s;
	m_envelope_date.length = (uint16) (line_size - (s - m_envelope));
	while (XP_IS_SPACE (m_envelope_date.value [m_envelope_date.length - 1]))
		m_envelope_date.length--;

	/* #### short-circuit const */
	((char *) m_envelope_from.value) [m_envelope_from.length] = 0;
	((char *) m_envelope_date.value) [m_envelope_date.length] = 0;

	return 0;
}

extern "C" 
{
	char *strip_continuations(char *original);
	int16 INTL_DefaultMailToWinCharSetID(int16 csid);
	char *INTL_EncodeMimePartIIStr_VarLen(char *subject, int16 wincsid, XP_Bool bUseMime,
											int encodedWordLen);
}

static char *
msg_condense_mime2_string(char *sourceStr)
{
	int16 string_csid = CS_DEFAULT;
	int16 win_csid = CS_DEFAULT;

	char *returnVal = XP_STRDUP(sourceStr);
	if (!returnVal) 
		return NULL;
	
	// If sourceStr has a MIME-2 encoded word in it, get the charset
	// name/ID from the first encoded word.
	char *p = XP_STRSTR(returnVal, "=?");
	if (p)
	{
		p += 2;
		char *q = XP_STRCHR(p, '?');
		if (q) *q = '\0';
		string_csid = INTL_CharSetNameToID(p);
		win_csid = INTL_DocToWinCharSetID(string_csid);
		if (q) *q = '?';

		// Decode any MIME-2 encoded strings, to save the overhead.
		char *cvt = INTL_DecodeMimePartIIStr(returnVal, win_csid, FALSE);
		if (cvt)
		{
			if (cvt != returnVal)
			{
				XP_FREEIF(returnVal);
				returnVal = cvt;
			}
			// MIME-2 decoding occurred, so re-encode into large encoded words
			cvt = INTL_EncodeMimePartIIStr_VarLen(returnVal, win_csid, TRUE,
												MSG_MAXSUBJECTLENGTH - 2);
			if (cvt && (cvt != returnVal))
			{
				XP_FREE(returnVal);		// encoding happened, deallocate decoded text
				returnVal = strip_continuations(cvt); // and remove CR+LF+spaces that occur
			}
			// else returnVal == cvt, in which case nothing needs to be done
		}
		else
			// no MIME-2 decoding occurred, so strip CR+LF+spaces ourselves
			strip_continuations(returnVal);
	}
	else if (returnVal)
		strip_continuations(returnVal);
	
	return returnVal;
}

int ParseMailMessageState::InternSubject (struct message_header *header)
{
	char *key;
	uint32 L;
	MSG_DBHandle db = (m_mailDB) ? m_mailDB->GetDB() : 0;

	if (!header || header->length == 0)
	{
		m_newMsgHdr->SetSubject("", db);
		return 0;
	}

	XP_ASSERT (header->length == (short) XP_STRLEN (header->value));

	key = (char *) header->value;  /* #### const evilness */

	L = header->length;


	/* strip "Re: " */
	if (msg_StripRE((const char **) &key, &L))
	{
		m_newMsgHdr->SetFlags(m_newMsgHdr->GetFlags() | kHasRe);
	}

//  if (!*key) return 0; /* To catch a subject of "Re:" */

	// Condense the subject text into as few MIME-2 encoded words as possible.
	char *condensedKey = msg_condense_mime2_string(key);

	m_newMsgHdr->SetSubject(condensedKey ? condensedKey : key, db);
	XP_FREEIF(condensedKey);

	return 0;
}

/* Like mbox_intern() but for headers which contain email addresses:
   we extract the "name" component of the first address, and discard
   the rest. */
int ParseMailMessageState::InternRfc822 (struct message_header *header, 
									 char **ret_name)
{
	char	*s;

	if (!header || header->length == 0)
		return 0;

	XP_ASSERT (header->length == (short) XP_STRLEN (header->value));
	XP_ASSERT (ret_name != NULL);
	/* #### The mallocs here might be a performance bottleneck... */
	s = MSG_ExtractRFC822AddressName (header->value);
	if (! s)
		return MK_OUT_OF_MEMORY;

	*ret_name = s;
	return 0;
}

int ParseMailMessageState::FinalizeHeaders()
{
	int status = 0;
	struct message_header *sender;
	struct message_header *recipient;
	struct message_header *subject;
	struct message_header *id;
	struct message_header *references;
	struct message_header *date;
	struct message_header *statush;
	struct message_header *mozstatus;
	struct message_header *mozstatus2;
	struct message_header *priority;
	struct message_header *ccList;
	struct message_header *mdn_dnt;

	HG23277
	const char *s;
	uint32 flags = 0;
	uint32 delta = 0;
	MSG_PRIORITY priorityFlags = MSG_PriorityNotSet;

	MSG_DBHandle db = (m_mailDB) ? m_mailDB->GetDB() : 0;
	if (!db)		// if we don't have a valid db, skip the header.
		return 0;
#ifdef _USRDLL
	return(0);
#endif

	struct message_header to;
	GetAggregateHeader (m_toList, &to);
	struct message_header cc;
	GetAggregateHeader (m_ccList, &cc);

	sender     = (m_from.length          ? &m_from :
				m_sender.length        ? &m_sender :
				m_envelope_from.length ? &m_envelope_from :
				0);
	recipient  = (to.length         ? &to :
				cc.length         ? &cc :
				m_newsgroups.length ? &m_newsgroups :
				sender);
	ccList	   = (cc.length ? &cc : 0);
	subject    = (m_subject.length    ? &m_subject    : 0);
	id         = (m_message_id.length ? &m_message_id : 0);
	references = (m_references.length ? &m_references : 0);
	statush    = (m_status.length     ? &m_status     : 0);
	mozstatus  = (m_mozstatus.length  ? &m_mozstatus  : 0);
	mozstatus2  = (m_mozstatus2.length  ? &m_mozstatus2  : 0);
	date       = (m_date.length       ? &m_date :
				m_envelope_date.length ? &m_envelope_date :
				0);
	priority   = (m_priority.length   ? &m_priority   : 0);
	mdn_dnt	   = (m_mdn_dnt.length	  ? &m_mdn_dnt	  : 0);

	if (mozstatus) 
	{
		if (strlen(mozstatus->value) == 4) 
		{
#define UNHEX(C) \
	  ((C >= '0' && C <= '9') ? C - '0' : \
	   ((C >= 'A' && C <= 'F') ? C - 'A' + 10 : \
		((C >= 'a' && C <= 'f') ? C - 'a' + 10 : 0)))
			int i;
			for (i=0,s=mozstatus->value ; i<4 ; i++,s++) 
			{
				flags = (flags << 4) | UNHEX(*s);
			}
			// strip off and remember priority bits.
			flags &= ~MSG_FLAG_RUNTIME_ONLY;
			priorityFlags = (MSG_PRIORITY) ((flags & MSG_FLAG_PRIORITIES) >> 13);
			flags &= ~MSG_FLAG_PRIORITIES;
		  /* We trust the X-Mozilla-Status line to be the smartest in almost
			 all things.  One exception, however, is the HAS_RE flag.  Since
			 we just parsed the subject header anyway, we expect that parsing
			 to be smartest.  (After all, what if someone just went in and
			 edited the subject line by hand?) */
		}
		delta = (m_headerstartpos +
			 (mozstatus->value - m_headers) -
			 (2 + X_MOZILLA_STATUS_LEN)		/* 2 extra bytes for ": ". */
			 ) - m_envelope_pos;
	}

	if (mozstatus2)
	{
		uint32 flags2 = 0;
		sscanf(mozstatus2->value, " %lx ", &flags2);
		flags |= flags2;
	}

	if (!(flags & MSG_FLAG_EXPUNGED))	// message was deleted, don't bother creating a hdr.
	{
		m_newMsgHdr = new MailMessageHdr; // TODO - should be try catch?
		if (m_newMsgHdr)
		{
			if (m_newMsgHdr->GetFlags() & kHasRe)
				flags |= MSG_FLAG_HAS_RE;
			else
				flags &= ~MSG_FLAG_HAS_RE;

			if (mdn_dnt && !(m_newMsgHdr->GetFlags() & kIsRead) &&
				!(m_newMsgHdr->GetFlags() & kMDNSent))
				flags |= MSG_FLAG_MDN_REPORT_NEEDED;

			MessageDB::ConvertPublicFlagsToDBFlags(&flags);
			m_newMsgHdr->SetFlags(flags);
			if (priorityFlags != MSG_PriorityNotSet)
				m_newMsgHdr->SetPriority(priorityFlags);

			if (delta < 0xffff) 
			{		/* Only use if fits in 16 bits. */
				m_newMsgHdr->SetStatusOffset((uint16) delta);
				if (!m_IgnoreXMozillaStatus)	// imap doesn't care about X-MozillaStatus
					XP_ASSERT(m_newMsgHdr->GetStatusOffset() < 10000); /* ### Debugging hack */
			}
			m_newMsgHdr->SetAuthor(sender->value, db);
			if (recipient == &m_newsgroups)
			{
			  /* In the case where the recipient is a newsgroup, truncate the string
				 at the first comma.  This is used only for presenting the thread list,
				 and newsgroup lines tend to be long and non-shared, and tend to bloat
				 the string table.  So, by only showing the first newsgroup, we can
				 reduce memory and file usage at the expense of only showing the one
				 group in the summary list, and only being able to sort on the first
				 group rather than the whole list.  It's worth it. */
				char *s;
				XP_ASSERT (recipient->length == (uint16) XP_STRLEN (recipient->value));
				s = XP_STRCHR (recipient->value, ',');
				if (s)
				{
					*s = 0;
					recipient->length = XP_STRLEN (recipient->value);
				}
				m_newMsgHdr->SetRecipients(recipient->value, db, FALSE);
			}
			else
			{
				// note that we're now setting the whole recipient list,
				// not just the pretty name of the first recipient.
				m_newMsgHdr->SetRecipients(recipient->value, db, TRUE);
			}
			if (ccList)
				m_newMsgHdr->SetCCList(ccList->value, db);
			status = InternSubject (subject);
			if (status >= 0)
			{
				
				HG92923
			  /* Take off <> around message ID. */
				if (id->value[0] == '<')
					id->value++, id->length--;
				if (id->value[id->length-1] == '>')
					((char *) id->value) [id->length-1] = 0, id->length--; /* #### const */

				m_newMsgHdr->SetMessageId(id->value, db);

				if (!mozstatus && statush)
				{
				  /* Parse a little bit of the Berkeley Mail status header. */
				  for (s = statush->value; *s; s++)
					switch (*s)
					  {
					  case 'R': case 'r':
						m_newMsgHdr->SetFlags(m_newMsgHdr->GetFlags() | MSG_FLAG_READ);
						break;
					  case 'D': case 'd':
						/* msg->flags |= MSG_FLAG_EXPUNGED;  ### Is this reasonable? */
						break;
					  case 'N': case 'n':
					  case 'U': case 'u':
						m_newMsgHdr->SetFlags(m_newMsgHdr->GetFlags()  & ~MSG_FLAG_READ);
						break;
					  }
				}

				if (references != NULL)
					m_newMsgHdr->SetReferences(references->value, db);
				if (date)
					m_newMsgHdr->SetDate(XP_ParseTimeString (date->value, FALSE));

				if (priority)
					m_newMsgHdr->SetPriority(priority->value);
				else if (priorityFlags == MSG_PriorityNotSet)
					m_newMsgHdr->SetPriority(MSG_NoPriority);
			}
		} 
		else
			status = MK_OUT_OF_MEMORY;	
	}
	else
		status = 0;

	//### why is this stuff const?
	char *tmp = (char*) to.value;
	XP_FREEIF(tmp);
	tmp = (char*) cc.value;
	XP_FREEIF(tmp);

	return status;
}

int ParseNewMailState::MarkFilteredMessageRead(MailMessageHdr *msgHdr)
{
	if (m_mailDB)
		m_mailDB->MarkHdrRead(msgHdr, TRUE, NULL);
	else
		msgHdr->OrFlags(kIsRead);
	return 0;
}

int ParseNewMailState::MoveIncorporatedMessage(MailMessageHdr *mailHdr, 
											   MailDB *sourceDB, 
											   char *destFolder,
											   MSG_Filter *filter)
{
	int err = 0;
	XP_File		destFid;
	XP_File		sourceFid = m_file;

	// Make sure no one else is writing into this folder
	MSG_FolderInfo *lockedFolder = m_mailMaster->FindMailFolder (destFolder, FALSE /*create*/);
	if (lockedFolder && (err = lockedFolder->AcquireSemaphore (this)) != 0)
		return err;

	if (sourceFid == 0)
	{
		sourceFid = XP_FileOpen(m_mailboxName,
										xpMailFolder, XP_FILE_READ_BIN);
	}
	XP_ASSERT(sourceFid != 0);
	if (sourceFid == 0)
	{
#ifdef DEBUG_bienvenu
		XP_ASSERT(FALSE);
#endif
		if (lockedFolder)
			lockedFolder->ReleaseSemaphore (this);

		return MK_MSG_FOLDER_UNREADABLE;	// ### dmb
	}

	XP_FileSeek (sourceFid, mailHdr->GetMessageOffset(), SEEK_SET);
	int newMsgPos;

	destFid = XP_FileOpen(destFolder, xpMailFolder, XP_FILE_APPEND_BIN);

	if (!destFid) 
	{
#ifdef DEBUG_bienvenu
		XP_ASSERT(FALSE);
#endif
		if (lockedFolder)
			lockedFolder->ReleaseSemaphore (this);
		XP_FileClose (sourceFid);
		return  MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	}

	if (!XP_FileSeek (destFid, 0, SEEK_END))
	{
		newMsgPos = ftell (destFid);
	}
	else
	{
		XP_ASSERT(FALSE);
		if (lockedFolder)
			lockedFolder->ReleaseSemaphore (this);
		XP_FileClose (destFid);
		XP_FileClose (sourceFid);
		return  MK_MSG_ERROR_WRITING_MAIL_FOLDER;
	}

	HG98373
	uint32 length = mailHdr->GetByteLength();

	m_ibuffer_size = 10240;
	m_ibuffer = NULL;

	while (!m_ibuffer && (m_ibuffer_size >= 512))
	{
		m_ibuffer = (char *) XP_ALLOC(m_ibuffer_size);
		if (m_ibuffer == NULL)
			m_ibuffer_size /= 2;
	}
	XP_ASSERT(m_ibuffer != NULL);
	while ((length > 0) && m_ibuffer)
	{
		uint32 nRead = XP_FileRead (m_ibuffer, length > m_ibuffer_size ? m_ibuffer_size  : length, sourceFid);
		if (nRead == 0)
			break;
		
		// we must monitor the number of bytes actually written to the file. (mscott)
		if (XP_FileWrite (m_ibuffer, nRead, destFid) != nRead) 
		{
			XP_FileClose(sourceFid);
			XP_FileClose(destFid);     

			// truncate  destination file in case message was partially written
			XP_FileTruncate(destFolder,xpMailFolder,newMsgPos);

			if (lockedFolder)
				lockedFolder->ReleaseSemaphore(this);

			return MK_MSG_ERROR_WRITING_MAIL_FOLDER;   // caller (ApplyFilters) currently ignores error conditions
		}
			
		length -= nRead;
	}
	
	XP_ASSERT(length == 0);

	// if we have made it this far then the message has successfully been written to the new folder
	// now add the header to the mailDb.
	
	MailDB *mailDb = NULL;
	// don't force upgrade in place
	MsgERR msgErr = MailDB::Open (destFolder, TRUE, &mailDb);	
	if (eSUCCESS == msgErr)
	{
		MailMessageHdr *newHdr = new MailMessageHdr();	
		if (newHdr)
		{
			newHdr->CopyFromMsgHdr (mailHdr, sourceDB->GetDB(), mailDb->GetDB());
			// set new byte offset, since the offset in the old file is certainly wrong
			newHdr->SetMessageKey (newMsgPos); 
			newHdr->OrFlags(kNew);

			msgErr = mailDb->AddHdrToDB (newHdr, NULL, m_updateAsWeGo);
			delete newHdr;
		}
	}
	else
	{
		if (mailDb)
		{
			mailDb->Close();
			mailDb = NULL;
		}
	}

	XP_FileClose(sourceFid);
	XP_FileClose(destFid);
	int truncRet = XP_FileTruncate(m_mailboxName, xpMailFolder, mailHdr->GetMessageOffset());
	XP_ASSERT(truncRet >= 0);

	if (lockedFolder)
		lockedFolder->ReleaseSemaphore (this);

	// tell outgoing parser that we've truncated the Inbox
	m_parseMsgState->Init(mailHdr->GetMessageOffset());
	MSG_FolderInfo *folder = m_mailMaster->FindMailFolder(destFolder, FALSE);

	if (folder)
		folder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);

	if (mailDb != NULL)
	{
		// update the folder size so we won't reparse.
		UpdateDBFolderInfo(mailDb, destFolder);
		if (folder != NULL)
			folder->SummaryChanged();

		mailDb->Close();
	}
	// We are logging the hit with the old mailHdr, which should work, as long
	// as LogRuleHit doesn't assume the new hdr.
	if (m_filterList->IsLoggingEnabled())
		LogRuleHit(filter, mailHdr);

	return err;

}

ParseNewMailState::ParseNewMailState(MSG_Master *master, MSG_FolderInfoMail
									 *folder) :
	ParseMailboxState(folder->GetPathname())
{
	SetMaster(master);
	if (MSG_FilterList::Open(master, filterInbox, NULL, folder, &m_filterList)
		!= FilterError_Success)
		m_filterList = NULL;
	if (m_filterList)
	{
		const char *folderName = NULL;
		int32 int_pref = 0;
		PREF_GetIntPref("mail.incorporate.return_receipt", &int_pref);
		if (int_pref == 1)
		{
			MSG_FolderInfo *folderInfo = NULL;
			int status = 0;
			char *defaultFolderName =
				msg_MagicFolderName(master->GetPrefs(),
									MSG_FOLDER_FLAG_SENTMAIL, &status); 
			if (defaultFolderName)
			{
				folderInfo = master->FindMailFolder(defaultFolderName, FALSE);
				if (folderInfo && folderInfo->GetMailFolderInfo())
					folderName = folderInfo->GetMailFolderInfo()->GetPathname();
				XP_FREE(defaultFolderName);
			}
		}

		if (folderName)
		{
			MSG_Filter *newFilter = new MSG_Filter(filterInboxRule, "receipt");
			if (newFilter)
			{
				MSG_Rule *rule = NULL;
				MSG_SearchValue value;
				newFilter->SetDescription("incorporate mdn report");
				newFilter->SetEnabled(TRUE);
				newFilter->GetRule(&rule);
				newFilter->SetFilterList(m_filterList);
				value.attribute = attribOtherHeader;
				value.u.string = "multipart/report";
				rule->AddTerm(attribOtherHeader, opContains,
							  &value, TRUE, "Content-Type");
				value.u.string = "disposition-notification";
				rule->AddTerm(attribOtherHeader, opContains,
							  &value, TRUE, "Content-Type");
#if 0
				value.u.string = "delivery-status";
				rule->AddTerm(attribOtherHeader, opContains,
							  &value, FALSE, "Content-Type");
#endif
				rule->SetAction(acMoveToFolder, (void*)folderName);
				m_filterList->InsertFilterAt(0, newFilter);
			}
		}
	}
	m_logFile = NULL;
	m_usingTempDB = FALSE;
	m_tmpdbName = NULL;
	m_disableFilters = FALSE;
}

ParseNewMailState::~ParseNewMailState()
{
	if (m_filterList != NULL)
		MSG_CancelFilterList(m_filterList);
	if (m_logFile != NULL)
		XP_FileClose(m_logFile);
	if (m_mailDB)
		m_mailDB->Close();
	if (m_usingTempDB)
	{
		XP_FileRemove(m_tmpdbName, xpMailFolderSummary);
	}
	FREEIF(m_tmpdbName);
	JSFilter_cleanup();
}

void ParseNewMailState::LogRuleHit(MSG_Filter *filter, MailMessageHdr *msgHdr)
{
	char	*filterName = "";
	time_t	date;
	char	dateStr[40];	/* 30 probably not enough */
	MSG_RuleActionType actionType;
	MSG_Rule	*rule;
	void				*value;
	MSG_DBHandle db = (m_mailDB) ? m_mailDB->GetDB() : 0;
	XPStringObj	author;
	XPStringObj	subject;

	if (m_logFile == NULL)
		m_logFile = XP_FileOpen("", xpMailFilterLog, XP_FILE_APPEND);	// will this create?
	
	filter->GetName(&filterName);
	if (filter->GetRule(&rule) != FilterError_Success)
		return;
	rule->GetAction(&actionType, &value);
	date = msgHdr->GetDate();
	XP_StrfTime(m_context, dateStr, sizeof(dateStr), XP_DATE_TIME_FORMAT,
				localtime(&date));
	msgHdr->GetAuthor(author, db);
	msgHdr->GetSubject(subject, TRUE, db);
	if (m_logFile)
	{
		XP_FilePrintf(m_logFile, "Applied filter \"%s\" to message from %s - %s at %s\n", filterName, 
			(const char *) author, (const char *) subject, dateStr);
		char *actionStr = rule->GetActionStr(actionType);
		char *actionValue = "";
		if (actionType == acMoveToFolder)
			actionValue = (char *) value;
		XP_FilePrintf(m_logFile, "Action = %s %s\n\n", actionStr, actionValue);
		if (actionType == acMoveToFolder)
		{
			XPStringObj msgId;
			msgHdr->GetMessageId(msgId, db);
			XP_FilePrintf(m_logFile, "mailbox:%s?id=%s\n", value, (const char *) msgId);
		}
	}
}

MSG_FolderInfoMail *ParseNewMailState::GetTrashFolder()
{
	MSG_FolderInfo *foundTrash = NULL;
	GetMaster()->GetLocalMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, &foundTrash, 1);
	return foundTrash ? foundTrash->GetMailFolderInfo() : (MSG_FolderInfoMail *)NULL;
}

void ParseNewMailState::ApplyFilters(XP_Bool *pMoved)
{
	MSG_Filter	*filter;
	int32		filterCount = 0;
	XP_Bool		msgMoved = FALSE;
	MsgERR		err = eSUCCESS;

	MailMessageHdr	*msgHdr = GetCurrentMsg();
	if (m_filterList != NULL)
		m_filterList->GetFilterCount(&filterCount);

	for (MSG_FilterIndex filterIndex = 0; filterIndex < filterCount; filterIndex++)
	{
		if (m_filterList->GetFilterAt(filterIndex, &filter) == FilterError_Success)
		{
			if (!filter->GetEnabled())
				continue;

			if (filter->GetType() == filterInboxJavaScript)
				{
					if (JSMailFilter_execute(this, filter, msgHdr, m_mailDB, &msgMoved) == SearchError_Success)
						break;
				}
			else if (filter->GetType() == filterInboxRule)
			{
				MSG_Rule	*rule;
				MSG_SearchError matchTermStatus;

				if (filter->GetRule(&rule) == FilterError_Success)
				{
					{	// put this in its own block so scope will get destroyed
						// before we apply the actions, so folder file will get closed.
						MSG_ScopeTerm scope (NULL, scopeMailFolder, m_folder);

						char * headers = GetMsgState()->m_headers;
						uint32 headersSize = GetMsgState()->m_headers_fp;

						matchTermStatus = msg_SearchOfflineMail::MatchTermsForFilter(
							msgHdr, rule->GetTermList(), &scope, m_mailDB, headers, headersSize);

					}
					if (matchTermStatus  == SearchError_Success)
					{
						MSG_RuleActionType actionType;
						void				*value = NULL;
						// look at action - currently handle move
						XP_Trace("got a rule hit!\n");
						if (rule->GetAction(&actionType, &value) == FilterError_Success)
						{
							XP_Bool isRead = msgHdr->GetFlags() & kIsRead;
							switch (actionType)
							{
							case acDelete:
							{
								MSG_IMAPFolderInfoMail *imapFolder = m_folder->GetIMAPFolderInfoMail();
								XP_Bool serverIsImap  = GetMaster()->GetPrefs()->GetMailServerIsIMAP4();
								XP_Bool deleteToTrash = !imapFolder || imapFolder->DeleteIsMoveToTrash();
								if (deleteToTrash || !serverIsImap)
								{
									// set value to trash folder
									MSG_FolderInfoMail *mailTrash = GetTrashFolder();
									if (mailTrash)
										value = (void *) mailTrash->GetPathname();

									msgHdr->OrFlags(kIsRead);	// mark read in trash.
								}
								else	// (!deleteToTrash && serverIsImap)
								{
									msgHdr->OrFlags(kIsRead | kIMAPdeleted);
									IDArray	keysToFlag;

									keysToFlag.Add(msgHdr->GetMessageKey());
									if (imapFolder)
										imapFolder->StoreImapFlags(m_pane, kImapMsgSeenFlag | kImapMsgDeletedFlag, TRUE, keysToFlag, ((ParseIMAPMailboxState *) this)->GetFilterUrlQueue());
								}
							}
							case acMoveToFolder:
								// if moving to a different file, do it.
								if (value && XP_FILENAMECMP(m_mailDB->GetFolderName(), (char *) value))
								{
									if (msgHdr->GetFlags() & kMDNNeeded &&
										!isRead)
									{
										ParseMailMessageState *state =
											GetMsgState(); 
										struct message_header to;
										struct message_header cc;
										state->GetAggregateHeader (state->m_toList, &to);
										state->GetAggregateHeader (state->m_ccList, &cc);
										msgHdr->SetFlags(msgHdr->GetFlags() & ~kMDNNeeded);
										msgHdr->SetFlags(msgHdr->GetFlags() | kMDNSent);

										if (actionType == acDelete)
										{
											MSG_ProcessMdnNeededState processMdnNeeded
												(MSG_ProcessMdnNeededState::eDeleted,
												 m_pane, m_folder, msgHdr->GetMessageKey(),
												 &state->m_return_path, &state->m_mdn_dnt, 
												 &to, &cc, &state->m_subject, 
												 &state->m_date, &state->m_mdn_original_recipient,
												 &state->m_message_id, state->m_headers, 
												 (int32) state->m_headers_fp, TRUE);
										}
										char *tmp = (char*) to.value;
										XP_FREEIF(tmp);
										tmp = (char*) cc.value;
										XP_FREEIF(tmp);
									}
									err = MoveIncorporatedMessage(msgHdr, m_mailDB, (char *) value, filter);
									if (err == eSUCCESS)
										msgMoved = TRUE;

								}
								break;
							case acMarkRead:
								MarkFilteredMessageRead(msgHdr);
								break;
							case acKillThread:
								// for ignore and watch, we will need the db
								// to check for the flags in msgHdr's that
								// get added, because only then will we know
								// the thread they're getting added to.
								msgHdr->OrFlags(kIgnored);
								break;
							case acWatchThread:
								msgHdr->OrFlags(kWatched);
								break;
							case acChangePriority:
								m_mailDB->SetPriority(msgHdr,  * ((MSG_PRIORITY *) &value));
								break;
							default:
								break;
							}
							if (m_filterList->IsLoggingEnabled() && !msgMoved && actionType != acMoveToFolder)
								LogRuleHit(filter, msgHdr);
						}
						break;
					}
				}
			}
		}
	}
	if (pMoved)
		*pMoved = msgMoved;
}

// This gets called for every message because libnet calls IncorporateBegin,
// IncorporateWrite (once or more), and IncorporateComplete for every message.
void ParseNewMailState::DoneParsingFolder()
{
	XP_Bool moved = FALSE;
/* End of file.  Flush out any partial line remaining in the buffer. */
	if (m_ibuffer_fp > 0) 
	{
		m_parseMsgState->ParseFolderLine(m_ibuffer, m_ibuffer_fp);
		m_ibuffer_fp = 0;
	}
	PublishMsgHeader();
	if (!moved && m_mailDB != NULL)	// finished parsing, so flush db folder info 
		UpdateDBFolderInfo();

	if (m_folder != NULL)
		m_folder->SummaryChanged();

	/* We're done reading the folder - we don't need these things
	 any more. */
	FREEIF (m_ibuffer);
	m_ibuffer_size = 0;
	FREEIF (m_obuffer);
	m_obuffer_size = 0;
}

int32 ParseNewMailState::PublishMsgHeader()
{
	XP_Bool		moved = FALSE;

	m_parseMsgState->FinishHeader();
	
	if (m_parseMsgState->m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_parseMsgState->m_newMsgHdr);
		if (!m_disableFilters) {
			ApplyFilters(&moved);
		}
		if (!moved)
		{
			if (m_mailDB)
			{
				m_parseMsgState->m_newMsgHdr->OrFlags(kNew);
				m_mailDB->AddHdrToDB (m_parseMsgState->m_newMsgHdr, NULL,
									  m_updateAsWeGo);
				delete m_parseMsgState->m_newMsgHdr;
			}
			if (m_folder)
				m_folder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);


		}		// if it was moved by imap filter, m_parseMsgState->m_newMsgHdr == NULL
		else if (m_parseMsgState->m_newMsgHdr)
		{
			// make sure global db is set correctly for delete of strings from hash tbl.
			// we do this now by remembering the db in the mail hdr object inside the db
			delete m_parseMsgState->m_newMsgHdr;
		}
		m_parseMsgState->m_newMsgHdr = NULL;
	}
	return 0;
}

void	ParseNewMailState::SetUsingTempDB(XP_Bool usingTempDB, char *tmpDBName)
{
	m_usingTempDB = usingTempDB;
	m_tmpdbName = tmpDBName;
}

ParseIMAPMailboxState::ParseIMAPMailboxState(MSG_Master *master, MSG_IMAPHost *host, MSG_FolderInfoMail *folder,
											 MSG_UrlQueue *urlQueue, TImapFlagAndUidState *flagStateAdopted)
											: ParseNewMailState(master, folder), fUrlQueue(urlQueue)
{
 	MSG_FolderInfoContainer *imapContainer =  m_mailMaster->GetImapMailFolderTreeForHost(host->GetHostName());
 	MSG_FolderInfo *filteredFolder = imapContainer->FindMailPathname(folder->GetPathname());
 	fParsingInbox = 0 != (filteredFolder->GetFlags() & MSG_FOLDER_FLAG_INBOX);
 	fFlagState = flagStateAdopted;
 	fB2HaveWarnedUserOfOfflineFiltertarget = FALSE;
 	
 	// we ignore X-mozilla status for imap messages
 	GetMsgState()->m_IgnoreXMozillaStatus = TRUE;
	fNextSequenceNum = -1;
	m_host = host;
	m_imapContainer = imapContainer;
}

ParseIMAPMailboxState::~ParseIMAPMailboxState()
{
}

void ParseIMAPMailboxState::SetPublishUID(int32 uid)
{
	fNextMessagePublishUID = uid;
}

void ParseIMAPMailboxState::SetPublishByteLength(uint32 byteLength)
{
	fNextMessageByteLength = byteLength;
}

void ParseIMAPMailboxState::DoneParsingFolder()
{
	ParseMailboxState::DoneParsingFolder();
	if (m_mailDB)
	{
		// make sure the highwater mark is correct
		if (m_mailDB->m_dbFolderInfo->GetNumVisibleMessages())
		{
			ListContext *listContext;
			DBMessageHdr *currentHdr;
			if ((m_mailDB->ListLast(&listContext, &currentHdr) == eSUCCESS) &&
				currentHdr)
			{
				m_mailDB->m_dbFolderInfo->m_LastMessageUID = currentHdr->GetMessageKey();
				delete currentHdr;
				m_mailDB->ListDone(listContext);
			}
			else
				m_mailDB->m_dbFolderInfo->m_LastMessageUID = 0;
		}
		else
			m_mailDB->m_dbFolderInfo->m_LastMessageUID = 0;
			
// DMB TODO    	m_mailDB->m_dbFolderInfo->setDirty();
    	
		m_mailDB->Close();
		m_mailDB = NULL;
	}
}

int ParseIMAPMailboxState::MarkFilteredMessageRead(MailMessageHdr *msgHdr)
{
	msgHdr->OrFlags(kIsRead);
	IDArray	keysToFlag;

	keysToFlag.Add(msgHdr->GetMessageKey());
	if (m_folder->GetType() == FOLDER_IMAPMAIL)
	{
		MSG_IMAPFolderInfoMail *imapFolder = m_folder->GetIMAPFolderInfoMail();
		if (imapFolder)
		{
			imapFolder->StoreImapFlags(m_pane, kImapMsgSeenFlag, TRUE, keysToFlag, GetFilterUrlQueue());
		}
		else
		{
			XP_ASSERT(FALSE);	// former location of a cast.
		}
	}

	return 0;
}


int ParseIMAPMailboxState::MoveIncorporatedMessage(MailMessageHdr *mailHdr, 
											   MailDB *sourceDB, 
											   char *destFolder,
											   MSG_Filter *filter)
{
	int err = eUNKNOWN;
	
	if (fUrlQueue && fUrlQueue->GetPane())
	{
	 	// look for matching imap folders, then pop folders
	 	MSG_FolderInfoContainer *imapContainer =  m_imapContainer;
		MSG_FolderInfo *sourceFolder = imapContainer->FindMailPathname(m_mailboxName);
	 	MSG_FolderInfo *destinationFolder = imapContainer->FindMailPathname(destFolder);
	 	if (!destinationFolder)
	 		destinationFolder = m_mailMaster->FindMailFolder(destFolder, FALSE);

	 	if (destinationFolder)
	 	{
			MSG_FolderInfo *inbox=NULL;
			imapContainer->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, &inbox, 1);
			if (inbox)
			{
				MSG_FolderInfoMail *destMailFolder = destinationFolder->GetMailFolderInfo();
				// put the header into the source db, since it needs to be there when we copy it
				// and we need a valid header to pass to StartAsyncCopyMessagesInto
				MessageKey keyToFilter = mailHdr->GetMessageKey();

				if (sourceDB && destMailFolder)
				{
					XP_Bool imapDeleteIsMoveToTrash = m_host->GetDeleteIsMoveToTrash();
					
					IDArray *idsToMoveFromInbox = destMailFolder->GetImapIdsToMoveFromInbox();
					idsToMoveFromInbox->Add(keyToFilter);

					// this is our last best chance to log this
					if (m_filterList->IsLoggingEnabled())
						LogRuleHit(filter, mailHdr);

					if (imapDeleteIsMoveToTrash)
					{
						if (m_parseMsgState->m_newMsgHdr)
						{
							delete m_parseMsgState->m_newMsgHdr;
							m_parseMsgState->m_newMsgHdr = NULL;
						}
					}
					
					destinationFolder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);
					
					if (imapDeleteIsMoveToTrash)	
						err = 0;
				}
			}
	 	}
	}
	
	
	// we have to return an error because we do not actually move the message
	// it is done async and that can fail
	return err;
}

MSG_FolderInfoMail *ParseIMAPMailboxState::GetTrashFolder()
{
	MSG_IMAPFolderInfoContainer *imapContainerInfo = m_imapContainer->GetIMAPFolderInfoContainer();
	if (!imapContainerInfo)
		return NULL;

	MSG_FolderInfo *foundTrash = MSG_GetTrashFolderForHost(imapContainerInfo->GetIMAPHost());
	return foundTrash ? foundTrash->GetMailFolderInfo() : (MSG_FolderInfoMail *)NULL;
}


// only apply filters for new unread messages in the imap inbox
void ParseIMAPMailboxState::ApplyFilters(XP_Bool *pMoved)
{
 	if (fParsingInbox && !(GetCurrentMsg()->GetFlags() & kIsRead) )
 		ParseNewMailState::ApplyFilters(pMoved);
 	else
 		*pMoved = FALSE;
 	
 	if (!*pMoved && m_parseMsgState->m_newMsgHdr)
 		fFetchBodyKeys.Add(m_parseMsgState->m_newMsgHdr->GetMessageKey());
}


// For IMAP, the message key is the IMAP UID
// This is where I will add code to fix the message length as well - km
void ParseIMAPMailboxState::FolderTypeSpecificTweakMsgHeader(MailMessageHdr *tweakMe)
{
	if (m_mailDB && tweakMe)
	{
		tweakMe->SetMessageKey(fNextMessagePublishUID);
		tweakMe->SetByteLength(fNextMessageByteLength);
		tweakMe->SetMessageSize(fNextMessageByteLength);
		
		if (fFlagState)
		{
			XP_Bool foundIt = FALSE;
			imapMessageFlagsType imap_flags = IMAP_GetMessageFlagsFromUID(fNextMessagePublishUID, &foundIt, fFlagState);
			if (foundIt)
			{
				// make a mask and clear these message flags
				uint32 mask = MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_MARKED | MSG_FLAG_IMAP_DELETED;
				tweakMe->SetFlags(tweakMe->GetFlags() & ~mask);
				
				// set the new value for these flags
				uint32 newFlags = 0;
				if (imap_flags & kImapMsgSeenFlag)
					newFlags |= MSG_FLAG_READ;
				else // if (imap_flags & kImapMsgRecentFlag)
					newFlags |= MSG_FLAG_NEW;

				// Okay here is the MDN needed logic (if DNT header seen):
				/* if server support user defined flag:
						MDNSent flag set => clear kMDNNeeded flag
						MDNSent flag not set => do nothing, leave kMDNNeeded on
				   else if 
						not MSG_FLAG_NEW => clear kMDNNeeded flag
						MSG_FLAG_NEW => do nothing, leave kMDNNeeded on
				 */
				if (imap_flags & kImapMsgSupportUserFlag)
				{
					if (imap_flags & kImapMsgMDNSentFlag)
					{
						newFlags |= kMDNSent;
						if (tweakMe->GetFlags() & kMDNNeeded)
							tweakMe->SetFlags(tweakMe->GetFlags() & ~kMDNNeeded);
					}
				}
				else
				{
					if (!(imap_flags & kImapMsgRecentFlag) && 
						tweakMe->GetFlags() & kMDNNeeded)
						tweakMe->SetFlags(tweakMe->GetFlags() & ~kMDNNeeded);
				}

				if (imap_flags & kImapMsgAnsweredFlag)
					newFlags |= MSG_FLAG_REPLIED;
				if (imap_flags & kImapMsgFlaggedFlag)
					newFlags |= MSG_FLAG_MARKED;
				if (imap_flags & kImapMsgDeletedFlag)
					newFlags |= MSG_FLAG_IMAP_DELETED;

				if (newFlags)
					tweakMe->SetFlags(tweakMe->GetFlags() | newFlags);
			}
		}
	}
}

int32	ParseIMAPMailboxState::PublishMsgHeader()
{
	XP_Bool		moved = FALSE;

	m_parseMsgState->FinishHeader();
	
	if (m_parseMsgState->m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_parseMsgState->m_newMsgHdr);
		if (!m_disableFilters) {
			ApplyFilters(&moved);
		}
		if (!moved)
		{
			XP_Bool thisMessageUnRead = !(m_parseMsgState->m_newMsgHdr->GetFlags() & kIsRead);
			if (m_mailDB)
			{
				if (thisMessageUnRead)
					m_parseMsgState->m_newMsgHdr->OrFlags(kNew);
				m_mailDB->AddHdrToDB (m_parseMsgState->m_newMsgHdr, NULL,
					(fNextSequenceNum == -1) ? m_updateAsWeGo : FALSE);
				// following is for cacheless imap - match sequence number
				// to location to insert in view.
				if (m_msgDBView != NULL && fNextSequenceNum != -1)
				{
					m_msgDBView->InsertHdrAt(m_parseMsgState->m_newMsgHdr, fNextSequenceNum++ - 1);
#ifdef DEBUG_bienvenu
					XP_Trace("adding hdr to cacheless view at %ld\n", fNextSequenceNum - 2);
#endif
				}
				delete m_parseMsgState->m_newMsgHdr;
			}
			if (m_folder && thisMessageUnRead)
				m_folder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);


		}		// if it was moved by imap filter, m_parseMsgState->m_newMsgHdr == NULL
		else if (m_parseMsgState->m_newMsgHdr)
		{
			// make sure global db is set correctly for delete of strings from hash tbl.
			delete m_parseMsgState->m_newMsgHdr;
		}
		m_parseMsgState->m_newMsgHdr = NULL;
	}
	return 0;
}

void ParseIMAPMailboxState::SetNextSequenceNum(int32 seqNum)
{
	fNextSequenceNum = seqNum;
}

ParseOutgoingMessage::ParseOutgoingMessage()
{
	m_bytes_written = 0;
	m_out_file = 0;
	m_wroteXMozillaStatus = FALSE;
	m_writeToOutFile = TRUE;
	m_lastBodyLineEmpty = FALSE;
	m_outputBuffer = 0;
	m_ouputBufferSize = 0;
	m_outputBufferIndex = 0;
}

ParseOutgoingMessage::~ParseOutgoingMessage()
{
	FREEIF (m_outputBuffer);
}

void ParseOutgoingMessage::Clear()
{
	ParseMailMessageState::Clear();
	m_wroteXMozillaStatus = FALSE;
	m_bytes_written = 0;
}

// We've found the start of the next message, so finish this one off.
void ParseOutgoingMessage::FinishHeader()
{
	int32 origPosition = m_position, len = 0;
	if (m_out_file && m_writeToOutFile)
	{
		if (origPosition > 0 && !m_lastBodyLineEmpty)
		{
			len = XP_FileWrite (LINEBREAK, LINEBREAK_LEN, m_out_file);
			m_bytes_written += LINEBREAK_LEN;
			m_position += LINEBREAK_LEN;
		}
	}
	ParseMailMessageState::FinishHeader();
}

int		ParseOutgoingMessage::StartNewEnvelope(const char *line, uint32 lineLength)
{
	int status = ParseMailMessageState::StartNewEnvelope(line, lineLength);

	if ((status >= 0) && m_out_file && m_writeToOutFile)
	{
		status = XP_FileWrite(line, lineLength, m_out_file);
		if (status > 0)
			m_bytes_written += lineLength;
	}
	return status;
}

int32	ParseOutgoingMessage::ParseFolderLine(const char *line, uint32 lineLength)
{
	int32 res = ParseMailMessageState::ParseFolderLine(line, lineLength);
	int len = 0;

	if (res < 0)
		return res;
	if (m_out_file && m_writeToOutFile)
	{
		if (!XP_STRNCMP(line, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN)) 
			m_wroteXMozillaStatus = TRUE;

		m_lastBodyLineEmpty = (m_state == MBOX_PARSE_BODY && (EMPTY_MESSAGE_LINE(line)));

		// make sure we mangle naked From lines
		if (line[0] == 'F' && !XP_STRNCMP (line, "From ", 5))
		{
			res = XP_FileWrite (">", 1, m_out_file);
			if (res < 1)
				return res;
			m_position += 1;
		}
		if (!m_wroteXMozillaStatus && m_state == MBOX_PARSE_BODY)
		{
			char buf[50];
			uint32 dbFlags = m_newMsgHdr ? m_newMsgHdr->GetFlags() : 0;

			if (m_newMsgHdr)
				m_newMsgHdr->SetStatusOffset(m_bytes_written);
			PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS_FORMAT LINEBREAK, (m_newMsgHdr) ? m_newMsgHdr->GetMozillaStatusFlags() & ~MSG_FLAG_RUNTIME_ONLY : 0);
			len = strlen(buf);
			res = XP_FileWrite(buf, len, m_out_file);
			if (res < len)
				return res;
			m_bytes_written += len;
			m_position += len;
			m_wroteXMozillaStatus = TRUE;

			MessageDB::ConvertDBFlagsToPublicFlags(&dbFlags);
			dbFlags &= (MSG_FLAG_MDN_REPORT_NEEDED | MSG_FLAG_MDN_REPORT_SENT | MSG_FLAG_TEMPLATE);
			PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS2_FORMAT LINEBREAK, dbFlags);
			len = strlen(buf);
			res = XP_FileWrite(buf, len, m_out_file);
			if (res < len)
				return res;
			m_bytes_written += len;
			m_position += len;
		}
		res = XP_FileWrite(line, lineLength, m_out_file);
		if (res == lineLength)
			m_bytes_written += lineLength;
	}
	return res;
}

/* static */
int32 ParseOutgoingMessage::LineBufferCallback(char *line, uint32 lineLength,
									  void *closure)
{
	ParseOutgoingMessage *parseState = (ParseOutgoingMessage *) closure;

	return parseState->ParseFolderLine(line, lineLength);
}

int32 ParseOutgoingMessage::ParseBlock(const char *block, uint32 length)
{
	m_ouputBufferSize = 10240;
	while (m_outputBuffer == 0)
	{
		
		m_outputBuffer = (char *) XP_ALLOC(m_ouputBufferSize);
		if (m_outputBuffer == NULL)
			m_ouputBufferSize /= 2;
	}
	XP_ASSERT(m_outputBuffer != NULL);

	return msg_LineBuffer (block, length, &m_outputBuffer, &m_ouputBufferSize,  &m_outputBufferIndex, FALSE,
#ifdef XP_OS2
					(int32 (_Optlink*) (char*,uint32,void*))
#endif
					   LineBufferCallback, this);
}

void ParseOutgoingMessage::AdvanceOutPosition(uint32 amountToAdvance)
{
	m_position += amountToAdvance;
	m_bytes_written += amountToAdvance;
}

void ParseOutgoingMessage::FlushOutputBuffer()
{
/* End of file.  Flush out any partial line remaining in the buffer. */
	if (m_outputBufferIndex > 0) 
	{
		ParseFolderLine(m_outputBuffer, m_outputBufferIndex);
		m_outputBufferIndex = 0;
	}
}

