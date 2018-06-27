/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArray.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLVertexArrayGL.h"
#include "WebGLVertexArrayFake.h"

namespace mozilla {

JSObject*
WebGLVertexArray::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLVertexArrayObject_Binding::Wrap(cx, this, givenProto);
}

WebGLVertexArray::WebGLVertexArray(WebGLContext* webgl)
    : WebGLRefCountedObject(webgl)
    , mGLName(0)
{
    mAttribs.SetLength(mContext->mGLMaxVertexAttribs);
    mContext->mVertexArrays.insertBack(this);
}

WebGLVertexArray::~WebGLVertexArray()
{
    MOZ_ASSERT(IsDeleted());
}

void
WebGLVertexArray::AddBufferBindCounts(int8_t addVal) const
{
    const GLenum target = 0; // Anything non-TF is fine.
    WebGLBuffer::AddBindCount(target, mElementArrayBuffer.get(), addVal);
    for (const auto& attrib : mAttribs) {
        WebGLBuffer::AddBindCount(target, attrib.mBuf.get(), addVal);
    }
}

WebGLVertexArray*
WebGLVertexArray::Create(WebGLContext* webgl)
{
    WebGLVertexArray* array;
    if (webgl->gl->IsSupported(gl::GLFeature::vertex_array_object)) {
        array = new WebGLVertexArrayGL(webgl);
    } else {
        array = new WebGLVertexArrayFake(webgl);
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
WebGLVertexArray::IsVertexArray() const
{
    return IsVertexArrayImpl();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLVertexArray,
                                      mAttribs,
                                      mElementArrayBuffer)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLVertexArray, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLVertexArray, Release)

} // namespace mozilla
