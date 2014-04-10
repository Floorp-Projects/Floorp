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

#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "GLContext.h"

#include "nsContentUtils.h"
#include "nsError.h"
#include "nsLayoutUtils.h"

#include "CanvasUtils.h"

#include "jsfriendapi.h"

#include "WebGLTexelConversions.h"
#include "WebGLValidateStrings.h"
#include <algorithm>

// needed to check if current OS is lower than 10.7
#if defined(MOZ_WIDGET_COCOA)
#include "nsCocoaFeatures.h"
#endif

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Endian.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gl;
using namespace mozilla::gfx;

static bool BaseTypeAndSizeFromUniformType(GLenum uType, GLenum *baseType, GLint *unitSize);
static GLenum InternalFormatForFormatAndType(GLenum format, GLenum type, bool isGLES2);

const WebGLRectangleObject*
WebGLContext::CurValidFBRectObject() const
{
    const WebGLRectangleObject* rect = nullptr;

    if (mBoundFramebuffer) {
        // We don't really need to ask the driver.
        // Use 'precheck' to just check that our internal state looks good.
        GLenum precheckStatus = mBoundFramebuffer->PrecheckFramebufferStatus();
        if (precheckStatus == LOCAL_GL_FRAMEBUFFER_COMPLETE)
            rect = &mBoundFramebuffer->RectangleObject();
    } else {
        rect = static_cast<const WebGLRectangleObject*>(this);
    }

    return rect;
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
WebGLContext::AttachShader(WebGLProgram *program, WebGLShader *shader)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("attachShader: program", program) ||
        !ValidateObject("attachShader: shader", shader))
        return;

    // Per GLSL ES 2.0, we can only have one of each type of shader
    // attached.  This renders the next test somewhat moot, but we'll
    // leave it for when we support more than one shader of each type.
    if (program->HasAttachedShaderOfType(shader->ShaderType()))
        return ErrorInvalidOperation("attachShader: only one of each type of shader may be attached to a program");

    if (!program->AttachShader(shader))
        return ErrorInvalidOperation("attachShader: shader is already attached");
}


void
WebGLContext::BindAttribLocation(WebGLProgram *prog, GLuint location,
                                 const nsAString& name)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("bindAttribLocation: program", prog))
        return;

    GLuint progname = prog->GLName();

    if (!ValidateGLSLVariableName(name, "bindAttribLocation"))
        return;

    if (!ValidateAttribIndex(location, "bindAttribLocation"))
        return;

    NS_LossyConvertUTF16toASCII cname(name);
    nsCString mappedName;
    prog->MapIdentifier(cname, &mappedName);

    MakeContextCurrent();
    gl->fBindAttribLocation(progname, location, mappedName.get());
}

void
WebGLContext::BindFramebuffer(GLenum target, WebGLFramebuffer *wfb)
{
    if (IsContextLost())
        return;

    if (target != LOCAL_GL_FRAMEBUFFER)
        return ErrorInvalidEnum("bindFramebuffer: target must be GL_FRAMEBUFFER");

    if (!ValidateObjectAllowDeletedOrNull("bindFramebuffer", wfb))
        return;

    // silently ignore a deleted frame buffer
    if (wfb && wfb->IsDeleted())
        return;

    MakeContextCurrent();

    if (!wfb) {
        gl->fBindFramebuffer(target, 0);
    } else {
        GLuint framebuffername = wfb->GLName();
        gl->fBindFramebuffer(target, framebuffername);
        wfb->SetHasEverBeenBound(true);
    }

    mBoundFramebuffer = wfb;
}

void
WebGLContext::BindRenderbuffer(GLenum target, WebGLRenderbuffer *wrb)
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
        wrb->SetHasEverBeenBound(true);

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
WebGLContext::BindTexture(GLenum target, WebGLTexture *newTex)
{
    if (IsContextLost())
        return;

     if (!ValidateObjectAllowDeletedOrNull("bindTexture", newTex))
        return;

    // silently ignore a deleted texture
    if (newTex && newTex->IsDeleted())
        return;

    WebGLRefPtr<WebGLTexture>* currentTexPtr = nullptr;

    if (target == LOCAL_GL_TEXTURE_2D) {
        currentTexPtr = &mBound2DTextures[mActiveTexture];
    } else if (target == LOCAL_GL_TEXTURE_CUBE_MAP) {
        currentTexPtr = &mBoundCubeMapTextures[mActiveTexture];
    } else {
        return ErrorInvalidEnumInfo("bindTexture: target", target);
    }

    WebGLTextureFakeBlackStatus currentTexFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
    if (*currentTexPtr) {
        currentTexFakeBlackStatus = (*currentTexPtr)->ResolvedFakeBlackStatus();
    }
    WebGLTextureFakeBlackStatus newTexFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
    if (newTex) {
        newTexFakeBlackStatus = newTex->ResolvedFakeBlackStatus();
    }

    *currentTexPtr = newTex;

    if (currentTexFakeBlackStatus != newTexFakeBlackStatus) {
        SetFakeBlackStatus(WebGLContextFakeBlackStatus::Unknown);
    }

    MakeContextCurrent();

    if (newTex)
        newTex->Bind(target);
    else
        gl->fBindTexture(target, 0 /* == texturename */);
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

    if (target != LOCAL_GL_FRAMEBUFFER) {
        ErrorInvalidEnum("checkFramebufferStatus: target must be FRAMEBUFFER");
        return 0;
    }

    if (!mBoundFramebuffer)
        return LOCAL_GL_FRAMEBUFFER_COMPLETE;

    return mBoundFramebuffer->CheckFramebufferStatus();
}

void
WebGLContext::CopyTexSubImage2D_base(GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height,
                                     bool sub)
{
    const WebGLRectangleObject* framebufferRect = CurValidFBRectObject();
    GLsizei framebufferWidth = framebufferRect ? framebufferRect->Width() : 0;
    GLsizei framebufferHeight = framebufferRect ? framebufferRect->Height() : 0;

    const char* info = sub ? "copyTexSubImage2D" : "copyTexImage2D";
    WebGLTexImageFunc func = sub ? WebGLTexImageFunc::CopyTexSubImage : WebGLTexImageFunc::CopyTexImage;

    // TODO: This changes with color_buffer_float. Reassess when the
    // patch lands.
    if (!ValidateTexImage(2, target, level, internalformat,
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0, internalformat, LOCAL_GL_UNSIGNED_BYTE,
                          func))
    {
        return;
    }

    MakeContextCurrent();

    WebGLTexture *tex = activeBoundTextureForTarget(target);

    if (!tex)
        return ErrorInvalidOperation("%s: no texture is bound to this target");

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, framebufferWidth, framebufferHeight)) {
        if (sub)
            gl->fCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
        else
            gl->fCopyTexImage2D(target, level, internalformat, x, y, width, height, 0);
    } else {

        // the rect doesn't fit in the framebuffer

        /*** first, we initialize the texture as black ***/

        // first, compute the size of the buffer we should allocate to initialize the texture as black

        if (!ValidateTexInputData(LOCAL_GL_UNSIGNED_BYTE, -1, func))
            return;

        uint32_t texelSize = GetBitsPerTexel(internalformat, LOCAL_GL_UNSIGNED_BYTE) / 8;

        CheckedUint32 checked_neededByteLength =
            GetImageSize(height, width, texelSize, mPixelStoreUnpackAlignment);

        if (!checked_neededByteLength.isValid())
            return ErrorInvalidOperation("%s: integer overflow computing the needed buffer size", info);

        uint32_t bytesNeeded = checked_neededByteLength.value();

        // now that the size is known, create the buffer

        // We need some zero pages, because GL doesn't guarantee the
        // contents of a texture allocated with nullptr data.
        // Hopefully calloc will just mmap zero pages here.
        void* tempZeroData = calloc(1, bytesNeeded);
        if (!tempZeroData)
            return ErrorOutOfMemory("%s: could not allocate %d bytes (for zero fill)", info, bytesNeeded);

        // now initialize the texture as black

        if (sub)
            gl->fTexSubImage2D(target, level, 0, 0, width, height,
                               internalformat, LOCAL_GL_UNSIGNED_BYTE, tempZeroData);
        else
            gl->fTexImage2D(target, level, internalformat, width, height,
                            0, internalformat, LOCAL_GL_UNSIGNED_BYTE, tempZeroData);
        free(tempZeroData);

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

        gl->fCopyTexSubImage2D(target, level, actual_xoffset, actual_yoffset, actual_x, actual_y, actual_width, actual_height);
    }
}

void
WebGLContext::CopyTexImage2D(GLenum target,
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
    GLenum type = LOCAL_GL_UNSIGNED_BYTE;

    if (!ValidateTexImage(2, target, level, internalformat,
                          0, 0, 0,
                          width, height, 0,
                          border, internalformat, type,
                          func))
    {
        return;
    }

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments())
            return ErrorInvalidFramebufferOperation("copyTexImage2D: incomplete framebuffer");

        GLenum readPlaneBits = LOCAL_GL_COLOR_BUFFER_BIT;
        if (!mBoundFramebuffer->HasCompletePlanes(readPlaneBits)) {
            return ErrorInvalidOperation("copyTexImage2D: Read source attachment doesn't have the"
                                         " correct color/depth/stencil type.");
        }
    }

    bool texFormatRequiresAlpha = internalformat == LOCAL_GL_RGBA ||
                                  internalformat == LOCAL_GL_ALPHA ||
                                  internalformat == LOCAL_GL_LUMINANCE_ALPHA;
    bool fboFormatHasAlpha = mBoundFramebuffer ? mBoundFramebuffer->ColorAttachment(0).HasAlpha()
                                               : bool(gl->GetPixelFormat().alpha > 0);
    if (texFormatRequiresAlpha && !fboFormatHasAlpha)
        return ErrorInvalidOperation("copyTexImage2D: texture format requires an alpha channel "
                                     "but the framebuffer doesn't have one");

    // check if the memory size of this texture may change with this call
    bool sizeMayChange = true;
    WebGLTexture* tex = activeBoundTextureForTarget(target);
    if (tex->HasImageInfoAt(target, level)) {
        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(target, level);

        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        internalformat != imageInfo.InternalFormat() ||
                        type != imageInfo.Type();
    }

    if (sizeMayChange)
        GetAndFlushUnderlyingGLErrors();

    CopyTexSubImage2D_base(target, level, internalformat, 0, 0, x, y, width, height, false);

    if (sizeMayChange) {
        GLenum error = GetAndFlushUnderlyingGLErrors();
        if (error) {
            GenerateWarning("copyTexImage2D generated error %s", ErrorName(error));
            return;
        }
    }

    tex->SetImageInfo(target, level, width, height, internalformat, type,
                      WebGLImageDataStatus::InitializedImageData);
}

void
WebGLContext::CopyTexSubImage2D(GLenum target,
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

    switch (target) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            return ErrorInvalidEnumInfo("copyTexSubImage2D: target", target);
    }

    if (level < 0)
        return ErrorInvalidValue("copyTexSubImage2D: level may not be negative");

    GLsizei maxTextureSize = MaxTextureSizeForTarget(target);
    if (!(maxTextureSize >> level))
        return ErrorInvalidValue("copyTexSubImage2D: 2^level exceeds maximum texture size");

    if (width < 0 || height < 0)
        return ErrorInvalidValue("copyTexSubImage2D: width and height may not be negative");

    if (xoffset < 0 || yoffset < 0)
        return ErrorInvalidValue("copyTexSubImage2D: xoffset and yoffset may not be negative");

    WebGLTexture *tex = activeBoundTextureForTarget(target);
    if (!tex)
        return ErrorInvalidOperation("copyTexSubImage2D: no texture bound to this target");

    if (!tex->HasImageInfoAt(target, level))
        return ErrorInvalidOperation("copyTexSubImage2D: no texture image previously defined for this level and face");

    const WebGLTexture::ImageInfo &imageInfo = tex->ImageInfoAt(target, level);
    GLsizei texWidth = imageInfo.Width();
    GLsizei texHeight = imageInfo.Height();

    if (xoffset + width > texWidth || xoffset + width < 0)
      return ErrorInvalidValue("copyTexSubImage2D: xoffset+width is too large");

    if (yoffset + height > texHeight || yoffset + height < 0)
      return ErrorInvalidValue("copyTexSubImage2D: yoffset+height is too large");

    GLenum internalFormat = imageInfo.InternalFormat();
    if (IsGLDepthFormat(internalFormat) ||
        IsGLDepthStencilFormat(internalFormat))
    {
        return ErrorInvalidOperation("copyTexSubImage2D: a base internal format of DEPTH_COMPONENT or DEPTH_STENCIL isn't supported");
    }

    if (mBoundFramebuffer) {
        if (!mBoundFramebuffer->CheckAndInitializeAttachments())
            return ErrorInvalidFramebufferOperation("copyTexSubImage2D: incomplete framebuffer");

        GLenum readPlaneBits = LOCAL_GL_COLOR_BUFFER_BIT;
        if (!mBoundFramebuffer->HasCompletePlanes(readPlaneBits)) {
            return ErrorInvalidOperation("copyTexSubImage2D: Read source attachment doesn't have the"
                                         " correct color/depth/stencil type.");
        }
    }

    bool texFormatRequiresAlpha = (internalFormat == LOCAL_GL_RGBA ||
                                   internalFormat == LOCAL_GL_ALPHA ||
                                   internalFormat == LOCAL_GL_LUMINANCE_ALPHA);
    bool fboFormatHasAlpha = mBoundFramebuffer ? mBoundFramebuffer->ColorAttachment(0).HasAlpha()
                                               : bool(gl->GetPixelFormat().alpha > 0);

    if (texFormatRequiresAlpha && !fboFormatHasAlpha)
        return ErrorInvalidOperation("copyTexSubImage2D: texture format requires an alpha channel "
                                     "but the framebuffer doesn't have one");

    if (imageInfo.HasUninitializedImageData()) {
        tex->DoDeferredImageInitialization(target, level);
    }

    return CopyTexSubImage2D_base(target, level, internalFormat, xoffset, yoffset, x, y, width, height, true);
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

    if (mBoundFramebuffer == fbuf)
        BindFramebuffer(LOCAL_GL_FRAMEBUFFER,
                        static_cast<WebGLFramebuffer*>(nullptr));
}

void
WebGLContext::DeleteRenderbuffer(WebGLRenderbuffer *rbuf)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteRenderbuffer", rbuf))
        return;

    if (!rbuf || rbuf->IsDeleted())
        return;

    if (mBoundFramebuffer)
        mBoundFramebuffer->DetachRenderbuffer(rbuf);

    // Invalidate framebuffer status cache
    rbuf->NotifyFBsStatusChanged();

    if (mBoundRenderbuffer == rbuf)
        BindRenderbuffer(LOCAL_GL_RENDERBUFFER,
                         static_cast<WebGLRenderbuffer*>(nullptr));

    rbuf->RequestDelete();
}

void
WebGLContext::DeleteTexture(WebGLTexture *tex)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowDeletedOrNull("deleteTexture", tex))
        return;

    if (!tex || tex->IsDeleted())
        return;

    if (mBoundFramebuffer)
        mBoundFramebuffer->DetachTexture(tex);

    // Invalidate framebuffer status cache
    tex->NotifyFBsStatusChanged();

    GLuint activeTexture = mActiveTexture;
    for (int32_t i = 0; i < mGLMaxTextureUnits; i++) {
        if ((tex->Target() == LOCAL_GL_TEXTURE_2D && mBound2DTextures[i] == tex) ||
            (tex->Target() == LOCAL_GL_TEXTURE_CUBE_MAP && mBoundCubeMapTextures[i] == tex))
        {
            ActiveTexture(LOCAL_GL_TEXTURE0 + i);
            BindTexture(tex->Target(), static_cast<WebGLTexture*>(nullptr));
        }
    }
    ActiveTexture(LOCAL_GL_TEXTURE0 + activeTexture);

    tex->RequestDelete();
}

void
WebGLContext::DeleteProgram(WebGLProgram *prog)
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
WebGLContext::DeleteShader(WebGLShader *shader)
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
WebGLContext::DetachShader(WebGLProgram *program, WebGLShader *shader)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("detachShader: program", program) ||
        // it's valid to attempt to detach a deleted shader, since it's
        // still a shader
        !ValidateObjectAllowDeleted("detashShader: shader", shader))
        return;

    if (!program->DetachShader(shader))
        return ErrorInvalidOperation("detachShader: shader is not attached");
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
WebGLContext::FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, WebGLRenderbuffer *wrb)
{
    if (IsContextLost())
        return;

    if (!mBoundFramebuffer)
        return ErrorInvalidOperation("framebufferRenderbuffer: cannot modify framebuffer 0");

    return mBoundFramebuffer->FramebufferRenderbuffer(target, attachment, rbtarget, wrb);
}

void
WebGLContext::FramebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   WebGLTexture *tobj,
                                   GLint level)
{
    if (IsContextLost())
        return;

    if (!mBoundFramebuffer)
        return ErrorInvalidOperation("framebufferRenderbuffer: cannot modify framebuffer 0");

    return mBoundFramebuffer->FramebufferTexture2D(target, attachment, textarget, tobj, level);
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

already_AddRefed<WebGLActiveInfo>
WebGLContext::GetActiveAttrib(WebGLProgram *prog, uint32_t index)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("getActiveAttrib: program", prog))
        return nullptr;

    MakeContextCurrent();

    GLint len = 0;
    GLuint progname = prog->GLName();;
    gl->fGetProgramiv(progname, LOCAL_GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &len);
    if (len == 0)
        return nullptr;

    nsAutoArrayPtr<char> name(new char[len]);
    GLint attrsize = 0;
    GLuint attrtype = 0;

    gl->fGetActiveAttrib(progname, index, len, &len, &attrsize, &attrtype, name);
    if (attrsize == 0 || attrtype == 0) {
        return nullptr;
    }

    nsCString reverseMappedName;
    prog->ReverseMapIdentifier(nsDependentCString(name), &reverseMappedName);

    nsRefPtr<WebGLActiveInfo> retActiveInfo =
        new WebGLActiveInfo(attrsize, attrtype, reverseMappedName);
    return retActiveInfo.forget();
}

void
WebGLContext::GenerateMipmap(GLenum target)
{
    if (IsContextLost())
        return;

    if (!ValidateTextureTargetEnum(target, "generateMipmap"))
        return;

    WebGLTexture *tex = activeBoundTextureForTarget(target);

    if (!tex)
        return ErrorInvalidOperation("generateMipmap: No texture is bound to this target.");

    GLenum imageTarget = (target == LOCAL_GL_TEXTURE_2D) ? LOCAL_GL_TEXTURE_2D
                                                         : LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    if (!tex->HasImageInfoAt(imageTarget, 0))
    {
        return ErrorInvalidOperation("generateMipmap: Level zero of texture is not defined.");
    }

    if (!tex->IsFirstImagePowerOfTwo())
        return ErrorInvalidOperation("generateMipmap: Level zero of texture does not have power-of-two width and height.");

    GLenum internalFormat = tex->ImageInfoAt(imageTarget, 0).InternalFormat();
    if (IsTextureFormatCompressed(internalFormat))
        return ErrorInvalidOperation("generateMipmap: Texture data at level zero is compressed.");

    if (IsExtensionEnabled(WEBGL_depth_texture) &&
        (IsGLDepthFormat(internalFormat) || IsGLDepthStencilFormat(internalFormat)))
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
        gl->fTexParameteri(target, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST_MIPMAP_NEAREST);
        gl->fGenerateMipmap(target);
        gl->fTexParameteri(target, LOCAL_GL_TEXTURE_MIN_FILTER, tex->MinFilter());
    } else {
        gl->fGenerateMipmap(target);
    }
}

already_AddRefed<WebGLActiveInfo>
WebGLContext::GetActiveUniform(WebGLProgram *prog, uint32_t index)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("getActiveUniform: program", prog))
        return nullptr;

    MakeContextCurrent();

    GLint len = 0;
    GLuint progname = prog->GLName();
    gl->fGetProgramiv(progname, LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH, &len);
    if (len == 0)
        return nullptr;

    nsAutoArrayPtr<char> name(new char[len]);

    GLint usize = 0;
    GLuint utype = 0;

    gl->fGetActiveUniform(progname, index, len, &len, &usize, &utype, name);
    if (len == 0 || usize == 0 || utype == 0) {
        return nullptr;
    }

    nsCString reverseMappedName;
    prog->ReverseMapIdentifier(nsDependentCString(name), &reverseMappedName);

    // OpenGL ES 2.0 specifies that if foo is a uniform array, GetActiveUniform returns its name as "foo[0]".
    // See section 2.10 page 35 in the OpenGL ES 2.0.24 specification:
    //
    // > If the active uniform is an array, the uniform name returned in name will always
    // > be the name of the uniform array appended with "[0]".
    //
    // There is no such requirement in the OpenGL (non-ES) spec and indeed we have OpenGL implementations returning
    // "foo" instead of "foo[0]". So, when implementing WebGL on top of desktop OpenGL, we must check if the
    // returned name ends in [0], and if it doesn't, append that.
    //
    // In principle we don't need to do that on OpenGL ES, but this is such a tricky difference between the ES and non-ES
    // specs that it seems probable that some ES implementers will overlook it. Since the work-around is quite cheap,
    // we do it unconditionally.
    if (usize > 1 && reverseMappedName.CharAt(reverseMappedName.Length()-1) != ']')
        reverseMappedName.AppendLiteral("[0]");

    nsRefPtr<WebGLActiveInfo> retActiveInfo =
        new WebGLActiveInfo(usize, utype, reverseMappedName);
    return retActiveInfo.forget();
}

void
WebGLContext::GetAttachedShaders(WebGLProgram *prog,
                                 Nullable< nsTArray<WebGLShader*> > &retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowNull("getAttachedShaders", prog))
        return;

    MakeContextCurrent();

    if (!prog) {
        retval.SetNull();
        ErrorInvalidValue("getAttachedShaders: invalid program");
    } else if (prog->AttachedShaders().Length() == 0) {
        retval.SetValue().TruncateLength(0);
    } else {
        retval.SetValue().AppendElements(prog->AttachedShaders());
    }
}

GLint
WebGLContext::GetAttribLocation(WebGLProgram *prog, const nsAString& name)
{
    if (IsContextLost())
        return -1;

    if (!ValidateObject("getAttribLocation: program", prog))
        return -1;

    if (!ValidateGLSLVariableName(name, "getAttribLocation"))
        return -1;

    NS_LossyConvertUTF16toASCII cname(name);
    nsCString mappedName;
    prog->MapIdentifier(cname, &mappedName);

    GLuint progname = prog->GLName();

    MakeContextCurrent();
    return gl->fGetAttribLocation(progname, mappedName.get());
}

JS::Value
WebGLContext::GetBufferParameter(GLenum target, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    if (target != LOCAL_GL_ARRAY_BUFFER && target != LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        ErrorInvalidEnumInfo("getBufferParameter: target", target);
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

    if (target != LOCAL_GL_FRAMEBUFFER) {
        ErrorInvalidEnumInfo("getFramebufferAttachmentParameter: target", target);
        return JS::NullValue();
    }

    if (!mBoundFramebuffer) {
        ErrorInvalidOperation("getFramebufferAttachmentParameter: cannot query framebuffer 0");
        return JS::NullValue();
    }

    if (attachment != LOCAL_GL_DEPTH_ATTACHMENT &&
        attachment != LOCAL_GL_STENCIL_ATTACHMENT &&
        attachment != LOCAL_GL_DEPTH_STENCIL_ATTACHMENT)
    {
        if (IsExtensionEnabled(WEBGL_draw_buffers))
        {
            if (attachment < LOCAL_GL_COLOR_ATTACHMENT0 ||
                attachment >= GLenum(LOCAL_GL_COLOR_ATTACHMENT0 + mGLMaxColorAttachments))
            {
                ErrorInvalidEnumInfo("getFramebufferAttachmentParameter: attachment", attachment);
                return JS::NullValue();
            }

            mBoundFramebuffer->EnsureColorAttachments(attachment - LOCAL_GL_COLOR_ATTACHMENT0);
        }
        else if (attachment != LOCAL_GL_COLOR_ATTACHMENT0)
        {
            ErrorInvalidEnumInfo("getFramebufferAttachmentParameter: attachment", attachment);
            return JS::NullValue();
        }
    }

    MakeContextCurrent();

    const WebGLFramebuffer::Attachment& fba = mBoundFramebuffer->GetAttachment(attachment);

    if (fba.Renderbuffer()) {
        switch (pname) {
            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
                if (IsExtensionEnabled(EXT_sRGB)) {
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
                if (!IsExtensionEnabled(EXT_color_buffer_half_float) &&
                    !IsExtensionEnabled(WEBGL_color_buffer_float))
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
                if (IsExtensionEnabled(EXT_sRGB)) {
                    const GLenum internalFormat =
                        fba.Texture()->ImageInfoBase().InternalFormat();
                    return (internalFormat == LOCAL_GL_SRGB_EXT ||
                            internalFormat == LOCAL_GL_SRGB_ALPHA_EXT) ?
                        JS::NumberValue(uint32_t(LOCAL_GL_SRGB_EXT)) :
                        JS::NumberValue(uint32_t(LOCAL_GL_LINEAR));
                }
                break;

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                return JS::NumberValue(uint32_t(LOCAL_GL_TEXTURE));

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                return WebGLObjectAsJSValue(cx, fba.Texture(), rv);

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
                return JS::Int32Value(fba.TexImageLevel());

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: {
                GLenum face = fba.TexImageTarget();
                if (face == LOCAL_GL_TEXTURE_2D)
                    face = 0;
                return JS::Int32Value(face);
            }

            case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: {
                if (!IsExtensionEnabled(EXT_color_buffer_half_float) &&
                    !IsExtensionEnabled(WEBGL_color_buffer_float))
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

                uint32_t ret = LOCAL_GL_NONE;
                GLenum type = fba.Texture()->ImageInfoAt(fba.TexImageTarget(),
                                                         fba.TexImageLevel()).Type();
                switch (type) {
                case LOCAL_GL_UNSIGNED_BYTE:
                case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
                case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
                case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
                    ret = LOCAL_GL_UNSIGNED_NORMALIZED;
                    break;
                case LOCAL_GL_FLOAT:
                case LOCAL_GL_HALF_FLOAT_OES:
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
    nsRefPtr<WebGLTexture> globj = new WebGLTexture(this);
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
WebGLContext::GetProgramParameter(WebGLProgram *prog, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObjectAllowDeleted("getProgramParameter: program", prog))
        return JS::NullValue();

    GLuint progname = prog->GLName();

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_ATTACHED_SHADERS:
        case LOCAL_GL_ACTIVE_UNIFORMS:
        case LOCAL_GL_ACTIVE_ATTRIBUTES:
        {
            GLint i = 0;
            gl->fGetProgramiv(progname, pname, &i);
            return JS::Int32Value(i);
        }
        case LOCAL_GL_DELETE_STATUS:
            return JS::BooleanValue(prog->IsDeleteRequested());
        case LOCAL_GL_LINK_STATUS:
        {
            return JS::BooleanValue(prog->LinkStatus());
        }
        case LOCAL_GL_VALIDATE_STATUS:
        {
            GLint i = 0;
#ifdef XP_MACOSX
            // See comment in ValidateProgram below.
            if (gl->WorkAroundDriverBugs())
                i = 1;
            else
                gl->fGetProgramiv(progname, pname, &i);
#else
            gl->fGetProgramiv(progname, pname, &i);
#endif
            return JS::BooleanValue(bool(i));
        }
            break;

        default:
            ErrorInvalidEnumInfo("getProgramParameter: parameter", pname);
    }

    return JS::NullValue();
}

void
WebGLContext::GetProgramInfoLog(WebGLProgram *prog, nsAString& retval)
{
    nsAutoCString s;
    GetProgramInfoLog(prog, s);
    if (s.IsVoid())
        retval.SetIsVoid(true);
    else
        CopyASCIItoUTF16(s, retval);
}

void
WebGLContext::GetProgramInfoLog(WebGLProgram *prog, nsACString& retval)
{
    if (IsContextLost())
    {
        retval.SetIsVoid(true);
        return;
    }

    if (!ValidateObject("getProgramInfoLog: program", prog)) {
        retval.Truncate();
        return;
    }

    GLuint progname = prog->GLName();

    MakeContextCurrent();

    GLint k = -1;
    gl->fGetProgramiv(progname, LOCAL_GL_INFO_LOG_LENGTH, &k);
    if (k == -1) {
        // If GetProgramiv doesn't modify |k|,
        // it's because there was a GL error.
        // GetProgramInfoLog should return null on error. (Bug 746740)
        retval.SetIsVoid(true);
        return;
    }

    if (k == 0) {
        retval.Truncate();
        return;
    }

    retval.SetCapacity(k);
    gl->fGetProgramInfoLog(progname, k, &k, (char*) retval.BeginWriting());
    retval.SetLength(k);
}

// here we have to support all pnames with both int and float params.
// See this discussion:
//  https://www.khronos.org/webgl/public-mailing-list/archives/1008/msg00014.html
void WebGLContext::TexParameter_base(GLenum target, GLenum pname,
                                     GLint *intParamPtr,
                                     GLfloat *floatParamPtr)
{
    MOZ_ASSERT(intParamPtr || floatParamPtr);

    if (IsContextLost())
        return;

    GLint intParam = intParamPtr ? *intParamPtr : GLint(*floatParamPtr);
    GLfloat floatParam = floatParamPtr ? *floatParamPtr : GLfloat(*intParamPtr);

    if (!ValidateTextureTargetEnum(target, "texParameter: target"))
        return;

    WebGLTexture *tex = activeBoundTextureForTarget(target);
    if (!tex)
        return ErrorInvalidOperation("texParameter: no texture is bound to this target");

    bool pnameAndParamAreIncompatible = false;
    bool paramValueInvalid = false;

    switch (pname) {
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
            if (IsExtensionEnabled(EXT_texture_filter_anisotropic)) {
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
        gl->fTexParameteri(target, pname, intParam);
    else
        gl->fTexParameterf(target, pname, floatParam);
}

JS::Value
WebGLContext::GetTexParameter(GLenum target, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    MakeContextCurrent();

    if (!ValidateTextureTargetEnum(target, "getTexParameter: target"))
        return JS::NullValue();

    if (!activeBoundTextureForTarget(target)) {
        ErrorInvalidOperation("getTexParameter: no texture bound");
        return JS::NullValue();
    }

    switch (pname) {
        case LOCAL_GL_TEXTURE_MIN_FILTER:
        case LOCAL_GL_TEXTURE_MAG_FILTER:
        case LOCAL_GL_TEXTURE_WRAP_S:
        case LOCAL_GL_TEXTURE_WRAP_T:
        {
            GLint i = 0;
            gl->fGetTexParameteriv(target, pname, &i);
            return JS::NumberValue(uint32_t(i));
        }
        case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
            if (IsExtensionEnabled(EXT_texture_filter_anisotropic)) {
                GLfloat f = 0.f;
                gl->fGetTexParameterfv(target, pname, &f);
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
WebGLContext::GetUniform(JSContext* cx, WebGLProgram *prog,
                         WebGLUniformLocation *location)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObject("getUniform: program", prog))
        return JS::NullValue();

    if (!ValidateObject("getUniform: location", location))
        return JS::NullValue();

    if (location->Program() != prog) {
        ErrorInvalidValue("getUniform: this uniform location corresponds to another program");
        return JS::NullValue();
    }

    if (location->ProgramGeneration() != prog->Generation()) {
        ErrorInvalidOperation("getUniform: this uniform location is obsolete since the program has been relinked");
        return JS::NullValue();
    }

    GLuint progname = prog->GLName();

    MakeContextCurrent();

    GLint uniforms = 0;
    GLint uniformNameMaxLength = 0;
    gl->fGetProgramiv(progname, LOCAL_GL_ACTIVE_UNIFORMS, &uniforms);
    gl->fGetProgramiv(progname, LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniformNameMaxLength);

    // we now need the type info to switch between fGetUniformfv and fGetUniformiv
    // the only way to get that is to iterate through all active uniforms by index until
    // one matches the given uniform location.
    GLenum uniformType = 0;
    nsAutoArrayPtr<GLchar> uniformName(new GLchar[uniformNameMaxLength]);
    // this buffer has 16 more bytes to be able to store [index] at the end.
    nsAutoArrayPtr<GLchar> uniformNameBracketIndex(new GLchar[uniformNameMaxLength + 16]);

    GLint index;
    for (index = 0; index < uniforms; ++index) {
        GLsizei length;
        GLint size;
        gl->fGetActiveUniform(progname, index, uniformNameMaxLength, &length,
                              &size, &uniformType, uniformName);
        if (gl->fGetUniformLocation(progname, uniformName) == location->Location())
            break;

        // now we handle the case of array uniforms. In that case, fGetActiveUniform returned as 'size'
        // the biggest index used plus one, so we need to loop over that. The 0 index has already been handled above,
        // so we can start at one. For each index, we construct the string uniformName + "[" + index + "]".
        if (size > 1) {
            bool found_it = false;
            if (uniformName[length - 1] == ']') { // if uniformName ends in [0]
                // remove the [0] at the end
                length -= 3;
                uniformName[length] = 0;
            }
            for (GLint arrayIndex = 1; arrayIndex < size; arrayIndex++) {
                sprintf(uniformNameBracketIndex.get(), "%s[%d]", uniformName.get(), arrayIndex);
                if (gl->fGetUniformLocation(progname, uniformNameBracketIndex) == location->Location()) {
                    found_it = true;
                    break;
                }
            }
            if (found_it) break;
        }
    }

    if (index == uniforms) {
        GenerateWarning("getUniform: internal error: hit an OpenGL driver bug");
        return JS::NullValue();
    }

    GLenum baseType;
    GLint unitSize;
    if (!BaseTypeAndSizeFromUniformType(uniformType, &baseType, &unitSize)) {
        GenerateWarning("getUniform: internal error: unknown uniform type 0x%x", uniformType);
        return JS::NullValue();
    }

    // this should never happen
    if (unitSize > 16) {
        GenerateWarning("getUniform: internal error: unexpected uniform unit size %d", unitSize);
        return JS::NullValue();
    }

    if (baseType == LOCAL_GL_FLOAT) {
        GLfloat fv[16] = { GLfloat(0) };
        gl->fGetUniformfv(progname, location->Location(), fv);
        if (unitSize == 1) {
            return JS::DoubleValue(fv[0]);
        } else {
            JSObject* obj = Float32Array::Create(cx, this, unitSize, fv);
            if (!obj) {
                ErrorOutOfMemory("getUniform: out of memory");
                return JS::NullValue();
            }
            return JS::ObjectOrNullValue(obj);
        }
    } else if (baseType == LOCAL_GL_INT) {
        GLint iv[16] = { 0 };
        gl->fGetUniformiv(progname, location->Location(), iv);
        if (unitSize == 1) {
            return JS::Int32Value(iv[0]);
        } else {
            JSObject* obj = Int32Array::Create(cx, this, unitSize, iv);
            if (!obj) {
                ErrorOutOfMemory("getUniform: out of memory");
                return JS::NullValue();
            }
            return JS::ObjectOrNullValue(obj);
        }
    } else if (baseType == LOCAL_GL_BOOL) {
        GLint iv[16] = { 0 };
        gl->fGetUniformiv(progname, location->Location(), iv);
        if (unitSize == 1) {
            return JS::BooleanValue(iv[0] ? true : false);
        } else {
            bool uv[16];
            for (int k = 0; k < unitSize; k++)
                uv[k] = iv[k];
            JS::Rooted<JS::Value> val(cx);
            // Be careful: we don't want to convert all of |uv|!
            if (!ToJSValue(cx, uv, unitSize, &val)) {
                ErrorOutOfMemory("getUniform: out of memory");
                return JS::NullValue();
            }
            return val;
        }
    }

    // Else preserving behavior, but I'm not sure this is correct per spec
    return JS::UndefinedValue();
}

already_AddRefed<WebGLUniformLocation>
WebGLContext::GetUniformLocation(WebGLProgram *prog, const nsAString& name)
{
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("getUniformLocation: program", prog))
        return nullptr;

    if (!ValidateGLSLVariableName(name, "getUniformLocation"))
        return nullptr;

    NS_LossyConvertUTF16toASCII cname(name);
    nsCString mappedName;
    prog->MapIdentifier(cname, &mappedName);

    GLuint progname = prog->GLName();
    MakeContextCurrent();
    GLint intlocation = gl->fGetUniformLocation(progname, mappedName.get());

    nsRefPtr<WebGLUniformLocation> loc;
    if (intlocation >= 0) {
        WebGLUniformInfo info = prog->GetUniformInfoForMappedIdentifier(mappedName);
        loc = new WebGLUniformLocation(this,
                                       prog,
                                       intlocation,
                                       info);
    }
    return loc.forget();
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
            if (IsExtensionEnabled(OES_standard_derivatives))
                isValid = true;
            break;
    }

    if (!isValid)
        return ErrorInvalidEnum("hint: invalid hint");

    MakeContextCurrent();
    gl->fHint(target, mode);
}

bool
WebGLContext::IsFramebuffer(WebGLFramebuffer *fb)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isFramebuffer", fb) &&
        !fb->IsDeleted() &&
        fb->HasEverBeenBound();
}

bool
WebGLContext::IsProgram(WebGLProgram *prog)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isProgram", prog) && !prog->IsDeleted();
}

bool
WebGLContext::IsRenderbuffer(WebGLRenderbuffer *rb)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isRenderBuffer", rb) &&
        !rb->IsDeleted() &&
        rb->HasEverBeenBound();
}

bool
WebGLContext::IsShader(WebGLShader *shader)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isShader", shader) &&
        !shader->IsDeleted();
}

bool
WebGLContext::IsTexture(WebGLTexture *tex)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isTexture", tex) &&
        !tex->IsDeleted() &&
        tex->HasEverBeenBound();
}

// Try to bind an attribute that is an array to location 0:
bool WebGLContext::BindArrayAttribToLocation0(WebGLProgram *program)
{
    if (mBoundVertexArray->IsAttribArrayEnabled(0)) {
        return false;
    }

    GLint leastArrayLocation = -1;

    std::map<GLint, nsCString>::iterator itr;
    for (itr = program->mActiveAttribMap.begin();
         itr != program->mActiveAttribMap.end();
         itr++) {
        int32_t index = itr->first;
        if (mBoundVertexArray->IsAttribArrayEnabled(index) &&
            index < leastArrayLocation)
        {
            leastArrayLocation = index;
        }
    }

    if (leastArrayLocation > 0) {
        nsCString& attrName = program->mActiveAttribMap.find(leastArrayLocation)->second;
        const char* attrNameCStr = attrName.get();
        gl->fBindAttribLocation(program->GLName(), 0, attrNameCStr);
        return true;
    }
    return false;
}

void
WebGLContext::LinkProgram(WebGLProgram *program)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("linkProgram", program))
        return;

    InvalidateBufferFetching(); // we do it early in this function
    // as some of the validation below changes program state

    GLuint progname = program->GLName();

    if (!program->NextGeneration()) {
        // XXX throw?
        return;
    }

    if (!program->HasBothShaderTypesAttached()) {
        GenerateWarning("linkProgram: this program doesn't have both a vertex shader"
                        " and a fragment shader");
        program->SetLinkStatus(false);
        return;
    }

    // bug 777028
    // Mesa can't handle more than 16 samplers per program, counting each array entry.
    if (gl->WorkAroundDriverBugs() &&
        mIsMesa &&
        program->UpperBoundNumSamplerUniforms() > 16)
    {
        GenerateWarning("Programs with more than 16 samplers are disallowed on Mesa drivers " "to avoid a Mesa crasher.");
        program->SetLinkStatus(false);
        return;
    }

    bool updateInfoSucceeded = false;
    GLint ok = 0;
    if (gl->WorkAroundDriverBugs() &&
        program->HasBadShaderAttached())
    {
        // it's a common driver bug, caught by program-test.html, that linkProgram doesn't
        // correctly preserve the state of an in-use program that has been attached a bad shader
        // see bug 777883
        ok = false;
    } else {
        MakeContextCurrent();
        gl->fLinkProgram(progname);
        gl->fGetProgramiv(progname, LOCAL_GL_LINK_STATUS, &ok);

        if (ok) {
            updateInfoSucceeded = program->UpdateInfo();
            program->SetLinkStatus(updateInfoSucceeded);

            if (BindArrayAttribToLocation0(program)) {
                GenerateWarning("linkProgram: relinking program to make attrib0 an "
                                "array.");
                gl->fLinkProgram(progname);
                gl->fGetProgramiv(progname, LOCAL_GL_LINK_STATUS, &ok);
                if (ok) {
                    updateInfoSucceeded = program->UpdateInfo();
                    program->SetLinkStatus(updateInfoSucceeded);
                }
            }
        }
    }

    if (ok) {
        // Bug 750527
        if (gl->WorkAroundDriverBugs() &&
            updateInfoSucceeded &&
            gl->Vendor() == gl::GLVendor::NVIDIA)
        {
            if (program == mCurrentProgram)
                gl->fUseProgram(progname);
        }
    } else {
        program->SetLinkStatus(false);

        if (ShouldGenerateWarnings()) {

            // report shader/program infoLogs as warnings.
            // note that shader compilation errors can be deferred to linkProgram,
            // which is why we can't do anything in compileShader. In practice we could
            // report in compileShader the translation errors generated by ANGLE,
            // but it seems saner to keep a single way of obtaining shader infologs.

            nsAutoCString log;

            bool alreadyReportedShaderInfoLog = false;

            for (size_t i = 0; i < program->AttachedShaders().Length(); i++) {

                WebGLShader* shader = program->AttachedShaders()[i];

                if (shader->CompileStatus())
                    continue;

                const char *shaderTypeName = nullptr;
                if (shader->ShaderType() == LOCAL_GL_VERTEX_SHADER) {
                    shaderTypeName = "vertex";
                } else if (shader->ShaderType() == LOCAL_GL_FRAGMENT_SHADER) {
                    shaderTypeName = "fragment";
                } else {
                    // should have been validated earlier
                    MOZ_ASSERT(false);
                    shaderTypeName = "<unknown>";
                }

                GetShaderInfoLog(shader, log);

                GenerateWarning("linkProgram: a %s shader used in this program failed to "
                                "compile, with this log:\n%s\n",
                                shaderTypeName,
                                log.get());
                alreadyReportedShaderInfoLog = true;
            }

            if (!alreadyReportedShaderInfoLog) {
                GetProgramInfoLog(program, log);
                if (!log.IsEmpty()) {
                    GenerateWarning("linkProgram failed, with this log:\n%s\n",
                                    log.get());
                }
            }
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

    const WebGLRectangleObject* framebufferRect = CurValidFBRectObject();
    GLsizei framebufferWidth = framebufferRect ? framebufferRect->Width() : 0;
    GLsizei framebufferHeight = framebufferRect ? framebufferRect->Height() : 0;

    uint32_t channels = 0;

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
            return ErrorInvalidEnum("readPixels: Bad format");
    }

    uint32_t bytesPerPixel = 0;
    int requiredDataType = 0;

    // Check the type param
    bool isReadTypeValid = false;
    bool isReadTypeFloat = false;
    switch (type) {
        case LOCAL_GL_UNSIGNED_BYTE:
            isReadTypeValid = true;
            bytesPerPixel = 1*channels;
            requiredDataType = js::ArrayBufferView::TYPE_UINT8;
            break;
        case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
            isReadTypeValid = true;
            bytesPerPixel = 2;
            requiredDataType = js::ArrayBufferView::TYPE_UINT16;
            break;
        case LOCAL_GL_FLOAT:
            if (IsExtensionEnabled(WEBGL_color_buffer_float) ||
                IsExtensionEnabled(EXT_color_buffer_half_float))
            {
                isReadTypeValid = true;
                isReadTypeFloat = true;
                bytesPerPixel = 4*channels;
                requiredDataType = js::ArrayBufferView::TYPE_FLOAT32;
            }
            break;
    }
    if (!isReadTypeValid)
        return ErrorInvalidEnum("readPixels: Bad type", type);

    int dataType = JS_GetArrayBufferViewType(pixels.Value().Obj());

    // Check the pixels param type
    if (dataType != requiredDataType)
        return ErrorInvalidOperation("readPixels: Mismatched type/pixels types");

    // Check the pixels param size
    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, bytesPerPixel, mPixelStorePackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * bytesPerPixel;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize, mPixelStorePackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("readPixels: integer overflow computing the needed buffer size");

    uint32_t dataByteLen = JS_GetTypedArrayByteLength(pixels.Value().Obj());
    if (checked_neededByteLength.value() > dataByteLen)
        return ErrorInvalidOperation("readPixels: buffer too small");

    void* data = pixels.Value().Data();
    if (!data) {
        ErrorOutOfMemory("readPixels: buffer storage is null. Did we run out of memory?");
        return rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    }

    bool isSourceTypeFloat = false;
    if (mBoundFramebuffer &&
        mBoundFramebuffer->ColorAttachmentCount() &&
        mBoundFramebuffer->ColorAttachment(0).IsDefined())
    {
        isSourceTypeFloat = mBoundFramebuffer->ColorAttachment(0).IsReadableFloat();
    }

    if (isReadTypeFloat != isSourceTypeFloat)
        return ErrorInvalidOperation("readPixels: Invalid type floatness");

    // Check the format and type params to assure they are an acceptable pair (as per spec)
    switch (format) {
        case LOCAL_GL_RGBA: {
            switch (type) {
                case LOCAL_GL_UNSIGNED_BYTE:
                    break;
                case LOCAL_GL_FLOAT:
                    break;
                default:
                    return ErrorInvalidOperation("readPixels: Invalid format/type pair");
            }
            break;
        }
        default:
            return ErrorInvalidOperation("readPixels: Invalid format/type pair");
    }

    MakeContextCurrent();

    if (mBoundFramebuffer) {
        // prevent readback of arbitrary video memory through uninitialized renderbuffers!
        if (!mBoundFramebuffer->CheckAndInitializeAttachments())
            return ErrorInvalidFramebufferOperation("readPixels: incomplete framebuffer");

        GLenum readPlaneBits = LOCAL_GL_COLOR_BUFFER_BIT;
        if (!mBoundFramebuffer->HasCompletePlanes(readPlaneBits)) {
            return ErrorInvalidOperation("readPixels: Read source attachment doesn't have the"
                                         " correct color/depth/stencil type.");
        }
    }
    // Now that the errors are out of the way, on to actually reading

    // If we won't be reading any pixels anyways, just skip the actual reading
    if (width == 0 || height == 0)
        return DummyFramebufferOperation("readPixels");

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, framebufferWidth, framebufferHeight)) {
        // the easy case: we're not reading out-of-range pixels
        gl->fReadPixels(x, y, width, height, format, type, data);
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
        GLubyte *subrect_data = new GLubyte[subrect_byteLength];
        gl->fReadPixels(subrect_x, subrect_y, subrect_width, subrect_height, format, type, subrect_data);

        // notice that this for loop terminates because we already checked that subrect_height is at most height
        for (GLint y_inside_subrect = 0; y_inside_subrect < subrect_height; ++y_inside_subrect) {
            GLint subrect_x_in_dest_buffer = subrect_x - x;
            GLint subrect_y_in_dest_buffer = subrect_y - y;
            memcpy(static_cast<GLubyte*>(data)
                     + checked_alignedRowSize.value() * (subrect_y_in_dest_buffer + y_inside_subrect)
                     + bytesPerPixel * subrect_x_in_dest_buffer, // destination
                   subrect_data + subrect_alignedRowSize * y_inside_subrect, // source
                   subrect_plainRowSize); // size
        }
        delete [] subrect_data;
    }

    // if we're reading alpha, we may need to do fixup.  Note that we don't allow
    // GL_ALPHA to readpixels currently, but we had the code written for it already.
    if (format == LOCAL_GL_ALPHA ||
        format == LOCAL_GL_RGBA)
    {
        bool needAlphaFixup;
        if (mBoundFramebuffer) {
            needAlphaFixup = !mBoundFramebuffer->ColorAttachment(0).HasAlpha();
        } else {
            needAlphaFixup = gl->GetPixelFormat().alpha == 0;
        }

        if (needAlphaFixup) {
            if (format == LOCAL_GL_ALPHA && type == LOCAL_GL_UNSIGNED_BYTE) {
                // this is easy; it's an 0xff memset per row
                uint8_t *row = static_cast<uint8_t*>(data);
                for (GLint j = 0; j < height; ++j) {
                    memset(row, 0xff, checked_plainRowSize.value());
                    row += checked_alignedRowSize.value();
                }
            } else if (format == LOCAL_GL_RGBA && type == LOCAL_GL_UNSIGNED_BYTE) {
                // this is harder, we need to just set the alpha byte here
                uint8_t *row = static_cast<uint8_t*>(data);
                for (GLint j = 0; j < height; ++j) {
                    uint8_t *rowp = row;
#if MOZ_LITTLE_ENDIAN
                    // offset to get the alpha byte; we're always going to
                    // move by 4 bytes
                    rowp += 3;
#endif
                    uint8_t *endrowp = rowp + 4 * width;
                    while (rowp != endrowp) {
                        *rowp = 0xff;
                        rowp += 4;
                    }

                    row += checked_alignedRowSize.value();
                }
            } else if (format == LOCAL_GL_RGBA && type == LOCAL_GL_FLOAT) {
                float* row = static_cast<float*>(data);

                for (GLint j = 0; j < height; ++j) {
                    float* pAlpha = row + 3;
                    float* pAlphaEnd = pAlpha + 4*width;

                    while (pAlpha != pAlphaEnd) {
                        *pAlpha = 1.0f;
                        pAlpha += 4;
                    }

                    row += checked_alignedRowSize.value();
                }
            } else {
                NS_WARNING("Unhandled case, how'd we get here?");
                return rv.Throw(NS_ERROR_FAILURE);
            }
        }
    }
}

void
WebGLContext::RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    if (!mBoundRenderbuffer)
        return ErrorInvalidOperation("renderbufferStorage called on renderbuffer 0");

    if (target != LOCAL_GL_RENDERBUFFER)
        return ErrorInvalidEnumInfo("renderbufferStorage: target", target);

    if (width < 0 || height < 0)
        return ErrorInvalidValue("renderbufferStorage: width and height must be >= 0");

    if (width > mGLMaxRenderbufferSize || height > mGLMaxRenderbufferSize)
        return ErrorInvalidValue("renderbufferStorage: width or height exceeds maximum renderbuffer size");

    // certain OpenGL ES renderbuffer formats may not exist on desktop OpenGL
    GLenum internalformatForGL = internalformat;

    switch (internalformat) {
    case LOCAL_GL_RGBA4:
    case LOCAL_GL_RGB5_A1:
        // 16-bit RGBA formats are not supported on desktop GL
        if (!gl->IsGLES()) internalformatForGL = LOCAL_GL_RGBA8;
        break;
    case LOCAL_GL_RGB565:
        // the RGB565 format is not supported on desktop GL
        if (!gl->IsGLES()) internalformatForGL = LOCAL_GL_RGB8;
        break;
    case LOCAL_GL_DEPTH_COMPONENT16:
        if (!gl->IsGLES() || gl->IsExtensionSupported(gl::GLContext::OES_depth24))
            internalformatForGL = LOCAL_GL_DEPTH_COMPONENT24;
        else if (gl->IsExtensionSupported(gl::GLContext::OES_packed_depth_stencil))
            internalformatForGL = LOCAL_GL_DEPTH24_STENCIL8;
        break;
    case LOCAL_GL_STENCIL_INDEX8:
        break;
    case LOCAL_GL_DEPTH_STENCIL:
        // We emulate this in WebGLRenderbuffer if we don't have the requisite extension.
        internalformatForGL = LOCAL_GL_DEPTH24_STENCIL8;
        break;
    case LOCAL_GL_SRGB8_ALPHA8_EXT:
        break;
    case LOCAL_GL_RGB16F:
    case LOCAL_GL_RGBA16F: {
        bool hasExtensions = IsExtensionEnabled(OES_texture_half_float) &&
                             IsExtensionEnabled(EXT_color_buffer_half_float);
        if (!hasExtensions)
            return ErrorInvalidEnumInfo("renderbufferStorage: internalformat", target);
        break;
    }
    case LOCAL_GL_RGB32F:
    case LOCAL_GL_RGBA32F: {
        bool hasExtensions = IsExtensionEnabled(OES_texture_float) &&
                             IsExtensionEnabled(WEBGL_color_buffer_float);
        if (!hasExtensions)
            return ErrorInvalidEnumInfo("renderbufferStorage: internalformat", target);
        break;
    }
    default:
        return ErrorInvalidEnumInfo("renderbufferStorage: internalformat", internalformat);
    }

    MakeContextCurrent();

    bool sizeChanges = width != mBoundRenderbuffer->Width() ||
                       height != mBoundRenderbuffer->Height() ||
                       internalformat != mBoundRenderbuffer->InternalFormat();
    if (sizeChanges) {
        // Invalidate framebuffer status cache
        mBoundRenderbuffer->NotifyFBsStatusChanged();
        GetAndFlushUnderlyingGLErrors();
        mBoundRenderbuffer->RenderbufferStorage(internalformatForGL, width, height);
        GLenum error = GetAndFlushUnderlyingGLErrors();
        if (error) {
            GenerateWarning("renderbufferStorage generated error %s", ErrorName(error));
            return;
        }
    } else {
        mBoundRenderbuffer->RenderbufferStorage(internalformatForGL, width, height);
    }

    mBoundRenderbuffer->SetInternalFormat(internalformat);
    mBoundRenderbuffer->SetInternalFormatForGL(internalformatForGL);
    mBoundRenderbuffer->setDimensions(width, height);
    mBoundRenderbuffer->SetImageDataStatus(WebGLImageDataStatus::UninitializedImageData);
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
                                                     RefPtr<DataSourceSurface>& imageOut, WebGLTexelFormat *format)
{
   *format = WebGLTexelFormat::None;

    if (!res.mSourceSurface)
        return NS_OK;
    RefPtr<DataSourceSurface> data = res.mSourceSurface->GetDataSurface();
    if (!data) {
        // SurfaceFromElement lied!
        return NS_OK;
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



void
WebGLContext::Uniform1i(WebGLUniformLocation *location_object, GLint a1)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform1i", location_object, location))
        return;

    // Only uniform1i can take sampler settings.
    if (!ValidateSamplerUniformSetter("Uniform1i", location_object, a1))
        return;

    MakeContextCurrent();
    gl->fUniform1i(location, a1);
}

void
WebGLContext::Uniform2i(WebGLUniformLocation *location_object, GLint a1,
                        GLint a2)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform2i", location_object, location))
        return;

    MakeContextCurrent();
    gl->fUniform2i(location, a1, a2);
}

void
WebGLContext::Uniform3i(WebGLUniformLocation *location_object, GLint a1,
                        GLint a2, GLint a3)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform3i", location_object, location))
        return;

    MakeContextCurrent();
    gl->fUniform3i(location, a1, a2, a3);
}

void
WebGLContext::Uniform4i(WebGLUniformLocation *location_object, GLint a1,
                        GLint a2, GLint a3, GLint a4)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform4i", location_object, location))
        return;

    MakeContextCurrent();
    gl->fUniform4i(location, a1, a2, a3, a4);
}

void
WebGLContext::Uniform1f(WebGLUniformLocation *location_object, GLfloat a1)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform1f", location_object, location))
        return;
    MakeContextCurrent();
    gl->fUniform1f(location, a1);
}

void
WebGLContext::Uniform2f(WebGLUniformLocation *location_object, GLfloat a1,
                        GLfloat a2)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform2f", location_object, location))
        return;
    MakeContextCurrent();
    gl->fUniform2f(location, a1, a2);
}

void
WebGLContext::Uniform3f(WebGLUniformLocation *location_object, GLfloat a1,
                        GLfloat a2, GLfloat a3)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform3f", location_object, location))
        return;
    MakeContextCurrent();
    gl->fUniform3f(location, a1, a2, a3);
}

void
WebGLContext::Uniform4f(WebGLUniformLocation *location_object, GLfloat a1,
                        GLfloat a2, GLfloat a3, GLfloat a4)
{
    GLint location;
    if (!ValidateUniformSetter("Uniform4f", location_object, location))
        return;
    MakeContextCurrent();
    gl->fUniform4f(location, a1, a2, a3, a4);
}

void
WebGLContext::Uniform1iv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLint* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform1iv", 1, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }

    if (!ValidateSamplerUniformSetter("Uniform1iv", location_object, data[0]))
        return;

    MakeContextCurrent();
    gl->fUniform1iv(location, numElementsToUpload, data);
}

void
WebGLContext::Uniform2iv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLint* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform2iv", 2, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }

    if (!ValidateSamplerUniformSetter("Uniform2iv", location_object, data[0]) ||
        !ValidateSamplerUniformSetter("Uniform2iv", location_object, data[1]))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform2iv(location, numElementsToUpload, data);
}

void
WebGLContext::Uniform3iv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLint* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform3iv", 3, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }

    if (!ValidateSamplerUniformSetter("Uniform3iv", location_object, data[0]) ||
        !ValidateSamplerUniformSetter("Uniform3iv", location_object, data[1]) ||
        !ValidateSamplerUniformSetter("Uniform3iv", location_object, data[2]))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform3iv(location, numElementsToUpload, data);
}

void
WebGLContext::Uniform4iv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLint* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform4iv", 4, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }

    if (!ValidateSamplerUniformSetter("Uniform4iv", location_object, data[0]) ||
        !ValidateSamplerUniformSetter("Uniform4iv", location_object, data[1]) ||
        !ValidateSamplerUniformSetter("Uniform4iv", location_object, data[2]) ||
        !ValidateSamplerUniformSetter("Uniform4iv", location_object, data[3]))
    {
        return;
    }

    MakeContextCurrent();
    gl->fUniform4iv(location, numElementsToUpload, data);
}

void
WebGLContext::Uniform1fv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLfloat* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform1fv", 1, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }
    MakeContextCurrent();
    gl->fUniform1fv(location, numElementsToUpload, data);
}

void
WebGLContext::Uniform2fv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLfloat* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform2fv", 2, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }
    MakeContextCurrent();
    gl->fUniform2fv(location, numElementsToUpload, data);
}

void
WebGLContext::Uniform3fv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLfloat* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform3fv", 3, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }
    MakeContextCurrent();
    gl->fUniform3fv(location, numElementsToUpload, data);
}

void
WebGLContext::Uniform4fv_base(WebGLUniformLocation *location_object,
                              uint32_t arrayLength, const GLfloat* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformArraySetter("Uniform4fv", 4, location_object, location,
                                    numElementsToUpload, arrayLength)) {
        return;
    }
    MakeContextCurrent();
    gl->fUniform4fv(location, numElementsToUpload, data);
}

void
WebGLContext::UniformMatrix2fv_base(WebGLUniformLocation* location_object,
                                    WebGLboolean aTranspose, uint32_t arrayLength,
                                    const float* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformMatrixArraySetter("UniformMatrix2fv", 2, location_object, location,
                                         numElementsToUpload, arrayLength, aTranspose)) {
        return;
    }
    MakeContextCurrent();
    gl->fUniformMatrix2fv(location, numElementsToUpload, false, data);
}

void
WebGLContext::UniformMatrix3fv_base(WebGLUniformLocation* location_object,
                                    WebGLboolean aTranspose, uint32_t arrayLength,
                                    const float* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformMatrixArraySetter("UniformMatrix3fv", 3, location_object, location,
                                         numElementsToUpload, arrayLength, aTranspose)) {
        return;
    }
    MakeContextCurrent();
    gl->fUniformMatrix3fv(location, numElementsToUpload, false, data);
}

void
WebGLContext::UniformMatrix4fv_base(WebGLUniformLocation* location_object,
                                    WebGLboolean aTranspose, uint32_t arrayLength,
                                    const float* data)
{
    uint32_t numElementsToUpload;
    GLint location;
    if (!ValidateUniformMatrixArraySetter("UniformMatrix4fv", 4, location_object, location,
                                         numElementsToUpload, arrayLength, aTranspose)) {
        return;
    }
    MakeContextCurrent();
    gl->fUniformMatrix4fv(location, numElementsToUpload, false, data);
}

void
WebGLContext::UseProgram(WebGLProgram *prog)
{
    if (IsContextLost())
        return;

    if (!ValidateObjectAllowNull("useProgram", prog))
        return;

    MakeContextCurrent();

    InvalidateBufferFetching();

    GLuint progname = prog ? prog->GLName() : 0;

    if (prog && !prog->LinkStatus())
        return ErrorInvalidOperation("useProgram: program was not linked successfully");

    gl->fUseProgram(progname);

    mCurrentProgram = prog;
}

void
WebGLContext::ValidateProgram(WebGLProgram *prog)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("validateProgram", prog))
        return;

    MakeContextCurrent();

#ifdef XP_MACOSX
    // see bug 593867 for NVIDIA and bug 657201 for ATI. The latter is confirmed with Mac OS 10.6.7
    if (gl->WorkAroundDriverBugs()) {
        GenerateWarning("validateProgram: implemented as a no-operation on Mac to work around crashes");
        return;
    }
#endif

    GLuint progname = prog->GLName();
    gl->fValidateProgram(progname);
}

already_AddRefed<WebGLFramebuffer>
WebGLContext::CreateFramebuffer()
{
    if (IsContextLost())
        return nullptr;
    nsRefPtr<WebGLFramebuffer> globj = new WebGLFramebuffer(this);
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
WebGLContext::CompileShader(WebGLShader *shader)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("compileShader", shader))
        return;

    GLuint shadername = shader->GLName();

    shader->SetCompileStatus(false);

    MakeContextCurrent();

    ShShaderOutput targetShaderSourceLanguage = gl->IsGLES() ? SH_ESSL_OUTPUT : SH_GLSL_OUTPUT;
    bool useShaderSourceTranslation = true;

    if (shader->NeedsTranslation() && mShaderValidation) {
        ShHandle compiler = 0;
        ShBuiltInResources resources;
        memset(&resources, 0, sizeof(ShBuiltInResources));

        resources.MaxVertexAttribs = mGLMaxVertexAttribs;
        resources.MaxVertexUniformVectors = mGLMaxVertexUniformVectors;
        resources.MaxVaryingVectors = mGLMaxVaryingVectors;
        resources.MaxVertexTextureImageUnits = mGLMaxVertexTextureImageUnits;
        resources.MaxCombinedTextureImageUnits = mGLMaxTextureUnits;
        resources.MaxTextureImageUnits = mGLMaxTextureImageUnits;
        resources.MaxFragmentUniformVectors = mGLMaxFragmentUniformVectors;
        resources.MaxDrawBuffers = mGLMaxDrawBuffers;

        if (IsExtensionEnabled(EXT_frag_depth))
            resources.EXT_frag_depth = 1;

        if (IsExtensionEnabled(OES_standard_derivatives))
            resources.OES_standard_derivatives = 1;

        if (IsExtensionEnabled(WEBGL_draw_buffers))
            resources.EXT_draw_buffers = 1;

        // Tell ANGLE to allow highp in frag shaders. (unless disabled)
        // If underlying GLES doesn't have highp in frag shaders, it should complain anyways.
        resources.FragmentPrecisionHigh = mDisableFragHighP ? 0 : 1;

        if (gl->WorkAroundDriverBugs()) {
#ifdef XP_MACOSX
            if (gl->Vendor() == gl::GLVendor::NVIDIA) {
                // Work around bug 890432
                resources.MaxExpressionComplexity = 1000;
            }
#endif
        }

        // We're storing an actual instance of StripComments because, if we don't, the
        // cleanSource nsAString instance will be destroyed before the reference is
        // actually used.
        StripComments stripComments(shader->Source());
        const nsAString& cleanSource = Substring(stripComments.result().Elements(), stripComments.length());
        if (!ValidateGLSLString(cleanSource, "compileShader"))
            return;

        // shaderSource() already checks that the source stripped of comments is in the
        // 7-bit ASCII range, so we can skip the NS_IsAscii() check.
        NS_LossyConvertUTF16toASCII sourceCString(cleanSource);

        if (gl->WorkAroundDriverBugs()) {
            const uint32_t maxSourceLength = 0x3ffff;
            if (sourceCString.Length() > maxSourceLength)
                return ErrorInvalidValue("compileShader: source has more than %d characters",
                                         maxSourceLength);
        }

        const char *s = sourceCString.get();

#define WEBGL2_BYPASS_ANGLE
#ifdef WEBGL2_BYPASS_ANGLE
        /*
         * The bypass don't bring a full support for GLSL ES 3.0, but the main purpose
         * is to natively bring gl_InstanceID (to do instanced rendering) and gl_FragData
         *
         * To remove the bypass code, just comment #define WEBGL2_BYPASS_ANGLE above
         *
         * To bypass angle, the context must be a WebGL 2 and the shader must have the
         * following line at the very top :
         *      #version proto-200
         *
         * In this case, byPassANGLE == true and here is what we do :
         *  We create two shader source code:
         *    - one for the driver, that enable GL_EXT_gpu_shader4
         *    - one for the angle compilor, to get informations about vertex attributes
         *      and uniforms
         */
        static const char *bypassPrefixSearch = "#version proto-200";
        static const char *bypassANGLEPrefix[2] = {"precision mediump float;\n"
                                                   "#define gl_VertexID 0\n"
                                                   "#define gl_InstanceID 0\n",

                                                   "precision mediump float;\n"
                                                   "#extension GL_EXT_draw_buffers : enable\n"
                                                   "#define gl_PrimitiveID 0\n"};

        const bool bypassANGLE = IsWebGL2() && (strstr(s, bypassPrefixSearch) != 0);

        const char *angleShaderCode = s;
        nsTArray<char> bypassANGLEShaderCode;
        nsTArray<char> bypassDriverShaderCode;

        if (bypassANGLE) {
            const int bypassStage = (shader->ShaderType() == LOCAL_GL_FRAGMENT_SHADER) ? 1 : 0;
            const char *originalShader = strstr(s, bypassPrefixSearch) + strlen(bypassPrefixSearch);
            int originalShaderSize = strlen(s) - (originalShader - s);
            int bypassShaderCodeSize = originalShaderSize + 4096 + 1;

            bypassANGLEShaderCode.SetLength(bypassShaderCodeSize);
            strcpy(bypassANGLEShaderCode.Elements(), bypassANGLEPrefix[bypassStage]);
            strcat(bypassANGLEShaderCode.Elements(), originalShader);

            bypassDriverShaderCode.SetLength(bypassShaderCodeSize);
            strcpy(bypassDriverShaderCode.Elements(), "#extension GL_EXT_gpu_shader4 : enable\n");
            strcat(bypassDriverShaderCode.Elements(), originalShader);

            angleShaderCode = bypassANGLEShaderCode.Elements();
        }
#endif

        compiler = ShConstructCompiler((ShShaderType) shader->ShaderType(),
                                       SH_WEBGL_SPEC,
                                       targetShaderSourceLanguage,
                                       &resources);

        int compileOptions = SH_ATTRIBUTES_UNIFORMS |
                             SH_ENFORCE_PACKING_RESTRICTIONS;

        if (resources.MaxExpressionComplexity > 0) {
            compileOptions |= SH_LIMIT_EXPRESSION_COMPLEXITY;
        }

        // We want to do this everywhere, but:
#ifndef XP_MACOSX // To do this on Mac, we need to do it only on Mac OSX > 10.6 as this
                  // causes the shader compiler in 10.6 to crash
        compileOptions |= SH_CLAMP_INDIRECT_ARRAY_BOUNDS;
#endif

        if (useShaderSourceTranslation) {
            compileOptions |= SH_OBJECT_CODE
                            | SH_MAP_LONG_VARIABLE_NAMES;

#ifdef XP_MACOSX
            if (gl->WorkAroundDriverBugs()) {
                // Work around bug 665578 and bug 769810
                if (gl->Vendor() == gl::GLVendor::ATI) {
                    compileOptions |= SH_EMULATE_BUILT_IN_FUNCTIONS;
                }

                // Work around bug 735560
                if (gl->Vendor() == gl::GLVendor::Intel) {
                    compileOptions |= SH_EMULATE_BUILT_IN_FUNCTIONS;
                }
            }
#endif
        }

#ifdef WEBGL2_BYPASS_ANGLE
        if (!ShCompile(compiler, &angleShaderCode, 1, compileOptions)) {
#else
        if (!ShCompile(compiler, &s, 1, compileOptions)) {
#endif
            size_t lenWithNull = 0;
            ShGetInfo(compiler, SH_INFO_LOG_LENGTH, &lenWithNull);

            if (!lenWithNull) {
                // Error in ShGetInfo.
                shader->SetTranslationFailure(NS_LITERAL_CSTRING("Internal error: failed to get shader info log"));
            } else {
                size_t len = lenWithNull - 1;

                nsAutoCString info;
                info.SetLength(len); // Allocates len+1, for the null-term.
                ShGetInfoLog(compiler, info.BeginWriting());

                shader->SetTranslationFailure(info);
            }
            ShDestruct(compiler);
            shader->SetCompileStatus(false);
            return;
        }

        size_t num_attributes = 0;
        ShGetInfo(compiler, SH_ACTIVE_ATTRIBUTES, &num_attributes);
        size_t num_uniforms = 0;
        ShGetInfo(compiler, SH_ACTIVE_UNIFORMS, &num_uniforms);
        size_t attrib_max_length = 0;
        ShGetInfo(compiler, SH_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attrib_max_length);
        size_t uniform_max_length = 0;
        ShGetInfo(compiler, SH_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_max_length);
        size_t mapped_max_length = 0;
        ShGetInfo(compiler, SH_MAPPED_NAME_MAX_LENGTH, &mapped_max_length);

        shader->mAttribMaxNameLength = attrib_max_length;

        shader->mAttributes.Clear();
        shader->mUniforms.Clear();
        shader->mUniformInfos.Clear();

        nsAutoArrayPtr<char> attribute_name(new char[attrib_max_length+1]);
        nsAutoArrayPtr<char> uniform_name(new char[uniform_max_length+1]);
        nsAutoArrayPtr<char> mapped_name(new char[mapped_max_length+1]);

        for (size_t i = 0; i < num_uniforms; i++) {
            size_t length;
            int size;
            ShDataType type;
            ShGetActiveUniform(compiler, (int)i,
                                &length, &size, &type,
                                uniform_name,
                                mapped_name);
            if (useShaderSourceTranslation) {
                shader->mUniforms.AppendElement(WebGLMappedIdentifier(
                                                    nsDependentCString(uniform_name),
                                                    nsDependentCString(mapped_name)));
            }

            // we always query uniform info, regardless of useShaderSourceTranslation,
            // as we need it to validate uniform setter calls, and it doesn't rely on
            // shader translation.
            char mappedNameLength = strlen(mapped_name);
            char mappedNameLastChar = mappedNameLength > 1
                                      ? mapped_name[mappedNameLength - 1]
                                      : 0;
            shader->mUniformInfos.AppendElement(WebGLUniformInfo(
                                                    size,
                                                    mappedNameLastChar == ']',
                                                    type));
        }

        if (useShaderSourceTranslation) {

            for (size_t i = 0; i < num_attributes; i++) {
                size_t length;
                int size;
                ShDataType type;
                ShGetActiveAttrib(compiler, (int)i,
                                  &length, &size, &type,
                                  attribute_name,
                                  mapped_name);
                shader->mAttributes.AppendElement(WebGLMappedIdentifier(
                                                    nsDependentCString(attribute_name),
                                                    nsDependentCString(mapped_name)));
            }

            size_t lenWithNull = 0;
            ShGetInfo(compiler, SH_OBJECT_CODE_LENGTH, &lenWithNull);
            MOZ_ASSERT(lenWithNull >= 1);
            size_t len = lenWithNull - 1;

            nsAutoCString translatedSrc;
            translatedSrc.SetLength(len); // Allocates len+1, for the null-term.
            ShGetObjectCode(compiler, translatedSrc.BeginWriting());

            CopyASCIItoUTF16(translatedSrc, shader->mTranslatedSource);

            const char *ts = translatedSrc.get();

#ifdef WEBGL2_BYPASS_ANGLE
            if (bypassANGLE) {
                const char* driverShaderCode = bypassDriverShaderCode.Elements();
                gl->fShaderSource(shadername, 1, (const GLchar**) &driverShaderCode, nullptr);
            }
            else {
                gl->fShaderSource(shadername, 1, &ts, nullptr);
            }
#else
            gl->fShaderSource(shadername, 1, &ts, nullptr);
#endif
        } else { // not useShaderSourceTranslation
            // we just pass the raw untranslated shader source. We then can't use ANGLE idenfier mapping.
            // that's really bad, as that means we can't be 100% conformant. We should work towards always
            // using ANGLE identifier mapping.
            gl->fShaderSource(shadername, 1, &s, nullptr);

            CopyASCIItoUTF16(s, shader->mTranslatedSource);
        }

        shader->SetTranslationSuccess();

        ShDestruct(compiler);

        gl->fCompileShader(shadername);
        GLint ok;
        gl->fGetShaderiv(shadername, LOCAL_GL_COMPILE_STATUS, &ok);
        shader->SetCompileStatus(ok);
    }
}

void
WebGLContext::CompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                                   GLsizei width, GLsizei height, GLint border,
                                   const ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexImage;

    if (!ValidateTexImage(2, target, level, internalformat,
                          0, 0, 0, width, height, 0,
                          border, internalformat, LOCAL_GL_UNSIGNED_BYTE,
                          func))
    {
        return;
    }

    uint32_t byteLength = view.Length();
    if (!ValidateCompTexImageDataSize(target, internalformat, width, height, byteLength, func)) {
        return;
    }

    if (!ValidateCompTexImageSize(target, level, internalformat, 0, 0,
                                  width, height, width, height, func))
    {
        return;
    }

    MakeContextCurrent();
    gl->fCompressedTexImage2D(target, level, internalformat, width, height, border, byteLength, view.Data());
    WebGLTexture* tex = activeBoundTextureForTarget(target);
    MOZ_ASSERT(tex);
    tex->SetImageInfo(target, level, width, height, internalformat, LOCAL_GL_UNSIGNED_BYTE,
                      WebGLImageDataStatus::InitializedImageData);
}

void
WebGLContext::CompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLsizei width, GLsizei height,
                                      GLenum format, const ArrayBufferView& view)
{
    if (IsContextLost())
        return;

    const WebGLTexImageFunc func = WebGLTexImageFunc::CompTexSubImage;

    if (!ValidateTexImage(2, target,
                          level, format,
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0, format, LOCAL_GL_UNSIGNED_BYTE,
                          func))
    {
        return;
    }

    WebGLTexture *tex = activeBoundTextureForTarget(target);
    MOZ_ASSERT(tex);
    WebGLTexture::ImageInfo& levelInfo = tex->ImageInfoAt(target, level);

    uint32_t byteLength = view.Length();
    if (!ValidateCompTexImageDataSize(target, format, width, height, byteLength, func))
        return;

    if (!ValidateCompTexImageSize(target, level, format,
                                  xoffset, yoffset,
                                  width, height,
                                  levelInfo.Width(), levelInfo.Height(),
                                  func))
    {
        return;
    }

    if (levelInfo.HasUninitializedImageData())
        tex->DoDeferredImageInitialization(target, level);

    MakeContextCurrent();
    gl->fCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, byteLength, view.Data());
}

JS::Value
WebGLContext::GetShaderParameter(WebGLShader *shader, GLenum pname)
{
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObject("getShaderParameter: shader", shader))
        return JS::NullValue();

    GLuint shadername = shader->GLName();

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_SHADER_TYPE:
        {
            GLint i = 0;
            gl->fGetShaderiv(shadername, pname, &i);
            return JS::NumberValue(uint32_t(i));
        }
            break;
        case LOCAL_GL_DELETE_STATUS:
            return JS::BooleanValue(shader->IsDeleteRequested());
            break;
        case LOCAL_GL_COMPILE_STATUS:
        {
            GLint i = 0;
            gl->fGetShaderiv(shadername, pname, &i);
            return JS::BooleanValue(bool(i));
        }
            break;
        default:
            ErrorInvalidEnumInfo("getShaderParameter: parameter", pname);
    }

    return JS::NullValue();
}

void
WebGLContext::GetShaderInfoLog(WebGLShader *shader, nsAString& retval)
{
    nsAutoCString s;
    GetShaderInfoLog(shader, s);
    if (s.IsVoid())
        retval.SetIsVoid(true);
    else
        CopyASCIItoUTF16(s, retval);
}

void
WebGLContext::GetShaderInfoLog(WebGLShader *shader, nsACString& retval)
{
    if (IsContextLost())
    {
        retval.SetIsVoid(true);
        return;
    }

    if (!ValidateObject("getShaderInfoLog: shader", shader))
        return;

    retval = shader->TranslationLog();
    if (!retval.IsVoid()) {
        return;
    }

    MakeContextCurrent();

    GLuint shadername = shader->GLName();
    GLint k = -1;
    gl->fGetShaderiv(shadername, LOCAL_GL_INFO_LOG_LENGTH, &k);
    if (k == -1) {
        // XXX GL Error? should never happen.
        return;
    }

    if (k == 0) {
        retval.Truncate();
        return;
    }

    retval.SetCapacity(k);
    gl->fGetShaderInfoLog(shadername, k, &k, (char*) retval.BeginWriting());
    retval.SetLength(k);
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
WebGLContext::GetShaderSource(WebGLShader *shader, nsAString& retval)
{
    if (IsContextLost()) {
        retval.SetIsVoid(true);
        return;
    }

    if (!ValidateObject("getShaderSource: shader", shader))
        return;

    retval.Assign(shader->Source());
}

void
WebGLContext::ShaderSource(WebGLShader *shader, const nsAString& source)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("shaderSource: shader", shader))
        return;

    // We're storing an actual instance of StripComments because, if we don't, the
    // cleanSource nsAString instance will be destroyed before the reference is
    // actually used.
    StripComments stripComments(source);
    const nsAString& cleanSource = Substring(stripComments.result().Elements(), stripComments.length());
    if (!ValidateGLSLString(cleanSource, "compileShader"))
        return;

    shader->SetSource(source);

    shader->SetNeedsTranslation();
}

void
WebGLContext::GetShaderTranslatedSource(WebGLShader *shader, nsAString& retval)
{
    if (IsContextLost()) {
        retval.SetIsVoid(true);
        return;
    }

    if (!ValidateObject("getShaderTranslatedSource: shader", shader))
        return;

    retval.Assign(shader->TranslatedSource());
}

GLenum WebGLContext::CheckedTexImage2D(GLenum target,
                                       GLint level,
                                       GLenum internalFormat,
                                       GLsizei width,
                                       GLsizei height,
                                       GLint border,
                                       GLenum format,
                                       GLenum type,
                                       const GLvoid *data)
{
    WebGLTexture *tex = activeBoundTextureForTarget(target);
    MOZ_ASSERT(tex != nullptr, "no texture bound");

    bool sizeMayChange = true;

    if (tex->HasImageInfoAt(target, level)) {
        const WebGLTexture::ImageInfo& imageInfo = tex->ImageInfoAt(target, level);
        sizeMayChange = width != imageInfo.Width() ||
                        height != imageInfo.Height() ||
                        format != imageInfo.InternalFormat() ||
                        type != imageInfo.Type();
    }

    // convert type for half float if not on GLES2
    GLenum realType = type;
    if (realType == LOCAL_GL_HALF_FLOAT_OES && !gl->IsGLES()) {
        realType = LOCAL_GL_HALF_FLOAT;
    }

    if (sizeMayChange) {
        GetAndFlushUnderlyingGLErrors();

        gl->fTexImage2D(target, level, internalFormat, width, height, border, format, realType, data);

        GLenum error = GetAndFlushUnderlyingGLErrors();
        return error;
    }

    gl->fTexImage2D(target, level, internalFormat, width, height, border, format, realType, data);

    return LOCAL_GL_NO_ERROR;
}

void
WebGLContext::TexImage2D_base(GLenum target, GLint level, GLenum internalformat,
                              GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                              GLint border,
                              GLenum format, GLenum type,
                              void *data, uint32_t byteLength,
                              int jsArrayType, // a TypedArray format enum, or -1 if not relevant
                              WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexImage;

    if (!ValidateTexImage(2, target, level, internalformat,
                          0, 0, 0,
                          width, height, 0,
                          border, format, type, func))
    {
        return;
    }

    const bool isDepthTexture = format == LOCAL_GL_DEPTH_COMPONENT ||
                                format == LOCAL_GL_DEPTH_STENCIL;

    if (isDepthTexture) {
        if (data != nullptr || level != 0)
            return ErrorInvalidOperation("texImage2D: "
                                         "with format of DEPTH_COMPONENT or DEPTH_STENCIL, "
                                         "data must be nullptr, "
                                         "level must be zero");
    }

    if (!ValidateTexInputData(type, jsArrayType, func))
        return;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(format, type);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    uint32_t srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(actualSrcFormat);

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, srcTexelSize, mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("texImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength && byteLength < bytesNeeded)
        return ErrorInvalidOperation("texImage2D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, byteLength);

    WebGLTexture *tex = activeBoundTextureForTarget(target);

    if (!tex)
        return ErrorInvalidOperation("texImage2D: no texture is bound to this target");

    MakeContextCurrent();

    // Handle ES2 and GL differences in floating point internal formats.  Note that
    // format == internalformat, as checked above and as required by ES.
    internalformat = InternalFormatForFormatAndType(format, type, gl->IsGLES());

    // Handle ES2 and GL differences when supporting sRGB internal formats. GL ES
    // requires that format == internalformat, but GL will fail in this case.
    // GL requires:
    //      format  ->  internalformat
    //      GL_RGB      GL_SRGB_EXT
    //      GL_RGBA     GL_SRGB_ALPHA_EXT
    if (!gl->IsGLES()) {
        switch (internalformat) {
            case LOCAL_GL_SRGB_EXT:
                format = LOCAL_GL_RGB;
                break;
            case LOCAL_GL_SRGB_ALPHA_EXT:
                format = LOCAL_GL_RGBA;
                break;
        }
    }

    GLenum error = LOCAL_GL_NO_ERROR;

    WebGLImageDataStatus imageInfoStatusIfSuccess = WebGLImageDataStatus::NoImageData;

    if (byteLength) {
        size_t srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();

        uint32_t dstTexelSize = GetBitsPerTexel(format, type) / 8;
        size_t dstPlainRowSize = dstTexelSize * width;
        size_t unpackAlignment = mPixelStoreUnpackAlignment;
        size_t dstStride = ((dstPlainRowSize + unpackAlignment-1) / unpackAlignment) * unpackAlignment;

        if (actualSrcFormat == dstFormat &&
            srcPremultiplied == mPixelStorePremultiplyAlpha &&
            srcStride == dstStride &&
            !mPixelStoreFlipY)
        {
            // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
            error = CheckedTexImage2D(target, level, internalformat,
                                      width, height, border, format, type, data);
        }
        else
        {
            size_t convertedDataSize = height * dstStride;
            nsAutoArrayPtr<uint8_t> convertedData(new uint8_t[convertedDataSize]);
            ConvertImage(width, height, srcStride, dstStride,
                        static_cast<uint8_t*>(data), convertedData,
                        actualSrcFormat, srcPremultiplied,
                        dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize);
            error = CheckedTexImage2D(target, level, internalformat,
                                      width, height, border, format, type, convertedData);
        }
        imageInfoStatusIfSuccess = WebGLImageDataStatus::InitializedImageData;
    } else {
        error = CheckedTexImage2D(target, level, internalformat,
                                  width, height, border, format, type, nullptr);
        imageInfoStatusIfSuccess = WebGLImageDataStatus::UninitializedImageData;
    }

    if (error) {
        GenerateWarning("texImage2D generated error %s", ErrorName(error));
        return;
    }

    // in all of the code paths above, we should have either initialized data,
    // or allocated data and left it uninitialized, but in any case we shouldn't
    // have NoImageData at this point.
    MOZ_ASSERT(imageInfoStatusIfSuccess != WebGLImageDataStatus::NoImageData);

    tex->SetImageInfo(target, level, width, height, internalformat, type, imageInfoStatusIfSuccess);
}

void
WebGLContext::TexImage2D(GLenum target, GLint level,
                         GLenum internalformat, GLsizei width,
                         GLsizei height, GLint border, GLenum format,
                         GLenum type, const Nullable<ArrayBufferView> &pixels, ErrorResult& rv)
{
    if (IsContextLost())
        return;

    return TexImage2D_base(target, level, internalformat, width, height, 0, border, format, type,
                           pixels.IsNull() ? 0 : pixels.Value().Data(),
                           pixels.IsNull() ? 0 : pixels.Value().Length(),
                           pixels.IsNull() ? -1 : (int)JS_GetArrayBufferViewType(pixels.Value().Obj()),
                           WebGLTexelFormat::Auto, false);
}

void
WebGLContext::TexImage2D(GLenum target, GLint level,
                         GLenum internalformat, GLenum format,
                         GLenum type, ImageData* pixels, ErrorResult& rv)
{
    if (IsContextLost())
        return;

    if (!pixels) {
        // Spec says to generate an INVALID_VALUE error
        return ErrorInvalidValue("texImage2D: null ImageData");
    }

    Uint8ClampedArray arr(pixels->GetDataObject());
    return TexImage2D_base(target, level, internalformat, pixels->Width(),
                           pixels->Height(), 4*pixels->Width(), 0,
                           format, type, arr.Data(), arr.Length(), -1,
                           WebGLTexelFormat::RGBA8, false);
}


void
WebGLContext::TexSubImage2D_base(GLenum target, GLint level,
                                 GLint xoffset, GLint yoffset,
                                 GLsizei width, GLsizei height, GLsizei srcStrideOrZero,
                                 GLenum format, GLenum type,
                                 void *pixels, uint32_t byteLength,
                                 int jsArrayType,
                                 WebGLTexelFormat srcFormat, bool srcPremultiplied)
{
    const WebGLTexImageFunc func = WebGLTexImageFunc::TexSubImage;

    if (!ValidateTexImage(2, target, level, format,
                          xoffset, yoffset, 0,
                          width, height, 0,
                          0, format, type, func))
    {
        return;
    }

    if (!ValidateTexInputData(type, jsArrayType, func))
        return;

    WebGLTexelFormat dstFormat = GetWebGLTexelFormat(format, type);
    WebGLTexelFormat actualSrcFormat = srcFormat == WebGLTexelFormat::Auto ? dstFormat : srcFormat;

    uint32_t srcTexelSize = WebGLTexelConversions::TexelBytesForFormat(actualSrcFormat);

    if (width == 0 || height == 0)
        return; // ES 2.0 says it has no effect, we better return right now

    CheckedUint32 checked_neededByteLength =
        GetImageSize(height, width, srcTexelSize, mPixelStoreUnpackAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * srcTexelSize;

    CheckedUint32 checked_alignedRowSize =
        RoundedToNextMultipleOf(checked_plainRowSize.value(), mPixelStoreUnpackAlignment);

    if (!checked_neededByteLength.isValid())
        return ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    uint32_t bytesNeeded = checked_neededByteLength.value();

    if (byteLength < bytesNeeded)
        return ErrorInvalidOperation("texSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, byteLength);

    WebGLTexture *tex = activeBoundTextureForTarget(target);
    const WebGLTexture::ImageInfo &imageInfo = tex->ImageInfoAt(target, level);

    if (imageInfo.HasUninitializedImageData())
        tex->DoDeferredImageInitialization(target, level);

    MakeContextCurrent();

    size_t srcStride = srcStrideOrZero ? srcStrideOrZero : checked_alignedRowSize.value();

    uint32_t dstTexelSize = GetBitsPerTexel(format, type) / 8;
    size_t dstPlainRowSize = dstTexelSize * width;
    // There are checks above to ensure that this won't overflow.
    size_t dstStride = RoundedToNextMultipleOf(dstPlainRowSize, mPixelStoreUnpackAlignment).value();

    // convert type for half float if not on GLES2
    GLenum realType = type;
    if (realType == LOCAL_GL_HALF_FLOAT_OES) {
        if (gl->IsSupported(gl::GLFeature::texture_half_float)) {
            realType = LOCAL_GL_HALF_FLOAT;
        } else {
            MOZ_ASSERT(gl->IsExtensionSupported(gl::GLContext::OES_texture_half_float));
        }
    }

    if (actualSrcFormat == dstFormat &&
        srcPremultiplied == mPixelStorePremultiplyAlpha &&
        srcStride == dstStride &&
        !mPixelStoreFlipY)
    {
        // no conversion, no flipping, so we avoid copying anything and just pass the source pointer
        gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height, format, realType, pixels);
    }
    else
    {
        size_t convertedDataSize = height * dstStride;
        nsAutoArrayPtr<uint8_t> convertedData(new uint8_t[convertedDataSize]);
        ConvertImage(width, height, srcStride, dstStride,
                    static_cast<const uint8_t*>(pixels), convertedData,
                    actualSrcFormat, srcPremultiplied,
                    dstFormat, mPixelStorePremultiplyAlpha, dstTexelSize);

        gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height, format, realType, convertedData);
    }
}

void
WebGLContext::TexSubImage2D(GLenum target, GLint level,
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

    return TexSubImage2D_base(target, level, xoffset, yoffset,
                              width, height, 0, format, type,
                              pixels.Value().Data(), pixels.Value().Length(),
                              JS_GetArrayBufferViewType(pixels.Value().Obj()),
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

    Uint8ClampedArray arr(pixels->GetDataObject());
    return TexSubImage2D_base(target, level, xoffset, yoffset,
                              pixels->Width(), pixels->Height(),
                              4*pixels->Width(), format, type,
                              arr.Data(), arr.Length(),
                              -1,
                              WebGLTexelFormat::RGBA8, false);
}

bool
WebGLContext::LoseContext()
{
    if (IsContextLost())
        return false;

    ForceLoseContext();

    return true;
}

bool
WebGLContext::RestoreContext()
{
    if (!IsContextLost() || !mAllowRestore) {
        return false;
    }

    ForceRestoreContext();

    return true;
}

bool
BaseTypeAndSizeFromUniformType(GLenum uType, GLenum *baseType, GLint *unitSize)
{
    switch (uType) {
        case LOCAL_GL_INT:
        case LOCAL_GL_INT_VEC2:
        case LOCAL_GL_INT_VEC3:
        case LOCAL_GL_INT_VEC4:
        case LOCAL_GL_SAMPLER_2D:
        case LOCAL_GL_SAMPLER_CUBE:
            *baseType = LOCAL_GL_INT;
            break;
        case LOCAL_GL_FLOAT:
        case LOCAL_GL_FLOAT_VEC2:
        case LOCAL_GL_FLOAT_VEC3:
        case LOCAL_GL_FLOAT_VEC4:
        case LOCAL_GL_FLOAT_MAT2:
        case LOCAL_GL_FLOAT_MAT3:
        case LOCAL_GL_FLOAT_MAT4:
            *baseType = LOCAL_GL_FLOAT;
            break;
        case LOCAL_GL_BOOL:
        case LOCAL_GL_BOOL_VEC2:
        case LOCAL_GL_BOOL_VEC3:
        case LOCAL_GL_BOOL_VEC4:
            *baseType = LOCAL_GL_BOOL; // pretend these are int
            break;
        default:
            return false;
    }

    switch (uType) {
        case LOCAL_GL_INT:
        case LOCAL_GL_FLOAT:
        case LOCAL_GL_BOOL:
        case LOCAL_GL_SAMPLER_2D:
        case LOCAL_GL_SAMPLER_CUBE:
            *unitSize = 1;
            break;
        case LOCAL_GL_INT_VEC2:
        case LOCAL_GL_FLOAT_VEC2:
        case LOCAL_GL_BOOL_VEC2:
            *unitSize = 2;
            break;
        case LOCAL_GL_INT_VEC3:
        case LOCAL_GL_FLOAT_VEC3:
        case LOCAL_GL_BOOL_VEC3:
            *unitSize = 3;
            break;
        case LOCAL_GL_INT_VEC4:
        case LOCAL_GL_FLOAT_VEC4:
        case LOCAL_GL_BOOL_VEC4:
            *unitSize = 4;
            break;
        case LOCAL_GL_FLOAT_MAT2:
            *unitSize = 4;
            break;
        case LOCAL_GL_FLOAT_MAT3:
            *unitSize = 9;
            break;
        case LOCAL_GL_FLOAT_MAT4:
            *unitSize = 16;
            break;
        default:
            return false;
    }

    return true;
}


WebGLTexelFormat mozilla::GetWebGLTexelFormat(GLenum internalformat, GLenum type)
{
    //
    // WEBGL_depth_texture
    if (internalformat == LOCAL_GL_DEPTH_COMPONENT) {
        switch (type) {
            case LOCAL_GL_UNSIGNED_SHORT:
                return WebGLTexelFormat::D16;
            case LOCAL_GL_UNSIGNED_INT:
                return WebGLTexelFormat::D32;
        }

        MOZ_CRASH("Invalid WebGL texture format/type?");
    }

    if (internalformat == LOCAL_GL_DEPTH_STENCIL) {
        switch (type) {
            case LOCAL_GL_UNSIGNED_INT_24_8_EXT:
                return WebGLTexelFormat::D24S8;
        }

        MOZ_CRASH("Invalid WebGL texture format/type?");
    }

    if (internalformat == LOCAL_GL_DEPTH_COMPONENT16) {
        return WebGLTexelFormat::D16;
    }

    if (internalformat == LOCAL_GL_DEPTH_COMPONENT32) {
        return WebGLTexelFormat::D32;
    }

    if (internalformat == LOCAL_GL_DEPTH24_STENCIL8) {
        return WebGLTexelFormat::D24S8;
    }

    if (type == LOCAL_GL_UNSIGNED_BYTE) {
        switch (internalformat) {
            case LOCAL_GL_RGBA:
            case LOCAL_GL_SRGB_ALPHA_EXT:
                return WebGLTexelFormat::RGBA8;
            case LOCAL_GL_RGB:
            case LOCAL_GL_SRGB_EXT:
                return WebGLTexelFormat::RGB8;
            case LOCAL_GL_ALPHA:
                return WebGLTexelFormat::A8;
            case LOCAL_GL_LUMINANCE:
                return WebGLTexelFormat::R8;
            case LOCAL_GL_LUMINANCE_ALPHA:
                return WebGLTexelFormat::RA8;
        }

        MOZ_CRASH("Invalid WebGL texture format/type?");
    }

    if (type == LOCAL_GL_FLOAT) {
        // OES_texture_float
        switch (internalformat) {
            case LOCAL_GL_RGBA:
            case LOCAL_GL_RGBA32F:
                return WebGLTexelFormat::RGBA32F;
            case LOCAL_GL_RGB:
                return WebGLTexelFormat::RGB32F;
            case LOCAL_GL_ALPHA:
                return WebGLTexelFormat::A32F;
            case LOCAL_GL_LUMINANCE:
                return WebGLTexelFormat::R32F;
            case LOCAL_GL_LUMINANCE_ALPHA:
                return WebGLTexelFormat::RA32F;
        }

        MOZ_CRASH("Invalid WebGL texture format/type?");
    } else if (type == LOCAL_GL_HALF_FLOAT_OES) {
        // OES_texture_half_float
        switch (internalformat) {
            case LOCAL_GL_RGBA:
                return WebGLTexelFormat::RGBA16F;
            case LOCAL_GL_RGB:
                return WebGLTexelFormat::RGB16F;
            case LOCAL_GL_ALPHA:
                return WebGLTexelFormat::A16F;
            case LOCAL_GL_LUMINANCE:
                return WebGLTexelFormat::R16F;
            case LOCAL_GL_LUMINANCE_ALPHA:
                return WebGLTexelFormat::RA16F;
            default:
                MOZ_ASSERT(false, "Coding mistake?! Should never reach this point.");
                return WebGLTexelFormat::BadFormat;
        }
    }

    switch (type) {
        case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
           return WebGLTexelFormat::RGBA4444;
        case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
           return WebGLTexelFormat::RGBA5551;
        case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
           return WebGLTexelFormat::RGB565;
        default:
            MOZ_ASSERT(false, "Coding mistake?! Should never reach this point.");
            return WebGLTexelFormat::BadFormat;
    }

    MOZ_CRASH("Invalid WebGL texture format/type?");
}

GLenum
InternalFormatForFormatAndType(GLenum format, GLenum type, bool isGLES2)
{
    // ES2 requires that format == internalformat; floating-point is
    // indicated purely by the type that's loaded.  For desktop GL, we
    // have to specify a floating point internal format.
    if (isGLES2)
        return format;

    if (format == LOCAL_GL_DEPTH_COMPONENT) {
        if (type == LOCAL_GL_UNSIGNED_SHORT)
            return LOCAL_GL_DEPTH_COMPONENT16;
        else if (type == LOCAL_GL_UNSIGNED_INT)
            return LOCAL_GL_DEPTH_COMPONENT32;
    }

    if (format == LOCAL_GL_DEPTH_STENCIL) {
        if (type == LOCAL_GL_UNSIGNED_INT_24_8_EXT)
            return LOCAL_GL_DEPTH24_STENCIL8;
    }

    switch (type) {
    case LOCAL_GL_UNSIGNED_BYTE:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        return format;

    case LOCAL_GL_FLOAT:
        switch (format) {
        case LOCAL_GL_RGBA:
            return LOCAL_GL_RGBA32F_ARB;
        case LOCAL_GL_RGB:
            return LOCAL_GL_RGB32F_ARB;
        case LOCAL_GL_ALPHA:
            return LOCAL_GL_ALPHA32F_ARB;
        case LOCAL_GL_LUMINANCE:
            return LOCAL_GL_LUMINANCE32F_ARB;
        case LOCAL_GL_LUMINANCE_ALPHA:
            return LOCAL_GL_LUMINANCE_ALPHA32F_ARB;
        }
        break;

    case LOCAL_GL_HALF_FLOAT_OES:
        switch (format) {
        case LOCAL_GL_RGBA:
            return LOCAL_GL_RGBA16F;
        case LOCAL_GL_RGB:
            return LOCAL_GL_RGB16F;
        case LOCAL_GL_ALPHA:
            return LOCAL_GL_ALPHA16F_ARB;
        case LOCAL_GL_LUMINANCE:
            return LOCAL_GL_LUMINANCE16F_ARB;
        case LOCAL_GL_LUMINANCE_ALPHA:
            return LOCAL_GL_LUMINANCE_ALPHA16F_ARB;
        }
        break;

    default:
        break;
    }

    NS_ASSERTION(false, "Coding mistake -- bad format/type passed?");
    return 0;
}

void
WebGLContext::BlendColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) {
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
WebGLContext::LineWidth(GLfloat width) {
    if (IsContextLost())
        return;
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
