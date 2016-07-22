/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TEX_UNPACK_BLOB_H_
#define TEX_UNPACK_BLOB_H_

#include "GLContextTypes.h"
#include "GLTypes.h"
#include "WebGLStrongTypes.h"
#include "WebGLTypes.h"


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
    const uint32_t mAlignment;
    const uint32_t mRowLength;
    const uint32_t mImageHeight;
    const uint32_t mSkipPixels;
    const uint32_t mSkipRows;
    const uint32_t mSkipImages;
    const uint32_t mWidth;
    const uint32_t mHeight;
    const uint32_t mDepth;
    const bool mIsSrcPremult;

    bool mNeedsExactUpload;

protected:
    TexUnpackBlob(const WebGLContext* webgl, TexImageTarget target, uint32_t rowLength,
                  uint32_t width, uint32_t height, uint32_t depth, bool isSrcPremult);

public:
    virtual ~TexUnpackBlob() { }

protected:
    bool ConvertIfNeeded(WebGLContext* webgl, const char* funcName,
                         const uint8_t* srcBytes, uint32_t srcStride, uint8_t srcBPP,
                         WebGLTexelFormat srcFormat,
                         const webgl::DriverUnpackInfo* dstDUI,
                         const uint8_t** const out_bytes,
                         UniqueBuffer* const out_anchoredBuffer) const;

public:
    virtual bool HasData() const { return true; }

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

    TexUnpackBytes(const WebGLContext* webgl, TexImageTarget target, uint32_t width,
                   uint32_t height, uint32_t depth, bool isClientData, const uint8_t* ptr);

    virtual bool HasData() const override { return !mIsClientData || bool(mPtr); }

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
                   bool isAlphaPremult);

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
                     bool isAlphaPremult);

    virtual bool TexOrSubImage(bool isSubImage, bool needsRespec, const char* funcName,
                               WebGLTexture* tex, TexImageTarget target, GLint level,
                               const webgl::DriverUnpackInfo* dui, GLint xOffset,
                               GLint yOffset, GLint zOffset,
                               GLenum* const out_error) const override;
};

} // namespace webgl
} // namespace mozilla

#endif // TEX_UNPACK_BLOB_H_
