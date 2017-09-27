/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLBLITHELPER_H_
#define GLBLITHELPER_H_

#include "GLContextTypes.h"
#include "GLConsts.h"
#include "nsSize.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/gfx/Point.h"
#include "../layers/ImageTypes.h"

#ifdef XP_WIN
#include <windows.h>
#endif

namespace mozilla {

namespace layers {
class D3D11YCbCrImage;
class Image;
class GPUVideoImage;
class PlanarYCbCrImage;
class SurfaceTextureImage;
class MacIOSurfaceImage;
class SurfaceDescriptorD3D10;
class SurfaceDescriptorDXGIYCbCr;
} // namespace layers

namespace gl {

class BindAnglePlanes;
class GLContext;

bool
GuessDivisors(const gfx::IntSize& ySize, const gfx::IntSize& uvSize,
              gfx::IntSize* const out_divisors);

class DrawBlitProg final
{
    const GLBlitHelper& mParent;
    const GLuint mProg;
    const GLint mLoc_u1ForYFlip;
    const GLint mLoc_uSrcRect;
    const GLint mLoc_uTexSize0;
    const GLint mLoc_uTexSize1;
    const GLint mLoc_uDivisors;
    const GLint mLoc_uColorMatrix;

public:
    struct Key final {
        const char* fragHeader;
        const char* fragBody;

        bool operator <(const Key& x) const {
            if (fragHeader != x.fragHeader)
                return fragHeader < x.fragHeader;
            return fragBody < x.fragBody;
        }
    };

    DrawBlitProg(const GLBlitHelper* parent, GLuint prog);
    ~DrawBlitProg();

    struct BaseArgs final {
        gfx::IntSize destSize;
        bool yFlip;
        gfx::IntRect srcRect;
        gfx::IntSize texSize0;
    };
    struct YUVArgs final {
        gfx::IntSize texSize1;
        gfx::IntSize divisors;
        YUVColorSpace colorSpace;
    };

    void Draw(const BaseArgs& args, const YUVArgs* argsYUV = nullptr) const;
};

class ScopedSaveMultiTex final
{
    GLContext& mGL;
    const uint8_t mTexCount;
    const GLenum mTexTarget;
    const GLuint mOldTexUnit;
    GLuint mOldTexSampler[3];
    GLuint mOldTex[3];

public:
    ScopedSaveMultiTex(GLContext* gl, uint8_t texCount, GLenum texTarget);
    ~ScopedSaveMultiTex();
};

/** Buffer blitting helper */
class GLBlitHelper final
{
    friend class BindAnglePlanes;
    friend class DrawBlitProg;
    friend class GLContext;

    GLContext* const mGL;
    mutable std::map<DrawBlitProg::Key, const DrawBlitProg*> mDrawBlitProgs;

    GLuint mQuadVAO;
    GLuint mQuadVBO;
    nsCString mDrawBlitProg_VersionLine;
    const GLuint mDrawBlitProg_VertShader;

    GLuint mYuvUploads[3];
    gfx::IntSize mYuvUploads_YSize;
    gfx::IntSize mYuvUploads_UVSize;

#ifdef XP_WIN
    mutable RefPtr<ID3D11Device> mD3D11;

    ID3D11Device* GetD3D11() const;
#endif

    const DrawBlitProg* GetDrawBlitProg(const DrawBlitProg::Key& key) const;
private:
    const DrawBlitProg* CreateDrawBlitProg(const DrawBlitProg::Key& key) const;
public:

    bool BlitImage(layers::PlanarYCbCrImage* yuvImage, const gfx::IntSize& destSize,
                   OriginPos destOrigin);
#ifdef MOZ_WIDGET_ANDROID
    // Blit onto the current FB.
    bool BlitImage(layers::SurfaceTextureImage* stImage, const gfx::IntSize& destSize,
                   OriginPos destOrigin) const;
#endif
#ifdef XP_MACOSX
    bool BlitImage(layers::MacIOSurfaceImage* srcImage, const gfx::IntSize& destSize,
                   OriginPos destOrigin) const;
#endif

    explicit GLBlitHelper(GLContext* gl);
public:
    ~GLBlitHelper();

    void BlitFramebuffer(const gfx::IntSize& srcSize,
                         const gfx::IntSize& destSize) const;
    void BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                      const gfx::IntSize& srcSize,
                                      const gfx::IntSize& destSize) const;
    void BlitFramebufferToTexture(GLuint destTex, const gfx::IntSize& srcSize,
                                  const gfx::IntSize& destSize,
                                  GLenum destTarget = LOCAL_GL_TEXTURE_2D) const;
    void BlitTextureToFramebuffer(GLuint srcTex, const gfx::IntSize& srcSize,
                                  const gfx::IntSize& destSize,
                                  GLenum srcTarget = LOCAL_GL_TEXTURE_2D) const;
    void BlitTextureToTexture(GLuint srcTex, GLuint destTex,
                              const gfx::IntSize& srcSize,
                              const gfx::IntSize& destSize,
                              GLenum srcTarget = LOCAL_GL_TEXTURE_2D,
                              GLenum destTarget = LOCAL_GL_TEXTURE_2D) const;


    void DrawBlitTextureToFramebuffer(GLuint srcTex, const gfx::IntSize& srcSize,
                                      const gfx::IntSize& destSize,
                                      GLenum srcTarget = LOCAL_GL_TEXTURE_2D) const;

    bool BlitImageToFramebuffer(layers::Image* srcImage, const gfx::IntSize& destSize,
                                OriginPos destOrigin);

private:
#ifdef XP_WIN
    // GLBlitHelperD3D.cpp:
    bool BlitImage(layers::GPUVideoImage* srcImage, const gfx::IntSize& destSize,
                   OriginPos destOrigin) const;
    bool BlitImage(layers::D3D11YCbCrImage* srcImage, const gfx::IntSize& destSize,
                   OriginPos destOrigin) const;

    bool BlitDescriptor(const layers::SurfaceDescriptorD3D10& desc,
                        const gfx::IntSize& destSize, OriginPos destOrigin) const;

    bool BlitAngleYCbCr(const WindowsHandle (&handleList)[3],
                        const gfx::IntRect& clipRect,
                        const gfx::IntSize& ySize, const gfx::IntSize& uvSize,
                        const YUVColorSpace colorSpace,
                        const gfx::IntSize& destSize, OriginPos destOrigin) const;

    bool BlitAnglePlanes(uint8_t numPlanes, const RefPtr<ID3D11Texture2D>* texD3DList,
                         const DrawBlitProg* prog, const DrawBlitProg::BaseArgs& baseArgs,
                         const DrawBlitProg::YUVArgs* const yuvArgs) const;
#endif
};

extern const char* const kFragHeader_Tex2D;
extern const char* const kFragHeader_Tex2DRect;
extern const char* const kFragHeader_TexExt;
extern const char* const kFragBody_RGBA;
extern const char* const kFragBody_CrYCb;
extern const char* const kFragBody_NV12;
extern const char* const kFragBody_PlanarYUV;

} // namespace gl
} // namespace mozilla

#endif // GLBLITHELPER_H_
