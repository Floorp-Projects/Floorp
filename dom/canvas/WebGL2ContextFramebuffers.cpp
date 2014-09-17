/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::dom;

// -------------------------------------------------------------------------
// Framebuffer objects

void
WebGL2Context::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                               GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                               GLbitfield mask, GLenum filter)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::FramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::GetInternalformatParameter(JSContext*, GLenum target, GLenum internalformat, GLenum pname, JS::MutableHandleValue retval)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::InvalidateFramebuffer(GLenum target, const dom::Sequence<GLenum>& attachments)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::InvalidateSubFramebuffer (GLenum target, const dom::Sequence<GLenum>& attachments,
                                         GLint x, GLint y, GLsizei width, GLsizei height)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::ReadBuffer(GLenum mode)
{
    MOZ_CRASH("Not Implemented.");
}

void
WebGL2Context::RenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat,
                                              GLsizei width, GLsizei height)
{
    MOZ_CRASH("Not Implemented.");
}
