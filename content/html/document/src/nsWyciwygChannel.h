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

#include "nsIWyciwygChannel.h"
#include "nsString.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsWyciwygProtocolHandler.h"
#include "nsIStreamListener.h"
#include "nsICacheListener.h"
#include "nsITransport.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIOutputStream.h"
#include "nsIProgressEventSink.h"
#include "prlog.h"

extern PRLogModuleInfo * gWyciwygLog;

#define LOG(args)  PR_LOG(gWyciwygLog, 4, args)

class nsWyciwygChannel: public nsIWyciwygChannel,                       
                        public nsIStreamListener,
                        public nsIInterfaceRequestor,
                        public nsICacheListener,
                        public nsIProgressEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIWYCIWYGCHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSICACHELISTENER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIPROGRESSEVENTSINK

    // nsWyciwygChannel methods:
    nsWyciwygChannel();
    virtual ~nsWyciwygChannel();

    nsresult Init(nsIURI* uri);

protected:
    nsresult AsyncAbort(nsresult rv);
    nsresult Connect(PRBool firstTime);
    nsresult ReadFromCache();
    nsresult OpenCacheEntry(const char * aCacheKey, nsCacheAccessMode aWriteAccess, PRBool * aDelayFlag = nsnull);
       
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;    
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsISupports>               mListenerContext;
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    
    // Cache related stuff    
    nsCOMPtr<nsITransport>              mCacheTransport;
    nsCOMPtr<nsIRequest>                mCacheReadRequest;
    nsCOMPtr<nsICacheEntryDescriptor>   mCacheEntry;
    nsCOMPtr<nsIOutputStream>           mCacheOutputStream;
    
    // flags
    PRUint32                            mStatus;
    PRUint32                            mLoadFlags;
    PRPackedBool                        mIsPending;
};

#endif /* nsWyciwygChannel_h___ */
