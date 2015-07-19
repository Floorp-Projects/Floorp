/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gfxTelemetry.h"

namespace mozilla {
namespace gfx {

const char*
FeatureStatusToString(FeatureStatus aStatus)
{
  switch (aStatus) {
    case FeatureStatus::Unused:
      return "unused";
    case FeatureStatus::Unavailable:
      return "unavailable";
    case FeatureStatus::Blocked:
      return "blocked";
    case FeatureStatus::Blacklisted:
      return "blacklisted";
    case FeatureStatus::Failed:
      return "failed";
    case FeatureStatus::Disabled:
      return "disabled";
    case FeatureStatus::Available:
      return "available";
    default:
      MOZ_ASSERT_UNREACHABLE("missing status case");
      return "unknown";
  }
}

} // namespace gfx
} // namespace mozilla
