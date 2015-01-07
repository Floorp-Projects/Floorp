/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InternalRequest_h
#define mozilla_dom_InternalRequest_h

#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/RequestBinding.h"

#include "nsIContentPolicy.h"
#include "nsIInputStream.h"
#include "nsISupportsImpl.h"

class nsIDocument;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class FetchBodyStream;
class Request;

class InternalRequest MOZ_FINAL
{
  friend class Request;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalRequest)

  enum ContextFrameType
  {
    FRAMETYPE_AUXILIARY = 0,
    FRAMETYPE_TOP_LEVEL,
    FRAMETYPE_NESTED,
    FRAMETYPE_NONE,
  };

  // Since referrer type can be none, client or a URL.
  enum ReferrerType
  {
    REFERRER_NONE = 0,
    REFERRER_CLIENT,
    REFERRER_URL,
  };

  enum ResponseTainting
  {
    RESPONSETAINT_BASIC,
    RESPONSETAINT_CORS,
    RESPONSETAINT_OPAQUE,
  };

  explicit InternalRequest()
    : mMethod("GET")
    , mHeaders(new InternalHeaders(HeadersGuardEnum::None))
    , mContextFrameType(FRAMETYPE_NONE)
    , mReferrerType(REFERRER_CLIENT)
    , mMode(RequestMode::No_cors)
    , mCredentialsMode(RequestCredentials::Omit)
    , mResponseTainting(RESPONSETAINT_BASIC)
    , mCacheMode(RequestCache::Default)
    , mAuthenticationFlag(false)
    , mForceOriginHeader(false)
    , mPreserveContentCodings(false)
      // FIXME(nsm): This should be false by default, but will lead to the
      // algorithm never loading data: URLs right now. See Bug 1018872 about
      // how certain contexts will override it to set it to true. Fetch
      // specification does not handle this yet.
    , mSameOriginDataURL(true)
    , mSkipServiceWorker(false)
    , mSynchronous(false)
    , mUnsafeRequest(false)
    , mUseURLCredentials(false)
  {
  }

  explicit InternalRequest(const InternalRequest& aOther)
    : mMethod(aOther.mMethod)
    , mURL(aOther.mURL)
    , mHeaders(aOther.mHeaders)
    , mBodyStream(aOther.mBodyStream)
    , mContext(aOther.mContext)
    , mContextFrameType(aOther.mContextFrameType)
    , mReferrerType(aOther.mReferrerType)
    , mReferrerURL(aOther.mReferrerURL)
    , mMode(aOther.mMode)
    , mCredentialsMode(aOther.mCredentialsMode)
    , mResponseTainting(aOther.mResponseTainting)
    , mCacheMode(aOther.mCacheMode)
    , mAuthenticationFlag(aOther.mAuthenticationFlag)
    , mForceOriginHeader(aOther.mForceOriginHeader)
    , mPreserveContentCodings(aOther.mPreserveContentCodings)
    , mSameOriginDataURL(aOther.mSameOriginDataURL)
    , mSandboxedStorageAreaURLs(aOther.mSandboxedStorageAreaURLs)
    , mSkipServiceWorker(aOther.mSkipServiceWorker)
    , mSynchronous(aOther.mSynchronous)
    , mUnsafeRequest(aOther.mUnsafeRequest)
    , mUseURLCredentials(aOther.mUseURLCredentials)
  {
  }

  void
  GetMethod(nsCString& aMethod) const
  {
    aMethod.Assign(mMethod);
  }

  void
  SetMethod(const nsACString& aMethod)
  {
    mMethod.Assign(aMethod);
  }

  bool
  HasSimpleMethod() const
  {
    return mMethod.LowerCaseEqualsASCII("get") ||
           mMethod.LowerCaseEqualsASCII("post") ||
           mMethod.LowerCaseEqualsASCII("head");
  }

  void
  GetURL(nsCString& aURL) const
  {
    aURL.Assign(mURL);
  }

  void
  SetURL(const nsACString& aURL)
  {
    mURL.Assign(aURL);
  }

  bool
  ReferrerIsNone() const
  {
    return mReferrerType == REFERRER_NONE;
  }

  bool
  ReferrerIsURL() const
  {
    return mReferrerType == REFERRER_URL;
  }

  bool
  ReferrerIsClient() const
  {
    return mReferrerType == REFERRER_CLIENT;
  }

  nsCString
  ReferrerAsURL() const
  {
    MOZ_ASSERT(ReferrerIsURL());
    return mReferrerURL;
  }

  void
  SetReferrer(const nsACString& aReferrer)
  {
    // May be removed later.
    MOZ_ASSERT(!ReferrerIsNone());
    mReferrerType = REFERRER_URL;
    mReferrerURL.Assign(aReferrer);
  }

  bool
  IsSynchronous() const
  {
    return mSynchronous;
  }

  RequestMode
  Mode() const
  {
    return mMode;
  }

  void
  SetMode(RequestMode aMode)
  {
    mMode = aMode;
  }

  RequestCredentials
  GetCredentialsMode() const
  {
    return mCredentialsMode;
  }

  void
  SetCredentialsMode(RequestCredentials aCredentialsMode)
  {
    mCredentialsMode = aCredentialsMode;
  }

  ResponseTainting
  GetResponseTainting() const
  {
    return mResponseTainting;
  }

  void
  SetResponseTainting(ResponseTainting aTainting)
  {
    mResponseTainting = aTainting;
  }

  RequestCache
  GetCacheMode() const
  {
    return mCacheMode;
  }

  nsContentPolicyType
  GetContext() const
  {
    return mContext;
  }

  bool
  UnsafeRequest() const
  {
    return mUnsafeRequest;
  }

  void
  SetUnsafeRequest()
  {
    mUnsafeRequest = true;
  }

  InternalHeaders*
  Headers()
  {
    return mHeaders;
  }

  bool
  ForceOriginHeader()
  {
    return mForceOriginHeader;
  }

  bool
  SameOriginDataURL() const
  {
    return mSameOriginDataURL;
  }

  void
  UnsetSameOriginDataURL()
  {
    mSameOriginDataURL = false;
  }

  void
  SetBody(nsIInputStream* aStream)
  {
    // A request's body may not be reset once set.
    MOZ_ASSERT(!mBodyStream);
    mBodyStream = aStream;
  }

  // Will return the original stream!
  // Use a tee or copy if you don't want to erase the original.
  void
  GetBody(nsIInputStream** aStream)
  {
    nsCOMPtr<nsIInputStream> s = mBodyStream;
    s.forget(aStream);
  }

  // The global is used as the client for the new object.
  already_AddRefed<InternalRequest>
  GetRequestConstructorCopy(nsIGlobalObject* aGlobal, ErrorResult& aRv) const;

private:
  ~InternalRequest();

  nsCString mMethod;
  nsCString mURL;
  nsRefPtr<InternalHeaders> mHeaders;
  nsCOMPtr<nsIInputStream> mBodyStream;

  // nsContentPolicyType does not cover the complete set defined in the spec,
  // but it is a good start.
  nsContentPolicyType mContext;

  ContextFrameType mContextFrameType;
  ReferrerType mReferrerType;

  // When mReferrerType is REFERRER_URL.
  nsCString mReferrerURL;

  RequestMode mMode;
  RequestCredentials mCredentialsMode;
  ResponseTainting mResponseTainting;
  RequestCache mCacheMode;

  bool mAuthenticationFlag;
  bool mForceOriginHeader;
  bool mPreserveContentCodings;
  bool mSameOriginDataURL;
  bool mSandboxedStorageAreaURLs;
  bool mSkipServiceWorker;
  bool mSynchronous;
  bool mUnsafeRequest;
  bool mUseURLCredentials;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InternalRequest_h
