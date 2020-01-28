/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCLayerTree.h"

#include "GLContext.h"
#include "GLContextEGL.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/webrender/RenderThread.h"

#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINBLUE
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WINBLUE

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>

namespace mozilla {
namespace wr {

/* static */
UniquePtr<DCLayerTree> DCLayerTree::Create(gl::GLContext* aGL,
                                           EGLConfig aEGLConfig,
                                           ID3D11Device* aDevice, HWND aHwnd) {
  RefPtr<IDCompositionDevice2> dCompDevice =
      gfx::DeviceManagerDx::Get()->GetDirectCompositionDevice();
  if (!dCompDevice) {
    return nullptr;
  }

  auto layerTree =
      MakeUnique<DCLayerTree>(aGL, aEGLConfig, aDevice, dCompDevice);
  if (!layerTree->Initialize(aHwnd)) {
    return nullptr;
  }

  return layerTree;
}

DCLayerTree::DCLayerTree(gl::GLContext* aGL, EGLConfig aEGLConfig,
                         ID3D11Device* aDevice,
                         IDCompositionDevice2* aCompositionDevice)
    : mGL(aGL),
      mEGLConfig(aEGLConfig),
      mDevice(aDevice),
      mCompositionDevice(aCompositionDevice),
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

bool DCLayerTree::Initialize(HWND aHwnd) {
  HRESULT hr;

  RefPtr<IDCompositionDesktopDevice> desktopDevice;
  hr = mCompositionDevice->QueryInterface(
      (IDCompositionDesktopDevice**)getter_AddRefs(desktopDevice));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to get IDCompositionDesktopDevice: "
                    << gfx::hexa(hr);
    return false;
  }

  hr = desktopDevice->CreateTargetForHwnd(aHwnd, TRUE,
                                          getter_AddRefs(mCompositionTarget));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create DCompositionTarget: " << gfx::hexa(hr);
    return false;
  }

  hr = mCompositionDevice->CreateVisual(getter_AddRefs(mRootVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
  }

  hr =
      mCompositionDevice->CreateVisual(getter_AddRefs(mDefaultSwapChainVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
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

void DCLayerTree::CompositorBeginFrame() {}

void DCLayerTree::CompositorEndFrame() {
  // Check if the visual tree of surfaces is the same as last frame.
  bool same = mPrevLayers == mCurrentLayers;

  if (!same) {
    // If not, we need to rebuild the visual tree. Note that addition or
    // removal of tiles no longer needs to rebuild the main visual tree
    // here, since they are added as children of the surface visual.
    mRootVisual->RemoveAllVisuals();

    // Add surfaces in z-order they were added to the scene.
    for (auto it = mCurrentLayers.begin(); it != mCurrentLayers.end(); ++it) {
      auto surface_it = mDCSurfaces.find(*it);
      MOZ_RELEASE_ASSERT(surface_it != mDCSurfaces.end());
      const auto surface = surface_it->second.get();
      const auto visual = surface->GetVisual();
      mRootVisual->AddVisual(visual, FALSE, nullptr);
    }
  }

  mPrevLayers.swap(mCurrentLayers);
  mCurrentLayers.clear();

  mCompositionDevice->Commit();
}

void DCLayerTree::Bind(wr::NativeTileId aId, wr::DeviceIntPoint* aOffset,
                       uint32_t* aFboId, wr::DeviceIntRect aDirtyRect) {
  auto surface = GetSurface(aId.surface_id);
  wr::DeviceIntPoint targetOffset{0, 0};

#ifdef USE_VIRTUAL_SURFACES
  wr::DeviceIntSize tileSize = surface->GetTileSize();
  RefPtr<IDCompositionSurface> compositionSurface =
      surface->GetCompositionSurface();
  targetOffset.x = VIRTUAL_OFFSET + tileSize.width * aId.x;
  targetOffset.y = VIRTUAL_OFFSET + tileSize.height * aId.y;
#else
  auto layer = surface->GetLayer(aId.x, aId.y);
  RefPtr<IDCompositionSurface> compositionSurface =
      layer->GetCompositionSurface();
#endif

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
                                wr::DeviceIntSize aTileSize, bool aIsOpaque) {
  auto it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(it == mDCSurfaces.end());
  if (it != mDCSurfaces.end()) {
    // DCSurface already exists.
    return;
  }

  auto surface = MakeUnique<DCSurface>(aTileSize, aIsOpaque, this);
  if (!surface->Initialize()) {
    gfxCriticalNote << "Failed to initialize DCSurface: " << wr::AsUint64(aId);
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

void DCLayerTree::AddSurface(wr::NativeSurfaceId aId,
                             wr::DeviceIntPoint aPosition,
                             wr::DeviceIntRect aClipRect) {
  auto it = mDCSurfaces.find(aId);
  MOZ_RELEASE_ASSERT(it != mDCSurfaces.end());
  const auto layer = it->second.get();
  const auto visual = layer->GetVisual();

#ifdef USE_VIRTUAL_SURFACES
  layer->UpdateAllocatedRect();

  aPosition.x -= VIRTUAL_OFFSET;
  aPosition.y -= VIRTUAL_OFFSET;
#endif

  // Place the visual - this changes frame to frame based on scroll position
  // of the slice.
  visual->SetOffsetX(aPosition.x);
  visual->SetOffsetY(aPosition.y);

  // Set the clip rect - converting from world space to the pre-offset space
  // that DC requires for rectangle clips.
  D2D_RECT_F clip_rect;
  clip_rect.left = aClipRect.origin.x - aPosition.x;
  clip_rect.top = aClipRect.origin.y - aPosition.y;
  clip_rect.right = clip_rect.left + aClipRect.size.width;
  clip_rect.bottom = clip_rect.top + aClipRect.size.height;
  visual->SetClip(clip_rect);

  mCurrentLayers.push_back(aId);
}

GLuint DCLayerTree::GetOrCreateFbo(int aWidth, int aHeight) {
  const auto gl = GetGLContext();
  GLuint fboId = 0;

  // Check if we have a cached FBO with matching dimensions
  for (auto it = mFrameBuffers.begin(); it != mFrameBuffers.end(); ++it) {
    if (it->width == aWidth && it->height == aHeight) {
      fboId = it->fboId;
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
    mFrameBuffers.push_back(frame_buffer_info);
  }

  return fboId;
}

DCSurface::DCSurface(wr::DeviceIntSize aTileSize, bool aIsOpaque,
                     DCLayerTree* aDCLayerTree)
    : mDCLayerTree(aDCLayerTree),
      mTileSize(aTileSize),
      mIsOpaque(aIsOpaque),
      mAllocatedRectDirty(true) {}

DCSurface::~DCSurface() {}

bool DCSurface::Initialize() {
  HRESULT hr;
  const auto dCompDevice = mDCLayerTree->GetCompositionDevice();
  hr = dCompDevice->CreateVisual(getter_AddRefs(mVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
  }

#ifdef USE_VIRTUAL_SURFACES
  DXGI_ALPHA_MODE alpha_mode =
      mIsOpaque ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_PREMULTIPLIED;

  hr = dCompDevice->CreateVirtualSurface(VIRTUAL_OFFSET * 2, VIRTUAL_OFFSET * 2,
                                         DXGI_FORMAT_B8G8R8A8_UNORM, alpha_mode,
                                         getter_AddRefs(mVirtualSurface));
  MOZ_ASSERT(SUCCEEDED(hr));

  // Bind the surface memory to this visual
  hr = mVisual->SetContent(mVirtualSurface);
  MOZ_ASSERT(SUCCEEDED(hr));
#endif

  return true;
}

void DCSurface::CreateTile(int aX, int aY) {
  TileKey key(aX, aY);
  MOZ_RELEASE_ASSERT(mDCLayers.find(key) == mDCLayers.end());

  auto layer = MakeUnique<DCLayer>(mDCLayerTree);
  if (!layer->Initialize(aX, aY, mTileSize, mIsOpaque)) {
    gfxCriticalNote << "Failed to initialize DCLayer: " << aX << aY;
    return;
  }

#ifdef USE_VIRTUAL_SURFACES
  mAllocatedRectDirty = true;
#else
  mVisual->AddVisual(layer->GetVisual(), FALSE, NULL);
#endif

  mDCLayers[key] = std::move(layer);
}

void DCSurface::DestroyTile(int aX, int aY) {
  TileKey key(aX, aY);
#ifdef USE_VIRTUAL_SURFACES
  mAllocatedRectDirty = true;
#else
  auto layer = GetLayer(aX, aY);
  mVisual->RemoveVisual(layer->GetVisual());
#endif
  mDCLayers.erase(key);
}

#ifdef USE_VIRTUAL_SURFACES
void DCSurface::UpdateAllocatedRect() {
  if (mAllocatedRectDirty) {
    // The virtual surface may have holes in it (for example, an empty tile
    // that has no primitives). Instead of trimming to a single bounding
    // rect, supply the rect of each valid tile to handle this case.
    std::vector<RECT> validRects;

    for (auto it = mDCLayers.begin(); it != mDCLayers.end(); ++it) {
      RECT rect;

      rect.left = (LONG)(VIRTUAL_OFFSET + it->first.mX * mTileSize.width);
      rect.top = (LONG)(VIRTUAL_OFFSET + it->first.mY * mTileSize.height);
      rect.right = rect.left + mTileSize.width;
      rect.bottom = rect.top + mTileSize.height;

      validRects.push_back(rect);
    }

    mVirtualSurface->Trim(validRects.data(), validRects.size());
    mAllocatedRectDirty = false;
  }
}
#endif

DCLayer* DCSurface::GetLayer(int aX, int aY) const {
  TileKey key(aX, aY);
  auto layer_it = mDCLayers.find(key);
  MOZ_RELEASE_ASSERT(layer_it != mDCLayers.end());
  return layer_it->second.get();
}

DCLayer::DCLayer(DCLayerTree* aDCLayerTree) : mDCLayerTree(aDCLayerTree) {}

DCLayer::~DCLayer() {}

bool DCLayer::Initialize(int aX, int aY, wr::DeviceIntSize aSize,
                         bool aIsOpaque) {
  if (aSize.width <= 0 || aSize.height <= 0) {
    return false;
  }

#ifndef USE_VIRTUAL_SURFACES
  HRESULT hr;
  const auto dCompDevice = mDCLayerTree->GetCompositionDevice();
  hr = dCompDevice->CreateVisual(getter_AddRefs(mVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
  }

  mCompositionSurface = CreateCompositionSurface(aSize, aIsOpaque);
  if (!mCompositionSurface) {
    return false;
  }

  hr = mVisual->SetContent(mCompositionSurface);
  if (FAILED(hr)) {
    gfxCriticalNote << "SetContent failed: " << gfx::hexa(hr);
    return false;
  }

  // Position this tile at a local space offset within the parent visual
  // Scroll offsets get applied to the parent visual only.
  mVisual->SetOffsetX(aX * aSize.width);
  mVisual->SetOffsetY(aY * aSize.height);
#endif

  return true;
}

#ifndef USE_VIRTUAL_SURFACES
RefPtr<IDCompositionSurface> DCLayer::CreateCompositionSurface(
    wr::DeviceIntSize aSize, bool aIsOpaque) {
  HRESULT hr;
  const auto dCompDevice = mDCLayerTree->GetCompositionDevice();
  const auto alphaMode =
      aIsOpaque ? DXGI_ALPHA_MODE_IGNORE : DXGI_ALPHA_MODE_PREMULTIPLIED;
  RefPtr<IDCompositionSurface> compositionSurface;

  hr = dCompDevice->CreateSurface(aSize.width, aSize.height,
                                  DXGI_FORMAT_B8G8R8A8_UNORM, alphaMode,
                                  getter_AddRefs(compositionSurface));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionSurface: "
                    << gfx::hexa(hr);
    return nullptr;
  }
  return compositionSurface;
}
#endif

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
    RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
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
  mEGLImage = egl->fCreateImage(egl->Display(), EGL_NO_CONTEXT,
                                LOCAL_EGL_D3D11_TEXTURE_ANGLE, buffer, attribs);

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
    egl->fDestroyImage(egl->Display(), mEGLImage);
    mEGLImage = EGL_NO_IMAGE;
  }
}

}  // namespace wr
}  // namespace mozilla
