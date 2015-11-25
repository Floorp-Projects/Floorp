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


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLSampler)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLSampler, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLSampler, Release)

} // namespace mozilla
