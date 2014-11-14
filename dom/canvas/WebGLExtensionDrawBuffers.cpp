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

    GLint maxColorAttachments = 0;
    GLint maxDrawBuffers = 0;

    webgl->MakeContextCurrent();
    gl::GLContext* gl = webgl->GL();

    gl->fGetIntegerv(LOCAL_GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    gl->fGetIntegerv(LOCAL_GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);

    // WEBGL_draw_buffers specifications don't give a maximal value reachable by MAX_COLOR_ATTACHMENTS.
    maxColorAttachments = std::min(maxColorAttachments, GLint(WebGLContext::kMaxColorAttachments));

    if (webgl->MinCapabilityMode())
        maxColorAttachments = std::min(maxColorAttachments, GLint(kMinColorAttachments));

    // WEBGL_draw_buffers specifications request MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    maxDrawBuffers = std::min(maxDrawBuffers, GLint(maxColorAttachments));

    webgl->mGLMaxColorAttachments = maxColorAttachments;
    webgl->mGLMaxDrawBuffers = maxDrawBuffers;
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

    GLint supportedColorAttachments = 0;
    GLint supportedDrawBuffers = 0;

    webgl->MakeContextCurrent();

    gl->fGetIntegerv(LOCAL_GL_MAX_COLOR_ATTACHMENTS, &supportedColorAttachments);
    gl->fGetIntegerv(LOCAL_GL_MAX_COLOR_ATTACHMENTS, &supportedDrawBuffers);

    // WEBGL_draw_buffers requires at least 4 color attachments.
    if (size_t(supportedColorAttachments) < kMinColorAttachments)
        return false;

    if (size_t(supportedDrawBuffers) < kMinDrawBuffers)
        return false;

    return true;
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDrawBuffers)

} // namespace mozilla
