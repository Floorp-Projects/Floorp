/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef _nsMimeXmlEmitter_h_
#define _nsMimeXmlEmitter_h_

#include "prtypes.h"
#include "prio.h"
#include "nsMimeBaseEmitter.h"
#include "nsMimeRebuffer.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIURI.h"
#include "nsIChannel.h"

class nsMimeXmlEmitter : public nsMimeBaseEmitter {
public: 
    nsMimeXmlEmitter ();
    virtual       ~nsMimeXmlEmitter (void);

    NS_IMETHOD    Complete();

    // Header handling routines.
    NS_IMETHOD    StartHeader(PRBool rootMailHeader, PRBool headerOnly, const char *msgID,
                              const char *outCharset);
    NS_IMETHOD    AddHeaderField(const char *field, const char *value);
    NS_IMETHOD    EndHeader();

    // Attachment handling routines
    NS_IMETHOD    StartAttachment(const char *name, const char *contentType, const char *url,
                                  PRBool aNotDownloaded);
    NS_IMETHOD    AddAttachmentField(const char *field, const char *value);
    NS_IMETHOD    EndAttachment();

    NS_IMETHOD    WriteXMLHeader(const char *msgID);
    NS_IMETHOD    WriteXMLTag(const char *tagName, const char *value);

protected:

    // For header determination...
    PRBool              mXMLHeaderStarted; 
    PRInt32             mAttachCount;
};

#endif /* _nsMimeXmlEmitter_h_ */
