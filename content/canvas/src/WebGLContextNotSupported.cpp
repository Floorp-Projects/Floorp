/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDOMWebGLRenderingContext.h"
#include "nsDOMClassInfoID.h"

#define DUMMY(func,rtype)  nsresult func (rtype ** aResult) { return NS_ERROR_FAILURE; }

DUMMY(NS_NewCanvasRenderingContextWebGL, nsIDOMWebGLRenderingContext)

DOMCI_DATA(WebGLRenderingContext, void)
DOMCI_DATA(WebGLBuffer, void)
DOMCI_DATA(WebGLTexture, void)
DOMCI_DATA(WebGLProgram, void)
DOMCI_DATA(WebGLShader, void)
DOMCI_DATA(WebGLFramebuffer, void)
DOMCI_DATA(WebGLRenderbuffer, void)
DOMCI_DATA(WebGLUniformLocation, void)
DOMCI_DATA(WebGLShaderPrecisionFormat, void)
DOMCI_DATA(WebGLActiveInfo, void)
DOMCI_DATA(WebGLExtension, void)
DOMCI_DATA(WebGLExtensionStandardDerivatives, void)
DOMCI_DATA(WebGLExtensionTextureFilterAnisotropic, void)
DOMCI_DATA(WebGLExtensionLoseContext, void)
DOMCI_DATA(WebGLExtensionCompressedTextureS3TC, void)
