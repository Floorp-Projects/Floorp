/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Brian Ryner.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com>
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsEventQueueUtils.h"
#include "nsNetUtil.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

#define BUFFER_SEG_SIZE (4*1024)
#define BUFFER_MAX_SIZE (64*1024)

// nsFingerChannel methods
nsFingerChannel::nsFingerChannel()
    : mLoadFlags(LOAD_NORMAL)
    , mStatus(NS_OK)
    , mPort(-1)
{
}

nsFingerChannel::~nsFingerChannel()
{
}

NS_IMPL_ISUPPORTS4(nsFingerChannel, 
                   nsIChannel, 
                   nsIRequest,
                   nsIStreamListener, 
                   nsIRequestObserver)

nsresult
nsFingerChannel::Init(nsIURI *uri, nsIProxyInfo *proxyInfo)
{
    nsresult rv;
    nsCAutoString autoBuffer;

    NS_ASSERTION(uri, "no uri");

    mURI = uri;
    mProxyInfo = proxyInfo;

//  For security reasons, we do not allow the user to specify a
//  non-default port for finger: URL's.

    mPort = FINGER_PORT;

    rv = mURI->GetPath(autoBuffer); // autoBuffer = user@host
    if (NS_FAILED(rv)) return rv;

    // Now parse out the user and host
    const char* buf = autoBuffer.get();
    const char* pos = strchr(buf, '@');

    // Catch the case of just the host being given
    if (!pos) {
        mUser.Truncate();
        mHost.Assign(buf);
    } else {
        mUser.Assign(buf, pos-buf);
        mHost.Assign(pos+1); // ignore '@'
    }

    if (mHost.IsEmpty())
        return NS_ERROR_MALFORMED_URI;

    mContentType.AssignLiteral(TEXT_HTML); // expected content-type
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFingerChannel::GetName(nsACString &result)
{
    return mURI->GetSpec(result);
}

NS_IMETHODIMP
nsFingerChannel::IsPending(PRBool *result)
{
    *result = (mPump != nsnull);
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::GetStatus(nsresult *status)
{
    if (NS_SUCCEEDED(mStatus) && mPump)
        mPump->GetStatus(status);
    else
        *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");

    mStatus = status;
    if (mPump)
        mPump->Cancel(status);

    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFingerChannel::Suspend()
{
    if (mPump)
        return mPump->Suspend();
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFingerChannel::Resume()
{
    if (mPump)
        return mPump->Resume();
    return NS_ERROR_UNEXPECTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsFingerChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mURI;
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
    *aURI = mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::Open(nsIInputStream **_retval)
{
    return NS_ImplementChannelOpen(this, _retval);
}

NS_IMETHODIMP
nsFingerChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
    nsresult rv = NS_OK;

    rv = NS_CheckPortSafety(mPort, "finger");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQ;
    rv = NS_GetCurrentEventQ(getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return rv;

    //
    // create transport
    //
    nsCOMPtr<nsISocketTransportService> sts = 
             do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = sts->CreateTransport(nsnull, 0, mHost, mPort, mProxyInfo,
                              getter_AddRefs(mTransport));
    if (NS_FAILED(rv)) return rv;

    // not fatal if these fail
    mTransport->SetSecurityCallbacks(mCallbacks);
    mTransport->SetEventSink(this, eventQ);

    rv = WriteRequest(mTransport);
    if (NS_FAILED(rv)) return rv;

    //
    // create TXT to HTML stream converter
    //
    nsCOMPtr<nsIStreamConverterService> scs = 
             do_GetService(kStreamConverterServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListener> convListener;
    rv = scs->AsyncConvertData("text/plain", "text/html", this, nsnull,
                               getter_AddRefs(convListener));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsITXTToHTMLConv> conv = do_QueryInterface(convListener);
    if (conv) {
        nsCAutoString userHost;
        rv = mURI->GetPath(userHost);

        nsAutoString title;
        title.AppendLiteral("Finger information for ");
        AppendUTF8toUTF16(userHost, title);

        conv->SetTitle(title.get());
        conv->PreFormatHTML(PR_TRUE);
    }

    //
    // open input stream, and create input stream pump...
    //
    nsCOMPtr<nsIInputStream> sockIn;
    rv = mTransport->OpenInputStream(0, 0, 0, getter_AddRefs(sockIn));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewInputStreamPump(getter_AddRefs(mPump), sockIn);
    if (NS_FAILED(rv)) return rv;

    rv = mPump->AsyncRead(convListener, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    mListener = aListener;
    mListenerContext = ctxt;
    return NS_OK;
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

NS_IMETHODIMP
nsFingerChannel::GetContentType(nsACString &aContentType)
{
    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetContentType(const nsACString &aContentType)
{
    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetContentCharset(const nsACString &aContentCharset)
{
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::SetContentLength(PRInt32 aContentLength)
{
    // silently ignore this...
    return NS_OK;
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
    mProgressSink = do_GetInterface(mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP 
nsFingerChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
    if (mTransport)
        return mTransport->GetSecurityInfo(aSecurityInfo);

    *aSecurityInfo = nsnull;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIRequestObserver methods
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFingerChannel::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    return mListener->OnStartRequest(this, mListenerContext);
}

NS_IMETHODIMP
nsFingerChannel::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    if (NS_SUCCEEDED(mStatus))
        mStatus = status;

    mListener->OnStopRequest(this, mListenerContext, mStatus);
    mListener = 0;
    mListenerContext = 0;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);

    mPump = 0;
    mTransport = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsFingerChannel::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                                 nsIInputStream *stream, PRUint32 offset,
                                 PRUint32 count)
{
    return mListener->OnDataAvailable(this, mListenerContext, stream, offset, count);
}

//-----------------------------------------------------------------------------
// nsITransportEventSink methods
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFingerChannel::OnTransportStatus(nsITransport *trans, nsresult status,
                                   PRUint32 progress, PRUint32 progressMax)
{
    // suppress status notification if channel is no longer pending!
    if (mProgressSink && NS_SUCCEEDED(mStatus) && mPump && !(mLoadFlags & LOAD_BACKGROUND)) {
        mProgressSink->OnStatus(this, nsnull, status,
                                NS_ConvertUTF8toUCS2(mHost).get());

        if (status == nsISocketTransport::STATUS_RECEIVING_FROM ||
            status == nsISocketTransport::STATUS_SENDING_TO) {
            mProgressSink->OnProgress(this, nsnull, progress, progressMax);
        }
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult
nsFingerChannel::WriteRequest(nsITransport *trans)
{
    // The text to send should already be in mUser
    nsresult rv;

    nsCAutoString requestBuf;
    requestBuf = mUser + NS_LITERAL_CSTRING("\r\n");

    nsCOMPtr<nsIOutputStream> stream;
    rv = trans->OpenOutputStream(0, requestBuf.Length(), 1, getter_AddRefs(stream));
    if (NS_FAILED(rv)) return rv;

    PRUint32 n;
    rv = stream->Write(requestBuf.get(), requestBuf.Length(), &n);
    if (NS_FAILED(rv)) return rv;

    NS_ENSURE_TRUE(n == requestBuf.Length(), NS_ERROR_UNEXPECTED);
    return NS_OK;
}
