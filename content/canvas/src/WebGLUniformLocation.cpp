/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

JSObject*
WebGLUniformLocation::WrapObject(JSContext *cx, JSObject *scope)
{
    return dom::WebGLUniformLocationBinding::Wrap(cx, scope, this);
}

WebGLUniformLocation::WebGLUniformLocation(WebGLContext *context, WebGLProgram *program, GLint location, const WebGLUniformInfo& info)
    : WebGLContextBoundObject(context)
    , mProgram(program)
    , mProgramGeneration(program->Generation())
    , mLocation(location)
    , mInfo(info)
{
    mElementSize = info.ElementSize();
}

NS_IMPL_CYCLE_COLLECTION_1(WebGLUniformLocation, mProgram)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLUniformLocation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLUniformLocation)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
