/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL1Context.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

#include "mozilla/Telemetry.h"

using namespace mozilla;

// -----------------------------------------------------------------------------
// CONSTRUCTOR & DESTRUCTOR

WebGL1Context::WebGL1Context()
    : WebGLContext()
{

}

WebGL1Context::~WebGL1Context()
{

}


// -----------------------------------------------------------------------------
// IMPLEMENT nsWrapperCache

JSObject*
WebGL1Context::WrapObject(JSContext *cx)
{
    return dom::WebGLRenderingContextBinding::Wrap(cx, this);
}


// -----------------------------------------------------------------------------
// INSTANCING nsIDOMWebGLRenderingContext

nsresult
NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** aResult)
{
    Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_USED, 1);
    nsIDOMWebGLRenderingContext* ctx = new WebGL1Context();

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

