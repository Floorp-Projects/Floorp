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
//
// This class allows you to output 
#ifndef _nsIMimeEmitter_h_
#define _nsIMimeEmitter_h_

#include "prtypes.h"
#include "nsINetOStream.h"

// {D01D7B59-DCCD-11d2-A411-00805F613C79}
#define NS_IMIME_EMITTER_IID \
      { 0xd01d7b59, 0xdccd, 0x11d2,   \
      { 0xa4, 0x11, 0x0, 0x80, 0x5f, 0x61, 0x3c, 0x79 } };

// {F0A8AF16-DCCE-11d2-A411-00805F613C79}
#define NS_HTML_MIME_EMITTER_CID   \
    { 0xf0a8af16, 0xdcce, 0x11d2,         \
    { 0xa4, 0x11, 0x0, 0x80, 0x5f, 0x61, 0x3c, 0x79 } };

// {977E418F-E392-11d2-A2AC-00A024A7D144}
#define NS_XML_MIME_EMITTER_CID   \
    { 0x977e418f, 0xe392, 0x11d2, \
    { 0xa2, 0xac, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } };

class nsIMimeEmitter : public nsISupports {
public: 
static const nsIID& GetIID() { static nsIID iid = NS_IMIME_EMITTER_IID; return iid; }

    // These will be called to start and stop the total operation
    NS_IMETHOD    Initialize(nsINetOStream *outStream) = 0;
    NS_IMETHOD    Complete() = 0;

    // Set the output stream for processed data.
    NS_IMETHOD    SetOutputStream(nsINetOStream *outStream) = 0;

    // Header handling routines.
    NS_IMETHOD    StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID) = 0;
    NS_IMETHOD    AddHeaderField(const char *field, const char *value) = 0;
    NS_IMETHOD    EndHeader() = 0;

    // Attachment handling routines
    NS_IMETHOD    StartAttachment(const char *name, const char *contentType, const char *url) = 0;
    NS_IMETHOD    AddAttachmentField(const char *field, const char *value) = 0;
    NS_IMETHOD    EndAttachment() = 0;

    // Body handling routines
    NS_IMETHOD    StartBody(PRBool bodyOnly, const char *msgID) = 0;
    NS_IMETHOD    WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten) = 0;
    NS_IMETHOD    EndBody() = 0;

    // Generic write routine. This is necessary for output that
    // libmime needs to pass through without any particular parsing
    // involved (i.e. decoded images, HTML Body Text, etc...
    NS_IMETHOD    Write(const char *buf, PRUint32 size, PRUint32 *amountWritten) = 0;
    NS_IMETHOD    UtilityWrite(const char *buf) = 0;
};

#endif /* _nsIMimeEmitter_h_ */
