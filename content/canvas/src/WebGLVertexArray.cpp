/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"

using namespace mozilla;

JSObject*
WebGLVertexArray::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope) {
    return dom::WebGLVertexArrayBinding::Wrap(cx, scope, this);
}

WebGLVertexArray::WebGLVertexArray(WebGLContext* context)
    : WebGLContextBoundObject(context)
    , mGLName(0)
    , mHasEverBeenBound(false)
{
    SetIsDOMBinding();
}

void WebGLVertexArray::Delete() {
    if (mGLName != 0) {
        mBoundElementArrayBuffer = nullptr;

        mContext->MakeContextCurrent();
        mContext->gl->fDeleteVertexArrays(1, &mGLName);
        LinkedListElement<WebGLVertexArray>::removeFrom(mContext->mVertexArrays);
    }

    mBoundElementArrayBuffer = nullptr;
    mAttribBuffers.Clear();
}

bool WebGLVertexArray::EnsureAttribIndex(GLuint index, const char *info)
{
    if (index >= GLuint(mContext->mGLMaxVertexAttribs)) {
        if (index == GLuint(-1)) {
            mContext->ErrorInvalidValue("%s: index -1 is invalid. That probably comes from a getAttribLocation() call, "
                                        "where this return value -1 means that the passed name didn't correspond to an active attribute in "
                                        "the specified program.", info);
        } else {
            mContext->ErrorInvalidValue("%s: index %d is out of range", info, index);
        }
        return false;
    }
    else if (index >= mAttribBuffers.Length()) {
        mAttribBuffers.SetLength(index + 1);
    }

    return true;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_2(WebGLVertexArray,
  mAttribBuffers,
  mBoundElementArrayBuffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLVertexArray, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLVertexArray, Release)
