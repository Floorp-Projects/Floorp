/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;

WebGLExtensionDebugShaders::WebGLExtensionDebugShaders(WebGLContext* context)
    : WebGLExtensionBase(context)
{
}

WebGLExtensionDebugShaders::~WebGLExtensionDebugShaders()
{
}

/* If no source has been defined, compileShader() has not been called,
 * or the translation has failed for shader, an empty string is
 * returned; otherwise, return the translated source.
 */
void
WebGLExtensionDebugShaders::GetTranslatedShaderSource(WebGLShader* shader,
                                                      nsAString& retval)
{
    if (mIsLost) {
        return mContext->ErrorInvalidOperation("getTranslatedShaderSource: "
                                               "Extension is lost.");
    }

    mContext->GetShaderTranslatedSource(shader, retval);

    if (retval.IsVoid()) {
        CopyASCIItoUTF16("", retval);
    }
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDebugShaders)
