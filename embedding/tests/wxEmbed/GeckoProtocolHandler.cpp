/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "GeckoProtocolHandler.h"

#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIProgressEventSink.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIByteArrayInputStream.h"
#include "nsIStreamListener.h"
#include "nsIInputStreamPump.h"
#include "nsEmbedString.h"

// Everytime register handler is called, it picks the next available CID in the
// list.
// TODO - is there a cross-platform way to generate UUIDs and obviate this?
static const nsCID kProtocolCIDs[] = 
{
    { 0xfc8b2366, 0x0d07, 0x45ef, { 0x9f, 0xab, 0x22, 0x31, 0x9d, 0xbc, 0xfa, 0x77 } },
    { 0x6b5db250, 0xcf4b, 0x4ab1, { 0xb3, 0xaa, 0x1a, 0x9a, 0xd6, 0xdf, 0x7f, 0x95 } },
    { 0x677c6eaf, 0x3c3d, 0x4e0d, { 0xad, 0x30, 0x5a, 0xb8, 0x69, 0x1d, 0x1f, 0xfc } },
    { 0xbe383b01, 0x58d3, 0x4e65, { 0x9d, 0x50, 0x05, 0xb4, 0xc3, 0x92, 0x43, 0x2e } },
    { 0x81290231, 0xedf0, 0x4876, { 0x94, 0xa2, 0xdb, 0x96, 0xca, 0xa3, 0xc1, 0xfc } },
    { 0xf9c466b0, 0x0da8, 0x48a7, { 0xbb, 0xe4, 0x2f, 0x63, 0xb0, 0x71, 0x41, 0x6f } },
    { 0x9cbaef5e, 0xdf94, 0x4cb0, { 0xb4, 0xc3, 0x89, 0x66, 0x89, 0xd0, 0x2d, 0x56 } },
    { 0xce79440d, 0xdafc, 0x4908, { 0xb8, 0x94, 0xb2, 0x74, 0xa3, 0x51, 0x2f, 0x45 } }
};
static const int kProtocolCIDsSize = sizeof(kProtocolCIDs) / sizeof(kProtocolCIDs[0]);
static PRUint32 gUsedCIDs = 0;
struct GeckoChannelCallbacks
{
    nsCString mScheme;
    GeckoChannelCallback *mCallback;
    // SUCKS, component registry should properly copy this variable or take ownership of
    // it so it doesn't have to be made a static or global like this.
    // I also wonder if having component info memory dotted all over the place doesn't
    // impact component registry performance in some way.
    nsModuleComponentInfo mComponentInfo;
};
static GeckoChannelCallbacks gCallbacks[kProtocolCIDsSize];

class GeckoProtocolHandlerImpl :
    public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
};


class GeckoProtocolChannel :
    public nsIChannel,
    public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER

    GeckoProtocolChannel();
    nsresult Init(nsIURI *aURI);

protected:
    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsISupports>               mOwner;
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsISupports>               mListenerContext;
    nsCOMPtr<nsIInputStream>            mContentStream;
    nsCString                           mContentType;
    nsCString                           mContentCharset;
    PRUint32                            mLoadFlags;
    nsresult                            mStatus;
    PRUint32                            mContentLength;
    void                              * mData;
    nsCOMPtr<nsIInputStreamPump>        mPump;

    virtual ~GeckoProtocolChannel();
};

nsresult GeckoProtocolHandler::RegisterHandler(const char *aScheme, const char *aDescription, GeckoChannelCallback *aCallback)
{
    if (!aScheme || !aCallback)
    {
        return NS_ERROR_INVALID_ARG;
    }

    if (gUsedCIDs >= kProtocolCIDsSize)
    {
        // We've run out of CIDs. Perhaps this code should be generating them
        // on the fly somehow instead?
        return NS_ERROR_FAILURE;
    }
    for (PRUint32 i = 0; i < gUsedCIDs; i++)
    {
        if (gCallbacks[i].mScheme.EqualsIgnoreCase(aScheme))
            return NS_ERROR_FAILURE;
    }

    nsCAutoString contractID(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX);
    contractID.Append(aScheme);
    nsCID cid = kProtocolCIDs[gUsedCIDs];
    gCallbacks[gUsedCIDs].mScheme = aScheme;
    gCallbacks[gUsedCIDs].mCallback = aCallback;
    gUsedCIDs++;

    nsModuleComponentInfo &ci = gCallbacks[gUsedCIDs].mComponentInfo;
    memset(&ci, 0, sizeof(ci));
    ci.mDescription = strdup(aDescription);
    ci.mCID = cid;
    ci.mContractID = strdup(contractID.get());
    ci.mConstructor = GeckoProtocolHandlerImpl::Create;

    // Create a factory object which will create the protocol handler on demand
    nsCOMPtr<nsIGenericFactory> factory;
    NS_NewGenericFactory(getter_AddRefs(factory), &ci);
    nsComponentManager::RegisterFactory(
        cid, aDescription, contractID.get(), factory, PR_FALSE);

    return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


GeckoProtocolChannel::GeckoProtocolChannel() :
    mContentLength(0),
    mData(nsnull),
    mStatus(NS_OK),
    mLoadFlags(LOAD_NORMAL)
{
}

GeckoProtocolChannel::~GeckoProtocolChannel()
{
//    if (mData)
//        nsMemory::Free(mData);
}

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

NS_METHOD GeckoProtocolHandlerImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    GeckoProtocolHandlerImpl *impl = new GeckoProtocolHandlerImpl();
    if (!impl)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    *aResult = nsnull;
    nsresult rv = impl->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv))
    {
        impl->Release();
    }
    return rv;
}

NS_IMPL_ISUPPORTS1(GeckoProtocolHandlerImpl, nsIProtocolHandler)

/* readonly attribute ACString scheme; */
NS_IMETHODIMP GeckoProtocolHandlerImpl::GetScheme(nsACString & aScheme)
{
    // Since we have no clue what scheme we're an implementation of,
    // just return the first one that was registered.
    aScheme = gCallbacks[0].mScheme;
    return NS_OK;
}

/* readonly attribute long defaultPort; */
NS_IMETHODIMP GeckoProtocolHandlerImpl::GetDefaultPort(PRInt32 *aDefaultPort)
{
    *aDefaultPort = -1;
    return NS_OK;
}

/* readonly attribute unsigned long protocolFlags; */
NS_IMETHODIMP GeckoProtocolHandlerImpl::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
    *aProtocolFlags = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

/* nsIURI newURI (in AUTF8String aSpec, in string aOriginCharset, in nsIURI aBaseURI); */
NS_IMETHODIMP GeckoProtocolHandlerImpl::NewURI(const nsACString & aSpec, const char *aOriginCharset, nsIURI *aBaseURI, nsIURI **_retval)
{
    nsresult rv;
    nsIURI* url = nsnull;
    rv = nsComponentManager::CreateInstance(
        kSimpleURICID, nsnull, NS_GET_IID(nsIURI), (void**) &url);
    if (NS_FAILED(rv))
        return rv;
    rv = url->SetSpec(aSpec);
    if (NS_FAILED(rv))
    {
        NS_RELEASE(url);
        return rv;
    }
    *_retval = url;
    return rv;
}

/* nsIChannel newChannel (in nsIURI aURI); */
NS_IMETHODIMP GeckoProtocolHandlerImpl::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
    GeckoProtocolChannel *channel = new GeckoProtocolChannel;
    if (!channel)
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    channel->Init(aURI);
    channel->QueryInterface(NS_GET_IID(nsIChannel), (void **) _retval);
    return NS_OK;
}

/* boolean allowPort (in long port, in string scheme); */
NS_IMETHODIMP GeckoProtocolHandlerImpl::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS4(GeckoProtocolChannel, nsIRequest, nsIChannel, nsIRequestObserver, nsIStreamListener)

nsresult GeckoProtocolChannel::Init(nsIURI *aURI)
{
    mURI = aURI;
    return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIRequest methods


NS_IMETHODIMP
GeckoProtocolChannel::GetName(nsACString &result)
{
    return mURI->GetSpec(result);
}

NS_IMETHODIMP
GeckoProtocolChannel::IsPending(PRBool *result)
{
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");

    mStatus = status;
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
GeckoProtocolChannel::Suspend()
{
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
GeckoProtocolChannel::Resume()
{
    return NS_ERROR_UNEXPECTED;
}


////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
GeckoProtocolChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::Open(nsIInputStream **_retval)
{
    NS_NOTREACHED("GeckoProtocolChannel::Open");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
GeckoProtocolChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
    nsresult rv = NS_OK;

    nsCAutoString scheme;
    mURI->GetScheme(scheme);
    for (PRUint32 i = 0; i < gUsedCIDs; i++)
    {
        if (stricmp(scheme.get(), gCallbacks[i].mScheme.get()) == 0)
        {
            rv = gCallbacks[i].mCallback->GetData(
                mURI, NS_STATIC_CAST(nsIChannel *,this), mContentType, &mData, &mContentLength);
            if (NS_FAILED(rv)) return rv;
            
            nsCOMPtr<nsIByteArrayInputStream> stream;
            rv = NS_NewByteArrayInputStream(getter_AddRefs(stream), (char *) mData, mContentLength);
            if (NS_FAILED(rv)) return rv;
            mContentStream = do_QueryInterface(stream);

            mListenerContext = aContext;
            mListener = aListener;

            nsresult rv = NS_NewInputStreamPump(
                getter_AddRefs(mPump), mContentStream, -1, mContentLength, 0, 0, PR_TRUE);
            if (NS_FAILED(rv)) return rv;

            if (mLoadGroup)
            {
                mLoadGroup->AddRequest(this, nsnull);
            }

            rv = mPump->AsyncRead(this, nsnull);
            if (NS_FAILED(rv)) return rv;

            return rv;
        }
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetContentType(nsACString &aContentType)
{
    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetContentType(const nsACString &aContentType)
{
    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetContentCharset(const nsACString &aContentCharset)
{
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetContentLength(PRInt32 aContentLength)
{
    // silently ignore this...
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    mProgressSink = do_GetInterface(mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP 
GeckoProtocolChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods


NS_IMETHODIMP
GeckoProtocolChannel::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    return mListener->OnStartRequest(this, mListenerContext);
}

NS_IMETHODIMP
GeckoProtocolChannel::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    if (NS_SUCCEEDED(mStatus))
        mStatus = status;

    mListener->OnStopRequest(this, mListenerContext, mStatus);
    mListener = 0;
    mListenerContext = 0;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);

    mPump = 0;
    mContentStream = 0;
    return NS_OK;
}

NS_IMETHODIMP
GeckoProtocolChannel::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                                      nsIInputStream *stream,
                                      PRUint32 offset, PRUint32 count)
{
    nsresult rv;

    rv = mListener->OnDataAvailable(this, mListenerContext, stream, offset, count);

    if (mProgressSink && NS_SUCCEEDED(rv) && !(mLoadFlags & LOAD_BACKGROUND))
        mProgressSink->OnProgress(this, nsnull, offset + count, mContentLength);

    return rv; // let the pump cancel on failure
}
