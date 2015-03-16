/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_FRAMEBUFFER_ATTACHABLE_H_
#define WEBGL_FRAMEBUFFER_ATTACHABLE_H_

#include "GLDefs.h"
#include "mozilla/WeakPtr.h"
#include "nsTArray.h"
#include "WebGLFramebuffer.h"
#include "WebGLStrongTypes.h"

namespace mozilla {

class WebGLFramebufferAttachable
{
    nsTArray<const WebGLFramebuffer::AttachPoint*> mAttachmentPoints;

public:
    // Track FBO/Attachment combinations
    void MarkAttachment(const WebGLFramebuffer::AttachPoint& attachment);
    void UnmarkAttachment(const WebGLFramebuffer::AttachPoint& attachment);
    void InvalidateStatusOfAttachedFBs() const;
};

} // namespace mozilla

#endif // !WEBGLFRAMEBUFFERATTACHABLE_H_
