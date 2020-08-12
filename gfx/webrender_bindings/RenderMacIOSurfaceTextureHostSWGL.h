/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERMACIOSURFACETEXTUREHOSTSWGL_H
#define MOZILLA_GFX_RENDERMACIOSURFACETEXTUREHOSTSWGL_H

#include "mozilla/gfx/MacIOSurface.h"
#include "RenderTextureHostSWGL.h"

namespace mozilla {
namespace wr {

class RenderMacIOSurfaceTextureHostSWGL final : public RenderTextureHostSWGL {
 public:
  explicit RenderMacIOSurfaceTextureHostSWGL(MacIOSurface* aSurface);

  size_t GetPlaneCount() override;

  bool MapPlane(uint8_t aChannelIndex, PlaneInfo& aPlaneInfo) override;

  void UnmapPlanes() override;

 private:
  virtual ~RenderMacIOSurfaceTextureHostSWGL();

  RefPtr<MacIOSurface> mSurface;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERMACIOSURFACETEXTUREHOSTSWGL_H
