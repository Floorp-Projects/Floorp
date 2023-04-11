/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentSecurityManager_h___
#define nsContentSecurityManager_h___

#include "mozilla/CORSMode.h"
#include "nsIContentSecurityManager.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "nsILoadInfo.h"

class nsILoadInfo;
class nsIStreamListener;

#define NS_CONTENTSECURITYMANAGER_CONTRACTID \
  "@mozilla.org/contentsecuritymanager;1"
// cdcc1ab8-3cea-4e6c-a294-a651fa35227f
#define NS_CONTENTSECURITYMANAGER_CID                \
  {                                                  \
    0xcdcc1ab8, 0x3cea, 0x4e6c, {                    \
      0xa2, 0x94, 0xa6, 0x51, 0xfa, 0x35, 0x22, 0x7f \
    }                                                \
  }

class nsContentSecurityManager : public nsIContentSecurityManager,
                                 public nsIChannelEventSink {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTSECURITYMANAGER
  NS_DECL_NSICHANNELEVENTSINK

  nsContentSecurityManager() = default;

  static nsresult doContentSecurityCheck(
      nsIChannel* aChannel, nsCOMPtr<nsIStreamListener>& aInAndOutListener);

  static bool AllowTopLevelNavigationToDataURI(nsIChannel* aChannel);
  static void ReportBlockedDataURI(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                   bool aIsRedirect = false);
  static bool AllowInsecureRedirectToDataURI(nsIChannel* aNewChannel);
  static void MeasureUnexpectedPrivilegedLoads(nsILoadInfo* aLoadInfo,
                                               nsIURI* aFinalURI,
                                               const nsACString& aRemoteType);

  enum CORSSecurityMapping {
    // Disables all CORS checking overriding the value of aCORSMode. All checks
    // are disabled even when CORSMode::CORS_ANONYMOUS or
    // CORSMode::CORS_USE_CREDENTIALS is passed. This is mostly used for chrome
    // code, where we don't need security checks. See
    // SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL for the detailed explanation
    // of the security mode.
    DISABLE_CORS_CHECKS,
    // Disables all CORS checking on CORSMode::CORS_NONE. The other two CORS
    // modes CORSMode::CORS_ANONYMOUS and CORSMode::CORS_USE_CREDENTIALS are
    // respected.
    CORS_NONE_MAPS_TO_DISABLED_CORS_CHECKS,
    // Allow load from any origin, but cross-origin requests require CORS. See
    // SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT. Like above the other two
    // CORS modes are unaffected and get parsed.
    CORS_NONE_MAPS_TO_INHERITED_CONTEXT,
    // Always require the server to acknowledge the request via CORS.
    // CORSMode::CORS_NONE is parsed as if CORSMode::CORS_ANONYMOUS is passed.
    REQUIRE_CORS_CHECKS,
  };

  // computes the security flags for the requested CORS mode
  // @param aCORSSecurityMapping: See CORSSecurityMapping for variant
  // descriptions
  static nsSecurityFlags ComputeSecurityFlags(
      mozilla::CORSMode aCORSMode, CORSSecurityMapping aCORSSecurityMapping);

  static void GetSerializedOrigin(nsIPrincipal* aOrigin,
                                  nsIPrincipal* aResourceOrigin,
                                  nsACString& aResult, nsILoadInfo* aLoadInfo);

  // https://html.spec.whatwg.org/multipage/browsers.html#compatible-with-cross-origin-isolation
  static bool IsCompatibleWithCrossOriginIsolation(
      nsILoadInfo::CrossOriginEmbedderPolicy aPolicy);

 private:
  static nsresult CheckChannel(nsIChannel* aChannel);
  static nsresult CheckAllowLoadInSystemPrivilegedContext(nsIChannel* aChannel);
  static nsresult CheckAllowLoadInPrivilegedAboutContext(nsIChannel* aChannel);
  static nsresult CheckChannelHasProtocolSecurityFlag(nsIChannel* aChannel);
  static bool CrossOriginEmbedderPolicyAllowsCredentials(nsIChannel* aChannel);

  virtual ~nsContentSecurityManager() = default;
};

#endif /* nsContentSecurityManager_h___ */
