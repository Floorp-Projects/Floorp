/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL1Context.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/Telemetry.h"
#include "WebGLFormats.h"

namespace mozilla {

/*static*/ WebGL1Context*
WebGL1Context::Create()
{
    return new WebGL1Context();
}

WebGL1Context::WebGL1Context()
    : WebGLContext()
{
}

WebGL1Context::~WebGL1Context()
{
}

UniquePtr<webgl::FormatUsageAuthority>
WebGL1Context::CreateFormatUsage(gl::GLContext* gl) const
{
    return webgl::FormatUsageAuthority::CreateForWebGL1(gl);
}

JSObject*
WebGL1Context::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLRenderingContextBinding::Wrap(cx, this, givenProto);
}

bool
WebGL1Context::ValidateQueryTarget(GLenum target, const char* info)
{
    return false;
}

} // namespace mozilla

nsresult
NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** out_result)
{
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::CANVAS_WEBGL_USED, 1);

    nsIDOMWebGLRenderingContext* ctx = mozilla::WebGL1Context::Create();

    NS_ADDREF(*out_result = ctx);
    return NS_OK;
}
