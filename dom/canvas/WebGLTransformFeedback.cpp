/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTransformFeedback.h"

#include "GLContext.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"
#include "WebGL2Context.h"

namespace mozilla {

WebGLTransformFeedback::WebGLTransformFeedback(WebGLContext* webgl,
                                               GLuint tf)
    : WebGLContextBoundObject(webgl)
    , mGLName(tf)
    , mMode(LOCAL_GL_NONE)
    , mIsActive(false)
    , mIsPaused(false)
{
    mContext->mTransformFeedbacks.insertBack(this);
}

WebGLTransformFeedback::~WebGLTransformFeedback()
{
    mMode = LOCAL_GL_NONE;
    mIsActive = false;
    mIsPaused = false;
    DeleteOnce();
}

void
WebGLTransformFeedback::Delete()
{
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteTransformFeedbacks(1, &mGLName);
    removeFrom(mContext->mTransformFeedbacks);
}

WebGLContext*
WebGLTransformFeedback::GetParentObject() const
{
    return mContext;
}

JSObject*
WebGLTransformFeedback::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLTransformFeedbackBinding::Wrap(cx, this, givenProto);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTransformFeedback)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTransformFeedback, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTransformFeedback, Release)

} // namespace mozilla
