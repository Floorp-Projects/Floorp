/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLSync.h"

#include "mozilla/dom/WebGL2RenderingContextBinding.h"

using namespace mozilla;

WebGLSync::WebGLSync(WebGLContext* context) :
    WebGLContextBoundObject(context)
{
    MOZ_CRASH("Not Implemented.");
}

WebGLSync::~WebGLSync()
{}

void
WebGLSync::Delete()
{
    MOZ_CRASH("Not Implemented.");
}

WebGLContext*
WebGLSync::GetParentObject() const
{
    MOZ_CRASH("Not Implemented.");
    return nullptr;
}

// -------------------------------------------------------------------------
// IMPLEMENT NS
JSObject*
WebGLSync::WrapObject(JSContext *cx)
{
    return dom::WebGLSyncBinding::Wrap(cx, this);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLSync)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLSync, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLSync, Release);
