/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PermissionUtils.h"
#include "nsIPermissionManager.h"

namespace mozilla::dom {

static const nsLiteralCString kPermissionTypes[] = {
    // clang-format off
    "geo"_ns,
    "desktop-notification"_ns,
    // Alias `push` to `desktop-notification`.
    "desktop-notification"_ns,
    "persistent-storage"_ns
    // clang-format on
};

const size_t kPermissionNameCount = PermissionNameValues::Count;

static_assert(MOZ_ARRAY_LENGTH(kPermissionTypes) == kPermissionNameCount,
              "kPermissionTypes and PermissionName count should match");

const nsLiteralCString& PermissionNameToType(PermissionName aName) {
  MOZ_ASSERT((size_t)aName < ArrayLength(kPermissionTypes));
  return kPermissionTypes[static_cast<size_t>(aName)];
}

Maybe<PermissionName> TypeToPermissionName(const nsACString& aType) {
  for (size_t i = 0; i < ArrayLength(kPermissionTypes); ++i) {
    if (kPermissionTypes[i].Equals(aType)) {
      return Some(static_cast<PermissionName>(i));
    }
  }

  return Nothing();
}

PermissionState ActionToPermissionState(uint32_t aAction) {
  switch (aAction) {
    case nsIPermissionManager::ALLOW_ACTION:
      return PermissionState::Granted;

    case nsIPermissionManager::DENY_ACTION:
      return PermissionState::Denied;

    default:
    case nsIPermissionManager::PROMPT_ACTION:
      return PermissionState::Prompt;
  }
}

}  // namespace mozilla::dom
