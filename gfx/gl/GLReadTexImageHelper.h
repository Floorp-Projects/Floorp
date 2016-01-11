/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLREADTEXIMAGEHELPER_H_
#define GLREADTEXIMAGEHELPER_H_

#include "GLContextTypes.h"
#include "mozilla/Attributes.h"
#include "nsSize.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {

namespace gfx {
class DataSourceSurface;
} // namespace gfx

namespace gl {

// Returns true if the `dest{Format,Type}` are the same as the
// `read{Format,Type}`.
bool GetActualReadFormats(GLContext* gl,
                          GLenum destFormat, GLenum destType,
                          GLenum* out_readFormat, GLenum* out_readType);

void ReadPixelsIntoDataSurface(GLContext* aGL,
                               gfx::DataSourceSurface* aSurface);

already_AddRefed<gfx::DataSourceSurface>
ReadBackSurface(GLContext* gl, GLuint aTexture, bool aYInvert, gfx::SurfaceFormat aFormat);

already_AddRefed<gfx::DataSourceSurface>
YInvertImageSurface(gfx::DataSourceSurface* aSurf);

void
SwapRAndBComponents(gfx::DataSourceSurface* surf);

class GLReadTexImageHelper final
{
    // The GLContext is the sole owner of the GLBlitHelper.
    GLContext* mGL;

    GLuint mPrograms[4];

    GLuint TextureImageProgramFor(GLenum aTextureTarget, int aShader);

    bool DidGLErrorOccur(const char* str);

public:

    explicit GLReadTexImageHelper(GLContext* gl);
    ~GLReadTexImageHelper();

    /**
      * Read the image data contained in aTexture, and return it as an ImageSurface.
      * If GL_RGBA is given as the format, a SurfaceFormat::A8R8G8B8_UINT32 surface is returned.
      * Not implemented yet:
      * If GL_RGB is given as the format, a SurfaceFormat::X8R8G8B8_UINT32 surface is returned.
      * If GL_LUMINANCE is given as the format, a SurfaceFormat::A8 surface is returned.
      *
      * THIS IS EXPENSIVE.  It is ridiculously expensive.  Only do this
      * if you absolutely positively must, and never in any performance
      * critical path.
      *
      * NOTE: aShaderProgram is really mozilla::layers::ShaderProgramType. It is
      * passed as int to eliminate including LayerManagerOGLProgram.h here.
      */
    already_AddRefed<gfx::DataSourceSurface> ReadTexImage(GLuint aTextureId,
                                                          GLenum aTextureTarget,
                                                          const gfx::IntSize& aSize,
                                  /* ShaderProgramType */ int aShaderProgram,
                                                          bool aYInvert = false);

    bool ReadTexImage(gfx::DataSourceSurface* aDest,
                      GLuint aTextureId,
                      GLenum aTextureTarget,
                      const gfx::IntSize& aSize,
                      int aShaderProgram,
                      bool aYInvert = false);
};

} // namespace gl
} // namespace mozilla

#endif
