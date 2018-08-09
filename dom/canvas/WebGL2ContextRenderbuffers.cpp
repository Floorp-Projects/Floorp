/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "WebGLContextUtils.h"

namespace mozilla {

void
WebGL2Context::GetInternalformatParameter(JSContext* cx, GLenum target,
                                          GLenum internalformat, GLenum pname,
                                          JS::MutableHandleValue retval,
                                          ErrorResult& out_rv)
{
    const char funcName[] = "getInternalfomratParameter";
    retval.setObjectOrNull(nullptr);

    if (IsContextLost())
        return;

    if (target != LOCAL_GL_RENDERBUFFER) {
        ErrorInvalidEnum("%s: `target` must be RENDERBUFFER, was: 0x%04x.", funcName,
                         target);
        return;
    }

    // GLES 3.0.4 $4.4.4 p212:
    // "An internal format is color-renderable if it is one of the formats from table 3.13
    //  noted as color-renderable or if it is unsized format RGBA or RGB."

    GLenum sizedFormat;
    switch (internalformat) {
    case LOCAL_GL_RGB:
        sizedFormat = LOCAL_GL_RGB8;
        break;
    case LOCAL_GL_RGBA:
        sizedFormat = LOCAL_GL_RGBA8;
        break;
    default:
        sizedFormat = internalformat;
        break;
    }

    // In RenderbufferStorage, we allow DEPTH_STENCIL. Therefore, it is accepted for
    // internalformat as well. Please ignore the conformance test fail for DEPTH_STENCIL.

    const auto usage = mFormatUsage->GetRBUsage(sizedFormat);
    if (!usage) {
        ErrorInvalidEnum("%s: `internalformat` must be color-, depth-, or stencil-renderable, was: 0x%04x.",
                         funcName, internalformat);
        return;
    }

    if (pname != LOCAL_GL_SAMPLES) {
        ErrorInvalidEnumInfo("%s: `pname` must be SAMPLES, was 0x%04x.", funcName, pname);
        return;
    }

    GLint* samples = nullptr;
    GLint sampleCount = 0;
    gl->fGetInternalformativ(LOCAL_GL_RENDERBUFFER, internalformat,
                             LOCAL_GL_NUM_SAMPLE_COUNTS, 1, &sampleCount);
    if (sampleCount > 0) {
        samples = new GLint[sampleCount];
        gl->fGetInternalformativ(LOCAL_GL_RENDERBUFFER, internalformat, LOCAL_GL_SAMPLES,
                                 sampleCount, samples);
    }

    JSObject* obj = dom::Int32Array::Create(cx, this, sampleCount, samples);
    if (!obj) {
        out_rv = NS_ERROR_OUT_OF_MEMORY;
    }

    delete[] samples;

    retval.setObjectOrNull(obj);
}

void
WebGL2Context::RenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                              GLenum internalFormat,
                                              GLsizei width, GLsizei height)
{
    const char funcName[] = "renderbufferStorageMultisample";
    if (IsContextLost())
        return;

    RenderbufferStorage_base(funcName, target, samples, internalFormat, width, height);
}

} // namespace mozilla
