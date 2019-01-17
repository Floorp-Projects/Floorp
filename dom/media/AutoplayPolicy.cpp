/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoplayPolicy.h"

#include "mozilla/EventStateManager.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "nsGlobalWindowInner.h"
#include "nsIAutoplay.h"
#include "nsContentUtils.h"
#include "mozilla/dom/Document.h"
#include "MediaManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsPIDOMWindow.h"

mozilla::LazyLogModule gAutoplayPermissionLog("Autoplay");

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

static Document* ApproverDocOf(const Document& aDocument) {
  nsCOMPtr<nsIDocShell> ds = aDocument.GetDocShell();
  if (!ds) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
  ds->GetSameTypeRootTreeItem(getter_AddRefs(rootTreeItem));
  if (!rootTreeItem) {
    return nullptr;
  }

  return rootTreeItem->GetDocument();
}

static bool IsActivelyCapturingOrHasAPermission(nsPIDOMWindowInner* aWindow) {
  // Pages which have been granted permission to capture WebRTC camera or
  // microphone or screen are assumed to be trusted, and are allowed to
  // autoplay.
  if (MediaManager::GetIfExists()) {
    return MediaManager::GetIfExists()->IsActivelyCapturingOrHasAPermission(
        aWindow->WindowID());
  }

  auto principal = nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  return (nsContentUtils::IsExactSitePermAllow(principal, "camera") ||
          nsContentUtils::IsExactSitePermAllow(principal, "microphone") ||
          nsContentUtils::IsExactSitePermAllow(principal, "screen"));
}

static bool IsSiteInAutoplayWhiteList(const Document* aDocument) {
  return aDocument ? nsContentUtils::IsExactSitePermAllow(
                         aDocument->NodePrincipal(), "autoplay-media")
                   : false;
}

static bool IsSiteInAutoplayBlackList(const Document* aDocument) {
  return aDocument ? nsContentUtils::IsExactSitePermDeny(
                         aDocument->NodePrincipal(), "autoplay-media")
                   : false;
}

static bool IsWindowAllowedToPlay(nsPIDOMWindowInner* aWindow) {
  if (!aWindow) {
    return false;
  }

  if (IsActivelyCapturingOrHasAPermission(aWindow)) {
    AUTOPLAY_LOG(
        "Allow autoplay as document has camera or microphone or screen"
        " permission.");
    return true;
  }

  if (!aWindow->GetExtantDoc()) {
    return false;
  }

  // Here we are checking whether the current document is blocked via
  // feature-policy, and further down we walk up the doc tree to the top level
  // content document and check permissions etc on the top level content
  // document. FeaturePolicy propagates the permission to any sub-documents if
  // they don't have special directives.
  if (!FeaturePolicyUtils::IsFeatureAllowed(aWindow->GetExtantDoc(),
                                            NS_LITERAL_STRING("autoplay"))) {
    return false;
  }

  Document* approver = ApproverDocOf(*aWindow->GetExtantDoc());
  if (!approver) {
    return false;
  }

  if (approver->HasBeenUserGestureActivated()) {
    AUTOPLAY_LOG("Allow autoplay as document activated by user gesture.");
    return true;
  }

  if (approver->IsExtensionPage()) {
    AUTOPLAY_LOG("Allow autoplay as in extension document.");
    return true;
  }

  if (approver->MediaDocumentKind() == Document::MediaDocumentKind::Video) {
    AUTOPLAY_LOG("Allow video document to autoplay.");
    return true;
  }

  return false;
}

static uint32_t DefaultAutoplayBehaviour() {
  int prefValue =
      Preferences::GetInt("media.autoplay.default", nsIAutoplay::ALLOWED);
  if (prefValue < nsIAutoplay::ALLOWED || prefValue > nsIAutoplay::BLOCKED) {
    // Invalid pref values are just converted to BLOCKED.
    return nsIAutoplay::BLOCKED;
  }
  return prefValue;
}

static bool IsMediaElementAllowedToPlay(const HTMLMediaElement& aElement) {
  const bool isAllowedMuted =
      Preferences::GetBool("media.autoplay.allow-muted", true);
  if ((aElement.Volume() == 0.0 || aElement.Muted()) && isAllowedMuted) {
    AUTOPLAY_LOG("Allow muted media %p to autoplay.", &aElement);
    return true;
  }

  if (!aElement.HasAudio() &&
      aElement.ReadyState() >= HTMLMediaElement_Binding::HAVE_METADATA &&
      isAllowedMuted) {
    AUTOPLAY_LOG("Allow media %p without audio track to autoplay", &aElement);
    return true;
  }

  return false;
}

static bool IsAudioContextAllowedToPlay(const AudioContext& aContext) {
  // Offline context won't directly output sound to audio devices.
  return aContext.IsOffline() ||
         IsWindowAllowedToPlay(aContext.GetParentObject());
}

static bool IsEnableBlockingWebAudioByUserGesturePolicy() {
  return DefaultAutoplayBehaviour() != nsIAutoplay::ALLOWED &&
         Preferences::GetBool("media.autoplay.block-webaudio", false) &&
         Preferences::GetBool("media.autoplay.enabled.user-gestures-needed",
                              false);
}

/* static */ bool AutoplayPolicy::WouldBeAllowedToPlayIfAutoplayDisabled(
    const HTMLMediaElement& aElement) {
  return IsMediaElementAllowedToPlay(aElement) ||
         IsWindowAllowedToPlay(aElement.OwnerDoc()->GetInnerWindow());
}

/* static */ bool AutoplayPolicy::WouldBeAllowedToPlayIfAutoplayDisabled(
    const AudioContext& aContext) {
  return IsAudioContextAllowedToPlay(aContext);
}

static bool IsAllowedToPlayInternal(const HTMLMediaElement& aElement) {
  /**
   * The autoplay checking has 4 different phases,
   * 1. check whether media element itself meets the autoplay condition
   * 2. check whethr the site is in the autoplay whitelist
   * 3. check global autoplay setting and check wether the site is in the
   *    autoplay blacklist.
   * 4. check whether media is allowed under current blocking model
   *    (click-to-play or user-gesture-activation)
   */
  if (IsMediaElementAllowedToPlay(aElement)) {
    return true;
  }

  Document* approver = ApproverDocOf(*aElement.OwnerDoc());
  if (IsSiteInAutoplayWhiteList(approver)) {
    AUTOPLAY_LOG(
        "Allow autoplay as document has permanent autoplay permission.");
    return true;
  }

  if (DefaultAutoplayBehaviour() == nsIAutoplay::ALLOWED &&
      !(IsSiteInAutoplayBlackList(approver) &&
        StaticPrefs::MediaAutoplayBlackListOverrideDefault())) {
    AUTOPLAY_LOG(
        "Allow autoplay as global autoplay setting is allowing autoplay by "
        "default.");
    return true;
  }

  if (!Preferences::GetBool("media.autoplay.enabled.user-gestures-needed",
                            false)) {
    // If element is blessed, it would always be allowed to play().
    return aElement.IsBlessed() || EventStateManager::IsHandlingUserInput();
  }
  return IsWindowAllowedToPlay(aElement.OwnerDoc()->GetInnerWindow());
}

/* static */ bool AutoplayPolicy::IsAllowedToPlay(
    const HTMLMediaElement& aElement) {
  const bool result = IsAllowedToPlayInternal(aElement);
  AUTOPLAY_LOG("IsAllowedToPlay, mediaElement=%p, isAllowToPlay=%s", &aElement,
               result ? "allowed" : "blocked");
  return result;
}

/* static */ bool AutoplayPolicy::IsAllowedToPlay(
    const AudioContext& aContext) {
  /**
   * The autoplay checking has 4 different phases,
   * 1. check whether audio context itself meets the autoplay condition
   * 2. check whethr the site is in the autoplay whitelist
   * 3. check global autoplay setting and check wether the site is in the
   *    autoplay blacklist.
   * 4. check whether media is allowed under current blocking model
   *    (only support user-gesture-activation)
   */
  if (aContext.IsOffline()) {
    return true;
  }

  nsPIDOMWindowInner* window = aContext.GetParentObject();
  Document* approver = aContext.GetParentObject() ?
      ApproverDocOf(*(window->GetExtantDoc())) : nullptr;
  if (IsSiteInAutoplayWhiteList(approver)) {
    AUTOPLAY_LOG(
        "Allow autoplay as document has permanent autoplay permission.");
    return true;
  }

  if (DefaultAutoplayBehaviour() == nsIAutoplay::ALLOWED &&
      !IsSiteInAutoplayBlackList(approver)) {
    AUTOPLAY_LOG(
        "Allow autoplay as global autoplay setting is allowing autoplay by "
        "default.");
    return true;
  }

  if (!IsEnableBlockingWebAudioByUserGesturePolicy()) {
    return true;
  }
  return IsWindowAllowedToPlay(window);
}

/* static */ DocumentAutoplayPolicy AutoplayPolicy::IsAllowedToPlay(
    const Document& aDocument) {
  if (DefaultAutoplayBehaviour() == nsIAutoplay::ALLOWED ||
      IsWindowAllowedToPlay(aDocument.GetInnerWindow())) {
    return DocumentAutoplayPolicy::Allowed;
  }

  if (StaticPrefs::MediaAutoplayAllowMuted()) {
    return DocumentAutoplayPolicy::Allowed_muted;
  }

  return DocumentAutoplayPolicy::Disallowed;
}

}  // namespace dom
}  // namespace mozilla
