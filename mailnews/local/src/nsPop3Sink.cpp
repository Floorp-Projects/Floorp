/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsPop3Sink.h"
#include "prprf.h"
#include "prlog.h"
#include "nscore.h"
#include <stdio.h>
#include <time.h>


static NS_DEFINE_IID(kIPop3SinkIID, NS_IPOP3SINK_IID);

NS_IMPL_ISUPPORTS(nsPop3Sink, kIPop3SinkIID);

nsPop3Sink::nsPop3Sink()
{
    m_authed = PR_FALSE;
    m_accountUrl = nsnull;
    m_biffState = 0;
    m_senderAuthed = PR_FALSE;
    m_outputBuffer = nsnull;
    m_outputBufferSize = 0;
    m_mailDirectory = 0;
    m_fileCounter = 0;
}

nsPop3Sink::~nsPop3Sink()
{
    PR_FREEIF(m_accountUrl);
    PR_FREEIF(m_outputBuffer);
    PR_FREEIF(m_mailDirectory);
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
    char *path = PR_smprintf("%s\\Inbox", m_mailDirectory);
#ifdef DEBUG
    m_fileCounter++;
#endif
    nsFilePath filePath(path);
    m_outFileStream = new nsOutputFileStream(filePath, 
                                             PR_WRONLY | PR_CREATE_FILE | PR_APPEND);
    PR_FREEIF(path);

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
        m_outFileStream->close();
        delete m_outFileStream;
        m_outFileStream = 0;
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
        m_outFileStream->close();
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

    *m_outFileStream << dummyEnvelope;
    *m_outFileStream << "X-Mozilla-Status: 8000\r\n";
    *m_outFileStream << "X-Mozilla-Status2: 00000000\r\n";

    return NS_OK;
}

nsresult
nsPop3Sink::SetMailDirectory(const char* path)
{
    PR_FREEIF(m_mailDirectory);
    if (path)
        m_mailDirectory = PL_strdup(path);
    return NS_OK;
}

nsresult
nsPop3Sink::GetMailDirectory(const char** path)
{
    if (path)
        *path = m_mailDirectory;
    return NS_OK;
}

#ifndef CR
#define CR '\r'
#define LF '\n'
#define LINEBREAK "\r\n"
#define LINEBREAK_LEN 2
#endif

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
  PL_strcpy(result + 7 + 24, LINEBREAK);
  return result;
}

nsresult
nsPop3Sink::IncorporateWrite(void* closure,
                             const char* block,
                             PRInt32 length)
{
    if(length > 1 && *block == '.' && 
       (*(block+1) == '\r' || *(block+1) == '\n')) return NS_OK;

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
        memcpy(m_outputBuffer, block, length);
        *(m_outputBuffer + length) = 0;
#ifdef DEBUG
        printf("%s\n", m_outputBuffer);
#endif 
        if (m_outFileStream)
            *m_outFileStream << m_outputBuffer;
    }
    return NS_OK;
}

nsresult
nsPop3Sink::IncorporateComplete(void* closure)
{
    if (m_outFileStream)
        *m_outFileStream << LINEBREAK;

#ifdef DEBUG
    printf("Incorporate message complete.\n");
#endif 
    return NS_OK;
}

nsresult
nsPop3Sink::IncorporateAbort(void* closure, PRInt32 status)
{
    if (m_outFileStream)
        *m_outFileStream << LINEBREAK;

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
        printf("You have got mail!\n");
        break;
    case 2:
        printf("You have no mail.\n");
        break;
    }
#endif 
    return NS_OK;
}
