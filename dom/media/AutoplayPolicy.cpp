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
#include "mozilla/AutoplayPermissionManager.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "nsGlobalWindowInner.h"
#include "nsIAutoplay.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "MediaManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsPIDOMWindow.h"

mozilla::LazyLogModule gAutoplayPermissionLog("Autoplay");

#define AUTOPLAY_LOG(msg, ...)                                             \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

static const char*
AllowAutoplayToStr(const uint32_t state)
{
  switch (state) {
    case nsIAutoplay::ALLOWED:
      return "allowed";
    case nsIAutoplay::BLOCKED:
      return "blocked";
    case nsIAutoplay::PROMPT:
      return "prompt";
    default:
      return "unknown";
  }
}

namespace mozilla {
namespace dom {

static nsIDocument*
ApproverDocOf(const nsIDocument& aDocument)
{
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

static bool
IsActivelyCapturingOrHasAPermission(nsPIDOMWindowInner* aWindow)
{
  // Pages which have been granted permission to capture WebRTC camera or
  // microphone or screen are assumed to be trusted, and are allowed to autoplay.
  if (MediaManager::GetIfExists()) {
    return MediaManager::GetIfExists()->IsActivelyCapturingOrHasAPermission(aWindow->WindowID());
  }

  auto principal = nsGlobalWindowInner::Cast(aWindow)->GetPrincipal();
  return (nsContentUtils::IsExactSitePermAllow(principal, "camera") ||
          nsContentUtils::IsExactSitePermAllow(principal, "microphone") ||
          nsContentUtils::IsExactSitePermAllow(principal, "screen"));
}

static bool
IsWindowAllowedToPlay(nsPIDOMWindowInner* aWindow)
{
  if (!aWindow) {
    return false;
  }

  if (IsActivelyCapturingOrHasAPermission(aWindow)) {
    AUTOPLAY_LOG("Allow autoplay as document has camera or microphone or screen"
                 " permission.");
    return true;
  }

  if (!aWindow->GetExtantDoc()) {
    return false;
  }

  nsIDocument* approver = ApproverDocOf(*aWindow->GetExtantDoc());
  if (nsContentUtils::IsExactSitePermAllow(approver->NodePrincipal(),
                                           "autoplay-media")) {
    AUTOPLAY_LOG("Allow autoplay as document has autoplay permission.");
    return true;
  }

  if (approver->HasBeenUserGestureActivated()) {
    AUTOPLAY_LOG("Allow autoplay as document activated by user gesture.");
    return true;
  }

  if (approver->IsExtensionPage()) {
    AUTOPLAY_LOG("Allow autoplay as in extension document.");
    return true;
  }

  return false;
}

/* static */
already_AddRefed<AutoplayPermissionManager>
AutoplayPolicy::RequestFor(const nsIDocument& aDocument)
{
  nsIDocument* document = ApproverDocOf(aDocument);
  if (!document) {
    return nullptr;
  }
  nsPIDOMWindowInner* window = document->GetInnerWindow();
  if (!window) {
    return nullptr;
  }
  return window->GetAutoplayPermissionManager();
}

static uint32_t
DefaultAutoplayBehaviour()
{
  int prefValue = Preferences::GetInt("media.autoplay.default", nsIAutoplay::ALLOWED);
  if (prefValue < nsIAutoplay::ALLOWED || prefValue > nsIAutoplay::PROMPT) {
    // Invalid pref values are just converted to ALLOWED.
    return nsIAutoplay::ALLOWED;
  }
  return prefValue;
}

static bool
IsMediaElementAllowedToPlay(const HTMLMediaElement& aElement)
{
  if ((aElement.Volume() == 0.0 || aElement.Muted()) &&
       Preferences::GetBool("media.autoplay.allow-muted", true)) {
    AUTOPLAY_LOG("Allow muted media %p to autoplay.", &aElement);
    return true;
  }

  if (IsWindowAllowedToPlay(aElement.OwnerDoc()->GetInnerWindow())) {
    AUTOPLAY_LOG("Autoplay allowed as activated/whitelisted window, media %p.", &aElement);
    return true;
  }

  nsIDocument* topDocument = ApproverDocOf(*aElement.OwnerDoc());
  if (topDocument &&
      topDocument->MediaDocumentKind() == nsIDocument::MediaDocumentKind::Video) {
    AUTOPLAY_LOG("Allow video document %p to autoplay", &aElement);
    return true;
  }

  if (!aElement.HasAudio() &&
      aElement.ReadyState() >= HTMLMediaElement_Binding::HAVE_METADATA) {
    AUTOPLAY_LOG("Allow media %p without audio track to autoplay", &aElement);
    return true;
  }

  if (!aElement.HasAudio() &&
      aElement.ReadyState() >= HTMLMediaElement_Binding::HAVE_METADATA) {
    AUTOPLAY_LOG("Allow media without audio track %p to autoplay\n", &aElement);
    return true;
  }

  return false;
}

/* static */ bool
AutoplayPolicy::WouldBeAllowedToPlayIfAutoplayDisabled(const HTMLMediaElement& aElement)
{
  return IsMediaElementAllowedToPlay(aElement);
}

/* static */ bool
AutoplayPolicy::IsAllowedToPlay(const HTMLMediaElement& aElement)
{
  const uint32_t autoplayDefault = DefaultAutoplayBehaviour();
  // TODO : this old way would be removed when user-gestures-needed becomes
  // as a default option to block autoplay.
  if (!Preferences::GetBool("media.autoplay.enabled.user-gestures-needed", false)) {
    // If element is blessed, it would always be allowed to play().
    return (autoplayDefault == nsIAutoplay::ALLOWED ||
            aElement.IsBlessed() ||
            EventStateManager::IsHandlingUserInput());
  }

  if (IsMediaElementAllowedToPlay(aElement)) {
    return true;
  }

  const bool result = IsMediaElementAllowedToPlay(aElement) ||
    autoplayDefault == nsIAutoplay::ALLOWED;

  AUTOPLAY_LOG("IsAllowedToPlay, mediaElement=%p, isAllowToPlay=%s",
                &aElement, AllowAutoplayToStr(result));

  return result;
}

/* static */ bool
AutoplayPolicy::IsAllowedToPlay(const AudioContext& aContext)
{
  if (!Preferences::GetBool("media.autoplay.block-webaudio", false)) {
    return true;
  }

  if (DefaultAutoplayBehaviour() == nsIAutoplay::ALLOWED) {
    return true;
  }

  if (!Preferences::GetBool("media.autoplay.enabled.user-gestures-needed", false)) {
    return true;
  }

  // Offline context won't directly output sound to audio devices.
  if (aContext.IsOffline()) {
    return true;
  }

  if (IsWindowAllowedToPlay(aContext.GetParentObject())) {
    return true;
  }

  return false;
}

} // namespace dom
} // namespace mozilla
