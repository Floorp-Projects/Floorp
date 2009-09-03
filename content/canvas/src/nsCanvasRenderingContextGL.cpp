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

#include "nsICanvasRenderingContextGL.h"

#include "nsIRenderingContext.h"

#include "nsICanvasRenderingContextInternal.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsIView.h"
#include "nsIViewManager.h"

#include "nsIDocument.h"

#include "nsTransform2D.h"

#include "nsIScriptSecurityManager.h"
#include "nsISecurityCheckedComponent.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsICanvasElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsDOMError.h"
#include "nsIJSRuntimeService.h"

#include "imgIEncoder.h"

#include "nsIPrefService.h"

#include "nsIClassInfoImpl.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "nsDOMError.h"

#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsarray.h"

#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDataContainerEvent.h"

#include "nsLayoutUtils.h"

#ifdef ARGH_NEED_SEPARATE_SERVICE
#include "nsIContentURIGrouper.h"
#include "nsIContentPrefService.h"
#endif

// we're hoping that something is setting us up the remap

#include "gfxContext.h"
#include "gfxASurface.h"

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#endif

#ifdef XP_MACOSX
#include "gfxQuartzImageSurface.h"
#endif

#ifdef MOZ_X11
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "cairo-xlib.h"
#endif

// can't do this due to linkage
#undef MOZ_MEDIA

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif

nsIXPConnect *gXPConnect = nsnull;
JSRuntime *gScriptRuntime = nsnull;
nsIJSRuntimeService *gJSRuntimeService = nsnull;

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
      mBufferID(0), mMaxUShort(0), mMaxUShortComputed(false)
{
    owner->GetWeakReference(getter_AddRefs(mOwnerContext));

    gl = owner->gl;

    bufferCount++;
    //fprintf (stderr, "VVVV Created; Buffers now: %d\n", bufferCount); fflush(stderr);
}

CanvasGLBuffer::~CanvasGLBuffer()
{
    Dispose();

    --bufferCount;
    //fprintf (stderr, "VVVV Released; Buffers now: %d\n", bufferCount); fflush(stderr);
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

    //fprintf (stderr, "VVVV CanvasGLBuffer::Init\n");

    if (!mDisposed)
        Dispose();

    if (usage != GL_STATIC_DRAW &&
        usage != GL_STREAM_DRAW &&
        usage != GL_DYNAMIC_DRAW)
        return NS_ERROR_INVALID_ARG;

    rv = mSimpleBuffer.InitFromJSArray(type, size, ctx, arrayObj, arrayLen);
    if (NS_FAILED(rv))
        return rv;

    mUsage = usage;
    mSize = size;
    mType = type;
    mLength = arrayLen;

    mMaxUShortComputed = false;
    
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
            priv->MakeContextCurrent();

            gl->fDeleteBuffers(1, &mBufferID);
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
      //mGlitzTextureSurface(nsnull), mGlitzTextureObject(nsnull),
      mWidth(0), mHeight(0)
{
    owner->GetWeakReference(getter_AddRefs(mOwnerContext));

    gl = owner->gl;
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
    //*aResult = glitz_texture_object_get_target (mGlitzTextureObject);
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

    //glitz_texture_object_set_filter (mGlitzTextureObject, (glitz_texture_filter_type_t)filterType, (glitz_texture_filter_t)filterMode);
    return NS_OK;
}

NS_IMETHODIMP
CanvasGLTexture::SetWrap(PRUint32 wrapType, PRUint32 wrapMode)
{
    if (wrapType != GL_TEXTURE_WRAP_S &&
        wrapType != GL_TEXTURE_WRAP_T)
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (wrapMode != GL_CLAMP_TO_EDGE &&
        wrapMode != GL_REPEAT &&
        wrapMode != GL_MIRRORED_REPEAT)
        return NS_ERROR_DOM_SYNTAX_ERR;

    //glitz_texture_object_set_wrap (mGlitzTextureObject, (glitz_texture_wrap_type_t)wrapType, (glitz_texture_wrap_t)wrapMode);
    return NS_OK;
}

/*
 * these got removed accidentally in bug 495499.  We make them just return false here and
 * take the slow path until we get that fixed up.
 */

#define FOO(_x,_y) JSBool _x (JSContext *cx, JSObject *obj, jsuint offset, jsuint count, _y *dest) { return 0; }

FOO(js_ArrayToJSUint8Buffer, JSUint8)
FOO(js_ArrayToJSUint16Buffer, JSUint16)
FOO(js_ArrayToJSUint32Buffer, JSUint32)
FOO(js_ArrayToJSInt8Buffer, JSInt8)
FOO(js_ArrayToJSInt16Buffer, JSInt16)
FOO(js_ArrayToJSInt32Buffer, JSInt32)
FOO(js_ArrayToJSDoubleBuffer, jsdouble)


PRBool
SimpleBuffer::InitFromJSArray(PRUint32 typeParam,
                              PRUint32 sizeParam,
                              JSContext *ctx,
                              JSObject *arrayObj,
                              jsuint arrayLen)
{
    if (typeParam == GL_SHORT) {
        Prepare(typeParam, sizeParam, arrayLen);
        short *ptr = (short*) data;

        if (!js_ArrayToJSInt16Buffer(ctx, arrayObj, 0, arrayLen, ptr)) {
            for (PRUint32 i = 0; i < arrayLen; i++) {
                jsval jv;
                int32 iv;
                ::JS_GetElement(ctx, arrayObj, i, &jv);
                ::JS_ValueToECMAInt32(ctx, jv, &iv);
                *ptr++ = (short) iv;
            }
        }
    } else if (typeParam == GL_FLOAT) {
        Prepare(typeParam, sizeParam, arrayLen);
        float *ptr = (float*) data;
        double *tmpd = new double[arrayLen];
        if (js_ArrayToJSDoubleBuffer(ctx, arrayObj, 0, arrayLen, tmpd)) {
            for (PRUint32 i = 0; i < arrayLen; i++)
                ptr[i] = (float) tmpd[i];
        } else {
            for (PRUint32 i = 0; i < arrayLen; i++) {
                jsval jv;
                jsdouble dv;
                ::JS_GetElement(ctx, arrayObj, i, &jv);
                ::JS_ValueToNumber(ctx, jv, &dv);
                *ptr++ = (float) dv;
            }
        }
        delete [] tmpd;
    } else if (typeParam == GL_UNSIGNED_BYTE) {
        Prepare(typeParam, sizeParam, arrayLen);
        unsigned char *ptr = (unsigned char*) data;
        if (!js_ArrayToJSUint8Buffer(ctx, arrayObj, 0, arrayLen, ptr)) {
            for (PRUint32 i = 0; i < arrayLen; i++) {
                jsval jv;
                uint32 iv;
                ::JS_GetElement(ctx, arrayObj, i, &jv);
                ::JS_ValueToECMAUint32(ctx, jv, &iv);
                *ptr++ = (unsigned char) iv;
            }
        }
    } else if (typeParam == GL_UNSIGNED_SHORT) {
        Prepare(typeParam, sizeParam, arrayLen);
        PRUint16 *ptr = (PRUint16*) data;
        if (!js_ArrayToJSUint16Buffer(ctx, arrayObj, 0, arrayLen, ptr)) {
            for (PRUint32 i = 0; i < arrayLen; i++) {
                jsval jv;
                uint32 iv;
                ::JS_GetElement(ctx, arrayObj, i, &jv);
                ::JS_ValueToECMAUint32(ctx, jv, &iv);
                *ptr++ = (unsigned short) iv;
            }
        }
    } else if (typeParam == GL_UNSIGNED_INT) {
        Prepare(typeParam, sizeParam, arrayLen);
        PRUint32 *ptr = (PRUint32*) data;
        if (!js_ArrayToJSUint32Buffer(ctx, arrayObj, 0, arrayLen, ptr)) {
            for (PRUint32 i = 0; i < arrayLen; i++) {
                jsval jv;
                uint32 iv;
                ::JS_GetElement(ctx, arrayObj, i, &jv);
                ::JS_ValueToECMAUint32(ctx, jv, &iv);
                *ptr++ = iv;
            }
        }
    } else if (typeParam == GL_INT) {
        Prepare(typeParam, sizeParam, arrayLen);
        PRInt32 *ptr = (PRInt32*) data;
        if (!js_ArrayToJSInt32Buffer(ctx, arrayObj, 0, arrayLen, ptr)) {
            for (PRUint32 i = 0; i < arrayLen; i++) {
                jsval jv;
                int32 iv;
                ::JS_GetElement(ctx, arrayObj, i, &jv);
                ::JS_ValueToECMAInt32(ctx, jv, &iv);
                *ptr++ = iv;
            }
        }
    } else {
        return PR_FALSE;
    }

    return PR_TRUE;
}

void
nsCanvasRenderingContextGLPrivate::MakeContextCurrent()
{
    mGLPbuffer->MakeContextCurrent();
}

void
nsCanvasRenderingContextGLPrivate::LostCurrentContext(void *closure)
{
    //nsCanvasRenderingContextGLPrivate* self = (nsCanvasRenderingContextGLPrivate*) closure;
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::SetCanvasElement(nsICanvasElement* aParentCanvas)
{
    nsresult rv;

    if (aParentCanvas == nsnull) {
        // we get this on shutdown; we should do some more cleanup here,
        // but instead we just let our destructor do it.
        return NS_OK;
    }

    if (!SafeToCreateCanvas3DContext(aParentCanvas))
        return NS_ERROR_FAILURE;

    // Let's find our prefs
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mPrefWireframe = PR_FALSE;
    PRBool forceSoftware = PR_FALSE;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("extensions.canvas3d.", getter_AddRefs(prefBranch));
    if (NS_SUCCEEDED(rv)) {
        PRBool val;
        rv = prefBranch->GetBoolPref("wireframe", &val);
        if (NS_SUCCEEDED(rv))
            mPrefWireframe = val;

        rv = prefBranch->GetBoolPref("software_render", &val);
        if (NS_SUCCEEDED(rv))
            forceSoftware = val;
    }

    fprintf (stderr, "Wireframe: %d\n", mPrefWireframe);


    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: creating PBuffer..."));

    if (!forceSoftware) {
#if defined(WINCE)
        mGLPbuffer = new nsGLPbufferEGL();
#elif defined(XP_WIN)
        mGLPbuffer = new nsGLPbufferWGL();
#elif defined(XP_UNIX) && defined(MOZ_X11)
        mGLPbuffer = new nsGLPbufferGLX();
#elif defined(XP_MACOSX)
        mGLPbuffer = new nsGLPbufferCGL();
#else
        mGLPbuffer = nsnull;
#endif

        if (mGLPbuffer && !mGLPbuffer->Init(this))
            mGLPbuffer = nsnull;
    }

    if (!mGLPbuffer) {
        mGLPbuffer = new nsGLPbufferOSMESA();
        if (!mGLPbuffer->Init(this))
            mGLPbuffer = nsnull;
    }

    if (!mGLPbuffer)
        return NS_ERROR_FAILURE;

    gl = mGLPbuffer->GL();

    if (!ValidateGL()) {
        // XXX over here we need to destroy mGLPbuffer and create a mesa buffer

        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Couldn't validate OpenGL implementation; is everything needed present?"));
        return NS_ERROR_FAILURE;
    }

    mCanvasElement = aParentCanvas;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::SetDimensions(PRInt32 width, PRInt32 height)
{
    if (mWidth == width && mHeight == height)
        return NS_OK;

    if (!mGLPbuffer->Resize(width, height)) {
        LogMessage(NS_LITERAL_CSTRING("mGLPbuffer->Resize failed"));
        return NS_ERROR_FAILURE;
    }

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: ready"));

    mWidth = width;
    mHeight = height;

    // Make sure that we clear this out, otherwise
    // we'll end up displaying random memory
#if 0
    int err = glGetError();
    if (err) {
        printf ("error before MakeContextCurrent! 0x%04x\n", err);
    }
#endif

    MakeContextCurrent();
    gl->fViewport(0, 0, mWidth, mHeight);
    gl->fClearColor(0, 0, 0, 0);
    gl->fClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

#if 0
    err = glGetError();
    if (err) {
        printf ("error after MakeContextCurrent! 0x%04x\n", err);
    }
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f)
{
    nsresult rv = NS_OK;

    if (!mGLPbuffer)
        return NS_OK;

    // use GL Drawing if we can get a target GL context; otherwise
    // go through the fallback path.
#ifdef HAVE_GL_DRAWING
    if (mCanvasElement->GLWidgetBeginDrawing()) {
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        int bwidth = mGLPbuffer->Width();
        int bheight = mGLPbuffer->Height();

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex);

        CGLError err =
            CGLTexImagePBuffer(CGLGetCurrentContext(),
                               ((nsGLPbufferCGL*)mGLPbuffer)->GetCGLPbuffer(),
                               GL_BACK);
        if (err) {
            fprintf (stderr, "CGLTexImagePBuffer failed: %d\n", err);
            glDeleteTextures(1, &tex);
            return NS_OK;
        }

        glEnable(GL_TEXTURE_RECTANGLE_EXT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        //glFrustum(-halfWidth, halfWidth, halfHeight, -halfHeight, 1.0, 100000.0);
        glOrtho(0, bwidth, bheight, 0, -0.5, 10.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glBegin(GL_QUADS);

        /* Note that the texture needs a y-flip */
        glTexCoord2f(0.0, bheight);
        glVertex3f(0.0, 0.0, 0.0);

        glTexCoord2f(bwidth, bheight);
        glVertex3f(bwidth, 0.0, 0.0);

        glTexCoord2f(bwidth, 0);
        glVertex3f(bwidth, bheight, 0.0);

        glTexCoord2f(0.0, 0);
        glVertex3f(0.0, bheight, 0.0);

        glEnd();

        glDisable(GL_TEXTURE_RECTANGLE_EXT);
        glDeleteTextures(1, &tex);

        mCanvasElement->GLWidgetSwapBuffers();
        mCanvasElement->GLWidgetEndDrawing();
    } else
#endif
    {
        nsRefPtr<gfxASurface> surf = mGLPbuffer->ThebesSurface();
        if (!surf)
            return NS_OK;

        nsRefPtr<gfxPattern> pat = CanvasGLThebes::CreatePattern(surf);
        gfxMatrix m;
        m.Translate(gfxPoint(0.0, mGLPbuffer->Height()));
        m.Scale(1.0, -1.0);
        pat->SetMatrix(m);

        // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
        // pixel alignment for this stuff!
        ctx->NewPath();
        ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
        ctx->Fill();
    }
    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::GetInputStream(const char* aMimeType,
                                                  const PRUnichar* aEncoderOptions,
                                                  nsIInputStream **aStream)
{
    // XXX disabled for now due to the win32 nsRefPtr situation -- we need
    // to manage allocations and deletions very carefully, and can't allocate
    // an object in our dll and have xul.dll call delete on it (which
    // Release() will do).
    return NS_ERROR_FAILURE;

#if 0
    if (!mGLPbuffer ||
        !mGLPbuffer->ThebesSurface())
        return NS_ERROR_FAILURE;

    nsresult rv;
    const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
    nsAutoArrayPtr<char> conid(new (std::nothrow) char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

    if (!conid)
        return NS_ERROR_OUT_OF_MEMORY;

    strcpy(conid, encoderPrefix);
    strcat(conid, aMimeType);

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
    if (!encoder)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<PRUint8> imageBuffer(new (std::nothrow) PRUint8[mWidth * mHeight * 4]);
    if (!imageBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(imageBuffer.get(),
                                                            gfxIntSize(mWidth, mHeight),
                                                            mWidth * 4,
                                                            gfxASurface::ImageFormatARGB32);

    if (!imgsurf || imgsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> ctx = new gfxContext(imgsurf);

    if (!ctx || ctx->HasError())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxASurface> surf = mGLPbuffer->ThebesSurface();
    nsRefPtr<gfxPattern> pat = CanvasGLThebes::CreatePattern(surf);
    gfxMatrix m;
    m.Translate(gfxPoint(0.0, mGLPbuffer->Height()));
    m.Scale(1.0, -1.0);
    pat->SetMatrix(m);

    // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
    // pixel alignment for this stuff!
    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->Fill();

    rv = encoder->InitFromData(imageBuffer.get(),
                               mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                               imgIEncoder::INPUT_FORMAT_HOSTARGB,
                               nsDependentString(aEncoderOptions));
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(encoder, aStream);
#endif
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::GetThebesSurface(gfxASurface **surface)
{
    if (!mGLPbuffer) {
        *surface = nsnull;
        return NS_ERROR_NOT_AVAILABLE;
    }

    *surface = mGLPbuffer->ThebesSurface();
    NS_IF_ADDREF(*surface);
    return NS_OK;
}

nsCanvasRenderingContextGLPrivate::nsCanvasRenderingContextGLPrivate()
    : mGLPbuffer(nsnull), mWidth(0), mHeight(0), mCanvasElement(nsnull)
{
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
}

nsCanvasRenderingContextGLPrivate::~nsCanvasRenderingContextGLPrivate()
{
    delete mGLPbuffer;
    mGLPbuffer = nsnull;
    
    // get rid of the context
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
    mGLPbuffer->SwapBuffers();

    // then invalidate the region and do a redraw
    if (!mCanvasElement)
        return NS_OK;

    mCanvasElement->InvalidateFrame();
    return NS_OK;
}

PRBool
nsCanvasRenderingContextGLPrivate::SafeToCreateCanvas3DContext(nsICanvasElement *canvasElement)
{
    nsresult rv;

    // first see if we're a chrome context
    PRBool is_caller_chrome = PR_FALSE;
    nsCOMPtr<nsIScriptSecurityManager> ssm =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = ssm->SubjectPrincipalIsSystem(&is_caller_chrome);
    if (NS_SUCCEEDED(rv) && is_caller_chrome)
        return PR_TRUE;

    // not chrome? check pref.

    // first check our global pref
    nsCOMPtr<nsIPrefBranch> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRBool allSites = PR_FALSE;
    rv = prefService->GetBoolPref("extensions.canvas3d.enabled_for_all_sites", &allSites);
    if (NS_SUCCEEDED(rv) && allSites) {
        // the all-sites pref was set, we're good to go
        return PR_TRUE;
    }

    // otherwise we'll check content prefs

#ifdef ARGH_NEED_SEPARATE_SERVICE
    // the content pref service IID changed after 3.1b3, so this will fail for nightly builds.
    nsCOMPtr<nsIContentPrefService> cpsvc = do_GetService("@mozilla.org/content-pref/service;1", &rv);
    if (NS_FAILED(rv)) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Failed to get Content Pref service.  If you are running Firefox 3.1b3, as a temporary fix until 3.5b4 is released, open the Canvas 3D Addon preferences and check the \"Enabled for all sites\" checkbox."));
        return PR_FALSE;
    }

    // grab our content URI
    nsCOMPtr<nsIURI> contentURI;

    nsCOMPtr<nsIPrincipal> principal;
    rv = ssm->GetSubjectPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (!principal) {
        // seriously? no script executing, but not the system principal?
        return PR_FALSE;
    }
    rv = principal->GetURI(getter_AddRefs(contentURI));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // our pref is 'canvas3d.enabled'
    nsCOMPtr<nsIVariant> val;
    rv = cpsvc->GetPref(contentURI, NS_LITERAL_STRING("canvas3d.enabled"), getter_AddRefs(val));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRInt32 iv;
    rv = val->GetAsInt32(&iv);
    if (NS_SUCCEEDED(rv)) {
        // 1 means "yes, allowed"
        if (iv == 1)
            return PR_TRUE;

        // -1 means "no, don't ask me again"
        if (iv == -1)
            return PR_FALSE;

        // otherwise, we'll throw an event and maybe ask the user
    }

    // grab the document that we can use to create the event
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(canvasElement);
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = node->GetOwnerDocument(getter_AddRefs(domDoc));

    /*
    // figure out where to throw the event.  we just go for the outermost
    // document.  ideally, I want to throw the event to the <browser> if one exists,
    // otherwise the topmost document, but that's more work than I want to deal with.
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    while (doc->GetParentDocument())
        doc = doc->GetParentDocument();
    */

    // set up the event
    nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(domDoc);
    NS_ENSURE_TRUE(docEvent, PR_FALSE);

    nsCOMPtr<nsIDOMEvent> eventBase;
    rv = docEvent->CreateEvent(NS_LITERAL_STRING("DataContainerEvent"), getter_AddRefs(eventBase));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = eventBase->InitEvent(NS_LITERAL_STRING("Canvas3DContextRequest"), PR_TRUE, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsIDOMDataContainerEvent> event = do_QueryInterface(eventBase);
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(eventBase);
    NS_ENSURE_TRUE(event && privateEvent, PR_FALSE);

    // mark it as trusted, so that it'll bubble upwards into chrome
    privateEvent->SetTrusted(PR_TRUE);

    // set some extra data on the event
    nsCOMPtr<nsIContentURIGrouper> grouper = do_GetService("@mozilla.org/content-pref/hostname-grouper;1", &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsAutoString group;
    rv = grouper->Group(contentURI, group);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsIWritableVariant> groupVariant = do_CreateInstance(NS_VARIANT_CONTRACTID);
    nsCOMPtr<nsIWritableVariant> uriVariant = do_CreateInstance(NS_VARIANT_CONTRACTID);

    groupVariant->SetAsAString(group);
    uriVariant->SetAsISupports(contentURI);

    rv = event->SetData(NS_LITERAL_STRING("group"), groupVariant);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = event->SetData(NS_LITERAL_STRING("uri"), uriVariant);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // our target...
    nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(canvasElement);

    // and go.
    PRBool defaultActionEnabled;
    targ->DispatchEvent(event, &defaultActionEnabled);
#endif

    return PR_FALSE;
}

gfxImageSurface *
CanvasGLThebes::CreateImageSurface (const gfxIntSize &isize,
                                    gfxASurface::gfxImageFormat fmt)
{
    /*void *p = NS_Alloc(sizeof(gfxImageSurface));*/
    return new /*(p)*/ gfxImageSurface (isize, fmt);
}

gfxContext *
CanvasGLThebes::CreateContext (gfxASurface *surf)
{
    void *p = NS_Alloc(sizeof(gfxContext));
    return new (p) gfxContext (surf);
}

gfxPattern *
CanvasGLThebes::CreatePattern (gfxASurface *surf)
{
    /*void *p = NS_Alloc(sizeof(gfxPattern));*/
    return new /*(p)*/ gfxPattern(surf);
}

/*
 * We need this here, because nsAString has a different type name based on whether it's
 * used internally or externally.  BeginPrinting isn't ever called, but gfxImageSurface
 * wants to inherit the default definition, and it can't find it.  So instead, we just
 * stick a stub here to shut the compiler up, because we never call this method.
 */

#if 0
#ifdef XP_WIN
nsresult
gfxWindowsSurface::BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
nsresult
gfxASurface::BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
#endif
