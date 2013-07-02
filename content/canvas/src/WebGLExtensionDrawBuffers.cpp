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
    const size_t buffersLength = buffers.Length();

    if (buffersLength == 0) {
        return mContext->ErrorInvalidValue("drawBuffersWEBGL: invalid <buffers> (buffers must not be empty)");
    }

    if (mContext->mBoundFramebuffer == 0)
    {
        // OK: we are rendering in the default framebuffer

        /* EXT_draw_buffers :
         If the GL is bound to the default framebuffer, then <buffersLength> must be 1
         and the constant must be BACK or NONE. When draw buffer zero is
         BACK, color values are written into the sole buffer for single-
         buffered contexts, or into the back buffer for double-buffered
         contexts. If DrawBuffersEXT is supplied with a constant other than
         BACK and NONE, the error INVALID_OPERATION is generated.
         */
        if (buffersLength != 1) {
            return mContext->ErrorInvalidValue("drawBuffersWEBGL: invalid <buffers> (main framebuffer: buffers.length must be 1)");
        }

        mContext->MakeContextCurrent();

        if (buffers[0] == LOCAL_GL_NONE) {
            const GLenum drawBufffersCommand = LOCAL_GL_NONE;
            mContext->GL()->fDrawBuffers(1, &drawBufffersCommand);
            return;
        }
        else if (buffers[0] == LOCAL_GL_BACK) {
            const GLenum drawBufffersCommand = LOCAL_GL_COLOR_ATTACHMENT0;
            mContext->GL()->fDrawBuffers(1, &drawBufffersCommand);
            return;
        }
        return mContext->ErrorInvalidOperation("drawBuffersWEBGL: invalid operation (main framebuffer: buffers[0] must be GL_NONE or GL_BACK)");
    }

    // OK: we are rendering in a framebuffer object

    if (buffersLength > size_t(mContext->mGLMaxDrawBuffers)) {
        /* EXT_draw_buffers :
         The maximum number of draw buffers is implementation-dependent. The
         number of draw buffers supported can be queried by calling
         GetIntegerv with the symbolic constant MAX_DRAW_BUFFERS_EXT. An
         INVALID_VALUE error is generated if <buffersLength> is greater than
         MAX_DRAW_BUFFERS_EXT.
         */
        return mContext->ErrorInvalidValue("drawBuffersWEBGL: invalid <buffers> (buffers.length > GL_MAX_DRAW_BUFFERS)");
    }

    for (uint32_t i = 0; i < buffersLength; i++)
    {
        /* EXT_draw_buffers :
         If the GL is bound to a draw framebuffer object, the <i>th buffer listed
         in <bufs> must be COLOR_ATTACHMENT<i>_EXT or NONE. Specifying a
         buffer out of order, BACK, or COLOR_ATTACHMENT<m>_EXT where <m> is
         greater than or equal to the value of MAX_COLOR_ATTACHMENTS_EXT,
         will generate the error INVALID_OPERATION.
         */
        /* WEBGL_draw_buffers :
         The value of the MAX_COLOR_ATTACHMENTS_WEBGL parameter must be greater than or equal to that of the MAX_DRAW_BUFFERS_WEBGL parameter. 
         */
        if (buffers[i] != LOCAL_GL_NONE &&
            buffers[i] != GLenum(LOCAL_GL_COLOR_ATTACHMENT0 + i)) {
            return mContext->ErrorInvalidOperation("drawBuffersWEBGL: invalid operation (buffers[i] must be GL_NONE or GL_COLOR_ATTACHMENTi)");
        }
    }

    mContext->MakeContextCurrent();

    mContext->GL()->fDrawBuffers(buffersLength, buffers.Elements());
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
