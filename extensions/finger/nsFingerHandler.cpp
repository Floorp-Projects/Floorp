/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Brian Ryner.
 * Portions created by Brian Ryner are Copyright (C) 2000 Brian Ryner.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *  Brian Ryner <bryner@uiuc.edu>
 */

#include "nspr.h"
#include "nsFingerChannel.h"
#include "nsFingerHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsFingerHandler::nsFingerHandler() {
}

nsFingerHandler::~nsFingerHandler() {
}

NS_IMPL_ISUPPORTS2(nsFingerHandler,
                   nsIProtocolHandler,
                   nsIProxiedProtocolHandler)

NS_METHOD
nsFingerHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {

    nsFingerHandler* ph = new nsFingerHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->QueryInterface(aIID, aResult);
    NS_RELEASE(ph);
    return rv;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsFingerHandler::GetScheme(nsACString &result) {
    result = "finger";
    return NS_OK;
}

NS_IMETHODIMP
nsFingerHandler::GetDefaultPort(PRInt32 *result) {
    *result = FINGER_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerHandler::GetProtocolFlags(PRUint32 *result) {
    *result = URI_NORELATIVE | URI_NOAUTH | ALLOWS_PROXY;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerHandler::NewURI(const nsACString &aSpec,
                        const char *aCharset, // ignore charset info
                        nsIURI *aBaseURI,
                        nsIURI **result) {
    nsresult rv;

    nsIURI* url;
    rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);
    if (NS_FAILED(rv)) return rv;
    rv = url->SetSpec(aSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsFingerHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    return NewProxiedChannel(url, nsnull, result);
}

NS_IMETHODIMP
nsFingerHandler::NewProxiedChannel(nsIURI* url, nsIProxyInfo* proxyInfo,
                                   nsIChannel* *result)
{
    nsresult rv;
    
    nsFingerChannel* channel;
    rv = nsFingerChannel::Create(nsnull, NS_GET_IID(nsIChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(url, proxyInfo);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

NS_IMETHODIMP 
nsFingerHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    if (port == FINGER_PORT)
        *_retval = PR_TRUE;
    else
        *_retval = PR_FALSE;
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
