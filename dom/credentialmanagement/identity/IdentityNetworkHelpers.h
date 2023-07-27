/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IdentityNetworkHelpers_h
#define mozilla_dom_IdentityNetworkHelpers_h

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/Response.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/MozPromise.h"

namespace mozilla::dom {

// Helper to get a JSON structure via a Fetch.
// The Request must already be built and T should be a webidl type with
// annotation GenerateConversionToJS so it has an Init method.
template <typename T, typename TPromise = MozPromise<T, nsresult, true>>
RefPtr<TPromise> FetchJSONStructure(Request* aRequest) {
  MOZ_ASSERT(XRE_IsParentProcess());

  // Create the returned Promise
  RefPtr<typename TPromise::Private> resultPromise =
      new typename TPromise::Private(__func__);

  // Fetch the provided request
  RequestOrUSVString fetchInput;
  fetchInput.SetAsRequest() = aRequest;
  RootedDictionary<RequestInit> requestInit(RootingCx());
  IgnoredErrorResult error;
  RefPtr<Promise> fetchPromise =
      FetchRequest(aRequest->GetParentObject(), fetchInput, requestInit,
                   CallerType::System, error);
  if (NS_WARN_IF(error.Failed())) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  // Working around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85883
  RefPtr<PromiseNativeHandler> reject =
      new MozPromiseRejectOnDestruction{resultPromise, __func__};

  // Handle the response
  fetchPromise->AddCallbacksWithCycleCollectedArgs(
      [resultPromise, reject](JSContext* aCx, JS::Handle<JS::Value> aValue,
                              ErrorResult&) {
        // Get the Response object from the argument to the callback
        if (NS_WARN_IF(!aValue.isObject())) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
        MOZ_ASSERT(obj);
        Response* response = nullptr;
        if (NS_WARN_IF(NS_FAILED(UNWRAP_OBJECT(Response, &obj, response)))) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }

        // Make sure the request was a success
        if (!response->Ok()) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }

        // Parse the body into JSON, which must be done async
        IgnoredErrorResult error;
        RefPtr<Promise> jsonPromise = response->ConsumeBody(
            aCx, BodyConsumer::ConsumeType::CONSUME_JSON, error);
        if (NS_WARN_IF(error.Failed())) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }

        // Handle the parsed JSON from the Response body
        jsonPromise->AddCallbacksWithCycleCollectedArgs(
            [resultPromise](JSContext* aCx, JS::Handle<JS::Value> aValue,
                            ErrorResult&) {
              // Parse the JSON into the correct type, validating fields and
              // types
              T result;
              bool success = result.Init(aCx, aValue);
              if (!success) {
                resultPromise->Reject(NS_ERROR_FAILURE, __func__);
                return;
              }

              resultPromise->Resolve(result, __func__);
            },
            [resultPromise](JSContext*, JS::Handle<JS::Value> aValue,
                            ErrorResult&) {
              resultPromise->Reject(
                  Promise::TryExtractNSResultFromRejectionValue(aValue),
                  __func__);
            });
        jsonPromise->AppendNativeHandler(reject);
      },
      [resultPromise](JSContext*, JS::Handle<JS::Value> aValue, ErrorResult&) {
        resultPromise->Reject(
            Promise::TryExtractNSResultFromRejectionValue(aValue), __func__);
      });
  fetchPromise->AppendNativeHandler(reject);

  return resultPromise;
}

}  // namespace mozilla::dom

#endif  // mozilla_dom_IdentityNetworkHelpers_h
