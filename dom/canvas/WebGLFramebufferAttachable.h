/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_FRAMEBUFFER_ATTACHABLE_H_
#define WEBGL_FRAMEBUFFER_ATTACHABLE_H_

#include "nsTArray.h"

namespace mozilla {
class WebGLFBAttachPoint;

class WebGLFramebufferAttachable
{
    nsTArray<const WebGLFBAttachPoint*> mAttachmentPoints;

public:
    // Track FBO/Attachment combinations
    void MarkAttachment(const WebGLFBAttachPoint& attachment);
    void UnmarkAttachment(const WebGLFBAttachPoint& attachment);
    void InvalidateStatusOfAttachedFBs() const;
};

} // namespace mozilla

#endif // !WEBGLFRAMEBUFFERATTACHABLE_H_
