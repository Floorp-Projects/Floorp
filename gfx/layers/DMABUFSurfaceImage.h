/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SURFACE_DMABUF_H
#define SURFACE_DMABUF_H

#include "ImageContainer.h"

class DMABufSurface;

namespace mozilla {
namespace layers {

class TextureClient;

class DMABUFSurfaceImage : public Image {
 public:
  explicit DMABUFSurfaceImage(DMABufSurface* aSurface);
  ~DMABUFSurfaceImage();

  DMABufSurface* GetSurface() { return mSurface; }
  gfx::IntSize GetSize() const override;
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  nsresult BuildSurfaceDescriptorBuffer(
      SurfaceDescriptorBuffer& aSdBuffer, BuildSdbFlags aFlags,
      const std::function<MemoryOrShmem(uint32_t)>& aAllocate) override;
  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;

 private:
  RefPtr<DMABufSurface> mSurface;
  RefPtr<TextureClient> mTextureClient;
};

}  // namespace layers
}  // namespace mozilla

#endif  // SURFACE_DMABUF_H
