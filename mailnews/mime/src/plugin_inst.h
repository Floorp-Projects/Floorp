/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef plugin_inst_h_
#define plugin_inst_h_

#include "nsIMimeEmitter.h"
#include "nsINetOStream.h"
#include "nsINetPluginInstance.h"

// The ProgID for message/rfc822 messages
#define	PROGRAM_ID	        "message/rfc822"

// {B21EDB21-D10C-11d2-B373-525400E2D63A}
#define NS_INET_PLUGIN_MIME_CID          \
    { 0xb21edb21, 0xd10c, 0x11d2,         \
    { 0xb3, 0x73, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } };

class MimePluginInstance : public nsINetPluginInstance, public nsINetOStream {
public:
//static const nsIID& IID() { static nsIID iid = NS_INETPLUGININSTANCE_IID; return iid; }
 
    MimePluginInstance(void);
    virtual ~MimePluginInstance(void);

    NS_DECL_ISUPPORTS

    // from nsINetPluginInstance
    NS_IMETHOD    Initialize(nsINetOStream* out_stream, const char *stream_name);
    NS_IMETHOD    GetMIMEOutput(const char* *result);
    NS_IMETHOD    Start(void);
    NS_IMETHOD    Stop(void);
    NS_IMETHOD    Destroy(void);

    //nsINetOStream interface
    NS_IMETHOD    Complete(void);
    NS_IMETHOD    Abort(PRInt32 status);
    NS_IMETHOD    WriteReady(PRUint32 *aReadyCount);
  
    // nsIOutputStream interface
    NS_IMETHOD    Write(const char  *aBuf,
                        PRUint32    aLen,
                        PRUint32    *aWriteLength);

    // nsIBaseStream interface
    NS_IMETHOD    Close(void);

    ////////////////////////////////////////////////////////////////////////////
    // MimePluginInstance specific methods:
    ////////////////////////////////////////////////////////////////////////////
    NS_IMETHOD    InternalCleanup(void);


    // Counter variable
    PRInt32         mTotalRead;

    nsINetOStream   *mOutStream;
    void            *mBridgeStream;
    nsIMimeEmitter  *mEmitter;
};

/* this function will be used by the factory to generate an class access object....*/
extern "C" nsresult NS_NewMimePluginInstance(MimePluginInstance **aInstancePtrResult);

#endif /* plugin_inst_h_ */
