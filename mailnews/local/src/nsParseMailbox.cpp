/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   David Bienvenu <bienvenu@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsILocalFile.h"
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
#include "nsIRDFService.h"
#include "nsMsgI18N.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsMsgUtils.h"
#include "prprf.h"
#include "nsEscape.h"
#include "nsIMimeHeaders.h"
#include "nsIMsgMdnGenerator.h"
#include "nsMsgSearchCore.h"
#include "nsMailHeaders.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

/* the following macros actually implement addref, release and query interface for our component. */
NS_IMPL_ISUPPORTS_INHERITED2(nsMsgMailboxParser, nsParseMailMessageState, nsIStreamListener, nsIDBChangeListener)

// Whenever data arrives from the connection, core netlib notifices the protocol by calling
// OnDataAvailable. We then read and process the incoming data from the input stream. 
NS_IMETHODIMP nsMsgMailboxParser::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *aIStream, PRUint32 sourceOffset, PRUint32 aLength)
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
    nsCOMPtr<nsIMsgFolder> folder = do_QueryReferent(m_folder);

    if (NS_SUCCEEDED(rv) && runningUrl && folder)
    {
        url->GetStatusFeedback(getter_AddRefs(m_statusFeedback));

        // okay, now fill in our event sinks...Note that each getter ref counts before
        // it returns the interface to us...we'll release when we are done

        folder->GetName(getter_Copies(m_folderName));

        nsCOMPtr<nsIFileSpec> path;
        folder->GetPath(getter_AddRefs(path));
       
        if (path)
        {
            nsFileSpec dbName;
            path->GetFileSpec(&dbName);
            // the size of the mailbox file is our total base line for measuring progress
            m_graph_progress_total = dbName.GetFileSize();
            UpdateStatusText(LOCAL_STATUS_SELECTING_MAILBOX);

            nsCOMPtr<nsIMsgDatabase> mailDB;
            rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, NS_GET_IID(nsIMsgDatabase), (void **) getter_AddRefs(mailDB));
            if (NS_SUCCEEDED(rv) && mailDB)
            {
                //Use OpenFolderDB to always open the db so that db's m_folder is set correctly.
                rv = mailDB->OpenFolderDB(folder, PR_TRUE, PR_TRUE, (nsIMsgDatabase **) getter_AddRefs(m_mailDB));
                if (m_mailDB)
                    m_mailDB->AddListener(this);
            }
            NS_ASSERTION(m_mailDB, "failed to open mail db parsing folder");
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

    ReleaseFolderLock();
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
  if (m_mailDB)
    m_mailDB->RemoveListener(this);

  m_newMsgHdr = nsnull;
  m_mailDB = nsnull;
  return NS_OK;
}

/* void OnReadChanged (in nsIDBChangeListener instigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnReadChanged(nsIDBChangeListener *instigator)
{
    return NS_OK;
}

/* void OnJunkScoreChanged (in nsIDBChangeListener instigator); */
NS_IMETHODIMP nsMsgMailboxParser::OnJunkScoreChanged(nsIDBChangeListener *instigator)
{
    return NS_OK;
}

nsMsgMailboxParser::nsMsgMailboxParser() : nsMsgLineBuffer(nsnull, PR_FALSE)
{
  Init();
}

nsMsgMailboxParser::nsMsgMailboxParser(nsIMsgFolder *aFolder) : nsMsgLineBuffer(nsnull, PR_FALSE)
{
  Init();
  m_folder = do_GetWeakReference(aFolder);
}

nsMsgMailboxParser::~nsMsgMailboxParser()
{
  ReleaseFolderLock();
}

void nsMsgMailboxParser::Init()
{
	m_obuffer = nsnull;
	m_obuffer_size = 0;
	m_graph_progress_total = 0;
	m_graph_progress_received = 0;
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
    // prevent overflow by dividing both by 100
    PRUint32 progressTotal = m_graph_progress_total / 100;
    PRUint32 progressReceived = m_graph_progress_received / 100;
    if (progressTotal > 0)
      m_statusFeedback->ShowProgress((100 *(progressReceived))  / progressTotal);	
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
	mailDB->SetSummaryValid(PR_TRUE);
	mailDB->Commit(nsMsgDBCommitType::kLargeCommit);
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
      // add hdr but don't notify - shouldn't be requiring notifications 
      // during summary file rebuilding
      m_mailDB->AddNewHdrToDB(m_newMsgHdr, PR_FALSE);
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
        else
          return NS_ERROR_NULL_POINTER; // need to error out if we don't have a db.

	return 0;

}

void
nsMsgMailboxParser::ReleaseFolderLock()
{
  nsresult result;
  nsCOMPtr<nsIMsgFolder> folder = do_QueryReferent(m_folder);
  if (!folder) 
    return;
  PRBool haveSemaphore;
  nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIMsgParseMailMsgState*, this));
  result = folder->TestSemaphore(supports, &haveSemaphore);
  if(NS_SUCCEEDED(result) && haveSemaphore)
    result = folder->ReleaseSemaphore(supports);
  return;
}

NS_IMPL_ISUPPORTS1(nsParseMailMessageState, nsIMsgParseMailMsgState)

nsParseMailMessageState::nsParseMailMessageState()
{
  m_position = 0;
  m_IgnoreXMozillaStatus = PR_FALSE;
  m_state = nsIMsgParseMailMsgState::ParseBodyState;
  Clear();

  m_HeaderAddressParser = do_GetService(NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID);
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
  m_account_key.length = 0;
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
    NS_IF_ADDREF(*aMsgHeader = m_newMsgHdr);

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
  m_mailDB = mailDB;
  return NS_OK;
}

NS_IMETHODIMP nsParseMailMessageState::SetDBFolderStream(nsIOFileStream *fileStream)
{
  NS_ASSERTION(m_mailDB, "m_mailDB is not set");
  if (m_mailDB)
    m_mailDB->SetFolderStream(fileStream);
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
  if (strncmp(buf, "From ", 5)) return PR_FALSE;

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
  if (strncmp(buf, "From ", 5)) return PR_FALSE;

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

// generate headers as a string, with CRLF between the headers
NS_IMETHODIMP nsParseMailMessageState::GetHeaders(char ** pHeaders)
{
  NS_ENSURE_ARG_POINTER(pHeaders);
  nsCString crlfHeaders;
  char *curHeader = m_headers.GetBuffer();
  for (PRUint32 headerPos = 0; headerPos < m_headers.GetBufferPos();)
  {
    crlfHeaders.Append(curHeader);
    crlfHeaders.Append(CRLF);
    PRInt32 headerLen = strlen(curHeader);
    curHeader += headerLen + 1;
    headerPos += headerLen + 1;
  }
  *pHeaders = nsCRT::strdup(crlfHeaders.get());
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
    NS_ASSERTION(header->length == (PRInt32)strlen(header->value), "header corrupted");
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
      else if (!nsCRT::strncasecmp(HEADER_X_MOZILLA_ACCOUNT_KEY, buf, end - buf))
        header = &m_account_key;
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
  
  NS_ASSERTION (header->length == (short) strlen(header->value), "subject corrupt while parsing message");
  
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
  nsXPIDLCString modifiedSubject;
  if (NS_MsgStripRE((const char **) &key, &L, getter_Copies(modifiedSubject)))
    flags |= MSG_FLAG_HAS_RE;
  else
    flags &= ~MSG_FLAG_HAS_RE;
  m_newMsgHdr->SetFlags(flags); // this *does not* update the mozilla-status header in the local folder
  
  //  if (!*key) return 0; /* To catch a subject of "Re:" */
  
  // Condense the subject text into as few MIME-2 encoded words as possible.
#ifdef WE_CONDENSE_MIME_STRINGS
  char *condensedKey = msg_condense_mime2_string(modifiedSubject.IsEmpty() ? key : modifiedSubject.get());
#else
  char *condensedKey = nsnull;
#endif
  m_newMsgHdr->SetSubject(condensedKey ? condensedKey : 
  (modifiedSubject.IsEmpty() ? key : modifiedSubject.get()));
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
  
  NS_ASSERTION (header->length == (short) strlen (header->value), "invalid message_header");
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
  struct message_header *account_key;
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
  PRUint32 labelFlags = 0;
  
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
  account_key = (m_account_key.length ? &m_account_key :0);
  
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
          !(origFlags & MSG_FLAG_MDN_REPORT_SENT) &&
          !(flags & MSG_FLAG_MDN_REPORT_SENT))
        flags |= MSG_FLAG_MDN_REPORT_NEEDED;
      
      m_newMsgHdr->SetFlags(flags);
      if (priorityFlags != nsMsgPriority::notSet)
        m_newMsgHdr->SetPriority(priorityFlags);
  
      // convert the flag values (0xE000000) to label values (0-5)
      if (mozstatus2) // only do this if we have a mozstatus2 header
      {
        labelFlags = ((flags & MSG_FLAG_LABELS) >> 25);
        m_newMsgHdr->SetLabel(labelFlags);
      }
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
        NS_ASSERTION (recipient->length == (PRUint16) strlen(recipient->value), "invalid recipient");
        ch = PL_strchr(recipient->value, ',');
        if (ch)
        {
          /* generate a new string that terminates before the , */
          nsCAutoString firstGroup;
          firstGroup.Assign(recipient->value, ch - recipient->value);
          m_newMsgHdr->SetRecipients(firstGroup.get());
        }
        m_newMsgHdr->SetRecipients(recipient->value);
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
          PR_Free(addresses);
          PR_Free(names);
        }
        else {	// hmm, should we just use the original string?
          m_newMsgHdr->SetRecipients(recipient->value);
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
          PR_Free(addresses);
          PR_Free(names);
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
          memcpy(md5_bin, "dummy message id", sizeof(md5_bin));						
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
          md5_header.length = strlen(md5_data);
          id = &md5_header;
        }
        
        /* Take off <> around message ID. */
        if (id->value[0] == '<')
          id->value++, id->length--;

        if (id->value[id->length-1] == '>') {
          /* generate a new null-terminated string without the final > */
          nsCAutoString rawMsgId;
          rawMsgId.Assign(id->value, id->length - 1);
          m_newMsgHdr->SetMessageId(rawMsgId.get());
        } else {
          m_newMsgHdr->SetMessageId(id->value);
        }

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
  
        if (account_key != nsnull)
          m_newMsgHdr->SetAccountKey(account_key->value);
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
                if (*end != '\0') {
                  // if we're not at the very end of the line, we need
                  // to generate a new string without the trailing crud
                  nsCAutoString rawCharSet;
                  rawCharSet.Assign(charset, end - charset);
                  m_newMsgHdr->SetCharset(rawCharSet.get());
                } else {
                  m_newMsgHdr->SetCharset(charset);
                }
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
  PR_Free(tmp);
  tmp = (char*) cc.value;
  PR_Free(tmp);

  return status;
}

nsParseNewMailState::nsParseNewMailState()
    : m_disableFilters(PR_FALSE)
{
  m_inboxFileStream = nsnull;
  m_ibuffer = nsnull;
  m_ibuffer_size = 0;
  m_ibuffer_fp = 0;
 }

NS_IMPL_ISUPPORTS_INHERITED1(nsParseNewMailState, nsMsgMailboxParser, nsIMsgFilterHitNotify)

nsresult
nsParseNewMailState::Init(nsIMsgFolder *rootFolder, nsIMsgFolder *downloadFolder, nsFileSpec &folder, nsIOFileStream *inboxFileStream, nsIMsgWindow *aMsgWindow)
{
  nsresult rv;
  m_position = folder.GetFileSize();
  m_rootFolder = rootFolder;
  m_inboxFileSpec = folder;
  m_inboxFileStream = inboxFileStream;
  m_msgWindow = aMsgWindow;
  m_downloadFolder = downloadFolder;

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
  {
    rv = server->GetFilterList(aMsgWindow, getter_AddRefs(m_filterList));
  
    if (m_filterList) 
    {
      rv = server->ConfigureTemporaryFilters(m_filterList);
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to configure temp filters");
    }
  }
  m_disableFilters = PR_FALSE;
  return NS_OK; 
}

nsParseNewMailState::~nsParseNewMailState()
{
  if (m_mailDB)
    m_mailDB->Close(PR_TRUE);
#ifdef DOING_JSFILTERS
  JSFilter_cleanup();
#endif
}

// not an IMETHOD so we don't need to do error checking or return an error.
// We only have one caller.
void nsParseNewMailState::GetMsgWindow(nsIMsgWindow **aMsgWindow)
{
  NS_IF_ADDREF(*aMsgWindow = m_msgWindow);
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
  
    /* We're done reading the folder - we don't need these things
	 any more. */
  PR_FREEIF (m_ibuffer);
  m_ibuffer_size = 0;
  PR_FREEIF (m_obuffer);
  m_obuffer_size = 0;
}

PRInt32 nsParseNewMailState::PublishMsgHeader(nsIMsgWindow *msgWindow)
{
  PRBool moved = PR_FALSE;
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
        PRUint32 newFlags, oldFlags;
        m_newMsgHdr->GetFlags(&oldFlags);
        if (!(oldFlags & MSG_FLAG_READ)) // don't mark read messages as new.
          m_newMsgHdr->OrFlags(MSG_FLAG_NEW, &newFlags);
        
        m_mailDB->AddNewHdrToDB(m_newMsgHdr, PR_TRUE);
      }
    }		// if it was moved by imap filter, m_parseMsgState->m_newMsgHdr == nsnull
    m_newMsgHdr = nsnull;
  }
  return 0;
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
      matchTermStatus = m_filterList->ApplyFiltersToHdr(nsMsgFilterType::InboxRule,
                  msgHdr, inbox, m_mailDB, headers, headersSize, this, msgWindow);
  }
  
  if (pMoved)
    *pMoved = m_msgMovedByFilter;
}

NS_IMETHODIMP nsParseNewMailState::ApplyFilterHit(nsIMsgFilter *filter, nsIMsgWindow *msgWindow, PRBool *applyMore)
{
  NS_ENSURE_ARG_POINTER(applyMore);
  
  nsMsgRuleActionType actionType;
  nsXPIDLCString actionTargetFolderUri;
  PRUint32	newFlags;
  nsresult rv = NS_OK;
  
  *applyMore = PR_TRUE;
  
  nsCOMPtr<nsIMsgDBHdr> msgHdr = m_newMsgHdr;
  
  nsCOMPtr<nsISupportsArray> filterActionList;
  rv = NS_NewISupportsArray(getter_AddRefs(filterActionList));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = filter->GetSortedActionList(filterActionList);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRUint32 numActions;
  rv = filterActionList->Count(&numActions);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool loggingEnabled = PR_FALSE;
  if (m_filterList && numActions)
    m_filterList->GetLoggingEnabled(&loggingEnabled);
  
  PRBool msgIsNew = PR_TRUE;
  for (PRUint32 actionIndex =0; actionIndex < numActions && *applyMore; actionIndex++)
  {
    nsCOMPtr<nsIMsgRuleAction> filterAction;
    filterActionList->QueryElementAt(actionIndex, NS_GET_IID(nsIMsgRuleAction), getter_AddRefs(filterAction));
    if (!filterAction)
      continue;
    
    if (NS_SUCCEEDED(filterAction->GetType(&actionType)))
    {
      if (actionType == nsMsgFilterAction::MoveToFolder)
      {
        filterAction->GetTargetFolderUri(getter_Copies(actionTargetFolderUri));
        if (actionTargetFolderUri.IsEmpty())
        {
          NS_ASSERTION(PR_FALSE, "actionTargetFolderUri is empty");
          continue;
        }
      }
      switch (actionType)
      {
      case nsMsgFilterAction::Delete:
        {
          nsCOMPtr <nsIMsgFolder> trash;
          // set value to trash folder
          rv = GetTrashFolder(getter_AddRefs(trash));
          if (NS_SUCCEEDED(rv) && trash)
            rv = trash->GetURI(getter_Copies(actionTargetFolderUri));
          
          msgHdr->OrFlags(MSG_FLAG_READ, &newFlags);	// mark read in trash.
          msgIsNew = PR_FALSE;
        }
      case nsMsgFilterAction::MoveToFolder:
        // if moving to a different file, do it.
        if ((const char*)actionTargetFolderUri &&
          nsCRT::strcasecmp(m_inboxUri, actionTargetFolderUri))
        {
          nsresult err = MoveIncorporatedMessage(msgHdr, m_mailDB, actionTargetFolderUri, filter, msgWindow);
          if (NS_SUCCEEDED(err))
          {
            if (loggingEnabled)
              (void)filter->LogRuleHit(filterAction, msgHdr); 
            
            // cleanup after mailHdr in source DB because we moved the message.
            m_mailDB->RemoveHeaderMdbRow(msgHdr);
            m_msgMovedByFilter = PR_TRUE;
          }
        }
        *applyMore = PR_FALSE; 
        break;
      case nsMsgFilterAction::MarkRead:
        msgIsNew = PR_FALSE;
        MarkFilteredMessageRead(msgHdr);
        break;
      case nsMsgFilterAction::KillThread:
        // The db will check for this flag when a hdr gets added to the db, and set the flag appropriately on the thread object
        msgHdr->OrFlags(MSG_FLAG_IGNORED, &newFlags);
        break;
      case nsMsgFilterAction::WatchThread:
        msgHdr->OrFlags(MSG_FLAG_WATCHED, &newFlags);
        break;
      case nsMsgFilterAction::MarkFlagged:
        msgHdr->MarkFlagged(PR_TRUE);
        break;
      case nsMsgFilterAction::ChangePriority:
        nsMsgPriorityValue filterPriority;
        filterAction->GetPriority(&filterPriority);
        msgHdr->SetPriority(filterPriority);
        break;
      case nsMsgFilterAction::Label:
        nsMsgLabelValue filterLabel;
        filterAction->GetLabel(&filterLabel);
        nsMsgKey msgKey;
        msgHdr->GetMessageKey(&msgKey);
        m_mailDB->SetLabel(msgKey, filterLabel);
        break;
      case nsMsgFilterAction::JunkScore:
      {
        nsCAutoString junkScoreStr;
        PRInt32 junkScore;
        filterAction->GetJunkScore(&junkScore);
        junkScoreStr.AppendInt(junkScore);
        nsMsgKey msgKey;
        msgHdr->GetMessageKey(&msgKey);
        m_mailDB->SetStringProperty(msgKey, "junkscore", junkScoreStr.get());
        m_mailDB->SetStringProperty(msgKey, "junkscoreorigin", /* ### should this be plugin? */"plugin");
      }
      case nsMsgFilterAction::DeleteFromPop3Server:
        {
          PRUint32 flags = 0;
          nsCOMPtr <nsIMsgFolder> downloadFolder;
          msgHdr->GetFolder(getter_AddRefs(downloadFolder));
          nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(downloadFolder);
          msgHdr->GetFlags(&flags);
          if (localFolder)
          {
            nsCOMPtr<nsISupportsArray> messages;
            rv = NS_NewISupportsArray(getter_AddRefs(messages));
            NS_ENSURE_SUCCESS(rv, rv);
            nsCOMPtr<nsISupports> iSupports = do_QueryInterface(msgHdr);
            messages->AppendElement(iSupports);
            // This action ignores the deleteMailLeftOnServer preference
            localFolder->MarkMsgsOnPop3Server(messages, POP3_FORCE_DEL);

            // If this is just a header, throw it away. It's useless now
            // that the server copy is being deleted.
            if (flags & MSG_FLAG_PARTIAL)
            {
              m_msgMovedByFilter = PR_TRUE;
              msgIsNew = PR_FALSE;
            }
          }
        }
        break;
      case nsMsgFilterAction::FetchBodyFromPop3Server:
        {
	  PRUint32 flags = 0;
          nsCOMPtr <nsIMsgFolder> downloadFolder;
          msgHdr->GetFolder(getter_AddRefs(downloadFolder));
          nsCOMPtr <nsIMsgLocalMailFolder> localFolder = do_QueryInterface(downloadFolder);
          msgHdr->GetFlags(&flags);
          if (localFolder && (flags & MSG_FLAG_PARTIAL))
          {
            nsCOMPtr<nsISupportsArray> messages;
            rv = NS_NewISupportsArray(getter_AddRefs(messages));
            NS_ENSURE_SUCCESS(rv, rv);
            nsCOMPtr<nsISupports> iSupports = do_QueryInterface(msgHdr);
            messages->AppendElement(iSupports);
            localFolder->MarkMsgsOnPop3Server(messages, POP3_FETCH_BODY);
	    // Don't add this header to the DB, we're going to replace it
	    // with the full message.
            m_msgMovedByFilter = PR_TRUE;
            msgIsNew = PR_FALSE;
	    // Don't do anything else in this filter, wait until we
	    // have the full message.
	    *applyMore = PR_FALSE;
          }
        }
        break;
      default:
        break;
      }
      if (loggingEnabled && actionType != nsMsgFilterAction::MoveToFolder && actionType != nsMsgFilterAction::Delete)  
        (void)filter->LogRuleHit(filterAction, msgHdr); 
    }
  }
  if (!msgIsNew)
  {
    PRInt32 numNewMessages;
    m_downloadFolder->GetNumNewMessages(PR_FALSE, &numNewMessages);
    m_downloadFolder->SetNumNewMessages(numNewMessages - 1);
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
                                                      const nsACString& destFolderUri,
                                                      nsIMsgFilter *filter,
                                                      nsIMsgWindow *msgWindow)
{
  nsresult err = 0;
  nsIOFileStream *destFile;
  
  nsCOMPtr<nsIRDFService> rdf(do_GetService(kRDFServiceCID, &err)); 
  NS_ENSURE_SUCCESS(err, err);
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
  nsCOMPtr<nsIMsgFolder> parentFolder;
  destIFolder->GetParent(getter_AddRefs(parentFolder));
  if (parentFolder)
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
  
  if (NS_FAILED(err))
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
  
  PRBool movedMsgIsNew = PR_TRUE;
  // if we have made it this far then the message has successfully been written to the new folder
  // now add the header to the destMailDB.
  if (NS_SUCCEEDED(rv) && destMailDB)
  {
    nsCOMPtr <nsIMsgDBHdr> newHdr;
    
    nsresult msgErr = destMailDB->CopyHdrFromExistingHdr(newMsgPos, mailHdr, PR_FALSE, getter_AddRefs(newHdr));
    if (NS_SUCCEEDED(msgErr) && newHdr)
    {
      PRUint32 newFlags;
      // set new byte offset, since the offset in the old file is certainly wrong
      newHdr->SetMessageKey (newMsgPos); 
      newHdr->GetFlags(&newFlags);
      if (! (newFlags & MSG_FLAG_READ))
      {
        newHdr->OrFlags(MSG_FLAG_NEW, &newFlags);
        destMailDB->AddToNewList(newMsgPos);
      }
      else
      {
        movedMsgIsNew = PR_FALSE;
      }
      destMailDB->AddNewHdrToDB(newHdr, PR_TRUE);
    }
  }
  else
  {
    if (destMailDB)
      destMailDB = nsnull;
  }
  if (movedMsgIsNew)
    destIFolder->SetHasNewMessages(PR_TRUE);
  destFile->close();
  delete destFile;
  m_inboxFileStream->close();
  // How are we going to do this with a stream?
  nsresult truncRet = m_inboxFileSpec.Truncate(messageOffset);
  NS_ASSERTION(NS_SUCCEEDED(truncRet), "unable to truncate file");
  if (NS_FAILED(truncRet))
   destIFolder->ThrowAlertMsg("filterFolderTruncateFailed", msgWindow);

  //  need to re-open the inbox file stream.
  m_inboxFileStream->Open(m_inboxFileSpec, (PR_RDWR | PR_CREATE_FILE));
  if (m_inboxFileStream)
    m_inboxFileStream->seek(m_inboxFileSpec.GetFileSize());
  
  if (destIFolder)
    destIFolder->ReleaseSemaphore (myISupports);
  
  // tell parser that we've truncated the Inbox
  mailHdr->GetMessageOffset(&messageOffset);
  nsParseMailMessageState::Init(messageOffset);
  
  (void) localFolder->RefreshSizeOnDisk();
  if (destIFolder)
    destIFolder->SetFlag(MSG_FOLDER_FLAG_GOT_NEW);
  
  if (destMailDB != nsnull)
  {
    // update the folder size so we won't reparse.
    UpdateDBFolderInfo(destMailDB);
    if (destIFolder != nsnull)
      destIFolder->UpdateSummaryTotals(PR_TRUE);
    
    destMailDB->Commit(nsMsgDBCommitType::kLargeCommit);
  }  
  return err;
}

