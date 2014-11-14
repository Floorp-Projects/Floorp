/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "WebGLTransformFeedback.h"

#include "GLContext.h"

#include "mozilla/dom/WebGL2RenderingContextBinding.h"

using namespace mozilla;

WebGLTransformFeedback::WebGLTransformFeedback(WebGLContext* context)
    : WebGLContextBoundObject(context)
{
    MOZ_CRASH("Not Implemented.");
}

WebGLTransformFeedback::~WebGLTransformFeedback()
{}

void
WebGLTransformFeedback::Delete()
{
    MOZ_CRASH("Not Implemented.");
}

WebGLContext*
WebGLTransformFeedback::GetParentObject() const
{
    MOZ_CRASH("Not Implemented.");
    return nullptr;
}

JSObject*
WebGLTransformFeedback::WrapObject(JSContext* cx)
{
    return dom::WebGLTransformFeedbackBinding::Wrap(cx, this);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTransformFeedback)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTransformFeedback, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTransformFeedback, Release)
