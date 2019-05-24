/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"
#include "WebGLVertexArray.h"

namespace mozilla {

WebGLExtensionVertexArray::WebGLExtensionVertexArray(WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {}

WebGLExtensionVertexArray::~WebGLExtensionVertexArray() {}

already_AddRefed<WebGLVertexArray>
WebGLExtensionVertexArray::CreateVertexArrayOES() {
  if (mIsLost || !mContext) return nullptr;

  return mContext->CreateVertexArray();
}

void WebGLExtensionVertexArray::DeleteVertexArrayOES(WebGLVertexArray* array) {
  if (mIsLost || !mContext) return;

  mContext->DeleteVertexArray(array);
}

bool WebGLExtensionVertexArray::IsVertexArrayOES(
    const WebGLVertexArray* array) {
  if (mIsLost || !mContext) return false;

  return mContext->IsVertexArray(array);
}

void WebGLExtensionVertexArray::BindVertexArrayOES(WebGLVertexArray* array) {
  if (mIsLost || !mContext) return;

  mContext->BindVertexArray(array);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionVertexArray, OES_vertex_array_object)

}  // namespace mozilla
