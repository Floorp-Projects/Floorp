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
#include "mozilla/net/ReferrerPolicy.h"

#include "nsIContentPolicy.h"
#include "nsIInputStream.h"
#include "nsISupportsImpl.h"
#ifdef DEBUG
#  include "nsIURLParser.h"
#  include "nsNetCID.h"
#  include "nsServiceManagerUtils.h"
#endif

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

namespace dom {

/*
 * The mapping of RequestDestination and nsContentPolicyType is currently as the
 * following.  Note that this mapping is not perfect yet (see the TODO comments
 * below for examples).
 *
 * RequestDestination| nsContentPolicyType
 * ------------------+--------------------
 * audio             | TYPE_INTERNAL_AUDIO
 * audioworklet      | TODO
 * document          | TYPE_DOCUMENT, TYPE_INTERNAL_IFRAME, TYPE_SUBDOCUMENT
 * embed             | TYPE_INTERNAL_EMBED
 * font              | TYPE_FONT
 * image             | TYPE_INTERNAL_IMAGE, TYPE_INTERNAL_IMAGE_PRELOAD,
 *                   | TYPE_IMAGE, TYPE_INTERNAL_IMAGE_FAVICON, TYPE_IMAGESET
 * manifest          | TYPE_WEB_MANIFEST
 * object            | TYPE_INTERNAL_OBJECT, TYPE_OBJECT
 * "paintworklet"    | TODO
 * report"           | TODO
 * script            | TYPE_INTERNAL_SCRIPT, TYPE_INTERNAL_SCRIPT_PRELOAD,
 *                   | TYPE_INTERNAL_MODULE, TYPE_INTERNAL_MODULE_PRELOAD,
 *                   | TYPE_SCRIPT,
 *                   | TYPE_INTERNAL_SERVICE_WORKER,
 *                   | TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS
 * sharedworker      | TYPE_INTERNAL_SHARED_WORKER
 * serviceworker     | The spec lists this as a valid value for the enum,
 *                   | however it is impossible to observe a request with this
 *                   | destination value.
 * style             | TYPE_INTERNAL_STYLESHEET,
 *                   | TYPE_INTERNAL_STYLESHEET_PRELOAD,
 *                   | TYPE_STYLESHEET
 * track             | TYPE_INTERNAL_TRACK
 * video             | TYPE_INTERNAL_VIDEO
 * worker            | TYPE_INTERNAL_WORKER
 * xslt              | TYPE_XSLT
 * _empty            | Default for everything else.
 *
 */

class Request;

#define kFETCH_CLIENT_REFERRER_STR "about:client"
class InternalRequest final {
  friend class Request;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InternalRequest)
  InternalRequest(const nsACString& aURL, const nsACString& aFragment);
  InternalRequest(const nsACString& aURL, const nsACString& aFragment,
                  const nsACString& aMethod,
                  already_AddRefed<InternalHeaders> aHeaders,
                  RequestCache aCacheMode, RequestMode aMode,
                  RequestRedirect aRequestRedirect,
                  RequestCredentials aRequestCredentials,
                  const nsAString& aReferrer, ReferrerPolicy aReferrerPolicy,
                  nsContentPolicyType aContentPolicyType,
                  const nsAString& aIntegrity);

  already_AddRefed<InternalRequest> Clone();

  void GetMethod(nsCString& aMethod) const { aMethod.Assign(mMethod); }

  void SetMethod(const nsACString& aMethod) { mMethod.Assign(aMethod); }

  bool HasSimpleMethod() const {
    return mMethod.LowerCaseEqualsASCII("get") ||
           mMethod.LowerCaseEqualsASCII("post") ||
           mMethod.LowerCaseEqualsASCII("head");
  }
  // GetURL should get the request's current url with fragment. A request has
  // an associated current url. It is a pointer to the last fetch URL in
  // request's url list.
  void GetURL(nsACString& aURL) const {
    aURL.Assign(GetURLWithoutFragment());
    if (GetFragment().IsEmpty()) {
      return;
    }
    aURL.AppendLiteral("#");
    aURL.Append(GetFragment());
  }

  const nsCString& GetURLWithoutFragment() const {
    MOZ_RELEASE_ASSERT(!mURLList.IsEmpty(),
                       "Internal Request's urlList should not be empty.");

    return mURLList.LastElement();
  }

  // A safe guard for ensuring that request's URL is only allowed to be set in a
  // sw internal redirect.
  void SetURLForInternalRedirect(const uint32_t aFlag, const nsACString& aURL,
                                 const nsACString& aFragment) {
    // Only check in debug build to prevent it from being used unexpectedly.
    MOZ_ASSERT(aFlag & nsIChannelEventSink::REDIRECT_INTERNAL);

    return SetURL(aURL, aFragment);
  }

  // AddURL should append the url into url list.
  // Normally we strip the fragment from the URL in Request::Constructor and
  // pass the fragment as the second argument into it.
  // If a fragment is present in the URL it must be stripped and passed in
  // separately.
  void AddURL(const nsACString& aURL, const nsACString& aFragment) {
    MOZ_ASSERT(!aURL.IsEmpty());
    MOZ_ASSERT(!aURL.Contains('#'));

    mURLList.AppendElement(aURL);

    mFragment.Assign(aFragment);
  }
  // Get the URL list without their fragments.
  void GetURLListWithoutFragment(nsTArray<nsCString>& aURLList) {
    aURLList.Assign(mURLList);
  }
  void GetReferrer(nsAString& aReferrer) const { aReferrer.Assign(mReferrer); }

  void SetReferrer(const nsAString& aReferrer) {
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
        nsresult rv =
            parser->ParseURL(ref.get(), ref.Length(), &schemePos, &schemeLen,
                             &authorityPos, &authorityLen, &pathPos, &pathLen);
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

  ReferrerPolicy ReferrerPolicy_() const { return mReferrerPolicy; }

  void SetReferrerPolicy(ReferrerPolicy aReferrerPolicy) {
    mReferrerPolicy = aReferrerPolicy;
  }

  void SetReferrerPolicy(net::ReferrerPolicy aReferrerPolicy) {
    switch (aReferrerPolicy) {
      case net::RP_Unset:
        mReferrerPolicy = ReferrerPolicy::_empty;
        break;
      case net::RP_No_Referrer:
        mReferrerPolicy = ReferrerPolicy::No_referrer;
        break;
      case net::RP_No_Referrer_When_Downgrade:
        mReferrerPolicy = ReferrerPolicy::No_referrer_when_downgrade;
        break;
      case net::RP_Origin:
        mReferrerPolicy = ReferrerPolicy::Origin;
        break;
      case net::RP_Origin_When_Crossorigin:
        mReferrerPolicy = ReferrerPolicy::Origin_when_cross_origin;
        break;
      case net::RP_Unsafe_URL:
        mReferrerPolicy = ReferrerPolicy::Unsafe_url;
        break;
      case net::RP_Same_Origin:
        mReferrerPolicy = ReferrerPolicy::Same_origin;
        break;
      case net::RP_Strict_Origin:
        mReferrerPolicy = ReferrerPolicy::Strict_origin;
        break;
      case net::RP_Strict_Origin_When_Cross_Origin:
        mReferrerPolicy = ReferrerPolicy::Strict_origin_when_cross_origin;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid ReferrerPolicy value");
        break;
    }
  }

  net::ReferrerPolicy GetReferrerPolicy() {
    switch (mReferrerPolicy) {
      case ReferrerPolicy::_empty:
        return net::RP_Unset;
      case ReferrerPolicy::No_referrer:
        return net::RP_No_Referrer;
      case ReferrerPolicy::No_referrer_when_downgrade:
        return net::RP_No_Referrer_When_Downgrade;
      case ReferrerPolicy::Origin:
        return net::RP_Origin;
      case ReferrerPolicy::Origin_when_cross_origin:
        return net::RP_Origin_When_Crossorigin;
      case ReferrerPolicy::Unsafe_url:
        return net::RP_Unsafe_URL;
      case ReferrerPolicy::Strict_origin:
        return net::RP_Strict_Origin;
      case ReferrerPolicy::Same_origin:
        return net::RP_Same_Origin;
      case ReferrerPolicy::Strict_origin_when_cross_origin:
        return net::RP_Strict_Origin_When_Cross_Origin;
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid ReferrerPolicy enum value?");
        break;
    }
    return net::RP_Unset;
  }

  net::ReferrerPolicy GetEnvironmentReferrerPolicy() const {
    return mEnvironmentReferrerPolicy;
  }

  void SetEnvironmentReferrerPolicy(net::ReferrerPolicy aReferrerPolicy) {
    mEnvironmentReferrerPolicy = aReferrerPolicy;
  }

  bool SkipServiceWorker() const { return mSkipServiceWorker; }

  void SetSkipServiceWorker() { mSkipServiceWorker = true; }

  bool IsSynchronous() const { return mSynchronous; }

  RequestMode Mode() const { return mMode; }

  void SetMode(RequestMode aMode) { mMode = aMode; }

  RequestCredentials GetCredentialsMode() const { return mCredentialsMode; }

  void SetCredentialsMode(RequestCredentials aCredentialsMode) {
    mCredentialsMode = aCredentialsMode;
  }

  LoadTainting GetResponseTainting() const { return mResponseTainting; }

  void MaybeIncreaseResponseTainting(LoadTainting aTainting) {
    if (aTainting > mResponseTainting) {
      mResponseTainting = aTainting;
    }
  }

  RequestCache GetCacheMode() const { return mCacheMode; }

  void SetCacheMode(RequestCache aCacheMode) { mCacheMode = aCacheMode; }

  RequestRedirect GetRedirectMode() const { return mRedirectMode; }

  void SetRedirectMode(RequestRedirect aRedirectMode) {
    mRedirectMode = aRedirectMode;
  }

  const nsString& GetIntegrity() const { return mIntegrity; }
  void SetIntegrity(const nsAString& aIntegrity) {
    MOZ_ASSERT(mIntegrity.IsEmpty());
    mIntegrity.Assign(aIntegrity);
  }

  bool MozErrors() const { return mMozErrors; }

  void SetMozErrors() { mMozErrors = true; }

  const nsCString& GetFragment() const { return mFragment; }

  nsContentPolicyType ContentPolicyType() const { return mContentPolicyType; }
  void SetContentPolicyType(nsContentPolicyType aContentPolicyType);

  void OverrideContentPolicyType(nsContentPolicyType aContentPolicyType);

  RequestDestination Destination() const {
    return MapContentPolicyTypeToRequestDestination(mContentPolicyType);
  }

  bool UnsafeRequest() const { return mUnsafeRequest; }

  void SetUnsafeRequest() { mUnsafeRequest = true; }

  InternalHeaders* Headers() { return mHeaders; }

  void SetHeaders(InternalHeaders* aHeaders) {
    MOZ_ASSERT(aHeaders);
    mHeaders = aHeaders;
  }

  bool SameOriginDataURL() const { return mSameOriginDataURL; }

  void UnsetSameOriginDataURL() { mSameOriginDataURL = false; }

  void SetBody(nsIInputStream* aStream, int64_t aBodyLength) {
    // A request's body may not be reset once set.
    MOZ_ASSERT_IF(aStream, !mBodyStream);
    mBodyStream = aStream;
    mBodyLength = aBodyLength;
  }

  // Will return the original stream!
  // Use a tee or copy if you don't want to erase the original.
  void GetBody(nsIInputStream** aStream, int64_t* aBodyLength = nullptr) {
    nsCOMPtr<nsIInputStream> s = mBodyStream;
    s.forget(aStream);

    if (aBodyLength) {
      *aBodyLength = mBodyLength;
    }
  }

  void SetBodyBlobURISpec(nsACString& aBlobURISpec) {
    mBodyBlobURISpec = aBlobURISpec;
  }

  const nsACString& BodyBlobURISpec() const { return mBodyBlobURISpec; }

  void SetBodyLocalPath(nsAString& aLocalPath) { mBodyLocalPath = aLocalPath; }

  const nsAString& BodyLocalPath() const { return mBodyLocalPath; }

  // The global is used as the client for the new object.
  already_AddRefed<InternalRequest> GetRequestConstructorCopy(
      nsIGlobalObject* aGlobal, ErrorResult& aRv) const;

  bool WasCreatedByFetchEvent() const { return mCreatedByFetchEvent; }

  void SetCreatedByFetchEvent() { mCreatedByFetchEvent = true; }

  void ClearCreatedByFetchEvent() { mCreatedByFetchEvent = false; }

  bool IsNavigationRequest() const;

  bool IsWorkerRequest() const;

  bool IsClientRequest() const;

  void MaybeSkipCacheIfPerformingRevalidation();

  bool IsContentPolicyTypeOverridden() const {
    return mContentPolicyTypeOverridden;
  }

  static RequestMode MapChannelToRequestMode(nsIChannel* aChannel);

  static RequestCredentials MapChannelToRequestCredentials(
      nsIChannel* aChannel);

  // Takes ownership of the principal info.
  void SetPrincipalInfo(UniquePtr<mozilla::ipc::PrincipalInfo> aPrincipalInfo);

  const UniquePtr<mozilla::ipc::PrincipalInfo>& GetPrincipalInfo() const {
    return mPrincipalInfo;
  }

  const nsCString& GetPreferredAlternativeDataType() const {
    return mPreferredAlternativeDataType;
  }

  void SetPreferredAlternativeDataType(const nsACString& aDataType) {
    mPreferredAlternativeDataType = aDataType;
  }

 private:
  // Does not copy mBodyStream.  Use fallible Clone() for complete copy.
  explicit InternalRequest(const InternalRequest& aOther);

  ~InternalRequest();

  // Map the content policy type to the associated fetch destination, as defined
  // by the spec at https://fetch.spec.whatwg.org/#concept-request-destination.
  // Note that while the HTML spec for the "Link" element and its "as" attribute
  // (https://html.spec.whatwg.org/#attr-link-as) reuse fetch's definition of
  // destination, and the Link class has an internal Link::AsDestination enum
  // type, the latter is only a support type to map the string values via
  // Link::ParseAsValue and Link::AsValueToContentPolicy to our canonical
  // nsContentPolicyType.
  static RequestDestination MapContentPolicyTypeToRequestDestination(
      nsContentPolicyType aContentPolicyType);

  static bool IsNavigationContentPolicy(nsContentPolicyType aContentPolicyType);

  static bool IsWorkerContentPolicy(nsContentPolicyType aContentPolicyType);

  // It should only be called while there is a service-worker-internal-redirect.
  void SetURL(const nsACString& aURL, const nsACString& aFragment) {
    MOZ_ASSERT(!aURL.IsEmpty());
    MOZ_ASSERT(!aURL.Contains('#'));
    MOZ_ASSERT(mURLList.Length() > 0);

    mURLList.LastElement() = aURL;
    mFragment.Assign(aFragment);
  }

  nsCString mMethod;
  // mURLList: a list of one or more fetch URLs
  nsTArray<nsCString> mURLList;
  RefPtr<InternalHeaders> mHeaders;
  nsCString mBodyBlobURISpec;
  nsString mBodyLocalPath;
  nsCOMPtr<nsIInputStream> mBodyStream;
  int64_t mBodyLength;

  nsCString mPreferredAlternativeDataType;

  nsContentPolicyType mContentPolicyType;

  // Empty string: no-referrer
  // "about:client": client (default)
  // URL: an URL
  nsString mReferrer;
  ReferrerPolicy mReferrerPolicy;

  // This will be used for request created from Window or Worker contexts
  // In case there's no Referrer Policy in Request, this will be passed to
  // channel.
  // The Environment Referrer Policy should be net::ReferrerPolicy so that it
  // could be associated with nsIHttpChannel.
  net::ReferrerPolicy mEnvironmentReferrerPolicy;
  RequestMode mMode;
  RequestCredentials mCredentialsMode;
  MOZ_INIT_OUTSIDE_CTOR LoadTainting mResponseTainting;
  RequestCache mCacheMode;
  RequestRedirect mRedirectMode;
  nsString mIntegrity;
  bool mMozErrors;
  nsCString mFragment;
  MOZ_INIT_OUTSIDE_CTOR bool mAuthenticationFlag;
  MOZ_INIT_OUTSIDE_CTOR bool mPreserveContentCodings;
  MOZ_INIT_OUTSIDE_CTOR bool mSameOriginDataURL;
  MOZ_INIT_OUTSIDE_CTOR bool mSkipServiceWorker;
  MOZ_INIT_OUTSIDE_CTOR bool mSynchronous;
  MOZ_INIT_OUTSIDE_CTOR bool mUnsafeRequest;
  MOZ_INIT_OUTSIDE_CTOR bool mUseURLCredentials;
  // This is only set when a Request object is created by a fetch event.  We
  // use it to check if Service Workers are simply fetching intercepted Request
  // objects without modifying them.
  bool mCreatedByFetchEvent = false;
  // This is only set when Request.overrideContentPolicyType() has been set.
  // It is illegal to pass such a Request object to a fetch() method unless
  // if the caller has chrome privileges.
  bool mContentPolicyTypeOverridden = false;

  UniquePtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_InternalRequest_h
