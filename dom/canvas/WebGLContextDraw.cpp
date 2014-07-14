/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include "GLContext.h"
#include "mozilla/CheckedInt.h"
#include "WebGLBuffer.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShader.h"
#include "WebGLTexture.h"
#include "WebGLUniformInfo.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gl;

// For a Tegra workaround.
static const int MAX_DRAW_CALLS_SINCE_FLUSH = 100;

bool
WebGLContext::DrawInstanced_check(const char* info)
{
    // This restriction was removed in GLES3, so WebGL2 shouldn't have it.
    if (!IsWebGL2() &&
        IsExtensionEnabled(WebGLExtensionID::ANGLE_instanced_arrays) &&
        !mBufferFetchingHasPerVertex)
    {
        /* http://www.khronos.org/registry/gles/extensions/ANGLE/ANGLE_instanced_arrays.txt
         *  If all of the enabled vertex attribute arrays that are bound to active
         *  generic attributes in the program have a non-zero divisor, the draw
         *  call should return INVALID_OPERATION.
         *
         * NB: This also appears to apply to NV_instanced_arrays, though the
         * INVALID_OPERATION emission is not explicitly stated.
         * ARB_instanced_arrays does not have this restriction.
         */
        ErrorInvalidOperation("%s: at least one vertex attribute divisor should be 0", info);
        return false;
    }

    return true;
}

bool WebGLContext::DrawArrays_check(GLint first, GLsizei count, GLsizei primcount, const char* info)
{
    if (first < 0 || count < 0) {
        ErrorInvalidValue("%s: negative first or count", info);
        return false;
    }

    if (primcount < 0) {
        ErrorInvalidValue("%s: negative primcount", info);
        return false;
    }

    if (!ValidateStencilParamsForDrawCall()) {
        return false;
    }

    // If count is 0, there's nothing to do.
    if (count == 0 || primcount == 0) {
        return false;
    }

    // Any checks below this depend on a program being available.
    if (!mCurrentProgram) {
        ErrorInvalidOperation("%s: null CURRENT_PROGRAM", info);
        return false;
    }

    if (!ValidateBufferFetching(info)) {
        return false;
    }

    CheckedInt<GLsizei> checked_firstPlusCount = CheckedInt<GLsizei>(first) + count;

    if (!checked_firstPlusCount.isValid()) {
        ErrorInvalidOperation("%s: overflow in first+count", info);
        return false;
    }

    if (uint32_t(checked_firstPlusCount.value()) > mMaxFetchedVertices) {
        ErrorInvalidOperation("%s: bound vertex attribute buffers do not have sufficient size for given first and count", info);
        return false;
    }

    if (uint32_t(primcount) > mMaxFetchedInstances) {
        ErrorInvalidOperation("%s: bound instance attribute buffers do not have sufficient size for given primcount", info);
        return false;
    }

    MakeContextCurrent();

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments()) {
            ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
            return false;
        }
    } else {
        ClearBackbufferIfNeeded();
    }

    if (!DoFakeVertexAttrib0(checked_firstPlusCount.value())) {
        return false;
    }

    if (!DrawInstanced_check(info)) {
        return false;
    }

    BindFakeBlackTextures();

    return true;
}

void
WebGLContext::DrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawArrays: mode"))
        return;

    if (!DrawArrays_check(first, count, 1, "drawArrays"))
        return;

    RunContextLossTimer();

    {
        ScopedMaskWorkaround autoMask(*this);
        gl->fDrawArrays(mode, first, count);
    }

    Draw_cleanup();
}

void
WebGLContext::DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei primcount)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawArraysInstanced: mode"))
        return;

    if (!DrawArrays_check(first, count, primcount, "drawArraysInstanced"))
        return;

    RunContextLossTimer();

    {
        ScopedMaskWorkaround autoMask(*this);
        gl->fDrawArraysInstanced(mode, first, count, primcount);
    }

    Draw_cleanup();
}

bool
WebGLContext::DrawElements_check(GLsizei count, GLenum type,
                                 WebGLintptr byteOffset, GLsizei primcount,
                                 const char* info, GLuint* out_upperBound)
{
    if (count < 0 || byteOffset < 0) {
        ErrorInvalidValue("%s: negative count or offset", info);
        return false;
    }

    if (primcount < 0) {
        ErrorInvalidValue("%s: negative primcount", info);
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

    GLsizei first = 0;

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
    else if (type == LOCAL_GL_UNSIGNED_INT && IsExtensionEnabled(WebGLExtensionID::OES_element_index_uint)) {
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

    // Any checks below this depend on a program being available.
    if (!mCurrentProgram) {
        ErrorInvalidOperation("%s: null CURRENT_PROGRAM", info);
        return false;
    }

    if (!mBoundVertexArray->mElementArrayBuffer) {
        ErrorInvalidOperation("%s: must have element array buffer binding", info);
        return false;
    }

    WebGLBuffer& elemArrayBuffer = *mBoundVertexArray->mElementArrayBuffer;

    if (!elemArrayBuffer.ByteLength()) {
        ErrorInvalidOperation("%s: bound element array buffer doesn't have any data", info);
        return false;
    }

    CheckedInt<GLsizei> checked_neededByteCount = checked_byteCount.toChecked<GLsizei>() + byteOffset;

    if (!checked_neededByteCount.isValid()) {
        ErrorInvalidOperation("%s: overflow in byteOffset+byteCount", info);
        return false;
    }

    if (uint32_t(checked_neededByteCount.value()) > elemArrayBuffer.ByteLength()) {
        ErrorInvalidOperation("%s: bound element array buffer is too small for given count and offset", info);
        return false;
    }

    if (!ValidateBufferFetching(info))
        return false;

    if (!mMaxFetchedVertices ||
        !elemArrayBuffer.Validate(type, mMaxFetchedVertices - 1, first, count, out_upperBound))
    {
        ErrorInvalidOperation(
                              "%s: bound vertex attribute buffers do not have sufficient "
                              "size for given indices from the bound element array", info);
        return false;
    }

    if (uint32_t(primcount) > mMaxFetchedInstances) {
        ErrorInvalidOperation("%s: bound instance attribute buffers do not have sufficient size for given primcount", info);
        return false;
    }

    // Bug 1008310 - Check if buffer has been used with a different previous type
    if (elemArrayBuffer.IsElementArrayUsedWithMultipleTypes()) {
        GenerateWarning("%s: bound element array buffer previously used with a type other than "
                        "%s, this will affect performance.",
                        info,
                        WebGLContext::EnumName(type));
    }

    MakeContextCurrent();

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments()) {
            ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
            return false;
        }
    } else {
        ClearBackbufferIfNeeded();
    }

    if (!DoFakeVertexAttrib0(mMaxFetchedVertices)) {
        return false;
    }

    if (!DrawInstanced_check(info)) {
        return false;
    }

    BindFakeBlackTextures();

    return true;
}

void
WebGLContext::DrawElements(GLenum mode, GLsizei count, GLenum type,
                           WebGLintptr byteOffset)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawElements: mode"))
        return;

    GLuint upperBound = 0;
    if (!DrawElements_check(count, type, byteOffset, 1, "drawElements",
                            &upperBound))
    {
        return;
    }

    RunContextLossTimer();

    {
        ScopedMaskWorkaround autoMask(*this);

        if (gl->IsSupported(gl::GLFeature::draw_range_elements)) {
            gl->fDrawRangeElements(mode, 0, upperBound, count, type,
                                   reinterpret_cast<GLvoid*>(byteOffset));
        } else {
            gl->fDrawElements(mode, count, type,
                              reinterpret_cast<GLvoid*>(byteOffset));
        }
    }

    Draw_cleanup();
}

void
WebGLContext::DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                                    WebGLintptr byteOffset, GLsizei primcount)
{
    if (IsContextLost())
        return;

    if (!ValidateDrawModeEnum(mode, "drawElementsInstanced: mode"))
        return;

    GLuint upperBound = 0;
    if (!DrawElements_check(count, type, byteOffset, primcount,
                            "drawElementsInstanced", &upperBound))
    {
        return;
    }

    RunContextLossTimer();

    {
        ScopedMaskWorkaround autoMask(*this);
        gl->fDrawElementsInstanced(mode, count, type,
                                   reinterpret_cast<GLvoid*>(byteOffset),
                                   primcount);
    }

    Draw_cleanup();
}

void WebGLContext::Draw_cleanup()
{
    UndoFakeVertexAttrib0();
    UnbindFakeBlackTextures();

    if (!mBoundFramebuffer) {
        Invalidate();
        mShouldPresent = true;
        MOZ_ASSERT(!mBackbufferNeedsClear);
    }

    if (gl->WorkAroundDriverBugs()) {
        if (gl->Renderer() == gl::GLRenderer::Tegra) {
            mDrawCallsSinceLastFlush++;

            if (mDrawCallsSinceLastFlush >= MAX_DRAW_CALLS_SINCE_FLUSH) {
                gl->fFlush();
                mDrawCallsSinceLastFlush = 0;
            }
        }
    }

    // Let's check the viewport
    const WebGLRectangleObject* rect = CurValidFBRectObject();
    if (rect) {
        if (mViewportWidth > rect->Width() ||
            mViewportHeight > rect->Height())
        {
            if (!mAlreadyWarnedAboutViewportLargerThanDest) {
                GenerateWarning("Drawing to a destination rect smaller than the viewport rect. "
                                "(This warning will only be given once)");
                mAlreadyWarnedAboutViewportLargerThanDest = true;
            }
        }
    }
}

/*
 * Verify that state is consistent for drawing, and compute max number of elements (maxAllowedCount)
 * that will be legal to be read from bound VBOs.
 */

bool
WebGLContext::ValidateBufferFetching(const char *info)
{
#ifdef DEBUG
    GLint currentProgram = 0;
    MakeContextCurrent();
    gl->fGetIntegerv(LOCAL_GL_CURRENT_PROGRAM, &currentProgram);
    MOZ_ASSERT(GLuint(currentProgram) == mCurrentProgram->GLName(),
               "WebGL: current program doesn't agree with GL state");
#endif

    if (mBufferFetchingIsVerified) {
        return true;
    }

    bool hasPerVertex = false;
    uint32_t maxVertices = UINT32_MAX;
    uint32_t maxInstances = UINT32_MAX;
    uint32_t attribs = mBoundVertexArray->mAttribs.Length();

    for (uint32_t i = 0; i < attribs; ++i) {
        const WebGLVertexAttribData& vd = mBoundVertexArray->mAttribs[i];

        // If the attrib array isn't enabled, there's nothing to check;
        // it's a static value.
        if (!vd.enabled)
            continue;

        if (vd.buf == nullptr) {
            ErrorInvalidOperation("%s: no VBO bound to enabled vertex attrib index %d!", info, i);
            return false;
        }

        // If the attrib is not in use, then we don't have to validate
        // it, just need to make sure that the binding is non-null.
        if (!mCurrentProgram->IsAttribInUse(i))
            continue;

        // the base offset
        CheckedUint32 checked_byteLength = CheckedUint32(vd.buf->ByteLength()) - vd.byteOffset;
        CheckedUint32 checked_sizeOfLastElement = CheckedUint32(vd.componentSize()) * vd.size;

        if (!checked_byteLength.isValid() ||
            !checked_sizeOfLastElement.isValid())
        {
            ErrorInvalidOperation("%s: integer overflow occured while checking vertex attrib %d", info, i);
            return false;
        }

        if (checked_byteLength.value() < checked_sizeOfLastElement.value()) {
            maxVertices = 0;
            maxInstances = 0;
            break;
        }

        CheckedUint32 checked_maxAllowedCount = ((checked_byteLength - checked_sizeOfLastElement) / vd.actualStride()) + 1;

        if (!checked_maxAllowedCount.isValid()) {
            ErrorInvalidOperation("%s: integer overflow occured while checking vertex attrib %d", info, i);
            return false;
        }

        if (vd.divisor == 0) {
            maxVertices = std::min(maxVertices, checked_maxAllowedCount.value());
            hasPerVertex = true;
        } else {
            CheckedUint32 checked_curMaxInstances = checked_maxAllowedCount * vd.divisor;

            uint32_t curMaxInstances = UINT32_MAX;
            // If this isn't valid, it's because we overflowed our
            // uint32 above. Just leave this as UINT32_MAX, since
            // sizeof(uint32) becomes our limiting factor.
            if (checked_curMaxInstances.isValid()) {
                curMaxInstances = checked_curMaxInstances.value();
            }

            maxInstances = std::min(maxInstances, curMaxInstances);
        }
    }

    mBufferFetchingIsVerified = true;
    mBufferFetchingHasPerVertex = hasPerVertex;
    mMaxFetchedVertices = maxVertices;
    mMaxFetchedInstances = maxInstances;

    return true;
}

WebGLVertexAttrib0Status
WebGLContext::WhatDoesVertexAttrib0Need()
{
    MOZ_ASSERT(mCurrentProgram);

    // work around Mac OSX crash, see bug 631420
#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs() &&
        mBoundVertexArray->IsAttribArrayEnabled(0) &&
        !mCurrentProgram->IsAttribInUse(0))
    {
        return WebGLVertexAttrib0Status::EmulatedUninitializedArray;
    }
#endif

    if (MOZ_LIKELY(gl->IsGLES() ||
                   mBoundVertexArray->IsAttribArrayEnabled(0)))
    {
        return WebGLVertexAttrib0Status::Default;
    }

    return mCurrentProgram->IsAttribInUse(0)
           ? WebGLVertexAttrib0Status::EmulatedInitializedArray
           : WebGLVertexAttrib0Status::EmulatedUninitializedArray;
}

bool
WebGLContext::DoFakeVertexAttrib0(GLuint vertexCount)
{
    WebGLVertexAttrib0Status whatDoesAttrib0Need = WhatDoesVertexAttrib0Need();

    if (MOZ_LIKELY(whatDoesAttrib0Need == WebGLVertexAttrib0Status::Default))
        return true;

    if (!mAlreadyWarnedAboutFakeVertexAttrib0) {
        GenerateWarning("Drawing without vertex attrib 0 array enabled forces the browser "
                        "to do expensive emulation work when running on desktop OpenGL "
                        "platforms, for example on Mac. It is preferable to always draw "
                        "with vertex attrib 0 array enabled, by using bindAttribLocation "
                        "to bind some always-used attribute to location 0.");
        mAlreadyWarnedAboutFakeVertexAttrib0 = true;
    }

    CheckedUint32 checked_dataSize = CheckedUint32(vertexCount) * 4 * sizeof(GLfloat);

    if (!checked_dataSize.isValid()) {
        ErrorOutOfMemory("Integer overflow trying to construct a fake vertex attrib 0 array for a draw-operation "
                         "with %d vertices. Try reducing the number of vertices.", vertexCount);
        return false;
    }

    GLuint dataSize = checked_dataSize.value();

    if (!mFakeVertexAttrib0BufferObject) {
        gl->fGenBuffers(1, &mFakeVertexAttrib0BufferObject);
    }

    // if the VBO status is already exactly what we need, or if the only difference is that it's initialized and
    // we don't need it to be, then consider it OK
    bool vertexAttrib0BufferStatusOK =
        mFakeVertexAttrib0BufferStatus == whatDoesAttrib0Need ||
        (mFakeVertexAttrib0BufferStatus == WebGLVertexAttrib0Status::EmulatedInitializedArray &&
         whatDoesAttrib0Need == WebGLVertexAttrib0Status::EmulatedUninitializedArray);

    if (!vertexAttrib0BufferStatusOK ||
        mFakeVertexAttrib0BufferObjectSize < dataSize ||
        mFakeVertexAttrib0BufferObjectVector[0] != mVertexAttrib0Vector[0] ||
        mFakeVertexAttrib0BufferObjectVector[1] != mVertexAttrib0Vector[1] ||
        mFakeVertexAttrib0BufferObjectVector[2] != mVertexAttrib0Vector[2] ||
        mFakeVertexAttrib0BufferObjectVector[3] != mVertexAttrib0Vector[3])
    {
        mFakeVertexAttrib0BufferStatus = whatDoesAttrib0Need;
        mFakeVertexAttrib0BufferObjectSize = dataSize;
        mFakeVertexAttrib0BufferObjectVector[0] = mVertexAttrib0Vector[0];
        mFakeVertexAttrib0BufferObjectVector[1] = mVertexAttrib0Vector[1];
        mFakeVertexAttrib0BufferObjectVector[2] = mVertexAttrib0Vector[2];
        mFakeVertexAttrib0BufferObjectVector[3] = mVertexAttrib0Vector[3];

        gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mFakeVertexAttrib0BufferObject);

        GetAndFlushUnderlyingGLErrors();

        if (mFakeVertexAttrib0BufferStatus == WebGLVertexAttrib0Status::EmulatedInitializedArray) {
            nsAutoArrayPtr<GLfloat> array(new GLfloat[4 * vertexCount]);
            for(size_t i = 0; i < vertexCount; ++i) {
                array[4 * i + 0] = mVertexAttrib0Vector[0];
                array[4 * i + 1] = mVertexAttrib0Vector[1];
                array[4 * i + 2] = mVertexAttrib0Vector[2];
                array[4 * i + 3] = mVertexAttrib0Vector[3];
            }
            gl->fBufferData(LOCAL_GL_ARRAY_BUFFER, dataSize, array, LOCAL_GL_DYNAMIC_DRAW);
        } else {
            gl->fBufferData(LOCAL_GL_ARRAY_BUFFER, dataSize, nullptr, LOCAL_GL_DYNAMIC_DRAW);
        }
        GLenum error = GetAndFlushUnderlyingGLErrors();

        gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mBoundArrayBuffer ? mBoundArrayBuffer->GLName() : 0);

        // note that we do this error checking and early return AFTER having restored the buffer binding above
        if (error) {
            ErrorOutOfMemory("Ran out of memory trying to construct a fake vertex attrib 0 array for a draw-operation "
                             "with %d vertices. Try reducing the number of vertices.", vertexCount);
            return false;
        }
    }

    gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mFakeVertexAttrib0BufferObject);
    gl->fVertexAttribPointer(0, 4, LOCAL_GL_FLOAT, LOCAL_GL_FALSE, 0, 0);

    return true;
}

void
WebGLContext::UndoFakeVertexAttrib0()
{
    WebGLVertexAttrib0Status whatDoesAttrib0Need = WhatDoesVertexAttrib0Need();

    if (MOZ_LIKELY(whatDoesAttrib0Need == WebGLVertexAttrib0Status::Default))
        return;

    if (mBoundVertexArray->HasAttrib(0) && mBoundVertexArray->mAttribs[0].buf) {
        const WebGLVertexAttribData& attrib0 = mBoundVertexArray->mAttribs[0];
        gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, attrib0.buf->GLName());
        gl->fVertexAttribPointer(0,
                                 attrib0.size,
                                 attrib0.type,
                                 attrib0.normalized,
                                 attrib0.stride,
                                 reinterpret_cast<const GLvoid *>(attrib0.byteOffset));
    } else {
        gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, 0);
    }

    gl->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mBoundArrayBuffer ? mBoundArrayBuffer->GLName() : 0);
}

WebGLContextFakeBlackStatus
WebGLContext::ResolvedFakeBlackStatus()
{
    // handle this case first, it's the generic case
    if (MOZ_LIKELY(mFakeBlackStatus == WebGLContextFakeBlackStatus::NotNeeded))
        return mFakeBlackStatus;

    if (mFakeBlackStatus == WebGLContextFakeBlackStatus::Needed)
        return mFakeBlackStatus;

    for (int32_t i = 0; i < mGLMaxTextureUnits; ++i) {
        if ((mBound2DTextures[i] && mBound2DTextures[i]->ResolvedFakeBlackStatus() != WebGLTextureFakeBlackStatus::NotNeeded) ||
            (mBoundCubeMapTextures[i] && mBoundCubeMapTextures[i]->ResolvedFakeBlackStatus() != WebGLTextureFakeBlackStatus::NotNeeded))
        {
            mFakeBlackStatus = WebGLContextFakeBlackStatus::Needed;
            return mFakeBlackStatus;
        }
    }

    // we have exhausted all cases where we do need fakeblack, so if the status is still unknown,
    // that means that we do NOT need it.
    mFakeBlackStatus = WebGLContextFakeBlackStatus::NotNeeded;
    return mFakeBlackStatus;
}

void
WebGLContext::BindFakeBlackTexturesHelper(
    GLenum target,
    const nsTArray<WebGLRefPtr<WebGLTexture> > & boundTexturesArray,
    UniquePtr<FakeBlackTexture> & opaqueTextureScopedPtr,
    UniquePtr<FakeBlackTexture> & transparentTextureScopedPtr)
{
    for (int32_t i = 0; i < mGLMaxTextureUnits; ++i) {
        if (!boundTexturesArray[i]) {
            continue;
        }

        WebGLTextureFakeBlackStatus s = boundTexturesArray[i]->ResolvedFakeBlackStatus();
        MOZ_ASSERT(s != WebGLTextureFakeBlackStatus::Unknown);

        if (MOZ_LIKELY(s == WebGLTextureFakeBlackStatus::NotNeeded)) {
            continue;
        }

        bool alpha = s == WebGLTextureFakeBlackStatus::UninitializedImageData &&
                     FormatHasAlpha(boundTexturesArray[i]->ImageInfoBase().WebGLInternalFormat());
        UniquePtr<FakeBlackTexture>&
            blackTexturePtr = alpha
                              ? transparentTextureScopedPtr
                              : opaqueTextureScopedPtr;

        if (!blackTexturePtr) {
            GLenum format = alpha ? LOCAL_GL_RGBA : LOCAL_GL_RGB;
            blackTexturePtr = MakeUnique<FakeBlackTexture>(gl, target, format);
        }

        gl->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
        gl->fBindTexture(target,
                         blackTexturePtr->GLName());
    }
}

void
WebGLContext::BindFakeBlackTextures()
{
    // this is the generic case: try to return early
    if (MOZ_LIKELY(ResolvedFakeBlackStatus() == WebGLContextFakeBlackStatus::NotNeeded))
        return;

    BindFakeBlackTexturesHelper(LOCAL_GL_TEXTURE_2D,
                                mBound2DTextures,
                                mBlackOpaqueTexture2D,
                                mBlackTransparentTexture2D);
    BindFakeBlackTexturesHelper(LOCAL_GL_TEXTURE_CUBE_MAP,
                                mBoundCubeMapTextures,
                                mBlackOpaqueTextureCubeMap,
                                mBlackTransparentTextureCubeMap);
}

void
WebGLContext::UnbindFakeBlackTextures()
{
    // this is the generic case: try to return early
    if (MOZ_LIKELY(ResolvedFakeBlackStatus() == WebGLContextFakeBlackStatus::NotNeeded))
        return;

    for (int32_t i = 0; i < mGLMaxTextureUnits; ++i) {
        if (mBound2DTextures[i] && mBound2DTextures[i]->ResolvedFakeBlackStatus() != WebGLTextureFakeBlackStatus::NotNeeded) {
            gl->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
            gl->fBindTexture(LOCAL_GL_TEXTURE_2D, mBound2DTextures[i]->GLName());
        }
        if (mBoundCubeMapTextures[i] && mBoundCubeMapTextures[i]->ResolvedFakeBlackStatus() != WebGLTextureFakeBlackStatus::NotNeeded) {
            gl->fActiveTexture(LOCAL_GL_TEXTURE0 + i);
            gl->fBindTexture(LOCAL_GL_TEXTURE_CUBE_MAP, mBoundCubeMapTextures[i]->GLName());
        }
    }

    gl->fActiveTexture(LOCAL_GL_TEXTURE0 + mActiveTexture);
}

WebGLContext::FakeBlackTexture::FakeBlackTexture(GLContext *gl, TexTarget target, GLenum format)
    : mGL(gl)
    , mGLName(0)
{
  MOZ_ASSERT(format == LOCAL_GL_RGB || format == LOCAL_GL_RGBA);

  mGL->MakeCurrent();
  GLuint formerBinding = 0;
  gl->GetUIntegerv(target == LOCAL_GL_TEXTURE_2D
                   ? LOCAL_GL_TEXTURE_BINDING_2D
                   : LOCAL_GL_TEXTURE_BINDING_CUBE_MAP,
                   &formerBinding);
  gl->fGenTextures(1, &mGLName);
  gl->fBindTexture(target.get(), mGLName);

  // we allocate our zeros on the heap, and we overallocate (16 bytes instead of 4)
  // to minimize the risk of running into a driver bug in texImage2D, as it is
  // a bit unusual maybe to create 1x1 textures, and the stack may not have the alignment
  // that texImage2D expects.
  void* zeros = calloc(1, 16);
  if (target == LOCAL_GL_TEXTURE_2D) {
      gl->fTexImage2D(target.get(), 0, format, 1, 1,
                      0, format, LOCAL_GL_UNSIGNED_BYTE, zeros);
  } else {
      for (GLuint i = 0; i < 6; ++i) {
          gl->fTexImage2D(LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, 1, 1,
                          0, format, LOCAL_GL_UNSIGNED_BYTE, zeros);
      }
  }
  free(zeros);

  gl->fBindTexture(target.get(), formerBinding);
}

WebGLContext::FakeBlackTexture::~FakeBlackTexture()
{
  if (mGL) {
      mGL->MakeCurrent();
      mGL->fDeleteTextures(1, &mGLName);
  }
}
