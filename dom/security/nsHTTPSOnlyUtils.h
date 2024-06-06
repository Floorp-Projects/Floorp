/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTTPSOnlyUtils_h___
#define nsHTTPSOnlyUtils_h___

#include "nsIScriptError.h"
#include "nsISupports.h"
#include "mozilla/net/DocumentLoadListener.h"

class nsHTTPSOnlyUtils {
 public:
  /**
   * Returns if HTTPS-Only Mode preference is enabled
   * @param aFromPrivateWindow true if executing in private browsing mode
   * @return true if HTTPS-Only Mode is enabled
   */
  static bool IsHttpsOnlyModeEnabled(bool aFromPrivateWindow);

  /**
   * Returns if HTTPS-First Mode preference is enabled
   * @param aFromPrivateWindow true if executing in private browsing mode
   * @return true if HTTPS-First Mode is enabled
   */
  static bool IsHttpsFirstModeEnabled(bool aFromPrivateWindow);

  /**
   * Potentially fires an http request for a top-level load (provided by
   * aDocumentLoadListener) in the background to avoid long timeouts in case
   * the upgraded https top-level load most likely will result in timeout.
   * @param aDocumentLoadListener The Document listener associated with
   *                              the original top-level channel.
   */
  static void PotentiallyFireHttpRequestToShortenTimout(
      mozilla::net::DocumentLoadListener* aDocumentLoadListener);

  /**
   * Determines if a request should get upgraded because of the HTTPS-Only mode.
   * If true, the httpsOnlyStatus flag in LoadInfo gets updated and a message is
   * logged in the console.
   * @param  aURI      nsIURI of request
   * @param  aLoadInfo nsILoadInfo of request
   * @return           true if request should get upgraded
   */
  static bool ShouldUpgradeRequest(nsIURI* aURI, nsILoadInfo* aLoadInfo);

  /**
   * Determines if a request should get upgraded because of the HTTPS-Only mode.
   * If true, a message is logged in the console.
   * @param  aURI      nsIURI of request
   * @param  aLoadInfo nsILoadInfo of request
   * @return           true if request should get upgraded
   */
  static bool ShouldUpgradeWebSocket(nsIURI* aURI, nsILoadInfo* aLoadInfo);

  /**
   * Determines if we might get stuck in an upgrade-downgrade-endless loop
   * where https-only upgrades the request to https and the website downgrades
   * the scheme to http again causing an endless upgrade downgrade loop. E.g.
   * https-only upgrades to https and the website answers with a meta-refresh
   * to downgrade to same-origin http version. Similarly this method breaks
   * the endless cycle for JS based redirects and 302 based redirects.
   * Note this function is also used when we got an HTTPS RR for the website.
   * @param  aURI      nsIURI of request
   * @param  aLoadInfo nsILoadInfo of request
   * @param  aOptions an options object indicating if the function
   *                  should be consulted for https-only or https-first mode or
   *                  the case that an HTTPS RR is presented.
   * @return           true if an endless loop is detected
   */
  enum class UpgradeDowngradeEndlessLoopOptions {
    EnforceForHTTPSOnlyMode,
    EnforceForHTTPSFirstMode,
    EnforceForHTTPSRR,
  };
  static bool IsUpgradeDowngradeEndlessLoop(
      nsIURI* aOldURI, nsIURI* aNewURI, nsILoadInfo* aLoadInfo,
      const mozilla::EnumSet<UpgradeDowngradeEndlessLoopOptions>& aOptions =
          {});

  /**
   * Determines if a request should get upgraded because of the HTTPS-First
   * mode. If true, the httpsOnlyStatus in LoadInfo gets updated and a message
   * is logged in the console.
   * @param  aURI      nsIURI of request
   * @param  aLoadInfo nsILoadInfo of request
   * @return           true if request should get upgraded
   */
  static bool ShouldUpgradeHttpsFirstRequest(nsIURI* aURI,
                                             nsILoadInfo* aLoadInfo);

  /**
   * Determines if the request was previously upgraded with HTTPS-First, creates
   * a downgraded URI and logs to console.
   * @param  aStatus               Status code
   * @param  aDocumentLoadListener Failed document load listener
   * @return                       URI with http-scheme or nullptr
   */
  static already_AddRefed<nsIURI> PotentiallyDowngradeHttpsFirstRequest(
      mozilla::net::DocumentLoadListener* aDocumentLoadListener,
      nsresult aStatus);

  /**
   * Checks if the error code is on a block-list of codes that are probably
   * not related to a HTTPS-Only Mode upgrade.
   * @param  aChannel The failed Channel.
   * @param  aError Error Code from Request
   * @return        false if error is not related to upgrade
   */
  static bool CouldBeHttpsOnlyError(nsIChannel* aChannel, nsresult aError);

  /**
   * Logs localized message to either content console or browser console
   * @param aName            Localization key
   * @param aParams          Localization parameters
   * @param aFlags           Logging Flag (see nsIScriptError)
   * @param aLoadInfo        The loadinfo of the request.
   * @param [aURI]           Optional: URI to log
   * @param [aUseHttpsFirst] Optional: Log using HTTPS-First (vs. HTTPS-Only)
   */
  static void LogLocalizedString(const char* aName,
                                 const nsTArray<nsString>& aParams,
                                 uint32_t aFlags, nsILoadInfo* aLoadInfo,
                                 nsIURI* aURI = nullptr,
                                 bool aUseHttpsFirst = false);

  /**
   * Tests if the HTTPS-Only upgrade exception is set for a given principal.
   * @param  aPrincipal The principal for whom the exception should be checked
   * @return            True if exempt
   */
  static bool TestIfPrincipalIsExempt(nsIPrincipal* aPrincipal,
                                      bool aCheckForHTTPSFirst = false);

  /**
   * Tests if the HTTPS-Only Mode upgrade exception is set for channel result
   * principal and sets or removes the httpsOnlyStatus-flag on the loadinfo
   * accordingly.
   * Note: This function only adds an exemption for loads of TYPE_DOCUMENT.
   * @param  aChannel The channel to be checked
   */
  static void TestSitePermissionAndPotentiallyAddExemption(
      nsIChannel* aChannel);

  /**
   * Checks whether CORS or mixed content requests are safe because they'll get
   * upgraded to HTTPS
   * @param  aLoadInfo nsILoadInfo of request
   * @return           true if it's safe to accept
   */
  static bool IsSafeToAcceptCORSOrMixedContent(nsILoadInfo* aLoadInfo);

  /**
   * Checks if https only or https first mode is enabled for this load
   * @param aLoadInfo nsILoadInfo of the request
   */
  static bool ShouldUpgradeConnection(nsILoadInfo* aLoadInfo);

  /**
   * Checks if two URIs are same origin modulo the difference that
   * aToURI scheme is downgraded to http from https aFromURI.
   * @param aFromURI nsIURI using scheme of https
   * @param aToURI nsIURI using scheme of http
   * @return true, if URIs are equal except scheme and ref
   */
  static bool IsHttpDowngrade(nsIURI* aFromURI, nsIURI* aToURI);

  /**
   * Will add a special temporary HTTPS-Only exception that only applies to
   * HTTPS-First, and is not exposed in the UI.
   * @param aURI      The URL for whose HTTP principal the exception should be
   *                  added
   * @param aLoadInfo The loadinfo of the request triggering this exception to
   *                  be added (needs to match aURI)
   */
  static nsresult AddHTTPSFirstExceptionForSession(
      nsCOMPtr<nsIURI> aURI, nsILoadInfo* const aLoadInfo);

  /**
   * Determines which HTTPS-Only status flags should get propagated to
   * sub-resources or sub-documents. As sub-resources and sub-documents are
   * exempt when the top-level document is exempt, we need to copy the "exempt"
   * flag. The HTTPS-First "upgraded" flag should not be copied to prevent a
   * unwanted downgrade (Bug 1885949).
   * @param aHttpsOnlyStatus The HTTPS-Only status of the top-level document.
   * @return The HTTPS-Only status that the sub-resource/document should
   * receive.
   */
  static uint32_t GetStatusForSubresourceLoad(uint32_t aHttpsOnlyStatus);

  /**
   * When a downgrade is happening because of HTTPS-First, this function will
   * update the load state for the new load accordingly. This includes
   * information about the downgrade for later telemetry use.
   * @param aDocumentLoadListener The calling document load listener.
   * @param aLoadState The load state to be updated
   */
  static void UpdateLoadStateAfterHTTPSFirstDowngrade(
      mozilla::net::DocumentLoadListener* aDocumentLoadListener,
      nsDocShellLoadState* aLoadState);

  /**
   * When a load is successful, this should be called by the document load
   * listener. In two cases, telemetry is then recorded:
   * a) If downgrade data has been passed, the passed load is a successful
   *    downgrade, which means telemetry based on the downgrade data will be
   *    submitted.
   * b) If the passed load info indicates that this load has been upgraded by
   *    HTTPS-First, this means the upgrade was successful, which will be
   *    recorded to telemetry.
   */
  static void SubmitHTTPSFirstTelemetry(
      nsCOMPtr<nsILoadInfo> const& aLoadInfo,
      RefPtr<HTTPSFirstDowngradeData> const& aHttpsFirstDowngradeData);

 private:
  /**
   * Checks if it can be ruled out that the error has something
   * to do with an HTTPS upgrade.
   * @param  aError error code
   * @return        true if error is unrelated to the upgrade
   */
  static bool HttpsUpgradeUnrelatedErrorCode(nsresult aError);
  /**
   * Logs localized message to either content console or browser console
   * @param aMessage         Message to log
   * @param aFlags           Logging Flag (see nsIScriptError)
   * @param aLoadInfo        The loadinfo of the request.
   * @param [aURI]           Optional: URI to log
   * @param [aUseHttpsFirst] Optional: Log using HTTPS-First (vs. HTTPS-Only)
   */
  static void LogMessage(const nsAString& aMessage, uint32_t aFlags,
                         nsILoadInfo* aLoadInfo, nsIURI* aURI = nullptr,
                         bool aUseHttpsFirst = false);

  /**
   * Checks whether the URI ends with .onion
   * @param  aURI URI object
   * @return      true if the URI is an Onion URI
   */
  static bool OnionException(nsIURI* aURI);

  /**
   * Checks whether the URI is a loopback- or local-IP
   * @param  aURI URI object
   * @return      true if the URI is either loopback or local
   */
  static bool LoopbackOrLocalException(nsIURI* aURI);
};

/**
 * Helper class to perform an http request with a N milliseconds
 * delay. If that http request is 'receiving data' before the
 * upgraded https request, then it's a strong indicator that
 * the https request will result in a timeout and hence we
 * cancel the https request which will result in displaying
 * the exception page.
 */
class TestHTTPAnswerRunnable final : public mozilla::Runnable,
                                     public nsIStreamListener,
                                     public nsIInterfaceRequestor,
                                     public nsITimerCallback {
 public:
  // TestHTTPAnswerRunnable needs to implement all these channel related
  // interfaces because otherwise our Necko code is not happy, but we
  // really only care about ::OnStartRequest.
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSITIMERCALLBACK

  explicit TestHTTPAnswerRunnable(
      nsIURI* aURI, mozilla::net::DocumentLoadListener* aDocumentLoadListener);

 protected:
  ~TestHTTPAnswerRunnable() = default;

 private:
  /**
   * Checks whether the HTTP background request results in a redirect
   * to the same upgraded top-level HTTPS URL
   * @param  aChannel a nsIHttpChannel object
   * @return  true if the backgroundchannel is redirected
   */
  static bool IsBackgroundRequestRedirected(nsIHttpChannel* aChannel);

  RefPtr<nsIURI> mURI;
  // We're keeping a reference to DocumentLoadListener instead of a specific
  // channel, because the current top-level channel can change (for example
  // through redirects)
  RefPtr<mozilla::net::DocumentLoadListener> mDocumentLoadListener;
  RefPtr<nsITimer> mTimer;
};

/**
 * Data about a HTTPS-First downgrade used for Telemetry. We need to store this
 * instead of directly submitting it when deciding to downgrade, because it is
 * only interesting for us if the downgraded load is actually succesful.
 */
struct HTTPSFirstDowngradeData
    : public mozilla::RefCounted<HTTPSFirstDowngradeData> {
  MOZ_DECLARE_REFCOUNTED_TYPENAME(HTTPSFirstDowngradeData)
  mozilla::TimeDuration downgradeTime;
  bool isOnTimer = false;
  bool isSchemeless = false;
};

#endif /* nsHTTPSOnlyUtils_h___ */
