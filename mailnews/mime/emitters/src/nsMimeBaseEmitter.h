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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _nsMimeBaseEmitter_h_
#define _nsMimeBaseEmitter_h_

#include "prtypes.h"
#include "prio.h"
#include "nsIMimeEmitter.h"
#include "nsMimeRebuffer.h"
#include "nsIStreamListener.h"
#include "nsIOutputStream.h"
#include "nsIURI.h"
#include "nsIPref.h"
#include "nsIChannel.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMsgHeaderParser.h"

class nsMimeBaseEmitter : public nsIMimeEmitter {
public: 
    nsMimeBaseEmitter ();
    virtual       ~nsMimeBaseEmitter (void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    NS_DECL_NSIMIMEEMITTER

    NS_IMETHOD    UtilityWriteCRLF(const char *buf);

protected:
    // For buffer management on output
    MimeRebuffer        *mBufferMgr;

	// mscott - dont ref count the streams....the emitter is owned by the converter
	// which owns these streams...
    nsIOutputStream     *mOutStream;
	nsIInputStream	    *mInputStream;
    nsIStreamListener   *mOutListener;
	nsIChannel			    *mChannel;

    PRUint32            mTotalWritten;
    PRUint32            mTotalRead;

    // For header determination...
    PRBool              mDocHeader;

    // For content type...
    char                *mAttachContentType;

    // the url for the data being processed...
    nsIURI              *mURL;

    // The setting for header output...
    nsIPref             *mPrefs;          /* Connnection to prefs service manager */
    PRInt32             mHeaderDisplayType; 
#ifdef DEBUG_rhp
    PRBool              mReallyOutput;
    PRFileDesc          *mLogFile;        /* Temp file to put generated HTML into. */ 
#endif 
};

#endif /* _nsMimeBaseEmitter_h_ */
