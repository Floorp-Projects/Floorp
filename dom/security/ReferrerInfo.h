/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ReferrerInfo_h
#define mozilla_dom_ReferrerInfo_h

#include "nsCOMPtr.h"
#include "nsIReferrerInfo.h"
#include "nsISerializable.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "nsReadableUtils.h"
#include "mozilla/Maybe.h"

#define REFERRERINFOF_CONTRACTID "@mozilla.org/referrer-info;1"
// 041a129f-10ce-4bda-a60d-e027a26d5ed0
#define REFERRERINFO_CID                             \
  {                                                  \
    0x041a129f, 0x10ce, 0x4bda, {                    \
      0xa6, 0x0d, 0xe0, 0x27, 0xa2, 0x6d, 0x5e, 0xd0 \
    }                                                \
  }

class nsIURI;
class nsIChannel;
class nsILoadInfo;

namespace mozilla {
namespace net {
class HttpBaseChannel;
class nsHttpChannel;
}  // namespace net
}  // namespace mozilla

using mozilla::Maybe;

namespace mozilla {
namespace dom {

/**
 * The ReferrerInfo class holds the raw referrer and potentially a referrer
 * policy which allows to query the computed referrer which should be applied to
 * a channel as the actual referrer value.
 *
 * The ReferrerInfo class solely contains readonly fields and represents a 1:1
 * sync to the referrer header of the corresponding channel. In turn that means
 * the class is immutable - so any modifications require to clone the current
 * ReferrerInfo.
 *
 * For example if a request undergoes a redirect, the new channel
 * will need a new ReferrerInfo clone with members being updated accordingly.
 */

class ReferrerInfo : public nsIReferrerInfo {
 public:
  ReferrerInfo();

  explicit ReferrerInfo(
      nsIURI* aOriginalReferrer, uint32_t aPolicy = mozilla::net::RP_Unset,
      bool aSendReferrer = true,
      const Maybe<nsCString>& aComputedReferrer = Maybe<nsCString>());

  // create an exact copy of the ReferrerInfo
  already_AddRefed<nsIReferrerInfo> Clone() const;

  // create an copy of the ReferrerInfo with new referrer policy
  already_AddRefed<nsIReferrerInfo> CloneWithNewPolicy(uint32_t aPolicy) const;

  /**
   * Check whether the given referrer's scheme is allowed to be computed and
   * sent. The whitelist schemes are: http, https, ftp.
   */
  static bool IsReferrerSchemeAllowed(nsIURI* aReferrer);

  /*
   * Check whether we need to hide referrer when leaving a .onion domain.
   * Controlled by user pref: network.http.referer.hideOnionSource
   */
  static bool HideOnionReferrerSource();

  /**
   * Return default referrer policy which is controlled by user
   * prefs:
   * network.http.referer.defaultPolicy for regular mode
   * network.http.referer.defaultPolicy.trackers for third-party trackers
   * in regular mode
   * network.http.referer.defaultPolicy.pbmode for private mode
   * network.http.referer.defaultPolicy.trackers.pbmode for third-party trackers
   * in private mode
   */
  static uint32_t GetDefaultReferrerPolicy(nsIHttpChannel* aChannel = nullptr,
                                           nsIURI* aURI = nullptr,
                                           bool privateBrowsing = false);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREFERRERINFO
  NS_DECL_NSISERIALIZABLE

 private:
  virtual ~ReferrerInfo() {}

  ReferrerInfo(const ReferrerInfo& rhs);

  /*
   * Default referrer policy to use
   */
  enum DefaultReferrerPolicy : uint32_t {
    eDefaultPolicyNoReferrer = 0,
    eDefaultPolicySameOrgin = 1,
    eDefaultPolicyStrictWhenXorigin = 2,
    eDefaultPolicyNoReferrerWhenDownGrade = 3,
  };

  /*
   * Trimming policy when compute referrer, indicate how much information in the
   * referrer will be sent. Order matters here.
   */
  enum TrimmingPolicy : uint32_t {
    ePolicyFullURI = 0,
    ePolicySchemeHostPortPath = 1,
    ePolicySchemeHostPort = 2,
  };

  /*
   * Referrer sending policy, indicates type of action could trigger to send
   * referrer header, not send at all, send only with user's action (click on a
   * link) or send even with inline content request (image request).
   * Order matters here.
   */
  enum ReferrerSendingPolicy : uint32_t {
    ePolicyNotSend = 0,
    ePolicySendWhenUserTrigger = 1,
    ePolicySendInlineContent = 2,
  };

  /*
   * Sending referrer when cross origin policy, indicates when referrer should
   * be send when compare 2 origins. Order matters here.
   */
  enum XOriginSendingPolicy : uint32_t {
    ePolicyAlwaysSend = 0,
    ePolicySendWhenSameDomain = 1,
    ePolicySendWhenSameHost = 2,
  };

  /**
   * Returns true if the given channel is cross-origin request
   *
   * Computing whether the request is cross-origin may be expensive, so please
   * do that in cases where we're going to use this information later on.
   */
  bool IsCrossOriginRequest(nsIHttpChannel* aChannel) const;

  /*
   * Check whether referrer is allowed to send in secure to insecure scenario.
   */
  nsresult HandleSecureToInsecureReferral(nsIURI* aURI, bool& aAllowed) const;

  /*
   * Handle user controlled pref network.http.referer.XOriginPolicy
   */
  nsresult HandleUserXOriginSendingPolicy(nsIURI* aURI, nsIURI* aReferrer,
                                          bool& aAllowed) const;

  /*
   * Handle user controlled pref network.http.sendRefererHeader
   */
  nsresult HandleUserReferrerSendingPolicy(nsIHttpChannel* aChannel,
                                           bool& aAllowed) const;

  /*
   * Compute trimming policy from user controlled prefs.
   * This function is called when we already made sure a nonempty referrer is
   * allowed to send.
   */
  TrimmingPolicy ComputeTrimmingPolicy(nsIHttpChannel* aChannel) const;

  // HttpBaseChannel could access IsInitialized() and ComputeReferrer();
  friend class mozilla::net::HttpBaseChannel;

  /*
   * Compute referrer for a given channel. The computation result then will be
   * stored in this class and then used to set the actual referrer header of
   * the channel. The computation could be controlled by sereral user prefs
   * which is defined in all.js (see all.js for more details):
   *  network.http.sendRefererHeader
   *  network.http.referer.spoofSource
   *  network.http.referer.hideOnionSource
   *  network.http.referer.XOriginPolicy
   *  network.http.referer.trimmingPolicy
   *  network.http.referer.XOriginTrimmingPolicy
   */
  nsresult ComputeReferrer(nsIHttpChannel* aChannel);

  /*
   * Check whether the ReferrerInfo has been initialized or not.
   */
  bool IsInitialized() { return mInitialized; }

  // nsHttpChannel could access IsPolicyOverrided();
  friend class mozilla::net::nsHttpChannel;
  /*
   * Check whether if unset referrer policy is overrided by default or not
   */
  bool IsPolicyOverrided() { return mOverridePolicyByDefault; }

  /*
   * Trim a given referrer with a given a trimming policy,
   */
  nsresult TrimReferrerWithPolicy(nsCOMPtr<nsIURI>& aReferrer,
                                  TrimmingPolicy aTrimmingPolicy,
                                  nsACString& aResult) const;

  nsCOMPtr<nsIURI> mOriginalReferrer;

  uint32_t mPolicy;

  // Indicates if the referrer should be sent or not even when it's available
  // (default is true).
  bool mSendReferrer;

  // Since the ReferrerInfo is immutable, we use this member as a helper to
  // ensure no one can call e.g. init() twice to modify state of the
  // ReferrerInfo.
  bool mInitialized;

  // Indicates if unset referrer policy is overrided by default
  bool mOverridePolicyByDefault;

  // Store a computed referrer for a given channel
  Maybe<nsCString> mComputedReferrer;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ReferrerInfo_h
