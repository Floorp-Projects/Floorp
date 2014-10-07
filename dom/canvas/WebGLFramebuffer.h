/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLFRAMEBUFFER_H_
#define WEBGLFRAMEBUFFER_H_

#include "WebGLBindableName.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"

#include "nsWrapperCache.h"

#include "mozilla/LinkedList.h"

namespace mozilla {

class WebGLFramebufferAttachable;
class WebGLTexture;
class WebGLRenderbuffer;
namespace gl {
    class GLContext;
}

class WebGLFramebuffer MOZ_FINAL
    : public nsWrapperCache
    , public WebGLBindableName<FBTarget>
    , public WebGLRefCountedObject<WebGLFramebuffer>
    , public LinkedListElement<WebGLFramebuffer>
    , public WebGLContextBoundObject
    , public SupportsWeakPtr<WebGLFramebuffer>
{
public:
    MOZ_DECLARE_REFCOUNTED_TYPENAME(WebGLFramebuffer)

    explicit WebGLFramebuffer(WebGLContext* context);

    struct Attachment
    {
        // deleting a texture or renderbuffer immediately detaches it
        WebGLRefPtr<WebGLTexture> mTexturePtr;
        WebGLRefPtr<WebGLRenderbuffer> mRenderbufferPtr;
        FBAttachment mAttachmentPoint;
        TexImageTarget mTexImageTarget;
        GLint mTexImageLevel;
        mutable bool mNeedsFinalize;

        explicit Attachment(FBAttachment aAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0);
        ~Attachment();

        bool IsDefined() const;

        bool IsDeleteRequested() const;

        TexInternalFormat EffectiveInternalFormat() const;

        bool HasAlpha() const;
        bool IsReadableFloat() const;

        void SetTexImage(WebGLTexture* tex, TexImageTarget target, GLint level);
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
        GLint MipLevel() const {
            return mTexImageLevel;
        }

        bool HasUninitializedImageData() const;
        void SetImageDataStatus(WebGLImageDataStatus x);

        void Reset();

        const WebGLRectangleObject& RectangleObject() const;

        bool HasImage() const;
        bool IsComplete() const;

        void FinalizeAttachment(gl::GLContext* gl, FBAttachment attachmentLoc) const;
    };

    void Delete();

    void FramebufferRenderbuffer(FBAttachment attachment,
                                 RBTarget rbtarget,
                                 WebGLRenderbuffer* wrb);

    void FramebufferTexture2D(FBAttachment attachment,
                              TexImageTarget texImageTarget,
                              WebGLTexture* wtex,
                              GLint level);

private:
    void DetachAttachment(WebGLFramebuffer::Attachment& attachment);
    void DetachAllAttachments();
    const WebGLRectangleObject& GetAnyRectObject() const;
    Attachment* GetAttachmentOrNull(FBAttachment attachment);

public:
    bool HasDefinedAttachments() const;
    bool HasIncompleteAttachments() const;
    bool AllImageRectsMatch() const;
    FBStatus PrecheckFramebufferStatus() const;
    FBStatus CheckFramebufferStatus() const;
    GLenum GetFormatForAttachment(const WebGLFramebuffer::Attachment& attachment) const;

    bool HasDepthStencilConflict() const {
        return int(mDepthAttachment.IsDefined()) +
               int(mStencilAttachment.IsDefined()) +
               int(mDepthStencilAttachment.IsDefined()) >= 2;
    }

    size_t ColorAttachmentCount() const {
        return mColorAttachments.Length();
    }
    const Attachment& ColorAttachment(size_t colorAttachmentId) const {
        return mColorAttachments[colorAttachmentId];
    }

    const Attachment& DepthAttachment() const {
        return mDepthAttachment;
    }

    const Attachment& StencilAttachment() const {
        return mStencilAttachment;
    }

    const Attachment& DepthStencilAttachment() const {
        return mDepthStencilAttachment;
    }

    const Attachment& GetAttachment(FBAttachment attachment) const;

    void DetachTexture(const WebGLTexture* tex);

    void DetachRenderbuffer(const WebGLRenderbuffer* rb);

    const WebGLRectangleObject& RectangleObject() const;

    WebGLContext* GetParentObject() const {
        return Context();
    }

    void FinalizeAttachments() const;

    virtual JSObject* WrapObject(JSContext* cx) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLFramebuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLFramebuffer)

    // mask mirrors glClear.
    bool HasCompletePlanes(GLbitfield mask);

    bool CheckAndInitializeAttachments();

    bool CheckColorAttachmentNumber(FBAttachment attachment, const char* functionName) const;

    void EnsureColorAttachments(size_t colorAttachmentId);

    void NotifyAttachableChanged() const;

private:
    ~WebGLFramebuffer() {
        DeleteOnce();
    }

    mutable GLenum mStatus;

    // we only store pointers to attached renderbuffers, not to attached textures, because
    // we will only need to initialize renderbuffers. Textures are already initialized.
    nsTArray<Attachment> mColorAttachments;
    Attachment mDepthAttachment,
               mStencilAttachment,
               mDepthStencilAttachment;
};

} // namespace mozilla

#endif
