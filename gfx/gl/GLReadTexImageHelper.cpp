/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLReadTexImageHelper.h"
#include "GLContext.h"
#include "OGLShaderProgram.h"
#include "gfxTypes.h"
#include "gfxContext.h"
#include "ScopedGLHelpers.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

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
GLReadTexImageHelper::TextureImageProgramFor(GLenum aTextureTarget, int aConfig) {
    int variant = 0;
    const GLchar* readTextureImageFS = nullptr;
    if (aTextureTarget == LOCAL_GL_TEXTURE_2D)
    {
        if (aConfig & mozilla::layers::ENABLE_TEXTURE_RB_SWAP)
        {   // Need to swizzle R/B.
            readTextureImageFS = readTextureImageFS_TEXTURE_2D_BGRA;
            variant = 1;
        }
        else
        {
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
    if (gl->IsGLES2()) {
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

static void SwapRAndBComponents(DataSourceSurface* surf)
{
  uint8_t *row = surf->GetData();

  size_t rowBytes = surf->GetSize().width*4;
  size_t rowHole = surf->Stride() - rowBytes;

  size_t rows = surf->GetSize().height;

  while (rows) {

    const uint8_t *rowEnd = row + rowBytes;

    while (row != rowEnd) {
      row[0] ^= row[2];
      row[2] ^= row[0];
      row[0] ^= row[2];
      row += 4;
    }

    row += rowHole;
    --rows;
  }
}

static uint16_t PackRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t pixel = ((r << 11) & 0xf800) |
                     ((g <<  5) & 0x07e0) |
                     ((b      ) & 0x001f);

    return pixel;
}

static void CopyDataSourceSurface(DataSourceSurface* aSource,
                                  DataSourceSurface* aDest)
{
  MOZ_ASSERT(aSource->GetSize() == aDest->GetSize());
  MOZ_ASSERT(aSource->GetFormat() == SurfaceFormat::R8G8B8A8 ||
             aSource->GetFormat() == SurfaceFormat::R8G8B8X8);

  uint8_t *srcRow = aSource->GetData();
  size_t srcRowBytes = aSource->GetSize().width * BytesPerPixel(aSource->GetFormat());
  size_t srcRowHole = aSource->Stride() - srcRowBytes;

  uint8_t *destRow = aDest->GetData();
  size_t destRowBytes = aDest->GetSize().width * BytesPerPixel(aDest->GetFormat());
  size_t destRowHole = aDest->Stride() - destRowBytes;

  bool needsRBSwap = false;
  if (aDest->GetFormat() == SurfaceFormat::B8G8R8A8 ||
      aDest->GetFormat() == SurfaceFormat::B8G8R8X8 ||
      aDest->GetFormat() == SurfaceFormat::R5G6B5) {
      needsRBSwap = true;
  }

  bool needsConvertTo16Bits = false;
  if (aDest->GetFormat() == SurfaceFormat::R5G6B5) {
      needsConvertTo16Bits = true;
  }

  size_t rows = aSource->GetSize().height;

  while (rows) {
    const uint8_t *srcRowEnd = srcRow + srcRowBytes;

    while (srcRow != srcRowEnd) {
      uint8_t r = needsRBSwap ? srcRow[2] : srcRow[0];
      uint8_t g = srcRow[1];
      uint8_t b = needsRBSwap ? srcRow[0] : srcRow[2];
      uint8_t a = srcRow[3];

      if (needsConvertTo16Bits) {
        *(uint16_t*)destRow = PackRGB565(r, g, b);
      } else {
        destRow[0] = r;
        destRow[1] = g;
        destRow[2] = b;
        destRow[3] = a;
      }
      srcRow += BytesPerPixel(aSource->GetFormat());
      destRow += BytesPerPixel(aDest->GetFormat());
    }

    srcRow += srcRowHole;
    destRow += destRowHole;
    --rows;
  }
}

static int
CalcStride(int width, int pixelSize, int alignment)
{
    MOZ_ASSERT(alignment);

    int stride = width * pixelSize;
    if (stride % alignment) { // Extra at the end of the line?
        int alignmentCount = stride / alignment;
        stride = (alignmentCount+1) * alignment;
    }
    return stride;
}

static int
GuessAlignment(int width, int pixelSize, int stride)
{
    int alignment = 8; // Max GLES allows.
    while (CalcStride(width, pixelSize, alignment) != stride) {
        alignment /= 2;
        if (!alignment) {
            MOZ_ASSERT(alignment);
            return 1;
        }
    }
    return alignment;
}

void
ReadPixelsIntoImageSurface(GLContext* gl, gfxImageSurface* dest) {
    gl->MakeCurrent();
    MOZ_ASSERT(dest->GetSize() != gfxIntSize(0, 0));

    /* gfxImageFormat::ARGB32:
     * RGBA+UByte:   be[RGBA], le[ABGR]
     * RGBA+UInt:    be[ABGR], le[RGBA]
     * BGRA+UInt:    be[ARGB], le[BGRA]
     * BGRA+UIntRev: be[BGRA], le[ARGB]
     *
     * gfxImageFormat::RGB16_565:
     * RGB+UShort: le[rrrrrggg,gggbbbbb]
     */
    bool hasAlpha = dest->Format() == gfxImageFormat::ARGB32;

    int destPixelSize;
    GLenum destFormat;
    GLenum destType;

    switch (dest->Format()) {
        case gfxImageFormat::RGB24: // XRGB
        case gfxImageFormat::ARGB32:
            destPixelSize = 4;
            // Needs host (little) endian ARGB.
            destFormat = LOCAL_GL_BGRA;
            destType = LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV;
            break;

        case gfxImageFormat::RGB16_565:
            destPixelSize = 2;
            destFormat = LOCAL_GL_RGB;
            destType = LOCAL_GL_UNSIGNED_SHORT_5_6_5_REV;
            break;

        default:
            MOZ_CRASH("Bad format.");
    }
    MOZ_ASSERT(dest->Width() * destPixelSize <= dest->Stride());

    GLenum readFormat = destFormat;
    GLenum readType = destType;
    bool needsTempSurf = !GetActualReadFormats(gl,
                                               destFormat, destType,
                                               readFormat, readType);

    nsAutoPtr<gfxImageSurface> tempSurf;
    gfxImageSurface* readSurf = nullptr;
    int readAlignment = 0;
    if (needsTempSurf) {
        if (gl->DebugMode()) {
            NS_WARNING("Needing intermediary surface for ReadPixels. This will be slow!");
        }
        SurfaceFormat readFormatGFX;

        switch (readFormat) {
            case LOCAL_GL_RGBA:
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

        tempSurf = new gfxImageSurface(dest->GetSize(),
                                       SurfaceFormatToImageFormat(readFormatGFX),
                                       false);
        readSurf = tempSurf;
    } else {
        // Figure out alignment. We don't need to know why, we just need it
        // to be valid.
        readAlignment = GuessAlignment(dest->Width(),
                                       destPixelSize,
                                       dest->Stride());
        readSurf = dest;
    }
    MOZ_ASSERT(readAlignment);

    GLint currentPackAlignment = 0;
    gl->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &currentPackAlignment);

    if (currentPackAlignment != readAlignment)
        gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, readAlignment);

    GLsizei width = dest->Width();
    GLsizei height = dest->Height();

    readSurf->Flush();
    gl->fReadPixels(0, 0,
                    width, height,
                    readFormat, readType,
                    readSurf->Data());
    readSurf->MarkDirty();

    if (currentPackAlignment != readAlignment)
        gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, currentPackAlignment);

    if (readSurf != dest) {
        MOZ_ASSERT(readFormat == LOCAL_GL_RGBA);
        MOZ_ASSERT(readType == LOCAL_GL_UNSIGNED_BYTE);
        // So we just copied in RGBA in big endian, or le: 0xAABBGGRR.
        // We want 0xAARRGGBB, so swap R and B:
        dest->Flush();
        RefPtr<DataSourceSurface> readDSurf =
            Factory::CreateWrappingDataSourceSurface(readSurf->Data(),
                                                     readSurf->Stride(),
                                                     ToIntSize(readSurf->GetSize()),
                                                     ImageFormatToSurfaceFormat(readSurf->Format()));
        SwapRAndBComponents(readDSurf);
        dest->MarkDirty();

        gfxContext ctx(dest);
        ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx.SetSource(readSurf);
        ctx.Paint();
    }

    // Check if GL is giving back 1.0 alpha for
    // RGBA reads to RGBA images from no-alpha buffers.
#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs() &&
        gl->Vendor() == gl::GLVendor::NVIDIA &&
        dest->Format() == gfxImageFormat::ARGB32 &&
        width && height)
    {
        GLint alphaBits = 0;
        gl->fGetIntegerv(LOCAL_GL_ALPHA_BITS, &alphaBits);
        if (!alphaBits) {
            const uint32_t alphaMask = gfxPackedPixelNoPreMultiply(0xff,0,0,0);

            MOZ_ASSERT(dest->Width() * destPixelSize == dest->Stride());

            dest->Flush();
            uint32_t* itr = (uint32_t*)dest->Data();
            uint32_t testPixel = *itr;
            if ((testPixel & alphaMask) != alphaMask) {
                // We need to set the alpha channel to 1.0 manually.
                uint32_t* itrEnd = itr + width*height;  // Stride is guaranteed to be width*4.

                for (; itr != itrEnd; itr++) {
                    *itr |= alphaMask;
                }
            }
            dest->MarkDirty();
        }
    }
#endif
}

void
ReadPixelsIntoDataSourceSurface(GLContext* gl, DataSourceSurface* dest) {
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
    DataSourceSurface* readSurf = nullptr;
    int readAlignment = 0;
    if (needsTempSurf) {
        if (gl->DebugMode()) {
            NS_WARNING("Needing intermediary surface for ReadPixels. This will be slow!");
        }
        SurfaceFormat readFormatGFX;

        // If needs temp surface, readFormat is always LOCAL_GL_RGBA
        // and readType is always LOCAL_GL_UNSIGNED_BYTE
        MOZ_ASSERT(readFormat == LOCAL_GL_RGBA);
        MOZ_ASSERT(readType == LOCAL_GL_UNSIGNED_BYTE);
        readFormatGFX = hasAlpha ? SurfaceFormat::R8G8B8A8
                                 : SurfaceFormat::R8G8B8X8;
        readAlignment = 1;
        int32_t stride = dest->GetSize().width * BytesPerPixel(readFormatGFX);
        tempSurf = Factory::CreateDataSourceSurfaceWithStride(dest->GetSize(),
                                                              readFormatGFX,
                                                              stride);
        readSurf = tempSurf;
    } else {
        // Figure out alignment. We don't need to know why, we just need it
        // to be valid.
        readAlignment = GuessAlignment(dest->GetSize().width,
                                       destPixelSize,
                                       dest->Stride());
        readSurf = dest;
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
        (dest->GetFormat() == SurfaceFormat::R8G8B8A8 ||
         dest->GetFormat() == SurfaceFormat::B8G8R8A8) &&
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

static TemporaryRef<DataSourceSurface> YInvertImageSurface(DataSourceSurface* aSurf)
{
  RefPtr<DataSourceSurface> temp =
    Factory::CreateDataSourceSurfaceWithStride(aSurf->GetSize(),
                                               aSurf->GetFormat(),
                                               aSurf->Stride());
  RefPtr<DrawTarget> dt =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     temp->GetData(),
                                     temp->GetSize(),
                                     temp->Stride(),
                                     temp->GetFormat());
  nsRefPtr<gfxContext> ctx = new gfxContext(dt);
  ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx->Scale(1.0, -1.0);
  ctx->Translate(-gfxPoint(0.0, aSurf->GetSize().height));

  nsRefPtr<gfxImageSurface> thebesSurf =
    new gfxImageSurface(aSurf->GetData(),
                        ThebesIntSize(aSurf->GetSize()),
                        aSurf->Stride(),
                        SurfaceFormatToImageFormat(aSurf->GetFormat()));
  ctx->SetSource(thebesSurf);
  ctx->Paint();
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

    if (!surf) {
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

void
ReadScreenIntoImageSurface(GLContext* gl, gfxImageSurface* dest)
{
    ScopedBindFramebuffer autoFB(gl, 0);
    ReadPixelsIntoImageSurface(gl, dest);
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
            mGL->IsGLES2()
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
        mGL->fEnableVertexAttribArray(0);
        mGL->fEnableVertexAttribArray(1);

        float w = (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) ? (float) aSize.width : 1.0f;
        float h = (aTextureTarget == LOCAL_GL_TEXTURE_RECTANGLE) ? (float) aSize.height : 1.0f;


        const float
        vertexArray[4*2] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f,  1.0f,
            1.0f,  1.0f
         };
        mGL->fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, vertexArray);

        const float u0 = 0.0f;
        const float u1 = w;
        const float v0 = aYInvert ? h : 0.0f;
        const float v1 = aYInvert ? 0.0f : h;
        const float texCoordArray[8] = { u0, v0,
                                         u1, v0,
                                         u0, v1,
                                         u1, v1 };
        mGL->fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, texCoordArray);

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

        mGL->fDisableVertexAttribArray(1);
        mGL->fDisableVertexAttribArray(0);

        /* Read-back draw results */
        ReadPixelsIntoDataSourceSurface(mGL, isurf);
        CLEANUP_IF_GLERROR_OCCURRED("when reading pixels into surface");
    } while (false);

    /* Restore GL state */
//cleanup:
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
