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
    const FuncScope funcScope(*this, "createSampler");
    if (IsContextLost())
        return nullptr;

    RefPtr<WebGLSampler> globj = new WebGLSampler(this);
    return globj.forget();
}

void
WebGL2Context::DeleteSampler(WebGLSampler* sampler)
{
    const FuncScope funcScope(*this, "deleteSampler");
    if (!ValidateDeleteObject(sampler))
        return;

    for (uint32_t n = 0; n < mGLMaxTextureUnits; n++) {
        if (mBoundSamplers[n] == sampler) {
            mBoundSamplers[n] = nullptr;
        }
    }

    sampler->RequestDelete();
}

bool
WebGL2Context::IsSampler(const WebGLSampler* const obj)
{
    const FuncScope funcScope(*this, "isSampler");
    if (!ValidateIsObject(obj))
        return false;

    return gl->fIsSampler(obj->mGLName);
}

void
WebGL2Context::BindSampler(GLuint unit, WebGLSampler* sampler)
{
    const FuncScope funcScope(*this, "bindSampler");
    if (IsContextLost())
        return;

    if (sampler && !ValidateObject("sampler", *sampler))
        return;

    if (unit >= mGLMaxTextureUnits)
        return ErrorInvalidValue("unit must be < %u", mGLMaxTextureUnits);

    ////

    gl->fBindSampler(unit, sampler ? sampler->mGLName : 0);

    mBoundSamplers[unit] = sampler;
}

void
WebGL2Context::SamplerParameteri(WebGLSampler& sampler, GLenum pname, GLint param)
{
    const FuncScope funcScope(*this, "samplerParameteri");
    if (IsContextLost())
        return;

    if (!ValidateObject("sampler", sampler))
        return;

    sampler.SamplerParameter(pname, FloatOrInt(param));
}

void
WebGL2Context::SamplerParameterf(WebGLSampler& sampler, GLenum pname, GLfloat param)
{
    const FuncScope funcScope(*this, "samplerParameterf");
    if (IsContextLost())
        return;

    if (!ValidateObject("sampler", sampler))
        return;

    sampler.SamplerParameter(pname, FloatOrInt(param));
}

void
WebGL2Context::GetSamplerParameter(JSContext*, const WebGLSampler& sampler, GLenum pname,
                                   JS::MutableHandleValue retval)
{
    const FuncScope funcScope(*this, "getSamplerParameter");
    retval.setNull();

    if (IsContextLost())
        return;

    if (!ValidateObject("sampler", sampler))
        return;

    ////

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
        ErrorInvalidEnumInfo("pname", pname);
        return;
    }
}

} // namespace mozilla
