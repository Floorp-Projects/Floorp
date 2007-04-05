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

#define NSGL_CONTEXT_NAME nsCanvasRenderingContextGLWeb20

#include "nsCanvasRenderingContextGL.h"
#include "nsICanvasRenderingContextGLWeb20.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsIView.h"
#include "nsIViewManager.h"

#ifndef MOZILLA_1_8_BRANCH
#include "nsIDocument.h"
#endif

#include "nsTransform2D.h"

#include "nsIScriptSecurityManager.h"
#include "nsISecurityCheckedComponent.h"

#include "nsWeakReference.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsICanvasElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIImage.h"
#include "nsIFrame.h"
#include "nsDOMError.h"
#include "nsIJSRuntimeService.h"

#ifndef MOZILLA_1_8_BRANCH
#include "nsIClassInfoImpl.h"
#endif

#include "nsServiceManagerUtils.h"

#include "nsDOMError.h"

#include "nsContentUtils.h"

#include "nsIXPConnect.h"
#include "jsapi.h"

#ifdef MOZ_CAIRO_GFX
#include "gfxContext.h"
#include "gfxASurface.h"
#endif

#ifdef XP_WIN
#include <windows.h>
#endif

// GLEW will pull in the GL bits that we want/need
#include "glew.h"

// we're hoping that something is setting us up the remap

#include "cairo.h"
#include "glitz.h"

#ifdef MOZ_X11
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "cairo-xlib.h"
#include "glitz-glx.h"
#endif

#ifdef XP_WIN
#include "cairo-win32.h"
#include "glitz-wgl.h"
#endif

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
};


// nsCanvasRenderingContextGLWeb20
// NS_DECL_CLASSINFO lives in nsCanvas3DModule
NS_IMPL_ADDREF(nsCanvasRenderingContextGLWeb20)
NS_IMPL_RELEASE(nsCanvasRenderingContextGLWeb20)

NS_IMPL_CI_INTERFACE_GETTER4(nsCanvasRenderingContextGLWeb20,
                             nsICanvasRenderingContextGL,
                             nsICanvasRenderingContextGLWeb20,
                             nsICanvasRenderingContextInternal,
                             nsISupportsWeakReference)

NS_INTERFACE_MAP_BEGIN(nsCanvasRenderingContextGLWeb20)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGL)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGLWeb20)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextGL)
  NS_IMPL_QUERY_CLASSINFO(nsCanvasRenderingContextGLWeb20)
NS_INTERFACE_MAP_END

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
    *_retval = nsnull;
    return NS_OK;
}

/* void activeTexture (in PRUint32 texture); */
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::ActiveTexture(PRUint32 texture)
{
    if (!GLEW_ARB_multitexture) {
        if (texture == GL_TEXTURE0)
            return NS_OK;
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
        return NS_ERROR_DOM_SYNTAX_ERR;

    glActiveTexture(texture);
    return NS_OK;
}

GL_SAME_METHOD_2(AttachShader, AttachShader, PRUint32, PRUint32)

GL_SAME_METHOD_3(BindAttribLocation, BindAttribLocation, PRUint32, PRUint32, const char*)

GL_SAME_METHOD_2(BindBuffer, BindBuffer, PRUint32, PRUint32)

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

    if (js.argc != 3)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    jsuint target;
    jsuint type;
    jsuint usage;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uouu", &target, &arrayObj, &type, &usage) ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer;
    nsresult rv = JSArrayToSimpleBuffer(sbuffer, type, 1, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glBufferData(target, sbuffer.length * sbuffer.ElementSize(), sbuffer.data, usage);
    return NS_OK;
}

/* target, offset, array, type */
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::BufferSubData()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    jsuint target;
    jsuint offset;
    jsuint type;
    if (!::JS_ConvertArguments(js.ctx, js.argc, js.argv, "uuou", &target, &offset, &arrayObj, &type) ||
        !::JS_IsArrayObject(js.ctx, arrayObj) ||
        !::JS_GetArrayLength(js.ctx, arrayObj, &arrayLen))
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    SimpleBuffer sbuffer;
    nsresult rv = JSArrayToSimpleBuffer(sbuffer, type, 1, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glBufferSubData(target, offset, sbuffer.length * sbuffer.ElementSize(), sbuffer.data);
    return NS_OK;
}

GL_SAME_METHOD_1(Clear, Clear, PRUint32);

GL_SAME_METHOD_4(ClearColor, ClearColor, float, float, float, float);

GL_SAME_METHOD_1(ClearDepth, ClearDepth, float);

GL_SAME_METHOD_1(ClearStencil, ClearStencil, PRInt32);

GL_SAME_METHOD_4(ColorMask, ColorMask, PRBool, PRBool, PRBool, PRBool);

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::CreateProgram(PRUint32 *retval)
{
    MakeContextCurrent();
    *retval = glCreateProgram();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::CreateShader(PRUint32 type, PRUint32 *retval)
{
    MakeContextCurrent();
    *retval = glCreateShader(type);
    return NS_OK;
}

GL_SAME_METHOD_1(CullFace, CullFace, PRUint32);

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DeleteBuffers()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (arrayLen == 0)
        return NS_OK;

    SimpleBuffer sbuffer;
    nsresult rv = JSArrayToSimpleBuffer(sbuffer, GL_UNSIGNED_INT, 1, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glDeleteBuffers(arrayLen, (GLuint*) sbuffer.data);

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
    if (!JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (arrayLen == 0)
        return NS_OK;

    SimpleBuffer sbuffer;
    nsresult rv = JSArrayToSimpleBuffer(sbuffer, GL_UNSIGNED_INT, 1, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glDeleteTextures(arrayLen, (GLuint*) sbuffer.data);

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

GL_SAME_METHOD_3(DrawArrays, DrawArrays, PRUint32, PRUint32, PRUint32)

// DrawElements
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::DrawElements()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

GL_SAME_METHOD_1(Enable, Enable, PRUint32)

GL_SAME_METHOD_1(EnableVertexAttribArray, EnableVertexAttribArray, PRUint32)

GL_SAME_METHOD_1(FrontFace, FrontFace, PRUint32)

// returns an object: { size: ..., type: ..., name: ... }
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetActiveAttrib(PRUint32 program, PRUint32 index)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    int len;
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &len);
    if (len == 0)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<char> name = new char[len+1];
    PRInt32 attrsize;
    PRUint32 attrtype;

    glGetActiveAttrib(program, index, len+1, &len, &attrsize, &attrtype, name);

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("size", attrsize);
    retobj.DefineProperty("type", attrtype);
    retobj.DefineProperty("name", name, len);

    js.SetRetVal(OBJECT_TO_JSVAL(retobj.Object()));

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetActiveUniform(PRUint32 program, PRUint32 index)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    int len;
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &len);
    if (len == 0)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<char> name = new char[len+1];
    PRInt32 attrsize;
    PRUint32 attrtype;

    glGetActiveUniform(program, index, len+1, &len, &attrsize, &attrtype, name);

    JSObjectHelper retobj(&js);
    retobj.DefineProperty("size", attrsize);
    retobj.DefineProperty("type", attrtype);
    retobj.DefineProperty("name", name, len);

    js.SetRetVal(OBJECT_TO_JSVAL(retobj.Object()));

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetAttachedShaders(PRUint32 program)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    int count;
    glGetProgramiv(program, GL_ATTACHED_SHADERS, &count);

    nsAutoArrayPtr<PRUint32> shaders = new PRUint32[count];

    glGetAttachedShaders(program, count, NULL, shaders);

    JSObject *obj = ArrayToJSArray(js.ctx, shaders, count);

    js.AddGCRoot(obj, "GetAttachedShaders");
    js.SetRetVal(OBJECT_TO_JSVAL(obj));
    js.ReleaseGCRoot(obj);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetAttribLocation(PRUint32 program,
                                                   const char *name,
                                                   PRInt32 *retval)
{
    MakeContextCurrent();
    *retval = glGetAttribLocation(program, name);
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
        ////case GL_MAX_RENDERBUFFER_SIZE:
        case GL_RED_BITS:
        case GL_GREEN_BITS:
        case GL_BLUE_BITS:
        case GL_ALPHA_BITS:
        case GL_DEPTH_BITS:
        case GL_STENCIL_BITS:
        //case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        //case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        ////case GL_RENDERBUFFER_BINDING:
        ////case GL_FRAMEBUFFER_BINDING:
        {
            PRInt32 iv;
            glGetIntegerv(pname, &iv);
            js.SetRetVal(iv);
        }
            break;

// float
        case GL_LINE_WIDTH:
        case GL_POLYGON_OFFSET_FACTOR:
        case GL_POLYGON_OFFSET_UNITS:
        case GL_SAMPLE_COVERAGE_VALUE:
        {
            float fv;
            glGetFloatv(pname, &fv);
            js.SetRetVal((double) fv);
        }
            break;
// bool
        case GL_SAMPLE_COVERAGE_INVERT:
        case GL_COLOR_WRITEMASK:
        case GL_DEPTH_WRITEMASK:
        ////case GL_SHADER_COMPILER: // pretty much must be true 
        {
            GLboolean bv;
            glGetBooleanv(pname, &bv);
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
            float fv[2];
            glGetFloatv(pname, &fv[0]);
            js.SetRetVal(fv, 2);
        }
            break;
        
        case GL_COLOR_CLEAR_VALUE: // 4 floats
        case GL_BLEND_COLOR: // 4 floats
        {
            float fv[4];
            glGetFloatv(pname, &fv[0]);
            js.SetRetVal(fv, 4);
        }
            break;

        case GL_MAX_VIEWPORT_DIMS: // 2 ints
        {
            PRInt32 iv[2];
            glGetIntegerv(pname, &iv[0]);
            js.SetRetVal(iv, 2);
        }
            break;

        case GL_SCISSOR_BOX: // 4 ints
        case GL_VIEWPORT: // 4 ints
        {
            PRInt32 iv[4];
            glGetIntegerv(pname, &iv[0]);
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
            PRInt32 iv;
            glGetBufferParameteriv(target, pname, &iv);
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
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (n == 0)
        return NS_OK;

    MakeContextCurrent();

    nsAutoArrayPtr<PRUint32> buffers(new PRUint32[n]);
    glGenBuffers(n, buffers.get());

    nsAutoArrayPtr<jsval> jsvector(new jsval[n]);
    for (PRUint32 i = 0; i < n; i++)
        jsvector[i] = INT_TO_JSVAL(buffers[i]);

    JSObject *obj = JS_NewArrayObject(js.ctx, n, jsvector);
    jsval *retvalPtr;
    js.ncc->GetRetValPtr(&retvalPtr);
    *retvalPtr = OBJECT_TO_JSVAL(obj);
    js.ncc->SetReturnValueWasSet(PR_TRUE);
    
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GenTextures(PRUint32 n)
{
    if (n == 0)
        return NS_OK;

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    nsAutoArrayPtr<PRUint32> textures(new PRUint32[n]);
    glGenTextures(n, textures.get());

    nsAutoArrayPtr<jsval> jsvector(new jsval[n]);
    for (PRUint32 i = 0; i < n; i++)
        jsvector[i] = INT_TO_JSVAL(textures[i]);

    JSObject *obj = JS_NewArrayObject(js.ctx, n, jsvector);
    jsval *retvalPtr;
    js.ncc->GetRetValPtr(&retvalPtr);
    *retvalPtr = OBJECT_TO_JSVAL(obj);
    js.ncc->SetReturnValueWasSet(PR_TRUE);
    
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetError(PRUint32 *_retval)
{
    MakeContextCurrent();
    *_retval = glGetError();
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
            PRInt32 iv;
            glGetProgramiv(program, pname, &iv);
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
    PRInt32 k;

    MakeContextCurrent();

    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &k);
    if (k == 0) {
        *retval = nsnull;
        return NS_OK;
    }

    char *s = (char *) PR_Malloc(k);

    glGetProgramInfoLog(program, k, &k, s);

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
        (!GLEW_ARB_texture_rectangle || targetVal != GL_TEXTURE_RECTANGLE_ARB))
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
            glTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case GL_TEXTURE_MAG_FILTER: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_NEAREST &&
                 ival != GL_LINEAR))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexParameteri (targetVal, pnameVal, ival);
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
            glTexParameteri (targetVal, pnameVal, ival);
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
            glTexParameteri (targetVal, pnameVal, ival);
        }
            break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT: {
            if (GLEW_EXT_texture_filter_anisotropic) {
                jsdouble dval;
                if (!::JS_ValueToNumber(js.ctx, js.argv[2], &dval))
                    return NS_ERROR_DOM_SYNTAX_ERR;
                glTexParameterf (targetVal, pnameVal, (float) dval);
            } else {
                return NS_ERROR_NOT_IMPLEMENTED;
            }
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
            PRInt32 iv;
            glGetTexParameteriv(target, pname, &iv);
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
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetUniformLocation(PRUint32 program, const char *name, PRInt32 *retval)
{
    MakeContextCurrent();
    *retval = glGetUniformLocation(program, name);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetVertexAttrib(PRUint32 index, PRUint32 pname)
{
    // ...
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::Hint(PRUint32 target, PRUint32 mode)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsBuffer(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = glIsBuffer(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsEnabled(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = glIsEnabled(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsProgram(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = glIsProgram(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsShader(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = glIsShader(k);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::IsTexture(PRUint32 k, PRBool *retval)
{
    MakeContextCurrent();
    *retval = glIsTexture(k);
    return NS_OK;
}

GL_SAME_METHOD_1(LineWidth, LineWidth, float)

GL_SAME_METHOD_1(LinkProgram, LinkProgram, PRUint32)

GL_SAME_METHOD_2(PolygonOffset, PolygonOffset, float, float)

GL_SAME_METHOD_2(SampleCoverage, SampleCoverage, float, PRBool)

GL_SAME_METHOD_4(Scissor, Scissor, PRInt32, PRInt32, PRInt32, PRInt32)

GL_SAME_METHOD_3(StencilFunc, StencilFunc, PRUint32, PRInt32, PRUint32)

GL_SAME_METHOD_4(StencilFuncSeparate, StencilFuncSeparate, PRUint32, PRUint32, PRInt32, PRUint32)

GL_SAME_METHOD_1(StencilMask, StencilMask, PRUint32)

GL_SAME_METHOD_2(StencilMaskSeparate, StencilMaskSeparate, PRUint32, PRUint32)

GL_SAME_METHOD_3(StencilOp, StencilOp, PRUint32, PRUint32, PRUint32)

GL_SAME_METHOD_4(StencilOpSeparate, StencilOpSeparate, PRUint32, PRUint32, PRUint32, PRUint32)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::TexImage2DHTML(PRUint32 target, nsIDOMHTMLElement *imageOrCanvas)
{
    nsresult rv;
    cairo_surface_t *cairo_surf = nsnull;
    PRUint8 *image_data = nsnull, *local_image_data = nsnull;
    PRInt32 width, height;
    nsCOMPtr<nsIURI> element_uri;
    PRBool force_write_only = PR_FALSE;

    rv = CairoSurfaceFromElement(imageOrCanvas, &cairo_surf, &image_data,
                                 &width, &height, getter_AddRefs(element_uri), &force_write_only);
    if (NS_FAILED(rv))
        return rv;

    DoDrawImageSecurityCheck(element_uri, force_write_only);

    if (target == GL_TEXTURE_2D) {
        if ((width & (width-1)) != 0 ||
            (height & (height-1)) != 0)
            return NS_ERROR_INVALID_ARG;
    } else if (GLEW_ARB_texture_rectangle && target == GL_TEXTURE_RECTANGLE_ARB) {
        if (width == 0 || height == 0)
            return NS_ERROR_INVALID_ARG;
    } else {
        return NS_ERROR_INVALID_ARG;
    }

    if (!image_data) {
        local_image_data = (PRUint8*) PR_Malloc(width * height * 4);
        if (!local_image_data)
            return NS_ERROR_FAILURE;

        cairo_surface_t *tmp = cairo_image_surface_create_for_data (local_image_data,
                                                                    CAIRO_FORMAT_ARGB32,
                                                                    width, height, width * 4);
        if (!tmp) {
            PR_Free(local_image_data);
            return NS_ERROR_FAILURE;
        }

        cairo_t *tmp_cr = cairo_create (tmp);
        cairo_set_source_surface (tmp_cr, cairo_surf, 0, 0);
        cairo_set_operator (tmp_cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint (tmp_cr);
        cairo_destroy (tmp_cr);
        cairo_surface_destroy (tmp);

        image_data = local_image_data;
    }

    // Er, I can do this with glPixelStore, no?
    // the incoming data will /always/ be
    // (A << 24) | (R << 16) | (G << 8) | B
    // in a native-endian 32-bit int.
    PRUint8* ptr = image_data;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
#ifdef IS_LITTLE_ENDIAN
            PRUint8 b = ptr[0];
            PRUint8 g = ptr[1];
            PRUint8 r = ptr[2];
            PRUint8 a = ptr[3];
#else
            PRUint8 a = ptr[0];
            PRUint8 r = ptr[1];
            PRUint8 g = ptr[2];
            PRUint8 b = ptr[3];
#endif
            ptr[0] = r;
            ptr[1] = g;
            ptr[2] = b;
            ptr[3] = a;
            ptr += 4;
        }
    }

    MakeContextCurrent();

    glTexImage2D(target, 0, 4, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    if (local_image_data)
        PR_Free(local_image_data);

    return NS_OK;
}

// two usages:
//  uniformi(idx, [a, b, c, d])
//  uniformi(idx, elcount, count, [......])
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::Uniformi()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 2 && js.argc != 4)
        return NS_ERROR_DOM_SYNTAX_ERR;

    int32 uniformIndex;
    jsuint elementCount = 0, uniformCount = 1;

    if (!::JS_ValueToInt32(js.ctx, js.argv[0], &uniformIndex))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (js.argc == 4 &&
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &elementCount) &&
        !::JS_ValueToECMAUint32(js.ctx, js.argv[2], &uniformCount))
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!JSValToJSArrayAndLength(js.ctx, js.argv[js.argc-1], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    SimpleBuffer sbuffer;
    nsresult rv = JSArrayToSimpleBuffer(sbuffer, GL_INT, 1, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (js.argc == 2) {
        elementCount = arrayLen;
        if (elementCount < 1 || elementCount > 4)
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (arrayLen < elementCount * uniformCount)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (elementCount) {
        case 1:
            glUniform1iv (uniformIndex, uniformCount, (GLint*) sbuffer.data);
            break;
        case 2:
            glUniform2iv (uniformIndex, uniformCount, (GLint*) sbuffer.data);
            break;
        case 3:
            glUniform3iv (uniformIndex, uniformCount, (GLint*) sbuffer.data);
            break;
        case 4:
            glUniform4iv (uniformIndex, uniformCount, (GLint*) sbuffer.data);
            break;
    }

    return NS_OK;
}

// two usages:
//  uniformf(idx, [a, b, c, d])
//  uniformf(idx, elcount, count, [......])
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::Uniformf()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 2 && js.argc != 4)
        return NS_ERROR_DOM_SYNTAX_ERR;

    int32 uniformIndex;
    jsuint elementCount = 0, uniformCount = 1;

    if (!::JS_ValueToInt32(js.ctx, js.argv[0], &uniformIndex))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (js.argc == 4 &&
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &elementCount) &&
        !::JS_ValueToECMAUint32(js.ctx, js.argv[2], &uniformCount))
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!JSValToJSArrayAndLength(js.ctx, js.argv[js.argc-1], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    SimpleBuffer sbuffer;
    nsresult rv = JSArrayToSimpleBuffer(sbuffer, GL_FLOAT, 1, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (js.argc == 2) {
        elementCount = arrayLen;
        if (elementCount < 1 || elementCount > 4)
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (arrayLen < elementCount * uniformCount)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (elementCount) {
        case 1:
            glUniform1fv (uniformIndex, uniformCount, (float*) sbuffer.data);
            break;
        case 2:
            glUniform2fv (uniformIndex, uniformCount, (float*) sbuffer.data);
            break;
        case 3:
            glUniform3fv (uniformIndex, uniformCount, (float*) sbuffer.data);
            break;
        case 4:
            glUniform4fv (uniformIndex, uniformCount, (float*) sbuffer.data);
            break;
    }

    return NS_OK;
}

// two usages:
//  uniformMatrix(idx, [4, 9, 16])
//  uniformMatrix(idx, elcount, count, [ 4*count, 9*count, 16*count ]
NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::UniformMatrix()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 2 && js.argc != 4)
        return NS_ERROR_DOM_SYNTAX_ERR;

    int32 uniformIndex;
    jsuint elementCount = 0, uniformCount = 1;

    if (!::JS_ValueToInt32(js.ctx, js.argv[0], &uniformIndex))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (js.argc == 4 &&
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &elementCount) &&
        !::JS_ValueToECMAUint32(js.ctx, js.argv[2], &uniformCount))
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!JSValToJSArrayAndLength(js.ctx, js.argv[js.argc-1], &arrayObj, &arrayLen))
        return NS_ERROR_DOM_SYNTAX_ERR;

    SimpleBuffer sbuffer;
    nsresult rv = JSArrayToSimpleBuffer(sbuffer, GL_FLOAT, 1, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (js.argc == 2) {
        if (arrayLen == 4)
            elementCount = 2;
        else if (arrayLen == 9)
            elementCount = 3;
        else if (arrayLen == 16)
            elementCount = 4;
        else
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (arrayLen < elementCount * uniformCount)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (elementCount) {
        case 2:
            glUniformMatrix2fv (uniformIndex, uniformCount, PR_FALSE, (float*) sbuffer.data);
            break;
        case 3:
            glUniformMatrix3fv (uniformIndex, uniformCount, PR_FALSE, (float*) sbuffer.data);
            break;
        case 4:
            glUniformMatrix4fv (uniformIndex, uniformCount, PR_FALSE, (float*) sbuffer.data);
            break;
    }

    return NS_OK;
}

GL_SAME_METHOD_1(UseProgram, UseProgram, PRUint32)

GL_SAME_METHOD_1(ValidateProgram, ValidateProgram, PRUint32)

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::VertexAttrib()
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
            PRInt32 iv;
            glGetShaderiv(shader, pname, &iv);
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
    PRInt32 k;

    MakeContextCurrent();

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &k);

    char *s = (char *) PR_Malloc(k);

    glGetShaderInfoLog(shader, k, &k, s);

    *retval = s;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::GetShaderSource(PRUint32 shader, char **retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::ShaderSource(PRUint32 shader, const char *source)
{
    MakeContextCurrent();

    glShaderSource(shader, 1, &source, NULL);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::SwapBuffers()
{
    return DoSwapBuffers();
}

NS_IMETHODIMP
nsCanvasRenderingContextGLWeb20::VertexAttribPointer()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    nsRefPtr<CanvasGLBuffer> newBuffer;
    nsresult rv;

    jsuint vertexAttribIndex;

    if (js.argc == 2) {
        if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &vertexAttribIndex))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<nsICanvasRenderingContextGLBuffer> bufferBase;
        rv = JSValToSpecificInterface(js.ctx, js.argv[1], (nsICanvasRenderingContextGLBuffer**) getter_AddRefs(bufferBase));
        if (NS_FAILED(rv))
            return rv;

        newBuffer = (CanvasGLBuffer*) bufferBase.get();

        if (newBuffer->mType != GL_SHORT &&
            newBuffer->mType != GL_FLOAT)
            return NS_ERROR_DOM_SYNTAX_ERR;

        if (newBuffer->mSize < 2 || newBuffer->mSize > 4)
            return NS_ERROR_DOM_SYNTAX_ERR;

    } else if (js.argc == 4) {
        jsuint sizeParam;
        jsuint typeParam;

        JSObject *arrayObj;
        jsuint arrayLen;

        if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &vertexAttribIndex) ||
            !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &sizeParam) ||
            !::JS_ValueToECMAUint32(js.ctx, js.argv[2], &typeParam) ||
            !JSValToJSArrayAndLength(js.ctx, js.argv[3], &arrayObj, &arrayLen))
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (typeParam != GL_SHORT &&
            typeParam != GL_FLOAT &&
            (sizeParam < 2 || sizeParam > 4))
            return NS_ERROR_DOM_SYNTAX_ERR;

        newBuffer = new CanvasGLBuffer(this);
        if (!newBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = newBuffer->Init (GL_STATIC_DRAW, sizeParam, typeParam, js.ctx, arrayObj, arrayLen);
        if (NS_FAILED(rv))
            return rv;
    } else {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    MakeContextCurrent();
    glVertexAttribPointer(vertexAttribIndex,
                          newBuffer->GetSimpleBuffer().sizePerVertex,
                          newBuffer->GetSimpleBuffer().type,
                          0,
                          0,
                          newBuffer->GetSimpleBuffer().data);
    return NS_OK;
}

PRBool
nsCanvasRenderingContextGLWeb20::ValidateGL()
{
    // make sure that the opengl stuff that we need is supported
    if (!GLEW_VERSION_2_0)
        return PR_FALSE;

    return PR_TRUE;
}
