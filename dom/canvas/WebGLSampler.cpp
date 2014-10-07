/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLSampler.h"

#include "GLContext.h"

#include "mozilla/dom/WebGL2RenderingContextBinding.h"

using namespace mozilla;

WebGLSampler::WebGLSampler(WebGLContext* context)
    : WebGLContextBoundObject(context)
{
    MOZ_CRASH("Not Implemented.");
}

WebGLSampler::~WebGLSampler()
{}

void
WebGLSampler::Delete()
{
    MOZ_CRASH("Not Implemented.");
}

WebGLContext*
WebGLSampler::GetParentObject() const
{
    MOZ_CRASH("Not Implemented.");
    return nullptr;
}

JSObject*
WebGLSampler::WrapObject(JSContext* cx)
{
    MOZ_CRASH("Not Implemented.");
    return dom::WebGLSamplerBinding::Wrap(cx, this);
}


NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLSampler)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLSampler, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLSampler, Release)
