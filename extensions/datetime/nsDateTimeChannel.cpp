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

// datetime implementation

#include "nsDateTimeChannel.h"
#include "nsIServiceManager.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsXPIDLString.h"
#include "nsISocketTransportService.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

// nsDateTimeChannel methods
nsDateTimeChannel::nsDateTimeChannel() {
    NS_INIT_REFCNT();
    mContentLength = -1;
    mPort = -1;
}

nsDateTimeChannel::~nsDateTimeChannel() {
}

NS_IMPL_ISUPPORTS4(nsDateTimeChannel, nsIChannel, nsIRequest, nsIStreamListener, nsIStreamObserver)

nsresult
nsDateTimeChannel::Init(nsIURI* uri)
{
    nsresult rv;

    NS_ASSERTION(uri, "no uri");

    mUrl = uri;

    rv = mUrl->GetPort(&mPort);
    if (NS_FAILED(rv) || mPort < 1)
        mPort = DATETIME_PORT;

    rv = mUrl->GetPath(getter_Copies(mHost));
    if (NS_FAILED(rv)) return rv;

    if (!*(const char *)mHost) return NS_ERROR_NOT_INITIALIZED;

    return NS_OK;
}

NS_METHOD
nsDateTimeChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsDateTimeChannel* dc = new nsDateTimeChannel();
    if (dc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dc);
    nsresult rv = dc->QueryInterface(aIID, aResult);
    NS_RELEASE(dc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsDateTimeChannel::GetName(PRUnichar* *result)
{
    NS_NOTREACHED("nsDateTimeChannel::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::IsPending(PRBool *result)
{
    NS_NOTREACHED("nsDateTimeChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetStatus(nsresult *status)
{
    *status = NS_OK;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    NS_NOTREACHED("nsDateTimeChannel::Cancel");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::Suspend(void)
{
    NS_NOTREACHED("nsDateTimeChannel::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::Resume(void)
{
    NS_NOTREACHED("nsDateTimeChannel::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsDateTimeChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mUrl;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mUrl;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetURI(nsIURI* aURI)
{
    mUrl = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::OpenInputStream(nsIInputStream **_retval)
{
    nsresult rv = NS_OK;

    NS_WITH_SERVICE(nsISocketTransportService, socketService, kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = socketService->CreateTransport(mHost, mPort, nsnull, -1, 32, 32, getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    rv = channel->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) return rv;

    return channel->OpenInputStream(_retval);
}

NS_IMETHODIMP
nsDateTimeChannel::OpenOutputStream(nsIOutputStream **_retval)
{
    NS_NOTREACHED("nsDateTimeChannel::OpenOutputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::AsyncRead(nsIStreamListener *aListener,
                             nsISupports *ctxt)
{
    nsresult rv = NS_OK;

    NS_WITH_SERVICE(nsISocketTransportService, socketService, kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> channel;
    rv = socketService->CreateTransport(mHost, mPort, nsnull, 0, 32, 32, getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;

    rv = channel->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) return rv;

    mListener = aListener;

    return channel->AsyncRead(this, ctxt);
}

NS_IMETHODIMP
nsDateTimeChannel::AsyncWrite(nsIInputStream *fromStream,
                              nsIStreamObserver *observer,
                              nsISupports *ctxt)
{
    NS_NOTREACHED("nsDateTimeChannel::AsyncWrite");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

#define DATETIME_TYPE "text/plain"

NS_IMETHODIMP
nsDateTimeChannel::GetContentType(char* *aContentType) {
    if (!aContentType) return NS_ERROR_NULL_POINTER;

    *aContentType = nsCRT::strdup(DATETIME_TYPE);
    if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetContentType(const char *aContentType)
{
    //It doesn't make sense to set the content-type on this type
    // of channel...
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDateTimeChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsDateTimeChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    NS_NOTREACHED("nsDateTimeChannel::GetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    NS_NOTREACHED("nsDateTimeChannel::SetTransferOffset");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    NS_NOTREACHED("nsDateTimeChannel::GetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::SetTransferCount(PRInt32 aTransferCount)
{
    NS_NOTREACHED("nsDateTimeChannel::SetTransferCount");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    NS_NOTREACHED("nsDateTimeChannel::GetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    NS_NOTREACHED("nsDateTimeChannel::SetBufferSegmentSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    NS_NOTREACHED("nsDateTimeChannel::GetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    NS_NOTREACHED("nsDateTimeChannel::SetBufferMaxSize");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetLocalFile(nsIFile* *file)
{
    *file = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsDateTimeChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDateTimeChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    if (mLoadGroup) // if we already had a load group remove ourselves...
      (void)mLoadGroup->RemoveChannel(this, nsnull, NS_OK, nsnull);

    mLoadGroup = aLoadGroup;
    if (mLoadGroup) {
        return mLoadGroup->AddChannel(this, nsnull);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsDateTimeChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    return NS_OK;
}

NS_IMETHODIMP 
nsDateTimeChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

// nsIStreamObserver methods
NS_IMETHODIMP
nsDateTimeChannel::OnStartRequest(nsIChannel *aChannel, nsISupports *aContext) {
    return mListener->OnStartRequest(this, aContext);
}


NS_IMETHODIMP
nsDateTimeChannel::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext,
                                 nsresult aStatus, const PRUnichar* aStatusArg) {
    if (mLoadGroup) {
        nsresult rv = mLoadGroup->RemoveChannel(this, nsnull, aStatus, aStatusArg);
        if (NS_FAILED(rv)) return rv;
    }
    return mListener->OnStopRequest(this, aContext, aStatus, aStatusArg);
}


// nsIStreamListener method
NS_IMETHODIMP
nsDateTimeChannel::OnDataAvailable(nsIChannel* aChannel, nsISupports* aContext,
                               nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                               PRUint32 aLength) {
    mContentLength = aLength;
    return mListener->OnDataAvailable(this, aContext, aInputStream, aSourceOffset, aLength);
}

