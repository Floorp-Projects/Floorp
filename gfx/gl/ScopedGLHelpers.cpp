/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

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
    mOldFB = mGL->GetFB();
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

    mGL->BindFB(mOldFB);
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
void
ScopedBindTexture::Init(GLenum aTarget)
{
    mTarget = aTarget;
    mOldTex = 0;
    GLenum bindingTarget = (aTarget == LOCAL_GL_TEXTURE_2D) ? LOCAL_GL_TEXTURE_BINDING_2D
                         : (aTarget == LOCAL_GL_TEXTURE_RECTANGLE_ARB) ? LOCAL_GL_TEXTURE_BINDING_RECTANGLE_ARB
                         : (aTarget == LOCAL_GL_TEXTURE_CUBE_MAP) ? LOCAL_GL_TEXTURE_BINDING_CUBE_MAP
                         : (aTarget == LOCAL_GL_TEXTURE_EXTERNAL) ? LOCAL_GL_TEXTURE_BINDING_EXTERNAL
                         : LOCAL_GL_NONE;
    MOZ_ASSERT(bindingTarget != LOCAL_GL_NONE);
    mGL->GetUIntegerv(bindingTarget, &mOldTex);
}

ScopedBindTexture::ScopedBindTexture(GLContext* aGL, GLuint aNewTex, GLenum aTarget)
        : ScopedGLWrapper<ScopedBindTexture>(aGL)
    {
        Init(aTarget);
        mGL->fBindTexture(aTarget, aNewTex);
    }

void
ScopedBindTexture::UnwrapImpl()
{
    // Check that we're not falling out of scope after the current context changed.
    MOZ_ASSERT(mGL->IsCurrent());

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

} /* namespace gl */
} /* namespace mozilla */
