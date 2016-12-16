/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLShader.h"

namespace mozilla {

WebGLExtensionDebugShaders::WebGLExtensionDebugShaders(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
}

WebGLExtensionDebugShaders::~WebGLExtensionDebugShaders()
{
}

// If no source has been defined, compileShader() has not been called, or the
// translation has failed for shader, an empty string is returned; otherwise,
// return the translated source.
void
WebGLExtensionDebugShaders::GetTranslatedShaderSource(const WebGLShader& shader,
                                                      nsAString& retval) const
{
    retval.SetIsVoid(true);

    if (mIsLost) {
        mContext->ErrorInvalidOperation("%s: Extension is lost.",
                                        "getTranslatedShaderSource");
        return;
    }

    if (mContext->IsContextLost())
        return;

    if (!mContext->ValidateObject("getShaderTranslatedSource: shader", shader))
        return;

    shader.GetShaderTranslatedSource(&retval);
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDebugShaders, WEBGL_debug_shaders)

} // namespace mozilla
