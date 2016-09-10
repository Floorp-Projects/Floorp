/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGL2Context.h"

#include "WebGLActiveInfo.h"
#include "WebGLContextUtils.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLFormats.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLTexture.h"
#include "WebGLExtensions.h"
#include "WebGLVertexArray.h"

#include "nsDebug.h"
#include "nsReadableUtils.h"
#include "nsString.h"

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
#include "mozilla/EndianUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtrExtensions.h"

namespace mozilla {

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gl;

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
        GLuint framebuffername = wfb->mGLName;
        gl->fBindFramebuffer(target, framebuffername);
#ifdef ANDROID
        wfb->mIsFB = true;
#endif
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

    // Usually, we would now call into glBindRenderbuffer. However, since we have to
    // potentially emulate packed-depth-stencil, there's not a specific renderbuffer that
    // we know we should bind here.
    // Instead, we do all renderbuffer binding lazily.

    if (wrb) {
        wrb->mHasBeenBound = true;
    }

    mBoundRenderbuffer = wrb;
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
    const char funcName[] = "checkFramebufferStatus";
    if (IsContextLost())
        return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;

    if (!ValidateFramebufferTarget(target, funcName))
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
        MOZ_CRASH("GFX: Bad target.");
    }

    if (!fb)
        return LOCAL_GL_FRAMEBUFFER_COMPLETE;

    return fb->CheckFramebufferStatus(funcName).get();
}

already_AddRefed<WebGLProgram>
WebGLContext::CreateProgram()
{
    if (IsContextLost())
        return nullptr;
    RefPtr<WebGLProgram> globj = new WebGLProgram(this);
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

    RefPtr<WebGLShader> shader = new WebGLShader(this, type);
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

    GLuint activeTexture = mActiveTexture;
    for (int32_t i = 0; i < mGLMaxTextureUnits; i++) {
        if (mBound2DTextures[i] == tex ||
            mBoundCubeMapTextures[i] == tex ||
            mBound3DTextures[i] == tex ||
            mBound2DArrayTextures[i] == tex)
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
    const char funcName[] = "framebufferRenderbuffer";
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, funcName))
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
        MOZ_CRASH("GFX: Bad target.");
    }

    if (!fb)
        return ErrorInvalidOperation("%s: Cannot modify framebuffer 0.", funcName);

    fb->FramebufferRenderbuffer(funcName, attachment, rbtarget, wrb);
}

void
WebGLContext::FramebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   WebGLTexture* tobj,
                                   GLint level)
{
    const char funcName[] = "framebufferTexture2D";
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target, funcName))
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
        MOZ_CRASH("GFX: Bad target.");
    }

    if (!fb)
        return ErrorInvalidOperation("%s: Cannot modify framebuffer 0.", funcName);

    fb->FramebufferTexture2D(funcName, attachment, textarget, tobj, level);
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
                                 dom::Nullable<nsTArray<RefPtr<WebGLShader>>>& retval)
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

    const auto& buffer = ValidateBufferSelection("getBufferParameter", target);
    if (!buffer)
        return JS::NullValue();

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
    const char funcName[] = "getFramebufferAttachmentParameter";

    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateFramebufferTarget(target, funcName))
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
        MOZ_CRASH("GFX: Bad target.");
    }

    MakeContextCurrent();

    if (fb)
        return fb->GetAttachmentParameter(funcName, cx, target, attachment, pname, &rv);

    ////////////////////////////////////

    if (!IsWebGL2()) {
        ErrorInvalidOperation("%s: Querying against the default framebuffer is not"
                              " allowed in WebGL 1.",
                              funcName);
        return JS::NullValue();
    }

    switch (attachment) {
    case LOCAL_GL_BACK:
    case LOCAL_GL_DEPTH:
    case LOCAL_GL_STENCIL:
        break;

    default:
        ErrorInvalidEnum("%s: For the default framebuffer, can only query COLOR, DEPTH,"
                         " or STENCIL.",
                         funcName);
        return JS::NullValue();
    }

    switch (pname) {
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        switch (attachment) {
        case LOCAL_GL_BACK:
            break;
        case LOCAL_GL_DEPTH:
            if (!mOptions.depth) {
              return JS::Int32Value(LOCAL_GL_NONE);
            }
            break;
        case LOCAL_GL_STENCIL:
            if (!mOptions.stencil) {
              return JS::Int32Value(LOCAL_GL_NONE);
            }
            break;
        default:
            ErrorInvalidEnum("%s: With the default framebuffer, can only query COLOR, DEPTH,"
                             " or STENCIL for GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE",
                             funcName);
            return JS::NullValue();
        }
        return JS::Int32Value(LOCAL_GL_FRAMEBUFFER_DEFAULT);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return JS::NullValue();

    ////////////////

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        if (attachment == LOCAL_GL_BACK)
            return JS::NumberValue(8);
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
        if (attachment == LOCAL_GL_BACK) {
            if (mOptions.alpha) {
                return JS::NumberValue(8);
            }
            ErrorInvalidOperation("The default framebuffer doesn't contain an alpha buffer");
            return JS::NullValue();
        }
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
        if (attachment == LOCAL_GL_DEPTH) {
            if (mOptions.depth) {
                return JS::NumberValue(24);
            }
            ErrorInvalidOperation("The default framebuffer doesn't contain an depth buffer");
            return JS::NullValue();
        }
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
        if (attachment == LOCAL_GL_STENCIL) {
            if (mOptions.stencil) {
                return JS::NumberValue(8);
            }
            ErrorInvalidOperation("The default framebuffer doesn't contain an stencil buffer");
            return JS::NullValue();
        }
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        if (attachment == LOCAL_GL_STENCIL) {
            if (mOptions.stencil) {
                return JS::NumberValue(LOCAL_GL_UNSIGNED_INT);
            }
            ErrorInvalidOperation("The default framebuffer doesn't contain an stencil buffer");
        } else if (attachment == LOCAL_GL_DEPTH) {
            if (mOptions.depth) {
                return JS::NumberValue(LOCAL_GL_UNSIGNED_NORMALIZED);
            }
            ErrorInvalidOperation("The default framebuffer doesn't contain an depth buffer");
        } else { // LOCAL_GL_BACK
            return JS::NumberValue(LOCAL_GL_UNSIGNED_NORMALIZED);
        }
        return JS::NullValue();

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        if (attachment == LOCAL_GL_STENCIL) {
            if (!mOptions.stencil) {
                ErrorInvalidOperation("The default framebuffer doesn't contain an stencil buffer");
                return JS::NullValue();
            }
        } else if (attachment == LOCAL_GL_DEPTH) {
            if (!mOptions.depth) {
                ErrorInvalidOperation("The default framebuffer doesn't contain an depth buffer");
                return JS::NullValue();
            }
        }
        return JS::NumberValue(LOCAL_GL_LINEAR);
    }

    ErrorInvalidEnum("%s: Invalid pname: 0x%04x", funcName, pname);
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
    case LOCAL_GL_RENDERBUFFER_SAMPLES:
        if (!IsWebGL2())
            break;
        MOZ_FALLTHROUGH;

    case LOCAL_GL_RENDERBUFFER_WIDTH:
    case LOCAL_GL_RENDERBUFFER_HEIGHT:
    case LOCAL_GL_RENDERBUFFER_RED_SIZE:
    case LOCAL_GL_RENDERBUFFER_GREEN_SIZE:
    case LOCAL_GL_RENDERBUFFER_BLUE_SIZE:
    case LOCAL_GL_RENDERBUFFER_ALPHA_SIZE:
    case LOCAL_GL_RENDERBUFFER_DEPTH_SIZE:
    case LOCAL_GL_RENDERBUFFER_STENCIL_SIZE:
    case LOCAL_GL_RENDERBUFFER_INTERNAL_FORMAT:
    {
        // RB emulation means we have to ask the RB itself.
        GLint i = mBoundRenderbuffer->GetRenderbufferParameter(target, pname);
        return JS::Int32Value(i);
    }

    default:
        break;
    }

    ErrorInvalidEnumInfo("getRenderbufferParameter: parameter", pname);
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

    RefPtr<WebGLTexture> globj = new WebGLTexture(this, tex);
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

    if (!loc->ValidateForProgram(prog, "getUniform"))
        return JS::NullValue();

    return loc->GetUniform(js);
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
        // Deprecated and removed in desktop GL Core profiles.
        if (gl->IsCoreProfile())
            return;

        isValid = true;
        break;

    case LOCAL_GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
        if (IsWebGL2() ||
            IsExtensionEnabled(WebGLExtensionID::OES_standard_derivatives))
        {
            isValid = true;
        }
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

    if (!ValidateObjectAllowDeleted("isFramebuffer", fb))
        return false;

    if (fb->IsDeleted())
        return false;

#ifdef ANDROID
    if (gl->WorkAroundDriverBugs() &&
        gl->Renderer() == GLRenderer::AndroidEmulator)
    {
        return fb->mIsFB;
    }
#endif

    MakeContextCurrent();
    return gl->fIsFramebuffer(fb->mGLName);
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

    if (!ValidateObjectAllowDeleted("isRenderBuffer", rb))
        return false;

    if (rb->IsDeleted())
        return false;

    return rb->mHasBeenBound;
}

bool
WebGLContext::IsShader(WebGLShader* shader)
{
    if (IsContextLost())
        return false;

    return ValidateObjectAllowDeleted("isShader", shader) &&
        !shader->IsDeleted();
}

void
WebGLContext::LinkProgram(WebGLProgram* prog)
{
    if (IsContextLost())
        return;

    if (!ValidateObject("linkProgram", prog))
        return;

    prog->LinkProgram();

    if (!prog->IsLinked()) {
        // If we failed to link, but `prog == mCurrentProgram`, we are *not* supposed to
        // null out mActiveProgramLinkInfo.
        return;
    }

    if (prog == mCurrentProgram) {
        mActiveProgramLinkInfo = prog->LinkInfo();

        if (gl->WorkAroundDriverBugs() &&
            gl->Vendor() == gl::GLVendor::NVIDIA)
        {
            gl->fUseProgram(prog->mGLName);
        }
    }
}

void
WebGLContext::PixelStorei(GLenum pname, GLint param)
{
    if (IsContextLost())
        return;

    if (IsWebGL2()) {
        uint32_t* pValueSlot = nullptr;
        switch (pname) {
        case LOCAL_GL_UNPACK_IMAGE_HEIGHT:
            pValueSlot = &mPixelStore_UnpackImageHeight;
            break;

        case LOCAL_GL_UNPACK_SKIP_IMAGES:
            pValueSlot = &mPixelStore_UnpackSkipImages;
            break;

        case LOCAL_GL_UNPACK_ROW_LENGTH:
            pValueSlot = &mPixelStore_UnpackRowLength;
            break;

        case LOCAL_GL_UNPACK_SKIP_ROWS:
            pValueSlot = &mPixelStore_UnpackSkipRows;
            break;

        case LOCAL_GL_UNPACK_SKIP_PIXELS:
            pValueSlot = &mPixelStore_UnpackSkipPixels;
            break;

        case LOCAL_GL_PACK_ROW_LENGTH:
            pValueSlot = &mPixelStore_PackRowLength;
            break;

        case LOCAL_GL_PACK_SKIP_ROWS:
            pValueSlot = &mPixelStore_PackSkipRows;
            break;

        case LOCAL_GL_PACK_SKIP_PIXELS:
            pValueSlot = &mPixelStore_PackSkipPixels;
            break;
        }

        if (pValueSlot) {
            if (param < 0) {
                ErrorInvalidValue("pixelStorei: param must be >= 0.");
                return;
            }

            MakeContextCurrent();
            gl->fPixelStorei(pname, param);
            *pValueSlot = param;
            return;
        }
    }

    switch (pname) {
    case UNPACK_FLIP_Y_WEBGL:
        mPixelStore_FlipY = bool(param);
        return;

    case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        mPixelStore_PremultiplyAlpha = bool(param);
        return;

    case UNPACK_COLORSPACE_CONVERSION_WEBGL:
        switch (param) {
        case LOCAL_GL_NONE:
        case BROWSER_DEFAULT_WEBGL:
            mPixelStore_ColorspaceConversion = param;
            return;

        default:
            ErrorInvalidEnumInfo("pixelStorei: colorspace conversion parameter",
                                 param);
            return;
        }

    case LOCAL_GL_PACK_ALIGNMENT:
    case LOCAL_GL_UNPACK_ALIGNMENT:
        switch (param) {
        case 1:
        case 2:
        case 4:
        case 8:
            if (pname == LOCAL_GL_PACK_ALIGNMENT)
                mPixelStore_PackAlignment = param;
            else if (pname == LOCAL_GL_UNPACK_ALIGNMENT)
                mPixelStore_UnpackAlignment = param;

            MakeContextCurrent();
            gl->fPixelStorei(pname, param);
            return;

        default:
            ErrorInvalidValue("pixelStorei: invalid pack/unpack alignment value");
            return;
        }



    default:
        break;
    }

    ErrorInvalidEnumInfo("pixelStorei: parameter", pname);
}

static bool
IsNeedsANGLEWorkAround(const webgl::FormatInfo* format)
{
    switch (format->effectiveFormat) {
    case webgl::EffectiveFormat::RGB16F:
    case webgl::EffectiveFormat::RGBA16F:
        return true;

    default:
        return false;
    }
}

bool
WebGLContext::DoReadPixelsAndConvert(const webgl::FormatInfo* srcFormat, GLint x, GLint y,
                                     GLsizei width, GLsizei height, GLenum format,
                                     GLenum destType, void* dest, uint32_t destSize,
                                     uint32_t rowStride)
{
    if (gl->WorkAroundDriverBugs() &&
        gl->IsANGLE() &&
        gl->Version() < 300 && // ANGLE ES2 doesn't support HALF_FLOAT reads properly.
        IsNeedsANGLEWorkAround(srcFormat))
    {
        MOZ_RELEASE_ASSERT(!IsWebGL2()); // No SKIP_PIXELS, etc.
        MOZ_ASSERT(!mBoundPixelPackBuffer); // Let's be real clear.

        // You'd think ANGLE would want HALF_FLOAT_OES, but it rejects that.
        const GLenum readType = LOCAL_GL_HALF_FLOAT;

        const char funcName[] = "readPixels";
        const auto readBytesPerPixel = webgl::BytesPerPixel({format, readType});
        const auto destBytesPerPixel = webgl::BytesPerPixel({format, destType});

        uint32_t readStride;
        uint32_t readByteCount;
        uint32_t destStride;
        uint32_t destByteCount;
        if (!ValidatePackSize(funcName, width, height, readBytesPerPixel, &readStride,
                              &readByteCount) ||
            !ValidatePackSize(funcName, width, height, destBytesPerPixel, &destStride,
                              &destByteCount))
        {
            ErrorOutOfMemory("readPixels: Overflow calculating sizes for conversion.");
            return false;
        }

        UniqueBuffer readBuffer = malloc(readByteCount);
        if (!readBuffer) {
            ErrorOutOfMemory("readPixels: Failed to alloc temp buffer for conversion.");
            return false;
        }

        gl::GLContext::LocalErrorScope errorScope(*gl);

        gl->fReadPixels(x, y, width, height, format, readType, readBuffer.get());

        const GLenum error = errorScope.GetError();
        if (error == LOCAL_GL_OUT_OF_MEMORY) {
            ErrorOutOfMemory("readPixels: Driver ran out of memory.");
            return false;
        }

        if (error) {
            MOZ_RELEASE_ASSERT(false, "GFX: Unexpected driver error.");
            return false;
        }

        size_t channelsPerRow = std::min(readStride / sizeof(uint16_t),
                                         destStride / sizeof(float));

        const uint8_t* srcRow = (uint8_t*)readBuffer.get();
        uint8_t* dstRow = (uint8_t*)dest;

        for (size_t j = 0; j < (size_t)height; j++) {
            auto src = (const uint16_t*)srcRow;
            auto dst = (float*)dstRow;

            const auto srcEnd = src + channelsPerRow;
            while (src != srcEnd) {
                *dst = unpackFromFloat16(*src);
                ++src;
                ++dst;
            }

            srcRow += readStride;
            dstRow += destStride;
        }

        return true;
    }

    // On at least Win+NV, we'll get PBO errors if we don't have at least
    // `rowStride * height` bytes available to read into.
    const auto naiveBytesNeeded = CheckedUint32(rowStride) * height;
    const bool isDangerCloseToEdge = (!naiveBytesNeeded.isValid() ||
                                      naiveBytesNeeded.value() > destSize);
    const bool useParanoidHandling = (gl->WorkAroundDriverBugs() &&
                                      isDangerCloseToEdge &&
                                      mBoundPixelPackBuffer);
    if (!useParanoidHandling) {
        gl->fReadPixels(x, y, width, height, format, destType, dest);
        return true;
    }

    // Read everything but the last row.
    const auto bodyHeight = height - 1;
    if (bodyHeight) {
        gl->fReadPixels(x, y, width, bodyHeight, format, destType, dest);
    }

    // Now read the last row.
    gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 1);
    gl->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, 0);
    gl->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, 0);

    const auto tailRowOffset = (char*)dest + rowStride * bodyHeight;
    gl->fReadPixels(x, y+bodyHeight, width, 1, format, destType, tailRowOffset);

    gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, mPixelStore_PackAlignment);
    gl->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, mPixelStore_PackRowLength);
    gl->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, mPixelStore_PackSkipRows);
    return true;
}

static bool
GetJSScalarFromGLType(GLenum type, js::Scalar::Type* const out_scalarType)
{
    switch (type) {
    case LOCAL_GL_BYTE:
        *out_scalarType = js::Scalar::Int8;
        return true;

    case LOCAL_GL_UNSIGNED_BYTE:
        *out_scalarType = js::Scalar::Uint8;
        return true;

    case LOCAL_GL_SHORT:
        *out_scalarType = js::Scalar::Int16;
        return true;

    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        *out_scalarType = js::Scalar::Uint16;
        return true;

    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV:
    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
    case LOCAL_GL_UNSIGNED_INT_24_8:
        *out_scalarType = js::Scalar::Uint32;
        return true;
    case LOCAL_GL_INT:
        *out_scalarType = js::Scalar::Int32;
        return true;

    case LOCAL_GL_FLOAT:
        *out_scalarType = js::Scalar::Float32;
        return true;

    default:
        return false;
    }
}

bool
WebGLContext::ReadPixels_SharedPrecheck(ErrorResult* const out_error)
{
    if (IsContextLost())
        return false;

    if (mCanvasElement &&
        mCanvasElement->IsWriteOnly() &&
        !nsContentUtils::IsCallerChrome())
    {
        GenerateWarning("readPixels: Not allowed");
        out_error->Throw(NS_ERROR_DOM_SECURITY_ERR);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidatePackSize(const char* funcName, uint32_t width, uint32_t height,
                               uint8_t bytesPerPixel, uint32_t* const out_rowStride,
                               uint32_t* const out_endOffset)
{
    if (!width || !height) {
        *out_rowStride = 0;
        *out_endOffset = 0;
        return true;
    }

    // GLES 3.0.4, p116 (PACK_ functions like UNPACK_)

    const auto rowLength = (mPixelStore_PackRowLength ? mPixelStore_PackRowLength
                                                      : width);
    const auto skipPixels = mPixelStore_PackSkipPixels;
    const auto skipRows = mPixelStore_PackSkipRows;
    const auto alignment = mPixelStore_PackAlignment;

    const auto usedPixelsPerRow = CheckedUint32(skipPixels) + width;
    const auto usedRowsPerImage = CheckedUint32(skipRows) + height;

    if (!usedPixelsPerRow.isValid() || usedPixelsPerRow.value() > rowLength) {
        ErrorInvalidOperation("%s: SKIP_PIXELS + width > ROW_LENGTH.", funcName);
        return false;
    }

    const auto rowLengthBytes = CheckedUint32(rowLength) * bytesPerPixel;
    const auto rowStride = RoundUpToMultipleOf(rowLengthBytes, alignment);

    const auto usedBytesPerRow = usedPixelsPerRow * bytesPerPixel;
    const auto usedBytesPerImage = (usedRowsPerImage - 1) * rowStride + usedBytesPerRow;

    if (!rowStride.isValid() || !usedBytesPerImage.isValid()) {
        ErrorInvalidOperation("%s: Invalid UNPACK_ params.", funcName);
        return false;
    }

    *out_rowStride = rowStride.value();
    *out_endOffset = usedBytesPerImage.value();
    return true;
}

void
WebGLContext::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                         GLenum type, const dom::ArrayBufferView& view,
                         ErrorResult& out_error)
{
    if (!ReadPixels_SharedPrecheck(&out_error))
        return;

    if (mBoundPixelPackBuffer) {
        ErrorInvalidOperation("readPixels: PIXEL_PACK_BUFFER must be null.");
        return;
    }

    //////

    js::Scalar::Type reqScalarType;
    if (!GetJSScalarFromGLType(type, &reqScalarType)) {
        ErrorInvalidEnum("readPixels: Bad `type`.");
        return;
    }

    const js::Scalar::Type dataScalarType = JS_GetArrayBufferViewType(view.Obj());
    if (dataScalarType != reqScalarType) {
        ErrorInvalidOperation("readPixels: `pixels` type does not match `type`.");
        return;
    }

    //////

    // Compute length and data.  Don't reenter after this point, lest the
    // precomputed go out of sync with the instant length/data.
    view.ComputeLengthAndData();
    void* data = view.DataAllowShared();
    const auto dataLen = view.LengthAllowShared();

    if (!data) {
        ErrorOutOfMemory("readPixels: buffer storage is null. Did we run out of memory?");
        out_error.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
    }

    ReadPixelsImpl(x, y, width, height, format, type, data, dataLen);
}

void
WebGL2Context::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                          GLenum type, WebGLsizeiptr offset, ErrorResult& out_error)
{
    if (!ReadPixels_SharedPrecheck(&out_error))
        return;

    if (!mBoundPixelPackBuffer) {
        ErrorInvalidOperation("readPixels: PIXEL_PACK_BUFFER must not be null.");
        return;
    }

    if (mBoundPixelPackBuffer->mNumActiveTFOs) {
        ErrorInvalidOperation("%s: Buffer is bound to an active transform feedback"
                              " object.",
                              "readPixels");
        return;
    }

    //////

    if (offset < 0) {
        ErrorInvalidValue("readPixels: offset must not be negative.");
        return;
    }

    {
        const auto bytesPerType = webgl::BytesPerPixel({LOCAL_GL_RED, type});

        if (offset % bytesPerType != 0) {
            ErrorInvalidOperation("readPixels: `offset` must be divisible by the size"
                                  " a `type` in bytes.");
            return;
        }
    }

    //////

    const auto bytesAvailable = mBoundPixelPackBuffer->ByteLength();
    const auto checkedBytesAfterOffset = CheckedUint32(bytesAvailable) - offset;

    uint32_t bytesAfterOffset = 0;
    if (checkedBytesAfterOffset.isValid()) {
        bytesAfterOffset = checkedBytesAfterOffset.value();
    }

    ReadPixelsImpl(x, y, width, height, format, type, (void*)offset, bytesAfterOffset);
}

static bool
ValidateReadPixelsFormatAndType(const webgl::FormatInfo* srcFormat,
                                const webgl::PackingInfo& pi, gl::GLContext* gl,
                                WebGLContext* webgl)
{
    // Check the format and type params to assure they are an acceptable pair (as per spec)
    GLenum mainFormat;
    GLenum mainType;

    switch (srcFormat->componentType) {
    case webgl::ComponentType::Float:
        mainFormat = LOCAL_GL_RGBA;
        mainType = LOCAL_GL_FLOAT;
        break;

    case webgl::ComponentType::UInt:
        mainFormat = LOCAL_GL_RGBA_INTEGER;
        mainType = LOCAL_GL_UNSIGNED_INT;
        break;

    case webgl::ComponentType::Int:
        mainFormat = LOCAL_GL_RGBA_INTEGER;
        mainType = LOCAL_GL_INT;
        break;

    case webgl::ComponentType::NormInt:
    case webgl::ComponentType::NormUInt:
        mainFormat = LOCAL_GL_RGBA;
        mainType = LOCAL_GL_UNSIGNED_BYTE;
        break;

    default:
        gfxCriticalNote << "Unhandled srcFormat->componentType: "
                        << (uint32_t)srcFormat->componentType;
        webgl->ErrorInvalidOperation("readPixels: Unhandled srcFormat->componentType."
                                     " Please file a bug!");
        return false;
    }

    if (pi.format == mainFormat && pi.type == mainType)
        return true;

    //////
    // OpenGL ES 3.0.4 p194 - When the internal format of the rendering surface is
    // RGB10_A2, a third combination of format RGBA and type UNSIGNED_INT_2_10_10_10_REV
    // is accepted.

    if (webgl->IsWebGL2() &&
        srcFormat->effectiveFormat == webgl::EffectiveFormat::RGB10_A2 &&
        pi.format == LOCAL_GL_RGBA &&
        pi.type == LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV)
    {
        return true;
    }

    //////
    // OpenGL ES 2.0 $4.3.1 - IMPLEMENTATION_COLOR_READ_{TYPE/FORMAT} is a valid
    // combination for glReadPixels()...

    // So yeah, we are actually checking that these are valid as /unpack/ formats, instead
    // of /pack/ formats here, but it should cover the INVALID_ENUM cases.
    if (!webgl->mFormatUsage->AreUnpackEnumsValid(pi.format, pi.type)) {
        webgl->ErrorInvalidEnum("readPixels: Bad format and/or type.");
        return false;
    }

    // Only valid when pulled from:
    // * GLES 2.0.25 p105:
    //   "table 3.4, excluding formats LUMINANCE and LUMINANCE_ALPHA."
    // * GLES 3.0.4 p193:
    //   "table 3.2, excluding formats DEPTH_COMPONENT and DEPTH_STENCIL."
    switch (pi.format) {
    case LOCAL_GL_LUMINANCE:
    case LOCAL_GL_LUMINANCE_ALPHA:
    case LOCAL_GL_DEPTH_COMPONENT:
    case LOCAL_GL_DEPTH_STENCIL:
        webgl->ErrorInvalidEnum("readPixels: Invalid format: 0x%04x", pi.format);
        return false;
    }

    if (pi.type == LOCAL_GL_UNSIGNED_INT_24_8) {
        webgl->ErrorInvalidEnum("readPixels: Invalid type: 0x%04x", pi.type);
        return false;
    }

    MOZ_ASSERT(gl->IsCurrent());
    if (gl->IsSupported(gl::GLFeature::ES2_compatibility)) {
        const auto auxFormat = gl->GetIntAs<GLenum>(LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT);
        const auto auxType = gl->GetIntAs<GLenum>(LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE);

        if (auxFormat && auxType &&
            pi.format == auxFormat && pi.type == auxType)
        {
            return true;
        }
    }

    //////

    webgl->ErrorInvalidOperation("readPixels: Invalid format or type.");
    return false;
}

void
WebGLContext::ReadPixelsImpl(GLint x, GLint y, GLsizei rawWidth, GLsizei rawHeight,
                             GLenum packFormat, GLenum packType, void* dest,
                             uint32_t dataLen)
{
    if (rawWidth < 0 || rawHeight < 0) {
        ErrorInvalidValue("readPixels: negative size passed");
        return;
    }

    const uint32_t width(rawWidth);
    const uint32_t height(rawHeight);

    //////

    MakeContextCurrent();

    const webgl::FormatUsageInfo* srcFormat;
    uint32_t srcWidth;
    uint32_t srcHeight;
    if (!ValidateCurFBForRead("readPixels", &srcFormat, &srcWidth, &srcHeight))
        return;

    //////

    const webgl::PackingInfo pi = {packFormat, packType};
    if (!ValidateReadPixelsFormatAndType(srcFormat->format, pi, gl, this))
        return;

    uint8_t bytesPerPixel;
    if (!webgl::GetBytesPerPixel(pi, &bytesPerPixel)) {
        ErrorInvalidOperation("readPixels: Unsupported format and type.");
        return;
    }

    //////

    uint32_t rowStride;
    uint32_t bytesNeeded;
    if (!ValidatePackSize("readPixels", width, height, bytesPerPixel, &rowStride,
                          &bytesNeeded))
    {
        return;
    }

    if (bytesNeeded > dataLen) {
        ErrorInvalidOperation("readPixels: buffer too small");
        return;
    }

    ////////////////
    // Now that the errors are out of the way, on to actually reading!

    uint32_t readX, readY;
    uint32_t writeX, writeY;
    uint32_t rwWidth, rwHeight;
    Intersect(srcWidth, x, width, &readX, &writeX, &rwWidth);
    Intersect(srcHeight, y, height, &readY, &writeY, &rwHeight);

    if (rwWidth == uint32_t(width) && rwHeight == uint32_t(height)) {
        DoReadPixelsAndConvert(srcFormat->format, x, y, width, height, packFormat,
                               packType, dest, dataLen, rowStride);
        return;
    }

    // Read request contains out-of-bounds pixels. Unfortunately:
    // GLES 3.0.4 p194 "Obtaining Pixels from the Framebuffer":
    // "If any of these pixels lies outside of the window allocated to the current GL
    //  context, or outside of the image attached to the currently bound framebuffer
    //  object, then the values obtained for those pixels are undefined."

    // This is a slow-path, so warn people away!
    GenerateWarning("readPixels: Out-of-bounds reads with readPixels are deprecated, and"
                    " may be slow.");

    ////////////////////////////////////
    // Read only the in-bounds pixels.

    if (!rwWidth || !rwHeight) {
        // There aren't any, so we're 'done'.
        DummyReadFramebufferOperation("readPixels");
        return;
    }

    if (IsWebGL2()) {
        if (!mPixelStore_PackRowLength) {
            gl->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH,
                             mPixelStore_PackSkipPixels + width);
        }
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_PIXELS, mPixelStore_PackSkipPixels + writeX);
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, mPixelStore_PackSkipRows + writeY);

        DoReadPixelsAndConvert(srcFormat->format, readX, readY, rwWidth, rwHeight,
                               packFormat, packType, dest, dataLen, rowStride);

        gl->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, mPixelStore_PackRowLength);
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_PIXELS, mPixelStore_PackSkipPixels);
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, mPixelStore_PackSkipRows);
    } else {
        // I *did* say "hilariously slow".

        uint8_t* row = (uint8_t*)dest + writeX * bytesPerPixel;
        row += writeY * rowStride;
        for (uint32_t j = 0; j < rwHeight; j++) {
            DoReadPixelsAndConvert(srcFormat->format, readX, readY+j, rwWidth, 1,
                                   packFormat, packType, row, dataLen, rowStride);
            row += rowStride;
        }
    }
}

void
WebGLContext::RenderbufferStorage_base(const char* funcName, GLenum target,
                                       GLsizei samples, GLenum internalFormat,
                                       GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    if (target != LOCAL_GL_RENDERBUFFER) {
        ErrorInvalidEnumInfo("`target`", funcName, target);
        return;
    }

    if (!mBoundRenderbuffer) {
        ErrorInvalidOperation("%s: Called on renderbuffer 0.", funcName);
        return;
    }

    if (samples < 0) {
        ErrorInvalidValue("%s: `samples` must be >= 0.", funcName);
        return;
    }

    if (width < 0 || height < 0) {
        ErrorInvalidValue("%s: `width` and `height` must be >= 0.", funcName);
        return;
    }

    mBoundRenderbuffer->RenderbufferStorage(funcName, uint32_t(samples), internalFormat,
                                            uint32_t(width), uint32_t(height));
}

void
WebGLContext::RenderbufferStorage(GLenum target, GLenum internalFormat, GLsizei width, GLsizei height)
{
    RenderbufferStorage_base("renderbufferStorage", target, 0, internalFormat, width,
                             height);
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

////////////////////////////////////////////////////////////////////////////////
// Uniform setters.

class ValidateIfSampler
{
    const WebGLUniformLocation* const mLoc;
    const size_t mDataCount;
    const GLint* const mData;
    bool mIsValidatedSampler;

public:
    ValidateIfSampler(WebGLContext* webgl, const char* funcName,
                      WebGLUniformLocation* loc, size_t dataCount, const GLint* data,
                      bool* const out_error)
        : mLoc(loc)
        , mDataCount(dataCount)
        , mData(data)
        , mIsValidatedSampler(false)
    {
        if (!mLoc->mInfo->mSamplerTexList) {
            *out_error = false;
            return;
        }

        for (size_t i = 0; i < mDataCount; i++) {
            const auto& val = mData[i];
            if (val < 0 || uint32_t(val) >= webgl->GLMaxTextureUnits()) {
                webgl->ErrorInvalidValue("%s: This uniform location is a sampler, but %d"
                                         " is not a valid texture unit.",
                                         funcName, val);
                *out_error = true;
                return;
            }
        }

        mIsValidatedSampler = true;
        *out_error = false;
    }

    ~ValidateIfSampler() {
        if (!mIsValidatedSampler)
            return;

        auto& samplerValues = mLoc->mInfo->mSamplerValues;

        for (size_t i = 0; i < mDataCount; i++) {
            const size_t curIndex = mLoc->mArrayIndex + i;
            if (curIndex >= samplerValues.size())
                break;

            samplerValues[curIndex] = mData[i];
        }
    }
};

////////////////////

void
WebGLContext::Uniform1i(WebGLUniformLocation* loc, GLint a1)
{
    const char funcName[] = "uniform1i";
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_INT, funcName))
        return;

    bool error;
    const ValidateIfSampler validate(this, funcName, loc, 1, &a1, &error);
    if (error)
        return;

    MakeContextCurrent();
    gl->fUniform1i(loc->mLoc, a1);
}

void
WebGLContext::Uniform2i(WebGLUniformLocation* loc, GLint a1, GLint a2)
{
    const char funcName[] = "uniform2i";
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_INT, funcName))
        return;

    MakeContextCurrent();
    gl->fUniform2i(loc->mLoc, a1, a2);
}

void
WebGLContext::Uniform3i(WebGLUniformLocation* loc, GLint a1, GLint a2, GLint a3)
{
    const char funcName[] = "uniform3i";
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_INT, funcName))
        return;

    MakeContextCurrent();
    gl->fUniform3i(loc->mLoc, a1, a2, a3);
}

void
WebGLContext::Uniform4i(WebGLUniformLocation* loc, GLint a1, GLint a2, GLint a3,
                        GLint a4)
{
    const char funcName[] = "uniform4i";
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_INT, funcName))
        return;

    MakeContextCurrent();
    gl->fUniform4i(loc->mLoc, a1, a2, a3, a4);
}

//////////

void
WebGLContext::Uniform1f(WebGLUniformLocation* loc, GLfloat a1)
{
    const char funcName[] = "uniform1f";
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_FLOAT, funcName))
        return;

    MakeContextCurrent();
    gl->fUniform1f(loc->mLoc, a1);
}

void
WebGLContext::Uniform2f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2)
{
    const char funcName[] = "uniform2f";
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_FLOAT, funcName))
        return;

    MakeContextCurrent();
    gl->fUniform2f(loc->mLoc, a1, a2);
}

void
WebGLContext::Uniform3f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2,
                        GLfloat a3)
{
    const char funcName[] = "uniform3f";
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_FLOAT, funcName))
        return;

    MakeContextCurrent();
    gl->fUniform3f(loc->mLoc, a1, a2, a3);
}

void
WebGLContext::Uniform4f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2,
                        GLfloat a3, GLfloat a4)
{
    const char funcName[] = "uniform4f";
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_FLOAT, funcName))
        return;

    MakeContextCurrent();
    gl->fUniform4f(loc->mLoc, a1, a2, a3, a4);
}

////////////////////////////////////////
// Array

void
WebGLContext::UniformNiv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                         const IntArr& arr)
{
    uint32_t numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, N, LOCAL_GL_INT, arr.dataCount, funcName,
                                    &numElementsToUpload))
    {
        return;
    }

    bool error;
    const ValidateIfSampler samplerValidator(this, funcName, loc, numElementsToUpload,
                                             arr.data, &error);
    if (error)
        return;

    static const decltype(&gl::GLContext::fUniform1iv) kFuncList[] = {
        &gl::GLContext::fUniform1iv,
        &gl::GLContext::fUniform2iv,
        &gl::GLContext::fUniform3iv,
        &gl::GLContext::fUniform4iv
    };
    const auto func = kFuncList[N-1];

    MakeContextCurrent();
    (gl->*func)(loc->mLoc, numElementsToUpload, arr.data);
}

void
WebGL2Context::UniformNuiv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                           const UintArr& arr)
{
    uint32_t numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, N, LOCAL_GL_UNSIGNED_INT, arr.dataCount,
                                    funcName, &numElementsToUpload))
    {
        return;
    }
    MOZ_ASSERT(!loc->mInfo->mSamplerTexList, "Should not be a sampler.");

    static const decltype(&gl::GLContext::fUniform1uiv) kFuncList[] = {
        &gl::GLContext::fUniform1uiv,
        &gl::GLContext::fUniform2uiv,
        &gl::GLContext::fUniform3uiv,
        &gl::GLContext::fUniform4uiv
    };
    const auto func = kFuncList[N-1];

    MakeContextCurrent();
    (gl->*func)(loc->mLoc, numElementsToUpload, arr.data);
}

void
WebGLContext::UniformNfv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                         const FloatArr& arr)
{
    uint32_t numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, N, LOCAL_GL_FLOAT, arr.dataCount, funcName,
                                    &numElementsToUpload))
    {
        return;
    }
    MOZ_ASSERT(!loc->mInfo->mSamplerTexList, "Should not be a sampler.");

    static const decltype(&gl::GLContext::fUniform1fv) kFuncList[] = {
        &gl::GLContext::fUniform1fv,
        &gl::GLContext::fUniform2fv,
        &gl::GLContext::fUniform3fv,
        &gl::GLContext::fUniform4fv
    };
    const auto func = kFuncList[N-1];

    MakeContextCurrent();
    (gl->*func)(loc->mLoc, numElementsToUpload, arr.data);
}

void
WebGLContext::UniformMatrixAxBfv(const char* funcName, uint8_t A, uint8_t B,
                                 WebGLUniformLocation* loc, bool transpose,
                                 const FloatArr& arr)
{
    uint32_t numElementsToUpload;
    if (!ValidateUniformMatrixArraySetter(loc, A, B, LOCAL_GL_FLOAT, arr.dataCount,
                                          transpose, funcName, &numElementsToUpload))
    {
        return;
    }
    MOZ_ASSERT(!loc->mInfo->mSamplerTexList, "Should not be a sampler.");

    static const decltype(&gl::GLContext::fUniformMatrix2fv) kFuncList[] = {
        &gl::GLContext::fUniformMatrix2fv,
        &gl::GLContext::fUniformMatrix2x3fv,
        &gl::GLContext::fUniformMatrix2x4fv,

        &gl::GLContext::fUniformMatrix3x2fv,
        &gl::GLContext::fUniformMatrix3fv,
        &gl::GLContext::fUniformMatrix3x4fv,

        &gl::GLContext::fUniformMatrix4x2fv,
        &gl::GLContext::fUniformMatrix4x3fv,
        &gl::GLContext::fUniformMatrix4fv
    };
    const auto func = kFuncList[3*(A-2) + (B-2)];

    MakeContextCurrent();
    (gl->*func)(loc->mLoc, numElementsToUpload, false, arr.data);
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

    RefPtr<WebGLFramebuffer> globj = new WebGLFramebuffer(this, fbo);
    return globj.forget();
}

already_AddRefed<WebGLRenderbuffer>
WebGLContext::CreateRenderbuffer()
{
    if (IsContextLost())
        return nullptr;

    MakeContextCurrent();
    RefPtr<WebGLRenderbuffer> globj = new WebGLRenderbuffer(this);
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

    RefPtr<WebGLShaderPrecisionFormat> retShaderPrecisionFormat
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

} // namespace mozilla
