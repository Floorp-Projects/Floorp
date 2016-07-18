/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLFramebuffer.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "nsPrintfCString.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"

namespace mozilla {

WebGLFBAttachPoint::WebGLFBAttachPoint(WebGLFramebuffer* fb, GLenum attachmentPoint)
    : mFB(fb)
    , mAttachmentPoint(attachmentPoint)
    , mTexImageTarget(LOCAL_GL_NONE)
    , mTexImageLayer(0)
    , mTexImageLevel(0)
{ }

WebGLFBAttachPoint::~WebGLFBAttachPoint()
{
    MOZ_ASSERT(!mRenderbufferPtr);
    MOZ_ASSERT(!mTexturePtr);
}

void
WebGLFBAttachPoint::Unlink()
{
    Clear();
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
    /*
    return (Renderbuffer() && Renderbuffer()->IsDefined()) ||
           (Texture() && Texture()->ImageInfoAt(mTexImageTarget,
                                                mTexImageLevel).IsDefined());
    */
    return (Renderbuffer() || Texture());
}

const webgl::FormatUsageInfo*
WebGLFBAttachPoint::Format() const
{
    MOZ_ASSERT(IsDefined());

    if (Texture())
        return Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).mFormat;

    if (Renderbuffer())
        return Renderbuffer()->Format();

    return nullptr;
}

uint32_t
WebGLFBAttachPoint::Samples() const
{
    MOZ_ASSERT(IsDefined());

    if (mRenderbufferPtr)
        return mRenderbufferPtr->Samples();

    return 0;
}

bool
WebGLFBAttachPoint::HasAlpha() const
{
    return Format()->format->a;
}

const webgl::FormatUsageInfo*
WebGLFramebuffer::GetFormatForAttachment(const WebGLFBAttachPoint& attachment) const
{
    MOZ_ASSERT(attachment.IsDefined());
    MOZ_ASSERT(attachment.Texture() || attachment.Renderbuffer());

    return attachment.Format();
}

bool
WebGLFBAttachPoint::IsReadableFloat() const
{
    auto formatUsage = Format();
    MOZ_ASSERT(formatUsage);

    auto format = formatUsage->format;
    if (!format->IsColorFormat())
        return false;

    return format->componentType == webgl::ComponentType::Float;
}

void
WebGLFBAttachPoint::Clear()
{
    if (mRenderbufferPtr) {
        MOZ_ASSERT(!mTexturePtr);
        mRenderbufferPtr->UnmarkAttachment(*this);
    } else if (mTexturePtr) {
        mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel).RemoveAttachPoint(this);
    }

    mTexturePtr = nullptr;
    mRenderbufferPtr = nullptr;

    OnBackingStoreRespecified();
}

void
WebGLFBAttachPoint::SetTexImage(WebGLTexture* tex, TexImageTarget target, GLint level)
{
    SetTexImageLayer(tex, target, level, 0);
}

void
WebGLFBAttachPoint::SetTexImageLayer(WebGLTexture* tex, TexImageTarget target,
                                     GLint level, GLint layer)
{
    Clear();

    mTexturePtr = tex;
    mTexImageTarget = target;
    mTexImageLevel = level;
    mTexImageLayer = layer;

    if (mTexturePtr) {
        mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel).AddAttachPoint(this);
    }
}

void
WebGLFBAttachPoint::SetRenderbuffer(WebGLRenderbuffer* rb)
{
    Clear();

    mRenderbufferPtr = rb;

    if (mRenderbufferPtr) {
        mRenderbufferPtr->MarkAttachment(*this);
    }
}

bool
WebGLFBAttachPoint::HasUninitializedImageData() const
{
    if (!HasImage())
        return false;

    if (mRenderbufferPtr)
        return mRenderbufferPtr->HasUninitializedImageData();

    MOZ_ASSERT(mTexturePtr);

    auto& imageInfo = mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel);
    MOZ_ASSERT(imageInfo.IsDefined());

    return !imageInfo.IsDataInitialized();
}

void
WebGLFBAttachPoint::SetImageDataStatus(WebGLImageDataStatus newStatus)
{
    if (!HasImage())
        return;

    if (mRenderbufferPtr) {
        mRenderbufferPtr->mImageDataStatus = newStatus;
        return;
    }

    MOZ_ASSERT(mTexturePtr);

    auto& imageInfo = mTexturePtr->ImageInfoAt(mTexImageTarget, mTexImageLevel);
    MOZ_ASSERT(imageInfo.IsDefined());

    const bool isDataInitialized = (newStatus == WebGLImageDataStatus::InitializedImageData);
    imageInfo.SetIsDataInitialized(isDataInitialized, mTexturePtr);
}

bool
WebGLFBAttachPoint::HasImage() const
{
    if (Texture() && Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).IsDefined())
        return true;

    if (Renderbuffer() && Renderbuffer()->IsDefined())
        return true;

    return false;
}

void
WebGLFBAttachPoint::Size(uint32_t* const out_width, uint32_t* const out_height) const
{
    MOZ_ASSERT(HasImage());

    if (Renderbuffer()) {
        *out_width = Renderbuffer()->Width();
        *out_height = Renderbuffer()->Height();
        return;
    }

    MOZ_ASSERT(Texture());
    MOZ_ASSERT(Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel).IsDefined());
    const auto& imageInfo = Texture()->ImageInfoAt(mTexImageTarget, mTexImageLevel);

    *out_width = imageInfo.mWidth;
    *out_height = imageInfo.mHeight;
}

void
WebGLFBAttachPoint::OnBackingStoreRespecified() const
{
    mFB->InvalidateFramebufferStatus();
}

void
WebGLFBAttachPoint::AttachmentName(nsCString* out) const
{
    switch (mAttachmentPoint) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
        out->AssignLiteral("DEPTH_ATTACHMENT");
        return;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        out->AssignLiteral("STENCIL_ATTACHMENT");
        return;

    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        out->AssignLiteral("DEPTH_STENCIL_ATTACHMENT");
        return;

    default:
        MOZ_ASSERT(mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0);
        out->AssignLiteral("COLOR_ATTACHMENT");
        const uint32_t n = mAttachmentPoint - LOCAL_GL_COLOR_ATTACHMENT0;
        out->AppendInt(n);
        return;
    }
}

bool
WebGLFBAttachPoint::IsComplete(WebGLContext* webgl, nsCString* const out_info) const
{
    MOZ_ASSERT(IsDefined());

    if (!HasImage()) {
        AttachmentName(out_info);
        out_info->AppendLiteral("'s image is not defined");
        return false;
    }

    uint32_t width;
    uint32_t height;
    Size(&width, &height);
    if (!width || !height) {
        AttachmentName(out_info);
        out_info->AppendLiteral(" has no width or height");
        return false;
    }

    const auto formatUsage = Format();
    if (!formatUsage->IsRenderable()) {
        nsAutoCString attachName;
        AttachmentName(&attachName);

        *out_info = nsPrintfCString("%s has an effective format of %s, which is not"
                                    " renderable",
                                    attachName.BeginReading(), formatUsage->format->name);
        return false;
    }

    const auto format = formatUsage->format;

    bool hasRequiredBits;

    switch (mAttachmentPoint) {
    case LOCAL_GL_DEPTH_ATTACHMENT:
        hasRequiredBits = format->d;
        break;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        hasRequiredBits = format->s;
        break;

    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        MOZ_ASSERT(!webgl->IsWebGL2());
        hasRequiredBits = (format->d && format->s);
        break;

    default:
        MOZ_ASSERT(mAttachmentPoint >= LOCAL_GL_COLOR_ATTACHMENT0);
        hasRequiredBits = format->IsColorFormat();
        break;
    }

    if (!hasRequiredBits) {
        AttachmentName(out_info);
        out_info->AppendLiteral("'s format is missing required color/depth/stencil bits");
        return false;
    }

    if (!webgl->IsWebGL2()) {
        bool hasSurplusPlanes = false;

        switch (mAttachmentPoint) {
        case LOCAL_GL_DEPTH_ATTACHMENT:
            hasSurplusPlanes = format->s;
            break;

        case LOCAL_GL_STENCIL_ATTACHMENT:
            hasSurplusPlanes = format->d;
            break;
        }

        if (hasSurplusPlanes) {
            AttachmentName(out_info);
            out_info->AppendLiteral("'s format has depth or stencil bits when it"
                                    " shouldn't");
            return false;
        }
    }

    return true;
}

void
WebGLFBAttachPoint::FinalizeAttachment(gl::GLContext* gl, FBTarget target,
                                       GLenum attachment) const
{
    if (!HasImage()) {
        switch (attachment) {
        case LOCAL_GL_DEPTH_ATTACHMENT:
        case LOCAL_GL_STENCIL_ATTACHMENT:
        case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
            break;

        default:
            gl->fFramebufferRenderbuffer(target, attachment,
                                         LOCAL_GL_RENDERBUFFER, 0);
            break;
        }

        return;
    }
    MOZ_ASSERT(HasImage());

    if (Texture()) {
        MOZ_ASSERT(gl == Texture()->mContext->GL());

        const GLenum imageTarget = ImageTarget().get();
        const GLint mipLevel = MipLevel();
        const GLint layer = Layer();
        const GLuint glName = Texture()->mGLName;

        switch (imageTarget) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
                gl->fFramebufferTexture2D(target, LOCAL_GL_DEPTH_ATTACHMENT,
                                          imageTarget, glName, mipLevel);
                gl->fFramebufferTexture2D(target,
                                          LOCAL_GL_STENCIL_ATTACHMENT, imageTarget,
                                          glName, mipLevel);
            } else {
                gl->fFramebufferTexture2D(target, attachment, imageTarget,
                                          glName, mipLevel);
            }
            break;

        case LOCAL_GL_TEXTURE_2D_ARRAY:
        case LOCAL_GL_TEXTURE_3D:
            if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
                gl->fFramebufferTextureLayer(target,
                                             LOCAL_GL_DEPTH_ATTACHMENT, glName, mipLevel,
                                             layer);
                gl->fFramebufferTextureLayer(target,
                                             LOCAL_GL_STENCIL_ATTACHMENT, glName,
                                             mipLevel, layer);
            } else {
                gl->fFramebufferTextureLayer(target, attachment, glName,
                                             mipLevel, layer);
            }
            break;
        }
        return;
    }

    if (Renderbuffer()) {
        Renderbuffer()->DoFramebufferRenderbuffer(target, attachment);
        return;
    }

    MOZ_CRASH("GFX: invalid render buffer");
}

JS::Value
WebGLFBAttachPoint::GetParameter(const char* funcName, WebGLContext* webgl, JSContext* cx,
                                 GLenum target, GLenum attachment, GLenum pname,
                                 ErrorResult* const out_error)
{
    const bool hasAttachment = (mTexturePtr || mRenderbufferPtr);
    if (!hasAttachment) {
        // Divergent between GLES 3 and 2.

        // GLES 2.0.25 p127:
        // "If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is NONE, then querying any
        //  other pname will generate INVALID_ENUM."

        // GLES 3.0.4 p240:
        // "If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is NONE, no framebuffer is
        //  bound to target. In this case querying pname
        //  FRAMEBUFFER_ATTACHMENT_OBJECT_NAME will return zero, and all other queries
        //  will generate an INVALID_OPERATION error."
        switch (pname) {
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return JS::Int32Value(LOCAL_GL_NONE);

        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            if (webgl->IsWebGL2())
                return JS::NullValue();

            break;

        default:
            break;
        }

        if (webgl->IsWebGL2()) {
            webgl->ErrorInvalidOperation("%s: No attachment at %s.", funcName,
                                         webgl->EnumName(attachment));
        } else {
            webgl->ErrorInvalidEnum("%s: No attachment at %s.", funcName,
                                    webgl->EnumName(attachment));
        }
        return JS::NullValue();
    }

    bool isPNameValid = false;
    switch (pname) {
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        return JS::Int32Value(mTexturePtr ? LOCAL_GL_TEXTURE
                                          : LOCAL_GL_RENDERBUFFER);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return (mTexturePtr ? webgl->WebGLObjectAsJSValue(cx, mTexturePtr.get(),
                                                          *out_error)
                            : webgl->WebGLObjectAsJSValue(cx, mRenderbufferPtr.get(),
                                                          *out_error));

        //////

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        if (mTexturePtr)
            return JS::Int32Value(MipLevel());
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        if (mTexturePtr) {
            GLenum face = 0;
            if (mTexturePtr->Target() == LOCAL_GL_TEXTURE_CUBE_MAP) {
                face = ImageTarget().get();
            }
            return JS::Int32Value(face);
        }
        break;

        //////

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
        if (webgl->IsWebGL2() && mTexturePtr) {
            int32_t layer = 0;
            if (ImageTarget() == LOCAL_GL_TEXTURE_2D_ARRAY ||
                ImageTarget() == LOCAL_GL_TEXTURE_3D)
            {
                layer = Layer();
            }
            return JS::Int32Value(layer);
        }
        break;

        //////

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        isPNameValid = webgl->IsWebGL2();
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        isPNameValid = (webgl->IsWebGL2() ||
                        webgl->IsExtensionEnabled(WebGLExtensionID::EXT_sRGB));
        break;
    }

    if (!isPNameValid) {
        webgl->ErrorInvalidEnum("%s: Invalid pname: 0x%04x", funcName, pname);
        return JS::NullValue();
    }

    const auto usage = Format();
    if (!usage)
        return JS::NullValue();

    auto format = usage->format;

    GLint ret = 0;
    switch (pname) {
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
        ret = format->r;
        break;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
        ret = format->g;
        break;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        ret = format->b;
        break;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
        ret = format->a;
        break;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
        ret = format->d;
        break;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
        ret = format->s;
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        ret = (format->isSRGB ? LOCAL_GL_SRGB
                              : LOCAL_GL_LINEAR);
        break;

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        MOZ_ASSERT(attachment != LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);

        if (format->componentType == webgl::ComponentType::Special) {
            // Special format is used for DS mixed format(e.g. D24S8 and D32FS8).
            MOZ_ASSERT(format->unsizedFormat == webgl::UnsizedFormat::DS);
            MOZ_ASSERT(attachment == LOCAL_GL_DEPTH_ATTACHMENT ||
                       attachment == LOCAL_GL_STENCIL_ATTACHMENT);

            if (attachment == LOCAL_GL_DEPTH_ATTACHMENT) {
                switch (format->effectiveFormat) {
                case webgl::EffectiveFormat::DEPTH24_STENCIL8:
                    format = webgl::GetFormat(webgl::EffectiveFormat::DEPTH_COMPONENT24);
                    break;
                case webgl::EffectiveFormat::DEPTH32F_STENCIL8:
                    format = webgl::GetFormat(webgl::EffectiveFormat::DEPTH_COMPONENT32F);
                    break;
                default:
                    MOZ_ASSERT(false, "no matched DS format");
                    break;
                }
            } else if (attachment == LOCAL_GL_STENCIL_ATTACHMENT) {
                switch (format->effectiveFormat) {
                case webgl::EffectiveFormat::DEPTH24_STENCIL8:
                case webgl::EffectiveFormat::DEPTH32F_STENCIL8:
                    format = webgl::GetFormat(webgl::EffectiveFormat::STENCIL_INDEX8);
                    break;
                default:
                    MOZ_ASSERT(false, "no matched DS format");
                    break;
                }
            }
        }

        switch (format->componentType) {
        case webgl::ComponentType::None:
            ret = LOCAL_GL_NONE;
            break;
        case webgl::ComponentType::Int:
            ret = LOCAL_GL_INT;
            break;
        case webgl::ComponentType::UInt:
            ret = LOCAL_GL_UNSIGNED_INT;
            break;
        case webgl::ComponentType::NormInt:
            ret = LOCAL_GL_SIGNED_NORMALIZED;
            break;
        case webgl::ComponentType::NormUInt:
            ret = LOCAL_GL_UNSIGNED_NORMALIZED;
            break;
        case webgl::ComponentType::Float:
            ret = LOCAL_GL_FLOAT;
            break;
        default:
            MOZ_ASSERT(false, "No matched component type");
            break;
        }
        break;

    default:
        MOZ_ASSERT(false, "Missing case.");
        break;
    }

    return JS::Int32Value(ret);
}

////////////////////////////////////////////////////////////////////////////////
// WebGLFramebuffer

WebGLFramebuffer::WebGLFramebuffer(WebGLContext* webgl, GLuint fbo)
    : WebGLContextBoundObject(webgl)
    , mGLName(fbo)
    , mIsKnownFBComplete(false)
    , mReadBufferMode(LOCAL_GL_COLOR_ATTACHMENT0)
    , mColorAttachment0(this, LOCAL_GL_COLOR_ATTACHMENT0)
    , mDepthAttachment(this, LOCAL_GL_DEPTH_ATTACHMENT)
    , mStencilAttachment(this, LOCAL_GL_STENCIL_ATTACHMENT)
    , mDepthStencilAttachment(this, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
    , mMoreColorAttachments(webgl->mGLMaxColorAttachments)
    , mDrawBuffers(1, LOCAL_GL_COLOR_ATTACHMENT0)
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

    for (auto& cur : mMoreColorAttachments) {
        cur.Clear();
    }

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteFramebuffers(1, &mGLName);

    LinkedListElement<WebGLFramebuffer>::removeFrom(mContext->mFramebuffers);

#ifdef ANDROID
    mIsFB = false;
#endif
}

void
WebGLFramebuffer::FramebufferRenderbuffer(GLenum attachment, RBTarget rbtarget,
                                          WebGLRenderbuffer* rb)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferRenderbuffer: renderbuffer", rb))
        return;

    // `attachPointEnum` is validated by ValidateFramebufferAttachment().

    RefPtr<WebGLRenderbuffer> rb_ = rb; // Bug 1201275
    const auto fnAttach = [this, &rb_](GLenum attachment) {
        const auto attachPoint = this->GetAttachPoint(attachment);
        MOZ_ASSERT(attachPoint);

        attachPoint->SetRenderbuffer(rb_);
    };

    if (mContext->IsWebGL2() && attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        fnAttach(LOCAL_GL_DEPTH_ATTACHMENT);
        fnAttach(LOCAL_GL_STENCIL_ATTACHMENT);
    } else {
        fnAttach(attachment);
    }

    InvalidateFramebufferStatus();
}

void
WebGLFramebuffer::FramebufferTexture2D(GLenum attachment, TexImageTarget texImageTarget,
                                       WebGLTexture* tex, GLint level)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!mContext->ValidateObjectAllowNull("framebufferTexture2D: texture", tex))
        return;

    if (tex) {
        if (!tex->HasEverBeenBound()) {
            mContext->ErrorInvalidOperation("framebufferTexture2D: the texture"
                                            " is not the name of a texture.");
            return;
        }

        const TexTarget destTexTarget = TexImageTargetToTexTarget(texImageTarget);
        if (tex->Target() != destTexTarget) {
            mContext->ErrorInvalidOperation("framebufferTexture2D: Mismatched"
                                            " texture and texture target.");
            return;
        }
    }

    RefPtr<WebGLTexture> tex_ = tex; // Bug 1201275
    const auto fnAttach = [this, &tex_, texImageTarget, level](GLenum attachment) {
        const auto attachPoint = this->GetAttachPoint(attachment);
        MOZ_ASSERT(attachPoint);

        attachPoint->SetTexImage(tex_, texImageTarget, level);
    };

    if (mContext->IsWebGL2() && attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        fnAttach(LOCAL_GL_DEPTH_ATTACHMENT);
        fnAttach(LOCAL_GL_STENCIL_ATTACHMENT);
    } else {
        fnAttach(attachment);
    }

    InvalidateFramebufferStatus();
}

void
WebGLFramebuffer::FramebufferTextureLayer(GLenum attachment, WebGLTexture* tex,
                                          GLint level, GLint layer)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    const TexImageTarget texImageTarget = (tex ? tex->Target().get()
                                               : LOCAL_GL_TEXTURE_2D);

    RefPtr<WebGLTexture> tex_ = tex; // Bug 1201275
    const auto fnAttach = [this, &tex_, texImageTarget, level, layer](GLenum attachment) {
        const auto attachPoint = this->GetAttachPoint(attachment);
        MOZ_ASSERT(attachPoint);

        attachPoint->SetTexImageLayer(tex_, texImageTarget, level, layer);
    };

    if (mContext->IsWebGL2() && attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        fnAttach(LOCAL_GL_DEPTH_ATTACHMENT);
        fnAttach(LOCAL_GL_STENCIL_ATTACHMENT);
    } else {
        fnAttach(attachment);
    }

    InvalidateFramebufferStatus();
}

WebGLFBAttachPoint*
WebGLFramebuffer::GetAttachPoint(GLenum attachPoint)
{
    switch (attachPoint) {
    case LOCAL_GL_COLOR_ATTACHMENT0:
        return &mColorAttachment0;

    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        return &mDepthStencilAttachment;

    case LOCAL_GL_DEPTH_ATTACHMENT:
        return &mDepthAttachment;

    case LOCAL_GL_STENCIL_ATTACHMENT:
        return &mStencilAttachment;

    default:
        break;
    }

    const auto lastCAEnum = mContext->LastColorAttachmentEnum();
    if (attachPoint < LOCAL_GL_COLOR_ATTACHMENT1 ||
        attachPoint > lastCAEnum)
    {
        return nullptr;
    }

    if (!mMoreColorAttachments.Size()) {
        for (GLenum cur = LOCAL_GL_COLOR_ATTACHMENT1; cur <= lastCAEnum; cur++) {
            mMoreColorAttachments.AppendNew(this, cur);
        }
    }
    MOZ_ASSERT(LOCAL_GL_COLOR_ATTACHMENT0 + mMoreColorAttachments.Size() == lastCAEnum);

    const size_t offset = attachPoint - LOCAL_GL_COLOR_ATTACHMENT1;
    MOZ_ASSERT(offset <= mMoreColorAttachments.Size());
    return &mMoreColorAttachments[offset];
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

    for (auto& cur : mMoreColorAttachments) {
        if (cur.Texture() == tex)
            cur.Clear();
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

    for (auto& cur : mMoreColorAttachments) {
        if (cur.Renderbuffer() == rb)
            cur.Clear();
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

    for (const auto& cur : mMoreColorAttachments) {
        hasAttachments |= cur.IsDefined();
    }

    return hasAttachments;
}

bool
WebGLFramebuffer::HasIncompleteAttachments(nsCString* const out_info) const
{
    const auto fnIsIncomplete = [this, out_info](const WebGLFBAttachPoint& cur) {
        if (!cur.IsDefined())
            return false; // Not defined, so can't count as incomplete.

        return !cur.IsComplete(this->mContext, out_info);
    };

    bool hasIncomplete = false;

    hasIncomplete |= fnIsIncomplete(mColorAttachment0);
    hasIncomplete |= fnIsIncomplete(mDepthAttachment);
    hasIncomplete |= fnIsIncomplete(mStencilAttachment);
    hasIncomplete |= fnIsIncomplete(mDepthStencilAttachment);

    for (const auto& cur : mMoreColorAttachments) {
        hasIncomplete |= fnIsIncomplete(cur);
    }

    return hasIncomplete;
}

bool
WebGLFramebuffer::AllImageRectsMatch() const
{
    MOZ_ASSERT(HasDefinedAttachments());
    DebugOnly<nsCString> fbStatusInfo;
    MOZ_ASSERT(!HasIncompleteAttachments(&fbStatusInfo));

    bool needsInit = true;
    uint32_t width = 0;
    uint32_t height = 0;

    const auto fnInitializeOrMatch = [&needsInit, &width,
                                      &height](const WebGLFBAttachPoint& attach)
    {
        if (!attach.HasImage())
            return true;

        uint32_t curWidth;
        uint32_t curHeight;
        attach.Size(&curWidth, &curHeight);

        if (needsInit) {
            needsInit = false;
            width = curWidth;
            height = curHeight;
            return true;
        }

        return (curWidth == width &&
                curHeight == height);
    };

    bool matches = true;

    matches &= fnInitializeOrMatch(mColorAttachment0      );
    matches &= fnInitializeOrMatch(mDepthAttachment       );
    matches &= fnInitializeOrMatch(mStencilAttachment     );
    matches &= fnInitializeOrMatch(mDepthStencilAttachment);

    for (const auto& cur : mMoreColorAttachments) {
        matches &= fnInitializeOrMatch(cur);
    }

    return matches;
}

bool
WebGLFramebuffer::AllImageSamplesMatch() const
{
    MOZ_ASSERT(HasDefinedAttachments());
    DebugOnly<nsCString> fbStatusInfo;
    MOZ_ASSERT(!HasIncompleteAttachments(&fbStatusInfo));

    bool needsInit = true;
    uint32_t samples = 0;

    const auto fnInitializeOrMatch = [&needsInit,
                                      &samples](const WebGLFBAttachPoint& attach)
    {
        if (!attach.HasImage())
          return true;

        const uint32_t curSamples = attach.Samples();

        if (needsInit) {
            needsInit = false;
            samples = curSamples;
            return true;
        }

        return (curSamples == samples);
    };

    bool matches = true;

    matches &= fnInitializeOrMatch(mColorAttachment0      );
    matches &= fnInitializeOrMatch(mDepthAttachment       );
    matches &= fnInitializeOrMatch(mStencilAttachment     );
    matches &= fnInitializeOrMatch(mDepthStencilAttachment);

    for (const auto& cur : mMoreColorAttachments) {
        matches &= fnInitializeOrMatch(cur);
    }

    return matches;
}

FBStatus
WebGLFramebuffer::PrecheckFramebufferStatus(nsCString* const out_info) const
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    if (!HasDefinedAttachments())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT; // No attachments

    if (HasIncompleteAttachments(out_info))
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

    if (!AllImageRectsMatch())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS; // Inconsistent sizes

    if (!AllImageSamplesMatch())
        return LOCAL_GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE; // Inconsistent samples

    if (!mContext->IsWebGL2()) {
        // INCOMPLETE_DIMENSIONS doesn't exist in GLES3.
        const auto depthOrStencilCount = int(mDepthAttachment.IsDefined()) +
                                         int(mStencilAttachment.IsDefined()) +
                                         int(mDepthStencilAttachment.IsDefined());
        if (depthOrStencilCount > 1)
            return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;
    }

    return LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

FBStatus
WebGLFramebuffer::CheckFramebufferStatus(FBTarget target, nsCString* const out_info) const
{
    if (mIsKnownFBComplete)
        return LOCAL_GL_FRAMEBUFFER_COMPLETE;

    FBStatus ret = PrecheckFramebufferStatus(out_info);
    if (ret != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return ret;

    // Looks good on our end. Let's ask the driver.
    mContext->MakeContextCurrent();

    // Ok, attach our chosen flavor of {DEPTH, STENCIL, DEPTH_STENCIL}.
    FinalizeAttachments();

    ret = mContext->gl->fCheckFramebufferStatus(target);

    if (ret == LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        mIsKnownFBComplete = true;
    } else {
        out_info->AssignLiteral("Bad status according to the driver");
    }

    return ret;
}

bool
WebGLFramebuffer::ValidateAndInitAttachments(const char* funcName)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    nsCString fbStatusInfo;
    const auto fbStatus = CheckFramebufferStatus(&fbStatusInfo);
    if (fbStatus != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        nsCString errorText = nsPrintfCString("Incomplete framebuffer: Status 0x%04x",
                                              fbStatus.get());
        if (fbStatusInfo.Length()) {
            errorText += ": ";
            errorText += fbStatusInfo;
        }

        mContext->ErrorInvalidFramebufferOperation("%s: %s.", funcName,
                                                   errorText.BeginReading());
        return false;
    }

    // Cool! We've checked out ok. Just need to initialize.

    //////
    // Check if we need to initialize anything

    std::vector<WebGLFBAttachPoint*> tex3DToClear;

    const auto fnGatherIf3D = [&](WebGLFBAttachPoint& attach) {
        if (!attach.Texture())
            return false;

        const auto& info = attach.Texture()->ImageInfoAt(attach.ImageTarget(),
                                                         attach.MipLevel());
        if (info.mDepth == 1)
            return false;

        tex3DToClear.push_back(&attach);
        return true;
    };

    //////

    uint32_t clearBits = 0;
    std::vector<GLenum> drawBuffersForClear(1 + mMoreColorAttachments.Size(),
                                            LOCAL_GL_NONE);

    std::vector<WebGLFBAttachPoint*> attachmentsToClear;
    attachmentsToClear.reserve(1 + mMoreColorAttachments.Size() + 3);

    const auto fnGatherColor = [&](WebGLFBAttachPoint& attach, uint32_t colorAttachNum) {
        if (!IsDrawBuffer(colorAttachNum) || !attach.HasUninitializedImageData())
            return;

        if (fnGatherIf3D(attach))
            return;

        attachmentsToClear.push_back(&attach);

        clearBits |= LOCAL_GL_COLOR_BUFFER_BIT;
        drawBuffersForClear[colorAttachNum] = LOCAL_GL_COLOR_ATTACHMENT0 + colorAttachNum;
    };

    const auto fnGatherOther = [&](WebGLFBAttachPoint& attach, GLenum attachClearBits) {
        if (!attach.HasUninitializedImageData())
            return;

        if (fnGatherIf3D(attach))
            return;

        attachmentsToClear.push_back(&attach);

        clearBits |= attachClearBits;
    };

    //////

    fnGatherColor(mColorAttachment0, 0);

    size_t colorAttachNum = 1;
    for (auto& cur : mMoreColorAttachments) {
        fnGatherColor(cur, colorAttachNum);
        ++colorAttachNum;
    }

    fnGatherOther(mDepthAttachment, LOCAL_GL_DEPTH_BUFFER_BIT);
    fnGatherOther(mStencilAttachment, LOCAL_GL_STENCIL_BUFFER_BIT);
    fnGatherOther(mDepthStencilAttachment,
                  LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

    //////

    mContext->MakeContextCurrent();

    if (clearBits) {
        const auto fnDrawBuffers = [this](const std::vector<GLenum>& list) {
            this->mContext->gl->fDrawBuffers(list.size(), list.data());
        };

        const auto drawBufferExt = WebGLExtensionID::WEBGL_draw_buffers;
        const bool hasDrawBuffers = (mContext->IsWebGL2() ||
                                     mContext->IsExtensionEnabled(drawBufferExt));

        if (hasDrawBuffers) {
            fnDrawBuffers(drawBuffersForClear);
        }

        ////////////

        // Clear!
        {
            gl::ScopedBindFramebuffer autoBind(mContext->gl, mGLName);

            mContext->ForceClearFramebufferWithDefaultValues(clearBits, false);
        }

        if (hasDrawBuffers) {
            fnDrawBuffers(mDrawBuffers);
        }

        for (auto* cur : attachmentsToClear) {
            cur->SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
        }
    }

    //////

    for (auto* attach : tex3DToClear) {
        auto* tex = attach->Texture();
        if (!tex->InitializeImageData(funcName, attach->ImageTarget(),
                                      attach->MipLevel()))
        {
            return false;
        }
    }

    return true;
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
WebGLFramebuffer::FinalizeAttachments(FBTarget target) const
{
    MOZ_ASSERT_IF(target == LOCAL_GL_READ_FRAMEBUFFER,
                  mContext->mBoundReadFramebuffer == this);
    MOZ_ASSERT_IF(target != LOCAL_GL_READ_FRAMEBUFFER,
                  mContext->mBoundDrawFramebuffer == this);

    gl::GLContext* gl = mContext->gl;

    ////

    mColorAttachment0.FinalizeAttachment(gl, target, LOCAL_GL_COLOR_ATTACHMENT0);

    for (size_t i = 0; i < mMoreColorAttachments.Size(); i++) {
        GLenum attachPoint = LOCAL_GL_COLOR_ATTACHMENT1 + i;
        mMoreColorAttachments[i].FinalizeAttachment(gl, target, attachPoint);
    }

    ////

    // Nuke the depth and stencil attachment points.
    gl->fFramebufferRenderbuffer(target, LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);
    gl->fFramebufferRenderbuffer(target, LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);

    mDepthAttachment.FinalizeAttachment(gl, target, LOCAL_GL_DEPTH_ATTACHMENT);
    mStencilAttachment.FinalizeAttachment(gl, target, LOCAL_GL_STENCIL_ATTACHMENT);
    mDepthStencilAttachment.FinalizeAttachment(gl, target, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);

    ////

    FinalizeDrawAndReadBuffers(gl, mColorAttachment0.IsDefined());
}

bool
WebGLFramebuffer::ValidateForRead(const char* funcName,
                                  const webgl::FormatUsageInfo** const out_format,
                                  uint32_t* const out_width, uint32_t* const out_height)
{
    if (!ValidateAndInitAttachments(funcName))
        return false;

    if (mReadBufferMode == LOCAL_GL_NONE) {
        mContext->ErrorInvalidOperation("%s: Read buffer mode must not be NONE.",
                                        funcName);
        return false;
    }

    const auto attachPoint = GetAttachPoint(mReadBufferMode);
    if (!attachPoint || !attachPoint->IsDefined()) {
        mContext->ErrorInvalidOperation("%s: The attachment specified for reading is"
                                        " null.", funcName);
        return false;
    }

    *out_format = attachPoint->Format();
    attachPoint->Size(out_width, out_height);
    return true;
}

static bool
AttachmentsDontMatch(const WebGLFBAttachPoint& a, const WebGLFBAttachPoint& b)
{
    if (a.Texture()) {
        return (a.Texture() != b.Texture());
    }

    if (a.Renderbuffer()) {
        return (a.Renderbuffer() != b.Renderbuffer());
    }

    return false;
}

JS::Value
WebGLFramebuffer::GetAttachmentParameter(const char* funcName, JSContext* cx,
                                         GLenum target, GLenum attachment,
                                         GLenum pname, ErrorResult* const out_error)
{
    auto attachPoint = GetAttachPoint(attachment);
    if (!attachPoint) {
        mContext->ErrorInvalidEnum("%s: Can only query COLOR_ATTACHMENTi,"
                                   " DEPTH_ATTACHMENT, DEPTH_STENCIL_ATTACHMENT, or"
                                   " STENCIL_ATTACHMENT for a framebuffer.",
                                   funcName);
        return JS::NullValue();
    }

    if (mContext->IsWebGL2() && attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        // There are a couple special rules for this one.

        if (pname == LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) {
            mContext->ErrorInvalidOperation("%s: Querying"
                                            " FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"
                                            " against DEPTH_STENCIL_ATTACHMENT is an"
                                            " error.",
                                            funcName);
            return JS::NullValue();
        }

        if (AttachmentsDontMatch(DepthAttachment(), StencilAttachment())) {
            mContext->ErrorInvalidOperation("%s: DEPTH_ATTACHMENT and STENCIL_ATTACHMENT"
                                            " have different objects bound.",
                                            funcName);
            return JS::NullValue();
        }

        attachPoint = GetAttachPoint(LOCAL_GL_DEPTH_ATTACHMENT);
    }

    return attachPoint->GetParameter(funcName, mContext, cx, target, attachment, pname,
                                     out_error);
}


void
WebGLFramebuffer::GatherAttachments(std::vector<const WebGLFBAttachPoint*>* const out) const
{
    auto itr = mDrawBuffers.cbegin();
    if (itr != mDrawBuffers.cend() &&
        *itr != LOCAL_GL_NONE)
    {
        out->push_back(&mColorAttachment0);
        ++itr;
    }

    size_t i = 0;
    for (; itr != mDrawBuffers.cend(); ++itr) {
        if (i >= mMoreColorAttachments.Size())
            break;

        if (*itr != LOCAL_GL_NONE) {
            out->push_back(&mMoreColorAttachments[i]);
        }
        ++i;
    }

    out->push_back(&mDepthAttachment);
    out->push_back(&mStencilAttachment);
    out->push_back(&mDepthStencilAttachment);
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

template<typename C>
inline void
ImplCycleCollectionUnlink(C& field)
{
    for (auto& cur : field) {
        cur.Unlink();
    }
}

template<typename C>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            C& field,
                            const char* name,
                            uint32_t flags = 0)
{
    for (auto& cur : field) {
        ImplCycleCollectionTraverse(callback, cur, name, flags);
    }
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
