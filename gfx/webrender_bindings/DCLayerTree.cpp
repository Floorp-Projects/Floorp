/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCLayerTree.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLContextEGL.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/HelpersD3D11.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderD3D11TextureHost.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/Telemetry.h"
#include "nsPrintfCString.h"
#include "ScopedGLHelpers.h"
#include "WinUtils.h"

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

extern LazyLogModule gRenderThreadLog;
#define LOG(...) MOZ_LOG(gRenderThreadLog, LogLevel::Debug, (__VA_ARGS__))

UniquePtr<GpuOverlayInfo> DCLayerTree::sGpuOverlayInfo;

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

void DCLayerTree::Shutdown() { DCLayerTree::sGpuOverlayInfo = nullptr; }

DCLayerTree::DCLayerTree(gl::GLContext* aGL, EGLConfig aEGLConfig,
                         ID3D11Device* aDevice, ID3D11DeviceContext* aCtx,
                         IDCompositionDevice2* aCompositionDevice)
    : mGL(aGL),
      mEGLConfig(aEGLConfig),
      mDevice(aDevice),
      mCtx(aCtx),
      mCompositionDevice(aCompositionDevice),
      mDebugCounter(false),
      mDebugVisualRedrawRegions(false),
      mEGLImage(EGL_NO_IMAGE),
      mColorRBO(0),
      mPendingCommit(false) {
  LOG("DCLayerTree::DCLayerTree()");
}

DCLayerTree::~DCLayerTree() {
  LOG("DCLayerTree::~DCLayerTree()");

  ReleaseNativeCompositorResources();
}

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
        "DCLayerTree(get IDCompositionDesktopDevice failed %lx)", hr));
    return false;
  }

  hr = desktopDevice->CreateTargetForHwnd(aHwnd, TRUE,
                                          getter_AddRefs(mCompositionTarget));
  if (FAILED(hr)) {
    aError.Assign(nsPrintfCString(
        "DCLayerTree(create DCompositionTarget failed %lx)", hr));
    return false;
  }

  hr = mCompositionDevice->CreateVisual(getter_AddRefs(mRootVisual));
  if (FAILED(hr)) {
    aError.Assign(nsPrintfCString(
        "DCLayerTree(create root DCompositionVisual failed %lx)", hr));
    return false;
  }

  hr =
      mCompositionDevice->CreateVisual(getter_AddRefs(mDefaultSwapChainVisual));
  if (FAILED(hr)) {
    aError.Assign(nsPrintfCString(
        "DCLayerTree(create swap chain DCompositionVisual failed %lx)", hr));
    return false;
  }

  if (gfx::gfxVars::UseWebRenderDCompVideoOverlayWin()) {
    if (!InitializeVideoOverlaySupport()) {
      RenderThread::Get()->HandleWebRenderError(WebRenderError::VIDEO_OVERLAY);
    }
  }
  if (!sGpuOverlayInfo) {
    // Set default if sGpuOverlayInfo was not set.
    sGpuOverlayInfo = MakeUnique<GpuOverlayInfo>();
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

bool FlagsSupportsOverlays(UINT flags) {
  return (flags & (DXGI_OVERLAY_SUPPORT_FLAG_DIRECT |
                   DXGI_OVERLAY_SUPPORT_FLAG_SCALING));
}

// A warpper of IDXGIOutput4::CheckOverlayColorSpaceSupport()
bool CheckOverlayColorSpaceSupport(DXGI_FORMAT aDxgiFormat,
                                   DXGI_COLOR_SPACE_TYPE aDxgiColorSpace,
                                   RefPtr<IDXGIOutput> aOutput,
                                   RefPtr<ID3D11Device> aD3d11Device) {
  UINT colorSpaceSupportFlags = 0;
  RefPtr<IDXGIOutput4> output4;

  if (FAILED(aOutput->QueryInterface(__uuidof(IDXGIOutput4),
                                     getter_AddRefs(output4)))) {
    return false;
  }

  if (FAILED(output4->CheckOverlayColorSpaceSupport(
          aDxgiFormat, aDxgiColorSpace, aD3d11Device,
          &colorSpaceSupportFlags))) {
    return false;
  }

  return (colorSpaceSupportFlags &
          DXGI_OVERLAY_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
}

bool DCLayerTree::InitializeVideoOverlaySupport() {
  MOZ_ASSERT(IsWin10AnniversaryUpdateOrLater());

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

  if (sGpuOverlayInfo) {
    return true;
  }

  UniquePtr<GpuOverlayInfo> info = MakeUnique<GpuOverlayInfo>();

  RefPtr<IDXGIDevice> dxgiDevice;
  RefPtr<IDXGIAdapter> adapter;
  mDevice->QueryInterface((IDXGIDevice**)getter_AddRefs(dxgiDevice));
  dxgiDevice->GetAdapter(getter_AddRefs(adapter));

  unsigned int i = 0;
  while (true) {
    RefPtr<IDXGIOutput> output;
    if (FAILED(adapter->EnumOutputs(i++, getter_AddRefs(output)))) {
      break;
    }
    RefPtr<IDXGIOutput3> output3;
    if (FAILED(output->QueryInterface(__uuidof(IDXGIOutput3),
                                      getter_AddRefs(output3)))) {
      break;
    }

    output3->CheckOverlaySupport(DXGI_FORMAT_NV12, mDevice,
                                 &info->mNv12OverlaySupportFlags);
    output3->CheckOverlaySupport(DXGI_FORMAT_YUY2, mDevice,
                                 &info->mYuy2OverlaySupportFlags);
    output3->CheckOverlaySupport(DXGI_FORMAT_B8G8R8A8_UNORM, mDevice,
                                 &info->mBgra8OverlaySupportFlags);
    output3->CheckOverlaySupport(DXGI_FORMAT_R10G10B10A2_UNORM, mDevice,
                                 &info->mRgb10a2OverlaySupportFlags);

    if (FlagsSupportsOverlays(info->mNv12OverlaySupportFlags)) {
      // NV12 format is preferred if it's supported.
      info->mOverlayFormatUsed = DXGI_FORMAT_NV12;
      info->mSupportsHardwareOverlays = true;
    }

    if (!info->mSupportsHardwareOverlays &&
        FlagsSupportsOverlays(info->mYuy2OverlaySupportFlags)) {
      // If NV12 isn't supported, fallback to YUY2 if it's supported.
      info->mOverlayFormatUsed = DXGI_FORMAT_YUY2;
      info->mSupportsHardwareOverlays = true;
    }

    // RGB10A2 overlay is used for displaying HDR content. In Intel's
    // platform, RGB10A2 overlay is enabled only when
    // DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 is supported.
    if (FlagsSupportsOverlays(info->mRgb10a2OverlaySupportFlags)) {
      if (!CheckOverlayColorSpaceSupport(
              DXGI_FORMAT_R10G10B10A2_UNORM,
              DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, output, mDevice))
        info->mRgb10a2OverlaySupportFlags = 0;
    }

    // Early out after the first output that reports overlay support. All
    // outputs are expected to report the same overlay support according to
    // Microsoft's WDDM documentation:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/multiplane-overlay-hardware-requirements
    if (info->mSupportsHardwareOverlays) {
      break;
    }
  }

  if (!StaticPrefs::gfx_webrender_dcomp_video_yuv_overlay_win_AtStartup()) {
    info->mOverlayFormatUsed = DXGI_FORMAT_B8G8R8A8_UNORM;
    info->mSupportsHardwareOverlays = false;
  }

  info->mSupportsOverlays = info->mSupportsHardwareOverlays;

  sGpuOverlayInfo = std::move(info);

  if (auto* gpuParent = gfx::GPUParent::GetSingleton()) {
    gpuParent->NotifyOverlayInfo(GetOverlayInfo());
  }

  return true;
}

DCSurface* DCLayerTree::GetSurface(wr::NativeSurfaceId aId) const {
  auto surface_it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surface_it != mDCSurfaces.end());
  return surface_it->second.get();
}

void DCLayerTree::SetDefaultSwapChain(IDXGISwapChain1* aSwapChain) {
  LOG("DCLayerTree::SetDefaultSwapChain()");

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

  gfx::IntRect validRect(aValidRect.min.x, aValidRect.min.y, aValidRect.width(),
                         aValidRect.height());
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

  auto surface = MakeUnique<DCSurfaceSwapChain>(aIsOpaque, this);
  if (!surface->Initialize()) {
    gfxCriticalNote << "Failed to initialize DCSurfaceSwapChain: "
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
  auto* surfaceSc = surface_it->second->AsDCSurfaceSwapChain();
  MOZ_RELEASE_ASSERT(surfaceSc);

  surfaceSc->AttachExternalImage(aExternalImage);
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

  auto* surfaceSc = surface->AsDCSurfaceSwapChain();
  if (surfaceSc) {
    const auto presentationTransform = surfaceSc->EnsurePresented(transform);
    if (presentationTransform) {
      transform = *presentationTransform;
    }  // else EnsurePresented failed, just limp along?
  }

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
  gfx::Rect clip = transform.Inverse().TransformBounds(gfx::Rect(
      aClipRect.min.x, aClipRect.min.y, aClipRect.width(), aClipRect.height()));
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

bool DCLayerTree::EnsureVideoProcessorAtLeast(const gfx::IntSize& aInputSize,
                                              const gfx::IntSize& aOutputSize) {
  HRESULT hr;

  if (!mVideoDevice || !mVideoContext) {
    return false;
  }

  if (mVideoProcessor && (aInputSize <= mVideoInputSize) &&
      (aOutputSize <= mVideoOutputSize)) {
    return true;
  }

  mVideoProcessor = nullptr;
  mVideoProcessorEnumerator = nullptr;

  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = aInputSize.width;
  desc.InputHeight = aInputSize.height;
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = aOutputSize.width;
  desc.OutputHeight = aOutputSize.height;
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

  mVideoInputSize = aInputSize;
  mVideoOutputSize = aOutputSize;

  return true;
}

bool DCLayerTree::SupportsHardwareOverlays() {
  return sGpuOverlayInfo->mSupportsHardwareOverlays;
}

DXGI_FORMAT DCLayerTree::GetOverlayFormatForSDR() {
  return sGpuOverlayInfo->mOverlayFormatUsed;
}

static layers::OverlaySupportType FlagsToOverlaySupportType(
    UINT aFlags, bool aSoftwareOverlaySupported) {
  if (aFlags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING) {
    return layers::OverlaySupportType::Scaling;
  }
  if (aFlags & DXGI_OVERLAY_SUPPORT_FLAG_DIRECT) {
    return layers::OverlaySupportType::Direct;
  }
  if (aSoftwareOverlaySupported) {
    return layers::OverlaySupportType::Software;
  }
  return layers::OverlaySupportType::None;
}

layers::OverlayInfo DCLayerTree::GetOverlayInfo() {
  layers::OverlayInfo info;

  info.mSupportsOverlays = sGpuOverlayInfo->mSupportsHardwareOverlays;
  info.mNv12Overlay =
      FlagsToOverlaySupportType(sGpuOverlayInfo->mNv12OverlaySupportFlags,
                                /* aSoftwareOverlaySupported */ false);
  info.mYuy2Overlay =
      FlagsToOverlaySupportType(sGpuOverlayInfo->mYuy2OverlaySupportFlags,
                                /* aSoftwareOverlaySupported */ false);
  info.mBgra8Overlay =
      FlagsToOverlaySupportType(sGpuOverlayInfo->mBgra8OverlaySupportFlags,
                                /* aSoftwareOverlaySupported */ true);
  info.mRgb10a2Overlay =
      FlagsToOverlaySupportType(sGpuOverlayInfo->mRgb10a2OverlaySupportFlags,
                                /* aSoftwareOverlaySupported */ false);

  return info;
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

// -
// DCSurfaceSwapChain

DCSurfaceSwapChain::DCSurfaceSwapChain(bool aIsOpaque,
                                       DCLayerTree* aDCLayerTree)
    : DCSurface(wr::DeviceIntSize{}, wr::DeviceIntPoint{}, aIsOpaque,
                aDCLayerTree) {
  if (mDCLayerTree->SupportsHardwareOverlays()) {
    mOverlayFormat = Some(mDCLayerTree->GetOverlayFormatForSDR());
  }
}

bool IsYuv(const DXGI_FORMAT aFormat) {
  if (aFormat == DXGI_FORMAT_NV12 || aFormat == DXGI_FORMAT_YUY2) {
    return true;
  }
  return false;
}

bool IsYuv(const gfx::SurfaceFormat aFormat) {
  switch (aFormat) {
    case gfx::SurfaceFormat::NV12:
    case gfx::SurfaceFormat::P010:
    case gfx::SurfaceFormat::P016:
    case gfx::SurfaceFormat::YUV:
    case gfx::SurfaceFormat::YUV422:
      return true;

    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::A8R8G8B8:
    case gfx::SurfaceFormat::X8R8G8B8:

    case gfx::SurfaceFormat::R8G8B8:
    case gfx::SurfaceFormat::B8G8R8:

    case gfx::SurfaceFormat::R5G6B5_UINT16:
    case gfx::SurfaceFormat::A8:
    case gfx::SurfaceFormat::A16:
    case gfx::SurfaceFormat::R8G8:
    case gfx::SurfaceFormat::R16G16:

    case gfx::SurfaceFormat::HSV:
    case gfx::SurfaceFormat::Lab:
    case gfx::SurfaceFormat::Depth:
    case gfx::SurfaceFormat::UNKNOWN:
      return false;
  }
  MOZ_ASSERT_UNREACHABLE();
}

void DCSurfaceSwapChain::AttachExternalImage(
    wr::ExternalImageId aExternalImage) {
  RenderTextureHost* texture =
      RenderThread::Get()->GetRenderTexture(aExternalImage);
  MOZ_RELEASE_ASSERT(texture);

  const auto textureDxgi = texture->AsRenderDXGITextureHost();
  if (!textureDxgi) {
    gfxCriticalNote << "Unsupported RenderTexture for overlay: "
                    << gfx::hexa(texture);
    return;
  }

  if (mSrc && mSrc->texture == textureDxgi) {
    return;  // Dupe.
  }

  Src src = {};
  src.texture = textureDxgi;
  src.format = src.texture->GetFormat();
  src.size = src.texture->GetSize(0);
  src.space = {
      src.texture->mColorSpace,
      IsYuv(src.format) ? Some(src.texture->mColorRange) : Nothing(),
  };

  mSrc = Some(src);
}

// -

Maybe<DCSurfaceSwapChain::Dest> CreateSwapChain(ID3D11Device&, gfx::IntSize,
                                                DXGI_FORMAT,
                                                DXGI_COLOR_SPACE_TYPE);

// -

static Maybe<DXGI_COLOR_SPACE_TYPE> ExactDXGIColorSpace(
    const CspaceAndRange& space) {
  switch (space.space) {
    case gfx::ColorSpace2::BT601_525:
      if (!space.yuvRange) {
        return {};
      } else if (*space.yuvRange == gfx::ColorRange::FULL) {
        return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601);
      } else {
        return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601);
      }
    case gfx::ColorSpace2::UNKNOWN:
    case gfx::ColorSpace2::SRGB:  // Gamma ~2.2
      if (!space.yuvRange) {
        return Some(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
      } else if (*space.yuvRange == gfx::ColorRange::FULL) {
        return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709);
      } else {
        return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);
      }
    case gfx::ColorSpace2::BT709:  // Gamma ~2.4
      if (!space.yuvRange) {
        // This should ideally be G24, but that only exists for STUDIO not FULL.
        return Some(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
      } else if (*space.yuvRange == gfx::ColorRange::FULL) {
        // TODO return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G24_LEFT_P709);
        return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709);
      } else {
        // TODO return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709);
        return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709);
      }
    case gfx::ColorSpace2::BT2020:
      if (!space.yuvRange) {
        return Some(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020);
      } else if (*space.yuvRange == gfx::ColorRange::FULL) {
        // XXX Add SMPTEST2084 handling. HDR content is not handled yet by
        // video overlay.
        return Some(DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020);
      } else {
        return Some(DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020);
      }
    case gfx::ColorSpace2::DISPLAY_P3:
      return {};
  }
  MOZ_ASSERT_UNREACHABLE();
}

// -

color::ColorspaceDesc ToColorspaceDesc(const CspaceAndRange& space) {
  color::ColorspaceDesc ret = {};
  switch (space.space) {
    case gfx::ColorSpace2::UNKNOWN:
    case gfx::ColorSpace2::SRGB:
      ret.chrom = color::Chromaticities::Srgb();
      ret.tf = {color::PiecewiseGammaDesc::Srgb()};
      MOZ_ASSERT(!space.yuvRange);
      return ret;

    case gfx::ColorSpace2::DISPLAY_P3:
      ret.chrom = color::Chromaticities::DisplayP3();
      ret.tf = {color::PiecewiseGammaDesc::DisplayP3()};
      MOZ_ASSERT(!space.yuvRange);
      return ret;

    case gfx::ColorSpace2::BT601_525:
      ret.chrom = color::Chromaticities::Rec601_525_Ntsc();
      ret.tf = {color::PiecewiseGammaDesc::Rec709()};
      if (space.yuvRange) {
        ret.yuv = {
            {color::YuvLumaCoeffs::Rec709(), color::YcbcrDesc::Narrow8()}};
        if (*space.yuvRange == gfx::ColorRange::FULL) {
          ret.yuv->ycbcr = color::YcbcrDesc::Full8();
        }
      }
      return ret;

    case gfx::ColorSpace2::BT709:
      ret.chrom = color::Chromaticities::Rec709();
      ret.tf = {color::PiecewiseGammaDesc::Rec709()};
      if (space.yuvRange) {
        ret.yuv = {
            {color::YuvLumaCoeffs::Rec709(), color::YcbcrDesc::Narrow8()}};
        if (*space.yuvRange == gfx::ColorRange::FULL) {
          ret.yuv->ycbcr = color::YcbcrDesc::Full8();
        }
      }
      return ret;

    case gfx::ColorSpace2::BT2020:
      ret.chrom = color::Chromaticities::Rec2020();
      ret.tf = {color::PiecewiseGammaDesc::Rec2020_10bit()};
      if (space.yuvRange) {
        ret.yuv = {
            {color::YuvLumaCoeffs::Rec709(), color::YcbcrDesc::Narrow8()}};
        if (*space.yuvRange == gfx::ColorRange::FULL) {
          ret.yuv->ycbcr = color::YcbcrDesc::Full8();
        }
      }
      return ret;
  }
  MOZ_ASSERT_UNREACHABLE();
}

// -

static CspaceTransformPlan ChooseCspaceTransformPlan(
    const CspaceAndRange& srcSpace) {
  // Unfortunately, it looks like the no-op
  //    DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709
  // => DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709
  // transform mis-translates colors if you ask VideoProcessor to resize.
  // (jgilbert's RTX 3070 machine "osiris")
  // Absent more investigation, let's avoid VP with non-YUV sources for now.
  const auto cmsMode = GfxColorManagementMode();
  const bool doColorManagement = cmsMode != CMSMode::Off;
  if (srcSpace.yuvRange && doColorManagement) {
    const auto exactDxgiSpace = ExactDXGIColorSpace(srcSpace);
    if (exactDxgiSpace) {
      auto plan = CspaceTransformPlan::WithVideoProcessor{};
      plan.srcSpace = *exactDxgiSpace;
      if (srcSpace.yuvRange) {
        plan.dstYuvSpace = Some(plan.srcSpace);
        plan.dstRgbSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
      } else {
        plan.dstRgbSpace = plan.srcSpace;
      }
      return {Some(plan), {}};
    }
  }

  auto plan = CspaceTransformPlan::WithGLBlitHelper{
      ToColorspaceDesc(srcSpace),
      {color::Chromaticities::Srgb(), {color::PiecewiseGammaDesc::Srgb()}},
      DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
      DXGI_FORMAT_B8G8R8A8_UNORM,
  };
  switch (srcSpace.space) {
    case gfx::ColorSpace2::SRGB:
    case gfx::ColorSpace2::BT601_525:
    case gfx::ColorSpace2::BT709:
    case gfx::ColorSpace2::UNKNOWN:
      break;
    case gfx::ColorSpace2::BT2020:
    case gfx::ColorSpace2::DISPLAY_P3:
      // We know our src cspace, and we need to pick a dest cspace.
      // The technically-correct thing to do is pick scrgb rgb16f, but
      // bandwidth! One option for us is rec2020, which is huge, if the banding
      // is acceptable. (We could even use rgb10 for this) EXCEPT,
      // display-p3(1,0,0) red is rec2020(0.869 0.175 -0.005). At cost of
      // clipping red by up to 0.5%, it's worth considering. Do scrgb for now
      // though.

      // Let's do DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020 +
      // DXGI_FORMAT_R10G10B10A2_UNORM
      plan = {
          // Rec2020 g2.2 rgb10
          plan.srcSpace,
          {color::Chromaticities::Rec2020(),
           {color::PiecewiseGammaDesc::Srgb()}},
          DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020,  // Note that this is srgb
                                                     // gamma!
          DXGI_FORMAT_R10G10B10A2_UNORM,
      };
      plan = {
          // Actually, that doesn't work, so use scRGB g1.0 rgb16f
          plan.srcSpace,
          {color::Chromaticities::Rec709(), {}},
          DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,  // scRGB
          DXGI_FORMAT_R16G16B16A16_FLOAT,
      };
      break;
  }
  if (!doColorManagement) {
    plan.dstSpace = plan.srcSpace;
    plan.dstSpace.yuv = {};
  }

  return {{}, Some(plan)};
}

// -

Maybe<gfx::Matrix> DCSurfaceSwapChain::EnsurePresented(
    const gfx::Matrix& aTransform) {
  MOZ_RELEASE_ASSERT(mSrc);
  const auto& srcSize = mSrc->size;

  // -

  auto dstSize = srcSize;
  auto presentationTransform = aTransform;

  // When video is rendered to axis aligned integer rectangle, video scaling
  // should be done by stretching in the VideoProcessor.
  // Sotaro has observed a reduction in gpu queue tasks by doing scaling
  // ourselves (via VideoProcessor) instead of having DComp handle it.
  if (StaticPrefs::gfx_webrender_dcomp_video_vp_scaling_win_AtStartup() &&
      aTransform.PreservesAxisAlignedRectangles()) {
    const auto absScales =
        aTransform.ScaleFactors();  // E.g. [2, 0, 0, -2] => [2, 2]
    const auto presentationTransformUnscaled =
        presentationTransform.Copy().PreScale(1 / absScales.xScale,
                                              1 / absScales.yScale);
    const auto dstSizeScaled =
        gfx::IntSize::Round(gfx::Size(dstSize) * aTransform.ScaleFactors());
    const auto presentSizeOld =
        presentationTransform.TransformSize(gfx::Size(dstSize));
    const auto presentSizeNew =
        presentationTransformUnscaled.TransformSize(gfx::Size(dstSizeScaled));
    if (gfx::FuzzyEqual(presentSizeNew.width, presentSizeOld.width, 0.1f) &&
        gfx::FuzzyEqual(presentSizeNew.height, presentSizeOld.height, 0.1f)) {
      dstSize = dstSizeScaled;
      presentationTransform = presentationTransformUnscaled;
    }
  }

  // 4:2:2 subsampled formats like YUY2 must have an even width, and 4:2:0
  // subsampled formats like NV12 must have an even width and height.
  // And we should be able to pad width and height because we clip the Visual to
  // the unpadded rect. Just do this unconditionally.
  if (dstSize.width % 2 == 1) {
    dstSize.width += 1;
  }
  if (dstSize.height % 2 == 1) {
    dstSize.height += 1;
  }

  // -

  if (mDest && mDest->dest->size != dstSize) {
    mDest = Nothing();
  }
  if (mDest && mDest->srcSpace != mSrc->space) {
    mDest = Nothing();
  }

  // -

  // Ok, we have some options:
  // * ID3D11VideoProcessor can change between colorspaces that D3D knows about.
  //   * But, sometimes VideoProcessor can output YUV, and sometimes it needs
  //     to output RGB.[1] And we can only find out by trying it.
  // * Otherwise, we need to do it ourselves, via GLBlitHelper.

  // GLBlitHelper will always work. However, we probably want to prefer to use
  // ID3D11VideoProcessor where we can, as an optimization.

  // From sotaro:
  // > We use VideoProcessor for scaling reducing GPU usage.
  // > "Video Processing" in Windows Task Manager showed that scaling by
  // > VideoProcessor used less cpu than just direct composition.

  // [1]: KG: I bet whenever we need a colorspace transform, we need to pull out
  //      to RGB. Otherwise we might need to YUV1->RGB1->RGB2->YUV2, which is
  //      maybe-wasteful. Maybe it's possible if everything is a linear
  //      transform to multiply it all into one matrix? Gamma translation is
  //      non-linear though, and those cannot be a matrix.

  // So, in order, try:
  // * VideoProcessor w/ YUV output
  // * VideoProcessor w/ RGB output
  // * GLBlitHelper (RGB output)
  // Because these Src->Dest differently, we need to be stateful.
  // However, if re-using the previous state fails, we can try from the top of
  // the list again.

  const auto CallBlit = [&]() {
    if (mDest->plan.videoProcessor) {
      return CallVideoProcessorBlt();
    }
    MOZ_RELEASE_ASSERT(mDest->plan.blitHelper);
    return CallBlitHelper();
  };

  bool needsPresent = mSrc->needsPresent;
  if (!mDest || mDest->needsPresent) {
    needsPresent = true;
  }

  if (needsPresent) {
    // Reuse previous method?
    if (mDest) {
      if (!CallBlit()) {
        mDest = Nothing();
      }
    }

    if (!mDest) {
      mDest.emplace();
      mDest->srcSpace = mSrc->space;
      mDest->plan = ChooseCspaceTransformPlan(mDest->srcSpace);

      const auto device = mDCLayerTree->GetDevice();
      if (mDest->plan.videoProcessor) {
        const auto& plan = *mDest->plan.videoProcessor;
        if (mOverlayFormat && plan.dstYuvSpace) {
          mDest->dest = CreateSwapChain(*device, dstSize, *mOverlayFormat,
                                        *plan.dstYuvSpace);
          // We need to check if this works. If it does, we're going to
          // immediately redundently call this again below, but that's ok.
          if (!CallVideoProcessorBlt()) {
            mDest->dest.reset();
          }
        }
        if (!mDest->dest) {
          mDest->dest = CreateSwapChain(
              *device, dstSize, DXGI_FORMAT_B8G8R8A8_UNORM, plan.dstRgbSpace);
        }
      } else if (mDest->plan.blitHelper) {
        const auto& plan = *mDest->plan.blitHelper;
        mDest->dest = CreateSwapChain(*device, dstSize, plan.dstDxgiFormat,
                                      plan.dstDxgiSpace);
      }
      MOZ_ASSERT(mDest->dest);
      mVisual->SetContent(mDest->dest->swapChain);
    }

    if (!CallBlit()) {
      RenderThread::Get()->NotifyWebRenderError(
          wr::WebRenderError::VIDEO_OVERLAY);
      return {};
    }

    const auto hr = mDest->dest->swapChain->Present(0, 0);
    if (FAILED(hr)) {
      gfxCriticalNoteOnce << "video Present failed: " << gfx::hexa(hr);
    }

    mSrc->needsPresent = false;
    mDest->needsPresent = false;
  }

  // -

  return Some(presentationTransform);
}

static Maybe<DCSurfaceSwapChain::Dest> CreateSwapChain(
    ID3D11Device& device, const gfx::IntSize aSize, const DXGI_FORMAT aFormat,
    const DXGI_COLOR_SPACE_TYPE aColorSpace) {
  auto swapChain = DCSurfaceSwapChain::Dest();
  swapChain.size = aSize;
  swapChain.format = aFormat;
  swapChain.space = aColorSpace;

  RefPtr<IDXGIDevice2> dxgiDevice;
  device.QueryInterface((IDXGIDevice2**)getter_AddRefs(dxgiDevice));

  RefPtr<IDXGIFactoryMedia> dxgiFactoryMedia;
  {
    RefPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(getter_AddRefs(adapter));
    adapter->GetParent(
        IID_PPV_ARGS((IDXGIFactoryMedia**)getter_AddRefs(dxgiFactoryMedia)));
  }
  RefPtr<IDXGIFactory2> dxgiFactory2;
  {
    RefPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(getter_AddRefs(adapter));
    adapter->GetParent(
        IID_PPV_ARGS((IDXGIFactory2**)getter_AddRefs(dxgiFactory2)));
  }

  swapChain.swapChainSurfaceHandle =
      MakeUnique<RaiiHANDLE>(gfx::DeviceManagerDx::CreateDCompSurfaceHandle());
  if (!*swapChain.swapChainSurfaceHandle) {
    gfxCriticalNote << "Failed to create DCompSurfaceHandle";
    return {};
  }

  {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = swapChain.size.width;
    desc.Height = swapChain.size.height;
    desc.Format = swapChain.format;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferCount = 2;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;
    if (IsYuv(desc.Format)) {
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;
    }
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    RefPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr;
    bool useSurfaceHandle = true;
    if (useSurfaceHandle) {
      hr = dxgiFactoryMedia->CreateSwapChainForCompositionSurfaceHandle(
          &device, *swapChain.swapChainSurfaceHandle, &desc, nullptr,
          getter_AddRefs(swapChain1));
    } else {
      // CreateSwapChainForComposition does not support
      // e.g. DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO.
      MOZ_ASSERT(!desc.Flags);
      hr = dxgiFactory2->CreateSwapChainForComposition(
          &device, &desc, nullptr, getter_AddRefs(swapChain1));
    }
    if (FAILED(hr)) {
      gfxCriticalNote << "Failed to create output SwapChain: " << gfx::hexa(hr)
                      << " " << swapChain.size;
      return {};
    }

    swapChain1->QueryInterface(
        static_cast<IDXGISwapChain3**>(getter_AddRefs(swapChain.swapChain)));
    if (!swapChain.swapChain) {
      gfxCriticalNote << "Failed to get IDXGISwapChain3";
      return {};
    }

    const auto cmsMode = GfxColorManagementMode();
    const bool doColorManagement = cmsMode != CMSMode::Off;
    if (doColorManagement) {
      hr = swapChain.swapChain->SetColorSpace1(aColorSpace);
      if (FAILED(hr)) {
        gfxCriticalNote << "SetColorSpace1 failed: " << gfx::hexa(hr);
        return {};
      }
    }
  }

  return Some(std::move(swapChain));
}

bool DCSurfaceSwapChain::CallVideoProcessorBlt() const {
  MOZ_RELEASE_ASSERT(mSrc);
  MOZ_RELEASE_ASSERT(mDest);
  MOZ_RELEASE_ASSERT(mDest->dest);

  HRESULT hr;
  const auto videoDevice = mDCLayerTree->GetVideoDevice();
  const auto videoContext = mDCLayerTree->GetVideoContext();
  const auto& texture = mSrc->texture;

  if (!mDCLayerTree->EnsureVideoProcessorAtLeast(mSrc->size,
                                                 mDest->dest->size)) {
    gfxCriticalNote << "EnsureVideoProcessor Failed";
    return false;
  }
  const auto videoProcessor = mDCLayerTree->GetVideoProcessor();
  const auto videoProcessorEnumerator =
      mDCLayerTree->GetVideoProcessorEnumerator();

  // -
  // Set color spaces

  {
    RefPtr<ID3D11VideoContext1> videoContext1;
    videoContext->QueryInterface(
        (ID3D11VideoContext1**)getter_AddRefs(videoContext1));
    if (!videoContext1) {
      gfxCriticalNote << "Failed to get ID3D11VideoContext1";
      return false;
    }
    const auto& plan = *mDest->plan.videoProcessor;
    videoContext1->VideoProcessorSetStreamColorSpace1(videoProcessor, 0,
                                                      plan.srcSpace);
    videoContext1->VideoProcessorSetOutputColorSpace1(videoProcessor,
                                                      mDest->dest->space);
  }

  // -
  // inputView

  RefPtr<ID3D11Texture2D> texture2D = texture->GetD3D11Texture2DWithGL();
  if (!texture2D) {
    gfxCriticalNote << "Failed to get D3D11Texture2D";
    return false;
  }

  if (!mSrc->texture->LockInternal()) {
    gfxCriticalNote << "CallVideoProcessorBlt LockInternal failed.";
    return false;
  }
  const auto unlock = MakeScopeExit([&]() { mSrc->texture->Unlock(); });

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc = {};
  inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  inputDesc.Texture2D.ArraySlice = texture->ArrayIndex();

  RefPtr<ID3D11VideoProcessorInputView> inputView;
  hr = videoDevice->CreateVideoProcessorInputView(
      texture2D, videoProcessorEnumerator, &inputDesc,
      getter_AddRefs(inputView));
  if (FAILED(hr)) {
    gfxCriticalNote << "ID3D11VideoProcessorInputView creation failed: "
                    << gfx::hexa(hr);
    return false;
  }

  // -
  // outputView

  RECT destRect;
  destRect.left = 0;
  destRect.top = 0;
  destRect.right = mDest->dest->size.width;
  destRect.bottom = mDest->dest->size.height;
  // KG: This causes stretching if this doesn't match `srcSize`?

  videoContext->VideoProcessorSetOutputTargetRect(videoProcessor, TRUE,
                                                  &destRect);
  videoContext->VideoProcessorSetStreamDestRect(videoProcessor, 0, TRUE,
                                                &destRect);
  RECT sourceRect;
  sourceRect.left = 0;
  sourceRect.top = 0;
  sourceRect.right = mSrc->size.width;
  sourceRect.bottom = mSrc->size.height;
  videoContext->VideoProcessorSetStreamSourceRect(videoProcessor, 0, TRUE,
                                                  &sourceRect);

  RefPtr<ID3D11VideoProcessorOutputView> outputView;
  {
    RefPtr<ID3D11Texture2D> backBuf;
    mDest->dest->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                      (void**)getter_AddRefs(backBuf));

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = {};
    outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2D.MipSlice = 0;

    hr = videoDevice->CreateVideoProcessorOutputView(
        backBuf, videoProcessorEnumerator, &outputDesc,
        getter_AddRefs(outputView));
    if (FAILED(hr)) {
      gfxCriticalNote << "ID3D11VideoProcessorOutputView creation failed: "
                      << gfx::hexa(hr);
      return false;
    }
  }

  // -
  // VideoProcessorBlt

  D3D11_VIDEO_PROCESSOR_STREAM stream = {};
  stream.Enable = true;
  stream.OutputIndex = 0;
  stream.InputFrameOrField = 0;
  stream.PastFrames = 0;
  stream.FutureFrames = 0;
  stream.pInputSurface = inputView.get();

  // KG: I guess we always copy here? The ideal case would instead be us getting
  // a swapchain from dcomp, and pulling buffers out of it for e.g. webgl to
  // render into.
  hr = videoContext->VideoProcessorBlt(videoProcessor, outputView, 0, 1,
                                       &stream);
  if (FAILED(hr)) {
    gfxCriticalNote << "VideoProcessorBlt failed: " << gfx::hexa(hr);
    return false;
  }

  return true;
}

bool DCSurfaceSwapChain::CallBlitHelper() const {
  MOZ_RELEASE_ASSERT(mSrc);
  MOZ_RELEASE_ASSERT(mDest);
  MOZ_RELEASE_ASSERT(mDest->dest);
  const auto& plan = *mDest->plan.blitHelper;

  const auto gl = mDCLayerTree->GetGLContext();
  const auto& blitHelper = gl->BlitHelper();

  const auto Mat3FromImage = [](const wr::WrExternalImage& image,
                                const gfx::IntSize& size) {
    auto ret = gl::Mat3::I();
    ret.at(0, 0) = (image.u1 - image.u0) / size.width;  // e.g. 0.8 - 0.1
    ret.at(0, 2) = image.u0 / size.width;               // e.g. 0.1
    ret.at(1, 1) = (image.v1 - image.v0) / size.height;
    ret.at(1, 2) = image.v0 / size.height;
    return ret;
  };

  // -
  // Bind LUT

  constexpr uint8_t LUT_TEX_UNIT = 3;
  const auto restore3D =
      gl::ScopedSaveMultiTex{gl, {LUT_TEX_UNIT}, LOCAL_GL_TEXTURE_3D};
  const auto restoreExt =
      gl::ScopedSaveMultiTex{gl, {0, 1}, LOCAL_GL_TEXTURE_EXTERNAL};

  if (plan.srcSpace != plan.dstSpace) {
    if (!mDest->lut) {
      mDest->lut = blitHelper->GetColorLutTex({plan.srcSpace, plan.dstSpace});
    }
    MOZ_ASSERT(mDest->lut);
    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + LUT_TEX_UNIT);
    gl->fBindTexture(LOCAL_GL_TEXTURE_3D, mDest->lut->name);
  }

  // -
  // Lock and bind src (image1 as needed below)

  const auto image0 = mSrc->texture->Lock(0, gl, wr::ImageRendering::Auto);
  const auto size0 = mSrc->texture->GetSize(0);
  const auto texCoordMat0 = Mat3FromImage(image0, size0);
  const auto unlock = MakeScopeExit([&]() { mSrc->texture->Unlock(); });

  gl->fActiveTexture(LOCAL_GL_TEXTURE0 + 0);
  gl->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, image0.handle);

  // -
  // Bind swapchain RT

  RefPtr<ID3D11Texture2D> backBuf;
  mDest->dest->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    (void**)getter_AddRefs(backBuf));
  MOZ_RELEASE_ASSERT(backBuf);

  bool clearForTesting = false;
  if (clearForTesting) {
    const auto device = mDCLayerTree->GetDevice();
    layers::ClearResource(device, backBuf, {1, 0, 1, 1});
    return true;
  }

  const auto gle = gl::GLContextEGL::Cast(gl);

  const auto fb = gl::ScopedFramebuffer(gl);
  const auto bindFb = gl::ScopedBindFramebuffer(gl, fb);

  const auto rb = gl::ScopedRenderbuffer(gl);
  {
    const auto bindRb = gl::ScopedBindRenderbuffer(gl, rb);

    const EGLint attribs[] = {LOCAL_EGL_NONE};
    const auto image = gle->mEgl->fCreateImage(
        0, LOCAL_EGL_D3D11_TEXTURE_ANGLE,
        reinterpret_cast<EGLClientBuffer>(backBuf.get()), attribs);
    MOZ_RELEASE_ASSERT(image);
    gl->fEGLImageTargetRenderbufferStorage(LOCAL_GL_RENDERBUFFER, image);
    gle->mEgl->fDestroyImage(image);  // Release as soon as attached to RB.

    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                 LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER, rb);
    MOZ_ASSERT(gl->IsFramebufferComplete(fb));
  }

  // -
  // Draw

  auto lutUintIfNeeded = Some(LUT_TEX_UNIT);
  auto fragConvert = gl::kFragConvert_ColorLut;
  if (!mDest->lut) {
    lutUintIfNeeded = Nothing();
    fragConvert = gl::kFragConvert_None;
  }

  const auto baseArgs = gl::DrawBlitProg::BaseArgs{
      texCoordMat0, false, mDest->dest->size, {}, lutUintIfNeeded,
  };
  Maybe<gl::DrawBlitProg::YUVArgs> yuvArgs;
  auto fragSample = gl::kFragSample_OnePlane;

  if (IsYuv(mSrc->format)) {
    const auto image1 = mSrc->texture->Lock(1, gl, wr::ImageRendering::Auto);
    const auto size1 = mSrc->texture->GetSize(1);
    const auto texCoordMat1 = Mat3FromImage(image1, size1);

    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + 1);
    gl->fBindTexture(LOCAL_GL_TEXTURE_EXTERNAL, image1.handle);

    yuvArgs = Some(gl::DrawBlitProg::YUVArgs{texCoordMat1, {}});
    fragSample = gl::kFragSample_TwoPlane;
  }

  const auto dbp = blitHelper->GetDrawBlitProg(
      {gl::kFragHeader_TexExt, {fragSample, fragConvert}});
  dbp->Draw(baseArgs, yuvArgs.ptrOr(nullptr));

  return true;
}

// -

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
  update_rect.left = aSurfaceOffset.x + aDirtyRect.min.x;
  update_rect.top = aSurfaceOffset.y + aDirtyRect.min.y;
  update_rect.right = aSurfaceOffset.x + aDirtyRect.max.x;
  update_rect.bottom = aSurfaceOffset.y + aDirtyRect.max.y;

  hr = aCompositionSurface->BeginDraw(&update_rect, __uuidof(ID3D11Texture2D),
                                      (void**)getter_AddRefs(backBuf), &offset);
  if (FAILED(hr)) {
    LayoutDeviceIntRect rect = widget::WinUtils::ToIntRect(update_rect);

    gfxCriticalNote << "DCompositionSurface::BeginDraw failed: "
                    << gfx::hexa(hr) << " " << rect;
    RenderThread::Get()->HandleWebRenderError(WebRenderError::BEGIN_DRAW);
    return false;
  }

  // DC includes the origin of the dirty / update rect in the draw offset,
  // undo that here since WR expects it to be an absolute offset.
  offset.x -= aDirtyRect.min.x;
  offset.y -= aDirtyRect.min.y;

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
