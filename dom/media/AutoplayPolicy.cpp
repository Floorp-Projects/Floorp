/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AutoplayPolicy.h"

#include "mozilla/EventStateManager.h"
#include "mozilla/NotNull.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

/* static */ bool
AutoplayPolicy::IsDocumentAllowedToPlay(nsIDocument* aDoc)
{
  if (!Preferences::GetBool("media.autoplay.enabled.user-gestures-needed")) {
    return true;
  }

  return aDoc ? aDoc->HasBeenUserActivated() : false;
}

/* static */ bool
AutoplayPolicy::IsMediaElementAllowedToPlay(NotNull<HTMLMediaElement*> aElement)
{
  if (Preferences::GetBool("media.autoplay.enabled")) {
    return true;
  }

  if (Preferences::GetBool("media.autoplay.enabled.user-gestures-needed", false)) {
    return AutoplayPolicy::IsDocumentAllowedToPlay(aElement->OwnerDoc());
  }

  // TODO : this old way would be removed when user-gestures-needed becomes
  // as a default option to block autoplay.
  // If elelement is blessed, it would always be allowed to play().
  return aElement->IsBlessed() ||
         EventStateManager::IsHandlingUserInput();
}

} // namespace dom
} // namespace mozilla