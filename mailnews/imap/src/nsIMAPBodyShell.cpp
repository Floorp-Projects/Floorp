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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsImapHostSessionList.h"
#include "nsIMAPBodyShell.h"
#include "nsImapProtocol.h"

#include "nsHashtable.h"
#include "net.h" /* should be defined into msgCore.h? - need MESSAGE_RFC822 */

// need to talk to Rich about this...
#define	IMAP_EXTERNAL_CONTENT_HEADER "X-Mozilla-IMAP-Part"

// imapbody.cpp
// Implementation of the nsIMAPBodyShell and associated classes
// These are used to parse IMAP BODYSTRUCTURE responses, and intelligently (?)
// figure out what parts we need to display inline.


/*
	Create a nsIMAPBodyShell from a full BODYSTRUCUTRE response from the parser.

	The body shell represents a single, top-level object, the message.  The message body
	might be treated as either a container or a leaf (just like any arbitrary part).

	Steps for creating a part:
	1. Pull out the paren grouping for the part
	2. Create a generic part object with that buffer
	3. The factory will return either a leaf or container, depending on what it really is.
	4. It is responsible for parsing its children, if there are any
*/


///////////// nsIMAPBodyShell ////////////////////////////////////

nsIMAPBodyShell::nsIMAPBodyShell(nsImapProtocol *protocolConnection, const char *buf, PRUint32 UID, const char *folderName)
{
	m_isValid = FALSE;
	m_isBeingGenerated = FALSE;
	m_cached = FALSE;
	m_gotAttachmentPref = FALSE;
	m_generatingWholeMessage = FALSE;
	m_showAttachmentsInline = TRUE;
	m_generatingPart = NULL;
	m_protocolConnection = protocolConnection;
	NS_ASSERTION(m_protocolConnection, "non null connection");
	if (!m_protocolConnection)
		return;
	m_prefetchQueue = new nsIMAPMessagePartIDArray();
	if (!m_prefetchQueue)
		return;
	NS_ASSERTION(buf, "null buffer passed to nsIMAPBodyShell constructor");
	if (!buf)
		return;
	m_UID = "";
	m_UID.Append(UID, 10);
#ifdef DEBUG_chrisf
	NS_ASSERTION(folderName);
#endif
	if (!folderName)
		return;
	m_folderName = nsCRT::strdup(folderName);
	if (!m_folderName)
		return;
	// Turn the BODYSTRUCTURE response into a form that the nsIMAPBodypartMessage can be constructed from.
	char *doctoredBuf = PR_smprintf("(\"message\" \"rfc822\" NIL NIL NIL NIL 0 () %s 0)", buf);
	if (!doctoredBuf)
		return;
	SetIsValid(TRUE);
	m_message = new nsIMAPBodypartMessage(this, NULL, doctoredBuf, NULL, TRUE);
	PR_Free(doctoredBuf);
	if (!m_message || !m_message->GetIsValid())
		return;
}

nsIMAPBodyShell::~nsIMAPBodyShell()
{
	delete m_message;
	delete m_prefetchQueue;
	PR_FREEIF(m_folderName);
}

void nsIMAPBodyShell::SetIsValid(PRBool valid)
{
//	if (!valid)
//		PR_LOG(IMAP, out, ("BODYSHELL: Shell is invalid."));
	m_isValid = valid;
}


PRBool	nsIMAPBodyShell::GetShowAttachmentsInline()
{
	if (!m_gotAttachmentPref)
	{
		m_showAttachmentsInline = m_protocolConnection->GetShowAttachmentsInline();
		m_gotAttachmentPref = TRUE;
	}

	return m_showAttachmentsInline;
}


// Fills in buffer (and adopts storage) for header object
void nsIMAPBodyShell::AdoptMessageHeaders(char *headers, const char *partNum)
{
	if (!GetIsValid())
		return;

	if (!partNum)
		partNum = "0";

	// we are going to say that a message header object only has
	// part data, and no header data.
	nsIMAPBodypart *foundPart = m_message->FindPartWithNumber(partNum);
	if (foundPart)
	{
		nsIMAPBodypartMessage *messageObj = foundPart->GetnsIMAPBodypartMessage();
		if (messageObj)
		{
			messageObj->AdoptMessageHeaders(headers);
			if (!messageObj->GetIsValid())
				SetIsValid(FALSE);
		}
		else
		{
			// We were filling in message headers for a given part number.
			// We looked up that part number, found an object, but it
			// wasn't of type message/rfc822.
			// Something's wrong.
			NS_ASSERTION(FALSE, "object not of type message rfc822");
		}
	}
	else
		SetIsValid(FALSE);
}

// Fills in buffer (and adopts storage) for MIME headers in appropriate object.
// If object can't be found, sets isValid to FALSE.
void nsIMAPBodyShell::AdoptMimeHeader(const char *partNum, char *mimeHeader)
{
	if (!GetIsValid())
		return;

	NS_ASSERTION(partNum, "null partnum in body shell");

	nsIMAPBodypart *foundPart = m_message->FindPartWithNumber(partNum);

	if (foundPart)
	{
		foundPart->AdoptHeaderDataBuffer(mimeHeader);
		if (!foundPart->GetIsValid())
			SetIsValid(FALSE);
	}
	else
	{
		SetIsValid(FALSE);
	}
}


void nsIMAPBodyShell::AddPrefetchToQueue(nsIMAPeFetchFields fields, const char *partNumber)
{
	nsIMAPMessagePartID *newPart = new nsIMAPMessagePartID(fields, partNumber);
	if (newPart)
	{
		m_prefetchQueue->AppendElement(newPart);
	}
	else
	{
		// HandleMemoryFailure();
	}
}

// Flushes all of the prefetches that have been queued up in the prefetch queue,
// freeing them as we go
void nsIMAPBodyShell::FlushPrefetchQueue()
{
	m_protocolConnection->PipelinedFetchMessageParts(GetUID(), m_prefetchQueue);
	m_prefetchQueue->RemoveAndFreeAll();
}

// Requires that the shell is valid when called
// Performs a preflight check on all message parts to see if they are all
// inline.  Returns TRUE if all parts are inline, FALSE otherwise.
PRBool nsIMAPBodyShell::PreflightCheckAllInline()
{
	PRBool rv = m_message->PreflightCheckAllInline();
//	if (rv)
//		PR_LOG(IMAP, out, ("BODYSHELL: All parts inline.  Reverting to whole message download."));
	return rv;
}

// When partNum is NULL, Generates a whole message and intelligently
// leaves out parts that are not inline.

// When partNum is not NULL, Generates a MIME part that hasn't been downloaded yet
// Ok, here's how we're going to do this.  Essentially, this
// will be the mirror image of the "normal" generation.
// All parts will be left out except a single part which is
// explicitly specified.  All relevant headers will be included.
// Libmime will extract only the part of interest, so we don't
// have to worry about the other parts.  This also has the
// advantage that it looks like it will be more workable for
// nested parts.  For instance, if a user clicks on a link to
// a forwarded message, then that forwarded message may be 
// generated along with any images that the forwarded message
// contains, for instance.


PRInt32 nsIMAPBodyShell::Generate(char *partNum)
{
	m_isBeingGenerated = TRUE;
	m_generatingPart = partNum;
	PRInt32 contentLength = 0;

	if (!GetIsValid() || PreflightCheckAllInline())
	{
		// We don't have a valid shell, or all parts are going to be inline anyway.  Fall back to fetching the whole message.
#ifdef DEBUG_chrisf
		NS_ASSERTION(GetIsValid());
#endif
		m_generatingWholeMessage = TRUE;
		PRUint32 messageSize = m_protocolConnection->GetMessageSize(GetUID(), TRUE);
		m_protocolConnection->SetContentModified(FALSE);	// So that when we cache it, we know we have the whole message
		if (!DeathSignalReceived())
			m_protocolConnection->FetchTryChunking(GetUID(), kEveryThingRFC822, TRUE, NULL, messageSize);
		contentLength = (PRInt32) messageSize;	// ugh
	}
	else
	{
		// We have a valid shell.
		PRBool streamCreated = FALSE;
		m_generatingWholeMessage = FALSE;

		////// PASS 1 : PREFETCH ///////
		// First, prefetch any additional headers/data that we need
		if (!GetPseudoInterrupted())
			m_message->Generate(FALSE, TRUE);	// This queues up everything we need to prefetch
		// Now, run a single pipelined prefetch  (neato!)
		FlushPrefetchQueue();

		////// PASS 2 : COMPUTE STREAM SIZE ///////
		// Next, figure out the size from the parts that we're going to fill in,
		// plus all of the MIME headers, plus the message header itself
		if (!GetPseudoInterrupted())
			contentLength = m_message->Generate(FALSE, FALSE);

		// Setup the stream
		if (!GetPseudoInterrupted() && !DeathSignalReceived())
		{
			m_protocolConnection->BeginMessageDownLoad(contentLength, MESSAGE_RFC822);
			streamCreated = TRUE;
		}

		////// PASS 3 : GENERATE ///////
		// Generate the message
		if (!GetPseudoInterrupted() && !DeathSignalReceived())
			m_message->Generate(TRUE, FALSE);

		// Close the stream here - normal.  If pseudointerrupted, the connection will abort the download stream
		if (!GetPseudoInterrupted() && !DeathSignalReceived())
			m_protocolConnection->NormalMessageEndDownload();
		else if (streamCreated)
			m_protocolConnection->AbortMessageDownLoad();

		m_generatingPart = NULL;
		
	}

	m_isBeingGenerated = FALSE;
	return contentLength;
}

PRBool nsIMAPBodyShell::GetPseudoInterrupted()
{
	PRBool rv = m_protocolConnection->GetPseudoInterrupted();
	return rv;
}

PRBool nsIMAPBodyShell::DeathSignalReceived()
{
	PRBool rv = m_protocolConnection->DeathSignalReceived();
	return rv;
}


////////////// Helpers //////////////////////////////////////////////////


// Pass in a buffer starting at the first open paren
// Returns the location of the corresponding closing paren,
// or NULL if there is none in the buffer
static char *findEndParenInBuffer(char *buf)
{
	char *where = buf;
	int numCloseParensNeeded = 1;
	while (where && *where && (numCloseParensNeeded > 0))
	{
		where++;
		if (*where == '(')
			numCloseParensNeeded++;
		else if (*where == ')')
			numCloseParensNeeded--;
	}
	return where;
}


///////////// nsIMAPBodypart ////////////////////////////////////



/* static */
nsIMAPBodypart *nsIMAPBodypart::CreatePart(nsIMAPBodyShell *shell, char *partNum, const char *buf, nsIMAPBodypart *parentPart)
{
	// Check to see if this buffer is a leaf or container
	// (Look at second character - if an open paren, then it is a container)
	if (buf[0] != '(' || !buf[1])
	{
		NS_ASSERTION(FALSE, "part is not a leaf or container");
		return NULL;
	}
	
	if (buf[1] == '(')
	{
		// If a container:
		return new nsIMAPBodypartMultipart(shell, partNum, buf, parentPart);
	}
	else
	{
		// If a leaf:
		nsIMAPBodypart *rv =  new nsIMAPBodypartLeaf(shell, partNum, buf, parentPart);
		if (rv && rv->GetIsValid())
		{
			const char *bodyType = rv->GetBodyType();
			const char *bodySubType = rv->GetBodySubType();
			if (!PL_strcasecmp(bodyType, "message") &&
				!PL_strcasecmp(bodySubType, "rfc822"))
			{
				// This is actually a part of type message/rfc822,
				// probably a forwarded message.  delete this and return
				// the new type
				char *keepPartNum = nsCRT::strdup(partNum);	// partNum will be deleted next...
				delete rv;
				return new nsIMAPBodypartMessage(shell, keepPartNum, buf, parentPart, FALSE);
			}
		}
		return rv;
	}
}


nsIMAPBodypart::nsIMAPBodypart(nsIMAPBodyShell *shell, char *partNumber, const char *buf, nsIMAPBodypart *parentPart) :
	nsIMAPGenericParser()
{
	SetIsValid(TRUE);
	m_parentPart = parentPart;
	m_partNumberString = partNumber;	// storage adopted
	if (!shell)
	{
		SetIsValid(FALSE);
		return;
	}
	if (buf)
		m_responseBuffer = nsCRT::strdup(buf);
	else
		m_responseBuffer = NULL;
	m_shell = shell;
	m_partData = NULL;
	m_headerData = NULL;
	m_boundaryData = NULL;	// initialize from parsed BODYSTRUCTURE
	m_contentLength = 0;
	m_partLength = 0;

	m_contentType = NULL;
	m_bodyType = NULL;
	m_bodySubType = NULL;
	m_bodyID = NULL;
	m_bodyDescription = NULL;
	m_bodyEncoding = NULL;
}

nsIMAPBodypart::~nsIMAPBodypart()
{
	PR_FREEIF(m_partNumberString);
	PR_FREEIF(m_responseBuffer);
	PR_FREEIF(m_contentType);
	PR_FREEIF(m_bodyType);
	PR_FREEIF(m_bodySubType);
	PR_FREEIF(m_bodyID);
	PR_FREEIF(m_bodyDescription);
	PR_FREEIF(m_bodyEncoding);
	PR_FREEIF(m_partData);
	PR_FREEIF(m_headerData);
	PR_FREEIF(m_boundaryData);
}

void nsIMAPBodypart::SetIsValid(PRBool valid)
{
	m_isValid = valid;
	if (!m_isValid)
	{
		//PR_LOG(IMAP, out, ("BODYSHELL: Part is invalid.  Part Number: %s Content-Type: %s", m_partNumberString, m_contentType));
		m_shell->SetIsValid(FALSE);
	}
}

PRBool nsIMAPBodypart::GetNextLineForParser(char **nextLine)
{
	PRBool rv = TRUE;
	*nextLine = m_responseBuffer;
	if (!m_responseBuffer)
		rv = FALSE;
	m_responseBuffer = NULL;
	return rv;
}

PRBool nsIMAPBodypart::ContinueParse()
{
	return GetIsValid() && nsIMAPGenericParser::ContinueParse() && m_shell->GetIsValid();
}

// Adopts storage for part data buffer.  If NULL, sets isValid to FALSE.
void nsIMAPBodypart::AdoptPartDataBuffer(char *buf)
{
	m_partData = buf;
	if (!m_partData)
	{
		SetIsValid(FALSE);
	}
}

// Adopts storage for header data buffer.  If NULL, sets isValid to FALSE.
void nsIMAPBodypart::AdoptHeaderDataBuffer(char *buf)
{
	m_headerData = buf;
	if (!m_headerData)
	{
		SetIsValid(FALSE);
	}
}

// Finds the part with given part number
// Returns a nsIMAPBodystructure of the matched part if it is this
// or one of its children.  Returns NULL otherwise.
nsIMAPBodypart *nsIMAPBodypart::FindPartWithNumber(const char *partNum)
{
	// either brute force, or do it the smart way - look at the number.
	// (the parts should be ordered, and hopefully indexed by their number)

	if (m_partNumberString && !PL_strcasecmp(partNum, m_partNumberString))
		return this;

	//if (!m_partNumberString && !PL_strcasecmp(partNum, "1"))
	//	return this;

	return NULL;
}

/*
void nsIMAPBodypart::PrefetchMIMEHeader()
{
	if (!m_headerData && !m_shell->DeathSignalReceived())
	{
		m_shell->GetConnection()->FetchMessage(m_shell->GetUID(), kMIMEHeader, TRUE, 0, 0, m_partNumberString);
		// m_headerLength will be filled in when it is adopted from the parser
	}
	if (!m_headerData)
	{
		SetIsValid(FALSE);
	}
}
*/

void nsIMAPBodypart::QueuePrefetchMIMEHeader()
{
	m_shell->AddPrefetchToQueue(kMIMEHeader, m_partNumberString);
}

PRInt32 nsIMAPBodypart::GenerateMIMEHeader(PRBool stream, PRBool prefetch)
{
	if (prefetch && !m_headerData)
	{
		QueuePrefetchMIMEHeader();
		return 0;
	}
	else if (m_headerData)
	{
		PRInt32 mimeHeaderLength = 0;

		if (!ShouldFetchInline())
		{
			// if this part isn't inline, add the X-Mozilla-IMAP-Part header
			char *xPartHeader = PR_smprintf("%s: %s", IMAP_EXTERNAL_CONTENT_HEADER, m_partNumberString);
			if (xPartHeader)
			{
				if (stream)
				{
					m_shell->GetConnection()->Log("SHELL","GENERATE-XHeader",m_partNumberString);
					m_shell->GetConnection()->HandleMessageDownLoadLine(xPartHeader, FALSE);
				}
				mimeHeaderLength += PL_strlen(xPartHeader);
				PR_Free(xPartHeader);
			}
		}

		mimeHeaderLength += PL_strlen(m_headerData);
		if (stream)
		{
			m_shell->GetConnection()->Log("SHELL","GENERATE-MIMEHeader",m_partNumberString);
			m_shell->GetConnection()->HandleMessageDownLoadLine(m_headerData, FALSE);	// all one line?  Can we do that?
		}

		return mimeHeaderLength;
	}
	else 
	{
		SetIsValid(FALSE);	// prefetch didn't adopt a MIME header
		return 0;
	}
}

PRInt32 nsIMAPBodypart::GeneratePart(PRBool stream, PRBool prefetch)
{
	if (prefetch)
		return 0;	// don't need to prefetch anything

	if (m_partData)	// we have prefetched the part data
	{
		if (stream)
		{
			m_shell->GetConnection()->Log("SHELL","GENERATE-Part-Prefetched",m_partNumberString);
			m_shell->GetConnection()->HandleMessageDownLoadLine(m_partData, FALSE);
		}
		return PL_strlen(m_partData);
	}
	else	// we are fetching and streaming this part's body as we go
	{
		if (stream && !m_shell->DeathSignalReceived())
		{
			m_shell->GetConnection()->Log("SHELL","GENERATE-Part-Inline",m_partNumberString);
			m_shell->GetConnection()->FetchTryChunking(m_shell->GetUID(), kMIMEPart, TRUE, m_partNumberString, m_partLength);
		}
		return m_partLength;	// the part length has been filled in from the BODYSTRUCTURE response
	}
}

PRInt32 nsIMAPBodypart::GenerateBoundary(PRBool stream, PRBool prefetch, PRBool lastBoundary)
{
	if (prefetch)
		return 0;	// don't need to prefetch anything

	if (m_boundaryData)
	{
		if (!lastBoundary)
		{
			if (stream)
			{
				m_shell->GetConnection()->Log("SHELL","GENERATE-Boundary",m_partNumberString);
				m_shell->GetConnection()->HandleMessageDownLoadLine(m_boundaryData, FALSE);
			}
			return PL_strlen(m_boundaryData);
		}
		else	// the last boundary
		{
			char *lastBoundaryData = PR_smprintf("%s--", m_boundaryData);
			if (lastBoundaryData)
			{
				if (stream)
				{
					m_shell->GetConnection()->Log("SHELL","GENERATE-Boundary-Last",m_partNumberString);
					m_shell->GetConnection()->HandleMessageDownLoadLine(lastBoundaryData, FALSE);
				}
				PRInt32 rv = PL_strlen(lastBoundaryData);
				PR_Free(lastBoundaryData);
				return rv;
			}
			else
			{
				//HandleMemoryFailure();
				return 0;
			}
		}
	}
	else
		return 0;
}

PRInt32 nsIMAPBodypart::GenerateEmptyFilling(PRBool stream, PRBool prefetch)
{
	if (prefetch)
		return 0;	// don't need to prefetch anything

	char *emptyString = "This body part will be downloaded on demand.";
	// XP_GetString(MK_IMAP_EMPTY_MIME_PART);  ### need to be able to localize
	if (emptyString)
	{
		if (stream)
		{
			m_shell->GetConnection()->Log("SHELL","GENERATE-Filling",m_partNumberString);
			m_shell->GetConnection()->HandleMessageDownLoadLine(emptyString, FALSE);
		}
		return PL_strlen(emptyString);
	}
	else
		return 0;
}


// Returns TRUE if the prefs say that this content type should
// explicitly be kept in when filling in the shell
PRBool nsIMAPBodypart::ShouldExplicitlyFetchInline()
{
	 return FALSE;
}


// Returns TRUE if the prefs say that this content type should
// explicitly be left out when filling in the shell
PRBool nsIMAPBodypart::ShouldExplicitlyNotFetchInline()
{
	return FALSE;
}


///////////// nsIMAPBodypartLeaf /////////////////////////////


nsIMAPBodypartLeaf::nsIMAPBodypartLeaf(nsIMAPBodyShell *shell, char *partNum, const char *buf, nsIMAPBodypart *parentPart) : 
nsIMAPBodypart(shell, partNum, buf, parentPart)
{
	SetIsValid(ParseIntoObjects());
}

nsIMAPBodypartType nsIMAPBodypartLeaf::GetType()
{
	return IMAP_BODY_LEAF;
}

PRInt32 nsIMAPBodypartLeaf::Generate(PRBool stream, PRBool prefetch)
{
	PRInt32 len = 0;

	if (GetIsValid())
	{

		if (stream && !prefetch)
			m_shell->GetConnection()->Log("SHELL","GENERATE-Leaf",m_partNumberString);

		// Stream out the MIME part boundary
		//GenerateBoundary();
		NS_ASSERTION(m_parentPart, "part has no parent");
		//nsIMAPBodypartMessage *parentMessage = m_parentPart ? m_parentPart->GetnsIMAPBodypartMessage() : NULL;

		// Stream out the MIME header of this part, if this isn't the only body part of a message
		//if (parentMessage ? !parentMessage->GetIsTopLevelMessage() : TRUE)
		if ((m_parentPart->GetType() != IMAP_BODY_MESSAGE_RFC822)
			&& !m_shell->GetPseudoInterrupted())
			len += GenerateMIMEHeader(stream, prefetch);

		if (!m_shell->GetPseudoInterrupted())
		{
			if (ShouldFetchInline())
			{
				// Fetch and stream the content of this part
				len += GeneratePart(stream, prefetch);
			}
			else
			{
				// fill in the filling within the empty part
				len += GenerateEmptyFilling(stream, prefetch);
			}
		}
	}
	m_contentLength = len;
	return m_contentLength;
}


// leaves parser at next token after basic fields are parsed.
// This can therefore be used for types derived from leaves,
// such as message/rfc822 and text/*.
PRBool nsIMAPBodypartLeaf::ParseIntoObjects()
{
	// No children for a leaf - just parse this

	// Eat the buffer into the parser
	fNextToken = GetNextToken();

	// body type  ("application", "text", "image", etc.)
	if (ContinueParse())
	{
		fNextToken++;	// eat the first '('
		m_bodyType = CreateNilString();
		if (ContinueParse())
			fNextToken = GetNextToken();
		else
			SetIsValid(FALSE);
	}
	else
		SetIsValid(FALSE);

	// body subtype  ("gif", "html", etc.)
	if (ContinueParse())
	{
		m_bodySubType = CreateNilString();
		if (ContinueParse())
			fNextToken = GetNextToken();
		else
			SetIsValid(FALSE);
	}
	else
		SetIsValid(FALSE);

	// body parameter parenthesized list
	if (ContinueParse())
	{
		if (!fNextToken)
			SetIsValid(FALSE);
		else if (fNextToken[0] == '(')
		{
			if (!fNextToken[1])
				SetIsValid(FALSE);
			else
			{
				if (fNextToken[1] != ')')
				{
					fNextToken++;
					skip_to_close_paren();
				}
				else
				{
					fNextToken = GetNextToken();
				}
			}
		}
		else if (!PL_strcasecmp(fNextToken, "NIL"))
			fNextToken = GetNextToken();
	}
	else
		SetIsValid(FALSE);

	// body id
	if (ContinueParse())
	{
		m_bodyID = CreateNilString();
		if (ContinueParse())
			fNextToken = GetNextToken();
		else
			SetIsValid(FALSE);
	}
	else
		SetIsValid(FALSE);

	// body description
	if (ContinueParse())
	{
		m_bodyDescription = CreateNilString();
		if (ContinueParse())
			fNextToken = GetNextToken();
		else
			SetIsValid(FALSE);
	}
	else
		SetIsValid(FALSE);

	// body encoding
	if (ContinueParse())
	{
		m_bodyEncoding = CreateNilString();
		if (ContinueParse())
			fNextToken = GetNextToken();
		else
			SetIsValid(FALSE);
	}
	else
		SetIsValid(FALSE);

	// body size
	if (ContinueParse())
	{
		char *bodySizeString = CreateAtom();
		if (bodySizeString)
		{
			// convert to an PRInt32 here, set m_partLength
			m_partLength = atoi(bodySizeString);
			PR_Free(bodySizeString);
		}
		else
			SetIsValid(FALSE);

		if (ContinueParse())
			fNextToken = GetNextToken();
		else
			SetIsValid(FALSE);
	}
	else
		SetIsValid(FALSE);


	// that's it, we can just leave the parser the way it is since
	// we'll never use it again.

	if (GetIsValid() && m_bodyType && m_bodySubType)
	{
		m_contentType = PR_smprintf("%s/%s",m_bodyType,m_bodySubType);
	}

	return TRUE;
}

// returns TRUE if this part should be fetched inline for generation.
PRBool	nsIMAPBodypartLeaf::ShouldFetchInline()
{
	char *generatingPart = m_shell->GetGeneratingPart();
	if (generatingPart)
	{
		// If we are generating a specific part
		if (!PL_strcmp(generatingPart, m_partNumberString))
		{
			// This is the part we're generating
			return TRUE;
		}
		else
		{
			// If this is the only body part of a message, and that
			// message is the part being generated, then this leaf should
			// be inline as well.
			if ((m_parentPart->GetType() == IMAP_BODY_MESSAGE_RFC822) &&
				(!PL_strcmp(m_parentPart->GetPartNumberString(), generatingPart)))
				return TRUE;

			// Leave out all other leaves if this isn't the one
			// we're generating.
			// Maybe change later to check parents, etc.
			return FALSE;
		}
	}
	else
	{
		// We are generating the whole message, possibly (hopefully)
		// leaving out non-inline parts

		if (ShouldExplicitlyFetchInline())
			return TRUE;
		if (ShouldExplicitlyNotFetchInline())
			return FALSE;

		// If the parent is a message (this is the only body part of that
		// message), and that message should be inline, then its body
		// should inherit the inline characteristics of that message
		if (m_parentPart->GetType() == IMAP_BODY_MESSAGE_RFC822)
			return m_parentPart->ShouldFetchInline();

		if (!m_shell->GetShowAttachmentsInline())
		{
			// The first text part is still displayed inline,
			// even if View Attachments As Links is on.
			if (!PL_strcmp(m_partNumberString, "1") &&
				!PL_strcasecmp(m_bodyType, "text"))
				return TRUE;	// we're downloading it inline
			else
				return FALSE;	// we can leave it on the server
		}
		if (!PL_strcasecmp(m_bodyType, "APPLICATION") &&	// If it is of type "application"
			PL_strncasecmp(m_bodySubType, "x-pkcs7", 7)	// and it's not a signature (signatures are inline)
#ifdef XP_MAC
			&& PL_strcasecmp(m_bodySubType, "applefile")	// and it's not an appledouble resource fork on Mac  (they're inline)
#endif
			)
			return FALSE;	// we can leave it on the server
		return TRUE;	// we're downloading it inline
	}
}

PRBool	nsIMAPBodypartLeaf::PreflightCheckAllInline()
{
	// only need to check this part, since it has no children.
	return ShouldFetchInline();
}


///////////// nsIMAPBodypartMessage ////////////////////////

nsIMAPBodypartMessage::nsIMAPBodypartMessage(nsIMAPBodyShell *shell, char *partNum, const char *buf, nsIMAPBodypart *parentPart,
										   PRBool topLevelMessage) : nsIMAPBodypartLeaf(shell, partNum, buf, parentPart)
{
	m_topLevelMessage = topLevelMessage;
	if (m_topLevelMessage)
	{
		m_partNumberString = PR_smprintf("0");
		if (!m_partNumberString)
		{
			SetIsValid(FALSE);
			return;
		}
	}
	m_body = NULL;
	m_headers = new nsIMAPMessageHeaders(shell, m_partNumberString, this);	// We always have a Headers object
	if (!m_headers || !m_headers->GetIsValid())
	{
		SetIsValid(FALSE);
		return;
	}
	SetIsValid(ParseIntoObjects());
}

nsIMAPBodypartType nsIMAPBodypartMessage::GetType()
{
	return IMAP_BODY_MESSAGE_RFC822;
}

nsIMAPBodypartMessage::~nsIMAPBodypartMessage()
{
	delete m_headers;
	delete m_body;
}

PRInt32 nsIMAPBodypartMessage::Generate(PRBool stream, PRBool prefetch)
{
	if (!GetIsValid())
		return 0;

	m_contentLength = 0;

	if (stream && !prefetch)
		m_shell->GetConnection()->Log("SHELL","GENERATE-MessageRFC822",m_partNumberString);

	if (!m_topLevelMessage && !m_shell->GetPseudoInterrupted())	// not the top-level message - we need the MIME header as well as the message header
	{
		m_contentLength += GenerateMIMEHeader(stream, prefetch);
	}

	if (!m_shell->GetPseudoInterrupted())
		m_contentLength += m_headers->Generate(stream, prefetch);
	if (!m_shell->GetPseudoInterrupted())
		m_contentLength += m_body->Generate(stream, prefetch);

	return m_contentLength;
}


PRBool nsIMAPBodypartMessage::ParseIntoObjects()
{
	if (ContinueParse())	// basic fields parsed out ok
	{
		// three additional fields: envelope structure, bodystructure, and size in lines
		if (*fNextToken == '(')
		{
			// ignore the envelope... maybe when we have time we can parse it for fields
			// char *envelope = CreateParenGroup();
			fNextToken++;
			skip_to_close_paren();
		}
		else
			SetIsValid(FALSE);

		if (ContinueParse())
		{
			if (*fNextToken == '(')
			{
				// extract the bodystructure and create the body from it
				char *parenGroup = CreateParenGroup();
				if (parenGroup)
				{
					char *bodyPartNum = NULL;
					if (!m_topLevelMessage)
						bodyPartNum = PR_smprintf("%s.1", m_partNumberString);
					else
						bodyPartNum = PR_smprintf("1");
					if (bodyPartNum && ContinueParse())	// storage adopted by child part
					{
						m_body = nsIMAPBodypart::CreatePart(m_shell, bodyPartNum, parenGroup, this);
					}
					else
					{
						// HandleMemoryFailure();
					}
					PR_Free(parenGroup);
				}
				else
				{
					SetSyntaxError(TRUE);
					SetIsValid(FALSE);
				}
			}
			else
				SetIsValid(FALSE);
		}
		else
			SetIsValid(FALSE);

		// ignore size in lines
	}

	if (!m_body || !m_body->GetIsValid())
		SetIsValid(FALSE);			

	return GetIsValid();
}


PRBool	nsIMAPBodypartMessage::ShouldFetchInline()
{
	if (m_topLevelMessage)	// the main message should always be defined as "inline"
		return TRUE;

	char *generatingPart = m_shell->GetGeneratingPart();
	if (generatingPart)
	{
		// If we are generating a specific part
		// Always generate containers (just don't fill them in)
		// because it is low cost (everything is cached)
		// and it gives the message its full MIME structure,
		// to avoid any potential mishap.
		return TRUE;
	}
	else
	{
		// Generating whole message

		if (ShouldExplicitlyFetchInline())
			return TRUE;
		if (ShouldExplicitlyNotFetchInline())
			return FALSE;


		if (!m_shell->GetShowAttachmentsInline())
			return FALSE;

		// Message types are inline, by default.
		return TRUE;
	}
}

PRBool	nsIMAPBodypartMessage::PreflightCheckAllInline()
{
	if (!ShouldFetchInline())
		return FALSE;

	return m_body->PreflightCheckAllInline();
}

// Fills in buffer (and adopts storage) for header object
void nsIMAPBodypartMessage::AdoptMessageHeaders(char *headers)
{
	if (!GetIsValid())
		return;

	// we are going to say that the message headers only have
	// part data, and no header data.
	m_headers->AdoptPartDataBuffer(headers);
	if (!m_headers->GetIsValid())
		SetIsValid(FALSE);
}

// Finds the part with given part number
// Returns a nsIMAPBodystructure of the matched part if it is this
// or one of its children.  Returns NULL otherwise.
nsIMAPBodypart *nsIMAPBodypartMessage::FindPartWithNumber(const char *partNum)
{
	// either brute force, or do it the smart way - look at the number.
	// (the parts should be ordered, and hopefully indexed by their number)

	if (!PL_strcasecmp(partNum, m_partNumberString))
		return this;

	return m_body->FindPartWithNumber(partNum);
}

///////////// nsIMAPBodypartMultipart ////////////////////////


nsIMAPBodypartMultipart::nsIMAPBodypartMultipart(nsIMAPBodyShell *shell, char *partNum, const char *buf, nsIMAPBodypart *parentPart) : 
nsIMAPBodypart(shell, partNum, buf, parentPart)
{
	if (!m_parentPart  || (m_parentPart->GetType() == IMAP_BODY_MESSAGE_RFC822))
	{
		// the multipart (this) will inherit the part number of its parent
		PR_FREEIF(m_partNumberString);
		if (!m_parentPart)
		{
			m_partNumberString = PR_smprintf("0");
		}
		else
		{
			m_partNumberString = nsCRT::strdup(m_parentPart->GetPartNumberString());
		}
	}
	m_partList = new nsVoidArray();
	if (!m_partList)
	{
		SetIsValid(FALSE);
		return;
	}
	if (!m_parentPart)
	{
		SetIsValid(FALSE);
		return;
	}
	SetIsValid(ParseIntoObjects());
}

nsIMAPBodypartType nsIMAPBodypartMultipart::GetType()
{
	return IMAP_BODY_MULTIPART;
}

nsIMAPBodypartMultipart::~nsIMAPBodypartMultipart()
{
	nsIMAPBodypart *bp = NULL;
	for (int i = m_partList->Count() - 1; i >= 0; i--)
	{
		delete (nsIMAPBodypart *)(m_partList->ElementAt(i));
	}
	delete m_partList;
}

PRInt32 nsIMAPBodypartMultipart::Generate(PRBool stream, PRBool prefetch)
{
	PRInt32 len = 0;

	if (GetIsValid())
	{
		if (stream && !prefetch)
			m_shell->GetConnection()->Log("SHELL","GENERATE-Multipart",m_partNumberString);

		// Stream out the MIME header of this part

		PRBool parentIsMessageType = GetParentPart() ? (GetParentPart()->GetType() == IMAP_BODY_MESSAGE_RFC822) : TRUE;

		// If this is multipart/signed, then we always want to generate the MIME headers of this multipart.
		// Otherwise, we only want to do it if the parent is not of type "message"
		PRBool needMIMEHeader = !parentIsMessageType;  // !PL_strcasecmp(m_bodySubType, "signed") ? TRUE : !parentIsMessageType;
		if (needMIMEHeader && !m_shell->GetPseudoInterrupted())	// not a message body's type
		{
			len += GenerateMIMEHeader(stream, prefetch);
		}

		if (ShouldFetchInline())
		{
			for (int i = 0; i < m_partList->Count(); i++)
			{
				if (!m_shell->GetPseudoInterrupted())
					len += GenerateBoundary(stream, prefetch, FALSE);
				if (!m_shell->GetPseudoInterrupted())
					len += ((nsIMAPBodypart *)(m_partList->ElementAt(i)))->Generate(stream, prefetch);
			}
			if (!m_shell->GetPseudoInterrupted())
				len += GenerateBoundary(stream, prefetch, TRUE);
		}
		else
		{
			// fill in the filling within the empty part
			if (!m_shell->GetPseudoInterrupted())
				len += GenerateEmptyFilling(stream, prefetch);
		}
	}
	m_contentLength = len;
	return m_contentLength;
}

PRBool nsIMAPBodypartMultipart::ParseIntoObjects()
{
	char *where = m_responseBuffer+1;
	int childCount = 0;
	// Parse children
	// Pull out all the children parts from buf, and send them on their way
	while (where[0] == '(' && ContinueParse())
	{
		char *endParen = findEndParenInBuffer(where);
		if (endParen)
		{
			PRInt32 len = 1 + endParen - where;
			char *parenGroup = (char *)PR_Malloc((len + 1)*sizeof(char));
			if (parenGroup)
			{
				PL_strncpy(parenGroup, where, len + 1);
				parenGroup[len] = 0;
				childCount++;
				char *childPartNum = NULL;
				if (PL_strcmp(m_partNumberString, "0"))	// not top-level
					childPartNum = PR_smprintf("%s.%d", m_partNumberString, childCount);
				else	// top-level
					childPartNum = PR_smprintf("%d", childCount);
				if (childPartNum)	// storage adopted for childPartNum
				{
					nsIMAPBodypart *child = nsIMAPBodypart::CreatePart(m_shell, childPartNum, parenGroup, this);
					if (child)
					{
						m_partList->AppendElement(child);
					}
					else
					{
						SetIsValid(FALSE);
					}
				}
				else
				{
					SetIsValid(FALSE);
				}
				PR_Free(parenGroup);
				
				// move the next child down
				char *newBuf = NULL;
				if (*(endParen + 1) == ' ')	// last child
					newBuf = PR_smprintf("(%s", endParen + 2);
				else
					newBuf = PR_smprintf("(%s", endParen + 1);
				PR_FREEIF(m_responseBuffer);
				m_responseBuffer = newBuf;
				where = m_responseBuffer+1;
			}
			else
			{
				SetIsValid(FALSE);
			}
		}
		else
		{
			SetIsValid(FALSE);
		}
	}

	if (GetIsValid())
	{
		m_bodyType = nsCRT::strdup("multipart");

		// Parse what's left of this.

		// Eat the buffer into the parser
		fNextToken = GetNextToken();
		if (ContinueParse())
		{
			// multipart subtype (mixed, alternative, etc.)
			fNextToken++;
			m_bodySubType = CreateNilString();
			if (ContinueParse())
				fNextToken = GetNextToken();
			else
				SetIsValid(FALSE);
		}

		if (ContinueParse())
		{
			// body parameter parenthesized list, including
			// boundary
			fNextToken++;
			while (ContinueParse() && *fNextToken != ')')
			{
				char *attribute = CreateNilString();
				if (ContinueParse())
					fNextToken = GetNextToken();
				else
					SetIsValid(FALSE);

				if (ContinueParse() && attribute && 
					!PL_strcasecmp(attribute, "BOUNDARY"))
				{
					char *boundary = CreateNilString();
					if (boundary)
					{
						m_boundaryData = PR_smprintf("--%s", boundary);
						PR_Free(boundary);
					}

					if (ContinueParse())
						fNextToken = GetNextToken();
					else
						SetIsValid(FALSE);
					PR_Free(attribute);
				}
				else
				{
					PR_FREEIF(attribute);
					if (ContinueParse())
					{
						// removed because parser was advancing one too many tokens
						//fNextToken = GetNextToken();
						char *value = CreateNilString();
						PR_FREEIF(value);
						if (ContinueParse())
							fNextToken = GetNextToken();
						//if (ContinueParse())
						//	fNextToken = GetNextToken();
					}
				}
			}
		}

		// We might want the disposition, but I think we can leave it like this for now.

		m_contentType = PR_smprintf("%s/%s", m_bodyType, m_bodySubType);
	}

	if (!m_boundaryData)
	{
		// Actually, we should probably generate a boundary here.
		SetIsValid(FALSE);
	}

	return GetIsValid();
}

PRBool	nsIMAPBodypartMultipart::ShouldFetchInline()
{
	char *generatingPart = m_shell->GetGeneratingPart();
	if (generatingPart)
	{
		// If we are generating a specific part
		// Always generate containers (just don't fill them in)
		// because it is low cost (everything is cached)
		// and it gives the message its full MIME structure,
		// to avoid any potential mishap.
		return TRUE;
	}
	else
	{
		// Generating whole message

		if (ShouldExplicitlyFetchInline())
			return TRUE;
		if (ShouldExplicitlyNotFetchInline())
			return FALSE;

		// If "Show Attachments as Links" is on, and
		// the parent of this multipart is not a message,
		// then it's not inline.
		if (!m_shell->GetShowAttachmentsInline() &&
			(m_parentPart->GetType() != IMAP_BODY_MESSAGE_RFC822))
			return FALSE;

		// multiparts are always inline
		// (their children might not be, though)
		return TRUE;
	}
}

PRBool	nsIMAPBodypartMultipart::PreflightCheckAllInline()
{
	PRBool rv = ShouldFetchInline();

	int i = 0;
	while (rv && (i < m_partList->Count()))
	{
		rv = ((nsIMAPBodypart *)(m_partList->ElementAt(i)))->PreflightCheckAllInline();
		i++;
	}

	return rv;
}

nsIMAPBodypart	*nsIMAPBodypartMultipart::FindPartWithNumber(const char *partNum)
{
	NS_ASSERTION(partNum, "null part passed into FindPartWithNumber");

	// check this
	if (!PL_strcmp(partNum, m_partNumberString))
		return this;

	// check children
	for (int i = m_partList->Count() - 1; i >= 0; i--)
	{
		nsIMAPBodypart *foundPart = ((nsIMAPBodypart *)(m_partList->ElementAt(i)))->FindPartWithNumber(partNum);
		if (foundPart)
			return foundPart;
	}

	// not this, or any of this's children
	return NULL;
}



///////////// nsIMAPMessageHeaders ////////////////////////////////////



nsIMAPMessageHeaders::nsIMAPMessageHeaders(nsIMAPBodyShell *shell, char *partNum, nsIMAPBodypart *parentPart) : 
nsIMAPBodypart(shell, partNum, NULL, parentPart)
{
	if (!partNum)
	{
		SetIsValid(FALSE);
		return;
	}
	m_partNumberString = nsCRT::strdup(partNum);
	if (!m_partNumberString)
	{
		SetIsValid(FALSE);
		return;
	}
	if (!m_parentPart || !m_parentPart->GetnsIMAPBodypartMessage())
	{
		// Message headers created without a valid Message parent
		NS_ASSERTION(FALSE, "creating message headers with invalid message parent");
		SetIsValid(FALSE);
	}
}

nsIMAPBodypartType nsIMAPMessageHeaders::GetType()
{
	return IMAP_BODY_MESSAGE_HEADER;
}

void nsIMAPMessageHeaders::QueuePrefetchMessageHeaders()
{

	if (!m_parentPart->GetnsIMAPBodypartMessage()->GetIsTopLevelMessage())	// not top-level headers
		m_shell->AddPrefetchToQueue(kRFC822HeadersOnly, m_partNumberString);
	else
		m_shell->AddPrefetchToQueue(kRFC822HeadersOnly, NULL);
}

PRInt32 nsIMAPMessageHeaders::Generate(PRBool stream, PRBool prefetch)
{
	// prefetch the header
	if (prefetch && !m_partData && !m_shell->DeathSignalReceived())
	{
		QueuePrefetchMessageHeaders();
	}

	if (stream && !prefetch)
		m_shell->GetConnection()->Log("SHELL","GENERATE-MessageHeaders",m_partNumberString);

	// stream out the part data
	if (ShouldFetchInline())
	{
		if (!m_shell->GetPseudoInterrupted())
			m_contentLength = GeneratePart(stream, prefetch);
	}
	else
	{
		m_contentLength = 0;	// don't fill in any filling for the headers
	}
	return m_contentLength;
}

PRBool nsIMAPMessageHeaders::ParseIntoObjects()
{
	return TRUE;
}

PRBool	nsIMAPMessageHeaders::ShouldFetchInline()
{
	return m_parentPart->ShouldFetchInline();
}


///////////// nsIMAPBodyShellCache ////////////////////////////////////


static int
imap_shell_cache_strcmp (const void *a, const void *b)
{
  return PL_strcmp ((const char *) a, (const char *) b);
}


nsIMAPBodyShellCache::nsIMAPBodyShellCache()
{
	m_shellHash = new nsHashtable(20); /* , XP_StringHash, imap_shell_cache_strcmp); */
	m_shellList = new nsVoidArray();
}

/* static */ nsIMAPBodyShellCache *nsIMAPBodyShellCache::Create()
{
	nsIMAPBodyShellCache *cache = new nsIMAPBodyShellCache();
	if (!cache || !cache->m_shellHash || !cache->m_shellList)
		return NULL;

	return cache;
}

nsIMAPBodyShellCache::~nsIMAPBodyShellCache()
{
	while (EjectEntry()) ;
	if (m_shellHash)
		delete m_shellHash;
	delete m_shellList;
}

// We'll use an LRU scheme here.
// We will add shells in numerical order, so the
// least recently used one will be in slot 0.
PRBool nsIMAPBodyShellCache::EjectEntry()
{
	if (m_shellList->Count() < 1)
		return FALSE;

	nsIMAPBodyShell *removedShell = (nsIMAPBodyShell *) (m_shellList->ElementAt(0));
	m_shellList->RemoveElementAt(0);
	nsCStringKey hashKey (removedShell->GetUID().GetBuffer());
	m_shellHash->Remove(&hashKey);
	delete removedShell;

	return TRUE;
}

PRBool	nsIMAPBodyShellCache::AddShellToCache(nsIMAPBodyShell *shell)
{
	// If it's already in the cache, then just return.
	// This has the side-effect of re-ordering the LRU list
	// to put this at the top, which is good, because it's what we want.
	if (FindShellForUID(shell->GetUID(), shell->GetFolderName()))
		return TRUE;

	// OK, so it's not in the cache currently.

	// First, for safety sake, remove any entry with the given UID,
	// just in case we have a collision between two messages in different
	// folders with the same UID.
	nsCStringKey hashKey1(shell->GetUID().GetBuffer());
	nsIMAPBodyShell *foundShell = (nsIMAPBodyShell *) m_shellHash->Get(&hashKey1);
	if (foundShell)
	{
		nsCStringKey hashKey(foundShell->GetUID().GetBuffer());
		m_shellHash->Remove(&hashKey);
		m_shellList->RemoveElement(foundShell);
	}

	// Add the new one to the cache
	m_shellList->AppendElement(shell);
	
	nsCStringKey hashKey2 (shell->GetUID().GetBuffer());
	m_shellHash->Put(&hashKey2, shell);
	shell->SetIsCached(TRUE);

	// while we're not over our size limit, eject entries
	PRBool rv = TRUE;
	while (GetSize() > GetMaxSize())
	{
		rv = EjectEntry();
	}

	return rv;

}

nsIMAPBodyShell *nsIMAPBodyShellCache::FindShellForUID(nsString2 &UID, const char *mailboxName)
{
	if (!UID)
		return NULL;

	nsCStringKey hashKey(UID.GetBuffer());
	nsIMAPBodyShell *foundShell = (nsIMAPBodyShell *) m_shellHash->Get(&hashKey);

	if (!foundShell)
		return NULL;

	// mailbox names must match also.
	if (PL_strcmp(mailboxName, foundShell->GetFolderName()))
		return NULL;

	// adjust the LRU stuff
	m_shellList->RemoveElement(foundShell);	// oh well, I suppose this defeats the performance gain of the hash if it actually is found
	m_shellList->AppendElement(foundShell);		// Adds to end
	
	return foundShell;
}

nsIMAPBodyShell *nsIMAPBodyShellCache::FindShellForUID(PRUint32 UID, const char *mailboxName)
{
	nsString2 uidString;
	
	uidString.Append(UID, 10);
	nsIMAPBodyShell *rv = FindShellForUID(uidString, mailboxName);
	return rv;
}


///////////// nsIMAPMessagePartID ////////////////////////////////////


nsIMAPMessagePartID::nsIMAPMessagePartID(nsIMAPeFetchFields fields, const char *partNumberString)
{
	m_fields = fields;
	m_partNumberString = partNumberString;
}

nsIMAPMessagePartIDArray::nsIMAPMessagePartIDArray()
{
}

nsIMAPMessagePartIDArray::~nsIMAPMessagePartIDArray()
{
	RemoveAndFreeAll();
}

void nsIMAPMessagePartIDArray::RemoveAndFreeAll()
{
	while (Count() > 0)
	{
		nsIMAPMessagePartID *part = GetPart(0);
		delete part;
		RemoveElementAt(0);
	}
}
