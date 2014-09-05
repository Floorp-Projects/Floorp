/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLReadTexImageHelper.h"

#include "gfx2DGlue.h"
#include "gfxTypes.h"
#include "GLContext.h"
#include "OGLShaderProgram.h"
#include "ScopedGLHelpers.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/Move.h"

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;

GLReadTexImageHelper::GLReadTexImageHelper(GLContext* gl)
    : mGL(gl)
{
    mPrograms[0] = 0;
    mPrograms[1] = 0;
    mPrograms[2] = 0;
    mPrograms[3] = 0;
}

GLReadTexImageHelper::~GLReadTexImageHelper()
{
    mGL->fDeleteProgram(mPrograms[0]);
    mGL->fDeleteProgram(mPrograms[1]);
    mGL->fDeleteProgram(mPrograms[2]);
    mGL->fDeleteProgram(mPrograms[3]);
}

static const GLchar
readTextureImageVS[] =
    "attribute vec2 aVertex;\n"
    "attribute vec2 aTexCoord;\n"
    "varying vec2 vTexCoord;\n"
    "void main() { gl_Position = vec4(aVertex, 0, 1); vTexCoord = aTexCoord; }";

static const GLchar
readTextureImageFS_TEXTURE_2D[] =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform sampler2D uTexture;\n"
    "void main() { gl_FragColor = texture2D(uTexture, vTexCoord); }";


static const GLchar
readTextureImageFS_TEXTURE_2D_BGRA[] =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform sampler2D uTexture;\n"
    "void main() { gl_FragColor = texture2D(uTexture, vTexCoord).bgra; }";

static const GLchar
readTextureImageFS_TEXTURE_EXTERNAL[] =
    "#extension GL_OES_EGL_image_external : require\n"
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform samplerExternalOES uTexture;\n"
    "void main() { gl_FragColor = texture2D(uTexture, vTexCoord); }";

static const GLchar
readTextureImageFS_TEXTURE_RECTANGLE[] =
    "#extension GL_ARB_texture_rectangle\n"
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vTexCoord;\n"
    "uniform sampler2DRect uTexture;\n"
    "void main() { gl_FragColor = texture2DRect(uTexture, vTexCoord).bgra; }";

GLuint
GLReadTexImageHelper::TextureImageProgramFor(GLenum aTextureTarget,
                                             int aConfig)
{
    int variant = 0;
    const GLchar* readTextureImageFS = nullptr;
    if (aTextureTarget == LOCAL_GL_TEXTURE_2D) {
        if (aConfig & mozilla::layers::ENABLE_TEXTURE_RB_SWAP) {
            // Need to swizzle R/B.
            readTextureImageFS = readTextureImageFS_TEXTURE_2D_BGRA;
            variant = 1;
        } else {
            readTextureImageFS = readTextureImageFS_TEXTURE_2D;
            variant = 0;
        }
    } else if (aTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL) {
        readTextureImageFS = readTextureImageFS_TEXTURE_EXTERNAL;
        variant = 2;
    } else if (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) {
        readTextureImageFS = readTextureImageFS_TEXTURE_RECTANGLE;
        variant = 3;
    }

    /* This might be overkill, but assure that we don't access out-of-bounds */
    MOZ_ASSERT((size_t) variant < ArrayLength(mPrograms));
    if (!mPrograms[variant]) {
        GLuint vs = mGL->fCreateShader(LOCAL_GL_VERTEX_SHADER);
        const GLchar* vsSourcePtr = &readTextureImageVS[0];
        mGL->fShaderSource(vs, 1, &vsSourcePtr, nullptr);
        mGL->fCompileShader(vs);

        GLuint fs = mGL->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);
        mGL->fShaderSource(fs, 1, &readTextureImageFS, nullptr);
        mGL->fCompileShader(fs);

        GLuint program = mGL->fCreateProgram();
        mGL->fAttachShader(program, vs);
        mGL->fAttachShader(program, fs);
        mGL->fBindAttribLocation(program, 0, "aVertex");
        mGL->fBindAttribLocation(program, 1, "aTexCoord");
        mGL->fLinkProgram(program);

        GLint success;
        mGL->fGetProgramiv(program, LOCAL_GL_LINK_STATUS, &success);

        if (!success) {
            mGL->fDeleteProgram(program);
            program = 0;
        }

        mGL->fDeleteShader(vs);
        mGL->fDeleteShader(fs);

        mPrograms[variant] = program;
    }

    return mPrograms[variant];
}

bool
GLReadTexImageHelper::DidGLErrorOccur(const char* str)
{
    GLenum error = mGL->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        printf_stderr("GL ERROR: %s (0x%04x) %s\n",
                      mGL->GLErrorToString(error), error, str);
        return true;
    }

    return false;
}

static bool
GetActualReadFormats(GLContext* gl,
                     GLenum destFormat, GLenum destType,
                     GLenum& readFormat, GLenum& readType)
{
    if (destFormat == LOCAL_GL_RGBA &&
        destType == LOCAL_GL_UNSIGNED_BYTE)
    {
        readFormat = destFormat;
        readType = destType;
        return true;
    }

    bool fallback = true;
    if (gl->IsGLES()) {
        GLenum auxFormat = 0;
        GLenum auxType = 0;

        gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT, (GLint*)&auxFormat);
        gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE, (GLint*)&auxType);

        if (destFormat == auxFormat &&
            destType == auxType)
        {
            fallback = false;
        }
    } else {
        switch (destFormat) {
            case LOCAL_GL_RGB: {
                if (destType == LOCAL_GL_UNSIGNED_SHORT_5_6_5_REV)
                    fallback = false;
                break;
            }
            case LOCAL_GL_BGRA: {
                if (destType == LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV)
                    fallback = false;
                break;
            }
        }
    }

    if (fallback) {
        readFormat = LOCAL_GL_RGBA;
        readType = LOCAL_GL_UNSIGNED_BYTE;
        return false;
    } else {
        readFormat = destFormat;
        readType = destType;
        return true;
    }
}

static void
SwapRAndBComponents(DataSourceSurface* surf)
{
    DataSourceSurface::MappedSurface map;
    MOZ_ALWAYS_TRUE( surf->Map(DataSourceSurface::MapType::READ_WRITE, &map) );
    MOZ_ASSERT(map.mStride >= 0);

    const size_t rowBytes = surf->GetSize().width*4;
    const size_t rowHole = map.mStride - rowBytes;

    uint8_t* row = map.mData;
    if (!row) {
        MOZ_ASSERT(false, "SwapRAndBComponents: Failed to get data from"
                          " DataSourceSurface.");
        surf->Unmap();
        return;
    }

    const size_t rows = surf->GetSize().height;
    for (size_t i = 0; i < rows; i++) {
        const uint8_t* rowEnd = row + rowBytes;

        while (row != rowEnd) {
            Swap(row[0], row[2]);
            row += 4;
        }

        row += rowHole;
    }

    surf->Unmap();
}

static uint16_t
PackRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t pixel = ((r << 11) & 0xf800) |
                     ((g <<  5) & 0x07e0) |
                     ((b      ) & 0x001f);

    return pixel;
}

static void
CopyDataSourceSurface(DataSourceSurface* aSource,
                      DataSourceSurface* aDest)
{
    // Don't worry too much about speed.
    MOZ_ASSERT(aSource->GetSize() == aDest->GetSize());
    MOZ_ASSERT(aSource->GetFormat() == SurfaceFormat::R8G8B8A8 ||
               aSource->GetFormat() == SurfaceFormat::R8G8B8X8 ||
               aSource->GetFormat() == SurfaceFormat::B8G8R8A8 ||
               aSource->GetFormat() == SurfaceFormat::B8G8R8X8);
    MOZ_ASSERT(aDest->GetFormat() == SurfaceFormat::R8G8B8A8 ||
               aDest->GetFormat() == SurfaceFormat::R8G8B8X8 ||
               aDest->GetFormat() == SurfaceFormat::B8G8R8A8 ||
               aDest->GetFormat() == SurfaceFormat::B8G8R8X8 ||
               aDest->GetFormat() == SurfaceFormat::R5G6B5);

    const bool isSrcBGR = aSource->GetFormat() == SurfaceFormat::B8G8R8A8 ||
                          aSource->GetFormat() == SurfaceFormat::B8G8R8X8;
    const bool isDestBGR = aDest->GetFormat() == SurfaceFormat::B8G8R8A8 ||
                           aDest->GetFormat() == SurfaceFormat::B8G8R8X8;
    const bool needsSwap02 = isSrcBGR != isDestBGR;

    const bool srcHasAlpha = aSource->GetFormat() == SurfaceFormat::R8G8B8A8 ||
                             aSource->GetFormat() == SurfaceFormat::B8G8R8A8;
    const bool destHasAlpha = aDest->GetFormat() == SurfaceFormat::R8G8B8A8 ||
                              aDest->GetFormat() == SurfaceFormat::B8G8R8A8;
    const bool needsAlphaMask = !srcHasAlpha && destHasAlpha;

    const bool needsConvertTo16Bits = aDest->GetFormat() == SurfaceFormat::R5G6B5;

    DataSourceSurface::MappedSurface srcMap;
    DataSourceSurface::MappedSurface destMap;
    MOZ_ALWAYS_TRUE( aSource->Map(DataSourceSurface::MapType::READ, &srcMap) );
    MOZ_ALWAYS_TRUE( aDest->Map(DataSourceSurface::MapType::WRITE, &destMap) );
    MOZ_ASSERT(srcMap.mStride >= 0);
    MOZ_ASSERT(destMap.mStride >= 0);

    const size_t srcBPP = BytesPerPixel(aSource->GetFormat());
    const size_t srcRowBytes = aSource->GetSize().width * srcBPP;
    const size_t srcRowHole = srcMap.mStride - srcRowBytes;

    const size_t destBPP = BytesPerPixel(aDest->GetFormat());
    const size_t destRowBytes = aDest->GetSize().width * destBPP;
    const size_t destRowHole = destMap.mStride - destRowBytes;

    uint8_t* srcRow = srcMap.mData;
    uint8_t* destRow = destMap.mData;
    const size_t rows = aSource->GetSize().height;
    for (size_t i = 0; i < rows; i++) {
        const uint8_t* srcRowEnd = srcRow + srcRowBytes;

        while (srcRow != srcRowEnd) {
            uint8_t d0 = needsSwap02 ? srcRow[2] : srcRow[0];
            uint8_t d1 = srcRow[1];
            uint8_t d2 = needsSwap02 ? srcRow[0] : srcRow[2];
            uint8_t d3 = needsAlphaMask ? 0xff : srcRow[3];

            if (needsConvertTo16Bits) {
                *(uint16_t*)destRow = PackRGB565(d0, d1, d2);
            } else {
                destRow[0] = d0;
                destRow[1] = d1;
                destRow[2] = d2;
                destRow[3] = d3;
            }
            srcRow += srcBPP;
            destRow += destBPP;
        }

        srcRow += srcRowHole;
        destRow += destRowHole;
    }

    aSource->Unmap();
    aDest->Unmap();
}

static int
CalcRowStride(int width, int pixelSize, int alignment)
{
    MOZ_ASSERT(alignment);

    int rowStride = width * pixelSize;
    if (rowStride % alignment) { // Extra at the end of the line?
        int alignmentCount = rowStride / alignment;
        rowStride = (alignmentCount+1) * alignment;
    }
    return rowStride;
}

static int
GuessAlignment(int width, int pixelSize, int rowStride)
{
    int alignment = 8; // Max GLES allows.
    while (CalcRowStride(width, pixelSize, alignment) != rowStride) {
        alignment /= 2;
        if (!alignment) {
            NS_WARNING("Bad alignment for GLES. Will use temp surf for readback.");
            return 0;
        }
    }
    return alignment;
}

void
ReadPixelsIntoDataSurface(GLContext* gl, DataSourceSurface* dest)
{
    gl->MakeCurrent();
    MOZ_ASSERT(dest->GetSize().width != 0);
    MOZ_ASSERT(dest->GetSize().height != 0);

    bool hasAlpha = dest->GetFormat() == SurfaceFormat::B8G8R8A8 ||
                    dest->GetFormat() == SurfaceFormat::R8G8B8A8;

    int destPixelSize;
    GLenum destFormat;
    GLenum destType;

    switch (dest->GetFormat()) {
    case SurfaceFormat::B8G8R8A8:
    case SurfaceFormat::B8G8R8X8:
        // Needs host (little) endian ARGB.
        destFormat = LOCAL_GL_BGRA;
        destType = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
        break;
    case SurfaceFormat::R8G8B8A8:
    case SurfaceFormat::R8G8B8X8:
        // Needs host (little) endian ABGR.
        destFormat = LOCAL_GL_RGBA;
        destType = LOCAL_GL_UNSIGNED_BYTE;
        break;
    case SurfaceFormat::R5G6B5:
        destFormat = LOCAL_GL_RGB;
        destType = LOCAL_GL_UNSIGNED_SHORT_5_6_5_REV;
        break;
    default:
        MOZ_CRASH("Bad format.");
    }
    destPixelSize = BytesPerPixel(dest->GetFormat());
    MOZ_ASSERT(dest->GetSize().width * destPixelSize <= dest->Stride());

    GLenum readFormat = destFormat;
    GLenum readType = destType;
    bool needsTempSurf = !GetActualReadFormats(gl,
                                               destFormat, destType,
                                               readFormat, readType);

    RefPtr<DataSourceSurface> tempSurf;
    DataSourceSurface* readSurf = dest;
    int readAlignment = GuessAlignment(dest->GetSize().width,
                                       destPixelSize,
                                       dest->Stride());
    if (!readAlignment) {
        needsTempSurf = true;
    }
    if (needsTempSurf) {
        if (gl->DebugMode()) {
            NS_WARNING("Needing intermediary surface for ReadPixels. This will be slow!");
        }
        SurfaceFormat readFormatGFX;

        switch (readFormat) {
            case LOCAL_GL_RGBA: {
                readFormatGFX = hasAlpha ? SurfaceFormat::R8G8B8A8
                                         : SurfaceFormat::R8G8B8X8;
                break;
            }
            case LOCAL_GL_BGRA: {
                readFormatGFX = hasAlpha ? SurfaceFormat::B8G8R8A8
                                         : SurfaceFormat::B8G8R8X8;
                break;
            }
            case LOCAL_GL_RGB: {
                MOZ_ASSERT(destPixelSize == 2);
                MOZ_ASSERT(readType == LOCAL_GL_UNSIGNED_SHORT_5_6_5_REV);
                readFormatGFX = SurfaceFormat::R5G6B5;
                break;
            }
            default: {
                MOZ_CRASH("Bad read format.");
            }
        }

        switch (readType) {
            case LOCAL_GL_UNSIGNED_BYTE: {
                MOZ_ASSERT(readFormat == LOCAL_GL_RGBA);
                readAlignment = 1;
                break;
            }
            case LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV: {
                MOZ_ASSERT(readFormat == LOCAL_GL_BGRA);
                readAlignment = 4;
                break;
            }
            case LOCAL_GL_UNSIGNED_SHORT_5_6_5_REV: {
                MOZ_ASSERT(readFormat == LOCAL_GL_RGB);
                readAlignment = 2;
                break;
            }
            default: {
                MOZ_CRASH("Bad read type.");
            }
        }

        int32_t stride = dest->GetSize().width * BytesPerPixel(readFormatGFX);
        tempSurf = Factory::CreateDataSourceSurfaceWithStride(dest->GetSize(),
                                                              readFormatGFX,
                                                              stride);
        if (NS_WARN_IF(!tempSurf)) {
            return;
        }

        readSurf = tempSurf;
    }
    MOZ_ASSERT(readAlignment);
    MOZ_ASSERT(reinterpret_cast<uintptr_t>(readSurf->GetData()) % readAlignment == 0);

    GLint currentPackAlignment = 0;
    gl->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &currentPackAlignment);

    if (currentPackAlignment != readAlignment)
        gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, readAlignment);

    GLsizei width = dest->GetSize().width;
    GLsizei height = dest->GetSize().height;

    gl->fReadPixels(0, 0,
                    width, height,
                    readFormat, readType,
                    readSurf->GetData());

    if (currentPackAlignment != readAlignment)
        gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, currentPackAlignment);

    if (readSurf != dest) {
        MOZ_ASSERT(readFormat == LOCAL_GL_RGBA);
        MOZ_ASSERT(readType == LOCAL_GL_UNSIGNED_BYTE);
        CopyDataSourceSurface(readSurf, dest);
    }

    // Check if GL is giving back 1.0 alpha for
    // RGBA reads to RGBA images from no-alpha buffers.
#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs() &&
        gl->Vendor() == gl::GLVendor::NVIDIA &&
        hasAlpha &&
        width && height)
    {
        GLint alphaBits = 0;
        gl->fGetIntegerv(LOCAL_GL_ALPHA_BITS, &alphaBits);
        if (!alphaBits) {
            const uint32_t alphaMask = gfxPackedPixelNoPreMultiply(0xff,0,0,0);

            MOZ_ASSERT(dest->GetSize().width * destPixelSize == dest->Stride());

            uint32_t* itr = (uint32_t*)dest->GetData();
            uint32_t testPixel = *itr;
            if ((testPixel & alphaMask) != alphaMask) {
                // We need to set the alpha channel to 1.0 manually.
                uint32_t* itrEnd = itr + width*height;  // Stride is guaranteed to be width*4.

                for (; itr != itrEnd; itr++) {
                    *itr |= alphaMask;
                }
            }
        }
    }
#endif
}

static TemporaryRef<DataSourceSurface>
YInvertImageSurface(DataSourceSurface* aSurf)
{
    RefPtr<DataSourceSurface> temp =
      Factory::CreateDataSourceSurfaceWithStride(aSurf->GetSize(),
                                                 aSurf->GetFormat(),
                                                 aSurf->Stride());
    if (NS_WARN_IF(!temp)) {
        return nullptr;
    }

    DataSourceSurface::MappedSurface map;
    if (!temp->Map(DataSourceSurface::MapType::WRITE, &map)) {
        return nullptr;
    }

    RefPtr<DrawTarget> dt =
      Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                       map.mData,
                                       temp->GetSize(),
                                       map.mStride,
                                       temp->GetFormat());
    if (!dt) {
        temp->Unmap();
        return nullptr;
    }

    dt->SetTransform(Matrix::Translation(0.0, aSurf->GetSize().height) *
                     Matrix::Scaling(1.0, -1.0));
    Rect rect(0, 0, aSurf->GetSize().width, aSurf->GetSize().height);
    dt->DrawSurface(aSurf, rect, rect, DrawSurfaceOptions(),
                    DrawOptions(1.0, CompositionOp::OP_SOURCE, AntialiasMode::NONE));
    temp->Unmap();
    return temp.forget();
}

TemporaryRef<DataSourceSurface>
ReadBackSurface(GLContext* gl, GLuint aTexture, bool aYInvert, SurfaceFormat aFormat)
{
    gl->MakeCurrent();
    gl->GuaranteeResolve();
    gl->fActiveTexture(LOCAL_GL_TEXTURE0);
    gl->fBindTexture(LOCAL_GL_TEXTURE_2D, aTexture);

    IntSize size;
    gl->fGetTexLevelParameteriv(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_TEXTURE_WIDTH, &size.width);
    gl->fGetTexLevelParameteriv(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_TEXTURE_HEIGHT, &size.height);

    RefPtr<DataSourceSurface> surf =
      Factory::CreateDataSourceSurfaceWithStride(size, SurfaceFormat::B8G8R8A8,
                                                 GetAlignedStride<4>(size.width * BytesPerPixel(SurfaceFormat::B8G8R8A8)));

    if (NS_WARN_IF(!surf)) {
        return nullptr;
    }

    uint32_t currentPackAlignment = 0;
    gl->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, (GLint*)&currentPackAlignment);
    if (currentPackAlignment != 4) {
        gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 4);
    }

    gl->fGetTexImage(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, surf->GetData());

    if (currentPackAlignment != 4) {
        gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, currentPackAlignment);
    }

    if (aFormat == SurfaceFormat::R8G8B8A8 || aFormat == SurfaceFormat::R8G8B8X8) {
        SwapRAndBComponents(surf);
    }

    if (aYInvert) {
        surf = YInvertImageSurface(surf);
    }

    return surf.forget();
}

#define CLEANUP_IF_GLERROR_OCCURRED(x)                                      \
    if (DidGLErrorOccur(x)) {                                               \
        isurf = nullptr;                                                    \
        break;                                                              \
    }

TemporaryRef<DataSourceSurface>
GLReadTexImageHelper::ReadTexImage(GLuint aTextureId,
                                   GLenum aTextureTarget,
                                   const gfx::IntSize& aSize,
    /* ShaderConfigOGL.mFeature */ int aConfig,
                                   bool aYInvert)
{
    MOZ_ASSERT(aTextureTarget == LOCAL_GL_TEXTURE_2D ||
               aTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL ||
               aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB);

    mGL->MakeCurrent();

    /* Allocate resulting image surface */
    int32_t stride = aSize.width * BytesPerPixel(SurfaceFormat::R8G8B8A8);
    RefPtr<DataSourceSurface> isurf =
        Factory::CreateDataSourceSurfaceWithStride(aSize,
                                                   SurfaceFormat::R8G8B8A8,
                                                   stride);
    if (NS_WARN_IF(isurf)) {
        return nullptr;
    }

    GLint oldrb, oldfb, oldprog, oldTexUnit, oldTex;
    GLuint rb, fb;

    do {
        mGL->fGetIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, &oldrb);
        mGL->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &oldfb);
        mGL->fGetIntegerv(LOCAL_GL_CURRENT_PROGRAM, &oldprog);
        mGL->fGetIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &oldTexUnit);
        mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
        switch (aTextureTarget) {
        case LOCAL_GL_TEXTURE_2D:
            mGL->fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_2D, &oldTex);
            break;
        case LOCAL_GL_TEXTURE_EXTERNAL:
            mGL->fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_EXTERNAL, &oldTex);
            break;
        case LOCAL_GL_TEXTURE_RECTANGLE:
            mGL->fGetIntegerv(LOCAL_GL_TEXTURE_BINDING_RECTANGLE, &oldTex);
            break;
        default: /* Already checked above */
            break;
        }

        ScopedGLState scopedScissorTestState(mGL, LOCAL_GL_SCISSOR_TEST, false);
        ScopedGLState scopedBlendState(mGL, LOCAL_GL_BLEND, false);
        ScopedViewportRect scopedViewportRect(mGL, 0, 0, aSize.width, aSize.height);

        /* Setup renderbuffer */
        mGL->fGenRenderbuffers(1, &rb);
        mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, rb);

        GLenum rbInternalFormat =
            mGL->IsGLES()
                ? (mGL->IsExtensionSupported(GLContext::OES_rgb8_rgba8) ? LOCAL_GL_RGBA8 : LOCAL_GL_RGBA4)
                : LOCAL_GL_RGBA;
        mGL->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER, rbInternalFormat, aSize.width, aSize.height);
        CLEANUP_IF_GLERROR_OCCURRED("when binding and creating renderbuffer");

        /* Setup framebuffer */
        mGL->fGenFramebuffers(1, &fb);
        mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fb);
        mGL->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                      LOCAL_GL_RENDERBUFFER, rb);
        CLEANUP_IF_GLERROR_OCCURRED("when binding and creating framebuffer");

        MOZ_ASSERT(mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) == LOCAL_GL_FRAMEBUFFER_COMPLETE);

        /* Setup vertex and fragment shader */
        GLuint program = TextureImageProgramFor(aTextureTarget, aConfig);
        MOZ_ASSERT(program);

        mGL->fUseProgram(program);
        CLEANUP_IF_GLERROR_OCCURRED("when using program");
        mGL->fUniform1i(mGL->fGetUniformLocation(program, "uTexture"), 0);
        CLEANUP_IF_GLERROR_OCCURRED("when setting uniform location");

        /* Setup quad geometry */
        mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

        float w = (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) ? (float) aSize.width : 1.0f;
        float h = (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) ? (float) aSize.height : 1.0f;

        const float
        vertexArray[4*2] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f,  1.0f,
            1.0f,  1.0f
        };
        ScopedVertexAttribPointer autoAttrib0(mGL, 0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, 0, vertexArray);

        const float u0 = 0.0f;
        const float u1 = w;
        const float v0 = aYInvert ? h : 0.0f;
        const float v1 = aYInvert ? 0.0f : h;
        const float texCoordArray[8] = { u0, v0,
                                         u1, v0,
                                         u0, v1,
                                         u1, v1 };
        ScopedVertexAttribPointer autoAttrib1(mGL, 1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, 0, texCoordArray);

        /* Bind the texture */
        if (aTextureId) {
            mGL->fBindTexture(aTextureTarget, aTextureId);
            CLEANUP_IF_GLERROR_OCCURRED("when binding texture");
        }

        /* Draw quad */
        mGL->fClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        mGL->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
        CLEANUP_IF_GLERROR_OCCURRED("when clearing color buffer");

        mGL->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
        CLEANUP_IF_GLERROR_OCCURRED("when drawing texture");

        /* Read-back draw results */
        ReadPixelsIntoDataSurface(mGL, isurf);
        CLEANUP_IF_GLERROR_OCCURRED("when reading pixels into surface");
    } while (false);

    /* Restore GL state */
    mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, oldrb);
    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, oldfb);
    mGL->fUseProgram(oldprog);

    // note that deleting 0 has no effect in any of these calls
    mGL->fDeleteRenderbuffers(1, &rb);
    mGL->fDeleteFramebuffers(1, &fb);

    if (aTextureId)
        mGL->fBindTexture(aTextureTarget, oldTex);

    if (oldTexUnit != LOCAL_GL_TEXTURE0)
        mGL->fActiveTexture(oldTexUnit);

    return isurf.forget();
}

#undef CLEANUP_IF_GLERROR_OCCURRED

}
}
