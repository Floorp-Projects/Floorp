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

// A datetime channel retrieves date time information from 
// RFC 867 compliant datetime servers. The date/time returned
// to the caller is of MIME type "text/plain".

#ifndef nsDateTimeChannel_h___
#define nsDateTimeChannel_h___

#include "nsString.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsDateTimeHandler.h"
#include "nsIStreamListener.h"


class nsDateTimeChannel 
: public nsIChannel, 
  public nsIStreamListener {

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMOBSERVER

    // nsDateTimeChannel methods:
    nsDateTimeChannel();
    virtual ~nsDateTimeChannel();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    nsresult Init(nsIURI* uri);

protected:
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIURI>                    mUrl;
    nsCOMPtr<nsIStreamListener>         mListener;
    PRUint32                            mLoadAttributes;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCString                           mContentType;
    PRInt32                             mContentLength;
    nsCOMPtr<nsISupports>               mOwner; 

    PRInt32                             mPort;
    nsXPIDLCString                      mHost;
};

#endif /* nsDateTimeChannel_h___ */
