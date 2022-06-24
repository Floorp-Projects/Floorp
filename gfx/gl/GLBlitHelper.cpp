/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLBlitHelper.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "GPUVideoImage.h"
#include "HeapCopyOfStackArray.h"
#include "ImageContainer.h"
#include "ScopedGLHelpers.h"
#include "gfxUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersSurfaces.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidSurfaceTexture.h"
#  include "GLLibraryEGL.h"
#endif

#ifdef XP_MACOSX
#  include "GLContextCGL.h"
#  include "MacIOSurfaceImage.h"
#endif

#ifdef XP_WIN
#  include "mozilla/layers/D3D11ShareHandleImage.h"
#  include "mozilla/layers/D3D11TextureIMFSampleImage.h"
#  include "mozilla/layers/D3D11YCbCrImage.h"
#endif

#ifdef MOZ_WAYLAND
#  include "mozilla/layers/DMABUFSurfaceImage.h"
#  include "mozilla/widget/DMABufSurface.h"
#endif

using mozilla::layers::PlanarYCbCrData;
using mozilla::layers::PlanarYCbCrImage;

namespace mozilla {
namespace gl {

// --

const char* const kFragHeader_Tex2D =
    "\
    #define SAMPLER sampler2D                                                \n\
    #if __VERSION__ >= 130                                                   \n\
        #define TEXTURE texture                                              \n\
    #else                                                                    \n\
        #define TEXTURE texture2D                                            \n\
    #endif                                                                   \n\
";
const char* const kFragHeader_Tex2DRect =
    "\
    #define SAMPLER sampler2DRect                                            \n\
    #if __VERSION__ >= 130                                                   \n\
        #define TEXTURE texture                                              \n\
    #else                                                                    \n\
        #define TEXTURE texture2DRect                                        \n\
    #endif                                                                   \n\
";
const char* const kFragHeader_TexExt =
    "\
    #extension GL_OES_EGL_image_external : require                           \n\
    #if __VERSION__ >= 130                                                   \n\
        #define TEXTURE texture                                              \n\
    #else                                                                    \n\
        #define TEXTURE texture2D                                            \n\
    #endif                                                                   \n\
    #define SAMPLER samplerExternalOES                                       \n\
";

const char* const kFragBody_RGBA =
    "\
    VARYING vec2 vTexCoord0;                                                 \n\
    uniform SAMPLER uTex0;                                                   \n\
                                                                             \n\
    void main(void)                                                          \n\
    {                                                                        \n\
        FRAG_COLOR = TEXTURE(uTex0, vTexCoord0);                             \n\
    }                                                                        \n\
";
const char* const kFragBody_BGRA =
    "\
    VARYING vec2 vTexCoord0;                                                 \n\
    uniform SAMPLER uTex0;                                                   \n\
                                                                             \n\
    void main(void)                                                          \n\
    {                                                                        \n\
        FRAG_COLOR = TEXTURE(uTex0, vTexCoord0).bgra;                        \n\
    }                                                                        \n\
";
const char* const kFragBody_CrYCb =
    "\
    VARYING vec2 vTexCoord0;                                                 \n\
    uniform SAMPLER uTex0;                                                   \n\
    uniform MAT4X3 uColorMatrix;                                             \n\
                                                                             \n\
    void main(void)                                                          \n\
    {                                                                        \n\
        vec4 yuv = vec4(TEXTURE(uTex0, vTexCoord0).gbr,                      \n\
                        1.0);                                                \n\
        FRAG_COLOR = vec4((uColorMatrix * yuv).rgb, 1.0);                    \n\
    }                                                                        \n\
";
const char* const kFragBody_NV12 =
    "\
    VARYING vec2 vTexCoord0;                                                 \n\
    VARYING vec2 vTexCoord1;                                                 \n\
    uniform SAMPLER uTex0;                                                   \n\
    uniform SAMPLER uTex1;                                                   \n\
    uniform MAT4X3 uColorMatrix;                                             \n\
                                                                             \n\
    void main(void)                                                          \n\
    {                                                                        \n\
        vec4 yuv = vec4(TEXTURE(uTex0, vTexCoord0).x,                        \n\
                        TEXTURE(uTex1, vTexCoord1).xy,                       \n\
                        1.0);                                                \n\
        FRAG_COLOR = vec4((uColorMatrix * yuv).rgb, 1.0);                    \n\
    }                                                                        \n\
";
const char* const kFragBody_PlanarYUV =
    "\
    VARYING vec2 vTexCoord0;                                                 \n\
    VARYING vec2 vTexCoord1;                                                 \n\
    uniform SAMPLER uTex0;                                                   \n\
    uniform SAMPLER uTex1;                                                   \n\
    uniform SAMPLER uTex2;                                                   \n\
    uniform MAT4X3 uColorMatrix;                                             \n\
                                                                             \n\
    void main(void)                                                          \n\
    {                                                                        \n\
        vec4 yuv = vec4(TEXTURE(uTex0, vTexCoord0).x,                        \n\
                        TEXTURE(uTex1, vTexCoord1).x,                        \n\
                        TEXTURE(uTex2, vTexCoord1).x,                        \n\
                        1.0);                                                \n\
        FRAG_COLOR = vec4((uColorMatrix * yuv).rgb, 1.0);                    \n\
    }                                                                        \n\
";

// --

template <uint8_t N>
/*static*/ Mat<N> Mat<N>::Zero() {
  Mat<N> ret;
  for (auto& x : ret.m) {
    x = 0.0f;
  }
  return ret;
}

template <uint8_t N>
/*static*/ Mat<N> Mat<N>::I() {
  auto ret = Mat<N>::Zero();
  for (uint8_t i = 0; i < N; i++) {
    ret.at(i, i) = 1.0f;
  }
  return ret;
}

template <uint8_t N>
Mat<N> Mat<N>::operator*(const Mat<N>& r) const {
  Mat<N> ret;
  for (uint8_t x = 0; x < N; x++) {
    for (uint8_t y = 0; y < N; y++) {
      float sum = 0.0f;
      for (uint8_t i = 0; i < N; i++) {
        sum += at(i, y) * r.at(x, i);
      }
      ret.at(x, y) = sum;
    }
  }
  return ret;
}

Mat3 SubRectMat3(const float x, const float y, const float w, const float h) {
  auto ret = Mat3::Zero();
  ret.at(0, 0) = w;
  ret.at(1, 1) = h;
  ret.at(2, 0) = x;
  ret.at(2, 1) = y;
  ret.at(2, 2) = 1.0f;
  return ret;
}

Mat3 SubRectMat3(const gfx::IntRect& subrect, const gfx::IntSize& size) {
  return SubRectMat3(float(subrect.X()) / size.width,
                     float(subrect.Y()) / size.height,
                     float(subrect.Width()) / size.width,
                     float(subrect.Height()) / size.height);
}

Mat3 SubRectMat3(const gfx::IntRect& bigSubrect, const gfx::IntSize& smallSize,
                 const gfx::IntSize& divisors) {
  const float x = float(bigSubrect.X()) / divisors.width;
  const float y = float(bigSubrect.Y()) / divisors.height;
  const float w = float(bigSubrect.Width()) / divisors.width;
  const float h = float(bigSubrect.Height()) / divisors.height;
  return SubRectMat3(x / smallSize.width, y / smallSize.height,
                     w / smallSize.width, h / smallSize.height);
}

// --

ScopedSaveMultiTex::ScopedSaveMultiTex(GLContext* const gl,
                                       const uint8_t texCount,
                                       const GLenum texTarget)
    : mGL(*gl),
      mTexCount(texCount),
      mTexTarget(texTarget),
      mOldTexUnit(mGL.GetIntAs<GLenum>(LOCAL_GL_ACTIVE_TEXTURE)) {
  GLenum texBinding;
  switch (mTexTarget) {
    case LOCAL_GL_TEXTURE_2D:
      texBinding = LOCAL_GL_TEXTURE_BINDING_2D;
      break;
    case LOCAL_GL_TEXTURE_RECTANGLE:
      texBinding = LOCAL_GL_TEXTURE_BINDING_RECTANGLE;
      break;
    case LOCAL_GL_TEXTURE_EXTERNAL:
      texBinding = LOCAL_GL_TEXTURE_BINDING_EXTERNAL;
      break;
    default:
      gfxCriticalError() << "Unhandled texTarget: " << texTarget;
  }

  for (uint8_t i = 0; i < mTexCount; i++) {
    mGL.fActiveTexture(LOCAL_GL_TEXTURE0 + i);
    if (mGL.IsSupported(GLFeature::sampler_objects)) {
      mOldTexSampler[i] = mGL.GetIntAs<GLuint>(LOCAL_GL_SAMPLER_BINDING);
      mGL.fBindSampler(i, 0);
    }
    mOldTex[i] = mGL.GetIntAs<GLuint>(texBinding);
  }
}

ScopedSaveMultiTex::~ScopedSaveMultiTex() {
  for (uint8_t i = 0; i < mTexCount; i++) {
    mGL.fActiveTexture(LOCAL_GL_TEXTURE0 + i);
    if (mGL.IsSupported(GLFeature::sampler_objects)) {
      mGL.fBindSampler(i, mOldTexSampler[i]);
    }
    mGL.fBindTexture(mTexTarget, mOldTex[i]);
  }
  mGL.fActiveTexture(mOldTexUnit);
}

// --

class ScopedBindArrayBuffer final {
 public:
  GLContext& mGL;
  const GLuint mOldVBO;

  ScopedBindArrayBuffer(GLContext* const gl, const GLuint vbo)
      : mGL(*gl), mOldVBO(mGL.GetIntAs<GLuint>(LOCAL_GL_ARRAY_BUFFER_BINDING)) {
    mGL.fBindBuffer(LOCAL_GL_ARRAY_BUFFER, vbo);
  }

  ~ScopedBindArrayBuffer() { mGL.fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mOldVBO); }
};

// --

class ScopedShader final {
  GLContext& mGL;
  const GLuint mName;

 public:
  ScopedShader(GLContext* const gl, const GLenum shaderType)
      : mGL(*gl), mName(mGL.fCreateShader(shaderType)) {}

  ~ScopedShader() { mGL.fDeleteShader(mName); }

  operator GLuint() const { return mName; }
};

// --

class SaveRestoreCurrentProgram final {
  GLContext& mGL;
  const GLuint mOld;

 public:
  explicit SaveRestoreCurrentProgram(GLContext* const gl)
      : mGL(*gl), mOld(mGL.GetIntAs<GLuint>(LOCAL_GL_CURRENT_PROGRAM)) {}

  ~SaveRestoreCurrentProgram() { mGL.fUseProgram(mOld); }
};

// --

class ScopedDrawBlitState final {
  GLContext& mGL;

  const bool blend;
  const bool cullFace;
  const bool depthTest;
  const bool dither;
  const bool polyOffsFill;
  const bool sampleAToC;
  const bool sampleCover;
  const bool scissor;
  const bool stencil;
  Maybe<bool> rasterizerDiscard;

  realGLboolean colorMask[4];
  GLint viewport[4];

 public:
  ScopedDrawBlitState(GLContext* const gl, const gfx::IntSize& destSize)
      : mGL(*gl),
        blend(mGL.PushEnabled(LOCAL_GL_BLEND, false)),
        cullFace(mGL.PushEnabled(LOCAL_GL_CULL_FACE, false)),
        depthTest(mGL.PushEnabled(LOCAL_GL_DEPTH_TEST, false)),
        dither(mGL.PushEnabled(LOCAL_GL_DITHER, true)),
        polyOffsFill(mGL.PushEnabled(LOCAL_GL_POLYGON_OFFSET_FILL, false)),
        sampleAToC(mGL.PushEnabled(LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE, false)),
        sampleCover(mGL.PushEnabled(LOCAL_GL_SAMPLE_COVERAGE, false)),
        scissor(mGL.PushEnabled(LOCAL_GL_SCISSOR_TEST, false)),
        stencil(mGL.PushEnabled(LOCAL_GL_STENCIL_TEST, false)) {
    if (mGL.IsSupported(GLFeature::transform_feedback2)) {
      // Technically transform_feedback2 requires transform_feedback, which
      // actually adds RASTERIZER_DISCARD.
      rasterizerDiscard =
          Some(mGL.PushEnabled(LOCAL_GL_RASTERIZER_DISCARD, false));
    }

    mGL.fGetBooleanv(LOCAL_GL_COLOR_WRITEMASK, colorMask);
    if (mGL.IsSupported(GLFeature::draw_buffers_indexed)) {
      mGL.fColorMaski(0, true, true, true, true);
    } else {
      mGL.fColorMask(true, true, true, true);
    }

    mGL.fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);
    MOZ_ASSERT(destSize.width && destSize.height);
    mGL.fViewport(0, 0, destSize.width, destSize.height);
  }

  ~ScopedDrawBlitState() {
    mGL.SetEnabled(LOCAL_GL_BLEND, blend);
    mGL.SetEnabled(LOCAL_GL_CULL_FACE, cullFace);
    mGL.SetEnabled(LOCAL_GL_DEPTH_TEST, depthTest);
    mGL.SetEnabled(LOCAL_GL_DITHER, dither);
    mGL.SetEnabled(LOCAL_GL_POLYGON_OFFSET_FILL, polyOffsFill);
    mGL.SetEnabled(LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE, sampleAToC);
    mGL.SetEnabled(LOCAL_GL_SAMPLE_COVERAGE, sampleCover);
    mGL.SetEnabled(LOCAL_GL_SCISSOR_TEST, scissor);
    mGL.SetEnabled(LOCAL_GL_STENCIL_TEST, stencil);
    if (rasterizerDiscard) {
      mGL.SetEnabled(LOCAL_GL_RASTERIZER_DISCARD, rasterizerDiscard.value());
    }

    if (mGL.IsSupported(GLFeature::draw_buffers_indexed)) {
      mGL.fColorMaski(0, colorMask[0], colorMask[1], colorMask[2],
                      colorMask[3]);
    } else {
      mGL.fColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    }
    mGL.fViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
  }
};

// --

DrawBlitProg::DrawBlitProg(const GLBlitHelper* const parent, const GLuint prog)
    : mParent(*parent),
      mProg(prog),
      mLoc_uDestMatrix(mParent.mGL->fGetUniformLocation(mProg, "uDestMatrix")),
      mLoc_uTexMatrix0(mParent.mGL->fGetUniformLocation(mProg, "uTexMatrix0")),
      mLoc_uTexMatrix1(mParent.mGL->fGetUniformLocation(mProg, "uTexMatrix1")),
      mLoc_uColorMatrix(
          mParent.mGL->fGetUniformLocation(mProg, "uColorMatrix")) {
  const auto& gl = mParent.mGL;
  MOZ_GL_ASSERT(gl, mLoc_uDestMatrix != -1);
  MOZ_GL_ASSERT(gl, mLoc_uTexMatrix0 != -1);
  if (mLoc_uColorMatrix != -1) {
    MOZ_GL_ASSERT(gl, mLoc_uTexMatrix1 != -1);

    int32_t numActiveUniforms = 0;
    gl->fGetProgramiv(mProg, LOCAL_GL_ACTIVE_UNIFORMS, &numActiveUniforms);

    const size_t kMaxNameSize = 32;
    char name[kMaxNameSize] = {0};
    GLint size = 0;
    GLenum type = 0;
    for (int32_t i = 0; i < numActiveUniforms; i++) {
      gl->fGetActiveUniform(mProg, i, kMaxNameSize, nullptr, &size, &type,
                            name);
      if (strcmp("uColorMatrix", name) == 0) {
        mType_uColorMatrix = type;
        break;
      }
    }
    MOZ_GL_ASSERT(gl, mType_uColorMatrix);
  }
}

DrawBlitProg::~DrawBlitProg() {
  const auto& gl = mParent.mGL;
  if (!gl->MakeCurrent()) return;

  gl->fDeleteProgram(mProg);
}

void DrawBlitProg::Draw(const BaseArgs& args,
                        const YUVArgs* const argsYUV) const {
  const auto& gl = mParent.mGL;

  const SaveRestoreCurrentProgram oldProg(gl);
  gl->fUseProgram(mProg);

  // --

  Mat3 destMatrix;
  if (args.destRect) {
    const auto& destRect = args.destRect.value();
    destMatrix = SubRectMat3(destRect.X() / args.destSize.width,
                             destRect.Y() / args.destSize.height,
                             destRect.Width() / args.destSize.width,
                             destRect.Height() / args.destSize.height);
  } else {
    destMatrix = Mat3::I();
  }

  if (args.yFlip) {
    // Apply the y-flip matrix before the destMatrix.
    // That is, flip y=[0-1] to y=[1-0] before we restrict to the destRect.
    destMatrix.at(2, 1) += destMatrix.at(1, 1);
    destMatrix.at(1, 1) *= -1.0f;
  }

  gl->fUniformMatrix3fv(mLoc_uDestMatrix, 1, false, destMatrix.m);
  gl->fUniformMatrix3fv(mLoc_uTexMatrix0, 1, false, args.texMatrix0.m);

  MOZ_ASSERT(bool(argsYUV) == (mLoc_uColorMatrix != -1));
  if (argsYUV) {
    gl->fUniformMatrix3fv(mLoc_uTexMatrix1, 1, false, argsYUV->texMatrix1.m);

    const auto& colorMatrix =
        gfxUtils::YuvToRgbMatrix4x4ColumnMajor(argsYUV->colorSpace);
    float mat4x3[4 * 3];
    switch (mType_uColorMatrix) {
      case LOCAL_GL_FLOAT_MAT4:
        gl->fUniformMatrix4fv(mLoc_uColorMatrix, 1, false, colorMatrix);
        break;
      case LOCAL_GL_FLOAT_MAT4x3:
        for (int x = 0; x < 4; x++) {
          for (int y = 0; y < 3; y++) {
            mat4x3[3 * x + y] = colorMatrix[4 * x + y];
          }
        }
        gl->fUniformMatrix4x3fv(mLoc_uColorMatrix, 1, false, mat4x3);
        break;
      default:
        gfxCriticalError() << "Bad mType_uColorMatrix: "
                           << gfx::hexa(mType_uColorMatrix);
    }
  }

  // --

  const ScopedDrawBlitState drawState(gl, args.destSize);

  GLuint oldVAO;
  GLint vaa0Enabled;
  GLint vaa0Size;
  GLenum vaa0Type;
  GLint vaa0Normalized;
  GLsizei vaa0Stride;
  GLvoid* vaa0Pointer;
  GLuint vaa0Buffer;
  if (mParent.mQuadVAO) {
    oldVAO = gl->GetIntAs<GLuint>(LOCAL_GL_VERTEX_ARRAY_BINDING);
    gl->fBindVertexArray(mParent.mQuadVAO);
  } else {
    // clang-format off
        gl->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED, &vaa0Enabled);
        gl->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE, &vaa0Size);
        gl->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint*)&vaa0Type);
        gl->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &vaa0Normalized);
        gl->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE, (GLint*)&vaa0Stride);
        gl->fGetVertexAttribPointerv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER, &vaa0Pointer);
    // clang-format on

    gl->fEnableVertexAttribArray(0);
    const ScopedBindArrayBuffer bindVBO(gl, mParent.mQuadVBO);
    vaa0Buffer = bindVBO.mOldVBO;
    gl->fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, false, 0, 0);
  }

  gl->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

  if (mParent.mQuadVAO) {
    gl->fBindVertexArray(oldVAO);
  } else {
    if (vaa0Enabled) {
      gl->fEnableVertexAttribArray(0);
    } else {
      gl->fDisableVertexAttribArray(0);
    }
    // The current VERTEX_ARRAY_BINDING is not necessarily the same as the
    // buffer set for vaa0Buffer.
    const ScopedBindArrayBuffer bindVBO(gl, vaa0Buffer);
    gl->fVertexAttribPointer(0, vaa0Size, vaa0Type, bool(vaa0Normalized),
                             vaa0Stride, vaa0Pointer);
  }
}

// --

GLBlitHelper::GLBlitHelper(GLContext* const gl)
    : mGL(gl),
      mDrawBlitProg_VertShader(mGL->fCreateShader(LOCAL_GL_VERTEX_SHADER))
//, mYuvUploads_YSize(0, 0)
//, mYuvUploads_UVSize(0, 0)
{
  mGL->fGenBuffers(1, &mQuadVBO);
  {
    const ScopedBindArrayBuffer bindVBO(mGL, mQuadVBO);

    const float quadData[] = {0, 0, 1, 0, 0, 1, 1, 1};
    const HeapCopyOfStackArray<float> heapQuadData(quadData);
    mGL->fBufferData(LOCAL_GL_ARRAY_BUFFER, heapQuadData.ByteLength(),
                     heapQuadData.Data(), LOCAL_GL_STATIC_DRAW);

    if (mGL->IsSupported(GLFeature::vertex_array_object)) {
      const auto prev = mGL->GetIntAs<GLuint>(LOCAL_GL_VERTEX_ARRAY_BINDING);

      mGL->fGenVertexArrays(1, &mQuadVAO);
      mGL->fBindVertexArray(mQuadVAO);
      mGL->fEnableVertexAttribArray(0);
      mGL->fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, false, 0, 0);

      mGL->fBindVertexArray(prev);
    }
  }

  // --

  const auto glslVersion = mGL->ShadingLanguageVersion();

  // Always use 100 on ES because some devices have OES_EGL_image_external but
  // not OES_EGL_image_external_essl3. We could just use 100 in that particular
  // case, but this is a lot easier and is not harmful to other usages.
  if (mGL->IsGLES()) {
    mDrawBlitProg_VersionLine = nsCString("#version 100\n");
  } else if (glslVersion >= 130) {
    mDrawBlitProg_VersionLine = nsPrintfCString("#version %u\n", glslVersion);
  }

  const char kVertSource[] =
      "\
        #if __VERSION__ >= 130                                               \n\
            #define ATTRIBUTE in                                             \n\
            #define VARYING out                                              \n\
        #else                                                                \n\
            #define ATTRIBUTE attribute                                      \n\
            #define VARYING varying                                          \n\
        #endif                                                               \n\
                                                                             \n\
        ATTRIBUTE vec2 aVert; // [0.0-1.0]                                   \n\
                                                                             \n\
        uniform mat3 uDestMatrix;                                            \n\
        uniform mat3 uTexMatrix0;                                            \n\
        uniform mat3 uTexMatrix1;                                            \n\
                                                                             \n\
        VARYING vec2 vTexCoord0;                                             \n\
        VARYING vec2 vTexCoord1;                                             \n\
                                                                             \n\
        void main(void)                                                      \n\
        {                                                                    \n\
            vec2 destPos = (uDestMatrix * vec3(aVert, 1.0)).xy;              \n\
            gl_Position = vec4(destPos * 2.0 - 1.0, 0.0, 1.0);               \n\
                                                                             \n\
            vTexCoord0 = (uTexMatrix0 * vec3(aVert, 1.0)).xy;                \n\
            vTexCoord1 = (uTexMatrix1 * vec3(aVert, 1.0)).xy;                \n\
        }                                                                    \n\
    ";
  const char* const parts[] = {mDrawBlitProg_VersionLine.get(), kVertSource};
  mGL->fShaderSource(mDrawBlitProg_VertShader, ArrayLength(parts), parts,
                     nullptr);
  mGL->fCompileShader(mDrawBlitProg_VertShader);
}

GLBlitHelper::~GLBlitHelper() {
  for (const auto& pair : mDrawBlitProgs) {
    const auto& ptr = pair.second;
    delete ptr;
  }
  mDrawBlitProgs.clear();

  if (!mGL->MakeCurrent()) return;

  mGL->fDeleteShader(mDrawBlitProg_VertShader);
  mGL->fDeleteBuffers(1, &mQuadVBO);

  if (mQuadVAO) {
    mGL->fDeleteVertexArrays(1, &mQuadVAO);
  }
}

// --

const DrawBlitProg* GLBlitHelper::GetDrawBlitProg(
    const DrawBlitProg::Key& key) const {
  const auto& res = mDrawBlitProgs.insert({key, nullptr});
  auto& pair = *(res.first);
  const auto& didInsert = res.second;
  if (didInsert) {
    pair.second = CreateDrawBlitProg(pair.first);
  }
  return pair.second;
}

const DrawBlitProg* GLBlitHelper::CreateDrawBlitProg(
    const DrawBlitProg::Key& key) const {
  const char kFragHeader_Global[] =
      "\
        #ifdef GL_ES                                                         \n\
            #ifdef GL_FRAGMENT_PRECISION_HIGH                                \n\
                precision highp float;                                       \n\
            #else                                                            \n\
                precision mediump float;                                     \n\
            #endif                                                           \n\
        #endif                                                               \n\
                                                                             \n\
        #if __VERSION__ >= 130                                               \n\
            #define VARYING in                                               \n\
            #define FRAG_COLOR oFragColor                                    \n\
            out vec4 FRAG_COLOR;                                             \n\
        #else                                                                \n\
            #define VARYING varying                                          \n\
            #define FRAG_COLOR gl_FragColor                                  \n\
        #endif                                                               \n\
                                                                             \n\
        #if __VERSION__ >= 120                                               \n\
            #define MAT4X3 mat4x3                                            \n\
        #else                                                                \n\
            #define MAT4X3 mat4                                              \n\
        #endif                                                               \n\
    ";

  const ScopedShader fs(mGL, LOCAL_GL_FRAGMENT_SHADER);
  const char* const parts[] = {mDrawBlitProg_VersionLine.get(), key.fragHeader,
                               kFragHeader_Global, key.fragBody};
  mGL->fShaderSource(fs, ArrayLength(parts), parts, nullptr);
  mGL->fCompileShader(fs);

  const auto prog = mGL->fCreateProgram();
  mGL->fAttachShader(prog, mDrawBlitProg_VertShader);
  mGL->fAttachShader(prog, fs);

  mGL->fBindAttribLocation(prog, 0, "aPosition");
  mGL->fLinkProgram(prog);

  GLenum status = 0;
  mGL->fGetProgramiv(prog, LOCAL_GL_LINK_STATUS, (GLint*)&status);
  if (status == LOCAL_GL_TRUE || mGL->CheckContextLost()) {
    const SaveRestoreCurrentProgram oldProg(mGL);
    mGL->fUseProgram(prog);
    const char* samplerNames[] = {"uTex0", "uTex1", "uTex2"};
    for (int i = 0; i < 3; i++) {
      const auto loc = mGL->fGetUniformLocation(prog, samplerNames[i]);
      if (loc == -1) continue;
      mGL->fUniform1i(loc, i);
    }

    return new DrawBlitProg(this, prog);
  }

  GLuint progLogLen = 0;
  mGL->fGetProgramiv(prog, LOCAL_GL_INFO_LOG_LENGTH, (GLint*)&progLogLen);
  const UniquePtr<char[]> progLog(new char[progLogLen + 1]);
  mGL->fGetProgramInfoLog(prog, progLogLen, nullptr, progLog.get());
  progLog[progLogLen] = 0;

  const auto& vs = mDrawBlitProg_VertShader;
  GLuint vsLogLen = 0;
  mGL->fGetShaderiv(vs, LOCAL_GL_INFO_LOG_LENGTH, (GLint*)&vsLogLen);
  const UniquePtr<char[]> vsLog(new char[vsLogLen + 1]);
  mGL->fGetShaderInfoLog(vs, vsLogLen, nullptr, vsLog.get());
  vsLog[vsLogLen] = 0;

  GLuint fsLogLen = 0;
  mGL->fGetShaderiv(fs, LOCAL_GL_INFO_LOG_LENGTH, (GLint*)&fsLogLen);
  const UniquePtr<char[]> fsLog(new char[fsLogLen + 1]);
  mGL->fGetShaderInfoLog(fs, fsLogLen, nullptr, fsLog.get());
  fsLog[fsLogLen] = 0;

  gfxCriticalError() << "DrawBlitProg link failed:\n"
                     << "progLog: " << progLog.get() << "\n"
                     << "vsLog: " << vsLog.get() << "\n"
                     << "fsLog: " << fsLog.get() << "\n";
  MOZ_CRASH();
}

// -----------------------------------------------------------------------------

#ifdef XP_MACOSX
static RefPtr<MacIOSurface> LookupSurface(
    const layers::SurfaceDescriptorMacIOSurface& sd) {
  return MacIOSurface::LookupSurface(sd.surfaceId(), !sd.isOpaque(),
                                     sd.yUVColorSpace());
}
#endif

bool GLBlitHelper::BlitSdToFramebuffer(const layers::SurfaceDescriptor& asd,
                                       const gfx::IntSize& destSize,
                                       const OriginPos destOrigin) {
  const auto sdType = asd.type();
  switch (sdType) {
    case layers::SurfaceDescriptor::TSurfaceDescriptorBuffer: {
      const auto& sd = asd.get_SurfaceDescriptorBuffer();
      const auto yuvData = PlanarYCbCrData::From(sd);
      if (!yuvData) {
        gfxCriticalNote << "[GLBlitHelper::BlitSdToFramebuffer] "
                           "PlanarYCbCrData::From failed";
        return false;
      }
      return BlitPlanarYCbCr(*yuvData, destSize, destOrigin);
    }
#ifdef XP_WIN
    case layers::SurfaceDescriptor::TSurfaceDescriptorD3D10: {
      const auto& sd = asd.get_SurfaceDescriptorD3D10();
      return BlitDescriptor(sd, destSize, destOrigin);
    }
    case layers::SurfaceDescriptor::TSurfaceDescriptorDXGIYCbCr: {
      const auto& sd = asd.get_SurfaceDescriptorDXGIYCbCr();
      return BlitDescriptor(sd, destSize, destOrigin);
    }
#endif
#ifdef XP_MACOSX
    case layers::SurfaceDescriptor::TSurfaceDescriptorMacIOSurface: {
      const auto& sd = asd.get_SurfaceDescriptorMacIOSurface();
      const auto surf = LookupSurface(sd);
      if (!surf) {
        NS_WARNING("LookupSurface(MacIOSurface) failed");
        // Sometimes that frame for our handle gone already. That's life, for
        // now.
        return false;
      }
      return BlitImage(surf, destSize, destOrigin);
    }
#endif
#ifdef MOZ_WIDGET_ANDROID
    case layers::SurfaceDescriptor::TSurfaceTextureDescriptor: {
      const auto& sd = asd.get_SurfaceTextureDescriptor();
      auto surfaceTexture = java::GeckoSurfaceTexture::Lookup(sd.handle());
      return Blit(surfaceTexture, destSize, destOrigin);
    }
#endif
    default:
      return false;
  }
}

bool GLBlitHelper::BlitImageToFramebuffer(layers::Image* const srcImage,
                                          const gfx::IntSize& destSize,
                                          const OriginPos destOrigin) {
  switch (srcImage->GetFormat()) {
    case ImageFormat::PLANAR_YCBCR: {
      const auto srcImage2 = static_cast<PlanarYCbCrImage*>(srcImage);
      const auto data = srcImage2->GetData();
      return BlitPlanarYCbCr(*data, destSize, destOrigin);
    }

    case ImageFormat::SURFACE_TEXTURE: {
#ifdef MOZ_WIDGET_ANDROID
      auto* image = srcImage->AsSurfaceTextureImage();
      MOZ_ASSERT(image);
      auto surfaceTexture =
          java::GeckoSurfaceTexture::Lookup(image->GetHandle());
      return Blit(surfaceTexture, destSize, destOrigin);
#else
      MOZ_ASSERT(false);
      return false;
#endif
    }
    case ImageFormat::MAC_IOSURFACE:
#ifdef XP_MACOSX
      return BlitImage(srcImage->AsMacIOSurfaceImage(), destSize, destOrigin);
#else
      MOZ_ASSERT(false);
      return false;
#endif

    case ImageFormat::GPU_VIDEO:
      return BlitImage(static_cast<layers::GPUVideoImage*>(srcImage), destSize,
                       destOrigin);
#ifdef XP_WIN
    case ImageFormat::D3D11_SHARE_HANDLE_TEXTURE:
      return BlitImage(static_cast<layers::D3D11ShareHandleImage*>(srcImage),
                       destSize, destOrigin);
    case ImageFormat::D3D11_TEXTURE_IMF_SAMPLE:
      return BlitImage(
          static_cast<layers::D3D11TextureIMFSampleImage*>(srcImage), destSize,
          destOrigin);
    case ImageFormat::D3D11_YCBCR_IMAGE:
      return BlitImage(static_cast<layers::D3D11YCbCrImage*>(srcImage),
                       destSize, destOrigin);
    case ImageFormat::D3D9_RGB32_TEXTURE:
      return false;  // todo
#else
    case ImageFormat::D3D11_SHARE_HANDLE_TEXTURE:
    case ImageFormat::D3D11_TEXTURE_IMF_SAMPLE:
    case ImageFormat::D3D11_YCBCR_IMAGE:
    case ImageFormat::D3D9_RGB32_TEXTURE:
      MOZ_ASSERT(false);
      return false;
#endif
    case ImageFormat::DMABUF:
#ifdef MOZ_WAYLAND
      return BlitImage(static_cast<layers::DMABUFSurfaceImage*>(srcImage),
                       destSize, destOrigin);
#else
      return false;
#endif
    case ImageFormat::CAIRO_SURFACE:
    case ImageFormat::NV_IMAGE:
    case ImageFormat::OVERLAY_IMAGE:
    case ImageFormat::SHARED_RGB:
    case ImageFormat::TEXTURE_WRAPPER:
      return false;  // todo
  }
  return false;
}

// -------------------------------------

#ifdef MOZ_WIDGET_ANDROID
bool GLBlitHelper::Blit(const java::GeckoSurfaceTexture::Ref& surfaceTexture,
                        const gfx::IntSize& destSize,
                        const OriginPos destOrigin) const {
  if (!surfaceTexture) {
    return false;
  }

  const ScopedBindTextureUnit boundTU(mGL, LOCAL_GL_TEXTURE0);

  if (!surfaceTexture->IsAttachedToGLContext((int64_t)mGL)) {
    GLuint tex;
    mGL->MakeCurrent();
    mGL->fGenTextures(1, &tex);

    if (NS_FAILED(surfaceTexture->AttachToGLContext((int64_t)mGL, tex))) {
      mGL->fDeleteTextures(1, &tex);
      return false;
    }
  }

  const ScopedBindTexture savedTex(mGL, surfaceTexture->GetTexName(),
                                   LOCAL_GL_TEXTURE_EXTERNAL);
  surfaceTexture->UpdateTexImage();
  const auto transform3 = Mat3::I();
  const auto srcOrigin = OriginPos::TopLeft;
  const bool yFlip = (srcOrigin != destOrigin);
  const auto& prog = GetDrawBlitProg({kFragHeader_TexExt, kFragBody_RGBA});
  const DrawBlitProg::BaseArgs baseArgs = {transform3, yFlip, destSize,
                                           Nothing()};
  prog->Draw(baseArgs, nullptr);

  if (surfaceTexture->IsSingleBuffer()) {
    surfaceTexture->ReleaseTexImage();
  }

  return true;
}
#endif

// -------------------------------------

bool GuessDivisors(const gfx::IntSize& ySize, const gfx::IntSize& uvSize,
                   gfx::IntSize* const out_divisors) {
  const gfx::IntSize divisors((ySize.width == uvSize.width) ? 1 : 2,
                              (ySize.height == uvSize.height) ? 1 : 2);
  if (uvSize.width * divisors.width != ySize.width ||
      uvSize.height * divisors.height != ySize.height) {
    return false;
  }
  *out_divisors = divisors;
  return true;
}

bool GLBlitHelper::BlitPlanarYCbCr(const PlanarYCbCrData& yuvData,
                                   const gfx::IntSize& destSize,
                                   const OriginPos destOrigin) {
  const auto& prog = GetDrawBlitProg({kFragHeader_Tex2D, kFragBody_PlanarYUV});

  if (!mYuvUploads[0]) {
    mGL->fGenTextures(3, mYuvUploads);
    const ScopedBindTexture bindTex(mGL, mYuvUploads[0]);
    mGL->TexParams_SetClampNoMips();
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mYuvUploads[1]);
    mGL->TexParams_SetClampNoMips();
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mYuvUploads[2]);
    mGL->TexParams_SetClampNoMips();
  }

  // --

  auto ySize = yuvData.YDataSize();
  auto cbcrSize = yuvData.CbCrDataSize();
  if (yuvData.mYSkip || yuvData.mCbSkip || yuvData.mCrSkip || ySize.width < 0 ||
      ySize.height < 0 || cbcrSize.width < 0 || cbcrSize.height < 0 ||
      yuvData.mYStride < 0 || yuvData.mCbCrStride < 0) {
    gfxCriticalError() << "Unusual PlanarYCbCrData: " << yuvData.mYSkip << ","
                       << yuvData.mCbSkip << "," << yuvData.mCrSkip << ", "
                       << ySize.width << "," << ySize.height << ", "
                       << cbcrSize.width << "," << cbcrSize.height << ", "
                       << yuvData.mYStride << "," << yuvData.mCbCrStride;
    return false;
  }

  gfx::IntSize divisors;
  switch (yuvData.mChromaSubsampling) {
    case gfx::ChromaSubsampling::FULL:
      divisors = gfx::IntSize(1, 1);
      break;
    case gfx::ChromaSubsampling::HALF_WIDTH:
      divisors = gfx::IntSize(2, 1);
      break;
    case gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT:
      divisors = gfx::IntSize(2, 2);
      break;
    default:
      gfxCriticalError() << "Unknown chroma subsampling:"
                         << int(yuvData.mChromaSubsampling);
      return false;
  }

  // --

  // RED textures aren't valid in GLES2, and ALPHA textures are not valid in
  // desktop GL Core Profiles. So use R8 textures on GL3.0+ and GLES3.0+, but
  // LUMINANCE/LUMINANCE/UNSIGNED_BYTE otherwise.
  GLenum internalFormat;
  GLenum unpackFormat;
  if (mGL->IsAtLeast(gl::ContextProfile::OpenGLCore, 300) ||
      mGL->IsAtLeast(gl::ContextProfile::OpenGLES, 300)) {
    internalFormat = LOCAL_GL_R8;
    unpackFormat = LOCAL_GL_RED;
  } else {
    internalFormat = LOCAL_GL_LUMINANCE;
    unpackFormat = LOCAL_GL_LUMINANCE;
  }

  // --

  const ScopedSaveMultiTex saveTex(mGL, 3, LOCAL_GL_TEXTURE_2D);
  const ResetUnpackState reset(mGL);
  const gfx::IntSize yTexSize(yuvData.mYStride, yuvData.YDataSize().height);
  const gfx::IntSize uvTexSize(yuvData.mCbCrStride,
                               yuvData.CbCrDataSize().height);

  if (yTexSize != mYuvUploads_YSize || uvTexSize != mYuvUploads_UVSize) {
    mYuvUploads_YSize = yTexSize;
    mYuvUploads_UVSize = uvTexSize;

    mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mYuvUploads[0]);
    mGL->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, internalFormat, yTexSize.width,
                     yTexSize.height, 0, unpackFormat, LOCAL_GL_UNSIGNED_BYTE,
                     nullptr);
    for (int i = 1; i < 3; i++) {
      mGL->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
      mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mYuvUploads[i]);
      mGL->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, internalFormat, uvTexSize.width,
                       uvTexSize.height, 0, unpackFormat,
                       LOCAL_GL_UNSIGNED_BYTE, nullptr);
    }
  }

  // --

  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mYuvUploads[0]);
  mGL->fTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0, 0, 0, yTexSize.width,
                      yTexSize.height, unpackFormat, LOCAL_GL_UNSIGNED_BYTE,
                      yuvData.mYChannel);
  mGL->fActiveTexture(LOCAL_GL_TEXTURE1);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mYuvUploads[1]);
  mGL->fTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0, 0, 0, uvTexSize.width,
                      uvTexSize.height, unpackFormat, LOCAL_GL_UNSIGNED_BYTE,
                      yuvData.mCbChannel);
  mGL->fActiveTexture(LOCAL_GL_TEXTURE2);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mYuvUploads[2]);
  mGL->fTexSubImage2D(LOCAL_GL_TEXTURE_2D, 0, 0, 0, uvTexSize.width,
                      uvTexSize.height, unpackFormat, LOCAL_GL_UNSIGNED_BYTE,
                      yuvData.mCrChannel);

  // --

  const auto& clipRect = yuvData.mPictureRect;
  const auto srcOrigin = OriginPos::BottomLeft;
  const bool yFlip = (destOrigin != srcOrigin);

  const DrawBlitProg::BaseArgs baseArgs = {SubRectMat3(clipRect, yTexSize),
                                           yFlip, destSize, Nothing()};
  const DrawBlitProg::YUVArgs yuvArgs = {
      SubRectMat3(clipRect, uvTexSize, divisors), yuvData.mYUVColorSpace};
  prog->Draw(baseArgs, &yuvArgs);
  return true;
}

// -------------------------------------

#ifdef XP_MACOSX
bool GLBlitHelper::BlitImage(layers::MacIOSurfaceImage* const srcImage,
                             const gfx::IntSize& destSize,
                             const OriginPos destOrigin) const {
  return BlitImage(srcImage->GetSurface(), destSize, destOrigin);
}

static std::string IntAsAscii(const int x) {
  std::string str;
  str.reserve(6);
  auto u = static_cast<unsigned int>(x);
  while (u) {
    str.insert(str.begin(), u & 0xff);
    u >>= 8;
  }
  str.insert(str.begin(), '\'');
  str.push_back('\'');
  return str;
}

bool GLBlitHelper::BlitImage(MacIOSurface* const iosurf,
                             const gfx::IntSize& destSize,
                             const OriginPos destOrigin) const {
  if (!iosurf) {
    gfxCriticalError() << "Null MacIOSurface for GLBlitHelper::BlitImage";
    return false;
  }
  if (mGL->GetContextType() != GLContextType::CGL) {
    MOZ_ASSERT(false);
    return false;
  }
  const auto glCGL = static_cast<GLContextCGL*>(mGL);
  const auto cglContext = glCGL->GetCGLContext();

  const auto& srcOrigin = OriginPos::BottomLeft;

  DrawBlitProg::BaseArgs baseArgs;
  baseArgs.yFlip = (destOrigin != srcOrigin);
  baseArgs.destSize = destSize;

  // TODO: The colorspace is known by the IOSurface, why override it?
  // See GetYUVColorSpace/GetFullRange()
  DrawBlitProg::YUVArgs yuvArgs;
  yuvArgs.colorSpace = iosurf->GetYUVColorSpace();

  const DrawBlitProg::YUVArgs* pYuvArgs = nullptr;

  auto planes = iosurf->GetPlaneCount();
  if (!planes) {
    planes = 1;  // Bad API. No cookie.
  }

  const GLenum texTarget = LOCAL_GL_TEXTURE_RECTANGLE;
  const char* const fragHeader = kFragHeader_Tex2DRect;

  const ScopedSaveMultiTex saveTex(mGL, planes, texTarget);
  const ScopedTexture tex0(mGL);
  const ScopedTexture tex1(mGL);
  const ScopedTexture tex2(mGL);
  const GLuint texs[3] = {tex0, tex1, tex2};

  const auto pixelFormat = iosurf->GetPixelFormat();
  if (mGL->ShouldSpew()) {
    const auto formatStr = IntAsAscii(pixelFormat);
    printf_stderr("iosurf format: %s (0x%08x)\n", formatStr.c_str(),
                  pixelFormat);
  }

  const char* fragBody;
  switch (planes) {
    case 1:
      switch (pixelFormat) {
        case kCVPixelFormatType_24RGB:
        case kCVPixelFormatType_24BGR:
        case kCVPixelFormatType_32ARGB:
        case kCVPixelFormatType_32BGRA:
        case kCVPixelFormatType_32ABGR:
        case kCVPixelFormatType_32RGBA:
        case kCVPixelFormatType_64ARGB:
        case kCVPixelFormatType_48RGB:
          fragBody = kFragBody_RGBA;
          break;
        case kCVPixelFormatType_422YpCbCr8:
        case kCVPixelFormatType_422YpCbCr8_yuvs:
          fragBody = kFragBody_CrYCb;
          pYuvArgs = &yuvArgs;
          break;
        default: {
          std::string str;
          if (pixelFormat <= 0xff) {
            str = std::to_string(pixelFormat);
          } else {
            str = IntAsAscii(pixelFormat);
          }
          gfxCriticalError() << "Unhandled kCVPixelFormatType_*: " << str;
        }
          // Probably YUV though
          fragBody = kFragBody_CrYCb;
          pYuvArgs = &yuvArgs;
          break;
      }
      break;
    case 2:
      fragBody = kFragBody_NV12;
      pYuvArgs = &yuvArgs;
      break;
    case 3:
      fragBody = kFragBody_PlanarYUV;
      pYuvArgs = &yuvArgs;
      break;
    default:
      gfxCriticalError() << "Unexpected plane count: " << planes;
      return false;
  }

  for (uint32_t p = 0; p < planes; p++) {
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0 + p);
    mGL->fBindTexture(texTarget, texs[p]);
    mGL->TexParams_SetClampNoMips(texTarget);

    auto err = iosurf->CGLTexImageIOSurface2D(mGL, cglContext, p);
    if (err) {
      return false;
    }

    if (p == 0) {
      const auto width = iosurf->GetDevicePixelWidth(p);
      const auto height = iosurf->GetDevicePixelHeight(p);
      baseArgs.texMatrix0 = SubRectMat3(0, 0, width, height);
      yuvArgs.texMatrix1 = SubRectMat3(0, 0, width / 2.0, height / 2.0);
    }
  }

  const auto& prog = GetDrawBlitProg({fragHeader, fragBody});
  prog->Draw(baseArgs, pYuvArgs);
  return true;
}
#endif

// -----------------------------------------------------------------------------

void GLBlitHelper::DrawBlitTextureToFramebuffer(const GLuint srcTex,
                                                const gfx::IntSize& srcSize,
                                                const gfx::IntSize& destSize,
                                                const GLenum srcTarget,
                                                const bool srcIsBGRA) const {
  const char* fragHeader = nullptr;
  Mat3 texMatrix0;
  switch (srcTarget) {
    case LOCAL_GL_TEXTURE_2D:
      fragHeader = kFragHeader_Tex2D;
      texMatrix0 = Mat3::I();
      break;
    case LOCAL_GL_TEXTURE_RECTANGLE_ARB:
      fragHeader = kFragHeader_Tex2DRect;
      texMatrix0 = SubRectMat3(0, 0, srcSize.width, srcSize.height);
      break;
    default:
      gfxCriticalError() << "Unexpected srcTarget: " << srcTarget;
      return;
  }
  const char* fragBody = srcIsBGRA ? kFragBody_BGRA : kFragBody_RGBA;
  const auto& prog = GetDrawBlitProg({fragHeader, fragBody});

  const ScopedSaveMultiTex saveTex(mGL, 1, srcTarget);
  mGL->fBindTexture(srcTarget, srcTex);

  const bool yFlip = false;
  const DrawBlitProg::BaseArgs baseArgs = {texMatrix0, yFlip, destSize,
                                           Nothing()};
  prog->Draw(baseArgs);
}

// -----------------------------------------------------------------------------

void GLBlitHelper::BlitFramebuffer(const gfx::IntRect& srcRect,
                                   const gfx::IntRect& destRect,
                                   GLuint filter) const {
  MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));

  const ScopedGLState scissor(mGL, LOCAL_GL_SCISSOR_TEST, false);
  mGL->fBlitFramebuffer(srcRect.x, srcRect.y, srcRect.XMost(), srcRect.YMost(),
                        destRect.x, destRect.y, destRect.XMost(),
                        destRect.YMost(), LOCAL_GL_COLOR_BUFFER_BIT, filter);
}

// --

void GLBlitHelper::BlitFramebufferToFramebuffer(const GLuint srcFB,
                                                const GLuint destFB,
                                                const gfx::IntRect& srcRect,
                                                const gfx::IntRect& destRect,
                                                GLuint filter) const {
  MOZ_ASSERT(mGL->IsSupported(GLFeature::framebuffer_blit));
  MOZ_GL_ASSERT(mGL, !srcFB || mGL->fIsFramebuffer(srcFB));
  MOZ_GL_ASSERT(mGL, !destFB || mGL->fIsFramebuffer(destFB));

  const ScopedBindFramebuffer boundFB(mGL);
  mGL->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, srcFB);
  mGL->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, destFB);

  BlitFramebuffer(srcRect, destRect, filter);
}

void GLBlitHelper::BlitTextureToFramebuffer(GLuint srcTex,
                                            const gfx::IntSize& srcSize,
                                            const gfx::IntSize& destSize,
                                            GLenum srcTarget) const {
  MOZ_GL_ASSERT(mGL, mGL->fIsTexture(srcTex));

  if (mGL->IsSupported(GLFeature::framebuffer_blit)) {
    const ScopedFramebufferForTexture srcWrapper(mGL, srcTex, srcTarget);
    const ScopedBindFramebuffer bindFB(mGL);
    mGL->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, srcWrapper.FB());
    BlitFramebuffer(gfx::IntRect({}, srcSize), gfx::IntRect({}, destSize));
    return;
  }

  DrawBlitTextureToFramebuffer(srcTex, srcSize, destSize, srcTarget);
}

void GLBlitHelper::BlitFramebufferToTexture(GLuint destTex,
                                            const gfx::IntSize& srcSize,
                                            const gfx::IntSize& destSize,
                                            GLenum destTarget) const {
  MOZ_GL_ASSERT(mGL, mGL->fIsTexture(destTex));

  if (mGL->IsSupported(GLFeature::framebuffer_blit)) {
    const ScopedFramebufferForTexture destWrapper(mGL, destTex, destTarget);
    const ScopedBindFramebuffer bindFB(mGL);
    mGL->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, destWrapper.FB());
    BlitFramebuffer(gfx::IntRect({}, srcSize), gfx::IntRect({}, destSize));
    return;
  }

  ScopedBindTexture autoTex(mGL, destTex, destTarget);
  ScopedGLState scissor(mGL, LOCAL_GL_SCISSOR_TEST, false);
  mGL->fCopyTexSubImage2D(destTarget, 0, 0, 0, 0, 0, srcSize.width,
                          srcSize.height);
}

void GLBlitHelper::BlitTextureToTexture(GLuint srcTex, GLuint destTex,
                                        const gfx::IntSize& srcSize,
                                        const gfx::IntSize& destSize,
                                        GLenum srcTarget,
                                        GLenum destTarget) const {
  MOZ_GL_ASSERT(mGL, mGL->fIsTexture(srcTex));
  MOZ_GL_ASSERT(mGL, mGL->fIsTexture(destTex));

  // Start down the CopyTexSubImage path, not the DrawBlit path.
  const ScopedFramebufferForTexture srcWrapper(mGL, srcTex, srcTarget);
  const ScopedBindFramebuffer bindFB(mGL, srcWrapper.FB());
  BlitFramebufferToTexture(destTex, srcSize, destSize, destTarget);
}

// -------------------------------------

bool GLBlitHelper::BlitImage(layers::GPUVideoImage* const srcImage,
                             const gfx::IntSize& destSize,
                             const OriginPos destOrigin) const {
  const auto& data = srcImage->GetData();
  if (!data) return false;

  const auto& desc = data->SD();

  MOZ_ASSERT(
      desc.type() ==
      layers::SurfaceDescriptorGPUVideo::TSurfaceDescriptorRemoteDecoder);
  const auto& subdescUnion =
      desc.get_SurfaceDescriptorRemoteDecoder().subdesc();
  switch (subdescUnion.type()) {
    case layers::RemoteDecoderVideoSubDescriptor::TSurfaceDescriptorDMABuf: {
      // TODO.
      return false;
    }
#ifdef XP_WIN
    case layers::RemoteDecoderVideoSubDescriptor::TSurfaceDescriptorD3D10: {
      const auto& subdesc = subdescUnion.get_SurfaceDescriptorD3D10();
      return BlitDescriptor(subdesc, destSize, destOrigin);
    }
    case layers::RemoteDecoderVideoSubDescriptor::TSurfaceDescriptorDXGIYCbCr: {
      const auto& subdesc = subdescUnion.get_SurfaceDescriptorDXGIYCbCr();
      return BlitDescriptor(subdesc, destSize, destOrigin);
    }
#endif
#ifdef XP_MACOSX
    case layers::RemoteDecoderVideoSubDescriptor::
        TSurfaceDescriptorMacIOSurface: {
      const auto& subdesc = subdescUnion.get_SurfaceDescriptorMacIOSurface();
      RefPtr<MacIOSurface> surface = MacIOSurface::LookupSurface(
          subdesc.surfaceId(), !subdesc.isOpaque(), subdesc.yUVColorSpace());
      MOZ_ASSERT(surface);
      if (!surface) {
        return false;
      }
      return BlitImage(surface, destSize, destOrigin);
    }
#endif
    case layers::RemoteDecoderVideoSubDescriptor::Tnull_t:
      // This GPUVideoImage isn't directly readable outside the GPU process.
      // Abort.
      return false;
    default:
      gfxCriticalError() << "Unhandled subdesc type: "
                         << uint32_t(subdescUnion.type());
      return false;
  }
}

// -------------------------------------
#ifdef MOZ_WAYLAND
bool GLBlitHelper::Blit(DMABufSurface* surface, const gfx::IntSize& destSize,
                        OriginPos destOrigin) const {
  const auto& srcOrigin = OriginPos::BottomLeft;

  DrawBlitProg::BaseArgs baseArgs;
  baseArgs.yFlip = (destOrigin != srcOrigin);
  baseArgs.destSize = destSize;

  // TODO: The colorspace is known by the DMABUFSurface, why override it?
  // See GetYUVColorSpace/GetFullRange()
  DrawBlitProg::YUVArgs yuvArgs;
  yuvArgs.colorSpace = surface->GetYUVColorSpace();

  const DrawBlitProg::YUVArgs* pYuvArgs = nullptr;

  const auto planes = surface->GetTextureCount();
  const GLenum texTarget = LOCAL_GL_TEXTURE_2D;

  const ScopedSaveMultiTex saveTex(mGL, planes, texTarget);
  const auto pixelFormat = surface->GetSurfaceType();

  const char* fragBody;
  switch (pixelFormat) {
    case DMABufSurface::SURFACE_RGBA:
      fragBody = kFragBody_RGBA;
      break;
    case DMABufSurface::SURFACE_NV12:
      fragBody = kFragBody_NV12;
      pYuvArgs = &yuvArgs;
      break;
    case DMABufSurface::SURFACE_YUV420:
      fragBody = kFragBody_PlanarYUV;
      pYuvArgs = &yuvArgs;
      break;
    default:
      gfxCriticalError() << "Unexpected pixel format: " << pixelFormat;
      return false;
  }

  for (const auto p : IntegerRange(planes)) {
    mGL->fActiveTexture(LOCAL_GL_TEXTURE0 + p);
    mGL->fBindTexture(texTarget, surface->GetTexture(p));
    mGL->TexParams_SetClampNoMips(texTarget);
  }

  // We support only NV12/YUV420 formats only with 1/2 texture scale.
  // We don't set cliprect as DMABus textures are created without padding.
  baseArgs.texMatrix0 = SubRectMat3(0, 0, 1, 1);
  yuvArgs.texMatrix1 = SubRectMat3(0, 0, 1, 1);

  const auto& prog = GetDrawBlitProg({kFragHeader_Tex2D, fragBody});
  prog->Draw(baseArgs, pYuvArgs);

  return true;
}

bool GLBlitHelper::BlitImage(layers::DMABUFSurfaceImage* srcImage,
                             const gfx::IntSize& destSize,
                             OriginPos destOrigin) const {
  DMABufSurface* surface = srcImage->GetSurface();
  if (!surface) {
    gfxCriticalError() << "Null DMABUFSurface for GLBlitHelper::BlitImage";
    return false;
  }
  return Blit(surface, destSize, destOrigin);
}
#endif

}  // namespace gl
}  // namespace mozilla
