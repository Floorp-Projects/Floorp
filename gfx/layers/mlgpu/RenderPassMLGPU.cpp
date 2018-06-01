/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderPassMLGPU.h"
#include "ContainerLayerMLGPU.h"
#include "FrameBuilder.h"
#include "ImageLayerMLGPU.h"
#include "LayersLogging.h"
#include "MaskOperation.h"
#include "MLGDevice.h"
#include "PaintedLayerMLGPU.h"
#include "RenderViewMLGPU.h"
#include "ShaderDefinitionsMLGPU.h"
#include "ShaderDefinitionsMLGPU-inl.h"
#include "SharedBufferMLGPU.h"
#include "mozilla/layers/LayersHelpers.h"
#include "mozilla/layers/LayersMessages.h"
#include "RenderPassMLGPU-inl.h"

namespace mozilla {
namespace layers {

using namespace gfx;

ItemInfo::ItemInfo(FrameBuilder* aBuilder,
                   RenderViewMLGPU* aView,
                   LayerMLGPU* aLayer,
                   int32_t aSortOrder,
                   const IntRect& aBounds,
                   Maybe<Polygon>&& aGeometry)
 : view(aView),
   layer(aLayer),
   type(RenderPassType::Unknown),
   layerIndex(kInvalidResourceIndex),
   sortOrder(aSortOrder),
   bounds(aBounds),
   geometry(std::move(aGeometry))
{
  const Matrix4x4& transform = aLayer->GetLayer()->GetEffectiveTransform();

  Matrix transform2D;
  if (!geometry &&
      transform.Is2D(&transform2D) &&
      transform2D.IsRectilinear())
  {
    this->rectilinear = true;
    if (transform2D.IsIntegerTranslation()) {
      this->translation = Some(IntPoint::Truncate(transform2D.GetTranslation()));
    }
  } else {
    this->rectilinear = false;
  }

  // Layers can have arbitrary clips or transforms, and we can't use built-in
  // scissor functionality when batching. Instead, pixel shaders will write
  // transparent pixels for positions outside of the clip. Unfortunately that
  // breaks z-buffering because the transparent pixels will still write to
  // the depth buffer.
  //
  // To make this work, we clamp the final vertices in the vertex shader to
  // the clip rect. We can only do this for rectilinear transforms. If a
  // transform can produce a rotation or perspective change, then we might
  // accidentally change the geometry. These items are not treated as
  // opaque.
  //
  // Also, we someday want non-rectilinear items to be antialiased with DEAA,
  // and we can't do this if the items are rendered front-to-back, since
  // such items cannot be blended. (Though we could consider adding these
  // items in two separate draw calls, one for DEAA and for not - that is
  // definitely future work.)
  if (aLayer->GetComputedOpacity() != 1.0f ||
      aLayer->GetMask() ||
      !aLayer->IsContentOpaque() ||
      !rectilinear)
  {
    this->opaque = false;
    this->renderOrder = RenderOrder::BackToFront;
  } else {
    this->opaque = true;
    this->renderOrder = aView->HasDepthBuffer()
                        ? RenderOrder::FrontToBack
                        : RenderOrder::BackToFront;
  }

  this->type = RenderPassMLGPU::GetPreferredPassType(aBuilder, *this);
}

RenderPassType
RenderPassMLGPU::GetPreferredPassType(FrameBuilder* aBuilder, const ItemInfo& aItem)
{
  LayerMLGPU* layer = aItem.layer;
  switch (layer->GetType()) {
  case Layer::TYPE_COLOR:
  {
    if (aBuilder->GetDevice()->CanUseClearView() &&
        aItem.HasRectTransformAndClip() &&
        aItem.translation &&
        aItem.opaque &&
        !aItem.view->HasDepthBuffer())
    {
      // Note: we don't have ClearView set up to do depth buffer writes, so we
      // exclude depth buffering from the test above.
      return RenderPassType::ClearView;
    }
    return RenderPassType::SolidColor;
  }
  case Layer::TYPE_PAINTED: {
    PaintedLayerMLGPU* painted = layer->AsPaintedLayerMLGPU();
    if (painted->HasComponentAlpha()) {
      return RenderPassType::ComponentAlpha;
    }
    return RenderPassType::SingleTexture;
  }
  case Layer::TYPE_CANVAS:
    return RenderPassType::SingleTexture;
  case Layer::TYPE_IMAGE: {
    ImageHost* host = layer->AsTexturedLayerMLGPU()->GetImageHost();
    TextureHost* texture = host->CurrentTextureHost();
    if (texture->GetReadFormat() == SurfaceFormat::YUV ||
        texture->GetReadFormat() == SurfaceFormat::NV12)
    {
      return RenderPassType::Video;
    }
    return RenderPassType::SingleTexture;
  }
  case Layer::TYPE_CONTAINER:
    return RenderPassType::RenderView;
  default:
    return RenderPassType::Unknown;
  }
}

RefPtr<RenderPassMLGPU>
RenderPassMLGPU::CreatePass(FrameBuilder* aBuilder, const ItemInfo& aItem)
{
  switch (aItem.type) {
  case RenderPassType::SolidColor:
    return MakeAndAddRef<SolidColorPass>(aBuilder, aItem);
  case RenderPassType::SingleTexture:
    return MakeAndAddRef<SingleTexturePass>(aBuilder, aItem);
  case RenderPassType::RenderView:
    return MakeAndAddRef<RenderViewPass>(aBuilder, aItem);
  case RenderPassType::Video:
    return MakeAndAddRef<VideoRenderPass>(aBuilder, aItem);
  case RenderPassType::ComponentAlpha:
    return MakeAndAddRef<ComponentAlphaPass>(aBuilder, aItem);
  case RenderPassType::ClearView:
    return MakeAndAddRef<ClearViewPass>(aBuilder, aItem);
  default:
    return nullptr;
  }
}

RenderPassMLGPU::RenderPassMLGPU(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : mBuilder(aBuilder),
   mDevice(aBuilder->GetDevice()),
   mLayerBufferIndex(aBuilder->CurrentLayerBufferIndex()),
   mMaskRectBufferIndex(kInvalidResourceIndex),
   mPrepared(false)
{
}

RenderPassMLGPU::~RenderPassMLGPU()
{
}

bool
RenderPassMLGPU::IsCompatible(const ItemInfo& aItem)
{
  if (GetType() != aItem.type) {
    return false;
  }
  if (mLayerBufferIndex != mBuilder->CurrentLayerBufferIndex()) {
    return false;
  }
  return true;
}

bool
RenderPassMLGPU::AcceptItem(ItemInfo& aInfo)
{
  MOZ_ASSERT(IsCompatible(aInfo));

  if (!AddToPass(aInfo.layer, aInfo)) {
    return false;
  }

  if (aInfo.renderOrder == RenderOrder::BackToFront) {
    mAffectedRegion.OrWith(aInfo.bounds);
    mAffectedRegion.SimplifyOutward(4);
  }
  return true;
}

bool
RenderPassMLGPU::Intersects(const ItemInfo& aItem)
{
  MOZ_ASSERT(aItem.renderOrder == RenderOrder::BackToFront);
  return !mAffectedRegion.Intersect(aItem.bounds).IsEmpty();
}

void
RenderPassMLGPU::PrepareForRendering()
{
  mPrepared = true;
}

ShaderRenderPass::ShaderRenderPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : RenderPassMLGPU(aBuilder, aItem),
   mGeometry(GeometryMode::Unknown),
   mHasRectTransformAndClip(aItem.HasRectTransformAndClip())
{
  mMask = aItem.layer->GetMask();
  if (mMask) {
    mMaskRectBufferIndex = mBuilder->CurrentMaskRectBufferIndex();
  }
}

bool
ShaderRenderPass::IsCompatible(const ItemInfo& aItem)
{
  MOZ_ASSERT(mGeometry != GeometryMode::Unknown);

  if (!RenderPassMLGPU::IsCompatible(aItem)) {
    return false;
  }

  // A masked batch cannot accept non-masked items, since the pixel shader
  // bakes in whether a mask is present. Also, the pixel shader can only bind
  // one specific mask at a time.
  if (aItem.layer->GetMask() != mMask) {
    return false;
  }
  if (mMask && mBuilder->CurrentMaskRectBufferIndex() != mMaskRectBufferIndex) {
    return false;
  }

  // We key batches on this property, since we can use more efficient pixel
  // shaders if we don't need to propagate a clip and a mask.
  if (mHasRectTransformAndClip != aItem.HasRectTransformAndClip()) {
    return false;
  }

  // We should be assured at this point, that if the item requires complex
  // geometry, then it should have already been rejected from a unit-quad
  // batch. Therefore this batch should be in polygon mode.
  MOZ_ASSERT_IF(aItem.geometry.isSome(), mGeometry == GeometryMode::Polygon);
  return true;
}

void
ShaderRenderPass::SetGeometry(const ItemInfo& aItem, GeometryMode aMode)
{
  MOZ_ASSERT(mGeometry == GeometryMode::Unknown);

  if (aMode == GeometryMode::Unknown) {
    mGeometry = mHasRectTransformAndClip
                ? GeometryMode::UnitQuad
                : GeometryMode::Polygon;
  } else {
    mGeometry = aMode;
  }

  // Since we process layers front-to-back, back-to-front items are
  // in the wrong order. We address this by automatically reversing
  // the buffers we use to build vertices.
  if (aItem.renderOrder != RenderOrder::FrontToBack) {
    mInstances.SetReversed();
  }
}

void
ShaderRenderPass::PrepareForRendering()
{
  if (mInstances.IsEmpty()) {
    return;
  }
  if (!mDevice->GetSharedVertexBuffer()->Allocate(&mInstanceBuffer, mInstances) ||
      !SetupPSBuffer0(GetOpacity()) ||
      !OnPrepareBuffers())
  {
    return;
  }
  return RenderPassMLGPU::PrepareForRendering();
}

bool
ShaderRenderPass::SetupPSBuffer0(float aOpacity)
{
  if (aOpacity == 1.0f && !HasMask()) {
    mPSBuffer0 = mBuilder->GetDefaultMaskInfo();
    return true;
  }

  MaskInformation cb(aOpacity, HasMask());
  return mDevice->GetSharedPSBuffer()->Allocate(&mPSBuffer0, cb);
}

void
ShaderRenderPass::ExecuteRendering()
{
  if (mInstances.IsEmpty()) {
    return;
  }

  // Change the blend state if needed.
  if (Maybe<MLGBlendState> blendState = GetBlendState()) {
    mDevice->SetBlendState(blendState.value());
  }

  mDevice->SetPSConstantBuffer(0, &mPSBuffer0);
  if (MaskOperation* mask = GetMask()) {
    mDevice->SetPSTexture(kMaskLayerTextureSlot, mask->GetTexture());
    mDevice->SetSamplerMode(kMaskSamplerSlot, SamplerMode::LinearClampToZero);
  }

  SetupPipeline();

  if (mGeometry == GeometryMode::Polygon) {
    mDevice->SetTopology(MLGPrimitiveTopology::UnitTriangle);
  } else {
    mDevice->SetTopology(MLGPrimitiveTopology::UnitQuad);
  }
  mDevice->SetVertexBuffer(1, &mInstanceBuffer);

  if (mGeometry == GeometryMode::Polygon) {
    mDevice->DrawInstanced(3, mInstanceBuffer.NumVertices(), 0, 0);
  } else {
    mDevice->DrawInstanced(4, mInstanceBuffer.NumVertices(), 0, 0);
  }
}

static inline Color
ComputeLayerColor(LayerMLGPU* aLayer, const Color& aColor)
{
  float opacity = aLayer->GetComputedOpacity();
  return Color(
    aColor.r * aColor.a * opacity,
    aColor.g * aColor.a * opacity,
    aColor.b * aColor.a * opacity,
    aColor.a * opacity);
}

ClearViewPass::ClearViewPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : RenderPassMLGPU(aBuilder, aItem),
   mView(aItem.view)
{
  // Note: we could write to the depth buffer, but since the depth buffer is
  // disabled by default, we don't bother yet.
  MOZ_ASSERT(!mView->HasDepthBuffer());

  ColorLayer* colorLayer = aItem.layer->GetLayer()->AsColorLayer();
  mColor = ComputeLayerColor(aItem.layer, colorLayer->GetColor());
}

bool
ClearViewPass::IsCompatible(const ItemInfo& aItem)
{
  if (!RenderPassMLGPU::IsCompatible(aItem)) {
    return false;
  }

  // These should be true if we computed a ClearView pass type.
  MOZ_ASSERT(aItem.translation);
  MOZ_ASSERT(aItem.opaque);
  MOZ_ASSERT(aItem.HasRectTransformAndClip());

  // Each call only supports a single color.
  ColorLayer* colorLayer = aItem.layer->GetLayer()->AsColorLayer();
  if (mColor != ComputeLayerColor(aItem.layer, colorLayer->GetColor())) {
    return false;
  }

  // We don't support opacity here since it would not blend correctly.
  MOZ_ASSERT(mColor.a == 1.0f);
  return true;
}

bool
ClearViewPass::AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo)
{
  const LayerIntRegion& region = aItem->GetRenderRegion();
  for (auto iter = region.RectIter(); !iter.Done(); iter.Next()) {
    IntRect rect = iter.Get().ToUnknownRect();
    rect += aInfo.translation.value();
    rect -= mView->GetTargetOffset();
    mRects.AppendElement(rect);
  }
  return true;
}

void
ClearViewPass::ExecuteRendering()
{
  mDevice->ClearView(mDevice->GetRenderTarget(), mColor, mRects.Elements(), mRects.Length());
}

SolidColorPass::SolidColorPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : BatchRenderPass(aBuilder, aItem)
{
  SetDefaultGeometry(aItem);
}

bool
SolidColorPass::AddToPass(LayerMLGPU* aLayer, ItemInfo& aInfo)
{
  MOZ_ASSERT(aLayer->GetType() == Layer::TYPE_COLOR);

  ColorLayer* colorLayer = aLayer->GetLayer()->AsColorLayer();

  Txn txn(this);

  gfx::Color color = ComputeLayerColor(aLayer, colorLayer->GetColor());

  const LayerIntRegion& region = aLayer->GetRenderRegion();
  for (auto iter = region.RectIter(); !iter.Done(); iter.Next()) {
    const IntRect rect = iter.Get().ToUnknownRect();
    ColorTraits traits(aInfo, Rect(rect), color);

    if (!txn.Add(traits)) {
      return false;
    }
  }
  return txn.Commit();
}

float
SolidColorPass::GetOpacity() const
{
  // Note our pixel shader just ignores the opacity, since we baked it
  // into our color values already. Just return 1, which ensures we can
  // use the default constant buffer binding.
  return 1.0f;
}

void
SolidColorPass::SetupPipeline()
{
  if (mGeometry == GeometryMode::UnitQuad) {
    mDevice->SetVertexShader(VertexShaderID::ColoredQuad);
    mDevice->SetPixelShader(PixelShaderID::ColoredQuad);
  } else {
    mDevice->SetVertexShader(VertexShaderID::ColoredVertex);
    mDevice->SetPixelShader(PixelShaderID::ColoredVertex);
  }
}

TexturedRenderPass::TexturedRenderPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : BatchRenderPass(aBuilder, aItem),
   mTextureFlags(TextureFlags::NO_FLAGS)
{
}

TexturedRenderPass::Info::Info(const ItemInfo& aItem, PaintedLayerMLGPU* aLayer)
 : item(aItem),
   textureSize(aLayer->GetTexture()->GetSize()),
   destOrigin(aLayer->GetDestOrigin()),
   decomposeIntoNoRepeatRects(aLayer->MayResample())
{
}

TexturedRenderPass::Info::Info(const ItemInfo& aItem, TexturedLayerMLGPU* aLayer)
 : item(aItem),
   textureSize(aLayer->GetTexture()->GetSize()),
   scale(aLayer->GetPictureScale()),
   decomposeIntoNoRepeatRects(false)
{
}

TexturedRenderPass::Info::Info(const ItemInfo& aItem, ContainerLayerMLGPU* aLayer)
 : item(aItem),
   textureSize(aLayer->GetTargetSize()),
   destOrigin(aLayer->GetTargetOffset()),
   decomposeIntoNoRepeatRects(false)
{
}

bool
TexturedRenderPass::AddItem(Txn& aTxn,
                            const Info& aInfo,
                            const Rect& aDrawRect)
{
  if (mGeometry == GeometryMode::Polygon) {
    // This path will not clamp the draw rect to the layer clip, so we can pass
    // the draw rect texture rects straight through.
    return AddClippedItem(aTxn, aInfo, aDrawRect);
  }

  const ItemInfo& item = aInfo.item;

  MOZ_ASSERT(!item.geometry);
  MOZ_ASSERT(item.HasRectTransformAndClip());
  MOZ_ASSERT(mHasRectTransformAndClip);

  const Matrix4x4& fullTransform = item.layer->GetLayer()->GetEffectiveTransformForBuffer();
  Matrix transform = fullTransform.As2D();
  Matrix inverse = transform;
  if (!inverse.Invert()) {
    // Degenerate transforms are not visible, since there is no mapping to
    // screen space. Just return without adding any draws.
    return true;
  }
  MOZ_ASSERT(inverse.IsRectilinear());

  // Transform the clip rect.
  IntRect clipRect = item.layer->GetComputedClipRect().ToUnknownRect();
  clipRect += item.view->GetTargetOffset();

  // Clip and adjust the texture rect.
  Rect localClip = inverse.TransformBounds(Rect(clipRect));
  Rect clippedDrawRect = aDrawRect.Intersect(localClip);
  if (clippedDrawRect.IsEmpty()) {
    return true;
  }

  return AddClippedItem(aTxn, aInfo, clippedDrawRect);
}

bool
TexturedRenderPass::AddClippedItem(Txn& aTxn,
                                   const Info& aInfo,
                                   const gfx::Rect& aDrawRect)
{
  float xScale = 1.0;
  float yScale = 1.0;
  if (aInfo.scale) {
    xScale = aInfo.scale->width;
    yScale = aInfo.scale->height;
  }

  Point offset = aDrawRect.TopLeft() - aInfo.destOrigin;
  Rect textureRect(
    offset.x * xScale,
    offset.y * yScale,
    aDrawRect.Width() * xScale,
    aDrawRect.Height() * yScale);

  Rect textureCoords = TextureRectToCoords(textureRect, aInfo.textureSize);
  if (mTextureFlags & TextureFlags::ORIGIN_BOTTOM_LEFT) {
    textureCoords.MoveToY(1.0 - textureCoords.Y());
    textureCoords.SetHeight(-textureCoords.Height());
  }

  if (!aInfo.decomposeIntoNoRepeatRects) {
    // Fast, normal case, we can use the texture coordinates as-s and the caller
    // will use a repeat sampler if needed.
    TexturedTraits traits(aInfo.item, aDrawRect, textureCoords);
    if (!aTxn.Add(traits)) {
      return false;
    }
  } else {
    Rect layerRects[4];
    Rect textureRects[4];
    size_t numRects =
      DecomposeIntoNoRepeatRects(aDrawRect, textureCoords, &layerRects, &textureRects);

    for (size_t i = 0; i < numRects; i++) {
      TexturedTraits traits(aInfo.item, layerRects[i], textureRects[i]);
      if (!aTxn.Add(traits)) {
        return false;
      }
    }
  }
  return true;
}

SingleTexturePass::SingleTexturePass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : TexturedRenderPass(aBuilder, aItem),
   mOpacity(1.0f)
{
  SetDefaultGeometry(aItem);
}

bool
SingleTexturePass::AddToPass(LayerMLGPU* aLayer, ItemInfo& aItem)
{
  RefPtr<TextureSource> texture;

  SamplerMode sampler;
  TextureFlags flags = TextureFlags::NO_FLAGS;
  if (PaintedLayerMLGPU* paintedLayer = aLayer->AsPaintedLayerMLGPU()) {
    if (paintedLayer->HasComponentAlpha()) {
      return false;
    }
    texture = paintedLayer->GetTexture();
    sampler = paintedLayer->GetSamplerMode();
  } else if (TexturedLayerMLGPU* texLayer = aLayer->AsTexturedLayerMLGPU()) {
    texture = texLayer->GetTexture();
    sampler = FilterToSamplerMode(texLayer->GetSamplingFilter());
    TextureHost* host = texLayer->GetImageHost()->CurrentTextureHost();
    flags = host->GetFlags();
  } else {
    return false;
  }

  // We should not assign a texture-based layer to tiles if it has no texture.
  MOZ_ASSERT(texture);

  float opacity = aLayer->GetComputedOpacity();
  if (mTexture) {
    if (texture != mTexture) {
      return false;
    }
    if (mSamplerMode != sampler) {
      return false;
    }
    if (mOpacity != opacity) {
      return false;
    }
    // Note: premultiplied, origin-bottom-left are already implied by the texture source.
  } else {
    mTexture = texture;
    mSamplerMode = sampler;
    mOpacity = opacity;
    mTextureFlags = flags;
  }

  Txn txn(this);

  // Note: these are two separate cases since no Info constructor takes in a
  // base LayerMLGPU class.
  if (PaintedLayerMLGPU* layer = aLayer->AsPaintedLayerMLGPU()) {
    Info info(aItem, layer);
    if (!AddItems(txn, info, layer->GetDrawRects())) {
      return false;
    }
  } else if (TexturedLayerMLGPU* layer = aLayer->AsTexturedLayerMLGPU()) {
    Info info(aItem, layer);
    if (!AddItems(txn, info, layer->GetRenderRegion())) {
      return false;
    }
  }

  return txn.Commit();
}

Maybe<MLGBlendState>
SingleTexturePass::GetBlendState() const
{
  return (mTextureFlags & TextureFlags::NON_PREMULTIPLIED)
         ? Some(MLGBlendState::OverAndPremultiply)
         : Some(MLGBlendState::Over);
}

void
SingleTexturePass::SetupPipeline()
{
  MOZ_ASSERT(mTexture);

  if (mGeometry == GeometryMode::UnitQuad) {
    mDevice->SetVertexShader(VertexShaderID::TexturedQuad);
  } else {
    mDevice->SetVertexShader(VertexShaderID::TexturedVertex);
  }

  mDevice->SetPSTexture(0, mTexture);
  mDevice->SetSamplerMode(kDefaultSamplerSlot, mSamplerMode);
  switch (mTexture.get()->GetFormat()) {
    case SurfaceFormat::B8G8R8A8:
    case SurfaceFormat::R8G8B8A8:
      if (mGeometry == GeometryMode::UnitQuad)
        mDevice->SetPixelShader(PixelShaderID::TexturedQuadRGBA);
      else
        mDevice->SetPixelShader(PixelShaderID::TexturedVertexRGBA);
      break;
    default:
      if (mGeometry == GeometryMode::UnitQuad)
        mDevice->SetPixelShader(PixelShaderID::TexturedQuadRGB);
      else
        mDevice->SetPixelShader(PixelShaderID::TexturedVertexRGB);
      break;
  }
}

ComponentAlphaPass::ComponentAlphaPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : TexturedRenderPass(aBuilder, aItem),
   mOpacity(1.0f)
{
  SetDefaultGeometry(aItem);
}

bool
ComponentAlphaPass::AddToPass(LayerMLGPU* aLayer, ItemInfo& aItem)
{
  PaintedLayerMLGPU* layer = aLayer->AsPaintedLayerMLGPU();
  MOZ_ASSERT(layer);

  if (mTextureOnBlack) {
    if (layer->GetTexture() != mTextureOnBlack ||
        layer->GetTextureOnWhite() != mTextureOnWhite ||
        layer->GetOpacity() != mOpacity ||
        layer->GetSamplerMode() != mSamplerMode)
    {
      return false;
    }
  } else {
    mOpacity = layer->GetComputedOpacity();
    mSamplerMode = layer->GetSamplerMode();
    mTextureOnBlack = layer->GetTexture();
    mTextureOnWhite = layer->GetTextureOnWhite();
  } 

  Txn txn(this);

  Info info(aItem, layer);
  if (!AddItems(txn, info, layer->GetDrawRects())) {
    return false;
  }
  return txn.Commit();
}

float
ComponentAlphaPass::GetOpacity() const
{
  return mOpacity;
}

void
ComponentAlphaPass::SetupPipeline()
{
  TextureSource* textures[2] = {
    mTextureOnBlack,
    mTextureOnWhite
  };
  MOZ_ASSERT(textures[0]);
  MOZ_ASSERT(textures[1]);

  if (mGeometry == GeometryMode::UnitQuad) {
    mDevice->SetVertexShader(VertexShaderID::TexturedQuad);
    mDevice->SetPixelShader(PixelShaderID::ComponentAlphaQuad);
  } else {
    mDevice->SetVertexShader(VertexShaderID::TexturedVertex);
    mDevice->SetPixelShader(PixelShaderID::ComponentAlphaVertex);
  }

  mDevice->SetSamplerMode(kDefaultSamplerSlot, mSamplerMode);
  mDevice->SetPSTextures(0, 2, textures);
}

VideoRenderPass::VideoRenderPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : TexturedRenderPass(aBuilder, aItem),
   mOpacity(1.0f)
{
  SetDefaultGeometry(aItem);
}

bool
VideoRenderPass::AddToPass(LayerMLGPU* aLayer, ItemInfo& aItem)
{
  ImageLayerMLGPU* layer = aLayer->AsImageLayerMLGPU();
  if (!layer) {
    return false;
  }

  RefPtr<TextureHost> host = layer->GetImageHost()->CurrentTextureHost();
  RefPtr<TextureSource> source = layer->GetTexture();
  float opacity = layer->GetComputedOpacity();
  SamplerMode sampler = FilterToSamplerMode(layer->GetSamplingFilter());

  if (mHost) {
    if (mHost != host) {
      return false;
    }
    if (mTexture != source) {
      return false;
    }
    if (mOpacity != opacity) {
      return false;
    }
    if (mSamplerMode != sampler) {
      return false;
    }
  } else {
    mHost = host;
    mTexture = source;
    mOpacity = opacity;
    mSamplerMode = sampler;
  }
  MOZ_ASSERT(!mTexture->AsBigImageIterator());
  MOZ_ASSERT(!(mHost->GetFlags() & TextureFlags::NON_PREMULTIPLIED));
  MOZ_ASSERT(!(mHost->GetFlags() & TextureFlags::ORIGIN_BOTTOM_LEFT));

  Txn txn(this);

  Info info(aItem, layer);
  if (!AddItems(txn, info, layer->GetRenderRegion())) {
    return false;
  }
  return txn.Commit();
}

void
VideoRenderPass::SetupPipeline()
{
  YUVColorSpace colorSpace = YUVColorSpace::UNKNOWN;
  switch (mHost->GetReadFormat()) {
  case SurfaceFormat::YUV: {
    colorSpace = mHost->GetYUVColorSpace();
    break;
  }
  case SurfaceFormat::NV12:
    colorSpace = YUVColorSpace::BT601;
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Unexpected surface format in VideoRenderPass");
    break;
  }
  MOZ_ASSERT(colorSpace != YUVColorSpace::UNKNOWN);

  RefPtr<MLGBuffer> ps1 = mDevice->GetBufferForColorSpace(colorSpace);
  if (!ps1) {
    return;
  }

  if (mGeometry == GeometryMode::UnitQuad) {
    mDevice->SetVertexShader(VertexShaderID::TexturedQuad);
  } else {
    mDevice->SetVertexShader(VertexShaderID::TexturedVertex);
  }

  switch (mHost->GetReadFormat()) {
  case SurfaceFormat::YUV:
  {
    if (mGeometry == GeometryMode::UnitQuad)
      mDevice->SetPixelShader(PixelShaderID::TexturedQuadIMC4);
    else
      mDevice->SetPixelShader(PixelShaderID::TexturedVertexIMC4);
    mDevice->SetPSTexturesYUV(0, mTexture);
    break;
  }
  case SurfaceFormat::NV12:
    if (mGeometry == GeometryMode::UnitQuad)
      mDevice->SetPixelShader(PixelShaderID::TexturedQuadNV12);
    else
      mDevice->SetPixelShader(PixelShaderID::TexturedVertexNV12);
    mDevice->SetPSTexturesNV12(0, mTexture);
    break;
  default:
    MOZ_ASSERT_UNREACHABLE("Unknown video format");
    break;
  }

  mDevice->SetSamplerMode(kDefaultSamplerSlot, mSamplerMode);
  mDevice->SetPSConstantBuffer(1, ps1);
}

RenderViewPass::RenderViewPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
 : TexturedRenderPass(aBuilder, aItem),
   mParentView(nullptr)
{
  mAssignedLayer = aItem.layer->AsContainerLayerMLGPU();

  CompositionOp blendOp = mAssignedLayer->GetMixBlendMode();
  if (BlendOpIsMixBlendMode(blendOp)) {
    mBlendMode = Some(blendOp);
  }

  if (mBlendMode) {
    // We do not have fast-path rect shaders for blending.
    SetGeometry(aItem, GeometryMode::Polygon);
  } else {
    SetDefaultGeometry(aItem);
  }
}

bool
RenderViewPass::AddToPass(LayerMLGPU* aLayer, ItemInfo& aItem)
{
  // We bake in the layer ahead of time, which also guarantees the blend mode
  // is baked in, as well as the geometry requirement.
  if (mAssignedLayer != aLayer) {
    return false;
  }

  mSource = mAssignedLayer->GetRenderTarget();
  if (!mSource) {
    return false;
  }

  mParentView = aItem.view;

  Txn txn(this);

  IntPoint offset = mAssignedLayer->GetTargetOffset();
  IntSize size = mAssignedLayer->GetTargetSize();

  // Clamp the visible region to the texture size.
  nsIntRegion visible = mAssignedLayer->GetRenderRegion().ToUnknownRegion();
  visible.AndWith(IntRect(offset, size));

  Info info(aItem, mAssignedLayer);
  if (!AddItems(txn, info, visible)) {
    return false;
  }
  return txn.Commit();
}

float
RenderViewPass::GetOpacity() const
{
  return mAssignedLayer->GetLayer()->GetEffectiveOpacity();
}

bool
RenderViewPass::OnPrepareBuffers()
{
  if (mBlendMode && !PrepareBlendState()) {
    return false;
  }
  return true;
}

static inline PixelShaderID
GetShaderForBlendMode(CompositionOp aOp)
{
  switch (aOp) {
  case CompositionOp::OP_MULTIPLY: return PixelShaderID::BlendMultiply;
  case CompositionOp::OP_SCREEN: return PixelShaderID::BlendScreen;
  case CompositionOp::OP_OVERLAY: return PixelShaderID::BlendOverlay;
  case CompositionOp::OP_DARKEN: return PixelShaderID::BlendDarken;
  case CompositionOp::OP_LIGHTEN: return PixelShaderID::BlendLighten;
  case CompositionOp::OP_COLOR_DODGE: return PixelShaderID::BlendColorDodge;
  case CompositionOp::OP_COLOR_BURN: return PixelShaderID::BlendColorBurn;
  case CompositionOp::OP_HARD_LIGHT: return PixelShaderID::BlendHardLight;
  case CompositionOp::OP_SOFT_LIGHT: return PixelShaderID::BlendSoftLight;
  case CompositionOp::OP_DIFFERENCE: return PixelShaderID::BlendDifference;
  case CompositionOp::OP_EXCLUSION: return PixelShaderID::BlendExclusion;
  case CompositionOp::OP_HUE: return PixelShaderID::BlendHue;
  case CompositionOp::OP_SATURATION: return PixelShaderID::BlendSaturation;
  case CompositionOp::OP_COLOR: return PixelShaderID::BlendColor;
  case CompositionOp::OP_LUMINOSITY: return PixelShaderID::BlendLuminosity;
  default:
    MOZ_ASSERT_UNREACHABLE("Unexpected blend mode");
    return PixelShaderID::TexturedVertexRGBA;
  }
}

bool
RenderViewPass::PrepareBlendState()
{
  Rect visibleRect(mAssignedLayer->GetRenderRegion().GetBounds().ToUnknownRect());
  IntRect clipRect(mAssignedLayer->GetComputedClipRect().ToUnknownRect());
  const Matrix4x4& transform = mAssignedLayer->GetLayer()->GetEffectiveTransformForBuffer();

  // Note that we must use our parent RenderView for this calculation,
  // since we're copying the backdrop, not our actual local target.
  IntRect rtRect(mParentView->GetTargetOffset(), mParentView->GetSize());

  Matrix4x4 backdropTransform;
  mBackdropCopyRect = ComputeBackdropCopyRect(
    visibleRect,
    clipRect,
    transform,
    rtRect,
    &backdropTransform);

  AutoBufferUpload<BlendVertexShaderConstants> cb;
  if (!mDevice->GetSharedVSBuffer()->Allocate(&mBlendConstants, &cb)) {
    return false;
  }
  memcpy(cb->backdropTransform, &backdropTransform._11, 64);
  return true;
}

void
RenderViewPass::SetupPipeline()
{
  if (mBlendMode) {
    RefPtr<MLGRenderTarget> backdrop = mParentView->GetRenderTarget();
    MOZ_ASSERT(mDevice->GetRenderTarget() == backdrop);

    RefPtr<MLGTexture> copy = mDevice->CreateTexture(
      mBackdropCopyRect.Size(),
      SurfaceFormat::B8G8R8A8,
      MLGUsage::Default,
      MLGTextureFlags::ShaderResource);
    if (!copy) {
      return;
    }

    mDevice->CopyTexture(
      copy,
      IntPoint(0, 0),
      backdrop->GetTexture(),
      mBackdropCopyRect);

    MOZ_ASSERT(mGeometry == GeometryMode::Polygon);
    mDevice->SetVertexShader(VertexShaderID::BlendVertex);
    mDevice->SetPixelShader(GetShaderForBlendMode(mBlendMode.value()));
    mDevice->SetVSConstantBuffer(kBlendConstantBufferSlot, &mBlendConstants);
    mDevice->SetPSTexture(1, copy);
  } else {
    if (mGeometry == GeometryMode::UnitQuad) {
      mDevice->SetVertexShader(VertexShaderID::TexturedQuad);
      mDevice->SetPixelShader(PixelShaderID::TexturedQuadRGBA);
    } else {
      mDevice->SetVertexShader(VertexShaderID::TexturedVertex);
      mDevice->SetPixelShader(PixelShaderID::TexturedVertexRGBA);
    }
  }

  mDevice->SetPSTexture(0, mSource->GetTexture());
  mDevice->SetSamplerMode(kDefaultSamplerSlot, SamplerMode::LinearClamp);
}

void
RenderViewPass::ExecuteRendering()
{
  if (mAssignedLayer->NeedsSurfaceCopy()) {
    RenderWithBackdropCopy();
    return;
  }

  TexturedRenderPass::ExecuteRendering();
}

void
RenderViewPass::RenderWithBackdropCopy()
{
  MOZ_ASSERT(mAssignedLayer->NeedsSurfaceCopy());

  DebugOnly<Matrix> transform2d;
  const Matrix4x4& transform = mAssignedLayer->GetEffectiveTransform();
  MOZ_ASSERT(transform.Is2D(&transform2d) &&
             !gfx::ThebesMatrix(transform2d).HasNonIntegerTranslation());

  IntPoint translation = IntPoint::Truncate(transform._41, transform._42);

  RenderViewMLGPU* childView = mAssignedLayer->GetRenderView();

  IntRect visible = mAssignedLayer->GetRenderRegion().GetBounds().ToUnknownRect();
  IntRect sourceRect = visible + translation - mParentView->GetTargetOffset();
  IntPoint destPoint = visible.TopLeft() - childView->GetTargetOffset();

  RefPtr<MLGTexture> dest = mAssignedLayer->GetRenderTarget()->GetTexture();
  RefPtr<MLGTexture> source = mParentView->GetRenderTarget()->GetTexture();

  // Clamp the source rect to the source texture size.
  sourceRect = sourceRect.Intersect(IntRect(IntPoint(0, 0), source->GetSize()));

  // Clamp the source rect to the destination texture size.
  IntRect destRect(destPoint, sourceRect.Size());
  destRect = destRect.Intersect(IntRect(IntPoint(0, 0), dest->GetSize()));
  sourceRect = sourceRect.Intersect(IntRect(sourceRect.TopLeft(), destRect.Size()));

  mDevice->CopyTexture(dest, destPoint, source, sourceRect);
  childView->RenderAfterBackdropCopy();
  mParentView->RestoreDeviceState();
  TexturedRenderPass::ExecuteRendering();
}

} // namespace layers
} // namespace mozilla
