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

#include "nsICanvasRenderingContextGLES11.h"
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

#ifdef PR_LOGGING
PRLogModuleInfo* gGLES11Log = nsnull;
#endif

#ifdef MOZILLA_1_8_BRANCH
struct _cairo_surface_win32_hack {
    void *ptr;
    unsigned int refcnt;
    cairo_status_t st;
    cairo_bool_t finished;
    /* array_t */
    int sz;
    int num_el;
    int el_sz;
    void *elements;
    double dx, dy, dxs, dys;
    unsigned int a;
    unsigned int b;

    /* win32 */
    cairo_format_t format;
};
#endif
#endif

static nsIXPConnect *gXPConnect = nsnull;
static JSRuntime *gScriptRuntime = nsnull;
static nsIJSRuntimeService *gJSRuntimeService = nsnull;

#define MAX_TEXTURE_UNITS 16

class nsCanvasRenderingContextGLES11;

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

    void addGCRoot (void *aPtr, const char *aName) {
        PRBool ok = JS_AddNamedRootRT(gScriptRuntime, aPtr, aName);
        if (!ok) {
            NS_ERROR("JS_AddNamedRootRT failed!\n");
            return;
        }
    }

    void releaseGCRoot (void *aPtr) {
        JS_RemoveRootRT(gScriptRuntime, aPtr);
    }

    nsCOMPtr<nsIXPCNativeCallContext> ncc;
    nsresult error;
    JSContext *ctx;
    PRUint32 argc;
    jsval *argv;
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


class CanvasGLES11Texture :
    public nsICanvasRenderingContextGLES11Texture
{
    friend class nsCanvasRenderingContextGLES11;
public:
    CanvasGLES11Texture(nsCanvasRenderingContextGLES11 *owner);
    ~CanvasGLES11Texture();

    NS_DECL_ISUPPORTS

    NS_DECL_NSICANVASRENDERINGCONTEXTGLES11TEXTURE

    nsresult Init();
    nsresult Dispose();

protected:
    PRBool mDisposed;
    nsCanvasRenderingContextGLES11 *mOwnerContext;

    glitz_surface_t *mGlitzTextureSurface;
    glitz_texture_object_t *mGlitzTextureObject;
    PRUint32 mWidth;
    PRUint32 mHeight;
};

#define CANVAS_GLES11_BUFFER_IID \
    { 0x506a7b9b, 0x9159, 0x40ee, { 0x84, 0x63, 0xbe, 0x3f, 0x07, 0x3b, 0xc2, 0x32 } }

// Private IID used for identifying an instance of this particular class
const nsIID CanvasGLES11BufferIID = CANVAS_GLES11_BUFFER_IID;

class CanvasGLES11Buffer :
    public nsICanvasRenderingContextGLES11Buffer,
    public nsISecurityCheckedComponent
{
    friend class nsCanvasRenderingContextGLES11;
public:

    CanvasGLES11Buffer(nsCanvasRenderingContextGLES11 *owner);
    ~CanvasGLES11Buffer();

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
    NS_DECL_NSICANVASRENDERINGCONTEXTGLES11BUFFER
    NS_DECL_NSISECURITYCHECKEDCOMPONENT

protected:
    CanvasGLES11Buffer() { }

    inline GLEWContext *glewGetContext();

    nsCanvasRenderingContextGLES11 *mOwnerContext;
    PRBool mDisposed;

    PRUint32 mLength;
    PRUint32 mSize;
    PRUint32 mType;
    PRUint32 mUsage;

    SimpleBuffer mSimpleBuffer;
    GLuint mBufferID;
};

class nsCanvasRenderingContextGLES11 :
    public nsICanvasRenderingContextGLES11,
    public nsICanvasRenderingContextInternal
{
    friend class CanvasGLES11Texture;
    friend class CanvasGLES11Buffer;
public:
    nsCanvasRenderingContextGLES11();
    virtual ~nsCanvasRenderingContextGLES11();

    NS_DECL_ISUPPORTS

    NS_DECL_NSICANVASRENDERINGCONTEXTGLES11

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD Render(nsIRenderingContext *rc);
    NS_IMETHOD RenderToSurface(cairo_surface_t *surf);
    NS_IMETHOD GetInputStream(const nsACString& aMimeType,
                              const nsAString& aEncoderOptions,
                              nsIInputStream **aStream);

    GLEWContext *glewGetContext() { return &mGlewContext; }

protected:
    nsIFrame *GetCanvasLayoutFrame();
    inline void MakeContextCurrent();
    static void LostCurrentContext(void *closure);

    inline PRBool CheckEnableMode(PRUint32 mode);

    void Destroy();

    static PRBool mGlitzInitialized;

    PRPackedBool mCurrent;

    PRInt32 mWidth, mHeight, mStride;
    PRUint8 *mImageBuffer;

    nsICanvasElement* mCanvasElement;

    glitz_context_t* mGlitzContext;
    glitz_drawable_t* mGlitzDrawable;
    glitz_surface_t* mGlitzSurface;
    glitz_buffer_t* mGlitzImageBuffer;

    cairo_surface_t *mCairoImageSurface;

    GLEWContext mGlewContext;
    void *mContextToken;

    static nsresult JSArrayToSimpleBuffer (SimpleBuffer& sbuffer, PRUint32 typeParam, PRUint32 sizeParam, JSContext *ctx, JSObject *arrayObj, jsuint arrayLen);

    nsresult CairoSurfaceFromElement(nsIDOMElement *imgElt,
                                     cairo_surface_t **aCairoSurface,
                                     PRUint8 **imgData,
                                     PRInt32 *widthOut, PRInt32 *heightOut,
                                     nsIURI **uriOut, PRBool *forceWriteOnlyOut);
    void DoDrawImageSecurityCheck(nsIURI* aURI, PRBool forceWriteOnly);

    /* These are nsCOMPtrs because we'll be swapping them a lot, and
     * don't want to have to worry about the ADDREF/RELEASE.  But
     * we still want easy access to the CanvasGLES11Buffer* pointer,
     * so there are some helper functions.
     */
    nsCOMPtr<nsICanvasRenderingContextGLES11Buffer> mVertexBuffer;
    nsCOMPtr<nsICanvasRenderingContextGLES11Buffer> mNormalBuffer;
    nsCOMPtr<nsICanvasRenderingContextGLES11Buffer> mColorBuffer;
    nsCOMPtr<nsICanvasRenderingContextGLES11Buffer> mTexCoordBuffer[MAX_TEXTURE_UNITS];

    CanvasGLES11Buffer* RealVertexBuffer() {
        nsICanvasRenderingContextGLES11Buffer* ptr = mVertexBuffer.get();
        return NS_STATIC_CAST(CanvasGLES11Buffer*, ptr);
    }
    CanvasGLES11Buffer* RealNormalBuffer() {
        nsICanvasRenderingContextGLES11Buffer* ptr = mNormalBuffer.get();
        return NS_STATIC_CAST(CanvasGLES11Buffer*, ptr);
    }
    CanvasGLES11Buffer* RealColorBuffer() {
        nsICanvasRenderingContextGLES11Buffer* ptr = mColorBuffer.get();
        return NS_STATIC_CAST(CanvasGLES11Buffer*, ptr);
    }
    CanvasGLES11Buffer* RealTexCoordBuffer(PRUint32 i) {
        nsICanvasRenderingContextGLES11Buffer* ptr = mTexCoordBuffer[i].get();
        return NS_STATIC_CAST(CanvasGLES11Buffer*, ptr);
    }

    PRPackedBool mVertexArrayEnabled;
    PRPackedBool mNormalArrayEnabled;
    PRPackedBool mColorArrayEnabled;
    PRPackedBool mTexCoordArrayEnabled[MAX_TEXTURE_UNITS];

    PRUint16 mActiveTextureUnit;

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
};

PRBool nsCanvasRenderingContextGLES11::mGlitzInitialized = PR_FALSE;

// let's all do the xpcom!

// CanvasGLES11Buffer
NS_DECL_CLASSINFO(CanvasGLES11Buffer)
NS_IMPL_ADDREF(CanvasGLES11Buffer)
NS_IMPL_RELEASE(CanvasGLES11Buffer)

NS_IMPL_CI_INTERFACE_GETTER1(CanvasGLES11Buffer, nsICanvasRenderingContextGLES11Buffer)

NS_INTERFACE_MAP_BEGIN(CanvasGLES11Buffer)
  if (aIID.Equals(CanvasGLES11BufferIID))
    foundInterface = NS_STATIC_CAST(nsICanvasRenderingContextGLES11Buffer*, this);
  else
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGLES11Buffer)
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextGLES11Buffer)
  NS_IMPL_QUERY_CLASSINFO(CanvasGLES11Buffer)
NS_INTERFACE_MAP_END

// CanvasGLES11Texture
NS_DECL_CLASSINFO(CanvasGLES11Texture)
NS_IMPL_ADDREF(CanvasGLES11Texture)
NS_IMPL_RELEASE(CanvasGLES11Texture)

NS_IMPL_CI_INTERFACE_GETTER1(CanvasGLES11Texture, nsICanvasRenderingContextGLES11Texture)

NS_INTERFACE_MAP_BEGIN(CanvasGLES11Texture)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGLES11Texture)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextGLES11Texture)
  NS_IMPL_QUERY_CLASSINFO(CanvasGLES11Texture)
NS_INTERFACE_MAP_END

// nsCanvasRenderingContextGLES11
// NS_DECL_CLASSINFO lives in nsCanvas3DModule
NS_IMPL_ADDREF(nsCanvasRenderingContextGLES11)
NS_IMPL_RELEASE(nsCanvasRenderingContextGLES11)

NS_IMPL_CI_INTERFACE_GETTER2(nsCanvasRenderingContextGLES11,
                             nsICanvasRenderingContextGLES11,
                             nsICanvasRenderingContextInternal)

NS_INTERFACE_MAP_BEGIN(nsCanvasRenderingContextGLES11)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGLES11)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextGLES11)
  NS_IMPL_QUERY_CLASSINFO(nsCanvasRenderingContextGLES11)
NS_INTERFACE_MAP_END

nsresult
NS_NewCanvasRenderingContextGLES11(nsICanvasRenderingContextGLES11** aResult)
{
    nsICanvasRenderingContextGLES11* ctx = new nsCanvasRenderingContextGLES11();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

nsCanvasRenderingContextGLES11::nsCanvasRenderingContextGLES11()
    : mCurrent(PR_FALSE),
      mWidth(0), mHeight(0), mStride(0), mImageBuffer(nsnull),
      mCanvasElement(nsnull),
      mGlitzContext(nsnull), mGlitzDrawable(nsnull), mGlitzSurface(nsnull), mGlitzImageBuffer(nsnull),
      mCairoImageSurface(nsnull),
      mVertexArrayEnabled(PR_FALSE),
      mNormalArrayEnabled(PR_FALSE),
      mColorArrayEnabled(PR_FALSE),
      mActiveTextureUnit(0),
      mContextToken(nsnull)
{
#ifdef PR_LOGGING
    if (!gGLES11Log)
        gGLES11Log = PR_NewLogModule("canvasGLES11");
#endif

    PR_LOG(gGLES11Log, PR_LOG_DEBUG, ("##[%p] Creating GLES11 canvas context\n", this));

    if (!mGlitzInitialized) {
#ifdef MOZ_X11
        glitz_glx_init("libGL.so.1");
#endif
#ifdef XP_WIN
        glitz_wgl_init(NULL);
#endif
        mGlitzInitialized = PR_TRUE;
    }

    for (int i = 0; i < MAX_TEXTURE_UNITS; i++)
        mTexCoordArrayEnabled[i] = PR_FALSE;

    // grab the xpconnect service
    if (!gXPConnect) {
        nsresult rv = CallGetService(nsIXPConnect::GetCID(), &gXPConnect);
        if (NS_FAILED(rv)) {
            NS_ERROR("Failed to get XPConnect!");
            return;
        }
    } else {
        NS_ADDREF(gXPConnect);
    }

    if (!gJSRuntimeService) {
        nsresult rv = CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                                     &gJSRuntimeService);
        if (NS_FAILED(rv)) {
            // uh..
            NS_ERROR("Failed to get JS RuntimeService!");
            return;
        }

        gJSRuntimeService->GetRuntime(&gScriptRuntime);
        if (!gScriptRuntime) {
            NS_RELEASE(gJSRuntimeService);
            gJSRuntimeService = nsnull;
            NS_ERROR("Unable to get JS runtime from JS runtime service");
        }
    } else {
        NS_ADDREF(gJSRuntimeService);
    }

#ifdef XP_WIN
    mWinBitmap = nsnull;
    mWinBitmapDC = nsnull;
#endif
}

nsCanvasRenderingContextGLES11::~nsCanvasRenderingContextGLES11()
{
    Destroy();
    if (gXPConnect && gXPConnect->Release() == 0)
        gXPConnect = nsnull;
    if (gJSRuntimeService && gJSRuntimeService->Release() == 0) {
        gJSRuntimeService = nsnull;
        gScriptRuntime = nsnull;
    }
}

void
nsCanvasRenderingContextGLES11::Destroy()
{
    PR_LOG(gGLES11Log, PR_LOG_DEBUG, ("##[%p] Destroying GLES11 canvas context\n", this));

    if (mCairoImageSurface) {
        cairo_surface_destroy (mCairoImageSurface);
        mCairoImageSurface = nsnull;
    }

    if (mGlitzImageBuffer) {
        glitz_buffer_destroy(mGlitzImageBuffer);
        mGlitzImageBuffer = nsnull;
    }

    if (mGlitzContext) {
        glitz_context_destroy (mGlitzContext);
        mGlitzContext = nsnull;
    }
    if (mGlitzSurface) {
        glitz_surface_destroy (mGlitzSurface);
        mGlitzSurface = nsnull;
    }

    // XXX bad memlk; but we already destroyed the context,
    // so when this tries to free the fragment shaders,
    // it tries to activate the context, and that blows up. wtf?!
    if (mGlitzDrawable) {
        glitz_drawable_destroy (mGlitzDrawable);
        mGlitzDrawable = nsnull;
    }

#ifdef XP_WIN
    if (mWinBitmapDC) {
        DeleteDC(mWinBitmapDC);
        mWinBitmapDC = nsnull;
    }

    if (mWinBitmap) {
        DeleteObject(mWinBitmap);
        mWinBitmap = nsnull;
        mImageBuffer = nsnull;
    }
#endif

    if (mImageBuffer) {
        PR_Free(mImageBuffer);
        mImageBuffer = nsnull;
    }
}

void
nsCanvasRenderingContextGLES11::MakeContextCurrent()
{
#ifdef XP_WIN
    if (mContextToken == nsnull) {
        glitz_context_make_current (mGlitzContext, mGlitzDrawable);
        mContextToken = wglGetCurrentContext();
        mCurrent = PR_TRUE;
        return;
    }

    if (mContextToken != wglGetCurrentContext()) {
        glitz_context_make_current (mGlitzContext, mGlitzDrawable);
        mCurrent = PR_TRUE;
    }
#endif

#if 0
    if (!mCurrent) {
        fprintf (stderr, "[this:%p] Making context %p current (current: %d)\n", this, mGlitzContext, mCurrent);
        fflush (stderr);
        glitz_context_set_user_data (mGlitzContext, this, LostCurrentContext);
    }
#endif
}

void
nsCanvasRenderingContextGLES11::LostCurrentContext(void *closure)
{
    nsCanvasRenderingContextGLES11* self = (nsCanvasRenderingContextGLES11*) closure;
    fprintf (stderr, "[this:%p] Lost context\n", closure);
    fflush (stderr);

    self->mCurrent = PR_FALSE;
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::SetCanvasElement(nsICanvasElement* aParentCanvas)
{
    mCanvasElement = aParentCanvas;
    fprintf (stderr, "SetCanvasElement: %p\n", mCanvasElement);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::SetDimensions(PRInt32 width, PRInt32 height)
{
    fprintf (stderr, "SetDimensions\n");
    if (mWidth == width && mHeight == height)
        return NS_OK;

    Destroy();

    PR_LOG(gGLES11Log, PR_LOG_DEBUG, ("##[%p] SetDimensions: %d %d\n", this, width, height));

    glitz_drawable_format_t *gdformat = nsnull;

#ifdef MOZ_X11
    Display* display = GDK_DISPLAY();

    glitz_drawable_format_t templ;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.doublebuffer = 0;
    
    gdformat = glitz_glx_find_pbuffer_format
        (display,
         gdk_x11_get_default_screen(),
         GLITZ_FORMAT_FOURCC_MASK |
         GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
         GLITZ_FORMAT_BLUE_SIZE_MASK | GLITZ_FORMAT_ALPHA_SIZE_MASK |
         GLITZ_FORMAT_DOUBLEBUFFER_MASK, &templ, 0);

    if (!gdformat)
        return NS_ERROR_INVALID_ARG;

    mGlitzDrawable =
        glitz_glx_create_pbuffer_drawable(display,
                                          DefaultScreen(display),
                                          gdformat,
                                          width,
                                          height);
#endif

#ifdef XP_WIN
    glitz_drawable_format_t templ;
    templ.color.fourcc = GLITZ_FOURCC_RGB;
    templ.color.red_size = 8;
    templ.color.green_size = 8;
    templ.color.blue_size = 8;
    templ.color.alpha_size = 8;
    templ.doublebuffer = 0;
    templ.samples = 8;

    gdformat = nsnull;

    do {
        templ.samples = templ.samples >> 1;
        gdformat = glitz_wgl_find_pbuffer_format
            (GLITZ_FORMAT_FOURCC_MASK |
             GLITZ_FORMAT_RED_SIZE_MASK | GLITZ_FORMAT_GREEN_SIZE_MASK |
             GLITZ_FORMAT_BLUE_SIZE_MASK | GLITZ_FORMAT_ALPHA_SIZE_MASK |
             GLITZ_FORMAT_DOUBLEBUFFER_MASK | GLITZ_FORMAT_SAMPLES_MASK,
             &templ, 0);
    } while (gdformat == nsnull && templ.samples > 0);

    if (!gdformat)
        return NS_ERROR_INVALID_ARG;

    mGlitzDrawable =
        glitz_wgl_create_pbuffer_drawable(gdformat,
                                          width,
                                          height);
#endif

    if (!gdformat || !mGlitzDrawable)
        return NS_ERROR_FAILURE;

    glitz_format_t *gformat =
        glitz_find_standard_format(mGlitzDrawable, GLITZ_STANDARD_ARGB32);
    if (!gformat)
        return NS_ERROR_INVALID_ARG;

    mGlitzSurface =
        glitz_surface_create(mGlitzDrawable,
                             gformat,
                             width,
                             height,
                             0,
                             NULL);
    if (!mGlitzSurface)
        return NS_ERROR_INVALID_ARG;

    glitz_surface_attach(mGlitzSurface, mGlitzDrawable, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

    mGlitzContext = glitz_context_create (mGlitzDrawable, gdformat);

    MakeContextCurrent();

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        // er, something very bad happened
        NS_ERROR("glewInit failed!  Leaking lots of memory");
        return NS_ERROR_FAILURE;
    }

    //mSurface = new gfxGlitzSurface (mGlitzDrawable, gsurf, PR_TRUE);
    mWidth = width;
    mHeight = height;

    mStride = (((mWidth*4) + 3) / 4) * 4;

#ifdef XP_WIN
    BITMAPINFO *bitmap_info = (BITMAPINFO*) PR_Malloc(sizeof(BITMAPINFOHEADER));
    bitmap_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info->bmiHeader.biWidth = mWidth;
    bitmap_info->bmiHeader.biHeight = mHeight;
    bitmap_info->bmiHeader.biSizeImage = 0;
    bitmap_info->bmiHeader.biXPelsPerMeter = 2834; /* unused here */
    bitmap_info->bmiHeader.biYPelsPerMeter = 2834; /* unused here */
    bitmap_info->bmiHeader.biPlanes = 1;
    bitmap_info->bmiHeader.biBitCount = 32;
    bitmap_info->bmiHeader.biCompression = BI_RGB;
    bitmap_info->bmiHeader.biClrUsed = 0;	/* unused */
    bitmap_info->bmiHeader.biClrImportant = 0;

    mWinBitmapDC = CreateCompatibleDC(NULL);
    if (mWinBitmapDC) {
        void *bits = nsnull;
        mWinBitmap = CreateDIBSection (mWinBitmapDC,
                                       bitmap_info,
                                       DIB_RGB_COLORS,
                                       &bits,
                                       NULL, 0);
        if (mWinBitmap) {
            SelectObject (mWinBitmapDC, mWinBitmap);
            GdiFlush();

            mImageBuffer = (PRUint8*) bits;
            mCairoImageSurface = cairo_win32_surface_create (mWinBitmapDC);
#ifdef MOZILLA_1_8_BRANCH
            ((struct _cairo_surface_win32_hack*)mCairoImageSurface)->format = CAIRO_FORMAT_ARGB32;
#endif
        } else {
            DeleteDC(mWinBitmapDC);
        }
    }

    PR_Free(bitmap_info);
#endif

    if (!mImageBuffer) {
        mImageBuffer = (PRUint8*) PR_Malloc(mStride * mHeight);
        if (mImageBuffer) {
            mCairoImageSurface = cairo_image_surface_create_for_data (mImageBuffer,
                                                                      CAIRO_FORMAT_ARGB32,
                                                                      mWidth,
                                                                      mHeight,
                                                                      mStride);
        }
    }

    if (!mImageBuffer)
        return NS_ERROR_FAILURE;

    mGlitzImageBuffer = glitz_buffer_create_for_data (mImageBuffer);

    return NS_OK;
}

/*
 * This is identical to nsCanvasRenderingContext2D::Render, we just don't
 * have a good place to put it; though maybe I want a CanvasContextImpl that
 * all this stuff can derive from?
 */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::Render(nsIRenderingContext *rc)
{
    nsresult rv = NS_OK;

    if (!mImageBuffer)
        return NS_OK;

#ifdef MOZ_CAIRO_GFX

    gfxContext* ctx = (gfxContext*) rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);
    nsRefPtr<gfxASurface> surf = gfxASurface::Wrap(mCairoImageSurface);
    nsRefPtr<gfxPattern> pat = new gfxPattern(surf);

    // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
    // pixel alignment for this stuff!
    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->Fill();

#else

    // non-Thebes; this becomes exciting
    cairo_surface_t *dest = nsnull;
    cairo_t *dest_cr = nsnull;

#ifdef XP_WIN
    void *ptr = nsnull;
#ifdef MOZILLA_1_8_BRANCH
    rv = rc->RetrieveCurrentNativeGraphicData(&ptr);
    if (NS_FAILED(rv) || !ptr)
        return NS_ERROR_FAILURE;
#else
    ptr = rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);
#endif
    HDC dc = (HDC) ptr;

    dest = cairo_win32_surface_create (dc);
    dest_cr = cairo_create (dest);
#endif

#ifdef MOZ_WIDGET_GTK2
    GdkDrawable *gdkdraw = nsnull;
#ifdef MOZILLA_1_8_BRANCH
    rv = rc->RetrieveCurrentNativeGraphicData((void**) &gdkdraw);
    if (NS_FAILED(rv) || !gdkdraw)
        return NS_ERROR_FAILURE;
#else
    gdkdraw = (GdkDrawable*) rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_GDK_DRAWABLE);
    if (!gdkdraw)
        return NS_ERROR_FAILURE;
#endif

    gint w, h;
    gdk_drawable_get_size (gdkdraw, &w, &h);
    dest = cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY(gdkdraw),
                                      GDK_DRAWABLE_XID(gdkdraw),
                                      GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(gdkdraw)),
                                      w, h);
    dest_cr = cairo_create (dest);
#endif

    nsTransform2D *tx = nsnull;
    rc->GetCurrentTransform(tx);

    nsCOMPtr<nsIDeviceContext> dctx;
    rc->GetDeviceContext(*getter_AddRefs(dctx));

    // Until we can use the quartz2 surface, mac will be different,
    // since we'll use CG to render.
#ifndef XP_MACOSX

    float x0 = 0.0, y0 = 0.0;
    float sx = 1.0, sy = 1.0;
    if (tx->GetType() & MG_2DTRANSLATION) {
        tx->Transform(&x0, &y0);
    }

    if (tx->GetType() & MG_2DSCALE) {
        sx = sy = dctx->DevUnitsToTwips();
        tx->TransformNoXLate(&sx, &sy);
    }

    cairo_translate (dest_cr, NSToIntRound(x0), NSToIntRound(y0));
    if (sx != 1.0 || sy != 1.0)
        cairo_scale (dest_cr, sx, sy);

    cairo_rectangle (dest_cr, 0, 0, mWidth, mHeight);
    cairo_clip (dest_cr);

    cairo_set_source_surface (dest_cr, mCairoImageSurface, 0, 0);
    cairo_paint (dest_cr);

    if (dest_cr)
        cairo_destroy (dest_cr);
    if (dest)
        cairo_surface_destroy (dest);

#else

    // OSX path

    CGrafPtr port = nsnull;
#ifdef MOZILLA_1_8_BRANCH
    rv = rc->RetrieveCurrentNativeGraphicData((void**) &port);
    if (NS_FAILED(rv) || !port)
        return NS_ERROR_FAILURE;
#else
    port = (CGrafPtr) rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_MAC_THING);
    if (!port)
        return NS_ERROR_FAILURE;
#endif

    struct Rect portRect;
    GetPortBounds(port, &portRect);

    CGContextRef cgc;
    OSStatus status;
    status = QDBeginCGContext (port, &cgc);
    if (status != noErr)
        return NS_ERROR_FAILURE;

    CGDataProviderRef dataProvider;
    CGImageRef img;

    dataProvider = CGDataProviderCreateWithData (NULL, mImageBuffer,
                                                 mWidth * mHeight * 4,
                                                 NULL);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    img = CGImageCreate (mWidth, mHeight, 8, 32, mWidth * 4, rgb,
                         kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                         dataProvider, NULL, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease (rgb);
    CGDataProviderRelease (dataProvider);

    float x0 = 0.0, y0 = 0.0;
    float sx = 1.0, sy = 1.0;
    if (tx->GetType() & MG_2DTRANSLATION) {
        tx->Transform(&x0, &y0);
    }

    if (tx->GetType() & MG_2DSCALE) {
        float p2t = dctx->DevUnitsToTwips();
        sx = p2t, sy = p2t;
        tx->TransformNoXLate(&sx, &sy);
    }

    /* Compensate for the bottom-left Y origin */
    CGContextTranslateCTM (cgc, NSToIntRound(x0),
                           portRect.bottom - portRect.top - NSToIntRound(y0) - NSToIntRound(mHeight * sy));
    if (sx != 1.0 || sy != 1.0)
        CGContextScaleCTM (cgc, sx, sy);

    CGContextDrawImage (cgc, CGRectMake(0, 0, mWidth, mHeight), img);

    CGImageRelease (img);

    status = QDEndCGContext (port, &cgc);
    /* if EndCGContext fails, what can we do? */
#endif
#endif

    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::RenderToSurface(cairo_surface_t *surf)
{
    return NS_OK;
}

nsIFrame*
nsCanvasRenderingContextGLES11::GetCanvasLayoutFrame()
{
    if (!mCanvasElement)
        return nsnull;

    nsIFrame *fr = nsnull;
    mCanvasElement->GetPrimaryCanvasFrame(&fr);
    return fr;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GetInputStream(const nsACString& aMimeType,
                                               const nsAString& aEncoderOptions,
                                               nsIInputStream **aStream)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
// Windowing system lookalike functions
//

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::SwapBuffers()
{
    if (!mImageBuffer)
        return NS_OK;

    MakeContextCurrent();

    // call glitz_get_pixels to pull out the pixels
    glitz_pixel_format_t argb_pf =
        { GLITZ_FOURCC_RGB,
          { 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff },
          0,
          0,
          mStride,
          GLITZ_PIXEL_SCANLINE_ORDER_TOP_DOWN
        };

    glitz_pixel_format_t *pf = &argb_pf;

    glitz_get_pixels (mGlitzSurface,
                      0, 0, mWidth, mHeight, pf, mGlitzImageBuffer);

    // then invalidate the region and do a sync redraw
    // (uh, why sync?)
    nsIFrame *frame = GetCanvasLayoutFrame();
    if (frame) {
        nsRect r = frame->GetRect();
        r.x = r.y = 0;

        // sync redraw
        //frame->Invalidate(r, PR_TRUE);

        // nsIFrame::Invalidate is an internal non-virtual method,
        // so we basically recreate it here.  I would suggest
        // an InvalidateExternal for the trunk.
        nsIPresShell *shell = frame->GetPresContext()->GetPresShell();
        if (shell) {
            PRBool suppressed = PR_FALSE;
            shell->IsPaintingSuppressed(&suppressed);
            if (suppressed)
                return NS_OK;
        }

        // maybe VMREFRESH_IMMEDIATE in some cases,
        // need to think
        PRUint32 flags = NS_VMREFRESH_NO_SYNC;
        if (frame->HasView()) {
            nsIView* view = frame->GetViewExternal();
            view->GetViewManager()->UpdateView(view, r, flags);
        } else {
            nsPoint offset;
            nsIView *view;
            frame->GetOffsetFromView(offset, &view);
            NS_ASSERTION(view, "no view");
            r += offset;
            view->GetViewManager()->UpdateView(view, r, flags);
        }
    }

    return NS_OK;
}

/* Helper macros for when we're just wrapping a gl method, so that
 * we can avoid having to type this 500 times.  Note that these MUST
 * NOT BE USED if we need to check any of the parameters.
 */

#define GL_SAME_METHOD_0(glname, name)                       \
NS_IMETHODIMP nsCanvasRenderingContextGLES11::name() {       \
    MakeContextCurrent(); gl##glname(); return NS_OK;        \
}

#define GL_SAME_METHOD_1(glname, name, t1)                            \
NS_IMETHODIMP nsCanvasRenderingContextGLES11::name(t1 a1) {           \
    MakeContextCurrent(); gl##glname(a1); return NS_OK;               \
}

#define GL_SAME_METHOD_2(glname, name, t1, t2)                          \
NS_IMETHODIMP nsCanvasRenderingContextGLES11::name(t1 a1, t2 a2) {      \
    MakeContextCurrent(); gl##glname(a1,a2); return NS_OK;              \
}

#define GL_SAME_METHOD_3(glname, name, t1, t2, t3)                        \
NS_IMETHODIMP nsCanvasRenderingContextGLES11::name(t1 a1, t2 a2, t3 a3) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3); return NS_OK;             \
}

#define GL_SAME_METHOD_4(glname, name, t1, t2, t3, t4)                           \
NS_IMETHODIMP nsCanvasRenderingContextGLES11::name(t1 a1, t2 a2, t3 a3, t4 a4) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3,a4); return NS_OK;                 \
}

#define GL_SAME_METHOD_5(glname, name, t1, t2, t3, t4, t5)                              \
NS_IMETHODIMP nsCanvasRenderingContextGLES11::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3,a4,a5); return NS_OK;                     \
}

#define GL_SAME_METHOD_6(glname, name, t1, t2, t3, t4, t5, t6)                                 \
NS_IMETHODIMP nsCanvasRenderingContextGLES11::name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) { \
    MakeContextCurrent(); gl##glname(a1,a2,a3,a4,a5,a6); return NS_OK;                         \
}


nsresult
nsCanvasRenderingContextGLES11::JSArrayToSimpleBuffer (SimpleBuffer& sbuffer, PRUint32 typeParam, PRUint32 sizeParam, JSContext *ctx, JSObject *arrayObj, jsuint arrayLen)
{
    sbuffer.Prepare(typeParam, sizeParam, arrayLen);

    if (typeParam == GL_SHORT) {
        short *ptr = (short*) sbuffer.data;
        for (PRUint32 i = 0; i < arrayLen; i++) {
            jsval jv;
            int32 iv;
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            ::JS_ValueToECMAInt32(ctx, jv, &iv);
            *ptr++ = (short) iv;
        }
    } else if (typeParam == GL_FLOAT) {
        float *ptr = (float*) sbuffer.data;
        for (PRUint32 i = 0; i < arrayLen; i++) {
            jsval jv;
            jsdouble dv;
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            ::JS_ValueToNumber(ctx, jv, &dv);
            *ptr++ = (float) dv;
        }
    } else if (typeParam == GL_UNSIGNED_BYTE) {
        unsigned char *ptr = (unsigned char*) sbuffer.data;
        for (PRUint32 i = 0; i < arrayLen; i++) {
            jsval jv;
            uint32 iv;
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            ::JS_ValueToECMAUint32(ctx, jv, &iv);
            *ptr++ = (unsigned char) iv;
        }
    } else if (typeParam == GL_UNSIGNED_INT) {
        PRUint32 *ptr = (PRUint32*) sbuffer.data;
        for (PRUint32 i = 0; i < arrayLen; i++) {
            jsval jv;
            uint32 iv;
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            ::JS_ValueToECMAUint32(ctx, jv, &iv);
            *ptr++ = iv;
        }
    } else {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    return NS_OK;
}

//
// nsICanvasRenderingContextGLES11
//

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GetCanvas(nsIDOMHTMLCanvasElement **_retval)
{
    *_retval = nsnull;
    return NS_OK;
}

#define BETWEEN(_x,_a,_b) ((_x) >= (_a) && (_x) < (_b))

PRBool
nsCanvasRenderingContextGLES11::CheckEnableMode(PRUint32 mode) {
    if (BETWEEN(mode, CLIP_PLANE0, CLIP_PLANE0+5) ||
        BETWEEN(mode, LIGHT0, LIGHT0+32))
    {
        return PR_TRUE;
    } else {
        switch (mode) {
            case NORMALIZE:
            case RESCALE_NORMAL:
            case CLIP_PLANE0:
            case CLIP_PLANE1:
            case CLIP_PLANE2:
            case CLIP_PLANE3:
            case CLIP_PLANE4:
            case CLIP_PLANE5:
            case FOG:
            case LIGHTING:
            case COLOR_MATERIAL:
            case POINT_SMOOTH:
            case LINE_SMOOTH:
            case CULL_FACE:
            case POLYGON_OFFSET_FILL:
            case MULTISAMPLE:
            case SAMPLE_ALPHA_TO_COVERAGE:
            case SAMPLE_ALPHA_TO_ONE:
            case SAMPLE_COVERAGE:
            case TEXTURE_2D:
            case TEXTURE_RECTANGLE:
            case SCISSOR_TEST:
            case ALPHA_TEST:
            case STENCIL_TEST:
            case DEPTH_TEST:
            case BLEND:
            case DITHER:
            case COLOR_LOGIC_OP:
                return PR_TRUE;
                break;
            default:
                return PR_FALSE;
        }
    }

    return PR_FALSE;
}

/* void enable (in PRUint32 mode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::Enable(PRUint32 mode)
{
    if (!CheckEnableMode(mode))
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    MakeContextCurrent();
    glEnable(mode);
    return NS_OK;
}

/* void disable (in PRUint32 mode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::Disable(PRUint32 mode)
{
    if (!CheckEnableMode(mode))
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    MakeContextCurrent();
    glDisable(mode);
    return NS_OK;
}

/* void clientActiveTexture (in PRUint32 texture); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::ClientActiveTexture(PRUint32 texture)
{
    if (!GLEW_ARB_multitexture) {
        if (texture == GL_TEXTURE0)
            return NS_OK;
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    if (texture < GL_TEXTURE0 || texture > (GL_TEXTURE0 + MAX_TEXTURE_UNITS))
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    mActiveTextureUnit = texture - GL_TEXTURE0;
    glClientActiveTexture(texture);
    return NS_OK;
}

/* void enableClientState (in PRUint32 mode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::EnableClientState(PRUint32 mode)
{
    switch(mode) {
        case VERTEX_ARRAY:
            mVertexArrayEnabled = PR_TRUE;
            break;
        case COLOR_ARRAY:
            mColorArrayEnabled = PR_TRUE;
            break;
        case NORMAL_ARRAY:
            mNormalArrayEnabled = PR_TRUE;
            break;
        case TEXTURE_COORD_ARRAY:
            mTexCoordArrayEnabled[mActiveTextureUnit] = PR_TRUE;
            break;
        default:
            return NS_ERROR_INVALID_ARG;
            break;
    }

    MakeContextCurrent();
    glEnableClientState(mode);
    return NS_OK;
}

/* void disableClientState (in PRUint32 mode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::DisableClientState(PRUint32 mode)
{
    switch(mode) {
        case VERTEX_ARRAY:
            mVertexArrayEnabled = PR_FALSE;
            break;
        case COLOR_ARRAY:
            mColorArrayEnabled = PR_FALSE;
            break;
        case NORMAL_ARRAY:
            mNormalArrayEnabled = PR_FALSE;
            break;
        case TEXTURE_COORD_ARRAY:
            mTexCoordArrayEnabled[mActiveTextureUnit] = PR_FALSE;
            break;
        default:
            return NS_ERROR_INVALID_ARG;
            break;
    }

    MakeContextCurrent();
    glDisableClientState(mode);
    return NS_OK;
}

/* PRUint32 getError (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GetError(PRUint32 *_retval)
{
    MakeContextCurrent();
    *_retval = glGetError();
    return NS_OK;
}

/* void normal (in float nx, in float ny, in float nz); */
GL_SAME_METHOD_3(Normal3f, Normal, float, float, float)

/* void multiTexCoord (in PRUint32 target, in float s, in float t, in float r, in float q); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::MultiTexCoord(PRUint32 target, float s, float t, float r, float q)
{
    if (!GLEW_ARB_multitexture) {
        if (target == GL_TEXTURE0) {
            glTexCoord4f(s,t,r,q);
            return NS_OK;
        }
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    if (target < GL_TEXTURE0 || target > (GL_TEXTURE0+MAX_TEXTURE_UNITS))
        return NS_ERROR_DOM_SYNTAX_ERR;

    glMultiTexCoord4f(target, s, t, r, q);

    return NS_OK;
}

/* void color (in float r, in float g, in float b, in float a); */
GL_SAME_METHOD_4(Color4f, Color, float, float, float, float)

/* void vertexPointer (in PRUint8 size, in PRUint32 type, in object [] vertexArray); */
/* void vertexPointer (in CanvasGLES11Buffer buffer); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::VertexPointer()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    CanvasGLES11Buffer *newBuffer = nsnull;
    nsresult rv;

    if (js.argc == 1) {
        if (!JSVAL_IS_OBJECT(js.argv[0]) ||
            JSVAL_IS_NULL(js.argv[0]))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<nsISupports> iface;
        rv = gXPConnect->WrapJS(js.ctx, JSVAL_TO_OBJECT(js.argv[0]),
                                NS_GET_IID(nsICanvasRenderingContextGLES11Buffer),
                                getter_AddRefs(iface));
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        // what we want is a CanvasGLES11Buffer; QI to our magic IID
        // will return this interface
        nsICanvasRenderingContextGLES11Buffer *bufferBase = nsnull;
        rv = iface->QueryInterface(CanvasGLES11BufferIID, (void **) &bufferBase);
        if (NS_FAILED(rv) || !bufferBase)
            return NS_ERROR_FAILURE;

        newBuffer = NS_STATIC_CAST(CanvasGLES11Buffer*, bufferBase);

        if (newBuffer->mType != GL_SHORT &&
            newBuffer->mType != GL_FLOAT)
            return NS_ERROR_DOM_SYNTAX_ERR;

        if (newBuffer->mSize < 2 || newBuffer->mSize > 4)
            return NS_ERROR_DOM_SYNTAX_ERR;

    } else if (js.argc == 3) {
        jsuint sizeParam;
        jsuint typeParam;

        JSObject *arrayObj;
        jsuint arrayLen;

        if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &sizeParam) ||
            !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &typeParam) ||
            !JSValToJSArrayAndLength(js.ctx, js.argv[2], &arrayObj, &arrayLen))
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (typeParam != GL_SHORT &&
            typeParam != GL_FLOAT &&
            (sizeParam < 2 || sizeParam > 4))
            return NS_ERROR_DOM_SYNTAX_ERR;

        newBuffer = new CanvasGLES11Buffer(this);
        if (!newBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = newBuffer->Init (GL_STATIC_DRAW, sizeParam, typeParam, js.ctx, arrayObj, arrayLen);
        if (NS_FAILED(rv)) {
            delete newBuffer;
            return rv;
        }
    } else {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    mVertexBuffer = newBuffer;

    MakeContextCurrent();
    glVertexPointer(newBuffer->GetSimpleBuffer().sizePerVertex,
                    newBuffer->GetSimpleBuffer().type,
                    0,
                    newBuffer->GetSimpleBuffer().data);
    return NS_OK;
}

/* void normalPointer (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::NormalPointer()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    CanvasGLES11Buffer *newBuffer = nsnull;
    nsresult rv;

    if (js.argc == 1) {
        if (!JSVAL_IS_OBJECT(js.argv[0]) ||
            JSVAL_IS_NULL(js.argv[0]))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<nsISupports> iface;
        rv = gXPConnect->WrapJS(js.ctx, JSVAL_TO_OBJECT(js.argv[0]),
                                NS_GET_IID(nsICanvasRenderingContextGLES11Buffer),
                                getter_AddRefs(iface));
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        // what we want is a CanvasGLES11Buffer; QI to our magic IID
        // will return this interface
        nsICanvasRenderingContextGLES11Buffer *bufferBase = nsnull;
        rv = iface->QueryInterface(CanvasGLES11BufferIID, (void **) &bufferBase);
        if (NS_FAILED(rv) || !bufferBase)
            return NS_ERROR_FAILURE;

        newBuffer = NS_STATIC_CAST(CanvasGLES11Buffer*, bufferBase);

        if (newBuffer->mType != GL_SHORT &&
            newBuffer->mType != GL_FLOAT)
            return NS_ERROR_DOM_SYNTAX_ERR;

        if (newBuffer->mSize != 3)
            return NS_ERROR_DOM_SYNTAX_ERR;
    } else if (js.argc == 2) {
        jsuint typeParam;

        JSObject *arrayObj;
        jsuint arrayLen;

        if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &typeParam) ||
            !JSValToJSArrayAndLength(js.ctx, js.argv[1], &arrayObj, &arrayLen))
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (typeParam != GL_SHORT && typeParam != GL_FLOAT)
            return NS_ERROR_DOM_SYNTAX_ERR;

        newBuffer = new CanvasGLES11Buffer(this);
        if (!newBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = newBuffer->Init (GL_STATIC_DRAW, 3, typeParam, js.ctx, arrayObj, arrayLen);
        if (NS_FAILED(rv)) {
            delete newBuffer;
            return NS_ERROR_DOM_SYNTAX_ERR;
        }
    } else {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    mNormalBuffer = newBuffer;

    MakeContextCurrent();
    glNormalPointer(newBuffer->GetSimpleBuffer().type,
                    0,
                    newBuffer->GetSimpleBuffer().data);
    return NS_OK;
}

/* void texCoordPointer (in PRUint8 size, in PRUint32 type, in object [] texArray); */
/* void texCoordPointer (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::TexCoordPointer()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    CanvasGLES11Buffer *newBuffer = nsnull;
    nsresult rv;

    if (js.argc == 1) {
        if (!JSVAL_IS_OBJECT(js.argv[0]) ||
            JSVAL_IS_NULL(js.argv[0]))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<nsISupports> iface;
        rv = gXPConnect->WrapJS(js.ctx, JSVAL_TO_OBJECT(js.argv[0]),
                                NS_GET_IID(nsICanvasRenderingContextGLES11Buffer),
                                getter_AddRefs(iface));
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        // what we want is a CanvasGLES11Buffer; QI to our magic IID
        // will return this interface
        nsICanvasRenderingContextGLES11Buffer *bufferBase = nsnull;
        rv = iface->QueryInterface(CanvasGLES11BufferIID, (void **) &bufferBase);
        if (NS_FAILED(rv) || !bufferBase)
            return NS_ERROR_FAILURE;

        newBuffer = NS_STATIC_CAST(CanvasGLES11Buffer*, bufferBase);

        if (newBuffer->mType != GL_SHORT &&
            newBuffer->mType != GL_FLOAT)
            return NS_ERROR_DOM_SYNTAX_ERR;

        if (newBuffer->mSize != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

    } else if (js.argc == 3) {
        jsuint sizeParam;
        jsuint typeParam;

        JSObject *arrayObj;
        jsuint arrayLen;

        if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &sizeParam) ||
            !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &typeParam) ||
            !JSValToJSArrayAndLength(js.ctx, js.argv[2], &arrayObj, &arrayLen))
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        if (typeParam != GL_SHORT &&
            typeParam != GL_FLOAT)
            return NS_ERROR_DOM_SYNTAX_ERR;

        // only 2d textures for now
        if (sizeParam != 2)
            return NS_ERROR_DOM_SYNTAX_ERR;

        newBuffer = new CanvasGLES11Buffer(this);
        if (!newBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = newBuffer->Init (GL_STATIC_DRAW, sizeParam, typeParam, js.ctx, arrayObj, arrayLen);
        if (NS_FAILED(rv)) {
            delete newBuffer;
            return rv;
        }
    } else {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    mTexCoordBuffer[mActiveTextureUnit] = newBuffer;

    MakeContextCurrent();
    glTexCoordPointer(newBuffer->GetSimpleBuffer().sizePerVertex,
                      newBuffer->GetSimpleBuffer().type,
                      0,
                      newBuffer->GetSimpleBuffer().data);
    return NS_OK;
}

/* void colorPointer (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::ColorPointer()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    CanvasGLES11Buffer *newBuffer = nsnull;
    nsresult rv;

    if (js.argc == 1) {
        if (!JSVAL_IS_OBJECT(js.argv[0]) ||
            JSVAL_IS_NULL(js.argv[0]))
            return NS_ERROR_DOM_SYNTAX_ERR;

        nsCOMPtr<nsISupports> iface;
        rv = gXPConnect->WrapJS(js.ctx, JSVAL_TO_OBJECT(js.argv[0]),
                                NS_GET_IID(nsICanvasRenderingContextGLES11Buffer),
                                getter_AddRefs(iface));
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        // what we want is a CanvasGLES11Buffer; QI to our magic IID
        // will return this interface
        nsICanvasRenderingContextGLES11Buffer *bufferBase = nsnull;
        rv = iface->QueryInterface(CanvasGLES11BufferIID, (void **) &bufferBase);
        if (NS_FAILED(rv) || !bufferBase)
            return NS_ERROR_FAILURE;

        newBuffer = NS_STATIC_CAST(CanvasGLES11Buffer*, bufferBase);

        if (newBuffer->mType != GL_UNSIGNED_BYTE &&
            newBuffer->mType != GL_FLOAT)
            return NS_ERROR_DOM_SYNTAX_ERR;

        if (newBuffer->mSize != 4)
            return NS_ERROR_DOM_SYNTAX_ERR;
    } else if (js.argc == 3) {
        jsuint sizeParam;
        jsuint typeParam;

        JSObject *arrayObj;
        jsuint arrayLen;

        if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &sizeParam) ||
            !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &typeParam) ||
            !JSValToJSArrayAndLength(js.ctx, js.argv[2], &arrayObj, &arrayLen))
        {
            return NS_ERROR_DOM_SYNTAX_ERR;
        }

        // XXapi if we're going to require sizeParam to be 4 here,
        // why not just omit the parameter?
        if (typeParam != GL_UNSIGNED_BYTE &&
            typeParam != GL_FLOAT &&
            sizeParam != 4)
            return NS_ERROR_DOM_SYNTAX_ERR;

        newBuffer = new CanvasGLES11Buffer(this);
        if (!newBuffer)
            return NS_ERROR_OUT_OF_MEMORY;

        rv = newBuffer->Init (GL_STATIC_DRAW, sizeParam, typeParam, js.ctx, arrayObj, arrayLen);
        if (NS_FAILED(rv)) {
            delete newBuffer;
            return rv;
        }
    }

    mColorBuffer = newBuffer;

    MakeContextCurrent();
    glColorPointer(newBuffer->GetSimpleBuffer().sizePerVertex,
                   newBuffer->GetSimpleBuffer().type,
                   0,
                   newBuffer->GetSimpleBuffer().data);
    return NS_OK;
}

/* void drawElements (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::DrawElements()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3)
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint modeParam;
    jsuint countParam;

    JSObject *arrayObj;
    jsuint arrayLen;

    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &modeParam) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &countParam) ||
        !JSValToJSArrayAndLength(js.ctx, js.argv[2], &arrayObj, &arrayLen))
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (modeParam != GL_POINTS &&
        modeParam != GL_LINES &&
        modeParam != GL_LINE_STRIP &&
        modeParam != GL_LINE_LOOP &&
        modeParam != GL_TRIANGLES &&
        modeParam != GL_TRIANGLE_STRIP &&
        modeParam != GL_TRIANGLE_FAN)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (countParam > arrayLen)
        return NS_ERROR_DOM_SYNTAX_ERR;

    // we're always going to do this as GL_UNSIGNED_SHORT
    SimpleBuffer indexBuffer;
    JSArrayToSimpleBuffer(indexBuffer, GL_UNSIGNED_SHORT, 1, js.ctx, arrayObj, countParam);

    // verify that the indices are valid; we should skip this step
    // if the caller is trusted
    PRUint32 vLen, nLen, cLen;

    if (mVertexBuffer)
        vLen = RealVertexBuffer()->mLength / RealVertexBuffer()->mSize;
    if (mNormalBuffer)
        nLen = RealNormalBuffer()->mLength / 3;
    if (mColorBuffer)
        cLen = RealColorBuffer()->mLength / 4;

    PRUint16 *indices = (PRUint16*) indexBuffer.data;
    for (PRUint32 i = 0; i < countParam; i++) {
        if ((mVertexArrayEnabled && indices[i] >= vLen) ||
            (mNormalArrayEnabled && indices[i] >= nLen) ||
            (mColorArrayEnabled && indices[i] >= cLen))
            return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    MakeContextCurrent();
    glDrawElements(modeParam, countParam, indexBuffer.type, indexBuffer.data);
    return NS_OK;
}

/* void drawArrays (in PRUint32 mode, in PRUint32 first, in PRUint32 count); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::DrawArrays(PRUint32 mode, PRUint32 first, PRUint32 count)
{
    // XXX error check
    MakeContextCurrent();
    glDrawArrays (mode, first, count);
    return NS_OK;
}

/* void depthRange (in float zNear, in float zFar); */
GL_SAME_METHOD_2(DepthRange, DepthRange, float, float)

/* void viewport (in PRInt32 x, in PRInt32 y, in PRInt32 width, in PRInt32 height); */
GL_SAME_METHOD_4(Viewport, Viewport, PRInt32, PRInt32, PRInt32, PRInt32)

/* void matrixMode (in PRUint32 mode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::MatrixMode(PRUint32 mode)
{
    MakeContextCurrent();
    glMatrixMode(mode);
    return NS_OK;
}

/* void loadMatrix (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::LoadMatrix()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    double mat[16];
    if (!JSValToDoubleArray(js.ctx, js.argv[0], 16, &mat[0]))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glLoadMatrixd(mat);
    return NS_OK;
}

/* void multMatrix (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::MultMatrix()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    double mat[16];
    if (!JSValToDoubleArray(js.ctx, js.argv[0], 16, &mat[0]))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glMultMatrixd(mat);
    return NS_OK;
}

/* void loadIdentity (); */
GL_SAME_METHOD_0(LoadIdentity, LoadIdentity)

/* void rotate (in float angle, in float x, in float y, in float z); */
GL_SAME_METHOD_4(Rotatef, Rotate, float, float, float, float)

/* void scale (in float x, in float y, in float z); */
GL_SAME_METHOD_3(Scalef, Scale, float, float, float)

/* void translate (in float x, in float y, in float z); */
GL_SAME_METHOD_3(Translatef, Translate, float, float, float)

/* void frustum (in float left, in float right, in float bottom, in float top, in float zNear, in float zFar); */
GL_SAME_METHOD_6(Frustum, Frustum, float, float, float, float, float, float)

/* void ortho (in float left, in float right, in float bottom, in float top, in float zNear, in float zFar); */
GL_SAME_METHOD_6(Ortho, Ortho, float, float, float, float, float, float)

/* void activeTexture (in PRUint32 texture); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::ActiveTexture(PRUint32 texture)
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

/* void pushMatrix (); */
GL_SAME_METHOD_0(PushMatrix, PushMatrix)

/* void popMatrix (); */
GL_SAME_METHOD_0(PopMatrix, PopMatrix)

/* void frontFace (in PRUint32 face); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::FrontFace(PRUint32 face)
{
    MakeContextCurrent();
    glFrontFace(face);
    return NS_OK;
}

/* void material (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::Material()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3)
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint faceVal;
    jsuint pnameVal;
    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &faceVal) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &pnameVal))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (faceVal != GL_FRONT_AND_BACK)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (pnameVal) {
        case GL_SHININESS: {
            jsdouble dval;
            if (!::JS_ValueToNumber(js.ctx, js.argv[2], &dval))
                return NS_ERROR_DOM_SYNTAX_ERR;

            glMaterialf (faceVal, pnameVal, (float) dval);
        }
            break;
        case GL_AMBIENT:
        case GL_DIFFUSE:
        case GL_SPECULAR:
        case GL_EMISSION:
        case GL_AMBIENT_AND_DIFFUSE: {
            float fv[4];
            if (!JSValToFloatArray(js.ctx, js.argv[2], 4, fv))
                return NS_ERROR_DOM_SYNTAX_ERR;

            glMaterialfv (faceVal, pnameVal, fv);
        }
            break;
        default:
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
}

/* void getMaterial (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GetMaterial()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void light (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::Light()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 3)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    jsuint lightIndex;
    jsuint lightParam;

    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &lightIndex) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &lightParam))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (lightParam) {
        case SPOT_EXPONENT:
        case SPOT_CUTOFF:
        case CONSTANT_ATTENUATION:
        case LINEAR_ATTENUATION:
        case QUADRATIC_ATTENUATION: {
            jsdouble dv;
            if (!::JS_ValueToNumber(js.ctx, js.argv[2], &dv))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glLightf(lightIndex, lightParam, (float)dv);
        }
            break;
        case AMBIENT:
        case DIFFUSE:
        case SPECULAR:
        case POSITION:
        case SPOT_DIRECTION: {
            float fv[4];
            int cnt = 4;
            if (lightParam == GL_SPOT_DIRECTION)
                cnt = 3;

            if (!JSValToFloatArray(js.ctx, js.argv[2], cnt, fv))
                return NS_ERROR_DOM_SYNTAX_ERR;

            glLightfv(lightIndex, lightParam, fv);
        }
            break;
        default:
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
}

/* void getLight (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GetLight()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void lightModel (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::LightModel()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 2)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    jsuint lightParam;

    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &lightParam))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (lightParam) {
        case LIGHT_MODEL_COLOR_CONTROL:
        case LIGHT_MODEL_LOCAL_VIEWER:
        case LIGHT_MODEL_TWO_SIDE: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[1], &ival))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glLightModeli(lightParam, (int) ival);
        }
            break;
        case LIGHT_MODEL_AMBIENT: {
            float fv[4];
            if (!JSValToFloatArray(js.ctx, js.argv[1], 4, fv))
                return NS_ERROR_DOM_SYNTAX_ERR;

            glLightModelfv(lightParam, fv);
        }
            break;
        default:
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
}

/* void shadeModel (in PRUint32 pname); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::ShadeModel(PRUint32 pname)
{
    MakeContextCurrent();
    glShadeModel(pname);
    return NS_OK;
}

/* void pointSize (in float size); */
GL_SAME_METHOD_1(PointSize, PointSize, float)

/* void pointParameter (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::PointParameter()
{
    if (!GLEW_ARB_point_parameters)
        return NS_ERROR_NOT_IMPLEMENTED;

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint pnameVal;
    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &pnameVal))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (pnameVal) {
        case GL_POINT_SIZE_MIN:
        case GL_POINT_SIZE_MAX:
        case GL_POINT_FADE_THRESHOLD_SIZE: {
            jsdouble dval;
            if (!::JS_ValueToNumber(js.ctx, js.argv[1], &dval))
                return NS_ERROR_DOM_SYNTAX_ERR;

            glPointParameterf (pnameVal, (float) dval);
        }
            break;
        case GL_POINT_DISTANCE_ATTENUATION: {
            float fv[3];
            if (!JSValToFloatArray(js.ctx, js.argv[1], 3, fv))
                return NS_ERROR_DOM_SYNTAX_ERR;

            glPointParameterfv (pnameVal, fv);
        }
            break;
        default:
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
}

/* void lineWidth (in float width); */
GL_SAME_METHOD_1(LineWidth, LineWidth, float)

/* void cullFace (in PRUint32 mode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::CullFace(PRUint32 mode)
{
    MakeContextCurrent();
    glCullFace(mode);
    return NS_OK;
}

/* void polygonOffset (in float factor, in float units); */
GL_SAME_METHOD_2(PolygonOffset, PolygonOffset, float, float)

/* nsICanvasRenderingContextGLES11Texture createTextureObject (in nsIDOMHTMLElement imageOrCanvas); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::CreateTextureObject(nsIDOMHTMLElement *imageOrCanvas,
                                                    nsICanvasRenderingContextGLES11Texture **aTextureObject)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::BindTextureObject(nsICanvasRenderingContextGLES11Texture *aTextureObject)
{
    if (!aTextureObject)
        return NS_ERROR_INVALID_ARG;

    CanvasGLES11Texture *texObj = (CanvasGLES11Texture*) aTextureObject;
    if (texObj->mDisposed)
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();
    glitz_context_bind_texture(mGlitzContext, texObj->mGlitzTextureObject);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::DeleteTextureObject(nsICanvasRenderingContextGLES11Texture *aTextureObject)
{
    if (!aTextureObject)
        return NS_ERROR_INVALID_ARG;

    CanvasGLES11Texture *texObj = (CanvasGLES11Texture*) aTextureObject;
    if (texObj->mDisposed)
        return NS_ERROR_INVALID_ARG;

    MakeContextCurrent();
    texObj->Dispose();
    return NS_OK;
}

/* void texImage2DHTML (in PRUint32 target, in nsIDOMHTMLElement imageOrCanvas); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::TexImage2DHTML(PRUint32 target, nsIDOMHTMLElement *imageOrCanvas)
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

/* void texParameter (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::TexParameter()
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
nsCanvasRenderingContextGLES11::GetTexParameter()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void bindTexture (in PRUint32 target, in PRUint32 texid); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::BindTexture(PRUint32 target, PRUint32 texid)
{
    if (target != GL_TEXTURE_2D &&
        (!GLEW_ARB_texture_rectangle || target != GL_TEXTURE_RECTANGLE_ARB))
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    MakeContextCurrent();
    glBindTexture(target, texid);
    return NS_OK;
}

/* void deleteTextures () */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::DeleteTextures()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (!JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen)) {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

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

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GenTextures(PRUint32 n)
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

/* void texEnv (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::TexEnv()
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

    if (targetVal != GL_TEXTURE_ENV)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (pnameVal) {
        case GL_TEXTURE_ENV_MODE: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_MODULATE &&
                 ival != GL_REPLACE &&
                 ival != GL_DECAL &&
                 ival != GL_BLEND &&
                 ival != GL_ADD &&
                 ival != GL_COMBINE))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexEnvi (targetVal, pnameVal, ival);
        }
            break;
        case GL_TEXTURE_ENV_COLOR: {
            float fv[4];
            if (!JSValToFloatArray(js.ctx, js.argv[2], 4, fv))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexEnvfv (targetVal, pnameVal, fv);
        }
            break;
        case GL_COMBINE_RGB:
        case GL_COMBINE_ALPHA: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_MODULATE &&
                 ival != GL_REPLACE &&
                 ival != GL_ADD &&
                 ival != GL_ADD_SIGNED &&
                 ival != GL_INTERPOLATE))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexEnvi (targetVal, pnameVal, ival);
        }
            break;
        case GL_SRC0_RGB:
        case GL_SRC1_RGB:
        case GL_SRC2_RGB:
        case GL_SRC0_ALPHA:
        case GL_SRC1_ALPHA:
        case GL_SRC2_ALPHA: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_PRIMARY_COLOR &&
                 ival != GL_CONSTANT &&
                 ival != GL_PREVIOUS &&
                 (ival < GL_TEXTURE0 || ival > (GL_TEXTURE0+MAX_TEXTURE_UNITS))))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexEnvi (targetVal, pnameVal, ival);
        }
            break;
        case GL_OPERAND0_RGB:
        case GL_OPERAND1_RGB:
        case GL_OPERAND2_RGB: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_SRC_COLOR &&
                 ival != GL_ONE_MINUS_SRC_COLOR &&
                 ival != GL_SRC_ALPHA &&
                 ival != GL_ONE_MINUS_SRC_ALPHA))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexEnvi (targetVal, pnameVal, ival);
        }
                break;
        case GL_OPERAND0_ALPHA:
        case GL_OPERAND1_ALPHA:
        case GL_OPERAND2_ALPHA: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[2], &ival) ||
                (ival != GL_SRC_ALPHA &&
                 ival != GL_ONE_MINUS_SRC_ALPHA))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexEnvi (targetVal, pnameVal, ival);
        }
            break;
        case RGB_SCALE:
        case ALPHA_SCALE: {
            jsdouble dval;
            if (!::JS_ValueToNumber(js.ctx, js.argv[2], &dval))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glTexEnvf (targetVal, pnameVal, (float)dval);
        }
            break;
        default:
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
}

/* void getTexEnv (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GetTexEnv()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void fogParameter (); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::FogParameter()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 2)
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint pnameVal;
    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &pnameVal))
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    switch (pnameVal) {
        case FOG_MODE: {
            jsuint ival;
            if (!::JS_ValueToECMAUint32(js.ctx, js.argv[1], &ival) ||
                (ival != GL_LINEAR &&
                 ival != GL_EXP &&
                 ival != GL_EXP2))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glFogi (pnameVal, ival);
        }
            break;
        case FOG_DENSITY:
        case FOG_START:
        case FOG_END: {
            jsdouble dval;
            if (!::JS_ValueToNumber(js.ctx, js.argv[1], &dval))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glFogf (pnameVal, (float) dval);
        }
            break;
        case FOG_COLOR: {
            float fv[4];
            if (!JSValToFloatArray(js.ctx, js.argv[1], 4, fv))
                return NS_ERROR_DOM_SYNTAX_ERR;
            glFogfv (pnameVal, fv);
        }
            break;
        default:
            return NS_ERROR_DOM_SYNTAX_ERR;
    }

    return NS_OK;
}

/* void scissor (in PRInt32 x, in PRInt32 y, in PRInt32 width, in PRInt32 height); */
GL_SAME_METHOD_4(Scissor, Scissor, PRInt32, PRInt32, PRInt32, PRInt32)

/* void sampleCoverage (in float value, in boolean invert); */
GL_SAME_METHOD_2(SampleCoverage, SampleCoverage, float, PRBool)

/* void alphaFunc (in PRUint32 func, in float ref); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::AlphaFunc(PRUint32 func, float ref)
{
    MakeContextCurrent();
    glAlphaFunc(func, ref);
    return NS_OK;
}

/* void stencilFunc (in PRUint32 func, in PRInt32 ref, in PRUint32 mask); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::StencilFunc(PRUint32 func, PRInt32 ref, PRUint32 mask)
{
    if (func != GL_NEVER &&
        func != GL_LESS &&
        func != GL_LEQUAL &&
        func != GL_GREATER &&
        func != GL_GEQUAL &&
        func != GL_EQUAL &&
        func != GL_NOTEQUAL &&
        func != GL_ALWAYS)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glStencilFunc(func, ref, mask);
    return NS_OK;
}

/* void stencilMask (in PRUint32 mask); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::StencilMask(PRUint32 mask)
{
    MakeContextCurrent();
    glStencilMask(mask);
    return NS_OK;
}

/* void stencilOp (in PRUint32 fail, in PRUint32 zfail, in PRUint32 zpass); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::StencilOp(PRUint32 fail, PRUint32 zfail, PRUint32 zpass)
{
    MakeContextCurrent();
    glStencilOp(fail, zfail, zpass);
    return NS_OK;
}

/* void depthFunc (in PRUint32 func); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::DepthFunc(PRUint32 func)
{
    MakeContextCurrent();
    glDepthFunc(func);
    return NS_OK;
}

/* void depthMask (in boolean flag); */
GL_SAME_METHOD_1(DepthMask, DepthMask, PRBool)

/* void blendFunc (in PRUint32 sfactor, in PRUint32 dfactor); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::BlendFunc(PRUint32 sfactor, PRUint32 dfactor)
{
    MakeContextCurrent();
    glBlendFunc(sfactor, dfactor);
    return NS_OK;
}

/* void logicOp (in PRUint32 opcode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::LogicOp(PRUint32 opcode)
{
    MakeContextCurrent();
    glLogicOp(opcode);
    return NS_OK;
}

/* void colorMask (in boolean red, in boolean green, in boolean blue, in boolean alpha); */
GL_SAME_METHOD_4(ColorMask, ColorMask, PRBool, PRBool, PRBool, PRBool)

/* void clear (in PRUint32 mask); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::Clear(PRUint32 mask)
{
    MakeContextCurrent();
    glClear(mask);
    return NS_OK;
}

/* void clearColor (in float red, in float green, in float blue, in float alpha); */
GL_SAME_METHOD_4(ClearColor, ClearColor, float, float, float, float)

/* void clearDepth (in float depth); */
GL_SAME_METHOD_1(ClearDepth, ClearDepth, float)

/* void clearStencil (in PRInt32 s); */
GL_SAME_METHOD_1(ClearStencil, ClearStencil, PRInt32)

/* void hint (in PRUint32 target, in PRUint32 mode); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::Hint(PRUint32 target, PRUint32 mode)
{
    //return NS_ERROR_NOT_IMPLEMENTED;
    return NS_OK;
}

/* void getParameter (in PRUint32 pname); */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GetParameter(PRUint32 pname)
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    MakeContextCurrent();

    jsval result = JSVAL_VOID;
    JSObject *rootToRelease = nsnull;

    // XXX incomplete
    switch (pname) {
        case GL_ACCUM_ALPHA_BITS:
        case GL_ACCUM_BLUE_BITS:
        case GL_ACCUM_GREEN_BITS:
        case GL_ACCUM_RED_BITS:
        case GL_ALPHA_BIAS:
        case GL_ALPHA_BITS:
        case GL_ALPHA_SCALE:
        case GL_ALPHA_TEST_FUNC:
        case GL_ALPHA_TEST_REF:
        case GL_ATTRIB_STACK_DEPTH:
        case GL_AUX_BUFFERS:
        case GL_BLEND_DST:
        case GL_BLEND_SRC:
        case GL_BLUE_BIAS:
        case GL_BLUE_BITS:
        case GL_BLUE_SCALE:
        case GL_COLOR_MATERIAL_FACE:
        case GL_COLOR_MATERIAL_PARAMETER: {
            float fv;
            glGetFloatv (pname, &fv);
            if (!JS_NewDoubleValue(js.ctx, (jsdouble) fv, &result))
                return NS_ERROR_OUT_OF_MEMORY;
        }
            break;

        case GL_ALPHA_TEST:
        case GL_AUTO_NORMAL:
        case GL_BLEND:
        case GL_CLIP_PLANE0:
        case GL_CLIP_PLANE1:
        case GL_CLIP_PLANE2:
        case GL_CLIP_PLANE3:
        case GL_CLIP_PLANE4:
        case GL_CLIP_PLANE5:
        case GL_COLOR_MATERIAL: {
            GLboolean bv;
            glGetBooleanv (pname, &bv);
            if (bv)
                result = JSVAL_TRUE;
            else
                result = JSVAL_FALSE;
        }
            break;

/*
        case GL_COLOR_CLEAR_VALUE:
            break;
*/

        case GL_MODELVIEW_MATRIX:
        case GL_PROJECTION_MATRIX:
        case GL_TEXTURE_MATRIX: {
            float mat[16];
            glGetFloatv (pname, &mat[0]);
            nsAutoArrayPtr<jsval> jsvector(new jsval[16]);
            for (int i = 0; i < 16; i++) {
                if (!JS_NewDoubleValue(js.ctx, (jsdouble) mat[i], &jsvector[i]))
                    return NS_ERROR_OUT_OF_MEMORY;
            }

            JSObject *jsarr = JS_NewArrayObject(js.ctx, 16, jsvector.get());
            if (!jsarr)
                return NS_ERROR_OUT_OF_MEMORY;

            js.addGCRoot (jsarr, "glGetParameter");
            rootToRelease = jsarr;

            result = OBJECT_TO_JSVAL(jsarr);
        }
            break;

        default:
            return NS_ERROR_NOT_IMPLEMENTED;
    }

    if (result == JSVAL_VOID)
        return NS_ERROR_NOT_IMPLEMENTED;

    jsval *retvalPtr;
    js.ncc->GetRetValPtr(&retvalPtr);
    *retvalPtr = result;
    js.ncc->SetReturnValueWasSet(PR_TRUE);

    if (rootToRelease)
        js.releaseGCRoot(rootToRelease);

    return NS_OK;
}

#if 0
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::BindBufferObject(PRUint32 target,
                                                 nsICanvasRenderingContextGLES11Buffer *obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::CreateBuffer(nsICanvasRenderingContextGLES11Buffer **obj)
{
    nsresult rv;

    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 4)
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint usageParam;
    jsuint sizeParam;
    jsuint typeParam;
    JSObject *arrayObj;
    jsuint arrayLen;

    if (!::JS_ValueToECMAUint32(js.ctx, js.argv[0], &usageParam) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[1], &sizeParam) ||
        !::JS_ValueToECMAUint32(js.ctx, js.argv[2], &typeParam) ||
        !JSValToJSArrayAndLength(js.ctx, js.argv[3], &arrayObj, &arrayLen))
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    CanvasGLES11Buffer *buffer = new CanvasGLES11Buffer(this);
    if (!buffer)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = buffer->Init(usageParam, sizeParam, typeParam, js.ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv)) {
        delete buffer;
        return rv;
    }

    NS_ADDREF(*obj = buffer);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::BindBuffer(PRUint32 target, PRUint32 buffer)
{
    if (target != GL_ARRAY_BUFFER &&
        target != GL_ELEMENT_ARRAY_BUFFER)
        return NS_ERROR_DOM_SYNTAX_ERR;

    MakeContextCurrent();
    glBindBuffer (target, buffer);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GenBuffers(PRUint32 n)
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
nsCanvasRenderingContextGLES11::DeleteBuffers()
{
    NativeJSContext js;
    if (NS_FAILED(js.error))
        return js.error;

    if (js.argc != 1)
        return NS_ERROR_DOM_SYNTAX_ERR;

    JSObject *arrayObj;
    jsuint arrayLen;
    if (JSValToJSArrayAndLength(js.ctx, js.argv[0], &arrayObj, &arrayLen)) {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

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

/* target, array, type, usage */
NS_IMETHODIMP
nsCanvasRenderingContextGLES11::BufferData()
{
    NativeJSContext js;
    if (!js.error)
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
nsCanvasRenderingContextGLES11::BufferSubData()
{
    NativeJSContext js;
    if (!js.error)
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

/**
 ** Helpers that really should be in some sort of cross-context shared library
 **/

nsresult
nsCanvasRenderingContextGLES11::CairoSurfaceFromElement(nsIDOMElement *imgElt,
                                                        cairo_surface_t **aCairoSurface,
                                                        PRUint8 **imgData,
                                                        PRInt32 *widthOut, PRInt32 *heightOut,
                                                        nsIURI **uriOut, PRBool *forceWriteOnlyOut)
{
    nsresult rv;

    nsCOMPtr<imgIContainer> imgContainer;

    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgElt);
    if (imageLoader) {
        nsCOMPtr<imgIRequest> imgRequest;
        rv = imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                     getter_AddRefs(imgRequest));
        NS_ENSURE_SUCCESS(rv, rv);
        if (!imgRequest)
            // XXX ERRMSG we need to report an error to developers here! (bug 329026)
            return NS_ERROR_NOT_AVAILABLE;

        nsCOMPtr<nsIURI> uri;
        rv = imageLoader->GetCurrentURI(uriOut);
        NS_ENSURE_SUCCESS(rv, rv);
       
        *forceWriteOnlyOut = PR_FALSE;

        rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        // maybe a canvas
        nsCOMPtr<nsICanvasElement> canvas = do_QueryInterface(imgElt);
        if (canvas) {
            PRUint32 w, h;
            rv = canvas->GetSize(&w, &h);
            NS_ENSURE_SUCCESS(rv, rv);

            PRUint8 *data = (PRUint8*) PR_Malloc(w * h * 4);
            cairo_surface_t *surf =
                cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32,
                                                     w, h, w*4);
            cairo_t *cr = cairo_create (surf);
            cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint (cr);
            cairo_destroy (cr);

            rv = canvas->RenderContextsToSurface(surf);
            if (NS_FAILED(rv)) {
                cairo_surface_destroy (surf);
                return rv;
            }

            *aCairoSurface = surf;
            *imgData = data;
            *widthOut = w;
            *heightOut = h;

            *uriOut = nsnull;
            *forceWriteOnlyOut = canvas->IsWriteOnly();

            return NS_OK;
        } else {
            NS_WARNING("No way to get surface from non-canvas, non-imageloader");
            return NS_ERROR_NOT_AVAILABLE;
        }
    }

    if (!imgContainer)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<gfxIImageFrame> frame;
    rv = imgContainer->GetCurrentFrame(getter_AddRefs(frame));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIImage> img(do_GetInterface(frame));

    PRInt32 imgWidth, imgHeight;
    rv = frame->GetWidth(&imgWidth);
    rv |= frame->GetHeight(&imgHeight);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    if (widthOut)
        *widthOut = imgWidth;
    if (heightOut)
        *heightOut = imgHeight;

#ifdef MOZ_CAIRO_GFX
    gfxASurface* gfxsurf = nsnull;
    rv = img->GetSurface(&gfxsurf);
    NS_ENSURE_SUCCESS(rv, rv);

    *aCairoSurface = gfxsurf->CairoSurface();
    cairo_surface_reference (*aCairoSurface);
    *imgData = nsnull;
#else
    //
    // We now need to create a cairo_surface with the same data as
    // this image element.
    //

    PRUint8 *cairoImgData = (PRUint8 *)nsMemory::Alloc(imgHeight * imgWidth * 4);
    PRUint8 *outData = cairoImgData;

    gfx_format format;
    rv = frame->GetFormat(&format);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = frame->LockImageData();
    if (img->GetHasAlphaMask())
        rv |= frame->LockAlphaData();
    if (NS_FAILED(rv)) {
        nsMemory::Free(cairoImgData);
        return NS_ERROR_FAILURE;
    }

    PRUint8 *inPixBits, *inAlphaBits = nsnull;
    PRUint32 inPixStride, inAlphaStride = 0;
    inPixBits = img->GetBits();
    inPixStride = img->GetLineStride();
    if (img->GetHasAlphaMask()) {
        inAlphaBits = img->GetAlphaBits();
        inAlphaStride = img->GetAlphaLineStride();
    }

    PRBool topToBottom = img->GetIsRowOrderTopToBottom();
    PRBool useBGR;

    // The gtk backend optimizes away the alpha mask of images
    // with a fully opaque alpha, but doesn't update its format (bug?);
    // you end up with a RGB_A8 image with GetHasAlphaMask() == false.
    // We need to treat that case as RGB.

    if ((format == gfxIFormats::RGB || format == gfxIFormats::BGR) ||
        (!(img->GetHasAlphaMask()) && (format == gfxIFormats::RGB_A8 || format == gfxIFormats::BGR_A8)))
    {
        useBGR = (format & 1);

#ifdef IS_BIG_ENDIAN
        useBGR = !useBGR;
#endif

        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint32 rowIndex;
            if (topToBottom)
                rowIndex = j;
            else
                rowIndex = imgHeight - j - 1;

            PRUint8 *inrowrgb = inPixBits + (inPixStride * rowIndex);

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // handle rgb data; no alpha to premultiply
#ifdef XP_MACOSX
                // skip extra OSX byte
                inrowrgb++;
#endif
                PRUint8 r, g, b;
                if (useBGR) {
                    b = *inrowrgb++;
                    g = *inrowrgb++;
                    r = *inrowrgb++;
                } else {
                    r = *inrowrgb++;
                    g = *inrowrgb++;
                    b = *inrowrgb++;
                }

#ifdef IS_BIG_ENDIAN
                // alpha
                *outData++ = 0xff;
#endif

                *outData++ = r;
                *outData++ = g;
                *outData++ = b;

#ifdef IS_LITTLE_ENDIAN
                // alpha
                *outData++ = 0xff;
#endif
            }
        }
        rv = NS_OK;
    } else if (format == gfxIFormats::RGB_A1 || format == gfxIFormats::BGR_A1) {
        useBGR = (format & 1);

#ifdef IS_BIG_ENDIAN
        useBGR = !useBGR;
#endif

        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint32 rowIndex;
            if (topToBottom)
                rowIndex = j;
            else
                rowIndex = imgHeight - j - 1;

            PRUint8 *inrowrgb = inPixBits + (inPixStride * rowIndex);
            PRUint8 *inrowalpha = inAlphaBits + (inAlphaStride * rowIndex);

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // pull out the bit value into alpha
                PRInt32 bit = i % 8;
                PRInt32 byte = i / 8;

#ifdef IS_LITTLE_ENDIAN
                PRUint8 a = (inrowalpha[byte] >> (7-bit)) & 1;
#else
                PRUint8 a = (inrowalpha[byte] >> bit) & 1;
#endif

#ifdef XP_MACOSX
                // skip extra X8 byte on OSX
                inrowrgb++;
#endif

                // handle rgb data; need to multiply the alpha out,
                // but we short-circuit that here since we know that a
                // can only be 0 or 1
                if (a) {
                    PRUint8 r, g, b;

                    if (useBGR) {
                        b = *inrowrgb++;
                        g = *inrowrgb++;
                        r = *inrowrgb++;
                    } else {
                        r = *inrowrgb++;
                        g = *inrowrgb++;
                        b = *inrowrgb++;
                    }

#ifdef IS_BIG_ENDIAN
                    // alpha
                    *outData++ = 0xff;
#endif

                    *outData++ = r;
                    *outData++ = g;
                    *outData++ = b;

#ifdef IS_LITTLE_ENDIAN
                    // alpha
                    *outData++ = 0xff;
#endif
                } else {
                    // alpha is 0, so we need to write all 0's,
                    // ignoring input color
                    inrowrgb += 3;
                    *outData++ = 0;
                    *outData++ = 0;
                    *outData++ = 0;
                    *outData++ = 0;
                }
            }
        }
        rv = NS_OK;
    } else if (format == gfxIFormats::RGB_A8 || format == gfxIFormats::BGR_A8) {
        useBGR = (format & 1);

#ifdef IS_BIG_ENDIAN
        useBGR = !useBGR;
#endif

        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint32 rowIndex;
            if (topToBottom)
                rowIndex = j;
            else
                rowIndex = imgHeight - j - 1;

            PRUint8 *inrowrgb = inPixBits + (inPixStride * rowIndex);
            PRUint8 *inrowalpha = inAlphaBits + (inAlphaStride * rowIndex);

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // pull out alpha; we'll need it to premultiply
                PRUint8 a = *inrowalpha++;

                // handle rgb data; we need to fully premultiply
                // with the alpha
#ifdef XP_MACOSX
                // skip extra X8 byte on OSX
                inrowrgb++;
#endif

                // XXX gcc bug: gcc seems to push "r" into a register
                // early, and pretends that it's in that register
                // throughout the 3 macros below.  At the end
                // of the 3rd macro, the correct r value is
                // calculated but never stored anywhere -- the r variable
                // has the value of the low byte of register that it
                // was stuffed into, which has the result of some 
                // intermediate calculation.
                // I've seen this on gcc 3.4.2 x86 (Fedora Core 3)
                // and gcc 3.3 PPC (OS X 10.3)

                //PRUint8 b, g, r;
                //FAST_DIVIDE_BY_255(b, *inrowrgb++ * a - a / 2);
                //FAST_DIVIDE_BY_255(g, *inrowrgb++ * a - a / 2);
                //FAST_DIVIDE_BY_255(r, *inrowrgb++ * a - a / 2);

                PRUint8 r, g, b;
                if (useBGR) {
                    b = (*inrowrgb++ * a - a / 2) / 255;
                    g = (*inrowrgb++ * a - a / 2) / 255;
                    r = (*inrowrgb++ * a - a / 2) / 255;
                } else {
                    r = (*inrowrgb++ * a - a / 2) / 255;
                    g = (*inrowrgb++ * a - a / 2) / 255;
                    b = (*inrowrgb++ * a - a / 2) / 255;
                }

#ifdef IS_BIG_ENDIAN
                *outData++ = a;
#endif

                *outData++ = r;
                *outData++ = g;
                *outData++ = b;

#ifdef IS_LITTLE_ENDIAN
                *outData++ = a;
#endif
            }
        }
        rv = NS_OK;
    } else {
        rv = NS_ERROR_FAILURE;
    }

    if (img->GetHasAlphaMask())
        frame->UnlockAlphaData();
    frame->UnlockImageData();

    if (NS_FAILED(rv)) {
        nsMemory::Free(cairoImgData);
        return rv;
    }

    cairo_surface_t *imgSurf =
        cairo_image_surface_create_for_data(cairoImgData, CAIRO_FORMAT_ARGB32,
                                            imgWidth, imgHeight, imgWidth*4);

    *aCairoSurface = imgSurf;
    *imgData = cairoImgData;
#endif

    return NS_OK;
}

void
nsCanvasRenderingContextGLES11::DoDrawImageSecurityCheck(nsIURI* aURI, PRBool forceWriteOnly)
{
    /* So; this is causing dll woes under win32.  Until we figure that out, we just return. */
    return;

#if 0
    fprintf (stderr, "DoDrawImageSecuritycheck this 1: %p\n", this);
    if (mCanvasElement->IsWriteOnly())
        return;

    fprintf (stderr, "DoDrawImageSecuritycheck this 2: %p\n", this);
    if (!aURI)
        return;

    fprintf (stderr, "DoDrawImageSecuritycheck this 3: %p\n", this);
    if (forceWriteOnly) {
        mCanvasElement->SetWriteOnly();
        return;
    }

    fprintf (stderr, "DoDrawImageSecuritycheck this 4: %p\n", this);
    nsCOMPtr<nsIScriptSecurityManager> ssm =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (!ssm) {
        mCanvasElement->SetWriteOnly();
        return;
    }

    fprintf (stderr, "DoDrawImageSecuritycheck this 5: %p\n", this);
#ifdef MOZILLA_1_8_BRANCH
#if 0
    nsCOMPtr<nsIDOMNode> elem = do_QueryInterface(mCanvasElement);
    if (elem && ssm) {
        nsCOMPtr<nsIPrincipal> elemPrincipal;
        nsCOMPtr<nsIPrincipal> uriPrincipal;
        nsCOMPtr<nsIDocument> elemDocument;
        nsContentUtils::GetDocumentAndPrincipal(elem, getter_AddRefs(elemDocument), getter_AddRefs(elemPrincipal));
        ssm->GetCodebasePrincipal(aURI, getter_AddRefs(uriPrincipal));

        if (uriPrincipal && elemPrincipal) {
            nsresult rv =
                ssm->CheckSameOriginPrincipal(elemPrincipal, uriPrincipal);
            if (NS_SUCCEEDED(rv)) {
                // Same origin
                return;
            }
        }
    }
#endif
#else
    nsCOMPtr<nsINode> elem = do_QueryInterface(mCanvasElement);
    if (elem && ssm) {
        nsCOMPtr<nsIPrincipal> uriPrincipal;
        ssm->GetCodebasePrincipal(aURI, getter_AddRefs(uriPrincipal));

        if (uriPrincipal) {
            nsresult rv = ssm->CheckSameOriginPrincipal(elem->NodePrincipal(),
                                                        uriPrincipal);
            if (NS_SUCCEEDED(rv)) {
                // Same origin
                return;
            }
        }
    }
#endif

    fprintf (stderr, "DoDrawImageSecuritycheck this 6: %p\n", this); fflush(stderr);
    mCanvasElement->SetWriteOnly();
#endif
}

/**
 ** CanvasGLES11Texture
 **/

CanvasGLES11Texture::CanvasGLES11Texture(nsCanvasRenderingContextGLES11 *owner)
    : mDisposed(PR_FALSE), mOwnerContext (owner),
      mGlitzTextureSurface(nsnull), mGlitzTextureObject(nsnull),
      mWidth(0), mHeight(0)
{
}

CanvasGLES11Texture::~CanvasGLES11Texture()
{
    Dispose();
}

nsresult
CanvasGLES11Texture::Init()
{
    return NS_OK;
}

nsresult
CanvasGLES11Texture::Dispose()
{
    if (mDisposed)
        return NS_OK;

    mDisposed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Texture::GetDisposed(PRBool *retval)
{
    *retval = mDisposed;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Texture::GetOwnerContext(nsICanvasRenderingContextGLES11 **ownerContext)
{
    *ownerContext = mOwnerContext;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Texture::GetTarget(PRUint32 *aResult)
{
    *aResult = glitz_texture_object_get_target (mGlitzTextureObject);
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Texture::GetWidth(PRUint32 *aWidth)
{
    *aWidth = mWidth;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Texture::GetHeight(PRUint32 *aHeight)
{
    *aHeight = mHeight;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Texture::SetFilter(PRUint32 filterType, PRUint32 filterMode)
{
    if (filterType < 0 || filterType > 1 ||
        filterMode < 0 || filterMode > 1)
    {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    glitz_texture_object_set_filter (mGlitzTextureObject, (glitz_texture_filter_type_t)filterType, (glitz_texture_filter_t)filterMode);
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Texture::SetWrap(PRUint32 wrapType, PRUint32 wrapMode)
{
    if (wrapType != GLITZ_TEXTURE_WRAP_TYPE_S &&
        wrapType != GLITZ_TEXTURE_WRAP_TYPE_T)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (wrapMode != GLITZ_TEXTURE_WRAP_CLAMP_TO_EDGE &&
        wrapMode != GLITZ_TEXTURE_WRAP_REPEAT &&
        wrapMode != GLITZ_TEXTURE_WRAP_MIRRORED_REPEAT)
        return NS_ERROR_DOM_SYNTAX_ERR;

    glitz_texture_object_set_wrap (mGlitzTextureObject, (glitz_texture_wrap_type_t)wrapType, (glitz_texture_wrap_t)wrapMode);
    return NS_OK;
}

/**
 ** CanvasGLES11Buffer
 **/

CanvasGLES11Buffer::CanvasGLES11Buffer(nsCanvasRenderingContextGLES11 *owner)
    : mDisposed(PR_TRUE), mOwnerContext(owner),
      mLength(0), mSize(0), mType(0), mUsage(GL_STATIC_DRAW),
      mBufferID(0)
{
}

CanvasGLES11Buffer::~CanvasGLES11Buffer()
{
    Dispose();
}

/* nsISecurityCheckedComponent bits */

static char* cloneAllAccess()
{
    static const char allAccess[] = "allAccess";
    return (char*)nsMemory::Clone(allAccess, sizeof(allAccess));
}

NS_IMETHODIMP
CanvasGLES11Buffer::CanCreateWrapper(const nsIID* iid, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::CanCallMethod(const nsIID *iid, const PRUnichar *methodName, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::CanGetProperty(const nsIID *iid, const PRUnichar *propertyName, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::CanSetProperty(const nsIID *iid, const PRUnichar *propertyName, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

GLEWContext*
CanvasGLES11Buffer::glewGetContext()
{
    return mOwnerContext->glewGetContext();
}

nsresult
CanvasGLES11Buffer::Init(PRUint32 usage,
                         PRUint32 size,
                         PRUint32 type,
                         JSContext *ctx,
                         JSObject *arrayObj,
                         jsuint arrayLen)
{
    nsresult rv;

    fprintf (stderr, "CanvasGLES11Buffer::Init\n");

    if (!mDisposed)
        Dispose();

    if (usage != GL_STATIC_DRAW &&
        usage != GL_DYNAMIC_DRAW)
        return NS_ERROR_INVALID_ARG;

    rv = nsCanvasRenderingContextGLES11::JSArrayToSimpleBuffer(mSimpleBuffer, type, size, ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return rv;

    mUsage = usage;
    mSize = size;
    mType = type;
    mLength = arrayLen;

    mBufferID = 0;

    mDisposed = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::Dispose()
{
    if (mDisposed)
        return NS_OK;

    if (mBufferID) {
        mOwnerContext->MakeContextCurrent();
        glDeleteBuffers(1, &mBufferID);
        mBufferID = 0;
    }

    mSimpleBuffer.Release();

    mDisposed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::GetOwnerContext(nsICanvasRenderingContextGLES11 **retval)
{
    *retval = mOwnerContext;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::GetDisposed(PRBool *retval)
{
    *retval = mDisposed;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::GetUsage(PRUint32 *usage)
{
    if (mDisposed)
        return NS_ERROR_FAILURE;

    *usage = mUsage;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::GetLength(PRUint32 *retval)
{
    if (mDisposed)
        return NS_ERROR_FAILURE;

    *retval = mLength;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLES11Buffer::GetType(PRUint32 *retval)
{
    if (mDisposed)
        return NS_ERROR_FAILURE;

    *retval = mType;
    return NS_OK;
}

/*
 * Utils yay!
 */

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GluPerspective(float fovy, float apsect, float znear, float zfar)
{
    MakeContextCurrent();
    gluPerspective(fovy, apsect, znear, zfar);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLES11::GluLookAt(float eyex, float eyey, float eyez,
                                          float ctrx, float ctry, float ctrz,
                                          float upx, float upy, float upz)
{
    MakeContextCurrent();
    gluLookAt(eyex, eyey, eyez,
              ctrx, ctry, ctrz,
              upx, upy, upz);
    return NS_OK;
}

