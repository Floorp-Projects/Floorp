/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceD3D11Interop.h"

#include <d3d11.h>
#include "gfxPrefs.h"
#include "GLContext.h"
#include "WGLLibrary.h"
#include "nsPrintfCString.h"
#include "mozilla/gfx/DeviceManagerD3D11.h"

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

hr = D3D11CreateDeviceAndSwapChain(NULL,                        // pAdapter
                                   D3D_DRIVER_TYPE_HARDWARE,    // DriverType
                                   NULL,                        // Software
                                   0,                           // Flags (Do not set D3D11_CREATE_DEVICE_SINGLETHREADED)
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
// DXGL Device

class DXGLDevice : public RefCounted<DXGLDevice>
{
public:
    MOZ_DECLARE_REFCOUNTED_TYPENAME(DXGLDevice)

    WGLLibrary* const mWGL;
    const RefPtr<ID3D11Device> mD3D; // Only needed for lifetime guarantee.
    const HANDLE mDXGLDeviceHandle;

    static already_AddRefed<DXGLDevice> Open(WGLLibrary* wgl)
    {
        MOZ_ASSERT(wgl->HasDXInterop2());

        RefPtr<ID3D11Device> d3d = gfx::DeviceManagerD3D11::Get()->GetContentDevice();
        if (!d3d) {
            NS_WARNING("Failed to create D3D11 device.");
            return nullptr;
        }

        HANDLE dxglDeviceHandle = wgl->fDXOpenDevice(d3d);
        if (!dxglDeviceHandle) {
            NS_WARNING("Failed to open D3D device for use by WGL.");
            return nullptr;
        }

        return MakeAndAddRef<DXGLDevice>(wgl, d3d, dxglDeviceHandle);
    }

    DXGLDevice(WGLLibrary* wgl, const RefPtr<ID3D11Device>& d3d, HANDLE dxglDeviceHandle)
        : mWGL(wgl)
        , mD3D(d3d)
        , mDXGLDeviceHandle(dxglDeviceHandle)
    { }

    ~DXGLDevice() {
        if (!mWGL->fDXCloseDevice(mDXGLDeviceHandle)) {
            uint32_t error = GetLastError();
            const nsPrintfCString errorMessage("wglDXCloseDevice(0x%x) failed: "
                                               "GetLastError(): 0x%x\n",
                                               mDXGLDeviceHandle, error);
            gfxCriticalError() << errorMessage.BeginReading();
            MOZ_CRASH("GFX: Problem closing DXGL device");
        }
    }

    HANDLE RegisterObject(void* dxObject, GLuint name, GLenum type, GLenum access) const {
        HANDLE ret = mWGL->fDXRegisterObject(mDXGLDeviceHandle, dxObject, name, type,
                                             access);
        if (!ret) {
            uint32_t error = GetLastError();
            const nsPrintfCString errorMessage("wglDXRegisterObject(0x%x, 0x%x, %u, 0x%x, 0x%x) failed:"
                                               " GetLastError(): 0x%x\n",
                                               mDXGLDeviceHandle, dxObject, name,
                                               type, access, error);
            gfxCriticalError() << errorMessage.BeginReading();
            MOZ_CRASH("GFX: Problem registering DXGL device");
        }
        return ret;
    }

    bool UnregisterObject(HANDLE hObject) const {
        bool ret = mWGL->fDXUnregisterObject(mDXGLDeviceHandle, hObject);
        if (!ret) {
            uint32_t error = GetLastError();
            const nsPrintfCString errorMessage("wglDXUnregisterObject(0x%x, 0x%x) failed: "
                                               "GetLastError(): 0x%x\n",
                                               mDXGLDeviceHandle, hObject, error);
            gfxCriticalError() << errorMessage.BeginReading();
            MOZ_CRASH("GFX: Problem unregistering DXGL device");
        }
        return ret;
    }

    bool LockObject(HANDLE hObject) const {
        bool ret = mWGL->fDXLockObjects(mDXGLDeviceHandle, 1, &hObject);
        if (!ret) {
            uint32_t error = GetLastError();
            const nsPrintfCString errorMessage("wglDXLockObjects(0x%x, 1, {0x%x}) "
                                               "failed: GetLastError(): 0x%x\n",
                                               mDXGLDeviceHandle, hObject, error);
            gfxCriticalError() << errorMessage.BeginReading();
            MOZ_CRASH("GFX: Problem locking DXGL device");
        }
        return ret;
    }

    bool UnlockObject(HANDLE hObject) const {
        bool ret = mWGL->fDXUnlockObjects(mDXGLDeviceHandle, 1, &hObject);
        if (!ret) {
            uint32_t error = GetLastError();
            const nsPrintfCString errorMessage("wglDXUnlockObjects(0x%x, 1, {0x%x}) "
                                               "failed: GetLastError(): 0x%x\n",
                                               mDXGLDeviceHandle, hObject, error);
            gfxCriticalError() << errorMessage.BeginReading();
            MOZ_CRASH("GFX: Problem unlocking DXGL device");
        }
        return ret;
    }
};

////////////////////////////////////////////////////////////////////////////////
// Shared Surface

/*static*/ UniquePtr<SharedSurface_D3D11Interop>
SharedSurface_D3D11Interop::Create(const RefPtr<DXGLDevice>& dxgl,
                                   GLContext* gl,
                                   const gfx::IntSize& size,
                                   bool hasAlpha)
{
    auto& d3d = *dxgl->mD3D;

    // Create a texture in case we need to readback.
    DXGI_FORMAT format = hasAlpha ? DXGI_FORMAT_B8G8R8A8_UNORM
                                  : DXGI_FORMAT_B8G8R8X8_UNORM;
    CD3D11_TEXTURE2D_DESC desc(format, size.width, size.height, 1, 1);
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    RefPtr<ID3D11Texture2D> textureD3D;
    HRESULT hr = d3d.CreateTexture2D(&desc, nullptr, getter_AddRefs(textureD3D));
    if (FAILED(hr)) {
        NS_WARNING("Failed to create texture for CanvasLayer!");
        return nullptr;
    }

    RefPtr<IDXGIResource> textureDXGI;
    hr = textureD3D->QueryInterface(__uuidof(IDXGIResource), getter_AddRefs(textureDXGI));
    if (FAILED(hr)) {
        NS_WARNING("Failed to open texture for sharing!");
        return nullptr;
    }

    RefPtr<IDXGIKeyedMutex> keyedMutex;
    hr = textureD3D->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(keyedMutex));
    if (FAILED(hr)) {
        NS_WARNING("Failed to obtained keyed mutex from texture!");
        return nullptr;
    }

    HANDLE sharedHandle;
    textureDXGI->GetSharedHandle(&sharedHandle);

    GLuint renderbufferGL = 0;
    gl->MakeCurrent();
    gl->fGenRenderbuffers(1, &renderbufferGL);
    HANDLE objectWGL = dxgl->RegisterObject(textureD3D, renderbufferGL,
                                            LOCAL_GL_RENDERBUFFER,
                                            LOCAL_WGL_ACCESS_WRITE_DISCARD_NV);
    if (!objectWGL) {
        NS_WARNING("Failed to register D3D object with WGL.");
        return nullptr;
    }

    typedef SharedSurface_D3D11Interop ptrT;
    UniquePtr<ptrT> ret ( new ptrT(gl, size, hasAlpha, renderbufferGL, dxgl, objectWGL,
                                   textureD3D, sharedHandle, keyedMutex) );
    return Move(ret);
}

SharedSurface_D3D11Interop::SharedSurface_D3D11Interop(GLContext* gl,
                                                       const gfx::IntSize& size,
                                                       bool hasAlpha,
                                                       GLuint renderbufferGL,
                                                       const RefPtr<DXGLDevice>& dxgl,
                                                       HANDLE objectWGL,
                                                       const RefPtr<ID3D11Texture2D>& textureD3D,
                                                       HANDLE sharedHandle,
                                                       const RefPtr<IDXGIKeyedMutex>& keyedMutex)
    : SharedSurface(SharedSurfaceType::DXGLInterop2,
                    AttachmentType::GLRenderbuffer,
                    gl,
                    size,
                    hasAlpha,
                    true)
    , mProdRB(renderbufferGL)
    , mDXGL(dxgl)
    , mObjectWGL(objectWGL)
    , mTextureD3D(textureD3D)
    , mNeedsFinish(gfxPrefs::WebGLDXGLNeedsFinish())
    , mSharedHandle(sharedHandle)
    , mKeyedMutex(keyedMutex)
    , mLockedForGL(false)
{ }

SharedSurface_D3D11Interop::~SharedSurface_D3D11Interop()
{
    MOZ_ASSERT(!mLockedForGL);

    if (!mDXGL->UnregisterObject(mObjectWGL)) {
        NS_WARNING("Failed to release a DXGL object, possibly leaking it.");
    }

    if (!mGL->MakeCurrent())
        return;

    mGL->fDeleteRenderbuffers(1, &mProdRB);

    // mDXGL is closed when it runs out of refs.
}

void
SharedSurface_D3D11Interop::ProducerAcquireImpl()
{
    MOZ_ASSERT(!mLockedForGL);

    if (mKeyedMutex) {
        const uint64_t keyValue = 0;
        const DWORD timeoutMs = 10000;
        HRESULT hr = mKeyedMutex->AcquireSync(keyValue, timeoutMs);
        if (hr == WAIT_TIMEOUT) {
            // Doubt we should do this? Maybe Wait for ever?
            MOZ_CRASH("GFX: d3d11Interop timeout");
        }
    }

    // Now we have the mutex, we can lock for GL.
    MOZ_ALWAYS_TRUE(mDXGL->LockObject(mObjectWGL));

    mLockedForGL = true;
}

void
SharedSurface_D3D11Interop::ProducerReleaseImpl()
{
    MOZ_ASSERT(mLockedForGL);

    mGL->fFlush();
    MOZ_ALWAYS_TRUE(mDXGL->UnlockObject(mObjectWGL));

    mLockedForGL = false;

    // Now we have unlocked for GL, we can release to consumer.
    if (mKeyedMutex) {
        mKeyedMutex->ReleaseSync(0);
    }

    if (mNeedsFinish) {
        mGL->fFinish();
    }
}

bool
SharedSurface_D3D11Interop::ToSurfaceDescriptor(layers::SurfaceDescriptor* const out_descriptor)
{
    gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::B8G8R8A8
                                          : gfx::SurfaceFormat::B8G8R8X8;
    *out_descriptor = layers::SurfaceDescriptorD3D10(WindowsHandle(mSharedHandle), format,
                                                     mSize);
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Factory

/*static*/ UniquePtr<SurfaceFactory_D3D11Interop>
SurfaceFactory_D3D11Interop::Create(GLContext* gl, const SurfaceCaps& caps,
                                    const RefPtr<layers::ClientIPCAllocator>& allocator,
                                    const layers::TextureFlags& flags)
{
    WGLLibrary* wgl = &sWGLLib;
    if (!wgl || !wgl->HasDXInterop2())
        return nullptr;

    RefPtr<DXGLDevice> dxgl = DXGLDevice::Open(wgl);
    if (!dxgl) {
        NS_WARNING("Failed to open D3D device for use by WGL.");
        return nullptr;
    }

    typedef SurfaceFactory_D3D11Interop ptrT;
    UniquePtr<ptrT> ret(new ptrT(gl, caps, allocator, flags, dxgl));

    return Move(ret);
}

SurfaceFactory_D3D11Interop::SurfaceFactory_D3D11Interop(GLContext* gl,
                                                         const SurfaceCaps& caps,
                                                         const RefPtr<layers::ClientIPCAllocator>& allocator,
                                                         const layers::TextureFlags& flags,
                                                         const RefPtr<DXGLDevice>& dxgl)
    : SurfaceFactory(SharedSurfaceType::DXGLInterop2, gl, caps, allocator, flags)
    , mDXGL(dxgl)
{ }

SurfaceFactory_D3D11Interop::~SurfaceFactory_D3D11Interop()
{ }

} // namespace gl
} // namespace mozilla
