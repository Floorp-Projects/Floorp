/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_TypesUtils_h
#define mozilla_dom_cache_TypesUtils_h

#include "mozilla/AlreadyAddRefed.h"     // for already_AddRefed
#include "mozilla/UniquePtr.h"           // for UniquePtr
#include "mozilla/dom/HeadersBinding.h"  // for HeadersGuardEnum, HeadersGua...
#include "mozilla/dom/SafeRefPtr.h"      // for SafeRefPtr
#include "nsStringFwd.h"                 // for nsACString, nsAString

class nsIGlobalObject;
class nsIAsyncInputStream;
class nsIInputStream;

namespace mozilla {

namespace ipc {
class PBackgroundChild;
class AutoIPCStream;
}  // namespace ipc

namespace dom {

struct CacheQueryOptions;
struct MultiCacheQueryOptions;
class InternalHeaders;
class InternalRequest;
class InternalResponse;
class OwningRequestOrUSVString;
class Request;
class RequestOrUSVString;
class Response;

namespace cache {

class CacheQueryParams;
class CacheReadStream;
class CacheRequest;
class CacheResponse;
class HeadersEntry;

class TypeUtils {
 public:
  enum BodyAction { IgnoreBody, ReadBody };

  enum SchemeAction { IgnoreInvalidScheme, TypeErrorOnInvalidScheme };

  ~TypeUtils() = default;
  virtual nsIGlobalObject* GetGlobalObject() const = 0;
#ifdef DEBUG
  virtual void AssertOwningThread() const = 0;
#else
  inline void AssertOwningThread() const {}
#endif

  // This is mainly declared to support serializing body streams.  Some
  // TypeUtils implementations do not expect to be used for this kind of
  // serialization.  These classes will MOZ_CRASH() if you try to call
  // GetIPCManager().
  virtual mozilla::ipc::PBackgroundChild* GetIPCManager() = 0;

  SafeRefPtr<InternalRequest> ToInternalRequest(JSContext* aCx,
                                                const RequestOrUSVString& aIn,
                                                BodyAction aBodyAction,
                                                ErrorResult& aRv);

  SafeRefPtr<InternalRequest> ToInternalRequest(
      JSContext* aCx, const OwningRequestOrUSVString& aIn,
      BodyAction aBodyAction, ErrorResult& aRv);

  void ToCacheRequest(
      CacheRequest& aOut, const InternalRequest& aIn, BodyAction aBodyAction,
      SchemeAction aSchemeAction,
      nsTArray<UniquePtr<mozilla::ipc::AutoIPCStream>>& aStreamCleanupList,
      ErrorResult& aRv);

  void ToCacheResponseWithoutBody(CacheResponse& aOut, InternalResponse& aIn,
                                  ErrorResult& aRv);

  void ToCacheResponse(
      JSContext* aCx, CacheResponse& aOut, Response& aIn,
      nsTArray<UniquePtr<mozilla::ipc::AutoIPCStream>>& aStreamCleanupList,
      ErrorResult& aRv);

  void ToCacheQueryParams(CacheQueryParams& aOut, const CacheQueryOptions& aIn);

  void ToCacheQueryParams(CacheQueryParams& aOut,
                          const MultiCacheQueryOptions& aIn);

  already_AddRefed<Response> ToResponse(const CacheResponse& aIn);

  SafeRefPtr<InternalRequest> ToInternalRequest(const CacheRequest& aIn);

  SafeRefPtr<Request> ToRequest(const CacheRequest& aIn);

  // static methods
  static already_AddRefed<InternalHeaders> ToInternalHeaders(
      const nsTArray<HeadersEntry>& aHeadersEntryList,
      HeadersGuardEnum aGuard = HeadersGuardEnum::None);

  // Utility method for parsing a URL and doing associated operations.  A mix
  // of things are done in this one method to avoid duplicated parsing:
  //
  //  1) The aUrl argument is modified to strip the fragment
  //  2) If aSchemaValidOut is set, then a boolean value is set indicating
  //     if the aUrl's scheme is valid or not for storing in the cache.
  //  3) If aUrlWithoutQueryOut is set, then a url string is provided without
  //     the search section.
  //  4) If aUrlQueryOut is set then its populated with the search section
  //     of the URL.  Note, this parameter must be set if aUrlWithoutQueryOut
  //     is set.  They must either both be nullptr or set to valid string
  //     pointers.
  //
  // Any errors are thrown on ErrorResult.
  static void ProcessURL(nsACString& aUrl, bool* aSchemeValidOut,
                         nsACString* aUrlWithoutQueryOut,
                         nsACString* aUrlQueryOut, ErrorResult& aRv);

 private:
  void CheckAndSetBodyUsed(JSContext* aCx, Request& aRequest,
                           BodyAction aBodyAction, ErrorResult& aRv);

  SafeRefPtr<InternalRequest> ToInternalRequest(const nsAString& aIn,
                                                ErrorResult& aRv);

  void SerializeCacheStream(
      nsIInputStream* aStream, Maybe<CacheReadStream>* aStreamOut,
      nsTArray<UniquePtr<mozilla::ipc::AutoIPCStream>>& aStreamCleanupList,
      ErrorResult& aRv);

  void SerializeSendStream(nsIInputStream* aStream,
                           CacheReadStream& aReadStreamOut, ErrorResult& aRv);
};

}  // namespace cache
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_cache_TypesUtils_h
