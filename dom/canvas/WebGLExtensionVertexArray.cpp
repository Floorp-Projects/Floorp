/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexArray.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"

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
    if (mIsLost) {
        mContext->GenerateWarning("createVertexArrayOES: Extension is lost. Returning null.");
        return nullptr;
    }

    return mContext->CreateVertexArray();
}

void WebGLExtensionVertexArray::DeleteVertexArrayOES(WebGLVertexArray* array)
{
    if (mIsLost)
        return mContext->GenerateWarning("deleteVertexArrayOES: Extension is lost.");

    mContext->DeleteVertexArray(array);
}

bool WebGLExtensionVertexArray::IsVertexArrayOES(WebGLVertexArray* array)
{
    if (mIsLost) {
        mContext->GenerateWarning("isVertexArrayOES: Extension is lost. Returning false.");
        return false;
    }

    return mContext->IsVertexArray(array);
}

void WebGLExtensionVertexArray::BindVertexArrayOES(WebGLVertexArray* array)
{
    if (mIsLost)
        return mContext->GenerateWarning("bindVertexArrayOES: Extension is lost.");

    mContext->BindVertexArray(array);
}

bool WebGLExtensionVertexArray::IsSupported(const WebGLContext* context)
{
    // If it is not supported then it's emulated, therefore it's always 'supported'
    // See - WebGLVertexArrayFake.h/cpp for the emulation
    return true;
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionVertexArray)
