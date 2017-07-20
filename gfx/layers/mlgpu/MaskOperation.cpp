/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MaskOperation.h"
#include "FrameBuilder.h"
#include "LayerMLGPU.h"
#include "mozilla/layers/LayersHelpers.h"
#include "MLGDevice.h"
#include "TexturedLayerMLGPU.h"

namespace mozilla {
namespace layers {

using namespace gfx;

MaskOperation::MaskOperation(FrameBuilder* aBuilder)
{
}

MaskOperation::MaskOperation(FrameBuilder* aBuilder, MLGTexture* aSource)
 : mTexture(aSource)
{
}

MaskOperation::~MaskOperation()
{
}

static gfx::Rect
ComputeQuadForMaskLayer(Layer* aLayer, const IntSize& aSize)
{
  const Matrix4x4& transform = aLayer->GetEffectiveTransform();
  MOZ_ASSERT(transform.Is2D(), "Mask layers should not have 3d transforms");

  Rect bounds(Point(0, 0), Size(aSize));
  return transform.As2D().TransformBounds(bounds);
}

Rect
MaskOperation::ComputeMaskRect(Layer* aLayer) const
{
  Layer* maskLayer = aLayer->GetMaskLayer()
                     ? aLayer->GetMaskLayer()
                     : aLayer->GetAncestorMaskLayerAt(0);
  MOZ_ASSERT((aLayer->GetAncestorMaskLayerCount() == 0 && aLayer->GetMaskLayer()) ||
             (aLayer->GetAncestorMaskLayerCount() == 1 && !aLayer->GetMaskLayer()));

  return ComputeQuadForMaskLayer(maskLayer, mTexture->GetSize());
}

// This is only needed for std::map.
bool
MaskTexture::operator <(const MaskTexture& aOther) const
{
  if (mRect.x != aOther.mRect.x) {
    return mRect.x < aOther.mRect.x;
  }
  if (mRect.y != aOther.mRect.y) {
    return mRect.y < aOther.mRect.y;
  }
  if (mRect.width != aOther.mRect.width) {
    return mRect.width < aOther.mRect.width;
  }
  if (mRect.height != aOther.mRect.height) {
    return mRect.height < aOther.mRect.height;
  }
  return mSource < aOther.mSource;
}

RefPtr<TextureSource>
GetMaskLayerTexture(Layer* aLayer)
{
  LayerMLGPU* layer = aLayer->AsHostLayer()->AsLayerMLGPU();
  TexturedLayerMLGPU* texLayer = layer->AsTexturedLayerMLGPU();
  if (!texLayer) {
    MOZ_ASSERT_UNREACHABLE("Mask layers should be texture layers");
    return nullptr;
  }

  RefPtr<TextureSource> source = texLayer->BindAndGetTexture();
  if (!source) {
    gfxWarning() << "Mask layer does not have a TextureSource";
    return nullptr;
  }
  return source.forget();
}

MaskCombineOperation::MaskCombineOperation(FrameBuilder* aBuilder)
 : MaskOperation(aBuilder),
   mBuilder(aBuilder)
{
}

MaskCombineOperation::~MaskCombineOperation()
{
}

void
MaskCombineOperation::Init(const MaskTextureList& aTextures)
{
  // All masks for a single layer exist in the same coordinate space. Find the
  // area that covers all rects.
  Rect area = aTextures[0].mRect;
  for (size_t i = 1; i < aTextures.size(); i++) {
    area = area.Intersect(aTextures[i].mRect);
  }

  // Go through and decide which areas of the textures are relevant.
  for (size_t i = 0; i < aTextures.size(); i++) {
    Rect rect = aTextures[i].mRect.Intersect(area);
    if (rect.IsEmpty()) {
      continue;
    }

    rect -= aTextures[i].mRect.TopLeft();
    mTextures.push_back(MaskTexture(rect, aTextures[i].mSource));
  }

  IntRect size;
  Rect bounds = area;
  bounds.RoundOut();
  bounds.ToIntRect(&size);

  if (size.IsEmpty()) {
    return;
  }

  mTarget = mBuilder->GetDevice()->CreateRenderTarget(size.Size());
  if (mTarget) {
    mTexture = mTarget->GetTexture();
  }
  mArea = area;
}

void
MaskCombineOperation::PrepareForRendering()
{
  for (const auto& entry : mTextures) {
    Rect texCoords = TextureRectToCoords(entry.mRect, entry.mSource->GetSize());

    SharedVertexBuffer* shared = mBuilder->GetDevice()->GetSharedVertexBuffer();

    VertexBufferSection section;
    if (!shared->Allocate(&section, 1, sizeof(texCoords), &texCoords)) {
      continue;
    }
    mInputBuffers.push_back(section);
  }
}

void
MaskCombineOperation::Render()
{
  if (!mTarget) {
    return;
  }

  RefPtr<MLGDevice> device = mBuilder->GetDevice();

  device->SetTopology(MLGPrimitiveTopology::UnitQuad);
  device->SetVertexShader(VertexShaderID::MaskCombiner);

  device->SetPixelShader(PixelShaderID::MaskCombiner);
  device->SetSamplerMode(0, SamplerMode::LinearClamp);
  device->SetBlendState(MLGBlendState::Min);

  // Since the mask operation is effectively an AND operation, we initialize
  // the entire r-channel to 1.
  device->Clear(mTarget, Color(1, 0, 0, 1));
  device->SetScissorRect(Nothing());
  device->SetRenderTarget(mTarget);
  device->SetViewport(IntRect(IntPoint(0, 0), mTarget->GetSize()));

  for (size_t i = 0; i < mInputBuffers.size(); i++) {
    if (!mInputBuffers[i].IsValid()) {
      continue;
    }
    device->SetVertexBuffer(1, &mInputBuffers[i]);
    device->SetPSTexture(0, mTextures[i].mSource);
    device->DrawInstanced(4, mInputBuffers[i].NumVertices(), 0, 0);
  }
}

void
AppendToMaskTextureList(MaskTextureList& aList, Layer* aLayer)
{
  RefPtr<TextureSource> source = GetMaskLayerTexture(aLayer);
  if (!source) {
    return;
  }

  gfx::Rect rect = ComputeQuadForMaskLayer(aLayer, source->GetSize());
  aList.push_back(MaskTexture(rect, source));
}

} // namespace layers
} // namespace mozilla
