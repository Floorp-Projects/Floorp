/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"    // precompiled header...
#include "nsPop3Sink.h"
#include "prprf.h"
#include "prlog.h"
#include "nscore.h"
#include <stdio.h>
#include <time.h>
#include "nsParseMailbox.h"

NS_IMPL_ISUPPORTS(nsPop3Sink, nsIPop3Sink::GetIID());

nsPop3Sink::nsPop3Sink()
{
    NS_INIT_REFCNT();
    m_authed = PR_FALSE;
    m_accountUrl = nsnull;
    m_biffState = 0;
    m_senderAuthed = PR_FALSE;
    m_outputBuffer = nsnull;
    m_outputBufferSize = 0;
    m_mailDirectory = 0;
    m_newMailParser = NULL;
#ifdef DEBUG
    m_fileCounter = 0;
#endif
    m_popServer = nsnull;
    m_outFileStream = nsnull;
}

nsPop3Sink::~nsPop3Sink()
{
    PR_FREEIF(m_accountUrl);
    PR_FREEIF(m_outputBuffer);
    PR_FREEIF(m_mailDirectory);
    NS_IF_RELEASE(m_popServer);
	if (m_newMailParser)
		delete m_newMailParser;
}

nsresult
nsPop3Sink::SetUserAuthenticated(PRBool authed)
{
#ifdef DEBUG
    if (authed)
        printf("User is authenticated. \n");
    else
        printf("User is NOT authenticated. \n");
#endif 
    m_authed = authed;
    return NS_OK;
}

nsresult
nsPop3Sink::SetSenderAuthedFlag(void* closure, PRBool authed)
{
#ifdef DEBUG
    if (authed)
        printf("Sender is authenticated. \n");
    else
        printf("Sender is NOT authenticated. \n");
#endif 
    m_authed = authed;
    return NS_OK;
    
}

nsresult 
nsPop3Sink::SetMailAccountURL(const char* urlString)
{
    if (urlString)
    {
        PR_FREEIF(m_accountUrl);
#ifdef DEBUG
        printf("Mail account URL: %s\n", urlString);
#endif 
        m_accountUrl = PL_strdup(urlString);
    }
    else
    {
#ifdef DEBUG
        printf("Null mail account URL.\n");
#endif 
    }

    return NS_OK;
}

nsresult 
nsPop3Sink::BeginMailDelivery(PRBool* aBool)
{
#ifdef DEBUG
    m_fileCounter++;
#endif

    nsFileSpec fileSpec(m_mailDirectory);
    fileSpec += "Inbox";
    m_outFileStream = new nsOutputFileStream(fileSpec, 
                                             PR_WRONLY | PR_CREATE_FILE | PR_APPEND);

	// create a new mail parser
    m_newMailParser = new nsParseNewMailState;
    if (m_newMailParser == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = m_newMailParser->Init(nsnull, fileSpec);
    if (NS_FAILED(rv)) return rv;

#ifdef DEBUG
    printf("Begin mail message delivery.\n");
#endif 
    if (aBool)
        *aBool = PR_TRUE;
    return NS_OK;
}

nsresult
nsPop3Sink::EndMailDelivery()
{
    if (m_outFileStream)
    {
        delete m_outFileStream;
        m_outFileStream = 0;
    }
	if (m_newMailParser)
	{
		m_newMailParser->OnStopBinding(nsnull, NS_OK, nsnull);
		delete m_newMailParser;
		m_newMailParser = NULL;
	}

#ifdef DEBUG
    printf("End mail message delivery.\n");
#endif 
    return NS_OK;
}

nsresult 
nsPop3Sink::AbortMailDelivery()
{
    if (m_outFileStream)
    {
        delete m_outFileStream;
        m_outFileStream = 0;
    }
#ifdef DEBUG
    printf("Abort mail message delivery.\n");
#endif 
    return NS_OK;
}

nsresult
nsPop3Sink::IncorporateBegin(const char* uidlString,
                             nsIURL* aURL,
                             PRUint32 flags,
                             void** closure)
{
#ifdef DEBUG
    printf("Incorporate message begin:\n");
    if (uidlString)
        printf("uidl string: %s\n", uidlString);
#endif 
    if (closure)
        *closure = (void*) this;
    PR_ASSERT(m_mailDirectory);
    
    char *dummyEnvelope = GetDummyEnvelope();

    WriteLineToMailbox(dummyEnvelope);
    WriteLineToMailbox("X-Mozilla-Status: 8000\r\n");
    WriteLineToMailbox("X-Mozilla-Status2: 00000000\r\n");

    return NS_OK;
}

nsresult
nsPop3Sink::SetPopServer(nsIPop3IncomingServer *server)
{
  NS_IF_RELEASE(m_popServer);
  m_popServer=server;
  NS_ADDREF(m_popServer);
  
  PR_FREEIF(m_mailDirectory);
  m_popServer->GetRootFolderPath(&m_mailDirectory);

  return NS_OK;
}

nsresult
nsPop3Sink::GetPopServer(nsIPop3IncomingServer* *server)
{
    if (!server) return NS_ERROR_NULL_POINTER;
    *server = m_popServer;
    if (*server) NS_ADDREF(*server);
    return NS_OK;
}

char*
nsPop3Sink::GetDummyEnvelope(void)
{
  static char result[75];
  char *ct;
  time_t now = time ((time_t *) 0);
#if defined (XP_WIN)
  if (now < 0 || now > 0x7FFFFFFF)
	  now = 0x7FFFFFFF;
#endif
  ct = ctime(&now);
  PR_ASSERT(ct[24] == CR || ct[24] == LF);
  ct[24] = 0;
  /* This value must be in ctime() format, with English abbreviations.
	 strftime("... %c ...") is no good, because it is localized. */
  PL_strcpy(result, "From - ");
  PL_strcpy(result + 7, ct);
  PL_strcpy(result + 7 + 24, MSG_LINEBREAK);
  return result;
}

nsresult
nsPop3Sink::IncorporateWrite(void* closure,
                             const char* block,
                             PRInt32 length)
{
    if(length > 1 && *block == '.' && 
       (*(block+1) == '\r' || *(block+1) == '\n')) return NS_OK;

	PRInt32 blockOffset = 0;
	if (!PL_strncmp(block, "From ", 5))
	{
		length++;
		blockOffset = 1;
	}
    if (!m_outputBuffer || length > m_outputBufferSize)
    {
        if (!m_outputBuffer)
            m_outputBuffer = (char*) PR_MALLOC(length+1);
        else
            m_outputBuffer = (char*) PR_REALLOC(m_outputBuffer, length+1);

        m_outputBufferSize = length;
    }
    if (m_outputBuffer)
    {
		if (blockOffset == 1)
			*m_outputBuffer = '>';
        memcpy(m_outputBuffer + blockOffset, block, length - blockOffset);
        *(m_outputBuffer + length) = 0;
		WriteLineToMailbox (m_outputBuffer);
		// Is this where we should hook up the new mail parser? Is this block a line, or a real block?
		// I think it's a real line. We're also not escaping lines that start with "From ", which is
		// a potentially horrible bug...Should this be done here, or in the mailbox parser? I vote for
		// here. Also, we're writing out the mozilla-status line in IncorporateBegin, but we need to 
		// pass that along to the mailbox parser so that the mozilla-status offset is handled correctly.
		// And what about uidl? Don't we need to be able to write out an X-UIDL header?
    }
    return NS_OK;
}

nsresult nsPop3Sink::WriteLineToMailbox(char *buffer)
{
	if (buffer)
	{
		if (m_newMailParser)
			m_newMailParser->HandleLine(buffer, PL_strlen(buffer));
		if (m_outFileStream)
			*m_outFileStream << buffer;
	}
	return NS_OK;
}

nsresult
nsPop3Sink::IncorporateComplete(void* closure)
{
	WriteLineToMailbox(MSG_LINEBREAK);
//    if (m_outFileStream)
//        *m_outFileStream << MSG_LINEBREAK;

#ifdef DEBUG
    printf("Incorporate message complete.\n");
#endif 
    return NS_OK;
}

nsresult
nsPop3Sink::IncorporateAbort(void* closure, PRInt32 status)
{
	WriteLineToMailbox(MSG_LINEBREAK);

#ifdef DEBUG
    printf("Incorporate message abort.\n");
#endif 
    return NS_OK;
}

nsresult
nsPop3Sink::BiffGetNewMail()
{
#ifdef DEBUG
    printf("Biff get new mail.\n");
#endif 
    return NS_OK;
}

nsresult
nsPop3Sink::SetBiffStateAndUpdateFE(PRUint32 aBiffState)
{
#ifdef DEBUG
    printf("Set biff state: %d\n", aBiffState);
#endif 
    m_biffState = aBiffState;
#ifdef DEBUG
    switch (aBiffState)
    {
    case 0:
    default:
        printf("Excuse me, Sir. I have no idea.\n");
        break;
    case 1:
        printf("Ya'll got mail!\n");
        break;
    case 2:
        printf("You have no mail.\n");
        break;
    }
#endif 
    return NS_OK;
}
