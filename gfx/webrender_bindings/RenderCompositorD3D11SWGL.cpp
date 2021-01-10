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
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/webrender/RenderD3D11TextureHost.h"
#include "RenderCompositorRecordedFrame.h"

namespace mozilla {
using namespace layers;

namespace wr {

RenderCompositorD3D11SWGL::UploadMode
RenderCompositorD3D11SWGL::GetUploadMode() {
  int mode = StaticPrefs::gfx_webrender_software_d3d11_upload_mode();
  switch (mode) {
    case 1:
      return Upload_Immediate;
    case 2:
      return Upload_Staging;
    case 3:
      return Upload_StagingNoBlock;
    case 4:
      return Upload_StagingPooled;
    default:
      return Upload_Staging;
  }
}

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
  compositor->UseForSoftwareWebRender();

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
  MOZ_ASSERT(!mInFrame);
  MakeCurrent();
  IntRect rect = IntRect(IntPoint(0, 0), GetBufferSize().ToUnknownSize());
  if (!mCompositor->BeginFrameForWindow(nsIntRegion(rect), Nothing(), rect,
                                        nsIntRegion())) {
    return false;
  }
  mInFrame = true;
  mUploadMode = GetUploadMode();
  return true;
}

void RenderCompositorD3D11SWGL::CancelFrame() {
  MOZ_ASSERT(mInFrame);
  mCompositor->CancelFrame();
  mInFrame = false;
}

void RenderCompositorD3D11SWGL::CompositorEndFrame() {
  nsTArray<FrameSurface> frameSurfaces = std::move(mFrameSurfaces);

  if (!mInFrame) {
    return;
  }

  for (auto& frameSurface : frameSurfaces) {
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
        if (!host->EnsureD3D11Texture2D(mCompositor->GetDevice())) {
          continue;
        }

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
        if (!host->EnsureD3D11Texture2D(mCompositor->GetDevice())) {
          continue;
        }

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
}

RenderedFrameId RenderCompositorD3D11SWGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  MOZ_ASSERT(mInFrame);
  mInFrame = false;
  mCompositor->EndFrame();
  return GetNextRenderFrameId();
}

void RenderCompositorD3D11SWGL::Pause() {}

bool RenderCompositorD3D11SWGL::Resume() { return true; }

LayoutDeviceIntSize RenderCompositorD3D11SWGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

GLenum RenderCompositorD3D11SWGL::IsContextLost(bool aForce) {
  // CompositorD3D11 uses ID3D11Device for composite. The device status needs to
  // be checked.
  auto reason = GetDevice()->GetDeviceRemovedReason();
  switch (reason) {
    case S_OK:
      return LOCAL_GL_NO_ERROR;
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
      NS_WARNING("Device reset due to system / different device");
      return LOCAL_GL_INNOCENT_CONTEXT_RESET_ARB;
    case DXGI_ERROR_DEVICE_HUNG:
    case DXGI_ERROR_DEVICE_RESET:
    case DXGI_ERROR_INVALID_CALL:
      gfxCriticalError() << "Device reset due to WR device: "
                         << gfx::hexa(reason);
      return LOCAL_GL_GUILTY_CONTEXT_RESET_ARB;
    default:
      gfxCriticalError() << "Device reset with WR device unexpected reason: "
                         << gfx::hexa(reason);
      return LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB;
  }
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

  // Check if this tile's upload method matches what we're using for this frame,
  // and if not then reallocate to fix it. Do this before we copy the struct
  // into mCurrentTile.
  if (mUploadMode == Upload_Immediate) {
    if (layerCursor->second.mStagingTexture) {
      MOZ_ASSERT(!layerCursor->second.mSurface);
      layerCursor->second.mStagingTexture = nullptr;
      layerCursor->second.mSurface = CreateStagingSurface(surface.TileSize());
    }
  } else {
    if (layerCursor->second.mSurface) {
      MOZ_ASSERT(!layerCursor->second.mStagingTexture);
      layerCursor->second.mSurface = nullptr;
      layerCursor->second.mStagingTexture =
          CreateStagingTexture(surface.TileSize());
    }
  }

  mCurrentTile = layerCursor->second;
  mCurrentTileId = aId;
  mCurrentTileDirty = IntRect(aDirtyRect.origin.x, aDirtyRect.origin.y,
                              aDirtyRect.size.width, aDirtyRect.size.height);

  if (mUploadMode == Upload_Immediate) {
    DataSourceSurface::MappedSurface map;
    if (!mCurrentTile.mSurface->Map(DataSourceSurface::READ_WRITE, &map)) {
      return false;
    }

    *aData =
        map.mData + aValidRect.origin.y * map.mStride + aValidRect.origin.x * 4;
    *aStride = map.mStride;
    layerCursor->second.mValidRect =
        gfx::Rect(aValidRect.origin.x, aValidRect.origin.y,
                  aValidRect.size.width, aValidRect.size.height);
    return true;
  }

  RefPtr<ID3D11DeviceContext> context;
  mCompositor->GetDevice()->GetImmediateContext(getter_AddRefs(context));

  D3D11_MAPPED_SUBRESOURCE mappedSubresource;

  bool shouldBlock = mUploadMode == Upload_Staging;

  HRESULT hr = context->Map(
      mCurrentTile.mStagingTexture, 0, D3D11_MAP_READ_WRITE,
      shouldBlock ? 0 : D3D11_MAP_FLAG_DO_NOT_WAIT, &mappedSubresource);
  if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
    // mCurrentTile is a copy of the real tile data, so we can just replace the
    // staging one with a temporary for this draw. The staging texture for the
    // real tile remains untouched, so we'll go back to using that for future
    // frames and discard the new one. In the future we could improve this by
    // having a pool of shared staging textures for all the tiles.

    // Mark the tile as having a temporary staging texture.
    mCurrentTile.mIsTemp = true;

    // Try grabbing a texture from the staging pool and see if we can use that.
    if (mUploadMode == Upload_StagingPooled && surface.mStagingPool.Length()) {
      mCurrentTile.mStagingTexture = surface.mStagingPool.ElementAt(0);
      surface.mStagingPool.RemoveElementAt(0);
      hr = context->Map(mCurrentTile.mStagingTexture, 0, D3D11_MAP_READ_WRITE,
                        D3D11_MAP_FLAG_DO_NOT_WAIT, &mappedSubresource);

      // If that failed, put it back into the pool (but at the end).
      if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
        surface.mStagingPool.AppendElement(mCurrentTile.mStagingTexture);
      }
    }

    // No staging textures, or we tried one and it was busy. Allocate a brand
    // new one instead.
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
      mCurrentTile.mStagingTexture = CreateStagingTexture(surface.TileSize());
      hr = context->Map(mCurrentTile.mStagingTexture, 0, D3D11_MAP_READ_WRITE,
                        0, &mappedSubresource);
    }
  }
  MOZ_ASSERT(SUCCEEDED(hr));

  // aData is expected to contain a pointer to the first pixel within the valid
  // rect, so take the mapped resource's data (which covers the full tile size)
  // and offset it by the top/left of the valid rect.
  *aData = (uint8_t*)mappedSubresource.pData +
           aValidRect.origin.y * mappedSubresource.RowPitch +
           aValidRect.origin.x * 4;
  *aStride = mappedSubresource.RowPitch;

  // Store the new valid rect, so that we can composite only those pixels
  layerCursor->second.mValidRect =
      gfx::Rect(aValidRect.origin.x, aValidRect.origin.y, aValidRect.size.width,
                aValidRect.size.height);
  return true;
}

void RenderCompositorD3D11SWGL::UnmapTile() {
  if (mCurrentTile.mSurface) {
    MOZ_ASSERT(mUploadMode == Upload_Immediate);
    mCurrentTile.mSurface->Unmap();
    nsIntRegion dirty(mCurrentTileDirty);
    // This uses UpdateSubresource, which blocks, so is likely implemented as a
    // memcpy into driver owned memory, followed by an async upload. The staging
    // method should avoid this extra copy, and is likely to be faster usually.
    // We could possible do this call on a background thread so that sw-wr can
    // start drawing the next tile while the memcpy is in progress.
    mCurrentTile.mTexture->Update(mCurrentTile.mSurface, &dirty);
    return;
  }

  RefPtr<ID3D11DeviceContext> context;
  mCompositor->GetDevice()->GetImmediateContext(getter_AddRefs(context));

  context->Unmap(mCurrentTile.mStagingTexture, 0);

  D3D11_BOX box;
  box.front = 0;
  box.back = 1;
  box.left = mCurrentTileDirty.X();
  box.top = mCurrentTileDirty.Y();
  box.right = mCurrentTileDirty.XMost();
  box.bottom = mCurrentTileDirty.YMost();

  context->CopySubresourceRegion(mCurrentTile.mTexture->GetD3D11Texture(), 0,
                                 mCurrentTileDirty.x, mCurrentTileDirty.y, 0,
                                 mCurrentTile.mStagingTexture, 0, &box);

  // If we allocated a temp staging texture for this tile, and we're running
  // in pooled mode, then consider adding it to the pool for later.
  if (mCurrentTile.mIsTemp && mUploadMode == Upload_StagingPooled) {
    auto surfaceCursor = mSurfaces.find(mCurrentTileId.surface_id);
    MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
    Surface& surface = surfaceCursor->second;

    static const uint32_t kMaxPoolSize = 5;
    if (surface.mStagingPool.Length() < kMaxPoolSize) {
      surface.mStagingPool.AppendElement(mCurrentTile.mStagingTexture);
    }
  }
  mCurrentTile.mStagingTexture = nullptr;
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

already_AddRefed<ID3D11Texture2D>
RenderCompositorD3D11SWGL::CreateStagingTexture(const gfx::IntSize aSize) {
  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width,
                             aSize.height, 1, 1);

  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;

  RefPtr<ID3D11Texture2D> cpuTexture;
  DebugOnly<HRESULT> hr = mCompositor->GetDevice()->CreateTexture2D(
      &desc, nullptr, getter_AddRefs(cpuTexture));
  MOZ_ASSERT(SUCCEEDED(hr));
  return cpuTexture.forget();
}

already_AddRefed<DataSourceSurface>
RenderCompositorD3D11SWGL::CreateStagingSurface(const gfx::IntSize aSize) {
  return Factory::CreateDataSourceSurface(aSize, SurfaceFormat::B8G8R8A8);
}

void RenderCompositorD3D11SWGL::CreateTile(wr::NativeSurfaceId aId, int32_t aX,
                                           int32_t aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface& surface = surfaceCursor->second;
  MOZ_RELEASE_ASSERT(!surface.mIsExternal);

  if (mUploadMode == Upload_Immediate) {
    RefPtr<DataTextureSourceD3D11> source =
        new DataTextureSourceD3D11(gfx::SurfaceFormat::B8G8R8A8, mCompositor,
                                   layers::TextureFlags::NO_FLAGS);
    RefPtr<DataSourceSurface> surf = CreateStagingSurface(surface.TileSize());
    surface.mTiles.insert({TileKey(aX, aY), Tile{source, nullptr, surf}});
    return;
  }

  MOZ_ASSERT(mUploadMode == Upload_Staging ||
             mUploadMode == Upload_StagingNoBlock ||
             mUploadMode == Upload_StagingPooled);

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             surface.mTileSize.width, surface.mTileSize.height,
                             1, 1);

  RefPtr<ID3D11Texture2D> texture;
  DebugOnly<HRESULT> hr = mCompositor->GetDevice()->CreateTexture2D(
      &desc, nullptr, getter_AddRefs(texture));
  MOZ_ASSERT(SUCCEEDED(hr));

  RefPtr<DataTextureSourceD3D11> source = new DataTextureSourceD3D11(
      mCompositor->GetDevice(), gfx::SurfaceFormat::B8G8R8A8, texture);

  RefPtr<ID3D11Texture2D> cpuTexture = CreateStagingTexture(surface.TileSize());
  surface.mTiles.insert({TileKey(aX, aY), Tile{source, cpuTexture, nullptr}});
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

void RenderCompositorD3D11SWGL::MaybeRequestAllowFrameRecording(
    bool aWillRecord) {
  mCompositor->RequestAllowFrameRecording(aWillRecord);
}

bool RenderCompositorD3D11SWGL::MaybeRecordFrame(
    layers::CompositionRecorder& aRecorder) {
  layers::WindowLMC window(mCompositor);
  gfx::IntSize size = GetBufferSize().ToUnknownSize();
  RefPtr<layers::profiler_screenshots::RenderSource> snapshot =
      window.GetWindowContents(size);
  if (!snapshot) {
    return true;
  }

  RefPtr<layers::profiler_screenshots::AsyncReadbackBuffer> buffer =
      window.CreateAsyncReadbackBuffer(size);
  buffer->CopyFrom(snapshot);

  RefPtr<layers::RecordedFrame> frame =
      new RenderCompositorRecordedFrame(TimeStamp::Now(), std::move(buffer));
  aRecorder.RecordFrame(frame);
  return false;
}

bool RenderCompositorD3D11SWGL::MaybeGrabScreenshot(
    const gfx::IntSize& aWindowSize) {
  layers::WindowLMC window(mCompositor);
  mProfilerScreenshotGrabber.MaybeGrabScreenshot(window, aWindowSize);
  return true;
}

bool RenderCompositorD3D11SWGL::MaybeProcessScreenshotQueue() {
  mProfilerScreenshotGrabber.MaybeProcessQueue();
  return true;
}

}  // namespace wr
}  // namespace mozilla
