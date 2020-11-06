/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorD3D11SWGL.h"

#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/webrender/RenderD3D11TextureHost.h"

namespace mozilla {
using namespace layers;

namespace wr {

UniquePtr<RenderCompositor> RenderCompositorD3D11SWGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget, nsACString& aError) {
  void* ctx = wr_swgl_create_context();
  if (!ctx) {
    gfxCriticalNote << "Failed SWGL context creation for WebRender";
    return nullptr;
  }

  RefPtr<CompositorD3D11> compositor =
      MakeAndAddRef<CompositorD3D11>(nullptr, aWidget);
  nsCString log;
  if (!compositor->Initialize(&log)) {
    gfxCriticalNote << "Failed to initialize CompositorD3D11 for SWGL: "
                    << log.get();
    return nullptr;
  }

  return MakeUnique<RenderCompositorD3D11SWGL>(compositor, std::move(aWidget),
                                               ctx);
}

RenderCompositorD3D11SWGL::RenderCompositorD3D11SWGL(
    CompositorD3D11* aCompositor, RefPtr<widget::CompositorWidget>&& aWidget,
    void* aContext)
    : RenderCompositor(std::move(aWidget)),
      mCompositor(aCompositor),
      mContext(aContext) {
  MOZ_ASSERT(mContext);
  mSyncObject = mCompositor->GetSyncObject();
}

RenderCompositorD3D11SWGL::~RenderCompositorD3D11SWGL() {
  wr_swgl_destroy_context(mContext);
}

bool RenderCompositorD3D11SWGL::MakeCurrent() {
  wr_swgl_make_current(mContext);
  return true;
}

bool RenderCompositorD3D11SWGL::BeginFrame() {
  MakeCurrent();
  IntRect rect = IntRect(IntPoint(0, 0), GetBufferSize().ToUnknownSize());
  mCompositor->BeginFrameForWindow(nsIntRegion(rect), Nothing(), rect,
                                   nsIntRegion());
  return true;
}

void RenderCompositorD3D11SWGL::CancelFrame() {
  mFrameSurfaces.Clear();
  mCompositor->CancelFrame();
}

void RenderCompositorD3D11SWGL::CompositorEndFrame() {
  for (auto& frameSurface : mFrameSurfaces) {
    auto surfaceCursor = mSurfaces.find(frameSurface.mId);
    MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
    Surface& surface = surfaceCursor->second;

    for (auto it = surface.mTiles.begin(); it != surface.mTiles.end(); ++it) {
      gfx::Point tileOffset(it->first.mX * surface.mTileSize.width,
                            it->first.mY * surface.mTileSize.height);
      gfx::Rect drawRect = it->second.mValidRect + tileOffset;

      RefPtr<TexturedEffect> texturedEffect = CreateTexturedEffect(
          surface.mIsOpaque ? SurfaceFormat::B8G8R8X8 : SurfaceFormat::B8G8R8A8,
          it->second.mTexture, frameSurface.mFilter, true);

      texturedEffect->mTextureCoords =
          gfx::Rect(it->second.mValidRect.x / surface.mTileSize.width,
                    it->second.mValidRect.y / surface.mTileSize.height,
                    it->second.mValidRect.width / surface.mTileSize.width,
                    it->second.mValidRect.height / surface.mTileSize.height);

      EffectChain effect;
      effect.mPrimaryEffect = texturedEffect;
      mCompositor->DrawQuad(drawRect, frameSurface.mClipRect, effect, 1.0,
                            frameSurface.mTransform, drawRect);
    }

    if (surface.mExternalImage) {
      // We need to hold the texture source separately from the effect,
      // since the effect doesn't hold a strong reference.
      RefPtr<DataTextureSourceD3D11> layer;
      RefPtr<TexturedEffect> texturedEffect;
      gfx::IntSize size;
      if (auto* host = surface.mExternalImage->AsRenderDXGITextureHost()) {
        host->EnsureD3D11Texture2D(mCompositor->GetDevice());

        layer = new DataTextureSourceD3D11(mCompositor->GetDevice(),
                                           host->GetFormat(),
                                           host->GetD3D11Texture2D());
        if (host->GetFormat() == SurfaceFormat::NV12 ||
            host->GetFormat() == SurfaceFormat::P010 ||
            host->GetFormat() == SurfaceFormat::P016) {
          texturedEffect = new EffectNV12(
              layer, host->GetYUVColorSpace(), host->GetColorRange(),
              host->GetColorDepth(), frameSurface.mFilter);
        } else {
          MOZ_ASSERT(host->GetFormat() == SurfaceFormat::B8G8R8X8 ||
                     host->GetFormat() == SurfaceFormat::B8G8R8A8);
          texturedEffect = CreateTexturedEffect(host->GetFormat(), layer,
                                                frameSurface.mFilter, true);
        }
        size = host->GetSize(0);
        host->LockInternal();
      } else if (auto* host =
                     surface.mExternalImage->AsRenderDXGIYCbCrTextureHost()) {
        host->EnsureD3D11Texture2D(mCompositor->GetDevice());

        layer = new DataTextureSourceD3D11(mCompositor->GetDevice(),
                                           SurfaceFormat::A8,
                                           host->GetD3D11Texture2D(0));
        RefPtr<DataTextureSourceD3D11> u = new DataTextureSourceD3D11(
            mCompositor->GetDevice(), SurfaceFormat::A8,
            host->GetD3D11Texture2D(1));
        layer->SetNextSibling(u);
        RefPtr<DataTextureSourceD3D11> v = new DataTextureSourceD3D11(
            mCompositor->GetDevice(), SurfaceFormat::A8,
            host->GetD3D11Texture2D(2));
        u->SetNextSibling(v);

        texturedEffect = new EffectYCbCr(
            layer, host->GetYUVColorSpace(), host->GetColorRange(),
            host->GetColorDepth(), frameSurface.mFilter);
        size = host->GetSize(0);
        host->LockInternal();
      }

      gfx::Rect drawRect(0, 0, size.width, size.height);

      EffectChain effect;
      effect.mPrimaryEffect = texturedEffect;
      mCompositor->DrawQuad(drawRect, frameSurface.mClipRect, effect, 1.0,
                            frameSurface.mTransform, drawRect);

      if (auto* host = surface.mExternalImage->AsRenderDXGITextureHost()) {
        host->Unlock();
      } else if (auto* host =
                     surface.mExternalImage->AsRenderDXGIYCbCrTextureHost()) {
        host->Unlock();
      }
    }
  }
  mFrameSurfaces.Clear();
}

RenderedFrameId RenderCompositorD3D11SWGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  mCompositor->EndFrame();
  return GetNextRenderFrameId();
}

void RenderCompositorD3D11SWGL::Pause() {}

bool RenderCompositorD3D11SWGL::Resume() { return true; }

LayoutDeviceIntSize RenderCompositorD3D11SWGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

CompositorCapabilities RenderCompositorD3D11SWGL::GetCompositorCapabilities() {
  CompositorCapabilities caps;

  caps.virtual_surface_size = 0;
  return caps;
}

void RenderCompositorD3D11SWGL::Bind(wr::NativeTileId aId,
                                     wr::DeviceIntPoint* aOffset,
                                     uint32_t* aFboId,
                                     wr::DeviceIntRect aDirtyRect,
                                     wr::DeviceIntRect aValidRect) {
  MOZ_RELEASE_ASSERT(false);
}

void RenderCompositorD3D11SWGL::Unbind() { MOZ_RELEASE_ASSERT(false); }

bool RenderCompositorD3D11SWGL::MapTile(wr::NativeTileId aId,
                                        wr::DeviceIntRect aDirtyRect,
                                        wr::DeviceIntRect aValidRect,
                                        void** aData, int32_t* aStride) {
  auto surfaceCursor = mSurfaces.find(aId.surface_id);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;

  auto layerCursor = surface.mTiles.find(TileKey(aId.x, aId.y));
  MOZ_RELEASE_ASSERT(layerCursor != surface.mTiles.end());
  mCurrentTile = layerCursor->second;
  mCurrentTileDirty = IntRect(aDirtyRect.origin.x, aDirtyRect.origin.y,
                              aDirtyRect.size.width, aDirtyRect.size.height);

  DataSourceSurface::MappedSurface map;
  if (!mCurrentTile.mSurface->Map(DataSourceSurface::READ_WRITE, &map)) {
    return false;
  }

  *aData =
      map.mData + aValidRect.origin.y * map.mStride + aValidRect.origin.x * 4;
  *aStride = map.mStride;
  layerCursor->second.mValidRect =
      gfx::Rect(aValidRect.origin.x, aValidRect.origin.y, aValidRect.size.width,
                aValidRect.size.height);
  return true;
}

void RenderCompositorD3D11SWGL::UnmapTile() {
  mCurrentTile.mSurface->Unmap();
  nsIntRegion dirty(mCurrentTileDirty);
  mCurrentTile.mTexture->Update(mCurrentTile.mSurface, &dirty);
}

void RenderCompositorD3D11SWGL::CreateSurface(wr::NativeSurfaceId aId,
                                              wr::DeviceIntPoint aVirtualOffset,
                                              wr::DeviceIntSize aTileSize,
                                              bool aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfaces.find(aId) == mSurfaces.end());
  mSurfaces.insert({aId, Surface{aTileSize, aIsOpaque}});
}

void RenderCompositorD3D11SWGL::CreateExternalSurface(wr::NativeSurfaceId aId,
                                                      bool aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfaces.find(aId) == mSurfaces.end());
  Surface surface{wr::DeviceIntSize{}, aIsOpaque};
  surface.mIsExternal = true;
  mSurfaces.insert({aId, std::move(surface)});
}

void RenderCompositorD3D11SWGL::DestroySurface(NativeSurfaceId aId) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  mSurfaces.erase(surfaceCursor);
}

void RenderCompositorD3D11SWGL::CreateTile(wr::NativeSurfaceId aId, int32_t aX,
                                           int32_t aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;
  MOZ_RELEASE_ASSERT(!surface.mIsExternal);

  RefPtr<DataTextureSourceD3D11> source =
      new DataTextureSourceD3D11(gfx::SurfaceFormat::B8G8R8A8, mCompositor,
                                 layers::TextureFlags::NO_FLAGS);
  RefPtr<DataSourceSurface> surf = Factory::CreateDataSourceSurface(
      IntSize(surface.mTileSize.width, surface.mTileSize.height),
      SurfaceFormat::B8G8R8A8);
  surface.mTiles.insert({TileKey(aX, aY), Tile{source, surf}});
}

void RenderCompositorD3D11SWGL::DestroyTile(wr::NativeSurfaceId aId, int32_t aX,
                                            int32_t aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;
  MOZ_RELEASE_ASSERT(!surface.mIsExternal);

  auto layerCursor = surface.mTiles.find(TileKey(aX, aY));
  MOZ_RELEASE_ASSERT(layerCursor != surface.mTiles.end());
  surface.mTiles.erase(layerCursor);
}

void RenderCompositorD3D11SWGL::AttachExternalImage(
    wr::NativeSurfaceId aId, wr::ExternalImageId aExternalImage) {
  RenderTextureHost* image =
      RenderThread::Get()->GetRenderTexture(aExternalImage);
  MOZ_RELEASE_ASSERT(image);
  MOZ_RELEASE_ASSERT(image->AsRenderDXGITextureHost() ||
                     image->AsRenderDXGIYCbCrTextureHost());

  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());

  Surface& surface = surfaceCursor->second;
  surface.mExternalImage = image;
  MOZ_RELEASE_ASSERT(surface.mTiles.empty());
  MOZ_RELEASE_ASSERT(surface.mIsExternal);
}

gfx::SamplingFilter ToSamplingFilter(wr::ImageRendering aImageRendering) {
  if (aImageRendering == wr::ImageRendering::Auto) {
    return gfx::SamplingFilter::LINEAR;
  }
  return gfx::SamplingFilter::POINT;
}

void RenderCompositorD3D11SWGL::AddSurface(
    wr::NativeSurfaceId aId, const wr::CompositorSurfaceTransform& aTransform,
    wr::DeviceIntRect aClipRect, wr::ImageRendering aImageRendering) {
  Matrix4x4 transform(
      aTransform.m11, aTransform.m12, aTransform.m13, aTransform.m14,
      aTransform.m21, aTransform.m22, aTransform.m23, aTransform.m24,
      aTransform.m31, aTransform.m32, aTransform.m33, aTransform.m34,
      aTransform.m41, aTransform.m42, aTransform.m43, aTransform.m44);

  gfx::IntRect clipRect(aClipRect.origin.x, aClipRect.origin.y,
                        aClipRect.size.width, aClipRect.size.height);

  mFrameSurfaces.AppendElement(FrameSurface{aId, transform, clipRect,
                                            ToSamplingFilter(aImageRendering)});
}

bool RenderCompositorD3D11SWGL::MaybeReadback(
    const gfx::IntSize& aReadbackSize, const wr::ImageFormat& aReadbackFormat,
    const Range<uint8_t>& aReadbackBuffer, bool* aNeedsYFlip) {
  MOZ_ASSERT(aReadbackFormat == wr::ImageFormat::BGRA8);

  auto stride =
      aReadbackSize.width * gfx::BytesPerPixel(SurfaceFormat::B8G8R8A8);
  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, &aReadbackBuffer[0], aReadbackSize, stride,
      SurfaceFormat::B8G8R8A8, false);
  if (!dt) {
    return false;
  }

  mCompositor->Readback(dt);
  return true;
}

}  // namespace wr
}  // namespace mozilla
