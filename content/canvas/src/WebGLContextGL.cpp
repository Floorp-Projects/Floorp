/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "SimpleBuffer.h"

using namespace mozilla;

// XXX why is this broken?
#ifndef GL_BLEND_EQUATION
#define GL_BLEND_EQUATION 0x8009
#endif

static PRBool BaseTypeAndSizeFromUniformType(GLenum uType, GLenum *baseType, GLint *unitSize);

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

//
//  WebGL API
//

/* nsIWebGLFloatArray createFloatArray (); */
NS_IMETHODIMP
WebGLContext::CreateFloatArray(nsIWebGLFloatArray **retval)
{
    NativeJSContext js;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;

    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "o", &arrayObj) ||
        arrayObj == NULL ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer(LOCAL_GL_FLOAT, 1, js.ctx, arrayObj, arrayLen);

    WebGLFloatArray *wgfa = new WebGLFloatArray(js.ctx, arrayObj, arrayLen);
    if (wgfa)
        NS_ADDREF(*retval = wgfa);

    return NS_OK;
}

/* nsIWebGLShortArray createShortArray (); */
NS_IMETHODIMP
WebGLContext::CreateShortArray(nsIWebGLShortArray **retval)
{
    NativeJSContext js;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;

    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "o", &arrayObj) ||
        arrayObj == NULL ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer(LOCAL_GL_SHORT, 1, js.ctx, arrayObj, arrayLen);

    WebGLShortArray *wgsa = new WebGLShortArray(js.ctx, arrayObj, arrayLen);
    if (wgsa)
        NS_ADDREF(*retval = wgsa);
    return NS_OK;
}

/* nsIWebGLUnsignedShortArray createUnsignedShortArray (); */
NS_IMETHODIMP WebGLContext::CreateUnsignedShortArray(nsIWebGLUnsignedShortArray **retval)
{
    NativeJSContext js;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;

    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "o", &arrayObj) ||
        arrayObj == NULL ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer(LOCAL_GL_UNSIGNED_SHORT, 1, js.ctx, arrayObj, arrayLen);

    WebGLUnsignedShortArray *wgusa = new WebGLUnsignedShortArray(js.ctx, arrayObj, arrayLen);
    if (wgusa)
        NS_ADDREF(*retval = wgusa);
    return NS_OK;
}

/* nsIWebGLUnsignedByteArray createUnsignedByteArray (); */
NS_IMETHODIMP
WebGLContext::CreateUnsignedByteArray(nsIWebGLUnsignedByteArray **retval)
{
    NativeJSContext js;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;

    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "o", &arrayObj) ||
        arrayObj == NULL ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer(LOCAL_GL_UNSIGNED_SHORT, 1, js.ctx, arrayObj, arrayLen);

    WebGLUnsignedByteArray *wguba = new WebGLUnsignedByteArray(js.ctx, arrayObj, arrayLen);
    if (wguba)
        NS_ADDREF(*retval = wguba);
    return NS_OK;
}

/* readonly attribute nsIDOMHTMLCanvasElement canvas; */
NS_IMETHODIMP
WebGLContext::GetCanvas(nsIDOMHTMLCanvasElement * *aCanvas)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void GlActiveTexture (in PRUint32 texture); */
NS_IMETHODIMP
WebGLContext::ActiveTexture(PRUint32 texture)
{
    if (texture < LOCAL_GL_TEXTURE0 || texture >= LOCAL_GL_TEXTURE0+mBound2DTextures.Length())
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    mActiveTexture = texture - LOCAL_GL_TEXTURE0;
    gl->fActiveTexture(texture);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::AttachShader(nsIWebGLProgram *prog, nsIWebGLShader *sh)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    if (!sh || static_cast<WebGLShader*>(sh)->Deleted())
        return ErrorMessage("%s: shader is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();
    GLuint shader = static_cast<WebGLShader*>(sh)->GLName();

    MakeContextCurrent();

    gl->fAttachShader(program, shader);

    return NS_OK;
}


NS_IMETHODIMP
WebGLContext::BindAttribLocation(nsIWebGLProgram *prog, GLuint location, const nsAString& name)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    if (name.IsEmpty())
        return ErrorMessage("glBindAttribLocation: name can't be null or empty!");

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    MakeContextCurrent();

    gl->fBindAttribLocation(program, location, NS_LossyConvertUTF16toASCII(name).get());

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindBuffer(GLenum target, nsIWebGLBuffer *buffer)
{
    WebGLBuffer *wbuf = static_cast<WebGLBuffer*>(buffer);

    if (wbuf && wbuf->Deleted())
        return ErrorMessage("glBindBuffer: buffer has already been deleted!");

    MakeContextCurrent();

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        mBoundArrayBuffer = wbuf;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        mBoundElementArrayBuffer = wbuf;
    } else {
        return ErrorMessage("glBindBuffer: invalid target!");
    }

    gl->fBindBuffer(target, wbuf ? wbuf->GLName() : 0);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindFramebuffer(GLenum target, nsIWebGLFramebuffer *fb)
{
    WebGLFramebuffer *wfb = static_cast<WebGLFramebuffer*>(fb);

    if (wfb && wfb->Deleted())
        return ErrorMessage("glBindFramebuffer: framebuffer has already been deleted!");

    MakeContextCurrent();

    if (target >= LOCAL_GL_COLOR_ATTACHMENT0 &&
        target < (LOCAL_GL_COLOR_ATTACHMENT0 + mBoundColorFramebuffers.Length()))
    {
        mBoundColorFramebuffers[target] = wfb;
    } else if (target == LOCAL_GL_DEPTH_ATTACHMENT) {
        mBoundDepthFramebuffer = wfb;
    } else if (target == LOCAL_GL_STENCIL_ATTACHMENT) {
        mBoundStencilFramebuffer = wfb;
    } else {
        return ErrorMessage("glBindFramebuffer: invalid target");
    }

    gl->fBindFramebuffer(target, wfb ? wfb->GLName() : 0);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindRenderbuffer(GLenum target, nsIWebGLRenderbuffer *rb)
{
    WebGLRenderbuffer *wrb = static_cast<WebGLRenderbuffer*>(rb);

    if (wrb && wrb->Deleted())
        return ErrorMessage("glBindRenderbuffer: renderbuffer has already been deleted!");

    if (target != LOCAL_GL_RENDERBUFFER)
        return ErrorMessage("glBindRenderbuffer: invalid target");

    MakeContextCurrent();

    gl->fBindRenderbuffer(target, wrb ? wrb->GLName() : 0);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BindTexture(GLenum target, nsIWebGLTexture *tex)
{
    WebGLTexture *wtex = static_cast<WebGLTexture*>(tex);

    if (wtex && wtex->Deleted())
        return ErrorMessage("glBindTexture: texture has already been deleted!");

    MakeContextCurrent();

    if (target == LOCAL_GL_TEXTURE_2D) {
        mBound2DTextures[mActiveTexture] = wtex;
    } else if (target == LOCAL_GL_TEXTURE_CUBE_MAP) {
        mBoundCubeMapTextures[mActiveTexture] = wtex;
    } else {
        return ErrorMessage("glBindTexture: invalid target");
    }

    gl->fBindTexture(target, wtex ? wtex->GLName() : 0);

    return NS_OK;
}

GL_SAME_METHOD_4(BlendColor, BlendColor, float, float, float, float)

GL_SAME_METHOD_1(BlendEquation, BlendEquation, PRUint32)

GL_SAME_METHOD_2(BlendEquationSeparate, BlendEquationSeparate, PRUint32, PRUint32)

GL_SAME_METHOD_2(BlendFunc, BlendFunc, PRUint32, PRUint32)

GL_SAME_METHOD_4(BlendFuncSeparate, BlendFuncSeparate, PRUint32, PRUint32, PRUint32, PRUint32)

NS_IMETHODIMP
WebGLContext::BufferData(GLenum target, nsIWebGLNumberArray *na, GLenum usage)
{
    WebGLBuffer *boundBuffer = NULL;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        if (na->NativeType() != LOCAL_GL_UNSIGNED_SHORT)
            return ErrorMessage("glBufferDatA: GL_ELEMENT_ARRAY_BUFFER target must be used with UnsignedShortBuffer");

        boundBuffer = mBoundElementArrayBuffer;
    } else {
        return ErrorMessage("glBufferData: invalid target");
    }

    if (boundBuffer == NULL) {
        return ErrorMessage("glBufferData: no buffer bound!");
    }

    MakeContextCurrent();

    boundBuffer->Set(na);

    gl->fBufferData(target, na->NativeSize(), na->NativePointer(), usage);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::BufferSubData(GLenum target, GLuint offset, nsIWebGLNumberArray *na)
{
    WebGLBuffer *boundBuffer = NULL;

    if (target == LOCAL_GL_ARRAY_BUFFER) {
        boundBuffer = mBoundArrayBuffer;
    } else if (target == LOCAL_GL_ELEMENT_ARRAY_BUFFER) {
        boundBuffer = mBoundElementArrayBuffer;
    } else {
        return ErrorMessage("glBufferSubData: invalid target");
    }

    if (boundBuffer == NULL) {
        return ErrorMessage("glBufferSubData: no buffer bound!");
    }

    // check type
    if (na->NativeType() != boundBuffer->GLType()) {
        return ErrorMessage("glBufferSubData: existing buffer has different base type (0x%04x) the sub data (0x%04x)!", boundBuffer->GLType(), na->NativeType());
        return NS_ERROR_FAILURE;
    }

    // check size
    if ((offset + na->NativeCount()) > boundBuffer->Count()) {
        return ErrorMessage("glBufferSubData: existing buffer is too small for additional data");
        return NS_ERROR_FAILURE;
    }

    // all good

    MakeContextCurrent();

    gl->fBufferSubData(target, offset * na->NativeElementSize(), na->NativeSize(), na->NativePointer());

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CheckFramebufferStatus(GLenum target, GLenum *retval)
{
    MakeContextCurrent();
    // XXX check target
    *retval = gl->fCheckFramebufferStatus(target);
    return NS_OK;
}

GL_SAME_METHOD_1(Clear, Clear, PRUint32)

GL_SAME_METHOD_4(ClearColor, ClearColor, float, float, float, float)

// XXX the Web IDL method should be glClearDepth!
#ifdef USE_GLES2
GL_SAME_METHOD_1(ClearDepthf, ClearDepthf, float)
#else
GL_SAME_METHOD_1(ClearDepth, ClearDepthf, float)
#endif

GL_SAME_METHOD_1(ClearStencil, ClearStencil, PRInt32)

GL_SAME_METHOD_4(ColorMask, ColorMask, GLboolean, GLboolean, GLboolean, GLboolean)

NS_IMETHODIMP
WebGLContext::CopyTexImage2D(GLenum target,
                               GLint level,
                               GLenum internalformat,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLint border)
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
            return ErrorMessage("copyTexImage2D: unsupported target");
    }

    switch (internalformat) {
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_LUMINANCE_ALPHA:
            break;
        default:
            return ErrorMessage("copyTexImage2D: internal format not supported");
    }

    if (border != 0) {
        return ErrorMessage("copyTexImage2D: border != 0");
    }

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width, height, mWidth, mHeight)) {
        return ErrorMessage("copyTexImage2D: copied rectangle out of bounds");
    }

    MakeContextCurrent();

    gl->fCopyTexImage2D(target, level, internalformat, x, y, width, height, border);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CopyTexSubImage2D(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height)
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
            return ErrorMessage("copyTexSubImage2D: unsupported target");
    }

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width, height, mWidth, mHeight)) {
        return ErrorMessage("copyTexSubImage2D: copied rectangle out of bounds");
    }

    MakeContextCurrent();

    gl->fCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

    return NS_OK;
}


NS_IMETHODIMP
WebGLContext::CreateProgram(nsIWebGLProgram **retval)
{
    MakeContextCurrent();

    GLuint name = gl->fCreateProgram();

    WebGLProgram *prog = new WebGLProgram(name);
    if (prog) {
        NS_ADDREF(*retval = prog);
        mMapPrograms.Put(name, prog);
    } else {
        gl->fDeleteProgram(name);
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::CreateShader(GLenum type, nsIWebGLShader **retval)
{
    MakeContextCurrent();

    GLuint name = gl->fCreateShader(type);

    WebGLShader *shader = new WebGLShader(name);
    if (shader) {
        NS_ADDREF(*retval = shader);
        mMapShaders.Put(name, shader);
    } else {
        gl->fDeleteShader(name);
    }

    return NS_OK;
}

GL_SAME_METHOD_1(CullFace, CullFace, GLenum)

NS_IMETHODIMP
WebGLContext::DeleteBuffer(nsIWebGLBuffer *globj)
{
    WebGLBuffer *obj = static_cast<WebGLBuffer*>(globj);
    if (!obj || obj->Deleted()) {
        return NS_OK;
    }

    MakeContextCurrent();

    GLuint name = obj->GLName();
    gl->fDeleteBuffers(1, &name);
    obj->Delete();
    mMapBuffers.Remove(name);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteFramebuffer(nsIWebGLFramebuffer *globj)
{
    WebGLFramebuffer *obj = static_cast<WebGLFramebuffer*>(globj);
    if (!obj || obj->Deleted()) {
        return NS_OK;
    }

    MakeContextCurrent();

    GLuint name = obj->GLName();
    gl->fDeleteFramebuffers(1, &name);
    obj->Delete();
    mMapFramebuffers.Remove(name);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteRenderbuffer(nsIWebGLRenderbuffer *globj)
{
    WebGLRenderbuffer *obj = static_cast<WebGLRenderbuffer*>(globj);
    if (!obj || obj->Deleted()) {
        return NS_OK;
    }

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

    GLuint name = obj->GLName();
    gl->fDeleteRenderbuffers(1, &name);
    obj->Delete();
    mMapRenderbuffers.Remove(name);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteTexture(nsIWebGLTexture *globj)
{
    WebGLTexture *obj = static_cast<WebGLTexture*>(globj);
    if (!obj || obj->Deleted()) {
        return NS_OK;
    }

    MakeContextCurrent();

    GLuint name = obj->GLName();
    gl->fDeleteTextures(1, &name);
    obj->Delete();
    mMapTextures.Remove(name);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteProgram(nsIWebGLProgram *globj)
{
    WebGLProgram *obj = static_cast<WebGLProgram*>(globj);
    if (!obj || obj->Deleted()) {
        return NS_OK;
    }

    MakeContextCurrent();

    GLuint name = obj->GLName();
    gl->fDeleteProgram(name);
    obj->Delete();
    mMapPrograms.Remove(name);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DeleteShader(nsIWebGLShader *globj)
{
    WebGLShader *obj = static_cast<WebGLShader*>(globj);
    if (!obj || obj->Deleted()) {
        return NS_OK;
    }

    MakeContextCurrent();

    GLuint name = obj->GLName();
    gl->fDeleteShader(name);
    obj->Delete();
    mMapShaders.Remove(name);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DetachShader(nsIWebGLProgram *prog, nsIWebGLShader *sh)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    if (!sh || static_cast<WebGLShader*>(sh)->Deleted())
        return ErrorMessage("%s: shader is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();
    GLuint shader = static_cast<WebGLShader*>(sh)->GLName();

    MakeContextCurrent();

    gl->fDetachShader(program, shader);

    return NS_OK;
}

GL_SAME_METHOD_1(DepthFunc, DepthFunc, GLenum)

GL_SAME_METHOD_1(DepthMask, DepthMask, GLboolean)

// XXX should just be glDepthRange in webgl!
#ifdef USE_GLES2
GL_SAME_METHOD_2(DepthRangef, DepthRangef, float, float)
#else
GL_SAME_METHOD_2(DepthRange, DepthRangef, float, float)
#endif

// XXX arg check!
GL_SAME_METHOD_1(Disable, Disable, GLenum)

NS_IMETHODIMP
WebGLContext::DisableVertexAttribArray(GLuint index)
{
    if (index > mAttribBuffers.Length())
        return ErrorMessage("glDisableVertexAttribArray: index out of range");

    MakeContextCurrent();

    gl->fDisableVertexAttribArray(index);
    mAttribBuffers[index].enabled = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::DrawArrays(GLenum mode, GLint offset, GLsizei count)
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
            return ErrorMessage("drawArrays: invalid mode");
    }

    if (offset+count < offset || offset+count < count) {
        return ErrorMessage("drawArrays: overflow in offset+count");
    }

    if (!ValidateBuffers(offset+count))
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();

    gl->fDrawArrays(mode, offset, count);

    Invalidate();

    return NS_OK;
}

// XXX why are count & type swapped?
NS_IMETHODIMP
WebGLContext::DrawElements(GLenum mode, GLenum type, GLuint count, GLuint offset)
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
            return ErrorMessage("drawElements: invalid mode");
    }

    switch (type) {
        case LOCAL_GL_UNSIGNED_SHORT:
            break;
        default:
            return ErrorMessage("drawElements: type must be UNSIGNED_SHORT");
    }

    if (!mBoundElementArrayBuffer)
        return ErrorMessage("glDrawElements: must have element array buffer binding!");

    if (offset+count < offset || offset+count < count)
        return ErrorMessage("glDrawElements: overflow in offset+count");

    if (count + offset > mBoundElementArrayBuffer->Count())
        return ErrorMessage("glDrawElements: bound element array buffer is too small for given count and offset");

    MakeContextCurrent();

    // XXXmark fix validation
#if 0
    GLuint maxindex = 0;
    GLushort *ubuf = (GLushort*) gl->fMapBuffer(LOCAL_GL_ELEMENT_ARRAY_BUFFER, LOCAL_GL_READ_ONLY);
    if (!ubuf)
        return ErrorMessage("glDrawElements: failed to map ELEMENT_ARRAY_BUFFER for validation!");

    ubuf += offset;

    // XXX cache results for this count,offset pair!
    for (PRUint32 i = 0; i < count; ++i)
        maxindex = PR_MAX(maxindex, *ubuf++);

    gl->fUnmapBuffer(LOCAL_GL_ELEMENT_ARRAY_BUFFER);

    if (!ValidateBuffers(maxindex))
        return ErrorMessage("glDrawElements: ValidateBuffers failed");
#endif
    // XXX uh, is this offset, or offset * elementsize?
    gl->fDrawElements(mode, count, type, (GLvoid*) (offset * mBoundElementArrayBuffer->ElementSize()));

    Invalidate();

    return NS_OK;
}

// XXX definitely need to validate this
GL_SAME_METHOD_1(Enable, Enable, PRUint32)

NS_IMETHODIMP
WebGLContext::EnableVertexAttribArray(GLuint index)
{
    if (index > mAttribBuffers.Length())
        return ErrorMessage("glEnableVertexAttribArray: index out of range");

    MakeContextCurrent();

    gl->fEnableVertexAttribArray(index);
    mAttribBuffers[index].enabled = PR_TRUE;

    return NS_OK;
}

// XXX need to track this -- see glDeleteRenderbuffer above and man page for DeleteRenderbuffers
NS_IMETHODIMP
WebGLContext::FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, nsIWebGLRenderbuffer *wrb)
{
    WebGLRenderbuffer *rb = static_cast<WebGLRenderbuffer*>(wrb);

    if (rb && rb->Deleted())
        return ErrorMessage("glFramebufferRenderbuffer: renderbuffer has already been deleted!");

    if (target != LOCAL_GL_FRAMEBUFFER)
        return ErrorMessage("glFramebufferRenderbuffer: target must be LOCAL_GL_FRAMEBUFFER");

    if ((attachment < LOCAL_GL_COLOR_ATTACHMENT0 || attachment >= LOCAL_GL_COLOR_ATTACHMENT0 + mBoundColorFramebuffers.Length()) &&
        attachment != LOCAL_GL_DEPTH_ATTACHMENT &&
        attachment != LOCAL_GL_STENCIL_ATTACHMENT)
        return ErrorMessage("glFramebufferRenderbuffer: invalid attachment");

    if (rbtarget != LOCAL_GL_RENDERBUFFER)
        return ErrorMessage("glFramebufferRenderbuffer: rbtarget must be GL_RENDERBUFFER");

    GLuint name = rb ? rb->GLName() : 0;

    MakeContextCurrent();

    gl->fFramebufferRenderbuffer(target, attachment, rbtarget, name);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::FramebufferTexture2D(GLenum target,
                                     GLenum attachment,
                                     GLenum textarget,
                                     nsIWebGLTexture *wtex,
                                     GLint level)
{
    WebGLTexture *tex = static_cast<WebGLTexture*>(wtex);

    if (tex && tex->Deleted())
        return ErrorMessage("glFramebufferTexture2D: texture has already been deleted!");

    if (target != LOCAL_GL_FRAMEBUFFER)
        return ErrorMessage("glFramebufferTexture2D: target must be GL_FRAMEBUFFER");

    if ((attachment < LOCAL_GL_COLOR_ATTACHMENT0 || attachment >= LOCAL_GL_COLOR_ATTACHMENT0 + mBoundColorFramebuffers.Length()) &&
        attachment != LOCAL_GL_DEPTH_ATTACHMENT &&
        attachment != LOCAL_GL_STENCIL_ATTACHMENT)
        return ErrorMessage("glFramebufferTexture2D: invalid attachment");

    if (textarget != LOCAL_GL_TEXTURE_2D &&
        (textarget < LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
         textarget > LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
        return ErrorMessage("glFramebufferTexture2D: invalid textarget (only 2D or cube face)");

    if (level != 0)
        return ErrorMessage("glFramebufferTexture2D: level must be 0");

    MakeContextCurrent();

    gl->fFramebufferTexture2D(target, attachment, textarget, tex->GLName(), level);

    return NS_OK;
}

GL_SAME_METHOD_0(Flush, Flush)

GL_SAME_METHOD_0(Finish, Finish)

GL_SAME_METHOD_1(FrontFace, FrontFace, GLenum)

GL_SAME_METHOD_1(GenerateMipmap, GenerateMipmap, GLenum)

// returns an object: { size: ..., type: ..., name: ... }
NS_IMETHODIMP
WebGLContext::GetActiveAttrib(nsIWebGLProgram *prog, PRUint32 index)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint len = 0;
    gl->fGetProgramiv(program, LOCAL_GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &len);
    if (len == 0)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<char> name(new char[len+1]);
    PRInt32 attrsize = 0;
    PRUint32 attrtype = 0;

    gl->fGetActiveAttrib(program, index, len+1, &len, (GLint*) &attrsize, (GLuint*) &attrtype, name);
    if (attrsize == 0 || attrtype == 0)
        return NS_ERROR_FAILURE;

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("size", attrsize);
    retobj.DefineProperty("type", attrtype);
    retobj.DefineProperty("name", name, len);

    js.SetRetVal(retobj);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetActiveUniform(nsIWebGLProgram *prog, PRUint32 index)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint len = 0;
    gl->fGetProgramiv(program, LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH, &len);
    if (len == 0)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<char> name(new char[len+1]);
    PRInt32 attrsize = 0;
    PRUint32 attrtype = 0;

    gl->fGetActiveUniform(program, index, len+1, &len, (GLint*) &attrsize, (GLenum*) &attrtype, name);
    if (attrsize == 0 || attrtype == 0)
        return NS_ERROR_FAILURE;

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("size", attrsize);
    retobj.DefineProperty("type", attrtype);
    retobj.DefineProperty("name", name, len);

    js.SetRetVal(retobj.Object());

    return NS_OK;
}

// XXX fixme to return a IntArray
#if 0
NS_IMETHODIMP
WebGLContext::GetAttachedShaders(nsIWebGLProgram *prog)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint count = 0;
    gl->fGetProgramiv(program, LOCAL_GL_ATTACHED_SHADERS, &count);
    if (count == 0) {
        JSObject *empty = JS_NewArrayObject(js.ctx, 0, NULL);
        js.SetRetVal(empty);
        return NS_OK;
    }

    nsAutoArrayPtr<PRUint32> shaders(new PRUint32[count]);

    gl->fGetAttachedShaders(program, count, NULL, (GLuint*) shaders.get());

    JSObject *obj = NativeJSContext::ArrayToJSArray(js.ctx, shaders, count);

    js.AddGCRoot(obj, "GetAttachedShaders");
    js.SetRetVal(obj);
    js.ReleaseGCRoot(obj);

    return NS_OK;
}
#endif

NS_IMETHODIMP
WebGLContext::GetAttribLocation(nsIWebGLProgram *prog,
                                  const nsAString& name,
                                  PRInt32 *retval)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    MakeContextCurrent();
    *retval = gl->fGetAttribLocation(program, NS_LossyConvertUTF16toASCII(name).get());
    return NS_OK;
}

// XXX fixme to return objects correctly for programs/etc.
NS_IMETHODIMP
WebGLContext::GetParameter(PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        //
        // String params
        //

        // XXX do we want to fake these?  Could be a problem to reveal this to web content
        case LOCAL_GL_VENDOR:
        case LOCAL_GL_RENDERER:
        case LOCAL_GL_VERSION:
        case LOCAL_GL_SHADING_LANGUAGE_VERSION:
        //case LOCAL_GL_EXTENSIONS:  // Not going to expose this

            break;

        //
        // Single-value params
        //

// int
        case LOCAL_GL_ARRAY_BUFFER_BINDING:
        case LOCAL_GL_ELEMENT_ARRAY_BUFFER_BINDING: // XXX really?
        case LOCAL_GL_CULL_FACE_MODE:
        case LOCAL_GL_FRONT_FACE:
        case LOCAL_GL_TEXTURE_BINDING_2D:
        case LOCAL_GL_TEXTURE_BINDING_CUBE_MAP:
        case LOCAL_GL_ACTIVE_TEXTURE:
        case LOCAL_GL_STENCIL_WRITEMASK:
        case LOCAL_GL_STENCIL_BACK_WRITEMASK:
        case LOCAL_GL_DEPTH_CLEAR_VALUE:
        case LOCAL_GL_STENCIL_CLEAR_VALUE:
        case LOCAL_GL_STENCIL_FUNC:
        case LOCAL_GL_STENCIL_VALUE_MASK:
        case LOCAL_GL_STENCIL_REF:
        case LOCAL_GL_STENCIL_FAIL:
        case LOCAL_GL_STENCIL_PASS_DEPTH_FAIL:
        case LOCAL_GL_STENCIL_PASS_DEPTH_PASS:
        case LOCAL_GL_STENCIL_BACK_FUNC:
        case LOCAL_GL_STENCIL_BACK_VALUE_MASK:
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
        //case LOCAL_GL_UNPACK_ALIGNMENT: // not supported
        //case LOCAL_GL_PACK_ALIGNMENT: // not supported
        case LOCAL_GL_CURRENT_PROGRAM:
        case LOCAL_GL_GENERATE_MIPMAP_HINT:
        case LOCAL_GL_SUBPIXEL_BITS:
        case LOCAL_GL_MAX_TEXTURE_SIZE:
        case LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        case LOCAL_GL_MAX_ELEMENTS_INDICES:
        case LOCAL_GL_MAX_ELEMENTS_VERTICES:
        case LOCAL_GL_SAMPLE_BUFFERS:
        case LOCAL_GL_SAMPLES:
        //case LOCAL_GL_COMPRESSED_TEXTURE_FORMATS:
        //case LOCAL_GL_NUM_COMPRESSED_TEXTURE_FORMATS:
        //case LOCAL_GL_SHADER_BINARY_FORMATS:
        //case LOCAL_GL_NUM_SHADER_BINARY_FORMATS:
        case LOCAL_GL_MAX_VERTEX_ATTRIBS:
        case LOCAL_GL_MAX_VERTEX_UNIFORM_COMPONENTS:
        case LOCAL_GL_MAX_VARYING_FLOATS:
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
        //case LOCAL_GL_IMPLEMENTATION_COLOR_READ_TYPE:
        //case LOCAL_GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        case LOCAL_GL_RENDERBUFFER_BINDING:
        case LOCAL_GL_FRAMEBUFFER_BINDING:
        {
            PRInt32 iv = 0;
            gl->fGetIntegerv(pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

// float
        case LOCAL_GL_LINE_WIDTH:
        case LOCAL_GL_POLYGON_OFFSET_FACTOR:
        case LOCAL_GL_POLYGON_OFFSET_UNITS:
        case LOCAL_GL_SAMPLE_COVERAGE_VALUE:
        {
            float fv = 0;
            gl->fGetFloatv(pname, &fv);
            js.SetRetVal((double) fv);
        }
            break;
// bool
        case LOCAL_GL_SAMPLE_COVERAGE_INVERT:
        case LOCAL_GL_COLOR_WRITEMASK:
        case LOCAL_GL_DEPTH_WRITEMASK:
        ////case LOCAL_GL_SHADER_COMPILER: // pretty much must be true 
        {
            realGLboolean bv = 0;
            gl->fGetBooleanv(pname, &bv);
            js.SetBoolRetVal(bv);
        }
            break;

        //
        // Complex values
        //
        case LOCAL_GL_DEPTH_RANGE: // 2 floats
        case LOCAL_GL_ALIASED_POINT_SIZE_RANGE: // 2 floats
        case LOCAL_GL_ALIASED_LINE_WIDTH_RANGE: // 2 floats
        {
            float fv[2] = { 0 };
            gl->fGetFloatv(pname, &fv[0]);
            js.SetRetVal(fv, 2);
        }
            break;
        
        case LOCAL_GL_COLOR_CLEAR_VALUE: // 4 floats
        case LOCAL_GL_BLEND_COLOR: // 4 floats
        {
            float fv[4] = { 0 };
            gl->fGetFloatv(pname, &fv[0]);
            js.SetRetVal(fv, 4);
        }
            break;

        case LOCAL_GL_MAX_VIEWPORT_DIMS: // 2 ints
        {
            PRInt32 iv[2] = { 0 };
            gl->fGetIntegerv(pname, (GLint*) &iv[0]);
            js.SetRetVal(iv, 2);
        }
            break;

        case LOCAL_GL_SCISSOR_BOX: // 4 ints
        case LOCAL_GL_VIEWPORT: // 4 ints
        {
            PRInt32 iv[4] = { 0 };
            gl->fGetIntegerv(pname, (GLint*) &iv[0]);
            js.SetRetVal(iv, 4);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetBufferParameter(GLenum target, GLenum pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_BUFFER_SIZE:
        case LOCAL_GL_BUFFER_USAGE:
        case LOCAL_GL_BUFFER_ACCESS:
        case LOCAL_GL_BUFFER_MAPPED:
        {
            PRInt32 iv = 0;
            gl->fGetBufferParameteriv(target, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetFramebufferAttachmentParameter(GLenum target, GLenum attachment, GLenum pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (attachment) {
        case LOCAL_GL_COLOR_ATTACHMENT0:
        case LOCAL_GL_DEPTH_ATTACHMENT:
        case LOCAL_GL_STENCIL_ATTACHMENT:
            break;
        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    switch (pname) {
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        {
            PRInt32 iv = 0;
            gl->fGetFramebufferAttachmentParameteriv(target, attachment, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetRenderbufferParameter(GLenum target, GLenum pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

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
            PRInt32 iv = 0;
            gl->fGetRenderbufferParameteriv(target, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GenBuffer(nsIWebGLBuffer **retval)
{
    MakeContextCurrent();

    GLuint name;
    gl->fGenBuffers(1, &name);

    WebGLBuffer *globj = new WebGLBuffer(name);
    if (globj) {
        NS_ADDREF(*retval = globj);
        mMapBuffers.Put(name, globj);
    } else {
        gl->fDeleteBuffers(1, &name);
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GenTexture(nsIWebGLTexture **retval)
{
    MakeContextCurrent();

    GLuint name;
    gl->fGenTextures(1, &name);

    WebGLTexture *globj = new WebGLTexture(name);
    if (globj) {
        NS_ADDREF(*retval = globj);
        mMapTextures.Put(name, globj);
    } else {
        gl->fDeleteTextures(1, &name);
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetError(GLenum *_retval)
{
    MakeContextCurrent();
    *_retval = gl->fGetError();
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetProgramParameter(nsIWebGLProgram *prog, PRUint32 pname)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_CURRENT_PROGRAM:
        case LOCAL_GL_DELETE_STATUS:
        case LOCAL_GL_LINK_STATUS:
        case LOCAL_GL_VALIDATE_STATUS:
        case LOCAL_GL_ATTACHED_SHADERS:
        case LOCAL_GL_INFO_LOG_LENGTH:
        case LOCAL_GL_ACTIVE_UNIFORMS:
        case LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH:
        case LOCAL_GL_ACTIVE_ATTRIBUTES:
        case LOCAL_GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
        {
            PRInt32 iv = 0;
            gl->fGetProgramiv(program, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetProgramInfoLog(nsIWebGLProgram *prog, nsAString& retval)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!");

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    MakeContextCurrent();

    PRInt32 k = -1;
    gl->fGetProgramiv(program, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &k);
    if (k == -1)
        return NS_ERROR_FAILURE;

    if (k == 0) {
        retval.Truncate();
        return NS_OK;
    }

    nsCAutoString log;
    log.SetCapacity(k);

    gl->fGetProgramInfoLog(program, k, (GLint*) &k, (char*) log.BeginWriting());

    log.SetLength(k);

    CopyASCIItoUTF16(log, retval);

    return NS_OK;
}

/* DOMString glGetString (in GLenum name); */
NS_IMETHODIMP
WebGLContext::GetString(GLenum name, nsAString & _retval NS_OUTPARAM)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* XXX fix */
/* void texParameter (); */
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

/* XXX fix */
/* void getTexParameter (); */
NS_IMETHODIMP
WebGLContext::GetTexParameter(GLenum target, GLenum pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_TEXTURE_MIN_FILTER:
        case LOCAL_GL_TEXTURE_MAG_FILTER:
        case LOCAL_GL_TEXTURE_WRAP_S:
        case LOCAL_GL_TEXTURE_WRAP_T:
        {
            PRInt32 iv = 0;
            gl->fGetTexParameteriv(target, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

/* XXX fix */
NS_IMETHODIMP
WebGLContext::GetUniform(nsIWebGLProgram *prog, GLint location)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint uArraySize = 0;
    GLenum uType = 0;

    fprintf (stderr, "GetUniform: program: %d location: %d\n", program, location);
    gl->fGetActiveUniform(program, location, 0, NULL, &uArraySize, &uType, NULL);
    if (uArraySize == 0)
        return NS_ERROR_FAILURE;

    // glGetUniform needs to be called for each element of an array separately, so we don't
    // have to deal with uArraySize at all.

    GLenum baseType;
    GLint unitSize;
    if (!BaseTypeAndSizeFromUniformType(uType, &baseType, &unitSize))
        return NS_ERROR_FAILURE;

    // this should never happen
    if (unitSize > 16)
        return NS_ERROR_FAILURE;

    if (baseType == LOCAL_GL_FLOAT) {
        GLfloat fv[16];
        gl->fGetUniformfv(program, location, fv);
        js.SetRetVal(fv, unitSize);
    } else if (baseType == LOCAL_GL_INT) {
        GLint iv[16];
        gl->fGetUniformiv(program, location, iv);
        js.SetRetVal((PRInt32*)iv, unitSize);
    } else {
        js.SetRetValAsJSVal(JSVAL_NULL);
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetUniformLocation(nsIWebGLProgram *prog, const nsAString& name, GLint *retval)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    MakeContextCurrent();
    *retval = gl->fGetUniformLocation(program, NS_LossyConvertUTF16toASCII(name).get());
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetVertexAttrib(GLuint index, GLenum pname)
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
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_ENABLED:
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        {
            PRInt32 iv = 0;
            gl->fGetVertexAttribiv(index, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        case LOCAL_GL_CURRENT_VERTEX_ATTRIB:
        {
            GLfloat fv[4] = { 0 };
            gl->fGetVertexAttribfv(index, LOCAL_GL_CURRENT_VERTEX_ATTRIB, &fv[0]);
            js.SetRetVal(fv, 4);
        }
            break;

        // not supported; doesn't make sense to return a pointer unless we have some kind of buffer object abstraction
        case LOCAL_GL_VERTEX_ATTRIB_ARRAY_POINTER:
        default:
            return NS_ERROR_NOT_IMPLEMENTED;

    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Hint(GLenum target, GLenum mode)
{
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsBuffer(nsIWebGLBuffer *iobj, GLboolean *retval)
{
    if (!iobj)
        return NS_ERROR_FAILURE;

    *retval = ! static_cast<WebGLBuffer*>(iobj)->Deleted();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsFramebuffer(nsIWebGLFramebuffer *iobj, GLboolean *retval)
{
    if (!iobj)
        return NS_ERROR_FAILURE;

    *retval = ! static_cast<WebGLFramebuffer*>(iobj)->Deleted();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsProgram(nsIWebGLProgram *iobj, GLboolean *retval)
{
    if (!iobj)
        return NS_ERROR_FAILURE;

    *retval = ! static_cast<WebGLProgram*>(iobj)->Deleted();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsRenderbuffer(nsIWebGLRenderbuffer *iobj, GLboolean *retval)
{
    if (!iobj)
        return NS_ERROR_FAILURE;

    *retval = ! static_cast<WebGLRenderbuffer*>(iobj)->Deleted();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsShader(nsIWebGLShader *iobj, GLboolean *retval)
{
    if (!iobj)
        return NS_ERROR_FAILURE;

    *retval = ! static_cast<WebGLShader*>(iobj)->Deleted();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsTexture(nsIWebGLTexture *iobj, GLboolean *retval)
{
    if (!iobj)
        return NS_ERROR_FAILURE;

    *retval = ! static_cast<WebGLTexture*>(iobj)->Deleted();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::IsEnabled(GLenum k, GLboolean *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsEnabled(k);
    return NS_OK;
}


GL_SAME_METHOD_1(LineWidth, LineWidth, float)

NS_IMETHODIMP
WebGLContext::LinkProgram(nsIWebGLProgram *prog)
{
    if (!prog || static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("%s: program is null or deleted!", __FUNCTION__);

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    MakeContextCurrent();

    gl->fLinkProgram(program);

    return NS_OK;
}

// XXX #if 0
NS_IMETHODIMP
WebGLContext::PixelStorei(GLenum pname, GLint param)
{
    if (pname != LOCAL_GL_PACK_ALIGNMENT &&
        pname != LOCAL_GL_UNPACK_ALIGNMENT)
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();
    gl->fPixelStorei(pname, param);

    return NS_OK;
}
//#endif

GL_SAME_METHOD_2(PolygonOffset, PolygonOffset, float, float)

NS_IMETHODIMP
WebGLContext::ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (mCanvasElement->IsWriteOnly() && !nsContentUtils::IsCallerTrustedForRead()) {
        LogMessage("readPixels: Not allowed");
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    MakeContextCurrent();

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width,height, mWidth, mHeight))
        return ErrorMessage("readPixels: rectangle outside canvas");

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
        return ErrorMessage("readPixels: unsupported pixel format");
    }
    switch (type) {
//         case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
//         case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
//         case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
      case LOCAL_GL_UNSIGNED_BYTE:
        break;
      default:
        return ErrorMessage("readPixels: unsupported pixel type");
    }

    PRUint32 len = width*height*size;

    nsAutoArrayPtr<PRUint8> data(new PRUint8[len]);
    gl->fReadPixels((GLint)x, (GLint)y, width, height, format, type, (GLvoid *)data.get());

    nsAutoArrayPtr<jsval> jsvector(new jsval[len]);
    for (PRUint32 i = 0; i < len; i++)
        jsvector[i] = INT_TO_JSVAL(data[i]);

    JSObject *dataArray = JS_NewArrayObject(js.ctx, len, jsvector);
    if (!dataArray)
        return NS_ERROR_OUT_OF_MEMORY;

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("width", width);
    retobj.DefineProperty("height", height);
    retobj.DefineProperty("data", dataArray);

    js.SetRetVal(retobj);

    return NS_OK;
}

GL_SAME_METHOD_4(RenderbufferStorage, RenderbufferStorage, GLenum, GLenum, GLsizei, GLsizei)

GL_SAME_METHOD_2(SampleCoverage, SampleCoverage, float, GLboolean)

GL_SAME_METHOD_4(Scissor, Scissor, GLint, GLint, GLsizei, GLsizei)

GL_SAME_METHOD_3(StencilFunc, StencilFunc, GLenum, GLint, GLuint)

GL_SAME_METHOD_4(StencilFuncSeparate, StencilFuncSeparate, GLenum, GLenum, GLint, GLuint)

GL_SAME_METHOD_1(StencilMask, StencilMask, GLuint)

GL_SAME_METHOD_2(StencilMaskSeparate, StencilMaskSeparate, GLenum, GLuint)

GL_SAME_METHOD_3(StencilOp, StencilOp, GLenum, GLenum, GLenum)

GL_SAME_METHOD_4(StencilOpSeparate, StencilOpSeparate, GLenum, GLenum, GLenum, GLenum)


nsresult
WebGLContext::TexImageElementBase(nsIDOMHTMLElement *imageOrCanvas,
                                  gfxImageSurface **imageOut)
{
    gfxImageSurface *surf = nsnull;

    nsLayoutUtils::SurfaceFromElementResult res =
        nsLayoutUtils::SurfaceFromElement(imageOrCanvas,
                                          nsLayoutUtils::SFE_WANT_NEW_SURFACE | nsLayoutUtils::SFE_WANT_IMAGE_SURFACE);
    if (!res.mSurface)
        return NS_ERROR_FAILURE;

    CanvasUtils::DoDrawImageSecurityCheck(mCanvasElement, res.mPrincipal, res.mIsWriteOnly);

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

    if (surf->Format() == gfxASurface::ImageFormatARGB32) {
        PRUint8* src = surf->Data();
        PRUint8* dst = surf->Data();

        // this wants some SSE love

        for (int j = 0; j < height; j++) {
            src = surf->Data() + j * surf->Stride();
            // note that dst's stride is always tightly packed
            for (int i = 0; i < width; i++) {
#ifdef IS_LITTLE_ENDIAN
                PRUint8 b = *src++;
                PRUint8 g = *src++;
                PRUint8 r = *src++;
                PRUint8 a = *src++;
#else
                PRUint8 a = *src++;
                PRUint8 r = *src++;
                PRUint8 g = *src++;
                PRUint8 b = *src++;
#endif
                // Convert to non-premultiplied color
                if (a != 0) {
                    r = (r * 255) / a;
                    g = (g * 255) / a;
                    b = (b * 255) / a;
                }

                *dst++ = r;
                *dst++ = g;
                *dst++ = b;
                *dst++ = a;
            }
        }
    } else if (surf->Format() == gfxASurface::ImageFormatRGB24) {
        PRUint8* src = surf->Data();
        PRUint8* dst = surf->Data();

        // this wants some SSE love

        for (int j = 0; j < height; j++) {
            src = surf->Data() + j * surf->Stride();
            // note that dst's stride is always tightly packed
            for (int i = 0; i < width; i++) {
#ifdef IS_LITTLE_ENDIAN
                PRUint8 b = *src++;
                PRUint8 g = *src++;
                PRUint8 r = *src++;
                src++;
#else
                src++;
                PRUint8 r = *src++;
                PRUint8 g = *src++;
                PRUint8 b = *src++;
#endif

                *dst++ = r;
                *dst++ = g;
                *dst++ = b;
                *dst++ = 255;
            }
        }
    } else {
        return NS_ERROR_FAILURE;
    }

    res.mSurface.forget();
    *imageOut = surf;

    return NS_OK;
}

#if 0
NS_IMETHODIMP
WebGLContext::TexSubImage2DHTML(PRUint32 target, PRUint32 level, PRInt32 x, PRInt32 y, nsIDOMHTMLElement *imageOrCanvas)
{

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

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
            return ErrorMessage("texImage2DHTML: unsupported target");
    }

    nsRefPtr<gfxImageSurface> isurf;
    nsresult rv;

    rv = TexImageElementBase(imageOrCanvas,
                             getter_AddRefs(isurf));
    if (NS_FAILED(rv))
        return rv;

    MakeContextCurrent();

    gl->fTexSubImage2D(target, level, x, y, isurf->Width(), isurf->Height(), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, isurf->Data());

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::TexImage2DHTML(PRUint32 target, PRUint32 level, nsIDOMHTMLElement *imageOrCanvas)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

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
            return ErrorMessage("texImage2DHTML: unsupported target");
    }

    nsRefPtr<gfxImageSurface> isurf;
    nsresult rv;

    rv = TexImageElementBase(imageOrCanvas,
                             getter_AddRefs(isurf));
    if (NS_FAILED(rv))
        return rv;
        
    MakeContextCurrent();

    gl->fTexImage2D(target, level, LOCAL_GL_RGBA, isurf->Width(), isurf->Height(), 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, isurf->Data());

    return NS_OK;
}

#endif /* XXX todo */

GL_SAME_METHOD_2(Uniform1i, Uniform1i, GLint, GLint)
GL_SAME_METHOD_3(Uniform2i, Uniform2i, GLint, GLint, GLint)
GL_SAME_METHOD_4(Uniform3i, Uniform3i, GLint, GLint, GLint, GLint)
GL_SAME_METHOD_5(Uniform4i, Uniform4i, GLint, GLint, GLint, GLint, GLint)

GL_SAME_METHOD_2(Uniform1f, Uniform1f, GLint, GLfloat)
GL_SAME_METHOD_3(Uniform2f, Uniform2f, GLint, GLfloat, GLfloat)
GL_SAME_METHOD_4(Uniform3f, Uniform3f, GLint, GLfloat, GLfloat, GLfloat)
GL_SAME_METHOD_5(Uniform4f, Uniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat)

// one uint arg followed by an array of c elements of glTypeConst.
#define GL_SIMPLE_ARRAY_METHOD(glname, name, c, glTypeConst, ptrType)   \
NS_IMETHODIMP                                                           \
WebGLContext::name()                                                    \
{                                                                       \
    NativeJSContext js;                                                 \
    if (NS_FAILED(js.error))                                            \
        return js.error;                                                \
    jsuint index;                                                       \
    JSObject *arrayObj;                                                 \
    jsuint arrayLen;                                                    \
    if (js.argc != 2 ||                                                 \
        !::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index) ||          \
        !NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[1], &arrayObj, &arrayLen)) \
        return NS_ERROR_INVALID_ARG;                                    \
    if (arrayLen % c != 0) {                                            \
        return ErrorMessage(#name ": array length not divisible by " #c); \
    }                                                                   \
    SimpleBuffer sbuffer(glTypeConst, c, js.ctx, arrayObj, arrayLen);   \
    if (!sbuffer.Valid())                                               \
        return NS_ERROR_FAILURE;                                        \
    MakeContextCurrent();                                               \
    gl->f##glname(index, arrayLen / c, ( ptrType *)sbuffer.data);       \
    return NS_OK;                                                       \
}

#define GL_SIMPLE_ARRAY_METHOD_NO_COUNT(glname, name, c, glTypeConst, ptrType) \
NS_IMETHODIMP                                                           \
WebGLContext::name()                                                    \
{                                                                       \
    NativeJSContext js;                                                 \
    if (NS_FAILED(js.error))                                            \
        return js.error;                                                \
    jsuint index;                                                       \
    JSObject *arrayObj;                                                 \
    jsuint arrayLen;                                                    \
    if (js.argc != 2 ||                                                 \
        !::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index) ||          \
        !NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[1], &arrayObj, &arrayLen)) \
        return NS_ERROR_INVALID_ARG;                                    \
    if (arrayLen != c) {                                                \
        return ErrorMessage(#name ": array wrong size, expected " #c);  \
    }                                                                   \
    SimpleBuffer sbuffer(glTypeConst, c, js.ctx, arrayObj, arrayLen);   \
    if (!sbuffer.Valid())                                               \
        return NS_ERROR_FAILURE;                                        \
    MakeContextCurrent();                                               \
    gl->f##glname(index, ( ptrType *)sbuffer.data);                     \
    return NS_OK;                                                       \
}

#define GL_SIMPLE_MATRIX_METHOD(glname, name, c, glTypeConst, ptrType)  \
NS_IMETHODIMP                                                           \
WebGLContext::name()                                                    \
{                                                                       \
    NativeJSContext js;                                                 \
    if (NS_FAILED(js.error))                                            \
        return js.error;                                                \
    jsuint index;                                                       \
    JSObject *arrayObj;                                                 \
    jsuint arrayLen;                                                    \
    if (js.argc != 2 ||                                                 \
        !::JS_ValueToECMAUint32(js.ctx, js.argv[0], &index) ||          \
        !NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[1], &arrayObj, &arrayLen)) \
        return NS_ERROR_INVALID_ARG;                                    \
    if (arrayLen != c) {                                                \
        return ErrorMessage(#name ": array wrong size, expected " #c);  \
    }                                                                   \
    SimpleBuffer sbuffer(glTypeConst, c, js.ctx, arrayObj, arrayLen);   \
    if (!sbuffer.Valid())                                               \
        return NS_ERROR_FAILURE;                                        \
    MakeContextCurrent();                                               \
    gl->f##glname(index, arrayLen / c, LOCAL_GL_FALSE, ( ptrType *)sbuffer.data); \
    return NS_OK;                                                       \
}

GL_SIMPLE_ARRAY_METHOD(Uniform1iv, Uniform1iv, 1, LOCAL_GL_INT, GLint)
GL_SIMPLE_ARRAY_METHOD(Uniform2iv, Uniform2iv, 2, LOCAL_GL_INT, GLint)
GL_SIMPLE_ARRAY_METHOD(Uniform3iv, Uniform3iv, 3, LOCAL_GL_INT, GLint)
GL_SIMPLE_ARRAY_METHOD(Uniform4iv, Uniform4iv, 4, LOCAL_GL_INT, GLint)

GL_SIMPLE_ARRAY_METHOD(Uniform1fv, Uniform1fv, 1, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD(Uniform2fv, Uniform2fv, 2, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD(Uniform3fv, Uniform3fv, 3, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD(Uniform4fv, Uniform4fv, 4, LOCAL_GL_FLOAT, GLfloat)

GL_SIMPLE_MATRIX_METHOD(UniformMatrix2fv, UniformMatrix2fv, 4, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_MATRIX_METHOD(UniformMatrix3fv, UniformMatrix3fv, 9, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_MATRIX_METHOD(UniformMatrix4fv, UniformMatrix4fv, 16, LOCAL_GL_FLOAT, GLfloat)

NS_IMETHODIMP
WebGLContext::UseProgram(nsIWebGLProgram *prog)
{
    if (prog && static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("glUseProgram: program has already been deleted!");

    GLuint program = prog ? static_cast<WebGLProgram*>(prog)->GLName() : 0;

    MakeContextCurrent();

    gl->fUseProgram(program);

    mCurrentProgram = static_cast<WebGLProgram*>(prog);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::ValidateProgram(nsIWebGLProgram *prog)
{
    if (!prog && static_cast<WebGLProgram*>(prog)->Deleted())
        return ErrorMessage("glValidateProgram: program is null or has already been deleted!");

    GLuint program = static_cast<WebGLProgram*>(prog)->GLName();

    MakeContextCurrent();

    gl->fValidateProgram(program);

    return NS_OK;
}

GL_SAME_METHOD_2(VertexAttrib1f, VertexAttrib1f, PRUint32, float)
GL_SAME_METHOD_3(VertexAttrib2f, VertexAttrib2f, PRUint32, float, float)
GL_SAME_METHOD_4(VertexAttrib3f, VertexAttrib3f, PRUint32, float, float, float)
GL_SAME_METHOD_5(VertexAttrib4f, VertexAttrib4f, PRUint32, float, float, float, float)

GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib1fv, VertexAttrib1fv, 1, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib2fv, VertexAttrib2fv, 2, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib3fv, VertexAttrib3fv, 3, LOCAL_GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib4fv, VertexAttrib4fv, 4, LOCAL_GL_FLOAT, GLfloat)

NS_IMETHODIMP
WebGLContext::GenFramebuffer(nsIWebGLFramebuffer **retval)
{
    MakeContextCurrent();

    GLuint name;
    gl->fGenFramebuffers(1, &name);

    WebGLFramebuffer *globj = new WebGLFramebuffer(name);
    if (globj) {
        NS_ADDREF(*retval = globj);
        mMapFramebuffers.Put(name, globj);
    } else {
        gl->fDeleteFramebuffers(1, &name);
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GenRenderbuffer(nsIWebGLRenderbuffer **retval)
{
    MakeContextCurrent();

    GLuint name;
    gl->fGenRenderbuffers(1, &name);

    WebGLRenderbuffer *globj = new WebGLRenderbuffer(name);
    if (globj) {
        NS_ADDREF(*retval = globj);
        mMapRenderbuffers.Put(name, globj);
    } else {
        gl->fDeleteRenderbuffers(1, &name);
    }

    return NS_OK;
}

GL_SAME_METHOD_4(Viewport, Viewport, PRInt32, PRInt32, PRInt32, PRInt32)

NS_IMETHODIMP
WebGLContext::CompileShader(nsIWebGLShader *shobj)
{
    if (!shobj || static_cast<WebGLShader*>(shobj)->Deleted())
        return ErrorMessage("%s: shader is null or deleted!", __FUNCTION__);

    GLuint shader = static_cast<WebGLShader*>(shobj)->GLName();
    
    MakeContextCurrent();

    gl->fCompileShader(shader);

    return NS_OK;
}


NS_IMETHODIMP
WebGLContext::GetShaderParameter(nsIWebGLShader *shobj, GLenum pname)
{
    if (!shobj || static_cast<WebGLShader*>(shobj)->Deleted())
        return ErrorMessage("%s: shader is null or deleted!", __FUNCTION__);

    GLuint shader = static_cast<WebGLShader*>(shobj)->GLName();
    
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case LOCAL_GL_SHADER_TYPE:
        case LOCAL_GL_DELETE_STATUS:
        case LOCAL_GL_COMPILE_STATUS:
        case LOCAL_GL_INFO_LOG_LENGTH:
        case LOCAL_GL_SHADER_SOURCE_LENGTH:
        {
            PRInt32 iv = 0;
            gl->fGetShaderiv(shader, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetShaderInfoLog(nsIWebGLShader *shobj, nsAString& retval)
{
    if (!shobj || static_cast<WebGLShader*>(shobj)->Deleted())
        return ErrorMessage("%s: shader is null or deleted!", __FUNCTION__);

    GLuint shader = static_cast<WebGLShader*>(shobj)->GLName();    

    MakeContextCurrent();

    PRInt32 k = -1;
    gl->fGetShaderiv(shader, LOCAL_GL_INFO_LOG_LENGTH, (GLint*) &k);
    if (k == -1)
        return NS_ERROR_FAILURE;

    if (k == 0) {
        retval.Truncate();
        return NS_OK;
    }

    nsCAutoString log;
    log.SetCapacity(k);

    gl->fGetShaderInfoLog(shader, k, (GLint*) &k, (char*) log.BeginWriting());

    log.SetLength(k);

    CopyASCIItoUTF16(log, retval);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetShaderSource(nsIWebGLShader *shobj, nsAString& retval)
{
    if (!shobj || static_cast<WebGLShader*>(shobj)->Deleted())
        return ErrorMessage("%s: shader is null or deleted!", __FUNCTION__);

    GLuint shader = static_cast<WebGLShader*>(shobj)->GLName();
    
    MakeContextCurrent();

    GLint slen = -1;
    gl->fGetShaderiv (shader, LOCAL_GL_SHADER_SOURCE_LENGTH, &slen);
    if (slen == -1)
        return NS_ERROR_FAILURE;

    if (slen == 0) {
        retval.Truncate();
        return NS_OK;
    }

    nsCAutoString src;
    src.SetCapacity(slen);

    gl->fGetShaderSource (shader, slen, NULL, (char*) src.BeginWriting());

    src.SetLength(slen);

    CopyASCIItoUTF16(src, retval);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::ShaderSource(nsIWebGLShader *shobj, const nsAString& source)
{
    if (!shobj || static_cast<WebGLShader*>(shobj)->Deleted())
        return ErrorMessage("%s: shader is null or deleted!", __FUNCTION__);

    GLuint shader = static_cast<WebGLShader*>(shobj)->GLName();
    
    MakeContextCurrent();

    NS_LossyConvertUTF16toASCII asciisrc(source);
    const char *p = asciisrc.get();

    gl->fShaderSource(shader, 1, &p, NULL);
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                    GLboolean normalized, GLuint stride,
                                    GLuint offset)
{
    if (mBoundArrayBuffer == nsnull)
        return ErrorMessage("glvertexattribpointer: must have GL_ARRAY_BUFFER binding!");

    if (index >= mAttribBuffers.Length())
        return ErrorMessage("glVertexAttribPointer: index out of range - %d >= %d", index, mAttribBuffers.Length());

    if (size < 1 || size > 4)
        return ErrorMessage("glVertexAttribPointer: invalid element size");

    if (type != mBoundArrayBuffer->GLType())
        return ErrorMessage("glVertexAttribPointer: type must match bound VBO type: %d != %d", type, mBoundArrayBuffer->GLType());

    // XXX 0 stride?
    //if (stride < (GLuint) size)
    //    return ErrorMessage("glVertexAttribPointer: stride must be >= size!");

    WebGLVertexAttribData &vd = mAttribBuffers[index];

    vd.buf = mBoundArrayBuffer;
    vd.stride = stride;
    vd.size = size;
    vd.offset = offset;

    MakeContextCurrent();

    gl->fVertexAttribPointer(index, size, type, normalized,
                             stride * mBoundArrayBuffer->ElementSize(),
                             (void*) (offset * mBoundArrayBuffer->ElementSize()));

    return NS_OK;
}

PRBool
WebGLContext::ValidateGL()
{
    // make sure that the opengl stuff that we need is supported
    GLint val = 0;

    gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_ATTRIBS, &val);
    mAttribBuffers.SetLength(val);

    gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_UNITS, &val);
    mBound2DTextures.SetLength(val);
    mBoundCubeMapTextures.SetLength(val);

    gl->fGetIntegerv(LOCAL_GL_MAX_COLOR_ATTACHMENTS, &val);
    mBoundColorFramebuffers.SetLength(val);

#ifndef USE_GLES2
    // gl_PointSize is always available in ES2 GLSL
    gl->fEnable(LOCAL_GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

    return PR_TRUE;
}

NS_IMETHODIMP
WebGLContext::TexSubImage2D()
{
    // XXX TODO
    return NS_ERROR_FAILURE;

#if 0
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 9) {
        return ErrorMessage("texSubImage2D: expected 9 arguments");
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    jsuint argTarget, argLevel, argX, argY, argWidth, argHeight, argFormat, argType;
    JSObject *argPixelsObj;
    jsuint argPixelsLen;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuuuuuuuo",
                               &argTarget, &argLevel, &argX, &argY,
                               &argWidth, &argHeight, &argFormat, &argType,
                               &argPixelsObj) ||
        !argPixelsObj ||
        !::JS_IsArrayObject(js.ctx, argPixelsObj) ||
        !::JS_GetArrayLength(js.ctx, argPixelsObj, &argPixelsLen))
    {
        return ErrorMessage("texSubImage2D: argument error");
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (argTarget) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            return ErrorMessage("texSubImage2D: unsupported target");
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    PRUint32 bufferType, bufferSize;
    switch (argFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_GREEN:
        case LOCAL_GL_BLUE:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
            bufferSize = 1;
            break;
        case LOCAL_GL_LUMINANCE_ALPHA:
            bufferSize = 2;
            break;
        case LOCAL_GL_RGB:
            bufferSize = 3;
            break;
        case LOCAL_GL_RGBA:
            bufferSize = 4;
            break;
        default:
            return ErrorMessage("texSubImage2D: pixel format not supported");
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (argType) {
        case LOCAL_GL_SHORT:
        case LOCAL_GL_UNSIGNED_SHORT:
        case LOCAL_GL_BYTE:
        case LOCAL_GL_UNSIGNED_BYTE:
        case LOCAL_GL_INT:
        case LOCAL_GL_UNSIGNED_INT:
        case LOCAL_GL_FLOAT:
            bufferType = argType;
            break;
        case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
            bufferType = LOCAL_GL_UNSIGNED_SHORT;
            break;
        default:
            return ErrorMessage("texSubImage2D: pixel packing not supported");
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    // make sure the size is valid
    PRInt32 tmp = argWidth * argHeight;
    if (tmp && tmp / argHeight != argWidth) {
        return ErrorMessage("texSubImage2D: too large width or height");
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    tmp = tmp * bufferSize;
    if (tmp && tmp / bufferSize != (argWidth * argHeight)) {
        return ErrorMessage("texSubImage2D: too large width or height (after multiplying with pixel size)");
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if ((PRUint32) tmp > argPixelsLen) {
        return ErrorMessage("texSubImage2D: array dimensions too small for width, height and pixel format");
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer(bufferType, bufferSize, js.ctx, argPixelsObj, argPixelsLen);
    if (!sbuffer.Valid())
        return NS_ERROR_FAILURE;

    MakeContextCurrent();
    gl->fTexSubImage2D (argTarget, argLevel, argX, argY, argWidth, argHeight, argFormat, argType, (void *) sbuffer.data);

    return NS_OK;
#endif
}

NS_IMETHODIMP
WebGLContext::TexImage2D()
{
    //return NS_ERROR_FAILURE;
    // XXX TODO
    //#if 0
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3 && js.argc != 9) {
        return ErrorMessage("texImage2D: expected 3 or 9 arguments");
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    jsuint argTarget, argLevel, argInternalFormat, argWidth, argHeight, argBorder, argFormat, argType;

    if (js.argc == 3) {
        JSObject *argPixelsObj;
        jsuint argPixelsLen;
        if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuo",
                                   &argTarget, &argLevel,
                                   &argPixelsObj) ||
            !argPixelsObj)
            {
                return ErrorMessage("texImage2D: argument error");
                return NS_ERROR_DOM_SYNTAX_ERR;
            }
        nsCOMPtr<nsIDOMHTMLElement> imgElt;
        nsresult rv;
        rv = nsContentUtils::XPConnect()->WrapJS(js.ctx, argPixelsObj, NS_GET_IID(nsIDOMHTMLElement), getter_AddRefs(imgElt));

        nsRefPtr<gfxImageSurface> isurf;

        rv = TexImageElementBase(imgElt, getter_AddRefs(isurf));

        if (NS_FAILED(rv))
            return ErrorMessage("texImage2D: failed to get image for element");

        MakeContextCurrent();

        gl->fTexImage2D (argTarget, argLevel, LOCAL_GL_RGBA, isurf->Width(), isurf->Height(), 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, isurf->Data());

    } else if (js.argc == 9) {
        JSObject *argPixelsObj;
        jsuint argPixelsLen;
        if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuuuuuuuo",
                                   &argTarget, &argLevel, &argInternalFormat, &argWidth,
                                   &argHeight, &argBorder, &argFormat, &argType,
                                   &argPixelsObj) ||
            !argPixelsObj ||
            !::JS_IsArrayObject(js.ctx, argPixelsObj) ||
            !::JS_GetArrayLength(js.ctx, argPixelsObj, &argPixelsLen))
            {
                return ErrorMessage("texImage2D: argument error");
                return NS_ERROR_DOM_SYNTAX_ERR;
            }
        if (argWidth == 0 || argHeight == 0) {
            return ErrorMessage("texImage2D: width or height is zero");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        switch (argTarget) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            return ErrorMessage("texImage2D: unsupported target");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (argBorder != 0) {
            return ErrorMessage("texImage2D: non-zero border given");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        switch (argInternalFormat) {
        case LOCAL_GL_RGB:
        case LOCAL_GL_RGBA:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
        case LOCAL_GL_LUMINANCE_ALPHA:
            break;
        default:
            return ErrorMessage("texImage2D: internal format not supported");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        PRUint32 bufferType, bufferSize;
        switch (argFormat) {
        case LOCAL_GL_RED:
        case LOCAL_GL_GREEN:
        case LOCAL_GL_BLUE:
        case LOCAL_GL_ALPHA:
        case LOCAL_GL_LUMINANCE:
            bufferSize = 1;
            break;
        case LOCAL_GL_LUMINANCE_ALPHA:
            bufferSize = 2;
            break;
        case LOCAL_GL_RGB:
            bufferSize = 3;
            break;
        case LOCAL_GL_RGBA:
            bufferSize = 4;
            break;
        default:
            return ErrorMessage("texImage2D: pixel format not supported");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        switch (argType) {
        case LOCAL_GL_SHORT:
        case LOCAL_GL_UNSIGNED_SHORT:
        case LOCAL_GL_BYTE:
        case LOCAL_GL_UNSIGNED_BYTE:
        case LOCAL_GL_INT:
        case LOCAL_GL_UNSIGNED_INT:
        case LOCAL_GL_FLOAT:
            bufferType = argType;
            break;
        case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
        case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
            bufferType = LOCAL_GL_UNSIGNED_SHORT;
            break;
        default:
            return ErrorMessage("texImage2D: pixel packing not supported");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        // make sure the size is valid
        PRInt32 tmp = argWidth * argHeight;
        if (tmp && tmp / argHeight != argWidth) {
            return ErrorMessage("texImage2D: too large width or height");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        tmp = tmp * bufferSize;
        if (tmp && tmp / bufferSize != (argWidth * argHeight)) {
            return ErrorMessage("texImage2D: too large width or height (after multiplying with pixel size)");
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        // XXX handle LOCAL_GL_UNPACK_ALIGNMENT !

        if (argPixelsObj == NULL) {
            MakeContextCurrent();
            gl->fTexImage2D (argTarget, argLevel, argInternalFormat, argWidth, argHeight, argBorder, argFormat, argType, NULL);
        } else {
            if ((PRUint32) tmp > argPixelsLen) {
                return ErrorMessage("texImage2D: array dimensions too small for width, height and pixel format");
                return NS_ERROR_DOM_SYNTAX_ERR;
            }

            SimpleBuffer sbuffer(bufferType, bufferSize, js.ctx, argPixelsObj, argPixelsLen);
            if (!sbuffer.Valid())
            return NS_ERROR_FAILURE;

            MakeContextCurrent();
            gl->fTexImage2D (argTarget, argLevel, argInternalFormat, argWidth, argHeight, argBorder, argFormat, argType, (void *) sbuffer.data);
        }
        //#endif
    }
    return NS_OK;
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

    if (mCanvasElement->IsWriteOnly() && !IsCallerTrustedForRead()) {
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
            *baseType = LOCAL_GL_INT; // pretend these are int
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
