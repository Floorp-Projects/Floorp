/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_FRAMEBUFFER_H_
#define WEBGL_FRAMEBUFFER_H_

#include <vector>

#include "mozilla/LinkedList.h"
#include "mozilla/WeakPtr.h"
#include "nsWrapperCache.h"

#include "WebGLObjectModel.h"
#include "WebGLRenderbuffer.h"
#include "WebGLStrongTypes.h"
#include "WebGLTexture.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLFramebuffer;
class WebGLRenderbuffer;
class WebGLTexture;

template<typename T>
class PlacementArray;

class WebGLFBAttachPoint
{
public:
    WebGLFramebuffer* const mFB;
    const GLenum mAttachmentPoint;
private:
    WebGLRefPtr<WebGLTexture> mTexturePtr;
    WebGLRefPtr<WebGLRenderbuffer> mRenderbufferPtr;
    TexImageTarget mTexImageTarget;
    GLint mTexImageLayer;
    uint32_t mTexImageLevel;

    // PlacementArray needs a default constructor.
    template<typename T>
    friend class PlacementArray;

    WebGLFBAttachPoint()
        : mFB(nullptr)
        , mAttachmentPoint(0)
    { }

public:
    WebGLFBAttachPoint(WebGLFramebuffer* fb, GLenum attachmentPoint);
    ~WebGLFBAttachPoint();

    void Unlink();

    bool IsDefined() const;
    bool IsDeleteRequested() const;

    const webgl::FormatUsageInfo* Format() const;
    uint32_t Samples() const;

    bool HasAlpha() const;
    bool IsReadableFloat() const;

    void Clear();

    void SetTexImage(WebGLTexture* tex, TexImageTarget target, GLint level);
    void SetTexImageLayer(WebGLTexture* tex, TexImageTarget target, GLint level,
                          GLint layer);
    void SetRenderbuffer(WebGLRenderbuffer* rb);

    const WebGLTexture* Texture() const {
        return mTexturePtr;
    }
    WebGLTexture* Texture() {
        return mTexturePtr;
    }
    const WebGLRenderbuffer* Renderbuffer() const {
        return mRenderbufferPtr;
    }
    WebGLRenderbuffer* Renderbuffer() {
        return mRenderbufferPtr;
    }
    TexImageTarget ImageTarget() const {
        return mTexImageTarget;
    }
    GLint Layer() const {
        return mTexImageLayer;
    }
    uint32_t MipLevel() const {
        return mTexImageLevel;
    }
    void AttachmentName(nsCString* out) const;

    bool HasUninitializedImageData() const;
    void SetImageDataStatus(WebGLImageDataStatus x);

    void Size(uint32_t* const out_width, uint32_t* const out_height) const;

    bool HasImage() const;
    bool IsComplete(WebGLContext* webgl, nsCString* const out_info) const;

    void FinalizeAttachment(gl::GLContext* gl, GLenum attachmentLoc) const;

    JS::Value GetParameter(const char* funcName, WebGLContext* webgl, JSContext* cx,
                           GLenum target, GLenum attachment, GLenum pname,
                           ErrorResult* const out_error);

    void OnBackingStoreRespecified() const;
};

template<typename T>
class PlacementArray
{
public:
    const size_t mCapacity;
protected:
    size_t mSize;
    T* const mArray;

public:
    explicit PlacementArray(size_t capacity)
        : mCapacity(capacity)
        , mSize(0)
        , mArray((T*)moz_xmalloc(sizeof(T) * capacity))
    { }

    ~PlacementArray() {
        for (auto& cur : *this) {
            cur.~T();
        }
        free(mArray);
    }

    T* begin() const {
        return mArray;
    }

    T* end() const {
        return mArray + mSize;
    }

    T& operator [](size_t offset) const {
        MOZ_ASSERT(offset < mSize);
        return mArray[offset];
    }

    const size_t& Size() const { return mSize; }

    template<typename A, typename B>
    void AppendNew(A a, B b) {
        if (mSize == mCapacity)
            MOZ_CRASH("GFX: Bad EmplaceAppend.");

        // Placement `new`:
        new (&(mArray[mSize])) T(a, b);
        ++mSize;
    }
};

class WebGLFramebuffer final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLFramebuffer>
    , public LinkedListElement<WebGLFramebuffer>
    , public WebGLContextBoundObject
    , public SupportsWeakPtr<WebGLFramebuffer>
{
    friend class WebGLContext;

public:
    MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebGLFramebuffer)

    const GLuint mGLName;

private:
    mutable bool mIsKnownFBComplete;

    GLenum mReadBufferMode;

    // No need to chase pointers for the oft-used color0.
    WebGLFBAttachPoint mColorAttachment0;
    WebGLFBAttachPoint mDepthAttachment;
    WebGLFBAttachPoint mStencilAttachment;
    WebGLFBAttachPoint mDepthStencilAttachment;

    PlacementArray<WebGLFBAttachPoint> mMoreColorAttachments;

    std::vector<GLenum> mDrawBuffers;

    bool IsDrawBuffer(size_t n) const {
        if (n < mDrawBuffers.size())
            return bool(mDrawBuffers[n]);

        return false;
    }

#ifdef ANDROID
    // Bug 1140459: Some drivers (including our test slaves!) don't
    // give reasonable answers for IsRenderbuffer, maybe others.
    // This shows up on Android 2.3 emulator.
    //
    // So we track the `is a Framebuffer` state ourselves.
    bool mIsFB;
#endif

public:
    WebGLFramebuffer(WebGLContext* webgl, GLuint fbo);

private:
    ~WebGLFramebuffer() {
        DeleteOnce();
    }

    const WebGLRectangleObject& GetAnyRectObject() const;

public:
    void Delete();

    void FramebufferRenderbuffer(GLenum attachment, RBTarget rbtarget,
                                 WebGLRenderbuffer* rb);
    void FramebufferTexture2D(GLenum attachment, TexImageTarget texImageTarget,
                              WebGLTexture* tex, GLint level);
    void FramebufferTextureLayer(GLenum attachment, WebGLTexture* tex, GLint level,
                                 GLint layer);

    bool HasDefinedAttachments() const;
    bool HasIncompleteAttachments(nsCString* const out_info) const;
    bool AllImageRectsMatch() const;
    bool AllImageSamplesMatch() const;
    FBStatus PrecheckFramebufferStatus(nsCString* const out_info) const;
    FBStatus CheckFramebufferStatus(nsCString* const out_info) const;

    const webgl::FormatUsageInfo*
    GetFormatForAttachment(const WebGLFBAttachPoint& attachment) const;

    const WebGLFBAttachPoint& ColorAttachment(size_t colorAttachmentId) const {
        MOZ_ASSERT(colorAttachmentId < 1 + mMoreColorAttachments.Size());
        return colorAttachmentId ? mMoreColorAttachments[colorAttachmentId - 1]
                                 : mColorAttachment0;
    }

    const WebGLFBAttachPoint& DepthAttachment() const {
        return mDepthAttachment;
    }

    const WebGLFBAttachPoint& StencilAttachment() const {
        return mStencilAttachment;
    }

    const WebGLFBAttachPoint& DepthStencilAttachment() const {
        return mDepthStencilAttachment;
    }

    void SetReadBufferMode(GLenum readBufferMode) {
        mReadBufferMode = readBufferMode;
    }

    GLenum ReadBufferMode() const { return mReadBufferMode; }

    void GatherAttachments(std::vector<const WebGLFBAttachPoint*>* const out) const;

protected:
    WebGLFBAttachPoint* GetAttachPoint(GLenum attachment); // Fallible

public:
    void DetachTexture(const WebGLTexture* tex);

    void DetachRenderbuffer(const WebGLRenderbuffer* rb);

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    void FinalizeAttachments() const;

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLFramebuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLFramebuffer)

    bool ValidateAndInitAttachments(const char* funcName);

    void InvalidateFramebufferStatus() const {
        mIsKnownFBComplete = false;
    }

    bool ValidateForRead(const char* info,
                         const webgl::FormatUsageInfo** const out_format,
                         uint32_t* const out_width, uint32_t* const out_height);

    JS::Value GetAttachmentParameter(const char* funcName, JSContext* cx, GLenum target,
                                     GLenum attachment, GLenum pname,
                                     ErrorResult* const out_error);
};

} // namespace mozilla

#endif // WEBGL_FRAMEBUFFER_H_
