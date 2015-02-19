/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_RENDERBUFFER_H_
#define WEBGL_RENDERBUFFER_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"
#include "WebGLBindableName.h"
#include "WebGLFramebufferAttachable.h"
#include "WebGLObjectModel.h"

namespace mozilla {

class WebGLRenderbuffer MOZ_FINAL
    : public nsWrapperCache
    , public WebGLBindable<RBTarget>
    , public WebGLRefCountedObject<WebGLRenderbuffer>
    , public LinkedListElement<WebGLRenderbuffer>
    , public WebGLRectangleObject
    , public WebGLContextBoundObject
    , public WebGLFramebufferAttachable
{
public:
    explicit WebGLRenderbuffer(WebGLContext* webgl);

    void Delete();

    bool HasUninitializedImageData() const {
        return mImageDataStatus == WebGLImageDataStatus::UninitializedImageData;
    }
    void SetImageDataStatus(WebGLImageDataStatus x) {
        // there is no way to go from having image data to not having any
        MOZ_ASSERT(x != WebGLImageDataStatus::NoImageData ||
                   mImageDataStatus == WebGLImageDataStatus::NoImageData);
        mImageDataStatus = x;
    }

    GLsizei Samples() const { return mSamples; }
    void SetSamples(GLsizei samples) { mSamples = samples; }

    GLenum InternalFormat() const { return mInternalFormat; }
    void SetInternalFormat(GLenum internalFormat) {
        mInternalFormat = internalFormat;
    }

    GLenum InternalFormatForGL() const { return mInternalFormatForGL; }
    void SetInternalFormatForGL(GLenum internalFormatForGL) {
        mInternalFormatForGL = internalFormatForGL;
    }

    int64_t MemoryUsage() const;

    WebGLContext* GetParentObject() const {
        return Context();
    }

    void BindRenderbuffer() const;
    void RenderbufferStorage(GLsizei samples, GLenum internalFormat,
                             GLsizei width, GLsizei height) const;
    void FramebufferRenderbuffer(FBAttachment attachment) const;
    // Only handles a subset of `pname`s.
    GLint GetRenderbufferParameter(RBTarget target, RBParam pname) const;

    virtual JSObject* WrapObject(JSContext* cx) MOZ_OVERRIDE;

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
    WebGLImageDataStatus mImageDataStatus;
    GLsizei mSamples;

    friend class WebGLFramebuffer;
};

} // namespace mozilla

#endif // WEBGL_RENDERBUFFER_H_
