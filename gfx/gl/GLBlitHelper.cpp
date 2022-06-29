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
#include "mozilla/StaticPrefs_gfx.h"
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

static const char kFragPreprocHeader[] = R"(
  #ifdef GL_ES
    #ifdef GL_FRAGMENT_PRECISION_HIGH
      #define MAXP highp
    #endif
  #else
    #define MAXP highp
  #endif
  #ifndef MAXP
    #define MAXP mediump
  #endif

  #if __VERSION__ >= 130
    #define VARYING in
  #else
    #define VARYING varying
  #endif
  #if __VERSION__ >= 120
    #define MAT4X3 mat4x3
  #else
    #define MAT4X3 mat4
  #endif
)";

// -

const char* const kFragHeader_Tex2D = R"(
    #define SAMPLER sampler2D
    #if __VERSION__ >= 130
        #define TEXTURE texture
    #else
        #define TEXTURE texture2D
    #endif
)";
const char* const kFragHeader_Tex2DRect = R"(
    #define SAMPLER sampler2DRect
    #if __VERSION__ >= 130
        #define TEXTURE texture
    #else
        #define TEXTURE texture2DRect
    #endif
)";
const char* const kFragHeader_TexExt = R"(
    #extension GL_OES_EGL_image_external : enable
    #extension GL_OES_EGL_image_external_essl3 : enable
    #if __VERSION__ >= 130
        #define TEXTURE texture
    #else
        #define TEXTURE texture2D
    #endif
    #define SAMPLER samplerExternalOES
)";

// -

static const char kFragDeclHeader[] = R"(
  precision PRECISION float;
  #if __VERSION__ >= 130
    #define FRAG_COLOR oFragColor
    out vec4 FRAG_COLOR;
  #else
    #define FRAG_COLOR gl_FragColor
  #endif
)";

// -

const char* const kFragSample_OnePlane = R"(
  VARYING mediump vec2 vTexCoord0;
  uniform PRECISION SAMPLER uTex0;

  vec4 metaSample() {
    vec4 src = TEXTURE(uTex0, vTexCoord0);
    return src;
  }
)";
// Ideally this would just change the color-matrix it uses, but this is
// acceptable debt for now.
const char* const kFragSample_OnePlane_YUV_via_GBR = R"(
  VARYING mediump vec2 vTexCoord0;
  uniform PRECISION SAMPLER uTex0;

  vec4 metaSample() {
    vec4 src = TEXTURE(uTex0, vTexCoord0);
    return src;
  }
)";
const char* const kFragSample_TwoPlane = R"(
  VARYING mediump vec2 vTexCoord0;
  VARYING mediump vec2 vTexCoord1;
  uniform PRECISION SAMPLER uTex0;
  uniform PRECISION SAMPLER uTex1;

  vec4 metaSample() {
    vec4 src = TEXTURE(uTex0, vTexCoord0); // Keep r and a.
    src.gb = TEXTURE(uTex1, vTexCoord1).rg;
    return src;
  }
)";
const char* const kFragSample_ThreePlane = R"(
  VARYING mediump vec2 vTexCoord0;
  VARYING mediump vec2 vTexCoord1;
  uniform PRECISION SAMPLER uTex0;
  uniform PRECISION SAMPLER uTex1;
  uniform PRECISION SAMPLER uTex2;

  vec4 metaSample() {
    vec4 src = TEXTURE(uTex0, vTexCoord0); // Keep r and a.
    src.g = TEXTURE(uTex1, vTexCoord1).r;
    src.b = TEXTURE(uTex2, vTexCoord1).r;
    return src;
  }
)";

// -

const char* const kFragConvert_None = R"(
  vec3 metaConvert(vec3 src) {
    return src;
  }
)";
const char* const kFragConvert_BGR = R"(
  vec3 metaConvert(vec3 src) {
    return src.bgr;
  }
)";
const char* const kFragConvert_ColorMatrix = R"(
  uniform mediump MAT4X3 uColorMatrix;

  vec3 metaConvert(vec3 src) {
    return (uColorMatrix * vec4(src, 1)).rgb;
  }
)";
const char* const kFragConvert_ColorLut = R"(
  uniform PRECISION sampler3D uColorLut;

  vec3 metaConvert(vec3 src) {
    // Half-texel filtering hazard!
    // E.g. For texture size of 2,
    // E.g. 0.5/2=0.25 is still sampling 100% of texel 0, 0% of texel 1.
    // For the LUT, we need 0.5/2=0.25 to filter 25/75 texel 0 and 1.
    // That is, we need to adjust our sampling point such that it's 0.25 of the
    // way from texel 0's center to texel 1's center.
    // We need, for N=2:
    // v=0.0|N=2 => v'=0.5/2
    // v=1.0|N=2 => v'=1.5/2
    // For N=3:
    // v=0.0|N=3 => v'=0.5/3
    // v=1.0|N=3 => v'=2.5/3
    // => v' = ( 0.5 + v * (3 - 1) )/3
    vec3 size = vec3(textureSize(uColorLut, 0));
    src = (0.5 + src * (size - 1.0)) / size;
    return texture(uColorLut, src).rgb;
  }
)";

// -

static const char kFragBody[] = R"(
  void main(void) {
    vec4 src = metaSample();
    vec3 dst = metaConvert(src.rgb);
    FRAG_COLOR = vec4(dst, src.a);
  }
)";

// --

Mat3 SubRectMat3(const float x, const float y, const float w, const float h) {
  auto ret = Mat3{};
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
                                       const std::vector<uint8_t>& texUnits,
                                       const GLenum texTarget)
    : mGL(*gl),
      mTexUnits(texUnits),
      mTexTarget(texTarget),
      mOldTexUnit(mGL.GetIntAs<GLenum>(LOCAL_GL_ACTIVE_TEXTURE)) {
  GLenum texBinding;
  switch (mTexTarget) {
    case LOCAL_GL_TEXTURE_2D:
      texBinding = LOCAL_GL_TEXTURE_BINDING_2D;
      break;
    case LOCAL_GL_TEXTURE_3D:
      texBinding = LOCAL_GL_TEXTURE_BINDING_3D;
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

  for (int i = 0; i < int(mTexUnits.size()); i++) {
    const auto& unit = mTexUnits[i];
    mGL.fActiveTexture(LOCAL_GL_TEXTURE0 + unit);
    if (mGL.IsSupported(GLFeature::sampler_objects)) {
      mOldTexSampler[i] = mGL.GetIntAs<GLuint>(LOCAL_GL_SAMPLER_BINDING);
      mGL.fBindSampler(unit, 0);
    }
    mOldTex[i] = mGL.GetIntAs<GLuint>(texBinding);
  }
}

ScopedSaveMultiTex::~ScopedSaveMultiTex() {
  for (int i = mTexUnits.size() - 1; i >= 0; i--) {  // reverse
    const auto& unit = mTexUnits[i];
    mGL.fActiveTexture(LOCAL_GL_TEXTURE0 + unit);
    if (mGL.IsSupported(GLFeature::sampler_objects)) {
      mGL.fBindSampler(unit, mOldTexSampler[i]);
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
      mLoc_uColorLut(mParent.mGL->fGetUniformLocation(mProg, "uColorLut")),
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

  if (args.texUnitForColorLut) {
    gl->fUniform1i(mLoc_uColorLut, *args.texUnitForColorLut);
  }

  MOZ_ASSERT(bool(argsYUV) == (mLoc_uColorMatrix != -1));
  if (argsYUV) {
    gl->fUniformMatrix3fv(mLoc_uTexMatrix1, 1, false, argsYUV->texMatrix1.m);

    if (mLoc_uColorMatrix != -1) {
      const auto& colorMatrix =
          gfxUtils::YuvToRgbMatrix4x4ColumnMajor(*argsYUV->colorSpaceForMatrix);
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
          gfxCriticalError()
              << "Bad mType_uColorMatrix: " << gfx::hexa(mType_uColorMatrix);
      }
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

  if (mGL->IsGLES()) {
    // If you run into problems on old android devices, it might be because some devices have OES_EGL_image_external but not OES_EGL_image_external_essl3.
    // We could just use 100 in that particular case, but then we lose out on e.g. sampler3D.
    // Let's just try 300 for now, and if we get regressions we'll add an essl100 fallback.
    if (glslVersion >= 300) {
      mDrawBlitProg_VersionLine = nsCString("#version 300 es\n");
    } else {
      mDrawBlitProg_VersionLine = nsCString("#version 100\n");
    }
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
  const auto precisionPref = StaticPrefs::gfx_blithelper_precision();
  const char* precision;
  switch (precisionPref) {
    case 0:
      precision = "lowp";
      break;
    case 1:
      precision = "mediump";
      break;
    default:
      if (precisionPref != 2) {
        NS_WARNING("gfx.blithelper.precision clamped to 2.");
      }
      precision = "MAXP";
      break;
  }

  nsPrintfCString precisionLine("\n#define PRECISION %s\n", precision);

  // -

  const ScopedShader fs(mGL, LOCAL_GL_FRAGMENT_SHADER);

  std::vector<const char*> parts;
  {
    parts.push_back(mDrawBlitProg_VersionLine.get());
    parts.push_back(kFragPreprocHeader);
    if (key.fragHeader) {
      parts.push_back(key.fragHeader);
    }
    parts.push_back(precisionLine.BeginReading());
    parts.push_back(kFragDeclHeader);
    for (const auto& part : key.fragParts) {
      if (part) {
        parts.push_back(part);
      }
    }
    parts.push_back(kFragBody);
  }
  mGL->fShaderSource(fs, parts.size(), parts.data(), nullptr);
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

  const auto logs =
      std::string("DrawBlitProg link failed:\n") + "progLog: " + progLog.get() +
      "\n" + "vsLog: " + vsLog.get() + "\n" + "fsLog: " + fsLog.get() + "\n";
  gfxCriticalError() << logs;

  printf_stderr("Frag source:\n");
  int i = 0;
  for (const auto& part : parts) {
    printf_stderr("// parts[%i]:\n%s\n", i, part);
    i += 1;
  }

  MOZ_CRASH("DrawBlitProg link failed");
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
  const auto& prog = GetDrawBlitProg(
      {kFragHeader_TexExt, kFragSample_OnePlane, kFragConvert_None});
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
  const auto& prog = GetDrawBlitProg(
      {kFragHeader_Tex2D, {kFragSample_ThreePlane, kFragConvert_ColorMatrix}});

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

  const ScopedSaveMultiTex saveTex(mGL, {0, 1, 2}, LOCAL_GL_TEXTURE_2D);
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
      SubRectMat3(clipRect, uvTexSize, divisors), Some(yuvData.mYUVColorSpace)};
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
  yuvArgs.colorSpaceForMatrix = Some(iosurf->GetYUVColorSpace());

  const DrawBlitProg::YUVArgs* pYuvArgs = nullptr;

  auto planes = iosurf->GetPlaneCount();
  if (!planes) {
    planes = 1;  // Bad API. No cookie.
  }

  const GLenum texTarget = LOCAL_GL_TEXTURE_RECTANGLE;

  std::vector<uint8_t> texUnits;
  for (uint8_t i = 0; i < planes; i++) {
    texUnits.push_back(i);
  }
  const ScopedSaveMultiTex saveTex(mGL, texUnits, texTarget);
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

  const char* fragSample;
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
          fragSample = kFragSample_OnePlane;
          break;
        case kCVPixelFormatType_422YpCbCr8:
        case kCVPixelFormatType_422YpCbCr8_yuvs:
          fragSample = kFragSample_OnePlane_YUV_via_GBR;
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
          // Probably YUV though
          fragSample = kFragSample_OnePlane_YUV_via_GBR;
          pYuvArgs = &yuvArgs;
          break;
        }
      }
      break;
    case 2:
      fragSample = kFragSample_TwoPlane;
      pYuvArgs = &yuvArgs;
      break;
    case 3:
      fragSample = kFragSample_ThreePlane;
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

  const auto& prog = GetDrawBlitProg({
      kFragHeader_Tex2DRect,
      {fragSample,
      kFragConvert_ColorMatrix},
  });
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
  const auto fragConvert = srcIsBGRA ? kFragConvert_BGR : kFragConvert_None;
  const auto& prog = GetDrawBlitProg({
      fragHeader,
      {kFragSample_OnePlane, fragConvert},
  });

  const ScopedSaveMultiTex saveTex(mGL, {0}, srcTarget);
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
  yuvArgs.colorSpaceForMatrix = Some(surface->GetYUVColorSpace());

  const DrawBlitProg::YUVArgs* pYuvArgs = nullptr;

  const auto planes = surface->GetTextureCount();
  const GLenum texTarget = LOCAL_GL_TEXTURE_2D;

  std::vector<uint8_t> texUnits;
  for (uint8_t i = 0; i < planes; i++) {
    texUnits.push_back(i);
  }
  const ScopedSaveMultiTex saveTex(mGL, texUnits, texTarget);
  const auto pixelFormat = surface->GetSurfaceType();

  const char* fragSample;
  auto fragConvert = kFragConvert_None;
  switch (pixelFormat) {
    case DMABufSurface::SURFACE_RGBA:
      fragSample = kFragSample_OnePlane;
      break;
    case DMABufSurface::SURFACE_NV12:
      fragSample = kFragSample_TwoPlane;
      pYuvArgs = &yuvArgs;
      fragConvert = kFragConvert_ColorMatrix;
      break;
    case DMABufSurface::SURFACE_YUV420:
      fragSample = kFragSample_ThreePlane;
      pYuvArgs = &yuvArgs;
      fragConvert = kFragConvert_ColorMatrix;
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

  const auto& prog =
      GetDrawBlitProg({kFragHeader_Tex2D, {fragSample, fragConvert}});
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

// -

template <size_t N>
static void PushUnorm(uint32_t* const out, const float inVal) {
  const uint32_t mask = (1 << N) - 1;
  auto fval = inVal;
  fval = std::max(0.0f, std::min(fval, 1.0f));
  fval *= mask;
  fval = roundf(fval);
  auto ival = static_cast<uint32_t>(fval);
  ival &= mask;

  *out <<= N;
  *out |= ival;
}

static uint32_t toRgb10A2(const color::vec4& val) {
  // R in LSB
  uint32_t ret = 0;
  PushUnorm<2>(&ret, val.w());
  PushUnorm<10>(&ret, val.z());
  PushUnorm<10>(&ret, val.y());
  PushUnorm<10>(&ret, val.x());
  return ret;
}

std::shared_ptr<gl::Texture> GLBlitHelper::GetColorLutTex(
    const ColorLutKey& key) const {
  auto& weak = mColorLutTexMap[key];
  auto strong = weak.lock();
  if (!strong) {
    auto& gl = *mGL;
    strong = std::make_shared<gl::Texture>(gl);
    weak = strong;

    const auto ct = color::ColorspaceTransform::Create(key.src, key.dst);

    // -

    const auto minLutSize = color::ivec3{2};
    const auto maxLutSize = color::ivec3{256};
    auto lutSize = minLutSize;
    if (ct.srcSpace.yuv) {
      lutSize.x(StaticPrefs::gfx_blithelper_lut_size_ycbcr_y());
      lutSize.y(StaticPrefs::gfx_blithelper_lut_size_ycbcr_cb());
      lutSize.z(StaticPrefs::gfx_blithelper_lut_size_ycbcr_cr());
    } else {
      lutSize.x(StaticPrefs::gfx_blithelper_lut_size_rgb_r());
      lutSize.y(StaticPrefs::gfx_blithelper_lut_size_rgb_g());
      lutSize.z(StaticPrefs::gfx_blithelper_lut_size_rgb_b());
    }
    lutSize = max(minLutSize, min(lutSize, maxLutSize)); // Clamp

    const auto lut = ct.ToLut3(lutSize);
    const auto& size = lut.size;

    // -

    constexpr GLenum target = LOCAL_GL_TEXTURE_3D;
    const auto bind = gl::ScopedBindTexture(&gl, strong->name, target);
    gl.fTexParameteri(target, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    gl.fTexParameteri(target, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
    gl.fTexParameteri(target, LOCAL_GL_TEXTURE_WRAP_R, LOCAL_GL_CLAMP_TO_EDGE);
    gl.fTexParameteri(target, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    gl.fTexParameteri(target, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);

    bool useFloat16 = true;
    if (useFloat16) {
      // Use rgba16f, which we can thankfully upload as rgba32f
      static_assert(sizeof(color::vec4) == sizeof(float) * 4);
      std::vector<color::vec4> uploadData;
      uploadData.reserve(lut.data.size());
      for (const auto& src : lut.data) {
        const auto dst = color::vec4{src, 1};
        uploadData.push_back(dst);
      }

      gl.fTexStorage3D(target, 1, LOCAL_GL_RGBA16F, size.x(), size.y(),
                       size.z());
      gl.fTexSubImage3D(target, 0, 0, 0, 0, size.x(), size.y(), size.z(),
                        LOCAL_GL_RGBA, LOCAL_GL_FLOAT, uploadData.data());
    } else {
      // Use Rgb10A2
      std::vector<uint32_t> uploadData;
      uploadData.reserve(lut.data.size());
      for (const auto& src : lut.data) {
        const auto dst = toRgb10A2({src, 1});
        uploadData.push_back(dst);
      }

      gl.fTexStorage3D(target, 1, LOCAL_GL_RGB10_A2, size.x(), size.y(),
                       size.z());
      gl.fTexSubImage3D(target, 0, 0, 0, 0, size.x(), size.y(), size.z(),
                        LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV,
                        uploadData.data());
    }
  }
  return strong;
}

}  // namespace gl
}  // namespace mozilla
