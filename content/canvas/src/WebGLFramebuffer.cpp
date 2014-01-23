/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLFramebuffer.h"
#include "WebGLExtensions.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLTexture.h"
#include "WebGLRenderbuffer.h"
#include "GLContext.h"
#include "WebGLContextUtils.h"

using namespace mozilla;
using namespace mozilla::gl;

JSObject*
WebGLFramebuffer::WrapObject(JSContext* cx, JS::Handle<JSObject*> scope)
{
    return dom::WebGLFramebufferBinding::Wrap(cx, scope, this);
}

WebGLFramebuffer::WebGLFramebuffer(WebGLContext* context)
    : WebGLContextBoundObject(context)
    , mHasEverBeenBound(false)
    , mDepthAttachment(LOCAL_GL_DEPTH_ATTACHMENT)
    , mStencilAttachment(LOCAL_GL_STENCIL_ATTACHMENT)
    , mDepthStencilAttachment(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
{
    SetIsDOMBinding();
    mContext->MakeContextCurrent();
    mContext->gl->fGenFramebuffers(1, &mGLName);
    mContext->mFramebuffers.insertBack(this);

    mColorAttachments.SetLength(1);
    mColorAttachments[0].mAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0;
}

bool
WebGLFramebuffer::Attachment::IsDeleteRequested() const
{
    return Texture() ? Texture()->IsDeleteRequested()
         : Renderbuffer() ? Renderbuffer()->IsDeleteRequested()
         : false;
}

bool
WebGLFramebuffer::Attachment::HasAlpha() const
{
    GLenum format = 0;
    if (Texture() && Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel))
        format = Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).InternalFormat();
    else if (Renderbuffer())
        format = Renderbuffer()->InternalFormat();
    return FormatHasAlpha(format);
}

void
WebGLFramebuffer::Attachment::SetTexImage(WebGLTexture* tex, GLenum target, GLint level)
{
    mTexturePtr = tex;
    mRenderbufferPtr = nullptr;
    mTexImageTarget = target;
    mTexImageLevel = level;
}

bool
WebGLFramebuffer::Attachment::HasUninitializedImageData() const
{
    if (!HasImage())
        return false;

    if (Renderbuffer()) {
        return Renderbuffer()->HasUninitializedImageData();
    } else if (Texture()) {
        MOZ_ASSERT(Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel));
        return Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).HasUninitializedImageData();
    }

    MOZ_ASSERT(false, "Should not get here.");
    return false;
}

void
WebGLFramebuffer::Attachment::SetImageDataStatus(WebGLImageDataStatus newStatus)
{
    if (!HasImage())
        return;

    if (mRenderbufferPtr) {
        mRenderbufferPtr->SetImageDataStatus(newStatus);
        return;
    } else if (mTexturePtr) {
        mTexturePtr->SetImageDataStatus(mTexImageTarget, mTexImageLevel, newStatus);
        return;
    }

    MOZ_ASSERT(false, "Should not get here.");
}

bool
WebGLFramebuffer::Attachment::HasImage() const
{
    if (Texture() && Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel))
        return true;
    else if (Renderbuffer())
        return true;

    return false;
}

const WebGLRectangleObject&
WebGLFramebuffer::Attachment::RectangleObject() const
{
    MOZ_ASSERT(HasImage(), "Make sure it has an image before requesting the rectangle.");

    if (Texture()) {
        MOZ_ASSERT(Texture()->HasImageInfoAt(mTexImageTarget, mTexImageLevel));
        return Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel);
    } else if (Renderbuffer()) {
        return *Renderbuffer();
    }

    MOZ_CRASH("Should not get here.");
}

static inline bool
IsValidAttachedTextureColorFormat(GLenum format)
{
    return (
        /* linear 8-bit formats */
        format == LOCAL_GL_ALPHA ||
        format == LOCAL_GL_LUMINANCE ||
        format == LOCAL_GL_LUMINANCE_ALPHA ||
        format == LOCAL_GL_RGB ||
        format == LOCAL_GL_RGBA ||
        /* sRGB 8-bit formats */
        format == LOCAL_GL_SRGB_EXT ||
        format == LOCAL_GL_SRGB_ALPHA_EXT ||
        /* linear float32 formats */
        format ==  LOCAL_GL_ALPHA32F_ARB ||
        format ==  LOCAL_GL_LUMINANCE32F_ARB ||
        format ==  LOCAL_GL_LUMINANCE_ALPHA32F_ARB ||
        format ==  LOCAL_GL_RGB32F_ARB ||
        format ==  LOCAL_GL_RGBA32F_ARB);
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

    if (mTexturePtr) {
        MOZ_ASSERT(mTexturePtr->HasImageInfoAt(mTexImageTarget, mTexImageLevel));
        GLenum format = mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel).InternalFormat();

        if (mAttachmentPoint == LOCAL_GL_DEPTH_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_COMPONENT;
        } else if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_STENCIL;
        } else if (mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                   mAttachmentPoint < GLenum(LOCAL_GL_COLOR_ATTACHMENT0 + WebGLContext::sMaxColorAttachments))
        {
            return IsValidAttachedTextureColorFormat(format);
        }
        MOZ_ASSERT(false, "Invalid WebGL attachment point?");
        return false;
    }

    if (mRenderbufferPtr) {
        GLenum format = mRenderbufferPtr->InternalFormat();

        if (mAttachmentPoint == LOCAL_GL_DEPTH_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_COMPONENT16;
        } else if (mAttachmentPoint == LOCAL_GL_STENCIL_ATTACHMENT) {
            return format == LOCAL_GL_STENCIL_INDEX8;
        } else if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_STENCIL;
        } else if (mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                   mAttachmentPoint < GLenum(LOCAL_GL_COLOR_ATTACHMENT0 + WebGLContext::sMaxColorAttachments))
        {
            return format == LOCAL_GL_RGB565 ||
                   format == LOCAL_GL_RGB5_A1 ||
                   format == LOCAL_GL_RGBA4 ||
                   format == LOCAL_GL_SRGB8_ALPHA8_EXT;
        }
        MOZ_ASSERT(false, "Invalid WebGL attachment point?");
        return false;
    }

    MOZ_ASSERT(false, "Should not get here.");
    return false;
}

void
WebGLFramebuffer::Attachment::FinalizeAttachment(GLenum attachmentLoc) const
{
    MOZ_ASSERT(HasImage());

    if (Texture()) {
        GLContext* gl = Texture()->Context()->gl;
        if (attachmentLoc == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                      TexImageTarget(), Texture()->GLName(), TexImageLevel());
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                      TexImageTarget(), Texture()->GLName(), TexImageLevel());
        } else {
            gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, attachmentLoc,
                                      TexImageTarget(), Texture()->GLName(), TexImageLevel());
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
    mColorAttachments.Clear();
    mDepthAttachment.Reset();
    mStencilAttachment.Reset();
    mDepthStencilAttachment.Reset();

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteFramebuffers(1, &mGLName);
    LinkedListElement<WebGLFramebuffer>::removeFrom(mContext->mFramebuffers);
}

void
WebGLFramebuffer::FramebufferRenderbuffer(GLenum target,
                                          GLenum attachment,
                                          GLenum rbtarget,
                                          WebGLRenderbuffer* wrb)
{
    MOZ_ASSERT(mContext->mBoundFramebuffer == this);
    if (!mContext->ValidateObjectAllowNull("framebufferRenderbuffer: renderbuffer", wrb))
        return;

    if (target != LOCAL_GL_FRAMEBUFFER)
        return mContext->ErrorInvalidEnumInfo("framebufferRenderbuffer: target", target);

    if (rbtarget != LOCAL_GL_RENDERBUFFER)
        return mContext->ErrorInvalidEnumInfo("framebufferRenderbuffer: renderbuffer target:", rbtarget);

    switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
        mDepthAttachment.SetRenderbuffer(wrb);
        break;
    case LOCAL_GL_STENCIL_ATTACHMENT:
        mStencilAttachment.SetRenderbuffer(wrb);
        break;
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        mDepthStencilAttachment.SetRenderbuffer(wrb);
        break;
    default:
        // finish checking that the 'attachment' parameter is among the allowed values
        if (!CheckColorAttachementNumber(attachment, "framebufferRenderbuffer")){
            return;
        }

        size_t colorAttachmentId = size_t(attachment - LOCAL_GL_COLOR_ATTACHMENT0);
        EnsureColorAttachments(colorAttachmentId);
        mColorAttachments[colorAttachmentId].SetRenderbuffer(wrb);
        break;
    }
}

void
WebGLFramebuffer::FramebufferTexture2D(GLenum target,
                                       GLenum attachment,
                                       GLenum textarget,
                                       WebGLTexture* wtex,
                                       GLint level)
{
    MOZ_ASSERT(mContext->mBoundFramebuffer == this);
    if (!mContext->ValidateObjectAllowNull("framebufferTexture2D: texture",
                                           wtex))
    {
        return;
    }

    if (target != LOCAL_GL_FRAMEBUFFER)
        return mContext->ErrorInvalidEnumInfo("framebufferTexture2D: target", target);

    if (textarget != LOCAL_GL_TEXTURE_2D &&
        (textarget < LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
         textarget > LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
    {
        return mContext->ErrorInvalidEnumInfo("framebufferTexture2D: invalid texture target", textarget);
    }

    if (wtex) {
        bool isTexture2D = wtex->Target() == LOCAL_GL_TEXTURE_2D;
        bool isTexTarget2D = textarget == LOCAL_GL_TEXTURE_2D;
        if (isTexture2D != isTexTarget2D) {
            return mContext->ErrorInvalidOperation("framebufferTexture2D: mismatched texture and texture target");
        }
    }

    if (level != 0)
        return mContext->ErrorInvalidValue("framebufferTexture2D: level must be 0");

    switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
        mDepthAttachment.SetTexImage(wtex, textarget, level);
        break;
    case LOCAL_GL_STENCIL_ATTACHMENT:
        mStencilAttachment.SetTexImage(wtex, textarget, level);
        break;
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        mDepthStencilAttachment.SetTexImage(wtex, textarget, level);
        break;
    default:
        if (!CheckColorAttachementNumber(attachment, "framebufferTexture2D"))
            return;

        size_t colorAttachmentId = size_t(attachment - LOCAL_GL_COLOR_ATTACHMENT0);
        EnsureColorAttachments(colorAttachmentId);
        mColorAttachments[colorAttachmentId].SetTexImage(wtex, textarget, level);
        break;
    }
}

const WebGLFramebuffer::Attachment&
WebGLFramebuffer::GetAttachment(GLenum attachment) const
{
    if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
        return mDepthStencilAttachment;
    if (attachment == LOCAL_GL_DEPTH_ATTACHMENT)
        return mDepthAttachment;
    if (attachment == LOCAL_GL_STENCIL_ATTACHMENT)
        return mStencilAttachment;

    if (!CheckColorAttachementNumber(attachment, "getAttachment")) {
        MOZ_ASSERT(false);
        return mColorAttachments[0];
    }

    size_t colorAttachmentId = attachment - LOCAL_GL_COLOR_ATTACHMENT0;
    if (colorAttachmentId >= mColorAttachments.Length()) {
        MOZ_ASSERT(false);
        return mColorAttachments[0];
    }

    return mColorAttachments[colorAttachmentId];
}

void
WebGLFramebuffer::DetachTexture(const WebGLTexture* tex)
{
    for (size_t i = 0; i < mColorAttachments.Length(); i++) {
        if (mColorAttachments[i].Texture() == tex) {
            FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_TEXTURE_2D, nullptr, 0);
            // a texture might be attached more that once while editing the framebuffer
        }
    }

    if (mDepthAttachment.Texture() == tex)
        FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nullptr, 0);
    if (mStencilAttachment.Texture() == tex)
        FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nullptr, 0);
    if (mDepthStencilAttachment.Texture() == tex)
        FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nullptr, 0);
}

void
WebGLFramebuffer::DetachRenderbuffer(const WebGLRenderbuffer* rb)
{
    for (size_t i = 0; i < mColorAttachments.Length(); i++) {
        if (mColorAttachments[0].Renderbuffer() == rb) {
            FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_RENDERBUFFER, nullptr);
            // a renderbuffer might be attached more that once while editing the framebuffer
        }
    }

    if (mDepthAttachment.Renderbuffer() == rb)
        FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT, LOCAL_GL_RENDERBUFFER, nullptr);
    if (mStencilAttachment.Renderbuffer() == rb)
        FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT, LOCAL_GL_RENDERBUFFER, nullptr);
    if (mDepthStencilAttachment.Renderbuffer() == rb)
        FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT, LOCAL_GL_RENDERBUFFER, nullptr);
}

bool
WebGLFramebuffer::HasDefinedAttachments() const
{
    bool hasAttachments = false;

    for (size_t i = 0; i < mColorAttachments.Length(); i++) {
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

    for (size_t i = 0; i < mColorAttachments.Length(); i++) {
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

    for (size_t i = 0; i < mColorAttachments.Length(); i++) {
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

    for (size_t i = 0; i < mColorAttachments.Length(); i++) {
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

GLenum
WebGLFramebuffer::PrecheckFramebufferStatus() const
{
    MOZ_ASSERT(mContext->mBoundFramebuffer == this);

    if (!HasDefinedAttachments())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT; // No attachments

    if (HasIncompleteAttachments())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

    if (!AllImageRectsMatch())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS; // No consistent size

    if (HasDepthStencilConflict())
        return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;

    return LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

GLenum
WebGLFramebuffer::CheckFramebufferStatus() const
{
    GLenum precheckStatus = PrecheckFramebufferStatus();
    if (precheckStatus != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return precheckStatus;

    // Looks good on our end. Let's ask the driver.
    mContext->MakeContextCurrent();

    // Ok, attach our chosen flavor of {DEPTH, STENCIL, DEPTH_STENCIL}.
    FinalizeAttachments();

    return mContext->gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
}



bool
WebGLFramebuffer::CheckAndInitializeAttachments()
{
    MOZ_ASSERT(mContext->mBoundFramebuffer == this);

    if (CheckFramebufferStatus() != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    // Cool! We've checked out ok. Just need to initialize.
    size_t colorAttachmentCount = size_t(mColorAttachments.Length());

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
    bool colorAttachmentsMask[WebGLContext::sMaxColorAttachments] = { false };
    MOZ_ASSERT(colorAttachmentCount <= WebGLContext::sMaxColorAttachments);

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

bool WebGLFramebuffer::CheckColorAttachementNumber(GLenum attachment, const char* functionName) const
{
    const char* const errorFormating = "%s: attachment: invalid enum value 0x%x";

    if (mContext->IsExtensionEnabled(WebGLContext::WEBGL_draw_buffers)) {
        if (attachment < LOCAL_GL_COLOR_ATTACHMENT0 ||
            attachment >= GLenum(LOCAL_GL_COLOR_ATTACHMENT0 + mContext->mGLMaxColorAttachments))
        {
            mContext->ErrorInvalidEnum(errorFormating, functionName, attachment);
            return false;
        }
    } else if (attachment != LOCAL_GL_COLOR_ATTACHMENT0) {
        if (attachment > LOCAL_GL_COLOR_ATTACHMENT0 &&
            attachment <= LOCAL_GL_COLOR_ATTACHMENT15)
        {
            mContext->ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x. "
                                       "Try the WEBGL_draw_buffers extension if supported.", functionName, attachment);
            return false;
        } else {
            mContext->ErrorInvalidEnum(errorFormating, functionName, attachment);
            return false;
        }
    }

    return true;
}

void WebGLFramebuffer::EnsureColorAttachments(size_t colorAttachmentId)
{
    MOZ_ASSERT(colorAttachmentId < WebGLContext::sMaxColorAttachments);

    size_t currentAttachmentCount = mColorAttachments.Length();
    if (colorAttachmentId < currentAttachmentCount)
        return;

    mColorAttachments.SetLength(colorAttachmentId + 1);

    for (size_t i = colorAttachmentId; i >= currentAttachmentCount; i--) {
        mColorAttachments[i].mAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0 + i;
    }
}

void
WebGLFramebuffer::FinalizeAttachments() const
{
    for (size_t i = 0; i < ColorAttachmentCount(); i++) {
        if (ColorAttachment(i).IsDefined())
            ColorAttachment(i).FinalizeAttachment(LOCAL_GL_COLOR_ATTACHMENT0 + i);
    }

    if (DepthAttachment().IsDefined())
        DepthAttachment().FinalizeAttachment(LOCAL_GL_DEPTH_ATTACHMENT);

    if (StencilAttachment().IsDefined())
        StencilAttachment().FinalizeAttachment(LOCAL_GL_STENCIL_ATTACHMENT);

    if (DepthStencilAttachment().IsDefined())
        DepthStencilAttachment().FinalizeAttachment(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);
}

inline void
ImplCycleCollectionUnlink(mozilla::WebGLFramebuffer::Attachment& aField)
{
    aField.mTexturePtr = nullptr;
    aField.mRenderbufferPtr = nullptr;
}

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            mozilla::WebGLFramebuffer::Attachment& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
    CycleCollectionNoteChild(aCallback, aField.mTexturePtr.get(),
                             aName, aFlags);

    CycleCollectionNoteChild(aCallback, aField.mRenderbufferPtr.get(),
                             aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_4(WebGLFramebuffer,
  mColorAttachments,
  mDepthAttachment,
  mStencilAttachment,
  mDepthStencilAttachment)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLFramebuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLFramebuffer, Release)
