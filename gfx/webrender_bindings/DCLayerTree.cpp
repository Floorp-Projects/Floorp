/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCLayerTree.h"

// -

#include <d3d11.h>
#include <dcomp.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

// -

#include "gfxWindowsPlatform.h"
#include "GLContext.h"
#include "GLContextEGL.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderD3D11TextureHost.h"
#include "mozilla/webrender/RenderDcompSurfaceTextureHost.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/Telemetry.h"
#include "nsPrintfCString.h"
#include "WinUtils.h"

// -

#if defined(__MINGW32__)  // 64 defines both 32 and 64
// We need to fake some things, while we wait on updates to mingw's dcomp.h
// header. Just enough that we can successfully fail to work there.
#  define MOZ_MINGW_DCOMP_H_INCOMPLETE
struct IDCompositionColorMatrixEffect : public IDCompositionFilterEffect {};
struct IDCompositionTableTransferEffect : public IDCompositionFilterEffect {};
#endif  // defined(__MINGW32__)

namespace mozilla {
namespace wr {

extern LazyLogModule gRenderThreadLog;
#define LOG(...) MOZ_LOG(gRenderThreadLog, LogLevel::Debug, (__VA_ARGS__))

#define LOG_H(msg, ...)                   \
  MOZ_LOG(gDcompSurface, LogLevel::Debug, \
          ("DCSurfaceHandle=%p, " msg, this, ##__VA_ARGS__))

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

  if (gfx::gfxVars::UseWebRenderDCompVideoHwOverlayWin() ||
      gfx::gfxVars::UseWebRenderDCompVideoSwOverlayWin()) {
    if (!InitializeVideoOverlaySupport()) {
      RenderThread::Get()->HandleWebRenderError(WebRenderError::VIDEO_OVERLAY);
    }
  }
  if (!sGpuOverlayInfo) {
    // Set default if sGpuOverlayInfo was not set.
    sGpuOverlayInfo = MakeUnique<GpuOverlayInfo>();
  }

  // Initialize SwapChainInfo
  SupportsSwapChainTearing();

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

void DCLayerTree::CompositorBeginFrame() {
  mCurrentFrame++;
  mUsedOverlayTypesInFrame = DCompOverlayTypes::NO_OVERLAY;
}

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
      mRootVisual->AddVisual(visual, false, nullptr);
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

  if (!StaticPrefs::gfx_webrender_dcomp_video_check_slow_present()) {
    return;
  }

  // Disable video overlay if mCompositionDevice->Commit() with video overlay is
  // too slow. It drops fps.

  const auto maxCommitWaitDurationMs = 20;
  const auto maxSlowCommitCount = 5;
  const auto commitDurationMs =
      static_cast<uint32_t>((end - start).ToMilliseconds());

  nsPrintfCString marker("CommitWait overlay %u %ums ",
                         (uint8_t)mUsedOverlayTypesInFrame, commitDurationMs);
  PROFILER_MARKER_TEXT("CommitWait", GRAPHICS, {}, marker);

  if (mUsedOverlayTypesInFrame != DCompOverlayTypes::NO_OVERLAY &&
      commitDurationMs > maxCommitWaitDurationMs) {
    mSlowCommitCount++;
  } else {
    mSlowCommitCount = 0;
  }

  if (mSlowCommitCount <= maxSlowCommitCount) {
    return;
  }

  if (mUsedOverlayTypesInFrame & DCompOverlayTypes::SOFTWARE_DECODED_VIDEO) {
    gfxCriticalNoteOnce << "Sw video swapchain present is slow";
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_SW_OVERLAY);
  }
  if (mUsedOverlayTypesInFrame & DCompOverlayTypes::HARDWARE_DECODED_VIDEO) {
    gfxCriticalNoteOnce << "Hw video swapchain present is slow";
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_HW_OVERLAY);
  }
}

void DCLayerTree::Bind(wr::NativeTileId aId, wr::DeviceIntPoint* aOffset,
                       uint32_t* aFboId, wr::DeviceIntRect aDirtyRect,
                       wr::DeviceIntRect aValidRect) {
  auto surface = GetSurface(aId.surface_id);
  auto tile = surface->GetTile(aId.x, aId.y);
  wr::DeviceIntPoint targetOffset{0, 0};

  // If tile owns an IDCompositionSurface we use it, otherwise we're using an
  // IDCompositionVirtualSurface owned by the DCSurface.
  RefPtr<IDCompositionSurface> compositionSurface;
  if (surface->mIsVirtualSurface) {
    gfx::IntRect validRect(aValidRect.min.x, aValidRect.min.y,
                           aValidRect.width(), aValidRect.height());
    if (!tile->mValidRect.IsEqualEdges(validRect)) {
      tile->mValidRect = validRect;
      surface->DirtyAllocatedRect();
    }
    wr::DeviceIntSize tileSize = surface->GetTileSize();
    compositionSurface = surface->GetCompositionSurface();
    wr::DeviceIntPoint virtualOffset = surface->GetVirtualOffset();
    targetOffset.x = virtualOffset.x + tileSize.width * aId.x;
    targetOffset.y = virtualOffset.y + tileSize.height * aId.y;
  } else {
    compositionSurface = tile->Bind(aValidRect);
  }

  if (tile->mNeedsFullDraw) {
    // dcomp requires that the first BeginDraw on a non-virtual surface is the
    // full size of the pixel buffer.
    auto tileSize = surface->GetTileSize();
    aDirtyRect.min.x = 0;
    aDirtyRect.min.y = 0;
    aDirtyRect.max.x = tileSize.width;
    aDirtyRect.max.y = tileSize.height;
    tile->mNeedsFullDraw = false;
  }

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

  bool isVirtualSurface =
      StaticPrefs::gfx_webrender_dcomp_use_virtual_surfaces_AtStartup();
  auto surface = MakeUnique<DCSurface>(aTileSize, aVirtualOffset,
                                       isVirtualSurface, aIsOpaque, this);
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

  auto surface = MakeUnique<DCExternalSurfaceWrapper>(aIsOpaque, this);
  if (!surface->Initialize()) {
    gfxCriticalNote << "Failed to initialize DCExternalSurfaceWrapper: "
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

void DCLayerTree::CreateTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY) {
  auto surface = GetSurface(aId);
  surface->CreateTile(aX, aY);
}

void DCLayerTree::DestroyTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY) {
  auto surface = GetSurface(aId);
  surface->DestroyTile(aX, aY);
}

void DCLayerTree::AttachExternalImage(wr::NativeSurfaceId aId,
                                      wr::ExternalImageId aExternalImage) {
  auto surface_it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(surface_it != mDCSurfaces.end());
  surface_it->second->AttachExternalImage(aExternalImage);
}

void DCExternalSurfaceWrapper::AttachExternalImage(
    wr::ExternalImageId aExternalImage) {
  if (auto* surface = EnsureSurfaceForExternalImage(aExternalImage)) {
    surface->AttachExternalImage(aExternalImage);
  }
}

template <class ToT>
struct QI {
  template <class FromT>
  [[nodiscard]] static inline RefPtr<ToT> From(FromT* const from) {
    RefPtr<ToT> to;
    (void)from->QueryInterface(static_cast<ToT**>(getter_AddRefs(to)));
    return to;
  }
};

DCSurface* DCExternalSurfaceWrapper::EnsureSurfaceForExternalImage(
    wr::ExternalImageId aExternalImage) {
  if (mSurface) {
    return mSurface.get();
  }

  // Create a new surface based on the texture type.
  RenderTextureHost* texture =
      RenderThread::Get()->GetRenderTexture(aExternalImage);
  if (texture && texture->AsRenderDXGITextureHost()) {
    mSurface.reset(new DCSurfaceVideo(mIsOpaque, mDCLayerTree));
    if (!mSurface->Initialize()) {
      gfxCriticalNote << "Failed to initialize DCSurfaceVideo: "
                      << wr::AsUint64(aExternalImage);
      mSurface = nullptr;
    }
  } else if (texture && texture->AsRenderDcompSurfaceTextureHost()) {
    mSurface.reset(new DCSurfaceHandle(mIsOpaque, mDCLayerTree));
    if (!mSurface->Initialize()) {
      gfxCriticalNote << "Failed to initialize DCSurfaceHandle: "
                      << wr::AsUint64(aExternalImage);
      mSurface = nullptr;
    }
  }
  if (!mSurface) {
    gfxCriticalNote << "Failed to create a surface for external image: "
                    << gfx::hexa(texture);
    return nullptr;
  }

  // Add surface's visual which will contain video data to our root visual.
  const auto surfaceVisual = mSurface->GetVisual();
  mVisual->AddVisual(surfaceVisual, true, nullptr);

  // -
  // Apply color management.

  [&]() {
    const auto cmsMode = GfxColorManagementMode();
    if (cmsMode == CMSMode::Off) return;

    const auto dcomp = mDCLayerTree->GetCompositionDevice();
    const auto dcomp3 = QI<IDCompositionDevice3>::From(dcomp);
    if (!dcomp3) {
      NS_WARNING(
          "No IDCompositionDevice3, cannot use dcomp for color management.");
      return;
    }

    // -

    const auto cspace = [&]() {
      const auto rangedCspace = texture->GetYUVColorSpace();
      const auto info = FromYUVRangedColorSpace(rangedCspace);
      auto ret = ToColorSpace2(info.space);
      if (ret == gfx::ColorSpace2::Display && cmsMode == CMSMode::All) {
        ret = gfx::ColorSpace2::SRGB;
      }
      return ret;
    }();

    const bool rec709GammaAsSrgb =
        StaticPrefs::gfx_color_management_rec709_gamma_as_srgb();
    const bool rec2020GammaAsRec709 =
        StaticPrefs::gfx_color_management_rec2020_gamma_as_rec709();

    auto cspaceDesc = color::ColorspaceDesc{};
    switch (cspace) {
      case gfx::ColorSpace2::Display:
        return;  // No color management needed!
      case gfx::ColorSpace2::SRGB:
        cspaceDesc.chrom = color::Chromaticities::Srgb();
        cspaceDesc.tf = color::PiecewiseGammaDesc::Srgb();
        break;

      case gfx::ColorSpace2::DISPLAY_P3:
        cspaceDesc.chrom = color::Chromaticities::DisplayP3();
        cspaceDesc.tf = color::PiecewiseGammaDesc::DisplayP3();
        break;

      case gfx::ColorSpace2::BT601_525:
        cspaceDesc.chrom = color::Chromaticities::Rec601_525_Ntsc();
        if (rec709GammaAsSrgb) {
          cspaceDesc.tf = color::PiecewiseGammaDesc::Srgb();
        } else {
          cspaceDesc.tf = color::PiecewiseGammaDesc::Rec709();
        }
        break;

      case gfx::ColorSpace2::BT709:
        cspaceDesc.chrom = color::Chromaticities::Rec709();
        if (rec709GammaAsSrgb) {
          cspaceDesc.tf = color::PiecewiseGammaDesc::Srgb();
        } else {
          cspaceDesc.tf = color::PiecewiseGammaDesc::Rec709();
        }
        break;

      case gfx::ColorSpace2::BT2020:
        cspaceDesc.chrom = color::Chromaticities::Rec2020();
        if (rec2020GammaAsRec709 && rec709GammaAsSrgb) {
          cspaceDesc.tf = color::PiecewiseGammaDesc::Srgb();
        } else if (rec2020GammaAsRec709) {
          cspaceDesc.tf = color::PiecewiseGammaDesc::Rec709();
        } else {
          // Just Rec709 with slightly more precision.
          cspaceDesc.tf = color::PiecewiseGammaDesc::Rec2020_12bit();
        }
        break;
    }

    const auto cprofileIn = color::ColorProfileDesc::From(cspaceDesc);
    auto cprofileOut = mDCLayerTree->OutputColorProfile();
    bool pretendSrgb = true;
    if (pretendSrgb) {
      cprofileOut = color::ColorProfileDesc::From({
          color::Chromaticities::Srgb(),
          color::PiecewiseGammaDesc::Srgb(),
      });
    }
    const auto conversion = color::ColorProfileConversionDesc::From({
        .src = cprofileIn,
        .dst = cprofileOut,
    });

    // -

    auto chain = ColorManagementChain::From(*dcomp3, conversion);
    mCManageChain = Some(chain);

    surfaceVisual->SetEffect(mCManageChain->last.get());
  }();

  return mSurface.get();
}

void DCExternalSurfaceWrapper::PresentExternalSurface(gfx::Matrix& aTransform) {
  MOZ_ASSERT(mSurface);
  if (auto* surface = mSurface->AsDCSurfaceVideo()) {
    if (surface->CalculateSwapChainSize(aTransform)) {
      surface->PresentVideo();
    }
  } else if (auto* surface = mSurface->AsDCSurfaceHandle()) {
    surface->PresentSurfaceHandle();
  }
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

  float sx = aTransform.scale.x;
  float sy = aTransform.scale.y;
  float tx = aTransform.offset.x;
  float ty = aTransform.offset.y;
  gfx::Matrix transform(sx, 0.0, 0.0, sy, tx, ty);

  surface->PresentExternalSurface(transform);

  transform.PreTranslate(-virtualOffset.x, -virtualOffset.y);

  // The DirectComposition API applies clipping *before* any
  // transforms/offset, whereas we want the clip applied after. Right now, we
  // only support rectilinear transforms, and then we transform our clip into
  // pre-transform coordinate space for it to be applied there.
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

bool DCLayerTree::EnsureVideoProcessor(const gfx::IntSize& aInputSize,
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
  // By default, the driver might perform certain processing tasks
  // automatically
  mVideoContext->VideoProcessorSetStreamAutoProcessingMode(mVideoProcessor, 0,
                                                           FALSE);

  mVideoInputSize = aInputSize;
  mVideoOutputSize = aOutputSize;

  return true;
}

bool DCLayerTree::SupportsHardwareOverlays() {
  return sGpuOverlayInfo->mSupportsHardwareOverlays;
}

bool DCLayerTree::SupportsSwapChainTearing() {
  RefPtr<ID3D11Device> device = mDevice;
  static const bool supported = [device] {
    RefPtr<IDXGIDevice> dxgiDevice;
    RefPtr<IDXGIAdapter> adapter;
    device->QueryInterface((IDXGIDevice**)getter_AddRefs(dxgiDevice));
    dxgiDevice->GetAdapter(getter_AddRefs(adapter));

    RefPtr<IDXGIFactory5> dxgiFactory;
    HRESULT hr = adapter->GetParent(
        IID_PPV_ARGS((IDXGIFactory5**)getter_AddRefs(dxgiFactory)));
    if (FAILED(hr)) {
      return false;
    }

    BOOL presentAllowTearing = FALSE;
    hr = dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                          &presentAllowTearing,
                                          sizeof(presentAllowTearing));
    if (FAILED(hr)) {
      return false;
    }

    if (auto* gpuParent = gfx::GPUParent::GetSingleton()) {
      gpuParent->NotifySwapChainInfo(
          layers::SwapChainInfo(!!presentAllowTearing));
    } else if (XRE_IsParentProcess()) {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    }
    return !!presentAllowTearing;
  }();
  return supported;
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

void DCLayerTree::SetUsedOverlayTypeInFrame(DCompOverlayTypes aTypes) {
  mUsedOverlayTypesInFrame |= aTypes;
}

DCSurface::DCSurface(wr::DeviceIntSize aTileSize,
                     wr::DeviceIntPoint aVirtualOffset, bool aIsVirtualSurface,
                     bool aIsOpaque, DCLayerTree* aDCLayerTree)
    : mIsVirtualSurface(aIsVirtualSurface),
      mDCLayerTree(aDCLayerTree),
      mTileSize(aTileSize),
      mIsOpaque(aIsOpaque),
      mAllocatedRectDirty(true),
      mVirtualOffset(aVirtualOffset) {}

DCSurface::~DCSurface() {}

bool DCSurface::Initialize() {
  // Create a visual for tiles to attach to, whether virtual or not.
  HRESULT hr;
  const auto dCompDevice = mDCLayerTree->GetCompositionDevice();
  hr = dCompDevice->CreateVisual(getter_AddRefs(mVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
  }

  // If virtual surface is enabled, create and attach to visual, in this case
  // the tiles won't own visuals or surfaces.
  if (mIsVirtualSurface) {
    DXGI_ALPHA_MODE alpha_mode =
        mIsOpaque ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_PREMULTIPLIED;

    hr = dCompDevice->CreateVirtualSurface(
        VIRTUAL_SURFACE_SIZE, VIRTUAL_SURFACE_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM,
        alpha_mode, getter_AddRefs(mVirtualSurface));
    MOZ_ASSERT(SUCCEEDED(hr));

    // Bind the surface memory to this visual
    hr = mVisual->SetContent(mVirtualSurface);
    MOZ_ASSERT(SUCCEEDED(hr));
  }

  return true;
}

void DCSurface::CreateTile(int32_t aX, int32_t aY) {
  TileKey key(aX, aY);
  MOZ_RELEASE_ASSERT(mDCTiles.find(key) == mDCTiles.end());

  auto tile = MakeUnique<DCTile>(mDCLayerTree);
  if (!tile->Initialize(aX, aY, mTileSize, mIsVirtualSurface, mIsOpaque,
                        mVisual)) {
    gfxCriticalNote << "Failed to initialize DCTile: " << aX << aY;
    return;
  }

  if (mIsVirtualSurface) {
    mAllocatedRectDirty = true;
  } else {
    mVisual->AddVisual(tile->GetVisual(), false, nullptr);
  }

  mDCTiles[key] = std::move(tile);
}

void DCSurface::DestroyTile(int32_t aX, int32_t aY) {
  TileKey key(aX, aY);
  if (mIsVirtualSurface) {
    mAllocatedRectDirty = true;
  } else {
    auto tile = GetTile(aX, aY);
    mVisual->RemoveVisual(tile->GetVisual());
  }
  mDCTiles.erase(key);
}

void DCSurface::DirtyAllocatedRect() { mAllocatedRectDirty = true; }

void DCSurface::UpdateAllocatedRect() {
  if (mAllocatedRectDirty) {
    if (mVirtualSurface) {
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
    }
    // When not using a virtual surface, we still want to reset this
    mAllocatedRectDirty = false;
  }
}

DCTile* DCSurface::GetTile(int32_t aX, int32_t aY) const {
  TileKey key(aX, aY);
  auto tile_it = mDCTiles.find(key);
  MOZ_RELEASE_ASSERT(tile_it != mDCTiles.end());
  return tile_it->second.get();
}

DCSurfaceVideo::DCSurfaceVideo(bool aIsOpaque, DCLayerTree* aDCLayerTree)
    : DCSurface(wr::DeviceIntSize{}, wr::DeviceIntPoint{}, false, aIsOpaque,
                aDCLayerTree) {}

DCSurfaceVideo::~DCSurfaceVideo() {
  ReleaseDecodeSwapChainResources();
  MOZ_ASSERT(!mSwapChainSurfaceHandle);
}

bool IsYUVSwapChainFormat(DXGI_FORMAT aFormat) {
  if (aFormat == DXGI_FORMAT_NV12 || aFormat == DXGI_FORMAT_YUY2) {
    return true;
  }
  return false;
}

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
      texture->GetFormat() != gfx::SurfaceFormat::NV12) {
    gfxCriticalNote << "Unsupported RenderTexture for overlay: "
                    << gfx::hexa(texture);
    return;
  }

  mRenderTextureHost = texture;
}

bool DCSurfaceVideo::CalculateSwapChainSize(gfx::Matrix& aTransform) {
  if (!mRenderTextureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return false;
  }

  const auto overlayType = mRenderTextureHost->IsSoftwareDecodedVideo()
                               ? DCompOverlayTypes::SOFTWARE_DECODED_VIDEO
                               : DCompOverlayTypes::HARDWARE_DECODED_VIDEO;
  mDCLayerTree->SetUsedOverlayTypeInFrame(overlayType);

  mVideoSize = mRenderTextureHost->AsRenderDXGITextureHost()->GetSize(0);

  // When RenderTextureHost, swapChainSize or VideoSwapChain are updated,
  // DCSurfaceVideo::PresentVideo() needs to be called.
  bool needsToPresent = mPrevTexture != mRenderTextureHost;
  gfx::IntSize swapChainSize = mVideoSize;
  gfx::Matrix transform = aTransform;
  const bool isDRM = mRenderTextureHost->IsFromDRMSource();

  // When video is rendered to axis aligned integer rectangle, video scaling
  // could be done by VideoProcessor
  bool scaleVideoAtVideoProcessor = false;
  if (StaticPrefs::gfx_webrender_dcomp_video_vp_scaling_win_AtStartup() &&
      aTransform.PreservesAxisAlignedRectangles()) {
    gfx::Size scaledSize = gfx::Size(mVideoSize) * aTransform.ScaleFactors();
    gfx::IntSize size(int32_t(std::round(scaledSize.width)),
                      int32_t(std::round(scaledSize.height)));
    if (gfx::FuzzyEqual(scaledSize.width, size.width, 0.1f) &&
        gfx::FuzzyEqual(scaledSize.height, size.height, 0.1f)) {
      scaleVideoAtVideoProcessor = true;
      swapChainSize = size;
    }
  }

  if (scaleVideoAtVideoProcessor) {
    // 4:2:2 subsampled formats like YUY2 must have an even width, and 4:2:0
    // subsampled formats like NV12 must have an even width and height.
    if (swapChainSize.width % 2 == 1) {
      swapChainSize.width += 1;
    }
    if (swapChainSize.height % 2 == 1) {
      swapChainSize.height += 1;
    }
    transform = gfx::Matrix::Translation(aTransform.GetTranslation());
  }

  if (!mVideoSwapChain || mSwapChainSize != swapChainSize || mIsDRM != isDRM) {
    needsToPresent = true;
    ReleaseDecodeSwapChainResources();
    // Update mSwapChainSize before creating SwapChain
    mSwapChainSize = swapChainSize;
    mIsDRM = isDRM;

    auto swapChainFormat = GetSwapChainFormat();
    bool useYUVSwapChain = IsYUVSwapChainFormat(swapChainFormat);
    if (useYUVSwapChain) {
      // Tries to create YUV SwapChain
      CreateVideoSwapChain();
      if (!mVideoSwapChain) {
        mFailedYuvSwapChain = true;
        ReleaseDecodeSwapChainResources();

        gfxCriticalNote << "Fallback to RGB SwapChain";
      }
    }
    // Tries to create RGB SwapChain
    if (!mVideoSwapChain) {
      CreateVideoSwapChain();
    }
  }

  aTransform = transform;

  return needsToPresent;
}

void DCSurfaceVideo::PresentVideo() {
  if (!mRenderTextureHost) {
    return;
  }

  if (!mVideoSwapChain) {
    gfxCriticalNote << "Failed to create VideoSwapChain";
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_OVERLAY);
    return;
  }

  mVisual->SetContent(mVideoSwapChain);

  if (!CallVideoProcessorBlt()) {
    auto swapChainFormat = GetSwapChainFormat();
    bool useYUVSwapChain = IsYUVSwapChainFormat(swapChainFormat);
    if (useYUVSwapChain) {
      mFailedYuvSwapChain = true;
      ReleaseDecodeSwapChainResources();
      return;
    }
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_OVERLAY);
    return;
  }

  auto start = TimeStamp::Now();
  HRESULT hr = mVideoSwapChain->Present(0, 0);
  auto end = TimeStamp::Now();

  if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) {
    gfxCriticalNoteOnce << "video Present failed: " << gfx::hexa(hr);
  }

  mPrevTexture = mRenderTextureHost;

  // Disable video overlay if mVideoSwapChain->Present() is too slow. It drops
  // fps.

  if (!StaticPrefs::gfx_webrender_dcomp_video_check_slow_present()) {
    return;
  }

  const auto maxPresentWaitDurationMs = 2;
  const auto maxSlowPresentCount = 5;
  const auto presentDurationMs =
      static_cast<uint32_t>((end - start).ToMilliseconds());
  const auto overlayType = mRenderTextureHost->IsSoftwareDecodedVideo()
                               ? DCompOverlayTypes::SOFTWARE_DECODED_VIDEO
                               : DCompOverlayTypes::HARDWARE_DECODED_VIDEO;

  nsPrintfCString marker("PresentWait overlay %u %ums ", (uint8_t)overlayType,
                         presentDurationMs);
  PROFILER_MARKER_TEXT("PresentWait", GRAPHICS, {}, marker);

  if (presentDurationMs > maxPresentWaitDurationMs) {
    mSlowPresentCount++;
  } else {
    mSlowPresentCount = 0;
  }

  if (mSlowPresentCount <= maxSlowPresentCount) {
    return;
  }

  if (overlayType == DCompOverlayTypes::SOFTWARE_DECODED_VIDEO) {
    gfxCriticalNoteOnce << "Sw video swapchain present is slow";
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_SW_OVERLAY);
  } else {
    gfxCriticalNoteOnce << "Hw video swapchain present is slow";
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_HW_OVERLAY);
  }
}

DXGI_FORMAT DCSurfaceVideo::GetSwapChainFormat() {
  if (mFailedYuvSwapChain || !mDCLayerTree->SupportsHardwareOverlays()) {
    return DXGI_FORMAT_B8G8R8A8_UNORM;
  }
  return mDCLayerTree->GetOverlayFormatForSDR();
}

bool DCSurfaceVideo::CreateVideoSwapChain() {
  MOZ_ASSERT(mRenderTextureHost);

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

  auto swapChainFormat = GetSwapChainFormat();
  bool useTripleBuffering =
      StaticPrefs::gfx_webrender_dcomp_video_force_triple_buffering();

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  desc.Width = mSwapChainSize.width;
  desc.Height = mSwapChainSize.height;
  desc.Format = swapChainFormat;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = useTripleBuffering ? 3 : 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.Flags = DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;
  if (IsYUVSwapChainFormat(swapChainFormat)) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO;
  }
  if (mIsDRM) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
  }
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

  HRESULT hr;
  hr = dxgiFactoryMedia->CreateSwapChainForCompositionSurfaceHandle(
      device, mSwapChainSurfaceHandle, &desc, nullptr,
      getter_AddRefs(mVideoSwapChain));

  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create video SwapChain: " << gfx::hexa(hr)
                    << " " << mSwapChainSize;
    return false;
  }

  mSwapChainFormat = swapChainFormat;
  return true;
}

// TODO: Replace with YUVRangedColorSpace
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

static Maybe<DXGI_COLOR_SPACE_TYPE> GetSourceDXGIColorSpace(
    const gfx::YUVRangedColorSpace aYUVColorSpace) {
  const auto info = FromYUVRangedColorSpace(aYUVColorSpace);
  return GetSourceDXGIColorSpace(info.space, info.range);
}

static void SetNvidiaVideoSuperRes(ID3D11VideoContext* videoContext,
                                   ID3D11VideoProcessor* videoProcessor,
                                   bool enabled) {
  LOG("SetNvidiaVideoSuperRes() enabled=%d", enabled);

  // Undocumented NVIDIA driver constants
  constexpr GUID nvGUID = {0xD43CE1B3,
                           0x1F4B,
                           0x48AC,
                           {0xBA, 0xEE, 0xC3, 0xC2, 0x53, 0x75, 0xE6, 0xF7}};

  constexpr UINT nvExtensionVersion = 0x1;
  constexpr UINT nvExtensionMethodSuperResolution = 0x2;
  struct {
    UINT version;
    UINT method;
    UINT enable;
  } streamExtensionInfo = {nvExtensionVersion, nvExtensionMethodSuperResolution,
                           enabled ? 1u : 0};

  HRESULT hr;
  hr = videoContext->VideoProcessorSetStreamExtension(
      videoProcessor, 0, &nvGUID, sizeof(streamExtensionInfo),
      &streamExtensionInfo);

  // Ignore errors as could be unsupported
  if (FAILED(hr)) {
    LOG("SetNvidiaVideoSuperRes() error: %lx", hr);
    return;
  }
}

static UINT GetVendorId(ID3D11VideoDevice* const videoDevice) {
  RefPtr<IDXGIDevice> dxgiDevice;
  RefPtr<IDXGIAdapter> adapter;
  videoDevice->QueryInterface((IDXGIDevice**)getter_AddRefs(dxgiDevice));
  dxgiDevice->GetAdapter(getter_AddRefs(adapter));

  DXGI_ADAPTER_DESC adapterDesc;
  adapter->GetDesc(&adapterDesc);

  return adapterDesc.VendorId;
}

bool DCSurfaceVideo::CallVideoProcessorBlt() {
  MOZ_ASSERT(mRenderTextureHost);

  HRESULT hr;
  const auto videoDevice = mDCLayerTree->GetVideoDevice();
  const auto videoContext = mDCLayerTree->GetVideoContext();
  const auto texture = mRenderTextureHost->AsRenderDXGITextureHost();

  Maybe<DXGI_COLOR_SPACE_TYPE> sourceColorSpace =
      GetSourceDXGIColorSpace(texture->GetYUVColorSpace());
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

  if (!mDCLayerTree->EnsureVideoProcessor(mVideoSize, mSwapChainSize)) {
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

  DXGI_COLOR_SPACE_TYPE outputColorSpace =
      IsYUVSwapChainFormat(mSwapChainFormat)
          ? inputColorSpace
          : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  hr = swapChain3->SetColorSpace1(outputColorSpace);
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "SetColorSpace1 failed: " << gfx::hexa(hr);
    RenderThread::Get()->NotifyWebRenderError(
        wr::WebRenderError::VIDEO_OVERLAY);
    return false;
  }
  videoContext1->VideoProcessorSetOutputColorSpace1(videoProcessor,
                                                    outputColorSpace);

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
  sourceRect.right = mVideoSize.width;
  sourceRect.bottom = mVideoSize.height;
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

  const UINT vendorId = GetVendorId(videoDevice);
  if (vendorId == 0x10DE &&
      StaticPrefs::gfx_webrender_super_resolution_nvidia_AtStartup()) {
    SetNvidiaVideoSuperRes(videoContext, videoProcessor, true);
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
}

DCSurfaceHandle::DCSurfaceHandle(bool aIsOpaque, DCLayerTree* aDCLayerTree)
    : DCSurface(wr::DeviceIntSize{}, wr::DeviceIntPoint{}, false, aIsOpaque,
                aDCLayerTree) {}

void DCSurfaceHandle::AttachExternalImage(wr::ExternalImageId aExternalImage) {
  RenderTextureHost* texture =
      RenderThread::Get()->GetRenderTexture(aExternalImage);
  RenderDcompSurfaceTextureHost* renderTexture =
      texture ? texture->AsRenderDcompSurfaceTextureHost() : nullptr;
  if (!renderTexture) {
    gfxCriticalNote << "Unsupported RenderTexture for DCSurfaceHandle: "
                    << gfx::hexa(texture);
    return;
  }

  const auto handle = renderTexture->GetDcompSurfaceHandle();
  if (GetSurfaceHandle() == handle) {
    return;
  }

  LOG_H("AttachExternalImage, ext-image=%" PRIu64 ", texture=%p, handle=%p",
        wr::AsUint64(aExternalImage), renderTexture, handle);
  mDcompTextureHost = renderTexture;
}

HANDLE DCSurfaceHandle::GetSurfaceHandle() const {
  if (mDcompTextureHost) {
    return mDcompTextureHost->GetDcompSurfaceHandle();
  }
  return nullptr;
}

IDCompositionSurface* DCSurfaceHandle::EnsureSurface() {
  if (auto* surface = mDcompTextureHost->GetSurface()) {
    return surface;
  }

  // Texture host hasn't created the surface yet, ask it to create a new one.
  RefPtr<IDCompositionDevice> device;
  HRESULT hr = mDCLayerTree->GetCompositionDevice()->QueryInterface(
      (IDCompositionDevice**)getter_AddRefs(device));
  if (FAILED(hr)) {
    gfxCriticalNote
        << "Failed to convert IDCompositionDevice2 to IDCompositionDevice: "
        << gfx::hexa(hr);
    return nullptr;
  }

  return mDcompTextureHost->CreateSurfaceFromDevice(device);
}

void DCSurfaceHandle::PresentSurfaceHandle() {
  LOG_H("PresentSurfaceHandle");
  if (IDCompositionSurface* surface = EnsureSurface()) {
    LOG_H("Set surface %p to visual", surface);
    mVisual->SetContent(surface);
  } else {
    mVisual->SetContent(nullptr);
  }
}

DCTile::DCTile(DCLayerTree* aDCLayerTree) : mDCLayerTree(aDCLayerTree) {}

DCTile::~DCTile() {}

bool DCTile::Initialize(int aX, int aY, wr::DeviceIntSize aSize,
                        bool aIsVirtualSurface, bool aIsOpaque,
                        RefPtr<IDCompositionVisual2> mSurfaceVisual) {
  if (aSize.width <= 0 || aSize.height <= 0) {
    return false;
  }

  mSize = aSize;
  mIsOpaque = aIsOpaque;
  mIsVirtualSurface = aIsVirtualSurface;
  mNeedsFullDraw = !aIsVirtualSurface;

  if (aIsVirtualSurface) {
    // Initially, the entire tile is considered valid, unless it is set by
    // the SetTileProperties method.
    mValidRect.x = 0;
    mValidRect.y = 0;
    mValidRect.width = aSize.width;
    mValidRect.height = aSize.height;
  } else {
    HRESULT hr;
    const auto dCompDevice = mDCLayerTree->GetCompositionDevice();
    // Create the visual and put it in the tree under the surface visual
    hr = dCompDevice->CreateVisual(getter_AddRefs(mVisual));
    if (FAILED(hr)) {
      gfxCriticalNote << "Failed to CreateVisual for DCTile: " << gfx::hexa(hr);
      return false;
    }
    mSurfaceVisual->AddVisual(mVisual, false, nullptr);
    // Position the tile relative to the surface visual
    mVisual->SetOffsetX(aX * aSize.width);
    mVisual->SetOffsetY(aY * aSize.height);
    // Clip the visual so it doesn't show anything until we update it
    D2D_RECT_F clip = {0, 0, 0, 0};
    mVisual->SetClip(clip);
    // Create the underlying pixel buffer.
    mCompositionSurface = CreateCompositionSurface(aSize, aIsOpaque);
    if (!mCompositionSurface) {
      return false;
    }
    hr = mVisual->SetContent(mCompositionSurface);
    if (FAILED(hr)) {
      gfxCriticalNote << "Failed to SetContent for DCTile: " << gfx::hexa(hr);
      return false;
    }
  }

  return true;
}

RefPtr<IDCompositionSurface> DCTile::CreateCompositionSurface(
    wr::DeviceIntSize aSize, bool aIsOpaque) {
  HRESULT hr;
  const auto dCompDevice = mDCLayerTree->GetCompositionDevice();
  const auto alphaMode =
      aIsOpaque ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_PREMULTIPLIED;
  RefPtr<IDCompositionSurface> compositionSurface;

  hr = dCompDevice->CreateSurface(aSize.width, aSize.height,
                                  DXGI_FORMAT_R8G8B8A8_UNORM, alphaMode,
                                  getter_AddRefs(compositionSurface));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to CreateSurface for DCTile: " << gfx::hexa(hr);
    return nullptr;
  }
  return compositionSurface;
}

RefPtr<IDCompositionSurface> DCTile::Bind(wr::DeviceIntRect aValidRect) {
  if (mVisual != nullptr) {
    // Tile owns a visual, set the size of the visual to match the portion we
    // want to be visible.
    D2D_RECT_F clip_rect;
    clip_rect.left = aValidRect.min.x;
    clip_rect.top = aValidRect.min.y;
    clip_rect.right = aValidRect.max.x;
    clip_rect.bottom = aValidRect.max.y;
    mVisual->SetClip(clip_rect);
  }
  return mCompositionSurface;
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

// -

color::ColorProfileDesc DCLayerTree::QueryOutputColorProfile() {
  // GPU process can't simply init gfxPlatform, (and we don't need most of it)
  // but we do need gfxPlatform::GetCMSOutputProfile().
  // So we steal what we need through the window:
  const auto outputProfileData =
      gfxWindowsPlatform::GetPlatformCMSOutputProfileData_Impl();

  const auto qcmsProfile = qcms_profile_from_memory(
      outputProfileData.Elements(), outputProfileData.Length());
  const auto release = MakeScopeExit([&]() {
    if (qcmsProfile) {
      qcms_profile_release(qcmsProfile);
    }
  });

  const bool print = gfxEnv::MOZ_GL_SPEW();

  const auto ret = [&]() {
    if (qcmsProfile) {
      return color::ColorProfileDesc::From(*qcmsProfile);
    }
    if (print) {
      printf_stderr(
          "Missing or failed to load display color profile, defaulting to "
          "sRGB.\n");
    }
    const auto MISSING_PROFILE_DEFAULT_SPACE = color::ColorspaceDesc{
        color::Chromaticities::Srgb(),
        color::PiecewiseGammaDesc::Srgb(),
    };
    return color::ColorProfileDesc::From(MISSING_PROFILE_DEFAULT_SPACE);
  }();

  if (print) {
    const auto gammaGuess = color::GuessGamma(ret.linearFromTf.r);
    printf_stderr(
        "Display profile:\n"
        "  Approx Gamma: %f\n"
        "  XYZ-D65 Red  : %f, %f, %f\n"
        "  XYZ-D65 Green: %f, %f, %f\n"
        "  XYZ-D65 Blue : %f, %f, %f\n",
        gammaGuess, ret.xyzd65FromLinearRgb.at(0, 0),
        ret.xyzd65FromLinearRgb.at(0, 1), ret.xyzd65FromLinearRgb.at(0, 2),

        ret.xyzd65FromLinearRgb.at(1, 0), ret.xyzd65FromLinearRgb.at(1, 1),
        ret.xyzd65FromLinearRgb.at(1, 2),

        ret.xyzd65FromLinearRgb.at(2, 0), ret.xyzd65FromLinearRgb.at(2, 1),
        ret.xyzd65FromLinearRgb.at(2, 2));
  }

  return ret;
}

inline D2D1_MATRIX_5X4_F to_D2D1_MATRIX_5X4_F(const color::mat4& m) {
  return D2D1_MATRIX_5X4_F{{{
      m.rows[0][0],
      m.rows[1][0],
      m.rows[2][0],
      m.rows[3][0],
      m.rows[0][1],
      m.rows[1][1],
      m.rows[2][1],
      m.rows[3][1],
      m.rows[0][2],
      m.rows[1][2],
      m.rows[2][2],
      m.rows[3][2],
      m.rows[0][3],
      m.rows[1][3],
      m.rows[2][3],
      m.rows[3][3],
      0,
      0,
      0,
      0,
  }}};
}

ColorManagementChain ColorManagementChain::From(
    IDCompositionDevice3& dcomp,
    const color::ColorProfileConversionDesc& conv) {
  auto ret = ColorManagementChain{};

#if !defined(MOZ_MINGW_DCOMP_H_INCOMPLETE)

  const auto Append = [&](const RefPtr<IDCompositionFilterEffect>& afterLast) {
    if (ret.last) {
      afterLast->SetInput(0, ret.last, 0);
    }
    ret.last = afterLast;
  };

  const auto MaybeAppendColorMatrix = [&](const color::mat4& m) {
    RefPtr<IDCompositionColorMatrixEffect> e;
    if (approx(m, color::mat4::Identity())) return e;
    dcomp.CreateColorMatrixEffect(getter_AddRefs(e));
    MOZ_ASSERT(e);
    if (!e) return e;
    e->SetMatrix(to_D2D1_MATRIX_5X4_F(m));
    Append(e);
    return e;
  };
  const auto MaybeAppendTableTransfer = [&](const color::RgbTransferTables& t) {
    RefPtr<IDCompositionTableTransferEffect> e;
    if (!t.r.size() && !t.g.size() && !t.b.size()) return e;
    dcomp.CreateTableTransferEffect(getter_AddRefs(e));
    MOZ_ASSERT(e);
    if (!e) return e;
    e->SetRedTable(t.r.data(), t.r.size());
    e->SetGreenTable(t.g.data(), t.g.size());
    e->SetBlueTable(t.b.data(), t.b.size());
    Append(e);
    return e;
  };

  ret.srcRgbFromSrcYuv = MaybeAppendColorMatrix(conv.srcRgbFromSrcYuv);
  ret.srcLinearFromSrcTf = MaybeAppendTableTransfer(conv.srcLinearFromSrcTf);
  ret.dstLinearFromSrcLinear =
      MaybeAppendColorMatrix(color::mat4(conv.dstLinearFromSrcLinear));
  ret.dstTfFromDstLinear = MaybeAppendTableTransfer(conv.dstTfFromDstLinear);

#endif  // !defined(MOZ_MINGW_DCOMP_H_INCOMPLETE)

  return ret;
}

ColorManagementChain::~ColorManagementChain() = default;

}  // namespace wr
}  // namespace mozilla

#undef LOG_H
