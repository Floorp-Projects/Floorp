/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGL2Context.h"

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "mozilla/CheckedInt.h"
#include "WebGLContextUtils.h"
#include "WebGLFormats.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

void WebGL2Context::BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1,
                                    GLint srcY1, GLint dstX0, GLint dstY0,
                                    GLint dstX1, GLint dstY1, GLbitfield mask,
                                    GLenum filter) {
  const FuncScope funcScope(*this, "blitFramebuffer");
  if (IsContextLost()) return;

  const GLbitfield validBits = LOCAL_GL_COLOR_BUFFER_BIT |
                               LOCAL_GL_DEPTH_BUFFER_BIT |
                               LOCAL_GL_STENCIL_BUFFER_BIT;
  if ((mask | validBits) != validBits) {
    ErrorInvalidValue("Invalid bit set in mask.");
    return;
  }

  switch (filter) {
    case LOCAL_GL_NEAREST:
    case LOCAL_GL_LINEAR:
      break;
    default:
      ErrorInvalidEnumInfo("filter", filter);
      return;
  }

  // --

  const auto fnLikelyOverflow = [](GLint p0, GLint p1) {
    auto checked = CheckedInt<GLint>(p1) - p0;
    checked = -checked;  // And check the negation!
    return !checked.isValid();
  };

  if (fnLikelyOverflow(srcX0, srcX1) || fnLikelyOverflow(srcY0, srcY1) ||
      fnLikelyOverflow(dstX0, dstX1) || fnLikelyOverflow(dstY0, dstY1)) {
    ErrorInvalidValue("Likely-to-overflow large ranges are forbidden.");
    return;
  }

  // --

  if (!ValidateAndInitFB(mBoundReadFramebuffer) ||
      !ValidateAndInitFB(mBoundDrawFramebuffer)) {
    return;
  }

  DoBindFB(mBoundReadFramebuffer, LOCAL_GL_READ_FRAMEBUFFER);
  DoBindFB(mBoundDrawFramebuffer, LOCAL_GL_DRAW_FRAMEBUFFER);

  WebGLFramebuffer::BlitFramebuffer(this, srcX0, srcY0, srcX1, srcY1, dstX0,
                                    dstY0, dstX1, dstY1, mask, filter);
}

////

static bool ValidateBackbufferAttachmentEnum(WebGLContext* webgl,
                                             GLenum attachment) {
  switch (attachment) {
    case LOCAL_GL_COLOR:
    case LOCAL_GL_DEPTH:
    case LOCAL_GL_STENCIL:
      return true;

    default:
      webgl->ErrorInvalidEnumInfo("attachment", attachment);
      return false;
  }
}

static bool ValidateFramebufferAttachmentEnum(WebGLContext* webgl,
                                              GLenum attachment) {
  switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
    case LOCAL_GL_STENCIL_ATTACHMENT:
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
      return true;
  }

  if (attachment < LOCAL_GL_COLOR_ATTACHMENT0) {
    webgl->ErrorInvalidEnumInfo("attachment", attachment);
    return false;
  }

  if (attachment > webgl->LastColorAttachmentEnum()) {
    // That these errors have different types is ridiculous.
    webgl->ErrorInvalidOperation("Too-large LOCAL_GL_COLOR_ATTACHMENTn.");
    return false;
  }

  return true;
}

bool WebGLContext::ValidateInvalidateFramebuffer(
    GLenum target, const Range<const GLenum>& attachments,
    std::vector<GLenum>* const scopedVector,
    GLsizei* const out_glNumAttachments,
    const GLenum** const out_glAttachments) {
  if (IsContextLost()) return false;

  if (!ValidateFramebufferTarget(target)) return false;

  const WebGLFramebuffer* fb;
  bool isDefaultFB = false;
  switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
      fb = mBoundDrawFramebuffer;
      break;

    case LOCAL_GL_READ_FRAMEBUFFER:
      fb = mBoundReadFramebuffer;
      break;

    default:
      MOZ_CRASH("GFX: Bad target.");
  }

  if (fb) {
    const auto fbStatus = fb->CheckFramebufferStatus();
    if (fbStatus != LOCAL_GL_FRAMEBUFFER_COMPLETE)
      return false;  // Not an error, but don't run forward to driver either.
  } else {
    if (!EnsureDefaultFB()) return false;
  }
  DoBindFB(fb, target);

  *out_glNumAttachments = attachments.length();
  *out_glAttachments = attachments.begin().get();

  if (fb) {
    for (const auto& attachment : attachments) {
      if (!ValidateFramebufferAttachmentEnum(this, attachment)) return false;
    }
  } else {
    for (const auto& attachment : attachments) {
      if (!ValidateBackbufferAttachmentEnum(this, attachment)) return false;
    }

    if (!isDefaultFB) {
      MOZ_ASSERT(scopedVector->empty());
      scopedVector->reserve(attachments.length());
      for (const auto& attachment : attachments) {
        switch (attachment) {
          case LOCAL_GL_COLOR:
            scopedVector->push_back(LOCAL_GL_COLOR_ATTACHMENT0);
            break;

          case LOCAL_GL_DEPTH:
            scopedVector->push_back(LOCAL_GL_DEPTH_ATTACHMENT);
            break;

          case LOCAL_GL_STENCIL:
            scopedVector->push_back(LOCAL_GL_STENCIL_ATTACHMENT);
            break;

          default:
            MOZ_CRASH();
        }
      }
      *out_glNumAttachments = scopedVector->size();
      *out_glAttachments = scopedVector->data();
    }
  }

  ////

  return true;
}

void WebGL2Context::InvalidateFramebuffer(
    GLenum target, const Range<const GLenum>& attachments) {
  const FuncScope funcScope(*this, "invalidateFramebuffer");

  std::vector<GLenum> scopedVector;
  GLsizei glNumAttachments;
  const GLenum* glAttachments;
  if (!ValidateInvalidateFramebuffer(target, attachments, &scopedVector,
                                     &glNumAttachments, &glAttachments)) {
    return;
  }

  ////

  // Some drivers (like OSX 10.9 GL) just don't support invalidate_framebuffer.
  const bool useFBInvalidation =
      (mAllowFBInvalidation &&
       gl->IsSupported(gl::GLFeature::invalidate_framebuffer));
  if (useFBInvalidation) {
    gl->fInvalidateFramebuffer(target, glNumAttachments, glAttachments);
    return;
  }

  // Use clear instead?
  // No-op for now.
}

void WebGL2Context::InvalidateSubFramebuffer(
    GLenum target, const Range<const GLenum>& attachments, GLint x, GLint y,
    GLsizei width, GLsizei height) {
  const FuncScope funcScope(*this, "invalidateSubFramebuffer");

  std::vector<GLenum> scopedVector;
  GLsizei glNumAttachments;
  const GLenum* glAttachments;
  if (!ValidateInvalidateFramebuffer(target, attachments, &scopedVector,
                                     &glNumAttachments, &glAttachments)) {
    return;
  }

  if (!ValidateNonNegative("width", width) ||
      !ValidateNonNegative("height", height)) {
    return;
  }

  ////

  // Some drivers (like OSX 10.9 GL) just don't support invalidate_framebuffer.
  const bool useFBInvalidation =
      (mAllowFBInvalidation &&
       gl->IsSupported(gl::GLFeature::invalidate_framebuffer));
  if (useFBInvalidation) {
    gl->fInvalidateSubFramebuffer(target, glNumAttachments, glAttachments, x, y,
                                  width, height);
    return;
  }

  // Use clear instead?
  // No-op for now.
}

void WebGL2Context::ReadBuffer(GLenum mode) {
  const FuncScope funcScope(*this, "readBuffer");
  if (IsContextLost()) return;

  if (mBoundReadFramebuffer) {
    mBoundReadFramebuffer->ReadBuffer(mode);
    return;
  }

  // Operating on the default framebuffer.
  if (mode != LOCAL_GL_NONE && mode != LOCAL_GL_BACK) {
    nsCString enumName;
    EnumName(mode, &enumName);
    ErrorInvalidOperation(
        "If READ_FRAMEBUFFER is null, `mode` must be BACK or"
        " NONE. Was %s.",
        enumName.BeginReading());
    return;
  }

  mDefaultFB_ReadBuffer = mode;
}

}  // namespace mozilla
