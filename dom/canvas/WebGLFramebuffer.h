/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLFRAMEBUFFER_H_
#define WEBGLFRAMEBUFFER_H_

#include "WebGLBindableName.h"
#include "WebGLObjectModel.h"

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
    , public WebGLBindableName
    , public WebGLRefCountedObject<WebGLFramebuffer>
    , public LinkedListElement<WebGLFramebuffer>
    , public WebGLContextBoundObject
    , public SupportsWeakPtr<WebGLFramebuffer>
{
public:
    MOZ_DECLARE_REFCOUNTED_TYPENAME(WebGLFramebuffer)

    WebGLFramebuffer(WebGLContext* context);

    struct Attachment
    {
        // deleting a texture or renderbuffer immediately detaches it
        WebGLRefPtr<WebGLTexture> mTexturePtr;
        WebGLRefPtr<WebGLRenderbuffer> mRenderbufferPtr;
        GLenum mAttachmentPoint;
        GLenum mTexImageTarget;
        GLint mTexImageLevel;
        mutable bool mNeedsFinalize;

        Attachment(GLenum aAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0);
        ~Attachment();

        bool IsDefined() const {
            return Texture() || Renderbuffer();
        }

        bool IsDeleteRequested() const;

        bool HasAlpha() const;
        bool IsReadableFloat() const;

        void SetTexImage(WebGLTexture* tex, GLenum target, GLint level);
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
        GLenum TexImageTarget() const {
            return mTexImageTarget;
        }
        GLint TexImageLevel() const {
            return mTexImageLevel;
        }

        bool HasUninitializedImageData() const;
        void SetImageDataStatus(WebGLImageDataStatus x);

        void Reset();

        const WebGLRectangleObject& RectangleObject() const;

        bool HasImage() const;
        bool IsComplete() const;

        void FinalizeAttachment(gl::GLContext* gl, GLenum attachmentLoc) const;
    };

    void Delete();

    void FramebufferRenderbuffer(GLenum target,
                                 GLenum attachment,
                                 GLenum rbtarget,
                                 WebGLRenderbuffer* wrb);

    void FramebufferTexture2D(GLenum target,
                              GLenum attachment,
                              GLenum textarget,
                              WebGLTexture* wtex,
                              GLint level);

private:
    void DetachAttachment(WebGLFramebuffer::Attachment& attachment);
    void DetachAllAttachments();
    const WebGLRectangleObject& GetAnyRectObject() const;
    Attachment* GetAttachmentOrNull(GLenum attachment);

public:
    bool HasDefinedAttachments() const;
    bool HasIncompleteAttachments() const;
    bool AllImageRectsMatch() const;
    GLenum PrecheckFramebufferStatus() const;
    GLenum CheckFramebufferStatus() const;
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

    const Attachment& GetAttachment(GLenum attachment) const;

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

    bool CheckColorAttachmentNumber(GLenum attachment, const char* functionName) const;

    void EnsureColorAttachments(size_t colorAttachmentId);

    Attachment* AttachmentFor(GLenum attachment);
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
