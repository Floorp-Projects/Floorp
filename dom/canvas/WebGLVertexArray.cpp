/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

WebGLVertexArray::WebGLVertexArray(WebGLContext* const webgl, const GLuint name)
    : WebGLRefCountedObject(webgl),
      mGLName(name),
      mAttribs(mContext->mGLMaxVertexAttribs) {
  mContext->mVertexArrays.insertBack(this);
}

WebGLVertexArray::~WebGLVertexArray() { MOZ_ASSERT(IsDeleted()); }

WebGLVertexArray* WebGLVertexArray::Create(WebGLContext* webgl) {
  WebGLVertexArray* array;
  if (webgl->gl->IsSupported(gl::GLFeature::vertex_array_object)) {
    array = new WebGLVertexArrayGL(webgl);
  } else {
    array = new WebGLVertexArrayFake(webgl);
  }
  return array;
}

void WebGLVertexArray::Delete() {
  DeleteImpl();

  LinkedListElement<WebGLVertexArray>::removeFrom(mContext->mVertexArrays);
  mElementArrayBuffer = nullptr;
  mAttribs.clear();
}

// -

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& callback,
    const std::vector<WebGLVertexAttribData>& field, const char* name,
    uint32_t flags = 0) {
  for (auto& cur : field) {
    ImplCycleCollectionTraverse(callback, cur.mBuf, name, flags);
  }
}

inline void ImplCycleCollectionUnlink(
    std::vector<WebGLVertexAttribData>& field) {
  for (auto& cur : field) {
    cur.mBuf = nullptr;
  }
}

}  // namespace mozilla
