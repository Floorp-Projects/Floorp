/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLFRAMEBUFFER_H_
#define WEBGLFRAMEBUFFER_H_

#include "WebGLObjectModel.h"

#include "nsWrapperCache.h"

#include "mozilla/LinkedList.h"

namespace mozilla {

class WebGLTexture;
class WebGLRenderbuffer;
namespace gl {
    class GLContext;
}

class WebGLFramebuffer MOZ_FINAL
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLFramebuffer>
    , public LinkedListElement<WebGLFramebuffer>
    , public WebGLContextBoundObject
{
public:
    WebGLFramebuffer(WebGLContext *context);

    ~WebGLFramebuffer() {
        DeleteOnce();
    }

    struct Attachment
    {
        // deleting a texture or renderbuffer immediately detaches it
        WebGLRefPtr<WebGLTexture> mTexturePtr;
        WebGLRefPtr<WebGLRenderbuffer> mRenderbufferPtr;
        GLenum mAttachmentPoint;
        GLint mTextureLevel;
        GLenum mTextureTarget;
        GLenum mTextureCubeMapFace;

        Attachment(GLenum aAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0)
            : mAttachmentPoint(aAttachmentPoint)
        {}

        bool IsDefined() const {
            return Texture() || Renderbuffer();
        }

        bool IsDeleteRequested() const;

        bool HasAlpha() const;

        void SetTexture(WebGLTexture *tex, GLenum target, GLint level, GLenum face);
        void SetRenderbuffer(WebGLRenderbuffer *rb) {
            mTexturePtr = nullptr;
            mRenderbufferPtr = rb;
        }
        const WebGLTexture *Texture() const {
            return mTexturePtr;
        }
        WebGLTexture *Texture() {
            return mTexturePtr;
        }
        const WebGLRenderbuffer *Renderbuffer() const {
            return mRenderbufferPtr;
        }
        WebGLRenderbuffer *Renderbuffer() {
            return mRenderbufferPtr;
        }
        GLenum TextureTarget() const {
            return mTextureTarget;
        }
        GLint TextureLevel() const {
            return mTextureLevel;
        }
        GLenum TextureCubeMapFace() const {
            return mTextureCubeMapFace;
        }

        bool HasUninitializedRenderbuffer() const;

        void Reset() {
            mTexturePtr = nullptr;
            mRenderbufferPtr = nullptr;
        }

        const WebGLRectangleObject* RectangleObject() const;
        bool HasSameDimensionsAs(const Attachment& other) const;

        bool IsComplete() const;

        void FinalizeAttachment(GLenum attachmentLoc) const;
    };

    void Delete();

    bool HasEverBeenBound() { return mHasEverBeenBound; }
    void SetHasEverBeenBound(bool x) { mHasEverBeenBound = x; }
    GLuint GLName() { return mGLName; }

    void FramebufferRenderbuffer(GLenum target,
                                 GLenum attachment,
                                 GLenum rbtarget,
                                 WebGLRenderbuffer *wrb);

    void FramebufferTexture2D(GLenum target,
                              GLenum attachment,
                              GLenum textarget,
                              WebGLTexture *wtex,
                              GLint level);

    bool HasIncompleteAttachment() const;

    bool HasDepthStencilConflict() const {
        return int(mDepthAttachment.IsDefined()) +
               int(mStencilAttachment.IsDefined()) +
               int(mDepthStencilAttachment.IsDefined()) >= 2;
    }

    bool HasAttachmentsOfMismatchedDimensions() const;

    const size_t ColorAttachmentCount() const {
        return mColorAttachments.Length();
    }
    const Attachment& ColorAttachment(uint32_t colorAttachmentId) const {
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

    void DetachTexture(const WebGLTexture *tex);

    void DetachRenderbuffer(const WebGLRenderbuffer *rb);

    const WebGLRectangleObject *RectangleObject() {
        return mColorAttachments[0].RectangleObject();
    }

    WebGLContext *GetParentObject() const {
        return Context();
    }

    void FinalizeAttachments() const;

    virtual JSObject* WrapObject(JSContext *cx,
                                 JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLFramebuffer)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLFramebuffer)

    bool CheckAndInitializeRenderbuffers();

    bool CheckColorAttachementNumber(GLenum attachment, const char * functionName) const;

    GLuint mGLName;
    bool mHasEverBeenBound;

    void EnsureColorAttachments(size_t colorAttachmentId);

    // we only store pointers to attached renderbuffers, not to attached textures, because
    // we will only need to initialize renderbuffers. Textures are already initialized.
    nsTArray<Attachment> mColorAttachments;
    Attachment mDepthAttachment,
               mStencilAttachment,
               mDepthStencilAttachment;
};

} // namespace mozilla

#endif
