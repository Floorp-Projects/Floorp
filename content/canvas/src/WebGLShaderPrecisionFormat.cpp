/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLShaderPrecisionFormat.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

NS_INTERFACE_MAP_BEGIN(WebGLShaderPrecisionFormat)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShaderPrecisionFormat)
NS_IMPL_RELEASE(WebGLShaderPrecisionFormat)

JSObject*
WebGLShaderPrecisionFormat::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
    return dom::WebGLShaderPrecisionFormatBinding::Wrap(cx, scope, this);
}
