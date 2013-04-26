/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLFramebuffer.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "nsContentUtils.h"

using namespace mozilla;

JSObject*
WebGLFramebuffer::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope) {
    return dom::WebGLFramebufferBinding::Wrap(cx, scope, this);
}

WebGLFramebuffer::WebGLFramebuffer(WebGLContext *context)
    : WebGLContextBoundObject(context)
    , mHasEverBeenBound(false)
    , mColorAttachment(LOCAL_GL_COLOR_ATTACHMENT0)
    , mDepthAttachment(LOCAL_GL_DEPTH_ATTACHMENT)
    , mStencilAttachment(LOCAL_GL_STENCIL_ATTACHMENT)
    , mDepthStencilAttachment(LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
{
    SetIsDOMBinding();
    mContext->MakeContextCurrent();
    mContext->gl->fGenFramebuffers(1, &mGLName);
    mContext->mFramebuffers.insertBack(this);
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
        switch (mAttachmentPoint)
        {
            case LOCAL_GL_COLOR_ATTACHMENT0:
                return format == LOCAL_GL_ALPHA ||
                       format == LOCAL_GL_LUMINANCE ||
                       format == LOCAL_GL_LUMINANCE_ALPHA ||
                       format == LOCAL_GL_RGB ||
                       format == LOCAL_GL_RGBA;
            case LOCAL_GL_DEPTH_ATTACHMENT:
                return format == LOCAL_GL_DEPTH_COMPONENT;
            case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
                return format == LOCAL_GL_DEPTH_STENCIL;

            default:
                MOZ_NOT_REACHED("Invalid WebGL texture format?");
        }
    }

    if (mRenderbufferPtr) {
        WebGLenum format = mRenderbufferPtr->InternalFormat();
        switch (mAttachmentPoint) {
            case LOCAL_GL_COLOR_ATTACHMENT0:
                return format == LOCAL_GL_RGB565 ||
                       format == LOCAL_GL_RGB5_A1 ||
                       format == LOCAL_GL_RGBA4;
            case LOCAL_GL_DEPTH_ATTACHMENT:
                return format == LOCAL_GL_DEPTH_COMPONENT16;
            case LOCAL_GL_STENCIL_ATTACHMENT:
                return format == LOCAL_GL_STENCIL_INDEX8;
            case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
                return format == LOCAL_GL_DEPTH_STENCIL;
            default:
                NS_ABORT(); // should have been validated earlier
        }
    }

    NS_ABORT(); // should never get there
    return false;
}

void
WebGLFramebuffer::Delete() {
    mColorAttachment.Reset();
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
        if (attachment != LOCAL_GL_COLOR_ATTACHMENT0)
            return mContext->ErrorInvalidEnumInfo("framebufferRenderbuffer: attachment", attachment);

        mColorAttachment.SetRenderbuffer(wrb);
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
        if (attachment != LOCAL_GL_COLOR_ATTACHMENT0)
            return mContext->ErrorInvalidEnumInfo("framebufferTexture2D: attachment", attachment);

        mColorAttachment.SetTexture(wtex, level, face);
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

const WebGLFramebuffer::Attachment&
WebGLFramebuffer::GetAttachment(WebGLenum attachment) const {
    if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
        return mDepthStencilAttachment;
    if (attachment == LOCAL_GL_DEPTH_ATTACHMENT)
        return mDepthAttachment;
    if (attachment == LOCAL_GL_STENCIL_ATTACHMENT)
        return mStencilAttachment;

    NS_ASSERTION(attachment == LOCAL_GL_COLOR_ATTACHMENT0, "bad attachment!");
    return mColorAttachment;
}

void
WebGLFramebuffer::DetachTexture(const WebGLTexture *tex) {
    if (mColorAttachment.Texture() == tex)
        FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_TEXTURE_2D, nullptr, 0);
    if (mDepthAttachment.Texture() == tex)
        FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nullptr, 0);
    if (mStencilAttachment.Texture() == tex)
        FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nullptr, 0);
    if (mDepthStencilAttachment.Texture() == tex)
        FramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT, LOCAL_GL_TEXTURE_2D, nullptr, 0);
}

void
WebGLFramebuffer::DetachRenderbuffer(const WebGLRenderbuffer *rb) {
    if (mColorAttachment.Renderbuffer() == rb)
        FramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_RENDERBUFFER, nullptr);
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
    // enforce WebGL section 6.5 which is WebGL-specific, hence OpenGL itself would not
    // generate the INVALID_FRAMEBUFFER_OPERATION that we need here
    if (HasDepthStencilConflict())
        return false;

    if (HasIncompleteAttachment())
        return false;

    if (!mColorAttachment.HasUninitializedRenderbuffer() &&
        !mDepthAttachment.HasUninitializedRenderbuffer() &&
        !mStencilAttachment.HasUninitializedRenderbuffer() &&
        !mDepthStencilAttachment.HasUninitializedRenderbuffer())
        return true;

    // ensure INVALID_FRAMEBUFFER_OPERATION in zero-size case
    const WebGLRectangleObject *rect = mColorAttachment.RectangleObject();
    if (!rect ||
        !rect->Width() ||
        !rect->Height())
        return false;

    mContext->MakeContextCurrent();

    WebGLenum status = mContext->CheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    uint32_t mask = 0;

    if (mColorAttachment.HasUninitializedRenderbuffer())
        mask |= LOCAL_GL_COLOR_BUFFER_BIT;

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

    mContext->ForceClearFramebufferWithDefaultValues(mask);

    if (mColorAttachment.HasUninitializedRenderbuffer())
        mColorAttachment.Renderbuffer()->SetInitialized(true);

    if (mDepthAttachment.HasUninitializedRenderbuffer())
        mDepthAttachment.Renderbuffer()->SetInitialized(true);

    if (mStencilAttachment.HasUninitializedRenderbuffer())
        mStencilAttachment.Renderbuffer()->SetInitialized(true);

    if (mDepthStencilAttachment.HasUninitializedRenderbuffer())
        mDepthStencilAttachment.Renderbuffer()->SetInitialized(true);

    return true;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_8(WebGLFramebuffer,
  mColorAttachment.mTexturePtr,
  mColorAttachment.mRenderbufferPtr,
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
