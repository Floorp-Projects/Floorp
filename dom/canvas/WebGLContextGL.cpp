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
#include "WebGLQuery.h"
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

bool
WebGLContext::ValidateObject(const char* const argName, const WebGLProgram& object)
{
    return ValidateObject(argName, object, true);
}

bool
WebGLContext::ValidateObject(const char* const argName, const WebGLShader& object)
{
    return ValidateObject(argName, object, true);
}

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gl;

//
//  WebGL API
//

void
WebGLContext::ActiveTexture(GLenum texture)
{
    const FuncScope funcScope(*this, "activeTexture");
    if (IsContextLost())
        return;

    if (texture < LOCAL_GL_TEXTURE0 ||
        texture >= LOCAL_GL_TEXTURE0 + mGLMaxTextureUnits)
    {
        return ErrorInvalidEnum(
            "Texture unit %d out of range. "
            "Accepted values range from TEXTURE0 to TEXTURE0 + %d. "
            "Notice that TEXTURE0 != 0.",
            texture, mGLMaxTextureUnits);
    }

    mActiveTexture = texture - LOCAL_GL_TEXTURE0;
    gl->fActiveTexture(texture);
}

void
WebGLContext::AttachShader(WebGLProgram& program, WebGLShader& shader)
{
    const FuncScope funcScope(*this, "attachShader");
    if (IsContextLost())
        return;

    if (!ValidateObject("program", program) ||
        !ValidateObject("shader", shader))
    {
        return;
    }

    program.AttachShader(&shader);
}

void
WebGLContext::BindAttribLocation(WebGLProgram& prog, GLuint location,
                                 const nsAString& name)
{
    const FuncScope funcScope(*this, "bindAttribLocation");
    if (IsContextLost())
        return;

    if (!ValidateObject("program", prog))
        return;

    prog.BindAttribLocation(location, name);
}

void
WebGLContext::BindFramebuffer(GLenum target, WebGLFramebuffer* wfb)
{
    const FuncScope funcScope(*this, "bindFramebuffer");
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target))
        return;

    if (wfb && !ValidateObject("fb", *wfb))
        return;

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
    const FuncScope funcScope(*this, "bindRenderbuffer");
    if (IsContextLost())
        return;

    if (target != LOCAL_GL_RENDERBUFFER)
        return ErrorInvalidEnumInfo("target", target);

    if (wrb && !ValidateObject("rb", *wrb))
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
    const FuncScope funcScope(*this, "blendEquation");
    if (IsContextLost())
        return;

    if (!ValidateBlendEquationEnum(mode, "mode"))
        return;

    gl->fBlendEquation(mode);
}

void WebGLContext::BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    const FuncScope funcScope(*this, "blendEquationSeparate");
    if (IsContextLost())
        return;

    if (!ValidateBlendEquationEnum(modeRGB, "modeRGB") ||
        !ValidateBlendEquationEnum(modeAlpha, "modeAlpha"))
    {
        return;
    }

    gl->fBlendEquationSeparate(modeRGB, modeAlpha);
}

static bool
ValidateBlendFuncEnum(WebGLContext* webgl, GLenum factor, const char* varName)
{
    switch (factor) {
    case LOCAL_GL_ZERO:
    case LOCAL_GL_ONE:
    case LOCAL_GL_SRC_COLOR:
    case LOCAL_GL_ONE_MINUS_SRC_COLOR:
    case LOCAL_GL_DST_COLOR:
    case LOCAL_GL_ONE_MINUS_DST_COLOR:
    case LOCAL_GL_SRC_ALPHA:
    case LOCAL_GL_ONE_MINUS_SRC_ALPHA:
    case LOCAL_GL_DST_ALPHA:
    case LOCAL_GL_ONE_MINUS_DST_ALPHA:
    case LOCAL_GL_CONSTANT_COLOR:
    case LOCAL_GL_ONE_MINUS_CONSTANT_COLOR:
    case LOCAL_GL_CONSTANT_ALPHA:
    case LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA:
    case LOCAL_GL_SRC_ALPHA_SATURATE:
        return true;

    default:
        webgl->ErrorInvalidEnumInfo(varName, factor);
        return false;
    }
}

static bool
ValidateBlendFuncEnums(WebGLContext* webgl, GLenum srcRGB, GLenum srcAlpha,
                       GLenum dstRGB, GLenum dstAlpha)
{
    if (!webgl->IsWebGL2()) {
       if (dstRGB == LOCAL_GL_SRC_ALPHA_SATURATE || dstAlpha == LOCAL_GL_SRC_ALPHA_SATURATE) {
          webgl->ErrorInvalidEnum("LOCAL_GL_SRC_ALPHA_SATURATE as a destination"
                                  " blend function is disallowed in WebGL 1 (dstRGB ="
                                  " 0x%04x, dstAlpha = 0x%04x).",
                                  dstRGB, dstAlpha);
          return false;
       }
    }

    if (!ValidateBlendFuncEnum(webgl, srcRGB, "srcRGB") ||
        !ValidateBlendFuncEnum(webgl, srcAlpha, "srcAlpha") ||
        !ValidateBlendFuncEnum(webgl, dstRGB, "dstRGB") ||
        !ValidateBlendFuncEnum(webgl, dstAlpha, "dstAlpha"))
    {
       return false;
    }

    return true;
}

void WebGLContext::BlendFunc(GLenum sfactor, GLenum dfactor)
{
    const FuncScope funcScope(*this, "blendFunc");
    if (IsContextLost())
        return;

    if (!ValidateBlendFuncEnums(this, sfactor, sfactor, dfactor, dfactor))
       return;

    if (!ValidateBlendFuncEnumsCompatibility(sfactor, dfactor, "srcRGB and dstRGB"))
        return;

    gl->fBlendFunc(sfactor, dfactor);
}

void
WebGLContext::BlendFuncSeparate(GLenum srcRGB, GLenum dstRGB,
                                GLenum srcAlpha, GLenum dstAlpha)
{
    const FuncScope funcScope(*this, "blendFuncSeparate");
    if (IsContextLost())
        return;

    if (!ValidateBlendFuncEnums(this, srcRGB, srcAlpha, dstRGB, dstAlpha))
       return;

    // note that we only check compatibity for the RGB enums, no need to for the Alpha enums, see
    // "Section 6.8 forgetting to mention alpha factors?" thread on the public_webgl mailing list
    if (!ValidateBlendFuncEnumsCompatibility(srcRGB, dstRGB, "srcRGB and dstRGB"))
        return;

    gl->fBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

GLenum
WebGLContext::CheckFramebufferStatus(GLenum target)
{
    const FuncScope funcScope(*this, "checkFramebufferStatus");
    if (IsContextLost())
        return LOCAL_GL_FRAMEBUFFER_UNSUPPORTED;

    if (!ValidateFramebufferTarget(target))
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

    return fb->CheckFramebufferStatus().get();
}

already_AddRefed<WebGLProgram>
WebGLContext::CreateProgram()
{
    const FuncScope funcScope(*this, "createProgram");
    if (IsContextLost())
        return nullptr;
    RefPtr<WebGLProgram> globj = new WebGLProgram(this);
    return globj.forget();
}

already_AddRefed<WebGLShader>
WebGLContext::CreateShader(GLenum type)
{
    const FuncScope funcScope(*this, "createShader");
    if (IsContextLost())
        return nullptr;

    if (type != LOCAL_GL_VERTEX_SHADER &&
        type != LOCAL_GL_FRAGMENT_SHADER)
    {
        ErrorInvalidEnumInfo("type", type);
        return nullptr;
    }

    RefPtr<WebGLShader> shader = new WebGLShader(this, type);
    return shader.forget();
}

void
WebGLContext::CullFace(GLenum face)
{
    const FuncScope funcScope(*this, "cullFace");
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face))
        return;

    gl->fCullFace(face);
}

void
WebGLContext::DeleteFramebuffer(WebGLFramebuffer* fbuf)
{
    const FuncScope funcScope(*this, "deleteFramebuffer");
    if (!ValidateDeleteObject(fbuf))
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
    const FuncScope funcScope(*this, "deleteRenderbuffer");
    if (!ValidateDeleteObject(rbuf))
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
    const FuncScope funcScope(*this, "deleteTexture");
    if (!ValidateDeleteObject(tex))
        return;

    if (mBoundDrawFramebuffer)
        mBoundDrawFramebuffer->DetachTexture(tex);

    if (mBoundReadFramebuffer)
        mBoundReadFramebuffer->DetachTexture(tex);

    GLuint activeTexture = mActiveTexture;
    for (uint32_t i = 0; i < mGLMaxTextureUnits; i++) {
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
    const FuncScope funcScope(*this, "deleteProgram");
    if (!ValidateDeleteObject(prog))
        return;

    prog->RequestDelete();
}

void
WebGLContext::DeleteShader(WebGLShader* shader)
{
    const FuncScope funcScope(*this, "deleteShader");
    if (!ValidateDeleteObject(shader))
        return;

    shader->RequestDelete();
}

void
WebGLContext::DetachShader(WebGLProgram& program, const WebGLShader& shader)
{
    const FuncScope funcScope(*this, "detachShader");
    if (IsContextLost())
        return;

    // It's valid to attempt to detach a deleted shader, since it's still a
    // shader.
    if (!ValidateObject("program", program) ||
        !ValidateObjectAllowDeleted("shader", shader))
    {
        return;
    }

    program.DetachShader(&shader);
}

static bool
ValidateComparisonEnum(WebGLContext& webgl, const GLenum func)
{
    switch (func) {
    case LOCAL_GL_NEVER:
    case LOCAL_GL_LESS:
    case LOCAL_GL_LEQUAL:
    case LOCAL_GL_GREATER:
    case LOCAL_GL_GEQUAL:
    case LOCAL_GL_EQUAL:
    case LOCAL_GL_NOTEQUAL:
    case LOCAL_GL_ALWAYS:
        return true;

    default:
        webgl.ErrorInvalidEnumInfo("func", func);
        return false;
    }
}

void
WebGLContext::DepthFunc(GLenum func)
{
    const FuncScope funcScope(*this, "depthFunc");
    if (IsContextLost())
        return;

    if (!ValidateComparisonEnum(*this, func))
        return;

    gl->fDepthFunc(func);
}

void
WebGLContext::DepthRange(GLfloat zNear, GLfloat zFar)
{
    const FuncScope funcScope(*this, "depthRange");
    if (IsContextLost())
        return;

    if (zNear > zFar)
        return ErrorInvalidOperation("the near value is greater than the far value!");

    gl->fDepthRange(zNear, zFar);
}

void
WebGLContext::FramebufferRenderbuffer(GLenum target, GLenum attachment,
                                      GLenum rbtarget, WebGLRenderbuffer* wrb)
{
    const FuncScope funcScope(*this, "framebufferRenderbuffer");
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target))
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
        return ErrorInvalidOperation("Cannot modify framebuffer 0.");

    fb->FramebufferRenderbuffer(attachment, rbtarget, wrb);
}

void
WebGLContext::FramebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   WebGLTexture* tobj,
                                   GLint level)
{
    const FuncScope funcScope(*this, "framebufferTexture2D");
    if (IsContextLost())
        return;

    if (!ValidateFramebufferTarget(target))
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
        return ErrorInvalidOperation("Cannot modify framebuffer 0.");

    fb->FramebufferTexture2D(attachment, textarget, tobj, level);
}

void
WebGLContext::FrontFace(GLenum mode)
{
    const FuncScope funcScope(*this, "frontFace");
    if (IsContextLost())
        return;

    switch (mode) {
        case LOCAL_GL_CW:
        case LOCAL_GL_CCW:
            break;
        default:
            return ErrorInvalidEnumInfo("mode", mode);
    }

    gl->fFrontFace(mode);
}

already_AddRefed<WebGLActiveInfo>
WebGLContext::GetActiveAttrib(const WebGLProgram& prog, GLuint index)
{
    const FuncScope funcScope(*this, "getActiveAttrib");
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("program", prog))
        return nullptr;

    return prog.GetActiveAttrib(index);
}

already_AddRefed<WebGLActiveInfo>
WebGLContext::GetActiveUniform(const WebGLProgram& prog, GLuint index)
{
    const FuncScope funcScope(*this, "getActiveUniform");
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("program", prog))
        return nullptr;

    return prog.GetActiveUniform(index);
}

void
WebGLContext::GetAttachedShaders(const WebGLProgram& prog,
                                 dom::Nullable<nsTArray<RefPtr<WebGLShader>>>& retval)
{
    retval.SetNull();
    const FuncScope funcScope(*this, "getAttachedShaders");
    if (IsContextLost())
        return;

    if (!ValidateObject("prog", prog))
        return;

    prog.GetAttachedShaders(&retval.SetValue());
}

GLint
WebGLContext::GetAttribLocation(const WebGLProgram& prog, const nsAString& name)
{
    const FuncScope funcScope(*this, "getAttribLocation");
    if (IsContextLost())
        return -1;

    if (!ValidateObject("program", prog))
        return -1;

    return prog.GetAttribLocation(name);
}

JS::Value
WebGLContext::GetBufferParameter(GLenum target, GLenum pname)
{
    const FuncScope funcScope(*this, "getBufferParameter");
    if (IsContextLost())
        return JS::NullValue();

    const auto& slot = ValidateBufferSlot(target);
    if (!slot)
        return JS::NullValue();
    const auto& buffer = *slot;

    if (!buffer) {
        ErrorInvalidOperation("Buffer for `target` is null.");
        return JS::NullValue();
    }

    switch (pname) {
    case LOCAL_GL_BUFFER_SIZE:
        return JS::NumberValue(buffer->ByteLength());

    case LOCAL_GL_BUFFER_USAGE:
        return JS::NumberValue(buffer->Usage());

    default:
        ErrorInvalidEnumInfo("pname", pname);
        return JS::NullValue();
    }
}

JS::Value
WebGLContext::GetFramebufferAttachmentParameter(JSContext* cx,
                                                GLenum target,
                                                GLenum attachment,
                                                GLenum pname,
                                                ErrorResult& rv)
{
    const FuncScope funcScope(*this, "getFramebufferAttachmentParameter");
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateFramebufferTarget(target))
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

    if (fb)
        return fb->GetAttachmentParameter(cx, target, attachment, pname, &rv);

    ////////////////////////////////////

    if (!IsWebGL2()) {
        ErrorInvalidOperation("Querying against the default framebuffer is not"
                              " allowed in WebGL 1.");
        return JS::NullValue();
    }

    switch (attachment) {
    case LOCAL_GL_BACK:
    case LOCAL_GL_DEPTH:
    case LOCAL_GL_STENCIL:
        break;

    default:
        ErrorInvalidEnum("For the default framebuffer, can only query COLOR, DEPTH,"
                         " or STENCIL.");
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
            ErrorInvalidEnum("With the default framebuffer, can only query COLOR, DEPTH,"
                             " or STENCIL for GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE");
            return JS::NullValue();
        }
        return JS::Int32Value(LOCAL_GL_FRAMEBUFFER_DEFAULT);

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

    ErrorInvalidEnumInfo("pname", pname);
    return JS::NullValue();
}

JS::Value
WebGLContext::GetRenderbufferParameter(GLenum target, GLenum pname)
{
    const FuncScope funcScope(*this, "getRenderbufferParameter");
    if (IsContextLost())
        return JS::NullValue();

    if (target != LOCAL_GL_RENDERBUFFER) {
        ErrorInvalidEnumInfo("target", target);
        return JS::NullValue();
    }

    if (!mBoundRenderbuffer) {
        ErrorInvalidOperation("No renderbuffer is bound.");
        return JS::NullValue();
    }

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

    ErrorInvalidEnumInfo("pname", pname);
    return JS::NullValue();
}

already_AddRefed<WebGLTexture>
WebGLContext::CreateTexture()
{
    const FuncScope funcScope(*this, "createTexture");
    if (IsContextLost())
        return nullptr;

    GLuint tex = 0;
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
    const FuncScope funcScope(*this, "getError");
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
            return LOCAL_GL_CONTEXT_LOST_WEBGL;
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

    GetAndFlushUnderlyingGLErrors();

    err = GetAndClearError(&mUnderlyingGLError);
    return err;
}

JS::Value
WebGLContext::GetProgramParameter(const WebGLProgram& prog, GLenum pname)
{
    const FuncScope funcScope(*this, "getProgramParameter");
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObjectAllowDeleted("program", prog))
        return JS::NullValue();

    return prog.GetProgramParameter(pname);
}

void
WebGLContext::GetProgramInfoLog(const WebGLProgram& prog, nsAString& retval)
{
    retval.SetIsVoid(true);
    const FuncScope funcScope(*this, "getProgramInfoLog");

    if (IsContextLost())
        return;

    if (!ValidateObject("program", prog))
        return;

    prog.GetProgramInfoLog(&retval);
}

JS::Value
WebGLContext::GetUniform(JSContext* js, const WebGLProgram& prog,
                         const WebGLUniformLocation& loc)
{
    const FuncScope funcScope(*this, "getUniform");
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObject("program", prog))
        return JS::NullValue();

    if (!ValidateObjectAllowDeleted("location", loc))
        return JS::NullValue();

    if (!loc.ValidateForProgram(&prog))
        return JS::NullValue();

    return loc.GetUniform(js);
}

already_AddRefed<WebGLUniformLocation>
WebGLContext::GetUniformLocation(const WebGLProgram& prog, const nsAString& name)
{
    const FuncScope funcScope(*this, "getUniformLocation");
    if (IsContextLost())
        return nullptr;

    if (!ValidateObject("program", prog))
        return nullptr;

    return prog.GetUniformLocation(name);
}

void
WebGLContext::Hint(GLenum target, GLenum mode)
{
    const FuncScope funcScope(*this, "hint");
    if (IsContextLost())
        return;

    bool isValid = false;

    switch (target) {
    case LOCAL_GL_GENERATE_MIPMAP_HINT:
        mGenerateMipmapHint = mode;
        isValid = true;

        // Deprecated and removed in desktop GL Core profiles.
        if (gl->IsCoreProfile())
            return;

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
        return ErrorInvalidEnumInfo("target", target);

    gl->fHint(target, mode);
}

// -

bool
WebGLContext::IsBuffer(const WebGLBuffer* const obj)
{
    const FuncScope funcScope(*this, "isBuffer");
    if (!ValidateIsObject(obj))
        return false;

    return gl->fIsBuffer(obj->mGLName);
}

bool
WebGLContext::IsFramebuffer(const WebGLFramebuffer* const obj)
{
    const FuncScope funcScope(*this, "isFramebuffer");
    if (!ValidateIsObject(obj))
        return false;

#ifdef ANDROID
    if (gl->WorkAroundDriverBugs() &&
        gl->Renderer() == GLRenderer::AndroidEmulator)
    {
        return obj->mIsFB;
    }
#endif

    return gl->fIsFramebuffer(obj->mGLName);
}

bool
WebGLContext::IsProgram(const WebGLProgram* const obj)
{
    const FuncScope funcScope(*this, "isProgram");
    return ValidateIsObject(obj);
}

bool
WebGLContext::IsQuery(const WebGLQuery* const obj)
{
    const FuncScope funcScope(*this, "isQuery");
    if (!ValidateIsObject(obj))
        return false;

    return obj->IsQuery();
}

bool
WebGLContext::IsRenderbuffer(const WebGLRenderbuffer* const obj)
{
    const FuncScope funcScope(*this, "isRenderbuffer");
    if (!ValidateIsObject(obj))
        return false;

    return obj->mHasBeenBound;
}

bool
WebGLContext::IsShader(const WebGLShader* const obj)
{
    const FuncScope funcScope(*this, "isShader");
    return ValidateIsObject(obj);
}

bool
WebGLContext::IsTexture(const WebGLTexture* const obj)
{
    const FuncScope funcScope(*this, "isTexture");
    if (!ValidateIsObject(obj))
        return false;

    return obj->IsTexture();
}

bool
WebGLContext::IsVertexArray(const WebGLVertexArray* const obj)
{
    const FuncScope funcScope(*this, "isVertexArray");
    if (!ValidateIsObject(obj))
        return false;

    return obj->IsVertexArray();
}

// -

void
WebGLContext::LinkProgram(WebGLProgram& prog)
{
    const FuncScope funcScope(*this, "linkProgram");
    if (IsContextLost())
        return;

    if (!ValidateObject("prog", prog))
        return;

    prog.LinkProgram();

    if (!prog.IsLinked()) {
        // If we failed to link, but `prog == mCurrentProgram`, we are *not* supposed to
        // null out mActiveProgramLinkInfo.
        return;
    }

    if (&prog == mCurrentProgram) {
        mActiveProgramLinkInfo = prog.LinkInfo();

        if (gl->WorkAroundDriverBugs() &&
            gl->Vendor() == gl::GLVendor::NVIDIA)
        {
            gl->fUseProgram(prog.mGLName);
        }
    }
}

void
WebGLContext::PixelStorei(GLenum pname, GLint param)
{
    const FuncScope funcScope(*this, "pixelStorei");
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
            if (!ValidateNonNegative("param", param))
                return;

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
            ErrorInvalidEnumInfo("colorspace conversion parameter", param);
            return;
        }

    case UNPACK_REQUIRE_FASTPATH:
        if (IsExtensionEnabled(WebGLExtensionID::MOZ_debug)) {
            mPixelStore_RequireFastPath = bool(param);
            return;
        }
        break;

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

            gl->fPixelStorei(pname, param);
            return;

        default:
            ErrorInvalidValue("Invalid pack/unpack alignment value.");
            return;
        }


    default:
        break;
    }

    ErrorInvalidEnumInfo("pname", pname);
}

bool
WebGLContext::DoReadPixelsAndConvert(const webgl::FormatInfo* srcFormat, GLint x, GLint y,
                                     GLsizei width, GLsizei height, GLenum format,
                                     GLenum destType, void* dest, uint32_t destSize,
                                     uint32_t rowStride)
{
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
WebGLContext::ReadPixels_SharedPrecheck(CallerType aCallerType,
                                        ErrorResult& out_error)
{
    if (mCanvasElement &&
        mCanvasElement->IsWriteOnly() &&
        aCallerType != CallerType::System)
    {
        GenerateWarning("readPixels: Not allowed");
        out_error.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return false;
    }

    return true;
}

bool
WebGLContext::ValidatePackSize(uint32_t width, uint32_t height,
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
        ErrorInvalidOperation("SKIP_PIXELS + width > ROW_LENGTH.");
        return false;
    }

    const auto rowLengthBytes = CheckedUint32(rowLength) * bytesPerPixel;
    const auto rowStride = RoundUpToMultipleOf(rowLengthBytes, alignment);

    const auto usedBytesPerRow = usedPixelsPerRow * bytesPerPixel;
    const auto usedBytesPerImage = (usedRowsPerImage - 1) * rowStride + usedBytesPerRow;

    if (!rowStride.isValid() || !usedBytesPerImage.isValid()) {
        ErrorInvalidOperation("Invalid UNPACK_ params.");
        return false;
    }

    *out_rowStride = rowStride.value();
    *out_endOffset = usedBytesPerImage.value();
    return true;
}

void
WebGLContext::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                         GLenum type, const dom::ArrayBufferView& dstView,
                         GLuint dstElemOffset, CallerType aCallerType,
                         ErrorResult& out_error)
{
    const FuncScope funcScope(*this, "readPixels");
    if (IsContextLost())
        return;

    if (!ReadPixels_SharedPrecheck(aCallerType, out_error))
        return;

    if (mBoundPixelPackBuffer) {
        ErrorInvalidOperation("PIXEL_PACK_BUFFER must be null.");
        return;
    }

    ////

    js::Scalar::Type reqScalarType;
    if (!GetJSScalarFromGLType(type, &reqScalarType)) {
        ErrorInvalidEnumInfo("type", type);
        return;
    }

    const auto& viewElemType = dstView.Type();
    if (viewElemType != reqScalarType) {
        ErrorInvalidOperation("`pixels` type does not match `type`.");
        return;
    }

    ////

    uint8_t* bytes;
    size_t byteLen;
    if (!ValidateArrayBufferView(dstView, dstElemOffset, 0, &bytes, &byteLen))
        return;

    ////

    ReadPixelsImpl(x, y, width, height, format, type, bytes, byteLen);
}

void
WebGLContext::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                         GLenum type, WebGLsizeiptr offset,
                         CallerType aCallerType, ErrorResult& out_error)
{
    const FuncScope funcScope(*this, "readPixels");
    if (IsContextLost())
        return;

    if (!ReadPixels_SharedPrecheck(aCallerType, out_error))
        return;

    const auto& buffer = ValidateBufferSelection(LOCAL_GL_PIXEL_PACK_BUFFER);
    if (!buffer)
        return;

    //////

    if (!ValidateNonNegative("offset", offset))
        return;

    {
        const auto bytesPerType = webgl::BytesPerPixel({LOCAL_GL_RED, type});

        if (offset % bytesPerType != 0) {
            ErrorInvalidOperation("`offset` must be divisible by the size of `type`"
                                  " in bytes.");
            return;
        }
    }

    //////

    const auto bytesAvailable = buffer->ByteLength();
    const auto checkedBytesAfterOffset = CheckedUint32(bytesAvailable) - offset;

    uint32_t bytesAfterOffset = 0;
    if (checkedBytesAfterOffset.isValid()) {
        bytesAfterOffset = checkedBytesAfterOffset.value();
    }

    const ScopedLazyBind lazyBind(gl, LOCAL_GL_PIXEL_PACK_BUFFER, buffer);

    ReadPixelsImpl(x, y, width, height, format, type, (void*)offset, bytesAfterOffset);

    buffer->ResetLastUpdateFenceId();
}

static webgl::PackingInfo
DefaultReadPixelPI(const webgl::FormatUsageInfo* usage)
{
    MOZ_ASSERT(usage->IsRenderable());

    switch (usage->format->componentType) {
    case webgl::ComponentType::NormUInt:
        return { LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE };

    case webgl::ComponentType::Int:
        return { LOCAL_GL_RGBA_INTEGER, LOCAL_GL_INT };

    case webgl::ComponentType::UInt:
        return { LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_INT };

    case webgl::ComponentType::Float:
        return { LOCAL_GL_RGBA, LOCAL_GL_FLOAT };

    default:
        MOZ_CRASH();
    }
}

static bool
ArePossiblePackEnums(const WebGLContext* webgl, const webgl::PackingInfo& pi)
{
    // OpenGL ES 2.0 $4.3.1 - IMPLEMENTATION_COLOR_READ_{TYPE/FORMAT} is a valid
    // combination for glReadPixels()...

    // So yeah, we are actually checking that these are valid as /unpack/ formats, instead
    // of /pack/ formats here, but it should cover the INVALID_ENUM cases.
    if (!webgl->mFormatUsage->AreUnpackEnumsValid(pi.format, pi.type))
        return false;

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
        return false;
    }

    if (pi.type == LOCAL_GL_UNSIGNED_INT_24_8)
        return false;

    return true;
}

webgl::PackingInfo
WebGLContext::ValidImplementationColorReadPI(const webgl::FormatUsageInfo* usage) const
{
    const auto defaultPI = DefaultReadPixelPI(usage);

    // ES2_compatibility always returns RGBA/UNSIGNED_BYTE, so branch on actual IsGLES().
    // Also OSX+NV generates an error here.
    if (!gl->IsGLES())
        return defaultPI;

    webgl::PackingInfo implPI;
    gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT, (GLint*)&implPI.format);
    gl->fGetIntegerv(LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE, (GLint*)&implPI.type);

    if (!ArePossiblePackEnums(this, implPI))
        return defaultPI;

    return implPI;
}

static bool
ValidateReadPixelsFormatAndType(const webgl::FormatUsageInfo* srcUsage,
                                const webgl::PackingInfo& pi, gl::GLContext* gl,
                                WebGLContext* webgl)
{
    if (!ArePossiblePackEnums(webgl, pi)) {
        webgl->ErrorInvalidEnum("Unexpected format or type.");
        return false;
    }

    const auto defaultPI = DefaultReadPixelPI(srcUsage);
    if (pi == defaultPI)
        return true;

    ////

    // OpenGL ES 3.0.4 p194 - When the internal format of the rendering surface is
    // RGB10_A2, a third combination of format RGBA and type UNSIGNED_INT_2_10_10_10_REV
    // is accepted.

    if (webgl->IsWebGL2() &&
        srcUsage->format->effectiveFormat == webgl::EffectiveFormat::RGB10_A2 &&
        pi.format == LOCAL_GL_RGBA &&
        pi.type == LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV)
    {
        return true;
    }

    ////

    MOZ_ASSERT(gl->IsCurrent());
    const auto implPI = webgl->ValidImplementationColorReadPI(srcUsage);
    if (pi == implPI)
        return true;

    ////

    webgl->ErrorInvalidOperation("Incompatible format or type.");
    return false;
}

void
WebGLContext::ReadPixelsImpl(GLint x, GLint y, GLsizei rawWidth, GLsizei rawHeight,
                             GLenum packFormat, GLenum packType, void* dest,
                             uint32_t dataLen)
{
    if (!ValidateNonNegative("width", rawWidth) ||
        !ValidateNonNegative("height", rawHeight))
    {
        return;
    }

    const uint32_t width(rawWidth);
    const uint32_t height(rawHeight);

    //////

    const webgl::FormatUsageInfo* srcFormat;
    uint32_t srcWidth;
    uint32_t srcHeight;
    if (!BindCurFBForColorRead(&srcFormat, &srcWidth, &srcHeight))
        return;

    //////

    const webgl::PackingInfo pi = {packFormat, packType};
    if (!ValidateReadPixelsFormatAndType(srcFormat, pi, gl, this))
        return;

    uint8_t bytesPerPixel;
    if (!webgl::GetBytesPerPixel(pi, &bytesPerPixel)) {
        ErrorInvalidOperation("Unsupported format and type.");
        return;
    }

    //////

    uint32_t rowStride;
    uint32_t bytesNeeded;
    if (!ValidatePackSize(width, height, bytesPerPixel, &rowStride, &bytesNeeded))
        return;

    if (bytesNeeded > dataLen) {
        ErrorInvalidOperation("buffer too small");
        return;
    }

    ////

    int32_t readX, readY;
    int32_t writeX, writeY;
    int32_t rwWidth, rwHeight;
    if (!Intersect(srcWidth, x, width, &readX, &writeX, &rwWidth) ||
        !Intersect(srcHeight, y, height, &readY, &writeY, &rwHeight))
    {
        ErrorOutOfMemory("Bad subrect selection.");
        return;
    }

    ////////////////
    // Now that the errors are out of the way, on to actually reading!

    if (!rwWidth || !rwHeight) {
        // Disjoint rects, so we're done already.
        DummyReadFramebufferOperation();
        return;
    }

    if (uint32_t(rwWidth) == width &&
        uint32_t(rwHeight) == height)
    {
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
    GenerateWarning("Out-of-bounds reads with readPixels are deprecated, and"
                    " may be slow.");

    ////////////////////////////////////
    // Read only the in-bounds pixels.

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
        for (uint32_t j = 0; j < uint32_t(rwHeight); j++) {
            DoReadPixelsAndConvert(srcFormat->format, readX, readY+j, rwWidth, 1,
                                   packFormat, packType, row, dataLen, rowStride);
            row += rowStride;
        }
    }
}

void
WebGLContext::RenderbufferStorage_base(GLenum target, GLsizei samples,
                                       GLenum internalFormat,
                                       GLsizei width, GLsizei height)
{
    if (IsContextLost())
        return;

    if (target != LOCAL_GL_RENDERBUFFER) {
        ErrorInvalidEnumInfo("target", target);
        return;
    }

    if (!mBoundRenderbuffer) {
        ErrorInvalidOperation("Called on renderbuffer 0.");
        return;
    }

    if (!ValidateNonNegative("width", width) ||
        !ValidateNonNegative("height", height) ||
        !ValidateNonNegative("samples", samples))
    {
        return;
    }

    mBoundRenderbuffer->RenderbufferStorage(uint32_t(samples), internalFormat,
                                            uint32_t(width), uint32_t(height));
}

void
WebGLContext::Scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    const FuncScope funcScope(*this, "scissor");
    if (IsContextLost())
        return;

    if (!ValidateNonNegative("width", width) ||
        !ValidateNonNegative("height", height))
    {
        return;
    }

    gl->fScissor(x, y, width, height);
}

void
WebGLContext::StencilFunc(GLenum func, GLint ref, GLuint mask)
{
    const FuncScope funcScope(*this, "stencilFunc");
    if (IsContextLost())
        return;

    if (!ValidateComparisonEnum(*this, func))
        return;

    mStencilRefFront = ref;
    mStencilRefBack = ref;
    mStencilValueMaskFront = mask;
    mStencilValueMaskBack = mask;

    gl->fStencilFunc(func, ref, mask);
}

void
WebGLContext::StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    const FuncScope funcScope(*this, "stencilFuncSeparate");
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face) ||
        !ValidateComparisonEnum(*this, func))
    {
        return;
    }

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

    gl->fStencilFuncSeparate(face, func, ref, mask);
}

void
WebGLContext::StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass)
{
    const FuncScope funcScope(*this, "stencilOp");
    if (IsContextLost())
        return;

    if (!ValidateStencilOpEnum(sfail, "sfail") ||
        !ValidateStencilOpEnum(dpfail, "dpfail") ||
        !ValidateStencilOpEnum(dppass, "dppass"))
        return;

    gl->fStencilOp(sfail, dpfail, dppass);
}

void
WebGLContext::StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
    const FuncScope funcScope(*this, "stencilOpSeparate");
    if (IsContextLost())
        return;

    if (!ValidateFaceEnum(face) ||
        !ValidateStencilOpEnum(sfail, "sfail") ||
        !ValidateStencilOpEnum(dpfail, "dpfail") ||
        !ValidateStencilOpEnum(dppass, "dppass"))
        return;

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
    ValidateIfSampler(WebGLContext* webgl,
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
                webgl->ErrorInvalidValue("This uniform location is a sampler, but %d"
                                         " is not a valid texture unit.",
                                         val);
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
    const FuncScope funcScope(*this, "uniform1i");
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_INT))
        return;

    bool error;
    const ValidateIfSampler validate(this, loc, 1, &a1, &error);
    if (error)
        return;

    gl->fUniform1i(loc->mLoc, a1);
}

void
WebGLContext::Uniform2i(WebGLUniformLocation* loc, GLint a1, GLint a2)
{
    const FuncScope funcScope(*this, "uniform2i");
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_INT))
        return;

    gl->fUniform2i(loc->mLoc, a1, a2);
}

void
WebGLContext::Uniform3i(WebGLUniformLocation* loc, GLint a1, GLint a2, GLint a3)
{
    const FuncScope funcScope(*this, "uniform3i");
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_INT))
        return;

    gl->fUniform3i(loc->mLoc, a1, a2, a3);
}

void
WebGLContext::Uniform4i(WebGLUniformLocation* loc, GLint a1, GLint a2, GLint a3,
                        GLint a4)
{
    const FuncScope funcScope(*this, "uniform4i");
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_INT))
        return;

    gl->fUniform4i(loc->mLoc, a1, a2, a3, a4);
}

//////////

void
WebGLContext::Uniform1f(WebGLUniformLocation* loc, GLfloat a1)
{
    const FuncScope funcScope(*this, "uniform1f");
    if (!ValidateUniformSetter(loc, 1, LOCAL_GL_FLOAT))
        return;

    gl->fUniform1f(loc->mLoc, a1);
}

void
WebGLContext::Uniform2f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2)
{
    const FuncScope funcScope(*this, "uniform2f");
    if (!ValidateUniformSetter(loc, 2, LOCAL_GL_FLOAT))
        return;

    gl->fUniform2f(loc->mLoc, a1, a2);
}

void
WebGLContext::Uniform3f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2,
                        GLfloat a3)
{
    const FuncScope funcScope(*this, "uniform3f");
    if (!ValidateUniformSetter(loc, 3, LOCAL_GL_FLOAT))
        return;

    gl->fUniform3f(loc->mLoc, a1, a2, a3);
}

void
WebGLContext::Uniform4f(WebGLUniformLocation* loc, GLfloat a1, GLfloat a2,
                        GLfloat a3, GLfloat a4)
{
    const FuncScope funcScope(*this, "uniform4f");
    if (!ValidateUniformSetter(loc, 4, LOCAL_GL_FLOAT))
        return;

    gl->fUniform4f(loc->mLoc, a1, a2, a3, a4);
}

////////////////////////////////////////
// Array

static bool
ValidateArrOffsetAndCount(WebGLContext* webgl, size_t elemsAvail,
                          GLuint elemOffset, GLuint elemCountOverride,
                          size_t* const out_elemCount)
{
    if (webgl->IsContextLost())
        return false;

    if (elemOffset > elemsAvail) {
        webgl->ErrorInvalidValue("Bad offset into list.");
        return false;
    }
    elemsAvail -= elemOffset;

    if (elemCountOverride) {
        if (elemCountOverride > elemsAvail) {
            webgl->ErrorInvalidValue("Bad count override for sub-list.");
            return false;
        }
        elemsAvail = elemCountOverride;
    }

    *out_elemCount = elemsAvail;
    return true;
}

void
WebGLContext::UniformNiv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                         const Int32Arr& arr, GLuint elemOffset, GLuint elemCountOverride)
{
    const FuncScope funcScope(*this, funcName);

    size_t elemCount;
    if (!ValidateArrOffsetAndCount(this, arr.elemCount, elemOffset,
                                   elemCountOverride, &elemCount))
    {
        return;
    }
    const auto elemBytes = arr.elemBytes + elemOffset;

    uint32_t numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, N, LOCAL_GL_INT, elemCount,
                                    &numElementsToUpload))
    {
        return;
    }

    bool error;
    const ValidateIfSampler samplerValidator(this, loc, numElementsToUpload,
                                             elemBytes, &error);
    if (error)
        return;

    static const decltype(&gl::GLContext::fUniform1iv) kFuncList[] = {
        &gl::GLContext::fUniform1iv,
        &gl::GLContext::fUniform2iv,
        &gl::GLContext::fUniform3iv,
        &gl::GLContext::fUniform4iv
    };
    const auto func = kFuncList[N-1];

    (gl->*func)(loc->mLoc, numElementsToUpload, elemBytes);
}

void
WebGLContext::UniformNuiv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                          const Uint32Arr& arr, GLuint elemOffset,
                          GLuint elemCountOverride)
{
    const FuncScope funcScope(*this, funcName);

    size_t elemCount;
    if (!ValidateArrOffsetAndCount(this, arr.elemCount, elemOffset,
                                   elemCountOverride, &elemCount))
    {
        return;
    }
    const auto elemBytes = arr.elemBytes + elemOffset;

    uint32_t numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, N, LOCAL_GL_UNSIGNED_INT, elemCount,
                                    &numElementsToUpload))
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

    (gl->*func)(loc->mLoc, numElementsToUpload, elemBytes);
}

void
WebGLContext::UniformNfv(const char* funcName, uint8_t N, WebGLUniformLocation* loc,
                         const Float32Arr& arr, GLuint elemOffset,
                         GLuint elemCountOverride)
{
    const FuncScope funcScope(*this, funcName);

    size_t elemCount;
    if (!ValidateArrOffsetAndCount(this, arr.elemCount, elemOffset,
                                   elemCountOverride, &elemCount))
    {
        return;
    }
    const auto elemBytes = arr.elemBytes + elemOffset;

    uint32_t numElementsToUpload;
    if (!ValidateUniformArraySetter(loc, N, LOCAL_GL_FLOAT, elemCount,
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

    (gl->*func)(loc->mLoc, numElementsToUpload, elemBytes);
}

static inline void
MatrixAxBToRowMajor(const uint8_t width, const uint8_t height,
                    const float* __restrict srcColMajor,
                    float* __restrict dstRowMajor)
{
    for (uint8_t x = 0; x < width; ++x) {
        for (uint8_t y = 0; y < height; ++y) {
            dstRowMajor[y * width + x] = srcColMajor[x * height + y];
        }
    }
}

void
WebGLContext::UniformMatrixAxBfv(const char* funcName, uint8_t A, uint8_t B,
                                 WebGLUniformLocation* loc, const bool transpose,
                                 const Float32Arr& arr, GLuint elemOffset,
                                 GLuint elemCountOverride)
{
    const FuncScope funcScope(*this, funcName);

    size_t elemCount;
    if (!ValidateArrOffsetAndCount(this, arr.elemCount, elemOffset,
                                   elemCountOverride, &elemCount))
    {
        return;
    }
    const auto elemBytes = arr.elemBytes + elemOffset;

    uint32_t numMatsToUpload;
    if (!ValidateUniformMatrixArraySetter(loc, A, B, LOCAL_GL_FLOAT, elemCount,
                                          transpose, &numMatsToUpload))
    {
        return;
    }
    MOZ_ASSERT(!loc->mInfo->mSamplerTexList, "Should not be a sampler.");

    ////

    bool uploadTranspose = transpose;
    const float* uploadBytes = elemBytes;

    UniqueBuffer temp;
    if (!transpose && gl->WorkAroundDriverBugs() && gl->IsANGLE() &&
        gl->IsAtLeast(gl::ContextProfile::OpenGLES, 300))
    {
        // ANGLE is really slow at non-GL-transposed matrices.
        const size_t kElemsPerMat = A * B;

        temp = malloc(numMatsToUpload * kElemsPerMat * sizeof(float));
        if (!temp) {
            ErrorOutOfMemory("Failed to alloc temporary buffer for transposition.");
            return;
        }

        auto srcItr = (const float*)elemBytes;
        auto dstItr = (float*)temp.get();
        const auto srcEnd = srcItr + numMatsToUpload * kElemsPerMat;

        while (srcItr != srcEnd) {
            MatrixAxBToRowMajor(A, B, srcItr, dstItr);
            srcItr += kElemsPerMat;
            dstItr += kElemsPerMat;
        }

        uploadBytes = (const float*)temp.get();
        uploadTranspose = true;
    }

    ////

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

    (gl->*func)(loc->mLoc, numMatsToUpload, uploadTranspose, uploadBytes);
}

////////////////////////////////////////////////////////////////////////////////

void
WebGLContext::UseProgram(WebGLProgram* prog)
{
    const FuncScope funcScope(*this, "useProgram");
    if (IsContextLost())
        return;

    if (!prog) {
        mCurrentProgram = nullptr;
        mActiveProgramLinkInfo = nullptr;
        return;
    }

    if (!ValidateObject("prog", *prog))
        return;

    if (prog->UseProgram()) {
        mCurrentProgram = prog;
        mActiveProgramLinkInfo = mCurrentProgram->LinkInfo();
    }
}

void
WebGLContext::ValidateProgram(const WebGLProgram& prog)
{
    const FuncScope funcScope(*this, "validateProgram");
    if (IsContextLost())
        return;

    if (!ValidateObject("prog", prog))
        return;

    prog.ValidateProgram();
}

already_AddRefed<WebGLFramebuffer>
WebGLContext::CreateFramebuffer()
{
    const FuncScope funcScope(*this, "createFramebuffer");
    if (IsContextLost())
        return nullptr;

    GLuint fbo = 0;
    gl->fGenFramebuffers(1, &fbo);

    RefPtr<WebGLFramebuffer> globj = new WebGLFramebuffer(this, fbo);
    return globj.forget();
}

already_AddRefed<WebGLRenderbuffer>
WebGLContext::CreateRenderbuffer()
{
    const FuncScope funcScope(*this, "createRenderbuffer");
    if (IsContextLost())
        return nullptr;

    RefPtr<WebGLRenderbuffer> globj = new WebGLRenderbuffer(this);
    return globj.forget();
}

void
WebGLContext::Viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    const FuncScope funcScope(*this, "viewport");
    if (IsContextLost())
        return;

    if (!ValidateNonNegative("width", width) ||
        !ValidateNonNegative("height", height))
    {
        return;
    }

    width = std::min(width, (GLsizei)mGLMaxViewportDims[0]);
    height = std::min(height, (GLsizei)mGLMaxViewportDims[1]);

    gl->fViewport(x, y, width, height);

    mViewportX = x;
    mViewportY = y;
    mViewportWidth = width;
    mViewportHeight = height;
}

void
WebGLContext::CompileShader(WebGLShader& shader)
{
    const FuncScope funcScope(*this, "compileShader");
    if (IsContextLost())
        return;

    if (!ValidateObject("shader", shader))
        return;

    shader.CompileShader();
}

JS::Value
WebGLContext::GetShaderParameter(const WebGLShader& shader, GLenum pname)
{
    const FuncScope funcScope(*this, "getShaderParameter");
    if (IsContextLost())
        return JS::NullValue();

    if (!ValidateObjectAllowDeleted("shader", shader))
        return JS::NullValue();

    return shader.GetShaderParameter(pname);
}

void
WebGLContext::GetShaderInfoLog(const WebGLShader& shader, nsAString& retval)
{
    retval.SetIsVoid(true);
    const FuncScope funcScope(*this, "getShaderInfoLog");

    if (IsContextLost())
        return;

    if (!ValidateObject("shader", shader))
        return;

    shader.GetShaderInfoLog(&retval);
}

already_AddRefed<WebGLShaderPrecisionFormat>
WebGLContext::GetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype)
{
    const FuncScope funcScope(*this, "getShaderPrecisionFormat");
    if (IsContextLost())
        return nullptr;

    switch (shadertype) {
        case LOCAL_GL_FRAGMENT_SHADER:
        case LOCAL_GL_VERTEX_SHADER:
            break;
        default:
            ErrorInvalidEnumInfo("shadertype", shadertype);
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
            ErrorInvalidEnumInfo("precisiontype", precisiontype);
            return nullptr;
    }

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
WebGLContext::GetShaderSource(const WebGLShader& shader, nsAString& retval)
{
    retval.SetIsVoid(true);
    const FuncScope funcScope(*this, "getShaderSource");

    if (IsContextLost())
        return;

    if (!ValidateObject("shader", shader))
        return;

    shader.GetShaderSource(&retval);
}

void
WebGLContext::ShaderSource(WebGLShader& shader, const nsAString& source)
{
    const FuncScope funcScope(*this, "shaderSource");
    if (IsContextLost())
        return;

    if (!ValidateObject("shader", shader))
        return;

    shader.ShaderSource(source);
}

void
WebGLContext::LoseContext()
{
    const FuncScope funcScope(*this, "loseContext");
    if (IsContextLost())
        return ErrorInvalidOperation("Context is already lost.");

    ForceLoseContext(true);
}

void
WebGLContext::RestoreContext()
{
    const FuncScope funcScope(*this, "restoreContext");
    if (!IsContextLost())
        return ErrorInvalidOperation("Context is not lost.");

    if (!mLastLossWasSimulated) {
        return ErrorInvalidOperation("Context loss was not simulated."
                                     " Cannot simulate restore.");
    }
    // If we're currently lost, and the last loss was simulated, then
    // we're currently only simulated-lost, allowing us to call
    // restoreContext().

    if (!mAllowContextRestore)
        return ErrorInvalidOperation("Context cannot be restored.");

    ForceRestoreContext();
}

void
WebGLContext::BlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    const FuncScope funcScope(*this, "blendColor");
    if (IsContextLost())
        return;

    gl->fBlendColor(r, g, b, a);
}

void
WebGLContext::Flush()
{
    const FuncScope funcScope(*this, "flush");
    if (IsContextLost())
        return;

    gl->fFlush();
}

void
WebGLContext::Finish()
{
    const FuncScope funcScope(*this, "finish");
    if (IsContextLost())
        return;

    gl->fFinish();

    mCompletedFenceId = mNextFenceId;
    mNextFenceId += 1;
}

void
WebGLContext::LineWidth(GLfloat width)
{
    const FuncScope funcScope(*this, "lineWidth");
    if (IsContextLost())
        return;

    // Doing it this way instead of `if (width <= 0.0)` handles NaNs.
    const bool isValid = width > 0.0;
    if (!isValid) {
        ErrorInvalidValue("`width` must be positive and non-zero.");
        return;
    }

    mLineWidth = width;

    if (gl->IsCoreProfile() && width > 1.0) {
        width = 1.0;
    }

    gl->fLineWidth(width);
}

void
WebGLContext::PolygonOffset(GLfloat factor, GLfloat units)
{
    const FuncScope funcScope(*this, "polygonOffset");
    if (IsContextLost())
        return;

    gl->fPolygonOffset(factor, units);
}

void
WebGLContext::SampleCoverage(GLclampf value, WebGLboolean invert)
{
    const FuncScope funcScope(*this, "sampleCoverage");
    if (IsContextLost())
        return;

    gl->fSampleCoverage(value, invert);
}

} // namespace mozilla
