/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoplayPolicy.h"

#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "mozilla/dom/NavigatorBinding.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/Logging.h"
#include "mozilla/MediaManager.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIAutoplay.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"

mozilla::LazyLogModule gAutoplayPermissionLog("Autoplay");

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

using namespace mozilla::dom;

namespace mozilla::media {

static const uint32_t sPOLICY_STICKY_ACTIVATION = 0;
// static const uint32_t sPOLICY_TRANSIENT_ACTIVATION = 1;
static const uint32_t sPOLICY_USER_INPUT_DEPTH = 2;

static bool IsActivelyCapturingOrHasAPermission(nsPIDOMWindowInner* aWindow) {
  // Pages which have been granted permission to capture WebRTC camera or
  // microphone or screen are assumed to be trusted, and are allowed to
  // autoplay.
  if (MediaManager::GetIfExists()) {
    return MediaManager::GetIfExists()->IsActivelyCapturingOrHasAPermission(
        aWindow->WindowID());
  }

  auto principal = nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  return (nsContentUtils::IsExactSitePermAllow(principal, "camera"_ns) ||
          nsContentUtils::IsExactSitePermAllow(principal, "microphone"_ns) ||
          nsContentUtils::IsExactSitePermAllow(principal, "screen"_ns));
}

static uint32_t SiteAutoplayPerm(nsPIDOMWindowInner* aWindow) {
  if (!aWindow || !aWindow->GetBrowsingContext()) {
    return nsIPermissionManager::UNKNOWN_ACTION;
  }

  WindowContext* topContext =
      aWindow->GetBrowsingContext()->GetTopWindowContext();
  if (!topContext) {
    return nsIPermissionManager::UNKNOWN_ACTION;
  }
  return topContext->GetAutoplayPermission();
}

static bool IsWindowAllowedToPlayByUserGesture(nsPIDOMWindowInner* aWindow) {
  if (!aWindow) {
    return false;
  }

  WindowContext* topContext =
      aWindow->GetBrowsingContext()->GetTopWindowContext();
  if (topContext && topContext->HasBeenUserGestureActivated()) {
    AUTOPLAY_LOG(
        "Allow autoplay as top-level context has been activated by user "
        "gesture.");
    return true;
  }
  return false;
}

static bool IsWindowAllowedToPlayByTraits(nsPIDOMWindowInner* aWindow) {
  if (!aWindow) {
    return false;
  }

  if (IsActivelyCapturingOrHasAPermission(aWindow)) {
    AUTOPLAY_LOG(
        "Allow autoplay as document has camera or microphone or screen"
        " permission.");
    return true;
  }

  Document* currentDoc = aWindow->GetExtantDoc();
  if (!currentDoc) {
    return false;
  }

  bool isTopLevelContent = !aWindow->GetBrowsingContext()->GetParent();
  if (currentDoc->MediaDocumentKind() == Document::MediaDocumentKind::Video &&
      isTopLevelContent) {
    AUTOPLAY_LOG("Allow top-level video document to autoplay.");
    return true;
  }

  if (StaticPrefs::media_autoplay_allow_extension_background_pages() &&
      currentDoc->IsExtensionPage()) {
    AUTOPLAY_LOG("Allow autoplay as in extension document.");
    return true;
  }

  return false;
}

static bool IsWindowAllowedToPlayOverall(nsPIDOMWindowInner* aWindow) {
  return IsWindowAllowedToPlayByUserGesture(aWindow) ||
         IsWindowAllowedToPlayByTraits(aWindow);
}

static uint32_t DefaultAutoplayBehaviour() {
  int32_t prefValue = StaticPrefs::media_autoplay_default();
  if (prefValue == nsIAutoplay::ALLOWED) {
    return nsIAutoplay::ALLOWED;
  }
  if (prefValue == nsIAutoplay::BLOCKED_ALL) {
    return nsIAutoplay::BLOCKED_ALL;
  }
  return nsIAutoplay::BLOCKED;
}

static bool IsMediaElementInaudible(const HTMLMediaElement& aElement) {
  if (aElement.Volume() == 0.0 || aElement.Muted()) {
    AUTOPLAY_LOG("Media %p is muted.", &aElement);
    return true;
  }

  if (!aElement.HasAudio() &&
      aElement.ReadyState() >= HTMLMediaElement_Binding::HAVE_METADATA) {
    AUTOPLAY_LOG("Media %p has no audio track", &aElement);
    return true;
  }

  return false;
}

static bool IsAudioContextAllowedToPlay(const AudioContext& aContext) {
  // Offline context won't directly output sound to audio devices.
  return aContext.IsOffline() ||
         IsWindowAllowedToPlayOverall(aContext.GetParentObject());
}

static bool IsEnableBlockingWebAudioByUserGesturePolicy() {
  return StaticPrefs::media_autoplay_blocking_policy() ==
         sPOLICY_STICKY_ACTIVATION;
}

static bool IsAllowedToPlayByBlockingModel(const HTMLMediaElement& aElement) {
  const uint32_t policy = StaticPrefs::media_autoplay_blocking_policy();
  if (policy == sPOLICY_STICKY_ACTIVATION) {
    const bool isAllowed =
        IsWindowAllowedToPlayOverall(aElement.OwnerDoc()->GetInnerWindow());
    AUTOPLAY_LOG("Use 'sticky-activation', isAllowed=%d", isAllowed);
    return isAllowed;
  }
  // If element is blessed, it would always be allowed to play().
  const bool isElementBlessed = aElement.IsBlessed();
  if (policy == sPOLICY_USER_INPUT_DEPTH) {
    const bool isUserInput = UserActivation::IsHandlingUserInput();
    AUTOPLAY_LOG("Use 'User-Input-Depth', isBlessed=%d, isUserInput=%d",
                 isElementBlessed, isUserInput);
    return isElementBlessed || isUserInput;
  }
  const bool hasTransientActivation =
      aElement.OwnerDoc()->HasValidTransientUserGestureActivation();
  AUTOPLAY_LOG(
      "Use 'transient-activation', isBlessed=%d, "
      "hasValidTransientActivation=%d",
      isElementBlessed, hasTransientActivation);
  return isElementBlessed || hasTransientActivation;
}

// On GeckoView, we don't store any site's permission in permission manager, we
// would check the GV request status to know if the site can be allowed to play.
// But on other platforms, we would store the site's permission in permission
// manager.
#if defined(MOZ_WIDGET_ANDROID)
using RType = GVAutoplayRequestType;

static bool IsGVAutoplayRequestAllowed(nsPIDOMWindowInner* aWindow,
                                       RType aType) {
  if (!aWindow) {
    return false;
  }

  RefPtr<BrowsingContext> context = aWindow->GetBrowsingContext()->Top();
  GVAutoplayRequestStatus status =
      aType == RType::eAUDIBLE ? context->GetGVAudibleAutoplayRequestStatus()
                               : context->GetGVInaudibleAutoplayRequestStatus();
  return status == GVAutoplayRequestStatus::eALLOWED;
}

static bool IsGVAutoplayRequestAllowed(const HTMLMediaElement& aElement,
                                       RType aType) {
  // On GV, blocking model is the first thing we would check inside Gecko, and
  // if the media is not allowed by that, then we would check the response from
  // the embedding app to decide the final result.
  if (IsAllowedToPlayByBlockingModel(aElement)) {
    return true;
  }

  RefPtr<nsPIDOMWindowInner> window = aElement.OwnerDoc()->GetInnerWindow();
  if (!window) {
    return false;
  }
  return IsGVAutoplayRequestAllowed(window, aType);
}
#endif

static bool IsAllowedToPlayInternal(const HTMLMediaElement& aElement) {
#if defined(MOZ_WIDGET_ANDROID)
  if (StaticPrefs::media_geckoview_autoplay_request()) {
    return IsGVAutoplayRequestAllowed(
        aElement, IsMediaElementInaudible(aElement) ? RType::eINAUDIBLE
                                                    : RType::eAUDIBLE);
  }
#endif
  bool isInaudible = IsMediaElementInaudible(aElement);
  bool isUsingAutoplayModel = IsAllowedToPlayByBlockingModel(aElement);

  uint32_t defaultBehaviour = DefaultAutoplayBehaviour();
  uint32_t sitePermission =
      SiteAutoplayPerm(aElement.OwnerDoc()->GetInnerWindow());

  AUTOPLAY_LOG(
      "IsAllowedToPlayInternal, isInaudible=%d,"
      "isUsingAutoplayModel=%d, sitePermission=%d, defaultBehaviour=%d",
      isInaudible, isUsingAutoplayModel, sitePermission, defaultBehaviour);

  // For site permissions we store permissionManager values except
  // for BLOCKED_ALL, for the default pref values we store
  // nsIAutoplay values.
  if (sitePermission == nsIPermissionManager::ALLOW_ACTION) {
    return true;
  }

  if (sitePermission == nsIPermissionManager::DENY_ACTION) {
    return isInaudible || isUsingAutoplayModel;
  }

  if (sitePermission == nsIAutoplay::BLOCKED_ALL) {
    return isUsingAutoplayModel;
  }

  if (defaultBehaviour == nsIAutoplay::ALLOWED) {
    return true;
  }

  if (defaultBehaviour == nsIAutoplay::BLOCKED) {
    return isInaudible || isUsingAutoplayModel;
  }

  MOZ_ASSERT(defaultBehaviour == nsIAutoplay::BLOCKED_ALL);
  return isUsingAutoplayModel;
}

/* static */
bool AutoplayPolicy::IsAllowedToPlay(const HTMLMediaElement& aElement) {
  const bool result = IsAllowedToPlayInternal(aElement);
  AUTOPLAY_LOG("IsAllowedToPlay, mediaElement=%p, isAllowToPlay=%s", &aElement,
               result ? "allowed" : "blocked");
  return result;
}

/* static */
bool AutoplayPolicy::IsAllowedToPlay(const AudioContext& aContext) {
  /**
   * The autoplay checking has 5 different phases,
   * 1. check whether audio context itself meets the autoplay condition
   * 2. check if we enable blocking web audio or not
   *    (only support blocking when using user-gesture-activation model)
   * 3. check whether the site is in the autoplay whitelist
   * 4. check global autoplay setting and check wether the site is in the
   *    autoplay blacklist.
   * 5. check whether media is allowed under current blocking model
   *    (only support user-gesture-activation model)
   */
  if (aContext.IsOffline()) {
    return true;
  }

  if (!IsEnableBlockingWebAudioByUserGesturePolicy()) {
    return true;
  }

  nsPIDOMWindowInner* window = aContext.GetParentObject();
  uint32_t sitePermission = SiteAutoplayPerm(window);

  if (sitePermission == nsIPermissionManager::ALLOW_ACTION) {
    AUTOPLAY_LOG(
        "Allow autoplay as document has permanent autoplay permission.");
    return true;
  }

  if (DefaultAutoplayBehaviour() == nsIAutoplay::ALLOWED &&
      sitePermission != nsIPermissionManager::DENY_ACTION &&
      sitePermission != nsIAutoplay::BLOCKED_ALL) {
    AUTOPLAY_LOG(
        "Allow autoplay as global autoplay setting is allowing autoplay by "
        "default.");
    return true;
  }

  return IsWindowAllowedToPlayOverall(window);
}

enum class DocumentAutoplayPolicy : uint8_t {
  Allowed,
  Allowed_muted,
  Disallowed
};

/* static */
DocumentAutoplayPolicy IsDocAllowedToPlay(const Document& aDocument) {
  RefPtr<nsPIDOMWindowInner> window = aDocument.GetInnerWindow();

#if defined(MOZ_WIDGET_ANDROID)
  if (StaticPrefs::media_geckoview_autoplay_request()) {
    const bool isWindowAllowedToPlay = IsWindowAllowedToPlayOverall(window);
    if (IsGVAutoplayRequestAllowed(window, RType::eAUDIBLE)) {
      return DocumentAutoplayPolicy::Allowed;
    }

    if (IsGVAutoplayRequestAllowed(window, RType::eINAUDIBLE)) {
      return isWindowAllowedToPlay ? DocumentAutoplayPolicy::Allowed
                                   : DocumentAutoplayPolicy::Allowed_muted;
    }

    return isWindowAllowedToPlay ? DocumentAutoplayPolicy::Allowed
                                 : DocumentAutoplayPolicy::Disallowed;
  }
#endif
  const uint32_t sitePermission = SiteAutoplayPerm(window);
  const uint32_t globalPermission = DefaultAutoplayBehaviour();
  const uint32_t policy = StaticPrefs::media_autoplay_blocking_policy();
  const bool isWindowAllowedToPlayByGesture =
      policy != sPOLICY_USER_INPUT_DEPTH &&
      IsWindowAllowedToPlayByUserGesture(window);
  const bool isWindowAllowedToPlayByTraits =
      IsWindowAllowedToPlayByTraits(window);

  AUTOPLAY_LOG(
      "IsDocAllowedToPlay(), policy=%d, sitePermission=%d, "
      "globalPermission=%d, isWindowAllowedToPlayByGesture=%d, "
      "isWindowAllowedToPlayByTraits=%d",
      policy, sitePermission, globalPermission, isWindowAllowedToPlayByGesture,
      isWindowAllowedToPlayByTraits);

  if ((globalPermission == nsIAutoplay::ALLOWED &&
       (sitePermission != nsIPermissionManager::DENY_ACTION &&
        sitePermission != nsIAutoplay::BLOCKED_ALL)) ||
      sitePermission == nsIPermissionManager::ALLOW_ACTION ||
      isWindowAllowedToPlayByGesture || isWindowAllowedToPlayByTraits) {
    return DocumentAutoplayPolicy::Allowed;
  }

  if ((globalPermission == nsIAutoplay::BLOCKED &&
       sitePermission != nsIAutoplay::BLOCKED_ALL) ||
      sitePermission == nsIPermissionManager::DENY_ACTION) {
    return DocumentAutoplayPolicy::Allowed_muted;
  }

  return DocumentAutoplayPolicy::Disallowed;
}

/* static */
uint32_t AutoplayPolicy::GetSiteAutoplayPermission(nsIPrincipal* aPrincipal) {
  if (!aPrincipal) {
    return nsIPermissionManager::DENY_ACTION;
  }

  nsCOMPtr<nsIPermissionManager> permMgr =
      components::PermissionManager::Service();
  if (!permMgr) {
    return nsIPermissionManager::DENY_ACTION;
  }

  uint32_t perm = nsIPermissionManager::DENY_ACTION;
  permMgr->TestExactPermissionFromPrincipal(aPrincipal, "autoplay-media"_ns,
                                            &perm);
  return perm;
}

/* static */
bool AutoplayPolicyTelemetryUtils::WouldBeAllowedToPlayIfAutoplayDisabled(
    const AudioContext& aContext) {
  return IsAudioContextAllowedToPlay(aContext);
}

/* static */
dom::AutoplayPolicy AutoplayPolicy::GetAutoplayPolicy(
    const dom::HTMLMediaElement& aElement) {
  // Note, the site permission can contain following values :
  // - UNKNOWN_ACTION : no permission set for this site
  // - ALLOW_ACTION : allowed to autoplay
  // - DENY_ACTION : allowed inaudible autoplay, disallowed inaudible autoplay
  // - nsIAutoplay::BLOCKED_ALL : autoplay disallowed
  // and the global permissions would be nsIAutoplay::{BLOCKED, ALLOWED,
  // BLOCKED_ALL}
  const uint32_t sitePermission =
      SiteAutoplayPerm(aElement.OwnerDoc()->GetInnerWindow());
  const uint32_t globalPermission = DefaultAutoplayBehaviour();
  const bool isAllowedToPlayByBlockingModel =
      IsAllowedToPlayByBlockingModel(aElement);

  AUTOPLAY_LOG(
      "IsAllowedToPlay(element), sitePermission=%d, globalPermission=%d, "
      "isAllowedToPlayByBlockingModel=%d",
      sitePermission, globalPermission, isAllowedToPlayByBlockingModel);

#if defined(MOZ_WIDGET_ANDROID)
  if (StaticPrefs::media_geckoview_autoplay_request()) {
    if (IsGVAutoplayRequestAllowed(aElement, RType::eAUDIBLE)) {
      return dom::AutoplayPolicy::Allowed;
    } else if (IsGVAutoplayRequestAllowed(aElement, RType::eINAUDIBLE)) {
      return isAllowedToPlayByBlockingModel
                 ? dom::AutoplayPolicy::Allowed
                 : dom::AutoplayPolicy::Allowed_muted;
    } else {
      return isAllowedToPlayByBlockingModel ? dom::AutoplayPolicy::Allowed
                                            : dom::AutoplayPolicy::Disallowed;
    }
  }
#endif

  // These are situations when an element is allowed to autoplay
  // 1. The site permission is explicitly allowed
  // 2. The global permission is allowed, and the site isn't explicitly
  //    disallowed
  // 3. The blocking model is explicitly allowed this element
  if (sitePermission == nsIPermissionManager::ALLOW_ACTION ||
      (globalPermission == nsIAutoplay::ALLOWED &&
       (sitePermission != nsIPermissionManager::DENY_ACTION &&
        sitePermission != nsIAutoplay::BLOCKED_ALL)) ||
      isAllowedToPlayByBlockingModel) {
    return dom::AutoplayPolicy::Allowed;
  }

  // These are situations when a element is allowed to autoplay only when it's
  // inaudible.
  // 1. The site permission is block-audible-autoplay
  // 2. The global permission is block-audible-autoplay, and the site permission
  //    isn't block-all-autoplay
  if (sitePermission == nsIPermissionManager::DENY_ACTION ||
      (globalPermission == nsIAutoplay::BLOCKED &&
       sitePermission != nsIAutoplay::BLOCKED_ALL)) {
    return dom::AutoplayPolicy::Allowed_muted;
  }

  return dom::AutoplayPolicy::Disallowed;
}

/* static */
dom::AutoplayPolicy AutoplayPolicy::GetAutoplayPolicy(
    const dom::AudioContext& aContext) {
  if (AutoplayPolicy::IsAllowedToPlay(aContext)) {
    return dom::AutoplayPolicy::Allowed;
  }
  return dom::AutoplayPolicy::Disallowed;
}

/* static */
dom::AutoplayPolicy AutoplayPolicy::GetAutoplayPolicy(
    const dom::AutoplayPolicyMediaType& aType, const dom::Document& aDoc) {
  DocumentAutoplayPolicy policy = IsDocAllowedToPlay(aDoc);
  // https://w3c.github.io/autoplay/#query-by-a-media-type
  if (aType == dom::AutoplayPolicyMediaType::Audiocontext) {
    return policy == DocumentAutoplayPolicy::Allowed
               ? dom::AutoplayPolicy::Allowed
               : dom::AutoplayPolicy::Disallowed;
  }
  MOZ_ASSERT(aType == dom::AutoplayPolicyMediaType::Mediaelement);
  if (policy == DocumentAutoplayPolicy::Allowed) {
    return dom::AutoplayPolicy::Allowed;
  }
  if (policy == DocumentAutoplayPolicy::Allowed_muted) {
    return dom::AutoplayPolicy::Allowed_muted;
  }
  return dom::AutoplayPolicy::Disallowed;
}

}  // namespace mozilla::media
