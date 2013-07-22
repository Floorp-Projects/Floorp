/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLTexture.h"
#include "WebGLRenderbuffer.h"
#include "WebGLFramebuffer.h"

#include <algorithm>

using namespace mozilla;
using namespace gl;

WebGLExtensionDrawBuffers::WebGLExtensionDrawBuffers(WebGLContext* context)
    : WebGLExtensionBase(context)
{
    GLint maxColorAttachments = 0;
    GLint maxDrawBuffers = 0;

    gl::GLContext* gl = context->GL();

    context->MakeContextCurrent();

    gl->fGetIntegerv(LOCAL_GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    gl->fGetIntegerv(LOCAL_GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);

    // WEBGL_draw_buffers specifications don't give a maximal value reachable by MAX_COLOR_ATTACHMENTS.
    maxColorAttachments = std::min(maxColorAttachments, GLint(WebGLContext::sMaxColorAttachments));

    if (context->MinCapabilityMode())
    {
        maxColorAttachments = std::min(maxColorAttachments, GLint(sMinColorAttachments));
    }

    // WEBGL_draw_buffers specifications request MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    maxDrawBuffers = std::min(maxDrawBuffers, GLint(maxColorAttachments));

    context->mGLMaxColorAttachments = maxColorAttachments;
    context->mGLMaxDrawBuffers = maxDrawBuffers;
}

WebGLExtensionDrawBuffers::~WebGLExtensionDrawBuffers()
{
}

void WebGLExtensionDrawBuffers::DrawBuffersWEBGL(const dom::Sequence<GLenum>& buffers)
{
    mContext->DrawBuffers(buffers);
}

bool WebGLExtensionDrawBuffers::IsSupported(const WebGLContext* context)
{
    gl::GLContext * gl = context->GL();

    if (!gl->IsExtensionSupported(gl->IsGLES2() ? GLContext::EXT_draw_buffers
                                                : GLContext::ARB_draw_buffers)) {
        return false;
    }

    GLint supportedColorAttachments = 0;
    GLint supportedDrawBuffers = 0;

    context->MakeContextCurrent();

    gl->fGetIntegerv(LOCAL_GL_MAX_COLOR_ATTACHMENTS, &supportedColorAttachments);
    gl->fGetIntegerv(LOCAL_GL_MAX_COLOR_ATTACHMENTS, &supportedDrawBuffers);

    if (size_t(supportedColorAttachments) < sMinColorAttachments){
        // WEBGL_draw_buffers specifications request for 4 color attachments at least.
        return false;
    }

    if (size_t(supportedDrawBuffers) < sMinDrawBuffers){
        return false;
    }

    return true;
}

IMPL_WEBGL_EXTENSION_GOOP(WebGLExtensionDrawBuffers)
