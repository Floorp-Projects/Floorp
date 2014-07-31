/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLFRAMEBUFFERATTACHABLE_H_
#define WEBGLFRAMEBUFFERATTACHABLE_H_

#include "GLDefs.h"
#include "nsTArray.h"
#include "mozilla/WeakPtr.h"
#include "WebGLFramebuffer.h"

namespace mozilla {

class WebGLFramebufferAttachable
{
    struct AttachmentPoint
    {
        AttachmentPoint(const WebGLFramebuffer* fb, GLenum attachment)
            : mFB(fb)
            , mAttachment(attachment)
        {}

        WeakPtr<const WebGLFramebuffer> mFB;
        GLenum mAttachment;

        bool operator==(const AttachmentPoint& o) const {
          return mFB == o.mFB && mAttachment == o.mAttachment;
        }
    };

    nsTArray<AttachmentPoint> mAttachmentPoints;

public:

    // Track FBO/Attachment combinations
    void AttachTo(WebGLFramebuffer* fb, GLenum attachment);
    void DetachFrom(WebGLFramebuffer* fb, GLenum attachment);
    void NotifyFBsStatusChanged();
};

} // namespace mozilla

#endif // !WEBGLFRAMEBUFFERATTACHABLE_H_
