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
#ifndef _nsMimeHtmlEmitter_h_
#define _nsMimeHtmlEmitter_h_

#include "prtypes.h"
#include "prio.h"
#include "nsMimeBaseEmitter.h"
#include "nsMimeRebuffer.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIChannel.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMimeConverter.h"

class nsMimeHtmlDisplayEmitter : public nsMimeBaseEmitter {
public: 
    nsMimeHtmlDisplayEmitter ();
    nsresult Init();

    virtual       ~nsMimeHtmlDisplayEmitter (void);

    // Header handling routines.
    NS_IMETHOD    EndHeader();

    // Attachment handling routines
    NS_IMETHOD    StartAttachment(const char *name, const char *contentType, const char *url,
                                  PRBool aNotDownloaded);
    NS_IMETHOD    AddAttachmentField(const char *field, const char *value);
    NS_IMETHOD    EndAttachment();
	NS_IMETHOD	  EndAllAttachments();

    // Body handling routines
    NS_IMETHOD    WriteBody(const char *buf, PRUint32 size, PRUint32 *amountWritten);
    NS_IMETHOD    EndBody();

    virtual nsresult            WriteHeaderFieldHTMLPrefix();
    virtual nsresult            WriteHeaderFieldHTML(const char *field, const char *value);
    virtual nsresult            WriteHeaderFieldHTMLPostfix();
    virtual nsresult            WriteHTMLHeaders();

protected:
    PRBool        mFirst;  // Attachment flag...
    PRBool        mSkipAttachment;  // attachments we shouldn't show...

    nsCOMPtr<nsIMsgHeaderSink> mHeaderSink;

    nsresult GetHeaderSink(nsIMsgHeaderSink ** aHeaderSink);
    PRBool BroadCastHeadersAndAttachments();
    nsresult StartAttachmentInBody(const char *name, const char *contentType, const char *url);
};


#endif /* _nsMimeHtmlEmitter_h_ */
