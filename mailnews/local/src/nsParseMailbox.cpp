/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsParseMailbox.h"
#include "nsIMessage.h"
#include "nsMailDatabase.h"
#include "nsIDBFolderInfo.h"
#include "nsIByteBuffer.h"
#include "nsIInputStream.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsMsgBaseCID.h"
#include "nsMsgDBCID.h"
#include "libi18n.h"
#include "nsIMailboxUrl.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ADDREF(nsMsgMailboxParser)
NS_IMPL_RELEASE(nsMsgMailboxParser)
NS_IMPL_QUERY_INTERFACE(nsMsgMailboxParser, nsIStreamListener::GetIID()); /* we need to pass in the interface ID of this interface */

// Whenever data arrives from the connection, core netlib notifices the protocol by calling
// OnDataAvailable. We then read and process the incoming data from the input stream. 
NS_IMETHODIMP nsMsgMailboxParser::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	// right now, this really just means turn around and process the url
	ProcessMailboxInputStream(aURL, aIStream, aLength);
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailboxParser::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
	// extract the appropriate event sinks from the url and initialize them in our protocol data
	// the URL should be queried for a nsIMailboxURL. If it doesn't support a mailbox URL interface then
	// we have an error.
	nsIMailboxUrl *runningUrl;
	printf("\n+++ nsMsgMailboxParser::OnStartBinding: URL: %p, Content type: %s\n", aURL, aContentType);

	nsresult rv = aURL->QueryInterface(nsIMailboxUrl::GetIID(), (void **)&runningUrl);
	if (NS_SUCCEEDED(rv) && runningUrl)
	{
		// okay, now fill in our event sinks...Note that each getter ref counts before
		// it returns the interface to us...we'll release when we are done
		const char	*fileName;
		runningUrl->GetFile(&fileName);
		if (fileName)
		{	
			nsFileSpec dbName(fileName);

			nsIMsgDatabase * mailDB = nsnull;
			rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDB);
			if (NS_SUCCEEDED(rv) && mailDB)
			{
				rv = mailDB->Open(dbName, PR_TRUE, (nsIMsgDatabase **) &m_mailDB, PR_TRUE);
				mailDB->Release();
			}
			NS_ASSERTION(m_mailDB, "failed to open mail db parsing folder");
			printf("url file = %s\n", fileName);
		}
	}

	// need to get the mailbox name out of the url and call SetMailboxName with it.
	// then, we need to open the mail db for this parser.
	return rv;

}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsMsgMailboxParser::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
	DoneParsingFolder();
	// what can we do? we can close the stream?
	m_urlInProgress = PR_FALSE;  // don't close the connection...we may be re-using it.

	// and we want to mark ourselves for deletion or some how inform our protocol manager that we are 
	// available for another url if there is one....
#ifdef DEBUG
	// let's dump out the contents of our db, if possible.
	if (m_mailDB)
	{
		nsMsgKeyArray	keys;
		nsString		author;
		nsString		subject;

//		m_mailDB->PrePopulate();
		m_mailDB->ListAllKeys(keys);
		for (PRUint32 index = 0; index < keys.GetSize(); index++)
		{
			nsIMessage *msgHdr = NULL;
			nsresult ret = m_mailDB->GetMsgHdrForKey(keys[index], &msgHdr);
			if (ret == NS_OK && msgHdr)
			{
				nsMsgKey key;

				msgHdr->GetMessageKey(&key);
				msgHdr->GetAuthor(author);
				msgHdr->GetSubject(subject);
				char *authorStr = author.ToNewCString();
				char *subjectStr = subject.ToNewCString();
#ifdef DEBUG
				// leak nsString return values...
				printf("hdr key = %d, author = %s subject = %s\n", key, (authorStr) ? authorStr : "", (subjectStr) ? subjectStr : "");
#endif
				delete [] authorStr;
				delete [] subjectStr;
				msgHdr->Release();
			}
		}
		m_mailDB->Close(TRUE);
	}
#endif
	return NS_OK;

}

nsMsgMailboxParser::nsMsgMailboxParser() : nsMsgLineBuffer(NULL, PR_FALSE)
{
  /* the following macro is used to initialize the ref counting data */
	NS_INIT_REFCNT();

	m_mailDB = NULL;
	m_mailboxName = NULL;
	m_obuffer = NULL;
	m_obuffer_size = 0;
	m_ibuffer = NULL;
	m_ibuffer_size = 0;
	m_ibuffer_fp = 0;
	m_graph_progress_total = 0;
	m_graph_progress_received = 0;
	m_updateAsWeGo = PR_TRUE;
	m_ignoreNonMailFolder = PR_FALSE;
	m_isRealMailFolder = PR_TRUE;
}

nsMsgMailboxParser::~nsMsgMailboxParser()
{
	PR_FREEIF(m_mailboxName);
}

void nsMsgMailboxParser::UpdateStatusText ()
{
#ifdef WE_HAVE_PROGRESS
	char *leafName = PL_strrchr (m_mailboxName, '/');
	if (!leafName)
		leafName = m_mailboxName;
	else
		leafName++;
	NET_UnEscape(leafName);
	char *upgrading = XP_GetString (MK_MSG_REPARSE_FOLDER);
	int progressLength = nsCRT::strlen(upgrading) + nsCRT::strlen(leafName) + 1;
	char *progress = new char [progressLength];
	PR_snprintf (progress, progressLength, upgrading, leafName); 
	FE_Progress (m_context, progress);
	delete [] progress;
#endif
}

void nsMsgMailboxParser::UpdateProgressPercent ()
{
#ifdef WE_HAVE_PROGRESS
	XP_ASSERT(m_context != NULL);
	XP_ASSERT(m_graph_progress_total != 0);
	if ((m_context) && (m_graph_progress_total != 0))
	{
		MSG_SetPercentProgress(m_context, m_graph_progress_received, m_graph_progress_total);
	}
#endif
}

int nsMsgMailboxParser::ProcessMailboxInputStream(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	nsresult ret = NS_OK;

	PRUint32 bytesRead = 0;

	if (m_inputStream.GrowBuffer(aLength) == NS_OK)
	{
		// OK, this sucks, but we're going to have to copy into our
		// own byte buffer, and then pass that to the line buffering code,
		// which means a couple buffer copies.
		ret = aIStream->Read(m_inputStream.GetBuffer(), aLength, &bytesRead);
		if (ret == NS_OK)
			ret = BufferInput(m_inputStream.GetBuffer(), bytesRead);
	}
	if (m_graph_progress_total > 0)
	{
		if (ret == NS_OK)
		  m_graph_progress_received += bytesRead;
	}
	return (ret);
}


void nsMsgMailboxParser::DoneParsingFolder()
{
	/* End of file.  Flush out any partial line remaining in the buffer. */
	FlushLastLine();
	PublishMsgHeader();

	if (m_mailDB != NULL)	// finished parsing, so flush db folder info 
		UpdateDBFolderInfo();

//	if (m_folder != NULL)
//		m_folder->SummaryChanged();
	FreeBuffers();
}

void nsMsgMailboxParser::FreeBuffers()
{
	/* We're done reading the folder - we don't need these things
	 any more. */
	PR_FREEIF (m_ibuffer);
	m_ibuffer_size = 0;
	PR_FREEIF (m_obuffer);
	m_obuffer_size = 0;
}

void nsMsgMailboxParser::UpdateDBFolderInfo()
{
	UpdateDBFolderInfo(m_mailDB, m_mailboxName);
}

// update folder info in db so we know not to reparse.
void nsMsgMailboxParser::UpdateDBFolderInfo(nsMailDatabase *mailDB, const char *mailboxName)
{
	// ### wrong - use method on db.
	mailDB->SetSummaryValid(PR_TRUE);
	mailDB->Commit(kLargeCommit);
//	m_mailDB->Close();
}

// By default, do nothing
void nsMsgMailboxParser::FolderTypeSpecificTweakMsgHeader(nsIMessage * /* tweakMe */)
{
}

// Tell the world about the message header (add to db, and view, if any)
PRInt32 nsMsgMailboxParser::PublishMsgHeader()
{
	FinishHeader();
	if (m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_newMsgHdr);

		PRUint32 flags;
        (void)m_newMsgHdr->GetFlags(&flags);
		if (flags & MSG_FLAG_EXPUNGED)
		{
			nsIDBFolderInfo *folderInfo = nsnull;
			m_mailDB->GetDBFolderInfo(&folderInfo);
            PRUint32 size;
            (void)m_newMsgHdr->GetMessageSize(&size);
            folderInfo->ChangeExpungedBytes(size);
			if (m_newMsgHdr)
			{
				m_newMsgHdr->Release();;
				m_newMsgHdr = NULL;
			}
			NS_RELEASE(folderInfo);
		}
		else if (m_mailDB != NULL)
		{
			m_mailDB->AddNewHdrToDB(m_newMsgHdr, m_updateAsWeGo);
			m_newMsgHdr->Release();
			// should we release here?
			m_newMsgHdr = NULL;
		}
		else
			NS_ASSERTION(FALSE, "no database while parsing local folder");	// should have a DB, no?
	}
	else if (m_mailDB)
	{
		nsIDBFolderInfo *folderInfo = nsnull;
		m_mailDB->GetDBFolderInfo(&folderInfo);
		if (folderInfo)
			folderInfo->ChangeExpungedBytes(m_position - m_envelope_pos);
	}
	return 0;
}

void nsMsgMailboxParser::AbortNewHeader()
{
	if (m_newMsgHdr && m_mailDB)
	{
		m_newMsgHdr->Release();
		m_newMsgHdr = NULL;
	}
}

PRInt32 nsMsgMailboxParser::HandleLine(char *line, PRUint32 lineLength)
{
	int status = 0;

	/* If this is the very first line of a non-empty folder, make sure it's an envelope */
	if (m_graph_progress_received == 0)
	{
		/* This is the first block from the file.  Check to see if this
		   looks like a mail file. */
		const char *s = line;
		const char *end = s + lineLength;
		while (s < end && XP_IS_SPACE(*s))
			s++;
		if ((end - s) < 20 || !IsEnvelopeLine(s, end - s))
		{
//			char buf[500];
//			PR_snprintf (buf, sizeof(buf),
//						 XP_GetString(MK_MSG_NON_MAIL_FILE_READ_QUESTION),
//						 folder_name);
			m_isRealMailFolder = FALSE;
			if (m_ignoreNonMailFolder)
				return 0;
//			else if (!FE_Confirm (m_context, buf))
//				return NS_MSG_NOT_A_MAIL_FOLDER; /* #### NOT_A_MAIL_FILE */
		}
	}
	m_graph_progress_received += lineLength;

	// mailbox parser needs to do special stuff when it finds an envelope
	// after parsing a message body. So do that.
	if (line[0] == 'F' && IsEnvelopeLine(line, lineLength))
	{
		// **** This used to be
		// XP_ASSERT (m_parseMsgState->m_state == MBOX_PARSE_BODY);
		// **** I am not sure this is a right thing to do. This happens when
		// going online, downloading a message while playing back append
		// draft/template offline operation. We are mixing MBOX_PARSE_BODY &&
		// MBOX_PARSE_HEADERS state. David I need your help here too. **** jt

		NS_ASSERTION (m_state == MBOX_PARSE_BODY ||
				   m_state == MBOX_PARSE_HEADERS, "invalid parse state"); /* else folder corrupted */
		PublishMsgHeader();
		Clear();
		status = StartNewEnvelope(line, lineLength);
		NS_ASSERTION(status >= 0, " error starting envelope parsing mailbox");
		if (status < 0)
			return status;
	}
	// otherwise, the message parser can handle it completely.
	else if (m_mailDB != NULL)	// if no DB, do we need to parse at all?
		return ParseFolderLine(line, lineLength);

	return 0;

}

nsParseMailMessageState::nsParseMailMessageState()
{
	m_mailDB = NULL;
	m_position = 0;
	m_IgnoreXMozillaStatus = FALSE;
	m_state = MBOX_PARSE_BODY;
	Clear();

    NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID);
    
    nsComponentManager::CreateInstance(kMsgHeaderParserCID,
                                       nsnull,
                                       nsIMsgHeaderParser::GetIID(),
                                       (void **)&m_HeaderAddressParser);
}

nsParseMailMessageState::~nsParseMailMessageState()
{
	ClearAggregateHeader (m_toList);
	ClearAggregateHeader (m_ccList);
	
	NS_RELEASE(m_HeaderAddressParser);
}

void nsParseMailMessageState::Init(PRUint32 fileposition)
{
	m_state = MBOX_PARSE_BODY;
	m_position = fileposition;
	m_newMsgHdr = NULL;
}

void nsParseMailMessageState::Clear()
{
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
	m_headers.ResetWritePos();
	m_envelope.ResetWritePos();
}

PRInt32 nsParseMailMessageState::ParseFolderLine(const char *line, PRUint32 lineLength)
{
	int status = 0;

	if (m_state == MBOX_PARSE_HEADERS)
	{
		if (EMPTY_MESSAGE_LINE(line))
		{
		  /* End of headers.  Now parse them. */
			status = ParseHeaders();
			 NS_ASSERTION(status >= 0, "error parsing headers parsing mailbox");
			if (status < 0)
				return status;

			 status = FinalizeHeaders();
			 NS_ASSERTION(status >= 0, "error finalizing headers parsing mailbox");
			if (status < 0)
				return status;
			 m_state = MBOX_PARSE_BODY;
		}
		else
		{
		  /* Otherwise, this line belongs to a header.  So append it to the
			 header data, and stay in MBOX `MIME_PARSE_HEADERS' state.
		   */
			m_headers.AppendBuffer(line, lineLength);
		}
	}
	else if ( m_state == MBOX_PARSE_BODY)
	{
		m_body_lines++;
	}

	m_position += lineLength;

	return 0;
}

void nsParseMailMessageState::SetMailDB(nsMailDatabase *mailDB)
{
	m_mailDB = mailDB;
}

/* #define STRICT_ENVELOPE */

PRBool
nsParseMailMessageState::IsEnvelopeLine(const char *buf, PRInt32 buf_size)
{
#ifdef STRICT_ENVELOPE
  /* The required format is
	   From jwz  Fri Jul  1 09:13:09 1994
	 But we should also allow at least:
	   From jwz  Fri, Jul 01 09:13:09 1994
	   From jwz  Fri Jul  1 09:13:09 1994 PST
	   From jwz  Fri Jul  1 09:13:09 1994 (+0700)

	 We can't easily call XP_ParseTimeString() because the string is not
	 null terminated (ok, we could copy it after a quick check...) but
	 XP_ParseTimeString() may be too lenient for our purposes.

	 DANGER!!  The released version of 2.0b1 was (on some systems,
	 some Unix, some NT, possibly others) writing out envelope lines
	 like "From - 10/13/95 11:22:33" which STRICT_ENVELOPE will reject!
   */
  const char *date, *end;

  if (buf_size < 29) return PR_FALSE;
  if (*buf != 'F') return PR_FALSE;
  if (nsCRT::strncmp(buf, "From ", 5)) return PR_FALSE;

  end = buf + buf_size;
  date = buf + 5;

  /* Skip horizontal whitespace between "From " and user name. */
  while ((*date == ' ' || *date == '\t') && date < end)
	date++;

  /* If at the end, it doesn't match. */
  if (XP_IS_SPACE(*date) || date == end)
	return PR_FALSE;

  /* Skip over user name. */
  while (!XP_IS_SPACE(*date) && date < end)
	date++;

  /* Skip horizontal whitespace between user name and date. */
  while ((*date == ' ' || *date == '\t') && date < end)
	date++;

  /* Don't want this to be localized. */
# define TMP_ISALPHA(x) (((x) >= 'A' && (x) <= 'Z') || \
						 ((x) >= 'a' && (x) <= 'z'))

  /* take off day-of-the-week. */
  if (date >= end - 3)
	return PR_FALSE;
  if (!TMP_ISALPHA(date[0]) || !TMP_ISALPHA(date[1]) || !TMP_ISALPHA(date[2]))
	return PR_FALSE;
  date += 3;
  /* Skip horizontal whitespace (and commas) between dotw and month. */
  if (*date != ' ' && *date != '\t' && *date != ',')
	return PR_FALSE;
  while ((*date == ' ' || *date == '\t' || *date == ',') && date < end)
	date++;

  /* take off month. */
  if (date >= end - 3)
	return PR_FALSE;
  if (!TMP_ISALPHA(date[0]) || !TMP_ISALPHA(date[1]) || !TMP_ISALPHA(date[2]))
	return PR_FALSE;
  date += 3;
  /* Skip horizontal whitespace between month and dotm. */
  if (date == end || (*date != ' ' && *date != '\t'))
	return PR_FALSE;
  while ((*date == ' ' || *date == '\t') && date < end)
	date++;

  /* Skip over digits and whitespace. */
  while (((*date >= '0' && *date <= '9') || *date == ' ' || *date == '\t') &&
		 date < end)
	date++;
  /* Next character should be a colon. */
  if (date >= end || *date != ':')
	return PR_FALSE;

  /* Ok, that ought to be enough... */

# undef TMP_ISALPHA

#else  /* !STRICT_ENVELOPE */

  if (buf_size < 5) return PR_FALSE;
  if (*buf != 'F') return PR_FALSE;
  if (nsCRT::strncmp(buf, "From ", 5)) return PR_FALSE;

#endif /* !STRICT_ENVELOPE */

  return PR_TRUE;
}


// We've found the start of the next message, so finish this one off.
void nsParseMailMessageState::FinishHeader()
{

	if (m_newMsgHdr)
	{
		  m_newMsgHdr->SetMessageKey(m_envelope_pos);
		  m_newMsgHdr->SetMessageSize(m_position - m_envelope_pos);	// dmb - no longer number of lines.
		  m_newMsgHdr->SetLineCount(m_body_lines);
	}
}

struct message_header *nsParseMailMessageState::GetNextHeaderInAggregate (nsVoidArray &list)
{
	// When parsing a message with multiple To or CC header lines, we're storing each line in a 
	// list, where the list represents the "aggregate" total of all the header. Here we get a new
	// line for the list

	struct message_header *header = (struct message_header*) PR_Calloc (1, sizeof(struct message_header));
	list.AppendElement (header);
	return header;
}

void nsParseMailMessageState::GetAggregateHeader (nsVoidArray &list, struct message_header *outHeader)
{
	// When parsing a message with multiple To or CC header lines, we're storing each line in a 
	// list, where the list represents the "aggregate" total of all the header. Here we combine
	// all the lines together, as though they were really all found on the same line

	struct message_header *header = NULL;
	int length = 0;
	int i;

	// Count up the bytes required to allocate the aggregated header
	for (i = 0; i < list.Count(); i++)
	{
		header = (struct message_header*) list.ElementAt(i);
		length += (header->length + 1); //+ for ","
		NS_ASSERTION(header->length == (PRInt32)nsCRT::strlen(header->value), "header corrupted");
	}

	if (length > 0)
	{
		char *value = (char*) PR_MALLOC (length + 1); //+1 for null term
		if (value)
		{
			// Catenate all the To lines together, separated by commas
			value[0] = '\0';
			int size = list.Count();
			for (i = 0; i < size; i++)
			{
				header = (struct message_header*) list.ElementAt(i);
				PL_strcat (value, header->value);
				if (i + 1 < size)
					PL_strcat (value, ",");
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

void nsParseMailMessageState::ClearAggregateHeader (nsVoidArray &list)
{
	// Reset the aggregate headers. Free only the message_header struct since 
	// we don't own the value pointer

	for (int i = 0; i < list.Count(); i++)
		PR_Free ((struct message_header*) list.ElementAt(i));
	list.Clear();
}

// We've found a new envelope to parse.
int nsParseMailMessageState::StartNewEnvelope(const char *line, PRUint32 lineLength)
{
	m_envelope_pos = m_position;
	m_state = MBOX_PARSE_HEADERS;
	m_position += lineLength;
	m_headerstartpos = m_position;
	return ParseEnvelope (line, lineLength);
}

/* largely lifted from mimehtml.c, which does similar parsing, sigh...
 */
int nsParseMailMessageState::ParseHeaders ()
{
  char *buf = m_headers.GetBuffer();
  char *buf_end = buf + m_headers.GetBufferPos();
  while (buf < buf_end)
	{
	  char *colon = PL_strchr (buf, ':');
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
		  if (!nsCRT::strncasecmp ("CC", buf, end - buf))
			header = GetNextHeaderInAggregate(m_ccList);
		  break;
		case 'D': case 'd':
		  if (!nsCRT::strncasecmp ("Date", buf, end - buf))
			header = &m_date;
		  else if (!nsCRT::strncasecmp("Disposition-Notification-To", buf, end - buf))
			header = &m_mdn_dnt;
		  break;
		case 'F': case 'f':
		  if (!nsCRT::strncasecmp ("From", buf, end - buf))
			header = &m_from;
		  break;
		case 'M': case 'm':
		  if (!nsCRT::strncasecmp ("Message-ID", buf, end - buf))
			header = &m_message_id;
		  break;
		case 'N': case 'n':
		  if (!nsCRT::strncasecmp ("Newsgroups", buf, end - buf))
			header = &m_newsgroups;
		  break;
		case 'O': case 'o':
			if (!nsCRT::strncasecmp ("Original-Recipient", buf, end - buf))
				header = &m_mdn_original_recipient;
			break;
		case 'R': case 'r':
		  if (!nsCRT::strncasecmp ("References", buf, end - buf))
			header = &m_references;
		  else if (!nsCRT::strncasecmp ("Return-Path", buf, end - buf))
			  header = &m_return_path;
		   // treat conventional Return-Receipt-To as MDN
		   // Disposition-Notification-To
		  else if (!nsCRT::strncasecmp ("Return-Receipt-To", buf, end - buf))
			  header = &m_mdn_dnt;
		  break;
		case 'S': case 's':
		  if (!nsCRT::strncasecmp ("Subject", buf, end - buf))
			header = &m_subject;
		  else if (!nsCRT::strncasecmp ("Sender", buf, end - buf))
			header = &m_sender;
		  else if (!nsCRT::strncasecmp ("Status", buf, end - buf))
			header = &m_status;
		  break;
		case 'T': case 't':
		  if (!nsCRT::strncasecmp ("To", buf, end - buf))
			header = GetNextHeaderInAggregate(m_toList);
		  break;
		case 'X':
		  if (X_MOZILLA_STATUS2_LEN == end - buf &&
			  !nsCRT::strncasecmp(X_MOZILLA_STATUS2, buf, end - buf) &&
			  !m_IgnoreXMozillaStatus)
			  header = &m_mozstatus2;
		  else if ( X_MOZILLA_STATUS_LEN == end - buf &&
			  !nsCRT::strncasecmp(X_MOZILLA_STATUS, buf, end - buf) && !m_IgnoreXMozillaStatus)
			header = &m_mozstatus;
		  // we could very well care what the priority header was when we 
		  // remember its value. If so, need to remember it here. Also, 
		  // different priority headers can appear in the same message, 
		  // but we only rememeber the last one that we see.
		  else if (!nsCRT::strncasecmp("X-Priority", buf, end - buf)
			  || !nsCRT::strncasecmp("Priority", buf, end - buf))
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

int nsParseMailMessageState::ParseEnvelope (const char *line, PRUint32 line_size)
{
	const char *end;
	char *s;

	m_envelope.AppendBuffer(line, line_size);
	end = m_envelope.GetBuffer() + line_size;
	s = m_envelope.GetBuffer() + 5;

	while (s < end && XP_IS_SPACE (*s))
		s++;
	m_envelope_from.value = s;
	while (s < end && !XP_IS_SPACE (*s))
		s++;
	m_envelope_from.length = s - m_envelope_from.value;

	while (s < end && XP_IS_SPACE (*s))
		s++;
	m_envelope_date.value = s;
	m_envelope_date.length = (PRUint16) (line_size - (s - m_envelope.GetBuffer()));
	while (XP_IS_SPACE (m_envelope_date.value [m_envelope_date.length - 1]))
		m_envelope_date.length--;

	/* #### short-circuit const */
	((char *) m_envelope_from.value) [m_envelope_from.length] = 0;
	((char *) m_envelope_date.value) [m_envelope_date.length] = 0;

	return 0;
}

#ifdef WE_CONDENSE_MIME_STRINGS

extern "C" 
{
	int16 INTL_DefaultMailToWinCharSetID(int16 csid);
	char *INTL_EncodeMimePartIIStr_VarLen(char *subject, int16 wincsid, PRBool bUseMime,
											int encodedWordLen);
}

static char *
msg_condense_mime2_string(char *sourceStr)
{
	int16 string_csid = CS_DEFAULT;
	int16 win_csid = CS_DEFAULT;

	char *returnVal = nsCRT::strdup(sourceStr);
	if (!returnVal) 
		return NULL;
	
	// If sourceStr has a MIME-2 encoded word in it, get the charset
	// name/ID from the first encoded word. (No, we're not multilingual.)
	char *p = PL_strstr(returnVal, "=?");
	if (p)
	{
		p += 2;
		char *q = PL_strchr(p, '?');
		if (q) *q = '\0';
		string_csid = INTL_CharSetNameToID(p);
		win_csid = INTL_DocToWinCharSetID(string_csid);
		if (q) *q = '?';

		// Decode any MIME-2 encoded strings, to save the overhead.
		char *cvt = (CS_UTF8 != win_csid) ? INTL_DecodeMimePartIIStr(returnVal, win_csid, FALSE) : NULL;
		if (cvt)
		{
			if (cvt != returnVal)
			{
				PR_FREEIF(returnVal);
				returnVal = cvt;
			}
			// MIME-2 decoding occurred, so re-encode into large encoded words
			cvt = INTL_EncodeMimePartIIStr_VarLen(returnVal, win_csid, TRUE,
												MSG_MAXSUBJECTLENGTH - 2);
			if (cvt && (cvt != returnVal))
			{
				XP_FREE(returnVal);		// encoding happened, deallocate decoded text
				returnVal = MIME_StripContinuations(cvt); // and remove CR+LF+spaces that occur
			}
			// else returnVal == cvt, in which case nothing needs to be done
		}
		else
			// no MIME-2 decoding occurred, so strip CR+LF+spaces ourselves
			MIME_StripContinuations(returnVal);
	}
	else if (returnVal)
		MIME_StripContinuations(returnVal);
	
	return returnVal;
}
#endif // WE_CONDENSE_MIME_STRINGS

int nsParseMailMessageState::InternSubject (struct message_header *header)
{
	char *key;
	PRUint32 L;

	if (!header || header->length == 0)
	{
		m_newMsgHdr->SetSubject("");
		return 0;
	}

	NS_ASSERTION (header->length == (short) nsCRT::strlen(header->value), "subject corrupt while parsing message");

	key = (char *) header->value;  /* #### const evilness */

	L = header->length;


	/* strip "Re: " */
	if (msg_StripRE((const char **) &key, &L))
	{
        PRUint32 flags;
        (void)m_newMsgHdr->GetFlags(&flags);
		m_newMsgHdr->SetFlags(flags | MSG_FLAG_HAS_RE);
	}

//  if (!*key) return 0; /* To catch a subject of "Re:" */

	// Condense the subject text into as few MIME-2 encoded words as possible.
#ifdef WE_CONDENSE_MIME_STRINGS
	char *condensedKey = msg_condense_mime2_string(key);
#else
	char *condensedKey = NULL;
#endif
	m_newMsgHdr->SetSubject(condensedKey ? condensedKey : key);
	PR_FREEIF(condensedKey);

	return 0;
}

/* Like mbox_intern() but for headers which contain email addresses:
   we extract the "name" component of the first address, and discard
   the rest. */
nsresult nsParseMailMessageState::InternRfc822 (struct message_header *header, 
									 char **ret_name)
{
	char	*s;
	nsresult ret;

	if (!header || header->length == 0)
		return NS_OK;

	NS_ASSERTION (header->length == (short) nsCRT::strlen (header->value), "invalid message_header");
	NS_ASSERTION (ret_name != NULL, "null ret_name");

	if (m_HeaderAddressParser)
	{
		ret = m_HeaderAddressParser->ExtractHeaderAddressName (nsnull, header->value, &s);
		if (! s)
			return NS_ERROR_OUT_OF_MEMORY;

		*ret_name = s;
	}
	return ret;
}

// we've reached the end of the envelope, and need to turn all our accumulated message_headers
// into a single nsIMessage to store in a database.
int nsParseMailMessageState::FinalizeHeaders()
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
	struct message_header md5_header;
	unsigned char md5_bin [16];
	char md5_data [50];

	const char *s;
	PRUint32 flags = 0;
	PRUint32 delta = 0;
	nsMsgPriority priorityFlags = nsMsgPriority_NotSet;

	if (!m_mailDB)		// if we don't have a valid db, skip the header.
		return 0;

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
			int i;
			for (i=0,s=mozstatus->value ; i<4 ; i++,s++) 
			{
				flags = (flags << 4) | msg_UnHex(*s);
			}
			// strip off and remember priority bits.
			flags &= ~MSG_FLAG_RUNTIME_ONLY;
			priorityFlags = (nsMsgPriority) ((flags & MSG_FLAG_PRIORITIES) >> 13);
			flags &= ~MSG_FLAG_PRIORITIES;
		  /* We trust the X-Mozilla-Status line to be the smartest in almost
			 all things.  One exception, however, is the HAS_RE flag.  Since
			 we just parsed the subject header anyway, we expect that parsing
			 to be smartest.  (After all, what if someone just went in and
			 edited the subject line by hand?) */
		}
		delta = (m_headerstartpos +
			 (mozstatus->value - m_headers.GetBuffer()) -
			 (2 + X_MOZILLA_STATUS_LEN)		/* 2 extra bytes for ": ". */
			 ) - m_envelope_pos;
	}

	if (mozstatus2)
	{
		PRUint32 flags2 = 0;
		sscanf(mozstatus2->value, " %x ", &flags2);
		flags |= flags2;
	}

	if (!(flags & MSG_FLAG_EXPUNGED))	// message was deleted, don't bother creating a hdr.
	{
		nsresult ret = m_mailDB->CreateNewHdr(m_envelope_pos, &m_newMsgHdr);
		if (ret == NS_OK && m_newMsgHdr)
		{
            PRUint32 origFlags;
            (void)m_newMsgHdr->GetFlags(&origFlags);
			if (origFlags & MSG_FLAG_HAS_RE)
				flags |= MSG_FLAG_HAS_RE;
			else
				flags &= ~MSG_FLAG_HAS_RE;

			if (mdn_dnt && !(origFlags & MSG_FLAG_READ) &&
				!(origFlags & MSG_FLAG_MDN_REPORT_SENT))
				flags |= MSG_FLAG_MDN_REPORT_NEEDED;

			m_newMsgHdr->SetFlags(flags);
			if (priorityFlags != nsMsgPriority_NotSet)
				m_newMsgHdr->SetPriority(priorityFlags);

			if (delta < 0xffff) 
			{		/* Only use if fits in 16 bits. */
				m_newMsgHdr->SetStatusOffset((PRUint16) delta);
				if (!m_IgnoreXMozillaStatus) {	// imap doesn't care about X-MozillaStatus
                    PRUint32 offset;
                    (void)m_newMsgHdr->GetStatusOffset(&offset);
					NS_ASSERTION(offset < 10000, "invalid status offset"); /* ### Debugging hack */
                }
			}
			m_newMsgHdr->SetAuthor(sender->value);
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
				NS_ASSERTION (recipient->length == (PRUint16) nsCRT::strlen(recipient->value), "invalid recipient");
				s = PL_strchr(recipient->value, ',');
				if (s)
				{
					*s = 0;
					recipient->length = nsCRT::strlen(recipient->value);
				}
				m_newMsgHdr->SetRecipients(recipient->value, FALSE);
			}
			else
			{
				// note that we're now setting the whole recipient list,
				// not just the pretty name of the first recipient.
				PRUint32 numAddresses;
				char	*names;
				char	*addresses;

				ret = m_HeaderAddressParser->ParseHeaderAddresses (nsnull, recipient->value, &names, &addresses, numAddresses);
				if (ret == NS_OK)
				{
					m_newMsgHdr->SetRecipientsArray(names, addresses, numAddresses);
					PR_FREEIF(addresses);
					PR_FREEIF(names);
				}
				else	// hmm, should we just use the original string?
					m_newMsgHdr->SetRecipients(recipient->value, TRUE);
			}
			if (ccList)
			{
				PRUint32 numAddresses;
				char	*names;
				char	*addresses;

				ret = m_HeaderAddressParser->ParseHeaderAddresses (nsnull, ccList->value, &names, &addresses, numAddresses);
				if (ret == NS_OK)
				{
					m_newMsgHdr->SetCCListArray(names, addresses, numAddresses);
					PR_FREEIF(addresses);
					PR_FREEIF(names);
				}
				else	// hmm, should we just use the original string?
					m_newMsgHdr->SetCCList(ccList->value);
			}
			status = InternSubject (subject);
			if (status >= 0)
			{
				if (! id)
				{
					// what to do about this? we used to do a hash of all the headers...
#ifdef SIMPLE_MD5
					HASH_HashBuf(HASH_AlgMD5, md5_bin, (unsigned char *)m_headers,
						 (int) m_headers_fp);
#else
					// ### TODO: is it worth doing something different?
					nsCRT::memcpy(md5_bin, "dummy message id", sizeof(md5_bin));						
#endif
					PR_snprintf (md5_data, sizeof(md5_data),
							   "<md5:"
							   "%02X%02X%02X%02X%02X%02X%02X%02X"
							   "%02X%02X%02X%02X%02X%02X%02X%02X"
							   ">",
							   md5_bin[0], md5_bin[1], md5_bin[2], md5_bin[3],
							   md5_bin[4], md5_bin[5], md5_bin[6], md5_bin[7],
							   md5_bin[8], md5_bin[9], md5_bin[10],md5_bin[11],
							   md5_bin[12],md5_bin[13],md5_bin[14],md5_bin[15]);
					md5_header.value = md5_data;
					md5_header.length = nsCRT::strlen(md5_data);
					id = &md5_header;
				}

			  /* Take off <> around message ID. */
				if (id->value[0] == '<')
					id->value++, id->length--;
				if (id->value[id->length-1] == '>')
					((char *) id->value) [id->length-1] = 0, id->length--; /* #### const */

				m_newMsgHdr->SetMessageId(id->value);

				if (!mozstatus && statush)
				{
				  /* Parse a little bit of the Berkeley Mail status header. */
                    for (s = statush->value; *s; s++) {
                        PRUint32 flags;
                        (void)m_newMsgHdr->GetFlags(&flags);
                        switch (*s)
                        {
                          case 'R': case 'r':
                            m_newMsgHdr->SetFlags(flags | MSG_FLAG_READ);
                            break;
                          case 'D': case 'd':
                            /* msg->flags |= MSG_FLAG_EXPUNGED;  ### Is this reasonable? */
                            break;
                          case 'N': case 'n':
                          case 'U': case 'u':
                            m_newMsgHdr->SetFlags(flags & ~MSG_FLAG_READ);
                            break;
                        }
                    }
				}

				if (references != NULL)
					m_newMsgHdr->SetReferences(references->value);
				if (date) {
					time_t	resDate = 0;
					PRTime resultTime, intermediateResult, microSecondsPerSecond;
					PRStatus status = PR_ParseTimeString (date->value, PR_FALSE, &resultTime);

					LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
					LL_DIV(intermediateResult, resultTime, microSecondsPerSecond);
					LL_L2I(resDate, intermediateResult);
					if (resDate < 0) 
						resDate = 0;

					// no reason to store milliseconds, since they aren't specified
					if (PR_SUCCESS == status)
						m_newMsgHdr->SetDate(resDate);
				}
				if (priority)
					m_newMsgHdr->SetPriority(priority->value);
				else if (priorityFlags == nsMsgPriority_NotSet)
					m_newMsgHdr->SetPriority(nsMsgPriority_None);
			}
		} 
		else
		{
			NS_ASSERTION(FALSE, "error creating message header");
			status = NS_ERROR_OUT_OF_MEMORY;	
		}
	}
	else
		status = 0;

	//### why is this stuff const?
	char *tmp = (char*) to.value;
	PR_FREEIF(tmp);
	tmp = (char*) cc.value;
	PR_Free(tmp);

	return status;
}


/* Given a string and a length, removes any "Re:" strings from the front.
   It also deals with that dumbass "Re[2]:" thing that some losing mailers do.

   Returns TRUE if it made a change, FALSE otherwise.

   The string is not altered: the pointer to its head is merely advanced,
   and the length correspondingly decreased.
 */
/* static */PRBool nsParseMailMessageState::msg_StripRE(const char **stringP, PRUint32 *lengthP)
{
  const char *s, *s_end;
  const char *last;
  uint32 L;
  PRBool result = PR_FALSE;
  NS_ASSERTION(stringP, "bad null param");
  if (!stringP) return PR_FALSE;
  s = *stringP;
  L = lengthP ? *lengthP : nsCRT::strlen(s);

  s_end = s + L;
  last = s;

 AGAIN:

  while (s < s_end && XP_IS_SPACE(*s))
	s++;

  if (s < (s_end-2) &&
	  (s[0] == 'r' || s[0] == 'R') &&
	  (s[1] == 'e' || s[1] == 'E'))
	{
	  if (s[2] == ':')
		{
		  s = s+3;			/* Skip over "Re:" */
		  result = PR_TRUE;	/* Yes, we stripped it. */
		  goto AGAIN;		/* Skip whitespace and try again. */
		}
	  else if (s[2] == '[' || s[2] == '(')
		{
		  const char *s2 = s+3;		/* Skip over "Re[" */

		  /* Skip forward over digits after the "[". */
		  while (s2 < (s_end-2) && XP_IS_DIGIT(*s2))
			s2++;

		  /* Now ensure that the following thing is "]:"
			 Only if it is do we alter `s'.
		   */
		  if ((s2[0] == ']' || s2[0] == ')') && s2[1] == ':')
			{
			  s = s2+2;			/* Skip over "]:" */
			  result = PR_TRUE;	/* Yes, we stripped it. */
			  goto AGAIN;		/* Skip whitespace and try again. */
			}
		}
	}

  /* Decrease length by difference between current ptr and original ptr.
	 Then store the current ptr back into the caller. */
  if (lengthP) 
	  *lengthP -= (s - (*stringP));
  *stringP = s;

  return result;
}




nsParseNewMailState::nsParseNewMailState()
    : m_tmpdbName(nsnull), m_usingTempDB(PR_FALSE), m_disableFilters(PR_FALSE)
#ifdef DOING_FILTERS
    , m_filterList(nsnull), m_logFile(nsnull)
#endif
{
}

nsresult
nsParseNewMailState::Init(MSG_Master *master, nsFileSpec &folder)
{
    nsresult rv;
//	SetMaster(master);
	m_mailboxName = nsCRT::strdup(folder);

	// the new mail parser isn't going to get the stream input, it seems, so we can't use
	// the OnStartBinding mechanism the mailbox parser uses. So, let's open the db right now.
	nsIMsgDatabase * mailDB = nsnull;
	rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), (void **) &mailDB);
	if (NS_SUCCEEDED(rv) && mailDB)
	{
		rv = mailDB->Open(folder, PR_TRUE, (nsIMsgDatabase **) &m_mailDB, PR_FALSE);
		mailDB->Release();
	}
//	rv = nsMailDatabase::Open(folder, PR_TRUE, &m_mailDB, PR_FALSE);
    if (NS_FAILED(rv)) return rv;

#ifdef DOING_FILTERS
	if (MSG_FilterList::Open(master, filterInbox, NULL, folder, &m_filterList)
		!= FilterError_Success)
		m_filterList = NULL;

	m_logFile = NULL;
#endif
#ifdef DOING_MDN
	if (m_filterList)
	{
		const char *folderName = NULL;
		PRInt32 int_pref = 0;
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
#endif // DOING_MDN
	m_usingTempDB = FALSE;
	m_tmpdbName = NULL;
	m_disableFilters = FALSE;

    return NS_OK; 
}

nsParseNewMailState::~nsParseNewMailState()
{
#ifdef DOING_FILTERS
	if (m_filterList != NULL)
		MSG_CancelFilterList(m_filterList);
	if (m_logFile != NULL)
		XP_FileClose(m_logFile);
#endif
	if (m_mailDB)
		m_mailDB->Close(PR_TRUE);
//	if (m_usingTempDB)
//	{
//		XP_FileRemove(m_tmpdbName, xpMailFolderSummary);
//	}
	PR_FREEIF(m_tmpdbName);
#ifdef DOING_FILTERS
	JSFilter_cleanup();
#endif
}


// This gets called for every message because libnet calls IncorporateBegin,
// IncorporateWrite (once or more), and IncorporateComplete for every message.
void nsParseNewMailState::DoneParsingFolder()
{
	PRBool moved = FALSE;
/* End of file.  Flush out any partial line remaining in the buffer. */
	if (m_ibuffer_fp > 0) 
	{
		ParseFolderLine(m_ibuffer, m_ibuffer_fp);
		m_ibuffer_fp = 0;
	}
	PublishMsgHeader();
	if (!moved && m_mailDB != NULL)	// finished parsing, so flush db folder info 
		UpdateDBFolderInfo();

#ifdef HAVE_FOLDERINFO
	if (m_folder != NULL)
		m_folder->SummaryChanged();
#endif

	/* We're done reading the folder - we don't need these things
	 any more. */
	PR_FREEIF (m_ibuffer);
	m_ibuffer_size = 0;
	PR_FREEIF (m_obuffer);
	m_obuffer_size = 0;
}

PRInt32 nsParseNewMailState::PublishMsgHeader()
{
	PRBool		moved = FALSE;

	FinishHeader();
	
	if (m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_newMsgHdr);
#ifdef DOING_FILTERS
		if (!m_disableFilters) {
			ApplyFilters(&moved);
		}
#endif // DOING_FILTERS
		if (!moved)
		{
			if (m_mailDB)
			{
				PRUint32 newFlags;
				m_newMsgHdr->OrFlags(MSG_FLAG_NEW, &newFlags);

				m_mailDB->AddNewHdrToDB (m_newMsgHdr, m_updateAsWeGo);
			}
#ifdef HAVE_FOLDERINFO
			if (m_folder)
				m_folder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);
#endif

		}		// if it was moved by imap filter, m_parseMsgState->m_newMsgHdr == NULL
		else if (m_newMsgHdr)
		{
			m_newMsgHdr->Release();
		}
		m_newMsgHdr = NULL;
	}
	return 0;
}

void	nsParseNewMailState::SetUsingTempDB(PRBool usingTempDB, char *tmpDBName)
{
	m_usingTempDB = usingTempDB;
	m_tmpdbName = tmpDBName;
}

#ifdef DOING_FILTERS

XP_File nsParseNewMailState::GetLogFile ()
{
	// This log file is used by regular filters and JS filters
	if (m_logFile == NULL)
		m_logFile = XP_FileOpen("", xpMailFilterLog, XP_FILE_APPEND);	// will this create?
	return m_logFile;
}

void nsParseNewMailState::LogRuleHit(MSG_Filter *filter, nsIMessage *msgHdr)
{
	char	*filterName = "";
	time_t	date;
	char	dateStr[40];	/* 30 probably not enough */
	MSG_RuleActionType actionType;
	MSG_Rule	*rule;
	void				*value;
	NsString	author;
	NsString	subject;

	if (m_logFile == NULL)
		m_logFile = GetLogFile();
	
	filter->GetName(&filterName);
	if (filter->GetRule(&rule) != FilterError_Success)
		return;
	rule->GetAction(&actionType, &value);
	date = msgHdr->GetDate();
	XP_StrfTime(m_context, dateStr, sizeof(dateStr), XP_DATE_TIME_FORMAT,
				localtime(&date));
	msgHdr->GetAuthor(author);
	msgHdr->GetSubject(subject, TRUE);
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
			NsString msgId;
			msgHdr->GetMessageId(msgId);
			XP_FilePrintf(m_logFile, "mailbox:%s?id=%s\n", value, (const char *) msgId);
		}
	}
}

MSG_FolderInfoMail *nsParseNewMailState::GetTrashFolder()
{
	MSG_FolderInfo *foundTrash = NULL;
	GetMaster()->GetLocalMailFolderTree()->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, &foundTrash, 1);
	return foundTrash ? foundTrash->GetMailFolderInfo() : (MSG_FolderInfoMail *)NULL;
}

void nsParseNewMailState::ApplyFilters(PRBool *pMoved)
{
	MSG_Filter	*filter;
	PRInt32		filterCount = 0;
	PRBool		msgMoved = FALSE;
	MsgERR		err = eSUCCESS;

	nsIMessage	*msgHdr = GetCurrentMsg();
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
						PRUint32 headersSize = GetMsgState()->m_headers_fp;

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
							PRBool isRead = msgHdr->GetFlags() & kIsRead;
							switch (actionType)
							{
							case acDelete:
							{
								MSG_IMAPFolderInfoMail *imapFolder = m_folder->GetIMAPFolderInfoMail();
								PRBool serverIsImap  = GetMaster()->GetPrefs()->GetMailServerIsIMAP4();
								PRBool deleteToTrash = !imapFolder || imapFolder->DeleteIsMoveToTrash();
								PRBool showDeletedMessages = (!imapFolder) || imapFolder->ShowDeletedMessages();
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
									nsMsgKeyArray	keysToFlag;

									keysToFlag.Add(msgHdr->GetMessageKey());
									if (imapFolder)
										imapFolder->StoreImapFlags(m_pane, kImapMsgSeenFlag | kImapMsgDeletedFlag, TRUE, keysToFlag, ((ParseIMAPMailboxState *) this)->GetFilterUrlQueue());
									if (!showDeletedMessages)
										msgMoved = TRUE;	// this will prevent us from adding the header to the db.

								}
							}
							case acMoveToFolder:
								// if moving to a different file, do it.
								if (value && XP_FILENAMECMP(m_mailDB->GetFolderName(), (char *) value))
								{
									if (msgHdr->GetFlags() & kMDNNeeded &&
										!isRead)
									{
										nsParseMailMessageState *state =
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
												 (PRInt32) state->m_headers_fp, TRUE);
										}
#if 0	// leave it to the user aciton
										else
										{
											MSG_ProcessMdnNeededState processMdnNeeded
												(MSG_ProcessMdnNeededState::eProcessed,
												 m_pane, m_folder, msgHdr->GetMessageKey(),
												 &state->m_return_path, &state->m_mdn_dnt, 
												 &to, &cc, &state->m_subject, 
												 &state->m_date, &state->m_mdn_original_recipient,
												 &state->m_message_id, state->m_headers, 
												 (PRInt32) state->m_headers_fp, TRUE);
										}
#endif
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
								m_mailDB->SetPriority(msgHdr,  * ((nsMsgPriority *) &value));
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

int nsParseNewMailState::MarkFilteredMessageRead(nsIMessage *msgHdr)
{
	if (m_mailDB)
		m_mailDB->MarkHdrRead(msgHdr, TRUE, NULL);
	else
		msgHdr->OrFlags(kIsRead);
	return 0;
}

int nsParseNewMailState::MoveIncorporatedMessage(nsIMessage *mailHdr, 
											   nsMailDatabase *sourceDB, 
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

	nsMailDatabase *mailDb = NULL;
	// don't force upgrade in place - open the db here before we start writing to the 
	// destination file because XP_Stat can return file size including bytes written...
	MsgERR msgErr = nsMailDatabase::Open (destFolder, TRUE, &mailDb);	
	PRUint32 length = mailHdr->GetByteLength();

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
		PRUint32 nRead = XP_FileRead (m_ibuffer, length > m_ibuffer_size ? m_ibuffer_size  : length, sourceFid);
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

			if (mailDb)
				mailDb->Close();

			return MK_MSG_ERROR_WRITING_MAIL_FOLDER;   // caller (ApplyFilters) currently ignores error conditions
		}
			
		length -= nRead;
	}
	
	XP_ASSERT(length == 0);

	// if we have made it this far then the message has successfully been written to the new folder
	// now add the header to the mailDb.
	if (eSUCCESS == msgErr)
	{
		nsIMessage *newHdr = new nsIMessage();	
		if (newHdr)
		{
			newHdr->CopyFromMsgHdr (mailHdr, sourceDB->GetDB(), mailDb->GetDB());
			// set new byte offset, since the offset in the old file is certainly wrong
			newHdr->SetMessageKey (newMsgPos); 
			newHdr->OrFlags(kNew);

			msgErr = mailDb->AddHdrToDB (newHdr, NULL, m_updateAsWeGo);
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
#endif // DOING_FILTERS

#ifdef IMAP_NEW_MAIL_HANDLED

ParseIMAPMailboxState::ParseIMAPMailboxState(MSG_Master *master, MSG_IMAPHost *host, MSG_FolderInfoMail *folder,
											 MSG_UrlQueue *urlQueue, TImapFlagAndUidState *flagStateAdopted)
											: nsParseNewMailState(master, folder), fUrlQueue(urlQueue)
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

void ParseIMAPMailboxState::SetPublishUID(PRInt32 uid)
{
	fNextMessagePublishUID = uid;
}

void ParseIMAPMailboxState::SetPublishByteLength(PRUint32 byteLength)
{
	fNextMessageByteLength = byteLength;
}

void ParseIMAPMailboxState::DoneParsingFolder()
{
	nsMsgMailboxParser::DoneParsingFolder();
	if (m_mailDB)
	{
		// make sure the highwater mark is correct
		if (m_mailDB->m_dbFolderInfo->GetNumVisibleMessages())
		{
			ListContext *listContext;
			nsIMessage *currentHdr;
			if ((m_mailDB->ListLast(&listContext, &currentHdr) == NS_OK) &&
				currentHdr)
			{
				m_mailDB->m_dbFolderInfo->m_LastMessageUID = currentHdr->GetMessageKey();
				currentHdr->Release();
				m_mailDB->ListDone(listContext);
			}
			else
				m_mailDB->m_dbFolderInfo->m_LastMessageUID = 0;
		}
		else
			m_mailDB->m_dbFolderInfo->m_LastMessageUID = 0;
			
		m_mailDB->Close();
		m_mailDB = NULL;
	}
}

int ParseIMAPMailboxState::MarkFilteredMessageRead(nsIMessage *msgHdr)
{
	msgHdr->OrFlags(kIsRead);
	nsMsgKeyArray	keysToFlag;

	keysToFlag.Add(msgHdr->GetMessageKey());
	MSG_IMAPFolderInfoMail *imapFolder = m_folder->GetIMAPFolderInfoMail();
	if (imapFolder)
		imapFolder->StoreImapFlags(m_pane, kImapMsgSeenFlag, TRUE, keysToFlag, GetFilterUrlQueue());

	return 0;
}


int ParseIMAPMailboxState::MoveIncorporatedMessage(nsIMessage *mailHdr, 
											   nsMailDatabase *sourceDB, 
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
				nsMsgKey keyToFilter = mailHdr->GetMessageKey();

				if (sourceDB && destMailFolder)
				{
					PRBool imapDeleteIsMoveToTrash = m_host->GetDeleteIsMoveToTrash();
					
					nsMsgKeyArray *idsToMoveFromInbox = destMailFolder->GetImapIdsToMoveFromInbox();
					idsToMoveFromInbox->Add(keyToFilter);

					// this is our last best chance to log this
					if (m_filterList->IsLoggingEnabled())
						LogRuleHit(filter, mailHdr);

					if (imapDeleteIsMoveToTrash)
					{
						if (m_parseMsgState->m_newMsgHdr)
						{
							m_parseMsgState->m_newMsgHdr->Release();
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
void ParseIMAPMailboxState::ApplyFilters(PRBool *pMoved)
{
 	if (fParsingInbox && !(GetCurrentMsg()->GetFlags() & kIsRead) )
 		nsParseNewMailState::ApplyFilters(pMoved);
 	else
 		*pMoved = FALSE;
 	
 	if (!*pMoved && m_parseMsgState->m_newMsgHdr)
 		fFetchBodyKeys.Add(m_parseMsgState->m_newMsgHdr->GetMessageKey());
}


// For IMAP, the message key is the IMAP UID
// This is where I will add code to fix the message length as well - km
void ParseIMAPMailboxState::FolderTypeSpecificTweakMsgHeader(nsIMessage *tweakMe)
{
	if (m_mailDB && tweakMe)
	{
		tweakMe->SetMessageKey(fNextMessagePublishUID);
		tweakMe->SetByteLength(fNextMessageByteLength);
		tweakMe->SetMessageSize(fNextMessageByteLength);
		
		if (fFlagState)
		{
			PRBool foundIt = FALSE;
			imapMessageFlagsType imap_flags = IMAP_GetMessageFlagsFromUID(fNextMessagePublishUID, &foundIt, fFlagState);
			if (foundIt)
			{
				// make a mask and clear these message flags
				PRUint32 mask = MSG_FLAG_READ | MSG_FLAG_REPLIED | MSG_FLAG_MARKED | MSG_FLAG_IMAP_DELETED;
				tweakMe->SetFlags(tweakMe->GetFlags() & ~mask);
				
				// set the new value for these flags
				PRUint32 newFlags = 0;
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
				if (IMAP_SupportMessageFlags(fFlagState, (kImapMsgSupportUserFlag |
														  kImapMsgSupportMDNSentFlag)))
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
				if (imap_flags & kImapMsgForwardedFlag)
					newFlags |= MSG_FLAG_FORWARDED;

				if (newFlags)
					tweakMe->SetFlags(tweakMe->GetFlags() | newFlags);
			}
		}
	}
}

PRInt32	ParseIMAPMailboxState::PublishMsgHeader()
{
	PRBool		moved = FALSE;

	m_parseMsgState->FinishHeader();
	
	if (m_parseMsgState->m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_parseMsgState->m_newMsgHdr);
		if (!m_disableFilters) {
			ApplyFilters(&moved);
		}
		if (!moved)
		{
			PRBool thisMessageUnRead = !(m_parseMsgState->m_newMsgHdr->GetFlags() & kIsRead);
			if (m_mailDB)
			{
				if (thisMessageUnRead)
					m_parseMsgState->m_newMsgHdr->OrFlags(kNew);
				m_mailDB->AddHdrToDB (m_parseMsgState->m_newMsgHdr, NULL,
					(fNextSequenceNum == -1) ? m_updateAsWeGo : FALSE);
				// following is for cacheless imap - match sequence number
				// to location to insert in view.
			}
			if (m_folder && thisMessageUnRead)
				m_folder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);


		}		// if it was moved by imap filter, m_parseMsgState->m_newMsgHdr == NULL
		else if (m_parseMsgState->m_newMsgHdr)
		{
			// make sure global db is set correctly for delete of strings from hash tbl.
			m_parseMsgState->m_newMsgHdr->unrefer();
		}
		m_parseMsgState->m_newMsgHdr = NULL;
	}
	return 0;
}

void ParseIMAPMailboxState::SetNextSequenceNum(PRInt32 seqNum)
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
	m_writeMozillaStatus = TRUE;
}

ParseOutgoingMessage::~ParseOutgoingMessage()
{
	FREEIF (m_outputBuffer);
}

void ParseOutgoingMessage::Clear()
{
	nsParseMailMessageState::Clear();
	m_wroteXMozillaStatus = FALSE;
	m_bytes_written = 0;
}

// We've found the start of the next message, so finish this one off.
void ParseOutgoingMessage::FinishHeader()
{
	PRInt32 origPosition = m_position, len = 0;
	if (m_out_file && m_writeToOutFile)
	{
		if (origPosition > 0 && !m_lastBodyLineEmpty)
		{
			len = XP_FileWrite (LINEBREAK, LINEBREAK_LEN, m_out_file);
			m_bytes_written += LINEBREAK_LEN;
			m_position += LINEBREAK_LEN;
		}
	}
	nsParseMailMessageState::FinishHeader();
}

int		ParseOutgoingMessage::StartNewEnvelope(const char *line, PRUint32 lineLength)
{
	int status = nsParseMailMessageState::StartNewEnvelope(line, lineLength);

	if ((status >= 0) && m_out_file && m_writeToOutFile)
	{
		status = XP_FileWrite(line, lineLength, m_out_file);
		if (status > 0)
			m_bytes_written += lineLength;
	}
	return status;
}

PRInt32	ParseOutgoingMessage::ParseFolderLine(const char *line, PRUint32 lineLength)
{
	PRInt32 res = nsParseMailMessageState::ParseFolderLine(line, lineLength);
	int len = 0;

	if (res < 0)
		return res;
	if (m_out_file && m_writeToOutFile)
	{
		if (!nsCRT::strncmp(line, X_MOZILLA_STATUS, X_MOZILLA_STATUS_LEN)) 
			m_wroteXMozillaStatus = TRUE;

		m_lastBodyLineEmpty = (m_state == MBOX_PARSE_BODY && (EMPTY_MESSAGE_LINE(line)));

		// make sure we mangle naked From lines
		if (line[0] == 'F' && !nsCRT::strncmp(line, "From ", 5))
		{
			res = XP_FileWrite (">", 1, m_out_file);
			if (res < 1)
				return res;
			m_position += 1;
		}
		if (!m_wroteXMozillaStatus && m_writeMozillaStatus && m_state == MBOX_PARSE_BODY)
		{
			char buf[50];
			PRUint32 dbFlags = m_newMsgHdr ? m_newMsgHdr->GetFlags() : 0;

			if (m_newMsgHdr)
				m_newMsgHdr->SetStatusOffset(m_bytes_written);
			PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS_FORMAT LINEBREAK, (m_newMsgHdr) ? m_newMsgHdr->GetFlags() & ~MSG_FLAG_RUNTIME_ONLY : 0);
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
PRInt32 ParseOutgoingMessage::LineBufferCallback(char *line, PRUint32 lineLength,
									  void *closure)
{
	ParseOutgoingMessage *parseState = (ParseOutgoingMessage *) closure;

	return parseState->ParseFolderLine(line, lineLength);
}

PRInt32 ParseOutgoingMessage::ParseBlock(const char *block, PRUint32 length)
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
					(PRInt32 (_Optlink*) (char*,PRUint32,void*))
#endif
					   LineBufferCallback, this);
}

void ParseOutgoingMessage::AdvanceOutPosition(PRUint32 amountToAdvance)
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

#endif /* IMAP_NEW_MAIL_HANDLED */
