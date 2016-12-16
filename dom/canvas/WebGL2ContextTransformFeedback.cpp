/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "WebGLActiveInfo.h"
#include "WebGLProgram.h"
#include "WebGLTransformFeedback.h"
#include "GLContext.h"

namespace mozilla {

// -------------------------------------------------------------------------
// Transform Feedback

already_AddRefed<WebGLTransformFeedback>
WebGL2Context::CreateTransformFeedback()
{
    if (IsContextLost())
        return nullptr;

    MakeContextCurrent();
    GLuint tf = 0;
    gl->fGenTransformFeedbacks(1, &tf);

    RefPtr<WebGLTransformFeedback> ret = new WebGLTransformFeedback(this, tf);
    return ret.forget();
}

void
WebGL2Context::DeleteTransformFeedback(WebGLTransformFeedback* tf)
{
    const char funcName[] = "deleteTransformFeedback";
    if (IsContextLost())
        return;

    if (!ValidateObject(funcName, tf))
        return;

    if (tf->mIsActive) {
        ErrorInvalidOperation("%s: Cannot delete active transform feedbacks.", funcName);
        return;
    }

    if (mBoundTransformFeedback == tf) {
        BindTransformFeedback(LOCAL_GL_TRANSFORM_FEEDBACK, nullptr);
    }

    tf->RequestDelete();
}

bool
WebGL2Context::IsTransformFeedback(WebGLTransformFeedback* tf)
{
    if (IsContextLost())
        return false;

    if (!ValidateObjectAllowDeletedOrNull("isTransformFeedback", tf))
        return false;

    if (!tf || tf->IsDeleted())
        return false;

    MakeContextCurrent();
    return gl->fIsTransformFeedback(tf->mGLName);
}

void
WebGL2Context::BindTransformFeedback(GLenum target, WebGLTransformFeedback* tf)
{
    const char funcName[] = "bindTransformFeedback";
    if (IsContextLost())
        return;

    if (target != LOCAL_GL_TRANSFORM_FEEDBACK)
        return ErrorInvalidEnum("%s: `target` must be TRANSFORM_FEEDBACK.", funcName);

    if (!ValidateObjectAllowDeletedOrNull(funcName, tf))
        return;

    if (tf && tf->IsDeleted())
        return ErrorInvalidOperation("%s: TFO already deleted.", funcName);

    if (mBoundTransformFeedback->mIsActive &&
        !mBoundTransformFeedback->mIsPaused)
    {
        ErrorInvalidOperation("%s: Currently bound transform feedback is active and not"
                              " paused.",
                              funcName);
        return;
    }

    ////

    mBoundTransformFeedback = (tf ? tf : mDefaultTransformFeedback);

    MakeContextCurrent();
    gl->fBindTransformFeedback(target, mBoundTransformFeedback->mGLName);
}

void
WebGL2Context::BeginTransformFeedback(GLenum primMode)
{
    if (IsContextLost())
        return;

    mBoundTransformFeedback->BeginTransformFeedback(primMode);
}

void
WebGL2Context::EndTransformFeedback()
{
    if (IsContextLost())
        return;

    mBoundTransformFeedback->EndTransformFeedback();
}

void
WebGL2Context::PauseTransformFeedback()
{
    if (IsContextLost())
        return;

    mBoundTransformFeedback->PauseTransformFeedback();
}

void
WebGL2Context::ResumeTransformFeedback()
{
    if (IsContextLost())
        return;

    mBoundTransformFeedback->ResumeTransformFeedback();
}

void
WebGL2Context::TransformFeedbackVaryings(WebGLProgram& program,
                                         const dom::Sequence<nsString>& varyings,
                                         GLenum bufferMode)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectRef("transformFeedbackVaryings: program", program))
        return;

    program.TransformFeedbackVaryings(varyings, bufferMode);
}

already_AddRefed<WebGLActiveInfo>
WebGL2Context::GetTransformFeedbackVarying(const WebGLProgram& program, GLuint index)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateObjectRef("getTransformFeedbackVarying: program", program))
        return nullptr;

    return program.GetTransformFeedbackVarying(index);
}

} // namespace mozilla
