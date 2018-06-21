/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoplayPolicy.h"

#include "mozilla/EventStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/AudioContext.h"
#include "mozilla/dom/AutoplayRequest.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/HTMLMediaElementBinding.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "MediaManager.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsPIDOMWindow.h"

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
IsAllowedToPlay(nsPIDOMWindowInner* aWindow)
{
  if (!aWindow) {
    return false;
  }

  // Pages which have been granted permission to capture WebRTC camera or
  // microphone are assumed to be trusted, and are allowed to autoplay.
  MediaManager* manager = MediaManager::GetIfExists();
  if (manager &&
      manager->IsActivelyCapturingOrHasAPermission(aWindow->WindowID())) {
    return true;
  }

  if (!aWindow->GetExtantDoc()) {
    return false;
  }

  nsIDocument* approver = ApproverDocOf(*aWindow->GetExtantDoc());
  if (nsContentUtils::IsExactSitePermAllow(approver->NodePrincipal(),
                                           "autoplay-media")) {
    // Autoplay permission has been granted already.
    return true;
  }

  if (approver->HasBeenUserGestureActivated()) {
    // Document has been activated by user gesture.
    return true;
  }

  return false;
}

/* static */
already_AddRefed<AutoplayRequest>
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
  return window->GetAutoplayRequest();
}

/* static */ bool
AutoplayPolicy::IsMediaElementAllowedToPlay(NotNull<HTMLMediaElement*> aElement)
{
  if (Preferences::GetBool("media.autoplay.enabled")) {
    return true;
  }

  // TODO : this old way would be removed when user-gestures-needed becomes
  // as a default option to block autoplay.
  if (!Preferences::GetBool("media.autoplay.enabled.user-gestures-needed", false)) {
    // If element is blessed, it would always be allowed to play().
    return aElement->IsBlessed() ||
           EventStateManager::IsHandlingUserInput();
  }

  // Muted content
  if (aElement->Volume() == 0.0 || aElement->Muted()) {
    return true;
  }

  if (IsAllowedToPlay(aElement->OwnerDoc()->GetInnerWindow())) {
    return true;
  }

  return false;
}

/* static */ bool
AutoplayPolicy::IsAudioContextAllowedToPlay(NotNull<AudioContext*> aContext)
{
  if (Preferences::GetBool("media.autoplay.enabled")) {
    return true;
  }

  if (!Preferences::GetBool("media.autoplay.enabled.user-gestures-needed", false)) {
    return true;
  }

  // Offline context won't directly output sound to audio devices.
  if (aContext->IsOffline()) {
    return true;
  }

  if (IsAllowedToPlay(aContext->GetOwner())) {
    return true;
  }

  return false;
}

} // namespace dom
} // namespace mozilla
