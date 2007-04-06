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

#include "prmem.h"
#include "prlog.h"

#include "nsCanvasRenderingContextGL.h"

#include "nsIRenderingContext.h"

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

// GLEW will pull in the GL bits that we want/need
#include "glew.h"

// we're hoping that something is setting us up the remap

#include "cairo.h"
#include "glitz.h"

#ifdef MOZ_CAIRO_GFX
#include "gfxContext.h"
#include "gfxASurface.h"
#endif

#ifdef XP_WIN
#include "cairo-win32.h"
#include "glitz-wgl.h"

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

#ifdef MOZ_X11
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "cairo-xlib.h"
#include "glitz-glx.h"
#endif

nsIXPConnect *gXPConnect = nsnull;
JSRuntime *gScriptRuntime = nsnull;
nsIJSRuntimeService *gJSRuntimeService = nsnull;
PRBool gGlitzInitialized = PR_FALSE;

// CanvasGLBuffer
NS_DECL_CLASSINFO(CanvasGLBuffer)
NS_IMPL_ADDREF(CanvasGLBuffer)
NS_IMPL_RELEASE(CanvasGLBuffer)

NS_IMPL_CI_INTERFACE_GETTER1(CanvasGLBuffer, nsICanvasRenderingContextGLBuffer)

NS_INTERFACE_MAP_BEGIN(CanvasGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
  NS_INTERFACE_MAP_ENTRY(nsICanvasGLBuffer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextGLBuffer)
  NS_IMPL_QUERY_CLASSINFO(CanvasGLBuffer)
NS_INTERFACE_MAP_END

// CanvasGLTexture
NS_DECL_CLASSINFO(CanvasGLTexture)
NS_IMPL_ADDREF(CanvasGLTexture)
NS_IMPL_RELEASE(CanvasGLTexture)

NS_IMPL_CI_INTERFACE_GETTER1(CanvasGLTexture, nsICanvasRenderingContextGLTexture)

NS_INTERFACE_MAP_BEGIN(CanvasGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsICanvasGLTexture)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextGLTexture)
  NS_IMPL_QUERY_CLASSINFO(CanvasGLTexture)
NS_INTERFACE_MAP_END

/**
 ** CanvasGLBuffer
 **/

static int bufferCount = 0;

CanvasGLBuffer::CanvasGLBuffer(nsCanvasRenderingContextGLPrivate *owner)
    : mDisposed(PR_TRUE),
      mLength(0), mSize(0), mType(0), mUsage(GL_STATIC_DRAW),
      mBufferID(0)
{
    owner->GetWeakReference(getter_AddRefs(mOwnerContext));

    bufferCount++;
    fprintf (stderr, "VVVV Created; Buffers now: %d\n", bufferCount); fflush(stderr);
}

CanvasGLBuffer::~CanvasGLBuffer()
{
    Dispose();

    --bufferCount;
    fprintf (stderr, "VVVV Released; Buffers now: %d\n", bufferCount); fflush(stderr);
}

/* nsISecurityCheckedComponent bits */

static char* cloneAllAccess()
{
    static const char allAccess[] = "allAccess";
    return (char*)nsMemory::Clone(allAccess, sizeof(allAccess));
}

NS_IMETHODIMP
CanvasGLBuffer::CanCreateWrapper(const nsIID* iid, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::CanCallMethod(const nsIID *iid, const PRUnichar *methodName, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::CanGetProperty(const nsIID *iid, const PRUnichar *propertyName, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::CanSetProperty(const nsIID *iid, const PRUnichar *propertyName, char **_retval) {
    *_retval = cloneAllAccess();
    return NS_OK;
}

nsresult
CanvasGLBuffer::Init(PRUint32 usage,
                     PRUint32 size,
                     PRUint32 type,
                     JSContext *ctx,
                     JSObject *arrayObj,
                     jsuint arrayLen)
{
    nsresult rv;

    fprintf (stderr, "VVVV CanvasGLBuffer::Init\n");

    if (!mDisposed)
        Dispose();

    if (usage != GL_STATIC_DRAW &&
        usage != GL_DYNAMIC_DRAW)
        return NS_ERROR_INVALID_ARG;

    rv = JSArrayToSimpleBuffer(mSimpleBuffer, type, size, ctx, arrayObj, arrayLen);
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
CanvasGLBuffer::Dispose()
{
    if (mDisposed)
        return NS_OK;

    if (mBufferID) {
        nsCOMPtr<nsICanvasRenderingContextInternal> ctx = do_QueryReferent(mOwnerContext);
        if (ctx) {
            nsCanvasRenderingContextGLPrivate *priv = (nsCanvasRenderingContextGLPrivate*) ctx.get();
            mGlewContextPtr = priv->glewGetContext();
            priv->MakeContextCurrent();

            glDeleteBuffers(1, &mBufferID);
            mBufferID = 0;
        }
    }

    mSimpleBuffer.Release();

    mDisposed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::GetOwnerContext(nsICanvasRenderingContextGL **retval)
{
    nsCOMPtr<nsICanvasRenderingContextInternal> ctx = do_QueryReferent(mOwnerContext);
    if (ctx) {
        nsCanvasRenderingContextGLPrivate *priv = (nsCanvasRenderingContextGLPrivate*) ctx.get();
        *retval = priv->GetSelf();
    } else {
        *retval = nsnull;
    }

    NS_IF_ADDREF(*retval);
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::GetDisposed(PRBool *retval)
{
    *retval = mDisposed;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::GetUsage(PRUint32 *usage)
{
    if (mDisposed)
        return NS_ERROR_FAILURE;

    *usage = mUsage;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::GetLength(PRUint32 *retval)
{
    if (mDisposed)
        return NS_ERROR_FAILURE;

    *retval = mLength;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLBuffer::GetType(PRUint32 *retval)
{
    if (mDisposed)
        return NS_ERROR_FAILURE;

    *retval = mType;
    return NS_OK;
}

/**
 ** CanvasGLTexture
 **/

CanvasGLTexture::CanvasGLTexture(nsCanvasRenderingContextGLPrivate *owner)
    : mDisposed(PR_FALSE),
      mGlitzTextureSurface(nsnull), mGlitzTextureObject(nsnull),
      mWidth(0), mHeight(0)
{
    owner->GetWeakReference(getter_AddRefs(mOwnerContext));
}

CanvasGLTexture::~CanvasGLTexture()
{
    Dispose();
}

nsresult
CanvasGLTexture::Init()
{
    return NS_OK;
}

nsresult
CanvasGLTexture::Dispose()
{
    if (mDisposed)
        return NS_OK;

    mDisposed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLTexture::GetDisposed(PRBool *retval)
{
    *retval = mDisposed;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLTexture::GetOwnerContext(nsICanvasRenderingContextGL **retval)
{
    nsCOMPtr<nsICanvasRenderingContextInternal> ctx = do_QueryReferent(mOwnerContext);
    if (ctx) {
        nsCanvasRenderingContextGLPrivate *priv = (nsCanvasRenderingContextGLPrivate*) ctx.get();
        *retval = priv->GetSelf();
    } else {
        *retval = nsnull;
    }

    NS_IF_ADDREF(*retval);
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLTexture::GetTarget(PRUint32 *aResult)
{
    *aResult = glitz_texture_object_get_target (mGlitzTextureObject);
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLTexture::GetWidth(PRUint32 *aWidth)
{
    *aWidth = mWidth;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLTexture::GetHeight(PRUint32 *aHeight)
{
    *aHeight = mHeight;
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLTexture::SetFilter(PRUint32 filterType, PRUint32 filterMode)
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
CanvasGLTexture::SetWrap(PRUint32 wrapType, PRUint32 wrapMode)
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

nsresult
JSArrayToSimpleBuffer (SimpleBuffer& sbuffer,
                       PRUint32 typeParam,
                       PRUint32 sizeParam,
                       JSContext *ctx,
                       JSObject *arrayObj,
                       jsuint arrayLen)
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

void
nsCanvasRenderingContextGLPrivate::MakeContextCurrent()
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
nsCanvasRenderingContextGLPrivate::LostCurrentContext(void *closure)
{
    nsCanvasRenderingContextGLPrivate* self = (nsCanvasRenderingContextGLPrivate*) closure;
    fprintf (stderr, "[this:%p] Lost context\n", closure);
    fflush (stderr);

    self->mCurrent = PR_FALSE;
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::SetCanvasElement(nsICanvasElement* aParentCanvas)
{
    if (!SafeToCreateCanvas3DContext())
        return NS_ERROR_FAILURE;

    if (!ValidateGL()) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Couldn't validate OpenGL implementation; is everything needed present?"));
        return NS_ERROR_FAILURE;
    }

    mCanvasElement = aParentCanvas;
    fprintf (stderr, "VVVV SetCanvasElement: %p\n", mCanvasElement);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::SetDimensions(PRInt32 width, PRInt32 height)
{
    if (mWidth == width && mHeight == height)
        return NS_OK;

    Destroy();

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

    if (!gdformat) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Unable to find pbuffer format (maybe pbuffers are not available?"));
        return NS_ERROR_INVALID_ARG;
    }

    mGlitzDrawable =
        glitz_wgl_create_pbuffer_drawable(gdformat,
                                          width,
                                          height);
#endif

    if (!gdformat || !mGlitzDrawable) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Failed to create pbuffer drawable."));
        return NS_ERROR_FAILURE;
    }

    glitz_format_t *gformat =
        glitz_find_standard_format(mGlitzDrawable, GLITZ_STANDARD_ARGB32);
    if (!gformat) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Couldn't find ARGB32 format (this should never happen!)"));
        return NS_ERROR_INVALID_ARG;
    }

    mGlitzSurface =
        glitz_surface_create(mGlitzDrawable,
                             gformat,
                             width,
                             height,
                             0,
                             NULL);
    if (!mGlitzSurface) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Failed to create glitz surface"));
        return NS_ERROR_INVALID_ARG;
    }

    glitz_surface_attach(mGlitzSurface, mGlitzDrawable, GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);

    mGlitzContext = glitz_context_create (mGlitzDrawable, gdformat);

    MakeContextCurrent();

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        // er, something very bad happened
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLEW init failed"));
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
nsCanvasRenderingContextGLPrivate::Render(nsIRenderingContext *rc)
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
nsCanvasRenderingContextGLPrivate::RenderToSurface(cairo_surface_t *surf)
{
    return NS_OK;
}

nsIFrame*
nsCanvasRenderingContextGLPrivate::GetCanvasLayoutFrame()
{
    if (!mCanvasElement)
        return nsnull;

    nsIFrame *fr = nsnull;
    mCanvasElement->GetPrimaryCanvasFrame(&fr);
    return fr;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::GetInputStream(const nsACString& aMimeType,
                                                  const nsAString& aEncoderOptions,
                                                  nsIInputStream **aStream)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsCanvasRenderingContextGLPrivate::Destroy()
{
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


/**
 ** Helpers that really should be in some sort of cross-context shared library
 **/

nsresult
nsCanvasRenderingContextGLPrivate::CairoSurfaceFromElement(nsIDOMElement *imgElt,
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
nsCanvasRenderingContextGLPrivate::DoDrawImageSecurityCheck(nsIURI* aURI, PRBool forceWriteOnly)
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


nsCanvasRenderingContextGLPrivate::nsCanvasRenderingContextGLPrivate()
    : mGlitzContext(nsnull),
      mGlitzDrawable(nsnull),
      mContextToken(nsnull),
      mCurrent(PR_FALSE),
      mWidth(0), mHeight(0), mStride(0), mImageBuffer(nsnull),
      mCanvasElement(nsnull), mGlitzSurface(nsnull), mGlitzImageBuffer(nsnull),
      mCairoImageSurface(nsnull)
{
    if (!gGlitzInitialized) {
#ifdef MOZ_X11
        glitz_glx_init("libGL.so.1");
#endif
#ifdef XP_WIN
        glitz_wgl_init(NULL);
#endif
        gGlitzInitialized = PR_TRUE;
    }

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

nsCanvasRenderingContextGLPrivate::~nsCanvasRenderingContextGLPrivate()
{
    Destroy();

    if (gXPConnect && gXPConnect->Release() == 0)
        gXPConnect = nsnull;
    if (gJSRuntimeService && gJSRuntimeService->Release() == 0) {
        gJSRuntimeService = nsnull;
        gScriptRuntime = nsnull;
    }
}

nsresult
nsCanvasRenderingContextGLPrivate::DoSwapBuffers()
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
        nsIPresShell *shell = frame->PresContext()->GetPresShell();
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

PRBool
nsCanvasRenderingContextGLPrivate::SafeToCreateCanvas3DContext()
{
    nsresult rv;

    // first see if we're a chrome context
    PRBool is_caller_chrome = PR_FALSE;
    nsCOMPtr<nsIScriptSecurityManager> ssm =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = ssm->SubjectPrincipalIsSystem(&is_caller_chrome);
    if (NS_SUCCEEDED(rv) && is_caller_chrome)
        return TRUE;

    // not chrome? check pref.

    // check whether it's safe to create a 3d context via a pref
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // our pref is 'extensions.canvas3d.enabledForWebContent'
    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("extensions.canvas3d.", getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRBool enabled;
    rv = prefBranch->GetBoolPref("enabledForWebContent", &enabled);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (enabled)
        return PR_TRUE;

    LogMessage("Canvas 3D: Web content tried to create 3D Canvas Context, but pref extensions.canvas3d.enabledForWebContent is not set!");

    return PR_FALSE;
}
