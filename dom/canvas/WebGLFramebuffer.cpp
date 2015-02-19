/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLFramebuffer.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"

namespace mozilla {

JSObject*
WebGLFramebuffer::WrapObject(JSContext* cx)
{
    return dom::WebGLFramebufferBinding::Wrap(cx, this);
}

WebGLFramebuffer::WebGLFramebuffer(WebGLContext* webgl, GLuint fbo)
    : WebGLBindableName<FBTarget>(fbo)
    , WebGLContextBoundObject(webgl)
    , mStatus(0)
    , mDepthAttachment(LOCAL_GL_DEPTH_ATTACHMENT)
    , mStencilAttachment(LOCAL_GL_STENCIL_ATTACHMENT)
    , mDepthStencilAttachment(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
    , mReadBufferMode(LOCAL_GL_COLOR_ATTACHMENT0)
{
    mContext->mFramebuffers.insertBack(this);

    mColorAttachments.SetLength(1);
    mColorAttachments[0].mAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0;
}

WebGLFramebuffer::Attachment::Attachment(FBAttachment attachmentPoint)
    : mAttachmentPoint(attachmentPoint)
    , mTexImageTarget(LOCAL_GL_NONE)
    , mNeedsFinalize(false)
{}

WebGLFramebuffer::Attachment::~Attachment()
{}

void
WebGLFramebuffer::Attachment::Reset()
{
    mTexturePtr = nullptr;
    mRenderbufferPtr = nullptr;
}

bool
WebGLFramebuffer::Attachment::IsDeleteRequested() const
{
    return Texture() ? Texture()->IsDeleteRequested()
         : Renderbuffer() ? Renderbuffer()->IsDeleteRequested()
         : false;
}

bool
WebGLFramebuffer::Attachment::IsDefined() const
{
    return Renderbuffer() ||
           (Texture() && Texture()->HasImageInfoAt(ImageTarget(), 0));
}

bool
WebGLFramebuffer::Attachment::HasAlpha() const
{
    MOZ_ASSERT(HasImage());

    if (Texture() &&
        Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel))
    {
        return FormatHasAlpha(Texture()->ImageInfoAt(mTexImageTarget,
                                                     mTexImageLevel).EffectiveInternalFormat());
    }

    if (Renderbuffer())
        return FormatHasAlpha(Renderbuffer()->InternalFormat());

    return false;
}

GLenum
WebGLFramebuffer::GetFormatForAttachment(const WebGLFramebuffer::Attachment& attachment) const
{
    MOZ_ASSERT(attachment.IsDefined());
    MOZ_ASSERT(attachment.Texture() || attachment.Renderbuffer());

    if (attachment.Texture()) {
        const WebGLTexture& tex = *attachment.Texture();
        MOZ_ASSERT(tex.HasImageInfoAt(attachment.ImageTarget(), 0));

        const WebGLTexture::ImageInfo& imgInfo = tex.ImageInfoAt(attachment.ImageTarget(),
                                                                 0);
        return imgInfo.EffectiveInternalFormat().get();
    }

    if (attachment.Renderbuffer())
        return attachment.Renderbuffer()->InternalFormat();

    return LOCAL_GL_NONE;
}

TexInternalFormat
WebGLFramebuffer::Attachment::EffectiveInternalFormat() const
{
    const WebGLTexture* tex = Texture();
    if (tex && tex->HasImageInfoAt(mTexImageTarget, mTexImageLevel)) {
        return tex->ImageInfoAt(mTexImageTarget,
                                mTexImageLevel).EffectiveInternalFormat();
    }

    const WebGLRenderbuffer* rb = Renderbuffer();
    if (rb)
        return rb->InternalFormat();

    return LOCAL_GL_NONE;
}

bool
WebGLFramebuffer::Attachment::IsReadableFloat() const
{
    TexInternalFormat internalformat = EffectiveInternalFormat();
    MOZ_ASSERT(internalformat != LOCAL_GL_NONE);
    TexType type = TypeFromInternalFormat(internalformat);
    return type == LOCAL_GL_FLOAT ||
           type == LOCAL_GL_HALF_FLOAT;
}

void
WebGLFramebuffer::Attachment::SetTexImage(WebGLTexture* tex,
                                          TexImageTarget target, GLint level)
{
    mTexturePtr = tex;
    mRenderbufferPtr = nullptr;
    mTexImageTarget = target;
    mTexImageLevel = level;

    mNeedsFinalize = true;
}

void
WebGLFramebuffer::Attachment::SetRenderbuffer(WebGLRenderbuffer* rb)
{
    mTexturePtr = nullptr;
    mRenderbufferPtr = rb;

    mNeedsFinalize = true;
}

bool
WebGLFramebuffer::Attachment::HasUninitializedImageData() const
{
    if (!HasImage())
        return false;

    if (Renderbuffer())
        return Renderbuffer()->HasUninitializedImageData();

    if (Texture()) {
        MOZ_ASSERT(Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel));
        return Texture()->ImageInfoAt(mTexImageTarget,
                                      mTexImageLevel).HasUninitializedImageData();
    }

    MOZ_ASSERT(false, "Should not get here.");
    return false;
}

void
WebGLFramebuffer::Attachment::SetImageDataStatus(WebGLImageDataStatus newStatus)
{
    if (!HasImage())
        return;

    if (Renderbuffer()) {
        Renderbuffer()->SetImageDataStatus(newStatus);
        return;
    }

    if (Texture()) {
        Texture()->SetImageDataStatus(mTexImageTarget, mTexImageLevel,
                                      newStatus);
        return;
    }

    MOZ_ASSERT(false, "Should not get here.");
}

bool
WebGLFramebuffer::Attachment::HasImage() const
{
    if (Texture() && Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel))
        return true;

    if (Renderbuffer())
        return true;

    return false;
}

const WebGLRectangleObject&
WebGLFramebuffer::Attachment::RectangleObject() const
{
    MOZ_ASSERT(HasImage(),
               "Make sure it has an image before requesting the rectangle.");

    if (Texture()) {
        MOZ_ASSERT(Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel));
        return Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel);
    }

    if (Renderbuffer())
        return *Renderbuffer();

    MOZ_CRASH("Should not get here.");
}

// The following IsValidFBOTextureXXX functions check the internal format that
// is used by GL or GL ES texture formats.  This corresponds to the state that
// is stored in WebGLTexture::ImageInfo::InternalFormat()

static inline bool
IsValidFBOTextureDepthFormat(GLenum internalformat)
{
    return IsGLDepthFormat(internalformat);
}

static inline bool
IsValidFBOTextureDepthStencilFormat(GLenum internalformat)
{
    return IsGLDepthStencilFormat(internalformat);
}

// The following IsValidFBORenderbufferXXX functions check the internal format
// that is stored by WebGLRenderbuffer::InternalFormat(). Valid values can be
// found in WebGLContext::RenderbufferStorage.

static inline bool
IsValidFBORenderbufferDepthFormat(GLenum internalFormat)
{
    return internalFormat == LOCAL_GL_DEPTH_COMPONENT16;
}

static inline bool
IsValidFBORenderbufferDepthStencilFormat(GLenum internalFormat)
{
    return internalFormat == LOCAL_GL_DEPTH_STENCIL;
}

static inline bool
IsValidFBORenderbufferStencilFormat(GLenum internalFormat)
{
    return internalFormat == LOCAL_GL_STENCIL_INDEX8;
}

bool
WebGLContext::IsFormatValidForFB(GLenum sizedFormat) const
{
    switch (sizedFormat) {
    case LOCAL_GL_ALPHA8:
    case LOCAL_GL_LUMINANCE8:
    case LOCAL_GL_LUMINANCE8_ALPHA8:
    case LOCAL_GL_RGB8:
    case LOCAL_GL_RGBA8:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_RGBA4:
        return true;

    case LOCAL_GL_SRGB8:
    case LOCAL_GL_SRGB8_ALPHA8_EXT:
        return IsExtensionEnabled(WebGLExtensionID::EXT_sRGB);

    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGBA32F:
        return IsExtensionEnabled(WebGLExtensionID::WEBGL_color_buffer_float);

    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGBA16F:
        return IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_half_float);
    }

    return false;
}

bool
WebGLFramebuffer::Attachment::IsComplete() const
{
    if (!HasImage())
        return false;

    const WebGLRectangleObject& rect = RectangleObject();

    if (!rect.Width() ||
        !rect.Height())
    {
        return false;
    }

    if (Texture()) {
        MOZ_ASSERT(Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel));
        const WebGLTexture::ImageInfo& imageInfo =
            Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel);
        GLenum sizedFormat = imageInfo.EffectiveInternalFormat().get();

        if (mAttachmentPoint == LOCAL_GL_DEPTH_ATTACHMENT)
            return IsValidFBOTextureDepthFormat(sizedFormat);

        if (mAttachmentPoint == LOCAL_GL_STENCIL_ATTACHMENT) {
            // Textures can't have the correct format for stencil buffers.
            return false;
        }

        if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
            return IsValidFBOTextureDepthStencilFormat(sizedFormat);

        if (mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0 &&
            mAttachmentPoint <= FBAttachment(LOCAL_GL_COLOR_ATTACHMENT0 - 1 +
                                             WebGLContext::kMaxColorAttachments))
        {
            WebGLContext* webgl = Texture()->Context();
            return webgl->IsFormatValidForFB(sizedFormat);
        }
        MOZ_ASSERT(false, "Invalid WebGL attachment point?");
        return false;
    }

    if (Renderbuffer()) {
        GLenum internalFormat = Renderbuffer()->InternalFormat();

        if (mAttachmentPoint == LOCAL_GL_DEPTH_ATTACHMENT)
            return IsValidFBORenderbufferDepthFormat(internalFormat);

        if (mAttachmentPoint == LOCAL_GL_STENCIL_ATTACHMENT)
            return IsValidFBORenderbufferStencilFormat(internalFormat);

        if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
            return IsValidFBORenderbufferDepthStencilFormat(internalFormat);

        if (mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0 &&
            mAttachmentPoint <= FBAttachment(LOCAL_GL_COLOR_ATTACHMENT0 - 1 +
                                             WebGLContext::kMaxColorAttachments))
        {
            WebGLContext* webgl = Renderbuffer()->Context();
            return webgl->IsFormatValidForFB(internalFormat);
        }
        MOZ_ASSERT(false, "Invalid WebGL attachment point?");
        return false;
    }
    MOZ_ASSERT(false, "Should not get here.");
    return false;
}

void
WebGLFramebuffer::Attachment::FinalizeAttachment(gl::GLContext* gl,
                                                 FBAttachment attachmentLoc) const
{
    if (!mNeedsFinalize)
        return;

    mNeedsFinalize = false;

    if (!HasImage()) {
        if (attachmentLoc == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                         LOCAL_GL_DEPTH_ATTACHMENT,
                                         LOCAL_GL_RENDERBUFFER, 0);
            gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                         LOCAL_GL_STENCIL_ATTACHMENT,
                                         LOCAL_GL_RENDERBUFFER, 0);
        } else {
            gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER,
                                         attachmentLoc.get(),
                                         LOCAL_GL_RENDERBUFFER, 0);
        }

        return;
    }
    MOZ_ASSERT(HasImage());

    if (Texture()) {
        MOZ_ASSERT(gl == Texture()->Context()->GL());

        const GLenum imageTarget = ImageTarget().get();
        const GLint mipLevel = MipLevel();
        const GLuint glName = Texture()->GLName();

        if (attachmentLoc == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                      LOCAL_GL_DEPTH_ATTACHMENT, imageTarget,
                                      glName, mipLevel);
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                      LOCAL_GL_STENCIL_ATTACHMENT, imageTarget,
                                      glName, mipLevel);
        } else {
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, attachmentLoc.get(),
                                      imageTarget, glName, mipLevel);
        }
        return;
    }

    if (Renderbuffer()) {
        Renderbuffer()->FramebufferRenderbuffer(attachmentLoc);
        return;
    }

    MOZ_ASSERT(false, "Should not get here.");
}

void
WebGLFramebuffer::Delete()
{
    DetachAllAttachments();
    mColorAttachments.Clear();
    mDepthAttachment.Reset();
    mStencilAttachment.Reset();
    mDepthStencilAttachment.Reset();

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteFramebuffers(1, &mGLName);
    LinkedListElement<WebGLFramebuffer>::removeFrom(mContext->mFramebuffers);
}

void
WebGLFramebuffer::DetachAttachment(WebGLFramebuffer::Attachment& attachment)
{
    if (attachment.Texture())
        attachment.Texture()->DetachFrom(this, attachment.mAttachmentPoint);

    if (attachment.Renderbuffer()) {
        attachment.Renderbuffer()->DetachFrom(this,
                                              attachment.mAttachmentPoint);
    }
}

void
WebGLFramebuffer::DetachAllAttachments()
{
    for (size_t i = 0; i < mColorAttachments.Length(); i++) {
        DetachAttachment(mColorAttachments[i]);
    }

    DetachAttachment(mDepthAttachment);
    DetachAttachment(mStencilAttachment);
    DetachAttachment(mDepthStencilAttachment);
}

void
WebGLFramebuffer::FramebufferRenderbuffer(FBAttachment attachPoint,
                                          RBTarget rbtarget,
                                          WebGLRenderbuffer* rb)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferRenderbuffer: renderbuffer",
                                           rb))
    {
        return;
    }

    /* Get the requested attachment. If result is NULL, attachment is invalid
     * and an error is generated.
     *
     * Don't use GetAttachment(...) here because it opt builds it returns
     * mColorAttachment[0] for invalid attachment, which we really don't want to
     * mess with.
     */
    Attachment* attachment = GetAttachmentOrNull(attachPoint);
    if (!attachment)
        return; // Error generated internally to GetAttachmentOrNull.

    // Invalidate cached framebuffer status and inform texture of its new
    // attachment.
    mStatus = 0;

    // Detach current:
    if (attachment->Texture())
        attachment->Texture()->DetachFrom(this, attachPoint);
    else if (attachment->Renderbuffer())
        attachment->Renderbuffer()->DetachFrom(this, attachPoint);

    // Attach new:
    if (rb)
        rb->AttachTo(this, attachPoint);

    attachment->SetRenderbuffer(rb);
}

void
WebGLFramebuffer::FramebufferTexture2D(FBAttachment attachPoint,
                                       TexImageTarget texImageTarget,
                                       WebGLTexture* tex, GLint level)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferTexture2D: texture",
                                           tex))
    {
        return;
    }

    if (tex) {
        bool isTexture2D = tex->Target() == LOCAL_GL_TEXTURE_2D;
        bool isTexTarget2D = texImageTarget == LOCAL_GL_TEXTURE_2D;
        if (isTexture2D != isTexTarget2D) {
            mContext->ErrorInvalidOperation("framebufferTexture2D: Mismatched"
                                            " texture and texture target.");
            return;
        }
    }

    if (level != 0) {
        mContext->ErrorInvalidValue("framebufferTexture2D: Level must be 0.");
        return;
    }

    /* Get the requested attachment. If result is NULL, attachment is invalid
     * and an error is generated.
     *
     * Don't use GetAttachment(...) here because it opt builds it returns
     * mColorAttachment[0] for invalid attachment, which we really don't want to
     * mess with.
     */
    Attachment* attachment = GetAttachmentOrNull(attachPoint);
    if (!attachment)
        return; // Error generated internally to GetAttachmentOrNull.

    // Invalidate cached framebuffer status and inform texture of its new
    // attachment.
    mStatus = 0;

    // Detach current:
    if (attachment->Texture())
        attachment->Texture()->DetachFrom(this, attachPoint);
    else if (attachment->Renderbuffer())
        attachment->Renderbuffer()->DetachFrom(this, attachPoint);

    // Attach new:
    if (tex)
        tex->AttachTo(this, attachPoint);

    attachment->SetTexImage(tex, texImageTarget, level);
}

WebGLFramebuffer::Attachment*
WebGLFramebuffer::GetAttachmentOrNull(FBAttachment attachPoint)
{
    switch (attachPoint.get()) {
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        return &mDepthStencilAttachment;

    case LOCAL_GL_DEPTH_ATTACHMENT:
        return &mDepthAttachment;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        return &mStencilAttachment;

    default:
        break;
    }

    if (!mContext->ValidateFramebufferAttachment(this, attachPoint.get(),
                                                 "getAttachmentOrNull"))
    {
        return nullptr;
    }

    size_t colorAttachmentId = attachPoint.get() - LOCAL_GL_COLOR_ATTACHMENT0;
    EnsureColorAttachments(colorAttachmentId);

    return &mColorAttachments[colorAttachmentId];
}

const WebGLFramebuffer::Attachment&
WebGLFramebuffer::GetAttachment(FBAttachment attachPoint) const
{
    switch (attachPoint.get()) {
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        return mDepthStencilAttachment;

    case LOCAL_GL_DEPTH_ATTACHMENT:
        return mDepthAttachment;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        return mStencilAttachment;

    default:
        break;
    }

    if (!mContext->ValidateFramebufferAttachment(this, attachPoint.get(),
                                                 "getAttachment"))
    {
        MOZ_ASSERT(false);
        return mColorAttachments[0];
    }

    size_t colorAttachmentId = attachPoint.get() - LOCAL_GL_COLOR_ATTACHMENT0;
    if (colorAttachmentId >= mColorAttachments.Length()) {
        MOZ_ASSERT(false);
        return mColorAttachments[0];
    }

    return mColorAttachments[colorAttachmentId];
}

void
WebGLFramebuffer::DetachTexture(const WebGLTexture* tex)
{
    for (size_t i = 0; i < (size_t)mColorAttachments.Length(); i++) {
        if (mColorAttachments[i].Texture() == tex) {
            FramebufferTexture2D(LOCAL_GL_COLOR_ATTACHMENT0+i,
                                 LOCAL_GL_TEXTURE_2D, nullptr, 0);
            // It might be attached in multiple places, so don't break.
        }
    }

    if (mDepthAttachment.Texture() == tex) {
        FramebufferTexture2D(LOCAL_GL_DEPTH_ATTACHMENT, LOCAL_GL_TEXTURE_2D,
                             nullptr, 0);
    }
    if (mStencilAttachment.Texture() == tex) {
        FramebufferTexture2D(LOCAL_GL_STENCIL_ATTACHMENT, LOCAL_GL_TEXTURE_2D,
                             nullptr, 0);
    }
    if (mDepthStencilAttachment.Texture() == tex) {
        FramebufferTexture2D(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT,
                             LOCAL_GL_TEXTURE_2D, nullptr, 0);
    }
}

void
WebGLFramebuffer::DetachRenderbuffer(const WebGLRenderbuffer* rb)
{
    for (size_t i = 0; i < (size_t)mColorAttachments.Length(); i++) {
        if (mColorAttachments[i].Renderbuffer() == rb) {
            FramebufferRenderbuffer(LOCAL_GL_COLOR_ATTACHMENT0+i,
                                    LOCAL_GL_RENDERBUFFER, nullptr);
            // It might be attached in multiple places, so don't break.
        }
    }

    if (mDepthAttachment.Renderbuffer() == rb) {
        FramebufferRenderbuffer(LOCAL_GL_DEPTH_ATTACHMENT,
                                LOCAL_GL_RENDERBUFFER, nullptr);
    }
    if (mStencilAttachment.Renderbuffer() == rb) {
        FramebufferRenderbuffer(LOCAL_GL_STENCIL_ATTACHMENT,
                                LOCAL_GL_RENDERBUFFER, nullptr);
    }
    if (mDepthStencilAttachment.Renderbuffer() == rb) {
        FramebufferRenderbuffer(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT,
                                LOCAL_GL_RENDERBUFFER, nullptr);
    }
}

bool
WebGLFramebuffer::HasDefinedAttachments() const
{
    bool hasAttachments = false;

    for (size_t i = 0; i < (size_t)mColorAttachments.Length(); i++) {
        hasAttachments |= mColorAttachments[i].IsDefined();
    }

    hasAttachments |= mDepthAttachment.IsDefined();
    hasAttachments |= mStencilAttachment.IsDefined();
    hasAttachments |= mDepthStencilAttachment.IsDefined();

    return hasAttachments;
}

static bool
IsIncomplete(const WebGLFramebuffer::Attachment& cur)
{
    return cur.IsDefined() && !cur.IsComplete();
}

bool
WebGLFramebuffer::HasIncompleteAttachments() const
{
    bool hasIncomplete = false;

    for (size_t i = 0; i < (size_t)mColorAttachments.Length(); i++) {
        hasIncomplete |= IsIncomplete(mColorAttachments[i]);
    }

    hasIncomplete |= IsIncomplete(mDepthAttachment);
    hasIncomplete |= IsIncomplete(mStencilAttachment);
    hasIncomplete |= IsIncomplete(mDepthStencilAttachment);

    return hasIncomplete;
}

const WebGLRectangleObject&
WebGLFramebuffer::GetAnyRectObject() const
{
    MOZ_ASSERT(HasDefinedAttachments());

    for (size_t i = 0; i < (size_t)mColorAttachments.Length(); i++) {
        if (mColorAttachments[i].HasImage())
            return mColorAttachments[i].RectangleObject();
    }

    if (mDepthAttachment.HasImage())
        return mDepthAttachment.RectangleObject();

    if (mStencilAttachment.HasImage())
        return mStencilAttachment.RectangleObject();

    if (mDepthStencilAttachment.HasImage())
        return mDepthStencilAttachment.RectangleObject();

    MOZ_CRASH("Should not get here.");
}

static bool
RectsMatch(const WebGLFramebuffer::Attachment& attachment,
           const WebGLRectangleObject& rect)
{
    return attachment.RectangleObject().HasSameDimensionsAs(rect);
}

bool
WebGLFramebuffer::AllImageRectsMatch() const
{
    MOZ_ASSERT(HasDefinedAttachments());
    MOZ_ASSERT(!HasIncompleteAttachments());

    const WebGLRectangleObject& rect = GetAnyRectObject();

    // Alright, we have *a* rect, let's check all the others.
    bool imageRectsMatch = true;

    for (size_t i = 0; i < (size_t)mColorAttachments.Length(); i++) {
        if (mColorAttachments[i].HasImage())
            imageRectsMatch &= RectsMatch(mColorAttachments[i], rect);
    }

    if (mDepthAttachment.HasImage())
        imageRectsMatch &= RectsMatch(mDepthAttachment, rect);

    if (mStencilAttachment.HasImage())
        imageRectsMatch &= RectsMatch(mStencilAttachment, rect);

    if (mDepthStencilAttachment.HasImage())
        imageRectsMatch &= RectsMatch(mDepthStencilAttachment, rect);

    return imageRectsMatch;
}

const WebGLRectangleObject&
WebGLFramebuffer::RectangleObject() const
{
    // If we're using this as the RectObj of an FB, we need to be sure the FB
    // has a consistent rect.
    MOZ_ASSERT(AllImageRectsMatch(), "Did you mean `GetAnyRectObject`?");
    return GetAnyRectObject();
}

FBStatus
WebGLFramebuffer::PrecheckFramebufferStatus() const
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!HasDefinedAttachments())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT; // No attachments

    if (HasIncompleteAttachments())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

    if (!AllImageRectsMatch())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS; // Inconsistent sizes

    if (HasDepthStencilConflict())
        return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;

    return LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

FBStatus
WebGLFramebuffer::CheckFramebufferStatus() const
{
    if (mStatus != 0)
        return mStatus;

    mStatus = PrecheckFramebufferStatus().get();
    if (mStatus != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return mStatus;

    // Looks good on our end. Let's ask the driver.
    mContext->MakeContextCurrent();

    // Ok, attach our chosen flavor of {DEPTH, STENCIL, DEPTH_STENCIL}.
    FinalizeAttachments();

    mStatus = mContext->gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    return mStatus;
}

bool
WebGLFramebuffer::HasCompletePlanes(GLbitfield mask)
{
    if (CheckFramebufferStatus() != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    bool hasPlanes = true;
    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        hasPlanes &= ColorAttachmentCount() &&
                     ColorAttachment(0).IsDefined();
    }

    if (mask & LOCAL_GL_DEPTH_BUFFER_BIT) {
        hasPlanes &= DepthAttachment().IsDefined() ||
                     DepthStencilAttachment().IsDefined();
    }

    if (mask & LOCAL_GL_STENCIL_BUFFER_BIT) {
        hasPlanes &= StencilAttachment().IsDefined() ||
                     DepthStencilAttachment().IsDefined();
    }

    return hasPlanes;
}

bool
WebGLFramebuffer::CheckAndInitializeAttachments()
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (CheckFramebufferStatus() != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    // Cool! We've checked out ok. Just need to initialize.
    const size_t colorAttachmentCount = mColorAttachments.Length();

    // Check if we need to initialize anything
    {
        bool hasUninitializedAttachments = false;

        for (size_t i = 0; i < colorAttachmentCount; i++) {
            if (mColorAttachments[i].HasImage())
                hasUninitializedAttachments |= mColorAttachments[i].HasUninitializedImageData();
        }

        if (mDepthAttachment.HasImage())
            hasUninitializedAttachments |= mDepthAttachment.HasUninitializedImageData();
        if (mStencilAttachment.HasImage())
            hasUninitializedAttachments |= mStencilAttachment.HasUninitializedImageData();
        if (mDepthStencilAttachment.HasImage())
            hasUninitializedAttachments |= mDepthStencilAttachment.HasUninitializedImageData();

        if (!hasUninitializedAttachments)
            return true;
    }

    // Get buffer-bit-mask and color-attachment-mask-list
    uint32_t mask = 0;
    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = { false };
    MOZ_ASSERT(colorAttachmentCount <= WebGLContext::kMaxColorAttachments);

    for (size_t i = 0; i < colorAttachmentCount; i++) {
        if (mColorAttachments[i].HasUninitializedImageData()) {
          colorAttachmentsMask[i] = true;
          mask |= LOCAL_GL_COLOR_BUFFER_BIT;
        }
    }

    if (mDepthAttachment.HasUninitializedImageData() ||
        mDepthStencilAttachment.HasUninitializedImageData())
    {
        mask |= LOCAL_GL_DEPTH_BUFFER_BIT;
    }

    if (mStencilAttachment.HasUninitializedImageData() ||
        mDepthStencilAttachment.HasUninitializedImageData())
    {
        mask |= LOCAL_GL_STENCIL_BUFFER_BIT;
    }

    // Clear!
    mContext->ForceClearFramebufferWithDefaultValues(mask, colorAttachmentsMask);

    // Mark all the uninitialized images as initialized.
    for (size_t i = 0; i < colorAttachmentCount; i++) {
        if (mColorAttachments[i].HasUninitializedImageData())
            mColorAttachments[i].SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    }

    if (mDepthAttachment.HasUninitializedImageData())
        mDepthAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mStencilAttachment.HasUninitializedImageData())
        mStencilAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mDepthStencilAttachment.HasUninitializedImageData())
        mDepthStencilAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);

    return true;
}

void WebGLFramebuffer::EnsureColorAttachments(size_t colorAttachmentId)
{
    MOZ_ASSERT(colorAttachmentId < WebGLContext::kMaxColorAttachments);

    size_t currentAttachmentCount = mColorAttachments.Length();
    if (colorAttachmentId < currentAttachmentCount)
        return;

    mColorAttachments.SetLength(colorAttachmentId + 1);

    for (size_t i = colorAttachmentId; i >= currentAttachmentCount; i--) {
        mColorAttachments[i].mAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0 + i;
    }
}

void
WebGLFramebuffer::NotifyAttachableChanged() const
{
    // Attachment has changed, so invalidate cached status
    mStatus = 0;
}

static void
FinalizeDrawAndReadBuffers(gl::GLContext* gl, bool isColorBufferDefined)
{
    MOZ_ASSERT(gl, "Expected a valid GLContext ptr.");
    // GLES don't support DrawBuffer()/ReadBuffer.
    // According to http://www.opengl.org/wiki/Framebuffer_Object
    //
    // Each draw buffers must either specify color attachment points that have images
    // attached or must be GL_NONE​. (GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER​ when false).
    //
    // If the read buffer is set, then it must specify an attachment point that has an
    // image attached. (GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER​ when false).
    //
    // Note that this test is not performed if OpenGL 4.2 or ARB_ES2_compatibility is
    // available.
    if (gl->IsGLES() ||
        gl->IsSupported(gl::GLFeature::ES2_compatibility) ||
        gl->IsAtLeast(gl::ContextProfile::OpenGL, 420))
    {
        return;
    }

    // TODO(djg): Assert that fDrawBuffer/fReadBuffer is not NULL.
    GLenum colorBufferSource = isColorBufferDefined ? LOCAL_GL_COLOR_ATTACHMENT0
                                                    : LOCAL_GL_NONE;
    gl->fDrawBuffer(colorBufferSource);
    gl->fReadBuffer(colorBufferSource);
}

void
WebGLFramebuffer::FinalizeAttachments() const
{
    gl::GLContext* gl = mContext->gl;

    for (size_t i = 0; i < ColorAttachmentCount(); i++) {
        ColorAttachment(i).FinalizeAttachment(gl, LOCAL_GL_COLOR_ATTACHMENT0+i);
    }

    DepthAttachment().FinalizeAttachment(gl, LOCAL_GL_DEPTH_ATTACHMENT);
    StencilAttachment().FinalizeAttachment(gl, LOCAL_GL_STENCIL_ATTACHMENT);
    DepthStencilAttachment().FinalizeAttachment(gl,
                                                LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);

    FinalizeDrawAndReadBuffers(gl, ColorAttachment(0).IsDefined());
}

bool
WebGLFramebuffer::ValidateForRead(const char* info, TexInternalFormat* const out_format)
{
    if (mReadBufferMode == LOCAL_GL_NONE) {
        mContext->ErrorInvalidOperation("%s: Read buffer mode must not be"
                                        " NONE.", info);
        return false;
    }

    const auto& attachment = GetAttachment(mReadBufferMode);

    if (!CheckAndInitializeAttachments()) {
        mContext->ErrorInvalidFramebufferOperation("readPixels: incomplete framebuffer");
        return false;
    }

    GLenum readPlaneBits = LOCAL_GL_COLOR_BUFFER_BIT;
    if (!HasCompletePlanes(readPlaneBits)) {
        mContext->ErrorInvalidOperation("readPixels: Read source attachment doesn't have the"
                                        " correct color/depth/stencil type.");
        return false;
    }

    if (!attachment.IsDefined()) {
        mContext->ErrorInvalidOperation("readPixels: ");
        return false;
    }

    *out_format = attachment.EffectiveInternalFormat();
    return true;
}

inline void
ImplCycleCollectionUnlink(mozilla::WebGLFramebuffer::Attachment& field)
{
    field.mTexturePtr = nullptr;
    field.mRenderbufferPtr = nullptr;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            mozilla::WebGLFramebuffer::Attachment& field,
                            const char* name,
                            uint32_t flags = 0)
{
    CycleCollectionNoteChild(callback, field.mTexturePtr.get(), name, flags);
    CycleCollectionNoteChild(callback, field.mRenderbufferPtr.get(), name,
                             flags);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLFramebuffer,
                                      mColorAttachments,
                                      mDepthAttachment,
                                      mStencilAttachment,
                                      mDepthStencilAttachment)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLFramebuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLFramebuffer, Release)

} // namespace mozilla
