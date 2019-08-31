/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#  include <windows.h>
#endif

namespace mozilla {

namespace layers {
class D3D11ShareHandleImage;
class D3D11YCbCrImage;
class Image;
class GPUVideoImage;
class PlanarYCbCrImage;
class SurfaceTextureImage;
class MacIOSurfaceImage;
class SurfaceDescriptorD3D10;
class SurfaceDescriptorDXGIYCbCr;
}  // namespace layers

namespace gl {

class BindAnglePlanes;
class GLContext;
class GLBlitHelper;

bool GuessDivisors(const gfx::IntSize& ySize, const gfx::IntSize& uvSize,
                   gfx::IntSize* const out_divisors);

template <uint8_t N>
struct Mat {
  float m[N * N];  // column-major, for GL

  float& at(const uint8_t x, const uint8_t y) { return m[N * x + y]; }

  static Mat<N> Zero();
  static Mat<N> I();

  Mat<N> operator*(const Mat<N>& r) const;
};
typedef Mat<3> Mat3;

Mat3 SubRectMat3(float x, float y, float w, float h);
Mat3 SubRectMat3(const gfx::IntRect& subrect, const gfx::IntSize& size);
Mat3 SubRectMat3(const gfx::IntRect& bigSubrect, const gfx::IntSize& smallSize,
                 const gfx::IntSize& divisors);

class DrawBlitProg final {
  const GLBlitHelper& mParent;
  const GLuint mProg;
  const GLint mLoc_uDestMatrix;
  const GLint mLoc_uTexMatrix0;
  const GLint mLoc_uTexMatrix1;
  const GLint mLoc_uColorMatrix;
  GLenum mType_uColorMatrix = 0;

 public:
  struct Key final {
    const char* const fragHeader;
    const char* const fragBody;

    bool operator<(const Key& x) const {
      if (fragHeader != x.fragHeader) return fragHeader < x.fragHeader;
      return fragBody < x.fragBody;
    }
  };

  DrawBlitProg(const GLBlitHelper* parent, GLuint prog);
  ~DrawBlitProg();

  struct BaseArgs final {
    Mat3 texMatrix0;
    bool yFlip;
    gfx::IntSize
        destSize;  // Always needed for (at least) setting the viewport.
    Maybe<gfx::IntRect> destRect;
  };
  struct YUVArgs final {
    Mat3 texMatrix1;
    gfx::YUVColorSpace colorSpace;
  };

  void Draw(const BaseArgs& args, const YUVArgs* argsYUV = nullptr) const;
};

class ScopedSaveMultiTex final {
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
class GLBlitHelper final {
  friend class BindAnglePlanes;
  friend class DrawBlitProg;
  friend class GLContext;

  GLContext* const mGL;
  mutable std::map<DrawBlitProg::Key, const DrawBlitProg*> mDrawBlitProgs;

  GLuint mQuadVAO = 0;
  GLuint mQuadVBO = 0;
  nsCString mDrawBlitProg_VersionLine;
  const GLuint mDrawBlitProg_VertShader;

  GLuint mYuvUploads[3] = {};
  gfx::IntSize mYuvUploads_YSize = {0, 0};
  gfx::IntSize mYuvUploads_UVSize = {0, 0};

#ifdef XP_WIN
  mutable RefPtr<ID3D11Device> mD3D11;

  ID3D11Device* GetD3D11() const;
#endif

  const DrawBlitProg* GetDrawBlitProg(const DrawBlitProg::Key& key) const;

 private:
  const DrawBlitProg* CreateDrawBlitProg(const DrawBlitProg::Key& key) const;

 public:
  bool BlitImage(layers::PlanarYCbCrImage* yuvImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin);
#ifdef MOZ_WIDGET_ANDROID
  // Blit onto the current FB.
  bool BlitImage(layers::SurfaceTextureImage* stImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;
#endif
#ifdef XP_MACOSX
  bool BlitImage(layers::MacIOSurfaceImage* srcImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;
#endif

  explicit GLBlitHelper(GLContext* gl);

 public:
  ~GLBlitHelper();

  void BlitFramebuffer(const gfx::IntRect& srcRect,
                       const gfx::IntRect& destRect,
                       GLuint filter = LOCAL_GL_NEAREST) const;
  void BlitFramebufferToFramebuffer(GLuint srcFB, GLuint destFB,
                                    const gfx::IntRect& srcRect,
                                    const gfx::IntRect& destRect,
                                    GLuint filter = LOCAL_GL_NEAREST) const;
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

  void DrawBlitTextureToFramebuffer(
      GLuint srcTex, const gfx::IntSize& srcSize, const gfx::IntSize& destSize,
      GLenum srcTarget = LOCAL_GL_TEXTURE_2D) const;

  bool BlitImageToFramebuffer(layers::Image* srcImage,
                              const gfx::IntSize& destSize,
                              OriginPos destOrigin);

 private:
#ifdef XP_WIN
  // GLBlitHelperD3D.cpp:
  bool BlitImage(layers::GPUVideoImage* srcImage, const gfx::IntSize& destSize,
                 OriginPos destOrigin) const;
  bool BlitImage(layers::D3D11ShareHandleImage* srcImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;
  bool BlitImage(layers::D3D11YCbCrImage* srcImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;

  bool BlitDescriptor(const layers::SurfaceDescriptorD3D10& desc,
                      const gfx::IntSize& destSize, OriginPos destOrigin) const;

  bool BlitAngleYCbCr(const WindowsHandle (&handleList)[3],
                      const gfx::IntRect& clipRect, const gfx::IntSize& ySize,
                      const gfx::IntSize& uvSize,
                      const gfx::YUVColorSpace colorSpace,
                      const gfx::IntSize& destSize, OriginPos destOrigin) const;

  bool BlitAnglePlanes(uint8_t numPlanes,
                       const RefPtr<ID3D11Texture2D>* texD3DList,
                       const DrawBlitProg* prog,
                       const DrawBlitProg::BaseArgs& baseArgs,
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

}  // namespace gl
}  // namespace mozilla

#endif  // GLBLITHELPER_H_
