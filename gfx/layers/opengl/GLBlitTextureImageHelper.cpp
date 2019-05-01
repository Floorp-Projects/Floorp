/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLBlitTextureImageHelper.h"
#include "GLUploadHelpers.h"
#include "DecomposeIntoNoRepeatTriangles.h"
#include "GLContext.h"
#include "GLTextureImage.h"
#include "ScopedGLHelpers.h"
#include "nsRect.h"
#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "CompositorOGL.h"
#include "mozilla/gfx/Point.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

GLBlitTextureImageHelper::GLBlitTextureImageHelper(CompositorOGL* aCompositor)
    : mCompositor(aCompositor),
      mBlitProgram(0),
      mBlitFramebuffer(0)

{}

GLBlitTextureImageHelper::~GLBlitTextureImageHelper() {
  GLContext* gl = mCompositor->gl();
  // Likely used by OGL Layers.
  gl->fDeleteProgram(mBlitProgram);
  gl->fDeleteFramebuffers(1, &mBlitFramebuffer);
}

void GLBlitTextureImageHelper::BlitTextureImage(TextureImage* aSrc,
                                                const gfx::IntRect& aSrcRect,
                                                TextureImage* aDst,
                                                const gfx::IntRect& aDstRect) {
  GLContext* gl = mCompositor->gl();

  if (!aSrc || !aDst || aSrcRect.IsEmpty() || aDstRect.IsEmpty()) return;

  int savedFb = 0;
  gl->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &savedFb);

  ScopedGLState scopedScissorTestState(gl, LOCAL_GL_SCISSOR_TEST, false);
  ScopedGLState scopedBlendState(gl, LOCAL_GL_BLEND, false);

  // 2.0 means scale up by two
  float blitScaleX = float(aDstRect.Width()) / float(aSrcRect.Width());
  float blitScaleY = float(aDstRect.Height()) / float(aSrcRect.Height());

  // We start iterating over all destination tiles
  aDst->BeginBigImageIteration();
  do {
    // calculate portion of the tile that is going to be painted to
    gfx::IntRect dstSubRect;
    gfx::IntRect dstTextureRect = aDst->GetTileRect();
    dstSubRect.IntersectRect(aDstRect, dstTextureRect);

    // this tile is not part of the destination rectangle aDstRect
    if (dstSubRect.IsEmpty()) continue;

    // (*) transform the rect of this tile into the rectangle defined by
    // aSrcRect...
    gfx::IntRect dstInSrcRect(dstSubRect);
    dstInSrcRect.MoveBy(-aDstRect.TopLeft());
    // ...which might be of different size, hence scale accordingly
    dstInSrcRect.ScaleRoundOut(1.0f / blitScaleX, 1.0f / blitScaleY);
    dstInSrcRect.MoveBy(aSrcRect.TopLeft());

    SetBlitFramebufferForDestTexture(aDst->GetTextureID());
    UseBlitProgram();

    aSrc->BeginBigImageIteration();
    // now iterate over all tiles in the source Image...
    do {
      // calculate portion of the source tile that is in the source rect
      gfx::IntRect srcSubRect;
      gfx::IntRect srcTextureRect = aSrc->GetTileRect();
      srcSubRect.IntersectRect(aSrcRect, srcTextureRect);

      // this tile is not part of the source rect
      if (srcSubRect.IsEmpty()) {
        continue;
      }
      // calculate intersection of source rect with destination rect
      srcSubRect.IntersectRect(srcSubRect, dstInSrcRect);
      // this tile does not overlap the current destination tile
      if (srcSubRect.IsEmpty()) {
        continue;
      }
      // We now have the intersection of
      //     the current source tile
      // and the desired source rectangle
      // and the destination tile
      // and the desired destination rectange
      // in destination space.
      // We need to transform this back into destination space, inverting the
      // transform from (*)
      gfx::IntRect srcSubInDstRect(srcSubRect);
      srcSubInDstRect.MoveBy(-aSrcRect.TopLeft());
      srcSubInDstRect.ScaleRoundOut(blitScaleX, blitScaleY);
      srcSubInDstRect.MoveBy(aDstRect.TopLeft());

      // we transform these rectangles to be relative to the current src and dst
      // tiles, respectively
      gfx::IntSize srcSize = srcTextureRect.Size();
      gfx::IntSize dstSize = dstTextureRect.Size();
      srcSubRect.MoveBy(-srcTextureRect.X(), -srcTextureRect.Y());
      srcSubInDstRect.MoveBy(-dstTextureRect.X(), -dstTextureRect.Y());

      float dx0 =
          2.0f * float(srcSubInDstRect.X()) / float(dstSize.width) - 1.0f;
      float dy0 =
          2.0f * float(srcSubInDstRect.Y()) / float(dstSize.height) - 1.0f;
      float dx1 =
          2.0f * float(srcSubInDstRect.XMost()) / float(dstSize.width) - 1.0f;
      float dy1 =
          2.0f * float(srcSubInDstRect.YMost()) / float(dstSize.height) - 1.0f;
      ScopedViewportRect autoViewportRect(gl, 0, 0, dstSize.width,
                                          dstSize.height);

      RectTriangles rects;

      gfx::IntSize realTexSize = srcSize;
      if (!CanUploadNonPowerOfTwo(gl)) {
        realTexSize = gfx::IntSize(RoundUpPow2(srcSize.width),
                                   RoundUpPow2(srcSize.height));
      }

      if (aSrc->GetWrapMode() == LOCAL_GL_REPEAT) {
        rects.addRect(/* dest rectangle */
                      dx0, dy0, dx1, dy1,
                      /* tex coords */
                      srcSubRect.X() / float(realTexSize.width),
                      srcSubRect.Y() / float(realTexSize.height),
                      srcSubRect.XMost() / float(realTexSize.width),
                      srcSubRect.YMost() / float(realTexSize.height));
      } else {
        DecomposeIntoNoRepeatTriangles(srcSubRect, realTexSize, rects);

        // now put the coords into the d[xy]0 .. d[xy]1 coordinate space
        // from the 0..1 that it comes out of decompose
        InfallibleTArray<RectTriangles::coord>& coords = rects.vertCoords();

        for (unsigned int i = 0; i < coords.Length(); ++i) {
          coords[i].x = (coords[i].x * (dx1 - dx0)) + dx0;
          coords[i].y = (coords[i].y * (dy1 - dy0)) + dy0;
        }
      }

      ScopedBindTextureUnit autoTexUnit(gl, LOCAL_GL_TEXTURE0);
      ScopedBindTexture autoTex(gl, aSrc->GetTextureID());
      ScopedVertexAttribPointer autoAttrib0(gl, 0, 2, LOCAL_GL_FLOAT,
                                            LOCAL_GL_FALSE, 0, 0,
                                            rects.vertCoords().Elements());
      ScopedVertexAttribPointer autoAttrib1(gl, 1, 2, LOCAL_GL_FLOAT,
                                            LOCAL_GL_FALSE, 0, 0,
                                            rects.texCoords().Elements());

      gl->fDrawArrays(LOCAL_GL_TRIANGLES, 0, rects.elements());

    } while (aSrc->NextTile());
  } while (aDst->NextTile());

  // unbind the previous texture from the framebuffer
  SetBlitFramebufferForDestTexture(0);

  gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, savedFb);
}

void GLBlitTextureImageHelper::SetBlitFramebufferForDestTexture(
    GLuint aTexture) {
  GLContext* gl = mCompositor->gl();
  if (!mBlitFramebuffer) {
    gl->fGenFramebuffers(1, &mBlitFramebuffer);
  }

  gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBlitFramebuffer);
  gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                            LOCAL_GL_TEXTURE_2D, aTexture, 0);

  GLenum result = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
  if (aTexture && (result != LOCAL_GL_FRAMEBUFFER_COMPLETE)) {
    nsAutoCString msg;
    msg.AppendLiteral("Framebuffer not complete -- error 0x");
    msg.AppendInt(result, 16);
    // Note: if you are hitting this, it is likely that
    // your texture is not texture complete -- that is, you
    // allocated a texture name, but didn't actually define its
    // size via a call to TexImage2D.
    MOZ_CRASH_UNSAFE(msg.get());
  }
}

void GLBlitTextureImageHelper::UseBlitProgram() {
  // XXX: GLBlitTextureImageHelper doesn't use ShaderProgramOGL
  // so we need to Reset the program
  mCompositor->ResetProgram();

  GLContext* gl = mCompositor->gl();
  if (mBlitProgram) {
    gl->fUseProgram(mBlitProgram);
    return;
  }

  mBlitProgram = gl->fCreateProgram();

  GLuint shaders[2];
  shaders[0] = gl->fCreateShader(LOCAL_GL_VERTEX_SHADER);
  shaders[1] = gl->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);

  const char* blitVSSrc =
      "attribute vec2 aVertex;"
      "attribute vec2 aTexCoord;"
      "varying vec2 vTexCoord;"
      "void main() {"
      "  vTexCoord = aTexCoord;"
      "  gl_Position = vec4(aVertex, 0.0, 1.0);"
      "}";
  const char* blitFSSrc =
      "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
      "uniform sampler2D uSrcTexture;"
      "varying vec2 vTexCoord;"
      "void main() {"
      "  gl_FragColor = texture2D(uSrcTexture, vTexCoord);"
      "}";

  gl->fShaderSource(shaders[0], 1, (const GLchar**)&blitVSSrc, nullptr);
  gl->fShaderSource(shaders[1], 1, (const GLchar**)&blitFSSrc, nullptr);

  for (int i = 0; i < 2; ++i) {
    GLint success, len = 0;

    gl->fCompileShader(shaders[i]);
    gl->fGetShaderiv(shaders[i], LOCAL_GL_COMPILE_STATUS, &success);
    NS_ASSERTION(success, "Shader compilation failed!");

    if (!success) {
      nsAutoCString log;
      gl->fGetShaderiv(shaders[i], LOCAL_GL_INFO_LOG_LENGTH, (GLint*)&len);
      log.SetLength(len);
      gl->fGetShaderInfoLog(shaders[i], len, (GLint*)&len,
                            (char*)log.BeginWriting());

      printf_stderr("Shader %d compilation failed:\n%s\n", i, log.get());
      return;
    }

    gl->fAttachShader(mBlitProgram, shaders[i]);
    gl->fDeleteShader(shaders[i]);
  }

  gl->fBindAttribLocation(mBlitProgram, 0, "aVertex");
  gl->fBindAttribLocation(mBlitProgram, 1, "aTexCoord");

  gl->fLinkProgram(mBlitProgram);

  GLint success, len = 0;
  gl->fGetProgramiv(mBlitProgram, LOCAL_GL_LINK_STATUS, &success);
  NS_ASSERTION(success, "Shader linking failed!");

  if (!success) {
    nsAutoCString log;
    gl->fGetProgramiv(mBlitProgram, LOCAL_GL_INFO_LOG_LENGTH, (GLint*)&len);
    log.SetLength(len);
    gl->fGetProgramInfoLog(mBlitProgram, len, (GLint*)&len,
                           (char*)log.BeginWriting());

    printf_stderr("Program linking failed:\n%s\n", log.get());
    return;
  }

  gl->fUseProgram(mBlitProgram);
  gl->fUniform1i(gl->fGetUniformLocation(mBlitProgram, "uSrcTexture"), 0);
}

}  // namespace layers
}  // namespace mozilla
