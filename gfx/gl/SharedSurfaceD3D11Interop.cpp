/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceD3D11Interop.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include "GLContext.h"
#include "MozFramebuffer.h"
#include "WGLLibrary.h"
#include "nsPrintfCString.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/StaticPrefs_webgl.h"

namespace mozilla {
namespace gl {

/*
Sample Code for WGL_NV_DX_interop2:
Example: Render to Direct3D 11 backbuffer with openGL:

// create D3D11 device, context and swap chain.
ID3D11Device *device;
ID3D11DeviceContext *devCtx;
IDXGISwapChain *swapChain;

DXGI_SWAP_CHAIN_DESC scd;

<set appropriate swap chain parameters in scd>

hr = D3D11CreateDeviceAndSwapChain(
         NULL,                        // pAdapter
         D3D_DRIVER_TYPE_HARDWARE,    // DriverType
         NULL,                        // Software
         0,                           // Flags (Do not set
                                      // D3D11_CREATE_DEVICE_SINGLETHREADED)
         NULL,                        // pFeatureLevels
         0,                           // FeatureLevels
         D3D11_SDK_VERSION,           // SDKVersion
         &scd,                        // pSwapChainDesc
         &swapChain,                  // ppSwapChain
         &device,                     // ppDevice
         NULL,                        // pFeatureLevel
         &devCtx);                    // ppImmediateContext

// Fetch the swapchain backbuffer
ID3D11Texture2D *dxColorbuffer;
swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&dxColorbuffer);

// Create depth stencil texture
ID3D11Texture2D *dxDepthBuffer;
D3D11_TEXTURE2D_DESC depthDesc;
depthDesc.Usage = D3D11_USAGE_DEFAULT;
<set other depthDesc parameters appropriately>

// Create Views
ID3D11RenderTargetView *colorBufferView;
D3D11_RENDER_TARGET_VIEW_DESC rtd;
<set rtd parameters appropriately>
device->CreateRenderTargetView(dxColorbuffer, &rtd, &colorBufferView);

ID3D11DepthStencilView *depthBufferView;
D3D11_DEPTH_STENCIL_VIEW_DESC dsd;
<set dsd parameters appropriately>
device->CreateDepthStencilView(dxDepthBuffer, &dsd, &depthBufferView);

// Attach back buffer and depth texture to redertarget for the device.
devCtx->OMSetRenderTargets(1, &colorBufferView, depthBufferView);

// Register D3D11 device with GL
HANDLE gl_handleD3D;
gl_handleD3D = wglDXOpenDeviceNV(device);

// register the Direct3D color and depth/stencil buffers as
// renderbuffers in opengl
GLuint gl_names[2];
HANDLE gl_handles[2];

glGenRenderbuffers(2, gl_names);

gl_handles[0] = wglDXRegisterObjectNV(gl_handleD3D, dxColorBuffer,
                                      gl_names[0],
                                      GL_RENDERBUFFER,
                                      WGL_ACCESS_READ_WRITE_NV);

gl_handles[1] = wglDXRegisterObjectNV(gl_handleD3D, dxDepthBuffer,
                                      gl_names[1],
                                      GL_RENDERBUFFER,
                                      WGL_ACCESS_READ_WRITE_NV);

// attach the Direct3D buffers to an FBO
glBindFramebuffer(GL_FRAMEBUFFER, fbo);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                          GL_RENDERBUFFER, gl_names[0]);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                          GL_RENDERBUFFER, gl_names[1]);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                          GL_RENDERBUFFER, gl_names[1]);

while (!done) {
      <direct3d renders to the render targets>

      // lock the render targets for GL access
      wglDXLockObjectsNVX(gl_handleD3D, 2, gl_handles);

      <opengl renders to the render targets>

      // unlock the render targets
      wglDXUnlockObjectsNVX(gl_handleD3D, 2, gl_handles);

      <direct3d renders to the render targets and presents
       the results on the screen>
}
*/

////////////////////////////////////////////////////////////////////////////////
// DXInterop2Device

class ScopedContextState final {
  ID3D11DeviceContext1* const mD3DContext;
  RefPtr<ID3DDeviceContextState> mOldContextState;

 public:
  ScopedContextState(ID3D11DeviceContext1* d3dContext,
                     ID3DDeviceContextState* newContextState)
      : mD3DContext(d3dContext), mOldContextState(nullptr) {
    if (!mD3DContext) return;

    mD3DContext->SwapDeviceContextState(newContextState,
                                        getter_AddRefs(mOldContextState));
  }

  ~ScopedContextState() {
    if (!mD3DContext) return;

    mD3DContext->SwapDeviceContextState(mOldContextState, nullptr);
  }
};

class DXInterop2Device : public RefCounted<DXInterop2Device> {
 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(DXInterop2Device)

  WGLLibrary* const mWGL;
  const RefPtr<ID3D11Device> mD3D;  // Only needed for lifetime guarantee.
  const HANDLE mInteropDevice;
  GLContext* const mGL;

  // AMD workaround.
  const RefPtr<ID3D11DeviceContext1> mD3DContext;
  const RefPtr<ID3DDeviceContextState> mContextState;

  static already_AddRefed<DXInterop2Device> Open(WGLLibrary* wgl,
                                                 GLContext* gl) {
    MOZ_ASSERT(wgl->HasDXInterop2());

    const RefPtr<ID3D11Device> d3d =
        gfx::DeviceManagerDx::Get()->GetContentDevice();
    if (!d3d) {
      gfxCriticalNote
          << "DXInterop2Device::Open: Failed to create D3D11 device.";
      return nullptr;
    }

    if (!gl->MakeCurrent()) return nullptr;

    RefPtr<ID3D11DeviceContext1> d3dContext;
    RefPtr<ID3DDeviceContextState> contextState;
    if (gl->WorkAroundDriverBugs() && gl->Vendor() == GLVendor::ATI) {
      // AMD calls ID3D10Device::Flush, so we need to be in ID3D10Device mode.
      RefPtr<ID3D11Device1> d3d11_1;
      auto hr =
          d3d->QueryInterface(__uuidof(ID3D11Device1), getter_AddRefs(d3d11_1));
      if (!SUCCEEDED(hr)) return nullptr;

      d3d11_1->GetImmediateContext1(getter_AddRefs(d3dContext));
      MOZ_ASSERT(d3dContext);

      const D3D_FEATURE_LEVEL featureLevel10_0 = D3D_FEATURE_LEVEL_10_0;
      hr = d3d11_1->CreateDeviceContextState(
          0, &featureLevel10_0, 1, D3D11_SDK_VERSION, __uuidof(ID3D10Device),
          nullptr, getter_AddRefs(contextState));
      if (!SUCCEEDED(hr)) return nullptr;
    }

    const auto interopDevice = wgl->mSymbols.fDXOpenDeviceNV(d3d);
    if (!interopDevice) {
      gfxCriticalNote << "DXInterop2Device::Open: DXOpenDevice failed.";
      return nullptr;
    }

    return MakeAndAddRef<DXInterop2Device>(wgl, d3d, interopDevice, gl,
                                           d3dContext, contextState);
  }

  DXInterop2Device(WGLLibrary* wgl, ID3D11Device* d3d, HANDLE interopDevice,
                   GLContext* gl, ID3D11DeviceContext1* d3dContext,
                   ID3DDeviceContextState* contextState)
      : mWGL(wgl),
        mD3D(d3d),
        mInteropDevice(interopDevice),
        mGL(gl),
        mD3DContext(d3dContext),
        mContextState(contextState) {}

  ~DXInterop2Device() {
    const auto isCurrent = mGL->MakeCurrent();

    if (mWGL->mSymbols.fDXCloseDeviceNV(mInteropDevice)) return;

    if (isCurrent) {
      // That shouldn't have failed.
      const uint32_t error = GetLastError();
      const nsPrintfCString errorMessage(
          "wglDXCloseDevice(0x%p) failed:"
          " GetLastError(): %u\n",
          mInteropDevice, error);
      gfxCriticalError() << errorMessage.BeginReading();
    }
  }

  HANDLE RegisterObject(void* d3dObject, GLuint name, GLenum type,
                        GLenum access) const {
    if (!mGL->MakeCurrent()) return nullptr;

    const ScopedContextState autoCS(mD3DContext, mContextState);
    const auto ret = mWGL->mSymbols.fDXRegisterObjectNV(
        mInteropDevice, d3dObject, name, type, access);
    if (ret) return ret;

    const uint32_t error = GetLastError();
    const nsPrintfCString errorMessage(
        "wglDXRegisterObject(0x%p, 0x%p, %u, 0x%04x,"
        " 0x%04x) failed: GetLastError(): %u\n",
        mInteropDevice, d3dObject, name, type, access, error);
    gfxCriticalNote << errorMessage.BeginReading();
    return nullptr;
  }

  bool UnregisterObject(HANDLE lockHandle) const {
    const auto isCurrent = mGL->MakeCurrent();

    const ScopedContextState autoCS(mD3DContext, mContextState);
    if (mWGL->mSymbols.fDXUnregisterObjectNV(mInteropDevice, lockHandle))
      return true;

    if (!isCurrent) {
      // That shouldn't have failed.
      const uint32_t error = GetLastError();
      const nsPrintfCString errorMessage(
          "wglDXUnregisterObject(0x%p, 0x%p) failed:"
          " GetLastError(): %u\n",
          mInteropDevice, lockHandle, error);
      gfxCriticalError() << errorMessage.BeginReading();
    }
    return false;
  }

  bool LockObject(HANDLE lockHandle) const {
    MOZ_ASSERT(mGL->IsCurrent());

    if (mWGL->mSymbols.fDXLockObjectsNV(mInteropDevice, 1, &lockHandle))
      return true;

    if (!mGL->MakeCurrent()) return false;

    gfxCriticalNote << "wglDXLockObjects called without mGL being current."
                    << " Retrying after MakeCurrent.";

    if (mWGL->mSymbols.fDXLockObjectsNV(mInteropDevice, 1, &lockHandle))
      return true;

    const uint32_t error = GetLastError();
    const nsPrintfCString errorMessage(
        "wglDXLockObjects(0x%p, 1, {0x%p}) failed:"
        " GetLastError(): %u\n",
        mInteropDevice, lockHandle, error);
    gfxCriticalError() << errorMessage.BeginReading();
    return false;
  }

  bool UnlockObject(HANDLE lockHandle) const {
    MOZ_ASSERT(mGL->IsCurrent());

    if (mWGL->mSymbols.fDXUnlockObjectsNV(mInteropDevice, 1, &lockHandle))
      return true;

    if (!mGL->MakeCurrent()) return false;

    gfxCriticalNote << "wglDXUnlockObjects called without mGL being current."
                    << " Retrying after MakeCurrent.";

    if (mWGL->mSymbols.fDXUnlockObjectsNV(mInteropDevice, 1, &lockHandle))
      return true;

    const uint32_t error = GetLastError();
    const nsPrintfCString errorMessage(
        "wglDXUnlockObjects(0x%p, 1, {0x%p}) failed:"
        " GetLastError(): %u\n",
        mInteropDevice, lockHandle, error);
    gfxCriticalError() << errorMessage.BeginReading();
    return false;
  }
};

////////////////////////////////////////////////////////////////////////////////
// Shared Surface

/*static*/
UniquePtr<SharedSurface_D3D11Interop> SharedSurface_D3D11Interop::Create(
    const SharedSurfaceDesc& desc, DXInterop2Device* interop) {
  const auto& gl = desc.gl;
  const auto& size = desc.size;
  const auto& d3d = interop->mD3D;

  auto data = Data{interop};

  // Create a texture in case we need to readback.
  const DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
  CD3D11_TEXTURE2D_DESC texDesc(format, size.width, size.height, 1, 1);
  texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  auto hr =
      d3d->CreateTexture2D(&texDesc, nullptr, getter_AddRefs(data.texD3D));
  if (FAILED(hr)) {
    NS_WARNING("Failed to create texture for CanvasLayer!");
    return nullptr;
  }

  RefPtr<IDXGIResource> texDXGI;
  hr = data.texD3D->QueryInterface(__uuidof(IDXGIResource),
                                   getter_AddRefs(texDXGI));
  if (FAILED(hr)) {
    NS_WARNING("Failed to open texture for sharing!");
    return nullptr;
  }

  texDXGI->GetSharedHandle(&data.dxgiHandle);

  ////

  if (!gl->MakeCurrent()) {
    NS_WARNING("MakeCurrent failed.");
    return nullptr;
  }

  data.interopRb = MakeUnique<Renderbuffer>(*gl);
  data.lockHandle = interop->RegisterObject(data.texD3D, data.interopRb->name,
                                            LOCAL_GL_RENDERBUFFER,
                                            LOCAL_WGL_ACCESS_WRITE_DISCARD_NV);
  if (!data.lockHandle) {
    NS_WARNING("Failed to register D3D object with WGL.");
    return nullptr;
  }

  auto fbForDrawing = MozFramebuffer::CreateForBacking(
      gl, size, 0, false, LOCAL_GL_RENDERBUFFER, data.interopRb->name);
  if (!fbForDrawing) return nullptr;

  // -

  {
    GLint samples = 0;
    {
      const ScopedBindRenderbuffer bindRB(gl, data.interopRb->name);
      gl->fGetRenderbufferParameteriv(LOCAL_GL_RENDERBUFFER,
                                      LOCAL_GL_RENDERBUFFER_SAMPLES, &samples);
    }
    if (samples > 0) {  // Intel
      // Intel's dx_interop GL-side textures have SAMPLES=1, likely because
      // that's what the D3DTextures technically have. However, SAMPLES=1 is
      // very different from SAMPLES=0 in GL. For more, see
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1325835

      // Our ShSurf tex or rb must be single-sampled.
      data.interopFbIfNeedsIndirect = std::move(fbForDrawing);
      fbForDrawing = MozFramebuffer::Create(gl, size, 0, false);
    }
  }

  // -

  return AsUnique(new SharedSurface_D3D11Interop(desc, std::move(fbForDrawing),
                                                 std::move(data)));
}

SharedSurface_D3D11Interop::SharedSurface_D3D11Interop(
    const SharedSurfaceDesc& desc, UniquePtr<MozFramebuffer>&& fbForDrawing,
    Data&& data)
    : SharedSurface(desc, std::move(fbForDrawing)),
      mData(std::move(data)),
      mNeedsFinish(StaticPrefs::webgl_dxgl_needs_finish()) {}

SharedSurface_D3D11Interop::~SharedSurface_D3D11Interop() {
  MOZ_ASSERT(!IsProducerAcquired());

  const auto& gl = mDesc.gl;
  if (!gl || !gl->MakeCurrent()) return;

  if (!mData.interop->UnregisterObject(mData.lockHandle)) {
    NS_WARNING("Failed to release mLockHandle, possibly leaking it.");
  }
}

void SharedSurface_D3D11Interop::ProducerAcquireImpl() {
  MOZ_ASSERT(!mLockedForGL);

  // Now we have the mutex, we can lock for GL.
  MOZ_ALWAYS_TRUE(mData.interop->LockObject(mData.lockHandle));

  mLockedForGL = true;
}

void SharedSurface_D3D11Interop::ProducerReleaseImpl() {
  const auto& gl = mDesc.gl;
  MOZ_ASSERT(mLockedForGL);

  if (mData.interopFbIfNeedsIndirect) {
    const ScopedBindFramebuffer bindFB(gl, mData.interopFbIfNeedsIndirect->mFB);
    gl->BlitHelper()->DrawBlitTextureToFramebuffer(mFb->ColorTex(), mDesc.size,
                                                   mDesc.size);
  }

  if (mNeedsFinish) {
    gl->fFinish();
  } else {
    // We probably don't even need this.
    gl->fFlush();
  }
  MOZ_ALWAYS_TRUE(mData.interop->UnlockObject(mData.lockHandle));

  mLockedForGL = false;
}

Maybe<layers::SurfaceDescriptor>
SharedSurface_D3D11Interop::ToSurfaceDescriptor() {
  const auto format = gfx::SurfaceFormat::B8G8R8A8;
  return Some(layers::SurfaceDescriptorD3D10(
      WindowsHandle(mData.dxgiHandle), /* gpuProcessTextureId */ Nothing(),
      /* arrayIndex */ 0, format, mDesc.size, gfx::YUVColorSpace::Identity,
      gfx::ColorRange::FULL));
}

//////////////////////////////////////////////////////////////////////////////////////////
// Factory

/*static*/
UniquePtr<SurfaceFactory_D3D11Interop> SurfaceFactory_D3D11Interop::Create(
    GLContext& gl) {
  WGLLibrary* wgl = &sWGLLib;
  if (!wgl || !wgl->HasDXInterop2()) return nullptr;

  if (XRE_IsContentProcess()) {
    gfxPlatform::GetPlatform()->EnsureDevicesInitialized();
  }

  const RefPtr<DXInterop2Device> interop = DXInterop2Device::Open(wgl, &gl);
  if (!interop) {
    NS_WARNING("Failed to open D3D device for use by WGL.");
    return nullptr;
  }

  return AsUnique(new SurfaceFactory_D3D11Interop(
      {&gl, SharedSurfaceType::DXGLInterop2, layers::TextureType::D3D11, true},
      interop));
}

SurfaceFactory_D3D11Interop::SurfaceFactory_D3D11Interop(
    const PartialSharedSurfaceDesc& desc, DXInterop2Device* interop)
    : SurfaceFactory(desc), mInterop(interop) {}

SurfaceFactory_D3D11Interop::~SurfaceFactory_D3D11Interop() = default;

}  // namespace gl
}  // namespace mozilla
