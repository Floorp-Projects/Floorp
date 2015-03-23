/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_TypesUtils_h
#define mozilla_dom_cache_TypesUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "nsError.h"

class nsIGlobalObject;
class nsIAsyncInputStream;
class nsIInputStream;

namespace mozilla {
namespace dom {

struct CacheQueryOptions;
class InternalRequest;
class InternalResponse;
class OwningRequestOrUSVString;
class Request;
class RequestOrUSVString;
class Response;

namespace cache {

class CachePushStreamChild;
class PCacheQueryParams;
class PCacheReadStream;
class PCacheReadStreamOrVoid;
class PCacheRequest;
class PCacheResponse;

class TypeUtils
{
public:
  enum BodyAction
  {
    IgnoreBody,
    ReadBody
  };

  enum ReferrerAction
  {
    PassThroughReferrer,
    ExpandReferrer
  };

  enum SchemeAction
  {
    IgnoreInvalidScheme,
    TypeErrorOnInvalidScheme,
    NetworkErrorOnInvalidScheme
  };

  ~TypeUtils() { }
  virtual nsIGlobalObject* GetGlobalObject() const = 0;
#ifdef DEBUG
  virtual void AssertOwningThread() const = 0;
#else
  inline void AssertOwningThread() const { }
#endif

  virtual CachePushStreamChild*
  CreatePushStream(nsIAsyncInputStream* aStream) = 0;

  already_AddRefed<InternalRequest>
  ToInternalRequest(const RequestOrUSVString& aIn, BodyAction aBodyAction,
                    ErrorResult& aRv);

  already_AddRefed<InternalRequest>
  ToInternalRequest(const OwningRequestOrUSVString& aIn, BodyAction aBodyAction,
                    ErrorResult& aRv);

  void
  ToPCacheRequest(PCacheRequest& aOut, InternalRequest* aIn,
                  BodyAction aBodyAction, ReferrerAction aReferrerAction,
                  SchemeAction aSchemeAction, ErrorResult& aRv);

  void
  ToPCacheResponseWithoutBody(PCacheResponse& aOut, InternalResponse& aIn,
                              ErrorResult& aRv);

  void
  ToPCacheResponse(PCacheResponse& aOut, Response& aIn, ErrorResult& aRv);

  void
  ToPCacheQueryParams(PCacheQueryParams& aOut, const CacheQueryOptions& aIn);

  already_AddRefed<Response>
  ToResponse(const PCacheResponse& aIn);

  already_AddRefed<InternalRequest>
  ToInternalRequest(const PCacheRequest& aIn);

  already_AddRefed<Request>
  ToRequest(const PCacheRequest& aIn);

private:
  void
  CheckAndSetBodyUsed(Request* aRequest, BodyAction aBodyAction,
                      ErrorResult& aRv);

  already_AddRefed<InternalRequest>
  ToInternalRequest(const nsAString& aIn, ErrorResult& aRv);

  void
  SerializeCacheStream(nsIInputStream* aStream, PCacheReadStreamOrVoid* aStreamOut,
                       ErrorResult& aRv);

  void
  SerializePushStream(nsIInputStream* aStream, PCacheReadStream& aReadStreamOut,
                      ErrorResult& aRv);
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_TypesUtils_h
