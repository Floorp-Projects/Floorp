/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FetchDriver.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIHttpChannel.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIScriptSecurityManager.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIUploadChannel2.h"

#include "nsContentPolicyUtils.h"
#include "nsDataHandler.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/workers/Workers.h"

#include "Fetch.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(FetchDriver, nsIStreamListener)

FetchDriver::FetchDriver(InternalRequest* aRequest, nsIPrincipal* aPrincipal)
  : mPrincipal(aPrincipal)
  , mRequest(aRequest)
  , mFetchRecursionCount(0)
  , mResponseAvailableCalled(false)
{
}

FetchDriver::~FetchDriver()
{
  // We assert this since even on failures, we should call
  // FailWithNetworkError().
  MOZ_ASSERT(mResponseAvailableCalled);
}

nsresult
FetchDriver::Fetch(FetchDriverObserver* aObserver)
{
  workers::AssertIsOnMainThread();
  mObserver = aObserver;

  return Fetch(false /* CORS flag */);
}

nsresult
FetchDriver::Fetch(bool aCORSFlag)
{
  // We do not currently implement parts of the spec that lead to recursion.
  MOZ_ASSERT(mFetchRecursionCount == 0);
  mFetchRecursionCount++;

  // FIXME(nsm): Deal with HSTS.

  if (!mRequest->IsSynchronous() && mFetchRecursionCount <= 1) {
    nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableMethodWithArg<bool>(this, &FetchDriver::ContinueFetch, aCORSFlag);
    nsresult rv = NS_DispatchToCurrentThread(r);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailWithNetworkError();
    }
    return rv;
  }

  MOZ_CRASH("Synchronous fetch not supported");
}

nsresult
FetchDriver::ContinueFetch(bool aCORSFlag)
{
  workers::AssertIsOnMainThread();

  nsAutoCString url;
  mRequest->GetURL(url);
  nsCOMPtr<nsIURI> requestURI;
  // FIXME(nsm): Deal with relative URLs.
  nsresult rv = NS_NewURI(getter_AddRefs(requestURI), url,
                          nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FailWithNetworkError();
  }

  // FIXME(nsm): Bug 1039846: Add CSP checks

  nsAutoCString scheme;
  rv = requestURI->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FailWithNetworkError();
  }

  rv = mPrincipal->CheckMayLoad(requestURI, false /* report */, false /* allowIfInheritsPrincipal */);
  if ((!aCORSFlag && NS_SUCCEEDED(rv)) ||
      (scheme.EqualsLiteral("data") && mRequest->SameOriginDataURL()) ||
      scheme.EqualsLiteral("about")) {
    return BasicFetch();
  }

  if (mRequest->Mode() == RequestMode::Same_origin) {
    return FailWithNetworkError();
  }

  if (mRequest->Mode() == RequestMode::No_cors) {
    mRequest->SetResponseTainting(InternalRequest::RESPONSETAINT_OPAQUE);
    return BasicFetch();
  }

  if (!scheme.EqualsLiteral("http") && !scheme.EqualsLiteral("https")) {
    return FailWithNetworkError();
  }

  bool corsPreflight = false;
  if (mRequest->Mode() == RequestMode::Cors_with_forced_preflight ||
      (mRequest->UnsafeRequest() && (mRequest->HasSimpleMethod() || !mRequest->Headers()->HasOnlySimpleHeaders()))) {
    corsPreflight = true;
  }

  mRequest->SetResponseTainting(InternalRequest::RESPONSETAINT_CORS);
  return HttpFetch(true /* aCORSFlag */, corsPreflight);
}

nsresult
FetchDriver::BasicFetch()
{
  nsAutoCString url;
  mRequest->GetURL(url);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                 url,
                 nullptr,
                 nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  if (scheme.LowerCaseEqualsLiteral("about")) {
    if (url.EqualsLiteral("about:blank")) {
      nsRefPtr<InternalResponse> response =
        new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
      ErrorResult result;
      response->Headers()->Append(NS_LITERAL_CSTRING("content-type"),
                                  NS_LITERAL_CSTRING("text/html;charset=utf-8"),
                                  result);
      MOZ_ASSERT(!result.Failed());
      nsCOMPtr<nsIInputStream> body;
      rv = NS_NewCStringInputStream(getter_AddRefs(body), EmptyCString());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        FailWithNetworkError();
        return rv;
      }

      response->SetBody(body);
      BeginResponse(response);
      return SucceedWithResponse();
    }
    return FailWithNetworkError();
  }

  if (scheme.LowerCaseEqualsLiteral("blob")) {
    nsRefPtr<FileImpl> blobImpl;
    rv = NS_GetBlobForBlobURI(uri, getter_AddRefs(blobImpl));
    FileImpl* blob = static_cast<FileImpl*>(blobImpl.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailWithNetworkError();
      return rv;
    }

    nsRefPtr<InternalResponse> response = new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
    {
      ErrorResult result;
      uint64_t size = blob->GetSize(result);
      if (NS_WARN_IF(result.Failed())) {
        FailWithNetworkError();
        return result.ErrorCode();
      }

      nsAutoString sizeStr;
      sizeStr.AppendInt(size);
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Length"), NS_ConvertUTF16toUTF8(sizeStr), result);
      if (NS_WARN_IF(result.Failed())) {
        FailWithNetworkError();
        return result.ErrorCode();
      }

      nsAutoString type;
      blob->GetType(type);
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"), NS_ConvertUTF16toUTF8(type), result);
      if (NS_WARN_IF(result.Failed())) {
        FailWithNetworkError();
        return result.ErrorCode();
      }
    }

    nsCOMPtr<nsIInputStream> stream;
    rv = blob->GetInternalStream(getter_AddRefs(stream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailWithNetworkError();
      return rv;
    }

    response->SetBody(stream);
    BeginResponse(response);
    return SucceedWithResponse();
  }

  if (scheme.LowerCaseEqualsLiteral("data")) {
    nsAutoCString method;
    mRequest->GetMethod(method);
    if (method.LowerCaseEqualsASCII("get")) {
      // Use nsDataHandler directly so that we can extract the content type.
      // XXX(nsm): Is there a way to acquire the charset without such tight
      // coupling with the DataHandler? nsIProtocolHandler does not provide
      // anything similar.
      nsAutoCString contentType, contentCharset, dataBuffer, hashRef;
      bool isBase64;
      rv = nsDataHandler::ParseURI(url,
                                   contentType,
                                   contentCharset,
                                   isBase64,
                                   dataBuffer,
                                   hashRef);
      if (NS_SUCCEEDED(rv)) {
        ErrorResult result;
        nsRefPtr<InternalResponse> response = new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
        if (!contentCharset.IsEmpty()) {
          contentType.Append(";charset=");
          contentType.Append(contentCharset);
        }

        response->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"), contentType, result);
        if (!result.Failed()) {
          nsCOMPtr<nsIInputStream> stream;
          rv = NS_NewCStringInputStream(getter_AddRefs(stream), dataBuffer);
          if (NS_SUCCEEDED(rv)) {
            response->SetBody(stream);
            BeginResponse(response);
            return SucceedWithResponse();
          }
        }
      }
    }

    return FailWithNetworkError();
  }

  if (scheme.LowerCaseEqualsLiteral("file")) {
  } else if (scheme.LowerCaseEqualsLiteral("http") ||
             scheme.LowerCaseEqualsLiteral("https")) {
    return HttpFetch();
  }

  return FailWithNetworkError();
}

nsresult
FetchDriver::HttpFetch(bool aCORSFlag, bool aPreflightCORSFlag, bool aAuthenticationFlag)
{
  mResponse = nullptr;

  // XXXnsm: The ServiceWorker interception should happen automatically.
  return ContinueHttpFetchAfterServiceWorker();
}

nsresult
FetchDriver::ContinueHttpFetchAfterServiceWorker()
{
  if (!mResponse) {
    // FIXME(nsm): Set skip SW flag.
    // FIXME(nsm): Deal with CORS flags cases which will also call
    // ContinueHttpFetchAfterCORSPreflight().
    return ContinueHttpFetchAfterCORSPreflight();
  }

  // Otherwise ServiceWorker replied with a response.
  return ContinueHttpFetchAfterNetworkFetch();
}

nsresult
FetchDriver::ContinueHttpFetchAfterCORSPreflight()
{
  // mResponse is currently the CORS response.
  // We may have to pass it via argument.
  if (mResponse && mResponse->IsError()) {
    return FailWithNetworkError();
  }

  return HttpNetworkFetch();
}

nsresult
FetchDriver::HttpNetworkFetch()
{
  nsRefPtr<InternalRequest> httpRequest = new InternalRequest(*mRequest);

  nsresult rv;

  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  nsAutoCString url;
  httpRequest->GetURL(url);
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri),
                          url,
                          nullptr,
                          nullptr,
                          ios);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }
  nsCOMPtr<nsIChannel> chan;
  rv = NS_NewChannel(getter_AddRefs(chan),
                     uri,
                     mPrincipal,
                     nsILoadInfo::SEC_NORMAL,
                     mRequest->GetContext(),
                     nullptr, /* FIXME(nsm): loadgroup */
                     nullptr, /* aCallbacks */
                     nsIRequest::LOAD_NORMAL,
                     ios);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> httpChan = do_QueryInterface(chan);
  if (httpChan) {
    nsAutoCString method;
    mRequest->GetMethod(method);
    rv = httpChan->SetRequestMethod(method);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailWithNetworkError();
      return rv;
    }
  }

  return chan->AsyncOpen(this, nullptr);
}

nsresult
FetchDriver::ContinueHttpFetchAfterNetworkFetch()
{
  workers::AssertIsOnMainThread();
  MOZ_ASSERT(mResponse);
  MOZ_ASSERT(!mResponse->IsError());

  return SucceedWithResponse();
}

already_AddRefed<InternalResponse>
FetchDriver::BeginAndGetFilteredResponse(InternalResponse* aResponse)
{
  MOZ_ASSERT(aResponse);
  nsAutoCString reqURL;
  mRequest->GetURL(reqURL);
  aResponse->SetUrl(reqURL);

  // FIXME(nsm): Handle mixed content check, step 7 of fetch.

  nsRefPtr<InternalResponse> filteredResponse;
  switch (mRequest->GetResponseTainting()) {
    case InternalRequest::RESPONSETAINT_BASIC:
      filteredResponse = InternalResponse::BasicResponse(aResponse);
      break;
    case InternalRequest::RESPONSETAINT_CORS:
      filteredResponse = InternalResponse::CORSResponse(aResponse);
      break;
    case InternalRequest::RESPONSETAINT_OPAQUE:
      filteredResponse = InternalResponse::OpaqueResponse();
      break;
    default:
      MOZ_CRASH("Unexpected case");
  }

  MOZ_ASSERT(filteredResponse);
  mObserver->OnResponseAvailable(filteredResponse);
  mResponseAvailableCalled = true;
  return filteredResponse.forget();
}

void
FetchDriver::BeginResponse(InternalResponse* aResponse)
{
  nsRefPtr<InternalResponse> r = BeginAndGetFilteredResponse(aResponse);
  // Release the ref.
}

nsresult
FetchDriver::SucceedWithResponse()
{
  mObserver->OnResponseEnd();
  return NS_OK;
}

nsresult
FetchDriver::FailWithNetworkError()
{
  MOZ_ASSERT(mObserver);
  nsRefPtr<InternalResponse> error = InternalResponse::NetworkError();
  mObserver->OnResponseAvailable(error);
  mResponseAvailableCalled = true;
  mObserver->OnResponseEnd();
  return NS_OK;
}

namespace {
class FillResponseHeaders MOZ_FINAL : public nsIHttpHeaderVisitor {
  InternalResponse* mResponse;

  ~FillResponseHeaders()
  { }
public:
  NS_DECL_ISUPPORTS

  explicit FillResponseHeaders(InternalResponse* aResponse)
    : mResponse(aResponse)
  {
  }

  NS_IMETHOD
  VisitHeader(const nsACString & aHeader, const nsACString & aValue) MOZ_OVERRIDE
  {
    ErrorResult result;
    mResponse->Headers()->Append(aHeader, aValue, result);
    return result.ErrorCode();
  }
};

NS_IMPL_ISUPPORTS(FillResponseHeaders, nsIHttpHeaderVisitor)
} // anonymous namespace

NS_IMETHODIMP
FetchDriver::OnStartRequest(nsIRequest* aRequest,
                            nsISupports* aContext)
{
  MOZ_ASSERT(!mPipeOutputStream);
  nsresult rv;
  aRequest->GetStatus(&rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    return rv;
  }

  nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest);
  // For now we only support HTTP.
  MOZ_ASSERT(channel);

  uint32_t status;
  channel->GetResponseStatus(&status);

  nsAutoCString statusText;
  channel->GetResponseStatusText(statusText);

  nsRefPtr<InternalResponse> response = new InternalResponse(status, statusText);

  nsRefPtr<FillResponseHeaders> visitor = new FillResponseHeaders(response);
  rv = channel->VisitResponseHeaders(visitor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    NS_WARNING("Failed to visit all headers.");
  }

  mResponse = BeginAndGetFilteredResponse(response);

  // We open a pipe so that we can immediately set the pipe's read end as the
  // response's body. Setting the segment size to UINT32_MAX means that the
  // pipe has infinite space. The nsIChannel will continue to buffer data in
  // xpcom events even if we block on a fixed size pipe.  It might be possible
  // to suspend the channel and then resume when there is space available, but
  // for now use an infinite pipe to avoid blocking.
  nsCOMPtr<nsIInputStream> pipeInputStream;
  rv = NS_NewPipe(getter_AddRefs(pipeInputStream),
                  getter_AddRefs(mPipeOutputStream),
                  0, /* default segment size */
                  UINT32_MAX /* infinite pipe */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    // Cancel request.
    return rv;
  }

  mResponse->SetBody(pipeInputStream);

  nsCOMPtr<nsIEventTarget> sts = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailWithNetworkError();
    // Cancel request.
    return rv;
  }

  // Try to retarget off main thread.
  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(aRequest);
  if (rr) {
    rr->RetargetDeliveryTo(sts);
  }
  return NS_OK;
}

NS_IMETHODIMP
FetchDriver::OnDataAvailable(nsIRequest* aRequest,
                             nsISupports* aContext,
                             nsIInputStream* aInputStream,
                             uint64_t aOffset,
                             uint32_t aCount)
{
  uint32_t aRead;
  MOZ_ASSERT(mResponse);
  MOZ_ASSERT(mPipeOutputStream);

  nsresult rv = aInputStream->ReadSegments(NS_CopySegmentToStream,
                                           mPipeOutputStream,
                                           aCount, &aRead);
  return rv;
}

NS_IMETHODIMP
FetchDriver::OnStopRequest(nsIRequest* aRequest,
                           nsISupports* aContext,
                           nsresult aStatusCode)
{
  MOZ_ASSERT(mPipeOutputStream);
  mPipeOutputStream->Close();

  if (NS_FAILED(aStatusCode)) {
    FailWithNetworkError();
    return aStatusCode;
  }

  ContinueHttpFetchAfterNetworkFetch();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
