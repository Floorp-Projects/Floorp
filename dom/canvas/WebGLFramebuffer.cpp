/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLFramebuffer.h"

// You know it's going to be fun when these two show up:
#include <algorithm>
#include <iterator>

#include "GLContext.h"
#include "GLScreenBuffer.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "nsPrintfCString.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"

namespace mozilla {

WebGLFBAttachPoint::WebGLFBAttachPoint()
    : mFB(nullptr)
    , mAttachmentPoint(0)
    , mTexImageTarget(LOCAL_GL_NONE)
    , mTexImageLayer(0)
    , mTexImageLevel(0)
{ }

WebGLFBAttachPoint::WebGLFBAttachPoint(WebGLFramebuffer* fb, GLenum attachmentPoint)
    : mFB(fb)
    , mAttachmentPoint(attachmentPoint)
    , mTexImageTarget(LOCAL_GL_NONE)
    , mTexImageLayer(0)
    , mTexImageLevel(0)
{ }

WebGLFBAttachPoint::~WebGLFBAttachPoint()
{
    MOZ_ASSERT(mFB, "Should have been Init'd.");
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
WebGLFBAttachPoint::SetTexImage(WebGLTexture* tex, TexImageTarget target, GLint level,
                                GLint layer)
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
WebGLFBAttachPoint::SetImageDataStatus(WebGLImageDataStatus newStatus) const
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
WebGLFBAttachPoint::Resolve(gl::GLContext* gl) const
{
    if (!HasImage())
        return;

    if (Renderbuffer()) {
        Renderbuffer()->DoFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, mAttachmentPoint);
        return;
    }
    MOZ_ASSERT(Texture());

    MOZ_ASSERT(gl == Texture()->mContext->GL());

    const auto& texName = Texture()->mGLName;

    ////

    const auto fnAttach2D = [&](GLenum attachmentPoint) {
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, attachmentPoint,
                                  mTexImageTarget.get(), texName, mTexImageLevel);
    };

    ////

    switch (mTexImageTarget.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (mAttachmentPoint == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
            fnAttach2D(LOCAL_GL_DEPTH_ATTACHMENT);
            fnAttach2D(LOCAL_GL_STENCIL_ATTACHMENT);
        } else {
            fnAttach2D(mAttachmentPoint);
        }
        break;

    case LOCAL_GL_TEXTURE_2D_ARRAY:
    case LOCAL_GL_TEXTURE_3D:
        // If we have fFramebufferTextureLayer, we can rely on having
        // DEPTH_STENCIL_ATTACHMENT.
        gl->fFramebufferTextureLayer(LOCAL_GL_FRAMEBUFFER, mAttachmentPoint, texName,
                                     mTexImageLevel, mTexImageLayer);
        break;
    }
}

JS::Value
WebGLFBAttachPoint::GetParameter(const char* funcName, WebGLContext* webgl, JSContext* cx,
                                 GLenum target, GLenum attachment, GLenum pname,
                                 ErrorResult* const out_error) const
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
        nsCString attachmentName;
        WebGLContext::EnumName(attachment, &attachmentName);
        if (webgl->IsWebGL2()) {
            webgl->ErrorInvalidOperation("%s: No attachment at %s.", funcName,
                                         attachmentName.BeginReading());
        } else {
            webgl->ErrorInvalidEnum("%s: No attachment at %s.", funcName,
                                    attachmentName.BeginReading());
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
    if (!usage) {
        if (pname == LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)
            return JS::NumberValue(LOCAL_GL_LINEAR);

        return JS::NullValue();
    }

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
            MOZ_ASSERT(format->unsizedFormat == webgl::UnsizedFormat::DEPTH_STENCIL);
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
////////////////////////////////////////////////////////////////////////////////
// WebGLFramebuffer

WebGLFramebuffer::WebGLFramebuffer(WebGLContext* webgl, GLuint fbo)
    : WebGLRefCountedObject(webgl)
    , mGLName(fbo)
#ifdef ANDROID
    , mIsFB(false)
#endif
    , mDepthAttachment(this, LOCAL_GL_DEPTH_ATTACHMENT)
    , mStencilAttachment(this, LOCAL_GL_STENCIL_ATTACHMENT)
    , mDepthStencilAttachment(this, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
{
    mContext->mFramebuffers.insertBack(this);

    size_t i = 0;
    for (auto& cur : mColorAttachments) {
        new (&cur) WebGLFBAttachPoint(this, LOCAL_GL_COLOR_ATTACHMENT0 + i);
        i++;
    }

    mColorDrawBuffers.push_back(&mColorAttachments[0]);
    mColorReadBuffer = &mColorAttachments[0];
}

void
WebGLFramebuffer::Delete()
{
    InvalidateFramebufferStatus();

    mDepthAttachment.Clear();
    mStencilAttachment.Clear();
    mDepthStencilAttachment.Clear();

    for (auto& cur : mColorAttachments) {
        cur.Clear();
    }

    mContext->MakeContextCurrent();
    mContext->gl->fDeleteFramebuffers(1, &mGLName);

    LinkedListElement<WebGLFramebuffer>::removeFrom(mContext->mFramebuffers);

#ifdef ANDROID
    mIsFB = false;
#endif
}

////

Maybe<WebGLFBAttachPoint*>
WebGLFramebuffer::GetColorAttachPoint(GLenum attachPoint)
{
    if (attachPoint == LOCAL_GL_NONE)
        return Some<WebGLFBAttachPoint*>(nullptr);

    if (attachPoint < LOCAL_GL_COLOR_ATTACHMENT0)
        return Nothing();

    const size_t colorId = attachPoint - LOCAL_GL_COLOR_ATTACHMENT0;

    MOZ_ASSERT(mContext->mImplMaxColorAttachments <= kMaxColorAttachments);
    if (colorId >= mContext->mImplMaxColorAttachments)
        return Nothing();

    return Some(&mColorAttachments[colorId]);
}

Maybe<WebGLFBAttachPoint*>
WebGLFramebuffer::GetAttachPoint(GLenum attachPoint)
{
    switch (attachPoint) {
    case LOCAL_GL_DEPTH_STENCIL_ATTACHMENT:
        return Some(&mDepthStencilAttachment);

    case LOCAL_GL_DEPTH_ATTACHMENT:
        return Some(&mDepthAttachment);

    case LOCAL_GL_STENCIL_ATTACHMENT:
        return Some(&mStencilAttachment);

    default:
        return GetColorAttachPoint(attachPoint);
    }
}

#define FOR_EACH_ATTACHMENT(X)            \
    X(mDepthAttachment);                  \
    X(mStencilAttachment);                \
    X(mDepthStencilAttachment);           \
                                          \
    for (auto& cur : mColorAttachments) { \
        X(cur);                           \
    }

void
WebGLFramebuffer::DetachTexture(const WebGLTexture* tex)
{
    const auto fnDetach = [&](WebGLFBAttachPoint& attach) {
        if (attach.Texture() == tex) {
            attach.Clear();
        }
    };

    FOR_EACH_ATTACHMENT(fnDetach)
}

void
WebGLFramebuffer::DetachRenderbuffer(const WebGLRenderbuffer* rb)
{
    const auto fnDetach = [&](WebGLFBAttachPoint& attach) {
        if (attach.Renderbuffer() == rb) {
            attach.Clear();
        }
    };

    FOR_EACH_ATTACHMENT(fnDetach)
}

////////////////////////////////////////////////////////////////////////////////
// Completeness

bool
WebGLFramebuffer::HasDefinedAttachments() const
{
    bool hasAttachments = false;
    const auto func = [&](const WebGLFBAttachPoint& attach) {
        hasAttachments |= attach.IsDefined();
    };

    FOR_EACH_ATTACHMENT(func)
    return hasAttachments;
}

bool
WebGLFramebuffer::HasIncompleteAttachments(nsCString* const out_info) const
{
    bool hasIncomplete = false;
    const auto func = [&](const WebGLFBAttachPoint& cur) {
        if (!cur.IsDefined())
            return; // Not defined, so can't count as incomplete.

        hasIncomplete |= !cur.IsComplete(mContext, out_info);
    };

    FOR_EACH_ATTACHMENT(func)
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

    bool hasMismatch = false;
    const auto func = [&](const WebGLFBAttachPoint& attach) {
        if (!attach.HasImage())
            return;

        uint32_t curWidth;
        uint32_t curHeight;
        attach.Size(&curWidth, &curHeight);

        if (needsInit) {
            needsInit = false;
            width = curWidth;
            height = curHeight;
            return;
        }

        hasMismatch |= (curWidth != width ||
                        curHeight != height);
    };

    FOR_EACH_ATTACHMENT(func)
    return !hasMismatch;
}

bool
WebGLFramebuffer::AllImageSamplesMatch() const
{
    MOZ_ASSERT(HasDefinedAttachments());
    DebugOnly<nsCString> fbStatusInfo;
    MOZ_ASSERT(!HasIncompleteAttachments(&fbStatusInfo));

    bool needsInit = true;
    uint32_t samples = 0;

    bool hasMismatch = false;
    const auto func = [&](const WebGLFBAttachPoint& attach) {
        if (!attach.HasImage())
          return;

        const uint32_t curSamples = attach.Samples();

        if (needsInit) {
            needsInit = false;
            samples = curSamples;
            return;
        }

        hasMismatch |= (curSamples != samples);
    };

    FOR_EACH_ATTACHMENT(func)
    return !hasMismatch;
}

#undef FOR_EACH_ATTACHMENT

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

    if (mContext->IsWebGL2()) {
        MOZ_ASSERT(!mDepthStencilAttachment.IsDefined());
    } else {
        const auto depthOrStencilCount = int(mDepthAttachment.IsDefined()) +
                                         int(mStencilAttachment.IsDefined()) +
                                         int(mDepthStencilAttachment.IsDefined());
        if (depthOrStencilCount > 1)
            return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;
    }

    return LOCAL_GL_FRAMEBUFFER_COMPLETE;
}

////////////////////////////////////////
// Validation

bool
WebGLFramebuffer::ValidateAndInitAttachments(const char* funcName)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    const auto fbStatus = CheckFramebufferStatus(funcName);
    if (fbStatus == LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return true;

    mContext->ErrorInvalidFramebufferOperation("%s: Framebuffer must be"
                                               " complete.",
                                               funcName);
    return false;
}

bool
WebGLFramebuffer::ValidateClearBufferType(const char* funcName, GLenum buffer,
                                          uint32_t drawBuffer, GLenum funcType) const
{
    if (buffer != LOCAL_GL_COLOR)
        return true;

    const auto& attach = mColorAttachments[drawBuffer];
    if (!attach.IsDefined())
        return true;

    if (!count(mColorDrawBuffers.begin(), mColorDrawBuffers.end(), &attach))
        return true; // DRAW_BUFFERi set to NONE.

    GLenum attachType;
    switch (attach.Format()->format->componentType) {
    case webgl::ComponentType::Int:
        attachType = LOCAL_GL_INT;
        break;
    case webgl::ComponentType::UInt:
        attachType = LOCAL_GL_UNSIGNED_INT;
        break;
    default:
        attachType = LOCAL_GL_FLOAT;
        break;
    }

    if (attachType != funcType) {
        mContext->ErrorInvalidOperation("%s: This attachment is of type 0x%04x, but"
                                        " this function is of type 0x%04x.",
                                        funcName, attachType, funcType);
        return false;
    }

    return true;
}

bool
WebGLFramebuffer::ValidateForRead(const char* funcName,
                                  const webgl::FormatUsageInfo** const out_format,
                                  uint32_t* const out_width, uint32_t* const out_height)
{
    if (!ValidateAndInitAttachments(funcName))
        return false;

    if (!mColorReadBuffer) {
        mContext->ErrorInvalidOperation("%s: READ_BUFFER must not be NONE.", funcName);
        return false;
    }

    if (!mColorReadBuffer->IsDefined()) {
        mContext->ErrorInvalidOperation("%s: The READ_BUFFER attachment is not defined.",
                                        funcName);
        return false;
    }

    if (mColorReadBuffer->Samples()) {
        mContext->ErrorInvalidOperation("%s: The READ_BUFFER attachment is multisampled.",
                                        funcName);
        return false;
    }

    *out_format = mColorReadBuffer->Format();
    mColorReadBuffer->Size(out_width, out_height);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Resolution and caching

void
WebGLFramebuffer::ResolveAttachments() const
{
    const auto& gl = mContext->gl;

    ////
    // Nuke attachment points.

    for (uint32_t i = 0; i < mContext->mImplMaxColorAttachments; i++) {
        const GLenum attachEnum = LOCAL_GL_COLOR_ATTACHMENT0 + i;
        gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, attachEnum,
                                     LOCAL_GL_RENDERBUFFER, 0);
    }

    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);
    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                 LOCAL_GL_RENDERBUFFER, 0);

    ////

    for (const auto& attach : mColorAttachments) {
        attach.Resolve(gl);
    }

    mDepthAttachment.Resolve(gl);
    mStencilAttachment.Resolve(gl);
    mDepthStencilAttachment.Resolve(gl);
}

bool
WebGLFramebuffer::ResolveAttachmentData(const char* funcName) const
{
    //////
    // Check if we need to initialize anything

    const auto fnIs3D = [&](const WebGLFBAttachPoint& attach) {
        const auto& tex = attach.Texture();
        if (!tex)
            return false;

        const auto& info = tex->ImageInfoAt(attach.ImageTarget(), attach.MipLevel());
        if (info.mDepth == 1)
            return false;

        return true;
    };

    uint32_t clearBits = 0;
    std::vector<const WebGLFBAttachPoint*> attachmentsToClear;
    std::vector<const WebGLFBAttachPoint*> colorAttachmentsToClear;
    std::vector<const WebGLFBAttachPoint*> tex3DAttachmentsToInit;

    const auto fnGather = [&](const WebGLFBAttachPoint& attach, GLenum attachClearBits) {
        if (!attach.HasUninitializedImageData())
            return false;

        if (fnIs3D(attach)) {
            tex3DAttachmentsToInit.push_back(&attach);
            return false;
        }

        clearBits |= attachClearBits;
        attachmentsToClear.push_back(&attach);
        return true;
    };

    //////

    for (auto& cur : mColorDrawBuffers) {
        if (fnGather(*cur, LOCAL_GL_COLOR_BUFFER_BIT)) {
            colorAttachmentsToClear.push_back(cur);
        }
    }

    fnGather(mDepthAttachment, LOCAL_GL_DEPTH_BUFFER_BIT);
    fnGather(mStencilAttachment, LOCAL_GL_STENCIL_BUFFER_BIT);
    fnGather(mDepthStencilAttachment, LOCAL_GL_DEPTH_BUFFER_BIT |
                                      LOCAL_GL_STENCIL_BUFFER_BIT);

    //////

    for (const auto& attach : tex3DAttachmentsToInit) {
        const auto& tex = attach->Texture();
        if (!tex->InitializeImageData(funcName, attach->ImageTarget(),
                                      attach->MipLevel()))
        {
            return false;
        }
    }

    if (clearBits) {
        const auto fnDrawBuffers = [&](const std::vector<const WebGLFBAttachPoint*>& src)
        {
            std::vector<GLenum> enumList;

            for (const auto& cur : src) {
                const auto& attachEnum = cur->mAttachmentPoint;
                const GLenum attachId = attachEnum - LOCAL_GL_COLOR_ATTACHMENT0;

                while (enumList.size() < attachId) {
                    enumList.push_back(LOCAL_GL_NONE);
                }
                enumList.push_back(attachEnum);
            }

            mContext->gl->fDrawBuffers(enumList.size(), enumList.data());
        };

        ////
        // Clear

        mContext->MakeContextCurrent();

        const bool hasDrawBuffers = mContext->HasDrawBuffers();
        if (hasDrawBuffers) {
            fnDrawBuffers(colorAttachmentsToClear);
        }

        {
            gl::ScopedBindFramebuffer autoBind(mContext->gl, mGLName);

            mContext->ForceClearFramebufferWithDefaultValues(clearBits, false);
        }

        if (hasDrawBuffers) {
            RefreshDrawBuffers();
        }

        // Mark initialized.
        for (const auto& cur : attachmentsToClear) {
            cur->SetImageDataStatus(WebGLImageDataStatus::InitializedImageData);
        }
    }

    return true;
}

WebGLFramebuffer::ResolvedData::ResolvedData(const WebGLFramebuffer& parent)
{

    texDrawBuffers.reserve(parent.mColorDrawBuffers.size() + 2); // +2 for depth+stencil.

    const auto fnCommon = [&](const WebGLFBAttachPoint& attach) {
        if (!attach.IsDefined())
            return false;

        if (attach.Texture()) {
            texDrawBuffers.push_back(&attach);
        }
        return true;
    };

    ////

    const auto fnDepthStencil = [&](const WebGLFBAttachPoint& attach) {
        if (!fnCommon(attach))
            return;

        drawSet.insert(WebGLFBAttachPoint::Ordered(attach));
        readSet.insert(WebGLFBAttachPoint::Ordered(attach));
    };

    fnDepthStencil(parent.mDepthAttachment);
    fnDepthStencil(parent.mStencilAttachment);
    fnDepthStencil(parent.mDepthStencilAttachment);

    ////

    for (const auto& pAttach : parent.mColorDrawBuffers) {
        const auto& attach = *pAttach;
        if (!fnCommon(attach))
            return;

        drawSet.insert(WebGLFBAttachPoint::Ordered(attach));
    }

    if (parent.mColorReadBuffer) {
        const auto& attach = *parent.mColorReadBuffer;
        if (!fnCommon(attach))
            return;

        readSet.insert(WebGLFBAttachPoint::Ordered(attach));
    }
}

void
WebGLFramebuffer::RefreshResolvedData()
{
    if (mResolvedCompleteData) {
        mResolvedCompleteData.reset(new ResolvedData(*this));
    }
}

////////////////////////////////////////////////////////////////////////////////
// Entrypoints

FBStatus
WebGLFramebuffer::CheckFramebufferStatus(const char* funcName)
{
    if (IsResolvedComplete())
        return LOCAL_GL_FRAMEBUFFER_COMPLETE;

    // Ok, let's try to resolve it!

    nsCString statusInfo;
    FBStatus ret = PrecheckFramebufferStatus(&statusInfo);
    do {
        if (ret != LOCAL_GL_FRAMEBUFFER_COMPLETE)
            break;

        // Looks good on our end. Let's ask the driver.
        gl::GLContext* const gl = mContext->gl;
        gl->MakeCurrent();

        const ScopedFBRebinder autoFB(mContext);
        gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mGLName);

        ////

        ResolveAttachments(); // OK, attach everything properly!
        RefreshDrawBuffers();
        RefreshReadBuffer();

        ret = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);

        ////

        if (ret != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
            const nsPrintfCString text("Bad status according to the driver: 0x%04x",
                                       ret.get());
            statusInfo = text;
            break;
        }

        if (!ResolveAttachmentData(funcName)) {
            ret = LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;
            statusInfo.AssignLiteral("Failed to lazily-initialize attachment data.");
            break;
        }

        mResolvedCompleteData.reset(new ResolvedData(*this));
        return LOCAL_GL_FRAMEBUFFER_COMPLETE;
    } while (false);

    MOZ_ASSERT(ret != LOCAL_GL_FRAMEBUFFER_COMPLETE);
    mContext->GenerateWarning("%s: Framebuffer not complete. (status: 0x%04x) %s",
                              funcName, ret.get(), statusInfo.BeginReading());
    return ret;
}

////

void
WebGLFramebuffer::RefreshDrawBuffers() const
{
    const auto& gl = mContext->gl;
    if (!gl->IsSupported(gl::GLFeature::draw_buffers))
        return;

    // Prior to GL4.1, having a no-image FB attachment that's selected by DrawBuffers
    // yields a framebuffer status of FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER.
    // We could workaround this only on affected versions, but it's easier be
    // unconditional.
    std::vector<GLenum> driverBuffers(mContext->mImplMaxDrawBuffers, LOCAL_GL_NONE);
    for (const auto& attach : mColorDrawBuffers) {
        if (attach->HasImage()) {
            const uint32_t index = attach->mAttachmentPoint - LOCAL_GL_COLOR_ATTACHMENT0;
            driverBuffers[index] = attach->mAttachmentPoint;
        }
    }

    gl->fDrawBuffers(driverBuffers.size(), driverBuffers.data());
}

void
WebGLFramebuffer::RefreshReadBuffer() const
{
    const auto& gl = mContext->gl;
    if (!gl->IsSupported(gl::GLFeature::read_buffer))
        return;

    // Prior to GL4.1, having a no-image FB attachment that's selected by ReadBuffer
    // yields a framebuffer status of FRAMEBUFFER_INCOMPLETE_READ_BUFFER.
    // We could workaround this only on affected versions, but it's easier be
    // unconditional.
    GLenum driverBuffer = LOCAL_GL_NONE;
    if (mColorReadBuffer && mColorReadBuffer->HasImage()) {
        driverBuffer = mColorReadBuffer->mAttachmentPoint;
    }

    gl->fReadBuffer(driverBuffer);
}

////

void
WebGLFramebuffer::DrawBuffers(const char* funcName, const dom::Sequence<GLenum>& buffers)
{
    if (buffers.Length() > mContext->mImplMaxDrawBuffers) {
        // "An INVALID_VALUE error is generated if `n` is greater than MAX_DRAW_BUFFERS."
        mContext->ErrorInvalidValue("%s: `buffers` must have a length <="
                                    " MAX_DRAW_BUFFERS.", funcName);
        return;
    }

    std::vector<const WebGLFBAttachPoint*> newColorDrawBuffers;
    newColorDrawBuffers.reserve(buffers.Length());

    for (size_t i = 0; i < buffers.Length(); i++) {
        // "If the GL is bound to a draw framebuffer object, the `i`th buffer listed in
        //  bufs must be COLOR_ATTACHMENTi or NONE. Specifying a buffer out of order,
        //  BACK, or COLOR_ATTACHMENTm where `m` is greater than or equal to the value of
        // MAX_COLOR_ATTACHMENTS, will generate the error INVALID_OPERATION.

        // WEBGL_draw_buffers:
        // "The value of the MAX_COLOR_ATTACHMENTS_WEBGL parameter must be greater than or
        //  equal to that of the MAX_DRAW_BUFFERS_WEBGL parameter."
        // This means that if buffers.Length() isn't larger than MaxDrawBuffers, it won't
        // be larger than MaxColorAttachments.
        const auto& cur = buffers[i];
        if (cur == LOCAL_GL_COLOR_ATTACHMENT0 + i) {
            const auto& attach = mColorAttachments[i];
            newColorDrawBuffers.push_back(&attach);
        } else if (cur != LOCAL_GL_NONE) {
            const bool isColorEnum = (cur >= LOCAL_GL_COLOR_ATTACHMENT0 &&
                                      cur < mContext->LastColorAttachmentEnum());
            if (cur != LOCAL_GL_BACK &&
                !isColorEnum)
            {
                mContext->ErrorInvalidEnum("%s: Unexpected enum in buffers.", funcName);
                return;
            }

            mContext->ErrorInvalidOperation("%s: `buffers[i]` must be NONE or"
                                            " COLOR_ATTACHMENTi.",
                                            funcName);
            return;
        }
    }

    ////

    mContext->MakeContextCurrent();

    mColorDrawBuffers.swap(newColorDrawBuffers);
    RefreshDrawBuffers(); // Calls glDrawBuffers.
    RefreshResolvedData();
}

void
WebGLFramebuffer::ReadBuffer(const char* funcName, GLenum attachPoint)
{
    const auto& maybeAttach = GetColorAttachPoint(attachPoint);
    if (!maybeAttach) {
        const char text[] = "%s: `mode` must be a COLOR_ATTACHMENTi, for 0 <= i <"
                            " MAX_DRAW_BUFFERS.";
        if (attachPoint == LOCAL_GL_BACK) {
            mContext->ErrorInvalidOperation(text, funcName);
        } else {
            mContext->ErrorInvalidEnum(text, funcName);
        }
        return;
    }
    const auto& attach = maybeAttach.value(); // Might be nullptr.

    ////

    mContext->MakeContextCurrent();

    mColorReadBuffer = attach;
    RefreshReadBuffer(); // Calls glReadBuffer.
    RefreshResolvedData();
}

////

void
WebGLFramebuffer::FramebufferRenderbuffer(const char* funcName, GLenum attachEnum,
                                          GLenum rbtarget, WebGLRenderbuffer* rb)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    // `attachment`
    const auto maybeAttach = GetAttachPoint(attachEnum);
    if (!maybeAttach || !maybeAttach.value()) {
        mContext->ErrorInvalidEnum("%s: Bad `attachment`: 0x%x.", funcName, attachEnum);
        return;
    }
    const auto& attach = maybeAttach.value();

    // `rbTarget`
    if (rbtarget != LOCAL_GL_RENDERBUFFER) {
        mContext->ErrorInvalidEnumInfo("framebufferRenderbuffer: rbtarget:", rbtarget);
        return;
    }

    // `rb`
    if (rb && !mContext->ValidateObject("framebufferRenderbuffer: rb", *rb))
        return;

    // End of validation.

    if (mContext->IsWebGL2() && attachEnum == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        mDepthAttachment.SetRenderbuffer(rb);
        mStencilAttachment.SetRenderbuffer(rb);
    } else {
        attach->SetRenderbuffer(rb);
    }

    InvalidateFramebufferStatus();
}

void
WebGLFramebuffer::FramebufferTexture2D(const char* funcName, GLenum attachEnum,
                                       GLenum texImageTarget, WebGLTexture* tex,
                                       GLint level)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    // `attachment`
    const auto maybeAttach = GetAttachPoint(attachEnum);
    if (!maybeAttach || !maybeAttach.value()) {
        mContext->ErrorInvalidEnum("%s: Bad `attachment`: 0x%x.", funcName, attachEnum);
        return;
    }
    const auto& attach = maybeAttach.value();

    // `texImageTarget`
    if (texImageTarget != LOCAL_GL_TEXTURE_2D &&
        (texImageTarget < LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
         texImageTarget > LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
    {
        mContext->ErrorInvalidEnumInfo("framebufferTexture2D: texImageTarget:",
                                       texImageTarget);
        return;
    }

    // `texture`
    if (tex) {
        if (!mContext->ValidateObject("framebufferTexture2D: texture", *tex))
            return;

        if (!tex->HasEverBeenBound()) {
            mContext->ErrorInvalidOperation("%s: `texture` has never been bound.",
                                            funcName);
            return;
        }

        const TexTarget destTexTarget = TexImageTargetToTexTarget(texImageTarget);
        if (tex->Target() != destTexTarget) {
            mContext->ErrorInvalidOperation("%s: Mismatched texture and texture target.",
                                            funcName);
            return;
        }
    }

    // `level`
    if (level < 0)
        return mContext->ErrorInvalidValue("%s: `level` must not be negative.", funcName);

    if (mContext->IsWebGL2()) {
        /* GLES 3.0.4 p208:
         *   If textarget is one of TEXTURE_CUBE_MAP_POSITIVE_X,
         *   TEXTURE_CUBE_MAP_POSITIVE_Y, TEXTURE_CUBE_MAP_POSITIVE_Z,
         *   TEXTURE_CUBE_MAP_NEGATIVE_X, TEXTURE_CUBE_MAP_NEGATIVE_Y,
         *   or TEXTURE_CUBE_MAP_NEGATIVE_Z, then level must be greater
         *   than or equal to zero and less than or equal to log2 of the
         *   value of MAX_CUBE_MAP_TEXTURE_SIZE. If textarget is TEXTURE_2D,
         *   level must be greater than or equal to zero and no larger than
         *   log2 of the value of MAX_TEXTURE_SIZE. Otherwise, an
         *   INVALID_VALUE error is generated.
         */

        if (texImageTarget == LOCAL_GL_TEXTURE_2D) {
            if (uint32_t(level) > FloorLog2(mContext->mImplMaxTextureSize))
                return mContext->ErrorInvalidValue("%s: `level` is too large.", funcName);
        } else {
            MOZ_ASSERT(texImageTarget >= LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
                       texImageTarget <= LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

            if (uint32_t(level) > FloorLog2(mContext->mImplMaxCubeMapTextureSize))
                return mContext->ErrorInvalidValue("%s: `level` is too large.", funcName);
        }
    } else if (level != 0) {
        return mContext->ErrorInvalidValue("%s: `level` must be 0.", funcName);
    }

    // End of validation.

    if (mContext->IsWebGL2() && attachEnum == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        mDepthAttachment.SetTexImage(tex, texImageTarget, level);
        mStencilAttachment.SetTexImage(tex, texImageTarget, level);
    } else {
        attach->SetTexImage(tex, texImageTarget, level);
    }

    InvalidateFramebufferStatus();
}

void
WebGLFramebuffer::FramebufferTextureLayer(const char* funcName, GLenum attachEnum,
                                          WebGLTexture* tex, GLint level, GLint layer)
{
    MOZ_ASSERT(mContext->mBoundDrawFramebuffer == this ||
               mContext->mBoundReadFramebuffer == this);

    // `attachment`
    const auto maybeAttach = GetAttachPoint(attachEnum);
    if (!maybeAttach || !maybeAttach.value()) {
        mContext->ErrorInvalidEnum("%s: Bad `attachment`: 0x%x.", funcName, attachEnum);
        return;
    }
    const auto& attach = maybeAttach.value();

    // `level`, `layer`
    if (layer < 0)
        return mContext->ErrorInvalidValue("%s: `layer` must be >= 0.", funcName);

    if (level < 0)
        return mContext->ErrorInvalidValue("%s: `level` must be >= 0.", funcName);

    // `texture`
    TexImageTarget texImageTarget = LOCAL_GL_TEXTURE_3D;
    if (tex) {
        if (!mContext->ValidateObject("framebufferTextureLayer: texture", *tex))
            return;

        if (!tex->HasEverBeenBound()) {
            mContext->ErrorInvalidOperation("%s: `texture` has never been bound.",
                                            funcName);
            return;
        }

        texImageTarget = tex->Target().get();
        switch (texImageTarget.get()) {
        case LOCAL_GL_TEXTURE_3D:
            if (uint32_t(layer) >= mContext->mImplMax3DTextureSize) {
                mContext->ErrorInvalidValue("%s: `layer` must be < %s.", funcName,
                                            "MAX_3D_TEXTURE_SIZE");
                return;
            }

            if (uint32_t(level) > FloorLog2(mContext->mImplMax3DTextureSize)) {
                mContext->ErrorInvalidValue("%s: `level` must be <= log2(%s).", funcName,
                                            "MAX_3D_TEXTURE_SIZE");
                return;
            }
            break;

        case LOCAL_GL_TEXTURE_2D_ARRAY:
            if (uint32_t(layer) >= mContext->mImplMaxArrayTextureLayers) {
                mContext->ErrorInvalidValue("%s: `layer` must be < %s.", funcName,
                                            "MAX_ARRAY_TEXTURE_LAYERS");
                return;
            }

            if (uint32_t(level) > FloorLog2(mContext->mImplMaxTextureSize)) {
                mContext->ErrorInvalidValue("%s: `level` must be <= log2(%s).", funcName,
                                            "MAX_TEXTURE_SIZE");
                return;
            }
            break;

        default:
            mContext->ErrorInvalidOperation("%s: `texture` must be a TEXTURE_3D or"
                                            " TEXTURE_2D_ARRAY.",
                                            funcName);
            return;
        }
    }

    // End of validation.

    if (mContext->IsWebGL2() && attachEnum == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        mDepthAttachment.SetTexImage(tex, texImageTarget, level, layer);
        mStencilAttachment.SetTexImage(tex, texImageTarget, level, layer);
    } else {
        attach->SetTexImage(tex, texImageTarget, level, layer);
    }

    InvalidateFramebufferStatus();
}

JS::Value
WebGLFramebuffer::GetAttachmentParameter(const char* funcName, JSContext* cx,
                                         GLenum target, GLenum attachEnum, GLenum pname,
                                         ErrorResult* const out_error)
{
    const auto maybeAttach = GetAttachPoint(attachEnum);
    if (!maybeAttach || attachEnum == LOCAL_GL_NONE) {
        mContext->ErrorInvalidEnum("%s: Can only query COLOR_ATTACHMENTi,"
                                   " DEPTH_ATTACHMENT, DEPTH_STENCIL_ATTACHMENT, or"
                                   " STENCIL_ATTACHMENT for a framebuffer.",
                                   funcName);
        return JS::NullValue();
    }
    auto attach = maybeAttach.value();

    if (mContext->IsWebGL2() && attachEnum == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
        // There are a couple special rules for this one.

        if (pname == LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE) {
            mContext->ErrorInvalidOperation("%s: Querying"
                                            " FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE"
                                            " against DEPTH_STENCIL_ATTACHMENT is an"
                                            " error.",
                                            funcName);
            return JS::NullValue();
        }

        if (mDepthAttachment.Renderbuffer() != mStencilAttachment.Renderbuffer() ||
            mDepthAttachment.Texture() != mStencilAttachment.Texture())
        {
            mContext->ErrorInvalidOperation("%s: DEPTH_ATTACHMENT and STENCIL_ATTACHMENT"
                                            " have different objects bound.",
                                            funcName);
            return JS::NullValue();
        }

        attach = &mDepthAttachment;
    }

    return attach->GetParameter(funcName, mContext, cx, target, attachEnum, pname,
                                out_error);
}

////////////////////

static void
GetBackbufferFormats(const WebGLContext* webgl,
                     const webgl::FormatInfo** const out_color,
                     const webgl::FormatInfo** const out_depth,
                     const webgl::FormatInfo** const out_stencil)
{
    const auto& options = webgl->Options();

    const auto effFormat = (options.alpha ? webgl::EffectiveFormat::RGBA8
                                          : webgl::EffectiveFormat::RGB8);
    *out_color = webgl::GetFormat(effFormat);

    *out_depth = nullptr;
    *out_stencil = nullptr;
    if (options.depth && options.stencil) {
        *out_depth = webgl::GetFormat(webgl::EffectiveFormat::DEPTH24_STENCIL8);
        *out_stencil = *out_depth;
    } else {
        if (options.depth) {
            *out_depth = webgl::GetFormat(webgl::EffectiveFormat::DEPTH_COMPONENT16);
        }
        if (options.stencil) {
            *out_stencil = webgl::GetFormat(webgl::EffectiveFormat::STENCIL_INDEX8);
        }
    }
}

/*static*/ void
WebGLFramebuffer::BlitFramebuffer(WebGLContext* webgl,
                                  const WebGLFramebuffer* srcFB, GLint srcX0, GLint srcY0,
                                  GLint srcX1, GLint srcY1,
                                  const WebGLFramebuffer* dstFB, GLint dstX0, GLint dstY0,
                                  GLint dstX1, GLint dstY1,
                                  GLbitfield mask, GLenum filter)
{
    const char funcName[] = "blitFramebuffer";
    const auto& gl = webgl->gl;

    ////
    // Collect data

    const auto fnGetDepthAndStencilAttach = [](const WebGLFramebuffer* fb,
                                               const WebGLFBAttachPoint** const out_depth,
                                               const WebGLFBAttachPoint** const out_stencil)
    {
        *out_depth = nullptr;
        *out_stencil = nullptr;

        if (!fb)
            return;

        if (fb->mDepthStencilAttachment.IsDefined()) {
            *out_depth = *out_stencil = &fb->mDepthStencilAttachment;
            return;
        }
        if (fb->mDepthAttachment.IsDefined()) {
            *out_depth = &fb->mDepthAttachment;
        }
        if (fb->mStencilAttachment.IsDefined()) {
            *out_stencil = &fb->mStencilAttachment;
        }
    };

    const WebGLFBAttachPoint* srcDepthAttach;
    const WebGLFBAttachPoint* srcStencilAttach;
    fnGetDepthAndStencilAttach(srcFB, &srcDepthAttach, &srcStencilAttach);
    const WebGLFBAttachPoint* dstDepthAttach;
    const WebGLFBAttachPoint* dstStencilAttach;
    fnGetDepthAndStencilAttach(dstFB, &dstDepthAttach, &dstStencilAttach);

    ////

    const auto fnGetFormat = [](const WebGLFBAttachPoint* cur,
                                bool* const out_hasSamples) -> const webgl::FormatInfo*
    {
        if (!cur || !cur->IsDefined())
            return nullptr;

        *out_hasSamples |= bool(cur->Samples());
        return cur->Format()->format;
    };

    const auto fnNarrowComponentType = [&](const webgl::FormatInfo* format) {
        switch (format->componentType) {
        case webgl::ComponentType::NormInt:
        case webgl::ComponentType::NormUInt:
            return webgl::ComponentType::Float;

        default:
            return format->componentType;
        }
    };

    bool srcHasSamples;
    const webgl::FormatInfo* srcColorFormat;
    webgl::ComponentType srcColorType = webgl::ComponentType::None;
    const webgl::FormatInfo* srcDepthFormat;
    const webgl::FormatInfo* srcStencilFormat;

    if (srcFB) {
        srcHasSamples = false;
        srcColorFormat = fnGetFormat(srcFB->mColorReadBuffer, &srcHasSamples);
        srcDepthFormat = fnGetFormat(srcDepthAttach, &srcHasSamples);
        srcStencilFormat = fnGetFormat(srcStencilAttach, &srcHasSamples);
    } else {
        srcHasSamples = false; // Always false.

        GetBackbufferFormats(webgl, &srcColorFormat, &srcDepthFormat, &srcStencilFormat);
    }

    if (srcColorFormat) {
        srcColorType = fnNarrowComponentType(srcColorFormat);
    }

    ////

    bool dstHasSamples;
    const webgl::FormatInfo* dstDepthFormat;
    const webgl::FormatInfo* dstStencilFormat;
    bool dstHasColor = false;
    bool colorFormatsMatch = true;
    bool colorTypesMatch = true;

    const auto fnCheckColorFormat = [&](const webgl::FormatInfo* dstFormat) {
        MOZ_ASSERT(dstFormat->r || dstFormat->g || dstFormat->b || dstFormat->a);
        dstHasColor = true;
        colorFormatsMatch &= (dstFormat == srcColorFormat);
        colorTypesMatch &= ( fnNarrowComponentType(dstFormat) == srcColorType );
    };

    if (dstFB) {
        dstHasSamples = false;

        for (const auto& cur : dstFB->mColorDrawBuffers) {
            const auto& format = fnGetFormat(cur, &dstHasSamples);
            if (!format)
                continue;

            fnCheckColorFormat(format);
        }

        dstDepthFormat = fnGetFormat(dstDepthAttach, &dstHasSamples);
        dstStencilFormat = fnGetFormat(dstStencilAttach, &dstHasSamples);
    } else {
        dstHasSamples = bool(gl->Screen()->Samples());

        const webgl::FormatInfo* dstColorFormat;
        GetBackbufferFormats(webgl, &dstColorFormat, &dstDepthFormat, &dstStencilFormat);

        fnCheckColorFormat(dstColorFormat);
    }

    ////
    // Clear unused buffer bits

    if (mask & LOCAL_GL_COLOR_BUFFER_BIT &&
        !srcColorFormat && !dstHasColor)
    {
        mask ^= LOCAL_GL_COLOR_BUFFER_BIT;
    }

    if (mask & LOCAL_GL_DEPTH_BUFFER_BIT &&
        !srcDepthFormat && !dstDepthFormat)
    {
        mask ^= LOCAL_GL_DEPTH_BUFFER_BIT;
    }

    if (mask & LOCAL_GL_STENCIL_BUFFER_BIT &&
        !srcStencilFormat && !dstStencilFormat)
    {
        mask ^= LOCAL_GL_STENCIL_BUFFER_BIT;
    }

    ////
    // Validation

    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        if (srcColorFormat && filter == LOCAL_GL_LINEAR) {
            const auto& type = srcColorFormat->componentType;
            if (type == webgl::ComponentType::Int ||
                type == webgl::ComponentType::UInt)
            {
                webgl->ErrorInvalidOperation("%s: `filter` is LINEAR and READ_BUFFER"
                                             " contains integer data.",
                                             funcName);
                return;
            }
        }

        if (!colorTypesMatch) {
            webgl->ErrorInvalidOperation("%s: Color component types (fixed/float/uint/"
                                         "int) must match.",
                                         funcName);
            return;
        }
    }

    const GLbitfield depthAndStencilBits = LOCAL_GL_DEPTH_BUFFER_BIT |
                                           LOCAL_GL_STENCIL_BUFFER_BIT;
    if (bool(mask & depthAndStencilBits) &&
        filter != LOCAL_GL_NEAREST)
    {
        webgl->ErrorInvalidOperation("%s: DEPTH_BUFFER_BIT and STENCIL_BUFFER_BIT can"
                                     " only be used with NEAREST filtering.",
                                     funcName);
        return;
    }

    /* GLES 3.0.4, p199:
     *   Calling BlitFramebuffer will result in an INVALID_OPERATION error if
     *   mask includes DEPTH_BUFFER_BIT or STENCIL_BUFFER_BIT, and the source
     *   and destination depth and stencil buffer formats do not match.
     *
     * jgilbert: The wording is such that if only DEPTH_BUFFER_BIT is specified,
     * the stencil formats must match. This seems wrong. It could be a spec bug,
     * or I could be missing an interaction in one of the earlier paragraphs.
     */
    if (mask & LOCAL_GL_DEPTH_BUFFER_BIT &&
        dstDepthFormat && dstDepthFormat != srcDepthFormat)
    {
        webgl->ErrorInvalidOperation("%s: Depth buffer formats must match if selected.",
                                     funcName);
        return;
    }

    if (mask & LOCAL_GL_STENCIL_BUFFER_BIT &&
        dstStencilFormat && dstStencilFormat != srcStencilFormat)
    {
        webgl->ErrorInvalidOperation("%s: Stencil buffer formats must match if selected.",
                                     funcName);
        return;
    }

    ////

    if (dstHasSamples) {
        webgl->ErrorInvalidOperation("%s: DRAW_FRAMEBUFFER may not have multiple"
                                     " samples.",
                                     funcName);
        return;
    }

    if (srcHasSamples) {
        if (mask & LOCAL_GL_COLOR_BUFFER_BIT &&
            dstHasColor && !colorFormatsMatch)
        {
            webgl->ErrorInvalidOperation("%s: Color buffer formats must match if"
                                         " selected, when reading from a multisampled"
                                         " source.",
                                         funcName);
            return;
        }

        if (dstX0 != srcX0 ||
            dstX1 != srcX1 ||
            dstY0 != srcY0 ||
            dstY1 != srcY1)
        {
            webgl->ErrorInvalidOperation("%s: If the source is multisampled, then the"
                                         " source and dest regions must match exactly.",
                                         funcName);
            return;
        }
    }

    ////
    // Check for feedback

    if (srcFB && dstFB) {
        const WebGLFBAttachPoint* feedback = nullptr;

        if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
            MOZ_ASSERT(srcFB->mColorReadBuffer->IsDefined());
            for (const auto& cur : dstFB->mColorDrawBuffers) {
                if (srcFB->mColorReadBuffer->IsEquivalentForFeedback(*cur)) {
                    feedback = cur;
                    break;
                }
            }
        }

        if (mask & LOCAL_GL_DEPTH_BUFFER_BIT &&
            srcDepthAttach->IsEquivalentForFeedback(*dstDepthAttach))
        {
            feedback = dstDepthAttach;
        }

        if (mask & LOCAL_GL_STENCIL_BUFFER_BIT &&
            srcStencilAttach->IsEquivalentForFeedback(*dstStencilAttach))
        {
            feedback = dstStencilAttach;
        }

        if (feedback) {
            webgl->ErrorInvalidOperation("%s: Feedback detected into DRAW_FRAMEBUFFER's"
                                         " 0x%04x attachment.",
                                         funcName, feedback->mAttachmentPoint);
            return;
        }
    } else if (!srcFB && !dstFB) {
        webgl->ErrorInvalidOperation("%s: Feedback with default framebuffer.", funcName);
        return;
    }

    ////

    gl->MakeCurrent();
    webgl->OnBeforeReadCall();
    WebGLContext::ScopedDrawCallWrapper wrapper(*webgl);
    gl->fBlitFramebuffer(srcX0, srcY0, srcX1, srcY1,
                         dstX0, dstY0, dstX1, dstY1,
                         mask, filter);
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
                                      mDepthAttachment,
                                      mStencilAttachment,
                                      mDepthStencilAttachment,
                                      mColorAttachments)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLFramebuffer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLFramebuffer, Release)

} // namespace mozilla
