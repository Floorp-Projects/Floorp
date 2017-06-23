/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEX_UNPACK_BLOB_H_
#define TEX_UNPACK_BLOB_H_

#include "GLContextTypes.h"
#include "mozilla/RefPtr.h"
#include "WebGLStrongTypes.h"
#include "WebGLTypes.h"


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
    const uint32_t mAlignment;
    const uint32_t mRowLength;
    const uint32_t mImageHeight;
    const uint32_t mSkipPixels;
    const uint32_t mSkipRows;
    const uint32_t mSkipImages;
    const uint32_t mWidth;
    const uint32_t mHeight;
    const uint32_t mDepth;

    const gfxAlphaType mSrcAlphaType;

    bool mNeedsExactUpload;

protected:
    TexUnpackBlob(const WebGLContext* webgl, TexImageTarget target, uint32_t rowLength,
                  uint32_t width, uint32_t height, uint32_t depth,
                  gfxAlphaType srcAlphaType);

public:
    virtual ~TexUnpackBlob() { }

protected:
    bool ConvertIfNeeded(WebGLContext* webgl, const char* funcName,
                         const uint32_t rowLength, const uint32_t rowCount,
                         WebGLTexelFormat srcFormat,
                         const uint8_t* const srcBegin, const ptrdiff_t srcStride,
                         WebGLTexelFormat dstFormat, const ptrdiff_t dstStride,

                         const uint8_t** const out_begin,
                         UniqueBuffer* const out_anchoredBuffer) const;

public:
    virtual bool HasData() const { return true; }

    virtual bool Validate(WebGLContext* webgl, const char* funcName,
                          const webgl::PackingInfo& pi) = 0;

    // Returns false when we've generated a WebGL error.
    // Returns true but with a non-zero *out_error if we still need to generate a WebGL
    // error.
    virtual bool TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_error) const = 0;
};

class TexUnpackBytes final : public TexUnpackBlob
{
public:
    const bool mIsClientData;
    const uint8_t* const mPtr;
    const size_t mAvailBytes;

    TexUnpackBytes(const WebGLContext* webgl, TexImageTarget target, uint32_t width,
                   uint32_t height, uint32_t depth, bool isClientData, const uint8_t* ptr,
                   size_t availBytes);

    virtual bool HasData() const override { return !mIsClientData || bool(mPtr); }

    virtual bool Validate(WebGLContext* webgl, const char* funcName,
                          const webgl::PackingInfo& pi) override;
    virtual bool TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_error) const override;
};

class TexUnpackImage final : public TexUnpackBlob
{
public:
    const RefPtr<layers::Image> mImage;

    TexUnpackImage(const WebGLContext* webgl, TexImageTarget target, uint32_t width,
                   uint32_t height, uint32_t depth, layers::Image* image,
                   gfxAlphaType srcAlphaType);

    ~TexUnpackImage(); // Prevent needing to define layers::Image in the header.

    virtual bool Validate(WebGLContext* webgl, const char* funcName,
                          const webgl::PackingInfo& pi) override;
    virtual bool TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_error) const override;
};

class TexUnpackSurface final : public TexUnpackBlob
{
public:
    const RefPtr<gfx::DataSourceSurface> mSurf;

    TexUnpackSurface(const WebGLContext* webgl, TexImageTarget target, uint32_t width,
                     uint32_t height, uint32_t depth, gfx::DataSourceSurface* surf,
                     gfxAlphaType srcAlphaType);

    virtual bool Validate(WebGLContext* webgl, const char* funcName,
                          const webgl::PackingInfo& pi) override;
    virtual bool TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_error) const override;
};

} // namespace webgl
} // namespace mozilla

#endif // TEX_UNPACK_BLOB_H_
