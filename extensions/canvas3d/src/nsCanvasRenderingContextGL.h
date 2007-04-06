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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#ifndef _NSCANVASRENDERINGCONTEXTGL_H_
#define _NSCANVASRENDERINGCONTEXTGL_H_

#include "nsICanvasRenderingContextGL.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIDOMHTMLCanvasElement.h"

#include "nsICanvasGLPrivate.h"

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

#include "nsIServiceManager.h"
#include "nsIConsoleService.h"

#include "nsDOMError.h"

#include "nsContentUtils.h"

#include "nsIXPConnect.h"
#include "jsapi.h"

#include "cairo.h"
#include "glitz.h"
#include "glew.h"

#ifdef XP_WIN
#include <windows.h>
#endif

extern nsIXPConnect *gXPConnect;
extern JSRuntime *gScriptRuntime;
extern nsIJSRuntimeService *gJSRuntimeService;
extern PRBool gGlitzInitialized;

class nsCanvasRenderingContextGLES11;
class nsCanvasRenderingContextGLWeb20;

class nsCanvasRenderingContextGLPrivate :
    public nsICanvasRenderingContextInternal,
    public nsSupportsWeakReference
{
public:
    nsCanvasRenderingContextGLPrivate();
    virtual ~nsCanvasRenderingContextGLPrivate();

    virtual nsICanvasRenderingContextGL *GetSelf() = 0;

    virtual PRBool ValidateGL() { return PR_TRUE; }

    inline void MakeContextCurrent();
    static void LostCurrentContext(void *closure);

    inline GLEWContext *glewGetContext() {
        return &mGlewContext;
    }

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD Render(nsIRenderingContext *rc);
    NS_IMETHOD RenderToSurface(cairo_surface_t *surf);
    NS_IMETHOD GetInputStream(const nsACString& aMimeType,
                              const nsAString& aEncoderOptions,
                              nsIInputStream **aStream);

protected:
    PRBool SafeToCreateCanvas3DContext();

    virtual void Destroy();

    nsIFrame *GetCanvasLayoutFrame();

    nsresult DoSwapBuffers();

    nsresult CairoSurfaceFromElement(nsIDOMElement *imgElt,
                                     cairo_surface_t **aCairoSurface,
                                     PRUint8 **imgData,
                                     PRInt32 *widthOut, PRInt32 *heightOut,
                                     nsIURI **uriOut, PRBool *forceWriteOnlyOut);
    void DoDrawImageSecurityCheck(nsIURI* aURI, PRBool forceWriteOnly);

    GLEWContext mGlewContext;

    glitz_context_t* mGlitzContext;
    glitz_drawable_t* mGlitzDrawable;

    void *mContextToken;

    PRPackedBool mCurrent;

    PRInt32 mWidth, mHeight, mStride;
    PRUint8 *mImageBuffer;

    nsICanvasElement* mCanvasElement;

    glitz_surface_t* mGlitzSurface;
    glitz_buffer_t* mGlitzImageBuffer;

    cairo_surface_t *mCairoImageSurface;

#ifdef XP_WIN
    HBITMAP mWinBitmap;
    HDC mWinBitmapDC;
#endif

    static inline PRBool JSValToFloatArray (JSContext *ctx, jsval val,
                                            jsuint cnt, float *array)
    {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsval jv;
        jsdouble dv;

        if (!::JS_ValueToObject(ctx, val, &arrayObj) ||
            !::JS_IsArrayObject(ctx, arrayObj) ||
            !::JS_GetArrayLength(ctx, arrayObj, &arrayLen) ||
            (arrayLen < cnt))
            return PR_FALSE;

        for (jsuint i = 0; i < cnt; i++) {
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            if (!::JS_ValueToNumber(ctx, jv, &dv))
                return PR_FALSE;
            array[i] = (float) dv;
        }

        return PR_TRUE;
    }

    static inline PRBool JSValToDoubleArray (JSContext *ctx, jsval val,
                                             jsuint cnt, double *array)
    {
        JSObject *arrayObj;
        jsuint arrayLen;
        jsval jv;
        jsdouble dv;

        if (!::JS_ValueToObject(ctx, val, &arrayObj) ||
            !::JS_IsArrayObject(ctx, arrayObj) ||
            !::JS_GetArrayLength(ctx, arrayObj, &arrayLen) ||
            (arrayLen < cnt))
            return PR_FALSE;

        for (jsuint i = 0; i < cnt; i++) {
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            if (!::JS_ValueToNumber(ctx, jv, &dv))
                return PR_FALSE;
            array[i] = dv;
        }

        return PR_TRUE;
    }

    static inline PRBool JSValToJSArrayAndLength (JSContext *ctx, jsval val,
                                                  JSObject **outObj, jsuint *outLen)
    {
        JSObject *obj = nsnull;
        jsuint len;
        if (!::JS_ValueToObject(ctx, val, &obj) ||
            !::JS_IsArrayObject(ctx, obj) ||
            !::JS_GetArrayLength(ctx, obj, &len))
        {
            return PR_FALSE;
        }

        *outObj = obj;
        *outLen = len;

        return PR_TRUE;
    }

    template<class T>
    static nsresult JSValToSpecificInterface(JSContext *ctx, jsval val, T **out)
    {
        if (JSVAL_IS_NULL(val)) {
            *out = nsnull;
            return NS_OK;
        }

        if (!JSVAL_IS_OBJECT(val))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<nsISupports> isup;
        nsresult rv = gXPConnect->WrapJS(ctx, JSVAL_TO_OBJECT(val),
                                         NS_GET_IID(nsISupports),
                                         getter_AddRefs(isup));
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<T> obj = do_QueryInterface(isup);
        if (!obj)
            return NS_ERROR_DOM_SYNTAX_ERR;

        NS_ADDREF(*out = obj.get());
        return NS_OK;
    }

    static inline JSObject *ArrayToJSArray (JSContext *ctx,
                                            const PRInt32 *vals,
                                            const PRUint32 len)
    {
        // XXX handle ints that are too big to fit
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);
        for (PRUint32 i = 0; i < len; i++)
            jsvector[i] = INT_TO_JSVAL(vals[i]);
        return JS_NewArrayObject(ctx, len, jsvector);
    }

    static inline JSObject *ArrayToJSArray (JSContext *ctx,
                                            const PRUint32 *vals,
                                            const PRUint32 len)
    {
        // XXX handle ints that are too big to fit
        nsAutoArrayPtr<jsval> jsvector(new jsval[len]);
        for (PRUint32 i = 0; i < len; i++)
            jsvector[i] = INT_TO_JSVAL(vals[i]);
        return JS_NewArrayObject(ctx, len, jsvector);
    }

    void LogMessage (const nsCString& errorString) {
        nsresult rv;
        nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
        console->LogStringMessage(NS_ConvertUTF8toUTF16(errorString).get());
    }
};

class NativeJSContext {
public:
    NativeJSContext() {
        error = gXPConnect->GetCurrentNativeCallContext(getter_AddRefs(ncc));
        if (NS_FAILED(error))
            return;

        if (!ncc) {
            error = NS_ERROR_FAILURE;
            return;
        }

        ctx = nsnull;

        error = ncc->GetJSContext(&ctx);
        if (NS_FAILED(error))
            return;

        ncc->GetArgc(&argc);
        ncc->GetArgvPtr(&argv);
    }

    PRBool AddGCRoot (void *aPtr, const char *aName) {
        return JS_AddNamedRootRT(gScriptRuntime, aPtr, aName);
    }

    void ReleaseGCRoot (void *aPtr) {
        JS_RemoveRootRT(gScriptRuntime, aPtr);
    }

    void SetRetVal (PRInt32 val) {
        if (INT_FITS_IN_JSVAL(val))
            SetRetVal(INT_TO_JSVAL(val));
        else
            SetRetVal((double) val);
    }

    void SetRetVal (PRUint32 val) {
        if (INT_FITS_IN_JSVAL(val))
            SetRetVal(INT_TO_JSVAL((int) val));
        else
            SetRetVal((double) val);
    }

    void SetRetVal (double val) {
        jsval *vp;
        ncc->GetRetValPtr(&vp);
        JS_NewDoubleValue(ctx, val, vp);
    }

    void SetBoolRetVal (PRBool val) {
        if (val)
            SetRetVal(JSVAL_TRUE);
        else
            SetRetVal(JSVAL_FALSE);
    }

    void SetRetVal (PRInt32 *vp, PRUint32 len) {
        nsAutoArrayPtr<jsval> jsvector = new jsval[len];
        for (PRUint32 i = 0; i < len; i++)
            jsvector[i] = INT_TO_JSVAL(vp[i]);
        JSObject *jsarr = JS_NewArrayObject(ctx, len, jsvector.get());
        SetRetVal(OBJECT_TO_JSVAL(jsarr));
    }

    void SetRetVal (float *fp, PRUint32 len) {
        nsAutoArrayPtr<jsval> jsvector = new jsval[len];

        if (!JS_EnterLocalRootScope(ctx))
            return; // XXX ???

        for (PRUint32 i = 0; i < len; i++)
            JS_NewDoubleValue(ctx, (jsdouble) fp[i], &jsvector[i]);
        JSObject *jsarr = JS_NewArrayObject(ctx, len, jsvector.get());
        SetRetVal(OBJECT_TO_JSVAL(jsarr));

        JS_LeaveLocalRootScope(ctx);
    }

    void SetRetVal (jsval val) {
        jsval *vp;
        ncc->GetRetValPtr(&vp);
        *vp = val;
        ncc->SetReturnValueWasSet(PR_TRUE);
    }

    nsCOMPtr<nsIXPCNativeCallContext> ncc;
    nsresult error;
    JSContext *ctx;
    PRUint32 argc;
    jsval *argv;
};

class JSObjectHelper {
public:
    JSObjectHelper(NativeJSContext *jsctx)
        : mCtx (jsctx)
    {
        mObject = JS_NewObject(mCtx->ctx, NULL, NULL, NULL);
        if (!mObject)
            return;

        if (!mCtx->AddGCRoot(&mObject, "JSObjectHelperCanvas3D"))
            mObject = nsnull;
    }

    ~JSObjectHelper() {
        if (mObject && mCtx)
            mCtx->ReleaseGCRoot(&mObject);
    }

    PRBool DefineProperty(const char *name, PRInt32 val) {
        // XXX handle too big ints
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, INT_TO_JSVAL(val), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, PRUint32 val) {
        // XXX handle too big ints
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, INT_TO_JSVAL((int)val), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, double val) {
        jsval dv;

        if (!JS_NewDoubleValue(mCtx->ctx, val, &dv))
            return PR_FALSE;

        if (!JS_DefineProperty(mCtx->ctx, mObject, name, dv, NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, JSObject *val) {
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, OBJECT_TO_JSVAL(val), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    // Blah.  We can't name this DefineProperty also because PRBool is the same as PRInt32
    PRBool DefineBoolProperty(const char *name, PRBool val) {
        if (!JS_DefineProperty(mCtx->ctx, mObject, name, val ? JS_TRUE : JS_FALSE, NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, const nsCSubstring& val) {
        JSString *jsstr = JS_NewStringCopyN(mCtx->ctx, val.BeginReading(), val.Length());
        if (!jsstr ||
            !JS_DefineProperty(mCtx->ctx, mObject, name, STRING_TO_JSVAL(jsstr), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, const nsSubstring& val) {
        JSString *jsstr = JS_NewUCStringCopyN(mCtx->ctx, val.BeginReading(), val.Length());
        if (!jsstr ||
            !JS_DefineProperty(mCtx->ctx, mObject, name, STRING_TO_JSVAL(jsstr), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    PRBool DefineProperty(const char *name, const char *val, PRUint32 len) {
        JSString *jsstr = JS_NewStringCopyN(mCtx->ctx, val, len);
        if (!jsstr ||
            !JS_DefineProperty(mCtx->ctx, mObject, name, STRING_TO_JSVAL(jsstr), NULL, NULL, JSPROP_ENUMERATE))
            return PR_FALSE;
        return PR_TRUE;
    }

    JSObject *Object() {
        return mObject;
    }

protected:
    NativeJSContext *mCtx;
    JSObject *mObject;
};

class SimpleBuffer {
public:
    SimpleBuffer() : type(GL_FLOAT), data(nsnull), length(0), capacity(0), sizePerVertex(0) {
    }

    ~SimpleBuffer() {
        Release();
    }

    inline PRUint32 ElementSize() {
        if (type == GL_FLOAT) return sizeof(float);
        if (type == GL_SHORT) return sizeof(short);
        if (type == GL_UNSIGNED_SHORT) return sizeof(unsigned short);
        if (type == GL_BYTE) return 1;
        if (type == GL_UNSIGNED_BYTE) return 1;
        if (type == GL_INT) return sizeof(int);
        if (type == GL_UNSIGNED_INT) return sizeof(unsigned int);
        if (type == GL_DOUBLE) return sizeof(double);
        return 0;
    }

    void Clear() {
        Release();
    }

    void Set(PRUint32 t, PRUint32 spv, PRUint32 count, void* vals) {
        Prepare(t, spv, count);

        if (count)
            memcpy(data, vals, count*ElementSize());
    }

    void Prepare(PRUint32 t, PRUint32 spv, PRUint32 count) {
        if (count == 0) {
            Release();
        } else {
            EnsureCapacity(PR_FALSE, count*ElementSize());
            type = t;
            length = count;
            sizePerVertex = spv;
        }
    }

    void Release() {
        if (data)
            PR_Free(data);
        length = 0;
        capacity = 0;
        data = nsnull;
    }

    void EnsureCapacity(PRBool preserve, PRUint32 cap) {
        if (capacity >= cap)
            return;

        void* newdata = PR_Malloc(cap);
        if (preserve && length)
            memcpy(newdata, data, length*ElementSize());
        PR_Free(data);
        data = newdata;
        capacity = cap;
    }

    PRUint32 type;
    void* data;
    PRUint32 length;        // # of elements
    PRUint32 capacity;      // bytes!
    PRUint32 sizePerVertex; // OpenGL "size" param; num coordinates per vertex
};


nsresult JSArrayToSimpleBuffer (SimpleBuffer& sbuffer,
                                PRUint32 typeParam,
                                PRUint32 sizeParam,
                                JSContext *ctx,
                                JSObject *arrayObj,
                                jsuint arrayLen);

class CanvasGLTexture :
    public nsICanvasRenderingContextGLTexture,
    public nsICanvasGLTexture
{
    friend class nsCanvasRenderingContextGLES11;
    friend class nsCanvasRenderingContextGLWeb20;
public:
    CanvasGLTexture(nsCanvasRenderingContextGLPrivate *owner);
    ~CanvasGLTexture();

    NS_DECL_ISUPPORTS

    NS_DECL_NSICANVASRENDERINGCONTEXTGLTEXTURE

    nsresult Init();
    nsresult Dispose();

protected:
    PRBool mDisposed;
    nsCOMPtr<nsIWeakReference> mOwnerContext;

    glitz_surface_t *mGlitzTextureSurface;
    glitz_texture_object_t *mGlitzTextureObject;
    PRUint32 mWidth;
    PRUint32 mHeight;
};

class CanvasGLBuffer :
    public nsICanvasRenderingContextGLBuffer,
    public nsISecurityCheckedComponent,
    public nsICanvasGLBuffer
{
    friend class nsCanvasRenderingContextGLES11;
    friend class nsCanvasRenderingContextGLWeb20;
public:

    CanvasGLBuffer(nsCanvasRenderingContextGLPrivate *owner);
    ~CanvasGLBuffer();

    // Init can be called multiple times to reinitialize this
    // buffer object
    nsresult Init (PRUint32 usage,
                   PRUint32 size,
                   PRUint32 type,
                   JSContext *ctx,
                   JSObject *arrayObj,
                   jsuint arrayLen);

    SimpleBuffer& GetSimpleBuffer() { return mSimpleBuffer; }

    NS_DECL_ISUPPORTS
    NS_DECL_NSICANVASRENDERINGCONTEXTGLBUFFER
    NS_DECL_NSISECURITYCHECKEDCOMPONENT

protected:
    CanvasGLBuffer() { }

    inline GLEWContext *glewGetContext() {
        return mGlewContextPtr;
    }

    nsCOMPtr<nsIWeakReference> mOwnerContext;
    GLEWContext *mGlewContextPtr;

    PRBool mDisposed;

    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mType;
    PRUint32 mUsage;

    SimpleBuffer mSimpleBuffer;
    GLuint mBufferID;
};

/* Helper macros for when we're just wrapping a gl method, so that
 * we can avoid having to type this 500 times.  Note that these MUST
 * NOT BE USED if we need to check any of the parameters.
 */

#define GL_SAME_METHOD_0(glname, name)                       \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name() {       \
    MakeContextCurrent(); gl##glname(); return NS_OK;        \
}

#define GL_SAME_METHOD_1(glname, name, t1)                            \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1) {           \
    MakeContextCurrent(); gl##glname(a1); return NS_OK;               \
}

#define GL_SAME_METHOD_2(glname, name, t1, t2)                          \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2) {      \
    MakeContextCurrent(); gl##glname(a1,a2); return NS_OK;              \
}

#define GL_SAME_METHOD_3(glname, name, t1, t2, t3)                        \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3); return NS_OK;             \
}

#define GL_SAME_METHOD_4(glname, name, t1, t2, t3, t4)                           \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3, t4 a4) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3,a4); return NS_OK;                 \
}

#define GL_SAME_METHOD_5(glname, name, t1, t2, t3, t4, t5)                              \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3,a4,a5); return NS_OK;                     \
}

#define GL_SAME_METHOD_6(glname, name, t1, t2, t3, t4, t5, t6)                                 \
NS_IMETHODIMP NSGL_CONTEXT_NAME::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3,a4,a5,a6); return NS_OK;                         \
}

#endif /* _NSCANVASRENDERINGCONTEXTGL_H_ */
