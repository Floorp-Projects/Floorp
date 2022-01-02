/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLUploadHelpers_h_
#define GLUploadHelpers_h_

#include "GLDefs.h"
#include "mozilla/gfx/Types.h"
#include "nsPoint.h"
#include "nsRegionFwd.h"

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}  // namespace gfx

namespace gl {

class GLContext;

/**
 * Uploads image data to an OpenGL texture, initializing the texture
 * first if necessary.
 *
 * \param gl The GL Context to use.
 * \param aData Start of image data of surface to upload.
 *              Corresponds to the first pixel of the texture.
 * \param aDataSize The image data's size.
 * \param aStride The image data's stride.
 * \param aFormat The image data's format.
 * \param aDstRegion Region of the texture to upload.
 * \param aTexture The OpenGL texture to use. Must refer to a valid texture.
 * \param aSize The full size of the texture.
 * \param aOutUploadSize If set, the number of bytes the texture requires will
 *                       be returned here.
 * \param aNeedInit Indicates whether a new texture must be allocated.
 * \param aTextureUnit The texture unit used temporarily to upload the surface.
 *                     This may be overridden, so clients should not rely on
 *                     the aTexture being bound to aTextureUnit after this call,
 *                     or even on aTextureUnit being active.
 * \param aTextureTarget The texture target to use.
 * \return Surface format of this texture.
 */
gfx::SurfaceFormat UploadImageDataToTexture(
    GLContext* gl, unsigned char* aData, const gfx::IntSize& aDataSize,
    const gfx::IntPoint& aDstOffset, int32_t aStride,
    gfx::SurfaceFormat aFormat, const nsIntRegion& aDstRegion, GLuint aTexture,
    const gfx::IntSize& aSize, size_t* aOutUploadSize = nullptr,
    bool aNeedInit = false, GLenum aTextureUnit = LOCAL_GL_TEXTURE0,
    GLenum aTextureTarget = LOCAL_GL_TEXTURE_2D);

/**
 * Convenience wrapper around UploadImageDataToTexture for
 * gfx::DataSourceSurface's.
 *
 * \param aSurface The surface from which to upload image data.
 * \param aSrcPoint Offset into aSurface where this texture's data begins.
 */
gfx::SurfaceFormat UploadSurfaceToTexture(
    GLContext* gl, gfx::DataSourceSurface* aSurface,
    const nsIntRegion& aDstRegion, GLuint aTexture, const gfx::IntSize& aSize,
    size_t* aOutUploadSize = nullptr, bool aNeedInit = false,
    const gfx::IntPoint& aSrcOffset = gfx::IntPoint(0, 0),
    const gfx::IntPoint& aDstOffset = gfx::IntPoint(0, 0),
    GLenum aTextureUnit = LOCAL_GL_TEXTURE0,
    GLenum aTextureTarget = LOCAL_GL_TEXTURE_2D);

bool ShouldUploadSubTextures(GLContext* gl);
bool CanUploadNonPowerOfTwo(GLContext* gl);

}  // namespace gl
}  // namespace mozilla

#endif
