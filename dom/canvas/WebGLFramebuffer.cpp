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

WebGLFBAttachPoint::WebGLFBAttachPoint(WebGLFramebuffer* fb,
                                       FBAttachment attachmentPoint)
    : mFB(fb)
    , mAttachmentPoint(attachmentPoint)
    , mTexImageTarget(LOCAL_GL_NONE)
{ }

WebGLFBAttachPoint::~WebGLFBAttachPoint()
{
    MOZ_ASSERT(!mRenderbufferPtr);
    MOZ_ASSERT(!mTexturePtr);
}

bool
WebGLFBAttachPoint::IsDeleteRequested() const
{
    return Texture() ? Texture()->IsDeleteRequested()
         : Renderbuffer() ? Renderbuffer()->IsDeleteRequested()
         : false;
}

bool
WebGLFBAttachPoint::IsDefined() const
{
    return Renderbuffer() ||
           (Texture() && Texture()->HasImageInfoAt(ImageTarget(), 0));
}

bool
WebGLFBAttachPoint::HasAlpha() const
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
WebGLFramebuffer::GetFormatForAttachment(const WebGLFBAttachPoint& attachment) const
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
WebGLFBAttachPoint::EffectiveInternalFormat() const
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
WebGLFBAttachPoint::IsReadableFloat() const
{
    TexInternalFormat internalformat = EffectiveInternalFormat();
    MOZ_ASSERT(internalformat != LOCAL_GL_NONE);
    TexType type = TypeFromInternalFormat(internalformat);
    return type == LOCAL_GL_FLOAT ||
           type == LOCAL_GL_HALF_FLOAT_OES ||
           type == LOCAL_GL_HALF_FLOAT;
}

static void
UnmarkAttachment(WebGLFBAttachPoint& attachment)
{
    WebGLFramebufferAttachable* maybe = attachment.Texture();
    if (!maybe)
        maybe = attachment.Renderbuffer();

    if (maybe)
        maybe->UnmarkAttachment(attachment);
}

void
WebGLFBAttachPoint::SetTexImage(WebGLTexture* tex, TexImageTarget target,
                                          GLint level)
{
    mFB->InvalidateFramebufferStatus();

    UnmarkAttachment(*this);

    mTexturePtr = tex;
    mRenderbufferPtr = nullptr;
    mTexImageTarget = target;
    mTexImageLevel = level;

    if (tex)
        tex->MarkAttachment(*this);
}

void
WebGLFBAttachPoint::SetRenderbuffer(WebGLRenderbuffer* rb)
{
    mFB->InvalidateFramebufferStatus();

    UnmarkAttachment(*this);

    mTexturePtr = nullptr;
    mRenderbufferPtr = rb;

    if (rb)
        rb->MarkAttachment(*this);
}

bool
WebGLFBAttachPoint::HasUninitializedImageData() const
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
WebGLFBAttachPoint::SetImageDataStatus(WebGLImageDataStatus newStatus)
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
WebGLFBAttachPoint::HasImage() const
{
    if (Texture() && Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel))
        return true;

    if (Renderbuffer())
        return true;

    return false;
}

const WebGLRectangleObject&
WebGLFBAttachPoint::RectangleObject() const
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
WebGLFBAttachPoint::IsComplete() const
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
WebGLFBAttachPoint::FinalizeAttachment(gl::GLContext* gl,
                                                 FBAttachment attachmentLoc) const
{
    if (!HasImage()) {
        switch (attachmentLoc.get()) {
        case LOCAL_GL_DEPTH_ATTACHMENT:
        case LOCAL_GL_STENCIL_ATTACHMENT:
        case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
            break;

        default:
            gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, attachmentLoc.get(),
                                         LOCAL_GL_RENDERBUFFER, 0);
            break;
        }

        return;
    }
    MOZ_ASSERT(HasImage());

    if (Texture()) {
        MOZ_ASSERT(gl == Texture()->Context()->GL());

        const GLenum imageTarget = ImageTarget().get();
        const GLint mipLevel = MipLevel();
        const GLuint glName = Texture()->mGLName;

        if (attachmentLoc == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                      imageTarget, glName, mipLevel);
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                      imageTarget, glName, mipLevel);
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

    MOZ_CRASH();
}

////////////////////////////////////////////////////////////////////////////////
// WebGLFramebuffer

WebGLFramebuffer::WebGLFramebuffer(WebGLContext* webgl, GLuint fbo)
    : WebGLContextBoundObject(webgl)
    , mGLName(fbo)
    , mStatus(0)
    , mReadBufferMode(LOCAL_GL_COLOR_ATTACHMENT0)
    , mColorAttachment0(this, LOCAL_GL_COLOR_ATTACHMENT0)
    , mDepthAttachment(this, LOCAL_GL_DEPTH_ATTACHMENT)
    , mStencilAttachment(this, LOCAL_GL_STENCIL_ATTACHMENT)
    , mDepthStencilAttachment(this, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
#ifdef ANDROID
    , mIsFB(false)
#endif

{
    mContext->mFramebuffers.insertBack(this);
}

void
WebGLFramebuffer::Delete()
{
    mColorAttachment0.Clear();
    mDepthAttachment.Clear();
    mStencilAttachment.Clear();
    mDepthStencilAttachment.Clear();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        mMoreColorAttachments[i].Clear();
    }

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteFramebuffers(1, &mGLName);
    LinkedListElement<WebGLFramebuffer>::removeFrom(mContext->mFramebuffers);

#ifdef ANDROID
    mIsFB = false;
#endif
}

void
WebGLFramebuffer::FramebufferRenderbuffer(FBAttachment attachPointEnum,
                                          RBTarget rbtarget,
                                          WebGLRenderbuffer* rb)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferRenderbuffer: renderbuffer", rb))
        return;

    // `attachPoint` is validated by ValidateFramebufferAttachment().
    WebGLFBAttachPoint& attachPoint = GetAttachPoint(attachPointEnum);
    attachPoint.SetRenderbuffer(rb);

    InvalidateFramebufferStatus();
}

void
WebGLFramebuffer::FramebufferTexture2D(FBAttachment attachPointEnum,
                                       TexImageTarget texImageTarget,
                                       WebGLTexture* tex, GLint level)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferTexture2D: texture", tex))
        return;

    if (tex) {
        bool isTexture2D = tex->Target() == LOCAL_GL_TEXTURE_2D;
        bool isTexTarget2D = texImageTarget == LOCAL_GL_TEXTURE_2D;
        if (isTexture2D != isTexTarget2D) {
            mContext->ErrorInvalidOperation("framebufferTexture2D: Mismatched"
                                            " texture and texture target.");
            return;
        }
    }

    WebGLFBAttachPoint& attachPoint = GetAttachPoint(attachPointEnum);
    attachPoint.SetTexImage(tex, texImageTarget, level);

    InvalidateFramebufferStatus();
}

WebGLFBAttachPoint&
WebGLFramebuffer::GetAttachPoint(FBAttachment attachPoint)
{
    switch (attachPoint.get()) {
    case LOCAL_GL_COLOR_ATTACHMENT0:
        return mColorAttachment0;

    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        return mDepthStencilAttachment;

    case LOCAL_GL_DEPTH_ATTACHMENT:
        return mDepthAttachment;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        return mStencilAttachment;

    default:
        break;
    }

    if (attachPoint >= LOCAL_GL_COLOR_ATTACHMENT1) {
        size_t colorAttachmentId = attachPoint.get() - LOCAL_GL_COLOR_ATTACHMENT0;
        if (colorAttachmentId < (size_t)mContext->mGLMaxColorAttachments) {
            EnsureColorAttachPoints(colorAttachmentId);
            return mMoreColorAttachments[colorAttachmentId - 1];
        }
    }

    MOZ_CRASH("bad `attachPoint` validation");
}

void
WebGLFramebuffer::DetachTexture(const WebGLTexture* tex)
{
    if (mColorAttachment0.Texture() == tex)
        mColorAttachment0.Clear();

    if (mDepthAttachment.Texture() == tex)
        mDepthAttachment.Clear();

    if (mStencilAttachment.Texture() == tex)
        mStencilAttachment.Clear();

    if (mDepthStencilAttachment.Texture() == tex)
        mDepthStencilAttachment.Clear();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].Texture() == tex)
            mMoreColorAttachments[i].Clear();
    }
}

void
WebGLFramebuffer::DetachRenderbuffer(const WebGLRenderbuffer* rb)
{
    if (mColorAttachment0.Renderbuffer() == rb)
        mColorAttachment0.Clear();

    if (mDepthAttachment.Renderbuffer() == rb)
        mDepthAttachment.Clear();

    if (mStencilAttachment.Renderbuffer() == rb)
        mStencilAttachment.Clear();

    if (mDepthStencilAttachment.Renderbuffer() == rb)
        mDepthStencilAttachment.Clear();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].Renderbuffer() == rb)
            mMoreColorAttachments[i].Clear();
    }
}

bool
WebGLFramebuffer::HasDefinedAttachments() const
{
    bool hasAttachments = false;

    hasAttachments |= mColorAttachment0.IsDefined();
    hasAttachments |= mDepthAttachment.IsDefined();
    hasAttachments |= mStencilAttachment.IsDefined();
    hasAttachments |= mDepthStencilAttachment.IsDefined();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        hasAttachments |= mMoreColorAttachments[i].IsDefined();
    }

    return hasAttachments;
}

static bool
IsIncomplete(const WebGLFBAttachPoint& cur)
{
    return cur.IsDefined() && !cur.IsComplete();
}

bool
WebGLFramebuffer::HasIncompleteAttachments() const
{
    bool hasIncomplete = false;

    hasIncomplete |= IsIncomplete(mColorAttachment0);
    hasIncomplete |= IsIncomplete(mDepthAttachment);
    hasIncomplete |= IsIncomplete(mStencilAttachment);
    hasIncomplete |= IsIncomplete(mDepthStencilAttachment);

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        hasIncomplete |= IsIncomplete(mMoreColorAttachments[i]);
    }

    return hasIncomplete;
}

const WebGLRectangleObject&
WebGLFramebuffer::GetAnyRectObject() const
{
    MOZ_ASSERT(HasDefinedAttachments());

    if (mColorAttachment0.HasImage())
        return mColorAttachment0.RectangleObject();

    if (mDepthAttachment.HasImage())
        return mDepthAttachment.RectangleObject();

    if (mStencilAttachment.HasImage())
        return mStencilAttachment.RectangleObject();

    if (mDepthStencilAttachment.HasImage())
        return mDepthStencilAttachment.RectangleObject();

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].HasImage())
            return mMoreColorAttachments[i].RectangleObject();
    }

    MOZ_CRASH("Should not get here.");
}

static bool
RectsMatch(const WebGLFBAttachPoint& attachment,
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

    if (mColorAttachment0.HasImage())
        imageRectsMatch &= RectsMatch(mColorAttachment0, rect);

    if (mDepthAttachment.HasImage())
        imageRectsMatch &= RectsMatch(mDepthAttachment, rect);

    if (mStencilAttachment.HasImage())
        imageRectsMatch &= RectsMatch(mStencilAttachment, rect);

    if (mDepthStencilAttachment.HasImage())
        imageRectsMatch &= RectsMatch(mDepthStencilAttachment, rect);

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].HasImage())
            imageRectsMatch &= RectsMatch(mMoreColorAttachments[i], rect);
    }

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

    // TODO: This should not be unconditionally GL_FRAMEBUFFER.
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
    if (mask & LOCAL_GL_COLOR_BUFFER_BIT)
        hasPlanes &= mColorAttachment0.IsDefined();

    if (mask & LOCAL_GL_DEPTH_BUFFER_BIT) {
        hasPlanes &= mDepthAttachment.IsDefined() ||
                     mDepthStencilAttachment.IsDefined();
    }

    if (mask & LOCAL_GL_STENCIL_BUFFER_BIT) {
        hasPlanes &= mStencilAttachment.IsDefined() ||
                     mDepthStencilAttachment.IsDefined();
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
    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();

    // Check if we need to initialize anything
    {
        bool hasUninitializedAttachments = false;

        if (mColorAttachment0.HasImage())
            hasUninitializedAttachments |= mColorAttachment0.HasUninitializedImageData();
        if (mDepthAttachment.HasImage())
            hasUninitializedAttachments |= mDepthAttachment.HasUninitializedImageData();
        if (mStencilAttachment.HasImage())
            hasUninitializedAttachments |= mStencilAttachment.HasUninitializedImageData();
        if (mDepthStencilAttachment.HasImage())
            hasUninitializedAttachments |= mDepthStencilAttachment.HasUninitializedImageData();

        for (size_t i = 0; i < moreColorAttachmentCount; i++) {
            if (mMoreColorAttachments[i].HasImage())
                hasUninitializedAttachments |= mMoreColorAttachments[i].HasUninitializedImageData();
        }

        if (!hasUninitializedAttachments)
            return true;
    }

    // Get buffer-bit-mask and color-attachment-mask-list
    uint32_t mask = 0;
    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = { false };
    MOZ_ASSERT(1 + moreColorAttachmentCount <= WebGLContext::kMaxColorAttachments);

    if (mColorAttachment0.HasUninitializedImageData()) {
        colorAttachmentsMask[0] = true;
        mask |= LOCAL_GL_COLOR_BUFFER_BIT;
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

    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].HasUninitializedImageData()) {
          colorAttachmentsMask[1 + i] = true;
          mask |= LOCAL_GL_COLOR_BUFFER_BIT;
        }
    }

    // Clear!
    mContext->ForceClearFramebufferWithDefaultValues(false, mask, colorAttachmentsMask);

    // Mark all the uninitialized images as initialized.
    if (mColorAttachment0.HasUninitializedImageData())
        mColorAttachment0.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mDepthAttachment.HasUninitializedImageData())
        mDepthAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mStencilAttachment.HasUninitializedImageData())
        mStencilAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    if (mDepthStencilAttachment.HasUninitializedImageData())
        mDepthStencilAttachment.SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);

    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        if (mMoreColorAttachments[i].HasUninitializedImageData())
            mMoreColorAttachments[i].SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
    }

    return true;
}

void WebGLFramebuffer::EnsureColorAttachPoints(size_t colorAttachmentId)
{
    size_t maxColorAttachments = mContext->mGLMaxColorAttachments;

    MOZ_ASSERT(colorAttachmentId < maxColorAttachments);

    if (colorAttachmentId < ColorAttachmentCount())
        return;

    while (ColorAttachmentCount() < maxColorAttachments) {
        GLenum nextAttachPoint = LOCAL_GL_COLOR_ATTACHMENT0 + ColorAttachmentCount();
        mMoreColorAttachments.AppendElement(WebGLFBAttachPoint(this, nextAttachPoint));
    }

    MOZ_ASSERT(ColorAttachmentCount() == maxColorAttachments);
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
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    MOZ_ASSERT(mStatus == LOCAL_GL_FRAMEBUFFER_COMPLETE);

    gl::GLContext* gl = mContext->gl;

    // Nuke the depth and stencil attachment points.
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);

    // Call finalize.
    mColorAttachment0.FinalizeAttachment(gl, LOCAL_GL_COLOR_ATTACHMENT0);
    mDepthAttachment.FinalizeAttachment(gl, LOCAL_GL_DEPTH_ATTACHMENT);
    mStencilAttachment.FinalizeAttachment(gl, LOCAL_GL_STENCIL_ATTACHMENT);
    mDepthStencilAttachment.FinalizeAttachment(gl, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);

    const size_t moreColorAttachmentCount = mMoreColorAttachments.Length();
    for (size_t i = 0; i < moreColorAttachmentCount; i++) {
        GLenum attachPoint = LOCAL_GL_COLOR_ATTACHMENT0 + 1 + i;
        mMoreColorAttachments[i].FinalizeAttachment(gl, attachPoint);
    }

    FinalizeDrawAndReadBuffers(gl, mColorAttachment0.IsDefined());
}

bool
WebGLFramebuffer::ValidateForRead(const char* info, TexInternalFormat* const out_format)
{
    if (mReadBufferMode == LOCAL_GL_NONE) {
        mContext->ErrorInvalidOperation("%s: Read buffer mode must not be"
                                        " NONE.", info);
        return false;
    }

    const auto& attachPoint = GetAttachPoint(mReadBufferMode);

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

    if (!attachPoint.IsDefined()) {
        mContext->ErrorInvalidOperation("readPixels: ");
        return false;
    }

    *out_format = attachPoint.EffectiveInternalFormat();
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Goop.

JSObject*
WebGLFramebuffer::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto)
{
    return dom::WebGLFramebufferBinding::Wrap(cx, this, givenProto);
}

inline void
ImplCycleCollectionUnlink(mozilla::WebGLFBAttachPoint& field)
{
    field.Unlink();
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            mozilla::WebGLFBAttachPoint& field,
                            const char* name,
                            uint32_t flags = 0)
{
    CycleCollectionNoteChild(callback, field.Texture(), name, flags);
    CycleCollectionNoteChild(callback, field.Renderbuffer(), name, flags);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLFramebuffer,
                                      mColorAttachment0,
                                      mDepthAttachment,
                                      mStencilAttachment,
                                      mDepthStencilAttachment,
                                      mMoreColorAttachments)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLFramebuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLFramebuffer, Release)

} // namespace mozilla
