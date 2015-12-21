/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PermissionUtils.h"

namespace mozilla {
namespace dom {

const char* kPermissionTypes[] = {
  "geo",
  "desktop-notification",
  // Alias `push` to `desktop-notification`.
  "desktop-notification"
};

// `-1` for the last null entry.
const size_t kPermissionNameCount =
  MOZ_ARRAY_LENGTH(PermissionNameValues::strings) - 1;

static_assert(MOZ_ARRAY_LENGTH(kPermissionTypes) == kPermissionNameCount,
              "kPermissionTypes and PermissionName count should match");

const char*
PermissionNameToType(PermissionName aName)
{
  MOZ_ASSERT((size_t)aName < ArrayLength(kPermissionTypes));
  return kPermissionTypes[static_cast<size_t>(aName)];
}

Maybe<PermissionName>
TypeToPermissionName(const char* aType)
{
  for (size_t i = 0; i < ArrayLength(kPermissionTypes); ++i) {
    if (!strcmp(aType, kPermissionTypes[i])) {
      return Some(static_cast<PermissionName>(i));
    }
  }

  return Nothing();
}

PermissionState
ActionToPermissionState(uint32_t aAction)
{
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

} // namespace dom
} // namespace mozilla
