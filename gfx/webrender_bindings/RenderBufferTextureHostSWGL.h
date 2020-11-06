/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERBUFFERTEXTUREHOSTSWGL_H
#define MOZILLA_GFX_RENDERBUFFERTEXTUREHOSTSWGL_H

#include "RenderTextureHostSWGL.h"

namespace mozilla {
namespace wr {

class RenderBufferTextureHostSWGL final : public RenderTextureHostSWGL {
 public:
  RenderBufferTextureHostSWGL(uint8_t* aBuffer,
                              const layers::BufferDescriptor& aDescriptor);

  size_t GetPlaneCount() const override;

  gfx::SurfaceFormat GetFormat() const override;

  gfx::ColorDepth GetColorDepth() const override;

  gfx::YUVColorSpace GetYUVColorSpace() const override;

  bool MapPlane(RenderCompositor* aCompositor, uint8_t aChannelIndex,
                PlaneInfo& aPlaneInfo) override;

  void UnmapPlanes() override;

 private:
  virtual ~RenderBufferTextureHostSWGL();

  uint8_t* mBuffer;
  layers::BufferDescriptor mDescriptor;
};

}  // namespace wr
}  // namespace mozilla

#endif  // MOZILLA_GFX_RENDERBUFFERTEXTUREHOSTSWGL_H
