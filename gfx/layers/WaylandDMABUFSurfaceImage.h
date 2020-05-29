/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WAYLAND_SURFACE_DMABUF_H
#define WAYLAND_SURFACE_DMABUF_H

#include "ImageContainer.h"
#include "mozilla/widget/WaylandDMABufSurface.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {
namespace layers {

class WaylandDMABUFSurfaceImage : public Image {
 public:
  explicit WaylandDMABUFSurfaceImage(WaylandDMABufSurface* aSurface)
      : Image(nullptr, ImageFormat::WAYLAND_DMABUF), mSurface(aSurface) {
    mSurface->GlobalRefAdd();
  }

  ~WaylandDMABUFSurfaceImage() { mSurface->GlobalRefRelease(); }

  WaylandDMABufSurface* GetSurface() { return mSurface; }

  gfx::IntSize GetSize() const override {
    return gfx::IntSize::Truncate(mSurface->GetWidth(), mSurface->GetHeight());
  }

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override {
    return nullptr;
  }

  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;

 private:
  RefPtr<WaylandDMABufSurface> mSurface;
  RefPtr<TextureClient> mTextureClient;
};

}  // namespace layers
}  // namespace mozilla

#endif  // WAYLAND_SURFACE_DMABUF_H
