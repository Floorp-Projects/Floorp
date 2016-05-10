/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_InternalRequest_h
#define mozilla_dom_InternalRequest_h

#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/LoadTainting.h"

#include "nsIContentPolicy.h"
#include "nsIInputStream.h"
#include "nsISupportsImpl.h"
#ifdef DEBUG
#include "nsIURLParser.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#endif

namespace mozilla {
namespace dom {

/*
 * The mapping of RequestContext and nsContentPolicyType is currently as the
 * following.  Note that this mapping is not perfect yet (see the TODO comments
 * below for examples).
 *
 * RequestContext    | nsContentPolicyType
 * ------------------+--------------------
 * audio             | TYPE_INTERNAL_AUDIO
 * beacon            | TYPE_BEACON
 * cspreport         | TYPE_CSP_REPORT
 * download          |
 * embed             | TYPE_INTERNAL_EMBED
 * eventsource       |
 * favicon           |
 * fetch             | TYPE_FETCH
 * font              | TYPE_FONT
 * form              |
 * frame             | TYPE_INTERNAL_FRAME
 * hyperlink         |
 * iframe            | TYPE_INTERNAL_IFRAME
 * image             | TYPE_INTERNAL_IMAGE, TYPE_INTERNAL_IMAGE_PRELOAD
 * imageset          | TYPE_IMAGESET
 * import            | Not supported by Gecko
 * internal          | TYPE_DOCUMENT, TYPE_XBL, TYPE_OTHER
 * location          |
 * manifest          | TYPE_WEB_MANIFEST
 * object            | TYPE_INTERNAL_OBJECT
 * ping              | TYPE_PING
 * plugin            | TYPE_OBJECT_SUBREQUEST
 * prefetch          |
 * script            | TYPE_INTERNAL_SCRIPT, TYPE_INTERNAL_SCRIPT_PRELOAD
 * sharedworker      | TYPE_INTERNAL_SHARED_WORKER
 * subresource       | Not supported by Gecko
 * style             | TYPE_INTERNAL_STYLESHEET, TYPE_INTERNAL_STYLESHEET_PRELOAD
 * track             | TYPE_INTERNAL_TRACK
 * video             | TYPE_INTERNAL_VIDEO
 * worker            | TYPE_INTERNAL_WORKER
 * xmlhttprequest    | TYPE_INTERNAL_XMLHTTPREQUEST
 * eventsource       | TYPE_INTERNAL_EVENTSOURCE
 * xslt              | TYPE_XSLT
 *
 * TODO: Figure out if TYPE_REFRESH maps to anything useful
 * TODO: Figure out if TYPE_DTD maps to anything useful
 * TODO: Figure out if TYPE_WEBSOCKET maps to anything useful
 * TODO: Add a content type for prefetch
 * TODO: Use the content type for manifest when it becomes available
 * TODO: Add a content type for location
 * TODO: Add a content type for hyperlink
 * TODO: Add a content type for form
 * TODO: Add a content type for favicon
 * TODO: Add a content type for download
 */

class Request;

#define kFETCH_CLIENT_REFERRER_STR "about:client"

class InternalRequest final
{
  friend class Request;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalRequest)

  InternalRequest()
    : mMethod("GET")
    , mHeaders(new InternalHeaders(HeadersGuardEnum::None))
    , mContentPolicyType(nsIContentPolicy::TYPE_FETCH)
    , mReferrer(NS_LITERAL_STRING(kFETCH_CLIENT_REFERRER_STR))
    , mReferrerPolicy(ReferrerPolicy::_empty)
    , mMode(RequestMode::No_cors)
    , mCredentialsMode(RequestCredentials::Omit)
    , mResponseTainting(LoadTainting::Basic)
    , mCacheMode(RequestCache::Default)
    , mRedirectMode(RequestRedirect::Follow)
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

  InternalRequest(const nsACString& aURL,
                  const nsACString& aMethod,
                  already_AddRefed<InternalHeaders> aHeaders,
                  RequestCache aCacheMode,
                  RequestMode aMode,
                  RequestRedirect aRequestRedirect,
                  RequestCredentials aRequestCredentials,
                  const nsAString& aReferrer,
                  ReferrerPolicy aReferrerPolicy,
                  nsContentPolicyType aContentPolicyType)
    : mMethod(aMethod)
    , mURL(aURL)
    , mHeaders(aHeaders)
    , mContentPolicyType(aContentPolicyType)
    , mReferrer(aReferrer)
    , mReferrerPolicy(aReferrerPolicy)
    , mMode(aMode)
    , mCredentialsMode(aRequestCredentials)
    , mResponseTainting(LoadTainting::Basic)
    , mCacheMode(aCacheMode)
    , mRedirectMode(aRequestRedirect)
    , mAuthenticationFlag(false)
    , mForceOriginHeader(false)
    , mPreserveContentCodings(false)
      // FIXME See the above comment in the default constructor.
    , mSameOriginDataURL(true)
    , mSkipServiceWorker(false)
    , mSynchronous(false)
    , mUnsafeRequest(false)
    , mUseURLCredentials(false)
  {
    // Normally we strip the fragment from the URL in Request::Constructor.
    // If internal code is directly constructing this object they must
    // strip the fragment first.  Since these should be well formed URLs we
    // can use a simple check for a fragment here.  The full parser is
    // difficult to use off the main thread.
    MOZ_ASSERT(mURL.Find(NS_LITERAL_CSTRING("#")) == kNotFound);
  }

  already_AddRefed<InternalRequest> Clone();

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

  void
  GetReferrer(nsAString& aReferrer) const
  {
    aReferrer.Assign(mReferrer);
  }

  void
  SetReferrer(const nsAString& aReferrer)
  {
#ifdef DEBUG
    bool validReferrer = false;
    if (aReferrer.IsEmpty() ||
        aReferrer.EqualsLiteral(kFETCH_CLIENT_REFERRER_STR)) {
      validReferrer = true;
    } else {
      nsCOMPtr<nsIURLParser> parser = do_GetService(NS_STDURLPARSER_CONTRACTID);
      if (!parser) {
        NS_WARNING("Could not get parser to validate URL!");
      } else {
        uint32_t schemePos;
        int32_t schemeLen;
        uint32_t authorityPos;
        int32_t authorityLen;
        uint32_t pathPos;
        int32_t pathLen;

        NS_ConvertUTF16toUTF8 ref(aReferrer);
        nsresult rv = parser->ParseURL(ref.get(), ref.Length(),
                                       &schemePos, &schemeLen,
                                       &authorityPos, &authorityLen,
                                       &pathPos, &pathLen);
        if (NS_FAILED(rv)) {
          NS_WARNING("Invalid referrer URL!");
        } else if (schemeLen < 0 || authorityLen < 0) {
          NS_WARNING("Invalid referrer URL!");
        } else {
          validReferrer = true;
        }
      }
    }

    MOZ_ASSERT(validReferrer);
#endif

    mReferrer.Assign(aReferrer);
  }

  ReferrerPolicy
  ReferrerPolicy_() const
  {
    return mReferrerPolicy;
  }

  void
  SetReferrerPolicy(ReferrerPolicy aReferrerPolicy)
  {
    mReferrerPolicy = aReferrerPolicy;
  }

  bool
  SkipServiceWorker() const
  {
    return mSkipServiceWorker;
  }

  void
  SetSkipServiceWorker()
  {
    mSkipServiceWorker = true;
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

  LoadTainting
  GetResponseTainting() const
  {
    return mResponseTainting;
  }

  void
  MaybeIncreaseResponseTainting(LoadTainting aTainting)
  {
    if (aTainting > mResponseTainting) {
      mResponseTainting = aTainting;
    }
  }

  RequestCache
  GetCacheMode() const
  {
    return mCacheMode;
  }

  void
  SetCacheMode(RequestCache aCacheMode)
  {
    mCacheMode = aCacheMode;
  }

  RequestRedirect
  GetRedirectMode() const
  {
    return mRedirectMode;
  }

  void
  SetRedirectMode(RequestRedirect aRedirectMode)
  {
    mRedirectMode = aRedirectMode;
  }

  nsContentPolicyType
  ContentPolicyType() const
  {
    return mContentPolicyType;
  }

  void
  SetContentPolicyType(nsContentPolicyType aContentPolicyType);

  void
  OverrideContentPolicyType(nsContentPolicyType aContentPolicyType);

  RequestContext
  Context() const
  {
    return MapContentPolicyTypeToRequestContext(mContentPolicyType);
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
    MOZ_ASSERT_IF(aStream, !mBodyStream);
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

  bool
  WasCreatedByFetchEvent() const
  {
    return mCreatedByFetchEvent;
  }

  void
  SetCreatedByFetchEvent()
  {
    mCreatedByFetchEvent = true;
  }

  void
  ClearCreatedByFetchEvent()
  {
    mCreatedByFetchEvent = false;
  }

  bool
  IsNavigationRequest() const;

  bool
  IsWorkerRequest() const;

  bool
  IsClientRequest() const;

  void
  MaybeSkipCacheIfPerformingRevalidation();

  bool
  IsContentPolicyTypeOverridden() const
  {
    return mContentPolicyTypeOverridden;
  }

  static RequestMode
  MapChannelToRequestMode(nsIChannel* aChannel);

  static RequestCredentials
  MapChannelToRequestCredentials(nsIChannel* aChannel);

private:
  // Does not copy mBodyStream.  Use fallible Clone() for complete copy.
  explicit InternalRequest(const InternalRequest& aOther);

  ~InternalRequest();

  static RequestContext
  MapContentPolicyTypeToRequestContext(nsContentPolicyType aContentPolicyType);

  static bool
  IsNavigationContentPolicy(nsContentPolicyType aContentPolicyType);

  static bool
  IsWorkerContentPolicy(nsContentPolicyType aContentPolicyType);

  nsCString mMethod;
  // mURL always stores the url with the ref stripped
  nsCString mURL;
  RefPtr<InternalHeaders> mHeaders;
  nsCOMPtr<nsIInputStream> mBodyStream;

  nsContentPolicyType mContentPolicyType;

  // Empty string: no-referrer
  // "about:client": client (default)
  // URL: an URL
  nsString mReferrer;
  ReferrerPolicy mReferrerPolicy;

  RequestMode mMode;
  RequestCredentials mCredentialsMode;
  LoadTainting mResponseTainting;
  RequestCache mCacheMode;
  RequestRedirect mRedirectMode;

  bool mAuthenticationFlag;
  bool mForceOriginHeader;
  bool mPreserveContentCodings;
  bool mSameOriginDataURL;
  bool mSkipServiceWorker;
  bool mSynchronous;
  bool mUnsafeRequest;
  bool mUseURLCredentials;
  // This is only set when a Request object is created by a fetch event.  We
  // use it to check if Service Workers are simply fetching intercepted Request
  // objects without modifying them.
  bool mCreatedByFetchEvent = false;
  // This is only set when Request.overrideContentPolicyType() has been set.
  // It is illegal to pass such a Request object to a fetch() method unless
  // if the caller has chrome privileges.
  bool mContentPolicyTypeOverridden = false;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_InternalRequest_h
