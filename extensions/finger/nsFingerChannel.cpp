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

// finger implementation

#include "nsFingerChannel.h"
#include "nsIServiceManager.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsXPIDLString.h"
#include "nsISocketTransportService.h"
#include "nsIStringStream.h"
#include "nsMimeTypes.h"
#include "nsIStreamConverterService.h"
#include "nsITXTToHTMLConv.h"
#include "nsIProgressEventSink.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

#define BUFFER_SEG_SIZE (4*1024)
#define BUFFER_MAX_SIZE (64*1024)

// nsFingerChannel methods
nsFingerChannel::nsFingerChannel()
    : mContentLength(-1),
      mActAsObserver(PR_TRUE),
      mPort(-1),
      mStatus(NS_OK)
{
    NS_INIT_REFCNT();
}

nsFingerChannel::~nsFingerChannel() {
}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsFingerChannel, 
                              nsIChannel, 
                              nsIRequest,
                              nsIStreamListener, 
                              nsIRequestObserver)

nsresult
nsFingerChannel::Init(nsIURI* uri, nsIProxyInfo* proxyInfo)
{
    nsresult rv;
    nsXPIDLCString autoBuffer;

    NS_ASSERTION(uri, "no uri");

    mUrl = uri;
    mProxyInfo = proxyInfo;

//  For security reasons, we do not allow the user to specify a
//  non-default port for finger: URL's.

    mPort = FINGER_PORT;

    rv = mUrl->GetPath(getter_Copies(autoBuffer)); // autoBuffer = user@host
    if (NS_FAILED(rv)) return rv;

    // Now parse out the user and host
    const char* buf = autoBuffer.get();
    const char* pos = PL_strchr(buf, '@');

    // Catch the case of just the host being given
    if (!pos) {
        mHost.Assign(buf);
    } else {
        mUser.Assign(buf,pos-buf);
        mHost.Assign(pos+1); // ignore '@'
    }

    if (mHost.IsEmpty()) return NS_ERROR_NOT_INITIALIZED;

    return NS_OK;
}

NS_METHOD
nsFingerChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsFingerChannel* fc = new nsFingerChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFingerChannel::GetName(PRUnichar* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFingerChannel::IsPending(PRBool *result)
{
    NS_NOTREACHED("nsFingerChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFingerChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsresult rv = NS_ERROR_FAILURE;

    mStatus = status;
    if (mTransportRequest) {
      rv = mTransportRequest->Cancel(status);
    }
    return rv;
}

NS_IMETHODIMP
nsFingerChannel::Suspend(void)
{
    NS_NOTREACHED("nsFingerChannel::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFingerChannel::Resume(void)
{
    NS_NOTREACHED("nsFingerChannel::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsFingerChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mUrl;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mUrl;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::Open(nsIInputStream **_retval)
{
    nsresult rv = NS_OK;

    rv = NS_CheckPortSafety(mPort, "finger");
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsISocketTransportService> socketService = 
             do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = socketService->CreateTransport(mHost.get(), mPort, mProxyInfo, BUFFER_SEG_SIZE,
            BUFFER_MAX_SIZE, getter_AddRefs(mTransport));
    if (NS_FAILED(rv)) return rv;

    mTransport->SetNotificationCallbacks(mCallbacks,
                                         (mLoadFlags & LOAD_BACKGROUND));

    return mTransport->OpenInputStream(0, PRUint32(-1), 0, _retval);
}

NS_IMETHODIMP
nsFingerChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
    nsresult rv = NS_OK;

    rv = NS_CheckPortSafety(mPort, "finger");
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsISocketTransportService> socketService = 
             do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = socketService->CreateTransport(mHost.get(), mPort, mProxyInfo, BUFFER_SEG_SIZE,
      BUFFER_MAX_SIZE, getter_AddRefs(mTransport));
    if (NS_FAILED(rv)) return rv;

    mTransport->SetNotificationCallbacks(mCallbacks,
                                         (mLoadFlags & LOAD_BACKGROUND));

    mListener = aListener;
    mResponseContext = ctxt;

    return SendRequest(mTransport);
}

NS_IMETHODIMP
nsFingerChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

#define FINGER_TYPE TEXT_HTML

NS_IMETHODIMP
nsFingerChannel::GetContentType(char* *aContentType) {
    if (!aContentType) return NS_ERROR_NULL_POINTER;

    *aContentType = nsCRT::strdup(FINGER_TYPE);
    if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetContentType(const char *aContentType)
{
    //It doesn't make sense to set the content-type on this type
    // of channel...
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFingerChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsFingerChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFingerChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    return NS_OK;
}

NS_IMETHODIMP 
nsFingerChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

// nsIRequestObserver methods
NS_IMETHODIMP
nsFingerChannel::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext) {
    if (!mActAsObserver) {
      // acting as a listener
      return mListener->OnStartRequest(this, mResponseContext);
    } else {
      // we don't want to pass our AsyncWrite's OnStart through
      // we just ignore this
      return NS_OK;
    }
}


NS_IMETHODIMP
nsFingerChannel::OnStopRequest(nsIRequest *aRequest, nsISupports* aContext,
                               nsresult aStatus)
{
    nsresult rv = NS_OK;

    if (NS_FAILED(aStatus) || !mActAsObserver) {
        if (mLoadGroup) {
          rv = mLoadGroup->RemoveRequest(this, nsnull, aStatus);
          if (NS_FAILED(rv)) return rv;
        }
        rv = mListener->OnStopRequest(this, mResponseContext, aStatus);
        mTransport = 0;
        return rv;
    } else {
        // at this point we know the request has been sent.
        // we're no longer acting as an observer.
 
        mActAsObserver = PR_FALSE;
        nsCOMPtr<nsIStreamListener> converterListener;

        nsCOMPtr<nsIStreamConverterService> StreamConvService = 
                 do_GetService(kStreamConverterServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsAutoString fromStr; fromStr.AssignWithConversion("text/plain");
        nsAutoString toStr; toStr.AssignWithConversion("text/html");

        rv = StreamConvService->AsyncConvertData(fromStr.get(),
              toStr.get(), this, mResponseContext,
              getter_AddRefs(converterListener));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsITXTToHTMLConv> converter(do_QueryInterface(converterListener));
        if (converter) {
          nsAutoString title; title.AssignWithConversion("Finger information for ");
          nsXPIDLCString userHost;
          rv = mUrl->GetPath(getter_Copies(userHost));
          title.AppendWithConversion(userHost);
          converter->SetTitle(title.get());
          converter->PreFormatHTML(PR_TRUE);
        }

        return mTransport->AsyncRead(converterListener, mResponseContext, 0, PRUint32(-1), 0,
                                     getter_AddRefs(mTransportRequest));
    }

}


// nsIStreamListener method
NS_IMETHODIMP
nsFingerChannel::OnDataAvailable(nsIRequest *aRequest, nsISupports* aContext,
                               nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                               PRUint32 aLength) {
    mContentLength = aLength;
    return mListener->OnDataAvailable(this, mResponseContext, aInputStream, aSourceOffset, aLength);
}

nsresult
nsFingerChannel::SendRequest(nsITransport* aTransport) {
  // The text to send should already be in mUser

  nsresult rv = NS_OK;
  nsCOMPtr<nsISupports> result;
  nsCOMPtr<nsIInputStream> charstream;
  nsCString requestBuffer(mUser);

  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nsnull);
  }

  requestBuffer.Append(CRLF);

  mRequest.Adopt(ToNewCString(requestBuffer));

  rv = NS_NewCharInputStream(getter_AddRefs(result), mRequest);
  if (NS_FAILED(rv)) return rv;

  charstream = do_QueryInterface(result, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = NS_AsyncWriteFromStream(getter_AddRefs(mTransportRequest),
                               aTransport, charstream,
                               0, requestBuffer.Length(), 0,
                               this, nsnull);
  return rv;
}


