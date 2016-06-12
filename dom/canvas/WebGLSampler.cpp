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
    : WebGLContextBoundObject(webgl)
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

void
WebGLSampler::SamplerParameter1i(GLenum pname, GLint param)
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
        mMinFilter = param;
        break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        mMagFilter = param;
        break;

    case LOCAL_GL_TEXTURE_WRAP_S:
        mWrapS = param;
        break;

    case LOCAL_GL_TEXTURE_WRAP_T:
        mWrapT = param;
        break;

    case LOCAL_GL_TEXTURE_WRAP_R:
        mWrapR = param;
        break;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        mCompareMode = param;
        break;

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        mCompareFunc = param;
        break;

    default:
        MOZ_CRASH("GFX: Unhandled pname");
        break;
    }

    for (uint32_t i = 0; i < mContext->mBoundSamplers.Length(); ++i) {
        if (this == mContext->mBoundSamplers[i])
            mContext->InvalidateResolveCacheForTextureWithTexUnit(i);
    }
}

void
WebGLSampler::SamplerParameter1f(GLenum pname, GLfloat param)
{
    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_LOD:
        mMinLod = param;
        break;

    case LOCAL_GL_TEXTURE_MAX_LOD:
        mMaxLod = param;
        break;

    default:
        MOZ_CRASH("GFX: Unhandled pname");
        break;
    }

    for (uint32_t i = 0; i < mContext->mBoundSamplers.Length(); ++i) {
        if (this == mContext->mBoundSamplers[i])
            mContext->InvalidateResolveCacheForTextureWithTexUnit(i);
    }
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLSampler)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLSampler, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLSampler, Release)

} // namespace mozilla
