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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

#include "prmem.h"
#include "prlog.h"

#include "nsIRenderingContext.h"

#include "nsTArray.h"

#define NSGL_CONTEXT_NAME nsCanvasRenderingContextGLWeb20

#include "nsCanvasRenderingContextGL.h"
#include "nsICanvasRenderingContextGLBuffer.h"
#include "nsICanvasRenderingContextGLWeb20.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsIView.h"
#include "nsIViewManager.h"

#include "nsIDocument.h"

#include "nsTransform2D.h"

#include "nsIScriptSecurityManager.h"
#include "nsISecurityCheckedComponent.h"

#include "nsWeakReference.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsDOMError.h"
#include "nsIJSRuntimeService.h"

#include "nsIClassInfoImpl.h"

#include "nsServiceManagerUtils.h"

#include "nsLayoutUtils.h"

#include "nsDOMError.h"

#include "nsIXPConnect.h"
#include "jsapi.h"

#include "CanvasUtils.h"
#include "NativeJSContext.h"

#ifdef XP_WIN
#include <windows.h>
#endif

#include <GL/gl.h>

using namespace mozilla;

#ifndef GL_MAX_RENDERBUFFER_SIZE
#define GL_MAX_RENDERBUFFER_SIZE      0x84E8
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME 0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL 0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET 0x8CD4
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_MAX_COLOR_ATTACHMENTS      0x8CDF
#define GL_COLOR_ATTACHMENT0          0x8CE0
#define GL_COLOR_ATTACHMENT1          0x8CE1
#define GL_COLOR_ATTACHMENT2          0x8CE2
#define GL_COLOR_ATTACHMENT3          0x8CE3
#define GL_COLOR_ATTACHMENT4          0x8CE4
#define GL_COLOR_ATTACHMENT5          0x8CE5
#define GL_COLOR_ATTACHMENT6          0x8CE6
#define GL_COLOR_ATTACHMENT7          0x8CE7
#define GL_COLOR_ATTACHMENT8          0x8CE8
#define GL_COLOR_ATTACHMENT9          0x8CE9
#define GL_COLOR_ATTACHMENT10         0x8CEA
#define GL_COLOR_ATTACHMENT11         0x8CEB
#define GL_COLOR_ATTACHMENT12         0x8CEC
#define GL_COLOR_ATTACHMENT13         0x8CED
#define GL_COLOR_ATTACHMENT14         0x8CEE
#define GL_COLOR_ATTACHMENT15         0x8CEF
#define GL_DEPTH_ATTACHMENT           0x8D00
#define GL_STENCIL_ATTACHMENT         0x8D20
#define GL_FRAMEBUFFER_BINDING        0x8CA6
#define GL_RENDERBUFFER_BINDING       0x8CA7
#endif

#ifndef GL_VERTEX_PROGRAM_POINT_SIZE
#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#endif

// we're hoping that something is setting us up the remap

#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "gfxPlatform.h"

#ifdef PR_LOGGING
PRLogModuleInfo* gGLES20Log = nsnull;
#endif

class nsCanvasRenderingContextGLWeb20 :
    public nsICanvasRenderingContextGLWeb20,
    public nsCanvasRenderingContextGLPrivate
{
public:
    nsCanvasRenderingContextGLWeb20();
    virtual ~nsCanvasRenderingContextGLWeb20();

    NS_DECL_ISUPPORTS

    NS_DECL_NSICANVASRENDERINGCONTEXTGL

    NS_DECL_NSICANVASRENDERINGCONTEXTGLWEB20

    // nsICanvasRenderingContextPrivate
    virtual nsICanvasRenderingContextGL *GetSelf() { return this; }
    virtual PRBool ValidateGL();

protected:
    nsresult TexImageElementBase(nsIDOMHTMLElement *imageOrCanvas,
                                 gfxImageSurface **imageOut);

    PRBool ValidateBuffers(PRUint32 count);

    nsTArray<nsRefPtr<CanvasGLBuffer> > mAttribBuffers;
    nsTArray<nsRefPtr<CanvasGLBuffer> > mBuffers;
};


// nsCanvasRenderingContextGLWeb20

NS_IMPL_ADDREF(nsCanvasRenderingContextGLWeb20)
NS_IMPL_RELEASE(nsCanvasRenderingContextGLWeb20)

NS_INTERFACE_MAP_BEGIN(nsCanvasRenderingContextGLWeb20)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGL)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGLWeb20)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextGL)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasRenderingContextGLWeb20)
NS_INTERFACE_MAP_END

static PRBool BaseTypeAndSizeFromUniformType(GLenum uType, GLenum *baseType, GLint *unitSize);

nsresult
NS_NewCanvasRenderingContextGLWeb20(nsICanvasRenderingContextGLWeb20** aResult)
{
    nsICanvasRenderingContextGLWeb20* ctx = new nsCanvasRenderingContextGLWeb20();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

nsCanvasRenderingContextGLWeb20::nsCanvasRenderingContextGLWeb20()
{
}

nsCanvasRenderingContextGLWeb20::~nsCanvasRenderingContextGLWeb20()
{
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetCanvas(nsIDOMHTMLCanvasElement **_retval)
{
    if (mCanvasElement == nsnull) {
        *_retval = nsnull;
        return NS_OK;
    }

    return CallQueryInterface(mCanvasElement, _retval);
}

/* void activeTexture (in PRUint32 texture); */
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::ActiveTexture(PRUint32 texture)
{
    // XXX query number of textures available
    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    gl->fActiveTexture(texture);
    return NS_OK;
}

GL_SAME_METHOD_2(AttachShader, AttachShader, PRUint32, PRUint32)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::BindAttribLocation(PRUint32 program, PRUint32 location, const char *name)
{
    if (!name)
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();
    gl->fBindAttribLocation(program, location, name);
    return NS_OK;
}

GL_SAME_METHOD_2(BindBuffer, BindBuffer, PRUint32, PRUint32)

GL_SAME_METHOD_2(BindFramebuffer, BindFramebuffer, PRUint32, PRUint32)

GL_SAME_METHOD_2(BindRenderbuffer, BindRenderbuffer, PRUint32, PRUint32)

GL_SAME_METHOD_2(BindTexture, BindTexture, PRUint32, PRUint32)

GL_SAME_METHOD_4(BlendColor, BlendColor, float, float, float, float)

GL_SAME_METHOD_1(BlendEquation, BlendEquation, PRUint32)

GL_SAME_METHOD_2(BlendEquationSeparate, BlendEquationSeparate, PRUint32, PRUint32)

GL_SAME_METHOD_2(BlendFunc, BlendFunc, PRUint32, PRUint32)

GL_SAME_METHOD_4(BlendFuncSeparate, BlendFuncSeparate, PRUint32, PRUint32, PRUint32, PRUint32)

/* target, array, type, usage */
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::BufferData()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 4)
        return NS_ERROR_DOM_SYNTAX_ERR;

    nsresult rv;

    JSObject *arrayObj;
    jsuint arrayLen;
    jsuint target;
    jsuint type;
    jsuint usage;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uouu", &target, &arrayObj, &type, &usage) ||
        arrayObj == NULL ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
        LogMessagef(("bufferData: invalid target"));
        return NS_ERROR_INVALID_ARG;
    }

    if (target == GL_ELEMENT_ARRAY_BUFFER && type != GL_UNSIGNED_SHORT) {
        LogMessagef(("bufferData: invalid type for element array"));
        return NS_ERROR_INVALID_ARG;
    }

    switch (usage) {
        case GL_STATIC_DRAW:
        case GL_DYNAMIC_DRAW:
        case GL_STREAM_DRAW:
            break;
        default:
            LogMessagef(("bufferData: invalid usage"));
            return NS_ERROR_INVALID_ARG;
    }

    MakeContextCurrent();
    GLint binding = 0;
    GLenum binding_target = GL_ARRAY_BUFFER_BINDING;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    gl->fGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        LogMessagef(("bufferData: no buffer bound"));
        return NS_ERROR_FAILURE;
    }

    nsRefPtr<CanvasGLBuffer> newBuffer;
    newBuffer = new CanvasGLBuffer(this);
    if (!newBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = newBuffer->Init (usage, 1, type, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return rv;

    if (!mBuffers.SetCapacity(binding+1))
        return NS_ERROR_OUT_OF_MEMORY;

    mBuffers[binding] = newBuffer;

    gl->fBufferData(target,
        newBuffer->GetSimpleBuffer().capacity,
        newBuffer->GetSimpleBuffer().data, usage);

    return NS_OK;
}

/* target, offset, array, type */
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::BufferSubData()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 4)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    jsuint target;
    jsuint offset;
    jsuint type;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuou", &target, &offset, &arrayObj, &type) ||
        arrayObj == NULL ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (target != GL_ARRAY_BUFFER && target != GL_ELEMENT_ARRAY_BUFFER) {
        LogMessagef(("bufferSubData: invalid target"));
        return NS_ERROR_INVALID_ARG;
    }

    if (target == GL_ELEMENT_ARRAY_BUFFER && type != GL_UNSIGNED_SHORT) {
        LogMessagef(("bufferSubData: invalid type for element array"));
        return NS_ERROR_INVALID_ARG;
    }

    MakeContextCurrent();
    GLint binding = 0;
    GLenum binding_target = GL_ARRAY_BUFFER_BINDING;
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        binding_target = GL_ELEMENT_ARRAY_BUFFER_BINDING;
    gl->fGetIntegerv(binding_target, &binding);
    if (binding <= 0) {
        LogMessagef(("bufferSubData: no buffer bound"));
        return NS_ERROR_FAILURE;
    }

    if ((GLushort)binding >= mBuffers.Length() || !mBuffers[binding]) {
        LogMessagef(("bufferSubData: no mBuffers[binding]"));
        return NS_ERROR_FAILURE;
    }

    SimpleBuffer sbuffer(type, 1, js.ctx, arrayObj, arrayLen);
    if (!sbuffer.Valid())
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (!mBuffers[binding]->UpdateBuffer(offset, sbuffer)) {
        LogMessagef(("bufferSubData: trying to write out of bounds"));
        return NS_ERROR_INVALID_ARG;
    }

    gl->fBufferSubData(target, offset, sbuffer.capacity, sbuffer.data);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::CheckFramebufferStatus(PRUint32 target, PRUint32 *retval)
{
    MakeContextCurrent();
    *retval = gl->fCheckFramebufferStatus(target);
    return NS_OK;
}

GL_SAME_METHOD_1(Clear, Clear, PRUint32)

GL_SAME_METHOD_4(ClearColor, ClearColor, float, float, float, float)

GL_SAME_METHOD_1(ClearDepth, ClearDepth, float)

GL_SAME_METHOD_1(ClearStencil, ClearStencil, PRInt32)

GL_SAME_METHOD_4(ColorMask, ColorMask, PRBool, PRBool, PRBool, PRBool)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::CopyTexImage2D(PRUint32 target,
                                                PRInt32 level,
                                                PRUint32 internalformat,
                                                PRInt32 x,
                                                PRInt32 y,
                                                PRUint32 width,
                                                PRUint32 height,
                                                PRInt32 border)
{
    switch (target) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("copyTexImage2D: unsupported target"));
            return NS_ERROR_INVALID_ARG;
    }

    switch (internalformat) {
        case GL_RGB:
        case GL_RGBA:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("copyTexImage2D: internal format not supported"));
            return NS_ERROR_INVALID_ARG;
    }

    if (border != 0) {
        LogMessage(NS_LITERAL_CSTRING("copyTexImage2D: border != 0"));
        return NS_ERROR_INVALID_ARG;
    }

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width, height, mWidth, mHeight)) {
        LogMessage(NS_LITERAL_CSTRING("copyTexImage2D: copied rectangle out of bounds"));
        return NS_ERROR_INVALID_ARG;
    }

    MakeContextCurrent();
    gl->fCopyTexImage2D(target, level, internalformat, x, y, width, height, border);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::CopyTexSubImage2D(PRUint32 target,
                                                   PRInt32 level,
                                                   PRInt32 xoffset,
                                                   PRInt32 yoffset,
                                                   PRInt32 x,
                                                   PRInt32 y,
                                                   PRUint32 width,
                                                   PRUint32 height)
{
    switch (target) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("copyTexSubImage2D: unsupported target"));
            return NS_ERROR_INVALID_ARG;
    }

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width, height, mWidth, mHeight)) {
        LogMessage(NS_LITERAL_CSTRING("copyTexSubImage2D: copied rectangle out of bounds"));
        return NS_ERROR_INVALID_ARG;
    }

    MakeContextCurrent();
    gl->fCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

    return NS_OK;
}


NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::CreateProgram(PRUint32 *retval)
{
    MakeContextCurrent();
    *retval = gl->fCreateProgram();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::CreateShader(PRUint32 type, PRUint32 *retval)
{
    MakeContextCurrent();
    *retval = gl->fCreateShader(type);
    return NS_OK;
}

GL_SAME_METHOD_1(CullFace, CullFace, PRUint32)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DeleteBuffers()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_INVALID_ARG;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (arrayLen <= 0)
        return NS_ERROR_INVALID_ARG;

    SimpleBuffer sbuffer(GL_UNSIGNED_INT, 1, js.ctx, arrayObj, arrayLen);
    if (!sbuffer.Valid())
        return NS_ERROR_DOM_SYNTAX_ERR;

    PRUint32 i;
    GLuint id;
    PRUint32 len = mBuffers.Length();
    PRBool doResize = PR_FALSE;
    for (i=0; i<arrayLen; ++i) {
        id = ((GLuint*)sbuffer.data)[i];
        if (id < len) {
            mBuffers[id] = NULL;
            if (id == len-1)
                doResize = PR_TRUE;
        }
    }
    if (doResize) {
        // find last non-null index
        for (i=len-1; i>0; --i)
            if (mBuffers[i]) break;

        // shrink mBuffers to fit contents
        if (mBuffers.Length() > i+1)
            mBuffers.SetLength(i+1);
    }

    MakeContextCurrent();
    gl->fDeleteBuffers(arrayLen, (GLuint*) sbuffer.data);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DeleteFramebuffers()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (arrayLen == 0)
        return NS_OK;

    SimpleBuffer sbuffer(GL_UNSIGNED_INT, 1, js.ctx, arrayObj, arrayLen);
    if (!sbuffer.Valid())
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    gl->fDeleteFramebuffers(arrayLen, (GLuint*) sbuffer.data);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DeleteRenderbuffers()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (arrayLen == 0)
        return NS_OK;

    SimpleBuffer sbuffer(GL_UNSIGNED_INT, 1, js.ctx, arrayObj, arrayLen);;
    if (!sbuffer.Valid())
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    gl->fDeleteRenderbuffers(arrayLen, (GLuint*) sbuffer.data);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DeleteTextures()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (arrayLen == 0)
        return NS_OK;

    SimpleBuffer sbuffer(GL_UNSIGNED_INT, 1, js.ctx, arrayObj, arrayLen);
    if (!sbuffer.Valid())
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    gl->fDeleteTextures(arrayLen, (GLuint*) sbuffer.data);

    return NS_OK;
}

GL_SAME_METHOD_1(DeleteProgram, DeleteProgram, PRUint32)

GL_SAME_METHOD_1(DeleteShader, DeleteShader, PRUint32)

GL_SAME_METHOD_2(DetachShader, DetachShader, PRUint32, PRUint32)

GL_SAME_METHOD_1(DepthFunc, DepthFunc, PRUint32)

GL_SAME_METHOD_1(DepthMask, DepthMask, PRBool)

GL_SAME_METHOD_2(DepthRange, DepthRange, float, float)

GL_SAME_METHOD_1(Disable, Disable, PRUint32)

GL_SAME_METHOD_1(DisableVertexAttribArray, DisableVertexAttribArray, PRUint32)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DrawArrays(PRUint32 mode, PRUint32 offset, PRUint32 count)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    switch (mode) {
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_POINTS:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_LINES:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("drawArrays: invalid mode"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (offset+count < offset || offset+count < count) {
        LogMessage(NS_LITERAL_CSTRING("drawArrays: overflow in offset+count"));
        return NS_ERROR_INVALID_ARG;
    }

    if (!ValidateBuffers(offset+count))
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();
    gl->fDrawArrays(mode, offset, count);
    return NS_OK;
}

// DrawElements
/*in PRUint32 mode, in PRUint32 count, in PRUint32 type, in PRUint32[] indices*/
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DrawElements()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 4)
        return NS_ERROR_INVALID_ARG;

    JSObject *arrayObj = NULL;
    jsuint arrayLen = 0;
    jsuint mode, count, type;
    jsuint bufferOffset;

    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &mode) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &count) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[2], &type))
    {
        LogMessagef(("drawElements: invalid arguments"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (mode) {
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_POINTS:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_LINES:
            break;
        default:
            LogMessagef(("drawElements: invalid mode"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (type) {
        case GL_UNSIGNED_SHORT:
            break;
        default:
            LogMessagef(("drawElements: type must be UNSIGNED_SHORT"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    MakeContextCurrent();

    if (NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[3], &arrayObj, &arrayLen)) {
        // We were given an actual array
        if (count > arrayLen) {
            LogMessagef(("drawElements: count > arrayLen"));
            return NS_ERROR_INVALID_ARG;
        }

        SimpleBuffer sbuffer(type, 1, js.ctx, arrayObj, arrayLen);
        if (!sbuffer.Valid())
            return NS_ERROR_FAILURE;

        // calculate the biggest index present in the index array, for validation
        GLushort max = 0;
        for (jsuint i = 0; i < arrayLen; ++i) {
            GLushort d = ((GLushort*)sbuffer.data)[i];
            if (d > max)
                max = d;
        }

        if (!ValidateBuffers(max))
            return NS_ERROR_INVALID_ARG;

        gl->fDrawElements(mode, count, type, sbuffer.data);
    } else {
        // We were given an integer offset into the currently bound VBO
        GLint array_buf = 0;
        gl->fGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &array_buf);

        if (array_buf <= 0 ||
            !::JS_ValueToECMAUint32(js.ctx, js.argv[3], &bufferOffset))
        {
            LogMessagef(("drawElements: invalid buffer argument"));
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        int sz = 2;
        GLint len = 0;
        if ((GLushort)array_buf >= mBuffers.Length() || !mBuffers[array_buf]) {
            LogMessagef(("drawElements: no mBuffers[array_buf]"));
            return NS_ERROR_INVALID_ARG;
        }

        gl->fGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &len);
        if (len < 0 || (bufferOffset+count)*sz > (GLuint)len) {
            LogMessagef(("drawElements: bufferOffset+count > buffer size"));
            return NS_ERROR_INVALID_ARG;
        }

        GLushort max = mBuffers[array_buf]->MaxUShortValue();

        if (!ValidateBuffers(max))
            return NS_ERROR_INVALID_ARG;

        gl->fDrawElements(mode, count, type, (GLvoid*)bufferOffset);
    }

    return NS_OK;
}

GL_SAME_METHOD_1(Enable, Enable, PRUint32)

GL_SAME_METHOD_1(EnableVertexAttribArray, EnableVertexAttribArray, PRUint32)

GL_SAME_METHOD_4(FramebufferRenderbuffer, FramebufferRenderbuffer, PRUint32, PRUint32, PRUint32, PRUint32)

GL_SAME_METHOD_5(FramebufferTexture2D, FramebufferTexture2D, PRUint32, PRUint32, PRUint32, PRUint32, PRInt32)

GL_SAME_METHOD_0(Flush, Flush)

GL_SAME_METHOD_0(Finish, Finish)

GL_SAME_METHOD_1(FrontFace, FrontFace, PRUint32)

GL_SAME_METHOD_1(GenerateMipmap, GenerateMipmap, PRUint32)

// returns an object: { size: ..., type: ..., name: ... }
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetActiveAttrib(PRUint32 program, PRUint32 index)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint len = 0;
    gl->fGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &len);
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
nsCanvasRenderingContextGLWeb20::GetActiveUniform(PRUint32 program, PRUint32 index)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint len = 0;
    gl->fGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &len);
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

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetAttachedShaders(PRUint32 program)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    GLint count = 0;
    gl->fGetProgramiv(program, GL_ATTACHED_SHADERS, &count);
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

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetAttribLocation(PRUint32 program,
                                                   const char *name,
                                                   PRInt32 *retval)
{
    if (!name) return NS_ERROR_INVALID_ARG;
    MakeContextCurrent();
    *retval = gl->fGetAttribLocation(program, name);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetParameter(PRUint32 pname)
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
        case GL_VENDOR:
        case GL_RENDERER:
        case GL_VERSION:
        case GL_SHADING_LANGUAGE_VERSION:
        //case GL_EXTENSIONS:  // Not going to expose this

            break;

        //
        // Single-value params
        //

// int
        case GL_ARRAY_BUFFER_BINDING:
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: // XXX really?
        case GL_CULL_FACE_MODE:
        case GL_FRONT_FACE:
        case GL_TEXTURE_BINDING_2D:
        case GL_TEXTURE_BINDING_CUBE_MAP:
        case GL_ACTIVE_TEXTURE:
        case GL_STENCIL_WRITEMASK:
        case GL_STENCIL_BACK_WRITEMASK:
        case GL_DEPTH_CLEAR_VALUE:
        case GL_STENCIL_CLEAR_VALUE:
        case GL_STENCIL_FUNC:
        case GL_STENCIL_VALUE_MASK:
        case GL_STENCIL_REF:
        case GL_STENCIL_FAIL:
        case GL_STENCIL_PASS_DEPTH_FAIL:
        case GL_STENCIL_PASS_DEPTH_PASS:
        case GL_STENCIL_BACK_FUNC:
        case GL_STENCIL_BACK_VALUE_MASK:
        case GL_STENCIL_BACK_REF:
        case GL_STENCIL_BACK_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:
        case GL_DEPTH_FUNC:
        case GL_BLEND_SRC_RGB:
        case GL_BLEND_SRC_ALPHA:
        case GL_BLEND_DST_RGB:
        case GL_BLEND_DST_ALPHA:
        case GL_BLEND_EQUATION_RGB:
        case GL_BLEND_EQUATION_ALPHA:
        //case GL_UNPACK_ALIGNMENT: // not supported
        //case GL_PACK_ALIGNMENT: // not supported
        case GL_CURRENT_PROGRAM:
        case GL_GENERATE_MIPMAP_HINT:
        case GL_SUBPIXEL_BITS:
        case GL_MAX_TEXTURE_SIZE:
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        case GL_MAX_ELEMENTS_INDICES:
        case GL_MAX_ELEMENTS_VERTICES:
        case GL_SAMPLE_BUFFERS:
        case GL_SAMPLES:
        //case GL_COMPRESSED_TEXTURE_FORMATS:
        //case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
        //case GL_SHADER_BINARY_FORMATS:
        //case GL_NUM_SHADER_BINARY_FORMATS:
        case GL_MAX_VERTEX_ATTRIBS:
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
        case GL_MAX_VARYING_FLOATS:
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        case GL_MAX_TEXTURE_IMAGE_UNITS:
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
        case GL_MAX_RENDERBUFFER_SIZE:
        case GL_RED_BITS:
        case GL_GREEN_BITS:
        case GL_BLUE_BITS:
        case GL_ALPHA_BITS:
        case GL_DEPTH_BITS:
        case GL_STENCIL_BITS:
        //case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        //case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        case GL_RENDERBUFFER_BINDING:
        case GL_FRAMEBUFFER_BINDING:
        {
            PRInt32 iv = 0;
            gl->fGetIntegerv(pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

// float
        case GL_LINE_WIDTH:
        case GL_POLYGON_OFFSET_FACTOR:
        case GL_POLYGON_OFFSET_UNITS:
        case GL_SAMPLE_COVERAGE_VALUE:
        {
            float fv = 0;
            gl->fGetFloatv(pname, &fv);
            js.SetRetVal((double) fv);
        }
            break;
// bool
        case GL_SAMPLE_COVERAGE_INVERT:
        case GL_COLOR_WRITEMASK:
        case GL_DEPTH_WRITEMASK:
        ////case GL_SHADER_COMPILER: // pretty much must be true 
        {
            GLboolean bv = 0;
            gl->fGetBooleanv(pname, &bv);
            js.SetBoolRetVal(bv);
        }
            break;

        //
        // Complex values
        //
        case GL_DEPTH_RANGE: // 2 floats
        case GL_ALIASED_POINT_SIZE_RANGE: // 2 floats
        case GL_ALIASED_LINE_WIDTH_RANGE: // 2 floats
        {
            float fv[2] = { 0 };
            gl->fGetFloatv(pname, &fv[0]);
            js.SetRetVal(fv, 2);
        }
            break;
        
        case GL_COLOR_CLEAR_VALUE: // 4 floats
        case GL_BLEND_COLOR: // 4 floats
        {
            float fv[4] = { 0 };
            gl->fGetFloatv(pname, &fv[0]);
            js.SetRetVal(fv, 4);
        }
            break;

        case GL_MAX_VIEWPORT_DIMS: // 2 ints
        {
            PRInt32 iv[2] = { 0 };
            gl->fGetIntegerv(pname, (GLint*) &iv[0]);
            js.SetRetVal(iv, 2);
        }
            break;

        case GL_SCISSOR_BOX: // 4 ints
        case GL_VIEWPORT: // 4 ints
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
nsCanvasRenderingContextGLWeb20::GetBufferParameter(PRUint32 target, PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case GL_BUFFER_SIZE:
        case GL_BUFFER_USAGE:
        case GL_BUFFER_ACCESS:
        case GL_BUFFER_MAPPED:
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
nsCanvasRenderingContextGLWeb20::GetFramebufferAttachmentParameter(PRUint32 target, PRUint32 attachment, PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (attachment) {
        case GL_COLOR_ATTACHMENT0:
        case GL_DEPTH_ATTACHMENT:
        case GL_STENCIL_ATTACHMENT:
            break;
        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    switch (pname) {
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
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
nsCanvasRenderingContextGLWeb20::GetRenderbufferParameter(PRUint32 target, PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case GL_RENDERBUFFER_WIDTH_EXT:
        case GL_RENDERBUFFER_HEIGHT_EXT:
        case GL_RENDERBUFFER_INTERNAL_FORMAT_EXT:
        case GL_RENDERBUFFER_RED_SIZE_EXT:
        case GL_RENDERBUFFER_GREEN_SIZE_EXT:
        case GL_RENDERBUFFER_BLUE_SIZE_EXT:
        case GL_RENDERBUFFER_ALPHA_SIZE_EXT:
        case GL_RENDERBUFFER_DEPTH_SIZE_EXT:
        case GL_RENDERBUFFER_STENCIL_SIZE_EXT:
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
nsCanvasRenderingContextGLWeb20::GenBuffers(PRUint32 n)
{
    if (n == 0) return NS_OK;
    if (n > 0xffffu) return NS_ERROR_INVALID_ARG;

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    nsAutoTArray<PRUint32, 16> ids;
    ids.SetCapacity(n);

    MakeContextCurrent();
    gl->fGenBuffers(n, (GLuint*) ids.Elements());

    js.SetRetVal(ids.Elements(), n);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GenTextures(PRUint32 n)
{
    if (n == 0) return NS_OK;
    if (n > 0xffffu) return NS_ERROR_INVALID_ARG;

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    nsAutoTArray<PRUint32, 16> ids;
    ids.SetCapacity(n);

    MakeContextCurrent();
    gl->fGenTextures(n, (GLuint*) ids.Elements());

    js.SetRetVal(ids.Elements(), n);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetError(PRUint32 *_retval)
{
    MakeContextCurrent();
    *_retval = gl->fGetError();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetProgramParameter(PRUint32 program, PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case GL_CURRENT_PROGRAM:
        case GL_DELETE_STATUS:
        case GL_LINK_STATUS:
        case GL_VALIDATE_STATUS:
        case GL_ATTACHED_SHADERS:
        case GL_INFO_LOG_LENGTH:
        case GL_ACTIVE_UNIFORMS:
        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
        case GL_ACTIVE_ATTRIBUTES:
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
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
nsCanvasRenderingContextGLWeb20::GetProgramInfoLog(PRUint32 program, char **retval)
{
    MakeContextCurrent();

    PRInt32 k = -1;
    gl->fGetProgramiv(program, GL_INFO_LOG_LENGTH, (GLint*) &k);
    if (k == -1)
        return NS_ERROR_FAILURE;

    if (k == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    char *s = (char *) PR_Malloc(k);
    if (!s)
        return NS_ERROR_OUT_OF_MEMORY;

    gl->fGetProgramInfoLog(program, k, (GLint*) &k, s);

    *retval = s;
    return NS_OK;
}

/* void texParameter (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::TexParameter()
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

    if (targetVal != GL_TEXTURE_2D &&
        targetVal != GL_TEXTURE_CUBE_MAP)
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    MakeContextCurrent();
    switch (pnameVal) {
        case GL_TEXTURE_MIN_FILTER: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_NEAREST &&
                 ival != GL_LINEAR &&
                 ival != GL_NEAREST_MIPMAP_NEAREST &&
                 ival != GL_LINEAR_MIPMAP_NEAREST &&
                 ival != GL_NEAREST_MIPMAP_LINEAR &&
                 ival != GL_LINEAR_MIPMAP_LINEAR))
                return NS_ERROR_DOM_SYNTAX_ERR;
            gl->fTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case GL_TEXTURE_MAG_FILTER: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_NEAREST &&
                 ival != GL_LINEAR))
                return NS_ERROR_DOM_SYNTAX_ERR;
            gl->fTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_CLAMP &&
                 ival != GL_CLAMP_TO_EDGE &&
                 ival != GL_REPEAT))
                return NS_ERROR_DOM_SYNTAX_ERR;
            gl->fTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case GL_GENERATE_MIPMAP: {
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
        case GL_TEXTURE_MAX_ANISOTROPY_EXT: {
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

/* void getTexParameter (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetTexParameter(PRUint32 target, PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
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

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetUniform(PRUint32 program, PRUint32 location)
{
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

    if (baseType == GL_FLOAT) {
        GLfloat fv[16];
        gl->fGetUniformfv(program, location, fv);
        js.SetRetVal(fv, unitSize);
    } else if (baseType == GL_INT) {
        GLint iv[16];
        gl->fGetUniformiv(program, location, iv);
        js.SetRetVal((PRInt32*)iv, unitSize);
    } else {
        js.SetRetValAsJSVal(JSVAL_NULL);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetUniformLocation(PRUint32 program, const char *name, PRInt32 *retval)
{
    if (!name)
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();
    *retval = gl->fGetUniformLocation(program, name);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetVertexAttrib(PRUint32 index, PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        // int
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        {
            PRInt32 iv = 0;
            gl->fGetVertexAttribiv(index, pname, (GLint*) &iv);
            js.SetRetVal(iv);
        }
            break;

        case GL_CURRENT_VERTEX_ATTRIB:
        {
            GLfloat fv[4] = { 0 };
            gl->fGetVertexAttribfv(index, GL_CURRENT_VERTEX_ATTRIB, &fv[0]);
            js.SetRetVal(fv, 4);
        }
            break;

        // not supported; doesn't make sense to return a pointer unless we have some kind of buffer object abstraction
        case GL_VERTEX_ATTRIB_ARRAY_POINTER:
        default:
            return NS_ERROR_NOT_IMPLEMENTED;

    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::Hint(PRUint32 target, PRUint32 mode)
{
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsBuffer(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsBuffer(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsFramebuffer(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsFramebuffer(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsEnabled(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsEnabled(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsProgram(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsProgram(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsRenderbuffer(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsRenderbuffer(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsShader(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsShader(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsTexture(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = gl->fIsTexture(k);
    return NS_OK;
}

GL_SAME_METHOD_1(LineWidth, LineWidth, float)

GL_SAME_METHOD_1(LinkProgram, LinkProgram, PRUint32)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::PixelStore(PRUint32 pname, PRInt32 param)
{
    if (pname != GL_PACK_ALIGNMENT &&
        pname != GL_UNPACK_ALIGNMENT)
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();
    gl->fPixelStorei(pname, param);

    return NS_OK;
}

GL_SAME_METHOD_2(PolygonOffset, PolygonOffset, float, float)

PRBool
IsCallerTrustedForRead()
{
  // FIXME this is a copy of nsContentUtils::IsCallerTrustedForRead
  // Figure out how to #include "nsContentUtils.h" and use that instead.

  // The secman really should handle UniversalXPConnect case, since that
  // should include UniversalBrowserRead... doesn't right now, though.
  PRBool hasCap;
  nsIScriptSecurityManager *sSecurityManager;
  nsresult rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                               &sSecurityManager);
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_FAILED(sSecurityManager->IsCapabilityEnabled("UniversalBrowserRead", &hasCap)))
    return PR_FALSE;
  if (hasCap)
    return PR_TRUE;

  if (NS_FAILED(sSecurityManager->IsCapabilityEnabled("UniversalXPConnect",
                                                      &hasCap)))
    return PR_FALSE;
  return hasCap;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::ReadPixels(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height, PRUint32 format, PRUint32 type)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (mCanvasElement->IsWriteOnly() && !IsCallerTrustedForRead()) {
        LogMessage(NS_LITERAL_CSTRING("readPixels: Not allowed"));
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    MakeContextCurrent();

    if (!CanvasUtils::CheckSaneSubrectSize(x,y,width,height, mWidth, mHeight)) {
        LogMessage(NS_LITERAL_CSTRING("readPixels: rectangle outside canvas"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    PRUint32 size = 0;
    switch (format) {
      case GL_ALPHA:
        size = 1;
        break;
      case GL_RGB:
        size = 3;
        break;
      case GL_RGBA:
        size = 4;
        break;
      default:
        LogMessage(NS_LITERAL_CSTRING("readPixels: unsupported pixel format"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }
    switch (type) {
//         case GL_UNSIGNED_SHORT_4_4_4_4:
//         case GL_UNSIGNED_SHORT_5_5_5_1:
//         case GL_UNSIGNED_SHORT_5_6_5:
      case GL_UNSIGNED_BYTE:
        break;
      default:
        LogMessage(NS_LITERAL_CSTRING("readPixels: unsupported pixel type"));
        return NS_ERROR_DOM_SYNTAX_ERR;
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

GL_SAME_METHOD_4(RenderbufferStorage, RenderbufferStorage, PRUint32, PRUint32, PRUint32, PRUint32)

GL_SAME_METHOD_2(SampleCoverage, SampleCoverage, float, PRBool)

GL_SAME_METHOD_4(Scissor, Scissor, PRInt32, PRInt32, PRInt32, PRInt32)

GL_SAME_METHOD_3(StencilFunc, StencilFunc, PRUint32, PRInt32, PRUint32)

GL_SAME_METHOD_4(StencilFuncSeparate, StencilFuncSeparate, PRUint32, PRUint32, PRInt32, PRUint32)

GL_SAME_METHOD_1(StencilMask, StencilMask, PRUint32)

GL_SAME_METHOD_2(StencilMaskSeparate, StencilMaskSeparate, PRUint32, PRUint32)

GL_SAME_METHOD_3(StencilOp, StencilOp, PRUint32, PRUint32, PRUint32)

GL_SAME_METHOD_4(StencilOpSeparate, StencilOpSeparate, PRUint32, PRUint32, PRUint32, PRUint32)

nsresult
nsCanvasRenderingContextGLWeb20::TexImageElementBase(nsIDOMHTMLElement *imageOrCanvas,
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

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::TexSubImage2DHTML(PRUint32 target, PRUint32 level, PRInt32 x, PRInt32 y, nsIDOMHTMLElement *imageOrCanvas)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    switch (target) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texImage2DHTML: unsupported target"));
            return NS_ERROR_INVALID_ARG;
    }

    nsRefPtr<gfxImageSurface> isurf;
    nsresult rv;

    rv = TexImageElementBase(imageOrCanvas,
                             getter_AddRefs(isurf));
    if (NS_FAILED(rv))
        return rv;

    MakeContextCurrent();

    gl->fTexSubImage2D(target, level, x, y, isurf->Width(), isurf->Height(), GL_RGBA, GL_UNSIGNED_BYTE, isurf->Data());

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::TexImage2DHTML(PRUint32 target, PRUint32 level, nsIDOMHTMLElement *imageOrCanvas)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    switch (target) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texImage2DHTML: unsupported target"));
            return NS_ERROR_INVALID_ARG;
    }

    nsRefPtr<gfxImageSurface> isurf;
    nsresult rv;

    rv = TexImageElementBase(imageOrCanvas,
                             getter_AddRefs(isurf));
    if (NS_FAILED(rv))
        return rv;
        
    MakeContextCurrent();

    gl->fTexImage2D(target, level, GL_RGBA, isurf->Width(), isurf->Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, isurf->Data());

    return NS_OK;
}

GL_SAME_METHOD_2(Uniform1i, Uniform1i, PRUint32, PRInt32)
GL_SAME_METHOD_3(Uniform2i, Uniform2i, PRUint32, PRInt32, PRInt32)
GL_SAME_METHOD_4(Uniform3i, Uniform3i, PRUint32, PRInt32, PRInt32, PRInt32)
GL_SAME_METHOD_5(Uniform4i, Uniform4i, PRUint32, PRInt32, PRInt32, PRInt32, PRInt32)

GL_SAME_METHOD_2(Uniform1f, Uniform1f, PRUint32, float)
GL_SAME_METHOD_3(Uniform2f, Uniform2f, PRUint32, float, float)
GL_SAME_METHOD_4(Uniform3f, Uniform3f, PRUint32, float, float, float)
GL_SAME_METHOD_5(Uniform4f, Uniform4f, PRUint32, float, float, float, float)

// one uint arg followed by an array of c elements of glTypeConst.
#define GL_SIMPLE_ARRAY_METHOD(glname, name, c, glTypeConst, ptrType)   \
NS_IMETHODIMP                                                           \
NSGL_CONTEXT_NAME::name()                                               \
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
        LogMessage(NS_LITERAL_CSTRING(#name ": array length not divisible by " #c)); \
        return NS_ERROR_INVALID_ARG;                                    \
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
NSGL_CONTEXT_NAME::name()                                               \
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
        LogMessage(NS_LITERAL_CSTRING(#name ": array wrong size, expected " #c)); \
        return NS_ERROR_INVALID_ARG;                                    \
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
NSGL_CONTEXT_NAME::name()                                               \
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
        LogMessage(NS_LITERAL_CSTRING(#name ": array wrong size, expected " #c)); \
        return NS_ERROR_INVALID_ARG;                                    \
    }                                                                   \
    SimpleBuffer sbuffer(glTypeConst, c, js.ctx, arrayObj, arrayLen);   \
    if (!sbuffer.Valid())                                               \
        return NS_ERROR_FAILURE;                                        \
    MakeContextCurrent();                                               \
    gl->f##glname(index, arrayLen / c, GL_FALSE, ( ptrType *)sbuffer.data); \
    return NS_OK;                                                       \
}

GL_SIMPLE_ARRAY_METHOD(Uniform1iv, Uniform1iv, 1, GL_INT, GLint)
GL_SIMPLE_ARRAY_METHOD(Uniform2iv, Uniform2iv, 2, GL_INT, GLint)
GL_SIMPLE_ARRAY_METHOD(Uniform3iv, Uniform3iv, 3, GL_INT, GLint)
GL_SIMPLE_ARRAY_METHOD(Uniform4iv, Uniform4iv, 4, GL_INT, GLint)

GL_SIMPLE_ARRAY_METHOD(Uniform1fv, Uniform1fv, 1, GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD(Uniform2fv, Uniform2fv, 2, GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD(Uniform3fv, Uniform3fv, 3, GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD(Uniform4fv, Uniform4fv, 4, GL_FLOAT, GLfloat)

GL_SIMPLE_MATRIX_METHOD(UniformMatrix2fv, UniformMatrix2fv, 4, GL_FLOAT, GLfloat)
GL_SIMPLE_MATRIX_METHOD(UniformMatrix3fv, UniformMatrix3fv, 9, GL_FLOAT, GLfloat)
GL_SIMPLE_MATRIX_METHOD(UniformMatrix4fv, UniformMatrix4fv, 16, GL_FLOAT, GLfloat)

GL_SAME_METHOD_1(UseProgram, UseProgram, PRUint32)

GL_SAME_METHOD_1(ValidateProgram, ValidateProgram, PRUint32)

GL_SAME_METHOD_2(VertexAttrib1f, VertexAttrib1f, PRUint32, float)
GL_SAME_METHOD_3(VertexAttrib2f, VertexAttrib2f, PRUint32, float, float)
GL_SAME_METHOD_4(VertexAttrib3f, VertexAttrib3f, PRUint32, float, float, float)
GL_SAME_METHOD_5(VertexAttrib4f, VertexAttrib4f, PRUint32, float, float, float, float)

GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib1fv, VertexAttrib1fv, 1, GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib2fv, VertexAttrib2fv, 2, GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib3fv, VertexAttrib3fv, 3, GL_FLOAT, GLfloat)
GL_SIMPLE_ARRAY_METHOD_NO_COUNT(VertexAttrib4fv, VertexAttrib4fv, 4, GL_FLOAT, GLfloat)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GenFramebuffers(PRUint32 n)
{
    if (n == 0) return NS_OK;
    if (n > 0xffffu) return NS_ERROR_INVALID_ARG;

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    nsAutoTArray<PRUint32, 16> ids;
    ids.SetCapacity(n);

    MakeContextCurrent();
    gl->fGenFramebuffers(n, (GLuint*) ids.Elements());

    js.SetRetVal(ids.Elements(), n);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GenRenderbuffers(PRUint32 n)
{
    if (n == 0) return NS_OK;
    if (n > 0xffffu) return NS_ERROR_INVALID_ARG;

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    nsAutoTArray<PRUint32, 16> ids;
    ids.SetCapacity(n);

    MakeContextCurrent();
    gl->fGenRenderbuffers(n, (GLuint*) ids.Elements());

    js.SetRetVal(ids.Elements(), n);

    return NS_OK;
}

GL_SAME_METHOD_4(Viewport, Viewport, PRInt32, PRInt32, PRInt32, PRInt32)

GL_SAME_METHOD_1(CompileShader, CompileShader, PRUint32)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetShaderParameter(PRUint32 shader, PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    switch (pname) {
        case GL_SHADER_TYPE:
        case GL_DELETE_STATUS:
        case GL_COMPILE_STATUS:
        case GL_INFO_LOG_LENGTH:
        case GL_SHADER_SOURCE_LENGTH:
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
nsCanvasRenderingContextGLWeb20::GetShaderInfoLog(PRUint32 shader, char **retval)
{
    MakeContextCurrent();

    PRInt32 k = -1;
    gl->fGetShaderiv(shader, GL_INFO_LOG_LENGTH, (GLint*) &k);
    if (k == -1)
        return NS_ERROR_FAILURE;

    if (k == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    char *s = (char *) PR_Malloc(k);
    if (!s)
        return NS_ERROR_OUT_OF_MEMORY;

    gl->fGetShaderInfoLog(shader, k, (GLint*) &k, s);

    *retval = s;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetShaderSource(PRUint32 shader, char **retval)
{
    MakeContextCurrent();

    GLint slen = -1;
    gl->fGetShaderiv (shader, GL_SHADER_SOURCE_LENGTH, &slen);
    if (slen == -1)
        return NS_ERROR_FAILURE;

    if (slen == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    char *src = (char *) nsMemory::Alloc(slen + 1);

    gl->fGetShaderSource (shader, slen, NULL, src);

    *retval = src;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::ShaderSource(PRUint32 shader, const char *source)
{
    if (!source)
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();

    gl->fShaderSource(shader, 1, &source, NULL);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::SwapBuffers()
{
    return DoSwapBuffers();
}

/*in PRUint32 index, in PRInt32 size, in PRUint32 type, in PRBool normalized, in PRUint32 stride, in Object[] array*/
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::VertexAttribPointer()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    nsRefPtr<CanvasGLBuffer> newBuffer;
    nsresult rv;

    if (js.argc != 6)
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint vertexAttribIndex;
    jsuint sizeParam;
    jsuint typeParam;
    JSBool normalizedParam;
    jsuint strideParam;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuubu",
                               &vertexAttribIndex, &sizeParam, &typeParam,
                               &normalizedParam, &strideParam))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (vertexAttribIndex >= mAttribBuffers.Length())
        return NS_ERROR_INVALID_ARG;

    if (typeParam != GL_SHORT && typeParam != GL_FLOAT) {
        LogMessage(NS_LITERAL_CSTRING("vertexAttribPointer: invalid element type"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (sizeParam < 1 || sizeParam > 4) {
        LogMessage(NS_LITERAL_CSTRING("vertexAttribPointer: invalid element size"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (strideParam != 0) {
        LogMessage(NS_LITERAL_CSTRING("vertexAttribPointer: stride must be 0 for now"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    MakeContextCurrent();

    GLint array_buf = 0;
    gl->fGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buf);

    // Are we being given a literal array, or are we being asked
    // to use the currently bound VBO?  If we're being asked to use a VBO,
    // then the 6th arg is an offset and not an array.
    JSObject *arrayObj;
    jsuint arrayLen;
    if (NativeJSContext::JSValToJSArrayAndLength(js.ctx, js.argv[5], &arrayObj, &arrayLen)) {
        // if we were given an array, we must not have a buffer binding
        if (array_buf != 0) {
            LogMessage(NS_LITERAL_CSTRING("vertexAttribPointer: called with array arg while ARRAY_BUFFER_BINDING != 0!"));
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        newBuffer = new CanvasGLBuffer(this);
        if (!newBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = newBuffer->Init (GL_STATIC_DRAW, sizeParam, typeParam, js.ctx, arrayObj, arrayLen);
        if (NS_FAILED(rv))
            return rv;

        mAttribBuffers[vertexAttribIndex] = newBuffer;

        gl->fVertexAttribPointer(vertexAttribIndex,
                                 newBuffer->GetSimpleBuffer().sizePerVertex,
                                 newBuffer->GetSimpleBuffer().type,
                                 normalizedParam ? GL_TRUE : GL_FALSE,
                                 strideParam,
                                 newBuffer->GetSimpleBuffer().data);
    } else {
        // grab the buffer offset
        jsuint bufferOffset;
        if (!::JS_ValueToECMAUint32(js.ctx, js.argv[3], &bufferOffset))
            return NS_ERROR_INVALID_ARG;

        int sz = (typeParam == GL_SHORT ? 2 : 4);

        GLint len = 0;
        gl->fGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &len);

        if (len < 0 || bufferOffset*sz > (GLuint)len) {
            LogMessage(NS_LITERAL_CSTRING("vertexAttribPointer: offset out of buffer bounds"));
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        mAttribBuffers[vertexAttribIndex] = NULL;

        gl->fVertexAttribPointer(vertexAttribIndex,
                                 sizeParam,
                                 typeParam,
                                 normalizedParam ? GL_TRUE : GL_FALSE,
                                 strideParam,
                                 (GLvoid*)(bufferOffset*sz));
    }

    return NS_OK;
}

PRBool
nsCanvasRenderingContextGLWeb20::ValidateGL()
{
    // make sure that the opengl stuff that we need is supported
    GLint val = 0;
    gl->fGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &val);
    fprintf (stderr, "-- %d vertex buffers\n", (int)val);
    mAttribBuffers.SetLength(val);
    mBuffers.SetLength(256);

    // gl_PointSize is always available in ES2 GLSL
    gl->fEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    return PR_TRUE;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::TexSubImage2D()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 9) {
        LogMessage(NS_LITERAL_CSTRING("texSubImage2D: expected 9 arguments"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    jsuint argTarget, argLevel, argX, argY, argWidth, argHeight, argFormat, argType;
    JSObject *argPixelsObj;
    jsuint argPixelsLen;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuuuuuuuo",
                               &argTarget, &argLevel, &argX, &argY,
                               &argWidth, &argHeight, &argFormat, &argType,
                               &argPixelsObj) ||
        JSVAL_IS_NULL(argPixelsObj) ||
        !::JS_IsArrayObject(js.ctx, argPixelsObj) ||
        !::JS_GetArrayLength(js.ctx, argPixelsObj, &argPixelsLen))
    {
        LogMessage(NS_LITERAL_CSTRING("texSubImage2D: argument error"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (argTarget) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texSubImage2D: unsupported target"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    PRUint32 bufferType, bufferSize;
    switch (argFormat) {
        case GL_RED:
        case GL_GREEN:
        case GL_BLUE:
        case GL_ALPHA:
        case GL_LUMINANCE:
            bufferSize = 1;
            break;
        case GL_LUMINANCE_ALPHA:
            bufferSize = 2;
            break;
        case GL_RGB:
            bufferSize = 3;
            break;
        case GL_RGBA:
            bufferSize = 4;
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texSubImage2D: pixel format not supported"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (argType) {
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            bufferType = argType;
            break;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_5_6_5:
            bufferType = GL_UNSIGNED_SHORT;
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texSubImage2D: pixel packing not supported"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    // make sure the size is valid
    PRInt32 tmp = argWidth * argHeight;
    if (tmp && tmp / argHeight != argWidth) {
        LogMessage(NS_LITERAL_CSTRING("texSubImage2D: too large width or height"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    tmp = tmp * bufferSize;
    if (tmp && tmp / bufferSize != (argWidth * argHeight)) {
        LogMessage(NS_LITERAL_CSTRING("texSubImage2D: too large width or height (after multiplying with pixel size)"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if ((PRUint32) tmp > argPixelsLen) {
        LogMessage(NS_LITERAL_CSTRING("texSubImage2D: array dimensions too small for width, height and pixel format"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer(bufferType, bufferSize, js.ctx, argPixelsObj, argPixelsLen);
    if (!sbuffer.Valid())
        return NS_ERROR_FAILURE;

    MakeContextCurrent();
    gl->fTexSubImage2D (argTarget, argLevel, argX, argY, argWidth, argHeight, argFormat, argType, (void *) sbuffer.data);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::TexImage2D()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 9) {
        LogMessage(NS_LITERAL_CSTRING("texImage2D: expected 9 arguments"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    jsuint argTarget, argLevel, argInternalFormat, argWidth, argHeight, argBorder, argFormat, argType;
    JSObject *argPixelsObj;
    jsuint argPixelsLen;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuuuuuuuo",
                               &argTarget, &argLevel, &argInternalFormat, &argWidth,
                               &argHeight, &argBorder, &argFormat, &argType,
                               &argPixelsObj) ||
        (argPixelsObj != NULL && !JSVAL_IS_NULL(argPixelsObj) && (
        !::JS_IsArrayObject(js.ctx, argPixelsObj) ||
        !::JS_GetArrayLength(js.ctx, argPixelsObj, &argPixelsLen))))
    {
        LogMessage(NS_LITERAL_CSTRING("texImage2D: argument error"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (argWidth == 0 || argHeight == 0) {
        LogMessage(NS_LITERAL_CSTRING("texImage2D: width or height is zero"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (argTarget) {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texImage2D: unsupported target"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (argBorder != 0) {
        LogMessage(NS_LITERAL_CSTRING("texImage2D: non-zero border given"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (argInternalFormat) {
        case GL_RGB:
        case GL_RGBA:
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_LUMINANCE_ALPHA:
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texImage2D: internal format not supported"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    PRUint32 bufferType, bufferSize;
    switch (argFormat) {
        case GL_RED:
        case GL_GREEN:
        case GL_BLUE:
        case GL_ALPHA:
        case GL_LUMINANCE:
            bufferSize = 1;
            break;
        case GL_LUMINANCE_ALPHA:
            bufferSize = 2;
            break;
        case GL_RGB:
            bufferSize = 3;
            break;
        case GL_RGBA:
            bufferSize = 4;
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texImage2D: pixel format not supported"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    switch (argType) {
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            bufferType = argType;
            break;
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_5_6_5:
            bufferType = GL_UNSIGNED_SHORT;
            break;
        default:
            LogMessage(NS_LITERAL_CSTRING("texImage2D: pixel packing not supported"));
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    // make sure the size is valid
    PRInt32 tmp = argWidth * argHeight;
    if (tmp && tmp / argHeight != argWidth) {
        LogMessage(NS_LITERAL_CSTRING("texImage2D: too large width or height"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    tmp = tmp * bufferSize;
    if (tmp && tmp / bufferSize != (argWidth * argHeight)) {
        LogMessage(NS_LITERAL_CSTRING("texImage2D: too large width or height (after multiplying with pixel size)"));
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    // XXX handle GL_UNPACK_ALIGNMENT !

    if (argPixelsObj == NULL || JSVAL_IS_NULL(argPixelsObj)) {
      MakeContextCurrent();
      gl->fTexImage2D (argTarget, argLevel, argInternalFormat, argWidth, argHeight, argBorder, argFormat, argType, NULL);
    } else {
        if ((PRUint32) tmp > argPixelsLen) {
            LogMessage(NS_LITERAL_CSTRING("texImage2D: array dimensions too small for width, height and pixel format"));
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        SimpleBuffer sbuffer(bufferType, bufferSize, js.ctx, argPixelsObj, argPixelsLen);
        if (!sbuffer.Valid())
            return NS_ERROR_FAILURE;

        MakeContextCurrent();
        gl->fTexImage2D (argTarget, argLevel, argInternalFormat, argWidth, argHeight, argBorder, argFormat, argType, (void *) sbuffer.data);
    }
    return NS_OK;
}

PRBool
nsCanvasRenderingContextGLWeb20::ValidateBuffers(PRUint32 count)
{
    GLint len = 0;
    GLint enabled = 0, size = 4, type = GL_FLOAT, binding = 0;
    PRBool someEnabled = PR_FALSE;
    GLint currentProgram = -1;
    GLint numAttributes = -1;

    MakeContextCurrent();

    gl->fGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    if (currentProgram == -1) {
        // what?
        LogMessagef("glGetIntegerv GL_CURRENT_PROGRAM failed: 0x%08x", (uint) gl->fGetError());
        return PR_FALSE;
    }

    gl->fGetProgramiv(currentProgram, GL_ACTIVE_ATTRIBUTES, &numAttributes);
    if (numAttributes == -1) {
        // what?
        LogMessagef("glGetProgramiv GL_ACTIVE_ATTRIBUTES failed: 0x%08x", (uint) gl->fGetError());
        return PR_FALSE;
    }

    // is this valid?
    if (numAttributes > (GLint) mAttribBuffers.Length()) {
        // what?
        LogMessagef("GL_ACTIVE_ATTRIBUTES > GL_MAX_VERTEX_ATTRIBS");
        return PR_FALSE;
    }
    PRUint32 maxAttribs = numAttributes;

    for (PRUint32 i = 0; i < maxAttribs; ++i) {
        GLsizei nameBufSz = 256;
        GLchar nameBuf[256];
        GLint vaSize;
        GLenum vaType;
        GLint attribLoc = -1;

        gl->fGetActiveAttrib(currentProgram, i, nameBufSz, &nameBufSz, &vaSize, &vaType, nameBuf);
        attribLoc = gl->fGetAttribLocation(currentProgram, nameBuf);

        if (attribLoc == -1) {
            LogMessagef(("Couldn't find an active attrib by name?"));
            return PR_FALSE;
        }

        enabled = 0;
        gl->fGetVertexAttribiv(attribLoc, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);

        if (enabled) {
            binding = 0;
            gl->fGetVertexAttribiv(attribLoc, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &binding);

            // figure out element size and type
            size = -1;
            type = GL_FLOAT;

            // Note that the VERTEX_ATTRIB_ARRAY_{SIZE,TYPE} don't matter,
            // just what the type is of the buffer since GL will convert/expand.

            if (binding) {
                if (binding < 0 || binding >= (GLint) mBuffers.Length() || !mBuffers[binding]) {
                    LogMessagef(("ValidateBuffers: invalid buffer bound"));
                    return PR_FALSE;
                }
                len = mBuffers[binding]->GetSimpleBuffer().capacity;
                size = -1;
                gl->fGetVertexAttribiv(attribLoc, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
                if (size <= 0) {
                    LogMessagef(("ValidateBuffers: invalid VertexAttribPointer size"));
                    return PR_FALSE;
                }
                type = mBuffers[binding]->Type();
            } else {
                if (!mAttribBuffers[attribLoc]) {
                    LogMessagef(("ValidateBuffers: invalid VertexAttribPointer"));
                    return PR_FALSE;
                }
                len = mAttribBuffers[attribLoc]->GetSimpleBuffer().capacity;
                size = mAttribBuffers[attribLoc]->Size();
                type = mAttribBuffers[attribLoc]->Type();
            }

            switch (type) {
                case GL_FLOAT:
                    size *= 4;
                    break;
                case GL_SHORT:
                case GL_UNSIGNED_SHORT:
                // case GL_FIXED:
                    size *= 2;
                    break;
                case GL_UNSIGNED_BYTE:
                case GL_BYTE:
                    break;
                default:
                    LogMessagef("ValidateBuffers: bad GL_VERTEX_ATTRIB_ARRAY_TYPE %d", type);
                    return PR_FALSE;
            }

            if (len <= 0 || size <= 0 || count > (PRUint32)(len / size)) {
                LogMessagef(("ValidateBuffers: trying to draw out of bounds"));
                return PR_FALSE;
            }

            someEnabled = PR_TRUE;
        }
    }

    if (!someEnabled) {
        LogMessagef(("ValidateBuffers: no vertex attribs enabled"));
        return PR_FALSE;
    }

    return PR_TRUE;
}

PRBool
BaseTypeAndSizeFromUniformType(GLenum uType, GLenum *baseType, GLint *unitSize)
{
        switch (uType) {
        case GL_INT:
        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_CUBE:
            *baseType = GL_INT;
            break;
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT4:
            *baseType = GL_FLOAT;
            break;
        case GL_BOOL:
        case GL_BOOL_VEC2:
        case GL_BOOL_VEC3:
        case GL_BOOL_VEC4:
            *baseType = GL_INT; // pretend these are int
            break;
        default:
            return PR_FALSE;
    }

    switch (uType) {
        case GL_INT:
        case GL_FLOAT:
        case GL_BOOL:
        case GL_SAMPLER_2D:
        case GL_SAMPLER_CUBE:
            *unitSize = 1;
            break;
        case GL_INT_VEC2:
        case GL_FLOAT_VEC2:
        case GL_BOOL_VEC2:
            *unitSize = 2;
            break;
        case GL_INT_VEC3:
        case GL_FLOAT_VEC3:
        case GL_BOOL_VEC3:
            *unitSize = 3;
            break;
        case GL_INT_VEC4:
        case GL_FLOAT_VEC4:
        case GL_BOOL_VEC4:
            *unitSize = 4;
            break;
        case GL_FLOAT_MAT2:
            *unitSize = 4;
            break;
        case GL_FLOAT_MAT3:
            *unitSize = 9;
            break;
        case GL_FLOAT_MAT4:
            *unitSize = 16;
            break;
        default:
            return PR_FALSE;
    }

    return PR_TRUE;
}

// ImageData getImageData (in float x, in float y, in float width, in float height);
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetImageData(PRUint32 x, PRUint32 y, PRUint32 w, PRUint32 h)
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
