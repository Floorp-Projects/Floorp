/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/UniquePtr.h"

#include "GLContext.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

#ifdef DEBUG
bool
IsContextCurrent(GLContext* gl)
{
    return gl->IsCurrent();
}
#endif

/* ScopedGLState - Wraps glEnable/glDisable. **********************************/

// Use |newState = true| to enable, |false| to disable.
ScopedGLState::ScopedGLState(GLContext* aGL, GLenum aCapability, bool aNewState)
    : ScopedGLWrapper<ScopedGLState>(aGL)
    , mCapability(aCapability)
{
    mOldState = mGL->fIsEnabled(mCapability);

    // Early out if we're already in the right state.
    if (aNewState == mOldState)
        return;

    if (aNewState) {
        mGL->fEnable(mCapability);
    } else {
        mGL->fDisable(mCapability);
    }
}

ScopedGLState::ScopedGLState(GLContext* aGL, GLenum aCapability)
    : ScopedGLWrapper<ScopedGLState>(aGL)
    , mCapability(aCapability)
{
    mOldState = mGL->fIsEnabled(mCapability);
}

void
ScopedGLState::UnwrapImpl()
{
    if (mOldState) {
        mGL->fEnable(mCapability);
    } else {
        mGL->fDisable(mCapability);
    }
}


/* ScopedBindFramebuffer - Saves and restores with GetUserBoundFB and BindUserFB. */

void
ScopedBindFramebuffer::Init()
{
    if (mGL->IsSupported(GLFeature::split_framebuffer)) {
        mOldReadFB = mGL->GetReadFB();
        mOldDrawFB = mGL->GetDrawFB();
    } else {
        mOldReadFB = mOldDrawFB = mGL->GetFB();
    }
}

ScopedBindFramebuffer::ScopedBindFramebuffer(GLContext* aGL)
    : ScopedGLWrapper<ScopedBindFramebuffer>(aGL)
{
    Init();
}

ScopedBindFramebuffer::ScopedBindFramebuffer(GLContext* aGL, GLuint aNewFB)
    : ScopedGLWrapper<ScopedBindFramebuffer>(aGL)
{
    Init();
    mGL->BindFB(aNewFB);
}

void
ScopedBindFramebuffer::UnwrapImpl()
{
    // Check that we're not falling out of scope after the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());

    if (mOldReadFB == mOldDrawFB) {
        mGL->BindFB(mOldDrawFB);
    } else {
        mGL->BindDrawFB(mOldDrawFB);
        mGL->BindReadFB(mOldReadFB);
    }
}


/* ScopedBindTextureUnit ******************************************************/

ScopedBindTextureUnit::ScopedBindTextureUnit(GLContext* aGL, GLenum aTexUnit)
    : ScopedGLWrapper<ScopedBindTextureUnit>(aGL)
{
    MOZ_ASSERT(aTexUnit >= LOCAL_GL_TEXTURE0);
    mGL->GetUIntegerv(LOCAL_GL_ACTIVE_TEXTURE, &mOldTexUnit);
    mGL->fActiveTexture(aTexUnit);
}

void
ScopedBindTextureUnit::UnwrapImpl() {
    // Check that we're not falling out of scope after the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());

    mGL->fActiveTexture(mOldTexUnit);
}


/* ScopedTexture **************************************************************/

ScopedTexture::ScopedTexture(GLContext* aGL)
    : ScopedGLWrapper<ScopedTexture>(aGL)
{
    MOZ_ASSERT(mGL->IsCurrent());
    mGL->fGenTextures(1, &mTexture);
}

void
ScopedTexture::UnwrapImpl()
{
    // Check that we're not falling out of scope after
    // the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());
    mGL->fDeleteTextures(1, &mTexture);
}


/* ScopedFramebuffer **************************************************************/

ScopedFramebuffer::ScopedFramebuffer(GLContext* aGL)
    : ScopedGLWrapper<ScopedFramebuffer>(aGL)
{
    MOZ_ASSERT(mGL->IsCurrent());
    mGL->fGenFramebuffers(1, &mFB);
}

void
ScopedFramebuffer::UnwrapImpl()
{
    // Check that we're not falling out of scope after
    // the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());
    mGL->fDeleteFramebuffers(1, &mFB);
}


/* ScopedRenderbuffer **************************************************************/

ScopedRenderbuffer::ScopedRenderbuffer(GLContext* aGL)
    : ScopedGLWrapper<ScopedRenderbuffer>(aGL)
{
    MOZ_ASSERT(mGL->IsCurrent());
    mGL->fGenRenderbuffers(1, &mRB);
}

void
ScopedRenderbuffer::UnwrapImpl()
{
    // Check that we're not falling out of scope after
    // the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());
    mGL->fDeleteRenderbuffers(1, &mRB);
}

/* ScopedBindTexture **********************************************************/

static GLuint
GetBoundTexture(GLContext* gl, GLenum texTarget)
{
    GLenum bindingTarget;
    switch (texTarget) {
    case LOCAL_GL_TEXTURE_2D:
        bindingTarget = LOCAL_GL_TEXTURE_BINDING_2D;
        break;

    case LOCAL_GL_TEXTURE_CUBE_MAP:
        bindingTarget = LOCAL_GL_TEXTURE_BINDING_CUBE_MAP;
        break;

    case LOCAL_GL_TEXTURE_3D:
        bindingTarget = LOCAL_GL_TEXTURE_BINDING_3D;
        break;

    case LOCAL_GL_TEXTURE_2D_ARRAY:
        bindingTarget = LOCAL_GL_TEXTURE_BINDING_2D_ARRAY;
        break;

    case LOCAL_GL_TEXTURE_RECTANGLE_ARB:
        bindingTarget = LOCAL_GL_TEXTURE_BINDING_RECTANGLE_ARB;
        break;

    case LOCAL_GL_TEXTURE_EXTERNAL:
        bindingTarget = LOCAL_GL_TEXTURE_BINDING_EXTERNAL;
        break;

    default:
        MOZ_CRASH("bad texTarget");
    }

    GLuint ret = 0;
    gl->GetUIntegerv(bindingTarget, &ret);
    return ret;
}

ScopedBindTexture::ScopedBindTexture(GLContext* aGL, GLuint aNewTex, GLenum aTarget)
        : ScopedGLWrapper<ScopedBindTexture>(aGL)
        , mTarget(aTarget)
        , mOldTex(GetBoundTexture(aGL, aTarget))
{
    mGL->fBindTexture(mTarget, aNewTex);
}

void
ScopedBindTexture::UnwrapImpl()
{
    mGL->fBindTexture(mTarget, mOldTex);
}


/* ScopedBindRenderbuffer *****************************************************/

void
ScopedBindRenderbuffer::Init()
{
    mOldRB = 0;
    mGL->GetUIntegerv(LOCAL_GL_RENDERBUFFER_BINDING, &mOldRB);
}

ScopedBindRenderbuffer::ScopedBindRenderbuffer(GLContext* aGL)
        : ScopedGLWrapper<ScopedBindRenderbuffer>(aGL)
{
    Init();
}

ScopedBindRenderbuffer::ScopedBindRenderbuffer(GLContext* aGL, GLuint aNewRB)
    : ScopedGLWrapper<ScopedBindRenderbuffer>(aGL)
{
    Init();
    mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, aNewRB);
}

void
ScopedBindRenderbuffer::UnwrapImpl() {
    // Check that we're not falling out of scope after the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());

    mGL->fBindRenderbuffer(LOCAL_GL_RENDERBUFFER, mOldRB);
}


/* ScopedFramebufferForTexture ************************************************/
ScopedFramebufferForTexture::ScopedFramebufferForTexture(GLContext* aGL,
                                                         GLuint aTexture,
                                                         GLenum aTarget)
    : ScopedGLWrapper<ScopedFramebufferForTexture>(aGL)
    , mComplete(false)
    , mFB(0)
{
    mGL->fGenFramebuffers(1, &mFB);
    ScopedBindFramebuffer autoFB(aGL, mFB);
    mGL->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                               LOCAL_GL_COLOR_ATTACHMENT0,
                               aTarget,
                               aTexture,
                               0);

    GLenum status = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status == LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        mComplete = true;
    } else {
        mGL->fDeleteFramebuffers(1, &mFB);
        mFB = 0;
    }
}

void ScopedFramebufferForTexture::UnwrapImpl()
{
    if (!mFB)
        return;

    mGL->fDeleteFramebuffers(1, &mFB);
    mFB = 0;
}


/* ScopedFramebufferForRenderbuffer *******************************************/


ScopedFramebufferForRenderbuffer::ScopedFramebufferForRenderbuffer(GLContext* aGL,
                                                                   GLuint aRB)
    : ScopedGLWrapper<ScopedFramebufferForRenderbuffer>(aGL)
    , mComplete(false)
    , mFB(0)
{
    mGL->fGenFramebuffers(1, &mFB);
    ScopedBindFramebuffer autoFB(aGL, mFB);
    mGL->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                  LOCAL_GL_COLOR_ATTACHMENT0,
                                  LOCAL_GL_RENDERBUFFER,
                                  aRB);

    GLenum status = mGL->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status == LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        mComplete = true;
    } else {
        mGL->fDeleteFramebuffers(1, &mFB);
        mFB = 0;
    }
}

void
ScopedFramebufferForRenderbuffer::UnwrapImpl()
{
    if (!mFB)
        return;

    mGL->fDeleteFramebuffers(1, &mFB);
    mFB = 0;
}

/* ScopedViewportRect *********************************************************/

ScopedViewportRect::ScopedViewportRect(GLContext* aGL,
                                       GLint x, GLint y,
                                       GLsizei width, GLsizei height)
  : ScopedGLWrapper<ScopedViewportRect>(aGL)
{
    mGL->fGetIntegerv(LOCAL_GL_VIEWPORT, mSavedViewportRect);
    mGL->fViewport(x, y, width, height);
}

void ScopedViewportRect::UnwrapImpl()
{
    mGL->fViewport(mSavedViewportRect[0],
                   mSavedViewportRect[1],
                   mSavedViewportRect[2],
                   mSavedViewportRect[3]);
}

/* ScopedScissorRect **********************************************************/

ScopedScissorRect::ScopedScissorRect(GLContext* aGL,
                                     GLint x, GLint y,
                                     GLsizei width, GLsizei height)
  : ScopedGLWrapper<ScopedScissorRect>(aGL)
{
    mGL->fGetIntegerv(LOCAL_GL_SCISSOR_BOX, mSavedScissorRect);
    mGL->fScissor(x, y, width, height);
}

ScopedScissorRect::ScopedScissorRect(GLContext* aGL)
  : ScopedGLWrapper<ScopedScissorRect>(aGL)
{
    mGL->fGetIntegerv(LOCAL_GL_SCISSOR_BOX, mSavedScissorRect);
}

void ScopedScissorRect::UnwrapImpl()
{
    mGL->fScissor(mSavedScissorRect[0],
                  mSavedScissorRect[1],
                  mSavedScissorRect[2],
                  mSavedScissorRect[3]);
}

/* ScopedVertexAttribPointer **************************************************/

ScopedVertexAttribPointer::ScopedVertexAttribPointer(GLContext* aGL,
                                                     GLuint index,
                                                     GLint size,
                                                     GLenum type,
                                                     realGLboolean normalized,
                                                     GLsizei stride,
                                                     GLuint buffer,
                                                     const GLvoid* pointer)
    : ScopedGLWrapper<ScopedVertexAttribPointer>(aGL)
{
    WrapImpl(index);
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, buffer);
    mGL->fVertexAttribPointer(index, size, type, normalized, stride, pointer);
    mGL->fEnableVertexAttribArray(index);
}

ScopedVertexAttribPointer::ScopedVertexAttribPointer(GLContext* aGL,
                                                     GLuint index)
    : ScopedGLWrapper<ScopedVertexAttribPointer>(aGL)
{
    WrapImpl(index);
}

void
ScopedVertexAttribPointer::WrapImpl(GLuint index)
{
    mAttribIndex = index;

    /*
     * mGL->fGetVertexAttribiv takes:
     *  VERTEX_ATTRIB_ARRAY_ENABLED
     *  VERTEX_ATTRIB_ARRAY_SIZE,
     *  VERTEX_ATTRIB_ARRAY_STRIDE,
     *  VERTEX_ATTRIB_ARRAY_TYPE,
     *  VERTEX_ATTRIB_ARRAY_NORMALIZED,
     *  VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
     *  CURRENT_VERTEX_ATTRIB
     *
     * CURRENT_VERTEX_ATTRIB is vertex shader state. \o/
     * Others appear to be vertex array state,
     * or alternatively in the internal vertex array state
     * for a buffer object.
    */

    mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED, &mAttribEnabled);
    mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE, &mAttribSize);
    mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE, &mAttribStride);
    mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE, &mAttribType);
    mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &mAttribNormalized);
    mGL->fGetVertexAttribiv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &mAttribBufferBinding);
    mGL->fGetVertexAttribPointerv(mAttribIndex, LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER, &mAttribPointer);

    // Note that uniform values are program state, so we don't need to rebind those.

    mGL->GetUIntegerv(LOCAL_GL_ARRAY_BUFFER_BINDING, &mBoundBuffer);
}

void
ScopedVertexAttribPointer::UnwrapImpl()
{
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mAttribBufferBinding);
    mGL->fVertexAttribPointer(mAttribIndex, mAttribSize, mAttribType, mAttribNormalized, mAttribStride, mAttribPointer);
    if (mAttribEnabled)
        mGL->fEnableVertexAttribArray(mAttribIndex);
    else
        mGL->fDisableVertexAttribArray(mAttribIndex);
    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, mBoundBuffer);
}

ScopedGLDrawState::ScopedGLDrawState(GLContext* aGL)
    : blend       (aGL, LOCAL_GL_BLEND,      false)
    , cullFace    (aGL, LOCAL_GL_CULL_FACE,  false)
    , depthTest   (aGL, LOCAL_GL_DEPTH_TEST, false)
    , dither      (aGL, LOCAL_GL_DITHER,     false)
    , polyOffsFill(aGL, LOCAL_GL_POLYGON_OFFSET_FILL,      false)
    , sampleAToC  (aGL, LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE, false)
    , sampleCover (aGL, LOCAL_GL_SAMPLE_COVERAGE, false)
    , scissor     (aGL, LOCAL_GL_SCISSOR_TEST,    false)
    , stencil     (aGL, LOCAL_GL_STENCIL_TEST,    false)
    , mGL(aGL)
    , packAlign(4)
{
    mGL->GetUIntegerv(LOCAL_GL_UNPACK_ALIGNMENT, &packAlign);
    mGL->GetUIntegerv(LOCAL_GL_CURRENT_PROGRAM, &boundProgram);
    mGL->GetUIntegerv(LOCAL_GL_ARRAY_BUFFER_BINDING, &boundBuffer);
    mGL->GetUIntegerv(LOCAL_GL_MAX_VERTEX_ATTRIBS, &maxAttrib);
    attrib_enabled = MakeUnique<GLint[]>(maxAttrib);

    for (unsigned int i = 0; i < maxAttrib; i++) {
        mGL->fGetVertexAttribiv(i, LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attrib_enabled[i]);
        mGL->fDisableVertexAttribArray(i);
    }
    // Only Attrib0's client side state affected
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE, &attrib0_size);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attrib0_stride);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE, &attrib0_type);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &attrib0_normalized);
    mGL->fGetVertexAttribiv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &attrib0_bufferBinding);
    mGL->fGetVertexAttribPointerv(0, LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER, &attrib0_pointer);
    mGL->fGetBooleanv(LOCAL_GL_COLOR_WRITEMASK, colorMask);
    mGL->fGetIntegerv(LOCAL_GL_VIEWPORT, viewport);
    mGL->fGetIntegerv(LOCAL_GL_SCISSOR_BOX, scissorBox);
}

ScopedGLDrawState::~ScopedGLDrawState()
{
    MOZ_ASSERT(mGL->IsCurrent());

    mGL->fScissor(scissorBox[0], scissorBox[1],
                  scissorBox[2], scissorBox[3]);

    mGL->fViewport(viewport[0], viewport[1],
                   viewport[2], viewport[3]);

    mGL->fColorMask(colorMask[0],
                    colorMask[1],
                    colorMask[2],
                    colorMask[3]);

    mGL->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, packAlign);

    for (unsigned int i = 0; i < maxAttrib; i++) {
        if (attrib_enabled[i])
            mGL->fEnableVertexAttribArray(i);
        else
            mGL->fDisableVertexAttribArray(i);
    }


    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, attrib0_bufferBinding);
    mGL->fVertexAttribPointer(0,
                              attrib0_size,
                              attrib0_type,
                              attrib0_normalized,
                              attrib0_stride,
                              attrib0_pointer);

    mGL->fBindBuffer(LOCAL_GL_ARRAY_BUFFER, boundBuffer);

    mGL->fUseProgram(boundProgram);
}

////////////////////////////////////////////////////////////////////////
// ScopedPackAlignment

ScopedPackAlignment::ScopedPackAlignment(GLContext* gl, GLint scopedVal)
    : ScopedGLWrapper<ScopedPackAlignment>(gl)
{
    MOZ_ASSERT(scopedVal == 1 ||
               scopedVal == 2 ||
               scopedVal == 4 ||
               scopedVal == 8);

    gl->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, &mOldVal);

    if (scopedVal != mOldVal) {
        gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, scopedVal);
    } else {
      // Don't try to re-set it during unwrap.
        mOldVal = 0;
    }
}

void
ScopedPackAlignment::UnwrapImpl() {
    // Check that we're not falling out of scope after the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());

    if (mOldVal) {
        mGL->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, mOldVal);
    }
}

////////////////////////////////////////////////////////////////////////
// ScopedUnpackAlignment

ScopedUnpackAlignment::ScopedUnpackAlignment(GLContext* gl, GLint scopedVal)
    : ScopedGLWrapper<ScopedUnpackAlignment>(gl)
{
    MOZ_ASSERT(scopedVal == 1 ||
               scopedVal == 2 ||
               scopedVal == 4 ||
               scopedVal == 8);

    gl->fGetIntegerv(LOCAL_GL_UNPACK_ALIGNMENT, &mOldVal);

    if (scopedVal != mOldVal) {
        gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, scopedVal);
    }
}

void
ScopedUnpackAlignment::UnwrapImpl() {
    // Check that we're not falling out of scope after the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());

    mGL->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, mOldVal);
}

} /* namespace gl */
} /* namespace mozilla */
