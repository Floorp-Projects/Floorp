/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

JSObject*
WebGLProgram::WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap) {
    return dom::WebGLProgramBinding::Wrap(cx, scope, this, triedToWrap);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WebGLProgram)
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(WebGLProgram)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebGLProgram)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mAttachedShaders)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebGLProgram)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_OF_NSCOMPTR(mAttachedShaders)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLProgram)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLProgram)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLProgram)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
