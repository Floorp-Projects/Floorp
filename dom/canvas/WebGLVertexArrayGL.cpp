/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLVertexArrayGL.h"

#include "GLContext.h"
#include "WebGLBuffer.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLVertexArrayGL::WebGLVertexArrayGL(WebGLContext* webgl)
    : WebGLVertexArray(webgl), mGLName([&]() {
        GLuint ret = 0;
        webgl->gl->fGenVertexArrays(1, &ret);
        return ret;
      }()) {}

WebGLVertexArrayGL::~WebGLVertexArrayGL() {
  if (!mContext) return;
  mContext->gl->fDeleteVertexArrays(1, &mGLName);
}

void WebGLVertexArrayGL::BindVertexArray() {
  mContext->mBoundVertexArray = this;
  mContext->gl->fBindVertexArray(mGLName);
}

}  // namespace mozilla
