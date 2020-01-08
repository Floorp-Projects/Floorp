/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionElementIndexUint::WebGLExtensionElementIndexUint(
    WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {}

WebGLExtensionElementIndexUint::~WebGLExtensionElementIndexUint() {}

}  // namespace mozilla
