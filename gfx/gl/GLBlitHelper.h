/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLBLITHELPER_H_
#define GLBLITHELPER_H_

#include <cstdint>
#include <map>
#include "GLConsts.h"
#include "GLContextTypes.h"
#include "GLTypes.h"
#include "nsSize.h"
#include "nsString.h"
#include "nsTString.h"
#include "mozilla/ipc/IPCTypes.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Types.h"

#include <map>

#ifdef XP_WIN
#  include <windows.h>
#  include "mozilla/RefPtr.h"
#  include "mozilla/ipc/IPCTypes.h"
struct ID3D11Device;
struct ID3D11Texture2D;
#endif

#ifdef XP_MACOSX
class MacIOSurface;
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoSurfaceTextureWrappers.h"
#endif

#ifdef MOZ_WAYLAND
class DMABufSurface;
#endif

namespace mozilla {

namespace layers {
class Image;
class GPUVideoImage;
struct PlanarYCbCrData;
class PlanarYCbCrImage;
class SurfaceDescriptor;
class SurfaceDescriptorBuffer;

#ifdef XP_WIN
class D3D11ShareHandleImage;
class D3D11TextureIMFSampleImage;
class D3D11YCbCrImage;
class SurfaceDescriptorD3D10;
class SurfaceDescriptorDXGIYCbCr;
#endif

#ifdef MOZ_WIDGET_ANDROID
class SurfaceTextureDescriptor;
#endif

#ifdef XP_MACOSX
class MacIOSurfaceImage;
#endif

#ifdef MOZ_WAYLAND
class DMABUFSurfaceImage;
#endif
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
  bool BlitPlanarYCbCr(const layers::PlanarYCbCrData&,
                       const gfx::IntSize& destSize, OriginPos destOrigin);
#ifdef MOZ_WIDGET_ANDROID
  bool Blit(const java::GeckoSurfaceTexture::Ref& surfaceTexture,
            const gfx::IntSize& destSize, const OriginPos destOrigin) const;
#endif
#ifdef XP_MACOSX
  bool BlitImage(layers::MacIOSurfaceImage* srcImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;
#endif
#ifdef MOZ_WAYLAND
  bool Blit(DMABufSurface* surface, const gfx::IntSize& destSize,
            OriginPos destOrigin) const;
  bool BlitImage(layers::DMABUFSurfaceImage* srcImage,
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

  void DrawBlitTextureToFramebuffer(GLuint srcTex, const gfx::IntSize& srcSize,
                                    const gfx::IntSize& destSize,
                                    GLenum srcTarget = LOCAL_GL_TEXTURE_2D,
                                    bool srcIsBGRA = false) const;

  bool BlitImageToFramebuffer(layers::Image* srcImage,
                              const gfx::IntSize& destSize,
                              OriginPos destOrigin);
  bool BlitSdToFramebuffer(const layers::SurfaceDescriptor&,
                           const gfx::IntSize& destSize, OriginPos destOrigin);

 private:
  bool BlitImage(layers::GPUVideoImage* srcImage, const gfx::IntSize& destSize,
                 OriginPos destOrigin) const;
#ifdef XP_MACOSX
  bool BlitImage(MacIOSurface* const iosurf, const gfx::IntSize& destSize,
                 OriginPos destOrigin) const;
#endif
#ifdef XP_WIN
  // GLBlitHelperD3D.cpp:
  bool BlitImage(layers::D3D11ShareHandleImage* srcImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;
  bool BlitImage(layers::D3D11TextureIMFSampleImage* srcImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;
  bool BlitImage(layers::D3D11YCbCrImage* srcImage,
                 const gfx::IntSize& destSize, OriginPos destOrigin) const;

  bool BlitDescriptor(const layers::SurfaceDescriptorD3D10& desc,
                      const gfx::IntSize& destSize, OriginPos destOrigin) const;
  bool BlitDescriptor(const layers::SurfaceDescriptorDXGIYCbCr& desc,
                      const gfx::IntSize& destSize,
                      const OriginPos destOrigin) const;
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
