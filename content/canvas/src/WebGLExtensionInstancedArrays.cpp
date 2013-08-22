/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

WebGLExtensionInstancedArrays::WebGLExtensionInstancedArrays(WebGLContext* context)
  : WebGLExtensionBase(context)
{
    MOZ_ASSERT(IsSupported(context), "should not construct WebGLExtensionInstancedArrays: "
                                     "ANGLE_instanced_arrays unsupported.");
}

WebGLExtensionInstancedArrays::~WebGLExtensionInstancedArrays()
{
}

void
WebGLExtensionInstancedArrays::DrawArraysInstancedANGLE(WebGLenum mode, WebGLint first,
                                                        WebGLsizei count, WebGLsizei primcount)
{
    mContext->DrawArraysInstanced(mode, first, count, primcount);
}

void
WebGLExtensionInstancedArrays::DrawElementsInstancedANGLE(WebGLenum mode, WebGLsizei count,
                                                          WebGLenum type, WebGLintptr offset,
                                                          WebGLsizei primcount)
{
    mContext->DrawElementsInstanced(mode, count, type, offset, primcount);
}

void
WebGLExtensionInstancedArrays::VertexAttribDivisorANGLE(WebGLuint index, WebGLuint divisor)
{
    mContext->VertexAttribDivisor(index, divisor);
}

bool
WebGLExtensionInstancedArrays::IsSupported(const WebGLContext* context)
{
    gl::GLContext* gl = context->GL();

    return gl->IsSupported(gl::GLFeature::draw_instanced) &&
           gl->IsSupported(gl::GLFeature::instanced_arrays);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionInstancedArrays)
