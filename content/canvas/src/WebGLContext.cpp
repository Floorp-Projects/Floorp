/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"

#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsDOMError.h"
#include "nsIGfxInfo.h"

#include "nsIPropertyBag.h"
#include "nsIVariant.h"

#include "imgIEncoder.h"

#include "gfxContext.h"
#include "gfxPattern.h"
#include "gfxUtils.h"

#include "CanvasUtils.h"
#include "nsDisplayList.h"

#include "GLContextProvider.h"

#include "gfxCrashReporterUtils.h"

#include "nsSVGEffects.h"

#include "prenv.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"

#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::layers;

NS_IMPL_ISUPPORTS1(WebGLMemoryPressureObserver, nsIObserver)

NS_IMETHODIMP
WebGLMemoryPressureObserver::Observe(nsISupports* aSubject,
                                     const char* aTopic,
                                     const PRUnichar* aSomeData)
{
  if (strcmp(aTopic, "memory-pressure") == 0)
    mContext->ForceLoseContext();
  return NS_OK;
}


nsresult NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** aResult);

nsresult
NS_NewCanvasRenderingContextWebGL(nsIDOMWebGLRenderingContext** aResult)
{
    Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_USED, 1);
    nsIDOMWebGLRenderingContext* ctx = new WebGLContext();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

WebGLContext::WebGLContext()
    : gl(nsnull)
{
    SetIsDOMBinding();
    mExtensions.SetLength(WebGLExtensionID_number_of_extensions);

    mGeneration = 0;
    mInvalidated = false;
    mResetLayer = true;
    mOptionsFrozen = false;

    mActiveTexture = 0;
    mWebGLError = LOCAL_GL_NO_ERROR;
    mPixelStoreFlipY = false;
    mPixelStorePremultiplyAlpha = false;
    mPixelStoreColorspaceConversion = BROWSER_DEFAULT_WEBGL;

    mShaderValidation = true;

    mBlackTexturesAreInitialized = false;
    mFakeBlackStatus = DoNotNeedFakeBlack;

    mVertexAttrib0Vector[0] = 0;
    mVertexAttrib0Vector[1] = 0;
    mVertexAttrib0Vector[2] = 0;
    mVertexAttrib0Vector[3] = 1;
    mFakeVertexAttrib0BufferObjectVector[0] = 0;
    mFakeVertexAttrib0BufferObjectVector[1] = 0;
    mFakeVertexAttrib0BufferObjectVector[2] = 0;
    mFakeVertexAttrib0BufferObjectVector[3] = 1;
    mFakeVertexAttrib0BufferObjectSize = 0;
    mFakeVertexAttrib0BufferObject = 0;
    mFakeVertexAttrib0BufferStatus = VertexAttrib0Status::Default;

    // these are de default values, see 6.2 State tables in the OpenGL ES 2.0.25 spec
    mColorWriteMask[0] = 1;
    mColorWriteMask[1] = 1;
    mColorWriteMask[2] = 1;
    mColorWriteMask[3] = 1;
    mDepthWriteMask = 1;
    mColorClearValue[0] = 0.f;
    mColorClearValue[1] = 0.f;
    mColorClearValue[2] = 0.f;
    mColorClearValue[3] = 0.f;
    mDepthClearValue = 1.f;
    mStencilClearValue = 0;
    mStencilRefFront = 0;
    mStencilRefBack = 0;
    mStencilValueMaskFront = 0xffffffff;
    mStencilValueMaskBack  = 0xffffffff;
    mStencilWriteMaskFront = 0xffffffff;
    mStencilWriteMaskBack  = 0xffffffff;

    mScissorTestEnabled = 0;
    mDitherEnabled = 1;
    mBackbufferClearingStatus = BackbufferClearingStatus::NotClearedSinceLastPresented;
    
    // initialize some GL values: we're going to get them from the GL and use them as the sizes of arrays,
    // so in case glGetIntegerv leaves them uninitialized because of a GL bug, we would have very weird crashes.
    mGLMaxVertexAttribs = 0;
    mGLMaxTextureUnits = 0;
    mGLMaxTextureSize = 0;
    mGLMaxCubeMapTextureSize = 0;
    mGLMaxTextureImageUnits = 0;
    mGLMaxVertexTextureImageUnits = 0;
    mGLMaxVaryingVectors = 0;
    mGLMaxFragmentUniformVectors = 0;
    mGLMaxVertexUniformVectors = 0;
    
    // See OpenGL ES 2.0.25 spec, 6.2 State Tables, table 6.13
    mPixelStorePackAlignment = 4;
    mPixelStoreUnpackAlignment = 4;

    WebGLMemoryMultiReporterWrapper::AddWebGLContext(this);

    mAllowRestore = true;
    mContextLossTimerRunning = false;
    mDrawSinceContextLossTimerSet = false;
    mContextRestorer = do_CreateInstance("@mozilla.org/timer;1");
    mContextStatus = ContextStable;
    mContextLostErrorSet = false;

    mAlreadyGeneratedWarnings = 0;
}

WebGLContext::~WebGLContext()
{
    DestroyResourcesAndContext();
    WebGLMemoryMultiReporterWrapper::RemoveWebGLContext(this);
    TerminateContextLossTimer();
    mContextRestorer = nsnull;
}

JSObject*
WebGLContext::WrapObject(JSContext *cx, JSObject *scope,
                         bool *triedToWrap)
{
    return dom::WebGLRenderingContextBinding::Wrap(cx, scope, this,
                                                   triedToWrap);
}

void
WebGLContext::DestroyResourcesAndContext()
{
    if (mMemoryPressureObserver) {
        nsCOMPtr<nsIObserverService> observerService
            = mozilla::services::GetObserverService();
        if (observerService) {
            observerService->RemoveObserver(mMemoryPressureObserver,
                                            "memory-pressure");
        }
        mMemoryPressureObserver = nsnull;
    }

    if (!gl)
        return;

    gl->MakeCurrent();

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();
    mBoundArrayBuffer = nsnull;
    mBoundElementArrayBuffer = nsnull;
    mCurrentProgram = nsnull;
    mBoundFramebuffer = nsnull;
    mBoundRenderbuffer = nsnull;

    mAttribBuffers.Clear();

    while (!mTextures.isEmpty())
        mTextures.getLast()->DeleteOnce();
    while (!mBuffers.isEmpty())
        mBuffers.getLast()->DeleteOnce();
    while (!mRenderbuffers.isEmpty())
        mRenderbuffers.getLast()->DeleteOnce();
    while (!mFramebuffers.isEmpty())
        mFramebuffers.getLast()->DeleteOnce();
    while (!mShaders.isEmpty())
        mShaders.getLast()->DeleteOnce();
    while (!mPrograms.isEmpty())
        mPrograms.getLast()->DeleteOnce();

    if (mBlackTexturesAreInitialized) {
        gl->fDeleteTextures(1, &mBlackTexture2D);
        gl->fDeleteTextures(1, &mBlackTextureCubeMap);
        mBlackTexturesAreInitialized = false;
    }

    if (mFakeVertexAttrib0BufferObject) {
        gl->fDeleteBuffers(1, &mFakeVertexAttrib0BufferObject);
    }

    // We just got rid of everything, so the context had better
    // have been going away.
#ifdef DEBUG
    if (gl->DebugMode()) {
        printf_stderr("--- WebGL context destroyed: %p\n", gl.get());
    }
#endif

    gl = nsnull;
}

void
WebGLContext::Invalidate()
{
    if (mInvalidated)
        return;

    if (!mCanvasElement)
        return;

    nsSVGEffects::InvalidateDirectRenderingObservers(mCanvasElement);

    mInvalidated = true;
    mCanvasElement->InvalidateCanvasContent(nsnull);
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

static bool
GetBoolFromPropertyBag(nsIPropertyBag *bag, const char *propName, bool *boolResult)
{
    nsCOMPtr<nsIVariant> vv;
    bool bv;

    nsresult rv = bag->GetProperty(NS_ConvertASCIItoUTF16(propName), getter_AddRefs(vv));
    if (NS_FAILED(rv) || !vv)
        return false;

    rv = vv->GetAsBool(&bv);
    if (NS_FAILED(rv))
        return false;

    *boolResult = bv ? true : false;
    return true;
}

NS_IMETHODIMP
WebGLContext::SetContextOptions(nsIPropertyBag *aOptions)
{
    if (!aOptions)
        return NS_OK;

    bool defaultNoAlpha =
        Preferences::GetBool("webgl.default-no-alpha", false);

    WebGLContextOptions newOpts;

    GetBoolFromPropertyBag(aOptions, "stencil", &newOpts.stencil);
    GetBoolFromPropertyBag(aOptions, "depth", &newOpts.depth);
    GetBoolFromPropertyBag(aOptions, "premultipliedAlpha", &newOpts.premultipliedAlpha);
    GetBoolFromPropertyBag(aOptions, "antialias", &newOpts.antialias);
    GetBoolFromPropertyBag(aOptions, "preserveDrawingBuffer", &newOpts.preserveDrawingBuffer);

    // alpha defaults to true as per the spec, but we want to evaluate
    // what will happen if it were to default to false based on a pref
    if (!GetBoolFromPropertyBag(aOptions, "alpha", &newOpts.alpha) && defaultNoAlpha) {
        newOpts.alpha = false;
    }

    // enforce that if stencil is specified, we also give back depth
    newOpts.depth |= newOpts.stencil;

#if 0
    GenerateWarning("aaHint: %d stencil: %d depth: %d alpha: %d premult: %d preserve: %d\n",
               newOpts.antialias ? 1 : 0,
               newOpts.stencil ? 1 : 0,
               newOpts.depth ? 1 : 0,
               newOpts.alpha ? 1 : 0,
               newOpts.premultipliedAlpha ? 1 : 0,
               newOpts.preserveDrawingBuffer ? 1 : 0);
#endif

    if (mOptionsFrozen && newOpts != mOptions) {
        // Error if the options are already frozen, and the ones that were asked for
        // aren't the same as what they were originally.
        return NS_ERROR_FAILURE;
    }

    mOptions = newOpts;
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::SetDimensions(PRInt32 width, PRInt32 height)
{
    /*** early success return cases ***/
  
    if (mCanvasElement) {
        mCanvasElement->InvalidateCanvas();
    }

    if (gl && mWidth == width && mHeight == height)
        return NS_OK;

    // Zero-sized surfaces can cause problems.
    if (width == 0 || height == 0) {
        width = 1;
        height = 1;
    }

    // If we already have a gl context, then we just need to resize it
    if (gl) {
        MakeContextCurrent();

        gl->ResizeOffscreen(gfxIntSize(width, height)); // Doesn't matter if it succeeds (soft-fail)
        // It's unlikely that we'll get a proper-sized context if we recreate if we didn't on resize

        // everything's good, we're done here
        mWidth = gl->OffscreenActualSize().width;
        mHeight = gl->OffscreenActualSize().height;
        mResetLayer = true;

        gl->ClearSafely();

        return NS_OK;
    }

    /*** end of early success return cases ***/

    // At this point we know that the old context is not going to survive, even though we still don't
    // know if creating the new context will succeed.
    DestroyResourcesAndContext();

    // Get some prefs for some preferred/overriden things
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);

    bool forceOSMesa =
        Preferences::GetBool("webgl.force_osmesa", false);
#ifdef XP_WIN
    bool preferEGL =
        Preferences::GetBool("webgl.prefer-egl", false);
    bool preferOpenGL =
        Preferences::GetBool("webgl.prefer-native-gl", false);
#endif
    bool forceEnabled =
        Preferences::GetBool("webgl.force-enabled", false);
    bool useMesaLlvmPipe =
        Preferences::GetBool("gfx.prefer-mesa-llvmpipe", false);
    bool disabled =
        Preferences::GetBool("webgl.disabled", false);
    bool prefer16bit =
        Preferences::GetBool("webgl.prefer-16bpp", false);

    ScopedGfxFeatureReporter reporter("WebGL", forceEnabled);

    if (disabled)
        return NS_ERROR_FAILURE;

    // We're going to create an entirely new context.  If our
    // generation is not 0 right now (that is, if this isn't the first
    // context we're creating), we may have to dispatch a context lost
    // event.

    // If incrementing the generation would cause overflow,
    // don't allow it.  Allowing this would allow us to use
    // resource handles created from older context generations.
    if (!(mGeneration + 1).isValid())
        return NS_ERROR_FAILURE; // exit without changing the value of mGeneration

    gl::ContextFormat format(gl::ContextFormat::BasicRGBA32);
    if (mOptions.depth) {
        format.depth = 24;
        format.minDepth = 16;
    }

    if (mOptions.stencil) {
        format.stencil = 8;
        format.minStencil = 8;
    }

    if (!mOptions.alpha) {
        format.alpha = 0;
        format.minAlpha = 0;
    }

    // we should really have this behind a
    // |gfxPlatform::GetPlatform()->GetScreenDepth() == 16| check, but
    // for now it's just behind a pref for testing/evaluation.
    if (prefer16bit) {
        // Select 4444 or 565 on 16-bit displays; we won't/shouldn't
        // hit this on the desktop, but let mobile know we're ok with
        // it.  Note that we don't just set this to 4440 if no alpha,
        // because that might cause us to choose 4444 anyway and we
        // don't want that.
        if (mOptions.alpha) {
            format.red = 4;
            format.green = 4;
            format.blue = 4;
            format.alpha = 4;
        } else {
            format.red = 5;
            format.green = 6;
            format.blue = 5;
            format.alpha = 0;
        }
    }

    bool forceMSAA =
        Preferences::GetBool("webgl.msaa-force", false);

    PRInt32 status;
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (mOptions.antialias &&
        gfxInfo &&
        NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_MSAA, &status))) {
        if (status == nsIGfxInfo::FEATURE_NO_INFO || forceMSAA) {
            PRUint32 msaaLevel = Preferences::GetUint("webgl.msaa-level", 2);
            format.samples = msaaLevel*msaaLevel;
        }
    }

#ifdef XP_WIN
    if (PR_GetEnv("MOZ_WEBGL_PREFER_EGL")) {
        preferEGL = true;
    }
#endif

    // Ask GfxInfo about what we should use
    bool useOpenGL = true;

#ifdef XP_WIN
    bool useANGLE = true;
#endif

    if (gfxInfo && !forceEnabled) {
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_OPENGL, &status))) {
            if (status != nsIGfxInfo::FEATURE_NO_INFO) {
                useOpenGL = false;
            }
        }
#ifdef XP_WIN
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_ANGLE, &status))) {
            if (status != nsIGfxInfo::FEATURE_NO_INFO) {
                useANGLE = false;
            }
        }
#endif
    }

#ifdef XP_WIN
    // allow forcing GL and not EGL/ANGLE
    if (useMesaLlvmPipe || PR_GetEnv("MOZ_WEBGL_FORCE_OPENGL")) {
        preferEGL = false;
        useANGLE = false;
        useOpenGL = true;
    }
#endif

    // if we're forcing osmesa, do it first
    if (forceOSMesa) {
        gl = gl::GLContextProviderOSMesa::CreateOffscreen(gfxIntSize(width, height), format);
        if (!gl || !InitAndValidateGL()) {
            GenerateWarning("OSMesa forced, but creating context failed -- aborting!");
            return NS_ERROR_FAILURE;
        }
        GenerateWarning("Using software rendering via OSMesa (THIS WILL BE SLOW)");
    }

#ifdef XP_WIN
    // if we want EGL, try it now
    if (!gl && (preferEGL || useANGLE) && !preferOpenGL) {
        gl = gl::GLContextProviderEGL::CreateOffscreen(gfxIntSize(width, height), format);
        if (!gl || !InitAndValidateGL()) {
            GenerateWarning("Error during ANGLE OpenGL ES initialization");
            return NS_ERROR_FAILURE;
        }
    }
#endif

    // try the default provider, whatever that is
    if (!gl && useOpenGL) {
        GLContext::ContextFlags flag = useMesaLlvmPipe 
                                       ? GLContext::ContextFlagsMesaLLVMPipe
                                       : GLContext::ContextFlagsNone;
        gl = gl::GLContextProvider::CreateOffscreen(gfxIntSize(width, height), 
                                                               format, flag);
        if (gl && !InitAndValidateGL()) {
            GenerateWarning("Error during %s initialization", 
                            useMesaLlvmPipe ? "Mesa LLVMpipe" : "OpenGL");
            return NS_ERROR_FAILURE;
        }
    }

    // finally, try OSMesa
    if (!gl) {
        gl = gl::GLContextProviderOSMesa::CreateOffscreen(gfxIntSize(width, height), format);
        if (gl) {
            if (!InitAndValidateGL()) {
                GenerateWarning("Error during OSMesa initialization");
                return NS_ERROR_FAILURE;
            } else {
                GenerateWarning("Using software rendering via OSMesa (THIS WILL BE SLOW)");
            }
        }
    }

    if (!gl) {
        GenerateWarning("Can't get a usable WebGL context");
        return NS_ERROR_FAILURE;
    }

#ifdef DEBUG
    if (gl->DebugMode()) {
        printf_stderr("--- WebGL context created: %p\n", gl.get());
    }
#endif

    mWidth = width;
    mHeight = height;
    mResetLayer = true;
    mOptionsFrozen = true;

    mHasRobustness = gl->HasRobustness();

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

    gl->ClearSafely();

    reporter.SetSuccessful();
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f, PRUint32 aFlags)
{
    if (!gl)
        return NS_OK;

    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                                         gfxASurface::ImageFormatARGB32);
    if (surf->CairoStatus() != 0)
        return NS_ERROR_FAILURE;

    gl->ReadPixelsIntoImageSurface(0, 0, mWidth, mHeight, surf);

    bool srcPremultAlpha = mOptions.premultipliedAlpha;
    bool dstPremultAlpha = aFlags & RenderFlagPremultAlpha;

    if (!srcPremultAlpha && dstPremultAlpha) {
        gfxUtils::PremultiplyImageSurface(surf);
    } else if (srcPremultAlpha && !dstPremultAlpha) {
        gfxUtils::UnpremultiplyImageSurface(surf);
    }

    nsRefPtr<gfxPattern> pat = new gfxPattern(surf);
    pat->SetFilter(f);

    // Pixels from ReadPixels will be "upside down" compared to
    // what cairo wants, so draw with a y-flip and a translte to
    // flip them.
    gfxMatrix m;
    m.Translate(gfxPoint(0.0, mHeight));
    m.Scale(1.0, -1.0);
    pat->SetMatrix(m);

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
    NS_ASSERTION(gl, "GetInputStream on invalid context?");
    if (!gl)
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                                         gfxASurface::ImageFormatARGB32);
    if (surf->CairoStatus() != 0)
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> tmpcx = new gfxContext(surf);
    // Use Render() to make sure that appropriate y-flip gets applied
    PRUint32 flags = mOptions.premultipliedAlpha ? RenderFlagPremultAlpha : 0;
    nsresult rv = Render(tmpcx, gfxPattern::FILTER_NEAREST, flags);
    if (NS_FAILED(rv))
        return rv;

    const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
    nsAutoArrayPtr<char> conid(new char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

    if (!conid)
        return NS_ERROR_OUT_OF_MEMORY;

    strcpy(conid, encoderPrefix);
    strcat(conid, aMimeType);

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
    if (!encoder)
        return NS_ERROR_FAILURE;

    int format = imgIEncoder::INPUT_FORMAT_HOSTARGB;
    if (!mOptions.premultipliedAlpha) {
        // We need to convert to INPUT_FORMAT_RGBA, otherwise
        // we are automatically considered premult, and unpremult'd.
        // Yes, it is THAT silly.
        // Except for different lossy conversions by color,
        // we could probably just change the label, and not change the data.
        gfxUtils::ConvertBGRAtoRGBA(surf);
        format = imgIEncoder::INPUT_FORMAT_RGBA;
    }

    rv = encoder->InitFromData(surf->Data(),
                               mWidth * mHeight * 4,
                               mWidth, mHeight,
                               surf->Stride(),
                               format,
                               nsDependentString(aEncoderOptions));
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(encoder, aStream);
}

NS_IMETHODIMP
WebGLContext::GetThebesSurface(gfxASurface **surface)
{
    return NS_ERROR_NOT_AVAILABLE;
}

static PRUint8 gWebGLLayerUserData;

namespace mozilla {

class WebGLContextUserData : public LayerUserData {
public:
    WebGLContextUserData(nsHTMLCanvasElement *aContent)
    : mContent(aContent) {}

  /** DidTransactionCallback gets called by the Layers code everytime the WebGL canvas gets composite,
    * so it really is the right place to put actions that have to be performed upon compositing
    */
  static void DidTransactionCallback(void* aData)
  {
    WebGLContextUserData *userdata = static_cast<WebGLContextUserData*>(aData);
    nsHTMLCanvasElement *canvas = userdata->mContent;
    WebGLContext *context = static_cast<WebGLContext*>(canvas->GetContextAtIndex(0));

    context->mBackbufferClearingStatus = BackbufferClearingStatus::NotClearedSinceLastPresented;
    canvas->MarkContextClean();
  }

private:
  nsRefPtr<nsHTMLCanvasElement> mContent;
};

} // end namespace mozilla

already_AddRefed<layers::CanvasLayer>
WebGLContext::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                             CanvasLayer *aOldLayer,
                             LayerManager *aManager)
{
    if (!IsContextStable())
        return nsnull;

    if (!mResetLayer && aOldLayer &&
        aOldLayer->HasUserData(&gWebGLLayerUserData)) {
        NS_ADDREF(aOldLayer);
        return aOldLayer;
    }

    nsRefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nsnull;
    }
    WebGLContextUserData *userData = nsnull;
    if (aBuilder->IsPaintingToWindow()) {
      // Make the layer tell us whenever a transaction finishes (including
      // the current transaction), so we can clear our invalidation state and
      // start invalidating again. We need to do this for the layer that is
      // being painted to a window (there shouldn't be more than one at a time,
      // and if there is, flushing the invalidation state more often than
      // necessary is harmless).

      // The layer will be destroyed when we tear down the presentation
      // (at the latest), at which time this userData will be destroyed,
      // releasing the reference to the element.
      // The userData will receive DidTransactionCallbacks, which flush the
      // the invalidation state to indicate that the canvas is up to date.
      userData = new WebGLContextUserData(mCanvasElement);
      canvasLayer->SetDidTransactionCallback(
              WebGLContextUserData::DidTransactionCallback, userData);
    }
    canvasLayer->SetUserData(&gWebGLLayerUserData, userData);

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
    data.mGLBufferIsPremultiplied = mOptions.premultipliedAlpha ? true : false;

    canvasLayer->Initialize(data);
    PRUint32 flags = gl->CreationFormat().alpha == 0 ? Layer::CONTENT_OPAQUE : 0;
    canvasLayer->SetContentFlags(flags);
    canvasLayer->Updated();

    mResetLayer = false;

    return canvasLayer.forget().get();
}

NS_IMETHODIMP
WebGLContext::GetContextAttributes(jsval *aResult)
{
    ErrorResult rv;
    JSObject* obj = GetContextAttributes(rv);
    if (rv.Failed())
        return rv.ErrorCode();

    *aResult = JS::ObjectOrNullValue(obj);
    return NS_OK;
}

JSObject*
WebGLContext::GetContextAttributes(ErrorResult &rv)
{
    if (!IsContextStable())
    {
        return NULL;
    }

    JSContext *cx = nsContentUtils::GetCurrentJSContext();
    if (!cx) {
        rv.Throw(NS_ERROR_FAILURE);
        return NULL;
    }

    JSObject *obj = JS_NewObject(cx, NULL, NULL, NULL);
    if (!obj) {
        rv.Throw(NS_ERROR_FAILURE);
        return NULL;
    }

    gl::ContextFormat cf = gl->ActualFormat();

    if (!JS_DefineProperty(cx, obj, "alpha", cf.alpha > 0 ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "depth", cf.depth > 0 ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "stencil", cf.stencil > 0 ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "antialias", cf.samples > 1 ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "premultipliedAlpha",
                           mOptions.premultipliedAlpha ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, obj, "preserveDrawingBuffer",
                           mOptions.preserveDrawingBuffer ? JSVAL_TRUE : JSVAL_FALSE,
                           NULL, NULL, JSPROP_ENUMERATE))
    {
        rv.Throw(NS_ERROR_FAILURE);
        return NULL;
    }

    return obj;
}

/* [noscript] DOMString mozGetUnderlyingParamString(in WebGLenum pname); */
NS_IMETHODIMP
WebGLContext::MozGetUnderlyingParamString(PRUint32 pname, nsAString& retval)
{
    if (!IsContextStable())
        return NS_OK;

    retval.SetIsVoid(true);

    MakeContextCurrent();

    switch (pname) {
    case LOCAL_GL_VENDOR:
    case LOCAL_GL_RENDERER:
    case LOCAL_GL_VERSION:
    case LOCAL_GL_SHADING_LANGUAGE_VERSION:
    case LOCAL_GL_EXTENSIONS: {
        const char *s = (const char *) gl->fGetString(pname);
        retval.Assign(NS_ConvertASCIItoUTF16(nsDependentCString(s)));
    }
        break;

    default:
        return NS_ERROR_INVALID_ARG;
    }

    return NS_OK;
}

bool WebGLContext::IsExtensionSupported(WebGLExtensionID ext)
{
    bool isSupported = false;

    switch (ext) {
        case OES_standard_derivatives:
        case WEBGL_lose_context:
            // We always support these extensions.
            isSupported = true;
            break;
        case OES_texture_float:
            isSupported = gl->IsExtensionSupported(gl->IsGLES2() ? GLContext::OES_texture_float 
                                                                 : GLContext::ARB_texture_float);
            break;
        case EXT_texture_filter_anisotropic:
            isSupported = gl->IsExtensionSupported(GLContext::EXT_texture_filter_anisotropic);
            break;
        case WEBGL_compressed_texture_s3tc:
            if (gl->IsExtensionSupported(GLContext::EXT_texture_compression_s3tc)) {
                isSupported = true;
            } else if (gl->IsExtensionSupported(GLContext::EXT_texture_compression_dxt1) &&
                       gl->IsExtensionSupported(GLContext::ANGLE_texture_compression_dxt3) &&
                       gl->IsExtensionSupported(GLContext::ANGLE_texture_compression_dxt5))
            {
                isSupported = true;
            }
            break;
        default:
            MOZ_ASSERT(false, "should not get there.");
    }

    return isSupported;
}

NS_IMETHODIMP
WebGLContext::GetExtension(const nsAString& aName, nsIWebGLExtension **retval)
{
    *retval = GetExtension(aName);
    NS_IF_ADDREF(*retval);
    return NS_OK;
}

nsIWebGLExtension*
WebGLContext::GetExtension(const nsAString& aName)
{
    if (!IsContextStable())
        return nsnull;

    if (mDisableExtensions) {
        return nsnull;
    }

    WebGLExtensionID ext = WebGLExtensionID_unknown_extension;

    if (aName.Equals(NS_LITERAL_STRING("OES_texture_float"),
        nsCaseInsensitiveStringComparator()))
    {
        if (IsExtensionSupported(OES_texture_float))
            ext = OES_texture_float;
    }
    else if (aName.Equals(NS_LITERAL_STRING("OES_standard_derivatives"),
             nsCaseInsensitiveStringComparator()))
    {
        if (IsExtensionSupported(OES_standard_derivatives))
            ext = OES_standard_derivatives;
    }
    else if (aName.Equals(NS_LITERAL_STRING("EXT_texture_filter_anisotropic"),
             nsCaseInsensitiveStringComparator()))
    {
        if (IsExtensionSupported(EXT_texture_filter_anisotropic))
            ext = EXT_texture_filter_anisotropic;
    }
    else if (aName.Equals(NS_LITERAL_STRING("MOZ_EXT_texture_filter_anisotropic"),
             nsCaseInsensitiveStringComparator()))
    {
        GenerateWarning("MOZ_EXT_texture_filter_anisotropic has been renamed to EXT_texture_filter_anisotropic. "
                        "Support for the MOZ_-prefixed string will be removed very soon.");
        if (IsExtensionSupported(EXT_texture_filter_anisotropic))
            ext = EXT_texture_filter_anisotropic;
    }
    else if (aName.Equals(NS_LITERAL_STRING("MOZ_WEBGL_lose_context"),
             nsCaseInsensitiveStringComparator()))
    {
        if (IsExtensionSupported(WEBGL_lose_context))
            ext = WEBGL_lose_context;
    }
    else if (aName.Equals(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_s3tc"),
             nsCaseInsensitiveStringComparator()))
    {
        if (IsExtensionSupported(WEBGL_compressed_texture_s3tc))
            ext = WEBGL_compressed_texture_s3tc;
    }

    if (ext == WebGLExtensionID_unknown_extension) {
      return nsnull;
    }

    if (!mExtensions[ext]) {
        switch (ext) {
            case OES_standard_derivatives:
                mExtensions[ext] = new WebGLExtensionStandardDerivatives(this);
                break;
            case EXT_texture_filter_anisotropic:
                mExtensions[ext] = new WebGLExtensionTextureFilterAnisotropic(this);
                break;
            case WEBGL_lose_context:
                mExtensions[ext] = new WebGLExtensionLoseContext(this);
                break;
            case WEBGL_compressed_texture_s3tc:
                mExtensions[ext] = new WebGLExtensionCompressedTextureS3TC(this);
                break;
            default:
                // create a generic WebGLExtension object for any extensions that don't
                // have any additional tokens or methods. We still need these to be separate
                // objects in case the user might extend the corresponding JS objects with custom
                // properties.
                mExtensions[ext] = new WebGLExtension(this);
                break;
        }
    }

    return mExtensions[ext];
}

void
WebGLContext::ForceClearFramebufferWithDefaultValues(PRUint32 mask, const nsIntRect& viewportRect)
{
    MakeContextCurrent();

    bool initializeColorBuffer = 0 != (mask & LOCAL_GL_COLOR_BUFFER_BIT);
    bool initializeDepthBuffer = 0 != (mask & LOCAL_GL_DEPTH_BUFFER_BIT);
    bool initializeStencilBuffer = 0 != (mask & LOCAL_GL_STENCIL_BUFFER_BIT);

    // fun GL fact: no need to worry about the viewport here, glViewport is just setting up a coordinates transformation,
    // it doesn't affect glClear at all

    // prepare GL state for clearing
    gl->fDisable(LOCAL_GL_SCISSOR_TEST);
    gl->fDisable(LOCAL_GL_DITHER);

    if (initializeColorBuffer) {
        gl->fColorMask(1, 1, 1, 1);
        gl->fClearColor(0.f, 0.f, 0.f, 0.f);
    }

    if (initializeDepthBuffer) {
        gl->fDepthMask(1);
        gl->fClearDepth(1.0f);
    }

    if (initializeStencilBuffer) {
        gl->fStencilMask(0xffffffff);
        gl->fClearStencil(0);
    }

    // do clear
    gl->fClear(mask);

    // restore GL state after clearing
    if (initializeColorBuffer) {
        gl->fColorMask(mColorWriteMask[0],
                       mColorWriteMask[1],
                       mColorWriteMask[2],
                       mColorWriteMask[3]);
        gl->fClearColor(mColorClearValue[0],
                        mColorClearValue[1],
                        mColorClearValue[2],
                        mColorClearValue[3]);
    }

    if (initializeDepthBuffer) {
        gl->fDepthMask(mDepthWriteMask);
        gl->fClearDepth(mDepthClearValue);
    }

    if (initializeStencilBuffer) {
        gl->fStencilMaskSeparate(LOCAL_GL_FRONT, mStencilWriteMaskFront);
        gl->fStencilMaskSeparate(LOCAL_GL_BACK, mStencilWriteMaskBack);
        gl->fClearStencil(mStencilClearValue);
    }

    if (mDitherEnabled)
        gl->fEnable(LOCAL_GL_DITHER);
    else
        gl->fDisable(LOCAL_GL_DITHER);

    if (mScissorTestEnabled)
        gl->fEnable(LOCAL_GL_SCISSOR_TEST);
    else
        gl->fDisable(LOCAL_GL_SCISSOR_TEST);
}

void
WebGLContext::EnsureBackbufferClearedAsNeeded()
{
    if (mOptions.preserveDrawingBuffer)
        return;

    NS_ABORT_IF_FALSE(!mBoundFramebuffer,
                      "EnsureBackbufferClearedAsNeeded must not be called when a FBO is bound");

    if (mBackbufferClearingStatus != BackbufferClearingStatus::NotClearedSinceLastPresented)
        return;

    mBackbufferClearingStatus = BackbufferClearingStatus::ClearedToDefaultValues;

    ForceClearFramebufferWithDefaultValues(LOCAL_GL_COLOR_BUFFER_BIT |
                                           LOCAL_GL_DEPTH_BUFFER_BIT |
                                           LOCAL_GL_STENCIL_BUFFER_BIT,
                                           nsIntRect(0, 0, mWidth, mHeight));

    Invalidate();
}

void
WebGLContext::DummyFramebufferOperation(const char *info)
{
    WebGLenum status;
    CheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER, &status);
    if (status == LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return;
    else
        return ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
}

// We use this timer for many things. Here are the things that it is activated for:
// 1) If a script is using the MOZ_WEBGL_lose_context extension.
// 2) If we are using EGL and _NOT ANGLE_, we query periodically to see if the
//    CONTEXT_LOST_WEBGL error has been triggered.
// 3) If we are using ANGLE, or anything that supports ARB_robustness, query the
//    GPU periodically to see if the reset status bit has been set.
// In all of these situations, we use this timer to send the script context lost
// and restored events asynchronously. For example, if it triggers a context loss,
// the webglcontextlost event will be sent to it the next time the robustness timer
// fires.
// Note that this timer mechanism is not used unless one of these 3 criteria
// are met.
// At a bare minimum, from context lost to context restores, it would take 3
// full timer iterations: detection, webglcontextlost, webglcontextrestored.
NS_IMETHODIMP
WebGLContext::Notify(nsITimer* timer)
{
    TerminateContextLossTimer();

    if (!mCanvasElement) {
        // the canvas is gone. That happens when the page was closed before we got
        // this timer event. In this case, there's nothing to do here, just don't crash.
        return NS_OK;
    }

    // If the context has been lost and we're waiting for it to be restored, do
    // that now.
    if (mContextStatus == ContextLostAwaitingEvent) {
        bool defaultAction;
        nsContentUtils::DispatchTrustedEvent(mCanvasElement->OwnerDoc(),
                                             static_cast<nsIDOMHTMLCanvasElement*>(mCanvasElement),
                                             NS_LITERAL_STRING("webglcontextlost"),
                                             true,
                                             true,
                                             &defaultAction);

        // If the script didn't handle the event, we don't allow restores.
        if (defaultAction)
            mAllowRestore = false;

        // If the script handled the event and we are allowing restores, then
        // mark it to be restored. Otherwise, leave it as context lost
        // (unusable).
        if (!defaultAction && mAllowRestore) {
            ForceRestoreContext();
            // Restart the timer so that it will be restored on the next
            // callback.
            SetupContextLossTimer();
        } else {
            mContextStatus = ContextLost;
        }
    } else if (mContextStatus == ContextLostAwaitingRestore) {
        // Try to restore the context. If it fails, try again later.
        if (NS_FAILED(SetDimensions(mWidth, mHeight))) {
            SetupContextLossTimer();
            return NS_OK;
        }
        mContextStatus = ContextStable;
        nsContentUtils::DispatchTrustedEvent(mCanvasElement->OwnerDoc(),
                                             static_cast<nsIDOMHTMLCanvasElement*>(mCanvasElement),
                                             NS_LITERAL_STRING("webglcontextrestored"),
                                             true,
                                             true);
        // Set all flags back to the state they were in before the context was
        // lost.
        mContextLostErrorSet = false;
        mAllowRestore = true;
    }

    MaybeRestoreContext();
    return NS_OK;
}

void
WebGLContext::MaybeRestoreContext()
{
    // Don't try to handle it if we already know it's busted.
    if (mContextStatus != ContextStable || gl == nsnull)
        return;

    bool isEGL = gl->GetContextType() == GLContext::ContextTypeEGL,
         isANGLE = gl->IsANGLE();

    GLContext::ContextResetARB resetStatus = GLContext::CONTEXT_NO_ERROR;
    if (mHasRobustness) {
        gl->MakeCurrent();
        resetStatus = (GLContext::ContextResetARB) gl->fGetGraphicsResetStatus();
    } else if (isEGL) {
        // Simulate a ARB_robustness guilty context loss for when we
        // get an EGL_CONTEXT_LOST error. It may not actually be guilty,
        // but we can't make any distinction, so we must assume the worst
        // case.
        if (!gl->MakeCurrent(true) && gl->IsContextLost()) {
            resetStatus = GLContext::CONTEXT_GUILTY_CONTEXT_RESET_ARB;
        }
    }
    
    if (resetStatus != GLContext::CONTEXT_NO_ERROR) {
        // It's already lost, but clean up after it and signal to JS that it is
        // lost.
        ForceLoseContext();
    }

    switch (resetStatus) {
        case GLContext::CONTEXT_NO_ERROR:
            // If there has been activity since the timer was set, it's possible
            // that we did or are going to miss something, so clear this flag and
            // run it again some time later.
            if (mDrawSinceContextLossTimerSet)
                SetupContextLossTimer();
            break;
        case GLContext::CONTEXT_GUILTY_CONTEXT_RESET_ARB:
            NS_WARNING("WebGL content on the page caused the graphics card to reset; not restoring the context");
            mAllowRestore = false;
            break;
        case GLContext::CONTEXT_INNOCENT_CONTEXT_RESET_ARB:
            break;
        case GLContext::CONTEXT_UNKNOWN_CONTEXT_RESET_ARB:
            NS_WARNING("WebGL content on the page might have caused the graphics card to reset");
            if (isEGL && isANGLE) {
                // If we're using ANGLE, we ONLY get back UNKNOWN context resets, including for guilty contexts.
                // This means that we can't restore it or risk restoring a guilty context. Should this ever change,
                // we can get rid of the whole IsANGLE() junk from GLContext.h since, as of writing, this is the
                // only use for it. See ANGLE issue 261.
                mAllowRestore = false;
            }
            break;
    }
}

void
WebGLContext::ForceLoseContext()
{
    if (mContextStatus == ContextLostAwaitingEvent)
        return;

    mContextStatus = ContextLostAwaitingEvent;
    // Queue up a task to restore the event.
    SetupContextLossTimer();
    DestroyResourcesAndContext();
}

void
WebGLContext::ForceRestoreContext()
{
    mContextStatus = ContextLostAwaitingRestore;
}

//
// XPCOM goop
//

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLContext)

NS_IMPL_CYCLE_COLLECTION_CLASS(WebGLContext)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WebGLContext)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WebGLContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCanvasElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mExtensions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WebGLContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mCanvasElement, nsINode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_OF_NSCOMPTR(mExtensions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(WebGLRenderingContext, WebGLContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMWebGLRenderingContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  // If the exact way we cast to nsISupports here ever changes, fix our
  // PreCreate hook!
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports,
                                   nsICanvasRenderingContextInternal)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLRenderingContext)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLBuffer)
NS_IMPL_RELEASE(WebGLBuffer)

DOMCI_DATA(WebGLBuffer, WebGLBuffer)

NS_INTERFACE_MAP_BEGIN(WebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLBuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLBuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLTexture)
NS_IMPL_RELEASE(WebGLTexture)

DOMCI_DATA(WebGLTexture, WebGLTexture)

NS_INTERFACE_MAP_BEGIN(WebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLTexture)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLTexture)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLProgram)
NS_IMPL_RELEASE(WebGLProgram)

DOMCI_DATA(WebGLProgram, WebGLProgram)

NS_INTERFACE_MAP_BEGIN(WebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLProgram)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLProgram)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLShader)
NS_IMPL_RELEASE(WebGLShader)

DOMCI_DATA(WebGLShader, WebGLShader)

NS_INTERFACE_MAP_BEGIN(WebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShader)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLShader)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLFramebuffer)
NS_IMPL_RELEASE(WebGLFramebuffer)

DOMCI_DATA(WebGLFramebuffer, WebGLFramebuffer)

NS_INTERFACE_MAP_BEGIN(WebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLFramebuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLFramebuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLRenderbuffer)
NS_IMPL_RELEASE(WebGLRenderbuffer)

DOMCI_DATA(WebGLRenderbuffer, WebGLRenderbuffer)

NS_INTERFACE_MAP_BEGIN(WebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLRenderbuffer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLRenderbuffer)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLUniformLocation)
NS_IMPL_RELEASE(WebGLUniformLocation)

DOMCI_DATA(WebGLUniformLocation, WebGLUniformLocation)

NS_INTERFACE_MAP_BEGIN(WebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLUniformLocation)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLUniformLocation)
NS_INTERFACE_MAP_END

JSObject*
WebGLUniformLocation::WrapObject(JSContext *cx, JSObject *scope)
{
    return dom::WebGLUniformLocationBinding::Wrap(cx, scope, this);
}

NS_IMPL_ADDREF(WebGLShaderPrecisionFormat)
NS_IMPL_RELEASE(WebGLShaderPrecisionFormat)

DOMCI_DATA(WebGLShaderPrecisionFormat, WebGLShaderPrecisionFormat)

NS_INTERFACE_MAP_BEGIN(WebGLShaderPrecisionFormat)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLShaderPrecisionFormat)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLShaderPrecisionFormat)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebGLActiveInfo)
NS_IMPL_RELEASE(WebGLActiveInfo)

DOMCI_DATA(WebGLActiveInfo, WebGLActiveInfo)

NS_INTERFACE_MAP_BEGIN(WebGLActiveInfo)
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

// WebGLExtension

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLExtension)
  
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLExtension)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIWebGLExtension)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(WebGLExtension)
NS_INTERFACE_MAP_END 

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLExtension)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLExtension)

DOMCI_DATA(WebGLExtension, WebGLExtension)

/* readonly attribute WebGLsizei drawingBufferWidth; */
NS_IMETHODIMP
WebGLContext::GetDrawingBufferWidth(WebGLsizei *aWidth)
{
    *aWidth = GetDrawingBufferWidth();
    return NS_OK;
}

/* readonly attribute WebGLsizei drawingBufferHeight; */
NS_IMETHODIMP
WebGLContext::GetDrawingBufferHeight(WebGLsizei *aHeight)
{
    *aHeight = GetDrawingBufferHeight();
    return NS_OK;
}

/* [noscript] attribute WebGLint location; */
NS_IMETHODIMP
WebGLUniformLocation::GetLocation(WebGLint *aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebGLUniformLocation::SetLocation(WebGLint aLocation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute WebGLint size; */
NS_IMETHODIMP
WebGLActiveInfo::GetSize(WebGLint *aSize)
{
    *aSize = mSize;
    return NS_OK;
}

/* readonly attribute WebGLenum type; */
NS_IMETHODIMP
WebGLActiveInfo::GetType(WebGLenum *aType)
{
    *aType = mType;
    return NS_OK;
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP
WebGLActiveInfo::GetName(nsAString & aName)
{
    aName = mName;
    return NS_OK;
}

/* readonly attribute WebGLint rangeMin */
NS_IMETHODIMP
WebGLShaderPrecisionFormat::GetRangeMin(WebGLint *aRangeMin)
{
    *aRangeMin = mRangeMin;
    return NS_OK;
}

/* readonly attribute WebGLint rangeMax */
NS_IMETHODIMP
WebGLShaderPrecisionFormat::GetRangeMax(WebGLint *aRangeMax)
{
    *aRangeMax = mRangeMax;
    return NS_OK;
}

/* readonly attribute WebGLint precision */
NS_IMETHODIMP
WebGLShaderPrecisionFormat::GetPrecision(WebGLint *aPrecision)
{
    *aPrecision = mPrecision;
    return NS_OK;
}

NS_IMETHODIMP
WebGLContext::GetSupportedExtensions(nsIVariant **retval)
{
    Nullable< nsTArray<nsString> > extensions;
    GetSupportedExtensions(extensions);

    if (extensions.IsNull()) {
        *retval = nsnull;
        return NS_OK;
    }

    nsCOMPtr<nsIWritableVariant> wrval = do_CreateInstance("@mozilla.org/variant;1");
    NS_ENSURE_TRUE(wrval, NS_ERROR_FAILURE);

    const nsTArray<nsString>& extList = extensions.Value();

    nsresult rv;
    if (extList.Length() > 0) {
        // nsIVariant can't handle SetAsArray with the AString or
        // DOMString type, so we have to spoon-feed it something it
        // knows how to handle.
        nsTArray<const PRUnichar*> exts(extList.Length());
        for (PRUint32 i = 0; i < extList.Length(); ++i) {
            exts.AppendElement(extList[i].get());
        }
        rv = wrval->SetAsArray(nsIDataType::VTYPE_WCHAR_STR, nsnull,
                               exts.Length(), exts.Elements());
    } else {
        rv = wrval->SetAsEmptyArray();
    }
    if (NS_FAILED(rv))
        return rv;

    *retval = wrval.forget().get();
    return NS_OK;

}

void
WebGLContext::GetSupportedExtensions(Nullable< nsTArray<nsString> > &retval)
{
    retval.SetNull();
    if (!IsContextStable())
        return;
    
    if (mDisableExtensions) {
        return;
    }

    nsTArray<nsString>& arr = retval.SetValue();
    
    if (IsExtensionSupported(OES_texture_float))
        arr.AppendElement(NS_LITERAL_STRING("OES_texture_float"));
    if (IsExtensionSupported(OES_standard_derivatives))
        arr.AppendElement(NS_LITERAL_STRING("OES_standard_derivatives"));
    if (IsExtensionSupported(EXT_texture_filter_anisotropic)) {
        arr.AppendElement(NS_LITERAL_STRING("EXT_texture_filter_anisotropic"));
        arr.AppendElement(NS_LITERAL_STRING("MOZ_EXT_texture_filter_anisotropic"));
    }
    if (IsExtensionSupported(WEBGL_lose_context))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_lose_context"));
    if (IsExtensionSupported(WEBGL_compressed_texture_s3tc))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_s3tc"));
}

NS_IMETHODIMP
WebGLContext::IsContextLost(WebGLboolean *retval)
{
    *retval = mContextStatus != ContextStable;
    return NS_OK;
}
