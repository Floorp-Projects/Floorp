/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionDebugRendererInfo::WebGLExtensionDebugRendererInfo(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
}

WebGLExtensionDebugRendererInfo::~WebGLExtensionDebugRendererInfo()
{
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDebugRendererInfo, WEBGL_debug_renderer_info)

} // namespace mozilla
