/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_RENDERBUFFER_H_
#define WEBGL_RENDERBUFFER_H_

#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "CacheInvalidator.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLTexture.h"

namespace mozilla {
namespace webgl {
struct FormatUsageInfo;
}

class WebGLRenderbuffer final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLRenderbuffer>
    , public LinkedListElement<WebGLRenderbuffer>
    , public WebGLRectangleObject
    , public CacheInvalidator
{
    friend class WebGLContext;
    friend class WebGLFramebuffer;
    friend class WebGLFBAttachPoint;

public:
    const GLuint mPrimaryRB;
protected:
    const bool mEmulatePackedDepthStencil;
    GLuint mSecondaryRB;
    bool mHasBeenBound;
    webgl::ImageInfo mImageInfo;

public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLRenderbuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLRenderbuffer)

    explicit WebGLRenderbuffer(WebGLContext* webgl);

    void Delete();

    const auto& ImageInfo() const { return mImageInfo; }

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    void RenderbufferStorage(uint32_t samples, GLenum internalFormat, uint32_t width,
                             uint32_t height);
    // Only handles a subset of `pname`s.
    GLint GetRenderbufferParameter(RBTarget target, RBParam pname) const;

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    auto MemoryUsage() const { return mImageInfo.MemoryUsage(); }

protected:
    ~WebGLRenderbuffer() {
        DeleteOnce();
    }

    void DoFramebufferRenderbuffer(GLenum attachment) const;
    GLenum DoRenderbufferStorage(uint32_t samples, const webgl::FormatUsageInfo* format,
                                 uint32_t width, uint32_t height);
};

} // namespace mozilla

#endif // WEBGL_RENDERBUFFER_H_
