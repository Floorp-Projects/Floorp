/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCLayerTree.h"

#include "GLContext.h"
#include "GLContextEGL.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderD3D11TextureHost.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/Telemetry.h"
#include "nsPrintfCString.h"

#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINBLUE
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WINBLUE

#include <d3d11.h>
#include <d3d11_1.h>
#include <dcomp.h>
#include <dxgi1_2.h>

namespace mozilla {
namespace wr {

/* static */
UniquePtr<DCLayerTree> DCLayerTree::Create(gl::GLContext* aGL,
                                           EGLConfig aEGLConfig,
                                           ID3D11Device* aDevice,
                                           ID3D11DeviceContext* aCtx,
                                           HWND aHwnd, nsACString& aError) {
  RefPtr<IDCompositionDevice2> dCompDevice =
      gfx::DeviceManagerDx::Get()->GetDirectCompositionDevice();
  if (!dCompDevice) {
    aError.Assign("DCLayerTree(no device)"_ns);
    return nullptr;
  }

  auto layerTree =
      MakeUnique<DCLayerTree>(aGL, aEGLConfig, aDevice, aCtx, dCompDevice);
  if (!layerTree->Initialize(aHwnd, aError)) {
    return nullptr;
  }

  return layerTree;
}

DCLayerTree::DCLayerTree(gl::GLContext* aGL, EGLConfig aEGLConfig,
                         ID3D11Device* aDevice, ID3D11DeviceContext* aCtx,
                         IDCompositionDevice2* aCompositionDevice)
    : mGL(aGL),
      mEGLConfig(aEGLConfig),
      mDevice(aDevice),
      mCtx(aCtx),
      mCompositionDevice(aCompositionDevice),
      mVideoOverlaySupported(false),
      mDebugCounter(false),
      mDebugVisualRedrawRegions(false),
      mEGLImage(EGL_NO_IMAGE),
      mColorRBO(0),
      mPendingCommit(false) {}

DCLayerTree::~DCLayerTree() { ReleaseNativeCompositorResources(); }

void DCLayerTree::ReleaseNativeCompositorResources() {
  const auto gl = GetGLContext();

  DestroyEGLSurface();

  // Delete any cached FBO objects
  for (auto it = mFrameBuffers.begin(); it != mFrameBuffers.end(); ++it) {
    gl->fDeleteRenderbuffers(1, &it->depthRboId);
    gl->fDeleteFramebuffers(1, &it->fboId);
  }
}

bool DCLayerTree::Initialize(HWND aHwnd, nsACString& aError) {
  HRESULT hr;

  RefPtr<IDCompositionDesktopDevice> desktopDevice;
  hr = mCompositionDevice->QueryInterface(
      (IDCompositionDesktopDevice**)getter_AddRefs(desktopDevice));
  if (FAILED(hr)) {
    aError.Assign(nsPrintfCString(
        "DCLayerTree(get IDCompositionDesktopDevice failed %x)", hr));
    return false;
  }

  hr = desktopDevice->CreateTargetForHwnd(aHwnd, TRUE,
                                          getter_AddRefs(mCompositionTarget));
  if (FAILED(hr)) {
    aError.Assign(nsPrintfCString(
        "DCLayerTree(create DCompositionTarget failed %x)", hr));
    return false;
  }

  hr = mCompositionDevice->CreateVisual(getter_AddRefs(mRootVisual));
  if (FAILED(hr)) {
    aError.Assign(nsPrintfCString(
        "DCLayerTree(create root DCompositionVisual failed %x)", hr));
    return false;
  }

  hr =
      mCompositionDevice->CreateVisual(getter_AddRefs(mDefaultSwapChainVisual));
  if (FAILED(hr)) {
    aError.Assign(nsPrintfCString(
        "DCLayerTree(create swap chain DCompositionVisual failed %x)", hr));
    return false;
  }

  if (gfx::gfxVars::UseWebRenderDCompVideoOverlayWin()) {
    if (!InitializeVideoOverlaySupport()) {
      RenderThread::Get()->HandleWebRenderError(WebRenderError::VIDEO_OVERLAY);
    }
  }

  mCompositionTarget->SetRoot(mRootVisual);
  // Set interporation mode to nearest, to ensure 1:1 sampling.
  // By default, a visual inherits the interpolation mode of the parent visual.
  // If no visuals set the interpolation mode, the default for the entire visual
  // tree is nearest neighbor interpolation.
  mRootVisual->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
  return true;
}

bool DCLayerTree::InitializeVideoOverlaySupport() {
  HRESULT hr;

  hr = mDevice->QueryInterface(
      (ID3D11VideoDevice**)getter_AddRefs(mVideoDevice));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to get D3D11VideoDevice: " << gfx::hexa(hr);
    return false;
  }

  hr =
      mCtx->QueryInterface((ID3D11VideoContext**)getter_AddRefs(mVideoContext));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to get D3D11VideoContext: " << gfx::hexa(hr);
    return false;
  }

  // XXX When video is rendered to DXGI_FORMAT_B8G8R8A8_UNORM SwapChain with
  // VideoProcessor, it seems that we do not need to check
  // IDXGIOutput3::CheckOverlaySupport().
  // If we want to yuv at DecodeSwapChain, its support seems necessary.

  mVideoOverlaySupported = true;
  return true;
}

DCSurface* DCLayerTree::GetSurface(wr::NativeSurfaceId aId) const {
  auto surface_it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surface_it != mDCSurfaces.end());
  return surface_it->second.get();
}

void DCLayerTree::SetDefaultSwapChain(IDXGISwapChain1* aSwapChain) {
  mRootVisual->AddVisual(mDefaultSwapChainVisual, TRUE, nullptr);
  mDefaultSwapChainVisual->SetContent(aSwapChain);
  // Default SwapChain's visual does not need linear interporation.
  mDefaultSwapChainVisual->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
  mPendingCommit = true;
}

void DCLayerTree::MaybeUpdateDebug() {
  bool updated = false;
  updated |= MaybeUpdateDebugCounter();
  updated |= MaybeUpdateDebugVisualRedrawRegions();
  if (updated) {
    mPendingCommit = true;
  }
}

void DCLayerTree::MaybeCommit() {
  if (!mPendingCommit) {
    return;
  }
  mCompositionDevice->Commit();
}

void DCLayerTree::WaitForCommitCompletion() {
  mCompositionDevice->WaitForCommitCompletion();
}

void DCLayerTree::DisableNativeCompositor() {
  MOZ_ASSERT(mCurrentSurface.isNothing());
  MOZ_ASSERT(mCurrentLayers.empty());

  ReleaseNativeCompositorResources();
  mPrevLayers.clear();
  mRootVisual->RemoveAllVisuals();
}

bool DCLayerTree::MaybeUpdateDebugCounter() {
  bool debugCounter = StaticPrefs::gfx_webrender_debug_dcomp_counter();
  if (mDebugCounter == debugCounter) {
    return false;
  }

  RefPtr<IDCompositionDeviceDebug> debugDevice;
  HRESULT hr = mCompositionDevice->QueryInterface(
      (IDCompositionDeviceDebug**)getter_AddRefs(debugDevice));
  if (FAILED(hr)) {
    return false;
  }

  if (debugCounter) {
    debugDevice->EnableDebugCounters();
  } else {
    debugDevice->DisableDebugCounters();
  }

  mDebugCounter = debugCounter;
  return true;
}

bool DCLayerTree::MaybeUpdateDebugVisualRedrawRegions() {
  bool debugVisualRedrawRegions =
      StaticPrefs::gfx_webrender_debug_dcomp_redraw_regions();
  if (mDebugVisualRedrawRegions == debugVisualRedrawRegions) {
    return false;
  }

  RefPtr<IDCompositionVisualDebug> visualDebug;
  HRESULT hr = mRootVisual->QueryInterface(
      (IDCompositionVisualDebug**)getter_AddRefs(visualDebug));
  if (FAILED(hr)) {
    return false;
  }

  if (debugVisualRedrawRegions) {
    visualDebug->EnableRedrawRegions();
  } else {
    visualDebug->DisableRedrawRegions();
  }

  mDebugVisualRedrawRegions = debugVisualRedrawRegions;
  return true;
}

void DCLayerTree::CompositorBeginFrame() { mCurrentFrame++; }

void DCLayerTree::CompositorEndFrame() {
  auto start = TimeStamp::Now();
  // Check if the visual tree of surfaces is the same as last frame.
  bool same = mPrevLayers == mCurrentLayers;

  if (!same) {
    // If not, we need to rebuild the visual tree. Note that addition or
    // removal of tiles no longer needs to rebuild the main visual tree
    // here, since they are added as children of the surface visual.
    mRootVisual->RemoveAllVisuals();
  }

  for (auto it = mCurrentLayers.begin(); it != mCurrentLayers.end(); ++it) {
    auto surface_it = mDCSurfaces.find(*it);
    MOZ_RELEASE_ASSERT(surface_it != mDCSurfaces.end());
    const auto surface = surface_it->second.get();
    // Ensure surface is trimmed to updated tile valid rects
    surface->UpdateAllocatedRect();
    if (!same) {
      // Add surfaces in z-order they were added to the scene.
      const auto visual = surface->GetVisual();
      mRootVisual->AddVisual(visual, FALSE, nullptr);
    }
  }

  mPrevLayers.swap(mCurrentLayers);
  mCurrentLayers.clear();

  mCompositionDevice->Commit();

  auto end = TimeStamp::Now();
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::COMPOSITE_SWAP_TIME,
                                 (end - start).ToMilliseconds() * 10.);

  // Remove any framebuffers that haven't been
  // used in the last 60 frames.
  //
  // This should use nsTArray::RemoveElementsBy once
  // CachedFrameBuffer is able to properly destroy
  // itself in the destructor.
  const auto gl = GetGLContext();
  for (uint32_t i = 0, len = mFrameBuffers.Length(); i < len; ++i) {
    auto& fb = mFrameBuffers[i];
    if ((mCurrentFrame - fb.lastFrameUsed) > 60) {
      gl->fDeleteRenderbuffers(1, &fb.depthRboId);
      gl->fDeleteFramebuffers(1, &fb.fboId);
      mFrameBuffers.UnorderedRemoveElementAt(i);
      --i;  // Examine the element again, if necessary.
      --len;
    }
  }
}

void DCLayerTree::Bind(wr::NativeTileId aId, wr::DeviceIntPoint* aOffset,
                       uint32_t* aFboId, wr::DeviceIntRect aDirtyRect,
                       wr::DeviceIntRect aValidRect) {
  auto surface = GetSurface(aId.surface_id);
  auto tile = surface->GetTile(aId.x, aId.y);
  wr::DeviceIntPoint targetOffset{0, 0};

  gfx::IntRect validRect(aValidRect.origin.x, aValidRect.origin.y,
                         aValidRect.size.width, aValidRect.size.height);
  if (!tile->mValidRect.IsEqualEdges(validRect)) {
    tile->mValidRect = validRect;
    surface->DirtyAllocatedRect();
  }
  wr::DeviceIntSize tileSize = surface->GetTileSize();
  RefPtr<IDCompositionSurface> compositionSurface =
      surface->GetCompositionSurface();
  wr::DeviceIntPoint virtualOffset = surface->GetVirtualOffset();
  targetOffset.x = virtualOffset.x + tileSize.width * aId.x;
  targetOffset.y = virtualOffset.y + tileSize.height * aId.y;

  *aFboId = CreateEGLSurfaceForCompositionSurface(
      aDirtyRect, aOffset, compositionSurface, targetOffset);
  mCurrentSurface = Some(compositionSurface);
}

void DCLayerTree::Unbind() {
  if (mCurrentSurface.isNothing()) {
    return;
  }

  RefPtr<IDCompositionSurface> surface = mCurrentSurface.ref();
  surface->EndDraw();

  DestroyEGLSurface();
  mCurrentSurface = Nothing();
}

void DCLayerTree::CreateSurface(wr::NativeSurfaceId aId,
                                wr::DeviceIntPoint aVirtualOffset,
                                wr::DeviceIntSize aTileSize, bool aIsOpaque) {
  auto it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(it == mDCSurfaces.end());
  if (it != mDCSurfaces.end()) {
    // DCSurface already exists.
    return;
  }

  // Tile size needs to be positive.
  if (aTileSize.width <= 0 || aTileSize.height <= 0) {
    gfxCriticalNote << "TileSize is not positive aId: " << wr::AsUint64(aId)
                    << " aTileSize(" << aTileSize.width << ","
                    << aTileSize.height << ")";
  }

  auto surface =
      MakeUnique<DCSurface>(aTileSize, aVirtualOffset, aIsOpaque, this);
  if (!surface->Initialize()) {
    gfxCriticalNote << "Failed to initialize DCSurface: " << wr::AsUint64(aId);
    return;
  }

  mDCSurfaces[aId] = std::move(surface);
}

void DCLayerTree::CreateExternalSurface(wr::NativeSurfaceId aId,
                                        bool aIsOpaque) {
  auto it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(it == mDCSurfaces.end());

  auto surface = MakeUnique<DCSurfaceVideo>(aIsOpaque, this);
  if (!surface->Initialize()) {
    gfxCriticalNote << "Failed to initialize DCSurfaceVideo: "
                    << wr::AsUint64(aId);
    return;
  }

  mDCSurfaces[aId] = std::move(surface);
}

void DCLayerTree::DestroySurface(NativeSurfaceId aId) {
  auto surface_it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surface_it != mDCSurfaces.end());
  auto surface = surface_it->second.get();

  mRootVisual->RemoveVisual(surface->GetVisual());
  mDCSurfaces.erase(surface_it);
}

void DCLayerTree::CreateTile(wr::NativeSurfaceId aId, int aX, int aY) {
  auto surface = GetSurface(aId);
  surface->CreateTile(aX, aY);
}

void DCLayerTree::DestroyTile(wr::NativeSurfaceId aId, int aX, int aY) {
  auto surface = GetSurface(aId);
  surface->DestroyTile(aX, aY);
}

void DCLayerTree::AttachExternalImage(wr::NativeSurfaceId aId,
                                      wr::ExternalImageId aExternalImage) {
  auto surface_it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surface_it != mDCSurfaces.end());
  auto* surfaceVideo = surface_it->second->AsDCSurfaceVideo();
  MOZ_RELEASE_ASSERT(surfaceVideo);

  surfaceVideo->AttachExternalImage(aExternalImage);
}

template <typename T>
static inline D2D1_RECT_F D2DRect(const T& aRect) {
  return D2D1::RectF(aRect.X(), aRect.Y(), aRect.XMost(), aRect.YMost());
}

static inline D2D1_MATRIX_3X2_F D2DMatrix(const gfx::Matrix& aTransform) {
  return D2D1::Matrix3x2F(aTransform._11, aTransform._12, aTransform._21,
                          aTransform._22, aTransform._31, aTransform._32);
}

void DCLayerTree::AddSurface(wr::NativeSurfaceId aId,
                             const wr::CompositorSurfaceTransform& aTransform,
                             wr::DeviceIntRect aClipRect,
                             wr::ImageRendering aImageRendering) {
  auto it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(it != mDCSurfaces.end());
  const auto surface = it->second.get();
  const auto visual = surface->GetVisual();

  wr::DeviceIntPoint virtualOffset = surface->GetVirtualOffset();

  gfx::Matrix transform(aTransform.m11, aTransform.m12, aTransform.m21,
                        aTransform.m22, aTransform.m41, aTransform.m42);
  transform.PreTranslate(-virtualOffset.x, -virtualOffset.y);

  // The DirectComposition API applies clipping *before* any transforms/offset,
  // whereas we want the clip applied after.
  // Right now, we only support rectilinear transforms, and then we transform
  // our clip into pre-transform coordinate space for it to be applied there.
  // DirectComposition does have an option for pre-transform clipping, if you
  // create an explicit IDCompositionEffectGroup object and set a 3D transform
  // on that. I suspect that will perform worse though, so we should only do
  // that for complex transforms (which are never provided right now).
  MOZ_ASSERT(transform.IsRectilinear());
  gfx::Rect clip = transform.Inverse().TransformBounds(
      gfx::Rect(aClipRect.origin.x, aClipRect.origin.y, aClipRect.size.width,
                aClipRect.size.height));
  // Set the clip rect - converting from world space to the pre-offset space
  // that DC requires for rectangle clips.
  visual->SetClip(D2DRect(clip));

  // TODO: The input matrix is a 4x4, but we only support a 3x2 at
  // the D3D API level (unless we QI to IDCompositionVisual3, which might
  // not be available?).
  // Should we assert here, or restrict at the WR API level.
  visual->SetTransform(D2DMatrix(transform));

  if (aImageRendering == wr::ImageRendering::Auto) {
    visual->SetBitmapInterpolationMode(
        DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
  } else {
    visual->SetBitmapInterpolationMode(
        DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
  }

  mCurrentLayers.push_back(aId);
}

GLuint DCLayerTree::GetOrCreateFbo(int aWidth, int aHeight) {
  const auto gl = GetGLContext();
  GLuint fboId = 0;

  // Check if we have a cached FBO with matching dimensions
  for (auto it = mFrameBuffers.begin(); it != mFrameBuffers.end(); ++it) {
    if (it->width == aWidth && it->height == aHeight) {
      fboId = it->fboId;
      it->lastFrameUsed = mCurrentFrame;
      break;
    }
  }

  // If not, create a new FBO with attached depth buffer
  if (fboId == 0) {
    // Create the depth buffer
    GLuint depthRboId;
    gl->fGenRenderbuffers(1, &depthRboId);
    gl->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, depthRboId);
    gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, LOCAL_GL_DEPTH_COMPONENT24,
                             aWidth, aHeight);

    // Create the framebuffer and attach the depth buffer to it
    gl->fGenFramebuffers(1, &fboId);
    gl->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, fboId);
    gl->fFramebufferRenderbuffer(LOCAL_GL_DRAW_FRAMEBUFFER,
                                 LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, depthRboId);

    // Store this in the cache for future calls.
    // TODO(gw): Maybe we should periodically scan this list and remove old
    // entries that
    //           haven't been used for some time?
    DCLayerTree::CachedFrameBuffer frame_buffer_info;
    frame_buffer_info.width = aWidth;
    frame_buffer_info.height = aHeight;
    frame_buffer_info.fboId = fboId;
    frame_buffer_info.depthRboId = depthRboId;
    frame_buffer_info.lastFrameUsed = mCurrentFrame;
    mFrameBuffers.AppendElement(frame_buffer_info);
  }

  return fboId;
}

bool DCLayerTree::EnsureVideoProcessor(const gfx::IntSize& aVideoSize) {
  HRESULT hr;

  if (!mVideoDevice || !mVideoContext) {
    return false;
  }

  if (mVideoProcessor && aVideoSize == mVideoSize) {
    return true;
  }

  mVideoProcessor = nullptr;
  mVideoProcessorEnumerator = nullptr;

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = aVideoSize.width;
  desc.InputHeight = aVideoSize.height;
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = aVideoSize.width;
  desc.OutputHeight = aVideoSize.height;
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

  hr = mVideoDevice->CreateVideoProcessorEnumerator(
      &desc, getter_AddRefs(mVideoProcessorEnumerator));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create VideoProcessorEnumerator: "
                    << gfx::hexa(hr);
    return false;
  }

  hr = mVideoDevice->CreateVideoProcessor(mVideoProcessorEnumerator, 0,
                                          getter_AddRefs(mVideoProcessor));
  if (FAILED(hr)) {
    mVideoProcessor = nullptr;
    mVideoProcessorEnumerator = nullptr;
    gfxCriticalNote << "Failed to create VideoProcessor: " << gfx::hexa(hr);
    return false;
  }

  // Reduce power cosumption
  // By default, the driver might perform certain processing tasks automatically
  mVideoContext->VideoProcessorSetStreamAutoProcessingMode(mVideoProcessor, 0,
                                                           FALSE);

  mVideoSize = aVideoSize;
  return true;
}

DCSurface::DCSurface(wr::DeviceIntSize aTileSize,
                     wr::DeviceIntPoint aVirtualOffset, bool aIsOpaque,
                     DCLayerTree* aDCLayerTree)
    : mDCLayerTree(aDCLayerTree),
      mTileSize(aTileSize),
      mIsOpaque(aIsOpaque),
      mAllocatedRectDirty(true),
      mVirtualOffset(aVirtualOffset) {}

DCSurface::~DCSurface() {}

bool DCSurface::Initialize() {
  HRESULT hr;
  const auto dCompDevice = mDCLayerTree->GetCompositionDevice();
  hr = dCompDevice->CreateVisual(getter_AddRefs(mVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
  }

  DXGI_ALPHA_MODE alpha_mode =
      mIsOpaque ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_PREMULTIPLIED;

  hr = dCompDevice->CreateVirtualSurface(
      VIRTUAL_SURFACE_SIZE, VIRTUAL_SURFACE_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM,
      alpha_mode, getter_AddRefs(mVirtualSurface));
  MOZ_ASSERT(SUCCEEDED(hr));

  // Bind the surface memory to this visual
  hr = mVisual->SetContent(mVirtualSurface);
  MOZ_ASSERT(SUCCEEDED(hr));

  return true;
}

void DCSurface::CreateTile(int aX, int aY) {
  TileKey key(aX, aY);
  MOZ_RELEASE_ASSERT(mDCTiles.find(key) == mDCTiles.end());

  auto tile = MakeUnique<DCTile>(mDCLayerTree);
  if (!tile->Initialize(aX, aY, mTileSize, mIsOpaque)) {
    gfxCriticalNote << "Failed to initialize DCTile: " << aX << aY;
    return;
  }

  mAllocatedRectDirty = true;

  mDCTiles[key] = std::move(tile);
}

void DCSurface::DestroyTile(int aX, int aY) {
  TileKey key(aX, aY);
  mAllocatedRectDirty = true;
  mDCTiles.erase(key);
}

void DCSurface::DirtyAllocatedRect() { mAllocatedRectDirty = true; }

void DCSurface::UpdateAllocatedRect() {
  if (mAllocatedRectDirty) {
    // The virtual surface may have holes in it (for example, an empty tile
    // that has no primitives). Instead of trimming to a single bounding
    // rect, supply the rect of each valid tile to handle this case.
    std::vector<RECT> validRects;

    for (auto it = mDCTiles.begin(); it != mDCTiles.end(); ++it) {
      auto tile = GetTile(it->first.mX, it->first.mY);
      RECT rect;

      rect.left = (LONG)(mVirtualOffset.x + it->first.mX * mTileSize.width +
                         tile->mValidRect.x);
      rect.top = (LONG)(mVirtualOffset.y + it->first.mY * mTileSize.height +
                        tile->mValidRect.y);
      rect.right = rect.left + tile->mValidRect.width;
      rect.bottom = rect.top + tile->mValidRect.height;

      validRects.push_back(rect);
    }

    mVirtualSurface->Trim(validRects.data(), validRects.size());
    mAllocatedRectDirty = false;
  }
}

DCTile* DCSurface::GetTile(int aX, int aY) const {
  TileKey key(aX, aY);
  auto tile_it = mDCTiles.find(key);
  MOZ_RELEASE_ASSERT(tile_it != mDCTiles.end());
  return tile_it->second.get();
}

DCSurfaceVideo::DCSurfaceVideo(bool aIsOpaque, DCLayerTree* aDCLayerTree)
    : DCSurface(wr::DeviceIntSize{}, wr::DeviceIntPoint{}, aIsOpaque,
                aDCLayerTree) {}

void DCSurfaceVideo::AttachExternalImage(wr::ExternalImageId aExternalImage) {
  RenderTextureHost* texture =
      RenderThread::Get()->GetRenderTexture(aExternalImage);
  MOZ_RELEASE_ASSERT(texture);

  if (mPrevTexture == texture) {
    return;
  }

  // XXX if software decoded video frame format is nv12, it could be used as
  // video overlay.
  if (!texture || !texture->AsRenderDXGITextureHost() ||
      texture->AsRenderDXGITextureHost()->GetFormat() !=
          gfx::SurfaceFormat::NV12) {
    gfxCriticalNote << "Unsupported RenderTexture for overlay: "
                    << gfx::hexa(texture);
    return;
  }

  gfx::IntSize size = texture->AsRenderDXGITextureHost()->GetSize(0);
  if (!mVideoSwapChain || mSwapChainSize != size) {
    ReleaseDecodeSwapChainResources();
    CreateVideoSwapChain(texture);
  }

  if (!mVideoSwapChain) {
    gfxCriticalNote << "Failed to create VideoSwapChain";
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_OVERLAY);
    return;
  }

  mVisual->SetContent(mVideoSwapChain);

  if (!CallVideoProcessorBlt(texture)) {
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_OVERLAY);
    return;
  }

  mVideoSwapChain->Present(0, 0);
  mPrevTexture = texture;
}

bool DCSurfaceVideo::CreateVideoSwapChain(RenderTextureHost* aTexture) {
  const auto device = mDCLayerTree->GetDevice();

  RefPtr<IDXGIDevice> dxgiDevice;
  device->QueryInterface((IDXGIDevice**)getter_AddRefs(dxgiDevice));

  RefPtr<IDXGIFactoryMedia> dxgiFactoryMedia;
  {
    RefPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(getter_AddRefs(adapter));
    adapter->GetParent(
        IID_PPV_ARGS((IDXGIFactoryMedia**)getter_AddRefs(dxgiFactoryMedia)));
  }

  mSwapChainSurfaceHandle = gfx::DeviceManagerDx::CreateDCompSurfaceHandle();
  if (!mSwapChainSurfaceHandle) {
    gfxCriticalNote << "Failed to create DCompSurfaceHandle";
    return false;
  }

  gfx::IntSize size = aTexture->AsRenderDXGITextureHost()->GetSize(0);
  DXGI_ALPHA_MODE alpha_mode =
      mIsOpaque ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_PREMULTIPLIED;

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = size.width;
  desc.Height = size.height;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags = 0;
  desc.AlphaMode = alpha_mode;

  HRESULT hr;
  hr = dxgiFactoryMedia->CreateSwapChainForCompositionSurfaceHandle(
      device, mSwapChainSurfaceHandle, &desc, nullptr,
      getter_AddRefs(mVideoSwapChain));

  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create video SwapChain: " << gfx::hexa(hr);
    return false;
  }

  mSwapChainSize = size;
  return true;
}

static Maybe<DXGI_COLOR_SPACE_TYPE> GetSourceDXGIColorSpace(
    const gfx::YUVColorSpace aYUVColorSpace,
    const gfx::ColorRange aColorRange) {
  if (aYUVColorSpace == gfx::YUVColorSpace::BT601) {
    if (aColorRange == gfx::ColorRange::FULL) {
      return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601);
    } else {
      return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601);
    }
  } else if (aYUVColorSpace == gfx::YUVColorSpace::BT709) {
    if (aColorRange == gfx::ColorRange::FULL) {
      return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709);
    } else {
      return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);
    }
  } else if (aYUVColorSpace == gfx::YUVColorSpace::BT2020) {
    if (aColorRange == gfx::ColorRange::FULL) {
      // XXX Add SMPTEST2084 handling. HDR content is not handled yet by
      // video overlay.
      return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020);
    } else {
      return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020);
    }
  }

  return Nothing();
}

bool DCSurfaceVideo::CallVideoProcessorBlt(RenderTextureHost* aTexture) {
  HRESULT hr;
  const auto videoDevice = mDCLayerTree->GetVideoDevice();
  const auto videoContext = mDCLayerTree->GetVideoContext();
  const auto texture = aTexture->AsRenderDXGITextureHost();

  Maybe<DXGI_COLOR_SPACE_TYPE> sourceColorSpace = GetSourceDXGIColorSpace(
      texture->GetYUVColorSpace(), texture->GetColorRange());
  if (sourceColorSpace.isNothing()) {
    gfxCriticalNote << "Unsupported color space";
    return false;
  }

  RefPtr<ID3D11Texture2D> texture2D = texture->GetD3D11Texture2DWithGL();
  if (!texture2D) {
    gfxCriticalNote << "Failed to get D3D11Texture2D";
    return false;
  }

  if (!mVideoSwapChain) {
    return false;
  }

  if (!mDCLayerTree->EnsureVideoProcessor(mSwapChainSize)) {
    gfxCriticalNote << "EnsureVideoProcessor Failed";
    return false;
  }

  RefPtr<IDXGISwapChain3> swapChain3;
  mVideoSwapChain->QueryInterface(
      (IDXGISwapChain3**)getter_AddRefs(swapChain3));
  if (!swapChain3) {
    gfxCriticalNote << "Failed to get IDXGISwapChain3";
    return false;
  }

  RefPtr<ID3D11VideoContext1> videoContext1;
  videoContext->QueryInterface(
      (ID3D11VideoContext1**)getter_AddRefs(videoContext1));
  if (!videoContext1) {
    gfxCriticalNote << "Failed to get ID3D11VideoContext1";
    return false;
  }

  const auto videoProcessor = mDCLayerTree->GetVideoProcessor();
  const auto videoProcessorEnumerator =
      mDCLayerTree->GetVideoProcessorEnumerator();

  DXGI_COLOR_SPACE_TYPE inputColorSpace = sourceColorSpace.ref();
  videoContext1->VideoProcessorSetStreamColorSpace1(videoProcessor, 0,
                                                    inputColorSpace);
  // XXX when content is hdr or yuv swapchain, it need to use other color space.
  DXGI_COLOR_SPACE_TYPE outputColorSpace =
      DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  hr = swapChain3->SetColorSpace1(outputColorSpace);
  if (FAILED(hr)) {
    gfxCriticalNote << "SetColorSpace1 failed: " << gfx::hexa(hr);
    return false;
  }
  videoContext1->VideoProcessorSetOutputColorSpace1(videoProcessor,
                                                    outputColorSpace);

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc = {};
  inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  inputDesc.Texture2D.ArraySlice = 0;

  RefPtr<ID3D11VideoProcessorInputView> inputView;
  hr = videoDevice->CreateVideoProcessorInputView(
      texture2D, videoProcessorEnumerator, &inputDesc,
      getter_AddRefs(inputView));
  if (FAILED(hr)) {
    gfxCriticalNote << "ID3D11VideoProcessorInputView creation failed: "
                    << gfx::hexa(hr);
    return false;
  }

  D3D11_VIDEO_PROCESSOR_STREAM stream = {};
  stream.Enable = true;
  stream.OutputIndex = 0;
  stream.InputFrameOrField = 0;
  stream.PastFrames = 0;
  stream.FutureFrames = 0;
  stream.pInputSurface = inputView.get();

  RECT destRect;
  destRect.left = 0;
  destRect.top = 0;
  destRect.right = mSwapChainSize.width;
  destRect.bottom = mSwapChainSize.height;

  videoContext->VideoProcessorSetOutputTargetRect(videoProcessor, TRUE,
                                                  &destRect);
  videoContext->VideoProcessorSetStreamDestRect(videoProcessor, 0, TRUE,
                                                &destRect);
  RECT sourceRect;
  sourceRect.left = 0;
  sourceRect.top = 0;
  sourceRect.right = mSwapChainSize.width;
  sourceRect.bottom = mSwapChainSize.height;
  videoContext->VideoProcessorSetStreamSourceRect(videoProcessor, 0, TRUE,
                                                  &sourceRect);

  if (!mOutputView) {
    RefPtr<ID3D11Texture2D> backBuf;
    mVideoSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                               (void**)getter_AddRefs(backBuf));

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = {};
    outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2D.MipSlice = 0;

    hr = videoDevice->CreateVideoProcessorOutputView(
        backBuf, videoProcessorEnumerator, &outputDesc,
        getter_AddRefs(mOutputView));
    if (FAILED(hr)) {
      gfxCriticalNote << "ID3D11VideoProcessorOutputView creation failed: "
                      << gfx::hexa(hr);
      return false;
    }
  }

  hr = videoContext->VideoProcessorBlt(videoProcessor, mOutputView, 0, 1,
                                       &stream);
  if (FAILED(hr)) {
    gfxCriticalNote << "VideoProcessorBlt failed: " << gfx::hexa(hr);
    return false;
  }

  return true;
}

void DCSurfaceVideo::ReleaseDecodeSwapChainResources() {
  mOutputView = nullptr;
  mVideoSwapChain = nullptr;
  mDecodeSwapChain = nullptr;
  mDecodeResource = nullptr;
  if (mSwapChainSurfaceHandle) {
    ::CloseHandle(mSwapChainSurfaceHandle);
    mSwapChainSurfaceHandle = 0;
  }
  mSwapChainSize = gfx::IntSize();
}

DCTile::DCTile(DCLayerTree* aDCLayerTree) : mDCLayerTree(aDCLayerTree) {}

DCTile::~DCTile() {}

bool DCTile::Initialize(int aX, int aY, wr::DeviceIntSize aSize,
                        bool aIsOpaque) {
  if (aSize.width <= 0 || aSize.height <= 0) {
    return false;
  }

  // Initially, the entire tile is considered valid, unless it is set by
  // the SetTileProperties method.
  mValidRect.x = 0;
  mValidRect.y = 0;
  mValidRect.width = aSize.width;
  mValidRect.height = aSize.height;

  return true;
}

GLuint DCLayerTree::CreateEGLSurfaceForCompositionSurface(
    wr::DeviceIntRect aDirtyRect, wr::DeviceIntPoint* aOffset,
    RefPtr<IDCompositionSurface> aCompositionSurface,
    wr::DeviceIntPoint aSurfaceOffset) {
  MOZ_ASSERT(aCompositionSurface.get());

  HRESULT hr;
  const auto gl = GetGLContext();
  RefPtr<ID3D11Texture2D> backBuf;
  POINT offset;

  RECT update_rect;
  update_rect.left = aSurfaceOffset.x + aDirtyRect.origin.x;
  update_rect.top = aSurfaceOffset.y + aDirtyRect.origin.y;
  update_rect.right = update_rect.left + aDirtyRect.size.width;
  update_rect.bottom = update_rect.top + aDirtyRect.size.height;

  hr = aCompositionSurface->BeginDraw(&update_rect, __uuidof(ID3D11Texture2D),
                                      (void**)getter_AddRefs(backBuf), &offset);
  if (FAILED(hr)) {
    gfxCriticalNote << "DCompositionSurface::BeginDraw failed: "
                    << gfx::hexa(hr);
    RenderThread::Get()->HandleWebRenderError(WebRenderError::BEGIN_DRAW);
    return false;
  }

  // DC includes the origin of the dirty / update rect in the draw offset,
  // undo that here since WR expects it to be an absolute offset.
  offset.x -= aDirtyRect.origin.x;
  offset.y -= aDirtyRect.origin.y;

  D3D11_TEXTURE2D_DESC desc;
  backBuf->GetDesc(&desc);

  const auto& gle = gl::GLContextEGL::Cast(gl);
  const auto& egl = gle->mEgl;

  const auto buffer = reinterpret_cast<EGLClientBuffer>(backBuf.get());

  // Construct an EGLImage wrapper around the D3D texture for ANGLE.
  const EGLint attribs[] = {LOCAL_EGL_NONE};
  mEGLImage = egl->fCreateImage(EGL_NO_CONTEXT, LOCAL_EGL_D3D11_TEXTURE_ANGLE,
                                buffer, attribs);

  // Get the current FBO and RBO id, so we can restore them later
  GLint currentFboId, currentRboId;
  gl->fGetIntegerv(LOCAL_GL_DRAW_FRAMEBUFFER_BINDING, &currentFboId);
  gl->fGetIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, &currentRboId);

  // Create a render buffer object that is backed by the EGL image.
  gl->fGenRenderbuffers(1, &mColorRBO);
  gl->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mColorRBO);
  gl->fEGLImageTargetRenderbufferStorage(LOCAL_GL_RENDERBUFFER, mEGLImage);

  // Get or create an FBO for the specified dimensions
  GLuint fboId = GetOrCreateFbo(desc.Width, desc.Height);

  // Attach the new renderbuffer to the FBO
  gl->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, fboId);
  gl->fFramebufferRenderbuffer(LOCAL_GL_DRAW_FRAMEBUFFER,
                               LOCAL_GL_COLOR_ATTACHMENT0,
                               LOCAL_GL_RENDERBUFFER, mColorRBO);

  // Restore previous FBO and RBO bindings
  gl->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, currentFboId);
  gl->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, currentRboId);

  aOffset->x = offset.x;
  aOffset->y = offset.y;

  return fboId;
}

void DCLayerTree::DestroyEGLSurface() {
  const auto gl = GetGLContext();

  if (mColorRBO) {
    gl->fDeleteRenderbuffers(1, &mColorRBO);
    mColorRBO = 0;
  }

  if (mEGLImage) {
    const auto& gle = gl::GLContextEGL::Cast(gl);
    const auto& egl = gle->mEgl;
    egl->fDestroyImage(mEGLImage);
    mEGLImage = EGL_NO_IMAGE;
  }
}

}  // namespace wr
}  // namespace mozilla
