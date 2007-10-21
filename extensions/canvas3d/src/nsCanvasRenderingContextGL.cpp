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
#include "gfxIImageFrame.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsICanvasElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIImage.h"
#include "nsDOMError.h"
#include "nsIJSRuntimeService.h"

#include "nsIPrefService.h"

#include "nsIClassInfoImpl.h"

#include "nsServiceManagerUtils.h"

#include "nsDOMError.h"

#include "nsIXPConnect.h"
#include "jsapi.h"

// GLEW will pull in the GL bits that we want/need
#include "glew.h"

// we're hoping that something is setting us up the remap

#include "gfxContext.h"
#include "gfxASurface.h"

#ifdef MOZ_X11
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "cairo-xlib.h"
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
      mBufferID(0)
{
    owner->GetWeakReference(getter_AddRefs(mOwnerContext));

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
      //mGlitzTextureSurface(nsnull), mGlitzTextureObject(nsnull),
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
    } else if (typeParam == GL_UNSIGNED_SHORT) {
        PRUint16 *ptr = (PRUint16*) sbuffer.data;
        for (PRUint32 i = 0; i < arrayLen; i++) {
            jsval jv;
            uint32 iv;
            ::JS_GetElement(ctx, arrayObj, i, &jv);
            ::JS_ValueToECMAUint32(ctx, jv, &iv);
            *ptr++ = (unsigned short) iv;
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
    mGLPbuffer->MakeContextCurrent();
}

void
nsCanvasRenderingContextGLPrivate::LostCurrentContext(void *closure)
{
    nsCanvasRenderingContextGLPrivate* self = (nsCanvasRenderingContextGLPrivate*) closure;
    //fprintf (stderr, "[this:%p] Lost context\n", closure);
    fflush (stderr);
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

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: hello?"));

    if (!SafeToCreateCanvas3DContext())
        return NS_ERROR_FAILURE;

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: is anyone there?"));

    mGLPbuffer = new nsGLPbuffer();

    if (!mGLPbuffer->Init(this))
        return NS_ERROR_FAILURE;

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: it's dark in here."));

    // Let's find our prefs
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mPrefWireframe = PR_FALSE;

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("extensions.canvas3d.", getter_AddRefs(prefBranch));
    if (NS_SUCCEEDED(rv)) {
        PRBool val;
        rv = prefBranch->GetBoolPref("wireframe", &val);
        if (NS_SUCCEEDED(rv))
            mPrefWireframe = val;
    }

    fprintf (stderr, "Wireframe: %d\n", mPrefWireframe);

    if (!ValidateGL()) {
        // XXX over here we need to destroy mGLPbuffer and create a mesa buffer

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
    fprintf (stderr, "VVVV CanvasGLBuffer::SetDimensions %d %d\n", width, height);

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: look! there's a light!"));

    if (mWidth == width && mHeight == height)
        return NS_OK;

    if (!mGLPbuffer->Resize(width, height)) {
        LogMessage(NS_LITERAL_CSTRING("mGLPbuffer->Resize failed"));
        return NS_ERROR_FAILURE;
    }

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: maybe that's the way out."));

    mWidth = width;
    mHeight = height;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::Render(gfxContext *ctx)
{
    nsresult rv = NS_OK;

    if (!mGLPbuffer)
        return NS_OK;

    if (!mGLPbuffer->ThebesSurface())
        return NS_OK;

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
    ctx->Fill();

    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContextGLPrivate::GetInputStream(const char* aMimeType,
                                                  const PRUnichar* aEncoderOptions,
                                                  nsIInputStream **aStream)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 ** Helpers that really should be in some sort of cross-context shared library
 **/

nsresult
nsCanvasRenderingContextGLPrivate::CairoSurfaceFromElement(nsIDOMElement *imgElt,
                                                           gfxASurface **aThebesSurface,
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

        PRUint32 status;
        imgRequest->GetImageStatus(&status);
        if ((status & imgIRequest::STATUS_LOAD_COMPLETE) == 0)
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

            nsRefPtr<gfxImageSurface> surf =
                CanvasGLThebes::CreateImageSurface(gfxIntSize(w, h), gfxASurface::ImageFormatARGB32);
            nsRefPtr<gfxContext> ctx = CanvasGLThebes::CreateContext(surf);
            ctx->SetOperator(gfxContext::OPERATOR_CLEAR);
            ctx->Paint();
            ctx->SetOperator(gfxContext::OPERATOR_OVER);

            rv = canvas->RenderContexts(ctx);
            if (NS_FAILED(rv))
                return rv;

            NS_ADDREF(surf.get());
            *aThebesSurface = surf;
            *imgData = surf->Data();
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

    rv = img->GetSurface(aThebesSurface);
    NS_ENSURE_SUCCESS(rv, rv);

    *imgData = nsnull;

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

    fprintf (stderr, "DoDrawImageSecuritycheck this 6: %p\n", this); fflush(stderr);
    mCanvasElement->SetWriteOnly();
#endif
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

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: where am I?"));
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

    LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Web content tried to create 3D Canvas Context, but pref extensions.canvas3d.enabledForWebContent is not set!"));

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

nsresult
gfxASurface::BeginPrinting(const nsAString& aTitle, const nsAString& aPrintToFileName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
