/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMixedContentBlocker_h___
#define nsMixedContentBlocker_h___

#define NS_MIXEDCONTENTBLOCKER_CONTRACTID "@mozilla.org/mixedcontentblocker;1"
/* daf1461b-bf29-4f88-8d0e-4bcdf332c862 */
#define NS_MIXEDCONTENTBLOCKER_CID                   \
  {                                                  \
    0xdaf1461b, 0xbf29, 0x4f88, {                    \
      0x8d, 0x0e, 0x4b, 0xcd, 0xf3, 0x32, 0xc8, 0x62 \
    }                                                \
  }

// This enum defines type of content that is detected when an
// nsMixedContentEvent fires
enum MixedContentTypes {
  // "Active" content, such as fonts, plugin content, JavaScript, stylesheets,
  // iframes, WebSockets, and XHR
  eMixedScript,
  // "Display" content, such as images, audio, video, and <a ping>
  eMixedDisplay
};

#include "nsIContentPolicy.h"
#include "nsIChannel.h"
#include "nsIChannelEventSink.h"
#include "imgRequest.h"

using mozilla::OriginAttributes;

class nsILoadInfo;  // forward declaration
namespace mozilla::net {
class nsProtocolProxyService;  // forward declaration
}  // namespace mozilla::net

class nsMixedContentBlocker : public nsIContentPolicy,
                              public nsIChannelEventSink {
 private:
  virtual ~nsMixedContentBlocker();

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY
  NS_DECL_NSICHANNELEVENTSINK

  nsMixedContentBlocker() = default;

  // See:
  // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy
  static bool IsPotentiallyTrustworthyLoopbackHost(
      const nsACString& aAsciiHost);
  static bool IsPotentiallyTrustworthyLoopbackURL(nsIURI* aURL);
  static bool IsPotentiallyTrustworthyOnion(nsIURI* aURL);
  static bool IsPotentiallyTrustworthyOrigin(nsIURI* aURI);

  /**
   * Returns true if the provided content policy type is subject to the
   * mixed content level 2 upgrading mechanism (audio, video, image).
   *
   * @param aConsiderPrefs A boolean that indicates whether the result of this
   * functions takes the `security.mixed_content.upgrade_display_content`
   * preferences into account.
   */
  static bool IsUpgradableContentType(nsContentPolicyType aType,
                                      bool aConsiderPrefs);

  /* Static version of ShouldLoad() that contains all the Mixed Content Blocker
   * logic.  Called from non-static ShouldLoad().
   * Called directly from imageLib when an insecure redirect exists in a cached
   * image load.
   * @param aHadInsecureImageRedirect
   *        boolean flag indicating that an insecure redirect through http
   *        occured when this image was initially loaded and cached.
   * @param aReportError
   *        boolean flag indicating if a rejection should automaticly be
   *        logged into the Console.
   * Remaining parameters are from nsIContentPolicy::ShouldLoad().
   */
  static nsresult ShouldLoad(bool aHadInsecureImageRedirect,
                             nsIURI* aContentLocation, nsILoadInfo* aLoadInfo,
                             bool aReportError, int16_t* aDecision);
  static void AccumulateMixedContentHSTS(
      nsIURI* aURI, bool aActive, const OriginAttributes& aOriginAttributes);

  static bool URISafeToBeLoadedInSecureContext(nsIURI* aURI);

  static void OnPrefChange(const char* aPref, void* aClosure);
  static void GetSecureContextAllowList(nsACString& aList);
  static void Shutdown();

  static bool sSecurecontextAllowlistCached;
  static nsCString* sSecurecontextAllowlist;
};

#endif /* nsMixedContentBlocker_h___ */
