/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "mozilla/dom/WebGL2RenderingContextBinding.h"

#include "mozilla/Telemetry.h"

using namespace mozilla;

// -----------------------------------------------------------------------------
// CONSTRUCTOR & DESTRUCTOR

WebGL2Context::WebGL2Context()
    : WebGLContext()
{
    MOZ_ASSERT(IsSupported(), "not supposed to create a WebGL2Context"
                              "context when not supported");
}

WebGL2Context::~WebGL2Context()
{

}


// -----------------------------------------------------------------------------
// STATIC FUNCTIONS

bool
WebGL2Context::IsSupported()
{
#ifdef RELEASE_BUILD
    return false;
#else
    return Preferences::GetBool("webgl.enable-prototype-webgl2", false);
#endif
}

WebGL2Context*
WebGL2Context::Create()
{
#ifdef RELEASE_BUILD
    return nullptr;
#else
    return new WebGL2Context();
#endif
}


// -----------------------------------------------------------------------------
// IMPLEMENT nsWrapperCache

JSObject*
WebGL2Context::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
    return dom::WebGL2RenderingContextBinding::Wrap(cx, scope, this);
}

