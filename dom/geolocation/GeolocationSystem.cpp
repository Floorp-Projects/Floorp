/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeolocationSystem.h"

namespace mozilla::dom::geolocation {

SystemGeolocationPermissionBehavior GetGeolocationPermissionBehavior() {
  return SystemGeolocationPermissionBehavior::NoPrompt;
}

already_AddRefed<SystemGeolocationPermissionRequest> PresentSystemSettings(
    BrowsingContext* aBrowsingContext, ParentRequestResolver&& aResolver) {
  MOZ_ASSERT_UNREACHABLE(
      "Should not warn user of need for system location permission "
      "since we cannot open system settings on this platform.");
  return nullptr;
}

}  // namespace mozilla::dom::geolocation
