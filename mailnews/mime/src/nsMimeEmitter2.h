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
#ifndef _nsMimeEmitter2_h_
#define _nsMimeEmitter2_h_

#include "prtypes.h"
#include "prio.h"
#include "nsMimeRebuffer.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
//#include "nsIURL.h"

class nsMimeEmitter2 {
public: 
    nsMimeEmitter2 ();
    virtual       ~nsMimeEmitter2 (void);

    // These will be called to start and stop the total operation
    NS_IMETHOD    Initialize(nsIURI *url);
    NS_IMETHOD    Complete();

    // Set the output stream/listener for processed data.
    NS_IMETHOD    SetOutputStream(nsIOutputStream *outStream);
    NS_IMETHOD    SetOutputListener(nsIStreamListener *listener);

    // Header handling routines.
    NS_IMETHOD    StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                              const char *outCharset);
    NS_IMETHOD    AddHeaderField(const char *field, const char *value);
    NS_IMETHOD    EndHeader();

    // Attachment handling routines
    NS_IMETHOD    StartAttachment(const char *name, const char *contentType, const char *url);
    NS_IMETHOD    AddAttachmentField(const char *field, const char *value);
    NS_IMETHOD    EndAttachment();

    // Body handling routines
    NS_IMETHOD    StartBody(PRBool bodyOnly, const char *msgID, const char *outCharset);
    NS_IMETHOD    WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten);
    NS_IMETHOD    EndBody();

    // Generic write routine. This is necessary for output that
    // libmime needs to pass through without any particular parsing
    // involved (i.e. decoded images, HTML Body Text, etc...
    NS_IMETHOD    Write(const char *buf, PRUint32 size, PRUint32 *amountWritten);
    NS_IMETHOD    UtilityWrite(const char *buf);

    NS_IMETHOD    ProcessContentType(const char *ct);

protected:
    // For buffer management on output
    MimeRebuffer  *mBufferMgr;

    // For the output stream
    nsIOutputStream     *mOutStream;
    nsIStreamListener   *mOutListener;
    PRUint32      mTotalWritten;
    PRUint32      mTotalRead;

    // For header determination...
    PRBool        mDocHeader;

    // For content type...
    char          *mAttachContentType;

    // the url for the data being processed...
    nsIURI        *mURL;

    // The setting for header output...
    PRInt32       mHeaderDisplayType;

#ifdef DEBUG_rhp
    PRBool        mReallyOutput;
    PRFileDesc    *mLogFile;        /* Temp file to put generated HTML into. */ 
#endif 
};

#endif /* _nsMimeEmitter2_h_ */
