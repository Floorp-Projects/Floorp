/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "WebGLSampler.h"
#include "GLContext.h"

namespace mozilla {

already_AddRefed<WebGLSampler>
WebGL2Context::CreateSampler()
{
    if (IsContextLost())
        return nullptr;

    GLuint sampler;
    MakeContextCurrent();
    gl->fGenSamplers(1, &sampler);

    RefPtr<WebGLSampler> globj = new WebGLSampler(this, sampler);
    return globj.forget();
}

void
WebGL2Context::DeleteSampler(WebGLSampler* sampler)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteSampler", sampler))
        return;

    if (!sampler || sampler->IsDeleted())
        return;

    for (int n = 0; n < mGLMaxTextureUnits; n++) {
        if (mBoundSamplers[n] == sampler) {
            mBoundSamplers[n] = nullptr;

            InvalidateResolveCacheForTextureWithTexUnit(n);
        }
    }

    sampler->RequestDelete();
}

bool
WebGL2Context::IsSampler(WebGLSampler* sampler)
{
    if (IsContextLost())
        return false;

    if (!sampler)
        return false;

    if (!ValidateObjectAllowDeleted("isSampler", sampler))
        return false;

    if (sampler->IsDeleted())
        return false;

    MakeContextCurrent();
    return gl->fIsSampler(sampler->mGLName);
}

void
WebGL2Context::BindSampler(GLuint unit, WebGLSampler* sampler)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindSampler", sampler))
        return;

    if (GLint(unit) >= mGLMaxTextureUnits)
        return ErrorInvalidValue("bindSampler: unit must be < %d", mGLMaxTextureUnits);

    if (sampler && sampler->IsDeleted())
        return ErrorInvalidOperation("bindSampler: binding deleted sampler");

    WebGLContextUnchecked::BindSampler(unit, sampler);
    InvalidateResolveCacheForTextureWithTexUnit(unit);

    mBoundSamplers[unit] = sampler;
}

void
WebGL2Context::SamplerParameteri(WebGLSampler* sampler, GLenum pname, GLint param)
{
    if (IsContextLost())
        return;

    if (!sampler || sampler->IsDeleted())
        return ErrorInvalidOperation("samplerParameteri: invalid sampler");

    if (!ValidateSamplerParameterParams(pname, WebGLIntOrFloat(param), "samplerParameteri"))
        return;

    sampler->SamplerParameter1i(pname, param);
    WebGLContextUnchecked::SamplerParameteri(sampler, pname, param);
}

void
WebGL2Context::SamplerParameteriv(WebGLSampler* sampler, GLenum pname, const dom::Int32Array& param)
{
    if (IsContextLost())
        return;

    if (!sampler || sampler->IsDeleted())
        return ErrorInvalidOperation("samplerParameteriv: invalid sampler");

    param.ComputeLengthAndData();
    if (param.Length() < 1)
        return /* TODO(djg): Error message */;

    /* TODO(djg): All of these calls in ES3 only take 1 param */
    if (!ValidateSamplerParameterParams(pname, WebGLIntOrFloat(param.Data()[0]), "samplerParameteriv"))
        return;

    sampler->SamplerParameter1i(pname, param.Data()[0]);
    WebGLContextUnchecked::SamplerParameteriv(sampler, pname, param.Data());
}

void
WebGL2Context::SamplerParameteriv(WebGLSampler* sampler, GLenum pname, const dom::Sequence<GLint>& param)
{
    if (IsContextLost())
        return;

    if (!sampler || sampler->IsDeleted())
        return ErrorInvalidOperation("samplerParameteriv: invalid sampler");

    if (param.Length() < 1)
        return /* TODO(djg): Error message */;

    /* TODO(djg): All of these calls in ES3 only take 1 param */
    if (!ValidateSamplerParameterParams(pname, WebGLIntOrFloat(param[0]), "samplerParameteriv"))
        return;

    sampler->SamplerParameter1i(pname, param[0]);
    WebGLContextUnchecked::SamplerParameteriv(sampler, pname, param.Elements());
}

void
WebGL2Context::SamplerParameterf(WebGLSampler* sampler, GLenum pname, GLfloat param)
{
    if (IsContextLost())
        return;

    if (!sampler || sampler->IsDeleted())
        return ErrorInvalidOperation("samplerParameterf: invalid sampler");

    if (!ValidateSamplerParameterParams(pname, WebGLIntOrFloat(param), "samplerParameterf"))
        return;

    sampler->SamplerParameter1f(pname, param);
    WebGLContextUnchecked::SamplerParameterf(sampler, pname, param);
}

void
WebGL2Context::SamplerParameterfv(WebGLSampler* sampler, GLenum pname, const dom::Float32Array& param)
{
    if (IsContextLost())
        return;

    if (!sampler || sampler->IsDeleted())
        return ErrorInvalidOperation("samplerParameterfv: invalid sampler");

    param.ComputeLengthAndData();
    if (param.Length() < 1)
        return /* TODO(djg): Error message */;

    /* TODO(djg): All of these calls in ES3 only take 1 param */
    if (!ValidateSamplerParameterParams(pname, WebGLIntOrFloat(param.Data()[0]), "samplerParameterfv"))
        return;

    sampler->SamplerParameter1f(pname, param.Data()[0]);
    WebGLContextUnchecked::SamplerParameterfv(sampler, pname, param.Data());
}

void
WebGL2Context::SamplerParameterfv(WebGLSampler* sampler, GLenum pname, const dom::Sequence<GLfloat>& param)
{
    if (IsContextLost())
        return;

    if (!sampler || sampler->IsDeleted())
        return ErrorInvalidOperation("samplerParameterfv: invalid sampler");

    if (param.Length() < 1)
        return /* TODO(djg): Error message */;

    /* TODO(djg): All of these calls in ES3 only take 1 param */
    if (!ValidateSamplerParameterParams(pname, WebGLIntOrFloat(param[0]), "samplerParameterfv"))
        return;

    sampler->SamplerParameter1f(pname, param[0]);
    WebGLContextUnchecked::SamplerParameterfv(sampler, pname, param.Elements());
}

void
WebGL2Context::GetSamplerParameter(JSContext*, WebGLSampler* sampler, GLenum pname, JS::MutableHandleValue retval)
{
    if (IsContextLost())
        return;

    if (!sampler || sampler->IsDeleted())
        return ErrorInvalidOperation("getSamplerParameter: invalid sampler");

    if (!ValidateSamplerParameterName(pname, "getSamplerParameter"))
        return;

    retval.set(JS::NullValue());

    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        retval.set(JS::Int32Value(
            WebGLContextUnchecked::GetSamplerParameteriv(sampler, pname)));
        return;

    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
        retval.set(JS::Float32Value(
            WebGLContextUnchecked::GetSamplerParameterfv(sampler, pname)));
        return;
    }
}

} // namespace mozilla
