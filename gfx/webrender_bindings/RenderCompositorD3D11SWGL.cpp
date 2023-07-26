/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorD3D11SWGL.h"

#include "gfxConfig.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/webrender/RenderD3D11TextureHost.h"
#include "RenderCompositorRecordedFrame.h"
#include "RenderThread.h"

namespace mozilla {
using namespace layers;

namespace wr {

extern LazyLogModule gRenderThreadLog;
#define LOG(...) MOZ_LOG(gRenderThreadLog, LogLevel::Debug, (__VA_ARGS__))

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
    const RefPtr<widget::CompositorWidget>& aWidget, nsACString& aError) {
  if (!aWidget->GetCompositorOptions().AllowSoftwareWebRenderD3D11() ||
      !gfx::gfxConfig::IsEnabled(gfx::Feature::D3D11_COMPOSITING)) {
    return nullptr;
  }

  void* ctx = wr_swgl_create_context();
  if (!ctx) {
    gfxCriticalNote << "Failed SWGL context creation for WebRender";
    return nullptr;
  }

  RefPtr<CompositorD3D11> compositor = MakeAndAddRef<CompositorD3D11>(aWidget);
  nsCString log;
  if (!compositor->Initialize(&log)) {
    gfxCriticalNote << "Failed to initialize CompositorD3D11 for SWGL: "
                    << log.get();
    return nullptr;
  }
  return MakeUnique<RenderCompositorD3D11SWGL>(compositor, aWidget, ctx);
}

RenderCompositorD3D11SWGL::RenderCompositorD3D11SWGL(
    CompositorD3D11* aCompositor,
    const RefPtr<widget::CompositorWidget>& aWidget, void* aContext)
    : RenderCompositorLayersSWGL(aCompositor, aWidget, aContext) {
  LOG("RenderCompositorD3D11SWGL::RenderCompositorD3D11SWGL()");

  mSyncObject = GetCompositorD3D11()->GetSyncObject();
}

RenderCompositorD3D11SWGL::~RenderCompositorD3D11SWGL() {
  LOG("RenderCompositorD3D11SWGL::~RenderCompositorD3D11SWGL()");
}

bool RenderCompositorD3D11SWGL::BeginFrame() {
  if (!RenderCompositorLayersSWGL::BeginFrame()) {
    return false;
  }

  mUploadMode = GetUploadMode();
  return true;
}

void RenderCompositorD3D11SWGL::HandleExternalImage(
    RenderTextureHost* aExternalImage, FrameSurface& aFrameSurface) {
  // We need to hold the texture source separately from the effect,
  // since the effect doesn't hold a strong reference.
  RefPtr<DataTextureSourceD3D11> layer;
  RefPtr<TexturedEffect> texturedEffect;
  gfx::IntSize size;
  if (auto* host = aExternalImage->AsRenderDXGITextureHost()) {
    if (!host->EnsureD3D11Texture2D(GetDevice())) {
      return;
    }

    layer = new DataTextureSourceD3D11(GetDevice(), host->GetFormat(),
                                       host->GetD3D11Texture2D());
    if (host->GetFormat() == gfx::SurfaceFormat::NV12 ||
        host->GetFormat() == gfx::SurfaceFormat::P010 ||
        host->GetFormat() == gfx::SurfaceFormat::P016) {
      const auto yuv = FromYUVRangedColorSpace(host->GetYUVColorSpace());
      texturedEffect =
          new EffectNV12(layer, yuv.space, yuv.range, host->GetColorDepth(),
                         aFrameSurface.mFilter);
    } else {
      MOZ_ASSERT(host->GetFormat() == gfx::SurfaceFormat::B8G8R8X8 ||
                 host->GetFormat() == gfx::SurfaceFormat::B8G8R8A8);
      texturedEffect = CreateTexturedEffect(host->GetFormat(), layer,
                                            aFrameSurface.mFilter, true);
    }
    size = host->GetSize(0);
    host->LockInternal();
  } else if (auto* host = aExternalImage->AsRenderDXGIYCbCrTextureHost()) {
    if (!host->EnsureD3D11Texture2D(GetDevice())) {
      return;
    }

    layer = new DataTextureSourceD3D11(GetDevice(), gfx::SurfaceFormat::A8,
                                       host->GetD3D11Texture2D(0));
    RefPtr<DataTextureSourceD3D11> u = new DataTextureSourceD3D11(
        GetDevice(), gfx::SurfaceFormat::A8, host->GetD3D11Texture2D(1));
    layer->SetNextSibling(u);
    RefPtr<DataTextureSourceD3D11> v = new DataTextureSourceD3D11(
        GetDevice(), gfx::SurfaceFormat::A8, host->GetD3D11Texture2D(2));
    u->SetNextSibling(v);

    const auto yuv = FromYUVRangedColorSpace(host->GetYUVColorSpace());
    texturedEffect =
        new EffectYCbCr(layer, yuv.space, yuv.range, host->GetColorDepth(),
                        aFrameSurface.mFilter);
    size = host->GetSize(0);
    host->LockInternal();
  }

  gfx::Rect drawRect(0, 0, size.width, size.height);

  EffectChain effect;
  effect.mPrimaryEffect = texturedEffect;
  mCompositor->DrawQuad(drawRect, aFrameSurface.mClipRect, effect, 1.0,
                        aFrameSurface.mTransform, drawRect);

  if (auto* host = aExternalImage->AsRenderDXGITextureHost()) {
    host->Unlock();
  } else if (auto* host = aExternalImage->AsRenderDXGIYCbCrTextureHost()) {
    host->Unlock();
  }
}

void RenderCompositorD3D11SWGL::Pause() {}

bool RenderCompositorD3D11SWGL::Resume() { return true; }

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

UniquePtr<RenderCompositorLayersSWGL::Surface>
RenderCompositorD3D11SWGL::DoCreateSurface(wr::DeviceIntSize aTileSize,
                                           bool aIsOpaque) {
  return MakeUnique<SurfaceD3D11SWGL>(aTileSize, aIsOpaque);
}

SurfaceD3D11SWGL::SurfaceD3D11SWGL(wr::DeviceIntSize aTileSize, bool aIsOpaque)
    : Surface(aTileSize, aIsOpaque) {}

RenderCompositorD3D11SWGL::TileD3D11::TileD3D11(
    layers::DataTextureSourceD3D11* aTexture, ID3D11Texture2D* aStagingTexture,
    gfx::DataSourceSurface* aDataSourceSurface, Surface* aOwner,
    RenderCompositorD3D11SWGL* aRenderCompositor)
    : Tile(),
      mTexture(aTexture),
      mStagingTexture(aStagingTexture),
      mSurface(aDataSourceSurface),
      mOwner(aOwner->AsSurfaceD3D11SWGL()),
      mRenderCompositor(aRenderCompositor) {}

bool RenderCompositorD3D11SWGL::TileD3D11::Map(wr::DeviceIntRect aDirtyRect,
                                               wr::DeviceIntRect aValidRect,
                                               void** aData, int32_t* aStride) {
  const UploadMode uploadMode = mRenderCompositor->GetUploadMode();
  const gfx::IntSize tileSize = mOwner->TileSize();

  if (!IsValid()) {
    return false;
  }

  // Check if this tile's upload method matches what we're using for this frame,
  // and if not then reallocate to fix it. Do this before we copy the struct
  // into mCurrentTile.
  if (uploadMode == Upload_Immediate) {
    if (mStagingTexture) {
      MOZ_ASSERT(!mSurface);
      mStagingTexture = nullptr;
      mSurface = mRenderCompositor->CreateStagingSurface(tileSize);
    }
  } else {
    if (mSurface) {
      MOZ_ASSERT(!mStagingTexture);
      mSurface = nullptr;
      mStagingTexture = mRenderCompositor->CreateStagingTexture(tileSize);
    }
  }

  mRenderCompositor->mCurrentStagingTexture = mStagingTexture;
  mRenderCompositor->mCurrentStagingTextureIsTemp = false;

  if (uploadMode == Upload_Immediate) {
    gfx::DataSourceSurface::MappedSurface map;
    if (!mSurface->Map(gfx::DataSourceSurface::READ_WRITE, &map)) {
      return false;
    }

    *aData = map.mData + aValidRect.min.y * map.mStride + aValidRect.min.x * 4;
    *aStride = map.mStride;
    // Ensure our mapped data is accessible by writing to the beginning and end
    // of the dirty region. See bug 1717519
    if (aDirtyRect.width() > 0 && aDirtyRect.height() > 0) {
      uint32_t* probeData = (uint32_t*)map.mData +
                            aDirtyRect.min.y * (map.mStride / 4) +
                            aDirtyRect.min.x;
      *probeData = 0;
      uint32_t* probeDataEnd = (uint32_t*)map.mData +
                               (aDirtyRect.max.y - 1) * (map.mStride / 4) +
                               (aDirtyRect.max.x - 1);
      *probeDataEnd = 0;
    }

    mValidRect = gfx::Rect(aValidRect.min.x, aValidRect.min.y,
                           aValidRect.width(), aValidRect.height());
    return true;
  }

  if (!mRenderCompositor->mCurrentStagingTexture) {
    return false;
  }

  RefPtr<ID3D11DeviceContext> context;
  mRenderCompositor->GetDevice()->GetImmediateContext(getter_AddRefs(context));

  D3D11_MAPPED_SUBRESOURCE mappedSubresource;

  bool shouldBlock = uploadMode == Upload_Staging;

  HRESULT hr = context->Map(
      mRenderCompositor->mCurrentStagingTexture, 0, D3D11_MAP_READ_WRITE,
      shouldBlock ? 0 : D3D11_MAP_FLAG_DO_NOT_WAIT, &mappedSubresource);
  if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
    // mCurrentTile is a copy of the real tile data, so we can just replace the
    // staging one with a temporary for this draw. The staging texture for the
    // real tile remains untouched, so we'll go back to using that for future
    // frames and discard the new one. In the future we could improve this by
    // having a pool of shared staging textures for all the tiles.

    // Mark the tile as having a temporary staging texture.
    mRenderCompositor->mCurrentStagingTextureIsTemp = true;

    // Try grabbing a texture from the staging pool and see if we can use that.
    if (uploadMode == Upload_StagingPooled && mOwner->mStagingPool.Length()) {
      mRenderCompositor->mCurrentStagingTexture =
          mOwner->mStagingPool.ElementAt(0);
      mOwner->mStagingPool.RemoveElementAt(0);
      hr = context->Map(mRenderCompositor->mCurrentStagingTexture, 0,
                        D3D11_MAP_READ_WRITE, D3D11_MAP_FLAG_DO_NOT_WAIT,
                        &mappedSubresource);

      // If that failed, put it back into the pool (but at the end).
      if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
        mOwner->mStagingPool.AppendElement(
            mRenderCompositor->mCurrentStagingTexture);
      }
    }

    // No staging textures, or we tried one and it was busy. Allocate a brand
    // new one instead.
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
      mRenderCompositor->mCurrentStagingTexture =
          mRenderCompositor->CreateStagingTexture(tileSize);
      if (!mRenderCompositor->mCurrentStagingTexture) {
        return false;
      }
      hr = context->Map(mRenderCompositor->mCurrentStagingTexture, 0,
                        D3D11_MAP_READ_WRITE, 0, &mappedSubresource);
    }
  }
  if (!SUCCEEDED(hr)) {
    gfxCriticalError() << "Failed to map tile: " << gfx::hexa(hr);
    // This is only expected to fail if we hit a device reset.
    MOZ_RELEASE_ASSERT(
        mRenderCompositor->GetDevice()->GetDeviceRemovedReason() != S_OK);
    return false;
  }

  // aData is expected to contain a pointer to the first pixel within the valid
  // rect, so take the mapped resource's data (which covers the full tile size)
  // and offset it by the top/left of the valid rect.
  *aData = (uint8_t*)mappedSubresource.pData +
           aValidRect.min.y * mappedSubresource.RowPitch + aValidRect.min.x * 4;
  *aStride = mappedSubresource.RowPitch;

  // Ensure our mapped data is accessible by writing to the beginning and end
  // of the dirty region. See bug 1717519
  if (aDirtyRect.width() > 0 && aDirtyRect.height() > 0) {
    uint32_t* probeData = (uint32_t*)mappedSubresource.pData +
                          aDirtyRect.min.y * (mappedSubresource.RowPitch / 4) +
                          aDirtyRect.min.x;
    *probeData = 0;
    uint32_t* probeDataEnd =
        (uint32_t*)mappedSubresource.pData +
        (aDirtyRect.max.y - 1) * (mappedSubresource.RowPitch / 4) +
        (aDirtyRect.max.x - 1);
    *probeDataEnd = 0;
  }

  // Store the new valid rect, so that we can composite only those pixels
  mValidRect = gfx::Rect(aValidRect.min.x, aValidRect.min.y, aValidRect.width(),
                         aValidRect.height());

  return true;
}

void RenderCompositorD3D11SWGL::TileD3D11::Unmap(
    const gfx::IntRect& aDirtyRect) {
  const UploadMode uploadMode = mRenderCompositor->GetUploadMode();

  if (!IsValid()) {
    return;
  }

  if (mSurface) {
    MOZ_ASSERT(uploadMode == Upload_Immediate);
    mSurface->Unmap();
    nsIntRegion dirty(aDirtyRect);
    // This uses UpdateSubresource, which blocks, so is likely implemented as a
    // memcpy into driver owned memory, followed by an async upload. The staging
    // method should avoid this extra copy, and is likely to be faster usually.
    // We could possible do this call on a background thread so that sw-wr can
    // start drawing the next tile while the memcpy is in progress.
    mTexture->Update(mSurface, &dirty);
    return;
  }

  if (!mRenderCompositor->mCurrentStagingTexture) {
    return;
  }

  RefPtr<ID3D11DeviceContext> context;
  mRenderCompositor->GetDevice()->GetImmediateContext(getter_AddRefs(context));

  context->Unmap(mRenderCompositor->mCurrentStagingTexture, 0);

  D3D11_BOX box;
  box.front = 0;
  box.back = 1;
  box.left = aDirtyRect.X();
  box.top = aDirtyRect.Y();
  box.right = aDirtyRect.XMost();
  box.bottom = aDirtyRect.YMost();

  context->CopySubresourceRegion(
      mTexture->GetD3D11Texture(), 0, aDirtyRect.x, aDirtyRect.y, 0,
      mRenderCompositor->mCurrentStagingTexture, 0, &box);

  // If we allocated a temp staging texture for this tile, and we're running
  // in pooled mode, then consider adding it to the pool for later.
  if (mRenderCompositor->mCurrentStagingTextureIsTemp &&
      uploadMode == Upload_StagingPooled) {
    static const uint32_t kMaxPoolSize = 5;
    if (mOwner->mStagingPool.Length() < kMaxPoolSize) {
      mOwner->mStagingPool.AppendElement(
          mRenderCompositor->mCurrentStagingTexture);
    }
  }

  mRenderCompositor->mCurrentStagingTexture = nullptr;
  mRenderCompositor->mCurrentStagingTextureIsTemp = false;
}

bool RenderCompositorD3D11SWGL::TileD3D11::IsValid() { return !!mTexture; }

already_AddRefed<ID3D11Texture2D>
RenderCompositorD3D11SWGL::CreateStagingTexture(const gfx::IntSize aSize) {
  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width,
                             aSize.height, 1, 1);

  desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;

  RefPtr<ID3D11Texture2D> cpuTexture;
  DebugOnly<HRESULT> hr =
      GetDevice()->CreateTexture2D(&desc, nullptr, getter_AddRefs(cpuTexture));
  MOZ_ASSERT(SUCCEEDED(hr));
  if (!cpuTexture) {
    gfxCriticalNote << "Failed to create StagingTexture: " << aSize;
    RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
  }
  return cpuTexture.forget();
}

already_AddRefed<gfx::DataSourceSurface>
RenderCompositorD3D11SWGL::CreateStagingSurface(const gfx::IntSize aSize) {
  return gfx::Factory::CreateDataSourceSurface(aSize,
                                               gfx::SurfaceFormat::B8G8R8A8);
}

UniquePtr<RenderCompositorLayersSWGL::Tile>
RenderCompositorD3D11SWGL::DoCreateTile(Surface* aSurface) {
  MOZ_RELEASE_ASSERT(aSurface);

  const auto tileSize = aSurface->TileSize();

  if (mUploadMode == Upload_Immediate) {
    RefPtr<DataTextureSourceD3D11> source =
        new DataTextureSourceD3D11(gfx::SurfaceFormat::B8G8R8A8, mCompositor,
                                   layers::TextureFlags::NO_FLAGS);
    RefPtr<gfx::DataSourceSurface> surf = CreateStagingSurface(tileSize);
    return MakeUnique<TileD3D11>(source, nullptr, surf, aSurface, this);
  }

  MOZ_ASSERT(mUploadMode == Upload_Staging ||
             mUploadMode == Upload_StagingNoBlock ||
             mUploadMode == Upload_StagingPooled);

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, tileSize.width,
                             tileSize.height, 1, 1);

  RefPtr<ID3D11Texture2D> texture;
  DebugOnly<HRESULT> hr =
      GetDevice()->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  MOZ_ASSERT(SUCCEEDED(hr));
  if (!texture) {
    gfxCriticalNote << "Failed to allocate Texture2D: " << aSurface->TileSize();
    RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
    return MakeUnique<TileD3D11>(nullptr, nullptr, nullptr, aSurface, this);
  }

  RefPtr<DataTextureSourceD3D11> source = new DataTextureSourceD3D11(
      GetDevice(), gfx::SurfaceFormat::B8G8R8A8, texture);

  RefPtr<ID3D11Texture2D> cpuTexture = CreateStagingTexture(tileSize);
  return MakeUnique<TileD3D11>(source, cpuTexture, nullptr, aSurface, this);
}

bool RenderCompositorD3D11SWGL::MaybeReadback(
    const gfx::IntSize& aReadbackSize, const wr::ImageFormat& aReadbackFormat,
    const Range<uint8_t>& aReadbackBuffer, bool* aNeedsYFlip) {
  MOZ_ASSERT(aReadbackFormat == wr::ImageFormat::BGRA8);

  auto stride =
      aReadbackSize.width * gfx::BytesPerPixel(gfx::SurfaceFormat::B8G8R8A8);
  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, &aReadbackBuffer[0], aReadbackSize, stride,
      gfx::SurfaceFormat::B8G8R8A8, false);
  if (!dt) {
    return false;
  }

  GetCompositorD3D11()->Readback(dt);
  return true;
}

}  // namespace wr
}  // namespace mozilla
