/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------------------------------------
// Transform Feedback

already_AddRefed<WebGLTransformFeedback>
WebGL2Context::CreateTransformFeedback()
{
    MOZ_CRASH("Not Implemented.");
    return nullptr;
}

void
WebGL2Context::DeleteTransformFeedback(WebGLTransformFeedback* tf)
{
    MOZ_CRASH("Not Implemented.");
}

bool
WebGL2Context::IsTransformFeedback(WebGLTransformFeedback* tf)
{
    MOZ_CRASH("Not Implemented.");
    return false;
}

void
WebGL2Context::BindTransformFeedback(GLenum target, GLuint id)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::BeginTransformFeedback(GLenum primitiveMode)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::EndTransformFeedback()
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::TransformFeedbackVaryings(WebGLProgram* program, GLsizei count,
                                         const dom::Sequence<nsString>& varyings, GLenum bufferMode)
{
    MOZ_CRASH("Not Implemented.");
}


already_AddRefed<WebGLActiveInfo>
WebGL2Context::GetTransformFeedbackVarying(WebGLProgram* program, GLuint index)
{
    MOZ_CRASH("Not Implemented.");
    return nullptr;
}

void
WebGL2Context::PauseTransformFeedback()
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::ResumeTransformFeedback()
{
    MOZ_CRASH("Not Implemented.");
}
