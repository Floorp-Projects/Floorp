/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIMAPHostSessionList.h"
#include "nsIMAPBodyShell.h"
#include "nsImapProtocol.h"

#include "nsHashtable.h"
#include "nsMimeTypes.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsITransport.h"

// need to talk to Rich about this...
#define	IMAP_EXTERNAL_CONTENT_HEADER "X-Mozilla-IMAP-Part"

// imapbody.cpp
// Implementation of the nsIMAPBodyShell and associated classes
// These are used to parse IMAP BODYSTRUCTURE responses, and intelligently (?)
// figure out what parts we need to display inline.

static PRInt32 gMaxDepth = 0;	// Maximum depth that we will descend before marking a shell invalid.
                                // This will be initialized from the prefs the first time a shell is created.
                                // This is to protect against excessively complex (deep) BODYSTRUCTURE responses.


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
  if (gMaxDepth == 0)
  {
    nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
    if (prefBranch) {
      // one-time initialization
      prefBranch->GetIntPref("mail.imap.mime_parts_on_demand_max_depth", &gMaxDepth);
    }
  }
  
  m_isValid = PR_FALSE;
  m_isBeingGenerated = PR_FALSE;
  m_cached = PR_FALSE;
  m_gotAttachmentPref = PR_FALSE;
  m_generatingWholeMessage = PR_FALSE;
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
  m_UID.AppendInt(UID);
#ifdef DEBUG_chrisf
  NS_ASSERTION(folderName);
#endif
  if (!folderName)
    return;
  m_folderName = nsCRT::strdup(folderName);
  if (!m_folderName)
    return;
  
  SetContentModified(GetShowAttachmentsInline() ? IMAP_CONTENT_MODIFIED_VIEW_INLINE : IMAP_CONTENT_MODIFIED_VIEW_AS_LINKS);
  // Turn the BODYSTRUCTURE response into a form that the nsIMAPBodypartMessage can be constructed from.
  char *doctoredBuf = PR_smprintf("(\"message\" \"rfc822\" NIL NIL NIL NIL 0 () %s 0)", buf);
  if (!doctoredBuf)
    return;
  SetIsValid(PR_TRUE);
  m_message = new nsIMAPBodypartMessage(this, NULL, doctoredBuf, NULL, PR_TRUE);
  PR_Free(doctoredBuf);
  if (!m_message || !m_message->GetIsValid())
    return;
}

nsIMAPBodyShell::~nsIMAPBodyShell()
{
  delete m_message;
  delete m_prefetchQueue;
  PR_Free(m_folderName);
}

void nsIMAPBodyShell::SetIsValid(PRBool valid)
{
  //	if (!valid)
  //		PR_LOG(IMAP, out, ("BODYSHELL: Shell is invalid."));
  m_isValid = valid;
}


PRBool nsIMAPBodyShell::GetShowAttachmentsInline()
{
  if (!m_gotAttachmentPref)
  {
    m_showAttachmentsInline = !m_protocolConnection || m_protocolConnection->GetShowAttachmentsInline();
    m_gotAttachmentPref = PR_TRUE;
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
        SetIsValid(PR_FALSE);
    }
    else
    {
      // We were filling in message headers for a given part number.
      // We looked up that part number, found an object, but it
      // wasn't of type message/rfc822.
      // Something's wrong.
      NS_ASSERTION(PR_FALSE, "object not of type message rfc822");
    }
  }
  else
    SetIsValid(PR_FALSE);
}

// Fills in buffer (and adopts storage) for MIME headers in appropriate object.
// If object can't be found, sets isValid to PR_FALSE.
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
      SetIsValid(PR_FALSE);
  }
  else
  {
    SetIsValid(PR_FALSE);
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
// inline.  Returns PR_TRUE if all parts are inline, PR_FALSE otherwise.
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
  m_isBeingGenerated = PR_TRUE;
  m_generatingPart = partNum;
  PRInt32 contentLength = 0;
  
  if (!GetIsValid() || PreflightCheckAllInline())
  {
    // We don't have a valid shell, or all parts are going to be inline anyway.  Fall back to fetching the whole message.
#ifdef DEBUG_chrisf
    NS_ASSERTION(GetIsValid());
#endif
    m_generatingWholeMessage = PR_TRUE;
    PRUint32 messageSize = m_protocolConnection->GetMessageSize(GetUID().get(), PR_TRUE);
    m_protocolConnection->SetContentModified(IMAP_CONTENT_NOT_MODIFIED);	// So that when we cache it, we know we have the whole message
    if (!DeathSignalReceived())
      m_protocolConnection->FallbackToFetchWholeMsg(GetUID().get(), messageSize);
    contentLength = (PRInt32) messageSize;	// ugh
  }
  else
  {
    // We have a valid shell.
    PRBool streamCreated = PR_FALSE;
    m_generatingWholeMessage = PR_FALSE;
    
    ////// PASS 1 : PREFETCH ///////
    // First, prefetch any additional headers/data that we need
    if (!GetPseudoInterrupted())
      m_message->Generate(PR_FALSE, PR_TRUE);	// This queues up everything we need to prefetch
    // Now, run a single pipelined prefetch  (neato!)
    FlushPrefetchQueue();
    
    ////// PASS 2 : COMPUTE STREAM SIZE ///////
    // Next, figure out the size from the parts that we're going to fill in,
    // plus all of the MIME headers, plus the message header itself
    if (!GetPseudoInterrupted())
      contentLength = m_message->Generate(PR_FALSE, PR_FALSE);
    
    // Setup the stream
    if (!GetPseudoInterrupted() && !DeathSignalReceived())
    {
      nsresult rv = 
        m_protocolConnection->BeginMessageDownLoad(contentLength, MESSAGE_RFC822);
      if (NS_FAILED(rv))
      {
        m_generatingPart = nsnull;
        m_protocolConnection->AbortMessageDownLoad();
        return 0;
      }
      else
      {
        streamCreated = PR_TRUE;
      }
    }
    
    ////// PASS 3 : GENERATE ///////
    // Generate the message
    if (!GetPseudoInterrupted() && !DeathSignalReceived())
      m_message->Generate(PR_TRUE, PR_FALSE);
    
    // Close the stream here - normal.  If pseudointerrupted, the connection will abort the download stream
    if (!GetPseudoInterrupted() && !DeathSignalReceived())
      m_protocolConnection->NormalMessageEndDownload();
    else if (streamCreated)
      m_protocolConnection->AbortMessageDownLoad();
    
    m_generatingPart = NULL;
    
  }
  
  m_isBeingGenerated = PR_FALSE;
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
    NS_ASSERTION(PR_FALSE, "part is not a leaf or container");
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
        return new nsIMAPBodypartMessage(shell, keepPartNum, buf, parentPart, PR_FALSE);
      }
    }
    return rv;
  }
}


nsIMAPBodypart::nsIMAPBodypart(nsIMAPBodyShell *shell, char *partNumber, const char *buf, nsIMAPBodypart *parentPart) :
nsIMAPGenericParser()
{
  SetIsValid(PR_TRUE);
  m_parentPart = parentPart;
  m_partNumberString = partNumber;	// storage adopted
  if (!shell)
  {
    SetIsValid(PR_FALSE);
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
    m_shell->SetIsValid(PR_FALSE);
  }
}

PRBool nsIMAPBodypart::GetNextLineForParser(char **nextLine)
{
  PRBool rv = PR_TRUE;
  *nextLine = m_responseBuffer;
  if (!m_responseBuffer)
    rv = PR_FALSE;
  m_responseBuffer = NULL;
  return rv;
}

PRBool nsIMAPBodypart::ContinueParse()
{
  return GetIsValid() && nsIMAPGenericParser::ContinueParse() && m_shell->GetIsValid();
}

// Adopts storage for part data buffer.  If NULL, sets isValid to PR_FALSE.
void nsIMAPBodypart::AdoptPartDataBuffer(char *buf)
{
  m_partData = buf;
  if (!m_partData)
  {
    SetIsValid(PR_FALSE);
  }
}

// Adopts storage for header data buffer.  If NULL, sets isValid to PR_FALSE.
void nsIMAPBodypart::AdoptHeaderDataBuffer(char *buf)
{
  m_headerData = buf;
  if (!m_headerData)
  {
    SetIsValid(PR_FALSE);
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
m_shell->GetConnection()->FetchMessage(m_shell->GetUID(), kMIMEHeader, PR_TRUE, 0, 0, m_partNumberString);
// m_headerLength will be filled in when it is adopted from the parser
}
if (!m_headerData)
{
SetIsValid(PR_FALSE);
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
          m_shell->GetConnection()->HandleMessageDownLoadLine(xPartHeader, PR_FALSE);
        }
        mimeHeaderLength += PL_strlen(xPartHeader);
        PR_Free(xPartHeader);
      }
    }
    
    mimeHeaderLength += PL_strlen(m_headerData);
    if (stream)
    {
      m_shell->GetConnection()->Log("SHELL","GENERATE-MIMEHeader",m_partNumberString);
      m_shell->GetConnection()->HandleMessageDownLoadLine(m_headerData, PR_FALSE);	// all one line?  Can we do that?
    }
    
    return mimeHeaderLength;
  }
  else 
  {
    SetIsValid(PR_FALSE);	// prefetch didn't adopt a MIME header
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
      m_shell->GetConnection()->HandleMessageDownLoadLine(m_partData, PR_FALSE);
    }
    return PL_strlen(m_partData);
  }
  else	// we are fetching and streaming this part's body as we go
  {
    if (stream && !m_shell->DeathSignalReceived())
    {
      char *generatingPart = m_shell->GetGeneratingPart();
      PRBool fetchingSpecificPart = (generatingPart && !PL_strcmp(generatingPart, m_partNumberString));
      
      m_shell->GetConnection()->Log("SHELL","GENERATE-Part-Inline",m_partNumberString);
      m_shell->GetConnection()->FetchTryChunking(m_shell->GetUID().get(), kMIMEPart, PR_TRUE, m_partNumberString, m_partLength, !fetchingSpecificPart);
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
        m_shell->GetConnection()->HandleMessageDownLoadLine(m_boundaryData, PR_FALSE);
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
          m_shell->GetConnection()->HandleMessageDownLoadLine(lastBoundaryData, PR_FALSE);
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
  
  const char emptyString[] = "This body part will be downloaded on demand.";
  // XP_GetString(MK_IMAP_EMPTY_MIME_PART);  ### need to be able to localize
  if (emptyString)
  {
    if (stream)
    {
      m_shell->GetConnection()->Log("SHELL","GENERATE-Filling",m_partNumberString);
      m_shell->GetConnection()->HandleMessageDownLoadLine(emptyString, PR_FALSE);
    }
    return PL_strlen(emptyString);
  }
  else
    return 0;
}


// Returns PR_TRUE if the prefs say that this content type should
// explicitly be kept in when filling in the shell
PRBool nsIMAPBodypart::ShouldExplicitlyFetchInline()
{
	 return PR_FALSE;
}


// Returns PR_TRUE if the prefs say that this content type should
// explicitly be left out when filling in the shell
PRBool nsIMAPBodypart::ShouldExplicitlyNotFetchInline()
{
  return PR_FALSE;
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
    //if (parentMessage ? !parentMessage->GetIsTopLevelMessage() : PR_TRUE)
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
      SetIsValid(PR_FALSE);
  }
  else
    SetIsValid(PR_FALSE);
  
  // body subtype  ("gif", "html", etc.)
  if (ContinueParse())
  {
    m_bodySubType = CreateNilString();
    if (ContinueParse())
      fNextToken = GetNextToken();
    else
      SetIsValid(PR_FALSE);
  }
  else
    SetIsValid(PR_FALSE);
  
  // body parameter parenthesized list
  if (ContinueParse())
  {
    if (!fNextToken)
      SetIsValid(PR_FALSE);
    else if (fNextToken[0] == '(')
    {
      if (!fNextToken[1])
        SetIsValid(PR_FALSE);
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
    SetIsValid(PR_FALSE);
  
  // body id
  if (ContinueParse())
  {
    m_bodyID = CreateNilString();
    if (ContinueParse())
      fNextToken = GetNextToken();
    else
      SetIsValid(PR_FALSE);
  }
  else
    SetIsValid(PR_FALSE);
  
  // body description
  if (ContinueParse())
  {
    m_bodyDescription = CreateNilString();
    if (ContinueParse())
      fNextToken = GetNextToken();
    else
      SetIsValid(PR_FALSE);
  }
  else
    SetIsValid(PR_FALSE);
  
  // body encoding
  if (ContinueParse())
  {
    m_bodyEncoding = CreateNilString();
    if (ContinueParse())
      fNextToken = GetNextToken();
    else
      SetIsValid(PR_FALSE);
  }
  else
    SetIsValid(PR_FALSE);
  
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
      SetIsValid(PR_FALSE);
    
    if (ContinueParse())
      fNextToken = GetNextToken();
    else
      SetIsValid(PR_FALSE);
  }
  else
    SetIsValid(PR_FALSE);
  
  
  // that's it, we can just leave the parser the way it is since
  // we'll never use it again.
  
  if (GetIsValid() && m_bodyType && m_bodySubType)
  {
    m_contentType = PR_smprintf("%s/%s",m_bodyType,m_bodySubType);
  }
  
  return PR_TRUE;
}

// returns PR_TRUE if this part should be fetched inline for generation.
PRBool	nsIMAPBodypartLeaf::ShouldFetchInline()
{
  char *generatingPart = m_shell->GetGeneratingPart();
  if (generatingPart)
  {
    // If we are generating a specific part
    if (!PL_strcmp(generatingPart, m_partNumberString))
    {
      // This is the part we're generating
      return PR_TRUE;
    }
    else
    {
      // If this is the only body part of a message, and that
      // message is the part being generated, then this leaf should
      // be inline as well.
      if ((m_parentPart->GetType() == IMAP_BODY_MESSAGE_RFC822) &&
        (!PL_strcmp(m_parentPart->GetPartNumberString(), generatingPart)))
        return PR_TRUE;
      
      // The parent of this part is a multipart
      if (m_parentPart->GetType() == IMAP_BODY_MULTIPART)
      {
        // This is the first text part of a forwarded message
        // with a multipart body, and that message is being generated,
        // then generate this part.
        nsIMAPBodypart *grandParent = m_parentPart->GetParentPart();
        NS_ASSERTION(grandParent, "grandparent doesn't exist for multi-part alt");		// grandParent must exist, since multiparts need parents
        if (grandParent && 
          (grandParent->GetType() == IMAP_BODY_MESSAGE_RFC822) &&
          (!PL_strcmp(grandParent->GetPartNumberString(), generatingPart)) &&
          (m_partNumberString[PL_strlen(m_partNumberString)-1] == '1') &&
          !PL_strcasecmp(m_bodyType, "text"))
          return PR_TRUE;	// we're downloading it inline
        
        
        // This is a child of a multipart/appledouble attachment,
        // and that multipart/appledouble attachment is being generated
        if (m_parentPart &&
          !PL_strcasecmp(m_parentPart->GetBodySubType(), "appledouble") &&
          !PL_strcmp(m_parentPart->GetPartNumberString(), generatingPart))
          return PR_TRUE;	// we're downloading it inline
      }
      
      // Leave out all other leaves if this isn't the one
      // we're generating.
      // Maybe change later to check parents, etc.
      return PR_FALSE;
    }
  }
  else
  {
    // We are generating the whole message, possibly (hopefully)
    // leaving out non-inline parts
    
    if (ShouldExplicitlyFetchInline())
      return PR_TRUE;
    if (ShouldExplicitlyNotFetchInline())
      return PR_FALSE;
    
    // If the parent is a message (this is the only body part of that
    // message), and that message should be inline, then its body
    // should inherit the inline characteristics of that message
    if (m_parentPart->GetType() == IMAP_BODY_MESSAGE_RFC822)
      return m_parentPart->ShouldFetchInline();
    
    // View Attachments As Links is on.
    if (!(m_shell->GetContentModified() == IMAP_CONTENT_MODIFIED_VIEW_INLINE))
    {
      // The last text part is still displayed inline,
      // even if View Attachments As Links is on.
      nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));
      PRBool preferPlainText = PR_FALSE;
      if (prefBranch)
        prefBranch->GetBoolPref("mailnews.display.prefer_plaintext", &preferPlainText);

      if ((preferPlainText && !PL_strcmp(m_partNumberString, "1") &&
                               !PL_strcasecmp(m_bodyType, "text"))
          || m_parentPart->IsLastTextPart(m_partNumberString))
      {
        return PR_TRUE;	// we're downloading it inline
      }
      else
      {
        // This is the first text part of a top-level multipart.
        // For instance, a message with multipart body, where the first
        // part is multipart, and this is the first leaf of that first part.
        if (m_parentPart->GetType() == IMAP_BODY_MULTIPART &&
          (PL_strlen(m_partNumberString) >= 2) &&
          !PL_strcmp(m_partNumberString + PL_strlen(m_partNumberString) - 2, ".1") &&	// this is the first text type on this level
          (!PL_strcmp(m_parentPart->GetPartNumberString(), "1") || !PL_strcmp(m_parentPart->GetPartNumberString(), "2")) &&
          !PL_strcasecmp(m_bodyType, "text"))
          return PR_TRUE;
        else
          return PR_FALSE;	// we can leave it on the server
      }
    }
#if defined(XP_MAC) || defined(XP_MACOSX)
    // If it is either applesingle, or a resource fork for appledouble
    if (!PL_strcasecmp(m_contentType, "application/applefile"))
    {
      // if it is appledouble
      if (m_parentPart->GetType() == IMAP_BODY_MULTIPART &&
        !PL_strcasecmp(m_parentPart->GetBodySubType(), "appledouble"))
      {
        // This is the resource fork of a multipart/appledouble.
        // We inherit the inline attributes of the parent,
        // which was derived from its OTHER child.  (The data fork.)
        return m_parentPart->ShouldFetchInline();
      }
      else	// it is applesingle
      {
        return PR_FALSE;	// we can leave it on the server
      }
    }
#endif	// XP_MAC
    
    // Leave out parts with type application/*
    if (!PL_strcasecmp(m_bodyType, "APPLICATION") &&	// If it is of type "application"
      PL_strncasecmp(m_bodySubType, "x-pkcs7", 7)	// and it's not a signature (signatures are inline)
      )
      return PR_FALSE;	// we can leave it on the server
    
    // Here's where we can add some more intelligence -- let's leave out
    // any other parts that we know we can't display inline.
    return PR_TRUE;	// we're downloading it inline
  }
}



PRBool nsIMAPBodypartMultipart::IsLastTextPart(const char *partNumberString)
{
 // iterate backwards over the parent's part list and if the part is
  // text, compare it to the part number string
  for (int i = m_partList->Count() - 1; i >= 0; i--)
  {
      nsIMAPBodypart *part = (nsIMAPBodypart *)(m_partList->ElementAt(i));
      if (!PL_strcasecmp(part->GetBodyType(), "text"))
        return !PL_strcasecmp(part->GetPartNumberString(), partNumberString);
  }
  return PR_FALSE;
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
      SetIsValid(PR_FALSE);
      return;
    }
  }
  m_body = NULL;
  m_headers = new nsIMAPMessageHeaders(shell, m_partNumberString, this);	// We always have a Headers object
  if (!m_headers || !m_headers->GetIsValid())
  {
    SetIsValid(PR_FALSE);
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
    // but we don't need the MIME headers of a message/rfc822 part if this content
    // type is in (part of) the main msg header. In other words, we still need
    // these MIME headers if this message/rfc822 body part is enclosed in the msg
    // body (most likely as a body part of a multipart/mixed msg).
    //       Don't fetch (bug 128888)              Do fetch (bug 168097)
    //  ----------------------------------  -----------------------------------
    //  message/rfc822  (parent part)       message/rfc822
    //   message/rfc822 <<<---               multipart/mixed  (parent part)
    //    multipart/mixed                     message/rfc822  <<<---
    //     text/html   (body text)             multipart/mixed
    //     text/plain  (attachment)             text/html   (body text)
    //     application/msword (attachment)      text/plain  (attachment)
    //                                          application/msword (attachment)
    // "<<<---" points to the part we're examining here.
    if ( PL_strcasecmp(m_bodyType, "message") || PL_strcasecmp(m_bodySubType, "rfc822") ||
      PL_strcasecmp(m_parentPart->GetBodyType(), "message") || PL_strcasecmp(m_parentPart->GetBodySubType(), "rfc822") )
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
      SetIsValid(PR_FALSE);
    
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
          SetSyntaxError(PR_TRUE);
          SetIsValid(PR_FALSE);
        }
      }
      else
        SetIsValid(PR_FALSE);
    }
    else
      SetIsValid(PR_FALSE);
    
    // ignore size in lines
  }
  
  if (!m_body || !m_body->GetIsValid())
    SetIsValid(PR_FALSE);			
  
  return GetIsValid();
}


PRBool	nsIMAPBodypartMessage::ShouldFetchInline()
{
  if (m_topLevelMessage)	// the main message should always be defined as "inline"
    return PR_TRUE;
  
  char *generatingPart = m_shell->GetGeneratingPart();
  if (generatingPart)
  {
    // If we are generating a specific part
    // Always generate containers (just don't fill them in)
    // because it is low cost (everything is cached)
    // and it gives the message its full MIME structure,
    // to avoid any potential mishap.
    return PR_TRUE;
  }
  else
  {
    // Generating whole message
    
    if (ShouldExplicitlyFetchInline())
      return PR_TRUE;
    if (ShouldExplicitlyNotFetchInline())
      return PR_FALSE;
    
    
    // Message types are inline, by default.
    return PR_TRUE;
  }
}

PRBool	nsIMAPBodypartMessage::PreflightCheckAllInline()
{
  if (!ShouldFetchInline())
    return PR_FALSE;
  
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
    SetIsValid(PR_FALSE);
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
    SetIsValid(PR_FALSE);
    return;
  }
  if (!m_parentPart)
  {
    SetIsValid(PR_FALSE);
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
    
    PRBool parentIsMessageType = GetParentPart() ? (GetParentPart()->GetType() == IMAP_BODY_MESSAGE_RFC822) : PR_TRUE;
    
    // If this is multipart/signed, then we always want to generate the MIME headers of this multipart.
    // Otherwise, we only want to do it if the parent is not of type "message"
    PRBool needMIMEHeader = !parentIsMessageType;  // !PL_strcasecmp(m_bodySubType, "signed") ? PR_TRUE : !parentIsMessageType;
    if (needMIMEHeader && !m_shell->GetPseudoInterrupted())	// not a message body's type
    {
      len += GenerateMIMEHeader(stream, prefetch);
    }
    
    if (ShouldFetchInline())
    {
      for (int i = 0; i < m_partList->Count(); i++)
      {
        if (!m_shell->GetPseudoInterrupted())
          len += GenerateBoundary(stream, prefetch, PR_FALSE);
        if (!m_shell->GetPseudoInterrupted())
          len += ((nsIMAPBodypart *)(m_partList->ElementAt(i)))->Generate(stream, prefetch);
      }
      if (!m_shell->GetPseudoInterrupted())
        len += GenerateBoundary(stream, prefetch, PR_TRUE);
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
            SetIsValid(PR_FALSE);
          }
        }
        else
        {
          SetIsValid(PR_FALSE);
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
        SetIsValid(PR_FALSE);
      }
    }
    else
    {
      SetIsValid(PR_FALSE);
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
        SetIsValid(PR_FALSE);
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
          SetIsValid(PR_FALSE);
        
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
            SetIsValid(PR_FALSE);
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
    SetIsValid(PR_FALSE);
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
    return PR_TRUE;
  }
  else
  {
    // Generating whole message
    
    if (ShouldExplicitlyFetchInline())
      return PR_TRUE;
    if (ShouldExplicitlyNotFetchInline())
      return PR_FALSE;
    
    nsIMAPBodypart *grandparentPart = m_parentPart->GetParentPart();
    
    // If "Show Attachments as Links" is on, and
    // the parent of this multipart is not a message,
    // then it's not inline.
    if (!(m_shell->GetContentModified() == IMAP_CONTENT_MODIFIED_VIEW_INLINE) &&
      (m_parentPart->GetType() != IMAP_BODY_MESSAGE_RFC822) &&
      (m_parentPart->GetType() == IMAP_BODY_MULTIPART ?
      (grandparentPart ? grandparentPart->GetType() != IMAP_BODY_MESSAGE_RFC822 : PR_TRUE)
      : PR_TRUE))
      return PR_FALSE;
    
    // multiparts are always inline (even multipart/appledouble)
    // (their children might not be, though)
    return PR_TRUE;
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
    SetIsValid(PR_FALSE);
    return;
  }
  m_partNumberString = nsCRT::strdup(partNum);
  if (!m_partNumberString)
  {
    SetIsValid(PR_FALSE);
    return;
  }
  if (!m_parentPart || !m_parentPart->GetnsIMAPBodypartMessage())
  {
    // Message headers created without a valid Message parent
    NS_ASSERTION(PR_FALSE, "creating message headers with invalid message parent");
    SetIsValid(PR_FALSE);
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
  return PR_TRUE;
}

PRBool	nsIMAPMessageHeaders::ShouldFetchInline()
{
  return m_parentPart->ShouldFetchInline();
}


///////////// nsIMAPBodyShellCache ////////////////////////////////////

#if 0  // mscott - commenting out because it does not appear to be used
static int
imap_shell_cache_strcmp (const void *a, const void *b)
{
  return PL_strcmp ((const char *) a, (const char *) b);
}
#endif

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
  delete m_shellHash;
  delete m_shellList;
}

// We'll use an LRU scheme here.
// We will add shells in numerical order, so the
// least recently used one will be in slot 0.
PRBool nsIMAPBodyShellCache::EjectEntry()
{
	if (m_shellList->Count() < 1)
		return PR_FALSE;



	nsIMAPBodyShell *removedShell = (nsIMAPBodyShell *) (m_shellList->ElementAt(0));

	m_shellList->RemoveElementAt(0);
	nsCStringKey hashKey (removedShell->GetUID());
	m_shellHash->Remove(&hashKey);
	delete removedShell;

	return PR_TRUE;
}

PRBool	nsIMAPBodyShellCache::AddShellToCache(nsIMAPBodyShell *shell)
{
	// If it's already in the cache, then just return.
	// This has the side-effect of re-ordering the LRU list
	// to put this at the top, which is good, because it's what we want.
	if (FindShellForUID(shell->GetUID(), shell->GetFolderName(), shell->GetContentModified()))
		return PR_TRUE;

	// OK, so it's not in the cache currently.

	// First, for safety sake, remove any entry with the given UID,
	// just in case we have a collision between two messages in different
	// folders with the same UID.
	nsCStringKey hashKey1(shell->GetUID());
	nsIMAPBodyShell *foundShell = (nsIMAPBodyShell *) m_shellHash->Get(&hashKey1);
	if (foundShell)
	{
		nsCStringKey hashKey(foundShell->GetUID());
		m_shellHash->Remove(&hashKey);
		m_shellList->RemoveElement(foundShell);
	}

	// Add the new one to the cache
	m_shellList->AppendElement(shell);
	
	nsCStringKey hashKey2 (shell->GetUID());
	m_shellHash->Put(&hashKey2, shell);
	shell->SetIsCached(PR_TRUE);

	// while we're not over our size limit, eject entries
	PRBool rv = PR_TRUE;
	while (GetSize() > GetMaxSize())
	{
		rv = EjectEntry();
	}

	return rv;

}

nsIMAPBodyShell *nsIMAPBodyShellCache::FindShellForUID(nsCString &UID, const char *mailboxName,
                                                       IMAP_ContentModifiedType modType)
{
	nsCStringKey hashKey(UID);
	nsIMAPBodyShell *foundShell = (nsIMAPBodyShell *) m_shellHash->Get(&hashKey);

	if (!foundShell)
		return nsnull;

  // Make sure the content-modified types are compatible.
  // This allows us to work seamlessly while people switch between
  // View Attachments Inline and View Attachments As Links.
  // Enforce the invariant that any cached shell we use
  // match the current content-modified settings.
  if (modType != foundShell->GetContentModified())
    return nsnull;

	// mailbox names must match also.
	if (PL_strcmp(mailboxName, foundShell->GetFolderName()))
		return nsnull;

	// adjust the LRU stuff
	m_shellList->RemoveElement(foundShell);	// oh well, I suppose this defeats the performance gain of the hash if it actually is found
	m_shellList->AppendElement(foundShell);		// Adds to end
	
	return foundShell;
}

nsIMAPBodyShell *nsIMAPBodyShellCache::FindShellForUID(PRUint32 UID, const char *mailboxName,
                                                       IMAP_ContentModifiedType modType)
{
	nsCString uidString;
	
	uidString.AppendInt(UID);
	nsIMAPBodyShell *rv = FindShellForUID(uidString, mailboxName, modType);
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
    int n = Count();
	for (int i = 0; i < n; i++)
	{
		nsIMAPMessagePartID *part = GetPart(i);
		delete part;
	}
    Clear();
}
