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
class CacheQueryParams;
class CacheReadStream;
class CacheReadStreamOrVoid;
class CacheRequest;
class CacheResponse;

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
  ToCacheRequest(CacheRequest& aOut, InternalRequest* aIn,
                 BodyAction aBodyAction, ReferrerAction aReferrerAction,
                 SchemeAction aSchemeAction, ErrorResult& aRv);

  void
  ToCacheResponseWithoutBody(CacheResponse& aOut, InternalResponse& aIn,
                             ErrorResult& aRv);

  void
  ToCacheResponse(CacheResponse& aOut, Response& aIn, ErrorResult& aRv);

  void
  ToCacheQueryParams(CacheQueryParams& aOut, const CacheQueryOptions& aIn);

  already_AddRefed<Response>
  ToResponse(const CacheResponse& aIn);

  already_AddRefed<InternalRequest>
  ToInternalRequest(const CacheRequest& aIn);

  already_AddRefed<Request>
  ToRequest(const CacheRequest& aIn);

private:
  void
  CheckAndSetBodyUsed(Request* aRequest, BodyAction aBodyAction,
                      ErrorResult& aRv);

  already_AddRefed<InternalRequest>
  ToInternalRequest(const nsAString& aIn, ErrorResult& aRv);

  void
  SerializeCacheStream(nsIInputStream* aStream, CacheReadStreamOrVoid* aStreamOut,
                       ErrorResult& aRv);

  void
  SerializePushStream(nsIInputStream* aStream, CacheReadStream& aReadStreamOut,
                      ErrorResult& aRv);
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_TypesUtils_h
