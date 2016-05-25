/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

namespace mozilla {

WebGLExtensionBase::WebGLExtensionBase(WebGLContext* context)
    : WebGLContextBoundObject(context)
    , mIsLost(false)
{
}

WebGLExtensionBase::~WebGLExtensionBase()
{
}

void
WebGLExtensionBase::MarkLost()
{
    mIsLost = true;

    OnMarkLost();
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLExtensionBase)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLExtensionBase, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLExtensionBase, Release)

} // namespace mozilla
