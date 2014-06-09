/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArray.h"

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArrayGL.h"
#include "WebGLVertexArrayFake.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"

using namespace mozilla;

JSObject*
WebGLVertexArray::WrapObject(JSContext *cx) {
    return dom::WebGLVertexArrayBinding::Wrap(cx, this);
}

WebGLVertexArray::WebGLVertexArray(WebGLContext* context)
    : WebGLContextBoundObject(context)
    , mGLName(0)
    , mHasEverBeenBound(false)
{
    SetIsDOMBinding();
    context->mVertexArrays.insertBack(this);
}

WebGLVertexArray*
WebGLVertexArray::Create(WebGLContext* context)
{
    WebGLVertexArray* array;
    if (context->gl->IsSupported(gl::GLFeature::vertex_array_object)) {
        array = new WebGLVertexArrayGL(context);
    } else {
        array = new WebGLVertexArrayFake(context);
    }

    return array;
}

void
WebGLVertexArray::Delete()
{
    DeleteImpl();

    LinkedListElement<WebGLVertexArray>::removeFrom(mContext->mVertexArrays);
    mElementArrayBuffer = nullptr;
    mAttribs.Clear();
}

bool
WebGLVertexArray::EnsureAttrib(GLuint index, const char *info)
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
    else if (index >= mAttribs.Length()) {
        mAttribs.SetLength(index + 1);
    }

    return true;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLVertexArray,
  mAttribs,
  mElementArrayBuffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLVertexArray, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLVertexArray, Release)
