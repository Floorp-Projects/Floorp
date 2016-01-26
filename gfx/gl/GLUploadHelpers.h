/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
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
} // namespace gfx

namespace gl {

class GLContext;

/**
  * Creates a RGB/RGBA texture (or uses one provided) and uploads the surface
  * contents to it within aSrcRect.
  *
  * aSrcRect.x/y will be uploaded to 0/0 in the texture, and the size
  * of the texture with be aSrcRect.width/height.
  *
  * If an existing texture is passed through aTexture, it is assumed it
  * has already been initialised with glTexImage2D (or this function),
  * and that its size is equal to or greater than aSrcRect + aDstPoint.
  * You can alternatively set the overwrite flag to true and have a new
  * texture memory block allocated.
  *
  * The aDstPoint parameter is ignored if no texture was provided
  * or aOverwrite is true.
  *
  * \param aData Image data to upload.
  * \param aDstRegion Region of texture to upload to.
  * \param aTexture Texture to use, or 0 to have one created for you.
  * \param aOutUploadSize if set, the number of bytes the texture requires will be returned here
  * \param aOverwrite Over an existing texture with a new one.
  * \param aSrcPoint Offset into aSrc where the region's bound's
  *  TopLeft() sits.
  * \param aPixelBuffer Pass true to upload texture data with an
  *  offset from the base data (generally for pixel buffer objects),
  *  otherwise textures are upload with an absolute pointer to the data.
  * \param aTextureUnit, the texture unit used temporarily to upload the
  *  surface. This testure may be overridden, clients should not rely on
  *  the contents of this texture after this call or even on this
  *  texture unit being active.
  * \return Surface format of this texture.
  */
gfx::SurfaceFormat
UploadImageDataToTexture(GLContext* gl,
                         unsigned char* aData,
                         int32_t aStride,
                         gfx::SurfaceFormat aFormat,
                         const nsIntRegion& aDstRegion,
                         GLuint& aTexture,
                         size_t* aOutUploadSize = nullptr,
                         bool aOverwrite = false,
                         bool aPixelBuffer = false,
                         GLenum aTextureUnit = LOCAL_GL_TEXTURE0,
                         GLenum aTextureTarget = LOCAL_GL_TEXTURE_2D);

/**
  * Convenience wrapper around UploadImageDataToTexture for gfx::DataSourceSurface's.
  */
gfx::SurfaceFormat
UploadSurfaceToTexture(GLContext* gl,
                       gfx::DataSourceSurface *aSurface,
                       const nsIntRegion& aDstRegion,
                       GLuint& aTexture,
                       size_t* aOutUploadSize = nullptr,
                       bool aOverwrite = false,
                       const gfx::IntPoint& aSrcPoint = gfx::IntPoint(0, 0),
                       bool aPixelBuffer = false,
                       GLenum aTextureUnit = LOCAL_GL_TEXTURE0,
                       GLenum aTextureTarget = LOCAL_GL_TEXTURE_2D);

bool CanUploadSubTextures(GLContext* gl);
bool CanUploadNonPowerOfTwo(GLContext* gl);

} // namespace gl
} // namespace mozilla

#endif
