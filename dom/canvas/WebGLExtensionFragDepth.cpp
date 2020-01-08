/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"

namespace mozilla {

WebGLExtensionFragDepth::WebGLExtensionFragDepth(WebGLContext* webgl)
    : WebGLExtensionBase(webgl) {
  MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");
}

WebGLExtensionFragDepth::~WebGLExtensionFragDepth() {}

bool WebGLExtensionFragDepth::IsSupported(const WebGLContext* webgl) {
  if (webgl->IsWebGL2()) return false;

  gl::GLContext* gl = webgl->GL();
  if (gl->IsGLES() && gl->Version() >= 300) {
    // ANGLE's shader translator can't translate ESSL1 exts to ESSL3. (bug
    // 1524804)
    return false;
  }
  return gl->IsSupported(gl::GLFeature::frag_depth);
}

}  // namespace mozilla
