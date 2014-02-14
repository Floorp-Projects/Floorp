/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"

using namespace mozilla;

WebGLExtensionFragDepth::WebGLExtensionFragDepth(WebGLContext* context)
    : WebGLExtensionBase(context)
{
    MOZ_ASSERT(IsSupported(context),
               "Should not construct extension object if unsupported.");
}

WebGLExtensionFragDepth::~WebGLExtensionFragDepth()
{
}

bool
WebGLExtensionFragDepth::IsSupported(const WebGLContext* context)
{
    gl::GLContext* gl = context->GL();

    return gl->IsSupported(gl::GLFeature::frag_depth);
}


IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionFragDepth)
