//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// D3DTextureSurfaceWGL.cpp: WGL implementation of egl::Surface for D3D texture interop.

#include "libANGLE/renderer/gl/wgl/D3DTextureSurfaceWGL.h"

#include "libANGLE/renderer/gl/FramebufferGL.h"
#include "libANGLE/renderer/gl/TextureGL.h"
#include "libANGLE/renderer/gl/RendererGL.h"
#include "libANGLE/renderer/gl/StateManagerGL.h"
#include "libANGLE/renderer/gl/wgl/DisplayWGL.h"
#include "libANGLE/renderer/gl/wgl/FunctionsWGL.h"

namespace rx
{

namespace
{

egl::Error GetD3DTextureInfo(EGLClientBuffer clientBuffer,
                             size_t *width,
                             size_t *height,
                             IUnknown **object,
                             IUnknown **device)
{
    IUnknown *buffer = static_cast<IUnknown *>(clientBuffer);

    IDirect3DTexture9 *texture9 = nullptr;
    ID3D11Texture2D *texture11  = nullptr;
    if (SUCCEEDED(buffer->QueryInterface<ID3D11Texture2D>(&texture11)))
    {
        D3D11_TEXTURE2D_DESC textureDesc;
        texture11->GetDesc(&textureDesc);

        // From table egl.restrictions in EGL_ANGLE_d3d_texture_client_buffer.
        switch (textureDesc.Format)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            case DXGI_FORMAT_R16G16B16A16_FLOAT:
            case DXGI_FORMAT_R32G32B32A32_FLOAT:
                break;

            default:
                SafeRelease(texture11);
                return egl::Error(EGL_BAD_PARAMETER, "Unknown client buffer texture format: %u.",
                                  textureDesc.Format);
        }

        ID3D11Device *d3d11Device = nullptr;
        texture11->GetDevice(&d3d11Device);
        if (d3d11Device == nullptr)
        {
            SafeRelease(texture11);
            return egl::Error(EGL_BAD_PARAMETER,
                              "Could not query the D3D11 device from the client buffer.");
        }

        if (width)
        {
            *width = textureDesc.Width;
        }
        if (height)
        {
            *height = textureDesc.Height;
        }

        if (device)
        {
            *device = d3d11Device;
        }
        else
        {
            SafeRelease(d3d11Device);
        }

        if (object)
        {
            *object = texture11;
        }
        else
        {
            SafeRelease(texture11);
        }

        return egl::Error(EGL_SUCCESS);
    }
    else if (SUCCEEDED(buffer->QueryInterface<IDirect3DTexture9>(&texture9)))
    {
        D3DSURFACE_DESC surfaceDesc;
        if (FAILED(texture9->GetLevelDesc(0, &surfaceDesc)))
        {
            SafeRelease(texture9);
            return egl::Error(EGL_BAD_PARAMETER,
                              "Could not query description of the D3D9 surface.");
        }

        // From table egl.restrictions in EGL_ANGLE_d3d_texture_client_buffer.
        switch (surfaceDesc.Format)
        {
            case D3DFMT_R8G8B8:
            case D3DFMT_A8R8G8B8:
            case D3DFMT_A16B16G16R16F:
            case D3DFMT_A32B32G32R32F:
                break;

            default:
                SafeRelease(texture9);
                return egl::Error(EGL_BAD_PARAMETER, "Unknown client buffer texture format: %u.",
                                  surfaceDesc.Format);
        }

        if (width)
        {
            *width = surfaceDesc.Width;
        }
        if (height)
        {
            *height = surfaceDesc.Height;
        }

        IDirect3DDevice9 *d3d9Device = nullptr;
        HRESULT result               = texture9->GetDevice(&d3d9Device);
        if (FAILED(result))
        {
            SafeRelease(texture9);
            return egl::Error(EGL_BAD_PARAMETER,
                              "Could not query the D3D9 device from the client buffer.");
        }

        if (device)
        {
            *device = d3d9Device;
        }
        else
        {
            SafeRelease(d3d9Device);
        }

        if (object)
        {
            *object = texture9;
        }
        else
        {
            SafeRelease(texture9);
        }

        return egl::Error(EGL_SUCCESS);
    }
    else
    {
        return egl::Error(EGL_BAD_PARAMETER,
                          "Provided buffer is not a IDirect3DTexture9 or ID3D11Texture2D.");
    }
}

}  // anonymous namespace

D3DTextureSurfaceWGL::D3DTextureSurfaceWGL(const egl::SurfaceState &state,
                                           RendererGL *renderer,
                                           EGLClientBuffer clientBuffer,
                                           DisplayWGL *display,
                                           HGLRC wglContext,
                                           HDC deviceContext,
                                           const FunctionsGL *functionsGL,
                                           const FunctionsWGL *functionsWGL)
    : SurfaceGL(state, renderer),
      mClientBuffer(clientBuffer),
      mRenderer(renderer),
      mDisplay(display),
      mStateManager(renderer->getStateManager()),
      mWorkarounds(renderer->getWorkarounds()),
      mFunctionsGL(functionsGL),
      mFunctionsWGL(functionsWGL),
      mWGLContext(wglContext),
      mDeviceContext(deviceContext),
      mWidth(0),
      mHeight(0),
      mDeviceHandle(nullptr),
      mObject(nullptr),
      mBoundObjectTextureHandle(nullptr),
      mBoundObjectRenderbufferHandle(nullptr),
      mRenderbufferID(0),
      mFramebufferID(0)
{
}

D3DTextureSurfaceWGL::~D3DTextureSurfaceWGL()
{
    ASSERT(mBoundObjectTextureHandle == nullptr);

    SafeRelease(mObject);

    if (mDeviceHandle)
    {
        if (mBoundObjectRenderbufferHandle)
        {
            mFunctionsWGL->dxUnregisterObjectNV(mDeviceHandle, mBoundObjectRenderbufferHandle);
            mBoundObjectRenderbufferHandle = nullptr;
        }
        mStateManager->deleteRenderbuffer(mRenderbufferID);

        if (mBoundObjectTextureHandle)
        {
            mFunctionsWGL->dxUnregisterObjectNV(mDeviceHandle, mBoundObjectTextureHandle);
            mBoundObjectTextureHandle = nullptr;
        }

        // GL framebuffer is deleted by the default framebuffer object
        mFramebufferID = 0;

        mDisplay->releaseD3DDevice(mDeviceHandle);
        mDeviceHandle = nullptr;
    }
}

egl::Error D3DTextureSurfaceWGL::ValidateD3DTextureClientBuffer(EGLClientBuffer clientBuffer)
{
    return GetD3DTextureInfo(clientBuffer, nullptr, nullptr, nullptr, nullptr);
}

egl::Error D3DTextureSurfaceWGL::initialize()
{
    IUnknown *device = nullptr;
    ANGLE_TRY(GetD3DTextureInfo(mClientBuffer, &mWidth, &mHeight, &mObject, &device));

    ASSERT(device != nullptr);
    egl::Error error = mDisplay->registerD3DDevice(device, &mDeviceHandle);
    SafeRelease(device);
    if (error.isError())
    {
        return error;
    }

    mFunctionsGL->genRenderbuffers(1, &mRenderbufferID);
    mStateManager->bindRenderbuffer(GL_RENDERBUFFER, mRenderbufferID);
    mBoundObjectRenderbufferHandle = mFunctionsWGL->dxRegisterObjectNV(
        mDeviceHandle, mObject, mRenderbufferID, GL_RENDERBUFFER, WGL_ACCESS_READ_WRITE_NV);
    if (mBoundObjectRenderbufferHandle == nullptr)
    {
        return egl::Error(EGL_BAD_ALLOC, "Failed to register D3D object, error: 0x%08x.",
                          HRESULT_CODE(GetLastError()));
    }

    mFunctionsGL->genFramebuffers(1, &mFramebufferID);
    mStateManager->bindFramebuffer(GL_FRAMEBUFFER, mFramebufferID);
    mFunctionsGL->framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                          mRenderbufferID);

    return egl::Error(EGL_SUCCESS);
}

egl::Error D3DTextureSurfaceWGL::makeCurrent()
{
    if (!mFunctionsWGL->makeCurrent(mDeviceContext, mWGLContext))
    {
        // TODO(geofflang): What error type here?
        return egl::Error(EGL_CONTEXT_LOST, "Failed to make the WGL context current.");
    }

    if (!mFunctionsWGL->dxLockObjectsNV(mDeviceHandle, 1, &mBoundObjectRenderbufferHandle))
    {
        DWORD error = GetLastError();
        return egl::Error(EGL_BAD_ALLOC, "Failed to lock object, error: 0x%08x.",
                          HRESULT_CODE(error));
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error D3DTextureSurfaceWGL::unMakeCurrent()
{
    if (!mFunctionsWGL->dxUnlockObjectsNV(mDeviceHandle, 1, &mBoundObjectRenderbufferHandle))
    {
        DWORD error = GetLastError();
        return egl::Error(EGL_BAD_ALLOC, "Failed to unlock object, error: 0x%08x.",
                          HRESULT_CODE(error));
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error D3DTextureSurfaceWGL::swap()
{
    return egl::Error(EGL_SUCCESS);
}

egl::Error D3DTextureSurfaceWGL::postSubBuffer(EGLint x, EGLint y, EGLint width, EGLint height)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error D3DTextureSurfaceWGL::querySurfacePointerANGLE(EGLint attribute, void **value)
{
    UNIMPLEMENTED();
    return egl::Error(EGL_SUCCESS);
}

egl::Error D3DTextureSurfaceWGL::bindTexImage(gl::Texture *texture, EGLint buffer)
{
    ASSERT(mBoundObjectTextureHandle == nullptr);

    const TextureGL *textureGL = GetImplAs<TextureGL>(texture);
    GLuint textureID           = textureGL->getTextureID();

    mBoundObjectTextureHandle = mFunctionsWGL->dxRegisterObjectNV(
        mDeviceHandle, mObject, textureID, GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);
    if (mBoundObjectTextureHandle == nullptr)
    {
        DWORD error = GetLastError();
        return egl::Error(EGL_BAD_ALLOC, "Failed to register D3D object, error: 0x%08x.",
                          HRESULT_CODE(error));
    }

    if (!mFunctionsWGL->dxLockObjectsNV(mDeviceHandle, 1, &mBoundObjectTextureHandle))
    {
        DWORD error = GetLastError();
        return egl::Error(EGL_BAD_ALLOC, "Failed to lock object, error: 0x%08x.",
                          HRESULT_CODE(error));
    }

    return egl::Error(EGL_SUCCESS);
}

egl::Error D3DTextureSurfaceWGL::releaseTexImage(EGLint buffer)
{
    ASSERT(mBoundObjectTextureHandle != nullptr);
    if (!mFunctionsWGL->dxUnlockObjectsNV(mDeviceHandle, 1, &mBoundObjectTextureHandle))
    {
        DWORD error = GetLastError();
        return egl::Error(EGL_BAD_ALLOC, "Failed to unlock object, error: 0x%08x.",
                          HRESULT_CODE(error));
    }

    if (!mFunctionsWGL->dxUnregisterObjectNV(mDeviceHandle, mBoundObjectTextureHandle))
    {
        DWORD error = GetLastError();
        return egl::Error(EGL_BAD_ALLOC, "Failed to unregister D3D object, error: 0x%08x.",
                          HRESULT_CODE(error));
    }
    mBoundObjectTextureHandle = nullptr;

    return egl::Error(EGL_SUCCESS);
}

void D3DTextureSurfaceWGL::setSwapInterval(EGLint interval)
{
    UNIMPLEMENTED();
}

EGLint D3DTextureSurfaceWGL::getWidth() const
{
    return static_cast<EGLint>(mWidth);
}

EGLint D3DTextureSurfaceWGL::getHeight() const
{
    return static_cast<EGLint>(mHeight);
}

EGLint D3DTextureSurfaceWGL::isPostSubBufferSupported() const
{
    return EGL_FALSE;
}

EGLint D3DTextureSurfaceWGL::getSwapBehavior() const
{
    return EGL_BUFFER_PRESERVED;
}

FramebufferImpl *D3DTextureSurfaceWGL::createDefaultFramebuffer(const gl::FramebufferState &data)
{
    return new FramebufferGL(mFramebufferID, data, mFunctionsGL, mWorkarounds,
                             mRenderer->getBlitter(), mStateManager);
}
}  // namespace rx
