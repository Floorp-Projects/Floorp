/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLBlitTextureImageHelper.h"
#include "GLUploadHelpers.h"
#include "DecomposeIntoNoRepeatTriangles.h"
#include "GLContext.h"
#include "ScopedGLHelpers.h"
#include "nsRect.h"
#include "gfx2DGlue.h"
#include "gfxUtils.h"

namespace mozilla {
namespace gl {

GLBlitTextureImageHelper::GLBlitTextureImageHelper(GLContext* gl)
    : mGL(gl)
    , mBlitProgram(0)
    , mBlitFramebuffer(0)

{
}

GLBlitTextureImageHelper::~GLBlitTextureImageHelper()
{
    // Likely used by OGL Layers.
    mGL->fDeleteProgram(mBlitProgram);
    mGL->fDeleteFramebuffers(1, &mBlitFramebuffer);
}

void
GLBlitTextureImageHelper::BlitTextureImage(TextureImage *aSrc, const nsIntRect& aSrcRect,
                                           TextureImage *aDst, const nsIntRect& aDstRect)
{
    NS_ASSERTION(!aSrc->InUpdate(), "Source texture is in update!");
    NS_ASSERTION(!aDst->InUpdate(), "Destination texture is in update!");

    if (aSrcRect.IsEmpty() || aDstRect.IsEmpty())
        return;

    int savedFb = 0;
    mGL->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &savedFb);

    ScopedGLState scopedScissorTestState(mGL, LOCAL_GL_SCISSOR_TEST, false);
    ScopedGLState scopedBlendState(mGL, LOCAL_GL_BLEND, false);

    // 2.0 means scale up by two
    float blitScaleX = float(aDstRect.width) / float(aSrcRect.width);
    float blitScaleY = float(aDstRect.height) / float(aSrcRect.height);

    // We start iterating over all destination tiles
    aDst->BeginTileIteration();
    do {
        // calculate portion of the tile that is going to be painted to
        nsIntRect dstSubRect;
        nsIntRect dstTextureRect = ThebesIntRect(aDst->GetTileRect());
        dstSubRect.IntersectRect(aDstRect, dstTextureRect);

        // this tile is not part of the destination rectangle aDstRect
        if (dstSubRect.IsEmpty())
            continue;

        // (*) transform the rect of this tile into the rectangle defined by aSrcRect...
        nsIntRect dstInSrcRect(dstSubRect);
        dstInSrcRect.MoveBy(-aDstRect.TopLeft());
        // ...which might be of different size, hence scale accordingly
        dstInSrcRect.ScaleRoundOut(1.0f / blitScaleX, 1.0f / blitScaleY);
        dstInSrcRect.MoveBy(aSrcRect.TopLeft());

        SetBlitFramebufferForDestTexture(aDst->GetTextureID());
        UseBlitProgram();

        aSrc->BeginTileIteration();
        // now iterate over all tiles in the source Image...
        do {
            // calculate portion of the source tile that is in the source rect
            nsIntRect srcSubRect;
            nsIntRect srcTextureRect = ThebesIntRect(aSrc->GetTileRect());
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
            // We need to transform this back into destination space, inverting the transform from (*)
            nsIntRect srcSubInDstRect(srcSubRect);
            srcSubInDstRect.MoveBy(-aSrcRect.TopLeft());
            srcSubInDstRect.ScaleRoundOut(blitScaleX, blitScaleY);
            srcSubInDstRect.MoveBy(aDstRect.TopLeft());

            // we transform these rectangles to be relative to the current src and dst tiles, respectively
            nsIntSize srcSize = srcTextureRect.Size();
            nsIntSize dstSize = dstTextureRect.Size();
            srcSubRect.MoveBy(-srcTextureRect.x, -srcTextureRect.y);
            srcSubInDstRect.MoveBy(-dstTextureRect.x, -dstTextureRect.y);

            float dx0 = 2.0f * float(srcSubInDstRect.x) / float(dstSize.width) - 1.0f;
            float dy0 = 2.0f * float(srcSubInDstRect.y) / float(dstSize.height) - 1.0f;
            float dx1 = 2.0f * float(srcSubInDstRect.x + srcSubInDstRect.width) / float(dstSize.width) - 1.0f;
            float dy1 = 2.0f * float(srcSubInDstRect.y + srcSubInDstRect.height) / float(dstSize.height) - 1.0f;
            ScopedViewportRect autoViewportRect(mGL, 0, 0, dstSize.width, dstSize.height);

            RectTriangles rects;

            nsIntSize realTexSize = srcSize;
            if (!CanUploadNonPowerOfTwo(mGL)) {
                realTexSize = nsIntSize(gfx::NextPowerOfTwo(srcSize.width),
                                        gfx::NextPowerOfTwo(srcSize.height));
            }

            if (aSrc->GetWrapMode() == LOCAL_GL_REPEAT) {
                rects.addRect(/* dest rectangle */
                        dx0, dy0, dx1, dy1,
                        /* tex coords */
                        srcSubRect.x / float(realTexSize.width),
                        srcSubRect.y / float(realTexSize.height),
                        srcSubRect.XMost() / float(realTexSize.width),
                        srcSubRect.YMost() / float(realTexSize.height));
            } else {
                DecomposeIntoNoRepeatTriangles(srcSubRect, realTexSize, rects);

                // now put the coords into the d[xy]0 .. d[xy]1 coordinate space
                // from the 0..1 that it comes out of decompose
                RectTriangles::vert_coord* v = (RectTriangles::vert_coord*)rects.vertexPointer();

                for (unsigned int i = 0; i < rects.elements(); ++i) {
                    v[i].x = (v[i].x * (dx1 - dx0)) + dx0;
                    v[i].y = (v[i].y * (dy1 - dy0)) + dy0;
                }
            }

            ScopedBindTextureUnit autoTexUnit(mGL, LOCAL_GL_TEXTURE0);
            ScopedBindTexture autoTex(mGL, aSrc->GetTextureID());

            mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);

            mGL->fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, rects.vertexPointer());
            mGL->fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, rects.texCoordPointer());

            mGL->fEnableVertexAttribArray(0);
            mGL->fEnableVertexAttribArray(1);

            mGL->fDrawArrays(LOCAL_GL_TRIANGLES, 0, rects.elements());

            mGL->fDisableVertexAttribArray(0);
            mGL->fDisableVertexAttribArray(1);

        } while (aSrc->NextTile());
    } while (aDst->NextTile());

    mGL->fVertexAttribPointer(0, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, nullptr);
    mGL->fVertexAttribPointer(1, 2, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, nullptr);

    // unbind the previous texture from the framebuffer
    SetBlitFramebufferForDestTexture(0);

    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, savedFb);
}

void
GLBlitTextureImageHelper::SetBlitFramebufferForDestTexture(GLuint aTexture)
{
    if (!mBlitFramebuffer) {
        mGL->fGenFramebuffers(1, &mBlitFramebuffer);
    }

    mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mBlitFramebuffer);
    mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                               LOCAL_GL_COLOR_ATTACHMENT0,
                               LOCAL_GL_TEXTURE_2D,
                               aTexture,
                               0);

    GLenum result = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (aTexture && (result != LOCAL_GL_FRAMEBUFFER_COMPLETE)) {
        nsAutoCString msg;
        msg.Append("Framebuffer not complete -- error 0x");
        msg.AppendInt(result, 16);
        // Note: if you are hitting this, it is likely that
        // your texture is not texture complete -- that is, you
        // allocated a texture name, but didn't actually define its
        // size via a call to TexImage2D.
        NS_RUNTIMEABORT(msg.get());
    }
}

void
GLBlitTextureImageHelper::UseBlitProgram()
{
    if (mBlitProgram) {
        mGL->fUseProgram(mBlitProgram);
        return;
    }

    mBlitProgram = mGL->fCreateProgram();

    GLuint shaders[2];
    shaders[0] = mGL->fCreateShader(LOCAL_GL_VERTEX_SHADER);
    shaders[1] = mGL->fCreateShader(LOCAL_GL_FRAGMENT_SHADER);

    const char *blitVSSrc =
        "attribute vec2 aVertex;"
        "attribute vec2 aTexCoord;"
        "varying vec2 vTexCoord;"
        "void main() {"
        "  vTexCoord = aTexCoord;"
        "  gl_Position = vec4(aVertex, 0.0, 1.0);"
        "}";
    const char *blitFSSrc = "#ifdef GL_ES\nprecision mediump float;\n#endif\n"
        "uniform sampler2D uSrcTexture;"
        "varying vec2 vTexCoord;"
        "void main() {"
        "  gl_FragColor = texture2D(uSrcTexture, vTexCoord);"
        "}";

    mGL->fShaderSource(shaders[0], 1, (const GLchar**) &blitVSSrc, nullptr);
    mGL->fShaderSource(shaders[1], 1, (const GLchar**) &blitFSSrc, nullptr);

    for (int i = 0; i < 2; ++i) {
        GLint success, len = 0;

        mGL->fCompileShader(shaders[i]);
        mGL->fGetShaderiv(shaders[i], LOCAL_GL_COMPILE_STATUS, &success);
        NS_ASSERTION(success, "Shader compilation failed!");

        if (!success) {
            nsAutoCString log;
            mGL->fGetShaderiv(shaders[i], LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &len);
            log.SetCapacity(len);
            mGL->fGetShaderInfoLog(shaders[i], len, (GLint*) &len, (char*) log.BeginWriting());
            log.SetLength(len);

            printf_stderr("Shader %d compilation failed:\n%s\n", log.get());
            return;
        }

        mGL->fAttachShader(mBlitProgram, shaders[i]);
        mGL->fDeleteShader(shaders[i]);
    }

    mGL->fBindAttribLocation(mBlitProgram, 0, "aVertex");
    mGL->fBindAttribLocation(mBlitProgram, 1, "aTexCoord");

    mGL->fLinkProgram(mBlitProgram);

    GLint success, len = 0;
    mGL->fGetProgramiv(mBlitProgram, LOCAL_GL_LINK_STATUS, &success);
    NS_ASSERTION(success, "Shader linking failed!");

    if (!success) {
        nsAutoCString log;
        mGL->fGetProgramiv(mBlitProgram, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &len);
        log.SetCapacity(len);
        mGL->fGetProgramInfoLog(mBlitProgram, len, (GLint*) &len, (char*) log.BeginWriting());
        log.SetLength(len);

        printf_stderr("Program linking failed:\n%s\n", log.get());
        return;
    }

    mGL->fUseProgram(mBlitProgram);
    mGL->fUniform1i(mGL->fGetUniformLocation(mBlitProgram, "uSrcTexture"), 0);
}

}
}
