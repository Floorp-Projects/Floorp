/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsIMsgFolder.h"
#include "nsXPIDLString.h"

class nsParseNewMailState;
class nsIFolder;

class nsPop3Sink : public nsIPop3Sink
{
public:
    nsPop3Sink();
    virtual ~nsPop3Sink();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPOP3SINK
    nsresult	GetServerFolder(nsIFolder **aFolder);

    static char*  GetDummyEnvelope(void);
    
protected:

	nsresult WriteLineToMailbox(char *buffer);

    PRBool m_authed;
    char* m_accountUrl;
    PRUint32 m_biffState;
    PRBool m_senderAuthed;
    char* m_outputBuffer;
    PRInt32 m_outputBufferSize;
    nsIPop3IncomingServer *m_popServer;
	//Currently the folder we want to update about biff info
	nsIMsgFolder *m_folder;
	nsParseNewMailState	*m_newMailParser;
#ifdef DEBUG
    PRInt32 m_fileCounter;
#endif
    nsIOFileStream* m_outFileStream;
    PRBool m_buildMessageUri;
    nsCString m_messageUri;
    nsXPIDLCString m_baseMessageUri;
};

#endif
