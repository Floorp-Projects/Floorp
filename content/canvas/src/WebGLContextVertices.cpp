/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLVertexArray.h"
#include "WebGLTexture.h"
#include "WebGLRenderbuffer.h"
#include "WebGLFramebuffer.h"

using namespace mozilla;

// For a Tegra workaround.
static const int MAX_DRAW_CALLS_SINCE_FLUSH = 100;


bool WebGLContext::DrawArrays_check(WebGLint first, WebGLsizei count, WebGLsizei primcount, const char* info)
{
    if (first < 0 || count < 0) {
        ErrorInvalidValue("%s: negative first or count", info);
        return false;
    }

    if (!ValidateStencilParamsForDrawCall()) {
        return false;
    }

    // If count is 0, there's nothing to do.
    if (count == 0 || primcount == 0) {
        return false;
    }

    // If there is no current program, this is silently ignored.
    // Any checks below this depend on a program being available.
    if (!mCurrentProgram) {
        return false;
    }

    uint32_t maxAllowedCount = 0;
    if (!ValidateBuffers(&maxAllowedCount, info)) {
        return false;
    }

    CheckedInt<GLsizei> checked_firstPlusCount = CheckedInt<GLsizei>(first) + count;

    if (!checked_firstPlusCount.isValid()) {
        ErrorInvalidOperation("%s: overflow in first+count", info);
        return false;
    }

    if (uint32_t(checked_firstPlusCount.value()) > maxAllowedCount) {
        ErrorInvalidOperation("%s: bound vertex attribute buffers do not have sufficient size for given first and count", info);
        return false;
    }

    MakeContextCurrent();

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeRenderbuffers()) {
            ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
            return false;
        }
    }

    if (!DoFakeVertexAttrib0(checked_firstPlusCount.value())) {
        return false;
    }
    BindFakeBlackTextures();

    return true;
}

void
WebGLContext::DrawArrays(GLenum mode, WebGLint first, WebGLsizei count)
{
    if (!IsContextStable())
        return;

    if (!ValidateDrawModeEnum(mode, "drawArrays: mode"))
        return;

    if (!DrawArrays_check(first, count, 1, "drawArrays"))
        return;

    SetupContextLossTimer();
    gl->fDrawArrays(mode, first, count);

    Draw_cleanup();
}

void
WebGLContext::DrawArraysInstanced(GLenum mode, WebGLint first, WebGLsizei count, WebGLsizei primcount)
{
    if (!IsContextStable())
        return;

    if (!ValidateDrawModeEnum(mode, "drawArraysInstanced: mode"))
        return;

    if (!DrawArrays_check(first, count, primcount, "drawArraysInstanced"))
        return;

    SetupContextLossTimer();
    gl->fDrawArraysInstanced(mode, first, count, primcount);

    Draw_cleanup();
}

bool
WebGLContext::DrawElements_check(WebGLsizei count, WebGLenum type, WebGLintptr byteOffset, WebGLsizei primcount, const char* info)
{
    if (count < 0 || byteOffset < 0) {
        ErrorInvalidValue("%s: negative count or offset", info);
        return false;
    }

    if (!ValidateStencilParamsForDrawCall()) {
        return false;
    }

    // If count is 0, there's nothing to do.
    if (count == 0 || primcount == 0) {
        return false;
    }

    CheckedUint32 checked_byteCount;

    WebGLsizei first = 0;

    if (type == LOCAL_GL_UNSIGNED_SHORT) {
        checked_byteCount = 2 * CheckedUint32(count);
        if (byteOffset % 2 != 0) {
            ErrorInvalidOperation("%s: invalid byteOffset for UNSIGNED_SHORT (must be a multiple of 2)", info);
            return false;
        }
        first = byteOffset / 2;
    }
    else if (type == LOCAL_GL_UNSIGNED_BYTE) {
        checked_byteCount = count;
        first = byteOffset;
    }
    else if (type == LOCAL_GL_UNSIGNED_INT && IsExtensionEnabled(OES_element_index_uint)) {
        checked_byteCount = 4 * CheckedUint32(count);
        if (byteOffset % 4 != 0) {
            ErrorInvalidOperation("%s: invalid byteOffset for UNSIGNED_INT (must be a multiple of 4)", info);
            return false;
        }
        first = byteOffset / 4;
    }
    else {
        ErrorInvalidEnum("%s: type must be UNSIGNED_SHORT or UNSIGNED_BYTE", info);
        return false;
    }

    if (!checked_byteCount.isValid()) {
        ErrorInvalidValue("%s: overflow in byteCount", info);
        return false;
    }

    // If there is no current program, this is silently ignored.
    // Any checks below this depend on a program being available.
    if (!mCurrentProgram) {
        return false;
    }

    if (!mBoundVertexArray->mBoundElementArrayBuffer) {
        ErrorInvalidOperation("%s: must have element array buffer binding", info);
        return false;
    }

    if (!mBoundVertexArray->mBoundElementArrayBuffer->ByteLength()) {
        ErrorInvalidOperation("%s: bound element array buffer doesn't have any data", info);
        return false;
    }

    CheckedInt<GLsizei> checked_neededByteCount = checked_byteCount.toChecked<GLsizei>() + byteOffset;

    if (!checked_neededByteCount.isValid()) {
        ErrorInvalidOperation("%s: overflow in byteOffset+byteCount", info);
        return false;
    }

    if (uint32_t(checked_neededByteCount.value()) > mBoundVertexArray->mBoundElementArrayBuffer->ByteLength()) {
        ErrorInvalidOperation("%s: bound element array buffer is too small for given count and offset", info);
        return false;
    }

    uint32_t maxAllowedCount = 0;
    if (!ValidateBuffers(&maxAllowedCount, info))
        return false;

    if (!maxAllowedCount ||
        !mBoundVertexArray->mBoundElementArrayBuffer->Validate(type, maxAllowedCount - 1, first, count))
    {
        ErrorInvalidOperation(
                              "%s: bound vertex attribute buffers do not have sufficient "
                              "size for given indices from the bound element array", info);
        return false;
    }

    MakeContextCurrent();

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeRenderbuffers()) {
            ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
            return false;
        }
    }

    if (!DoFakeVertexAttrib0(maxAllowedCount)) {
        return false;
    }
    BindFakeBlackTextures();

    return true;
}

void
WebGLContext::DrawElements(WebGLenum mode, WebGLsizei count, WebGLenum type,
                               WebGLintptr byteOffset)
{
    if (!IsContextStable())
        return;

    if (!ValidateDrawModeEnum(mode, "drawElements: mode"))
        return;

    if (!DrawElements_check(count, type, byteOffset, 1, "drawElements"))
        return;

    SetupContextLossTimer();
    gl->fDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(byteOffset));

    Draw_cleanup();
}

void
WebGLContext::DrawElementsInstanced(WebGLenum mode, WebGLsizei count, WebGLenum type,
                                        WebGLintptr byteOffset, WebGLsizei primcount)
{
    if (!IsContextStable())
        return;

    if (!ValidateDrawModeEnum(mode, "drawElementsInstanced: mode"))
        return;

    if (!DrawElements_check(count, type, byteOffset, 1, "drawElementsInstanced"))
        return;

    SetupContextLossTimer();
    gl->fDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(byteOffset));

    Draw_cleanup();
}

void WebGLContext::Draw_cleanup()
{
    UndoFakeVertexAttrib0();
    UnbindFakeBlackTextures();

    if (!mBoundFramebuffer) {
        Invalidate();
        mShouldPresent = true;
        mIsScreenCleared = false;
    }

    if (gl->WorkAroundDriverBugs()) {
        if (gl->Renderer() == gl::GLContext::RendererTegra) {
            mDrawCallsSinceLastFlush++;
            
            if (mDrawCallsSinceLastFlush >= MAX_DRAW_CALLS_SINCE_FLUSH) {
                gl->fFlush();
                mDrawCallsSinceLastFlush = 0;
            }
        }
    }
}

