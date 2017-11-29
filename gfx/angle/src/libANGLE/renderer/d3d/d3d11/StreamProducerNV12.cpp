//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StreamProducerNV12.cpp: Implements the stream producer for NV12 textures

#include "libANGLE/renderer/d3d/d3d11/StreamProducerNV12.h"

#include "common/utilities.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

static egl::Stream::GLTextureDescription getGLDescFromTex(ID3D11Texture2D* tex,
                                                          UINT planeIndex,
                                                          const char** const out_error)
{
    *out_error = "Undocumented error";

    egl::Stream::GLTextureDescription ret = { 0 };
    if (!tex)
    {
        *out_error = "Texture is null";
        return ret;
    }

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    if (desc.Width < 1 || desc.Height < 1)
    {
        *out_error = "Width or height < 1";
        return ret;
    }

    ret.width = desc.Width;
    ret.height = desc.Height;
    ret.mipLevels = 0;

    UINT maxPlaneIndex = 0;
    switch (desc.Format) {
    case DXGI_FORMAT_NV12:
        // The UV plane of NV12 textures has half the width/height of the Y plane
        if ((desc.Width % 2) != 0 || (desc.Height % 2) != 0)
        {
            *out_error = "NV12 tetxures must have even width and height.";
            break; // Bad width/height.
        }

        maxPlaneIndex = 1;
        if (planeIndex == 0)
        {
            ret.internalFormat = GL_R8;
        }
        else
        {
            ret.internalFormat = GL_RG8;
            ret.width  /= 2;
            ret.height /= 2;
        }
        break;

    case DXGI_FORMAT_R8_UNORM:
        ret.internalFormat = GL_R8;
        break;
    case DXGI_FORMAT_R8G8_UNORM:
        ret.internalFormat = GL_RG8;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        ret.internalFormat = GL_RGBA8;
        break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        ret.internalFormat = GL_BGRA8_EXT;
        break;

    default:
        *out_error = "Unsupported format";
        return ret;
    }

    if (planeIndex > maxPlaneIndex)
    {
        // Just kidding, there's no plane out there.
        ret.internalFormat = 0;
        *out_error = "Plane out of range";
    }

    return ret;
}


StreamProducerNV12::StreamProducerNV12(Renderer11 *renderer)
    : mRenderer(renderer), mTexture(nullptr), mArraySlice(0), mPlaneOffset(0)
{
}

StreamProducerNV12::~StreamProducerNV12()
{
    SafeRelease(mTexture);
}

egl::Error StreamProducerNV12::validateD3DNV12Texture(void *pointer, const egl::AttributeMap &attributes) const
{
    ID3D11Texture2D *textureD3D = static_cast<ID3D11Texture2D *>(pointer);

    // Check that the texture originated from our device
    ID3D11Device *device;
    textureD3D->GetDevice(&device);
    if (device != mRenderer->getDevice())
    {
        return egl::EglBadParameter() << "Texture not created on ANGLE D3D device";
    }

    const auto planeId = static_cast<UINT>(attributes.get(EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG, 0));
    const char* errorText;
    const auto glDesc = getGLDescFromTex(textureD3D, planeId, &errorText);
    if (!glDesc.internalFormat)
    {
        return egl::EglBadParameter() << errorText;
    }

    return egl::NoError();
}

void StreamProducerNV12::postD3DNV12Texture(void *pointer, const egl::AttributeMap &attributes)
{
    ASSERT(pointer != nullptr);
    ID3D11Texture2D *textureD3D = static_cast<ID3D11Texture2D *>(pointer);

    // Release the previous texture if there is one
    SafeRelease(mTexture);

    mTexture = textureD3D;
    mTexture->AddRef();
    mPlaneOffset = static_cast<UINT>(attributes.get(EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG, 0));
    mArraySlice = static_cast<UINT>(attributes.get(EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, 0));
}

egl::Stream::GLTextureDescription StreamProducerNV12::getGLFrameDescription(int planeIndex)
{
    const char* errorText;
    return getGLDescFromTex(mTexture, static_cast<UINT>(planeIndex + mPlaneOffset),
                            &errorText);
}

ID3D11Texture2D *StreamProducerNV12::getD3DTexture()
{
    return mTexture;
}

UINT StreamProducerNV12::getArraySlice()
{
    return mArraySlice;
}

}  // namespace rx
