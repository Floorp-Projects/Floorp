/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLRENDERBUFFER_H_
#define WEBGLRENDERBUFFER_H_

#include "WebGLObjectModel.h"
#include "WebGLFramebufferAttachable.h"

#include "nsWrapperCache.h"

#include "mozilla/LinkedList.h"

namespace mozilla {

class WebGLRenderbuffer MOZ_FINAL
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLRenderbuffer>
    , public LinkedListElement<WebGLRenderbuffer>
    , public WebGLRectangleObject
    , public WebGLContextBoundObject
    , public WebGLFramebufferAttachable
{
public:
    WebGLRenderbuffer(WebGLContext *context);

    void Delete();

    bool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(bool x) { mHasEverBeenBound = x; }

    bool HasUninitializedImageData() const { return mImageDataStatus == WebGLImageDataStatus::UninitializedImageData; }
    void SetImageDataStatus(WebGLImageDataStatus x) {
        // there is no way to go from having image data to not having any
        MOZ_ASSERT(x != WebGLImageDataStatus::NoImageData ||
                   mImageDataStatus == WebGLImageDataStatus::NoImageData);
        mImageDataStatus = x;
    }

    GLenum InternalFormat() const { return mInternalFormat; }
    void SetInternalFormat(GLenum aInternalFormat) { mInternalFormat = aInternalFormat; }

    GLenum InternalFormatForGL() const { return mInternalFormatForGL; }
    void SetInternalFormatForGL(GLenum aInternalFormatForGL) { mInternalFormatForGL = aInternalFormatForGL; }

    int64_t MemoryUsage() const;

    WebGLContext *GetParentObject() const {
        return Context();
    }

    void BindRenderbuffer() const;
    void RenderbufferStorage(GLenum internalFormat, GLsizei width, GLsizei height) const;
    void FramebufferRenderbuffer(GLenum attachment) const;
    // Only handles a subset of `pname`s.
    GLint GetRenderbufferParameter(GLenum target, GLenum pname) const;

    virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLRenderbuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLRenderbuffer)

protected:
    ~WebGLRenderbuffer() {
        DeleteOnce();
    }

    GLuint mPrimaryRB;
    GLuint mSecondaryRB;
    GLenum mInternalFormat;
    GLenum mInternalFormatForGL;
    bool mHasEverBeenBound;
    WebGLImageDataStatus mImageDataStatus;

    friend class WebGLFramebuffer;
};
} // namespace mozilla

#endif
