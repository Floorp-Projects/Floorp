/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_MaskOperation_h
#define mozilla_gfx_layers_mlgpu_MaskOperation_h

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Rect.h"
#include "SharedBufferMLGPU.h"
#include <vector>

namespace mozilla {
namespace layers {

class FrameBuilder;
class Layer;
class MLGDevice;
class MLGRenderTarget;
class MLGTexture;
class TextureSource;

class MaskOperation {
  NS_INLINE_DECL_REFCOUNTING(MaskOperation)

 public:
  // For when the exact texture is known ahead of time.
  MaskOperation(FrameBuilder* aBuilder, MLGTexture* aSource);

  // Return the mask rectangle in screen coordinates. This function takes a
  // layer because a single-texture mask operation is not dependent on a
  // specific mask transform. (Multiple mask layer operations are, and they
  // ignore the layer parameter).
  virtual gfx::Rect ComputeMaskRect(Layer* aLayer) const;

  MLGTexture* GetTexture() const { return mTexture; }
  bool IsEmpty() const { return !mTexture; }

 protected:
  explicit MaskOperation(FrameBuilder* aBuilder);
  virtual ~MaskOperation();

 protected:
  RefPtr<MLGTexture> mTexture;
};

struct MaskTexture {
  MaskTexture() : mSource(nullptr) {}
  MaskTexture(const gfx::Rect& aRect, TextureSource* aSource)
      : mRect(aRect), mSource(aSource) {}

  bool operator<(const MaskTexture& aOther) const;

  gfx::Rect mRect;
  RefPtr<TextureSource> mSource;
};

typedef std::vector<MaskTexture> MaskTextureList;

class MaskCombineOperation final : public MaskOperation {
 public:
  explicit MaskCombineOperation(FrameBuilder* aBuilder);
  virtual ~MaskCombineOperation();

  void Init(const MaskTextureList& aTextures);

  void PrepareForRendering();
  void Render();

  gfx::Rect ComputeMaskRect(Layer* aLayer) const override { return mArea; }

 private:
  FrameBuilder* mBuilder;
  gfx::Rect mArea;
  MaskTextureList mTextures;
  RefPtr<MLGRenderTarget> mTarget;

  std::vector<VertexBufferSection> mInputBuffers;
};

RefPtr<TextureSource> GetMaskLayerTexture(Layer* aLayer);
void AppendToMaskTextureList(MaskTextureList& aList, Layer* aLayer);

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_gfx_layers_mlgpu_MaskOperation_h
