/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CANVASCLIENT_H
#define MOZILLA_GFX_CANVASCLIENT_H

#include "mozilla/Assertions.h"                 // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"                 // for override
#include "mozilla/RefPtr.h"                     // for RefPtr, already_AddRefed
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/CompositorTypes.h"     // for TextureInfo, etc
#include "mozilla/layers/LayersSurfaces.h"      // for SurfaceDescriptor
#include "mozilla/layers/TextureClient.h"       // for TextureClient, etc
#include "mozilla/layers/PersistentBufferProvider.h"

#include "mozilla/MaybeOneOf.h"

#include "mozilla/mozalloc.h"  // for operator delete

#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/gfx/Types.h"  // for SurfaceFormat

namespace mozilla {
namespace layers {

class CompositableForwarder;

/**
 * Compositable client for 2d and webgl canvas.
 */
class CanvasClient final : public CompositableClient {
  int32_t mFrameID = 0;
  RefPtr<TextureClient> mFrontBuffer;

 public:
  /**
   * Creates, configures, and returns a new canvas client. If necessary, a
   * message will be sent to the compositor to create a corresponding image
   * host.
   */
  CanvasClient(CompositableForwarder* aFwd, const TextureFlags flags)
      : CompositableClient(aFwd, flags) {}

  virtual ~CanvasClient() = default;

  void Clear() { mFrontBuffer = nullptr; }

  bool AddTextureClient(TextureClient* aTexture) override {
    ++mFrameID;
    return CompositableClient::AddTextureClient(aTexture);
  }

  TextureInfo GetTextureInfo() const override {
    return TextureInfo(CompositableType::IMAGE, ImageUsageType::Canvas,
                       mTextureFlags);
  }

  void OnDetach() override { Clear(); }

  RefPtr<TextureClient> CreateTextureClientForCanvas(gfx::SurfaceFormat,
                                                     gfx::IntSize,
                                                     TextureFlags);
  void UseTexture(TextureClient*);
};

}  // namespace layers
}  // namespace mozilla

#endif
