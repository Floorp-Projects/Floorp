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
#ifndef nsPop3Sink_h__
#define nsPop3Sink_h__

#include "nscore.h"
#include "nsIURL.h"
#include "nsIPop3Sink.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prenv.h"

class nsParseNewMailState;

class nsPop3Sink : public nsIPop3Sink
{
public:
    nsPop3Sink();
    virtual ~nsPop3Sink();

    NS_DECL_ISUPPORTS

    NS_IMETHOD SetUserAuthenticated(PRBool authed);
    NS_IMETHOD SetMailAccountURL(const char* urlString);
    NS_IMETHOD BeginMailDelivery(PRBool *aBool);
    NS_IMETHOD EndMailDelivery();
    NS_IMETHOD AbortMailDelivery();
    NS_IMETHOD IncorporateBegin(const char* uidlString, nsIURL* aURL, 
                                PRUint32 flags, void** closure);
    NS_IMETHOD IncorporateWrite(void* closure, const char* block,
                                PRInt32 length);
    NS_IMETHOD IncorporateComplete(void* closure);
    NS_IMETHOD IncorporateAbort(void* closure, PRInt32 status);
    NS_IMETHOD BiffGetNewMail();
    NS_IMETHOD SetBiffStateAndUpdateFE(PRUint32 aBiffState);
    NS_IMETHOD SetSenderAuthedFlag(void* closure, PRBool authed);
    NS_IMETHOD SetPopServer(nsIPop3IncomingServer *server);
    NS_IMETHOD GetPopServer(nsIPop3IncomingServer* *server);

    static char*  GetDummyEnvelope(void);
    
protected:

	nsresult WriteLineToMailbox(char *buffer);

    PRBool m_authed;
    char* m_accountUrl;
    PRUint32 m_biffState;
    PRBool m_senderAuthed;
    char* m_outputBuffer;
    PRInt32 m_outputBufferSize;
    char* m_mailDirectory;
    nsIPop3IncomingServer *m_popServer;
	nsParseNewMailState	*m_newMailParser;
#ifdef DEBUG
    PRInt32 m_fileCounter;
#endif
    nsOutputFileStream* m_outFileStream;
};

#endif
