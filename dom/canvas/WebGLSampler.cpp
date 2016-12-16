/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLSampler.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLSampler::WebGLSampler(WebGLContext* webgl, GLuint sampler)
    : WebGLRefCountedObject(webgl)
    , mGLName(sampler)
    , mMinFilter(LOCAL_GL_NEAREST_MIPMAP_LINEAR)
    , mMagFilter(LOCAL_GL_LINEAR)
    , mWrapS(LOCAL_GL_REPEAT)
    , mWrapT(LOCAL_GL_REPEAT)
    , mWrapR(LOCAL_GL_REPEAT)
    , mMinLod(-1000)
    , mMaxLod(1000)
    , mCompareMode(LOCAL_GL_NONE)
    , mCompareFunc(LOCAL_GL_LEQUAL)
{
    mContext->mSamplers.insertBack(this);
}

WebGLSampler::~WebGLSampler()
{
    DeleteOnce();
}

void
WebGLSampler::Delete()
{
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteSamplers(1, &mGLName);

    removeFrom(mContext->mSamplers);
}

WebGLContext*
WebGLSampler::GetParentObject() const
{
    return mContext;
}

JSObject*
WebGLSampler::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLSamplerBinding::Wrap(cx, this, givenProto);
}

static bool
ValidateSamplerParameterParams(WebGLContext* webgl, const char* funcName, GLenum pname,
                               GLint paramInt)
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
        switch (paramInt) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
        case LOCAL_GL_NEAREST_MIPMAP_NEAREST:
        case LOCAL_GL_NEAREST_MIPMAP_LINEAR:
        case LOCAL_GL_LINEAR_MIPMAP_NEAREST:
        case LOCAL_GL_LINEAR_MIPMAP_LINEAR:
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        switch (paramInt) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
        switch (paramInt) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_REPEAT:
        case LOCAL_GL_MIRRORED_REPEAT:
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_MAX_LOD:
        return true;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        switch (paramInt) {
        case LOCAL_GL_NONE:
        case LOCAL_GL_COMPARE_REF_TO_TEXTURE:
            return true;

        default:
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        switch (paramInt) {
        case LOCAL_GL_LEQUAL:
        case LOCAL_GL_GEQUAL:
        case LOCAL_GL_LESS:
        case LOCAL_GL_GREATER:
        case LOCAL_GL_EQUAL:
        case LOCAL_GL_NOTEQUAL:
        case LOCAL_GL_ALWAYS:
        case LOCAL_GL_NEVER:
            return true;

        default:
            break;
        }
        break;

    default:
        webgl->ErrorInvalidEnum("%s: invalid pname: %s", funcName,
                                webgl->EnumName(pname));
        return false;
    }

    webgl->ErrorInvalidEnum("%s: invalid param: %s", funcName, webgl->EnumName(paramInt));
    return false;
}

void
WebGLSampler::SamplerParameter(const char* funcName, GLenum pname, GLint paramInt)
{
    if (!ValidateSamplerParameterParams(mContext, funcName, pname, paramInt))
        return;

    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
        mMinFilter = paramInt;
        break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        mMagFilter = paramInt;
        break;

    case LOCAL_GL_TEXTURE_WRAP_S:
        mWrapS = paramInt;
        break;

    case LOCAL_GL_TEXTURE_WRAP_T:
        mWrapT = paramInt;
        break;

    case LOCAL_GL_TEXTURE_WRAP_R:
        mWrapR = paramInt;
        break;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        mCompareMode = paramInt;
        break;

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        mCompareFunc = paramInt;
        break;

    case LOCAL_GL_TEXTURE_MIN_LOD:
        mMinLod = paramInt;
        break;

    case LOCAL_GL_TEXTURE_MAX_LOD:
        mMaxLod = paramInt;
        break;

    default:
        MOZ_CRASH("GFX: Unhandled pname");
        break;
    }

    for (uint32_t i = 0; i < mContext->mBoundSamplers.Length(); ++i) {
        if (this == mContext->mBoundSamplers[i])
            mContext->InvalidateResolveCacheForTextureWithTexUnit(i);
    }

    ////

    mContext->gl->MakeCurrent();
    mContext->gl->fSamplerParameteri(mGLName, pname, paramInt);
}

////

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLSampler)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLSampler, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLSampler, Release)

} // namespace mozilla
