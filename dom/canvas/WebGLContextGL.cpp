/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

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
#include "mozilla/Endian.h"
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

    MakeContextCurrent();

    // Sometimes we emulate renderbuffers (depth-stencil emu), so there's not
    // always a 1-1 mapping from `wrb` to GL name. Just have `wrb` handle it.
    if (wrb) {
        wrb->BindRenderbuffer();
#ifdef ANDROID
        wrb->mIsRB = true;
#endif
    } else {
        gl->fBindRenderbuffer(target, 0);
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
        MOZ_CRASH("Bad target.");
    }

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
    case LOCAL_GL_COLOR:
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
        return JS::Int32Value(LOCAL_GL_FRAMEBUFFER_DEFAULT);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        return JS::NullValue();

    ////////////////

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        if (attachment == LOCAL_GL_COLOR)
            return JS::NumberValue(8);
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
        if (attachment == LOCAL_GL_COLOR)
            return JS::NumberValue(mOptions.alpha ? 8 : 0);
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
        if (attachment == LOCAL_GL_DEPTH)
            return JS::NumberValue(mOptions.depth ? 24 : 0);
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
        if (attachment == LOCAL_GL_STENCIL)
            return JS::NumberValue(mOptions.stencil ? 8 : 0);
        return JS::NumberValue(0);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        if (attachment == LOCAL_GL_STENCIL)
            return JS::NumberValue(LOCAL_GL_UNSIGNED_INT);
        else
            return JS::NumberValue(LOCAL_GL_UNSIGNED_NORMALIZED);

    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
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

#ifdef ANDROID
    if (gl->WorkAroundDriverBugs() &&
        gl->Renderer() == GLRenderer::AndroidEmulator)
    {
         return rb->mIsRB;
    }
#endif

    MakeContextCurrent();
    return gl->fIsRenderbuffer(rb->PrimaryGLName());
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

// `width` in pixels.
// `stride` in bytes.
static void
SetFullAlpha(void* data, GLenum format, GLenum type, size_t width, size_t height,
             size_t stride)
{
    if (format == LOCAL_GL_ALPHA && type == LOCAL_GL_UNSIGNED_BYTE) {
        // Just memset the rows.
        uint8_t* row = static_cast<uint8_t*>(data);
        for (size_t j = 0; j < height; ++j) {
            memset(row, 0xff, width);
            row += stride;
        }

        return;
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

        return;
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

        return;
    }

    MOZ_CRASH("Unhandled case, how'd we get here?");
}

bool
WebGLContext::DoReadPixelsAndConvert(GLint x, GLint y, GLsizei width, GLsizei height,
                                     GLenum destFormat, GLenum destType, void* destBytes,
                                     GLenum auxReadFormat, GLenum auxReadType)
{
    GLenum readFormat = destFormat;
    GLenum readType = destType;

    if (gl->WorkAroundDriverBugs() &&
        gl->IsANGLE() &&
        readType == LOCAL_GL_FLOAT &&
        auxReadFormat == destFormat &&
        auxReadType == LOCAL_GL_HALF_FLOAT)
    {
        readType = auxReadType;

        const auto readBytesPerPixel = webgl::BytesPerPixel({readFormat, readType});
        const auto destBytesPerPixel = webgl::BytesPerPixel({destFormat, destType});

        CheckedUint32 readOffset;
        CheckedUint32 readStride;
        const CheckedUint32 readSize = GetPackSize(width, height, readBytesPerPixel,
                                                   &readOffset, &readStride);

        CheckedUint32 destOffset;
        CheckedUint32 destStride;
        const CheckedUint32 destSize = GetPackSize(width, height, destBytesPerPixel,
                                                   &destOffset, &destStride);
        if (!readSize.isValid() || !destSize.isValid()) {
            ErrorOutOfMemory("readPixels: Overflow calculating sizes for conversion.");
            return false;
        }

        UniqueBuffer readBuffer = malloc(readSize.value());
        if (!readBuffer) {
            ErrorOutOfMemory("readPixels: Failed to alloc temp buffer for conversion.");
            return false;
        }

        gl::GLContext::LocalErrorScope errorScope(*gl);

        gl->fReadPixels(x, y, width, height, readFormat, readType, readBuffer.get());

        const GLenum error = errorScope.GetError();
        if (error == LOCAL_GL_OUT_OF_MEMORY) {
            ErrorOutOfMemory("readPixels: Driver ran out of memory.");
            return false;
        }

        if (error) {
            MOZ_RELEASE_ASSERT(false, "Unexpected driver error.");
            return false;
        }

        size_t channelsPerRow = std::min(readStride.value() / sizeof(uint16_t),
                                         destStride.value() / sizeof(float));

        const uint8_t* srcRow = (uint8_t*)(readBuffer.get()) + readOffset.value();
        uint8_t* dstRow = (uint8_t*)(destBytes) + destOffset.value();

        for (size_t j = 0; j < (size_t)height; j++) {
            auto src = (const uint16_t*)srcRow;
            auto dst = (float*)dstRow;

            const auto srcEnd = src + channelsPerRow;
            while (src != srcEnd) {
                *dst = unpackFromFloat16(*src);
                ++src;
                ++dst;
            }

            srcRow += readStride.value();
            dstRow += destStride.value();
        }

        return true;
    }

    gl->fReadPixels(x, y, width, height, destFormat, destType, destBytes);
    return true;
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

CheckedUint32
WebGLContext::GetPackSize(uint32_t width, uint32_t height, uint8_t bytesPerPixel,
                          CheckedUint32* const out_startOffset,
                          CheckedUint32* const out_rowStride)
{
    if (!width || !height) {
        *out_startOffset = 0;
        *out_rowStride = 0;
        return 0;
    }

    const CheckedUint32 pixelsPerRow = (mPixelStore_PackRowLength ? mPixelStore_PackRowLength
                                                                  : width);
    const CheckedUint32 skipPixels = mPixelStore_PackSkipPixels;
    const CheckedUint32 skipRows = mPixelStore_PackSkipRows;
    const CheckedUint32 alignment = mPixelStore_PackAlignment;

    // GLES 3.0.4, p116 (PACK_ functions like UNPACK_)
    const auto totalBytesPerRow = bytesPerPixel * pixelsPerRow;
    const auto rowStride = RoundUpToMultipleOf(totalBytesPerRow, alignment);

    const auto startOffset = rowStride * skipRows + bytesPerPixel * skipPixels;
    const auto usedBytesPerRow = bytesPerPixel * width;

    const auto bytesNeeded = startOffset + rowStride * (height - 1) + usedBytesPerRow;

    *out_startOffset = startOffset;
    *out_rowStride = rowStride;
    return bytesNeeded;
}

void
WebGLContext::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                         GLenum type,
                         const dom::Nullable<dom::ArrayBufferView>& pixels,
                         ErrorResult& out_error)
{
    if (IsContextLost())
        return;

    if (mCanvasElement &&
        mCanvasElement->IsWriteOnly() &&
        !nsContentUtils::IsCallerChrome())
    {
        GenerateWarning("readPixels: Not allowed");
        out_error.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return;
    }

    if (width < 0 || height < 0)
        return ErrorInvalidValue("readPixels: negative size passed");

    if (pixels.IsNull())
        return ErrorInvalidValue("readPixels: null destination buffer");

    if (!IsFormatAndTypeUnpackable(format, type))
        return ErrorInvalidEnum("readPixels: Bad format or type.");

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

    const auto& view = pixels.Value();

    // Compute length and data.  Don't reenter after this point, lest the
    // precomputed go out of sync with the instant length/data.
    view.ComputeLengthAndData();
    void* data = view.DataAllowShared();
    size_t bytesAvailable = view.LengthAllowShared();
    js::Scalar::Type dataType = JS_GetArrayBufferViewType(view.Obj());

    // Check the pixels param type
    if (dataType != requiredDataType)
        return ErrorInvalidOperation("readPixels: Mismatched type/pixels types");

    CheckedUint32 startOffset;
    CheckedUint32 rowStride;
    const auto bytesNeeded = GetPackSize(width, height, bytesPerPixel, &startOffset,
                                         &rowStride);
    if (!bytesNeeded.isValid()) {
        ErrorInvalidOperation("readPixels: Integer overflow computing the needed buffer"
                              " size.");
        return;
    }

    if (bytesNeeded.value() > bytesAvailable) {
        ErrorInvalidOperation("readPixels: buffer too small");
        return;
    }

    if (!data) {
        ErrorOutOfMemory("readPixels: buffer storage is null. Did we run out of memory?");
        out_error.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
    }

    MakeContextCurrent();

    const webgl::FormatUsageInfo* srcFormat;
    uint32_t srcWidth;
    uint32_t srcHeight;
    if (!ValidateCurFBForRead("readPixels", &srcFormat, &srcWidth, &srcHeight))
        return;

    auto srcType = srcFormat->format->componentType;
    const bool isSrcTypeFloat = (srcType == webgl::ComponentType::Float);

    // Check the format and type params to assure they are an acceptable pair (as per spec)

    const GLenum mainReadFormat = LOCAL_GL_RGBA;
    const GLenum mainReadType = isSrcTypeFloat ? LOCAL_GL_FLOAT
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

    // Now that the errors are out of the way, on to actually reading!

    uint32_t readX, readY;
    uint32_t writeX, writeY;
    uint32_t rwWidth, rwHeight;
    Intersect(srcWidth, x, width, &readX, &writeX, &rwWidth);
    Intersect(srcHeight, y, height, &readY, &writeY, &rwHeight);

    if (rwWidth == uint32_t(width) && rwHeight == uint32_t(height)) {
        // Warning: Possibly shared memory.  See bug 1225033.
        DoReadPixelsAndConvert(x, y, width, height, format, type, data, auxReadFormat,
                               auxReadType);
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

    // Currently, the spec dictates that we need to zero the out-of-bounds pixels.

    // Ideally we could just ReadPixels into the buffer, then zero the undefined parts.
    // However, we can't do this for *shared* ArrayBuffers, as they can have racey
    // accesses from Workers.

    // We can use a couple tricks to do this faster, but we shouldn't encourage this
    // anyway. Why not just do it the really safe, dead-simple way, even if it is
    // hilariously slow?

    ////////////////////////////////////
    // Clear the targetted pixels to zero.

    if (mPixelStore_PackRowLength ||
        mPixelStore_PackSkipPixels ||
        mPixelStore_PackSkipRows)
    {
        // Targetted bytes might not be contiguous, so do it row-by-row.
        uint8_t* row = (uint8_t*)data + startOffset.value();
        const auto bytesPerRow = bytesPerPixel * width;
        for (uint32_t j = 0; j < uint32_t(height); j++) {
            std::memset(row, 0, bytesPerRow);
            row += rowStride.value();
        }
    } else {
        std::memset(data, 0, bytesNeeded.value());
    }

    ////////////////////////////////////
    // Read only the in-bounds pixels.

    if (!rwWidth || !rwHeight) {
        // There aren't any, so we're 'done'.
        DummyFramebufferOperation("readPixels");
        return;
    }

    if (IsWebGL2()) {
        if (!mPixelStore_PackRowLength) {
            gl->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, width);
        }
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_PIXELS, mPixelStore_PackSkipPixels + writeX);
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, mPixelStore_PackSkipRows + writeY);

        DoReadPixelsAndConvert(readX, readY, rwWidth, rwHeight, format, type, data,
                               auxReadFormat, auxReadType);

        gl->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, mPixelStore_PackRowLength);
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_PIXELS, mPixelStore_PackSkipPixels);
        gl->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, mPixelStore_PackSkipRows);
    } else {
        // I *did* say "hilariously slow".

        uint8_t* row = (uint8_t*)data + startOffset.value() + writeX * bytesPerPixel;
        row += writeY * rowStride.value();
        for (uint32_t j = 0; j < rwHeight; j++) {
            DoReadPixelsAndConvert(readX, readY+j, rwWidth, 1, format, type, row,
                                   auxReadFormat, auxReadType);
            row += rowStride.value();
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

    SetFullAlpha(data, format, type, width, height, rowStride.value());
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

    if (uint32_t(width) > mImplMaxRenderbufferSize ||
        uint32_t(height) > mImplMaxRenderbufferSize)
    {
        ErrorInvalidValue("%s: Width or height exceeds maximum renderbuffer"
                          " size.", funcName);
        return;
    }

    const auto usage = mFormatUsage->GetRBUsage(internalFormat);
    if (!usage) {
        ErrorInvalidEnumInfo("`internalFormat`", funcName, internalFormat);
        return;
    }

    // Validation complete.

    MakeContextCurrent();

    GetAndFlushUnderlyingGLErrors();
    mBoundRenderbuffer->RenderbufferStorage(samples, usage, width, height);
    GLenum error = GetAndFlushUnderlyingGLErrors();
    if (error) {
        GenerateWarning("%s generated error %s", funcName,
                        ErrorName(error));
        return;
    }
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

    RefPtr<WebGLFramebuffer> globj = new WebGLFramebuffer(this, fbo);
    return globj.forget();
}

already_AddRefed<WebGLRenderbuffer>
WebGLContext::CreateRenderbuffer()
{
    if (IsContextLost())
        return nullptr;
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

WebGLTexelFormat
GetWebGLTexelFormat(TexInternalFormat effectiveInternalFormat)
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

} // namespace mozilla
