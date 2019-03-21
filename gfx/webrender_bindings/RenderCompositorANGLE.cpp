/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorANGLE.h"

#include "GLContext.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/HelpersD3D11.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/WinCompositorWidget.h"
#include "mozilla/WindowsVersion.h"

#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>

namespace mozilla {
namespace wr {

/* static */
UniquePtr<RenderCompositor> RenderCompositorANGLE::Create(
    RefPtr<widget::CompositorWidget>&& aWidget) {
  if (!RenderThread::Get()->SharedGL()) {
    gfxCriticalNote << "Failed to get shared GL context";
    return nullptr;
  }

  UniquePtr<RenderCompositorANGLE> compositor =
      MakeUnique<RenderCompositorANGLE>(std::move(aWidget));
  if (!compositor->Initialize()) {
    return nullptr;
  }
  return compositor;
}

RenderCompositorANGLE::RenderCompositorANGLE(
    RefPtr<widget::CompositorWidget>&& aWidget)
    : RenderCompositor(std::move(aWidget)),
      mEGLConfig(nullptr),
      mEGLSurface(nullptr),
      mUseTripleBuffering(false) {}

RenderCompositorANGLE::~RenderCompositorANGLE() {
  DestroyEGLSurface();
  MOZ_ASSERT(!mEGLSurface);
}

ID3D11Device* RenderCompositorANGLE::GetDeviceOfEGLDisplay() {
  auto* egl = gl::GLLibraryEGL::Get();
  MOZ_ASSERT(egl);
  if (!egl || !egl->IsExtensionSupported(gl::GLLibraryEGL::EXT_device_query)) {
    return nullptr;
  }

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(egl->Display(), LOCAL_EGL_DEVICE_EXT,
                              (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE,
                             (EGLAttrib*)&device);
  if (!device) {
    gfxCriticalNote << "Failed to get D3D11Device from EGLDisplay";
    return nullptr;
  }
  return device;
}

bool RenderCompositorANGLE::SutdownEGLLibraryIfNecessary() {
  const RefPtr<gl::GLLibraryEGL> egl = gl::GLLibraryEGL::Get();
  if (!egl) {
    // egl is not initialized yet;
    return true;
  }

  RefPtr<ID3D11Device> device =
      gfx::DeviceManagerDx::Get()->GetCompositorDevice();
  // When DeviceReset is handled by GPUProcessManager/GPUParent,
  // CompositorDevice is updated to a new device. EGLDisplay also needs to be
  // updated, since EGLDisplay uses DeviceManagerDx::mCompositorDevice on ANGLE
  // WebRender use case. EGLDisplay could be updated when Renderer count becomes
  // 0. It is ensured by GPUProcessManager during handling DeviceReset.
  // GPUChild::RecvNotifyDeviceReset() destroys all CompositorSessions before
  // re-creating them.
  if (device.get() != GetDeviceOfEGLDisplay() &&
      RenderThread::Get()->RendererCount() == 0) {
    // Shutdown GLLibraryEGL for updating EGLDisplay.
    RenderThread::Get()->ClearSharedGL();
    egl->Shutdown();
  }
  return true;
}

bool RenderCompositorANGLE::Initialize() {
  if (RenderThread::Get()->IsHandlingDeviceReset()) {
    gfxCriticalNote << "Waiting for handling device reset";
    return false;
  }

  // Update device if necessary.
  if (!SutdownEGLLibraryIfNecessary()) {
    return false;
  }
  if (!RenderThread::Get()->SharedGL()) {
    gfxCriticalNote << "[WR] failed to get shared GL context.";
    return false;
  }

  mDevice = GetDeviceOfEGLDisplay();

  if (!mDevice) {
    gfxCriticalNote << "[WR] failed to get compositor device.";
    return false;
  }

  mDevice->GetImmediateContext(getter_AddRefs(mCtx));
  if (!mCtx) {
    gfxCriticalNote << "[WR] failed to get immediate context.";
    return false;
  }

  HWND hwnd = mWidget->AsWindows()->GetHwnd();

  RefPtr<IDXGIDevice> dxgiDevice;
  mDevice->QueryInterface((IDXGIDevice**)getter_AddRefs(dxgiDevice));

  RefPtr<IDXGIFactory> dxgiFactory;
  {
    RefPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(getter_AddRefs(adapter));

    adapter->GetParent(
        IID_PPV_ARGS((IDXGIFactory**)getter_AddRefs(dxgiFactory)));
  }

  RefPtr<IDXGIFactory2> dxgiFactory2;
  HRESULT hr = dxgiFactory->QueryInterface(
      (IDXGIFactory2**)getter_AddRefs(dxgiFactory2));
  if (FAILED(hr)) {
    dxgiFactory2 = nullptr;
  }

  CreateSwapChainForDCompIfPossible(dxgiFactory2);

  if (!mSwapChain && dxgiFactory2 && IsWin8OrLater()) {
    RefPtr<IDXGISwapChain1> swapChain1;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 0;
    desc.Height = 0;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    // DXGI_USAGE_SHADER_INPUT is set for improving performanc of copying from
    // framebuffer to texture on intel gpu.
    desc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    // Do not use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, since it makes HWND
    // unreusable.
    // desc.BufferCount = 2;
    // desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.BufferCount = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.Flags = 0;

    hr = dxgiFactory2->CreateSwapChainForHwnd(
        mDevice, hwnd, &desc, nullptr, nullptr, getter_AddRefs(swapChain1));
    if (SUCCEEDED(hr) && swapChain1) {
      DXGI_RGBA color = {1.0f, 1.0f, 1.0f, 1.0f};
      swapChain1->SetBackgroundColor(&color);
      mSwapChain = swapChain1;
    }
  }

  if (!mSwapChain) {
    DXGI_SWAP_CHAIN_DESC swapDesc{};
    swapDesc.BufferDesc.Width = 0;
    swapDesc.BufferDesc.Height = 0;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    // DXGI_USAGE_SHADER_INPUT is set for improving performanc of copying from
    // framebuffer to texture on intel gpu.
    swapDesc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    swapDesc.BufferCount = 1;
    swapDesc.OutputWindow = hwnd;
    swapDesc.Windowed = TRUE;
    swapDesc.Flags = 0;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;

    HRESULT hr = dxgiFactory->CreateSwapChain(dxgiDevice, &swapDesc,
                                              getter_AddRefs(mSwapChain));
    if (FAILED(hr)) {
      gfxCriticalNote << "Could not create swap chain: " << gfx::hexa(hr);
      return false;
    }
  }

  // We need this because we don't want DXGI to respond to Alt+Enter.
  dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_WINDOW_CHANGES);

  mSyncObject = layers::SyncObjectHost::CreateSyncObjectHost(mDevice);
  if (!mSyncObject->Init()) {
    // Some errors occur. Clear the mSyncObject here.
    // Then, there will be no texture synchronization.
    return false;
  }

  // Force enable alpha channel to make sure ANGLE use correct framebuffer
  // formart
  if (!gl::CreateConfig(&mEGLConfig, /* bpp */ 32,
                        /* enableDepthBuffer */ true)) {
    gfxCriticalNote << "Failed to create EGLConfig for WebRender";
  }
  MOZ_ASSERT(mEGLConfig);

  if (!ResizeBufferIfNeeded()) {
    return false;
  }

  return true;
}

void RenderCompositorANGLE::CreateSwapChainForDCompIfPossible(
    IDXGIFactory2* aDXGIFactory2) {
  if (!aDXGIFactory2) {
    return;
  }

  RefPtr<IDCompositionDevice> dCompDevice =
      gfx::DeviceManagerDx::Get()->GetDirectCompositionDevice();
  if (!dCompDevice) {
    return;
  }
  MOZ_ASSERT(XRE_IsGPUProcess());

  RefPtr<IDXGIDevice> dxgiDevice;
  mDevice->QueryInterface((IDXGIDevice**)getter_AddRefs(dxgiDevice));

  RefPtr<IDXGIFactory> dxgiFactory;
  {
    RefPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(getter_AddRefs(adapter));
    adapter->GetParent(
        IID_PPV_ARGS((IDXGIFactory**)getter_AddRefs(dxgiFactory)));
  }

  HWND hwnd = mWidget->AsWindows()->GetCompositorHwnd();
  if (!hwnd) {
    gfxCriticalNote << "Compositor window was not created ";
    return;
  }

  HRESULT hr = dCompDevice->CreateTargetForHwnd(
      hwnd, TRUE, getter_AddRefs(mCompositionTarget));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create DCompositionTarget: " << gfx::hexa(hr);
    return;
  }

  hr = dCompDevice->CreateVisual(getter_AddRefs(mVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create DCompositionVisualt: "
                    << gfx::hexa(hr);
    return;
  }

  RefPtr<IDXGISwapChain1> swapChain1;
  bool useTripleBuffering = gfx::gfxVars::UseWebRenderDCompWinTripleBuffering();

  DXGI_SWAP_CHAIN_DESC1 desc{};
  // DXGI does not like 0x0 swapchains. Swap chain creation failed when 0x0 was
  // set.
  desc.Width = 1;
  desc.Height = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  // DXGI_USAGE_SHADER_INPUT is set for improving performanc of copying from
  // framebuffer to texture on intel gpu.
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
  if (useTripleBuffering) {
    desc.BufferCount = 3;
  } else {
    desc.BufferCount = 2;
  }
  // DXGI_SCALING_NONE caused swap chain creation failure.
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  desc.Flags = 0;

  hr = aDXGIFactory2->CreateSwapChainForComposition(mDevice, &desc, nullptr,
                                                    getter_AddRefs(swapChain1));
  if (SUCCEEDED(hr) && swapChain1) {
    DXGI_RGBA color = {1.0f, 1.0f, 1.0f, 1.0f};
    swapChain1->SetBackgroundColor(&color);
    mSwapChain = swapChain1;
    mVisual->SetContent(swapChain1);
    mCompositionTarget->SetRoot(mVisual);
    mCompositionDevice = dCompDevice;
    mUseTripleBuffering = useTripleBuffering;
  }
}

bool RenderCompositorANGLE::BeginFrame() {
  if (mDevice->GetDeviceRemovedReason() != S_OK) {
    RenderThread::Get()->HandleDeviceReset("BeginFrame", /* aNotify */ true);
    return false;
  }

  mWidget->AsWindows()->UpdateCompositorWndSizeIfNecessary();

  if (!ResizeBufferIfNeeded()) {
    return false;
  }

  if (!MakeCurrent()) {
    gfxCriticalNote << "Failed to make render context current, can't draw.";
    return false;
  }

  if (mSyncObject) {
    if (!mSyncObject->Synchronize(/* aFallible */ true)) {
      // It's timeout or other error. Handle the device-reset here.
      RenderThread::Get()->HandleDeviceReset("SyncObject", /* aNotify */ true);
      return false;
    }
  }
  return true;
}

void RenderCompositorANGLE::EndFrame() {
  InsertPresentWaitQuery();

  mSwapChain->Present(0, 0);

  if (mCompositionDevice) {
    mCompositionDevice->Commit();
  }
}

void RenderCompositorANGLE::WaitForGPU() {
  // Note: this waits on the query we inserted in the previous frame,
  // not the one we just inserted now. Example:
  //   Insert query #1
  //   Present #1
  //   (first frame, no wait)
  //   Insert query #2
  //   Present #2
  //   Wait for query #1.
  //   Insert query #3
  //   Present #3
  //   Wait for query #2.
  //
  // This ensures we're done reading textures before swapping buffers.
  WaitForPreviousPresentQuery();
}

bool RenderCompositorANGLE::ResizeBufferIfNeeded() {
  MOZ_ASSERT(mSwapChain);

  LayoutDeviceIntSize size = mWidget->GetClientSize();

  // DXGI does not like 0x0 swapchains. ResizeBuffers() failed when 0x0 was set
  // when DComp is used.
  size.width = std::max(size.width, 1);
  size.height = std::max(size.height, 1);

  if (mBufferSize.isSome() && mBufferSize.ref() == size) {
    MOZ_ASSERT(mEGLSurface);
    return true;
  }

  HRESULT hr;
  RefPtr<ID3D11Texture2D> backBuf;

  // Release EGLSurface of back buffer before calling ResizeBuffers().
  DestroyEGLSurface();

  // Reset buffer size
  mBufferSize.reset();

  // Resize swap chain
  DXGI_SWAP_CHAIN_DESC desc;
  hr = mSwapChain->GetDesc(&desc);
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to read swap chain description: "
                    << gfx::hexa(hr) << " Size : " << size;
    return false;
  }
  hr = mSwapChain->ResizeBuffers(desc.BufferCount, size.width, size.height,
                                 DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to resize swap chain buffers: " << gfx::hexa(hr)
                    << " Size : " << size;
    return false;
  }

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                             (void**)getter_AddRefs(backBuf));
  if (hr == DXGI_ERROR_INVALID_CALL) {
    // This happens on some GPUs/drivers when there's a TDR.
    if (mDevice->GetDeviceRemovedReason() != S_OK) {
      gfxCriticalError() << "GetBuffer returned invalid call: " << gfx::hexa(hr)
                         << " Size : " << size;
      return false;
    }
  }

  auto* egl = gl::GLLibraryEGL::Get();

  const EGLint pbuffer_attribs[]{
      LOCAL_EGL_WIDTH,
      size.width,
      LOCAL_EGL_HEIGHT,
      size.height,
      LOCAL_EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE,
      LOCAL_EGL_TRUE,
      LOCAL_EGL_NONE};

  const auto buffer = reinterpret_cast<EGLClientBuffer>(backBuf.get());

  const EGLSurface surface = egl->fCreatePbufferFromClientBuffer(
      egl->Display(), LOCAL_EGL_D3D_TEXTURE_ANGLE, buffer, mEGLConfig,
      pbuffer_attribs);

  EGLint err = egl->fGetError();
  if (err != LOCAL_EGL_SUCCESS) {
    gfxCriticalError() << "Failed to create Pbuffer of back buffer error: "
                       << gfx::hexa(err) << " Size : " << size;
    return false;
  }

  mEGLSurface = surface;
  mBufferSize = Some(size);

  return true;
}

void RenderCompositorANGLE::DestroyEGLSurface() {
  auto* egl = gl::GLLibraryEGL::Get();

  // Release EGLSurface of back buffer before calling ResizeBuffers().
  if (mEGLSurface) {
    gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    egl->fDestroySurface(egl->Display(), mEGLSurface);
    mEGLSurface = nullptr;
  }
}

void RenderCompositorANGLE::Pause() {}

bool RenderCompositorANGLE::Resume() { return true; }

bool RenderCompositorANGLE::MakeCurrent() {
  gl::GLContextEGL::Cast(gl())->SetEGLSurfaceOverride(mEGLSurface);
  return gl()->MakeCurrent();
}

LayoutDeviceIntSize RenderCompositorANGLE::GetBufferSize() {
  MOZ_ASSERT(mBufferSize.isSome());
  if (mBufferSize.isNothing()) {
    return LayoutDeviceIntSize();
  }
  return mBufferSize.ref();
}

RefPtr<ID3D11Query> RenderCompositorANGLE::GetD3D11Query() {
  RefPtr<ID3D11Query> query;

  if (mRecycledQuery) {
    query = mRecycledQuery.forget();
    return query;
  }

  CD3D11_QUERY_DESC desc(D3D11_QUERY_EVENT);
  HRESULT hr = mDevice->CreateQuery(&desc, getter_AddRefs(query));
  if (FAILED(hr) || !query) {
    gfxWarning() << "Could not create D3D11_QUERY_EVENT: " << gfx::hexa(hr);
    return nullptr;
  }
  return query;
}

void RenderCompositorANGLE::InsertPresentWaitQuery() {
  RefPtr<ID3D11Query> query;
  query = GetD3D11Query();
  if (!query) {
    return;
  }

  mCtx->End(query);
  mWaitForPresentQueries.emplace(query);
}

void RenderCompositorANGLE::WaitForPreviousPresentQuery() {
  size_t waitLatency = mUseTripleBuffering ? 3 : 2;

  while (mWaitForPresentQueries.size() >= waitLatency) {
    RefPtr<ID3D11Query>& query = mWaitForPresentQueries.front();
    BOOL result;
    layers::WaitForFrameGPUQuery(mDevice, mCtx, query, &result);

    // Recycle query for later use.
    mRecycledQuery = query;
    mWaitForPresentQueries.pop();
  }
}

}  // namespace wr
}  // namespace mozilla
