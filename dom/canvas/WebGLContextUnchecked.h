/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCONTEXTUNCHECKED_H
#define WEBGLCONTEXTUNCHECKED_H

#include "mozilla/RefPtr.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLBuffer;
class WebGLSampler;

class WebGLContextUnchecked
{
public:
    explicit WebGLContextUnchecked(gl::GLContext* gl);

    // -------------------------------------------------------------------------
    // Buffer Objects
    void BindBuffer(GLenum target, WebGLBuffer* buffer);
    void BindBufferBase(GLenum target, GLuint index, WebGLBuffer* buffer);
    void BindBufferRange(GLenum taret, GLuint index, WebGLBuffer* buffer, WebGLintptr offset, WebGLsizeiptr size);
    void CopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);

    // -------------------------------------------------------------------------
    // Sampler Objects
    void BindSampler(GLuint unit, WebGLSampler* sampler);

    GLint   GetSamplerParameteriv(WebGLSampler* sampler, GLenum pname);
    GLfloat GetSamplerParameterfv(WebGLSampler* sampler, GLenum pname);

    void SamplerParameteri(WebGLSampler* sampler, GLenum pname, GLint param);
    void SamplerParameteriv(WebGLSampler* sampler, GLenum pname, const GLint* param);
    void SamplerParameterf(WebGLSampler* sampler, GLenum pname, GLfloat param);
    void SamplerParameterfv(WebGLSampler* sampler, GLenum pname, const GLfloat* param);

protected:
    // We've had issues in the past with nulling `gl` without actually releasing
    // all of our resources. This construction ensures that we are aware that we
    // should only null `gl` in DestroyResourcesAndContext.
    RefPtr<gl::GLContext> mGL_OnlyClearInDestroyResourcesAndContext;
public:
    // Grab a const reference so we can see changes, but can't make changes.
    const decltype(mGL_OnlyClearInDestroyResourcesAndContext)& gl;
};

} // namespace mozilla

#endif // !WEBGLCONTEXTUNCHECKED_H
