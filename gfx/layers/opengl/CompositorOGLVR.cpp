/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorOGL.h"
#include <sstream>                      // for ostringstream
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint8_t
#include <stdlib.h>                     // for free, malloc
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite, etc
#include "mozilla/layers/Effects.h"     // for EffectChain, TexturedEffect, etc
#include "mozilla/layers/CompositingRenderTargetOGL.h"
#include "mozilla/layers/TextureHost.h"  // for TextureSource, etc
#include "mozilla/layers/TextureHostOGL.h"  // for TextureSourceOGL, etc
#include "GLContextProvider.h"          // for GLContextProvider
#include "GLContext.h"                  // for GLContext
#include "GLUploadHelpers.h"
#include "Layers.h"                     // for WriteSnapshotToDumpFile
#include "LayerScope.h"                 // for LayerScope
#include "gfx2DGlue.h"                  // for ThebesFilter
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxCrashReporterUtils.h"      // for ScopedGfxFeatureReporter
#include "gfxMatrix.h"                  // for gfxMatrix
#include "GraphicsFilter.h"             // for GraphicsFilter
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for NextPowerOfTwo, gfxUtils, etc
#include "gfxVR.h"

namespace mozilla {

using namespace std;
using namespace gfx;

namespace layers {

using namespace mozilla::gl;

bool
CompositorOGL::InitializeVR()
{
  gl()->fGenBuffers(2, mVR.mDistortionVertices);
  gl()->fGenBuffers(2, mVR.mDistortionIndices);

  // these are explicitly bound later
  mVR.mAPosition = 0;
  mVR.mATexCoord0 = 1;
  mVR.mATexCoord1 = 2;
  mVR.mATexCoord2 = 3;
  mVR.mAGenericAttribs = 4;

  ostringstream vs;

  vs << "uniform vec4 uVREyeToSource;\n";
  vs << "uniform vec4 uVRDestinationScaleAndOffset;\n";
  vs << "uniform float uHeight;\n";
  vs << "attribute vec2 aPosition;\n";
  vs << "attribute vec2 aTexCoord0;\n";
  vs << "attribute vec2 aTexCoord1;\n";
  vs << "attribute vec2 aTexCoord2;\n";
  vs << "attribute vec4 aGenericAttribs;\n";
  vs << "varying vec2 vTexCoord0;\n";
  vs << "varying vec2 vTexCoord1;\n";
  vs << "varying vec2 vTexCoord2;\n";
  vs << "varying vec4 vGenericAttribs;\n";
  vs << "void main() {\n";
  vs << "  gl_Position = vec4(aPosition.xy * uVRDestinationScaleAndOffset.zw + uVRDestinationScaleAndOffset.xy, 0.5, 1.0);\n";
  vs << "  vTexCoord0 = uVREyeToSource.zw * aTexCoord0 + uVREyeToSource.xy;\n";
  vs << "  vTexCoord1 = uVREyeToSource.zw * aTexCoord1 + uVREyeToSource.xy;\n";
  vs << "  vTexCoord2 = uVREyeToSource.zw * aTexCoord2 + uVREyeToSource.xy;\n";
  vs << "  vTexCoord0.y = uHeight - vTexCoord0.y;\n";
  vs << "  vTexCoord1.y = uHeight - vTexCoord1.y;\n";
  vs << "  vTexCoord2.y = uHeight - vTexCoord2.y;\n";
  vs << "  vGenericAttribs = aGenericAttribs;\n";
  vs << "}\n";

  std::string vsSrcString(vs.str());
  const char *vsSrc = vsSrcString.c_str();

  for (int programIndex = 0; programIndex < 2; programIndex++) {
    GLenum textureTarget = programIndex == 0 ? LOCAL_GL_TEXTURE_2D : LOCAL_GL_TEXTURE_RECTANGLE;

    const char *sampler2D = "sampler2D";
    const char *texture2D = "texture2D";

    ostringstream fs;

    if (textureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
      fs << "#extension GL_ARB_texture_rectangle : require\n";
      sampler2D = "sampler2DRect";
      texture2D = "texture2DRect";
    }

    if (gl()->IsGLES()) {
      fs << "precision highp float;\n";
    }

    fs << "uniform " << sampler2D << " uTexture;\n";
    fs << "varying vec2 vTexCoord0;\n";
    fs << "varying vec2 vTexCoord1;\n";
    fs << "varying vec2 vTexCoord2;\n";
    fs << "varying vec4 vGenericAttribs;\n";
    fs << "void main() {\n";
    fs << "  float resR = " << texture2D << "(uTexture, vTexCoord0.xy).r;\n";
    fs << "  float resG = " << texture2D << "(uTexture, vTexCoord1.xy).g;\n";
    fs << "  float resB = " << texture2D << "(uTexture, vTexCoord2.xy).b;\n";
    fs << "  gl_FragColor = vec4(resR * vGenericAttribs.r, resG * vGenericAttribs.r, resB * vGenericAttribs.r, 1.0);\n";
    fs << "}\n";

    std::string fsSrcString(fs.str());
    const char *fsSrc = fsSrcString.c_str();

    GLint status;
#ifdef DEBUG
    GLint length;
    nsCString logStr;
#endif

    GLuint shaders[2];
    shaders[0] = gl()->fCreateShader(LOCAL_GL_VERTEX_SHADER);
    shaders[1] = gl()->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);

    GLint shaderLen[2] = { (GLint) strlen(vsSrc), (GLint) strlen(fsSrc) };
    gl()->fShaderSource(shaders[0], 1, &vsSrc, &shaderLen[0]);
    gl()->fShaderSource(shaders[1], 1, &fsSrc, &shaderLen[1]);

    for (int k = 0; k < 2; ++k) {
      gl()->fCompileShader(shaders[k]);
      gl()->fGetShaderiv(shaders[k], LOCAL_GL_COMPILE_STATUS, &status);
      if (!status) {
        NS_WARNING("VR GL shader failed to compile!");

#ifdef DEBUG
        gl()->fGetShaderiv(shaders[k], LOCAL_GL_INFO_LOG_LENGTH, &length);
        logStr.SetLength(length);
        gl()->fGetShaderInfoLog(shaders[k], length, &length, logStr.BeginWriting());
        printf_stderr("GL %s shader failed to compile:\n%s\n", k == 0 ? "vertex" : "fragment", logStr.BeginReading());
#endif

        gl()->fDeleteShader(shaders[0]);
        gl()->fDeleteShader(shaders[1]);

        return false;
      }
    }

    GLuint prog = gl()->fCreateProgram();
    gl()->fAttachShader(prog, shaders[0]);
    gl()->fAttachShader(prog, shaders[1]);

    gl()->fBindAttribLocation(prog, mVR.mAPosition, "aPosition");
    gl()->fBindAttribLocation(prog, mVR.mATexCoord0, "aTexCoord0");
    gl()->fBindAttribLocation(prog, mVR.mATexCoord1, "aTexCoord1");
    gl()->fBindAttribLocation(prog, mVR.mATexCoord2, "aTexCoord2");
    gl()->fBindAttribLocation(prog, mVR.mAGenericAttribs, "aGenericAttribs");

    gl()->fLinkProgram(prog);

    gl()->fGetProgramiv(prog, LOCAL_GL_LINK_STATUS, &status);
    if (!status) {
      NS_WARNING("VR GL program failed to link!");

#ifdef DEBUG
      gl()->fGetProgramiv(prog, LOCAL_GL_INFO_LOG_LENGTH, &length);
      logStr.SetLength(length);
      gl()->fGetProgramInfoLog(prog, length, &length, logStr.BeginWriting());
      printf_stderr("GL shader failed to link:\n%s\n", logStr.BeginReading());
#endif

      gl()->fDeleteProgram(prog);
      gl()->fDeleteShader(shaders[0]);
      gl()->fDeleteShader(shaders[1]);

      return false;
    }

    mVR.mUTexture[programIndex] = gl()->fGetUniformLocation(prog, "uTexture");
    mVR.mUVREyeToSource[programIndex] = gl()->fGetUniformLocation(prog, "uVREyeToSource");
    mVR.mUVRDestionatinScaleAndOffset[programIndex] = gl()->fGetUniformLocation(prog, "uVRDestinationScaleAndOffset");
    mVR.mUHeight[programIndex] = gl()->fGetUniformLocation(prog, "uHeight");

    mVR.mDistortionProgram[programIndex] = prog;

    gl()->fDeleteShader(shaders[0]);
    gl()->fDeleteShader(shaders[1]);
  }

  mVR.mInitialized = true;
  return true;
}

void
CompositorOGL::DestroyVR(GLContext *gl)
{
  if (!mVR.mInitialized)
    return;

  gl->fDeleteBuffers(2, mVR.mDistortionVertices);
  gl->fDeleteBuffers(2, mVR.mDistortionIndices);
  gl->fDeleteProgram(mVR.mDistortionProgram[0]);
  gl->fDeleteProgram(mVR.mDistortionProgram[1]);

  mVR.mInitialized = false;
}

void
CompositorOGL::DrawVRDistortion(const gfx::Rect& aRect,
                                const gfx::Rect& aClipRect,
                                const EffectChain& aEffectChain,
                                gfx::Float aOpacity,
                                const gfx::Matrix4x4& aTransform)
{
  MOZ_ASSERT(aEffectChain.mPrimaryEffect->mType == EffectTypes::VR_DISTORTION);
  MOZ_ASSERT(mVR.mInitialized);

  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK] ||
      aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE])
  {
    NS_WARNING("DrawVRDistortion: ignoring secondary effect!");
  }

  EffectVRDistortion* vrEffect =
    static_cast<EffectVRDistortion*>(aEffectChain.mPrimaryEffect.get());

  GLenum textureTarget = LOCAL_GL_TEXTURE_2D;
  if (vrEffect->mRenderTarget)
    textureTarget = mFBOTextureTarget;

  RefPtr<CompositingRenderTargetOGL> surface =
    static_cast<CompositingRenderTargetOGL*>(vrEffect->mRenderTarget.get());

  gfx::IntSize size = surface->GetInitSize(); // XXX source->GetSize()

  VRHMDInfo* hmdInfo = vrEffect->mHMD;
  VRDistortionConstants shaderConstants;

  if (hmdInfo->GetConfiguration() != mVR.mConfiguration) {
    for (uint32_t eye = 0; eye < 2; eye++) {
      const gfx::VRDistortionMesh& mesh = hmdInfo->GetDistortionMesh(eye);
      gl()->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mVR.mDistortionVertices[eye]);
      gl()->fBufferData(LOCAL_GL_ARRAY_BUFFER,
                       mesh.mVertices.Length() * sizeof(gfx::VRDistortionVertex),
                       mesh.mVertices.Elements(),
                       LOCAL_GL_STATIC_DRAW);

      gl()->fBindBuffer(LOCAL_GL_ELEMENT_ARRAY_BUFFER, mVR.mDistortionIndices[eye]);
      gl()->fBufferData(LOCAL_GL_ELEMENT_ARRAY_BUFFER,
                       mesh.mIndices.Length() * sizeof(uint16_t),
                       mesh.mIndices.Elements(),
                       LOCAL_GL_STATIC_DRAW);

      mVR.mDistortionIndexCount[eye] = mesh.mIndices.Length();
    }

    mVR.mConfiguration = hmdInfo->GetConfiguration();
  }

  int programIndex = textureTarget == LOCAL_GL_TEXTURE_2D ? 0 : 1;

  gl()->fScissor(aClipRect.x, FlipY(aClipRect.y + aClipRect.height),
                 aClipRect.width, aClipRect.height);

  // Clear out the entire area that we want to render; this ensures that
  // the layer will be opaque, even though the mesh geometry we'll be
  // drawing below won't cover the full rectangle.
  gl()->fClearColor(0.0, 0.0, 0.0, 1.0);
  gl()->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT);

  // Make sure that the cached current program is reset for the
  // rest of the compositor, since we're using a custom program here
  ResetProgram();

  gl()->fUseProgram(mVR.mDistortionProgram[programIndex]);

  gl()->fEnableVertexAttribArray(mVR.mAPosition);
  gl()->fEnableVertexAttribArray(mVR.mATexCoord0);
  gl()->fEnableVertexAttribArray(mVR.mATexCoord1);
  gl()->fEnableVertexAttribArray(mVR.mATexCoord2);
  gl()->fEnableVertexAttribArray(mVR.mAGenericAttribs);

  surface->BindTexture(LOCAL_GL_TEXTURE0, mFBOTextureTarget);
  gl()->fUniform1i(mVR.mUTexture[programIndex], 0);

  gfx::IntSize vpSizeInt = mCurrentRenderTarget->GetInitSize();
  gfx::Size vpSize(vpSizeInt.width, vpSizeInt.height);

  for (uint32_t eye = 0; eye < 2; eye++) {
    gfx::IntRect eyeViewport;
    eyeViewport.x = eye * size.width / 2;
    eyeViewport.y = 0;
    eyeViewport.width = size.width / 2;
    eyeViewport.height = size.height;

    hmdInfo->FillDistortionConstants(eye,
                                     size, eyeViewport,
                                     vpSize, aRect,
                                     shaderConstants);

    float height = 1.0f;
    float texScaleAndOffset[4] = { shaderConstants.eyeToSourceScaleAndOffset[0],
                                   shaderConstants.eyeToSourceScaleAndOffset[1],
                                   shaderConstants.eyeToSourceScaleAndOffset[2],
                                   shaderConstants.eyeToSourceScaleAndOffset[3] };
    if (textureTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) {
      texScaleAndOffset[0] *= size.width;
      texScaleAndOffset[1] *= size.height;
      texScaleAndOffset[2] *= size.width;
      texScaleAndOffset[3] *= size.height;
      height = size.height;
    }

    gl()->fUniform4fv(mVR.mUVRDestionatinScaleAndOffset[programIndex], 1, shaderConstants.destinationScaleAndOffset);
    gl()->fUniform4fv(mVR.mUVREyeToSource[programIndex], 1, texScaleAndOffset);
    gl()->fUniform1f(mVR.mUHeight[programIndex], height);

    gl()->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mVR.mDistortionVertices[eye]);

    /* This is for Oculus DistortionVertex */

    gl()->fVertexAttribPointer(mVR.mAPosition,  2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, sizeof(gfx::VRDistortionVertex), (void*) (sizeof(float) * 0));
    gl()->fVertexAttribPointer(mVR.mATexCoord0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, sizeof(gfx::VRDistortionVertex), (void*) (sizeof(float) * 2));
    gl()->fVertexAttribPointer(mVR.mATexCoord1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, sizeof(gfx::VRDistortionVertex), (void*) (sizeof(float) * 4));
    gl()->fVertexAttribPointer(mVR.mATexCoord2, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, sizeof(gfx::VRDistortionVertex), (void*) (sizeof(float) * 6));
    gl()->fVertexAttribPointer(mVR.mAGenericAttribs, 4, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, sizeof(gfx::VRDistortionVertex),  (void*) (sizeof(float) * 8));

    gl()->fBindBuffer(LOCAL_GL_ELEMENT_ARRAY_BUFFER, mVR.mDistortionIndices[eye]);
    gl()->fDrawElements(LOCAL_GL_TRIANGLES, mVR.mDistortionIndexCount[eye], LOCAL_GL_UNSIGNED_SHORT, 0);
  }

  // Not clear if I should disable all of this; but going to do it and hope that
  // any later code will enable what it needs.
  gl()->fDisableVertexAttribArray(mVR.mAPosition);
  gl()->fDisableVertexAttribArray(mVR.mATexCoord0);
  gl()->fDisableVertexAttribArray(mVR.mATexCoord1);
  gl()->fDisableVertexAttribArray(mVR.mATexCoord2);
  gl()->fDisableVertexAttribArray(mVR.mAGenericAttribs);
}

} // namespace layers
} // namespace mozilla
