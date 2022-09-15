/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_DisplayConfigWindows_h
#define mozilla_gfx_thebes_DisplayConfigWindows_h

#include <optional>  // for std::optional
#include <utility>   // for std::pair
#include <vector>    // for std::vector
#include <wingdi.h>
#include "mozilla/gfx/Point.h"  // for IntSize
#include "nsTArray.h"

namespace mozilla {
namespace gfx {

struct DisplayConfig {
  std::vector<DISPLAYCONFIG_PATH_INFO> mPaths;
  std::vector<DISPLAYCONFIG_MODE_INFO> mModes;
};

std::optional<DisplayConfig> GetDisplayConfig();

extern bool HasScaledResolution();

typedef nsTArray<std::pair<IntSize, IntSize>> ScaledResolutionSet;
void GetScaledResolutions(ScaledResolutionSet& aRv);

}  // namespace gfx
}  // namespace mozilla

#endif  // mozilla_gfx_thebes_DisplayConfigWindows_h
