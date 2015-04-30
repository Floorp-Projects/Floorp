/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLTexture.h"
#include "WebGLExtensions.h"
#include "WebGLVertexArray.h"

#include "nsString.h"
#include "nsDebug.h"
#include "nsReadableUtils.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "GLContext.h"

#include "nsContentUtils.h"
#include "nsError.h"
#include "nsLayoutUtils.h"

#include "CanvasUtils.h"
#include "gfxUtils.h"

#include "jsfriendapi.h"

#include "WebGLTexelConversions.h"
#include "WebGLValidateStrings.h"
#include <algorithm>

// needed to check if current OS is lower than 10.7
#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Endian.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gl;

static const WebGLRectangleObject*
CurValidFBRectObject(const WebGLContext* webgl,
                     const WebGLFramebuffer* boundFB)
{
    const WebGLRectangleObject* rect = nullptr;

    if (boundFB) {
        // We don't really need to ask the driver.
        // Use 'precheck' to just check that our internal state looks good.
        FBStatus precheckStatus = boundFB->PrecheckFramebufferStatus();
        if (precheckStatus == LOCAL_GL_FRAMEBUFFER_COMPLETE)
            rect = &boundFB->RectangleObject();
    } else {
        rect = static_cast<const WebGLRectangleObject*>(webgl);
    }

    return rect;
}

const WebGLRectangleObject*
WebGLContext::CurValidDrawFBRectObject() const
{
    return CurValidFBRectObject(this, mBoundDrawFramebuffer);
}

const WebGLRectangleObject*
WebGLContext::CurValidReadFBRectObject() const
{
    return CurValidFBRectObject(this, mBoundReadFramebuffer);
}

//
//  WebGL API
//

void
WebGLContext::ActiveTexture(GLenum texture)
{
    if (IsContextLost())
        return;

    if (texture < LOCAL_GL_TEXTURE0 ||
        texture >= LOCAL_GL_TEXTURE0 + uint32_t(mGLMaxTextureUnits))
    {
        return ErrorInvalidEnum(
            "ActiveTexture: texture unit %d out of range. "
            "Accepted values range from TEXTURE0 to TEXTURE0 + %d. "
            "Notice that TEXTURE0 != 0.",
            texture, mGLMaxTextureUnits);
    }

    MakeContextCurrent();
    mActiveTexture = texture - LOCAL_GL_TEXTURE0;
    gl->fActiveTexture(texture);
}

void
WebGLContext::AttachShader(WebGLProgram* program, WebGLShader* shader)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("attachShader: program", program) ||
        !ValidateObject("attachShader: shader", shader))
    {
        return;
    }

    program->AttachShader(shader);
}

void
WebGLContext::BindAttribLocation(WebGLProgram* prog, GLuint location,
                                 const nsAString& name)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("bindAttribLocation: program", prog))
        return;

    prog->BindAttribLocation(location, name);
}

void
WebGLContext::BindFramebuffer(GLenum target, WebGLFramebuffer* wfb)
{
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, "bindFramebuffer"))
        return;

    if (!ValidateObjectAllowDeletedOrNull("bindFramebuffer", wfb))
        return;

    // silently ignore a deleted frame buffer
    if (wfb && wfb->IsDeleted())
        return;

    MakeContextCurrent();

    if (!wfb) {
        gl->fBindFramebuffer(target, 0);
    } else {
        wfb->BindTo(target);
        GLuint framebuffername = wfb->GLName();
        gl->fBindFramebuffer(target, framebuffername);
    }

    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
        mBoundDrawFramebuffer = wfb;
        mBoundReadFramebuffer = wfb;
        break;
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        mBoundDrawFramebuffer = wfb;
        break;
    case LOCAL_GL_READ_FRAMEBUFFER:
        mBoundReadFramebuffer = wfb;
        break;
    default:
        break;
    }
}

void
WebGLContext::BindRenderbuffer(GLenum target, WebGLRenderbuffer* wrb)
{
    if (IsContextLost())
        return;

    if (target != LOCAL_GL_RENDERBUFFER)
        return ErrorInvalidEnumInfo("bindRenderbuffer: target", target);

    if (!ValidateObjectAllowDeletedOrNull("bindRenderbuffer", wrb))
        return;

    // silently ignore a deleted buffer
    if (wrb && wrb->IsDeleted())
        return;

    if (wrb)
        wrb->BindTo(target);

    MakeContextCurrent();

    // Sometimes we emulate renderbuffers (depth-stencil emu), so there's not
    // always a 1-1 mapping from `wrb` to GL name. Just have `wrb` handle it.
    if (wrb) {
        wrb->BindRenderbuffer();
    } else {
        gl->fBindRenderbuffer(target, 0);
    }

    mBoundRenderbuffer = wrb;
}

void
WebGLContext::BindTexture(GLenum rawTarget, WebGLTexture* newTex)
{
    if (IsContextLost())
        return;

     if (!ValidateObjectAllowDeletedOrNull("bindTexture", newTex))
        return;

    // Need to check rawTarget first before comparing against newTex->Target() as
    // newTex->Target() returns a TexTarget, which will assert on invalid value.
    WebGLRefPtr<WebGLTexture>* currentTexPtr = nullptr;
    switch (rawTarget) {
        case LOCAL_GL_TEXTURE_2D:
            currentTexPtr = &mBound2DTextures[mActiveTexture];
            break;
       case LOCAL_GL_TEXTURE_CUBE_MAP:
            currentTexPtr = &mBoundCubeMapTextures[mActiveTexture];
            break;
       case LOCAL_GL_TEXTURE_3D:
            if (!IsWebGL2()) {
                return ErrorInvalidEnum("bindTexture: target TEXTURE_3D is only available in WebGL version 2.0 or newer");
            }
            currentTexPtr = &mBound3DTextures[mActiveTexture];
            break;
       default:
            return ErrorInvalidEnumInfo("bindTexture: target", rawTarget);
    }
    const TexTarget target(rawTarget);

    if (newTex) {
        // silently ignore a deleted texture
        if (newTex->IsDeleted())
            return;

        if (newTex->HasEverBeenBound() && newTex->Target() != rawTarget)
            return ErrorInvalidOperation("bindTexture: this texture has already been bound to a different target");
    }

    *currentTexPtr = newTex;

    MakeContextCurrent();

    if (newTex) {
        SetFakeBlackStatus(WebGLContextFakeBlackStatus::Unknown);
        newTex->Bind(target);
    } else {
        gl->fBindTexture(target.get(), 0);
    }
}

void WebGLContext::BlendEquation(GLenum mode)
{
    if (IsContextLost())
        return;

    if (!ValidateBlendEquationEnum(mode, "blendEquation: mode"))
        return;

    MakeContextCurrent();
    gl->fBlendEquation(mode);
}

void WebGLContext::BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    if (IsContextLost())
        return;

    if (!ValidateBlendEquationEnum(modeRGB, "blendEquationSeparate: modeRGB") ||
        !ValidateBlendEquationEnum(modeAlpha, "blendEquationSeparate: modeAlpha"))
        return;

    MakeContextCurrent();
    gl->fBlendEquationSeparate(modeRGB, modeAlpha);
}

void WebGLContext::BlendFunc(GLenum sfactor, GLenum dfactor)
{
    if (IsContextLost())
        return;

    if (!ValidateBlendFuncSrcEnum(sfactor, "blendFunc: sfactor") ||
        !ValidateBlendFuncDstEnum(dfactor, "blendFunc: dfactor"))
        return;

    if (!ValidateBlendFuncEnumsCompatibility(sfactor, dfactor, "blendFuncSeparate: srcRGB and dstRGB"))
        return;

    MakeContextCurrent();
    gl->fBlendFunc(sfactor, dfactor);
}

void
WebGLContext::BlendFuncSeparate(GLenum srcRGB, GLenum dstRGB,
                                GLenum srcAlpha, GLenum dstAlpha)
{
    if (IsContextLost())
        return;

    if (!ValidateBlendFuncSrcEnum(srcRGB, "blendFuncSeparate: srcRGB") ||
        !ValidateBlendFuncSrcEnum(srcAlpha, "blendFuncSeparate: srcAlpha") ||
        !ValidateBlendFuncDstEnum(dstRGB, "blendFuncSeparate: dstRGB") ||
        !ValidateBlendFuncDstEnum(dstAlpha, "blendFuncSeparate: dstAlpha"))
        return;

    // note that we only check compatibity for the RGB enums, no need to for the Alpha enums, see
    // "Section 6.8 forgetting to mention alpha factors?" thread on the public_webgl mailing list
    if (!ValidateBlendFuncEnumsCompatibility(srcRGB, dstRGB, "blendFuncSeparate: srcRGB and dstRGB"))
        return;

    MakeContextCurrent();
    gl->fBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

GLenum
WebGLContext::CheckFramebufferStatus(GLenum target)
{
    if (IsContextLost())
        return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;

    if (!ValidateFramebufferTarget(target, "invalidateFramebuffer"))
        return 0;

    WebGLFramebuffer* fb;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    if (!fb)
        return LOCAL_GL_FRAMEBUFFER_COMPLETE;

    return fb->CheckFramebufferStatus().get();
}

void
WebGLContext::CopyTexSubImage2D_base(TexImageTarget texImageTarget, GLint level,
                                     TexInternalFormat internalformat,
                                     GLint xoffset, GLint yoffset, GLint x,
                                     GLint y, GLsizei width, GLsizei height,
                                     bool sub)
{
    const WebGLRectangleObject* framebufferRect = CurValidReadFBRectObject();
    GLsizei framebufferWidth = framebufferRect ? framebufferRect->Width() : 0;
    GLsizei framebufferHeight = framebufferRect ? framebufferRect->Height() : 0;

    WebGLTexImageFunc func = sub
                             ? WebGLTexImageFunc::CopyTexSubImage
                             : WebGLTexImageFunc::CopyTexImage;
    WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;
    const char* info = InfoFrom(func, dims);

    // TODO: This changes with color_buffer_float. Reassess when the
    // patch lands.
    if (!ValidateTexImage(texImageTarget, level, internalformat.get(),
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0,
                          LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    if (!ValidateCopyTexImage(internalformat.get(), func, dims))
        return;

    if (!mBoundReadFramebuffer)
        ClearBackbufferIfNeeded();

    MakeContextCurrent();

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);

    if (!tex)
        return ErrorInvalidOperation("%s: no texture is bound to this target");

    if (tex->IsImmutable()) {
        if (!sub) {
            return ErrorInvalidOperation("copyTexImage2D: disallowed because the texture bound to this target has already been made immutable by texStorage2D");
        }
    }

    TexType framebuffertype = LOCAL_GL_NONE;
    if (mBoundReadFramebuffer) {
        TexInternalFormat framebuffereffectiveformat = mBoundReadFramebuffer->ColorAttachment(0).EffectiveInternalFormat();
        framebuffertype = TypeFromInternalFormat(framebuffereffectiveformat);
    } else {
        // FIXME - here we're assuming that the default framebuffer is backed by UNSIGNED_BYTE
        // that might not always be true, say if we had a 16bpp default framebuffer.
        framebuffertype = LOCAL_GL_UNSIGNED_BYTE;
    }

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromUnsizedInternalFormatAndType(internalformat, framebuffertype);

    // this should never fail, validation happened earlier.
    MOZ_ASSERT(effectiveInternalFormat != LOCAL_GL_NONE);

    const bool widthOrHeightIsZero = (width == 0 || height == 0);
    if (gl->WorkAroundDriverBugs() &&
        sub && widthOrHeightIsZero)
    {
        // NV driver on Linux complains that CopyTexSubImage2D(level=0,
        // xoffset=0, yoffset=2, x=0, y=0, width=0, height=0) from a 300x150 FB
        // to a 0x2 texture. This a useless thing to do, but technically legal.
        // NV331.38 generates INVALID_VALUE.
        return DummyFramebufferOperation(info);
    }

    // check if the memory size of this texture may change with this call
    bool sizeMayChange = !sub;
    if (!sub && tex->HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        effectiveInternalFormat != imageInfo.EffectiveInternalFormat();
    }

    if (sizeMayChange)
        GetAndFlushUnderlyingGLErrors();

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, framebufferWidth, framebufferHeight)) {
        if (sub)
            gl->fCopyTexSubImage2D(texImageTarget.get(), level, xoffset, yoffset, x, y, width, height);
        else
            gl->fCopyTexImage2D(texImageTarget.get(), level, internalformat.get(), x, y, width, height, 0);
    } else {

        // the rect doesn't fit in the framebuffer

        // first, we initialize the texture as black
        if (!sub) {
            tex->SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat,
                      WebGLImageDataStatus::UninitializedImageData);
            tex->EnsureNoUninitializedImageData(texImageTarget, level);
        }

        // if we are completely outside of the framebuffer, we can exit now with our black texture
        if (   x >= framebufferWidth
            || x+width <= 0
            || y >= framebufferHeight
            || y+height <= 0)
        {
            // we are completely outside of range, can exit now with buffer filled with zeros
            return DummyFramebufferOperation(info);
        }

        GLint   actual_x             = clamped(x, 0, framebufferWidth);
        GLint   actual_x_plus_width  = clamped(x + width, 0, framebufferWidth);
        GLsizei actual_width   = actual_x_plus_width  - actual_x;
        GLint   actual_xoffset = xoffset + actual_x - x;

        GLint   actual_y             = clamped(y, 0, framebufferHeight);
        GLint   actual_y_plus_height = clamped(y + height, 0, framebufferHeight);
        GLsizei actual_height  = actual_y_plus_height - actual_y;
        GLint   actual_yoffset = yoffset + actual_y - y;

        gl->fCopyTexSubImage2D(texImageTarget.get(), level, actual_xoffset, actual_yoffset, actual_x, actual_y, actual_width, actual_height);
    }

    if (sizeMayChange) {
        GLenum error = GetAndFlushUnderlyingGLErrors();
        if (error) {
            GenerateWarning("copyTexImage2D generated error %s", ErrorName(error));
            return;
        }
    }

    if (!sub) {
        tex->SetImageInfo(texImageTarget, level, width, height, 1,
                          effectiveInternalFormat,
                          WebGLImageDataStatus::InitializedImageData);
    }
}

void
WebGLContext::CopyTexImage2D(GLenum rawTexImgTarget,
                             GLint level,
                             GLenum internalformat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    if (IsContextLost())
        return;

    // copyTexImage2D only generates textures with type = UNSIGNED_BYTE
    const WebGLTexImageFunc func = WebGLTexImageFunc::CopyTexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (!ValidateTexImageTarget(rawTexImgTarget, func, dims))
        return;

    if (!ValidateTexImage(rawTexImgTarget, level, internalformat,
                          0, 0, 0,
                          width, height, 0,
                          border, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    if (!ValidateCopyTexImage(internalformat, func, dims))
        return;

    if (!mBoundReadFramebuffer)
        ClearBackbufferIfNeeded();

    CopyTexSubImage2D_base(rawTexImgTarget, level, internalformat, 0, 0, x, y, width, height, false);
}

void
WebGLContext::CopyTexSubImage2D(GLenum rawTexImgTarget,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    if (IsContextLost())
        return;

    switch (rawTexImgTarget) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            return ErrorInvalidEnumInfo("copyTexSubImage2D: target", rawTexImgTarget);
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    if (level < 0)
        return ErrorInvalidValue("copyTexSubImage2D: level may not be negative");

    GLsizei maxTextureSize = MaxTextureSizeForTarget(TexImageTargetToTexTarget(texImageTarget));
    if (!(maxTextureSize >> level))
        return ErrorInvalidValue("copyTexSubImage2D: 2^level exceeds maximum texture size");

    if (width < 0 || height < 0)
        return ErrorInvalidValue("copyTexSubImage2D: width and height may not be negative");

    if (xoffset < 0 || yoffset < 0)
        return ErrorInvalidValue("copyTexSubImage2D: xoffset and yoffset may not be negative");

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex)
        return ErrorInvalidOperation("copyTexSubImage2D: no texture bound to this target");

    if (!tex->HasImageInfoAt(texImageTarget, level))
        return ErrorInvalidOperation("copyTexSubImage2D: no texture image previously defined for this level and face");

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    GLsizei texWidth = imageInfo.Width();
    GLsizei texHeight = imageInfo.Height();

    if (xoffset + width > texWidth || xoffset + width < 0)
      return ErrorInvalidValue("copyTexSubImage2D: xoffset+width is too large");

    if (yoffset + height > texHeight || yoffset + height < 0)
      return ErrorInvalidValue("copyTexSubImage2D: yoffset+height is too large");

    if (!mBoundReadFramebuffer)
        ClearBackbufferIfNeeded();

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                width == texWidth &&
                                height == texHeight;
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            tex->EnsureNoUninitializedImageData(texImageTarget, level);
        }
    }

    TexInternalFormat internalformat;
    TexType type;
    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(imageInfo.EffectiveInternalFormat(),
                                             &internalformat, &type);
    return CopyTexSubImage2D_base(texImageTarget, level, internalformat, xoffset, yoffset, x, y, width, height, true);
}


already_AddRefed<WebGLProgram>
WebGLContext::CreateProgram()
{
    if (IsContextLost())
        return nullptr;
    nsRefPtr<WebGLProgram> globj = new WebGLProgram(this);
    return globj.forget();
}

already_AddRefed<WebGLShader>
WebGLContext::CreateShader(GLenum type)
{
    if (IsContextLost())
        return nullptr;

    if (type != LOCAL_GL_VERTEX_SHADER &&
        type != LOCAL_GL_FRAGMENT_SHADER)
    {
        ErrorInvalidEnumInfo("createShader: type", type);
        return nullptr;
    }

    nsRefPtr<WebGLShader> shader = new WebGLShader(this, type);
    return shader.forget();
}

void
WebGLContext::CullFace(GLenum face)
{
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face, "cullFace"))
        return;

    MakeContextCurrent();
    gl->fCullFace(face);
}

void
WebGLContext::DeleteFramebuffer(WebGLFramebuffer* fbuf)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteFramebuffer", fbuf))
        return;

    if (!fbuf || fbuf->IsDeleted())
        return;

    fbuf->RequestDelete();

    if (mBoundReadFramebuffer == mBoundDrawFramebuffer) {
        if (mBoundDrawFramebuffer == fbuf) {
            BindFramebuffer(LOCAL_GL_FRAMEBUFFER,
                            static_cast<WebGLFramebuffer*>(nullptr));
        }
    } else if (mBoundDrawFramebuffer == fbuf) {
        BindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER,
                        static_cast<WebGLFramebuffer*>(nullptr));
    } else if (mBoundReadFramebuffer == fbuf) {
        BindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER,
                        static_cast<WebGLFramebuffer*>(nullptr));
    }
}

void
WebGLContext::DeleteRenderbuffer(WebGLRenderbuffer* rbuf)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteRenderbuffer", rbuf))
        return;

    if (!rbuf || rbuf->IsDeleted())
        return;

    if (mBoundDrawFramebuffer)
        mBoundDrawFramebuffer->DetachRenderbuffer(rbuf);

    if (mBoundReadFramebuffer)
        mBoundReadFramebuffer->DetachRenderbuffer(rbuf);

    rbuf->InvalidateStatusOfAttachedFBs();

    if (mBoundRenderbuffer == rbuf)
        BindRenderbuffer(LOCAL_GL_RENDERBUFFER, nullptr);

    rbuf->RequestDelete();
}

void
WebGLContext::DeleteTexture(WebGLTexture* tex)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteTexture", tex))
        return;

    if (!tex || tex->IsDeleted())
        return;

    if (mBoundDrawFramebuffer)
        mBoundDrawFramebuffer->DetachTexture(tex);

    if (mBoundReadFramebuffer)
        mBoundReadFramebuffer->DetachTexture(tex);

    tex->InvalidateStatusOfAttachedFBs();

    GLuint activeTexture = mActiveTexture;
    for (int32_t i = 0; i < mGLMaxTextureUnits; i++) {
        if ((mBound2DTextures[i] == tex && tex->Target() == LOCAL_GL_TEXTURE_2D) ||
            (mBoundCubeMapTextures[i] == tex && tex->Target() == LOCAL_GL_TEXTURE_CUBE_MAP) ||
            (mBound3DTextures[i] == tex && tex->Target() == LOCAL_GL_TEXTURE_3D))
        {
            ActiveTexture(LOCAL_GL_TEXTURE0 + i);
            BindTexture(tex->Target().get(), nullptr);
        }
    }
    ActiveTexture(LOCAL_GL_TEXTURE0 + activeTexture);

    tex->RequestDelete();
}

void
WebGLContext::DeleteProgram(WebGLProgram* prog)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteProgram", prog))
        return;

    if (!prog || prog->IsDeleted())
        return;

    prog->RequestDelete();
}

void
WebGLContext::DeleteShader(WebGLShader* shader)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteShader", shader))
        return;

    if (!shader || shader->IsDeleted())
        return;

    shader->RequestDelete();
}

void
WebGLContext::DetachShader(WebGLProgram* program, WebGLShader* shader)
{
    if (IsContextLost())
        return;

    // It's valid to attempt to detach a deleted shader, since it's still a
    // shader.
    if (!ValidateObject("detachShader: program", program) ||
        !ValidateObjectAllowDeleted("detashShader: shader", shader))
    {
        return;
    }

    program->DetachShader(shader);
}

void
WebGLContext::DepthFunc(GLenum func)
{
    if (IsContextLost())
        return;

    if (!ValidateComparisonEnum(func, "depthFunc"))
        return;

    MakeContextCurrent();
    gl->fDepthFunc(func);
}

void
WebGLContext::DepthRange(GLfloat zNear, GLfloat zFar)
{
    if (IsContextLost())
        return;

    if (zNear > zFar)
        return ErrorInvalidOperation("depthRange: the near value is greater than the far value!");

    MakeContextCurrent();
    gl->fDepthRange(zNear, zFar);
}

void
WebGLContext::FramebufferRenderbuffer(GLenum target, GLenum attachment,
                                      GLenum rbtarget, WebGLRenderbuffer* wrb)
{
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, "framebufferRenderbuffer"))
        return;

    WebGLFramebuffer* fb;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    if (!fb) {
        return ErrorInvalidOperation("framebufferRenderbuffer: cannot modify"
                                     " framebuffer 0.");
    }

    if (rbtarget != LOCAL_GL_RENDERBUFFER) {
        return ErrorInvalidEnumInfo("framebufferRenderbuffer: rbtarget:",
                                    rbtarget);
    }

    if (!ValidateFramebufferAttachment(fb, attachment, "framebufferRenderbuffer"))
        return;

    fb->FramebufferRenderbuffer(attachment, rbtarget, wrb);
}

void
WebGLContext::FramebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   WebGLTexture* tobj,
                                   GLint level)
{
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, "framebufferTexture2D"))
        return;

    if (!IsWebGL2() && level != 0) {
        ErrorInvalidValue("framebufferTexture2D: level must be 0.");
        return;
    }

    WebGLFramebuffer* fb;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    if (!fb) {
        return ErrorInvalidOperation("framebufferTexture2D: cannot modify"
                                     " framebuffer 0.");
    }

    if (textarget != LOCAL_GL_TEXTURE_2D &&
        (textarget < LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
         textarget > LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
    {
        return ErrorInvalidEnumInfo("framebufferTexture2D: textarget:",
                                    textarget);
    }

    if (!ValidateFramebufferAttachment(fb, attachment, "framebufferTexture2D"))
        return;

    fb->FramebufferTexture2D(attachment, textarget, tobj, level);
}

void
WebGLContext::FrontFace(GLenum mode)
{
    if (IsContextLost())
        return;

    switch (mode) {
        case LOCAL_GL_CW:
        case LOCAL_GL_CCW:
            break;
        default:
            return ErrorInvalidEnumInfo("frontFace: mode", mode);
    }

    MakeContextCurrent();
    gl->fFrontFace(mode);
}

void
WebGLContext::GenerateMipmap(GLenum rawTarget)
{
    if (IsContextLost())
        return;

    if (!ValidateTextureTargetEnum(rawTarget, "generateMipmap"))
        return;

    const TexTarget target(rawTarget);

    WebGLTexture* tex = ActiveBoundTextureForTarget(target);

    if (!tex)
        return ErrorInvalidOperation("generateMipmap: No texture is bound to this target.");

    const TexImageTarget imageTarget = (target == LOCAL_GL_TEXTURE_2D)
                                                  ? LOCAL_GL_TEXTURE_2D
                                                  : LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    if (!tex->IsMipmapRangeValid())
    {
        return ErrorInvalidOperation("generateMipmap: Texture does not have a valid mipmap range.");
    }
    if (!tex->HasImageInfoAt(imageTarget, tex->EffectiveBaseMipmapLevel()))
    {
        return ErrorInvalidOperation("generateMipmap: Level zero of texture is not defined.");
    }

    if (!IsWebGL2() && !tex->IsFirstImagePowerOfTwo())
        return ErrorInvalidOperation("generateMipmap: Level zero of texture does not have power-of-two width and height.");

    TexInternalFormat internalformat = tex->ImageInfoAt(imageTarget, 0).EffectiveInternalFormat();
    if (IsTextureFormatCompressed(internalformat))
        return ErrorInvalidOperation("generateMipmap: Texture data at level zero is compressed.");

    if (IsExtensionEnabled(WebGLExtensionID::WEBGL_depth_texture) &&
        (IsGLDepthFormat(internalformat) || IsGLDepthStencilFormat(internalformat)))
    {
        return ErrorInvalidOperation("generateMipmap: "
                                     "A texture that has a base internal format of "
                                     "DEPTH_COMPONENT or DEPTH_STENCIL isn't supported");
    }

    if (!tex->AreAllLevel0ImageInfosEqual())
        return ErrorInvalidOperation("generateMipmap: The six faces of this cube map have different dimensions, format, or type.");

    tex->SetGeneratedMipmap();

    MakeContextCurrent();

    if (gl->WorkAroundDriverBugs()) {
        // bug 696495 - to work around failures in the texture-mips.html test on various drivers, we
        // set the minification filter before calling glGenerateMipmap. This should not carry a significant performance
        // overhead so we do it unconditionally.
        //
        // note that the choice of GL_NEAREST_MIPMAP_NEAREST really matters. See Chromium bug 101105.
        gl->fTexParameteri(target.get(), LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST_MIPMAP_NEAREST);
        gl->fGenerateMipmap(target.get());
        gl->fTexParameteri(target.get(), LOCAL_GL_TEXTURE_MIN_FILTER, tex->MinFilter().get());
    } else {
        gl->fGenerateMipmap(target.get());
    }
}

already_AddRefed<WebGLActiveInfo>
WebGLContext::GetActiveAttrib(WebGLProgram* prog, GLuint index)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("getActiveAttrib: program", prog))
        return nullptr;

    return prog->GetActiveAttrib(index);
}

already_AddRefed<WebGLActiveInfo>
WebGLContext::GetActiveUniform(WebGLProgram* prog, GLuint index)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("getActiveUniform: program", prog))
        return nullptr;

    return prog->GetActiveUniform(index);
}

void
WebGLContext::GetAttachedShaders(WebGLProgram* prog,
                                 Nullable<nsTArray<nsRefPtr<WebGLShader>>>& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    if (!prog) {
        ErrorInvalidValue("getAttachedShaders: Invalid program.");
        return;
    }

    if (!ValidateObject("getAttachedShaders", prog))
        return;

    prog->GetAttachedShaders(&retval.SetValue());
}

GLint
WebGLContext::GetAttribLocation(WebGLProgram* prog, const nsAString& name)
{
    if (IsContextLost())
        return -1;

    if (!ValidateObject("getAttribLocation: program", prog))
        return -1;

    return prog->GetAttribLocation(name);
}

JS::Value
WebGLContext::GetBufferParameter(GLenum target, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateBufferTarget(target, "getBufferParameter"))
        return JS::NullValue();

    WebGLRefPtr<WebGLBuffer>& slot = GetBufferSlotByTarget(target);
    if (!slot) {
        ErrorInvalidOperation("No buffer bound to `target` (0x%4x).", target);
        return JS::NullValue();
    }

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_BUFFER_SIZE:
        case LOCAL_GL_BUFFER_USAGE:
        {
            GLint i = 0;
            gl->fGetBufferParameteriv(target, pname, &i);
            if (pname == LOCAL_GL_BUFFER_SIZE) {
                return JS::Int32Value(i);
            }

            MOZ_ASSERT(pname == LOCAL_GL_BUFFER_USAGE);
            return JS::NumberValue(uint32_t(i));
        }
            break;

        default:
            ErrorInvalidEnumInfo("getBufferParameter: parameter", pname);
    }

    return JS::NullValue();
}

JS::Value
WebGLContext::GetFramebufferAttachmentParameter(JSContext* cx,
                                                GLenum target,
                                                GLenum attachment,
                                                GLenum pname,
                                                ErrorResult& rv)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateFramebufferTarget(target, "getFramebufferAttachmentParameter"))
        return JS::NullValue();

    WebGLFramebuffer* fb;
    switch (target) {
    case LOCAL_GL_FRAMEBUFFER:
    case LOCAL_GL_DRAW_FRAMEBUFFER:
        fb = mBoundDrawFramebuffer;
        break;

    case LOCAL_GL_READ_FRAMEBUFFER:
        fb = mBoundReadFramebuffer;
        break;

    default:
        MOZ_CRASH("Bad target.");
    }

    if (!fb) {
        ErrorInvalidOperation("getFramebufferAttachmentParameter: cannot query"
                              " framebuffer 0.");
        return JS::NullValue();
    }

    if (!ValidateFramebufferAttachment(fb, attachment,
                                       "getFramebufferAttachmentParameter"))
    {
        return JS::NullValue();
    }

    if (IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers))
        fb->EnsureColorAttachPoints(attachment - LOCAL_GL_COLOR_ATTACHMENT0);

    MakeContextCurrent();

    const WebGLFramebuffer::AttachPoint& fba = fb->GetAttachPoint(attachment);

    if (fba.Renderbuffer()) {
        switch (pname) {
            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
                if (IsExtensionEnabled(WebGLExtensionID::EXT_sRGB)) {
                    const GLenum internalFormat = fba.Renderbuffer()->InternalFormat();
                    return (internalFormat == LOCAL_GL_SRGB_EXT ||
                            internalFormat == LOCAL_GL_SRGB_ALPHA_EXT ||
                            internalFormat == LOCAL_GL_SRGB8_ALPHA8_EXT) ?
                        JS::NumberValue(uint32_t(LOCAL_GL_SRGB_EXT)) :
                        JS::NumberValue(uint32_t(LOCAL_GL_LINEAR));
                }
                break;

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                return JS::NumberValue(uint32_t(LOCAL_GL_RENDERBUFFER));

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                return WebGLObjectAsJSValue(cx, fba.Renderbuffer(), rv);

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: {
                if (!IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_half_float) &&
                    !IsExtensionEnabled(WebGLExtensionID::WEBGL_color_buffer_float))
                {
                    break;
                }

                if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
                    ErrorInvalidOperation("getFramebufferAttachmentParameter: Cannot get component"
                                          " type of a depth-stencil attachment.");
                    return JS::NullValue();
                }

                if (!fba.IsComplete())
                    return JS::NumberValue(uint32_t(LOCAL_GL_NONE));

                uint32_t ret = LOCAL_GL_NONE;
                switch (fba.Renderbuffer()->InternalFormat()) {
                case LOCAL_GL_RGBA4:
                case LOCAL_GL_RGB5_A1:
                case LOCAL_GL_RGB565:
                case LOCAL_GL_SRGB8_ALPHA8:
                    ret = LOCAL_GL_UNSIGNED_NORMALIZED;
                    break;
                case LOCAL_GL_RGB16F:
                case LOCAL_GL_RGBA16F:
                case LOCAL_GL_RGB32F:
                case LOCAL_GL_RGBA32F:
                    ret = LOCAL_GL_FLOAT;
                    break;
                case LOCAL_GL_DEPTH_COMPONENT16:
                case LOCAL_GL_STENCIL_INDEX8:
                    ret = LOCAL_GL_UNSIGNED_INT;
                    break;
                default:
                    MOZ_ASSERT(false, "Unhandled RB component type.");
                    break;
                }
                return JS::NumberValue(uint32_t(ret));
            }
        }

        ErrorInvalidEnumInfo("getFramebufferAttachmentParameter: pname", pname);
        return JS::NullValue();
    } else if (fba.Texture()) {
        switch (pname) {
             case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
                if (IsExtensionEnabled(WebGLExtensionID::EXT_sRGB)) {
                    const TexInternalFormat effectiveInternalFormat =
                        fba.Texture()->ImageInfoBase().EffectiveInternalFormat();
                    TexInternalFormat unsizedinternalformat = LOCAL_GL_NONE;
                    TexType type = LOCAL_GL_NONE;
                    UnsizedInternalFormatAndTypeFromEffectiveInternalFormat(
                        effectiveInternalFormat, &unsizedinternalformat, &type);
                    MOZ_ASSERT(unsizedinternalformat != LOCAL_GL_NONE);
                    const bool srgb = unsizedinternalformat == LOCAL_GL_SRGB ||
                                      unsizedinternalformat == LOCAL_GL_SRGB_ALPHA;
                    return srgb ? JS::NumberValue(uint32_t(LOCAL_GL_SRGB))
                                : JS::NumberValue(uint32_t(LOCAL_GL_LINEAR));
                }
                break;

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                return JS::NumberValue(uint32_t(LOCAL_GL_TEXTURE));

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                return WebGLObjectAsJSValue(cx, fba.Texture(), rv);

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
                return JS::Int32Value(fba.MipLevel());

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: {
                GLenum face = fba.ImageTarget().get();
                if (face == LOCAL_GL_TEXTURE_2D)
                    face = 0;
                return JS::Int32Value(face);
            }

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: {
                if (!IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_half_float) &&
                    !IsExtensionEnabled(WebGLExtensionID::WEBGL_color_buffer_float))
                {
                    break;
                }

                if (attachment == LOCAL_GL_DEPTH_STENCIL_ATTACHMENT) {
                    ErrorInvalidOperation("getFramebufferAttachmentParameter: cannot component"
                                          " type of depth-stencil attachments.");
                    return JS::NullValue();
                }

                if (!fba.IsComplete())
                    return JS::NumberValue(uint32_t(LOCAL_GL_NONE));

                TexInternalFormat effectiveInternalFormat =
                    fba.Texture()->ImageInfoAt(fba.ImageTarget(), fba.MipLevel()).EffectiveInternalFormat();
                TexType type = TypeFromInternalFormat(effectiveInternalFormat);
                GLenum ret = LOCAL_GL_NONE;
                switch (type.get()) {
                case LOCAL_GL_UNSIGNED_BYTE:
                case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
                case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
                case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
                    ret = LOCAL_GL_UNSIGNED_NORMALIZED;
                    break;
                case LOCAL_GL_FLOAT:
                case LOCAL_GL_HALF_FLOAT:
                    ret = LOCAL_GL_FLOAT;
                    break;
                case LOCAL_GL_UNSIGNED_SHORT:
                case LOCAL_GL_UNSIGNED_INT:
                    ret = LOCAL_GL_UNSIGNED_INT;
                    break;
                default:
                    MOZ_ASSERT(false, "Unhandled RB component type.");
                    break;
                }
                return JS::NumberValue(uint32_t(ret));
            }
        }

        ErrorInvalidEnumInfo("getFramebufferAttachmentParameter: pname", pname);
        return JS::NullValue();
    } else {
        switch (pname) {
            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                return JS::NumberValue(uint32_t(LOCAL_GL_NONE));

            default:
                ErrorInvalidEnumInfo("getFramebufferAttachmentParameter: pname", pname);
                return JS::NullValue();
        }
    }

    return JS::NullValue();
}

JS::Value
WebGLContext::GetRenderbufferParameter(GLenum target, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    if (target != LOCAL_GL_RENDERBUFFER) {
        ErrorInvalidEnumInfo("getRenderbufferParameter: target", target);
        return JS::NullValue();
    }

    if (!mBoundRenderbuffer) {
        ErrorInvalidOperation("getRenderbufferParameter: no render buffer is bound");
        return JS::NullValue();
    }

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_RENDERBUFFER_WIDTH:
        case LOCAL_GL_RENDERBUFFER_HEIGHT:
        case LOCAL_GL_RENDERBUFFER_RED_SIZE:
        case LOCAL_GL_RENDERBUFFER_GREEN_SIZE:
        case LOCAL_GL_RENDERBUFFER_BLUE_SIZE:
        case LOCAL_GL_RENDERBUFFER_ALPHA_SIZE:
        case LOCAL_GL_RENDERBUFFER_DEPTH_SIZE:
        case LOCAL_GL_RENDERBUFFER_STENCIL_SIZE:
        {
            // RB emulation means we have to ask the RB itself.
            GLint i = mBoundRenderbuffer->GetRenderbufferParameter(target, pname);
            return JS::Int32Value(i);
        }
        case LOCAL_GL_RENDERBUFFER_INTERNAL_FORMAT:
        {
            return JS::NumberValue(mBoundRenderbuffer->InternalFormat());
        }
        default:
            ErrorInvalidEnumInfo("getRenderbufferParameter: parameter", pname);
    }

    return JS::NullValue();
}

already_AddRefed<WebGLTexture>
WebGLContext::CreateTexture()
{
    if (IsContextLost())
        return nullptr;

    GLuint tex = 0;
    MakeContextCurrent();
    gl->fGenTextures(1, &tex);

    nsRefPtr<WebGLTexture> globj = new WebGLTexture(this, tex);
    return globj.forget();
}

static GLenum
GetAndClearError(GLenum* errorVar)
{
    MOZ_ASSERT(errorVar);
    GLenum ret = *errorVar;
    *errorVar = LOCAL_GL_NO_ERROR;
    return ret;
}

GLenum
WebGLContext::GetError()
{
    /* WebGL 1.0: Section 5.14.3: Setting and getting state:
     *   If the context's webgl context lost flag is set, returns
     *   CONTEXT_LOST_WEBGL the first time this method is called.
     *   Afterward, returns NO_ERROR until the context has been
     *   restored.
     *
     * WEBGL_lose_context:
     *   [When this extension is enabled: ] loseContext and
     *   restoreContext are allowed to generate INVALID_OPERATION errors
     *   even when the context is lost.
     */

    if (IsContextLost()) {
        if (mEmitContextLostErrorOnce) {
            mEmitContextLostErrorOnce = false;
            return LOCAL_GL_CONTEXT_LOST;
        }
        // Don't return yet, since WEBGL_lose_contexts contradicts the
        // original spec, and allows error generation while lost.
    }

    GLenum err = GetAndClearError(&mWebGLError);
    if (err != LOCAL_GL_NO_ERROR)
        return err;

    if (IsContextLost())
        return LOCAL_GL_NO_ERROR;

    // Either no WebGL-side error, or it's already been cleared.
    // UnderlyingGL-side errors, now.

    MakeContextCurrent();
    GetAndFlushUnderlyingGLErrors();

    err = GetAndClearError(&mUnderlyingGLError);
    return err;
}

JS::Value
WebGLContext::GetProgramParameter(WebGLProgram* prog, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObjectAllowDeleted("getProgramParameter: program", prog))
        return JS::NullValue();

    return prog->GetProgramParameter(pname);
}

void
WebGLContext::GetProgramInfoLog(WebGLProgram* prog, nsAString& retval)
{
    retval.SetIsVoid(true);

    if (IsContextLost())
        return;

    if (!ValidateObject("getProgramInfoLog: program", prog))
        return;

    prog->GetProgramInfoLog(&retval);

    retval.SetIsVoid(false);
}

// here we have to support all pnames with both int and float params.
// See this discussion:
//  https://www.khronos.org/webgl/public-mailing-list/archives/1008/msg00014.html
void WebGLContext::TexParameter_base(GLenum rawTarget, GLenum pname,
                                     GLint* intParamPtr,
                                     GLfloat* floatParamPtr)
{
    MOZ_ASSERT(intParamPtr || floatParamPtr);

    if (IsContextLost())
        return;

    GLint intParam = intParamPtr ? *intParamPtr : GLint(*floatParamPtr);
    GLfloat floatParam = floatParamPtr ? *floatParamPtr : GLfloat(*intParamPtr);

    if (!ValidateTextureTargetEnum(rawTarget, "texParameter: target"))
        return;

    const TexTarget texTarget = TexTarget(rawTarget);

    WebGLTexture* tex = ActiveBoundTextureForTarget(texTarget);
    if (!tex)
        return ErrorInvalidOperation("texParameter: no texture is bound to this target");

    bool pnameAndParamAreIncompatible = false;
    bool paramValueInvalid = false;

    switch (pname) {
        case LOCAL_GL_TEXTURE_BASE_LEVEL:
        case LOCAL_GL_TEXTURE_MAX_LEVEL:
            if (!IsWebGL2())
                return ErrorInvalidEnumInfo("texParameter: pname", pname);
            if (intParam < 0) {
                paramValueInvalid = true;
                break;
            }
            if (pname == LOCAL_GL_TEXTURE_BASE_LEVEL)
                tex->SetBaseMipmapLevel(intParam);
            else
                tex->SetMaxMipmapLevel(intParam);
            break;

        case LOCAL_GL_TEXTURE_COMPARE_MODE:
            if (!IsWebGL2())
                return ErrorInvalidEnumInfo("texParameter: pname", pname);

            paramValueInvalid = (intParam != LOCAL_GL_NONE &&
                                 intParam != LOCAL_GL_COMPARE_REF_TO_TEXTURE);
            break;

        case LOCAL_GL_TEXTURE_COMPARE_FUNC:
            if (!IsWebGL2())
                return ErrorInvalidEnumInfo("texParameter: pname", pname);

            switch (intParam) {
            case LOCAL_GL_LEQUAL:
            case LOCAL_GL_GEQUAL:
            case LOCAL_GL_LESS:
            case LOCAL_GL_GREATER:
            case LOCAL_GL_EQUAL:
            case LOCAL_GL_NOTEQUAL:
            case LOCAL_GL_ALWAYS:
            case LOCAL_GL_NEVER:
                break;

            default:
                pnameAndParamAreIncompatible = true;
            }
            break;

        case LOCAL_GL_TEXTURE_MIN_FILTER:
            switch (intParam) {
                case LOCAL_GL_NEAREST:
                case LOCAL_GL_LINEAR:
                case LOCAL_GL_NEAREST_MIPMAP_NEAREST:
                case LOCAL_GL_LINEAR_MIPMAP_NEAREST:
                case LOCAL_GL_NEAREST_MIPMAP_LINEAR:
                case LOCAL_GL_LINEAR_MIPMAP_LINEAR:
                    tex->SetMinFilter(intParam);
                    break;
                default:
                    pnameAndParamAreIncompatible = true;
            }
            break;
        case LOCAL_GL_TEXTURE_MAG_FILTER:
            switch (intParam) {
                case LOCAL_GL_NEAREST:
                case LOCAL_GL_LINEAR:
                    tex->SetMagFilter(intParam);
                    break;
                default:
                    pnameAndParamAreIncompatible = true;
            }
            break;
        case LOCAL_GL_TEXTURE_WRAP_S:
            switch (intParam) {
                case LOCAL_GL_CLAMP_TO_EDGE:
                case LOCAL_GL_MIRRORED_REPEAT:
                case LOCAL_GL_REPEAT:
                    tex->SetWrapS(intParam);
                    break;
                default:
                    pnameAndParamAreIncompatible = true;
            }
            break;
        case LOCAL_GL_TEXTURE_WRAP_T:
            switch (intParam) {
                case LOCAL_GL_CLAMP_TO_EDGE:
                case LOCAL_GL_MIRRORED_REPEAT:
                case LOCAL_GL_REPEAT:
                    tex->SetWrapT(intParam);
                    break;
                default:
                    pnameAndParamAreIncompatible = true;
            }
            break;
        case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic)) {
                if (floatParamPtr && floatParam < 1.f)
                    paramValueInvalid = true;
                else if (intParamPtr && intParam < 1)
                    paramValueInvalid = true;
            }
            else
                pnameAndParamAreIncompatible = true;
            break;
        default:
            return ErrorInvalidEnumInfo("texParameter: pname", pname);
    }

    if (pnameAndParamAreIncompatible) {
        if (intParamPtr)
            return ErrorInvalidEnum("texParameteri: pname %x and param %x (decimal %d) are mutually incompatible",
                                    pname, intParam, intParam);
        else
            return ErrorInvalidEnum("texParameterf: pname %x and param %g are mutually incompatible",
                                    pname, floatParam);
    } else if (paramValueInvalid) {
        if (intParamPtr)
            return ErrorInvalidValue("texParameteri: pname %x and param %x (decimal %d) is invalid",
                                    pname, intParam, intParam);
        else
            return ErrorInvalidValue("texParameterf: pname %x and param %g is invalid",
                                    pname, floatParam);
    }

    MakeContextCurrent();
    if (intParamPtr)
        gl->fTexParameteri(texTarget.get(), pname, intParam);
    else
        gl->fTexParameterf(texTarget.get(), pname, floatParam);
}

JS::Value
WebGLContext::GetTexParameter(GLenum rawTarget, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    MakeContextCurrent();

    if (!ValidateTextureTargetEnum(rawTarget, "getTexParameter: target"))
        return JS::NullValue();

    const TexTarget target(rawTarget);

    if (!ActiveBoundTextureForTarget(target)) {
        ErrorInvalidOperation("getTexParameter: no texture bound");
        return JS::NullValue();
    }

    return GetTexParameterInternal(target, pname);
}

JS::Value
WebGLContext::GetTexParameterInternal(const TexTarget& target, GLenum pname)
{
    switch (pname) {
        case LOCAL_GL_TEXTURE_MIN_FILTER:
        case LOCAL_GL_TEXTURE_MAG_FILTER:
        case LOCAL_GL_TEXTURE_WRAP_S:
        case LOCAL_GL_TEXTURE_WRAP_T:
        {
            GLint i = 0;
            gl->fGetTexParameteriv(target.get(), pname, &i);
            return JS::NumberValue(uint32_t(i));
        }
        case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic)) {
                GLfloat f = 0.f;
                gl->fGetTexParameterfv(target.get(), pname, &f);
                return JS::DoubleValue(f);
            }

            ErrorInvalidEnumInfo("getTexParameter: parameter", pname);
            break;

        default:
            ErrorInvalidEnumInfo("getTexParameter: parameter", pname);
    }

    return JS::NullValue();
}

JS::Value
WebGLContext::GetUniform(JSContext* js, WebGLProgram* prog,
                         WebGLUniformLocation* loc)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObject("getUniform: `program`", prog))
        return JS::NullValue();

    if (!ValidateObject("getUniform: `location`", loc))
        return JS::NullValue();

    if (!loc->ValidateForProgram(prog, this, "getUniform"))
        return JS::NullValue();

    return loc->GetUniform(js, this);
}

already_AddRefed<WebGLUniformLocation>
WebGLContext::GetUniformLocation(WebGLProgram* prog, const nsAString& name)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("getUniformLocation: program", prog))
        return nullptr;

    return prog->GetUniformLocation(name);
}

void
WebGLContext::Hint(GLenum target, GLenum mode)
{
    if (IsContextLost())
        return;

    bool isValid = false;

    switch (target) {
        case LOCAL_GL_GENERATE_MIPMAP_HINT:
            isValid = true;
            break;
        case LOCAL_GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
            if (IsExtensionEnabled(WebGLExtensionID::OES_standard_derivatives))
                isValid = true;
            break;
    }

    if (!isValid)
        return ErrorInvalidEnum("hint: invalid hint");

    MakeContextCurrent();
    gl->fHint(target, mode);
}

bool
WebGLContext::IsFramebuffer(WebGLFramebuffer* fb)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isFramebuffer", fb) &&
        !fb->IsDeleted() &&
        fb->HasEverBeenBound();
}

bool
WebGLContext::IsProgram(WebGLProgram* prog)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isProgram", prog) && !prog->IsDeleted();
}

bool
WebGLContext::IsRenderbuffer(WebGLRenderbuffer* rb)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isRenderBuffer", rb) &&
        !rb->IsDeleted() &&
        rb->HasEverBeenBound();
}

bool
WebGLContext::IsShader(WebGLShader* shader)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isShader", shader) &&
        !shader->IsDeleted();
}

bool
WebGLContext::IsTexture(WebGLTexture* tex)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isTexture", tex) &&
        !tex->IsDeleted() &&
        tex->HasEverBeenBound();
}

void
WebGLContext::LinkProgram(WebGLProgram* prog)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("linkProgram", prog))
        return;

    prog->LinkProgram();

    if (prog->IsLinked()) {
        mActiveProgramLinkInfo = prog->LinkInfo();

        if (gl->WorkAroundDriverBugs() &&
            gl->Vendor() == gl::GLVendor::NVIDIA)
        {
            if (mCurrentProgram == prog)
                gl->fUseProgram(prog->mGLName);
        }
    }
}

void
WebGLContext::PixelStorei(GLenum pname, GLint param)
{
    if (IsContextLost())
        return;

    switch (pname) {
        case UNPACK_FLIP_Y_WEBGL:
            mPixelStoreFlipY = (param != 0);
            break;
        case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
            mPixelStorePremultiplyAlpha = (param != 0);
            break;
        case UNPACK_COLORSPACE_CONVERSION_WEBGL:
            if (param == LOCAL_GL_NONE || param == BROWSER_DEFAULT_WEBGL)
                mPixelStoreColorspaceConversion = param;
            else
                return ErrorInvalidEnumInfo("pixelStorei: colorspace conversion parameter", param);
            break;
        case LOCAL_GL_PACK_ALIGNMENT:
        case LOCAL_GL_UNPACK_ALIGNMENT:
            if (param != 1 &&
                param != 2 &&
                param != 4 &&
                param != 8)
                return ErrorInvalidValue("pixelStorei: invalid pack/unpack alignment value");
            if (pname == LOCAL_GL_PACK_ALIGNMENT)
                mPixelStorePackAlignment = param;
            else if (pname == LOCAL_GL_UNPACK_ALIGNMENT)
                mPixelStoreUnpackAlignment = param;
            MakeContextCurrent();
            gl->fPixelStorei(pname, param);
            break;
        default:
            return ErrorInvalidEnumInfo("pixelStorei: parameter", pname);
    }
}

// `width` in pixels.
// `stride` in bytes.
static bool
SetFullAlpha(void* data, GLenum format, GLenum type, size_t width,
             size_t height, size_t stride)
{
    if (format == LOCAL_GL_ALPHA && type == LOCAL_GL_UNSIGNED_BYTE) {
        // Just memset the rows.
        for (size_t j = 0; j < height; ++j) {
            uint8_t* row = static_cast<uint8_t*>(data) + j*stride;
            memset(row, 0xff, width);
            row += stride;
        }

        return true;
    }

    if (format == LOCAL_GL_RGBA && type == LOCAL_GL_UNSIGNED_BYTE) {
        for (size_t j = 0; j < height; ++j) {
            uint8_t* row = static_cast<uint8_t*>(data) + j*stride;

            uint8_t* pAlpha = row + 3;
            uint8_t* pAlphaEnd = pAlpha + 4*width;
            while (pAlpha != pAlphaEnd) {
                *pAlpha = 0xff;
                pAlpha += 4;
            }
        }

        return true;
    }

    if (format == LOCAL_GL_RGBA && type == LOCAL_GL_FLOAT) {
        for (size_t j = 0; j < height; ++j) {
            uint8_t* rowBytes = static_cast<uint8_t*>(data) + j*stride;
            float* row = reinterpret_cast<float*>(rowBytes);

            float* pAlpha = row + 3;
            float* pAlphaEnd = pAlpha + 4*width;
            while (pAlpha != pAlphaEnd) {
                *pAlpha = 1.0f;
                pAlpha += 4;
            }
        }

        return true;
    }

    MOZ_ASSERT(false, "Unhandled case, how'd we get here?");
    return false;
}

static void
ReadPixelsAndConvert(gl::GLContext* gl, GLint x, GLint y, GLsizei width, GLsizei height,
                     GLenum readFormat, GLenum readType, size_t pixelStorePackAlignment,
                     GLenum destFormat, GLenum destType, void* destBytes)
{
    if (readFormat == destFormat && readType == destType) {
        gl->fReadPixels(x, y, width, height, destFormat, destType, destBytes);
        return;
    }

    if (readFormat == LOCAL_GL_RGBA &&
        readType == LOCAL_GL_HALF_FLOAT &&
        destFormat == LOCAL_GL_RGBA &&
        destType == LOCAL_GL_FLOAT)
    {
        size_t readBytesPerPixel = sizeof(uint16_t) * 4;
        size_t destBytesPerPixel = sizeof(float) * 4;

        size_t readBytesPerRow = readBytesPerPixel * width;

        size_t readStride = RoundUpToMultipleOf(readBytesPerRow, pixelStorePackAlignment);
        size_t destStride = RoundUpToMultipleOf(destBytesPerPixel * width,
                                                pixelStorePackAlignment);

        size_t bytesNeeded = ((height - 1) * readStride) + readBytesPerRow;
        UniquePtr<uint8_t[]> readBuffer(new uint8_t[bytesNeeded]);

        gl->fReadPixels(x, y, width, height, readFormat, readType, readBuffer.get());

        size_t channelsPerRow = width * 4;
        for (size_t j = 0; j < (size_t)height; j++) {
            uint16_t* src = (uint16_t*)(readBuffer.get()) + j*readStride;
            float* dst = (float*)(destBytes) + j*destStride;

            uint16_t* srcEnd = src + channelsPerRow;
            while (src != srcEnd) {
                *dst = unpackFromFloat16(*src);

                ++src;
                ++dst;
            }
        }

        return;
    }

    MOZ_CRASH("bad format/type");
}

static bool
IsFormatAndTypeUnpackable(GLenum format, GLenum type)
{
    switch (type) {
    case LOCAL_GL_UNSIGNED_BYTE:
    case LOCAL_GL_FLOAT:
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        switch (format) {
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
            return true;
        default:
            return false;
        }

    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        return format == LOCAL_GL_RGBA;

    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        return format == LOCAL_GL_RGB;

    default:
        return false;
    }
}

void
WebGLContext::ReadPixels(GLint x, GLint y, GLsizei width,
                         GLsizei height, GLenum format,
                         GLenum type, const Nullable<ArrayBufferView> &pixels,
                         ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (mCanvasElement->IsWriteOnly() && !nsContentUtils::IsCallerChrome()) {
        GenerateWarning("readPixels: Not allowed");
        return rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    }

    if (width < 0 || height < 0)
        return ErrorInvalidValue("readPixels: negative size passed");

    if (pixels.IsNull())
        return ErrorInvalidValue("readPixels: null destination buffer");

    if (!IsFormatAndTypeUnpackable(format, type))
        return ErrorInvalidEnum("readPixels: Bad format or type.");

    const WebGLRectangleObject* framebufferRect = CurValidReadFBRectObject();
    GLsizei framebufferWidth = framebufferRect ? framebufferRect->Width() : 0;
    GLsizei framebufferHeight = framebufferRect ? framebufferRect->Height() : 0;

    int channels = 0;

    // Check the format param
    switch (format) {
    case LOCAL_GL_ALPHA:
        channels = 1;
        break;
    case LOCAL_GL_RGB:
        channels = 3;
        break;
    case LOCAL_GL_RGBA:
        channels = 4;
        break;
    default:
        MOZ_CRASH("bad `format`");
    }


    // Check the type param
    int bytesPerPixel;
    int requiredDataType;
    switch (type) {
    case LOCAL_GL_UNSIGNED_BYTE:
        bytesPerPixel = 1*channels;
        requiredDataType = js::Scalar::Uint8;
        break;

    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        bytesPerPixel = 2;
        requiredDataType = js::Scalar::Uint16;
        break;

    case LOCAL_GL_FLOAT:
        bytesPerPixel = 4*channels;
        requiredDataType = js::Scalar::Float32;
        break;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        bytesPerPixel = 2*channels;
        requiredDataType = js::Scalar::Uint16;
        break;

    default:
        MOZ_CRASH("bad `type`");
    }

    const ArrayBufferView& pixbuf = pixels.Value();
    int dataType = JS_GetArrayBufferViewType(pixbuf.Obj());

    // Check the pixels param type
    if (dataType != requiredDataType)
        return ErrorInvalidOperation("readPixels: Mismatched type/pixels types");

    // Check the pixels param size
    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, 1, bytesPerPixel, mPixelStorePackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * bytesPerPixel;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize, mPixelStorePackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("readPixels: integer overflow computing the needed buffer size");

    // Compute length and data.  Don't reenter after this point, lest the
    // precomputed go out of sync with the instant length/data.
    pixbuf.ComputeLengthAndData();

    uint32_t dataByteLen = pixbuf.Length();
    if (checked_neededByteLength.value() > dataByteLen)
        return ErrorInvalidOperation("readPixels: buffer too small");

    void* data = pixbuf.Data();
    if (!data) {
        ErrorOutOfMemory("readPixels: buffer storage is null. Did we run out of memory?");
        return rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }

    MakeContextCurrent();

    bool isSourceTypeFloat;
    if (mBoundReadFramebuffer) {
        TexInternalFormat srcFormat;
        if (!mBoundReadFramebuffer->ValidateForRead("readPixels", &srcFormat))
            return;

        MOZ_ASSERT(srcFormat != LOCAL_GL_NONE);
        TexType type = TypeFromInternalFormat(srcFormat);
        isSourceTypeFloat = (type == LOCAL_GL_FLOAT ||
                             type == LOCAL_GL_HALF_FLOAT);
    } else {
        ClearBackbufferIfNeeded();

        isSourceTypeFloat = false;
    }

    // Check the format and type params to assure they are an acceptable pair (as per spec)

    const GLenum mainReadFormat = LOCAL_GL_RGBA;
    const GLenum mainReadType = isSourceTypeFloat ? LOCAL_GL_FLOAT
                                                  : LOCAL_GL_UNSIGNED_BYTE;

    GLenum auxReadFormat = mainReadFormat;
    GLenum auxReadType = mainReadType;

    // OpenGL ES 2.0 $4.3.1 - IMPLEMENTATION_COLOR_READ_{TYPE/FORMAT} is a valid
    // combination for glReadPixels().
    if (gl->IsSupported(gl::GLFeature::ES2_compatibility)) {
        gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT,
                         reinterpret_cast<GLint*>(&auxReadFormat));
        gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE,
                         reinterpret_cast<GLint*>(&auxReadType));
    }

    const bool mainMatches = (format == mainReadFormat && type == mainReadType);
    const bool auxMatches = (format == auxReadFormat && type == auxReadType);
    const bool isValid = mainMatches || auxMatches;
    if (!isValid)
        return ErrorInvalidOperation("readPixels: Invalid format/type pair");

    GLenum readType = type;
    if (gl->WorkAroundDriverBugs() && gl->IsANGLE()) {
        if (type == LOCAL_GL_FLOAT &&
            auxReadFormat == format &&
            auxReadType == LOCAL_GL_HALF_FLOAT)
        {
            readType = auxReadType;
        }
    }

    // Now that the errors are out of the way, on to actually reading

    // If we won't be reading any pixels anyways, just skip the actual reading
    if (width == 0 || height == 0)
        return DummyFramebufferOperation("readPixels");

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, framebufferWidth, framebufferHeight)) {
        // the easy case: we're not reading out-of-range pixels

        // Effectively: gl->fReadPixels(x, y, width, height, format, type, dest);
        ReadPixelsAndConvert(gl, x, y, width, height, format, readType,
                             mPixelStorePackAlignment, format, type, data);
    } else {
        // the rectangle doesn't fit entirely in the bound buffer. We then have to set to zero the part
        // of the buffer that correspond to out-of-range pixels. We don't want to rely on system OpenGL
        // to do that for us, because passing out of range parameters to a buggy OpenGL implementation
        // could conceivably allow to read memory we shouldn't be allowed to read. So we manually initialize
        // the buffer to zero and compute the parameters to pass to OpenGL. We have to use an intermediate buffer
        // to accomodate the potentially different strides (widths).

        // Zero the whole pixel dest area in the destination buffer.
        memset(data, 0, checked_neededByteLength.value());

        if (   x >= framebufferWidth
            || x+width <= 0
            || y >= framebufferHeight
            || y+height <= 0)
        {
            // we are completely outside of range, can exit now with buffer filled with zeros
            return DummyFramebufferOperation("readPixels");
        }

        // compute the parameters of the subrect we're actually going to call glReadPixels on
        GLint   subrect_x      = std::max(x, 0);
        GLint   subrect_end_x  = std::min(x+width, framebufferWidth);
        GLsizei subrect_width  = subrect_end_x - subrect_x;

        GLint   subrect_y      = std::max(y, 0);
        GLint   subrect_end_y  = std::min(y+height, framebufferHeight);
        GLsizei subrect_height = subrect_end_y - subrect_y;

        if (subrect_width < 0 || subrect_height < 0 ||
            subrect_width > width || subrect_height > height)
            return ErrorInvalidOperation("readPixels: integer overflow computing clipped rect size");

        // now we know that subrect_width is in the [0..width] interval, and same for heights.

        // now, same computation as above to find the size of the intermediate buffer to allocate for the subrect
        // no need to check again for integer overflow here, since we already know the sizes aren't greater than before
        uint32_t subrect_plainRowSize = subrect_width * bytesPerPixel;
    // There are checks above to ensure that this doesn't overflow.
        uint32_t subrect_alignedRowSize =
            RoundedToNextMultipleOf(subrect_plainRowSize, mPixelStorePackAlignment).value();
        uint32_t subrect_byteLength = (subrect_height-1)*subrect_alignedRowSize + subrect_plainRowSize;

        // create subrect buffer, call glReadPixels, copy pixels into destination buffer, delete subrect buffer
        UniquePtr<GLubyte> subrect_data(new (fallible) GLubyte[subrect_byteLength]);
        if (!subrect_data)
            return ErrorOutOfMemory("readPixels: subrect_data");

        // Effectively: gl->fReadPixels(subrect_x, subrect_y, subrect_width,
        //                              subrect_height, format, type, subrect_data.get());
        ReadPixelsAndConvert(gl, subrect_x, subrect_y, subrect_width, subrect_height,
                             format, readType, mPixelStorePackAlignment, format, type,
                             subrect_data.get());

        // notice that this for loop terminates because we already checked that subrect_height is at most height
        for (GLint y_inside_subrect = 0; y_inside_subrect < subrect_height; ++y_inside_subrect) {
            GLint subrect_x_in_dest_buffer = subrect_x - x;
            GLint subrect_y_in_dest_buffer = subrect_y - y;
            memcpy(static_cast<GLubyte*>(data)
                     + checked_alignedRowSize.value() * (subrect_y_in_dest_buffer + y_inside_subrect)
                     + bytesPerPixel * subrect_x_in_dest_buffer, // destination
                   subrect_data.get() + subrect_alignedRowSize * y_inside_subrect, // source
                   subrect_plainRowSize); // size
        }
    }

    // if we're reading alpha, we may need to do fixup.  Note that we don't allow
    // GL_ALPHA to readpixels currently, but we had the code written for it already.
    const bool formatHasAlpha = format == LOCAL_GL_ALPHA ||
                                format == LOCAL_GL_RGBA;
    if (!formatHasAlpha)
        return;

    bool needAlphaFilled;
    if (mBoundReadFramebuffer) {
        needAlphaFilled = !mBoundReadFramebuffer->ColorAttachment(0).HasAlpha();
    } else {
        needAlphaFilled = !mOptions.alpha;
    }

    if (!needAlphaFilled)
        return;

    size_t stride = checked_alignedRowSize.value(); // In bytes!
    if (!SetFullAlpha(data, format, type, width, height, stride)) {
        return rv.Throw(NS_ERROR_FAILURE);
    }
}

void
WebGLContext::RenderbufferStorage_base(const char* funcName, GLenum target,
                                       GLsizei samples,
                                       GLenum internalFormat, GLsizei width,
                                       GLsizei height)
{
    if (IsContextLost())
        return;

    if (!mBoundRenderbuffer) {
        ErrorInvalidOperation("%s: Called on renderbuffer 0.", funcName);
        return;
    }

    if (target != LOCAL_GL_RENDERBUFFER) {
        ErrorInvalidEnumInfo("`target`", funcName, target);
        return;
    }

    if (samples < 0 || samples > mGLMaxSamples) {
        ErrorInvalidValue("%s: `samples` is out of the valid range.", funcName);
        return;
    }

    if (width < 0 || height < 0) {
        ErrorInvalidValue("%s: Width and height must be >= 0.", funcName);
        return;
    }

    if (width > mGLMaxRenderbufferSize || height > mGLMaxRenderbufferSize) {
        ErrorInvalidValue("%s: Width or height exceeds maximum renderbuffer"
                          " size.", funcName);
        return;
    }

    bool isFormatValid = false;
    switch (internalFormat) {
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
    case LOCAL_GL_RGB565:
    case LOCAL_GL_DEPTH_COMPONENT16:
    case LOCAL_GL_STENCIL_INDEX8:
    case LOCAL_GL_DEPTH_STENCIL:
        isFormatValid = true;
        break;

    case LOCAL_GL_SRGB8_ALPHA8_EXT:
        if (IsExtensionEnabled(WebGLExtensionID::EXT_sRGB))
            isFormatValid = true;
        break;

    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGBA16F:
        if (IsExtensionEnabled(WebGLExtensionID::OES_texture_half_float) &&
            IsExtensionEnabled(WebGLExtensionID::EXT_color_buffer_half_float))
        {
            isFormatValid = true;
        }
        break;

    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGBA32F:
        if (IsExtensionEnabled(WebGLExtensionID::OES_texture_float) &&
            IsExtensionEnabled(WebGLExtensionID::WEBGL_color_buffer_float))
        {
            isFormatValid = true;
        }
        break;

    default:
        break;
    }

    if (!isFormatValid) {
        ErrorInvalidEnumInfo("`internalFormat`", funcName, internalFormat);
        return;
    }

    // certain OpenGL ES renderbuffer formats may not exist on desktop OpenGL
    GLenum internalFormatForGL = internalFormat;

    switch (internalFormat) {
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
        // 16-bit RGBA formats are not supported on desktop GL
        if (!gl->IsGLES())
            internalFormatForGL = LOCAL_GL_RGBA8;
        break;

    case LOCAL_GL_RGB565:
        // the RGB565 format is not supported on desktop GL
        if (!gl->IsGLES())
            internalFormatForGL = LOCAL_GL_RGB8;
        break;

    case LOCAL_GL_DEPTH_COMPONENT16:
        if (!gl->IsGLES() || gl->IsExtensionSupported(gl::GLContext::OES_depth24))
            internalFormatForGL = LOCAL_GL_DEPTH_COMPONENT24;
        else if (gl->IsExtensionSupported(gl::GLContext::OES_packed_depth_stencil))
            internalFormatForGL = LOCAL_GL_DEPTH24_STENCIL8;
        break;

    case LOCAL_GL_DEPTH_STENCIL:
        // We emulate this in WebGLRenderbuffer if we don't have the requisite extension.
        internalFormatForGL = LOCAL_GL_DEPTH24_STENCIL8;
        break;

    default:
        break;
    }

    // Validation complete.

    MakeContextCurrent();

    bool willRealloc = samples != mBoundRenderbuffer->Samples() ||
                       internalFormat != mBoundRenderbuffer->InternalFormat() ||
                       width != mBoundRenderbuffer->Width() ||
                       height != mBoundRenderbuffer->Height();

    if (willRealloc) {
        GetAndFlushUnderlyingGLErrors();
        mBoundRenderbuffer->RenderbufferStorage(samples, internalFormatForGL,
                                                width, height);
        GLenum error = GetAndFlushUnderlyingGLErrors();
        if (error) {
            GenerateWarning("%s generated error %s", funcName,
                            ErrorName(error));
            return;
        }
    } else {
        mBoundRenderbuffer->RenderbufferStorage(samples, internalFormatForGL,
                                                width, height);
    }

    mBoundRenderbuffer->SetSamples(samples);
    mBoundRenderbuffer->SetInternalFormat(internalFormat);
    mBoundRenderbuffer->SetInternalFormatForGL(internalFormatForGL);
    mBoundRenderbuffer->setDimensions(width, height);
    mBoundRenderbuffer->SetImageDataStatus(WebGLImageDataStatus::UninitializedImageData);
}

void
WebGLContext::RenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height)
{
    RenderbufferStorage_base("renderbufferStorage", target, 0,
                             internalFormat, width, height);
}

void
WebGLContext::Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    if (width < 0 || height < 0)
        return ErrorInvalidValue("scissor: negative size");

    MakeContextCurrent();
    gl->fScissor(x, y, width, height);
}

void
WebGLContext::StencilFunc(GLenum func, GLint ref, GLuint mask)
{
    if (IsContextLost())
        return;

    if (!ValidateComparisonEnum(func, "stencilFunc: func"))
        return;

    mStencilRefFront = ref;
    mStencilRefBack = ref;
    mStencilValueMaskFront = mask;
    mStencilValueMaskBack = mask;

    MakeContextCurrent();
    gl->fStencilFunc(func, ref, mask);
}

void
WebGLContext::StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face, "stencilFuncSeparate: face") ||
        !ValidateComparisonEnum(func, "stencilFuncSeparate: func"))
        return;

    switch (face) {
        case LOCAL_GL_FRONT_AND_BACK:
            mStencilRefFront = ref;
            mStencilRefBack = ref;
            mStencilValueMaskFront = mask;
            mStencilValueMaskBack = mask;
            break;
        case LOCAL_GL_FRONT:
            mStencilRefFront = ref;
            mStencilValueMaskFront = mask;
            break;
        case LOCAL_GL_BACK:
            mStencilRefBack = ref;
            mStencilValueMaskBack = mask;
            break;
    }

    MakeContextCurrent();
    gl->fStencilFuncSeparate(face, func, ref, mask);
}

void
WebGLContext::StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass)
{
    if (IsContextLost())
        return;

    if (!ValidateStencilOpEnum(sfail, "stencilOp: sfail") ||
        !ValidateStencilOpEnum(dpfail, "stencilOp: dpfail") ||
        !ValidateStencilOpEnum(dppass, "stencilOp: dppass"))
        return;

    MakeContextCurrent();
    gl->fStencilOp(sfail, dpfail, dppass);
}

void
WebGLContext::StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face, "stencilOpSeparate: face") ||
        !ValidateStencilOpEnum(sfail, "stencilOpSeparate: sfail") ||
        !ValidateStencilOpEnum(dpfail, "stencilOpSeparate: dpfail") ||
        !ValidateStencilOpEnum(dppass, "stencilOpSeparate: dppass"))
        return;

    MakeContextCurrent();
    gl->fStencilOpSeparate(face, sfail, dpfail, dppass);
}

nsresult
WebGLContext::SurfaceFromElementResultToImageSurface(nsLayoutUtils::SurfaceFromElementResult& res,
                                                     RefPtr<DataSourceSurface>& imageOut,
                                                     WebGLTexelFormat* format)
{
   *format = WebGLTexelFormat::None;

    if (!res.mSourceSurface)
        return NS_OK;
    RefPtr<DataSourceSurface> data = res.mSourceSurface->GetDataSurface();
    if (!data) {
        // SurfaceFromElement lied!
        return NS_OK;
    }

    if (!mPixelStorePremultiplyAlpha && res.mIsPremultiplied) {
        switch (data->GetFormat()) {
        case SurfaceFormat::B8G8R8X8:
            // No alpha, so de-facto premult'd.
            break;
        case SurfaceFormat::B8G8R8A8:
            data = gfxUtils::CreateUnpremultipliedDataSurface(data);
            break;
        default:
            MOZ_ASSERT(false, "Format unsupported.");
            break;
        }
    }

    // We disallow loading cross-domain images and videos that have not been validated
    // with CORS as WebGL textures. The reason for doing that is that timing
    // attacks on WebGL shaders are able to retrieve approximations of the
    // pixel values in WebGL textures; see bug 655987.
    //
    // To prevent a loophole where a Canvas2D would be used as a proxy to load
    // cross-domain textures, we also disallow loading textures from write-only
    // Canvas2D's.

    // part 1: check that the DOM element is same-origin, or has otherwise been
    // validated for cross-domain use.
    if (!res.mCORSUsed) {
        bool subsumes;
        nsresult rv = mCanvasElement->NodePrincipal()->Subsumes(res.mPrincipal, &subsumes);
        if (NS_FAILED(rv) || !subsumes) {
            GenerateWarning("It is forbidden to load a WebGL texture from a cross-domain element that has not been validated with CORS. "
                                "See https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures");
            return NS_ERROR_DOM_SECURITY_ERR;
        }
    }

    // part 2: if the DOM element is write-only, it might contain
    // cross-domain image data.
    if (res.mIsWriteOnly) {
        GenerateWarning("The canvas used as source for texImage2D here is tainted (write-only). It is forbidden "
                        "to load a WebGL texture from a tainted canvas. A Canvas becomes tainted for example "
                        "when a cross-domain image is drawn on it. "
                        "See https://developer.mozilla.org/en/WebGL/Cross-Domain_Textures");
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    // End of security checks, now we should be safe regarding cross-domain images
    // Notice that there is never a need to mark the WebGL canvas as write-only, since we reject write-only/cross-domain
    // texture sources in the first place.

    switch (data->GetFormat()) {
        case SurfaceFormat::B8G8R8A8:
            *format = WebGLTexelFormat::BGRA8; // careful, our ARGB means BGRA
            break;
        case SurfaceFormat::B8G8R8X8:
            *format = WebGLTexelFormat::BGRX8; // careful, our RGB24 is not tightly packed. Whence BGRX8.
            break;
        case SurfaceFormat::A8:
            *format = WebGLTexelFormat::A8;
            break;
        case SurfaceFormat::R5G6B5:
            *format = WebGLTexelFormat::RGB565;
            break;
        default:
            NS_ASSERTION(false, "Unsupported image format. Unimplemented.");
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    imageOut = data;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Uniform setters.

void
WebGLContext::Uniform1i(WebGLUniformLocation* loc, GLint a1)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_INT, "uniform1i", &rawLoc))
        return;

    // Only uniform1i can take sampler settings.
    if (!loc->ValidateSamplerSetter(a1, this, "uniform1i"))
        return;

    MakeContextCurrent();
    gl->fUniform1i(rawLoc, a1);
}

void
WebGLContext::Uniform2i(WebGLUniformLocation* loc, GLint a1, GLint a2)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_INT, "uniform2i", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform2i(rawLoc, a1, a2);
}

void
WebGLContext::Uniform3i(WebGLUniformLocation* loc, GLint a1, GLint a2, GLint a3)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_INT, "uniform3i", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform3i(rawLoc, a1, a2, a3);
}

void
WebGLContext::Uniform4i(WebGLUniformLocation* loc, GLint a1, GLint a2, GLint a3,
                        GLint a4)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_INT, "uniform4i", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform4i(rawLoc, a1, a2, a3, a4);
}

void
WebGLContext::Uniform1f(WebGLUniformLocation* loc, GLfloat a1)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_FLOAT, "uniform1f", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform1f(rawLoc, a1);
}

void
WebGLContext::Uniform2f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_FLOAT, "uniform2f", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform2f(rawLoc, a1, a2);
}

void
WebGLContext::Uniform3f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2,
                        GLfloat a3)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_FLOAT, "uniform3f", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform3f(rawLoc, a1, a2, a3);
}

void
WebGLContext::Uniform4f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2,
                        GLfloat a3, GLfloat a4)
{
    GLuint rawLoc;
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_FLOAT, "uniform4f", &rawLoc))
        return;

    MakeContextCurrent();
    gl->fUniform4f(rawLoc, a1, a2, a3, a4);
}

////////////////////////////////////////
// Array

void
WebGLContext::Uniform1iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 1, LOCAL_GL_INT, arrayLength,
                                    "uniform1iv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    if (!loc->ValidateSamplerSetter(data[0], this, "uniform1iv"))
        return;

    MakeContextCurrent();
    gl->fUniform1iv(rawLoc, numElementsToUpload, data);
}

void
WebGLContext::Uniform2iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 2, LOCAL_GL_INT, arrayLength,
                                    "uniform2iv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    if (!loc->ValidateSamplerSetter(data[0], this, "uniform2iv") ||
        !loc->ValidateSamplerSetter(data[1], this, "uniform2iv"))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform2iv(rawLoc, numElementsToUpload, data);
}

void
WebGLContext::Uniform3iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 3, LOCAL_GL_INT, arrayLength,
                                    "uniform3iv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    if (!loc->ValidateSamplerSetter(data[0], this, "uniform3iv") ||
        !loc->ValidateSamplerSetter(data[1], this, "uniform3iv") ||
        !loc->ValidateSamplerSetter(data[2], this, "uniform3iv"))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform3iv(rawLoc, numElementsToUpload, data);
}

void
WebGLContext::Uniform4iv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLint* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 4, LOCAL_GL_INT, arrayLength,
                                    "uniform4iv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    if (!loc->ValidateSamplerSetter(data[0], this, "uniform4iv") ||
        !loc->ValidateSamplerSetter(data[1], this, "uniform4iv") ||
        !loc->ValidateSamplerSetter(data[2], this, "uniform4iv") ||
        !loc->ValidateSamplerSetter(data[3], this, "uniform4iv"))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform4iv(rawLoc, numElementsToUpload, data);
}

void
WebGLContext::Uniform1fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 1, LOCAL_GL_FLOAT, arrayLength,
                                    "uniform1fv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform1fv(rawLoc, numElementsToUpload, data);
}

void
WebGLContext::Uniform2fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 2, LOCAL_GL_FLOAT, arrayLength,
                                    "uniform2fv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform2fv(rawLoc, numElementsToUpload, data);
}

void
WebGLContext::Uniform3fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 3, LOCAL_GL_FLOAT, arrayLength,
                                    "uniform3fv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform3fv(rawLoc, numElementsToUpload, data);
}

void
WebGLContext::Uniform4fv_base(WebGLUniformLocation* loc, size_t arrayLength,
                              const GLfloat* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, 4, LOCAL_GL_FLOAT, arrayLength,
                                    "uniform4fv", &rawLoc,
                                    &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform4fv(rawLoc, numElementsToUpload, data);
}

////////////////////////////////////////
// Matrix

void
WebGLContext::UniformMatrix2fv_base(WebGLUniformLocation* loc, bool transpose,
                                    size_t arrayLength, const float* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformMatrixArraySetter(loc, 2, 2, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix2fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix2fv(rawLoc, numElementsToUpload, false, data);
}

void
WebGLContext::UniformMatrix3fv_base(WebGLUniformLocation* loc, bool transpose,
                                    size_t arrayLength, const float* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformMatrixArraySetter(loc, 3, 3, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix3fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix3fv(rawLoc, numElementsToUpload, false, data);
}

void
WebGLContext::UniformMatrix4fv_base(WebGLUniformLocation* loc, bool transpose,
                                    size_t arrayLength, const float* data)
{
    GLuint rawLoc;
    GLsizei numElementsToUpload;
    if (!ValidateUniformMatrixArraySetter(loc, 4, 4, LOCAL_GL_FLOAT, arrayLength,
                                          transpose, "uniformMatrix4fv",
                                          &rawLoc, &numElementsToUpload))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniformMatrix4fv(rawLoc, numElementsToUpload, false, data);
}

////////////////////////////////////////////////////////////////////////////////

void
WebGLContext::UseProgram(WebGLProgram* prog)
{
    if (IsContextLost())
        return;

    if (!prog) {
        mCurrentProgram = nullptr;
        mActiveProgramLinkInfo = nullptr;
        return;
    }

    if (!ValidateObject("useProgram", prog))
        return;

    if (prog->UseProgram()) {
        mCurrentProgram = prog;
        mActiveProgramLinkInfo = mCurrentProgram->LinkInfo();
    }
}

void
WebGLContext::ValidateProgram(WebGLProgram* prog)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("validateProgram", prog))
        return;

    prog->ValidateProgram();
}

already_AddRefed<WebGLFramebuffer>
WebGLContext::CreateFramebuffer()
{
    if (IsContextLost())
        return nullptr;

    GLuint fbo = 0;
    MakeContextCurrent();
    gl->fGenFramebuffers(1, &fbo);

    nsRefPtr<WebGLFramebuffer> globj = new WebGLFramebuffer(this, fbo);
    return globj.forget();
}

already_AddRefed<WebGLRenderbuffer>
WebGLContext::CreateRenderbuffer()
{
    if (IsContextLost())
        return nullptr;
    nsRefPtr<WebGLRenderbuffer> globj = new WebGLRenderbuffer(this);
    return globj.forget();
}

void
WebGLContext::Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    if (width < 0 || height < 0)
        return ErrorInvalidValue("viewport: negative size");

    MakeContextCurrent();
    gl->fViewport(x, y, width, height);

    mViewportX = x;
    mViewportY = y;
    mViewportWidth = width;
    mViewportHeight = height;
}

void
WebGLContext::CompileShader(WebGLShader* shader)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("compileShader", shader))
        return;

    shader->CompileShader();
}

void
WebGLContext::CompressedTexImage2D(GLenum rawTexImgTarget,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width, GLsizei height, GLint border,
                                   const ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (!ValidateTexImageTarget(rawTexImgTarget, func, dims))
        return;

    if (!ValidateTexImage(rawTexImgTarget, level, internalformat,
                          0, 0, 0, width, height, 0,
                          border, LOCAL_GL_NONE,
                          LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    view.ComputeLengthAndData();

    uint32_t byteLength = view.Length();
    if (!ValidateCompTexImageDataSize(level, internalformat, width, height, byteLength, func, dims)) {
        return;
    }

    if (!ValidateCompTexImageSize(level, internalformat, 0, 0, width, height, width, height, func, dims))
    {
        return;
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    MOZ_ASSERT(tex);
    if (tex->IsImmutable()) {
        return ErrorInvalidOperation(
            "compressedTexImage2D: disallowed because the texture bound to "
            "this target has already been made immutable by texStorage2D");
    }

    MakeContextCurrent();
    gl->fCompressedTexImage2D(texImageTarget.get(), level, internalformat, width, height, border, byteLength, view.Data());

    tex->SetImageInfo(texImageTarget, level, width, height, 1, internalformat,
                      WebGLImageDataStatus::InitializedImageData);
}

void
WebGLContext::CompressedTexSubImage2D(GLenum rawTexImgTarget, GLint level, GLint xoffset,
                                      GLint yoffset, GLsizei width, GLsizei height,
                                      GLenum internalformat,
                                      const ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (!ValidateTexImageTarget(rawTexImgTarget, func, dims))
        return;

    if (!ValidateTexImage(rawTexImgTarget,
                          level, internalformat,
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0, LOCAL_GL_NONE, LOCAL_GL_NONE,
                          func, dims))
    {
        return;
    }

    const TexImageTarget texImageTarget(rawTexImgTarget);

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    MOZ_ASSERT(tex);
    WebGLTexture::ImageInfo& levelInfo = tex->ImageInfoAt(texImageTarget, level);

    if (internalformat != levelInfo.EffectiveInternalFormat()) {
        return ErrorInvalidOperation("compressedTexImage2D: internalformat does not match the existing image");
    }

    view.ComputeLengthAndData();

    uint32_t byteLength = view.Length();
    if (!ValidateCompTexImageDataSize(level, internalformat, width, height, byteLength, func, dims))
        return;

    if (!ValidateCompTexImageSize(level, internalformat,
                                  xoffset, yoffset,
                                  width, height,
                                  levelInfo.Width(), levelInfo.Height(),
                                  func, dims))
    {
        return;
    }

    if (levelInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                width == levelInfo.Width() &&
                                height == levelInfo.Height();
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            tex->EnsureNoUninitializedImageData(texImageTarget, level);
        }
    }

    MakeContextCurrent();
    gl->fCompressedTexSubImage2D(texImageTarget.get(), level, xoffset, yoffset, width, height, internalformat, byteLength, view.Data());
}

JS::Value
WebGLContext::GetShaderParameter(WebGLShader* shader, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObject("getShaderParameter: shader", shader))
        return JS::NullValue();

    return shader->GetShaderParameter(pname);
}

void
WebGLContext::GetShaderInfoLog(WebGLShader* shader, nsAString& retval)
{
    retval.SetIsVoid(true);

    if (IsContextLost())
        return;

    if (!ValidateObject("getShaderInfoLog: shader", shader))
        return;

    shader->GetShaderInfoLog(&retval);

    retval.SetIsVoid(false);
}

already_AddRefed<WebGLShaderPrecisionFormat>
WebGLContext::GetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype)
{
    if (IsContextLost())
        return nullptr;

    switch (shadertype) {
        case LOCAL_GL_FRAGMENT_SHADER:
        case LOCAL_GL_VERTEX_SHADER:
            break;
        default:
            ErrorInvalidEnumInfo("getShaderPrecisionFormat: shadertype", shadertype);
            return nullptr;
    }

    switch (precisiontype) {
        case LOCAL_GL_LOW_FLOAT:
        case LOCAL_GL_MEDIUM_FLOAT:
        case LOCAL_GL_HIGH_FLOAT:
        case LOCAL_GL_LOW_INT:
        case LOCAL_GL_MEDIUM_INT:
        case LOCAL_GL_HIGH_INT:
            break;
        default:
            ErrorInvalidEnumInfo("getShaderPrecisionFormat: precisiontype", precisiontype);
            return nullptr;
    }

    MakeContextCurrent();
    GLint range[2], precision;

    if (mDisableFragHighP &&
        shadertype == LOCAL_GL_FRAGMENT_SHADER &&
        (precisiontype == LOCAL_GL_HIGH_FLOAT ||
         precisiontype == LOCAL_GL_HIGH_INT))
    {
      precision = 0;
      range[0] = 0;
      range[1] = 0;
    } else {
      gl->fGetShaderPrecisionFormat(shadertype, precisiontype, range, &precision);
    }

    nsRefPtr<WebGLShaderPrecisionFormat> retShaderPrecisionFormat
        = new WebGLShaderPrecisionFormat(this, range[0], range[1], precision);
    return retShaderPrecisionFormat.forget();
}

void
WebGLContext::GetShaderSource(WebGLShader* shader, nsAString& retval)
{
    retval.SetIsVoid(true);

    if (IsContextLost())
        return;

    if (!ValidateObject("getShaderSource: shader", shader))
        return;

    shader->GetShaderSource(&retval);
}

void
WebGLContext::ShaderSource(WebGLShader* shader, const nsAString& source)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("shaderSource: shader", shader))
        return;

    shader->ShaderSource(source);
}

void
WebGLContext::GetShaderTranslatedSource(WebGLShader* shader, nsAString& retval)
{
    retval.SetIsVoid(true);

    if (IsContextLost())
        return;

    if (!ValidateObject("getShaderTranslatedSource: shader", shader))
        return;

    shader->GetShaderTranslatedSource(&retval);
}

GLenum WebGLContext::CheckedTexImage2D(TexImageTarget texImageTarget,
                                       GLint level,
                                       TexInternalFormat internalformat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       TexFormat format,
                                       TexType type,
                                       const GLvoid* data)
{
    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    MOZ_ASSERT(tex, "no texture bound");

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalformat, type);
    bool sizeMayChange = true;

    if (tex->HasImageInfoAt(texImageTarget, level)) {
        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        effectiveInternalFormat != imageInfo.EffectiveInternalFormat();
    }

    // Convert to format and type required by OpenGL 'driver'.
    GLenum driverType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             effectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverFormat,
                                             &driverType);

    if (sizeMayChange) {
        GetAndFlushUnderlyingGLErrors();
    }

    gl->fTexImage2D(texImageTarget.get(), level, driverInternalFormat, width, height, border, driverFormat, driverType, data);

    GLenum error = LOCAL_GL_NO_ERROR;
    if (sizeMayChange) {
        error = GetAndFlushUnderlyingGLErrors();
    }

    return error;
}

void
WebGLContext::TexImage2D_base(TexImageTarget texImageTarget, GLint level,
                              GLenum internalformat,
                              GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              void* data, uint32_t byteLength,
                              js::Scalar::Type jsArrayType,
                              WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (type == LOCAL_GL_HALF_FLOAT_OES) {
        type = LOCAL_GL_HALF_FLOAT;
    }

    if (!ValidateTexImage(texImageTarget, level, internalformat,
                          0, 0, 0,
                          width, height, 0,
                          border, format, type, func, dims))
    {
        return;
    }

    const bool isDepthTexture = format == LOCAL_GL_DEPTH_COMPONENT ||
                                format == LOCAL_GL_DEPTH_STENCIL;

    if (isDepthTexture && !IsWebGL2()) {
        if (data != nullptr || level != 0)
            return ErrorInvalidOperation("texImage2D: "
                                         "with format of DEPTH_COMPONENT or DEPTH_STENCIL, "
                                         "data must be nullptr, "
                                         "level must be zero");
    }

    if (!ValidateTexInputData(type, jsArrayType, func, dims))
        return;

    TexInternalFormat effectiveInternalFormat =
        EffectiveInternalFormatFromInternalFormatAndType(internalformat, type);

    if (effectiveInternalFormat == LOCAL_GL_NONE) {
        return ErrorInvalidOperation("texImage2D: bad combination of internalformat and type");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        // we need to find the exact sized format of the source data. Slightly abusing
        // EffectiveInternalFormatFromInternalFormatAndType for that purpose. Really, an unsized source format
        // is the same thing as an unsized internalformat.
        TexInternalFormat effectiveSourceFormat =
            EffectiveInternalFormatFromInternalFormatAndType(format, type);
        MOZ_ASSERT(effectiveSourceFormat != LOCAL_GL_NONE); // should have validated format/type combo earlier
        const size_t srcbitsPerTexel = GetBitsPerTexel(effectiveSourceFormat);
        MOZ_ASSERT((srcbitsPerTexel % 8) == 0); // should not have compressed formats here.
        srcTexelSize = srcbitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, 1, srcTexelSize, mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;
    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("texImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength && byteLength < bytesNeeded)
        return ErrorInvalidOperation("texImage2D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, byteLength);

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);

    if (!tex)
        return ErrorInvalidOperation("texImage2D: no texture is bound to this target");

    if (tex->IsImmutable()) {
        return ErrorInvalidOperation(
            "texImage2D: disallowed because the texture "
            "bound to this target has already been made immutable by texStorage2D");
    }
    MakeContextCurrent();

    nsAutoArrayPtr<uint8_t> convertedData;
    void* pixels = nullptr;
    WebGLImageDataStatus imageInfoStatusIfSuccess = WebGLImageDataStatus::UninitializedImageData;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(effectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    if (byteLength) {
        size_t   bitsPerTexel = GetBitsPerTexel(effectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
        size_t   dstTexelSize = bitsPerTexel / 8;
        size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
        size_t   dstPlainRowSize = dstTexelSize * width;
        size_t   unpackAlignment = mPixelStoreUnpackAlignment;
        size_t   dstStride = ((dstPlainRowSize + unpackAlignment-1) / unpackAlignment) * unpackAlignment;

        if (actualSrcFormat == dstFormat &&
            srcPremultiplied == mPixelStorePremultiplyAlpha &&
            srcStride == dstStride &&
            !mPixelStoreFlipY)
        {
            // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
            pixels = data;
        }
        else
        {
            size_t convertedDataSize = height * dstStride;
            convertedData = new (fallible) uint8_t[convertedDataSize];
            if (!convertedData) {
                ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                                 " a buffer for doing format conversion.");
                return;
            }
            if (!ConvertImage(width, height, srcStride, dstStride,
                              static_cast<uint8_t*>(data), convertedData,
                              actualSrcFormat, srcPremultiplied,
                              dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize))
            {
                return ErrorInvalidOperation("texImage2D: Unsupported texture format conversion");
            }
            pixels = reinterpret_cast<void*>(convertedData.get());
        }
        imageInfoStatusIfSuccess = WebGLImageDataStatus::InitializedImageData;
    }

    GLenum error = CheckedTexImage2D(texImageTarget, level, internalformat, width,
                                     height, border, format, type, pixels);

    if (error) {
        GenerateWarning("texImage2D generated error %s", ErrorName(error));
        return;
    }

    // in all of the code paths above, we should have either initialized data,
    // or allocated data and left it uninitialized, but in any case we shouldn't
    // have NoImageData at this point.
    MOZ_ASSERT(imageInfoStatusIfSuccess != WebGLImageDataStatus::NoImageData);

    tex->SetImageInfo(texImageTarget, level, width, height, 1,
                      effectiveInternalFormat, imageInfoStatusIfSuccess);
}

void
WebGLContext::TexImage2D(GLenum rawTarget, GLint level,
                         GLenum internalformat, GLsizei width,
                         GLsizei height, GLint border, GLenum format,
                         GLenum type, const Nullable<ArrayBufferView> &pixels, ErrorResult& rv)
{
    if (IsContextLost())
        return;

    void* data;
    uint32_t length;
    js::Scalar::Type jsArrayType;
    if (pixels.IsNull()) {
        data = nullptr;
        length = 0;
        jsArrayType = js::Scalar::MaxTypedArrayViewType;
    } else {
        const ArrayBufferView& view = pixels.Value();
        view.ComputeLengthAndData();

        data = view.Data();
        length = view.Length();
        jsArrayType = JS_GetArrayBufferViewType(view.Obj());
    }

    if (!ValidateTexImageTarget(rawTarget, WebGLTexImageFunc::TexImage, WebGLTexDimensions::Tex2D))
        return;

    return TexImage2D_base(rawTarget, level, internalformat, width, height, 0, border, format, type,
                           data, length, jsArrayType,
                           WebGLTexelFormat::Auto, false);
}

void
WebGLContext::TexImage2D(GLenum rawTarget, GLint level,
                         GLenum internalformat, GLenum format,
                         GLenum type, ImageData* pixels, ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (!pixels) {
        // Spec says to generate an INVALID_VALUE error
        return ErrorInvalidValue("texImage2D: null ImageData");
    }

    Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(pixels->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    void* pixelData = arr.Data();
    const uint32_t pixelDataLength = arr.Length();

    if (!ValidateTexImageTarget(rawTarget, WebGLTexImageFunc::TexImage, WebGLTexDimensions::Tex2D))
        return;

    return TexImage2D_base(rawTarget, level, internalformat, pixels->Width(),
                           pixels->Height(), 4*pixels->Width(), 0,
                           format, type, pixelData, pixelDataLength, js::Scalar::MaxTypedArrayViewType,
                           WebGLTexelFormat::RGBA8, false);
}


void
WebGLContext::TexSubImage2D_base(GLenum rawImageTarget, GLint level,
                                 GLint xoffset, GLint yoffset,
                                 GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                                 GLenum format, GLenum type,
                                 void* data, uint32_t byteLength,
                                 js::Scalar::Type jsArrayType,
                                 WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;
    const WebGLTexDimensions dims = WebGLTexDimensions::Tex2D;

    if (type == LOCAL_GL_HALF_FLOAT_OES)
        type = LOCAL_GL_HALF_FLOAT;

    if (!ValidateTexImageTarget(rawImageTarget, func, dims))
        return;

    TexImageTarget texImageTarget(rawImageTarget);

    WebGLTexture* tex = ActiveBoundTextureForTexImageTarget(texImageTarget);
    if (!tex)
        return ErrorInvalidOperation("texSubImage2D: no texture bound on active texture unit");

    if (!tex->HasImageInfoAt(texImageTarget, level))
        return ErrorInvalidOperation("texSubImage2D: no previously defined texture image");

    const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(texImageTarget, level);
    const TexInternalFormat existingEffectiveInternalFormat = imageInfo.EffectiveInternalFormat();

    if (!ValidateTexImage(texImageTarget, level,
                          existingEffectiveInternalFormat.get(),
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0, format, type, func, dims))
    {
        return;
    }

    if (!ValidateTexInputData(type, jsArrayType, func, dims))
        return;

    if (type != TypeFromInternalFormat(existingEffectiveInternalFormat)) {
        return ErrorInvalidOperation("texSubImage2D: type differs from that of the existing image");
    }

    size_t srcTexelSize = size_t(-1);
    if (srcFormat == WebGLTexelFormat::Auto) {
        const size_t bitsPerTexel = GetBitsPerTexel(existingEffectiveInternalFormat);
        MOZ_ASSERT((bitsPerTexel % 8) == 0); // should not have compressed formats here.
        srcTexelSize = bitsPerTexel / 8;
    } else {
        srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(srcFormat);
    }

    if (width == 0 || height == 0)
        return; // ES 2.0 says it has no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, 1, srcTexelSize, mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength < bytesNeeded)
        return ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, byteLength);

    if (imageInfo.HasUninitializedImageData()) {
        bool coversWholeImage = xoffset == 0 &&
                                yoffset == 0 &&
                                width == imageInfo.Width() &&
                                height == imageInfo.Height();
        if (coversWholeImage) {
            tex->SetImageDataStatus(texImageTarget, level, WebGLImageDataStatus::InitializedImageData);
        } else {
            tex->EnsureNoUninitializedImageData(texImageTarget, level);
        }
    }
    MakeContextCurrent();

    size_t   srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();
    uint32_t dstTexelSize = GetBitsPerTexel(existingEffectiveInternalFormat) / 8;
    size_t   dstPlainRowSize = dstTexelSize * width;
    // There are checks above to ensure that this won't overflow.
    size_t   dstStride = RoundedToNextMultipleOf(dstPlainRowSize, mPixelStoreUnpackAlignment).value();

    void* pixels = data;
    nsAutoArrayPtr<uint8_t> convertedData;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(existingEffectiveInternalFormat);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
    bool noConversion = (actualSrcFormat == dstFormat &&
                         srcPremultiplied == mPixelStorePremultiplyAlpha &&
                         srcStride == dstStride &&
                         !mPixelStoreFlipY);

    if (!noConversion) {
        size_t convertedDataSize = height * dstStride;
        convertedData = new (fallible) uint8_t[convertedDataSize];
        if (!convertedData) {
            ErrorOutOfMemory("texImage2D: Ran out of memory when allocating"
                             " a buffer for doing format conversion.");
            return;
        }
        if (!ConvertImage(width, height, srcStride, dstStride,
                          static_cast<const uint8_t*>(data), convertedData,
                          actualSrcFormat, srcPremultiplied,
                          dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize))
        {
            return ErrorInvalidOperation("texSubImage2D: Unsupported texture format conversion");
        }
        pixels = reinterpret_cast<void*>(convertedData.get());
    }

    GLenum driverType = LOCAL_GL_NONE;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl,
                                             existingEffectiveInternalFormat,
                                             &driverInternalFormat,
                                             &driverFormat,
                                             &driverType);

    gl->fTexSubImage2D(texImageTarget.get(), level, xoffset, yoffset, width, height, driverFormat, driverType, pixels);
}

void
WebGLContext::TexSubImage2D(GLenum rawTarget, GLint level,
                            GLint xoffset, GLint yoffset,
                            GLsizei width, GLsizei height,
                            GLenum format, GLenum type,
                            const Nullable<ArrayBufferView> &pixels,
                            ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (pixels.IsNull())
        return ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    const ArrayBufferView& view = pixels.Value();
    view.ComputeLengthAndData();

    if (!ValidateTexImageTarget(rawTarget, WebGLTexImageFunc::TexSubImage, WebGLTexDimensions::Tex2D))
        return;

    return TexSubImage2D_base(rawTarget, level, xoffset, yoffset,
                              width, height, 0, format, type,
                              view.Data(), view.Length(),
                              JS_GetArrayBufferViewType(view.Obj()),
                              WebGLTexelFormat::Auto, false);
}

void
WebGLContext::TexSubImage2D(GLenum target, GLint level,
                            GLint xoffset, GLint yoffset,
                            GLenum format, GLenum type, ImageData* pixels,
                            ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (!pixels)
        return ErrorInvalidValue("texSubImage2D: pixels must not be null!");

    Uint8ClampedArray arr;
    DebugOnly<bool> inited = arr.Init(pixels->GetDataObject());
    MOZ_ASSERT(inited);
    arr.ComputeLengthAndData();

    return TexSubImage2D_base(target, level, xoffset, yoffset,
                              pixels->Width(), pixels->Height(),
                              4*pixels->Width(), format, type,
                              arr.Data(), arr.Length(),
                              js::Scalar::MaxTypedArrayViewType,
                              WebGLTexelFormat::RGBA8, false);
}

void
WebGLContext::LoseContext()
{
    if (IsContextLost())
        return ErrorInvalidOperation("loseContext: Context is already lost.");

    ForceLoseContext(true);
}

void
WebGLContext::RestoreContext()
{
    if (!IsContextLost())
        return ErrorInvalidOperation("restoreContext: Context is not lost.");

    if (!mLastLossWasSimulated) {
        return ErrorInvalidOperation("restoreContext: Context loss was not simulated."
                                     " Cannot simulate restore.");
    }
    // If we're currently lost, and the last loss was simulated, then
    // we're currently only simulated-lost, allowing us to call
    // restoreContext().

    if (!mAllowContextRestore)
        return ErrorInvalidOperation("restoreContext: Context cannot be restored.");

    ForceRestoreContext();
}

WebGLTexelFormat
mozilla::GetWebGLTexelFormat(TexInternalFormat effectiveInternalFormat)
{
    switch (effectiveInternalFormat.get()) {
        case LOCAL_GL_RGBA8:                  return WebGLTexelFormat::RGBA8;
        case LOCAL_GL_SRGB8_ALPHA8:           return WebGLTexelFormat::RGBA8;
        case LOCAL_GL_RGB8:                   return WebGLTexelFormat::RGB8;
        case LOCAL_GL_SRGB8:                  return WebGLTexelFormat::RGB8;
        case LOCAL_GL_ALPHA8:                 return WebGLTexelFormat::A8;
        case LOCAL_GL_LUMINANCE8:             return WebGLTexelFormat::R8;
        case LOCAL_GL_LUMINANCE8_ALPHA8:      return WebGLTexelFormat::RA8;
        case LOCAL_GL_RGBA32F:                return WebGLTexelFormat::RGBA32F;
        case LOCAL_GL_RGB32F:                 return WebGLTexelFormat::RGB32F;
        case LOCAL_GL_ALPHA32F_EXT:           return WebGLTexelFormat::A32F;
        case LOCAL_GL_LUMINANCE32F_EXT:       return WebGLTexelFormat::R32F;
        case LOCAL_GL_LUMINANCE_ALPHA32F_EXT: return WebGLTexelFormat::RA32F;
        case LOCAL_GL_RGBA16F:                return WebGLTexelFormat::RGBA16F;
        case LOCAL_GL_RGB16F:                 return WebGLTexelFormat::RGB16F;
        case LOCAL_GL_ALPHA16F_EXT:           return WebGLTexelFormat::A16F;
        case LOCAL_GL_LUMINANCE16F_EXT:       return WebGLTexelFormat::R16F;
        case LOCAL_GL_LUMINANCE_ALPHA16F_EXT: return WebGLTexelFormat::RA16F;
        case LOCAL_GL_RGBA4:                  return WebGLTexelFormat::RGBA4444;
        case LOCAL_GL_RGB5_A1:                return WebGLTexelFormat::RGBA5551;
        case LOCAL_GL_RGB565:                 return WebGLTexelFormat::RGB565;
        default:
            return WebGLTexelFormat::FormatNotSupportingAnyConversion;
    }
}

void
WebGLContext::BlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    if (IsContextLost())
        return;
    MakeContextCurrent();
    gl->fBlendColor(r, g, b, a);
}

void
WebGLContext::Flush() {
    if (IsContextLost())
        return;
    MakeContextCurrent();
    gl->fFlush();
}

void
WebGLContext::Finish() {
    if (IsContextLost())
        return;
    MakeContextCurrent();
    gl->fFinish();
}

void
WebGLContext::LineWidth(GLfloat width)
{
    if (IsContextLost())
        return;

    // Doing it this way instead of `if (width <= 0.0)` handles NaNs.
    const bool isValid = width > 0.0;
    if (!isValid) {
        ErrorInvalidValue("lineWidth: `width` must be positive and non-zero.");
        return;
    }

    MakeContextCurrent();
    gl->fLineWidth(width);
}

void
WebGLContext::PolygonOffset(GLfloat factor, GLfloat units) {
    if (IsContextLost())
        return;
    MakeContextCurrent();
    gl->fPolygonOffset(factor, units);
}

void
WebGLContext::SampleCoverage(GLclampf value, WebGLboolean invert) {
    if (IsContextLost())
        return;
    MakeContextCurrent();
    gl->fSampleCoverage(value, invert);
}
