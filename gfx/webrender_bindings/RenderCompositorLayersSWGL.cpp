/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorLayersSWGL.h"

#include "GLContext.h"
#include "GLContextEGL.h"
#include "mozilla/layers/BuildConstants.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "RenderCompositorRecordedFrame.h"

#if defined(XP_WIN)
#  include "mozilla/webrender/RenderCompositorD3D11SWGL.h"
#else
#  include "mozilla/webrender/RenderCompositorOGLSWGL.h"
#endif

namespace mozilla {
using namespace layers;

namespace wr {

UniquePtr<RenderCompositor> RenderCompositorLayersSWGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget, nsACString& aError) {
#ifdef XP_WIN
  return RenderCompositorD3D11SWGL::Create(std::move(aWidget), aError);
#else
  return RenderCompositorOGLSWGL::Create(std::move(aWidget), aError);
#endif
}

RenderCompositorLayersSWGL::RenderCompositorLayersSWGL(
    Compositor* aCompositor, RefPtr<widget::CompositorWidget>&& aWidget,
    void* aContext)
    : RenderCompositor(std::move(aWidget)),
      mCompositor(aCompositor),
      mContext(aContext) {
  MOZ_ASSERT(mCompositor);
  MOZ_ASSERT(mContext);
}

RenderCompositorLayersSWGL::~RenderCompositorLayersSWGL() {
  wr_swgl_destroy_context(mContext);
}

bool RenderCompositorLayersSWGL::MakeCurrent() {
  wr_swgl_make_current(mContext);
  return true;
}

bool RenderCompositorLayersSWGL::BeginFrame() {
  MOZ_ASSERT(!mInFrame);
  MakeCurrent();
  gfx::IntRect rect =
      gfx::IntRect(gfx::IntPoint(0, 0), GetBufferSize().ToUnknownSize());
  if (!mCompositor->BeginFrameForWindow(nsIntRegion(rect), Nothing(), rect,
                                        nsIntRegion())) {
    return false;
  }
  mInFrame = true;
  return true;
}

void RenderCompositorLayersSWGL::CancelFrame() {
  MOZ_ASSERT(mInFrame);
  mCompositor->CancelFrame();
  mInFrame = false;
  mCompositingStarted = false;
}

void RenderCompositorLayersSWGL::StartCompositing(
    const wr::DeviceIntRect* aDirtyRects, size_t aNumDirtyRects,
    const wr::DeviceIntRect* aOpaqueRects, size_t aNumOpaqueRects) {
  mCompositingStarted = mInFrame;
}

void RenderCompositorLayersSWGL::CompositorEndFrame() {
  nsTArray<FrameSurface> frameSurfaces = std::move(mFrameSurfaces);

  if (!mInFrame) {
    return;
  }

  for (auto& frameSurface : frameSurfaces) {
    auto surfaceCursor = mSurfaces.find(frameSurface.mId);
    MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
    Surface* surface = surfaceCursor->second.get();

    for (auto it = surface->mTiles.begin(); it != surface->mTiles.end(); ++it) {
      if (!it->second->IsValid()) {
        continue;
      }

      gfx::Point tileOffset(it->first.mX * surface->mTileSize.width,
                            it->first.mY * surface->mTileSize.height);
      gfx::Rect drawRect = it->second->mValidRect + tileOffset;

      RefPtr<TexturedEffect> texturedEffect = CreateTexturedEffect(
          surface->mIsOpaque ? gfx::SurfaceFormat::B8G8R8X8
                             : gfx::SurfaceFormat::B8G8R8A8,
          it->second->GetTextureSource(), frameSurface.mFilter, true);

      texturedEffect->mTextureCoords =
          gfx::Rect(it->second->mValidRect.x / surface->mTileSize.width,
                    it->second->mValidRect.y / surface->mTileSize.height,
                    it->second->mValidRect.width / surface->mTileSize.width,
                    it->second->mValidRect.height / surface->mTileSize.height);

      EffectChain effect;
      effect.mPrimaryEffect = texturedEffect;
      mCompositor->DrawQuad(drawRect, frameSurface.mClipRect, effect, 1.0,
                            frameSurface.mTransform, drawRect);
    }

    if (surface->mExternalImage) {
      HandleExternalImage(surface->mExternalImage, frameSurface);
    }
  }
}

RenderedFrameId RenderCompositorLayersSWGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  MOZ_ASSERT(mInFrame);
  mInFrame = false;
  if (mCompositingStarted) {
    mCompositor->EndFrame();
    mCompositingStarted = false;
  } else {
    mCompositor->CancelFrame();
  }
  return GetNextRenderFrameId();
}

LayoutDeviceIntSize RenderCompositorLayersSWGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

void RenderCompositorLayersSWGL::Bind(wr::NativeTileId aId,
                                      wr::DeviceIntPoint* aOffset,
                                      uint32_t* aFboId,
                                      wr::DeviceIntRect aDirtyRect,
                                      wr::DeviceIntRect aValidRect) {
  MOZ_RELEASE_ASSERT(false);
}

void RenderCompositorLayersSWGL::Unbind() { MOZ_RELEASE_ASSERT(false); }

bool RenderCompositorLayersSWGL::MapTile(wr::NativeTileId aId,
                                         wr::DeviceIntRect aDirtyRect,
                                         wr::DeviceIntRect aValidRect,
                                         void** aData, int32_t* aStride) {
  auto surfaceCursor = mSurfaces.find(aId.surface_id);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface* surface = surfaceCursor->second.get();

  auto layerCursor = surface->mTiles.find(TileKey(aId.x, aId.y));
  MOZ_RELEASE_ASSERT(layerCursor != surface->mTiles.end());

  mCurrentTile = layerCursor->second.get();
  mCurrentTileId = aId;
  mCurrentTileDirty =
      gfx::IntRect(aDirtyRect.origin.x, aDirtyRect.origin.y,
                   aDirtyRect.size.width, aDirtyRect.size.height);

  if (!mCurrentTile->Map(aDirtyRect, aValidRect, aData, aStride)) {
    return false;
  }

  // Store the new valid rect, so that we can composite only those pixels
  mCurrentTile->mValidRect =
      gfx::Rect(aValidRect.origin.x, aValidRect.origin.y, aValidRect.size.width,
                aValidRect.size.height);
  return true;
}

void RenderCompositorLayersSWGL::UnmapTile() {
  mCurrentTile->Unmap(mCurrentTileDirty);
  mCurrentTile = nullptr;
}

void RenderCompositorLayersSWGL::CreateSurface(
    wr::NativeSurfaceId aId, wr::DeviceIntPoint aVirtualOffset,
    wr::DeviceIntSize aTileSize, bool aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfaces.find(aId) == mSurfaces.end());
  auto surface = DoCreateSurface(aTileSize, aIsOpaque);
  mSurfaces.insert({aId, std::move(surface)});
}

UniquePtr<RenderCompositorLayersSWGL::Surface>
RenderCompositorLayersSWGL::DoCreateSurface(wr::DeviceIntSize aTileSize,
                                            bool aIsOpaque) {
  return MakeUnique<Surface>(aTileSize, aIsOpaque);
}

void RenderCompositorLayersSWGL::CreateExternalSurface(wr::NativeSurfaceId aId,
                                                       bool aIsOpaque) {
  MOZ_RELEASE_ASSERT(mSurfaces.find(aId) == mSurfaces.end());
  auto surface = MakeUnique<Surface>(wr::DeviceIntSize{}, aIsOpaque);
  surface->mIsExternal = true;
  mSurfaces.insert({aId, std::move(surface)});
}

void RenderCompositorLayersSWGL::DestroySurface(NativeSurfaceId aId) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  mSurfaces.erase(surfaceCursor);
}

void RenderCompositorLayersSWGL::CreateTile(wr::NativeSurfaceId aId, int32_t aX,
                                            int32_t aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface* surface = surfaceCursor->second.get();
  MOZ_RELEASE_ASSERT(!surface->mIsExternal);

  auto tile = DoCreateTile(surface);
  surface->mTiles.insert({TileKey(aX, aY), std::move(tile)});
}

void RenderCompositorLayersSWGL::DestroyTile(wr::NativeSurfaceId aId,
                                             int32_t aX, int32_t aY) {
  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());
  Surface* surface = surfaceCursor->second.get();
  MOZ_RELEASE_ASSERT(!surface->mIsExternal);

  auto layerCursor = surface->mTiles.find(TileKey(aX, aY));
  MOZ_RELEASE_ASSERT(layerCursor != surface->mTiles.end());
  surface->mTiles.erase(layerCursor);
}

void RenderCompositorLayersSWGL::AttachExternalImage(
    wr::NativeSurfaceId aId, wr::ExternalImageId aExternalImage) {
  RenderTextureHost* image =
      RenderThread::Get()->GetRenderTexture(aExternalImage);
  MOZ_ASSERT(image);
  if (!image) {
    gfxCriticalNoteOnce
        << "Failed to get RenderTextureHost for D3D11SWGL extId:"
        << AsUint64(aExternalImage);
    return;
  }
#if defined(XP_WIN)
  MOZ_RELEASE_ASSERT(image->AsRenderDXGITextureHost() ||
                     image->AsRenderDXGIYCbCrTextureHost());
#elif defined(ANDROID)
  MOZ_RELEASE_ASSERT(image->AsRenderAndroidSurfaceTextureHost());
#endif

  auto surfaceCursor = mSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surfaceCursor != mSurfaces.end());

  Surface* surface = surfaceCursor->second.get();
  surface->mExternalImage = image;
  MOZ_RELEASE_ASSERT(surface->mTiles.empty());
  MOZ_RELEASE_ASSERT(surface->mIsExternal);
}

// static
gfx::SamplingFilter RenderCompositorLayersSWGL::ToSamplingFilter(
    wr::ImageRendering aImageRendering) {
  if (aImageRendering == wr::ImageRendering::Auto) {
    return gfx::SamplingFilter::LINEAR;
  }
  return gfx::SamplingFilter::POINT;
}

void RenderCompositorLayersSWGL::AddSurface(
    wr::NativeSurfaceId aId, const wr::CompositorSurfaceTransform& aTransform,
    wr::DeviceIntRect aClipRect, wr::ImageRendering aImageRendering) {
  gfx::Matrix4x4 transform(
      aTransform.m11, aTransform.m12, aTransform.m13, aTransform.m14,
      aTransform.m21, aTransform.m22, aTransform.m23, aTransform.m24,
      aTransform.m31, aTransform.m32, aTransform.m33, aTransform.m34,
      aTransform.m41, aTransform.m42, aTransform.m43, aTransform.m44);

  gfx::IntRect clipRect(aClipRect.origin.x, aClipRect.origin.y,
                        aClipRect.size.width, aClipRect.size.height);

  mFrameSurfaces.AppendElement(FrameSurface{aId, transform, clipRect,
                                            ToSamplingFilter(aImageRendering)});
}

void RenderCompositorLayersSWGL::MaybeRequestAllowFrameRecording(
    bool aWillRecord) {
  mCompositor->RequestAllowFrameRecording(aWillRecord);
}

bool RenderCompositorLayersSWGL::MaybeRecordFrame(
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

bool RenderCompositorLayersSWGL::MaybeGrabScreenshot(
    const gfx::IntSize& aWindowSize) {
  layers::WindowLMC window(mCompositor);
  mProfilerScreenshotGrabber.MaybeGrabScreenshot(window, aWindowSize);
  return true;
}

bool RenderCompositorLayersSWGL::MaybeProcessScreenshotQueue() {
  mProfilerScreenshotGrabber.MaybeProcessQueue();
  return true;
}

}  // namespace wr
}  // namespace mozilla
