/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsWyciwygChannel_h___
#define nsWyciwygChannel_h___

#include "nsWyciwygProtocolHandler.h"
#include "nsXPIDLString.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "prlog.h"

#include "nsIWyciwygChannel.h"
#include "nsILoadGroup.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIStreamListener.h"
#include "nsICacheListener.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIURI.h"

extern PRLogModuleInfo * gWyciwygLog;

//-----------------------------------------------------------------------------

class nsWyciwygChannel: public nsIWyciwygChannel,
                        public nsIStreamListener,
                        public nsICacheListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIWYCIWYGCHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICACHELISTENER

    // nsWyciwygChannel methods:
    nsWyciwygChannel();
    virtual ~nsWyciwygChannel();

    nsresult Init(nsIURI *uri);

protected:
    nsresult ReadFromCache();
    nsresult OpenCacheEntry(const char * aCacheKey, nsCacheAccessMode aWriteAccess, PRBool * aDelayFlag = nsnull);
       
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsISupports>               mListenerContext;
    nsCString                           mContentType;
    nsCString                           mContentCharset;
    PRInt32                             mContentLength;
    PRUint32                            mLoadFlags;
    nsresult                            mStatus;
    PRBool                              mIsPending;

    // reuse as much of this channel implementation as we can
    nsCOMPtr<nsIInputStreamPump>        mPump;
    
    // Cache related stuff    
    nsCOMPtr<nsICacheEntryDescriptor>   mCacheEntry;
    nsCOMPtr<nsIOutputStream>           mCacheOutputStream;
    nsCOMPtr<nsIInputStream>            mCacheInputStream;
};

#endif /* nsWyciwygChannel_h___ */
