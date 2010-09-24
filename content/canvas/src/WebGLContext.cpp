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

#include "nsIConsoleService.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsDOMError.h"
#include "nsIGfxInfo.h"

#include "gfxContext.h"
#include "gfxPattern.h"
#include "gfxUtils.h"

#include "CanvasUtils.h"

#include "GLContextProvider.h"

#ifdef MOZ_SVG
#include "nsSVGEffects.h"
#endif

#include "prenv.h"

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::layers;

nsresult NS_NewCanvasRenderingContextWebGL(nsICanvasRenderingContextWebGL** aResult);

nsresult
NS_NewCanvasRenderingContextWebGL(nsICanvasRenderingContextWebGL** aResult)
{
    nsICanvasRenderingContextWebGL* ctx = new WebGLContext();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

WebGLContext::WebGLContext()
    : mCanvasElement(nsnull),
      gl(nsnull)
{
    mWidth = mHeight = 0;
    mGeneration = 0;
    mInvalidated = PR_FALSE;
    mResetLayer = PR_TRUE;
    mVerbose = PR_FALSE;

    mActiveTexture = 0;
    mSynthesizedGLError = LOCAL_GL_NO_ERROR;
    mPixelStoreFlipY = PR_FALSE;
    mPixelStorePremultiplyAlpha = PR_FALSE;

    mShaderValidation = PR_TRUE;

    mMapBuffers.Init();
    mMapTextures.Init();
    mMapPrograms.Init();
    mMapShaders.Init();
    mMapFramebuffers.Init();
    mMapRenderbuffers.Init();

    mBlackTexturesAreInitialized = PR_FALSE;
    mFakeBlackStatus = DoNotNeedFakeBlack;

    mFakeVertexAttrib0Array = nsnull;
    mVertexAttrib0Vector[0] = 0;
    mVertexAttrib0Vector[1] = 0;
    mVertexAttrib0Vector[2] = 0;
    mVertexAttrib0Vector[3] = 1;
}

WebGLContext::~WebGLContext()
{
    DestroyResourcesAndContext();
}

static PLDHashOperator
DeleteTextureFunction(const PRUint32& aKey, WebGLTexture *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Texture is still in mMapTextures, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteTextures(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteBufferFunction(const PRUint32& aKey, WebGLBuffer *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Buffer is still in mMapBuffers, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteBuffers(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteFramebufferFunction(const PRUint32& aKey, WebGLFramebuffer *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Framebuffer is still in mMapFramebuffers, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteFramebuffers(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteRenderbufferFunction(const PRUint32& aKey, WebGLRenderbuffer *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Renderbuffer is still in mMapRenderbuffers, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteRenderbuffers(1, &name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteProgramFunction(const PRUint32& aKey, WebGLProgram *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Program is still in mMapPrograms, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteProgram(name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

static PLDHashOperator
DeleteShaderFunction(const PRUint32& aKey, WebGLShader *aValue, void *aData)
{
    gl::GLContext *gl = (gl::GLContext *) aData;
    NS_ASSERTION(!aValue->Deleted(), "Shader is still in mMapShaders, but is deleted?");
    GLuint name = aValue->GLName();
    gl->fDeleteShader(name);
    aValue->Delete();
    return PL_DHASH_NEXT;
}

void
WebGLContext::DestroyResourcesAndContext()
{
    if (!gl)
        return;

    gl->MakeCurrent();

    mMapTextures.EnumerateRead(DeleteTextureFunction, gl);
    mMapTextures.Clear();

    mMapBuffers.EnumerateRead(DeleteBufferFunction, gl);
    mMapBuffers.Clear();

    mMapPrograms.EnumerateRead(DeleteProgramFunction, gl);
    mMapPrograms.Clear();

    mMapShaders.EnumerateRead(DeleteShaderFunction, gl);
    mMapShaders.Clear();

    mMapFramebuffers.EnumerateRead(DeleteFramebufferFunction, gl);
    mMapFramebuffers.Clear();

    mMapRenderbuffers.EnumerateRead(DeleteRenderbufferFunction, gl);
    mMapRenderbuffers.Clear();

    if (mBlackTexturesAreInitialized) {
        gl->fDeleteTextures(1, &mBlackTexture2D);
        gl->fDeleteTextures(1, &mBlackTextureCubeMap);
        mBlackTexturesAreInitialized = PR_FALSE;
    }

    // We just got rid of everything, so the context had better
    // have been going away.
#ifdef DEBUG
    printf_stderr("--- WebGL context destroyed: %p\n", gl.get());
#endif

    gl = nsnull;
}

void
WebGLContext::Invalidate()
{
    if (!mCanvasElement)
        return;

#ifdef MOZ_SVG
    nsSVGEffects::InvalidateDirectRenderingObservers(HTMLCanvasElement());
#endif

    if (mInvalidated)
        return;

    mInvalidated = PR_TRUE;
    HTMLCanvasElement()->InvalidateFrame();
}

/* readonly attribute nsIDOMHTMLCanvasElement canvas; */
NS_IMETHODIMP
WebGLContext::GetCanvas(nsIDOMHTMLCanvasElement **canvas)
{
    NS_IF_ADDREF(*canvas = mCanvasElement);

    return NS_OK;
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
WebGLContext::SetCanvasElement(nsHTMLCanvasElement* aParentCanvas)
{
    if (aParentCanvas && !SafeToCreateCanvas3DContext(aParentCanvas))
        return NS_ERROR_FAILURE;

    mCanvasElement = aParentCanvas;

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::SetDimensions(PRInt32 width, PRInt32 height)
{
    if (mWidth == width && mHeight == height)
        return NS_OK;

    // If we already have a gl context, then we just need to resize
    // FB0.
    if (gl &&
        gl->ResizeOffscreen(gfxIntSize(width, height)))
    {
        // everything's good, we're done here
        mWidth = width;
        mHeight = height;
        mResetLayer = PR_TRUE;
        return NS_OK;
    }

    // We're going to create an entirely new context.  If our
    // generation is not 0 right now (that is, if this isn't the first
    // context we're creating), we may have to dispatch a context lost
    // event.

    // If incrementing the generation would cause overflow,
    // don't allow it.  Allowing this would allow us to use
    // resource handles created from older context generations.
    if (!(mGeneration+1).valid())
        return NS_ERROR_FAILURE; // exit without changing the value of mGeneration

    // We're going to recreate our context, so make sure we clean up
    // after ourselves.
    DestroyResourcesAndContext();

    gl::ContextFormat format(gl::ContextFormat::BasicRGBA32);
    format.depth = 16;
    format.minDepth = 1;

    nsCOMPtr<nsIPrefBranch> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(prefService != nsnull, NS_ERROR_FAILURE);

    PRBool verbose = PR_FALSE;
    prefService->GetBoolPref("webgl.verbose", &verbose);
    mVerbose = verbose;

    PRBool forceOSMesa = PR_FALSE;
    prefService->GetBoolPref("webgl.force_osmesa", &forceOSMesa);

    if (!forceOSMesa) {

    PRBool useOpenGL = PR_TRUE;
    PRBool useANGLE = PR_TRUE;

    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (gfxInfo) {
        PRInt32 status;
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_OPENGL, &status))) {
            if (status == nsIGfxInfo::FEATURE_BLOCKED ||
                status == nsIGfxInfo::FEATURE_NOT_AVAILABLE)
            {
                useOpenGL = PR_FALSE;
            }
        }
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_ANGLE, &status))) {
            if (status == nsIGfxInfo::FEATURE_BLOCKED ||
                status == nsIGfxInfo::FEATURE_NOT_AVAILABLE)
            {
                useANGLE = PR_FALSE;
            }
        }
    }

    #ifdef XP_WIN
        // On Windows, we may have a choice of backends, including straight
        // OpenGL, D3D through ANGLE via EGL, or straight EGL/GLES2.
        // We don't differentiate the latter two yet, but we allow for
        // a env var to try EGL first, instead of last.
        bool preferEGL = PR_GetEnv("MOZ_WEBGL_PREFER_EGL") != nsnull;

        // if we want EGL, try it first
        if (!gl && preferEGL && useANGLE) {
            gl = gl::GLContextProviderEGL::CreateOffscreen(gfxIntSize(width, height), format);
            if (gl && !InitAndValidateGL()) {
                gl = nsnull;
            }
        }

        // if it failed, then try the default provider, whatever that is
        if (!gl && useOpenGL) {
            gl = gl::GLContextProvider::CreateOffscreen(gfxIntSize(width, height), format);
            if (gl && !InitAndValidateGL()) {
                gl = nsnull;
            }
        }

        // if that failed, and we weren't already preferring EGL, try it now.
        if (!gl && !preferEGL && useANGLE) {
            gl = gl::GLContextProviderEGL::CreateOffscreen(gfxIntSize(width, height), format);
            if (gl && !InitAndValidateGL()) {
                gl = nsnull;
            }
        }
    #else
        // other platforms just use whatever the default is
        if (!gl && useOpenGL) {
            gl = gl::GLContextProvider::CreateOffscreen(gfxIntSize(width, height), format);
            if (gl && !InitAndValidateGL()) {
                gl = nsnull;
            }
        }
    #endif
    }

    // last chance, try OSMesa
    if (!gl) {
        gl = gl::GLContextProviderOSMesa::CreateOffscreen(gfxIntSize(width, height), format);
        if (gl) {
            if (!InitAndValidateGL()) {
                gl = nsnull;
            } else {
                // make sure we notify always in this case, because it's likely going to be
                // painfully slow
                LogMessage("WebGL: Using software rendering via OSMesa");
            }
        }
    }

    if (!gl) {
        if (forceOSMesa) {
            LogMessage("WebGL: You set the webgl.force_osmesa preference to true, but OSMesa can't be found. "
                       "Either install OSMesa and let webgl.osmesalib point to it, "
                       "or set webgl.force_osmesa back to false.");
        } else {
            #ifdef XP_WIN
                LogMessage("WebGL: Can't get a usable OpenGL context (also tried Direct3D via ANGLE)");
            #else
                LogMessage("WebGL: Can't get a usable OpenGL context");
            #endif
        }
        return NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    printf_stderr ("--- WebGL context created: %p\n", gl.get());
#endif

    mWidth = width;
    mHeight = height;
    mResetLayer = PR_TRUE;

    // increment the generation number
    ++mGeneration;

#if 0
    if (mGeneration > 0) {
        // XXX dispatch context lost event
    }
#endif

    MakeContextCurrent();

    // Make sure that we clear this out, otherwise
    // we'll end up displaying random memory
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, gl->GetOffscreenFBO());
    gl->fViewport(0, 0, mWidth, mHeight);
    gl->fClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->fClearDepth(1.0f);
    gl->fClearStencil(0);
    gl->fClear(LOCAL_GL_COLOR_BUFFER_BIT | LOCAL_GL_DEPTH_BUFFER_BIT | LOCAL_GL_STENCIL_BUFFER_BIT);

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f)
{
    if (!gl)
        return NS_OK;

    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                                         gfxASurface::ImageFormatARGB32);
    if (surf->CairoStatus() != 0)
        return NS_ERROR_FAILURE;

    MakeContextCurrent();
    gl->fReadPixels(0, 0, mWidth, mHeight,
                    LOCAL_GL_BGRA,
                    LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV,
                    surf->Data());

    gfxUtils::PremultiplyImageSurface(surf);

    nsRefPtr<gfxPattern> pat = new gfxPattern(surf);
    pat->SetFilter(f);

    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->Fill();

    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetInputStream(const char* aMimeType,
                             const PRUnichar* aEncoderOptions,
                             nsIInputStream **aStream)
{
    return NS_ERROR_FAILURE;

    // XXX fix this
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
WebGLContext::GetThebesSurface(gfxASurface **surface)
{
    return NS_ERROR_NOT_AVAILABLE;
}

static PRUint8 gWebGLLayerUserData;

already_AddRefed<layers::CanvasLayer>
WebGLContext::GetCanvasLayer(CanvasLayer *aOldLayer,
                             LayerManager *aManager)
{
    if (!mResetLayer && aOldLayer &&
        aOldLayer->HasUserData(&gWebGLLayerUserData)) {
        NS_ADDREF(aOldLayer);
        if (mInvalidated) {
            aOldLayer->Updated(nsIntRect(0, 0, mWidth, mHeight));
            mInvalidated = PR_FALSE;
        }
        return aOldLayer;
    }

    nsRefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nsnull;
    }
    canvasLayer->SetUserData(&gWebGLLayerUserData, nsnull);

    CanvasLayer::Data data;

    // the gl context may either provide a native PBuffer, in which case we want to initialize
    // data with the gl context directly, or may provide a surface to which it renders (this is the case
    // of OSMesa contexts), in which case we want to initialize data with that surface.

    void* native_surface = gl->GetNativeData(gl::GLContext::NativeImageSurface);

    if (native_surface) {
        data.mSurface = static_cast<gfxASurface*>(native_surface);
    } else {
        data.mGLContext = gl.get();
    }

    data.mSize = nsIntSize(mWidth, mHeight);
    data.mGLBufferIsPremultiplied = PR_FALSE;

    canvasLayer->Initialize(data);
    PRUint32 flags = gl->CreationFormat().alpha == 0 ? Layer::CONTENT_OPAQUE : 0;
    canvasLayer->SetContentFlags(flags);
    canvasLayer->Updated(nsIntRect(0, 0, mWidth, mHeight));

    mInvalidated = PR_FALSE;
    mResetLayer = PR_FALSE;

    return canvasLayer.forget().get();
}

//
// XPCOM goop
//

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(WebGLContext, nsICanvasRenderingContextWebGL)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(WebGLContext, nsICanvasRenderingContextWebGL)

NS_IMPL_CYCLE_COLLECTION_CLASS(WebGLContext)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebGLContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebGLContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(CanvasRenderingContextWebGL, WebGLContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextWebGL)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextWebGL)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasRenderingContextWebGL)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLBuffer)
NS_IMPL_RELEASE(WebGLBuffer)

DOMCI_DATA(WebGLBuffer, WebGLBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLTexture)
NS_IMPL_RELEASE(WebGLTexture)

DOMCI_DATA(WebGLTexture, WebGLTexture)

NS_INTERFACE_MAP_BEGIN(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLTexture)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLProgram)
NS_IMPL_RELEASE(WebGLProgram)

DOMCI_DATA(WebGLProgram, WebGLProgram)

NS_INTERFACE_MAP_BEGIN(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLProgram)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShader)
NS_IMPL_RELEASE(WebGLShader)

DOMCI_DATA(WebGLShader, WebGLShader)

NS_INTERFACE_MAP_BEGIN(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLShader)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFramebuffer)
NS_IMPL_RELEASE(WebGLFramebuffer)

DOMCI_DATA(WebGLFramebuffer, WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLFramebuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLRenderbuffer)
NS_IMPL_RELEASE(WebGLRenderbuffer)

DOMCI_DATA(WebGLRenderbuffer, WebGLRenderbuffer)

NS_INTERFACE_MAP_BEGIN(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLRenderbuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUniformLocation)
NS_IMPL_RELEASE(WebGLUniformLocation)

DOMCI_DATA(WebGLUniformLocation, WebGLUniformLocation)

NS_INTERFACE_MAP_BEGIN(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLUniformLocation)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLActiveInfo)
NS_IMPL_RELEASE(WebGLActiveInfo)

DOMCI_DATA(WebGLActiveInfo, WebGLActiveInfo)

NS_INTERFACE_MAP_BEGIN(WebGLActiveInfo)
  NS_INTERFACE_MAP_ENTRY(WebGLActiveInfo)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLActiveInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLActiveInfo)
NS_INTERFACE_MAP_END

#define NAME_NOT_SUPPORTED(base) \
NS_IMETHODIMP base::GetName(WebGLuint *aName) \
{ return NS_ERROR_NOT_IMPLEMENTED; } \
NS_IMETHODIMP base::SetName(WebGLuint aName) \
{ return NS_ERROR_NOT_IMPLEMENTED; }

NAME_NOT_SUPPORTED(WebGLTexture)
NAME_NOT_SUPPORTED(WebGLBuffer)
NAME_NOT_SUPPORTED(WebGLProgram)
NAME_NOT_SUPPORTED(WebGLShader)
NAME_NOT_SUPPORTED(WebGLFramebuffer)
NAME_NOT_SUPPORTED(WebGLRenderbuffer)

/* [noscript] attribute WebGLint location; */
NS_IMETHODIMP WebGLUniformLocation::GetLocation(WebGLint *aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP WebGLUniformLocation::SetLocation(WebGLint aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute WebGLint size; */
NS_IMETHODIMP WebGLActiveInfo::GetSize(WebGLint *aSize)
{
    *aSize = mSize;
    return NS_OK;
}

/* readonly attribute WebGLenum type; */
NS_IMETHODIMP WebGLActiveInfo::GetType(WebGLenum *aType)
{
    *aType = mType;
    return NS_OK;
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP WebGLActiveInfo::GetName(nsAString & aName)
{
    aName = mName;
    return NS_OK;
}
