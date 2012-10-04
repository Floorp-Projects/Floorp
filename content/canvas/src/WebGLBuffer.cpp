/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

JSObject*
WebGLBuffer::WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap) {
    return dom::WebGLBufferBinding::Wrap(cx, scope, this, triedToWrap);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLBuffer)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLBuffer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLBuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLBuffer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
