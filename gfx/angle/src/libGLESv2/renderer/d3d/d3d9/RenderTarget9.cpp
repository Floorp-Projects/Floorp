//
// Copyright (c) 2012-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderTarget9.cpp: Implements a D3D9-specific wrapper for IDirect3DSurface9
// pointers retained by renderbuffers.

#include "libGLESv2/renderer/d3d/d3d9/RenderTarget9.h"
#include "libGLESv2/renderer/d3d/d3d9/Renderer9.h"
#include "libGLESv2/renderer/d3d/d3d9/renderer9_utils.h"
#include "libGLESv2/renderer/d3d/d3d9/formatutils9.h"
#include "libGLESv2/main.h"

namespace rx
{

// TODO: AddRef the incoming surface to take ownership instead of expecting that its ref is being given.
RenderTarget9::RenderTarget9(Renderer *renderer, IDirect3DSurface9 *surface, GLenum internalFormatOverride)
{
    mRenderer = Renderer9::makeRenderer9(renderer);
    mRenderTarget = surface;

    if (mRenderTarget)
    {
        D3DSURFACE_DESC description;
        mRenderTarget->GetDesc(&description);

        mWidth = description.Width;
        mHeight = description.Height;
        mDepth = 1;

        const d3d9::D3DFormat &d3dFormatInfo = d3d9::GetD3DFormatInfo(description.Format);
        mInternalFormat = d3dFormatInfo.internalFormat;
        mActualFormat = d3dFormatInfo.internalFormat;
        mSamples = d3d9_gl::GetSamplesCount(description.MultiSampleType);

        if (internalFormatOverride) {
            mInternalFormat = internalFormatOverride;
        }
    }
}

RenderTarget9::RenderTarget9(Renderer *renderer, GLsizei width, GLsizei height, GLenum internalFormat, GLsizei samples)
{
    mRenderer = Renderer9::makeRenderer9(renderer);
    mRenderTarget = NULL;

    const d3d9::TextureFormat &d3d9FormatInfo = d3d9::GetTextureFormatInfo(internalFormat);
    const d3d9::D3DFormat &d3dFormatInfo = d3d9::GetD3DFormatInfo(d3d9FormatInfo.renderFormat);

    const gl::TextureCaps &textureCaps = mRenderer->getRendererTextureCaps().get(internalFormat);
    GLuint supportedSamples = textureCaps.getNearestSamples(samples);

    HRESULT result = D3DERR_INVALIDCALL;

    if (width > 0 && height > 0)
    {
        IDirect3DDevice9 *device = mRenderer->getDevice();

        bool requiresInitialization = false;

        const gl::InternalFormat &formatInfo = gl::GetInternalFormatInfo(internalFormat);
        if (formatInfo.depthBits > 0 || formatInfo.stencilBits > 0)
        {
            result = device->CreateDepthStencilSurface(width, height, d3d9FormatInfo.renderFormat,
                                                       gl_d3d9::GetMultisampleType(supportedSamples),
                                                       0, FALSE, &mRenderTarget, NULL);
        }
        else
        {
            requiresInitialization = (d3d9FormatInfo.dataInitializerFunction != NULL);
            result = device->CreateRenderTarget(width, height, d3d9FormatInfo.renderFormat,
                                                gl_d3d9::GetMultisampleType(supportedSamples),
                                                0, FALSE, &mRenderTarget, NULL);
        }

        if (result == D3DERR_OUTOFVIDEOMEMORY ||
            result == E_INVALIDARG ||
            result == E_OUTOFMEMORY)
        {
            gl::error(GL_OUT_OF_MEMORY);

            return;
        }

        ASSERT(SUCCEEDED(result));

        if (requiresInitialization)
        {
            // This format requires that the data be initialized before the render target can be used
            // Unfortunately this requires a Get call on the d3d device but it is far better than having
            // to mark the render target as lockable and copy data to the gpu.
            IDirect3DSurface9 *prevRenderTarget = NULL;
            device->GetRenderTarget(0, &prevRenderTarget);
            device->SetRenderTarget(0, mRenderTarget);
            device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 255), 0.0f, 0);
            device->SetRenderTarget(0, prevRenderTarget);
        }
    }

    mWidth = width;
    mHeight = height;
    mDepth = 1;
    mInternalFormat = internalFormat;
    mSamples = supportedSamples;
    mActualFormat = d3dFormatInfo.internalFormat;
}

RenderTarget9::~RenderTarget9()
{
    SafeRelease(mRenderTarget);
}

RenderTarget9 *RenderTarget9::makeRenderTarget9(RenderTarget *target)
{
    ASSERT(HAS_DYNAMIC_TYPE(rx::RenderTarget9*, target));
    return static_cast<rx::RenderTarget9*>(target);
}

void RenderTarget9::invalidate(GLint x, GLint y, GLsizei width, GLsizei height)
{
    // Currently a no-op
}

IDirect3DSurface9 *RenderTarget9::getSurface()
{
    // Caller is responsible for releasing the returned surface reference.
    // TODO: remove the AddRef to match RenderTarget11
    if (mRenderTarget)
    {
        mRenderTarget->AddRef();
    }

    return mRenderTarget;
}

}
