/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"
#include "nsIURI.h"
#include "nsParseMailbox.h"
#include "nsIMsgHdr.h"
#include "nsIMsgDatabase.h"
#include "nsMsgMessageFlags.h"
#include "nsIDBFolderInfo.h"
#include "nsIInputStream.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsMsgBaseCID.h"
#include "nsMsgDBCID.h"
#include "nsIMailboxUrl.h"
#include "nsCRT.h"
#include "nsFileStream.h"
#include "nsMsgFolderFlags.h"
#include "nsIMsgFolder.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsIMsgMailNewsUrl.h"
#include "nsLocalStringBundle.h"
#include "nsIMsgFilterList.h"
#include "nsIMsgFilter.h"
#include "nsIIOService.h"
#include "nsNetCID.h"
#include "nsRDFCID.h"
#include "nsIPref.h"
#include "nsIRDFService.h"
#include "nsMsgI18N.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsMsgUtils.h"
#include "prprf.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS_INHERITED2(nsMsgMailboxParser, nsParseMailMessageState, nsIStreamListener, nsIDBChangeListener);

// Whenever data arrives from the connection, core netlib notifices the protocol by calling
// OnDataAvailable. We then read and process the incoming data from the input stream. 
NS_IMETHODIMP nsMsgMailboxParser::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *aIStream, PRUint32 sourceOffset, 
												  PRUint32 aLength)
{
	// right now, this really just means turn around and process the url
	nsresult rv = NS_OK;
	nsCOMPtr<nsIURI> url = do_QueryInterface(ctxt, &rv);
	if (NS_SUCCEEDED(rv))
		rv = ProcessMailboxInputStream(url, aIStream, aLength);
	return rv;
}

NS_IMETHODIMP nsMsgMailboxParser::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
	nsTime currentTime;
	m_startTime = currentTime;


	// extract the appropriate event sinks from the url and initialize them in our protocol data
	// the URL should be queried for a nsIMailboxURL. If it doesn't support a mailbox URL interface then
	// we have an error.
	nsresult rv = NS_OK;

    nsCOMPtr<nsIIOService> ioServ(do_GetService(kIOServiceCID, &rv));

	nsCOMPtr<nsIMailboxUrl> runningUrl = do_QueryInterface(ctxt, &rv);

	nsCOMPtr<nsIMsgMailNewsUrl> url = do_QueryInterface(ctxt);

	if (NS_SUCCEEDED(rv) && runningUrl)
	{
		url->GetStatusFeedback(getter_AddRefs(m_statusFeedback));

		// okay, now fill in our event sinks...Note that each getter ref counts before
		// it returns the interface to us...we'll release when we are done
        nsXPIDLCString fileName;
        nsXPIDLCString folderName;
        rv = url->GetFilePath(getter_Copies(fileName));
		url->GetFileName(getter_Copies(folderName));
        
        nsXPIDLCString tempfolder;
        rv = nsStdUnescape((char*)folderName.get(), getter_Copies(tempfolder));

        // convert from OS native charset to unicode
        rv = ConvertToUnicode(nsMsgI18NFileSystemCharset(), tempfolder, m_folderName);
        if (NS_FAILED(rv))
          m_folderName.AssignWithConversion(tempfolder);

		if (fileName)
		{
            char* result = nsnull;
            rv = nsStdUnescape((char*)fileName.get(), &result);
			nsFilePath dbPath(result);
            CRTFREEIF(result);
			nsFileSpec dbName(dbPath);

			// the size of the mailbox file is our total base line for measuring progress
			m_graph_progress_total = dbName.GetFileSize();
			UpdateStatusText(LOCAL_STATUS_SELECTING_MAILBOX);
			
			nsCOMPtr<nsIMsgDatabase> mailDB;
			rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDB));
			if (NS_SUCCEEDED(rv) && mailDB)
			{
				nsCOMPtr <nsIFileSpec> dbFileSpec;
				NS_NewFileSpecWithSpec(dbName, getter_AddRefs(dbFileSpec));
				rv = mailDB->Open(dbFileSpec, PR_TRUE, PR_TRUE, (nsIMsgDatabase **) getter_AddRefs(m_mailDB));
                                if (m_mailDB)
                                  m_mailDB->AddListener(this);
			}
			NS_ASSERTION(m_mailDB, "failed to open mail db parsing folder");
#ifdef DEBUG_mscott
			printf("url file = %s\n", (const char *)fileName);
#endif
		}
 	}

	// need to get the mailbox name out of the url and call SetMailboxName with it.
	// then, we need to open the mail db for this parser.
	return rv;

}

// stop binding is a "notification" informing us that the stream associated with aURL is going away. 
NS_IMETHODIMP nsMsgMailboxParser::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult aStatus)
{
	DoneParsingFolder(aStatus);
	// what can we do? we can close the stream?
	m_urlInProgress = PR_FALSE;  // don't close the connection...we may be re-using it.

        if (m_mailDB)
          m_mailDB->RemoveListener(this);
	// and we want to mark ourselves for deletion or some how inform our protocol manager that we are 
	// available for another url if there is one....
#ifdef DEBUG1
	// let's dump out the contents of our db, if possible.
	if (m_mailDB)
	{
		nsMsgKeyArray	keys;
		nsCAutoString	author;
		nsCAutoString	subject;

//		m_mailDB->PrePopulate();
		m_mailDB->ListAllKeys(keys);
        PRUint32 size = keys.GetSize();
		for (PRUint32 keyindex = 0; keyindex < size; keyindex++)
		{
			nsCOMPtr<nsIMsgDBHdr> msgHdr;
			nsresult ret =
                m_mailDB->GetMsgHdrForKey(keys[keyindex],
                                          getter_AddRefs(msgHdr));
			if (NS_SUCCEEDED(ret) && msgHdr)
			{
				nsMsgKey key;

				msgHdr->GetMessageKey(&key);
				msgHdr->GetAuthor(&author);
				msgHdr->GetSubject(&subject);
#ifdef DEBUG_bienvenu
				// leak nsString return values...
				printf("hdr key = %d, author = %s subject = %s\n", key, author.get(), subject.get());
#endif
			}
		}
		m_mailDB->Close(PR_TRUE);
	}
#endif


	// be sure to clear any status text and progress info..
	m_graph_progress_received = 0;
	UpdateProgressPercent();
	UpdateStatusText(LOCAL_STATUS_DOCUMENT_DONE);

	return NS_OK;

}



/* void OnKeyChange (in nsMsgKey aKeyChanged, in unsigned long aOldFlags, in unsigned long aNewFlags, in nsIDBChangeListener aInstigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnKeyChange(nsMsgKey aKeyChanged, PRUint32 aOldFlags, PRUint32 aNewFlags, nsIDBChangeListener *aInstigator)
{
    return NS_OK;
}

/* void OnKeyDeleted (in nsMsgKey aKeyChanged, in nsMsgKey aParentKey, in long aFlags, in nsIDBChangeListener aInstigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnKeyDeleted(nsMsgKey aKeyChanged, nsMsgKey aParentKey, PRInt32 aFlags, nsIDBChangeListener *aInstigator)
{
    return NS_OK;
}

/* void OnKeyAdded (in nsMsgKey aKeyChanged, in nsMsgKey aParentKey, in long aFlags, in nsIDBChangeListener aInstigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnKeyAdded(nsMsgKey aKeyChanged, nsMsgKey aParentKey, PRInt32 aFlags, nsIDBChangeListener *aInstigator)
{
    return NS_OK;
}

/* void OnParentChanged (in nsMsgKey aKeyChanged, in nsMsgKey oldParent, in nsMsgKey newParent, in nsIDBChangeListener aInstigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnParentChanged(nsMsgKey aKeyChanged, nsMsgKey oldParent, nsMsgKey newParent, nsIDBChangeListener *aInstigator)
{
    return NS_OK;
}

/* void OnAnnouncerGoingAway (in nsIDBChangeAnnouncer instigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnAnnouncerGoingAway(nsIDBChangeAnnouncer *instigator)
{
    m_mailDB = nsnull; // what else do we need to do here?
    return NS_OK;
}

/* void OnReadChanged (in nsIDBChangeListener instigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnReadChanged(nsIDBChangeListener *instigator)
{
    return NS_OK;
}


nsMsgMailboxParser::nsMsgMailboxParser() : nsMsgLineBuffer(nsnull, PR_FALSE)
{
  /* the following macro is used to initialize the ref counting data */
	NS_INIT_REFCNT();

	m_obuffer = nsnull;
	m_obuffer_size = 0;
	m_graph_progress_total = 0;
	m_graph_progress_received = 0;
	m_updateAsWeGo = PR_TRUE;
	m_ignoreNonMailFolder = PR_FALSE;
	m_isRealMailFolder = PR_TRUE;
}

nsMsgMailboxParser::~nsMsgMailboxParser()
{
}

void nsMsgMailboxParser::UpdateStatusText (PRUint32 stringID)
{
	if (m_statusFeedback)
	{
        nsresult rv;
        if (!mStringService) // if we haven't gotten the serivce yet...
            mStringService = do_GetService(NS_MSG_MAILBOXSTRINGSERVICE_CONTRACTID);
    
        nsXPIDLString finalString;
		if (stringID == LOCAL_STATUS_SELECTING_MAILBOX)
		{
            nsCOMPtr<nsIStringBundle> bundle;
            rv = mStringService->GetBundle(getter_AddRefs(bundle));
            NS_ASSERTION(NS_SUCCEEDED(rv), "GetBundle failed");
            if (NS_FAILED(rv)) return;

            const PRUnichar * stringArray[] = { m_folderName.get() };
            rv = bundle->FormatStringFromID(stringID, stringArray, 1,
                                                   getter_Copies(finalString));
		}
		else
            mStringService->GetStringByID(stringID,
                                          getter_Copies(finalString));

        m_statusFeedback->ShowStatusString(finalString);

    }
}

void nsMsgMailboxParser::UpdateProgressPercent ()
{
	if (m_statusFeedback && m_graph_progress_total != 0)
	{
		m_statusFeedback->ShowProgress((100 *(m_graph_progress_received))  / m_graph_progress_total);	
	}
}

int nsMsgMailboxParser::ProcessMailboxInputStream(nsIURI* aURL, nsIInputStream *aIStream, PRUint32 aLength)
{
	nsresult ret = NS_OK;

	PRUint32 bytesRead = 0;

	if (NS_SUCCEEDED(m_inputStream.GrowBuffer(aLength)))
	{
		// OK, this sucks, but we're going to have to copy into our
		// own byte buffer, and then pass that to the line buffering code,
		// which means a couple buffer copies.
		ret = aIStream->Read(m_inputStream.GetBuffer(), aLength, &bytesRead);
		if (NS_SUCCEEDED(ret))
			ret = BufferInput(m_inputStream.GetBuffer(), bytesRead);
	}
	if (m_graph_progress_total > 0)
	{
		if (NS_SUCCEEDED(ret))
		  m_graph_progress_received += bytesRead;
	}
	return (ret);
}

void nsMsgMailboxParser::DoneParsingFolder(nsresult status)
{
	/* End of file.  Flush out any partial line remaining in the buffer. */
	FlushLastLine();
	PublishMsgHeader(nsnull);

	// only mark the db valid if we've succeeded.
	if (NS_SUCCEEDED(status) && m_mailDB)	// finished parsing, so flush db folder info 
		UpdateDBFolderInfo();
	else if (m_mailDB)
	{
		m_mailDB->SetSummaryValid(PR_FALSE);
		m_mailDB->Commit(nsMsgDBCommitType::kLargeCommit);
	}

//	if (m_folder != nsnull)
//		m_folder->SummaryChanged();
	FreeBuffers();
}

void nsMsgMailboxParser::FreeBuffers()
{
	/* We're done reading the folder - we don't need these things
	 any more. */
	PR_FREEIF (m_obuffer);
	m_obuffer_size = 0;
}

void nsMsgMailboxParser::UpdateDBFolderInfo()
{
	UpdateDBFolderInfo(m_mailDB);
}

// update folder info in db so we know not to reparse.
void nsMsgMailboxParser::UpdateDBFolderInfo(nsIMsgDatabase *mailDB)
{
	// ### wrong - use method on db.
	mailDB->SetSummaryValid(PR_TRUE);
	mailDB->Commit(nsMsgDBCommitType::kLargeCommit);
//	m_mailDB->Close();
}

// By default, do nothing
void nsMsgMailboxParser::FolderTypeSpecificTweakMsgHeader(nsIMsgDBHdr * /* tweakMe */)
{
}

// Tell the world about the message header (add to db, and view, if any)
PRInt32 nsMsgMailboxParser::PublishMsgHeader(nsIMsgWindow *msgWindow)
{
	FinishHeader();
	if (m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_newMsgHdr);

		PRUint32 flags;
        (void)m_newMsgHdr->GetFlags(&flags);
		if (flags & MSG_FLAG_EXPUNGED)
		{
			nsCOMPtr<nsIDBFolderInfo> folderInfo;
			m_mailDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
            PRUint32 size;
            (void)m_newMsgHdr->GetMessageSize(&size);
            folderInfo->ChangeExpungedBytes(size);
			m_newMsgHdr = nsnull;
		}
		else if (m_mailDB)
		{
			m_mailDB->AddNewHdrToDB(m_newMsgHdr, m_updateAsWeGo);
			m_newMsgHdr = nsnull;
		}
		else
			NS_ASSERTION(PR_FALSE, "no database while parsing local folder");	// should have a DB, no?
	}
	else if (m_mailDB)
	{
		nsCOMPtr<nsIDBFolderInfo> folderInfo;
		m_mailDB->GetDBFolderInfo(getter_AddRefs(folderInfo));
		if (folderInfo)
			folderInfo->ChangeExpungedBytes(m_position - m_envelope_pos);
	}
	return 0;
}

void nsMsgMailboxParser::AbortNewHeader()
{
	if (m_newMsgHdr && m_mailDB)
		m_newMsgHdr = nsnull;
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
		while (s < end && IS_SPACE(*s))
			s++;
		if ((end - s) < 20 || !IsEnvelopeLine(s, end - s))
		{
//			char buf[500];
//			PR_snprintf (buf, sizeof(buf),
//						 XP_GetString(MK_MSG_NON_MAIL_FILE_READ_QUESTION),
//						 folder_name);
			m_isRealMailFolder = PR_FALSE;
			if (m_ignoreNonMailFolder)
				return 0;
//			else if (!FE_Confirm (m_context, buf))
//				return NS_MSG_NOT_A_MAIL_FOLDER; /* #### NOT_A_MAIL_FILE */
		}
	}
//	m_graph_progress_received += lineLength;

	// mailbox parser needs to do special stuff when it finds an envelope
	// after parsing a message body. So do that.
	if (line[0] == 'F' && IsEnvelopeLine(line, lineLength))
	{
		// **** This used to be
		// PR_ASSERT (m_parseMsgState->m_state == nsMailboxParseBodyState);
		// **** I am not sure this is a right thing to do. This happens when
		// going online, downloading a message while playing back append
		// draft/template offline operation. We are mixing
        // nsMailboxParseBodyState &&
		// nsMailboxParseHeadersState. David I need your help here too. **** jt

		NS_ASSERTION (m_state == nsIMsgParseMailMsgState::ParseBodyState ||
				   m_state == nsIMsgParseMailMsgState::ParseHeadersState, "invalid parse state"); /* else folder corrupted */
		PublishMsgHeader(nsnull);
		Clear();
		status = StartNewEnvelope(line, lineLength);
		NS_ASSERTION(status >= 0, " error starting envelope parsing mailbox");
		// at the start of each new message, update the progress bar
		UpdateProgressPercent();
		if (status < 0)
			return status;
	}
	// otherwise, the message parser can handle it completely.
	else if (m_mailDB != nsnull)	// if no DB, do we need to parse at all?
		return ParseFolderLine(line, lineLength);

	return 0;

}

NS_IMPL_ISUPPORTS1(nsParseMailMessageState, nsIMsgParseMailMsgState)

nsParseMailMessageState::nsParseMailMessageState()
{
	NS_INIT_REFCNT();
	m_position = 0;
	m_IgnoreXMozillaStatus = PR_FALSE;
	m_state = nsIMsgParseMailMsgState::ParseBodyState;
	Clear();
  NS_DEFINE_CID(kMsgHeaderParserCID, NS_MSGHEADERPARSER_CID);

  m_HeaderAddressParser = do_GetService(kMsgHeaderParserCID);
}

nsParseMailMessageState::~nsParseMailMessageState()
{
	ClearAggregateHeader (m_toList);
	ClearAggregateHeader (m_ccList);
}

void nsParseMailMessageState::Init(PRUint32 fileposition)
{
	m_state = nsIMsgParseMailMsgState::ParseBodyState;
	m_position = fileposition;
	m_newMsgHdr = nsnull;
}

NS_IMETHODIMP nsParseMailMessageState::Clear()
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
  m_in_reply_to.length = 0;
	m_content_type.length = 0;
	m_mdn_original_recipient.length = 0;
	m_body_lines = 0;
	m_newMsgHdr = nsnull;
	m_envelope_pos = 0;
	ClearAggregateHeader (m_toList);
	ClearAggregateHeader (m_ccList);
	m_headers.ResetWritePos();
	m_envelope.ResetWritePos();
	return NS_OK;
}

NS_IMETHODIMP nsParseMailMessageState::SetState(nsMailboxParseState aState)
{
	m_state = aState;
	return NS_OK;
}

NS_IMETHODIMP nsParseMailMessageState::GetState(nsMailboxParseState *aState)
{
	if (!aState) 
		return NS_ERROR_NULL_POINTER;

	*aState = m_state;
	return NS_OK;
}

NS_IMETHODIMP
nsParseMailMessageState::GetEnvelopePos(PRUint32 *aEnvelopePos)
{
    if (!aEnvelopePos) 
        return NS_ERROR_NULL_POINTER;
    *aEnvelopePos = m_envelope_pos;
    return NS_OK;
}

NS_IMETHODIMP nsParseMailMessageState::SetEnvelopePos(PRUint32 aEnvelopePos)
{
	m_envelope_pos = aEnvelopePos;
    m_position = m_envelope_pos;
    m_headerstartpos = m_position;
	return NS_OK;
}

NS_IMETHODIMP nsParseMailMessageState::GetNewMsgHdr(nsIMsgDBHdr ** aMsgHeader)
{
  if (aMsgHeader)
  {
    *aMsgHeader = m_newMsgHdr;
    NS_IF_ADDREF(*aMsgHeader);
  }
  return NS_OK;
}

NS_IMETHODIMP nsParseMailMessageState::ParseAFolderLine(const char *line, PRUint32 lineLength)
{
	ParseFolderLine(line, lineLength);
	return NS_OK;
}

PRInt32 nsParseMailMessageState::ParseFolderLine(const char *line, PRUint32 lineLength)
{
	int status = 0;

	if (m_state == nsIMsgParseMailMsgState::ParseHeadersState)
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
			 m_state = nsIMsgParseMailMsgState::ParseBodyState;
		}
		else
		{
		  /* Otherwise, this line belongs to a header.  So append it to the
			 header data, and stay in MBOX `MIME_PARSE_HEADERS' state.
		   */
			m_headers.AppendBuffer(line, lineLength);
		}
	}
	else if ( m_state == nsIMsgParseMailMsgState::ParseBodyState)
	{
		m_body_lines++;
	}

	m_position += lineLength;

	return 0;
}

NS_IMETHODIMP nsParseMailMessageState::SetMailDB(nsIMsgDatabase *mailDB)
{
	m_mailDB = dont_QueryInterface(mailDB);
	return NS_OK;
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
  if (IS_SPACE(*date) || date == end)
	return PR_FALSE;

  /* Skip over user name. */
  while (!IS_SPACE(*date) && date < end)
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
NS_IMETHODIMP nsParseMailMessageState::FinishHeader()
{
  if (m_newMsgHdr)
  {
    m_newMsgHdr->SetMessageKey(m_envelope_pos);
    m_newMsgHdr->SetMessageSize(m_position - m_envelope_pos);	// dmb - no longer number of lines.
    m_newMsgHdr->SetLineCount(m_body_lines);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsParseMailMessageState::GetAllHeaders(char ** pHeaders, PRInt32 *pHeadersSize)
{
	if (!pHeaders || !pHeadersSize)
		return NS_ERROR_NULL_POINTER;
	*pHeaders = m_headers.GetBuffer();
	*pHeadersSize = m_headers.GetBufferPos();
	return NS_OK;
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

	struct message_header *header = nsnull;
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
		outHeader->value = nsnull;
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
	m_state = nsIMsgParseMailMsgState::ParseHeadersState;
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
		  else if (!nsCRT::strncasecmp ("Content-Type", buf, end - buf))
		  header = &m_content_type;
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
    case 'I' : case 'i':
      if (!nsCRT::strncasecmp ("In-Reply-To", buf, end - buf))
        header = &m_in_reply_to;
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
	  while (*buf != 0 && *buf != nsCRT::CR && *buf != nsCRT::LF)
		buf++;

	  if (buf+1 >= buf_end)
		;
	  /* If "\r\n " or "\r\n\t" is next, that doesn't terminate the header. */
	  else if (buf+2 < buf_end &&
			   (buf[0] == nsCRT::CR  && buf[1] == nsCRT::LF) &&
			   (buf[2] == ' ' || buf[2] == '\t'))
		{
		  buf += 3;
		  goto SEARCH_NEWLINE;
		}
	  /* If "\r " or "\r\t" or "\n " or "\n\t" is next, that doesn't terminate
		 the header either. */
	  else if ((buf[0] == nsCRT::CR  || buf[0] == nsCRT::LF) &&
			   (buf[1] == ' ' || buf[1] == '\t'))
		{
		  buf += 2;
		  goto SEARCH_NEWLINE;
		}

	  if (header)
		header->length = buf - header->value;

	  if (*buf == nsCRT::CR || *buf == nsCRT::LF)
		{
		  char *last = buf;
		  if (*buf == nsCRT::CR && buf[1] == nsCRT::LF)
			buf++;
		  buf++;
		  *last = 0;	/* short-circuit const, and null-terminate header. */
		}

	  if (header)
		{
		  /* More const short-circuitry... */
		  /* strip leading whitespace */
		  while (IS_SPACE (*header->value))
			header->value++, header->length--;
		  /* strip trailing whitespace */
		  while (header->length > 0 &&
				 IS_SPACE (header->value [header->length - 1]))
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

	while (s < end && IS_SPACE (*s))
		s++;
	m_envelope_from.value = s;
	while (s < end && !IS_SPACE (*s))
		s++;
	m_envelope_from.length = s - m_envelope_from.value;

	while (s < end && IS_SPACE (*s))
		s++;
	m_envelope_date.value = s;
	m_envelope_date.length = (PRUint16) (line_size - (s - m_envelope.GetBuffer()));
	while (IS_SPACE (m_envelope_date.value [m_envelope_date.length - 1]))
		m_envelope_date.length--;

	/* #### short-circuit const */
	((char *) m_envelope_from.value) [m_envelope_from.length] = 0;
	((char *) m_envelope_date.value) [m_envelope_date.length] = 0;

	return 0;
}

#ifdef WE_CONDENSE_MIME_STRINGS
static char *
msg_condense_mime2_string(char *sourceStr)
{
	char *returnVal = nsCRT::strdup(sourceStr);
	if (!returnVal) 
		return nsnull;
	
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


  PRUint32 flags;
  (void)m_newMsgHdr->GetFlags(&flags);
	/* strip "Re: " */
	/* We trust the X-Mozilla-Status line to be the smartest in almost
	 all things.  One exception, however, is the HAS_RE flag.  Since
	 we just parsed the subject header anyway, we expect that parsing
	 to be smartest.  (After all, what if someone just went in and
	 edited the subject line by hand?) */
	if (NS_MsgStripRE((const char **) &key, &L))
    flags |= MSG_FLAG_HAS_RE;
	else
    flags &= ~MSG_FLAG_HAS_RE;
	m_newMsgHdr->SetFlags(flags); // this *does not* update the mozilla-status header in the local folder

//  if (!*key) return 0; /* To catch a subject of "Re:" */

	// Condense the subject text into as few MIME-2 encoded words as possible.
#ifdef WE_CONDENSE_MIME_STRINGS
	char *condensedKey = msg_condense_mime2_string(key);
#else
	char *condensedKey = nsnull;
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
	nsresult ret=NS_OK;

	if (!header || header->length == 0)
		return NS_OK;

	NS_ASSERTION (header->length == (short) nsCRT::strlen (header->value), "invalid message_header");
	NS_ASSERTION (ret_name != nsnull, "null ret_name");

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
// into a single nsIMsgDBHdr to store in a database.
int nsParseMailMessageState::FinalizeHeaders()
{
	int status = 0;
	struct message_header *sender;
	struct message_header *recipient;
	struct message_header *subject;
	struct message_header *id;
  struct message_header *inReplyTo;
	struct message_header *references;
	struct message_header *date;
	struct message_header *statush;
	struct message_header *mozstatus;
	struct message_header *mozstatus2;
	struct message_header *priority;
	struct message_header *ccList;
	struct message_header *mdn_dnt;
	struct message_header md5_header;
	struct message_header *content_type;
	unsigned char md5_bin [16];
	char md5_data [50];

	const char *s;
	PRUint32 flags = 0;
	PRUint32 delta = 0;
	nsMsgPriorityValue priorityFlags = nsMsgPriority::notSet;

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
  inReplyTo = (m_in_reply_to.length ? &m_in_reply_to : 0);
	content_type = (m_content_type.length ? &m_content_type : 0);

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
			priorityFlags = (nsMsgPriorityValue) ((flags & MSG_FLAG_PRIORITIES) >> 13);
			flags &= ~MSG_FLAG_PRIORITIES;
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
		nsresult ret = m_mailDB->CreateNewHdr(m_envelope_pos, getter_AddRefs(m_newMsgHdr));
		if (NS_SUCCEEDED(ret) && m_newMsgHdr)
		{
      PRUint32 origFlags;
      (void)m_newMsgHdr->GetFlags(&origFlags);
			if (origFlags & MSG_FLAG_HAS_RE)
				flags |= MSG_FLAG_HAS_RE;
			else
				flags &= ~MSG_FLAG_HAS_RE;

      flags &= ~MSG_FLAG_OFFLINE; // don't keep MSG_FLAG_OFFLINE for local msgs
			if (mdn_dnt && !(origFlags & MSG_FLAG_READ) &&
				!(origFlags & MSG_FLAG_MDN_REPORT_SENT))
				flags |= MSG_FLAG_MDN_REPORT_NEEDED;

			m_newMsgHdr->SetFlags(flags);
			if (priorityFlags != nsMsgPriority::notSet)
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
			if (sender)
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
				char * ch;
				NS_ASSERTION (recipient->length == (PRUint16) nsCRT::strlen(recipient->value), "invalid recipient");
				ch = PL_strchr(recipient->value, ',');
				if (ch)
				{
					*ch = 0;
					recipient->length = nsCRT::strlen(recipient->value);
				}
				m_newMsgHdr->SetRecipients(recipient->value);
                m_newMsgHdr->SetRecipientsIsNewsgroup(PR_FALSE);
			}
			else if (recipient)
			{
				// note that we're now setting the whole recipient list,
				// not just the pretty name of the first recipient.
				PRUint32 numAddresses;
				char	*names;
				char	*addresses;

				ret = m_HeaderAddressParser->ParseHeaderAddresses (nsnull, recipient->value, &names, &addresses, &numAddresses);
				if (ret == NS_OK)
				{
					m_newMsgHdr->SetRecipientsArray(names, addresses, numAddresses);
					PR_FREEIF(addresses);
					PR_FREEIF(names);
				}
				else {	// hmm, should we just use the original string?
					m_newMsgHdr->SetRecipients(recipient->value);
                    m_newMsgHdr->SetRecipientsIsNewsgroup(PR_TRUE);
                }
			}
			if (ccList)
			{
				PRUint32 numAddresses;
				char	*names;
				char	*addresses;

				ret = m_HeaderAddressParser->ParseHeaderAddresses (nsnull, ccList->value, &names, &addresses, &numAddresses);
				if (ret == NS_OK)
				{
					m_newMsgHdr->SetCCListArray(names, addresses, numAddresses);
					PR_FREEIF(addresses);
					PR_FREEIF(names);
				}
				else	// hmm, should we just use the original string?
					m_newMsgHdr->SetCcList(ccList->value);
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
                        PRUint32 msgFlags = 0;
                        (void)m_newMsgHdr->GetFlags(&msgFlags);
                        switch (*s)
                        {
                          case 'R': case 'r':
                            m_newMsgHdr->SetFlags(msgFlags | MSG_FLAG_READ);
                            break;
                          case 'D': case 'd':
                            /* msg->flags |= MSG_FLAG_EXPUNGED;  ### Is this reasonable? */
                            break;
                          case 'N': case 'n':
                          case 'U': case 'u':
                            m_newMsgHdr->SetFlags(msgFlags & ~MSG_FLAG_READ);
                            break;
                        }
                    }
				}

        // use in-reply-to header as references, if there's no references header
				if (references != nsnull)
					m_newMsgHdr->SetReferences(references->value);
        else if (inReplyTo != nsnull)
          m_newMsgHdr->SetReferences(inReplyTo->value);

				if (date) {
					PRTime resultTime;
					PRStatus timeStatus = PR_ParseTimeString (date->value, PR_FALSE, &resultTime);
					if (PR_SUCCESS == timeStatus)
						m_newMsgHdr->SetDate(nsTime(resultTime));
				}
				if (priority)
					m_newMsgHdr->SetPriorityString(priority->value);
				else if (priorityFlags == nsMsgPriority::notSet)
					m_newMsgHdr->SetPriority(nsMsgPriority::none);
        if (content_type)
        {
          char *substring = PL_strstr(content_type->value, "charset");
          if (substring)
          {
            char *charset = PL_strchr (substring, '=');
            if (charset)
            {
              charset++;
              /* strip leading whitespace and double-quote */
              while (*charset && (IS_SPACE (*charset) || '\"' == *charset))
                charset++;
              /* strip trailing whitespace and double-quote */
              char *end = charset;
              while (*end && !IS_SPACE (*end) && '\"' != *end && ';' != *end)
                end++;
              if (*charset)
              {
                *end = '\0';
                m_newMsgHdr->SetCharset(charset);
              }
            }
          }
        }
			}
		} 
		else
		{
			NS_ASSERTION(PR_FALSE, "error creating message header");
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

nsParseNewMailState::nsParseNewMailState()
    : m_tmpdbName(nsnull), m_usingTempDB(PR_FALSE), m_disableFilters(PR_FALSE)
{
	m_inboxFileStream = nsnull;
	m_logFile = nsnull;
	m_ibuffer = nsnull;
	m_ibuffer_size = 0;
	m_ibuffer_fp = 0;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsParseNewMailState, nsMsgMailboxParser, nsIMsgFilterHitNotify)

nsresult
nsParseNewMailState::Init(nsIFolder *rootFolder, nsIMsgFolder *downloadFolder, nsFileSpec &folder, nsIOFileStream *inboxFileStream)
{
    nsresult rv;
	m_position = folder.GetFileSize();
	m_rootFolder = rootFolder;
	m_inboxFileSpec = folder;
	m_inboxFileStream = inboxFileStream;
	// the new mail parser isn't going to get the stream input, it seems, so we can't use
	// the OnStartRequest mechanism the mailbox parser uses. So, let's open the db right now.
	nsCOMPtr<nsIMsgDatabase> mailDB;
	rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDB));
	if (NS_SUCCEEDED(rv) && mailDB)
	{
		nsCOMPtr <nsIFileSpec> dbFileSpec;
		NS_NewFileSpecWithSpec(folder, getter_AddRefs(dbFileSpec));
		rv = mailDB->OpenFolderDB(downloadFolder, PR_TRUE, PR_FALSE, (nsIMsgDatabase **) getter_AddRefs(m_mailDB));
	}
//	rv = nsMailDatabase::Open(folder, PR_TRUE, &m_mailDB, PR_FALSE);
    if (NS_FAILED(rv)) 
		return rv;

	nsCOMPtr <nsIMsgFolder> rootMsgFolder = do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsIMsgIncomingServer> server;
    rv = rootMsgFolder->GetServer(getter_AddRefs(server));
    if (NS_SUCCEEDED(rv))
        rv = server->GetFilterList(getter_AddRefs(m_filterList));

    m_logFile = nsnull;
#ifdef DOING_MDN
	if (m_filterList)
	{
		const char *folderName = nsnull;
		PRInt32 int_pref = 0;
		PREF_GetIntPref("mail.incorporate.return_receipt", &int_pref);
		if (int_pref == 1)
		{
			nsIMsgFolder *folderInfo = nsnull;
			int status = 0;
			char *defaultFolderName =
				msg_MagicFolderName(master->GetPrefs(),
									MSG_FOLDER_FLAG_SENTMAIL, &status); 
			if (defaultFolderName)
			{
				folderInfo = master->FindMailFolder(defaultFolderName, PR_FALSE);
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
				MSG_Rule *rule = nsnull;
				MSG_SearchValue value;
				newFilter->SetDescription("incorporate mdn report");
				newFilter->SetEnabled(PR_TRUE);
				newFilter->GetRule(&rule);
				newFilter->SetFilterList(m_filterList);
				value.attribute = attribOtherHeader;
				value.u.string = "multipart/report";
				rule->AddTerm(attribOtherHeader, opContains,
							  &value, PR_TRUE, "Content-Type");
				value.u.string = "disposition-notification";
				rule->AddTerm(attribOtherHeader, opContains,
							  &value, PR_TRUE, "Content-Type");
#if 0
				value.u.string = "delivery-status";
				rule->AddTerm(attribOtherHeader, opContains,
							  &value, PR_FALSE, "Content-Type");
#endif
				rule->SetAction(nsMsgFilterActionMoveToFolder, (void*)folderName);
				m_filterList->InsertFilterAt(0, newFilter);
			}
		}
	}
#endif // DOING_MDN
	m_usingTempDB = PR_FALSE;
	m_tmpdbName = nsnull;
	m_disableFilters = PR_FALSE;

    return NS_OK; 
}

nsParseNewMailState::~nsParseNewMailState()
{
	if (m_logFile != nsnull)
	{
		m_logFile->close();
		delete m_logFile;
	}
	if (m_mailDB)
		m_mailDB->Close(PR_TRUE);
//	if (m_usingTempDB)
//	{
//		XP_FileRemove(m_tmpdbName, xpMailFolderSummary);
//	}
	PR_FREEIF(m_tmpdbName);
#ifdef DOING_JSFILTERS
	JSFilter_cleanup();
#endif
}


// This gets called for every message because libnet calls IncorporateBegin,
// IncorporateWrite (once or more), and IncorporateComplete for every message.
void nsParseNewMailState::DoneParsingFolder(nsresult status)
{
	PRBool moved = PR_FALSE;
/* End of file.  Flush out any partial line remaining in the buffer. */
	if (m_ibuffer_fp > 0) 
	{
		ParseFolderLine(m_ibuffer, m_ibuffer_fp);
		m_ibuffer_fp = 0;
	}
	PublishMsgHeader(nsnull);
	if (!moved && m_mailDB)	// finished parsing, so flush db folder info 
		UpdateDBFolderInfo();

#ifdef HAVE_FOLDERINFO
	if (m_folder != nsnull)
		m_folder->SummaryChanged();
#endif

	/* We're done reading the folder - we don't need these things
	 any more. */
	PR_FREEIF (m_ibuffer);
	m_ibuffer_size = 0;
	PR_FREEIF (m_obuffer);
	m_obuffer_size = 0;
}

PRInt32 nsParseNewMailState::PublishMsgHeader(nsIMsgWindow *msgWindow)
{
	PRBool		moved = PR_FALSE;

	FinishHeader();
	
	if (m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_newMsgHdr);
		if (!m_disableFilters)
		{
			// flush the inbox because filters will read from disk
			m_inboxFileStream->flush();
			ApplyFilters(&moved, msgWindow);
		}
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

		}		// if it was moved by imap filter, m_parseMsgState->m_newMsgHdr == nsnull
		m_newMsgHdr = nsnull;
	}
	return 0;
}

void	nsParseNewMailState::SetUsingTempDB(PRBool usingTempDB, char *tmpDBName)
{
	m_usingTempDB = usingTempDB;
	m_tmpdbName = tmpDBName;
}


nsOutputFileStream * nsParseNewMailState::GetLogFile ()
{
	// This log file is used by regular filters and JS filters
	if (m_logFile == nsnull)
	{
    nsCOMPtr<nsIFile> logDir;
    NS_GetSpecialDirectory(NS_APP_MAIL_50_DIR, getter_AddRefs(logDir));

    nsXPIDLCString pathBuf;
    logDir->Append("filter.log");
    nsresult rv = logDir->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return nsnull;
    nsCOMPtr<nsIFileSpec> outSpec;

		nsFileSpec logFile(pathBuf);
		m_logFile = new nsOutputFileStream(logFile, PR_WRONLY | PR_CREATE_FILE, 00600);
	}
	return m_logFile;
}


nsresult nsParseNewMailState::GetTrashFolder(nsIMsgFolder **pTrashFolder)
{
	nsresult rv=NS_ERROR_UNEXPECTED;
	if (!pTrashFolder)
		return NS_ERROR_NULL_POINTER;

	if(m_rootFolder)
	{
		nsCOMPtr <nsIMsgFolder> rootMsgFolder = do_QueryInterface(m_rootFolder);
		if (rootMsgFolder)
		{
			PRUint32 numFolders;
			rv = rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_TRASH, 1, &numFolders, pTrashFolder);
			if (*pTrashFolder)
				NS_ADDREF(*pTrashFolder);
		}
	}
	return rv;
}

void nsParseNewMailState::ApplyFilters(PRBool *pMoved, nsIMsgWindow *msgWindow)
{
	m_msgMovedByFilter = PR_FALSE;

	nsCOMPtr<nsIMsgDBHdr> msgHdr = m_newMsgHdr;
	nsCOMPtr<nsIMsgFolder> inbox;
	nsCOMPtr <nsIMsgFolder> rootMsgFolder = do_QueryInterface(m_rootFolder);
	if (rootMsgFolder)
	{
		PRUint32 numFolders;
		rootMsgFolder->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, 1, &numFolders, getter_AddRefs(inbox));
    if (inbox)
      inbox->GetURI(getter_Copies(m_inboxUri));
		char * headers = m_headers.GetBuffer();
		PRUint32 headersSize = m_headers.GetBufferPos();
		nsresult matchTermStatus;
        if (m_filterList)
            matchTermStatus =
                m_filterList->ApplyFiltersToHdr(nsMsgFilterType::InboxRule,
                                                msgHdr, inbox, m_mailDB,
                                                headers, headersSize, this,
                                                msgWindow);
	}

	if (pMoved)
		*pMoved = m_msgMovedByFilter;
}

NS_IMETHODIMP nsParseNewMailState::ApplyFilterHit(nsIMsgFilter *filter, nsIMsgWindow *msgWindow, PRBool *applyMore)
{
	nsMsgRuleActionType actionType;
    nsXPIDLCString actionTargetFolderUri;
	PRUint32	newFlags;
	nsresult rv = NS_OK;

	if (!applyMore)
	{
		NS_ASSERTION(PR_FALSE, "need to return status!");
		return NS_ERROR_NULL_POINTER;
	}

	*applyMore = PR_TRUE;

	// look at action - currently handle move
#ifdef DEBUG_bienvenu
	printf("got a rule hit!\n");
#endif
	if (NS_SUCCEEDED(filter->GetAction(&actionType)))
	{
        if (actionType == nsMsgFilterAction::MoveToFolder)
        {
            filter->GetActionTargetFolderUri(getter_Copies(actionTargetFolderUri));
            if (!actionTargetFolderUri)
              return rv;
        }
		nsCOMPtr<nsIMsgDBHdr> msgHdr = m_newMsgHdr;
		PRUint32 msgFlags;

		msgHdr->GetFlags(&msgFlags);

		PRBool isRead = (msgFlags & MSG_FLAG_READ);
		switch (actionType)
		{
		case nsMsgFilterAction::Delete :
		{
			nsCOMPtr <nsIMsgFolder> trash;
			// set value to trash folder
			rv = GetTrashFolder(getter_AddRefs(trash));
			if (NS_SUCCEEDED(rv) && trash)
				rv = trash->GetURI(getter_Copies(actionTargetFolderUri));

			msgHdr->OrFlags(MSG_FLAG_READ, &newFlags);	// mark read in trash.
		}
		case nsMsgFilterAction::MoveToFolder:
			// if moving to a different file, do it.
			if ((const char*)actionTargetFolderUri &&
                nsCRT::strcasecmp(m_inboxUri, actionTargetFolderUri))
			{
				msgHdr->GetFlags(&msgFlags);

				if (msgFlags & MSG_FLAG_MDN_REPORT_NEEDED &&
					!isRead)
				{
					struct message_header to;
					struct message_header cc;
					GetAggregateHeader (m_toList, &to);
					GetAggregateHeader (m_ccList, &cc);
					msgHdr->SetFlags(msgFlags & ~MSG_FLAG_MDN_REPORT_NEEDED);
					msgHdr->OrFlags(MSG_FLAG_MDN_REPORT_SENT, &newFlags);

#if DOING_MDN	// leave it to the user aciton
					if (actionType == nsMsgFilterActionDelete)
					{
						MSG_ProcessMdnNeededState processMdnNeeded
							(MSG_ProcessMdnNeededState::eDeleted,
							 m_pane, m_folder, msgHdr->GetMessageKey(),
							 &state->m_return_path, &state->m_mdn_dnt, 
							 &to, &cc, &state->m_subject, 
							 &state->m_date, &state->m_mdn_original_recipient,
							 &state->m_message_id, state->m_headers, 
							 (PRInt32) state->m_headers_fp, PR_TRUE);
					}
					else
					{
						MSG_ProcessMdnNeededState processMdnNeeded
							(MSG_ProcessMdnNeededState::eProcessed,
							 m_pane, m_folder, msgHdr->GetMessageKey(),
							 &state->m_return_path, &state->m_mdn_dnt, 
							 &to, &cc, &state->m_subject, 
							 &state->m_date, &state->m_mdn_original_recipient,
							 &state->m_message_id, state->m_headers, 
							 (PRInt32) state->m_headers_fp, PR_TRUE);
					}
#endif
					char *tmp = (char*) to.value;
					PR_FREEIF(tmp);
					tmp = (char*) cc.value;
					PR_FREEIF(tmp);
				}
				nsresult err = MoveIncorporatedMessage(msgHdr, m_mailDB, (const char *) actionTargetFolderUri, filter, msgWindow);
				if (NS_SUCCEEDED(err))
					m_msgMovedByFilter = PR_TRUE;

			}
      *applyMore = PR_FALSE; 
			break;
		case nsMsgFilterAction::MarkRead:
			MarkFilteredMessageRead(msgHdr);
			break;
		case nsMsgFilterAction::KillThread:
			// for ignore and watch, we will need the db
			// to check for the flags in msgHdr's that
			// get added, because only then will we know
			// the thread they're getting added to.
			msgHdr->OrFlags(MSG_FLAG_IGNORED, &newFlags);
			break;
		case nsMsgFilterAction::WatchThread:
			msgHdr->OrFlags(MSG_FLAG_WATCHED, &newFlags);
			break;
    case nsMsgFilterAction::MarkFlagged:
      msgHdr->MarkFlagged(PR_TRUE);
      break;
		case nsMsgFilterAction::ChangePriority:
            {
                nsMsgPriorityValue filterPriority;
                filter->GetActionPriority(&filterPriority);
                msgHdr->SetPriority(filterPriority);
            }
			break;
		default:
			break;
		}
	PRBool loggingEnabled;
	m_filterList->GetLoggingEnabled(&loggingEnabled);
	if (loggingEnabled && !m_msgMovedByFilter && actionType != nsMsgFilterAction::MoveToFolder)
		filter->LogRuleHit(GetLogFile(), msgHdr);
	}
	return rv;
}

int nsParseNewMailState::MarkFilteredMessageRead(nsIMsgDBHdr *msgHdr)
{
	PRUint32 newFlags;
	if (m_mailDB)
		m_mailDB->MarkHdrRead(msgHdr, PR_TRUE, nsnull);
	else
		msgHdr->OrFlags(MSG_FLAG_READ, &newFlags);
	return 0;
}

nsresult nsParseNewMailState::MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
											   nsIMsgDatabase *sourceDB, 
											   const char *destFolderUri,
                                                      nsIMsgFilter *filter,
                                                      nsIMsgWindow *msgWindow)
{
  nsresult err = 0;
  nsIOFileStream *destFile;
  
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &err)); 
  nsCOMPtr<nsIRDFResource> res;
  err = rdf->GetResource(destFolderUri, getter_AddRefs(res));
  if (NS_FAILED(err))
    return err;
  
  nsCOMPtr<nsIMsgFolder> destIFolder(do_QueryInterface(res, &err));
  if (NS_FAILED(err))
    return err;        
  
  // check if the destination is a real folder (by checking for null parent)
  // and if it can file messages (e.g., servers or news folders can't file messages).
  // Or read only imap folders...
  PRBool canFileMessages = PR_TRUE;
  nsCOMPtr<nsIFolder> parentFolder;
  destIFolder->GetParent(getter_AddRefs(parentFolder));
  destIFolder->GetCanFileMessages(&canFileMessages);
  if (!parentFolder || !canFileMessages)
  {
    filter->SetEnabled(PR_FALSE);
    destIFolder->ThrowAlertMsg("filterDisabled", msgWindow);
    return NS_MSG_NOT_A_MAIL_FOLDER;
  }
  
  nsCOMPtr <nsIFileSpec> destIFolderSpec;
  
  nsFileSpec destFolderSpec;
  destIFolder->GetPath(getter_AddRefs(destIFolderSpec));
  err = destIFolderSpec->GetFileSpec(&destFolderSpec);
  
  if (!NS_SUCCEEDED(err))
    return err;
  
  nsCOMPtr <nsISupports> myISupports = do_QueryInterface(NS_STATIC_CAST(nsIMsgParseMailMsgState*, this));
  
  //	NS_RELEASE(myThis);
  // Make sure no one else is writing into this folder
  if (destIFolder && (err = destIFolder->AcquireSemaphore (myISupports)) != 0)
  {
    destIFolder->ThrowAlertMsg("filterFolderDeniedLocked", msgWindow);
    return err;
  }
  
  NS_ASSERTION(m_inboxFileStream != 0, "no input file stream");
  if (m_inboxFileStream == 0)
  {
#ifdef DEBUG_bienvenu
    NS_ASSERTION(PR_FALSE, "couldn't get source file in move filter");
#endif
    if (destIFolder)
      destIFolder->ReleaseSemaphore (myISupports);
    
    return NS_MSG_FOLDER_UNREADABLE;	// ### dmb
  }
  
  PRUint32 messageOffset = 0;
  
  mailHdr->GetMessageOffset(&messageOffset);
  m_inboxFileStream->seek(PR_SEEK_SET, messageOffset);
  int newMsgPos;
  
  destFile = new nsIOFileStream(destFolderSpec, PR_WRONLY | PR_CREATE_FILE);
  
  if (!destFile) 
  {
#ifdef DEBUG_bienvenu
    NS_ASSERTION(PR_FALSE, "out of memory");
#endif
    if (destIFolder)
      destIFolder->ReleaseSemaphore (myISupports);
    destIFolder->ThrowAlertMsg("filterFolderWriteFailed", msgWindow);
    return  NS_MSG_ERROR_WRITING_MAIL_FOLDER;
  }
  
  destFile->seek(PR_SEEK_END, 0);
  newMsgPos = destFile->tell();
  
  nsCOMPtr<nsIMsgLocalMailFolder> localFolder = do_QueryInterface(destIFolder);
  nsCOMPtr<nsIMsgDatabase> destMailDB;
  
  if (!localFolder)
    return NS_MSG_POP_FILTER_TARGET_ERROR;
  
  nsresult rv = localFolder->GetDatabaseWOReparse(getter_AddRefs(destMailDB));
  NS_ASSERTION(destMailDB, "failed to open mail db parsing folder");
  // don't force upgrade in place - open the db here before we start writing to the 
  // destination file because XP_Stat can return file size including bytes written...
  PRUint32 length;
  mailHdr->GetMessageSize(&length);
  
  if (!m_ibuffer)
    m_ibuffer_size = 10240;
  m_ibuffer_fp = 0;
  
  while (!m_ibuffer && (m_ibuffer_size >= 512))
  {
    m_ibuffer = (char *) PR_Malloc(m_ibuffer_size);
    if (m_ibuffer == nsnull)
      m_ibuffer_size /= 2;
  }
  NS_ASSERTION(m_ibuffer != nsnull, "couldn't get memory to move msg");
  while ((length > 0) && m_ibuffer)
  {
    PRUint32 nRead = m_inboxFileStream->read (m_ibuffer, length > m_ibuffer_size ? m_ibuffer_size  : length);
    if (nRead == 0)
      break;
    
    // we must monitor the number of bytes actually written to the file. (mscott)
    if (destFile->write(m_ibuffer, nRead) != (PRInt32) nRead) 
    {
      destFile->close();     
      
      // truncate  destination file in case message was partially written
      // ### how to do this with a stream?
      destFolderSpec.Truncate(newMsgPos);
      
      if (destIFolder)
        destIFolder->ReleaseSemaphore(myISupports);
      
      if (destMailDB)
        destMailDB->Close(PR_TRUE);
      destIFolder->ThrowAlertMsg("filterFolderWriteFailed", msgWindow);
      return NS_MSG_ERROR_WRITING_MAIL_FOLDER;   // caller (ApplyFilters) currently ignores error conditions
    }
    
    length -= nRead;
  }
  
  NS_ASSERTION(length == 0, "didn't read all of original message in filter move");
  
  // if we have made it this far then the message has successfully been written to the new folder
  // now add the header to the destMailDB.
  if (NS_SUCCEEDED(rv) && destMailDB)
  {
    nsCOMPtr <nsIMsgDBHdr> newHdr;
    
    nsresult msgErr = destMailDB->CopyHdrFromExistingHdr(newMsgPos, mailHdr, PR_TRUE, getter_AddRefs(newHdr));
    if (NS_SUCCEEDED(msgErr) && newHdr)
    {
      PRUint32 newFlags;
      // set new byte offset, since the offset in the old file is certainly wrong
      newHdr->SetMessageKey (newMsgPos); 
      newHdr->OrFlags(MSG_FLAG_NEW, &newFlags);
      destMailDB->AddToNewList(newMsgPos);
    }
  }
  else
  {
    if (destMailDB)
      destMailDB = nsnull;
  }
  destIFolder->SetHasNewMessages(PR_TRUE);
  destFile->close();
  delete destFile;
  m_inboxFileStream->close();
  // How are we going to do this with a stream?
  nsresult truncRet = m_inboxFileSpec.Truncate(messageOffset);
  NS_ASSERTION(NS_SUCCEEDED(truncRet), "unable to truncate file");
  
  //  need to re-open the inbox file stream.
  m_inboxFileStream->Open(m_inboxFileSpec, (PR_RDWR | PR_CREATE_FILE));
  if (m_inboxFileStream)
    m_inboxFileStream->seek(m_inboxFileSpec.GetFileSize());
  
  if (destIFolder)
    destIFolder->ReleaseSemaphore (myISupports);
  
  // tell parser that we've truncated the Inbox
  mailHdr->GetMessageOffset(&messageOffset);
  nsParseMailMessageState::Init(messageOffset);
  
  if (destIFolder)
    destIFolder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);
  
  if (destMailDB != nsnull)
  {
    // update the folder size so we won't reparse.
    UpdateDBFolderInfo(destMailDB);
    if (destIFolder != nsnull)
      destIFolder->SummaryChanged();
    
    destMailDB->Commit(nsMsgDBCommitType::kLargeCommit);
  }
  // We are logging the hit with the old mailHdr, which should work, as long
  // as LogRuleHit doesn't assume the new hdr.
  PRBool loggingEnabled;
  m_filterList->GetLoggingEnabled(&loggingEnabled);
  if (loggingEnabled)
    filter->LogRuleHit(GetLogFile(), mailHdr);
  
  // cleanup after mailHdr in source DB because we moved the message.
  m_mailDB->RemoveHeaderMdbRow(mailHdr);
  
  return err;

}

#ifdef IMAP_NEW_MAIL_HANDLED

ParseIMAPMailboxState::ParseIMAPMailboxState(MSG_IMAPHost *host, nsIMsgFolderMail *folder,
											 MSG_UrlQueue *urlQueue, TImapFlagAndUidState *flagStateAdopted)
											: nsParseNewMailState(master, folder), fUrlQueue(urlQueue)
{
 	MSG_FolderInfoContainer *imapContainer =  m_mailMaster->GetImapMailFolderTreeForHost(host->GetHostName());
 	nsIMsgFolder *filteredFolder = imapContainer->FindMailPathname(folder->GetPathname());
 	fParsingInbox = 0 != (filteredFolder->GetFlags() & MSG_FOLDER_FLAG_INBOX);
 	fFlagState = flagStateAdopted;
 	fB2HaveWarnedUserOfOfflineFiltertarget = PR_FALSE;
 	
 	// we ignore X-mozilla status for imap messages
 	GetMsgState()->m_IgnoreXMozillaStatus = PR_TRUE;
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

void ParseIMAPMailboxState::DoneParsingFolder(nsresult status)
{
	nsMsgMailboxParser::DoneParsingFolder(status);
	if (m_mailDB)
	{
		// make sure the highwater mark is correct
		if (m_mailDB->m_dbFolderInfo->GetNumVisibleMessages())
		{
			ListContext *listContext;
			nsIMsgDBHdr *currentHdr;
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
		m_mailDB = nsnull;
	}
}

int ParseIMAPMailboxState::MarkFilteredMessageRead(nsIMsgDBHdr *msgHdr)
{
	msgHdr->OrFlags(MSG_FLAG_READ);
	nsMsgKeyArray	keysToFlag;

	keysToFlag.Add(msgHdr->GetMessageKey());
	MSG_IMAPFolderInfoMail *imapFolder = m_folder->GetIMAPFolderInfoMail();
	if (imapFolder)
		imapFolder->StoreImapFlags(m_pane, kImapMsgSeenFlag, PR_TRUE, keysToFlag, GetFilterUrlQueue());

	return 0;
}


nsresult ParseIMAPMailboxState::MoveIncorporatedMessage(nsIMsgDBHdr *mailHdr, 
											   nsIMsgDatabase *sourceDB, 
											   char *destFolder,
											   nsIMsgFilter *filter)
{
	nsresult err = NS_OK;
	
	if (fUrlQueue && fUrlQueue->GetPane())
	{
	 	// look for matching imap folders, then pop folders
	 	MSG_FolderInfoContainer *imapContainer =  m_imapContainer;
		nsIMsgFolder *sourceFolder = imapContainer->FindMailPathname(m_mailboxName);
	 	nsIMsgFolder *destinationFolder = imapContainer->FindMailPathname(destFolder);
	 	if (!destinationFolder)
	 		destinationFolder = m_mailMaster->FindMailFolder(destFolder, PR_FALSE);

	 	if (destinationFolder)
	 	{
			nsCOMPtr<nsIMsgFolder> inbox;
			imapContainer->GetFoldersWithFlag(MSG_FOLDER_FLAG_INBOX, getter_AddRefs(inbox), 1);
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
					if (m_filterList->LoggingEnabled())
						filter->LogRuleHit(GetLogFile(), mailHdr);

					if (imapDeleteIsMoveToTrash)
					{
						if (m_parseMsgState->m_newMsgHdr)
						{
							m_parseMsgState->m_newMsgHdr->Release();
							m_parseMsgState->m_newMsgHdr = nsnull;
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
		return nsnull;

	nsIMsgFolder *foundTrash = MSG_GetTrashFolderForHost(imapContainerInfo->GetIMAPHost());
	return foundTrash ? foundTrash->GetMailFolderInfo() : (MSG_FolderInfoMail *)nsnull;
}


// only apply filters for new unread messages in the imap inbox
void ParseIMAPMailboxState::ApplyFilters(PRBool *pMoved, nsIMsgWindow *msgWindow)
{
 	if (fParsingInbox && !(GetCurrentMsg()->GetFlags() & MSG_FLAG_READ) )
 		nsParseNewMailState::ApplyFilters(pMoved, msgWindow);
 	else
 		*pMoved = PR_FALSE;
 	
 	if (!*pMoved && m_parseMsgState->m_newMsgHdr)
 		fFetchBodyKeys.Add(m_parseMsgState->m_newMsgHdr->GetMessageKey());
}


PRInt32	ParseIMAPMailboxState::PublishMsgHeader(nsIMsgWindow *msgWindow)
{
	PRBool		moved = PR_FALSE;

	m_parseMsgState->FinishHeader();
	
	if (m_parseMsgState->m_newMsgHdr)
	{
		FolderTypeSpecificTweakMsgHeader(m_parseMsgState->m_newMsgHdr);
		if (!m_disableFilters) {
			ApplyFilters(&moved, msgWindow);
		}
		if (!moved)
		{
			PRBool thisMessageUnRead = !(m_parseMsgState->m_newMsgHdr->GetFlags() & MSG_FLAG_READ);
			if (m_mailDB)
			{
				if (thisMessageUnRead)
					m_parseMsgState->m_newMsgHdr->OrFlags(kNew);
				m_mailDB->AddHdrToDB (m_parseMsgState->m_newMsgHdr, nsnull,
					(fNextSequenceNum == -1) ? m_updateAsWeGo : PR_FALSE);
				// following is for cacheless imap - match sequence number
				// to location to insert in view.
			}
			if (m_folder && thisMessageUnRead)
				m_folder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);


		}		// if it was moved by imap filter, m_parseMsgState->m_newMsgHdr == nsnull
		else if (m_parseMsgState->m_newMsgHdr)
		{
			// make sure global db is set correctly for delete of strings from hash tbl.
			m_parseMsgState->m_newMsgHdr->unrefer();
		}
		m_parseMsgState->m_newMsgHdr = nsnull;
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
	m_wroteXMozillaStatus = PR_FALSE;
	m_writeToOutFile = PR_TRUE;
	m_lastBodyLineEmpty = PR_FALSE;
	m_outputBuffer = 0;
	m_ouputBufferSize = 0;
	m_outputBufferIndex = 0;
	m_writeMozillaStatus = PR_TRUE;
}

ParseOutgoingMessage::~ParseOutgoingMessage()
{
	FREEIF (m_outputBuffer);
}

void ParseOutgoingMessage::Clear()
{
	nsParseMailMessageState::Clear();
	m_wroteXMozillaStatus = PR_FALSE;
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
			len = XP_FileWrite (MSG_LINEBREAK, MSG_LINEBREAK_LEN, m_out_file);
			m_bytes_written += MSG_LINEBREAK_LEN;
			m_position += MSG_LINEBREAK_LEN;
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
			m_wroteXMozillaStatus = PR_TRUE;

		m_lastBodyLineEmpty = (m_state == nsIMsgParseMailMsgState::ParseBodyState && (EMPTY_MESSAGE_LINE(line)));

		// make sure we mangle naked From lines
		if (line[0] == 'F' && !nsCRT::strncmp(line, "From ", 5))
		{
			res = XP_FileWrite (">", 1, m_out_file);
			if (res < 1)
				return res;
			m_position += 1;
		}
		if (!m_wroteXMozillaStatus && m_writeMozillaStatus && m_state == nsIMsgParseMailMsgState::ParseBodyState)
		{
			char buf[50];
			PRUint32 dbFlags = m_newMsgHdr ? m_newMsgHdr->GetFlags() : 0;

			if (m_newMsgHdr)
				m_newMsgHdr->SetStatusOffset(m_bytes_written);
			PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS_FORMAT MSG_LINEBREAK, (m_newMsgHdr) ? m_newMsgHdr->GetFlags() & ~MSG_FLAG_RUNTIME_ONLY : 0);
			len = strlen(buf);
			res = XP_FileWrite(buf, len, m_out_file);
			if (res < len)
				return res;
			m_bytes_written += len;
			m_position += len;
			m_wroteXMozillaStatus = PR_TRUE;

			MessageDB::ConvertDBFlagsToPublicFlags(&dbFlags);
			dbFlags &= (MSG_FLAG_MDN_REPORT_NEEDED | MSG_FLAG_MDN_REPORT_SENT | MSG_FLAG_TEMPLATE);
			PR_snprintf(buf, sizeof(buf), X_MOZILLA_STATUS2_FORMAT MSG_LINEBREAK, dbFlags);
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
		
		m_outputBuffer = (char *) PR_Malloc(m_ouputBufferSize);
		if (m_outputBuffer == nsnull)
			m_ouputBufferSize /= 2;
	}
	PR_ASSERT(m_outputBuffer != nsnull);

	return msg_LineBuffer (block, length, &m_outputBuffer, &m_ouputBufferSize,  &m_outputBufferIndex, PR_FALSE,
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
