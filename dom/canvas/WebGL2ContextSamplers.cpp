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

    ////

    gl->MakeCurrent();
    gl->fBindSampler(unit, sampler ? sampler->mGLName : 0);

    InvalidateResolveCacheForTextureWithTexUnit(unit);
    mBoundSamplers[unit] = sampler;
}

void
WebGL2Context::SamplerParameteri(WebGLSampler& sampler, GLenum pname, GLint paramInt)
{
    const char funcName[] = "samplerParameteri";
    if (IsContextLost())
        return;

    if (!ValidateObjectRef(funcName, sampler))
        return;

    sampler.SamplerParameter(funcName, pname, paramInt);
}

void
WebGL2Context::SamplerParameterf(WebGLSampler& sampler, GLenum pname, GLfloat paramFloat)
{
    const char funcName[] = "samplerParameterf";
    if (IsContextLost())
        return;

    if (!ValidateObjectRef(funcName, sampler))
        return;

    sampler.SamplerParameter(funcName, pname, WebGLIntOrFloat(paramFloat).AsInt());
}

void
WebGL2Context::GetSamplerParameter(JSContext*, const WebGLSampler& sampler, GLenum pname,
                                   JS::MutableHandleValue retval)
{
    const char funcName[] = "getSamplerParameter";
    retval.setNull();

    if (IsContextLost())
        return;

    if (!ValidateObjectRef(funcName, sampler))
        return;

    ////

    gl->MakeCurrent();

    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        {
            GLint param = 0;
            gl->fGetSamplerParameteriv(sampler.mGLName, pname, &param);
            retval.set(JS::Int32Value(param));
        }
        return;

    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
        {
            GLfloat param = 0;
            gl->fGetSamplerParameterfv(sampler.mGLName, pname, &param);
            retval.set(JS::Float32Value(param));
        }
        return;

    default:
        ErrorInvalidEnum("%s: invalid pname: %s", funcName, EnumName(pname));
        return;
    }
}

} // namespace mozilla
