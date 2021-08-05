/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_2D_DWRITESETTINGS_H_
#define MOZILLA_GFX_2D_DWRITESETTINGS_H_

#include <dwrite.h>

#include "mozilla/AlreadyAddRefed.h"
#include "Types.h"

namespace mozilla {
namespace gfx {

class DWriteSettings final {
 public:
  static void Initialize();

  static DWriteSettings& Get(bool aGDISettings);

  Float ClearTypeLevel();
  Float EnhancedContrast();
  Float Gamma();
  DWRITE_PIXEL_GEOMETRY PixelGeometry();
  DWRITE_RENDERING_MODE RenderingMode();
  DWRITE_MEASURING_MODE MeasuringMode();
  already_AddRefed<IDWriteRenderingParams> RenderingParams();

 private:
  explicit DWriteSettings(bool aUseGDISettings);

  const bool mUseGDISettings;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // MOZILLA_GFX_2D_DWRITESETTINGS_H_
