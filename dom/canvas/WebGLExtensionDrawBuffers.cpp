/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLExtensions.h"

#include <algorithm>
#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"

namespace mozilla {

WebGLExtensionDrawBuffers::WebGLExtensionDrawBuffers(WebGLContext* webgl)
    : WebGLExtensionBase(webgl)
{
    MOZ_ASSERT(IsSupported(webgl), "Don't construct extension if unsupported.");

    // WEBGL_draw_buffers:
    // "The value of the MAX_COLOR_ATTACHMENTS_WEBGL parameter must be greater than or
    //  equal to that of the MAX_DRAW_BUFFERS_WEBGL parameter."
    webgl->mImplMaxColorAttachments = webgl->mGLMaxColorAttachments;
    webgl->mImplMaxDrawBuffers = std::min(webgl->mGLMaxDrawBuffers,
                                          webgl->mImplMaxColorAttachments);
}

WebGLExtensionDrawBuffers::~WebGLExtensionDrawBuffers()
{
}

void
WebGLExtensionDrawBuffers::DrawBuffersWEBGL(const dom::Sequence<GLenum>& buffers)
{
    if (mIsLost) {
        mContext->ErrorInvalidOperation("drawBuffersWEBGL: Extension is lost.");
        return;
    }

    mContext->DrawBuffers(buffers);
}

bool
WebGLExtensionDrawBuffers::IsSupported(const WebGLContext* webgl)
{
    gl::GLContext* gl = webgl->GL();

    if (!gl->IsSupported(gl::GLFeature::draw_buffers))
        return false;

    // WEBGL_draw_buffers requires at least 4 color attachments.
    if (webgl->mGLMaxDrawBuffers < webgl->kMinMaxDrawBuffers ||
        webgl->mGLMaxColorAttachments < webgl->kMinMaxColorAttachments)
    {
        return false;
    }

    return true;
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDrawBuffers, WEBGL_draw_buffers)

} // namespace mozilla
