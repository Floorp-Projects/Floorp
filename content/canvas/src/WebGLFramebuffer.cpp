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
#include "nsContentUtils.h"
#include "WebGLTexture.h"
#include "WebGLRenderbuffer.h"

using namespace mozilla;

JSObject*
WebGLFramebuffer::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope) {
    return dom::WebGLFramebufferBinding::Wrap(cx, scope, this);
}

WebGLFramebuffer::WebGLFramebuffer(WebGLContext *context)
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
WebGLFramebuffer::Attachment::IsDeleteRequested() const {
    return Texture() ? Texture()->IsDeleteRequested()
         : Renderbuffer() ? Renderbuffer()->IsDeleteRequested()
         : false;
}

bool
WebGLFramebuffer::Attachment::HasAlpha() const {
    WebGLenum format = 0;
    if (Texture() && Texture()->HasImageInfoAt(mTextureLevel, mTextureCubeMapFace))
        format = Texture()->ImageInfoAt(mTextureLevel, mTextureCubeMapFace).Format();
    else if (Renderbuffer())
        format = Renderbuffer()->InternalFormat();
    return format == LOCAL_GL_RGBA ||
           format == LOCAL_GL_LUMINANCE_ALPHA ||
           format == LOCAL_GL_ALPHA ||
           format == LOCAL_GL_RGBA4 ||
           format == LOCAL_GL_RGB5_A1;
}

void
WebGLFramebuffer::Attachment::SetTexture(WebGLTexture *tex, WebGLint level, WebGLenum face) {
    mTexturePtr = tex;
    mRenderbufferPtr = nullptr;
    mTextureLevel = level;
    mTextureCubeMapFace = face;
}

bool
WebGLFramebuffer::Attachment::HasUninitializedRenderbuffer() const {
    return mRenderbufferPtr && !mRenderbufferPtr->Initialized();
}

const WebGLRectangleObject*
WebGLFramebuffer::Attachment::RectangleObject() const {
    if (Texture() && Texture()->HasImageInfoAt(mTextureLevel, mTextureCubeMapFace))
        return &Texture()->ImageInfoAt(mTextureLevel, mTextureCubeMapFace);
    else if (Renderbuffer())
        return Renderbuffer();
    else
        return nullptr;
}

bool
WebGLFramebuffer::Attachment::HasSameDimensionsAs(const Attachment& other) const {
    const WebGLRectangleObject *thisRect = RectangleObject();
    const WebGLRectangleObject *otherRect = other.RectangleObject();
    return thisRect &&
           otherRect &&
           thisRect->HasSameDimensionsAs(*otherRect);
}

bool
WebGLFramebuffer::Attachment::IsComplete() const {
    const WebGLRectangleObject *thisRect = RectangleObject();

    if (!thisRect ||
        !thisRect->Width() ||
        !thisRect->Height())
        return false;

    if (mTexturePtr) {
        if (!mTexturePtr->HasImageInfoAt(0, 0))
            return false;

        WebGLenum format = mTexturePtr->ImageInfoAt(0).Format();

        if (mAttachmentPoint == LOCAL_GL_DEPTH_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_COMPONENT;
        }
        else if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_STENCIL;
        }
        else if (mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                 mAttachmentPoint < WebGLenum(LOCAL_GL_COLOR_ATTACHMENT0 + WebGLContext::sMaxColorAttachments)) {
            return (format == LOCAL_GL_ALPHA ||
                    format == LOCAL_GL_LUMINANCE ||
                    format == LOCAL_GL_LUMINANCE_ALPHA ||
                    format == LOCAL_GL_RGB ||
                    format == LOCAL_GL_RGBA);
        }
        MOZ_NOT_REACHED("Invalid WebGL attachment poin?");
    }

    if (mRenderbufferPtr) {
        WebGLenum format = mRenderbufferPtr->InternalFormat();

        if (mAttachmentPoint == LOCAL_GL_DEPTH_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_COMPONENT16;
        }
        else if (mAttachmentPoint == LOCAL_GL_STENCIL_ATTACHMENT) {
            return format == LOCAL_GL_STENCIL_INDEX8;
        }
        else if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            return format == LOCAL_GL_DEPTH_STENCIL;
        }
        else if (mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                 mAttachmentPoint < WebGLenum(LOCAL_GL_COLOR_ATTACHMENT0 + WebGLContext::sMaxColorAttachments)) {
            return (format == LOCAL_GL_RGB565 ||
                    format == LOCAL_GL_RGB5_A1 ||
                    format == LOCAL_GL_RGBA4);
        }
        MOZ_NOT_REACHED("Invalid WebGL attachment poin?");
    }

    NS_ABORT(); // should never get there
    return false;
}

void
WebGLFramebuffer::Delete() {
    mColorAttachments.Clear();
    mDepthAttachment.Reset();
    mStencilAttachment.Reset();
    mDepthStencilAttachment.Reset();
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteFramebuffers(1, &mGLName);
    LinkedListElement<WebGLFramebuffer>::removeFrom(mContext->mFramebuffers);
}

void
WebGLFramebuffer::FramebufferRenderbuffer(WebGLenum target,
                             WebGLenum attachment,
                             WebGLenum rbtarget,
                             WebGLRenderbuffer *wrb)
{
    MOZ_ASSERT(mContext->mBoundFramebuffer == this);
    if (!mContext->ValidateObjectAllowNull("framebufferRenderbuffer: renderbuffer", wrb))
    {
        return;
    }

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

    mContext->MakeContextCurrent();
    WebGLuint parambuffername = wrb ? wrb->GLName() : 0;
    if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        WebGLuint depthbuffername = parambuffername;
        WebGLuint stencilbuffername = parambuffername;
        if (!parambuffername){
            depthbuffername   = mDepthAttachment.Renderbuffer()   ? mDepthAttachment.Renderbuffer()->GLName()   : 0;
            stencilbuffername = mStencilAttachment.Renderbuffer() ? mStencilAttachment.Renderbuffer()->GLName() : 0;
        }
        mContext->gl->fFramebufferRenderbuffer(target, LOCAL_GL_DEPTH_ATTACHMENT, rbtarget, depthbuffername);
        mContext->gl->fFramebufferRenderbuffer(target, LOCAL_GL_STENCIL_ATTACHMENT, rbtarget, stencilbuffername);
    } else {
        WebGLuint renderbuffername = parambuffername;
        if(!parambuffername && (attachment == LOCAL_GL_DEPTH_ATTACHMENT || attachment == LOCAL_GL_STENCIL_ATTACHMENT)){
            renderbuffername = mDepthStencilAttachment.Renderbuffer() ? mDepthStencilAttachment.Renderbuffer()->GLName() : 0;
        }
        mContext->gl->fFramebufferRenderbuffer(target, attachment, rbtarget, renderbuffername);
    }
}

void
WebGLFramebuffer::FramebufferTexture2D(WebGLenum target,
                          WebGLenum attachment,
                          WebGLenum textarget,
                          WebGLTexture *wtex,
                          WebGLint level)
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
        return mContext->ErrorInvalidEnumInfo("framebufferTexture2D: invalid texture target", textarget);

    if (level != 0)
        return mContext->ErrorInvalidValue("framebufferTexture2D: level must be 0");

    size_t face = WebGLTexture::FaceForTarget(textarget);
    switch (attachment) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
        mDepthAttachment.SetTexture(wtex, level, face);
        break;
    case LOCAL_GL_STENCIL_ATTACHMENT:
        mStencilAttachment.SetTexture(wtex, level, face);
        break;
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        mDepthStencilAttachment.SetTexture(wtex, level, face);
        break;
    default:
        if (!CheckColorAttachementNumber(attachment, "framebufferTexture2D")){
            return;
        }

        size_t colorAttachmentId = size_t(attachment - LOCAL_GL_COLOR_ATTACHMENT0);
        EnsureColorAttachments(colorAttachmentId);
        mColorAttachments[colorAttachmentId].SetTexture(wtex, level, face);
        break;
    }

    mContext->MakeContextCurrent();
    WebGLuint paramtexturename = wtex ? wtex->GLName() : 0;
    if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        WebGLuint depthtexturename = paramtexturename;
        WebGLuint stenciltexturename = paramtexturename;
        if(!paramtexturename){
            depthtexturename   = mDepthAttachment.Texture()   ? mDepthAttachment.Texture()->GLName()   : 0;
            stenciltexturename = mStencilAttachment.Texture() ? mStencilAttachment.Texture()->GLName() : 0;
        }
        mContext->gl->fFramebufferTexture2D(target, LOCAL_GL_DEPTH_ATTACHMENT, textarget, depthtexturename, level);
        mContext->gl->fFramebufferTexture2D(target, LOCAL_GL_STENCIL_ATTACHMENT, textarget, stenciltexturename, level);
    } else {
        WebGLuint texturename = paramtexturename;
        if(!paramtexturename && (attachment == LOCAL_GL_DEPTH_ATTACHMENT || attachment == LOCAL_GL_STENCIL_ATTACHMENT)){
            texturename = mDepthStencilAttachment.Texture() ? mDepthStencilAttachment.Texture()->GLName() : 0;
        }
        mContext->gl->fFramebufferTexture2D(target, attachment, textarget, texturename, level);
    }

    return;
}

bool
WebGLFramebuffer::HasIncompleteAttachment() const {
    int32_t colorAttachmentCount = mColorAttachments.Length();

    for (int32_t i = 0; i < colorAttachmentCount; i++)
    {
        if (mColorAttachments[i].IsDefined() && !mColorAttachments[i].IsComplete())
        {
            return true;
        }
    }

    return ((mDepthAttachment.IsDefined() && !mDepthAttachment.IsComplete()) ||
            (mStencilAttachment.IsDefined() && !mStencilAttachment.IsComplete()) ||
            (mDepthStencilAttachment.IsDefined() && !mDepthStencilAttachment.IsComplete()));
}

bool
WebGLFramebuffer::HasAttachmentsOfMismatchedDimensions() const {
    int32_t colorAttachmentCount = mColorAttachments.Length();

    for (int32_t i = 1; i < colorAttachmentCount; i++)
    {
        if (mColorAttachments[i].IsDefined() && !mColorAttachments[i].HasSameDimensionsAs(mColorAttachments[0]))
        {
            return true;
        }
    }

    return ((mDepthAttachment.IsDefined() && !mDepthAttachment.HasSameDimensionsAs(mColorAttachments[0])) ||
            (mStencilAttachment.IsDefined() && !mStencilAttachment.HasSameDimensionsAs(mColorAttachments[0])) ||
            (mDepthStencilAttachment.IsDefined() && !mDepthStencilAttachment.HasSameDimensionsAs(mColorAttachments[0])));
}

const WebGLFramebuffer::Attachment&
WebGLFramebuffer::GetAttachment(WebGLenum attachment) const {
    if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
        return mDepthStencilAttachment;
    if (attachment == LOCAL_GL_DEPTH_ATTACHMENT)
        return mDepthAttachment;
    if (attachment == LOCAL_GL_STENCIL_ATTACHMENT)
        return mStencilAttachment;

    if (!CheckColorAttachementNumber(attachment, "getAttachment")) {
        NS_ABORT();
        return mColorAttachments[0];
    }

    uint32_t colorAttachmentId = uint32_t(attachment - LOCAL_GL_COLOR_ATTACHMENT0);

    if (colorAttachmentId >= mColorAttachments.Length()) {
        NS_ABORT();
        return mColorAttachments[0];
    }

    return mColorAttachments[colorAttachmentId];
}

void
WebGLFramebuffer::DetachTexture(const WebGLTexture *tex) {
    int32_t colorAttachmentCount = mColorAttachments.Length();

    for (int32_t i = 0; i < colorAttachmentCount; i++) {
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
WebGLFramebuffer::DetachRenderbuffer(const WebGLRenderbuffer *rb) {
    int32_t colorAttachmentCount = mColorAttachments.Length();

    for (int32_t i = 0; i < colorAttachmentCount; i++) {
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
WebGLFramebuffer::CheckAndInitializeRenderbuffers()
{
    MOZ_ASSERT(mContext->mBoundFramebuffer == this);
    // enforce WebGL section 6.5 which is WebGL-specific, hence OpenGL itself would not
    // generate the INVALID_FRAMEBUFFER_OPERATION that we need here
    if (HasDepthStencilConflict())
        return false;

    if (HasIncompleteAttachment())
        return false;

    size_t colorAttachmentCount = size_t(mColorAttachments.Length());

    {
        bool hasUnitializedRenderbuffers = false;

        for (size_t i = 0; i < colorAttachmentCount; i++) {
            hasUnitializedRenderbuffers |= mColorAttachments[i].HasUninitializedRenderbuffer();
        }

        if (!hasUnitializedRenderbuffers &&
            !mDepthAttachment.HasUninitializedRenderbuffer() &&
            !mStencilAttachment.HasUninitializedRenderbuffer() &&
            !mDepthStencilAttachment.HasUninitializedRenderbuffer())
        {
            return true;
        }
    }

    // ensure INVALID_FRAMEBUFFER_OPERATION in zero-size case
    const WebGLRectangleObject *rect = mColorAttachments[0].RectangleObject();
    if (!rect ||
        !rect->Width() ||
        !rect->Height())
        return false;

    mContext->MakeContextCurrent();

    WebGLenum status = mContext->CheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    uint32_t mask = 0;
    bool colorAttachmentsMask[WebGLContext::sMaxColorAttachments] = { false };

    MOZ_ASSERT( colorAttachmentCount <= WebGLContext::sMaxColorAttachments );

    for (size_t i = 0; i < colorAttachmentCount; i++)
    {
        colorAttachmentsMask[i] = mColorAttachments[i].HasUninitializedRenderbuffer();

        if (colorAttachmentsMask[i]) {
            mask |= LOCAL_GL_COLOR_BUFFER_BIT;
        }
    }

    if (mDepthAttachment.HasUninitializedRenderbuffer() ||
        mDepthStencilAttachment.HasUninitializedRenderbuffer())
    {
        mask |= LOCAL_GL_DEPTH_BUFFER_BIT;
    }

    if (mStencilAttachment.HasUninitializedRenderbuffer() ||
        mDepthStencilAttachment.HasUninitializedRenderbuffer())
    {
        mask |= LOCAL_GL_STENCIL_BUFFER_BIT;
    }

    mContext->ForceClearFramebufferWithDefaultValues(mask, colorAttachmentsMask);

    for (size_t i = 0; i < colorAttachmentCount; i++)
    {
        if (colorAttachmentsMask[i]) {
            mColorAttachments[i].Renderbuffer()->SetInitialized(true);
        }
    }

    if (mDepthAttachment.HasUninitializedRenderbuffer())
        mDepthAttachment.Renderbuffer()->SetInitialized(true);

    if (mStencilAttachment.HasUninitializedRenderbuffer())
        mStencilAttachment.Renderbuffer()->SetInitialized(true);

    if (mDepthStencilAttachment.HasUninitializedRenderbuffer())
        mDepthStencilAttachment.Renderbuffer()->SetInitialized(true);

    return true;
}

bool WebGLFramebuffer::CheckColorAttachementNumber(WebGLenum attachment, const char * functionName) const
{
    const char* const errorFormating = "%s: attachment: invalid enum value 0x%x";

    if (mContext->IsExtensionEnabled(WebGLContext::WEBGL_draw_buffers))
    {
        if (attachment < LOCAL_GL_COLOR_ATTACHMENT0 ||
            attachment > WebGLenum(LOCAL_GL_COLOR_ATTACHMENT0 + mContext->mGLMaxColorAttachments))
        {
            mContext->ErrorInvalidEnum(errorFormating, functionName, attachment);
            return false;
        }
    }
    else if (attachment != LOCAL_GL_COLOR_ATTACHMENT0)
    {
        if (attachment > LOCAL_GL_COLOR_ATTACHMENT0 &&
            attachment <= LOCAL_GL_COLOR_ATTACHMENT15)
        {
            mContext->ErrorInvalidEnum("%s: attachment: invalid enum value 0x%x. "
                                       "Try the WEBGL_draw_buffers extension if supported.", functionName, attachment);
            return false;
        }
        else
        {
            mContext->ErrorInvalidEnum(errorFormating, functionName, attachment);
            return false;
        }
    }

    return true;
}

void WebGLFramebuffer::EnsureColorAttachments(size_t colorAttachmentId) {
    size_t currentAttachmentCount = mColorAttachments.Length();

    if (mColorAttachments.Length() > colorAttachmentId) {
        return;
    }

    MOZ_ASSERT( colorAttachmentId < WebGLContext::sMaxColorAttachments );

    mColorAttachments.SetLength(colorAttachmentId + 1);

    for (size_t i = colorAttachmentId; i >= currentAttachmentCount; i--) {
        mColorAttachments[i].mAttachmentPoint = LOCAL_GL_COLOR_ATTACHMENT0 + i;
    }
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
    CycleCollectionNoteEdgeName(aCallback, aName, aFlags);
    aCallback.NoteXPCOMChild(aField.mTexturePtr);
    aCallback.NoteXPCOMChild(aField.mRenderbufferPtr);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_7(WebGLFramebuffer,
  mColorAttachments,
  mDepthAttachment.mTexturePtr,
  mDepthAttachment.mRenderbufferPtr,
  mStencilAttachment.mTexturePtr,
  mStencilAttachment.mRenderbufferPtr,
  mDepthStencilAttachment.mTexturePtr,
  mDepthStencilAttachment.mRenderbufferPtr)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLFramebuffer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLFramebuffer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
