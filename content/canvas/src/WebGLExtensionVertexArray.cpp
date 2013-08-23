/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

WebGLExtensionVertexArray::WebGLExtensionVertexArray(WebGLContext* context)
  : WebGLExtensionBase(context)
{
    MOZ_ASSERT(IsSupported(context), "should not construct WebGLExtensionVertexArray :"
                                     "OES_vertex_array_object unsuported.");
}

WebGLExtensionVertexArray::~WebGLExtensionVertexArray()
{
}

already_AddRefed<WebGLVertexArray> WebGLExtensionVertexArray::CreateVertexArrayOES()
{
    return mContext->CreateVertexArray();
}

void WebGLExtensionVertexArray::DeleteVertexArrayOES(WebGLVertexArray* array)
{
    mContext->DeleteVertexArray(array);
}

bool WebGLExtensionVertexArray::IsVertexArrayOES(WebGLVertexArray* array)
{
    return mContext->IsVertexArray(array);
}

void WebGLExtensionVertexArray::BindVertexArrayOES(WebGLVertexArray* array)
{
    mContext->BindVertexArray(array);
}

bool WebGLExtensionVertexArray::IsSupported(const WebGLContext* context)
{
    gl::GLContext* gl = context->GL();

    return gl->IsSupported(gl::GLFeature::vertex_array_object);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionVertexArray)
