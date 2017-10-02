//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RendererD3D.cpp: Implementation of the base D3D Renderer.

#include "libANGLE/renderer/d3d/RendererD3D.h"

#include "common/MemoryBuffer.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/State.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/DeviceD3D.h"
#include "libANGLE/renderer/d3d/DisplayD3D.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/ProgramD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"

namespace rx
{

RendererD3D::RendererD3D(egl::Display *display)
    : mDisplay(display),
      mPresentPathFastEnabled(false),
      mCapsInitialized(false),
      mWorkaroundsInitialized(false),
      mDisjoint(false),
      mDeviceLost(false),
      mWorkerThreadPool(4)
{
}

RendererD3D::~RendererD3D()
{
    cleanup();
}

void RendererD3D::cleanup()
{
    for (auto &incompleteTexture : mIncompleteTextures)
    {
        ANGLE_SWALLOW_ERR(incompleteTexture.second->onDestroy(mDisplay->getProxyContext()));
        incompleteTexture.second.set(mDisplay->getProxyContext(), nullptr);
    }
    mIncompleteTextures.clear();
}

bool RendererD3D::skipDraw(const gl::State &glState, GLenum drawMode)
{
    if (drawMode == GL_POINTS)
    {
        bool usesPointSize = GetImplAs<ProgramD3D>(glState.getProgram())->usesPointSize();

        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!usesPointSize && !glState.isTransformFeedbackActiveUnpaused())
        {
            // Notify developers of risking undefined behavior.
            WARN() << "Point rendering without writing to gl_PointSize.";
            return true;
        }
    }
    else if (gl::IsTriangleMode(drawMode))
    {
        if (glState.getRasterizerState().cullFace &&
            glState.getRasterizerState().cullMode == GL_FRONT_AND_BACK)
        {
            return true;
        }
    }

    return false;
}

size_t RendererD3D::getBoundFramebufferTextures(const gl::ContextState &data,
                                                FramebufferTextureArray *outTextureArray)
{
    size_t textureCount = 0;

    const gl::Framebuffer *drawFramebuffer = data.getState().getDrawFramebuffer();
    for (size_t i = 0; i < drawFramebuffer->getNumColorBuffers(); i++)
    {
        const gl::FramebufferAttachment *attachment = drawFramebuffer->getColorbuffer(i);
        if (attachment && attachment->type() == GL_TEXTURE)
        {
            (*outTextureArray)[textureCount++] = attachment->getTexture();
        }
    }

    const gl::FramebufferAttachment *depthStencilAttachment =
        drawFramebuffer->getDepthOrStencilbuffer();
    if (depthStencilAttachment && depthStencilAttachment->type() == GL_TEXTURE)
    {
        (*outTextureArray)[textureCount++] = depthStencilAttachment->getTexture();
    }

    std::sort(outTextureArray->begin(), outTextureArray->begin() + textureCount);

    return textureCount;
}

gl::Error RendererD3D::getIncompleteTexture(const gl::Context *context,
                                            GLenum type,
                                            gl::Texture **textureOut)
{
    if (mIncompleteTextures.find(type) == mIncompleteTextures.end())
    {
        GLImplFactory *implFactory = context->getImplementation();
        const GLubyte color[] = {0, 0, 0, 255};
        const gl::Extents colorSize(1, 1, 1);
        const gl::PixelUnpackState unpack(1, 0);
        const gl::Box area(0, 0, 0, 1, 1, 1);

        // If a texture is external use a 2D texture for the incomplete texture
        GLenum createType = (type == GL_TEXTURE_EXTERNAL_OES) ? GL_TEXTURE_2D : type;

        // Skip the API layer to avoid needing to pass the Context and mess with dirty bits.
        gl::Texture *t =
            new gl::Texture(implFactory, std::numeric_limits<GLuint>::max(), createType);
        if (createType == GL_TEXTURE_2D_MULTISAMPLE)
        {
            ANGLE_TRY(t->setStorageMultisample(nullptr, createType, 1, GL_RGBA8, colorSize, true));
        }
        else
        {
            ANGLE_TRY(t->setStorage(nullptr, createType, 1, GL_RGBA8, colorSize));
        }
        if (type == GL_TEXTURE_CUBE_MAP)
        {
            for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                 face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++)
            {
                ANGLE_TRY(t->getImplementation()->setSubImage(nullptr, face, 0, area, GL_RGBA8,
                                                              GL_UNSIGNED_BYTE, unpack, color));
            }
        }
        else if (type == GL_TEXTURE_2D_MULTISAMPLE)
        {
            gl::ColorF clearValue(0, 0, 0, 1);
            gl::ImageIndex index = gl::ImageIndex::Make2DMultisample();
            ANGLE_TRY(GetImplAs<TextureD3D>(t)->clearLevel(context, index, clearValue, 1.0f, 0));
        }
        else
        {
            ANGLE_TRY(t->getImplementation()->setSubImage(nullptr, createType, 0, area, GL_RGBA8,
                                                          GL_UNSIGNED_BYTE, unpack, color));
        }
        mIncompleteTextures[type].set(context, t);
    }

    *textureOut = mIncompleteTextures[type].get();
    return gl::NoError();
}

GLenum RendererD3D::getResetStatus()
{
    if (!mDeviceLost)
    {
        if (testDeviceLost())
        {
            mDeviceLost = true;
            notifyDeviceLost();
            return GL_UNKNOWN_CONTEXT_RESET_EXT;
        }
        return GL_NO_ERROR;
    }

    if (testDeviceResettable())
    {
        return GL_NO_ERROR;
    }

    return GL_UNKNOWN_CONTEXT_RESET_EXT;
}

void RendererD3D::notifyDeviceLost()
{
    mDisplay->notifyDeviceLost();
}

std::string RendererD3D::getVendorString() const
{
    LUID adapterLuid = {0};

    if (getLUID(&adapterLuid))
    {
        char adapterLuidString[64];
        sprintf_s(adapterLuidString, sizeof(adapterLuidString), "(adapter LUID: %08x%08x)",
                  adapterLuid.HighPart, adapterLuid.LowPart);
        return std::string(adapterLuidString);
    }

    return std::string("");
}

void RendererD3D::setGPUDisjoint()
{
    mDisjoint = true;
}

GLint RendererD3D::getGPUDisjoint()
{
    bool disjoint = mDisjoint;

    // Disjoint flag is cleared when read
    mDisjoint = false;

    return disjoint;
}

GLint64 RendererD3D::getTimestamp()
{
    // D3D has no way to get an actual timestamp reliably so 0 is returned
    return 0;
}

void RendererD3D::ensureCapsInitialized() const
{
    if (!mCapsInitialized)
    {
        generateCaps(&mNativeCaps, &mNativeTextureCaps, &mNativeExtensions, &mNativeLimitations);
        mCapsInitialized = true;
    }
}

const gl::Caps &RendererD3D::getNativeCaps() const
{
    ensureCapsInitialized();
    return mNativeCaps;
}

const gl::TextureCapsMap &RendererD3D::getNativeTextureCaps() const
{
    ensureCapsInitialized();
    return mNativeTextureCaps;
}

const gl::Extensions &RendererD3D::getNativeExtensions() const
{
    ensureCapsInitialized();
    return mNativeExtensions;
}

const gl::Limitations &RendererD3D::getNativeLimitations() const
{
    ensureCapsInitialized();
    return mNativeLimitations;
}

angle::WorkerThreadPool *RendererD3D::getWorkerThreadPool()
{
    return &mWorkerThreadPool;
}

bool RendererD3D::isRobustResourceInitEnabled() const
{
    return mDisplay->isRobustResourceInitEnabled();
}

Serial RendererD3D::generateSerial()
{
    return mSerialFactory.generate();
}

bool InstancedPointSpritesActive(ProgramD3D *programD3D, GLenum mode)
{
    return programD3D->usesPointSize() && programD3D->usesInstancedPointSpriteEmulation() &&
           mode == GL_POINTS;
}

unsigned int GetBlendSampleMask(const gl::State &glState, int samples)
{
    unsigned int mask   = 0;
    if (glState.isSampleCoverageEnabled())
    {
        GLfloat coverageValue = glState.getSampleCoverageValue();
        if (coverageValue != 0)
        {
            float threshold = 0.5f;

            for (int i = 0; i < samples; ++i)
            {
                mask <<= 1;

                if ((i + 1) * coverageValue >= threshold)
                {
                    threshold += 1.0f;
                    mask |= 1;
                }
            }
        }

        bool coverageInvert = glState.getSampleCoverageInvert();
        if (coverageInvert)
        {
            mask = ~mask;
        }
    }
    else
    {
        mask = 0xFFFFFFFF;
    }

    return mask;
}

}  // namespace rx
