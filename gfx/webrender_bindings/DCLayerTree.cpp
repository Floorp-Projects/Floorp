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
      mDebugVisualRedrawRegions(false) {}

DCLayerTree::~DCLayerTree() {
  const auto gl = GetGLContext();

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
  // Set interporation mode to Linear.
  // By default, a visual inherits the interpolation mode of the parent visual.
  // If no visuals set the interpolation mode, the default for the entire visual
  // tree is nearest neighbor interpolation.
  mRootVisual->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
  return true;
}

void DCLayerTree::SetDefaultSwapChain(IDXGISwapChain1* aSwapChain) {
  mRootVisual->AddVisual(mDefaultSwapChainVisual, TRUE, nullptr);
  mDefaultSwapChainVisual->SetContent(aSwapChain);
  // Default SwapChain's visual does not need linear interporation.
  mDefaultSwapChainVisual->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
  mCompositionDevice->Commit();
}

void DCLayerTree::MaybeUpdateDebug() {
  bool updated = false;
  updated |= MaybeUpdateDebugCounter();
  updated |= MaybeUpdateDebugVisualRedrawRegions();
  if (updated) {
    mCompositionDevice->Commit();
  }
}

void DCLayerTree::WaitForCommitCompletion() {
  mCompositionDevice->WaitForCommitCompletion();
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
  bool same = mPrevLayers == mCurrentLayers;

  if (!same) {
    mRootVisual->RemoveAllVisuals();

    for (auto it = mCurrentLayers.begin(); it != mCurrentLayers.end(); ++it) {
      auto layer_it = mDCLayers.find(*it);
      MOZ_ASSERT(layer_it != mDCLayers.end());
      const auto layer = layer_it->second.get();
      const auto visual = layer->GetVisual();
      mRootVisual->AddVisual(visual, FALSE, nullptr);
    }
  }

  mPrevLayers.swap(mCurrentLayers);
  mCurrentLayers.clear();

  mCompositionDevice->Commit();
}

void DCLayerTree::Bind(wr::NativeSurfaceId aId, wr::DeviceIntPoint* aOffset,
                       uint32_t* aFboId, wr::DeviceIntRect aDirtyRect) {
  auto it = mDCLayers.find(wr::AsUint64(aId));
  MOZ_ASSERT(it != mDCLayers.end());
  if (it == mDCLayers.end()) {
    gfxCriticalNote << "Failed to get DCLayer for bind: " << wr::AsUint64(aId);
    return;
  }

  const auto layer = it->second.get();

  *aFboId = layer->CreateEGLSurfaceForCompositionSurface(aDirtyRect, aOffset);

  mCurrentId = Some(aId);
}

void DCLayerTree::Unbind() {
  if (mCurrentId.isNothing()) {
    return;
  }

  auto it = mDCLayers.find(wr::AsUint64(mCurrentId.ref()));
  MOZ_RELEASE_ASSERT(it != mDCLayers.end());

  const auto layer = it->second.get();

  layer->EndDraw();
  mCurrentId = Nothing();
}

void DCLayerTree::CreateSurface(wr::NativeSurfaceId aId,
                                wr::DeviceIntSize aSize, bool aIsOpaque) {
  auto it = mDCLayers.find(wr::AsUint64(aId));
  MOZ_RELEASE_ASSERT(it == mDCLayers.end());
  MOZ_ASSERT(it == mDCLayers.end());
  if (it != mDCLayers.end()) {
    // DCLayer already exists.
    return;
  }

  auto layer = MakeUnique<DCLayer>(this);
  if (!layer->Initialize(aSize, aIsOpaque)) {
    gfxCriticalNote << "Failed to initialize DCLayer: " << wr::AsUint64(aId);
    return;
  }

  mDCLayers[wr::AsUint64(aId)] = std::move(layer);
}

void DCLayerTree::DestroySurface(NativeSurfaceId aId) {
  auto it = mDCLayers.find(wr::AsUint64(aId));
  MOZ_ASSERT(it != mDCLayers.end());
  if (it == mDCLayers.end()) {
    return;
  }
  mDCLayers.erase(it);
}

void DCLayerTree::AddSurface(wr::NativeSurfaceId aId,
                             wr::DeviceIntPoint aPosition,
                             wr::DeviceIntRect aClipRect) {
  auto it = mDCLayers.find(wr::AsUint64(aId));
  MOZ_ASSERT(it != mDCLayers.end());
  if (it == mDCLayers.end()) {
    return;
  }
  const auto layer = it->second.get();
  const auto visual = layer->GetVisual();

  // Place the visual - this changes frame to frame based on scroll position
  // of the slice.
  int offset_x = aPosition.x;
  int offset_y = aPosition.y;
  visual->SetOffsetX(offset_x);
  visual->SetOffsetY(offset_y);

  // Set the clip rect - converting from world space to the pre-offset space
  // that DC requires for rectangle clips.
  D2D_RECT_F clip_rect;
  clip_rect.left = aClipRect.origin.x - offset_x;
  clip_rect.top = aClipRect.origin.y - offset_y;
  clip_rect.right = clip_rect.left + aClipRect.size.width;
  clip_rect.bottom = clip_rect.top + aClipRect.size.height;
  visual->SetClip(clip_rect);

  mCurrentLayers.push_back(wr::AsUint64(aId));
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

DCLayer::DCLayer(DCLayerTree* aDCLayerTree)
    : mDCLayerTree(aDCLayerTree), mEGLImage(EGL_NO_IMAGE), mColorRBO(0) {}

DCLayer::~DCLayer() { DestroyEGLSurface(); }

bool DCLayer::Initialize(wr::DeviceIntSize aSize, bool aIsOpaque) {
  if (aSize.width <= 0 || aSize.height <= 0) {
    return false;
  }
  mBufferSize = LayoutDeviceIntSize(aSize.width, aSize.height);

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

  return true;
}

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

GLuint DCLayer::CreateEGLSurfaceForCompositionSurface(
    wr::DeviceIntRect aDirtyRect, wr::DeviceIntPoint* aOffset) {
  MOZ_ASSERT(mCompositionSurface.get());

  HRESULT hr;
  const auto gl = mDCLayerTree->GetGLContext();
  RefPtr<ID3D11Texture2D> backBuf;
  POINT offset;

  LayoutDeviceIntRect dirtyRect(aDirtyRect.origin.x, aDirtyRect.origin.y,
                                aDirtyRect.size.width, aDirtyRect.size.height);

  RECT update_rect;
  update_rect.left = dirtyRect.X();
  update_rect.top = dirtyRect.Y();
  update_rect.right = dirtyRect.XMost();
  update_rect.bottom = dirtyRect.YMost();

  RECT* rect = &update_rect;
  if (StaticPrefs::gfx_webrender_compositor_max_update_rects_AtStartup() <= 0) {
    // Update entire surface
    rect = nullptr;
  }

  hr = mCompositionSurface->BeginDraw(rect, __uuidof(ID3D11Texture2D),
                                      (void**)getter_AddRefs(backBuf), &offset);
  if (FAILED(hr)) {
    gfxCriticalNote << "DCompositionSurface::BeginDraw failed: "
                    << gfx::hexa(hr) << "dirtyRect: " << dirtyRect;
    return false;
  }

  // DC includes the origin of the dirty / update rect in the draw offset,
  // undo that here since WR expects it to be an absolute offset.
  offset.x -= dirtyRect.X();
  offset.y -= dirtyRect.Y();

  // Texture size could be diffrent from mBufferSize.
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
  GLuint fboId = mDCLayerTree->GetOrCreateFbo(desc.Width, desc.Height);

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

void DCLayer::DestroyEGLSurface() {
  const auto gl = mDCLayerTree->GetGLContext();

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

void DCLayer::EndDraw() {
  MOZ_ASSERT(mCompositionSurface.get());
  if (!mCompositionSurface) {
    return;
  }

  mCompositionSurface->EndDraw();
  DestroyEGLSurface();
}

}  // namespace wr
}  // namespace mozilla
