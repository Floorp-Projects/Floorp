/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Mark Steele <mwsteele@gmail.com>
 *   Cedric Vivier <cedricv@neonux.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "WebGLContext.h"

#include "nsString.h"

#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
//#include "nsIDOMHTMLCanvasElement.h"

#include "nsContentUtils.h"
#include "nsDOMError.h"
#include "nsLayoutUtils.h"

#include "CanvasUtils.h"
#include "NativeJSContext.h"

#include "jstypedarray.h"

using namespace mozilla;

static PRBool BaseTypeAndSizeFromUniformType(WebGLenum uType, WebGLenum *baseType, WebGLint *unitSize);

/* Helper macros for when we're just wrapping a gl method, so that
 * we can avoid having to type this 500 times.  Note that these MUST
 * NOT BE USED if we need to check any of the parameters.
 */

#define GL_SAME_METHOD_0(glname, name)                          \
NS_IMETHODIMP WebGLContext::name() {                            \
    MakeContextCurrent(); gl->f##glname(); return NS_OK;        \
}

#define GL_SAME_METHOD_1(glname, name, t1)          \
NS_IMETHODIMP WebGLContext::name(t1 a1) {           \
    MakeContextCurrent(); gl->f##glname(a1); return NS_OK;  \
}

#define GL_SAME_METHOD_2(glname, name, t1, t2)        \
NS_IMETHODIMP WebGLContext::name(t1 a1, t2 a2) {      \
    MakeContextCurrent(); gl->f##glname(a1,a2); return NS_OK;           \
}

#define GL_SAME_METHOD_3(glname, name, t1, t2, t3)      \
NS_IMETHODIMP WebGLContext::name(t1 a1, t2 a2, t3 a3) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3); return NS_OK;        \
}

#define GL_SAME_METHOD_4(glname, name, t1, t2, t3, t4)         \
NS_IMETHODIMP WebGLContext::name(t1 a1, t2 a2, t3 a3, t4 a4) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3,a4); return NS_OK;     \
}

#define GL_SAME_METHOD_5(glname, name, t1, t2, t3, t4, t5)            \
NS_IMETHODIMP WebGLContext::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3,a4,a5); return NS_OK;  \
}

#define GL_SAME_METHOD_6(glname, name, t1, t2, t3, t4, t5, t6)          \
NS_IMETHODIMP WebGLContext::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) { \
    MakeContextCurrent(); gl->f##glname(a1,a2,a3,a4,a5,a6); return NS_OK; \
}

already_AddRefed<WebGLUniformLocation> WebGLProgram::GetUniformLocationObject(GLint glLocation)
{
    WebGLUniformLocation *existingLocationObject;
    if (mMapUniformLocations.Get(glLocation, &existingLocationObject)) {
        NS_ADDREF(existingLocationObject);
        return existingLocationObject;
    } else {
        nsRefPtr<WebGLUniformLocation> loc = new WebGLUniformLocation(mContext, this, glLocation);
        mMapUniformLocations.Put(glLocation, loc);
        return loc.forget();
    }
}

//
//  WebGL API
//


/* void present (); */
NS_IMETHODIMP
WebGLContext::Present()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GlActiveTexture (in GLenum texture); */
NS_IMETHODIMP
WebGLContext::ActiveTexture(WebGLenum texture)
{
    if (texture < LOCAL_GL_TEXTURE0 || texture >= LOCAL_GL_TEXTURE0+mBound2DTextures.Length())
        return ErrorInvalidEnum("ActiveTexture: texture unit %d out of range (0..%d)",
                                texture, mBound2DTextures.Length()-1);

    MakeContextCurrent();
    mActiveTexture = texture - LOCAL_GL_TEXTURE0;
    gl->fActiveTexture(texture);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::AttachShader(nsIWebGLProgram *pobj, nsIWebGLShader *shobj)
{
    WebGLuint progname, shadername;
    WebGLProgram *program;
    WebGLShader *shader;
    if (!GetConcreteObjectAndGLName(pobj, &program, &progname))
        return ErrorInvalidOperation("AttachShader: invalid program");

    if (!GetConcreteObjectAndGLName(shobj, &shader, &shadername))
        return ErrorInvalidOperation("AttachShader: invalid shader");

    if (!program->AttachShader(shader))
        return ErrorInvalidOperation("AttachShader: shader is already attached");

    MakeContextCurrent();

    gl->fAttachShader(progname, shadername);

    return NS_OK;
}


NS_IMETHODIMP
WebGLContext::BindAttribLocation(nsIWebGLProgram *pobj, WebGLuint location, const nsAString& name)
{
    WebGLuint progname;
    if (!GetGLName<WebGLProgram>(pobj, &progname))
        return ErrorInvalidOperation("BindAttribLocation: invalid program");

    if (name.IsEmpty())
        return ErrorInvalidValue("BindAttribLocation: name can't be null or empty");

    MakeContextCurrent();

    gl->fBindAttribLocation(progname, location, NS_LossyConvertUTF16toASCII(name).get());

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindBuffer(WebGLenum target, nsIWebGLBuffer *bobj)
{
    WebGLuint bufname;
    WebGLBuffer* buf;
    PRBool isNull;
    if (!GetConcreteObjectAndGLName(bobj, &buf, &bufname, &isNull))
        return ErrorInvalidOperation("BindBuffer: invalid buffer");

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        mBoundArrayBuffer = buf;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        mBoundElementArrayBuffer = buf;
    } else {
        return ErrorInvalidEnum("BindBuffer: invalid target");
    }

    if (!isNull) {
        if ((buf->Target() != LOCAL_GL_NONE) && (target != buf->Target()))
            return ErrorInvalidOperation("BindBuffer: buffer already bound to a different target");
        buf->SetTarget(target);
    }

    MakeContextCurrent();

    gl->fBindBuffer(target, bufname);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindFramebuffer(WebGLenum target, nsIWebGLFramebuffer *fbobj)
{
    WebGLuint framebuffername;
    PRBool isNull;
    WebGLFramebuffer *wfb;

    if (target != LOCAL_GL_FRAMEBUFFER)
        return ErrorInvalidOperation("BindFramebuffer: target must be GL_FRAMEBUFFER");

    if (!GetConcreteObjectAndGLName(fbobj, &wfb, &framebuffername, &isNull))
        return ErrorInvalidOperation("BindFramebuffer: invalid framebuffer");

    MakeContextCurrent();

    gl->fBindFramebuffer(target, framebuffername);

    mBoundFramebuffer = wfb;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindRenderbuffer(WebGLenum target, nsIWebGLRenderbuffer *rbobj)
{
    WebGLuint renderbuffername;
    PRBool isNull;
    WebGLRenderbuffer *wrb;

    if (target != LOCAL_GL_RENDERBUFFER)
        return ErrorInvalidEnum("BindRenderbuffer: target must be GL_RENDERBUFFER");

    if (!GetConcreteObjectAndGLName(rbobj, &wrb, &renderbuffername, &isNull))
        return ErrorInvalidOperation("BindRenderbuffer: invalid renderbuffer");

    MakeContextCurrent();

    gl->fBindRenderbuffer(target, renderbuffername);

    mBoundRenderbuffer = wrb;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindTexture(WebGLenum target, nsIWebGLTexture *tobj)
{
    WebGLuint texturename;
    WebGLTexture *tex;
    PRBool isNull;
    if (!GetConcreteObjectAndGLName(tobj, &tex, &texturename, &isNull))
        return ErrorInvalidOperation("BindTexture: invalid texture");

    if (target == LOCAL_GL_TEXTURE_2D) {
        mBound2DTextures[mActiveTexture] = tex;
    } else if (target == LOCAL_GL_TEXTURE_CUBE_MAP) {
        mBoundCubeMapTextures[mActiveTexture] = tex;
    } else {
        return ErrorInvalidEnum("BindTexture: invalid target");
    }

    MakeContextCurrent();

    gl->fBindTexture(target, texturename);

    return NS_OK;
}

GL_SAME_METHOD_4(BlendColor, BlendColor, WebGLfloat, WebGLfloat, WebGLfloat, WebGLfloat)

NS_IMETHODIMP WebGLContext::BlendEquation(WebGLenum mode)
{
    if (!ValidateBlendEquationEnum(mode, "blendEquation: mode"))
        return NS_OK;

    MakeContextCurrent();
    gl->fBlendEquation(mode);
    return NS_OK;
}

NS_IMETHODIMP WebGLContext::BlendEquationSeparate(WebGLenum modeRGB, WebGLenum modeAlpha)
{
    if (!ValidateBlendEquationEnum(modeRGB, "blendEquationSeparate: modeRGB") ||
        !ValidateBlendEquationEnum(modeAlpha, "blendEquationSeparate: modeAlpha"))
        return NS_OK;

    MakeContextCurrent();
    gl->fBlendEquationSeparate(modeRGB, modeAlpha);
    return NS_OK;
}

NS_IMETHODIMP WebGLContext::BlendFunc(WebGLenum sfactor, WebGLenum dfactor)
{
    if (!ValidateBlendFuncSrcEnum(sfactor, "blendFunc: sfactor") ||
        !ValidateBlendFuncDstEnum(dfactor, "blendFunc: dfactor"))
        return NS_OK;

    MakeContextCurrent();
    gl->fBlendFunc(sfactor, dfactor);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BlendFuncSeparate(WebGLenum srcRGB, WebGLenum dstRGB,
                                WebGLenum srcAlpha, WebGLenum dstAlpha)
{
    if (!ValidateBlendFuncSrcEnum(srcRGB, "blendFuncSeparate: srcRGB") ||
        !ValidateBlendFuncSrcEnum(srcAlpha, "blendFuncSeparate: srcAlpha") ||
        !ValidateBlendFuncDstEnum(dstRGB, "blendFuncSeparate: dstRGB") ||
        !ValidateBlendFuncDstEnum(dstAlpha, "blendFuncSeparate: dstAlpha"))
        return NS_OK;

    MakeContextCurrent();
    gl->fBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BufferData(PRInt32 dummy)
{
    // this should never be called
    LogMessage("BufferData");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
WebGLContext::BufferData_size(WebGLenum target, WebGLsizei size, WebGLenum usage)
{
    WebGLBuffer *boundBuffer = NULL;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnum("BufferData: invalid target");
    }

    if (size < 0)
        return ErrorInvalidValue("bufferData: negative size");

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return NS_OK;

    if (!boundBuffer)
        return ErrorInvalidOperation("BufferData: no buffer bound!");

    MakeContextCurrent();

    boundBuffer->SetByteLength(size);
    boundBuffer->ZeroDataIfElementArray();

    gl->fBufferData(target, size, 0, usage);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BufferData_buf(WebGLenum target, js::ArrayBuffer *wb, WebGLenum usage)
{
    WebGLBuffer *boundBuffer = NULL;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnum("BufferData: invalid target");
    }

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return NS_OK;

    if (!boundBuffer)
        return ErrorInvalidOperation("BufferData: no buffer bound!");

    MakeContextCurrent();

    boundBuffer->SetByteLength(wb->byteLength);
    boundBuffer->CopyDataIfElementArray(wb->data);

    gl->fBufferData(target, wb->byteLength, wb->data, usage);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BufferData_array(WebGLenum target, js::TypedArray *wa, WebGLenum usage)
{
    WebGLBuffer *boundBuffer = NULL;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnum("BufferData: invalid target");
    }

    if (!ValidateBufferUsageEnum(usage, "bufferData: usage"))
        return NS_OK;

    if (!boundBuffer)
        return ErrorInvalidOperation("BufferData: no buffer bound!");

    MakeContextCurrent();

    boundBuffer->SetByteLength(wa->byteLength);
    boundBuffer->CopyDataIfElementArray(wa->data);

    gl->fBufferData(target, wa->byteLength, wa->data, usage);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BufferSubData(PRInt32 dummy)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
WebGLContext::BufferSubData_buf(GLenum target, WebGLsizei byteOffset, js::ArrayBuffer *wb)
{
    WebGLBuffer *boundBuffer = NULL;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnum("BufferSubData: invalid target");
    }

    if (!boundBuffer)
        return ErrorInvalidOperation("BufferData: no buffer bound!");

    CheckedUint32 checked_neededByteLength = CheckedUint32(byteOffset) + wb->byteLength;
    if (!checked_neededByteLength.valid())
        return ErrorInvalidOperation("bufferSubData: integer overflow computing the needed byte length");

    if (checked_neededByteLength.value() > boundBuffer->ByteLength())
        return ErrorInvalidOperation("BufferSubData: not enough data - operation requires %d bytes, but buffer only has %d bytes",
                                     byteOffset, wb->byteLength, boundBuffer->ByteLength());

    MakeContextCurrent();

    boundBuffer->CopySubDataIfElementArray(byteOffset, wb->byteLength, wb->data);

    gl->fBufferSubData(target, byteOffset, wb->byteLength, wb->data);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BufferSubData_array(WebGLenum target, WebGLsizei byteOffset, js::TypedArray *wa)
{
    WebGLBuffer *boundBuffer = NULL;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundElementArrayBuffer;
    } else {
        return ErrorInvalidEnum("BufferSubData: invalid target");
    }

    if (!boundBuffer)
        return ErrorInvalidOperation("BufferData: no buffer bound!");

    CheckedUint32 checked_neededByteLength = CheckedUint32(byteOffset) + wa->byteLength;
    if (!checked_neededByteLength.valid())
        return ErrorInvalidOperation("bufferSubData: integer overflow computing the needed byte length");

    if (checked_neededByteLength.value() > boundBuffer->ByteLength())
        return ErrorInvalidOperation("BufferSubData: not enough data -- operation requires %d bytes, but buffer only has %d bytes",
                                     byteOffset, wa->byteLength, boundBuffer->ByteLength());

    MakeContextCurrent();

    boundBuffer->CopySubDataIfElementArray(byteOffset, wa->byteLength, wa->data);

    gl->fBufferSubData(target, byteOffset, wa->byteLength, wa->data);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CheckFramebufferStatus(WebGLenum target, WebGLenum *retval)
{
    MakeContextCurrent();
    // XXX check target
    *retval = gl->fCheckFramebufferStatus(target);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Clear(PRUint32 mask)
{
    MakeContextCurrent();
    gl->fClear(mask);
    Invalidate();

    return NS_OK;
}

GL_SAME_METHOD_4(ClearColor, ClearColor, WebGLfloat, WebGLfloat, WebGLfloat, WebGLfloat)

#ifdef USE_GLES2
GL_SAME_METHOD_1(ClearDepthf, ClearDepth, WebGLfloat)
#else
GL_SAME_METHOD_1(ClearDepth, ClearDepth, WebGLfloat)
#endif

GL_SAME_METHOD_1(ClearStencil, ClearStencil, WebGLint)

GL_SAME_METHOD_4(ColorMask, ColorMask, WebGLboolean, WebGLboolean, WebGLboolean, WebGLboolean)

NS_IMETHODIMP
WebGLContext::CopyTexImage2D(WebGLenum target,
                             WebGLint level,
                             WebGLenum internalformat,
                             WebGLint x,
                             WebGLint y,
                             WebGLsizei width,
                             WebGLsizei height,
                             WebGLint border)
{
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
            return ErrorInvalidEnum("CopyTexImage2D: unsupported target");
    }

    switch (internalformat) {
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_LUMINANCE_ALPHA:
            break;
        default:
            return ErrorInvalidEnum("CopyTexImage2D: internal format not supported");
    }

    if (border != 0) {
        return ErrorInvalidValue("CopyTexImage2D: border != 0");
    }

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width, height, mWidth, mHeight))
        return ErrorInvalidOperation("CopyTexImage2D: copied rectangle out of bounds");

    MakeContextCurrent();

    gl->fCopyTexImage2D(target, level, internalformat, x, y, width, height, border);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CopyTexSubImage2D(WebGLenum target,
                                WebGLint level,
                                WebGLint xoffset,
                                WebGLint yoffset,
                                WebGLint x,
                                WebGLint y,
                                WebGLsizei width,
                                WebGLsizei height)
{
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
            return ErrorInvalidEnum("CopyTexSubImage2D: unsupported target");
    }

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width, height, mWidth, mHeight))
        return ErrorInvalidOperation("CopyTexSubImage2D: copied rectangle out of bounds");

    MakeContextCurrent();

    gl->fCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

    return NS_OK;
}


NS_IMETHODIMP
WebGLContext::CreateProgram(nsIWebGLProgram **retval)
{
    MakeContextCurrent();

    WebGLuint name = gl->fCreateProgram();

    WebGLProgram *prog = new WebGLProgram(this, name);
    NS_ADDREF(*retval = prog);
    mMapPrograms.Put(name, prog);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CreateShader(WebGLenum type, nsIWebGLShader **retval)
{
    if (type != LOCAL_GL_VERTEX_SHADER &&
        type != LOCAL_GL_FRAGMENT_SHADER)
    {
        return ErrorInvalidEnum("Invalid shader type specified");
    }

    MakeContextCurrent();

    WebGLuint name = gl->fCreateShader(type);

    WebGLShader *shader = new WebGLShader(this, name, type);
    NS_ADDREF(*retval = shader);
    mMapShaders.Put(name, shader);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CullFace(WebGLenum face)
{
    if (!ValidateFaceEnum(face, "cullFace"))
        return NS_OK;

    MakeContextCurrent();
    gl->fCullFace(face);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteBuffer(nsIWebGLBuffer *bobj)
{
    WebGLuint bufname;
    WebGLBuffer *buf;
    PRBool isNull, isDeleted;
    if (!GetConcreteObjectAndGLName(bobj, &buf, &bufname, &isNull, &isDeleted))
        return ErrorInvalidOperation("DeleteBuffer: invalid buffer");

    if (isNull || isDeleted)
        return NS_OK;

    MakeContextCurrent();

    gl->fDeleteBuffers(1, &bufname);
    buf->Delete();
    mMapBuffers.Remove(bufname);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteFramebuffer(nsIWebGLFramebuffer *fbobj)
{
    WebGLuint fbufname;
    WebGLFramebuffer *fbuf;
    PRBool isNull, isDeleted;
    if (!GetConcreteObjectAndGLName(fbobj, &fbuf, &fbufname, &isNull, &isDeleted))
        return ErrorInvalidOperation("DeleteFramebuffer: invalid framebuffer");

    if (isNull || isDeleted)
        return NS_OK;

    MakeContextCurrent();

    gl->fDeleteFramebuffers(1, &fbufname);
    fbuf->Delete();
    mMapFramebuffers.Remove(fbufname);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteRenderbuffer(nsIWebGLRenderbuffer *rbobj)
{
    WebGLuint rbufname;
    WebGLRenderbuffer *rbuf;
    PRBool isNull, isDeleted;
    if (!GetConcreteObjectAndGLName(rbobj, &rbuf, &rbufname, &isNull, &isDeleted))
        return ErrorInvalidOperation("DeleteRenderbuffer: invalid renderbuffer");

    if (isNull || isDeleted)
        return NS_OK;

    MakeContextCurrent();

    // XXX we need to track renderbuffer attachments; from glDeleteRenderbuffers man page:

    /*
            If a renderbuffer object that is currently bound is deleted, the binding reverts
            to 0 (the absence of any renderbuffer object). Additionally, special care
            must be taken when deleting a renderbuffer object if the image of the renderbuffer
            is attached to a framebuffer object. In this case, if the deleted renderbuffer object is
            attached to the currently bound framebuffer object, it is 
            automatically detached.  However, attachments to any other framebuffer objects are the
            responsibility of the application.
    */

    gl->fDeleteRenderbuffers(1, &rbufname);
    rbuf->Delete();
    mMapRenderbuffers.Remove(rbufname);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteTexture(nsIWebGLTexture *tobj)
{
    WebGLuint texname;
    WebGLTexture *tex;
    PRBool isNull, isDeleted;
    if (!GetConcreteObjectAndGLName(tobj, &tex, &texname, &isNull, &isDeleted))
        return ErrorInvalidOperation("DeleteTexture: invalid texture");

    if (isNull || isDeleted)
        return NS_OK;

    MakeContextCurrent();

    gl->fDeleteTextures(1, &texname);
    tex->Delete();
    mMapTextures.Remove(texname);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteProgram(nsIWebGLProgram *pobj)
{
    WebGLuint progname;
    WebGLProgram *prog;
    PRBool isNull, isDeleted;
    if (!GetConcreteObjectAndGLName(pobj, &prog, &progname, &isNull, &isDeleted))
        return ErrorInvalidOperation("DeleteProgram: invalid program");

    if (isNull || isDeleted)
        return NS_OK;

    MakeContextCurrent();

    gl->fDeleteProgram(progname);
    prog->Delete();
    mMapPrograms.Remove(progname);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteShader(nsIWebGLShader *sobj)
{
    WebGLuint shadername;
    WebGLShader *shader;
    PRBool isNull, isDeleted;
    if (!GetConcreteObjectAndGLName(sobj, &shader, &shadername, &isNull, &isDeleted))
        return ErrorInvalidOperation("DeleteShader: invalid shader");

    if (isNull || isDeleted)
        return NS_OK;

    MakeContextCurrent();

    gl->fDeleteShader(shadername);
    shader->Delete();
    mMapShaders.Remove(shadername);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DetachShader(nsIWebGLProgram *pobj, nsIWebGLShader *shobj)
{
    WebGLuint progname, shadername;
    WebGLProgram *program;
    WebGLShader *shader;
    if (!GetConcreteObjectAndGLName(pobj, &program, &progname))
        return ErrorInvalidOperation("DetachShader: invalid program");

    if (!GetConcreteObjectAndGLName(shobj, &shader, &shadername))
        return ErrorInvalidOperation("DetachShader: invalid shader");

    if (!program->DetachShader(shader))
        return ErrorInvalidOperation("DetachShader: shader is not attached");

    MakeContextCurrent();

    gl->fDetachShader(progname, shadername);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DepthFunc(WebGLenum func)
{
    if (!ValidateComparisonEnum(func, "depthFunc"))
        return NS_OK;

    MakeContextCurrent();
    gl->fDepthFunc(func);
    return NS_OK;
}

GL_SAME_METHOD_1(DepthMask, DepthMask, WebGLboolean)

#ifdef USE_GLES2
GL_SAME_METHOD_2(DepthRangef, DepthRange, WebGLfloat, WebGLfloat)
#else
GL_SAME_METHOD_2(DepthRange, DepthRange, WebGLfloat, WebGLfloat)
#endif

NS_IMETHODIMP
WebGLContext::DisableVertexAttribArray(WebGLuint index)
{
    if (index > mAttribBuffers.Length())
        return ErrorInvalidValue("DisableVertexAttribArray: index out of range");

    MakeContextCurrent();

    gl->fDisableVertexAttribArray(index);
    mAttribBuffers[index].enabled = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DrawArrays(GLenum mode, WebGLint first, WebGLsizei count)
{
    switch (mode) {
        case LOCAL_GL_TRIANGLES:
        case LOCAL_GL_TRIANGLE_STRIP:
        case LOCAL_GL_TRIANGLE_FAN:
        case LOCAL_GL_POINTS:
        case LOCAL_GL_LINE_STRIP:
        case LOCAL_GL_LINE_LOOP:
        case LOCAL_GL_LINES:
            break;
        default:
            return ErrorInvalidEnum("DrawArrays: invalid mode");
    }

    if (first < 0 || count < 0)
        return ErrorInvalidValue("DrawArrays: negative first or count");

    // If count is 0, there's nothing to do.
    if (count == 0)
        return NS_OK;

    // If there is no current program, this is silently ignored.
    // Any checks below this depend on a program being available.
    if (!mCurrentProgram)
        return NS_OK;

    CheckedInt32 checked_firstPlusCount = CheckedInt32(first) + count;

    if (!checked_firstPlusCount.valid())
        return ErrorInvalidOperation("drawArrays: overflow in first+count");

    if (!ValidateBuffers(checked_firstPlusCount.value()))
        return ErrorInvalidOperation("DrawArrays: bound vertex attribute buffers do not have sufficient data for given first and count");

    MakeContextCurrent();

    gl->fDrawArrays(mode, first, count);

    Invalidate();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DrawElements(WebGLenum mode, WebGLsizei count, WebGLenum type, WebGLint byteOffset)
{
    switch (mode) {
        case LOCAL_GL_TRIANGLES:
        case LOCAL_GL_TRIANGLE_STRIP:
        case LOCAL_GL_TRIANGLE_FAN:
        case LOCAL_GL_POINTS:
        case LOCAL_GL_LINE_STRIP:
        case LOCAL_GL_LINE_LOOP:
        case LOCAL_GL_LINES:
            break;
        default:
            return ErrorInvalidEnum("DrawElements: invalid mode");
    }

    if (count < 0 || byteOffset < 0)
        return ErrorInvalidValue("DrawElements: negative count or offset");

    CheckedUint32 checked_byteCount;

    if (type == LOCAL_GL_UNSIGNED_SHORT) {
        checked_byteCount = 2 * CheckedUint32(count);
        if (byteOffset % 2 != 0)
            return ErrorInvalidValue("DrawElements: invalid byteOffset for UNSIGNED_SHORT (must be a multiple of 2)");
    } else if (type == LOCAL_GL_UNSIGNED_BYTE) {
        checked_byteCount = count;
    } else {
        return ErrorInvalidEnum("DrawElements: type must be UNSIGNED_SHORT or UNSIGNED_BYTE");
    }

    if (!checked_byteCount.valid())
        return ErrorInvalidValue("DrawElements: overflow in byteCount");

    // If count is 0, there's nothing to do.
    if (count == 0)
        return NS_OK;

    // If there is no current program, this is silently ignored.
    // Any checks below this depend on a program being available.
    if (!mCurrentProgram)
        return NS_OK;

    if (!mBoundElementArrayBuffer)
        return ErrorInvalidOperation("DrawElements: must have element array buffer binding");

    CheckedUint32 checked_neededByteCount = checked_byteCount + byteOffset;

    if (!checked_neededByteCount.valid())
        return ErrorInvalidOperation("DrawElements: overflow in byteOffset+byteCount");

    if (checked_neededByteCount.value() > mBoundElementArrayBuffer->ByteLength())
        return ErrorInvalidOperation("DrawElements: bound element array buffer is too small for given count and offset");

    WebGLuint maxIndex = 0;
    if (type == LOCAL_GL_UNSIGNED_SHORT) {
        maxIndex = mBoundElementArrayBuffer->FindMaximum<GLushort>(count, byteOffset);
    } else if (type == LOCAL_GL_UNSIGNED_BYTE) {
        maxIndex = mBoundElementArrayBuffer->FindMaximum<GLubyte>(count, byteOffset);
    }

    // maxIndex+1 because ValidateBuffers expects the number of elements needed.
    // it is very important here to check tha maxIndex+1 doesn't overflow, otherwise the buffer validation is bypassed !!!
    // maxIndex is a WebGLuint, ValidateBuffers takes a PRUint32, we validate maxIndex+1 as a PRUint32.
    CheckedUint32 checked_neededCount = CheckedUint32(maxIndex) + 1;
    if (!checked_neededCount.valid())
        return ErrorInvalidOperation("drawElements: overflow in maxIndex+1");
    if (!ValidateBuffers(checked_neededCount.value())) {
        return ErrorInvalidOperation("DrawElements: bound vertex attribute buffers do not have sufficient "
                                     "data for given indices from the bound element array");
    }

    MakeContextCurrent();

    gl->fDrawElements(mode, count, type, (GLvoid*) (byteOffset));

    Invalidate();

    return NS_OK;
}

NS_IMETHODIMP WebGLContext::Enable(WebGLenum cap)
{
    if (!ValidateCapabilityEnum(cap, "enable"))
        return NS_OK;

    MakeContextCurrent();
    gl->fEnable(cap);
    return NS_OK;
}

NS_IMETHODIMP WebGLContext::Disable(WebGLenum cap)
{
    if (!ValidateCapabilityEnum(cap, "disable"))
        return NS_OK;

    MakeContextCurrent();
    gl->fDisable(cap);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::EnableVertexAttribArray(WebGLuint index)
{
    if (index > mAttribBuffers.Length())
        return ErrorInvalidValue("EnableVertexAttribArray: index out of range");

    MakeContextCurrent();

    gl->fEnableVertexAttribArray(index);
    mAttribBuffers[index].enabled = PR_TRUE;

    return NS_OK;
}

// XXX need to track this -- see glDeleteRenderbuffer above and man page for DeleteRenderbuffers
NS_IMETHODIMP
WebGLContext::FramebufferRenderbuffer(WebGLenum target, WebGLenum attachment, WebGLenum rbtarget, nsIWebGLRenderbuffer *rbobj)
{
    WebGLuint renderbuffername;
    PRBool isNull;
    WebGLRenderbuffer *wrb;

    if (!GetConcreteObjectAndGLName(rbobj, &wrb, &renderbuffername, &isNull))
        return ErrorInvalidOperation("FramebufferRenderbuffer: invalid renderbuffer");

    if (target != LOCAL_GL_FRAMEBUFFER)
        return ErrorInvalidEnum("FramebufferRenderbuffer: target must be GL_FRAMEBUFFER");

    if ((attachment < LOCAL_GL_COLOR_ATTACHMENT0 ||
         attachment >= LOCAL_GL_COLOR_ATTACHMENT0 + mFramebufferColorAttachments.Length()) &&
        attachment != LOCAL_GL_DEPTH_ATTACHMENT &&
        attachment != LOCAL_GL_STENCIL_ATTACHMENT)
    {
        return ErrorInvalidEnum("FramebufferRenderbuffer: invalid attachment");
    }

    if (rbtarget != LOCAL_GL_RENDERBUFFER)
        return ErrorInvalidEnum("FramebufferRenderbuffer: renderbuffer target must be GL_RENDERBUFFER");

    // dimensions are kept for readPixels primarily, function only uses COLOR_ATTACHMENT0
    if (mBoundFramebuffer && attachment == LOCAL_GL_COLOR_ATTACHMENT0)
        mBoundFramebuffer->setDimensions(wrb);

    MakeContextCurrent();

    gl->fFramebufferRenderbuffer(target, attachment, rbtarget, renderbuffername);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::FramebufferTexture2D(WebGLenum target,
                                   WebGLenum attachment,
                                   WebGLenum textarget,
                                   nsIWebGLTexture *tobj,
                                   WebGLint level)
{
    WebGLuint texturename;
    PRBool isNull;
    WebGLTexture *wtex;

    if (!GetConcreteObjectAndGLName(tobj, &wtex, &texturename, &isNull))
        return ErrorInvalidOperation("FramebufferTexture2D: invalid texture");

    if (target != LOCAL_GL_FRAMEBUFFER)
        return ErrorInvalidEnum("FramebufferTexture2D: target must be GL_FRAMEBUFFER");

    if ((attachment < LOCAL_GL_COLOR_ATTACHMENT0 ||
         attachment >= LOCAL_GL_COLOR_ATTACHMENT0 + mFramebufferColorAttachments.Length()) &&
        attachment != LOCAL_GL_DEPTH_ATTACHMENT &&
        attachment != LOCAL_GL_STENCIL_ATTACHMENT)
        return ErrorInvalidEnum("FramebufferTexture2D: invalid attachment");

    if (textarget != LOCAL_GL_TEXTURE_2D &&
        (textarget < LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
         textarget > LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
        return ErrorInvalidEnum("FramebufferTexture2D: invalid textarget (only 2D or cube face)");

    if (level != 0)
        return ErrorInvalidValue("FramebufferTexture2D: level must be 0");

    // dimensions are kept for readPixels primarily, function only uses COLOR_ATTACHMENT0
    if (mBoundFramebuffer && attachment == LOCAL_GL_COLOR_ATTACHMENT0)
        mBoundFramebuffer->setDimensions(wtex);

    // XXXXX we need to store/reference this attachment!

    MakeContextCurrent();

    gl->fFramebufferTexture2D(target, attachment, textarget, texturename, level);

    return NS_OK;
}

GL_SAME_METHOD_0(Flush, Flush)

GL_SAME_METHOD_0(Finish, Finish)

NS_IMETHODIMP
WebGLContext::FrontFace(WebGLenum mode)
{
    switch (mode) {
        case LOCAL_GL_CW:
        case LOCAL_GL_CCW:
            break;
        default:
            return ErrorInvalidEnum("FrontFace: invalid mode");
    }

    MakeContextCurrent();
    gl->fFrontFace(mode);
    return NS_OK;
}

// returns an object: { size: ..., type: ..., name: ... }
NS_IMETHODIMP
WebGLContext::GetActiveAttrib(nsIWebGLProgram *pobj, PRUint32 index, nsIWebGLActiveInfo **retval)
{
    WebGLuint progname;
    if (!GetGLName<WebGLProgram>(pobj, &progname))
        return ErrorInvalidOperation("GetActiveAttrib: invalid program");

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint len = 0;
    gl->fGetProgramiv(progname, LOCAL_GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &len);
    if (len == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    nsAutoArrayPtr<char> name(new char[len]);
    PRInt32 attrsize = 0;
    PRUint32 attrtype = 0;

    gl->fGetActiveAttrib(progname, index, len, &len, (GLint*) &attrsize, (WebGLuint*) &attrtype, name);
    if (attrsize == 0 || attrtype == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("size", attrsize);
    retobj.DefineProperty("type", attrtype);
    retobj.DefineProperty("name", name, len);

    js.SetRetVal(retobj);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GenerateMipmap(WebGLenum target)
{
    if (!ValidateTextureTargetEnum(target, "generateMipmap"))
        return NS_OK;

    MakeContextCurrent();
    gl->fGenerateMipmap(target);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetActiveUniform(nsIWebGLProgram *pobj, PRUint32 index, nsIWebGLActiveInfo **retval)
{
    WebGLuint progname;
    if (!GetGLName<WebGLProgram>(pobj, &progname))
        return ErrorInvalidOperation("GetActiveUniform: invalid program");

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint len = 0;
    gl->fGetProgramiv(progname, LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH, &len);
    if (len == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    nsAutoArrayPtr<char> name(new char[len]);
    PRInt32 attrsize = 0;
    PRUint32 attrtype = 0;

    gl->fGetActiveUniform(progname, index, len, &len, (GLint*) &attrsize, (WebGLenum*) &attrtype, name);
    if (len == 0 || attrsize == 0 || attrtype == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("size", attrsize);
    retobj.DefineProperty("type", attrtype);
    retobj.DefineProperty("name", name, len);

    js.SetRetVal(retobj.Object());

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetAttachedShaders(nsIWebGLProgram *pobj, nsIVariant **retval)
{
    WebGLProgram *prog;
    if (!GetConcreteObject(pobj, &prog))
        return ErrorInvalidOperation("GetAttachedShaders: invalid program");

    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    MakeContextCurrent();

    if (prog->AttachedShaders().Length() == 0) {
        wrval->SetAsEmptyArray();
    }
    else {
        wrval->SetAsArray(nsIDataType::VTYPE_INTERFACE,
                        &NS_GET_IID(nsIWebGLShader),
                        prog->AttachedShaders().Length(),
                        const_cast<void*>( // @#$% SetAsArray doesn't accept a const void*
                            static_cast<const void*>(
                                prog->AttachedShaders().Elements()
                            )
                        )
                        );
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetAttribLocation(nsIWebGLProgram *pobj,
                                const nsAString& name,
                                PRInt32 *retval)
{
    WebGLuint progname;
    if (!GetGLName<WebGLProgram>(pobj, &progname))
        return ErrorInvalidOperation("GetAttribLocation: invalid program");

    MakeContextCurrent();
    *retval = gl->fGetAttribLocation(progname, NS_LossyConvertUTF16toASCII(name).get());
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetParameter(PRUint32 pname, nsIVariant **retval)
{
    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    MakeContextCurrent();

    switch (pname) {
        //
        // String params
        //

        case LOCAL_GL_VENDOR:
            wrval->SetAsDOMString(NS_LITERAL_STRING("Mozilla"));
            break;
        case LOCAL_GL_RENDERER:
            wrval->SetAsDOMString(NS_LITERAL_STRING("Mozilla"));
            break;
        case LOCAL_GL_VERSION:
            wrval->SetAsDOMString(NS_LITERAL_STRING("WebGL 1.0"));
            break;
        case LOCAL_GL_SHADING_LANGUAGE_VERSION:
            wrval->SetAsDOMString(NS_LITERAL_STRING("WebGL GLSL ES 1.0"));
            break;

        //
        // Single-value params
        //

// int
        case LOCAL_GL_CULL_FACE_MODE:
        case LOCAL_GL_FRONT_FACE:
        case LOCAL_GL_ACTIVE_TEXTURE:
        case LOCAL_GL_DEPTH_CLEAR_VALUE:
        case LOCAL_GL_STENCIL_CLEAR_VALUE:
        case LOCAL_GL_STENCIL_FUNC:
        case LOCAL_GL_STENCIL_REF:
        case LOCAL_GL_STENCIL_FAIL:
        case LOCAL_GL_STENCIL_PASS_DEPTH_FAIL:
        case LOCAL_GL_STENCIL_PASS_DEPTH_PASS:
        case LOCAL_GL_STENCIL_BACK_FUNC:
        case LOCAL_GL_STENCIL_BACK_REF:
        case LOCAL_GL_STENCIL_BACK_FAIL:
        case LOCAL_GL_STENCIL_BACK_PASS_DEPTH_FAIL:
        case LOCAL_GL_STENCIL_BACK_PASS_DEPTH_PASS:
        case LOCAL_GL_DEPTH_FUNC:
        case LOCAL_GL_BLEND_SRC_RGB:
        case LOCAL_GL_BLEND_SRC_ALPHA:
        case LOCAL_GL_BLEND_DST_RGB:
        case LOCAL_GL_BLEND_DST_ALPHA:
        case LOCAL_GL_BLEND_EQUATION_RGB:
        case LOCAL_GL_BLEND_EQUATION_ALPHA:
        case LOCAL_GL_UNPACK_ALIGNMENT:
        case LOCAL_GL_PACK_ALIGNMENT:
        case LOCAL_GL_GENERATE_MIPMAP_HINT:
        case LOCAL_GL_SUBPIXEL_BITS:
        case LOCAL_GL_MAX_TEXTURE_SIZE:
        case LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        case LOCAL_GL_SAMPLE_BUFFERS:
        case LOCAL_GL_SAMPLES:
        case LOCAL_GL_MAX_VERTEX_ATTRIBS:
        case LOCAL_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        case LOCAL_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        case LOCAL_GL_MAX_TEXTURE_IMAGE_UNITS:
        case LOCAL_GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
        case LOCAL_GL_MAX_RENDERBUFFER_SIZE:
        case LOCAL_GL_RED_BITS:
        case LOCAL_GL_GREEN_BITS:
        case LOCAL_GL_BLUE_BITS:
        case LOCAL_GL_ALPHA_BITS:
        case LOCAL_GL_DEPTH_BITS:
        case LOCAL_GL_STENCIL_BITS:
        case LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE:
        case LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        {
            GLint i = 0;
            gl->fGetIntegerv(pname, &i);
            wrval->SetAsInt32(i);
        }
            break;

        #define LOCAL_GL_MAX_VARYING_VECTORS 0x8dfc // not present in desktop OpenGL

        case LOCAL_GL_MAX_VARYING_VECTORS:
        {
            #ifdef USE_GLES2
                GLint i = 0;
                gl->fGetIntegerv(pname, &i);
                wrval->SetAsInt32(i);
            #else
                // since this pname is absent from desktop OpenGL, we have to implement it by hand.
                // The formula below comes from the public_webgl list, "problematic GetParameter pnames" thread
                GLint i = 0, j = 0;
                gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_OUTPUT_COMPONENTS, &i);
                gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_INPUT_COMPONENTS, &j);
                wrval->SetAsInt32(PR_MIN(i,j)/4);
            #endif
        }
            break;

        case LOCAL_GL_NUM_COMPRESSED_TEXTURE_FORMATS:
            wrval->SetAsInt32(0);
            break;
        case LOCAL_GL_COMPRESSED_TEXTURE_FORMATS:
            wrval->SetAsVoid(); // the spec says we must return null
            break;

// unsigned int. here we may have to return very large values like 2^32-1 that can't be represented as
// javascript integer values. We just return them as doubles and javascript doesn't care.
        case LOCAL_GL_STENCIL_BACK_VALUE_MASK:
        case LOCAL_GL_STENCIL_BACK_WRITEMASK:
        case LOCAL_GL_STENCIL_VALUE_MASK:
        case LOCAL_GL_STENCIL_WRITEMASK:
        {
            GLint i = 0; // the GL api (glGetIntegerv) only does signed ints
            gl->fGetIntegerv(pname, &i);
            GLuint i_unsigned(i); // this is where -1 becomes 2^32-1
            double i_double(i_unsigned); // pass as FP value to allow large values such as 2^32-1.
            wrval->SetAsDouble(i_double);
        }
            break;

// float
        case LOCAL_GL_LINE_WIDTH:
        case LOCAL_GL_POLYGON_OFFSET_FACTOR:
        case LOCAL_GL_POLYGON_OFFSET_UNITS:
        case LOCAL_GL_SAMPLE_COVERAGE_VALUE:
        {
            GLfloat f = 0.f;
            gl->fGetFloatv(pname, &f);
            wrval->SetAsFloat(f);
        }
            break;

// bool
        case LOCAL_GL_BLEND:
        case LOCAL_GL_DEPTH_TEST:
        case LOCAL_GL_STENCIL_TEST:
        case LOCAL_GL_CULL_FACE:
        case LOCAL_GL_DITHER:
        case LOCAL_GL_POLYGON_OFFSET_FILL:
        case LOCAL_GL_SCISSOR_TEST:
        case LOCAL_GL_SAMPLE_COVERAGE_INVERT:
        case LOCAL_GL_DEPTH_WRITEMASK:
        {
            realGLboolean b = 0;
            gl->fGetBooleanv(pname, &b);
            wrval->SetAsBool(PRBool(b));
        }
            break;

// bool, WebGL-specific
        case UNPACK_FLIP_Y_WEBGL:
            wrval->SetAsBool(mPixelStoreFlipY);
            break;
        case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
            wrval->SetAsBool(mPixelStorePremultiplyAlpha);
            break;

        //
        // Complex values
        //
        case LOCAL_GL_DEPTH_RANGE: // 2 floats
        case LOCAL_GL_ALIASED_POINT_SIZE_RANGE: // 2 floats
        case LOCAL_GL_ALIASED_LINE_WIDTH_RANGE: // 2 floats
        {
            GLfloat fv[2] = { 0 };
            gl->fGetFloatv(pname, fv);
            wrval->SetAsArray(nsIDataType::VTYPE_FLOAT, nsnull,
                              2, static_cast<void*>(fv));
        }
            break;
        
        case LOCAL_GL_COLOR_CLEAR_VALUE: // 4 floats
        case LOCAL_GL_BLEND_COLOR: // 4 floats
        {
            GLfloat fv[4] = { 0 };
            gl->fGetFloatv(pname, fv);
            wrval->SetAsArray(nsIDataType::VTYPE_FLOAT, nsnull,
                              4, static_cast<void*>(fv));
        }
            break;

        case LOCAL_GL_MAX_VIEWPORT_DIMS: // 2 ints
        {
            GLint iv[2] = { 0 };
            gl->fGetIntegerv(pname, iv);
            wrval->SetAsArray(nsIDataType::VTYPE_INT32, nsnull,
                              2, static_cast<void*>(iv));
        }
            break;

        case LOCAL_GL_SCISSOR_BOX: // 4 ints
        case LOCAL_GL_VIEWPORT: // 4 ints
        {
            GLint iv[2] = { 0 };
            gl->fGetIntegerv(pname, iv);
            wrval->SetAsArray(nsIDataType::VTYPE_INT32, nsnull,
                              4, static_cast<void*>(iv));
        }
            break;

        case LOCAL_GL_COLOR_WRITEMASK: // 4 bools
        {
            realGLboolean gl_bv[4] = { 0 };
            gl->fGetBooleanv(pname, gl_bv);
            PRBool pr_bv[4] = { gl_bv[0], gl_bv[1], gl_bv[2], gl_bv[3] };
            wrval->SetAsArray(nsIDataType::VTYPE_BOOL, nsnull,
                              4, static_cast<void*>(pr_bv));
        }
            break;

        case LOCAL_GL_ARRAY_BUFFER_BINDING:
            wrval->SetAsISupports(mBoundArrayBuffer);
            break;

        case LOCAL_GL_ELEMENT_ARRAY_BUFFER_BINDING:
            wrval->SetAsISupports(mBoundElementArrayBuffer);
            break;

        case LOCAL_GL_RENDERBUFFER_BINDING:
            wrval->SetAsISupports(mBoundRenderbuffer);
            break;

        case LOCAL_GL_FRAMEBUFFER_BINDING:
            wrval->SetAsISupports(mBoundFramebuffer);
            break;

        case LOCAL_GL_CURRENT_PROGRAM:
            wrval->SetAsISupports(mCurrentProgram);
            break;

        case LOCAL_GL_TEXTURE_BINDING_2D:
            wrval->SetAsISupports( mBound2DTextures[mActiveTexture]);
            break;

        case LOCAL_GL_TEXTURE_BINDING_CUBE_MAP:
            wrval->SetAsISupports(mBoundCubeMapTextures[mActiveTexture]);
            break;

        default:
            return ErrorInvalidEnum("GetParameter: invalid parameter");
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetBufferParameter(WebGLenum target, WebGLenum pname, nsIVariant **retval)
{
    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    if (target != LOCAL_GL_ARRAY_BUFFER && target != LOCAL_GL_ELEMENT_ARRAY_BUFFER)
        return ErrorInvalidEnum("GetBufferParameter: invalid target");

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_BUFFER_SIZE:
        case LOCAL_GL_BUFFER_USAGE:
        case LOCAL_GL_BUFFER_ACCESS:
        case LOCAL_GL_BUFFER_MAPPED:
        {
            GLint i = 0;
            gl->fGetBufferParameteriv(target, pname, &i);
            wrval->SetAsInt32(i);
        }
            break;

        default:
            return ErrorInvalidEnum("GetBufferParameter: invalid parameter");
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetFramebufferAttachmentParameter(WebGLenum target, WebGLenum attachment, WebGLenum pname, nsIVariant **retval)
{
    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    if (target != LOCAL_GL_FRAMEBUFFER)
        return ErrorInvalidEnum("GetFramebufferAttachmentParameter: invalid target");

    switch (attachment) {
        case LOCAL_GL_COLOR_ATTACHMENT0:
        case LOCAL_GL_DEPTH_ATTACHMENT:
        case LOCAL_GL_STENCIL_ATTACHMENT:
            break;
        default:
            return ErrorInvalidEnum("GetFramebufferAttachmentParameter: invalid attachment");
    }

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        {
            GLint i = 0;
            gl->fGetFramebufferAttachmentParameteriv(target, attachment, pname, &i);
            wrval->SetAsInt32(i);
        }
            break;

        default:
            return ErrorInvalidEnum("GetFramebufferAttachmentParameter: invalid parameter");
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetRenderbufferParameter(WebGLenum target, WebGLenum pname, nsIVariant **retval)
{
    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    if (target != LOCAL_GL_RENDERBUFFER)
        return ErrorInvalidEnum("GetRenderbufferParameter: invalid target");

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_RENDERBUFFER_WIDTH:
        case LOCAL_GL_RENDERBUFFER_HEIGHT:
        case LOCAL_GL_RENDERBUFFER_INTERNAL_FORMAT:
        case LOCAL_GL_RENDERBUFFER_RED_SIZE:
        case LOCAL_GL_RENDERBUFFER_GREEN_SIZE:
        case LOCAL_GL_RENDERBUFFER_BLUE_SIZE:
        case LOCAL_GL_RENDERBUFFER_ALPHA_SIZE:
        case LOCAL_GL_RENDERBUFFER_DEPTH_SIZE:
        case LOCAL_GL_RENDERBUFFER_STENCIL_SIZE:
        {
            GLint i = 0;
            gl->fGetRenderbufferParameteriv(target, pname, &i);
            wrval->SetAsInt32(i);
        }
            break;

        default:
            return ErrorInvalidEnum("GetRenderbufferParameter: invalid parameter");
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CreateBuffer(nsIWebGLBuffer **retval)
{
    MakeContextCurrent();

    WebGLuint name;
    gl->fGenBuffers(1, &name);

    WebGLBuffer *globj = new WebGLBuffer(this, name);
    NS_ADDREF(*retval = globj);
    mMapBuffers.Put(name, globj);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CreateTexture(nsIWebGLTexture **retval)
{
    MakeContextCurrent();

    WebGLuint name;
    gl->fGenTextures(1, &name);

    WebGLTexture *globj = new WebGLTexture(this, name);
    NS_ADDREF(*retval = globj);
    mMapTextures.Put(name, globj);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetError(WebGLenum *_retval)
{
    MakeContextCurrent();

    // Always call glGetError to clear any pending
    // real GL error.
    WebGLenum err = gl->fGetError();

    // mSynthesizedGLError has the first error that occurred,
    // whether synthesized or real; if it's not NO_ERROR, use it.
    if (mSynthesizedGLError != LOCAL_GL_NO_ERROR) {
        err = mSynthesizedGLError;
        mSynthesizedGLError = LOCAL_GL_NO_ERROR;
    }

    *_retval = err;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetProgramParameter(nsIWebGLProgram *pobj, PRUint32 pname, nsIVariant **retval)
{
    WebGLuint progname;
    if (!GetGLName<WebGLProgram>(pobj, &progname))
        return ErrorInvalidOperation("GetProgramParameter: invalid program");

    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_CURRENT_PROGRAM:
        case LOCAL_GL_ATTACHED_SHADERS:
        case LOCAL_GL_INFO_LOG_LENGTH:
        case LOCAL_GL_ACTIVE_UNIFORMS:
        case LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH:
        case LOCAL_GL_ACTIVE_ATTRIBUTES:
        case LOCAL_GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
        {
            GLint i = 0;
            gl->fGetProgramiv(progname, pname, &i);
            wrval->SetAsInt32(i);
        }
            break;
        case LOCAL_GL_DELETE_STATUS:
        case LOCAL_GL_LINK_STATUS:
        case LOCAL_GL_VALIDATE_STATUS:
        {
            GLint i = 0;
            gl->fGetProgramiv(progname, pname, &i);
            wrval->SetAsBool(PRBool(i));
        }
            break;

        default:
            return ErrorInvalidEnum("GetProgramParameter: invalid parameter");
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetProgramInfoLog(nsIWebGLProgram *pobj, nsAString& retval)
{
    WebGLuint progname;
    if (!GetGLName<WebGLProgram>(pobj, &progname))
        return ErrorInvalidOperation("GetProgramInfoLog: invalid program");

    MakeContextCurrent();

    PRInt32 k = -1;
    gl->fGetProgramiv(progname, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &k);
    if (k == -1)
        return NS_ERROR_FAILURE; // XXX GL error? shouldn't happen!

    if (k == 0) {
        retval.Truncate();
        return NS_OK;
    }

    nsCAutoString log;
    log.SetCapacity(k);

    gl->fGetProgramInfoLog(progname, k, (GLint*) &k, (char*) log.BeginWriting());

    log.SetLength(k);

    CopyASCIItoUTF16(log, retval);

    return NS_OK;
}

/* XXX fix */
/* void texParameter (); */
NS_IMETHODIMP
WebGLContext::TexParameterf(WebGLenum target, WebGLenum pname, WebGLfloat param)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();

    gl->fTexParameterf (target, pname, param);

    return NS_OK;
}
NS_IMETHODIMP
WebGLContext::TexParameteri(WebGLenum target, WebGLenum pname, WebGLint param)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();

    gl->fTexParameteri (target, pname, param);

    return NS_OK;
}

#if 0
NS_IMETHODIMP
WebGLContext::TexParameter()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3)
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint targetVal;
    jsuint pnameVal;
    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &targetVal) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &pnameVal))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (targetVal != LOCAL_GL_TEXTURE_2D &&
        targetVal != LOCAL_GL_TEXTURE_CUBE_MAP)
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    MakeContextCurrent();
    switch (pnameVal) {
        case LOCAL_GL_TEXTURE_MIN_FILTER: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != LOCAL_GL_NEAREST &&
                 ival != LOCAL_GL_LINEAR &&
                 ival != LOCAL_GL_NEAREST_MIPMAP_NEAREST &&
                 ival != LOCAL_GL_LINEAR_MIPMAP_NEAREST &&
                 ival != LOCAL_GL_NEAREST_MIPMAP_LINEAR &&
                 ival != LOCAL_GL_LINEAR_MIPMAP_LINEAR))
                return NS_ERROR_DOM_SYNTAX_ERR;
            gl->fTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case LOCAL_GL_TEXTURE_MAG_FILTER: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != LOCAL_GL_NEAREST &&
                 ival != LOCAL_GL_LINEAR))
                return NS_ERROR_DOM_SYNTAX_ERR;
            gl->fTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case LOCAL_GL_TEXTURE_WRAP_S:
        case LOCAL_GL_TEXTURE_WRAP_T: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != LOCAL_GL_CLAMP &&
                 ival != LOCAL_GL_CLAMP_TO_EDGE &&
                 ival != LOCAL_GL_REPEAT))
                return NS_ERROR_DOM_SYNTAX_ERR;
            gl->fTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case LOCAL_GL_GENERATE_MIPMAP: {
            jsuint ival;
            if (js.argv[2] == JSVAL_TRUE)
                ival = 1;
            else if (js.argv[2] == JSVAL_FALSE)
                ival = 0;
            else if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                     (ival != 0 && ival != 1))
                return NS_ERROR_DOM_SYNTAX_ERR;
            gl->fTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT: {
#if 0
            if (GLEW_EXT_texture_filter_anisotropic) {
                jsdouble dval;
                if (!::JS_ValueToNumber(js.ctx, js.argv[2], &dval))
                    return NS_ERROR_DOM_SYNTAX_ERR;
                gl->fTexParameterf (targetVal, pnameVal, (float) dval);
            } else {
                return NS_ERROR_NOT_IMPLEMENTED;
            }
#else
            return NS_ERROR_NOT_IMPLEMENTED;
#endif
        }
            break;
        default:
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
}
#endif

NS_IMETHODIMP
WebGLContext::GetTexParameter(WebGLenum target, WebGLenum pname, nsIVariant **retval)
{
    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    MakeContextCurrent();

    if (!ValidateTextureTargetEnum(target, "getTexParameter: target"))
        return NS_OK;

    switch (pname) {
        case LOCAL_GL_TEXTURE_MIN_FILTER:
        case LOCAL_GL_TEXTURE_MAG_FILTER:
        case LOCAL_GL_TEXTURE_WRAP_S:
        case LOCAL_GL_TEXTURE_WRAP_T:
        {
            GLint i = 0;
            gl->fGetTexParameteriv(target, pname, &i);
            wrval->SetAsInt32(i);
        }
            break;

        default:
            return ErrorInvalidEnum("getTexParameter: invalid parameter");
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

/* any getUniform(in WebGLProgram program, in WebGLUniformLocation location) raises(DOMException); */
NS_IMETHODIMP
WebGLContext::GetUniform(nsIWebGLProgram *pobj, nsIWebGLUniformLocation *ploc, nsIVariant **retval)
{
    WebGLuint progname;
    WebGLProgram *prog;
    if (!GetConcreteObjectAndGLName(pobj, &prog, &progname))
        return ErrorInvalidOperation("GetUniform: invalid program");

    WebGLUniformLocation *location;
    if (!GetConcreteObject(ploc, &location))
        return ErrorInvalidValue("GetUniform: invalid uniform location");

    if (location->Program() != prog)
        return ErrorInvalidValue("GetUniform: this uniform location corresponds to another program");

    if (location->ProgramGeneration() != prog->Generation())
        return ErrorInvalidValue("GetUniform: this uniform location is obsolete since the program has been relinked");

    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

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
    GLint index;
    for (index = 0; index < uniforms; ++index) {
        GLsizei dummyLength;
        GLint dummySize;
        gl->fGetActiveUniform(progname, index, uniformNameMaxLength, &dummyLength,
                              &dummySize, &uniformType, uniformName);
        if (gl->fGetUniformLocation(progname, uniformName) == location->Location())
            break;
    }

    if (index == uniforms)
        return NS_ERROR_FAILURE; // XXX GL error? shouldn't happen.

    GLenum baseType;
    GLint unitSize;
    if (!BaseTypeAndSizeFromUniformType(uniformType, &baseType, &unitSize))
        return NS_ERROR_FAILURE;

    // this should never happen
    if (unitSize > 16)
        return NS_ERROR_FAILURE;

    if (baseType == LOCAL_GL_FLOAT) {
        GLfloat fv[16];
        gl->fGetUniformfv(progname, location->Location(), fv);
        if (unitSize == 1) {
            wrval->SetAsFloat(fv[0]);
        } else {
            wrval->SetAsArray(nsIDataType::VTYPE_FLOAT, nsnull,
                              unitSize, static_cast<void*>(fv));
        }
    } else if (baseType == LOCAL_GL_INT) {
        GLint iv[16];
        gl->fGetUniformiv(progname, location->Location(), iv);
        if (unitSize == 1) {
            wrval->SetAsInt32(iv[0]);
        } else {
            wrval->SetAsArray(nsIDataType::VTYPE_INT32, nsnull,
                              unitSize, static_cast<void*>(iv));
        }
    } else if (baseType == LOCAL_GL_BOOL) {
        GLint iv[16];
        gl->fGetUniformiv(progname, location->Location(), iv);
        if (unitSize == 1) {
            wrval->SetAsBool(PRBool(iv[0]));
        } else {
            PRUint8 uv[16];
            for (int k = 0; k < unitSize; k++)
                uv[k] = PRUint8(iv[k]);
            wrval->SetAsArray(nsIDataType::VTYPE_UINT8, nsnull,
                              unitSize, static_cast<void*>(uv));
        }
    } else {
        wrval->SetAsVoid();
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetUniformLocation(nsIWebGLProgram *pobj, const nsAString& name, nsIWebGLUniformLocation **retval)
{
    WebGLuint progname;
    WebGLProgram *prog;
    if (!GetConcreteObjectAndGLName(pobj, &prog, &progname))
        return ErrorInvalidOperation("GetUniformLocation: invalid program");

    MakeContextCurrent();

    GLint intlocation = gl->fGetUniformLocation(progname, NS_LossyConvertUTF16toASCII(name).get());

    nsRefPtr<nsIWebGLUniformLocation> loc = prog->GetUniformLocationObject(intlocation);
    *retval = loc.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetVertexAttrib(WebGLuint index, WebGLenum pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        // int
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_SIZE:
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_STRIDE:
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_TYPE:
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        {
            PRInt32 i = 0;
            gl->fGetVertexAttribiv(index, pname, (GLint*) &i);
            js.SetRetVal(i);
        }
            break;

        case LOCAL_GL_CURRENT_VERTEX_ATTRIB:
        {
            GLfloat fv[4] = { 0 };
            gl->fGetVertexAttribfv(index, LOCAL_GL_CURRENT_VERTEX_ATTRIB, fv);
            js.SetRetVal(fv, 4);
        }
            break;
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED:
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
        {
            PRInt32 i = 0;
            gl->fGetVertexAttribiv(index, pname, (GLint*) &i);
            js.SetBoolRetVal(PRBool(i));
        }
            break;

        // not supported; doesn't make sense to return a pointer unless we have some kind of buffer object abstraction
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER:
        default:
            return ErrorInvalidEnum("GetVertexAttrib: invalid parameter");
    }

    return NS_OK;
}

/* GLuint getVertexAttribOffset (in GLuint index, in GLenum pname); */
NS_IMETHODIMP
WebGLContext::GetVertexAttribOffset(WebGLuint index, WebGLenum pname, WebGLuint *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebGLContext::Hint(WebGLenum target, WebGLenum mode)
{
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsBuffer(nsIWebGLBuffer *bobj, WebGLboolean *retval)
{
    PRBool isDeleted;
    *retval = CheckConversion<WebGLBuffer>(bobj, 0, &isDeleted) && !isDeleted;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsFramebuffer(nsIWebGLFramebuffer *fbobj, WebGLboolean *retval)
{
    PRBool isDeleted;
    *retval = CheckConversion<WebGLFramebuffer>(fbobj, 0, &isDeleted) && !isDeleted;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsProgram(nsIWebGLProgram *pobj, WebGLboolean *retval)
{
    PRBool isDeleted;
    *retval = CheckConversion<WebGLProgram>(pobj, 0, &isDeleted) && !isDeleted;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsRenderbuffer(nsIWebGLRenderbuffer *rbobj, WebGLboolean *retval)
{
    PRBool isDeleted;
    *retval = CheckConversion<WebGLRenderbuffer>(rbobj, 0, &isDeleted) && !isDeleted;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsShader(nsIWebGLShader *sobj, WebGLboolean *retval)
{
    PRBool isDeleted;
    *retval = CheckConversion<WebGLShader>(sobj, 0, &isDeleted) && !isDeleted;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsTexture(nsIWebGLTexture *tobj, WebGLboolean *retval)
{
    PRBool isDeleted;
    *retval = CheckConversion<WebGLTexture>(tobj, 0, &isDeleted) && !isDeleted;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsEnabled(WebGLenum cap, WebGLboolean *retval)
{
    if (!ValidateCapabilityEnum(cap, "isEnabled")) {
        *retval = 0; // as per the OpenGL ES spec
        return NS_OK;
    }

    MakeContextCurrent();
    *retval = gl->fIsEnabled(cap);
    return NS_OK;
}

GL_SAME_METHOD_1(LineWidth, LineWidth, WebGLfloat)

NS_IMETHODIMP
WebGLContext::LinkProgram(nsIWebGLProgram *pobj)
{
    GLuint progname;
    WebGLProgram *program;
    if (!GetConcreteObjectAndGLName(pobj, &program, &progname))
        return ErrorInvalidOperation("LinkProgram: invalid program");

    if (!program->NextGeneration())
        return NS_ERROR_FAILURE;

    if (!program->HasBothShaderTypesAttached()) {
        program->SetLinkStatus(PR_FALSE);
        return NS_OK;
    }

    MakeContextCurrent();

    gl->fLinkProgram(progname);

    GLint ok;
    gl->fGetProgramiv(progname, LOCAL_GL_LINK_STATUS, &ok);
    if (ok) {
        program->SetLinkStatus(PR_TRUE);
        program->UpdateInfo(gl);
    } else {
        program->SetLinkStatus(PR_FALSE);
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::PixelStorei(WebGLenum pname, WebGLint param)
{
    switch (pname) {
        case UNPACK_FLIP_Y_WEBGL:
            mPixelStoreFlipY = (param != 0);
            break;
        case UNPACK_PREMULTIPLY_ALPHA_WEBGL:
            mPixelStorePremultiplyAlpha = (param != 0);
            break;
        case LOCAL_GL_PACK_ALIGNMENT:
        case LOCAL_GL_UNPACK_ALIGNMENT:
             if (param != 1 &&
                 param != 2 &&
                 param != 4 &&
                 param != 8)
                 return ErrorInvalidValue("PixelStorei: invalid pack/unpack alignment value");
            MakeContextCurrent();
            gl->fPixelStorei(pname, param);
            break;
        default:
            return ErrorInvalidEnum("PixelStorei: invalid parameter");
    }

    return NS_OK;
}


GL_SAME_METHOD_2(PolygonOffset, PolygonOffset, WebGLfloat, WebGLfloat)

NS_IMETHODIMP
WebGLContext::ReadPixels(PRInt32 dummy)
{
    return NS_ERROR_FAILURE;
}

nsresult
WebGLContext::ReadPixels_base(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height,
                              WebGLenum format, WebGLenum type, void *data, PRUint32 byteLength)
{
    if (HTMLCanvasElement()->IsWriteOnly() && !nsContentUtils::IsCallerTrustedForRead()) {
        LogMessage("ReadPixels: Not allowed");
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    if (width < 0 || height < 0)
        return ErrorInvalidValue("ReadPixels: negative size passed");

    WebGLsizei boundWidth = mBoundFramebuffer ? mBoundFramebuffer->width() : mWidth;
    WebGLsizei boundHeight = mBoundFramebuffer ? mBoundFramebuffer->height() : mHeight;

    PRUint32 size = 0;
    switch (format) {
      case LOCAL_GL_ALPHA:
        size = 1;
        break;
      case LOCAL_GL_RGB:
        size = 3;
        break;
      case LOCAL_GL_RGBA:
        size = 4;
        break;
      default:
        return ErrorInvalidEnum("ReadPixels: unsupported pixel format");
    }

    switch (type) {
//         case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
//         case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
//         case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
      case LOCAL_GL_UNSIGNED_BYTE:
        break;
      default:
        return ErrorInvalidEnum("ReadPixels: unsupported pixel type");
    }

    MakeContextCurrent();

    PRUint32 packAlignment;
    gl->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, (GLint*) &packAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * size;

    // alignedRowSize = row size rounded up to next multiple of packAlignment
    CheckedUint32 checked_alignedRowSize
        = ((checked_plainRowSize + packAlignment-1) / packAlignment) * packAlignment;

    CheckedUint32 checked_neededByteLength
        = (height-1) * checked_alignedRowSize + checked_plainRowSize;

    if (!checked_neededByteLength.valid())
        return ErrorInvalidOperation("ReadPixels: integer overflow computing the needed buffer size");

    if (checked_neededByteLength.value() > byteLength)
        return ErrorInvalidOperation("ReadPixels: buffer too small");

    if (CanvasUtils::CheckSaneSubrectSize(x, y, width, height, boundWidth, boundHeight)) {
        // the easy case: we're not reading out-of-range pixels
        gl->fReadPixels(x, y, width, height, format, type, data);
    } else {
        // the rectangle doesn't fit entirely in the bound buffer. We then have to set to zero the part
        // of the buffer that correspond to out-of-range pixels. We don't want to rely on system OpenGL
        // to do that for us, because passing out of range parameters to a buggy OpenGL implementation
        // could conceivably allow to read memory we shouldn't be allowed to read. So we manually initialize
        // the buffer to zero and compute the parameters to pass to OpenGL. We have to use an intermediate buffer
        // to accomodate the potentially different strides (widths).

        // zero the whole destination buffer. Too bad for the part that's going to be overwritten, we're not
        // 100% efficient here, but in practice this is a quite rare case anyway.
        memset(data, 0, byteLength);

        if (   x >= boundWidth
            || x+width <= 0
            || y >= boundHeight
            || y+height <= 0)
        {
            // we are completely outside of range, can exit now with buffer filled with zeros
            return NS_OK;
        }

        // compute the parameters of the subrect we're actually going to call glReadPixels on
        GLint   subrect_x      = PR_MAX(x, 0);
        GLint   subrect_end_x  = PR_MIN(x+width, boundWidth);
        GLsizei subrect_width  = subrect_end_x - subrect_x;

        GLint   subrect_y      = PR_MAX(y, 0);
        GLint   subrect_end_y  = PR_MIN(y+height, boundHeight);
        GLsizei subrect_height = subrect_end_y - subrect_y;

        if (subrect_width < 0 || subrect_height < 0 ||
            subrect_width > width || subrect_height)
            return ErrorInvalidOperation("ReadPixels: integer overflow computing clipped rect size");

        // now we know that subrect_width is in the [0..width] interval, and same for heights.

        // now, same computation as above to find the size of the intermediate buffer to allocate for the subrect
        // no need to check again for integer overflow here, since we already know the sizes aren't greater than before
        PRUint32 subrect_plainRowSize = subrect_width * size;
        PRUint32 subrect_alignedRowSize = (subrect_plainRowSize + packAlignment-1) &
            ~PRUint32(packAlignment-1);
        PRUint32 subrect_byteLength = (subrect_height-1)*subrect_alignedRowSize + subrect_plainRowSize;

        // create subrect buffer, call glReadPixels, copy pixels into destination buffer, delete subrect buffer
        GLubyte *subrect_data = new GLubyte[subrect_byteLength];
        gl->fReadPixels(subrect_x, subrect_y, subrect_width, subrect_height, format, type, subrect_data);

        // notice that this for loop terminates because we already checked that subrect_height is at most height
        for (GLint y_inside_subrect = 0; y_inside_subrect < subrect_height; ++y_inside_subrect) {
            GLint subrect_x_in_dest_buffer = subrect_x - x;
            GLint subrect_y_in_dest_buffer = subrect_y - y;
            memcpy(static_cast<GLubyte*>(data)
                     + checked_alignedRowSize.value() * (subrect_y_in_dest_buffer + y_inside_subrect)
                     + size * subrect_x_in_dest_buffer, // destination
                   subrect_data + subrect_alignedRowSize * y_inside_subrect, // source
                   subrect_plainRowSize); // size
        }
        delete [] subrect_data;
    }
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::ReadPixels_array(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height,
                               WebGLenum format, WebGLenum type, js::TypedArray *pixels)
{
    return ReadPixels_base(x, y, width, height, format, type,
                           pixels ? pixels->data : 0,
                           pixels ? pixels->byteLength : 0);
}

NS_IMETHODIMP
WebGLContext::ReadPixels_buf(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height,
                             WebGLenum format, WebGLenum type, js::ArrayBuffer *pixels)
{
    return ReadPixels_base(x, y, width, height, format, type,
                           pixels ? pixels->data : 0,
                           pixels ? pixels->byteLength : 0);
}

NS_IMETHODIMP
WebGLContext::ReadPixels_byteLength_old_API_deprecated(WebGLsizei width, WebGLsizei height,
                             WebGLenum format, WebGLenum type, WebGLsizei *retval)
{
    *retval = 0;
    if (width < 0 || height < 0)
        return ErrorInvalidValue("ReadPixels: negative size passed");

    PRUint32 size = 0;
    switch (format) {
      case LOCAL_GL_ALPHA:
        size = 1;
        break;
      case LOCAL_GL_RGB:
        size = 3;
        break;
      case LOCAL_GL_RGBA:
        size = 4;
        break;
      default:
        return ErrorInvalidEnum("ReadPixels: unsupported pixel format");
    }

    switch (type) {
//         case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
//         case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
//         case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
      case LOCAL_GL_UNSIGNED_BYTE:
        break;
      default:
        return ErrorInvalidEnum("ReadPixels: unsupported pixel type");
    }
    PRUint32 packAlignment;
    gl->fGetIntegerv(LOCAL_GL_PACK_ALIGNMENT, (GLint*) &packAlignment);

    CheckedUint32 checked_plainRowSize = CheckedUint32(width) * size;

    // alignedRowSize = row size rounded up to next multiple of
    // packAlignment which is a power of 2
    CheckedUint32 checked_alignedRowSize
        = ((checked_plainRowSize + packAlignment-1) / packAlignment) * packAlignment;

    CheckedUint32 checked_neededByteLength = (height-1)*checked_alignedRowSize + checked_plainRowSize;

    if (!checked_neededByteLength.valid())
        return ErrorInvalidOperation("ReadPixels: integer overflow computing the needed buffer size");

    *retval = checked_neededByteLength.value();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::RenderbufferStorage(WebGLenum target, WebGLenum internalformat, WebGLsizei width, WebGLsizei height)
{
    if (target != LOCAL_GL_RENDERBUFFER)
        return ErrorInvalidEnum("RenderbufferStorage: invalid target.");

    switch (internalformat) {
      case LOCAL_GL_RGBA4:
      // XXX case LOCAL_GL_RGB565:
      case LOCAL_GL_RGB5_A1:
      case LOCAL_GL_DEPTH_COMPONENT:
      case LOCAL_GL_DEPTH_COMPONENT16:
      case LOCAL_GL_STENCIL_INDEX8:
          break;
      default:
          return ErrorInvalidEnum("RenderbufferStorage: invalid internalformat.");
    }

    if (width <= 0 || height <= 0)
        return ErrorInvalidValue("RenderbufferStorage: width and height must be > 0");

    if (mBoundRenderbuffer)
        mBoundRenderbuffer->setDimensions(width, height);

    MakeContextCurrent();
    gl->fRenderbufferStorage(target, internalformat, width, height);

    return NS_OK;
}

GL_SAME_METHOD_2(SampleCoverage, SampleCoverage, WebGLfloat, WebGLboolean)

NS_IMETHODIMP
WebGLContext::Scissor(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height)
{
    if (width < 0 || height < 0)
        return ErrorInvalidValue("Scissor: negative size");

    MakeContextCurrent();
    gl->fScissor(x, y, width, height);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::StencilFunc(WebGLenum func, WebGLint ref, WebGLuint mask)
{
    if (!ValidateComparisonEnum(func, "stencilFunc: func"))
        return NS_OK;

    MakeContextCurrent();
    gl->fStencilFunc(func, ref, mask);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::StencilFuncSeparate(WebGLenum face, WebGLenum func, WebGLint ref, WebGLuint mask)
{
    if (!ValidateFaceEnum(face, "stencilFuncSeparate: face") ||
        !ValidateComparisonEnum(func, "stencilFuncSeparate: func"))
        return NS_OK;

    MakeContextCurrent();
    gl->fStencilFuncSeparate(face, func, ref, mask);
    return NS_OK;
}

GL_SAME_METHOD_1(StencilMask, StencilMask, WebGLuint)

NS_IMETHODIMP
WebGLContext::StencilMaskSeparate(WebGLenum face, WebGLuint mask)
{
    if (!ValidateFaceEnum(face, "stencilMaskSeparate: face"))
        return NS_OK;

    MakeContextCurrent();
    gl->fStencilMaskSeparate(face, mask);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::StencilOp(WebGLenum sfail, WebGLenum dpfail, WebGLenum dppass)
{
    if (!ValidateStencilOpEnum(sfail, "stencilOp: sfail") ||
        !ValidateStencilOpEnum(dpfail, "stencilOp: dpfail") ||
        !ValidateStencilOpEnum(dppass, "stencilOp: dppass"))
        return NS_OK;

    MakeContextCurrent();
    gl->fStencilOp(sfail, dpfail, dppass);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::StencilOpSeparate(WebGLenum face, WebGLenum sfail, WebGLenum dpfail, WebGLenum dppass)
{
    if (!ValidateFaceEnum(face, "stencilOpSeparate: face") ||
        !ValidateStencilOpEnum(sfail, "stencilOpSeparate: sfail") ||
        !ValidateStencilOpEnum(dpfail, "stencilOpSeparate: dpfail") ||
        !ValidateStencilOpEnum(dppass, "stencilOpSeparate: dppass"))
        return NS_OK;

    MakeContextCurrent();
    gl->fStencilOpSeparate(face, sfail, dpfail, dppass);
    return NS_OK;
}

template<int format>
inline void convert_pixel(PRUint8* dst, const PRUint8* src)
{
    // since has_alpha is a compile time constant, any if(has_alpha) evaluates
    // at compile time, so has zero runtime cost.
    enum { has_alpha = format == gfxASurface::ImageFormatARGB32 };

#ifdef IS_LITTLE_ENDIAN
    PRUint8 b = *src++;
    PRUint8 g = *src++;
    PRUint8 r = *src++;
    PRUint8 a = *src;
#else
    PRUint8 a = *src++;
    PRUint8 r = *src++;
    PRUint8 g = *src++;
    PRUint8 b = *src;
#endif

    if (has_alpha) {
        // Convert to non-premultiplied color
        if (a != 0) {
            r = (r * 255) / a;
            g = (g * 255) / a;
            b = (b * 255) / a;
        }
    }

    *dst++ = r;
    *dst++ = g;
    *dst++ = b;
    if (has_alpha)
        *dst = a;
    else
        *dst = 255;
}

nsresult
WebGLContext::DOMElementToImageSurface(nsIDOMElement *imageOrCanvas,
                                       gfxImageSurface **imageOut,
                                       PRBool flipY, PRBool premultiplyAlpha)
{
    gfxImageSurface *surf = nsnull;

    nsLayoutUtils::SurfaceFromElementResult res =
        nsLayoutUtils::SurfaceFromElement(imageOrCanvas,
                                          nsLayoutUtils::SFE_WANT_NEW_SURFACE | nsLayoutUtils::SFE_WANT_IMAGE_SURFACE);
    if (!res.mSurface)
        return NS_ERROR_FAILURE;

    CanvasUtils::DoDrawImageSecurityCheck(HTMLCanvasElement(), res.mPrincipal, res.mIsWriteOnly);

    if (res.mSurface->GetType() != gfxASurface::SurfaceTypeImage) {
        // SurfaceFromElement lied!
        return NS_ERROR_FAILURE;
    }

    surf = static_cast<gfxImageSurface*>(res.mSurface.get());

    PRInt32 width, height;
    width = res.mSize.width;
    height = res.mSize.height;

    if (width <= 0 || height <= 0)
        return NS_ERROR_FAILURE;

    // this wants some SSE love
    int row1 = 0, row2 = height-1;
    for (; flipY ? (row1 <= row2) : (row1 < height); row1++, row2--) {
        PRUint8 *row1_start = surf->Data() + row1 * surf->Stride();
        PRUint8 *row1_end = row1_start + surf->Stride();
        PRUint8 *row2_start = surf->Data() + row2 * surf->Stride();

        if (flipY == PR_FALSE || row1 == row2) {
            if (surf->Format() == gfxASurface::ImageFormatARGB32) {
                for (PRUint8 *row1_ptr = row1_start; row1_ptr != row1_end; row1_ptr += 4) {
                    convert_pixel<gfxASurface::ImageFormatARGB32>(row1_ptr, row1_ptr);
                }
            } else if (surf->Format() == gfxASurface::ImageFormatRGB24) {
                for (PRUint8 *row1_ptr = row1_start; row1_ptr != row1_end; row1_ptr += 4) {
                    convert_pixel<gfxASurface::ImageFormatRGB24>(row1_ptr, row1_ptr);
                }
            } else {
                return NS_ERROR_FAILURE;
            }
        } else {
            PRUint8 *row1_ptr = row1_start;
            PRUint8 *row2_ptr = row2_start;
            PRUint8 tmp[4];
            if (surf->Format() == gfxASurface::ImageFormatARGB32) {
                for (; row1_ptr != row1_end; row1_ptr += 4, row2_ptr += 4) {
                    convert_pixel<gfxASurface::ImageFormatARGB32>(tmp, row1_ptr);
                    convert_pixel<gfxASurface::ImageFormatARGB32>(row1_ptr, row2_ptr);
                    *reinterpret_cast<PRUint32*>(row2_ptr) = *reinterpret_cast<PRUint32*>(tmp);
                }
            } else if (surf->Format() == gfxASurface::ImageFormatRGB24) {
                for (; row1_ptr != row1_end; row1_ptr += 4, row2_ptr += 4) {
                    convert_pixel<gfxASurface::ImageFormatRGB24>(tmp, row1_ptr);
                    convert_pixel<gfxASurface::ImageFormatRGB24>(row1_ptr, row2_ptr);
                    *reinterpret_cast<PRUint32*>(row2_ptr) = *reinterpret_cast<PRUint32*>(tmp);
                }
            } else {
                return NS_ERROR_FAILURE;
            }
        }
    }

    res.mSurface.forget();
    *imageOut = surf;

    return NS_OK;
}

#define OBTAIN_UNIFORM_LOCATION                                         \
    WebGLUniformLocation *location_object;                              \
    if (!GetConcreteObject(ploc, &location_object))                     \
        return ErrorInvalidValue("Invalid uniform location parameter"); \
    if (mCurrentProgram != location_object->Program())                  \
        return ErrorInvalidValue("This uniform location corresponds to another program"); \
    if (mCurrentProgram->Generation() != location_object->ProgramGeneration())            \
        return ErrorInvalidValue("This uniform location is obsolete since the program has been relinked"); \
    GLint location = location_object->Location();

#define SIMPLE_ARRAY_METHOD_UNIFORM(name, cnt, arrayType, ptrType)      \
NS_IMETHODIMP                                                           \
WebGLContext::name(PRInt32 dummy) {                                     \
     return NS_ERROR_NOT_IMPLEMENTED;                                   \
}                                                                       \
NS_IMETHODIMP                                                           \
WebGLContext::name##_array(nsIWebGLUniformLocation *ploc, js::TypedArray *wa) \
{                                                                       \
    OBTAIN_UNIFORM_LOCATION                                             \
    if (!wa || wa->type != js::TypedArray::arrayType)                   \
        return ErrorInvalidOperation("array must be " #arrayType);      \
    if (wa->length == 0 || wa->length % cnt != 0)                       \
        return ErrorInvalidOperation("array must be > 0 elements and have a length multiple of %d", cnt); \
    MakeContextCurrent();                                               \
    gl->f##name(location, wa->length / cnt, (ptrType *)wa->data);            \
    return NS_OK;                                                       \
}

#define SIMPLE_ARRAY_METHOD_NO_COUNT(name, cnt, arrayType, ptrType)  \
NS_IMETHODIMP                                                           \
WebGLContext::name(PRInt32 dummy) {                                     \
     return NS_ERROR_NOT_IMPLEMENTED;                                   \
}                                                                       \
NS_IMETHODIMP                                                           \
WebGLContext::name##_array(WebGLuint idx, js::TypedArray *wa)              \
{                                                                       \
    if (!wa || wa->type != js::TypedArray::arrayType)                   \
        return ErrorInvalidOperation("array must be " #arrayType);      \
    if (wa->length < cnt)                                               \
        return ErrorInvalidOperation("array must be >= %d elements", cnt); \
    MakeContextCurrent();                                               \
    gl->f##name(idx, (ptrType *)wa->data);                              \
    return NS_OK;                                                       \
}

#define SIMPLE_MATRIX_METHOD_UNIFORM(name, dim, arrayType, ptrType)     \
NS_IMETHODIMP                                                           \
WebGLContext::name(PRInt32 dummy) {                                     \
     return NS_ERROR_NOT_IMPLEMENTED;                                   \
}                                                                       \
NS_IMETHODIMP                                                           \
WebGLContext::name##_array(nsIWebGLUniformLocation *ploc, WebGLboolean transpose, js::TypedArray *wa)  \
{                                                                       \
    OBTAIN_UNIFORM_LOCATION                                             \
    if (!wa || wa->type != js::TypedArray::arrayType)                   \
        return ErrorInvalidOperation("array must be " #arrayType);      \
    if (wa->length == 0 || wa->length % (dim*dim) != 0)                 \
        return ErrorInvalidOperation("array must be > 0 elements and have a length multiple of %d", dim*dim); \
    MakeContextCurrent();                                               \
    gl->f##name(location, wa->length / (dim*dim), transpose, (ptrType *)wa->data); \
    return NS_OK;                                                       \
}

#define SIMPLE_METHOD_UNIFORM_1(glname, name, t1)        \
NS_IMETHODIMP WebGLContext::name(nsIWebGLUniformLocation *ploc, t1 a1) {      \
    OBTAIN_UNIFORM_LOCATION \
    MakeContextCurrent(); gl->f##glname(location, a1); return NS_OK; \
}

#define SIMPLE_METHOD_UNIFORM_2(glname, name, t1, t2)        \
NS_IMETHODIMP WebGLContext::name(nsIWebGLUniformLocation *ploc, t1 a1, t2 a2) {      \
    OBTAIN_UNIFORM_LOCATION \
    MakeContextCurrent(); gl->f##glname(location, a1, a2); return NS_OK; \
}

#define SIMPLE_METHOD_UNIFORM_3(glname, name, t1, t2, t3)        \
NS_IMETHODIMP WebGLContext::name(nsIWebGLUniformLocation *ploc, t1 a1, t2 a2, t3 a3) {      \
    OBTAIN_UNIFORM_LOCATION \
    MakeContextCurrent(); gl->f##glname(location, a1, a2, a3); return NS_OK; \
}

#define SIMPLE_METHOD_UNIFORM_4(glname, name, t1, t2, t3, t4)        \
NS_IMETHODIMP WebGLContext::name(nsIWebGLUniformLocation *ploc, t1 a1, t2 a2, t3 a3, t4 a4) {      \
    OBTAIN_UNIFORM_LOCATION \
    MakeContextCurrent(); gl->f##glname(location, a1, a2, a3, a4); return NS_OK; \
}

SIMPLE_METHOD_UNIFORM_1(Uniform1i, Uniform1i, WebGLint)
SIMPLE_METHOD_UNIFORM_2(Uniform2i, Uniform2i, WebGLint, WebGLint)
SIMPLE_METHOD_UNIFORM_3(Uniform3i, Uniform3i, WebGLint, WebGLint, WebGLint)
SIMPLE_METHOD_UNIFORM_4(Uniform4i, Uniform4i, WebGLint, WebGLint, WebGLint, WebGLint)

SIMPLE_METHOD_UNIFORM_1(Uniform1f, Uniform1f, WebGLfloat)
SIMPLE_METHOD_UNIFORM_2(Uniform2f, Uniform2f, WebGLfloat, WebGLfloat)
SIMPLE_METHOD_UNIFORM_3(Uniform3f, Uniform3f, WebGLfloat, WebGLfloat, WebGLfloat)
SIMPLE_METHOD_UNIFORM_4(Uniform4f, Uniform4f, WebGLfloat, WebGLfloat, WebGLfloat, WebGLfloat)

SIMPLE_ARRAY_METHOD_UNIFORM(Uniform1iv, 1, TYPE_INT32, WebGLint)
SIMPLE_ARRAY_METHOD_UNIFORM(Uniform2iv, 2, TYPE_INT32, WebGLint)
SIMPLE_ARRAY_METHOD_UNIFORM(Uniform3iv, 3, TYPE_INT32, WebGLint)
SIMPLE_ARRAY_METHOD_UNIFORM(Uniform4iv, 4, TYPE_INT32, WebGLint)

SIMPLE_ARRAY_METHOD_UNIFORM(Uniform1fv, 1, TYPE_FLOAT32, WebGLfloat)
SIMPLE_ARRAY_METHOD_UNIFORM(Uniform2fv, 2, TYPE_FLOAT32, WebGLfloat)
SIMPLE_ARRAY_METHOD_UNIFORM(Uniform3fv, 3, TYPE_FLOAT32, WebGLfloat)
SIMPLE_ARRAY_METHOD_UNIFORM(Uniform4fv, 4, TYPE_FLOAT32, WebGLfloat)

SIMPLE_MATRIX_METHOD_UNIFORM(UniformMatrix2fv, 2, TYPE_FLOAT32, WebGLfloat)
SIMPLE_MATRIX_METHOD_UNIFORM(UniformMatrix3fv, 3, TYPE_FLOAT32, WebGLfloat)
SIMPLE_MATRIX_METHOD_UNIFORM(UniformMatrix4fv, 4, TYPE_FLOAT32, WebGLfloat)

GL_SAME_METHOD_2(VertexAttrib1f, VertexAttrib1f, PRUint32, WebGLfloat)
GL_SAME_METHOD_3(VertexAttrib2f, VertexAttrib2f, PRUint32, WebGLfloat, WebGLfloat)
GL_SAME_METHOD_4(VertexAttrib3f, VertexAttrib3f, PRUint32, WebGLfloat, WebGLfloat, WebGLfloat)
GL_SAME_METHOD_5(VertexAttrib4f, VertexAttrib4f, PRUint32, WebGLfloat, WebGLfloat, WebGLfloat, WebGLfloat)

SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib1fv, 1, TYPE_FLOAT32, WebGLfloat)
SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib2fv, 2, TYPE_FLOAT32, WebGLfloat)
SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib3fv, 3, TYPE_FLOAT32, WebGLfloat)
SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib4fv, 4, TYPE_FLOAT32, WebGLfloat)

NS_IMETHODIMP
WebGLContext::UseProgram(nsIWebGLProgram *pobj)
{
    WebGLProgram *prog;
    WebGLuint progname;
    PRBool isNull;
    if (!GetConcreteObjectAndGLName(pobj, &prog, &progname, &isNull))
        return ErrorInvalidOperation("UseProgram: invalid program object");

    MakeContextCurrent();

    if (isNull) {
        gl->fUseProgram(0);
        mCurrentProgram = nsnull;
    } else {
        if (!prog->LinkStatus())
            return ErrorInvalidOperation("UseProgram: program was not linked successfully");
        gl->fUseProgram(progname);
        mCurrentProgram = prog;
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::ValidateProgram(nsIWebGLProgram *pobj)
{
    WebGLuint progname;
    if (!GetGLName<WebGLProgram>(pobj, &progname))
        return ErrorInvalidOperation("ValidateProgram: Invalid program object");

    MakeContextCurrent();

    gl->fValidateProgram(progname);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CreateFramebuffer(nsIWebGLFramebuffer **retval)
{
    MakeContextCurrent();

    GLuint name;
    gl->fGenFramebuffers(1, &name);

    WebGLFramebuffer *globj = new WebGLFramebuffer(this, name);
    NS_ADDREF(*retval = globj);
    mMapFramebuffers.Put(name, globj);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CreateRenderbuffer(nsIWebGLRenderbuffer **retval)
{
    MakeContextCurrent();

    GLuint name;
    gl->fGenRenderbuffers(1, &name);

    WebGLRenderbuffer *globj = new WebGLRenderbuffer(this, name);
    NS_ADDREF(*retval = globj);
    mMapRenderbuffers.Put(name, globj);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Viewport(WebGLint x, WebGLint y, WebGLsizei width, WebGLsizei height)
{
    if (width < 0 || height < 0)
        return ErrorInvalidOperation("Viewport: negative size");

    MakeContextCurrent();
    gl->fViewport(x, y, width, height);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CompileShader(nsIWebGLShader *sobj)
{
    WebGLuint shadername;
    if (!GetGLName<WebGLShader>(sobj, &shadername))
        return ErrorInvalidOperation("CompileShader: invalid shader");

    MakeContextCurrent();

    gl->fCompileShader(shadername);

    return NS_OK;
}


NS_IMETHODIMP
WebGLContext::GetShaderParameter(nsIWebGLShader *sobj, WebGLenum pname, nsIVariant **retval)
{
    WebGLuint shadername;
    if (!GetGLName<WebGLShader>(sobj, &shadername))
        return ErrorInvalidOperation("GetShaderParameter: invalid shader");

    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_SHADER_TYPE:
        case LOCAL_GL_INFO_LOG_LENGTH:
        case LOCAL_GL_SHADER_SOURCE_LENGTH:
        {
            GLint i = 0;
            gl->fGetShaderiv(shadername, pname, &i);
            wrval->SetAsInt32(i);
        }
            break;
        case LOCAL_GL_DELETE_STATUS:
        case LOCAL_GL_COMPILE_STATUS:
        {
            GLint i = 0;
            gl->fGetShaderiv(shadername, pname, &i);
            wrval->SetAsBool(PRBool(i));
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    *retval = wrval.forget().get();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetShaderInfoLog(nsIWebGLShader *sobj, nsAString& retval)
{
    WebGLuint shadername;
    if (!GetGLName<WebGLShader>(sobj, &shadername))
        return ErrorInvalidOperation("GetShaderInfoLog: invalid shader");

    MakeContextCurrent();

    PRInt32 k = -1;
    gl->fGetShaderiv(shadername, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &k);
    if (k == -1)
        return NS_ERROR_FAILURE; // XXX GL Error? should never happen.

    if (k == 0) {
        retval.Truncate();
        return NS_OK;
    }

    nsCAutoString log;
    log.SetCapacity(k);

    gl->fGetShaderInfoLog(shadername, k, (GLint*) &k, (char*) log.BeginWriting());

    log.SetLength(k);

    CopyASCIItoUTF16(log, retval);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetShaderSource(nsIWebGLShader *sobj, nsAString& retval)
{
    WebGLuint shadername;
    if (!GetGLName<WebGLShader>(sobj, &shadername))
        return ErrorInvalidOperation("GetShaderSource: invalid shader");

    MakeContextCurrent();

    GLint slen = -1;
    gl->fGetShaderiv(shadername, LOCAL_GL_SHADER_SOURCE_LENGTH, &slen);
    if (slen == -1)
        return NS_ERROR_FAILURE;

    if (slen == 0) {
        retval.Truncate();
        return NS_OK;
    }

    nsCAutoString src;
    src.SetCapacity(slen);

    gl->fGetShaderSource(shadername, slen, NULL, (char*) src.BeginWriting());

    src.SetLength(slen);

    CopyASCIItoUTF16(src, retval);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::ShaderSource(nsIWebGLShader *sobj, const nsAString& source)
{
    WebGLuint shadername;
    if (!GetGLName<WebGLShader>(sobj, &shadername))
        return ErrorInvalidOperation("ShaderSource: invalid shader");
    
    MakeContextCurrent();

    NS_LossyConvertUTF16toASCII asciisrc(source);
    const char *p = asciisrc.get();

    gl->fShaderSource(shadername, 1, &p, NULL);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::VertexAttribPointer(WebGLuint index, WebGLint size, WebGLenum type,
                                  WebGLboolean normalized, WebGLuint stride,
                                  WebGLuint byteOffset)
{
    if (mBoundArrayBuffer == nsnull)
        return ErrorInvalidOperation("VertexAttribPointer: must have valid GL_ARRAY_BUFFER binding");

    switch (type) {
      case LOCAL_GL_BYTE:
      case LOCAL_GL_UNSIGNED_BYTE:
      case LOCAL_GL_SHORT:
      case LOCAL_GL_UNSIGNED_SHORT:
      // XXX case LOCAL_GL_FIXED:
      case LOCAL_GL_FLOAT:
          break;
      default:
          return ErrorInvalidEnum("VertexAttribPointer: invalid type");
    }

    if (index >= mAttribBuffers.Length())
        return ErrorInvalidValue("VertexAttribPointer: index out of range - %d >= %d", index, mAttribBuffers.Length());

    if (size < 1 || size > 4)
        return ErrorInvalidValue("VertexAttribPointer: invalid element size");

    if (stride < 0)
        return ErrorInvalidValue("VertexAttribPointer: stride cannot be negative");

    /* XXX make work with bufferSubData & heterogeneous types 
    if (type != mBoundArrayBuffer->GLType())
        return ErrorInvalidOperation("VertexAttribPointer: type must match bound VBO type: %d != %d", type, mBoundArrayBuffer->GLType());
    */

    // XXX 0 stride?
    //if (stride < (GLuint) size)
    //    return ErrorInvalidOperation("VertexAttribPointer: stride must be >= size!");

    WebGLVertexAttribData &vd = mAttribBuffers[index];

    vd.buf = mBoundArrayBuffer;
    vd.stride = stride;
    vd.size = size;
    vd.byteOffset = byteOffset;
    vd.type = type;

    MakeContextCurrent();

    gl->fVertexAttribPointer(index, size, type, normalized,
                             stride,
                             (void*) (byteOffset));

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::TexImage2D(PRInt32 dummy)
{
    return NS_ERROR_FAILURE;
}

nsresult
WebGLContext::TexImage2D_base(WebGLenum target, WebGLint level, WebGLenum internalformat,
                              WebGLsizei width, WebGLsizei height, WebGLint border,
                              WebGLenum format, WebGLenum type,
                              void *data, PRUint32 byteLength)
{
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
            return ErrorInvalidEnum("TexImage2D: unsupported target");
    }

    if (level < 0)
        return ErrorInvalidValue("TexImage2D: level must be >= 0");

    switch (internalformat) {
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_LUMINANCE_ALPHA:
            break;
        default:
            return ErrorInvalidEnum("TexImage2D: invalid internal format");
    }

    if (width < 0 || height < 0)
        return ErrorInvalidValue("TexImage2D: width and height must be >= 0");

    if (border != 0)
        return ErrorInvalidValue("TexImage2D: border must be 0");

    PRUint32 texelSize = 0;
    if (!ValidateTexFormatAndType(format, type, &texelSize, "texImage2D"))
        return NS_OK;

    CheckedUint32 checked_bytesNeeded = CheckedUint32(width) * height * texelSize;

    if (!checked_bytesNeeded.valid())
        return ErrorInvalidOperation("texImage2D: integer overflow computing the needed buffer size");

    PRUint32 bytesNeeded = checked_bytesNeeded.value();
    
    if (byteLength && byteLength < bytesNeeded)
        return ErrorInvalidOperation("TexImage2D: not enough data for operation (need %d, have %d)",
                                 bytesNeeded, byteLength);

    MakeContextCurrent();

    if (byteLength) {
        gl->fTexImage2D(target, level, internalformat, width, height, border, format, type, data);
    } else {
        // We need some zero pages, because GL doesn't guarantee the
        // contents of a texture allocated with NULL data.
        // Hopefully calloc will just mmap zero pages here.
        void *tempZeroData = calloc(1, bytesNeeded);
        if (!tempZeroData)
            return SynthesizeGLError(LOCAL_GL_OUT_OF_MEMORY, "texImage2D: could not allocate %d bytes (for zero fill)", bytesNeeded);

        gl->fTexImage2D(target, level, internalformat, width, height, border, format, type, tempZeroData);

        free(tempZeroData);
    }

    if (mBound2DTextures[mActiveTexture])
        mBound2DTextures[mActiveTexture]->setDimensions(width, height);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::TexImage2D_buf(WebGLenum target, WebGLint level, WebGLenum internalformat,
                             WebGLsizei width, WebGLsizei height, WebGLint border,
                             WebGLenum format, WebGLenum type,
                             js::ArrayBuffer *pixels)
{
    return TexImage2D_base(target, level, internalformat, width, height, border, format, type,
                           pixels ? pixels->data : 0,
                           pixels ? pixels->byteLength : 0);
}

NS_IMETHODIMP
WebGLContext::TexImage2D_array(WebGLenum target, WebGLint level, WebGLenum internalformat,
                               WebGLsizei width, WebGLsizei height, WebGLint border,
                               WebGLenum format, WebGLenum type,
                               js::TypedArray *pixels)
{
    return TexImage2D_base(target, level, internalformat, width, height, border, format, type,
                           pixels ? pixels->data : 0,
                           pixels ? pixels->byteLength : 0);
}

NS_IMETHODIMP
WebGLContext::TexImage2D_dom(WebGLenum target, WebGLint level, WebGLenum internalformat,
                             WebGLenum format, GLenum type, nsIDOMElement *elt)
{
    nsRefPtr<gfxImageSurface> isurf;

    nsresult rv = DOMElementToImageSurface(elt, getter_AddRefs(isurf),
                                           mPixelStoreFlipY, mPixelStorePremultiplyAlpha);
    if (NS_FAILED(rv))
        return rv;

    NS_ASSERTION(isurf->Stride() == isurf->Width() * 4, "Bad stride!");

    PRUint32 byteLength = isurf->Stride() * isurf->Height();

    return TexImage2D_base(target, level, internalformat,
                           isurf->Width(), isurf->Height(), 0,
                           format, type,
                           isurf->Data(), byteLength);
}

NS_IMETHODIMP
WebGLContext::TexImage2D_dom_old_API_deprecated(WebGLenum target, WebGLint level, nsIDOMElement *elt,
                                                PRBool flipY, PRBool premultiplyAlpha)
{
    nsRefPtr<gfxImageSurface> isurf;

    nsresult rv = DOMElementToImageSurface(elt, getter_AddRefs(isurf),
                                           flipY, premultiplyAlpha);
    if (NS_FAILED(rv))
        return rv;

    NS_ASSERTION(isurf->Stride() == isurf->Width() * 4, "Bad stride!");

    PRUint32 byteLength = isurf->Stride() * isurf->Height();

    return TexImage2D_base(target, level, LOCAL_GL_RGBA,
                           isurf->Width(), isurf->Height(), 0,
                           LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                           isurf->Data(), byteLength);
}

NS_IMETHODIMP
WebGLContext::TexSubImage2D(PRInt32 dummy)
{
    return NS_ERROR_FAILURE;
}

nsresult
WebGLContext::TexSubImage2D_base(WebGLenum target, WebGLint level,
                                 WebGLint xoffset, WebGLint yoffset,
                                 WebGLsizei width, WebGLsizei height,
                                 WebGLenum format, WebGLenum type,
                                 void *pixels, PRUint32 byteLength)
{
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
            return ErrorInvalidEnum("TexSubImage2D: unsupported target");
    }

    if (level < 0)
        return ErrorInvalidValue("TexSubImage2D: level must be >= 0");

    if (width < 0 || height < 0)
        return ErrorInvalidValue("TexSubImage2D: width and height must be > 0!");

    PRUint32 texelSize = 0;
    if (!ValidateTexFormatAndType(format, type, &texelSize, "texSubImage2D"))
        return NS_OK;

    if (width == 0 || height == 0)
        return NS_OK; // ES 2.0 says it has no effect, we better return right now

    CheckedUint32 checked_bytesNeeded = CheckedUint32(width) * height * texelSize;

    if (!checked_bytesNeeded.valid())
        return ErrorInvalidOperation("texSubImage2D: integer overflow computing the needed buffer size");

    PRUint32 bytesNeeded = checked_bytesNeeded.value();
 
    if (byteLength < bytesNeeded)
        return ErrorInvalidValue("TexSubImage2D: not enough data for operation (need %d, have %d)", bytesNeeded, byteLength);

    MakeContextCurrent();

    gl->fTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::TexSubImage2D_buf(WebGLenum target, WebGLint level,
                                WebGLint xoffset, WebGLint yoffset,
                                WebGLsizei width, WebGLsizei height,
                                WebGLenum format, WebGLenum type,
                                js::ArrayBuffer *pixels)
{
    if (!pixels)
        return ErrorInvalidValue("TexSubImage2D: pixels must not be null!");

    return TexSubImage2D_base(target, level, xoffset, yoffset,
                              width, height, format, type,
                              pixels->data, pixels->byteLength);
}

NS_IMETHODIMP
WebGLContext::TexSubImage2D_array(WebGLenum target, WebGLint level,
                                  WebGLint xoffset, WebGLint yoffset,
                                  WebGLsizei width, WebGLsizei height,
                                  WebGLenum format, WebGLenum type,
                                  js::TypedArray *pixels)
{
    if (!pixels)
        return ErrorInvalidValue("TexSubImage2D: pixels must not be null!");

    return TexSubImage2D_base(target, level, xoffset, yoffset,
                              width, height, format, type,
                              pixels->data, pixels->byteLength);
}

NS_IMETHODIMP
WebGLContext::TexSubImage2D_dom(WebGLenum target, WebGLint level,
                                WebGLint xoffset, WebGLint yoffset,
                                WebGLenum format, WebGLenum type,
                                nsIDOMElement *elt)
{
    nsRefPtr<gfxImageSurface> isurf;

    nsresult rv = DOMElementToImageSurface(elt, getter_AddRefs(isurf),
                                           mPixelStoreFlipY, mPixelStorePremultiplyAlpha);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 byteLength = isurf->Stride() * isurf->Height();

    return TexSubImage2D_base(target, level,
                              xoffset, yoffset,
                              isurf->Width(), isurf->Height(),
                              LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                              isurf->Data(), byteLength);
}

#if 0
// ImageData getImageData (in float x, in float y, in float width, in float height);
NS_IMETHODIMP
WebGLContext::GetImageData(PRUint32 x, PRUint32 y, PRUint32 w, PRUint32 h)
{
    // disabled due to win32 linkage issues with thebes symbols and NS_RELEASE
    return NS_ERROR_FAILURE;

#if 0
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 4) return NS_ERROR_INVALID_ARG;
    
    if (!mGLPbuffer ||
        !mGLPbuffer->ThebesSurface())
        return NS_ERROR_FAILURE;

    if (!mCanvasElement)
        return NS_ERROR_FAILURE;

    if (HTMLCanvasElement()->IsWriteOnly() && !IsCallerTrustedForRead()) {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    JSContext *ctx = js.ctx;

    if (!CanvasUtils::CheckSaneSubrectSize (x, y, w, h, mWidth, mHeight))
        return NS_ERROR_DOM_SYNTAX_ERR;

    nsAutoArrayPtr<PRUint8> surfaceData (new (std::nothrow) PRUint8[w * h * 4]);
    int surfaceDataStride = w*4;
    int surfaceDataOffset = 0;

    if (!surfaceData)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<gfxImageSurface> tmpsurf = new gfxImageSurface(surfaceData,
                                                            gfxIntSize(w, h),
                                                            w * 4,
                                                            gfxASurface::ImageFormatARGB32);
    if (!tmpsurf || tmpsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> tmpctx = new gfxContext(tmpsurf);

    if (!tmpctx || tmpctx->HasError())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxASurface> surf = mGLPbuffer->ThebesSurface();
    nsRefPtr<gfxPattern> pat = CanvasGLThebes::CreatePattern(surf);
    gfxMatrix m;
    m.Translate(gfxPoint(x, mGLPbuffer->Height()-y));
    m.Scale(1.0, -1.0);
    pat->SetMatrix(m);

    // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
    // pixel alignment for this stuff!
    tmpctx->NewPath();
    tmpctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, w, h), pat);
    tmpctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpctx->Fill();

    tmpctx = nsnull;
    tmpsurf = nsnull;

    PRUint32 len = w * h * 4;
    if (len > (((PRUint32)0xfff00000)/sizeof(jsval)))
        return NS_ERROR_INVALID_ARG;

    nsAutoArrayPtr<jsval> jsvector(new (std::nothrow) jsval[w * h * 4]);
    if (!jsvector)
        return NS_ERROR_OUT_OF_MEMORY;
    jsval *dest = jsvector.get();
    PRUint8 *row;
    for (PRUint32 j = 0; j < h; j++) {
        row = surfaceData + surfaceDataOffset + (surfaceDataStride * j);
        for (PRUint32 i = 0; i < w; i++) {
            // XXX Is there some useful swizzle MMX we can use here?
            // I guess we have to INT_TO_JSVAL still
#ifdef IS_LITTLE_ENDIAN
            PRUint8 b = *row++;
            PRUint8 g = *row++;
            PRUint8 r = *row++;
            PRUint8 a = *row++;
#else
            PRUint8 a = *row++;
            PRUint8 r = *row++;
            PRUint8 g = *row++;
            PRUint8 b = *row++;
#endif
            // Convert to non-premultiplied color
            if (a != 0) {
                r = (r * 255) / a;
                g = (g * 255) / a;
                b = (b * 255) / a;
            }

            *dest++ = INT_TO_JSVAL(r);
            *dest++ = INT_TO_JSVAL(g);
            *dest++ = INT_TO_JSVAL(b);
            *dest++ = INT_TO_JSVAL(a);
        }
    }

    JSObject *dataArray = JS_NewArrayObject(ctx, w*h*4, jsvector);
    if (!dataArray)
        return NS_ERROR_OUT_OF_MEMORY;

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("width", w);
    retobj.DefineProperty("height", h);
    retobj.DefineProperty("data", dataArray);

    js.SetRetVal(retobj);

    return NS_OK;
#endif
}
#endif

PRBool
BaseTypeAndSizeFromUniformType(WebGLenum uType, WebGLenum *baseType, WebGLint *unitSize)
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
            return PR_FALSE;
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
            return PR_FALSE;
    }

    return PR_TRUE;
}
