/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEX_UNPACK_BLOB_H_
#define TEX_UNPACK_BLOB_H_

#include "GLContextTypes.h"
#include "GLTypes.h"
#include "WebGLStrongTypes.h"


template <class T>
class RefPtr;

namespace mozilla {

class UniqueBuffer;
class WebGLContext;
class WebGLTexture;

namespace dom {
class Element;
class HTMLCanvasElement;
class HTMLVideoElement;
} // namespace dom

namespace gfx {
class DataSourceSurface;
} // namespace gfx

namespace gl {
class GLContext;
} // namespace gl

namespace layers {
class Image;
class ImageContainer;
} // namespace layers

namespace webgl {

struct PackingInfo;
struct DriverUnpackInfo;

class TexUnpackBlob
{
public:
    const GLsizei mWidth;
    const GLsizei mHeight;
    const GLsizei mDepth;
    const bool mHasData;

protected:
    TexUnpackBlob(GLsizei width, GLsizei height, GLsizei depth, bool hasData)
        : mWidth(width)
        , mHeight(height)
        , mDepth(depth)
        , mHasData(hasData)
    { }

public:
    virtual ~TexUnpackBlob() {}

    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& pi) = 0;

    virtual void TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) = 0;

    static void OriginsForDOM(WebGLContext* webgl, gl::OriginPos* const out_src,
                              gl::OriginPos* const out_dst);
};

class TexUnpackBytes : public TexUnpackBlob
{
public:
    const size_t mByteCount;
    const void* const mBytes;

    TexUnpackBytes(GLsizei width, GLsizei height, GLsizei depth, size_t byteCount,
                    const void* bytes)
        : TexUnpackBlob(width, height, depth, bool(bytes))
        , mByteCount(byteCount)
        , mBytes(bytes)
    { }

    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& pi) override;

    virtual void TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) override;
};

class TexUnpackImage : public TexUnpackBlob
{
public:
    const RefPtr<layers::Image> mImage;
    const bool mIsAlphaPremult;

    TexUnpackImage(const RefPtr<layers::Image>& image, bool isAlphaPremult);
    virtual ~TexUnpackImage() override;

    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& pi) override
    {
        return true;
    }

    virtual void TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) override;
};

class TexUnpackSurface : public TexUnpackBlob
{
public:
    const RefPtr<gfx::SourceSurface> mSurf;
    const bool mIsAlphaPremult;

    TexUnpackSurface(const RefPtr<gfx::SourceSurface>& surf, bool isAlphaPremult);
    virtual ~TexUnpackSurface() override;

    virtual bool ValidateUnpack(WebGLContext* webgl, const char* funcName, bool isFunc3D,
                                const webgl::PackingInfo& pi) override
    {
        return true;
    }

    virtual void TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_glError) override;

protected:
    static bool ConvertSurface(WebGLContext* webgl, const webgl::DriverUnpackInfo* dui,
                               gfx::DataSourceSurface* surf, bool isSurfAlphaPremult,
                               UniqueBuffer* const out_convertedBuffer,
                               uint8_t* const out_convertedAlignment,
                               bool* const out_outOfMemory);
    static bool UploadDataSurface(bool isSubImage, WebGLContext* webgl,
                                  TexImageTarget target, GLint level,
                                  const webgl::DriverUnpackInfo* dui, GLint xOffset,
                                  GLint yOffset, GLint zOffset, GLsizei width,
                                  GLsizei height, gfx::DataSourceSurface* surf,
                                  bool isSurfAlphaPremult, GLenum* const out_glError);
};

} // namespace webgl
} // namespace mozilla

#endif // TEX_UNPACK_BLOB_H_
