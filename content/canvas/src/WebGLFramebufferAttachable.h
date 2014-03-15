/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLFRAMEBUFFERATTACHABLE_H_
#define WEBGLFRAMEBUFFERATTACHABLE_H_

#include "GLDefs.h"
#include "mozilla/Vector.h"

namespace mozilla {

class WebGLFramebuffer;

class WebGLFramebufferAttachable
{
    struct AttachmentPoint
    {
        AttachmentPoint(const WebGLFramebuffer* fb, GLenum attachment)
            : mFB(fb)
            , mAttachment(attachment)
        {}

        const WebGLFramebuffer* mFB;
        GLenum mAttachment;
    };

    Vector<AttachmentPoint> mAttachmentPoints;

    AttachmentPoint* Contains(const WebGLFramebuffer* fb, GLenum attachment);

public:

    // Track FBO/Attachment combinations
    void AttachTo(WebGLFramebuffer* fb, GLenum attachment);
    void DetachFrom(WebGLFramebuffer* fb, GLenum attachment);
    void NotifyFBsStatusChanged();
};

} // namespace mozilla

#endif // !WEBGLFRAMEBUFFERATTACHABLE_H_
