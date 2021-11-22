/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersTypes.h"

#include <cinttypes>
#include "nsPrintfCString.h"
#include "mozilla/gfx/gfxVars.h"

#ifdef XP_WIN
#  include "gfxConfig.h"
#  include "mozilla/StaticPrefs_gfx.h"
#endif

namespace mozilla {
namespace layers {

const char* kCompositionPayloadTypeNames[kCompositionPayloadTypeCount] = {
    "KeyPress",
    "APZScroll",
    "APZPinchZoom",
    "ContentPaint",
    "MouseUpFollowedByClick",
};

const char* GetLayersBackendName(LayersBackend aBackend) {
  switch (aBackend) {
    case LayersBackend::LAYERS_NONE:
      return "none";
    case LayersBackend::LAYERS_WR:
      MOZ_ASSERT(gfx::gfxVars::UseWebRender());
      if (gfx::gfxVars::UseSoftwareWebRender()) {
#ifdef XP_WIN
        if (gfx::gfxVars::AllowSoftwareWebRenderD3D11() &&
            gfx::gfxConfig::IsEnabled(gfx::Feature::D3D11_COMPOSITING)) {
          return "webrender_software_d3d11";
        }
#endif
        return "webrender_software";
      }
      return "webrender";
    default:
      MOZ_ASSERT_UNREACHABLE("unknown layers backend");
      return "unknown";
  }
}

std::ostream& operator<<(std::ostream& aStream, const LayersId& aId) {
  return aStream << nsPrintfCString("0x%" PRIx64, aId.mId).get();
}

}  // namespace layers
}  // namespace mozilla
