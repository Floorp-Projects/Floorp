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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsIPipe.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"

class nsMimeBaseEmitter : public nsIMimeEmitter, nsIPipeObserver {
public: 
    nsMimeBaseEmitter ();
    virtual       ~nsMimeBaseEmitter (void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    NS_DECL_NSIMIMEEMITTER
    NS_DECL_NSIPIPEOBSERVER

    NS_IMETHOD    UtilityWriteCRLF(const char *buf);

    // For cacheing string bundles...
    char          *MimeGetStringByName(const char *aHeaderName);
    char          *LocalizeHeaderName(const char *aHeaderName, const char *aDefaultName);

protected:
    // For buffer management on output
    MimeRebuffer        *mBufferMgr;

    nsCOMPtr<nsIStringBundle>	m_stringBundle;     // For string bundle usage...


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

  // the url for the data being processed...
  nsIURI              *mURL;

  // The setting for header output...
  nsIPref             *mPrefs;          /* Connnection to prefs service manager */
  PRInt32             mHeaderDisplayType; 
};

#endif /* _nsMimeBaseEmitter_h_ */
