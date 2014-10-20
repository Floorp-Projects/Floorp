/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FetchDriver.h"

#include "nsIScriptSecurityManager.h"

#include "nsContentPolicyUtils.h"
#include "nsDataHandler.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/workers/Workers.h"

#include "Fetch.h"
#include "InternalRequest.h"
#include "InternalResponse.h"

namespace mozilla {
namespace dom {

FetchDriver::FetchDriver(InternalRequest* aRequest)
  : mRequest(aRequest)
  , mFetchRecursionCount(0)
{
}

FetchDriver::~FetchDriver()
{
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
    return NS_DispatchToCurrentThread(r);
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

  nsAutoCString originURL;
  mRequest->GetOrigin(originURL);
  nsCOMPtr<nsIURI> originURI;
  rv = NS_NewURI(getter_AddRefs(originURI), originURL, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return FailWithNetworkError();
  }

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  rv = ssm->CheckSameOriginURI(requestURI, originURI, false);
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

  if (mRequest->Mode() == RequestMode::Cors_with_forced_preflight ||
      (mRequest->UnsafeRequest() && (mRequest->HasSimpleMethod() || !mRequest->Headers()->HasOnlySimpleHeaders()))) {
    // FIXME(nsm): Set corsPreflight;
  }

  mRequest->SetResponseTainting(InternalRequest::RESPONSETAINT_CORS);
  // FIXME(nsm): HttpFetch.
  return FailWithNetworkError();
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
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString scheme;
  rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

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
        return FailWithNetworkError();
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
      return FailWithNetworkError();
    }

    nsRefPtr<InternalResponse> response = new InternalResponse(200, NS_LITERAL_CSTRING("OK"));
    {
      ErrorResult result;
      uint64_t size = blob->GetSize(result);
      if (NS_WARN_IF(result.Failed())) {
        return FailWithNetworkError();
      }

      nsAutoString sizeStr;
      sizeStr.AppendInt(size);
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Length"), NS_ConvertUTF16toUTF8(sizeStr), result);
      if (NS_WARN_IF(result.Failed())) {
        return FailWithNetworkError();
      }

      nsAutoString type;
      blob->GetType(type);
      response->Headers()->Append(NS_LITERAL_CSTRING("Content-Type"), NS_ConvertUTF16toUTF8(type), result);
      if (NS_WARN_IF(result.Failed())) {
        return FailWithNetworkError();
      }
    }

    nsCOMPtr<nsIInputStream> stream;
    rv = blob->GetInternalStream(getter_AddRefs(stream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return FailWithNetworkError();
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

    NS_WARN_IF(NS_FAILED(rv));
    return FailWithNetworkError();
  }

  if (scheme.LowerCaseEqualsLiteral("file")) {
  } else if (scheme.LowerCaseEqualsLiteral("http") ||
             scheme.LowerCaseEqualsLiteral("https")) {
    // FIXME(nsm): HttpFetch.
    return FailWithNetworkError();
  }

  return FailWithNetworkError();
}

nsresult
FetchDriver::BeginResponse(InternalResponse* aResponse)
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
  return NS_OK;
}

nsresult
FetchDriver::SucceedWithResponse()
{
  return NS_OK;
}

nsresult
FetchDriver::FailWithNetworkError()
{
  nsRefPtr<InternalResponse> error = InternalResponse::NetworkError();
  mObserver->OnResponseAvailable(error);
  // FIXME(nsm): Some sort of shutdown?
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
