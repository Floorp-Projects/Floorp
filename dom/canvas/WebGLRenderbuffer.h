/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_RENDERBUFFER_H_
#define WEBGL_RENDERBUFFER_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "WebGLFramebufferAttachable.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"

namespace mozilla {
namespace webgl {
struct FormatUsageInfo;
}

class WebGLRenderbuffer final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLRenderbuffer>
    , public LinkedListElement<WebGLRenderbuffer>
    , public WebGLRectangleObject
    , public WebGLContextBoundObject
    , public WebGLFramebufferAttachable
{
    friend class WebGLContext;
    friend class WebGLFramebuffer;
    friend class WebGLFBAttachPoint;

public:
    const GLuint mPrimaryRB;
protected:
    const bool mEmulatePackedDepthStencil;
    GLuint mSecondaryRB;
    const webgl::FormatUsageInfo* mFormat;
    GLsizei mSamples;

    WebGLImageDataStatus mImageDataStatus;

    bool mHasBeenBound;

public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLRenderbuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLRenderbuffer)

    explicit WebGLRenderbuffer(WebGLContext* webgl);

    void Delete();

    bool HasUninitializedImageData() const {
        MOZ_ASSERT(mImageDataStatus != WebGLImageDataStatus::NoImageData);
        return mImageDataStatus == WebGLImageDataStatus::UninitializedImageData;
    }

    bool IsDefined() const {
        if (!mFormat) {
            MOZ_ASSERT(!mWidth && !mHeight);
            return false;
        }
        return true;
    }

    GLsizei Samples() const { return mSamples; }

    const webgl::FormatUsageInfo* Format() const { return mFormat; }

    int64_t MemoryUsage() const;

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    void RenderbufferStorage(const char* funcName, uint32_t samples,
                             GLenum internalFormat, uint32_t width, uint32_t height);
    // Only handles a subset of `pname`s.
    GLint GetRenderbufferParameter(RBTarget target, RBParam pname) const;

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

protected:
    ~WebGLRenderbuffer() {
        DeleteOnce();
    }

    void DoFramebufferRenderbuffer(FBTarget target, GLenum attachment) const;
    GLenum DoRenderbufferStorage(uint32_t samples, const webgl::FormatUsageInfo* format,
                                 uint32_t width, uint32_t height);
};

} // namespace mozilla

#endif // WEBGL_RENDERBUFFER_H_
