/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<WebGLSampler>
WebGL2Context::CreateSampler()
{
    MOZ_CRASH("Not Implemented.");
    return nullptr;
}

void
WebGL2Context::DeleteSampler(WebGLSampler* sampler)
{
    MOZ_CRASH("Not Implemented.");
}

bool
WebGL2Context::IsSampler(WebGLSampler* sampler)
{
    MOZ_CRASH("Not Implemented.");
    return false;
}

void
WebGL2Context::BindSampler(GLuint unit, WebGLSampler* sampler)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::SamplerParameteri(WebGLSampler* sampler, GLenum pname, GLint param)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::SamplerParameteriv(WebGLSampler* sampler, GLenum pname, const dom::Int32Array& param)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::SamplerParameteriv(WebGLSampler* sampler, GLenum pname, const dom::Sequence<GLint>& param)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::SamplerParameterf(WebGLSampler* sampler, GLenum pname, GLfloat param)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::SamplerParameterfv(WebGLSampler* sampler, GLenum pname, const dom::Float32Array& param)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::SamplerParameterfv(WebGLSampler* sampler, GLenum pname, const dom::Sequence<GLfloat>& param)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetSamplerParameter(JSContext*, WebGLSampler* sampler, GLenum pname, JS::MutableHandleValue retval)
{
    MOZ_CRASH("Not Implemented.");
}
