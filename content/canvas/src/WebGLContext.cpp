/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLObjectModel.h"
#include "WebGLExtensions.h"
#include "WebGLContextUtils.h"

#include "AccessCheck.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsError.h"
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
#include "mozilla/dom/BindingUtils.h"

#include "Layers.h"

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::dom;
using namespace mozilla::layers;

NS_IMETHODIMP
WebGLMemoryPressureObserver::Observe(nsISupports* aSubject,
                                     const char* aTopic,
                                     const PRUnichar* aSomeData)
{
    if (strcmp(aTopic, "memory-pressure"))
        return NS_OK;

    bool wantToLoseContext = true;

    if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING("heap-minimize").get()))
        wantToLoseContext = mContext->mLoseContextOnHeapMinimize;

    if (wantToLoseContext)
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

WebGLContextOptions::WebGLContextOptions()
    : alpha(true), depth(true), stencil(false),
      premultipliedAlpha(true), antialias(true),
      preserveDrawingBuffer(false)
{
    // Set default alpha state based on preference.
    if (Preferences::GetBool("webgl.default-no-alpha", false))
        alpha = false;
}

WebGLContext::WebGLContext()
    : gl(nullptr)
{
    SetIsDOMBinding();

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
    mLoseContextOnHeapMinimize = false;

    mAlreadyGeneratedWarnings = 0;
    mAlreadyWarnedAboutFakeVertexAttrib0 = false;

    mLastUseIndex = 0;

    mMinInUseAttribArrayLengthCached = false;
    mMinInUseAttribArrayLength = 0;
}

WebGLContext::~WebGLContext()
{
    DestroyResourcesAndContext();
    WebGLMemoryMultiReporterWrapper::RemoveWebGLContext(this);
    TerminateContextLossTimer();
    mContextRestorer = nullptr;
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
        mMemoryPressureObserver = nullptr;
    }

    if (!gl)
        return;

    gl->MakeCurrent();

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();
    mBoundArrayBuffer = nullptr;
    mBoundElementArrayBuffer = nullptr;
    mCurrentProgram = nullptr;
    mBoundFramebuffer = nullptr;
    mBoundRenderbuffer = nullptr;

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

    gl = nullptr;
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
    mCanvasElement->InvalidateCanvasContent(nullptr);
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

    WebGLContextOptions newOpts;

    GetBoolFromPropertyBag(aOptions, "stencil", &newOpts.stencil);
    GetBoolFromPropertyBag(aOptions, "depth", &newOpts.depth);
    GetBoolFromPropertyBag(aOptions, "premultipliedAlpha", &newOpts.premultipliedAlpha);
    GetBoolFromPropertyBag(aOptions, "antialias", &newOpts.antialias);
    GetBoolFromPropertyBag(aOptions, "preserveDrawingBuffer", &newOpts.preserveDrawingBuffer);
    GetBoolFromPropertyBag(aOptions, "alpha", &newOpts.alpha);

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
WebGLContext::SetDimensions(int32_t width, int32_t height)
{
    // Early error return cases

    if (width < 0 || height < 0) {
        GenerateWarning("Canvas size is too large (seems like a negative value wrapped)");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!GetCanvas())
        return NS_ERROR_FAILURE;

    // Early success return cases

    GetCanvas()->InvalidateCanvas();

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

    // End of early return cases.
    // At this point we know that we're not just resizing an existing context,
    // we are initializing a new context.

    // if we exceeded either the global or the per-principal limit for WebGL contexts,
    // lose the oldest-used context now to free resources. Note that we can't do that
    // in the WebGLContext constructor as we don't have a canvas element yet there.
    // Here is the right place to do so, as we are about to create the OpenGL context
    // and that is what can fail if we already have too many.
    LoseOldestWebGLContextIfLimitExceeded();

    // Get some prefs for some preferred/overriden things
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), NS_ERROR_FAILURE);

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

    int32_t status;
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (mOptions.antialias &&
        gfxInfo &&
        NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_MSAA, &status))) {
        if (status == nsIGfxInfo::FEATURE_NO_INFO || forceMSAA) {
            uint32_t msaaLevel = Preferences::GetUint("webgl.msaa-level", 2);
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
WebGLContext::Render(gfxContext *ctx, gfxPattern::GraphicsFilter f, uint32_t aFlags)
{
    if (!gl)
        return NS_OK;

    nsRefPtr<gfxImageSurface> surf = new gfxImageSurface(gfxIntSize(mWidth, mHeight),
                                                         gfxASurface::ImageFormatARGB32);
    if (surf->CairoStatus() != 0)
        return NS_ERROR_FAILURE;

    gl->ReadPixelsIntoImageSurface(surf);

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

void WebGLContext::LoseOldestWebGLContextIfLimitExceeded()
{
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    // some mobile devices can't have more than 8 GL contexts overall
    const size_t kMaxWebGLContextsPerPrincipal = 2;
    const size_t kMaxWebGLContexts             = 4;
#else
    const size_t kMaxWebGLContextsPerPrincipal = 16;
    const size_t kMaxWebGLContexts             = 32;
#endif
    MOZ_ASSERT(kMaxWebGLContextsPerPrincipal < kMaxWebGLContexts);

    // it's important to update the index on a new context before losing old contexts,
    // otherwise new unused contexts would all have index 0 and we couldn't distinguish older ones
    // when choosing which one to lose first.
    UpdateLastUseIndex();

    WebGLMemoryMultiReporterWrapper::ContextsArrayType &contexts
      = WebGLMemoryMultiReporterWrapper::Contexts();

    // quick exit path, should cover a majority of cases
    if (contexts.Length() <= kMaxWebGLContextsPerPrincipal) {
        return;
    }

    // note that here by "context" we mean "non-lost context". See the check for
    // IsContextLost() below. Indeed, the point of this function is to maybe lose
    // some currently non-lost context.

    uint64_t oldestIndex = UINT64_MAX;
    uint64_t oldestIndexThisPrincipal = UINT64_MAX;
    const WebGLContext *oldestContext = nullptr;
    const WebGLContext *oldestContextThisPrincipal = nullptr;
    size_t numContexts = 0;
    size_t numContextsThisPrincipal = 0;

    for(size_t i = 0; i < contexts.Length(); ++i) {

        // don't want to lose ourselves.
        if (contexts[i] == this)
            continue;

        if (contexts[i]->IsContextLost())
            continue;

        if (!contexts[i]->GetCanvas()) {
            // Zombie context: the canvas is already destroyed, but something else
            // (typically the compositor) is still holding on to the context.
            // Killing zombies is a no-brainer.
            const_cast<WebGLContext*>(contexts[i])->LoseContext();
            continue;
        }

        numContexts++;
        if (contexts[i]->mLastUseIndex < oldestIndex) {
            oldestIndex = contexts[i]->mLastUseIndex;
            oldestContext = contexts[i];
        }

        nsIPrincipal *ourPrincipal = GetCanvas()->NodePrincipal();
        nsIPrincipal *theirPrincipal = contexts[i]->GetCanvas()->NodePrincipal();
        bool samePrincipal;
        nsresult rv = ourPrincipal->Equals(theirPrincipal, &samePrincipal);
        if (NS_SUCCEEDED(rv) && samePrincipal) {
            numContextsThisPrincipal++;
            if (contexts[i]->mLastUseIndex < oldestIndexThisPrincipal) {
                oldestIndexThisPrincipal = contexts[i]->mLastUseIndex;
                oldestContextThisPrincipal = contexts[i];
            }
        }
    }

    if (numContextsThisPrincipal > kMaxWebGLContextsPerPrincipal) {
        GenerateWarning("Exceeded %d live WebGL contexts for this principal, losing the "
                        "least recently used one.", kMaxWebGLContextsPerPrincipal);
        MOZ_ASSERT(oldestContextThisPrincipal); // if we reach this point, this can't be null
        const_cast<WebGLContext*>(oldestContextThisPrincipal)->LoseContext();
    } else if (numContexts > kMaxWebGLContexts) {
        GenerateWarning("Exceeded %d live WebGL contexts, losing the least recently used one.",
                        kMaxWebGLContexts);
        MOZ_ASSERT(oldestContext); // if we reach this point, this can't be null
        const_cast<WebGLContext*>(oldestContext)->LoseContext();
    }
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
    uint32_t flags = mOptions.premultipliedAlpha ? RenderFlagPremultAlpha : 0;
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

void WebGLContext::UpdateLastUseIndex()
{
    static CheckedInt<uint64_t> sIndex = 0;

    sIndex++;

    // should never happen with 64-bit; trying to handle this would be riskier than
    // not handling it as the handler code would never get exercised.
    if (!sIndex.isValid()) {
        NS_RUNTIMEABORT("Can't believe it's been 2^64 transactions already!");
    }

    mLastUseIndex = sIndex.value();
}

static uint8_t gWebGLLayerUserData;

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

    context->UpdateLastUseIndex();
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
        return nullptr;

    if (!mResetLayer && aOldLayer &&
        aOldLayer->HasUserData(&gWebGLLayerUserData)) {
        NS_ADDREF(aOldLayer);
        return aOldLayer;
    }

    nsRefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nullptr;
    }
    WebGLContextUserData *userData = nullptr;
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
    uint32_t flags = gl->CreationFormat().alpha == 0 ? Layer::CONTENT_OPAQUE : 0;
    canvasLayer->SetContentFlags(flags);
    canvasLayer->Updated();

    mResetLayer = false;

    return canvasLayer.forget().get();
}

void
WebGLContext::GetContextAttributes(Nullable<dom::WebGLContextAttributesInitializer> &retval)
{
    retval.SetNull();
    if (!IsContextStable())
        return;

    dom::WebGLContextAttributes& result = retval.SetValue();

    gl::ContextFormat cf = gl->ActualFormat();
    result.alpha = cf.alpha > 0;
    result.depth = cf.depth > 0;
    result.stencil = cf.stencil > 0;
    result.antialias = cf.samples > 1;
    result.premultipliedAlpha = mOptions.premultipliedAlpha;
    result.preserveDrawingBuffer = mOptions.preserveDrawingBuffer;
}

bool
WebGLContext::IsExtensionEnabled(WebGLExtensionID ext) const {
    return mExtensions.SafeElementAt(ext);
}

/* [noscript] DOMString mozGetUnderlyingParamString(in WebGLenum pname); */
NS_IMETHODIMP
WebGLContext::MozGetUnderlyingParamString(uint32_t pname, nsAString& retval)
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

bool WebGLContext::IsExtensionSupported(JSContext *cx, WebGLExtensionID ext) const
{
    if (mDisableExtensions) {
        return false;
    }

    switch (ext) {
        case OES_standard_derivatives:
        case WEBGL_lose_context:
            // We always support these extensions.
            return true;
        case OES_texture_float:
            return gl->IsExtensionSupported(gl->IsGLES2() ? GLContext::OES_texture_float
                                                          : GLContext::ARB_texture_float);
        case EXT_texture_filter_anisotropic:
            return gl->IsExtensionSupported(GLContext::EXT_texture_filter_anisotropic);
        case WEBGL_compressed_texture_s3tc:
            if (gl->IsExtensionSupported(GLContext::EXT_texture_compression_s3tc)) {
                return true;
            }
            else if (gl->IsExtensionSupported(GLContext::EXT_texture_compression_dxt1) &&
                       gl->IsExtensionSupported(GLContext::ANGLE_texture_compression_dxt3) &&
                       gl->IsExtensionSupported(GLContext::ANGLE_texture_compression_dxt5))
            {
                return true;
            }
            else
            {
                return false;
            }
        case WEBGL_compressed_texture_atc:
            return gl->IsExtensionSupported(GLContext::AMD_compressed_ATC_texture);
        case WEBGL_compressed_texture_pvrtc:
            return gl->IsExtensionSupported(GLContext::IMG_texture_compression_pvrtc);
        case WEBGL_depth_texture:
            if (gl->IsGLES2() && 
                gl->IsExtensionSupported(GLContext::OES_packed_depth_stencil) &&
                gl->IsExtensionSupported(GLContext::OES_depth_texture)) 
            {
                return true;
            }
            else if (!gl->IsGLES2() &&
                     gl->IsExtensionSupported(GLContext::EXT_packed_depth_stencil))
            {
                return true;
            }
            else
            {
                return false;
            }
        case WEBGL_debug_renderer_info:
            return xpc::AccessCheck::isChrome(js::GetContextCompartment(cx));
        default:
            MOZ_ASSERT(false, "should not get there.");
    }

    MOZ_ASSERT(false, "should not get there.");
    return false;
}

static bool
CompareWebGLExtensionName(const nsACString& name, const char *other)
{
    return name.Equals(other, nsCaseInsensitiveCStringComparator());
}

JSObject*
WebGLContext::GetExtension(JSContext *cx, const nsAString& aName, ErrorResult& rv)
{
    if (!IsContextStable())
        return nullptr;

    NS_LossyConvertUTF16toASCII name(aName);

    WebGLExtensionID ext = WebGLExtensionID_unknown_extension;

    // step 1: figure what extension is wanted
    if (CompareWebGLExtensionName(name, "OES_texture_float"))
    {
        ext = OES_texture_float;
    }
    else if (CompareWebGLExtensionName(name, "OES_standard_derivatives"))
    {
        ext = OES_standard_derivatives;
    }
    else if (CompareWebGLExtensionName(name, "EXT_texture_filter_anisotropic"))
    {
        ext = EXT_texture_filter_anisotropic;
    }
    else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_lose_context"))
    {
        ext = WEBGL_lose_context;
    }
    else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_compressed_texture_s3tc"))
    {
        ext = WEBGL_compressed_texture_s3tc;
    }
    else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_compressed_texture_atc"))
    {
        ext = WEBGL_compressed_texture_atc;
    }
    else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_compressed_texture_pvrtc"))
    {
        ext = WEBGL_compressed_texture_pvrtc;
    }
    else if (CompareWebGLExtensionName(name, "WEBGL_debug_renderer_info"))
    {
        ext = WEBGL_debug_renderer_info;
    }
    else if (CompareWebGLExtensionName(name, "MOZ_WEBGL_depth_texture"))
    {
        ext = WEBGL_depth_texture;
    }

    if (ext == WebGLExtensionID_unknown_extension) {
      return nullptr;
    }

    // step 2: check if the extension is supported
    if (!IsExtensionSupported(cx, ext)) {
        return nullptr;
    }

    // step 3: if the extension hadn't been previously been created, create it now, thus enabling it
    if (!IsExtensionEnabled(ext)) {
        WebGLExtensionBase *obj = nullptr;
        switch (ext) {
            case OES_standard_derivatives:
                obj = new WebGLExtensionStandardDerivatives(this);
                break;
            case EXT_texture_filter_anisotropic:
                obj = new WebGLExtensionTextureFilterAnisotropic(this);
                break;
            case WEBGL_lose_context:
                obj = new WebGLExtensionLoseContext(this);
                break;
            case WEBGL_compressed_texture_s3tc:
                obj = new WebGLExtensionCompressedTextureS3TC(this);
                break;
            case WEBGL_compressed_texture_atc:
                obj = new WebGLExtensionCompressedTextureATC(this);
                break;
            case WEBGL_compressed_texture_pvrtc:
                obj = new WebGLExtensionCompressedTexturePVRTC(this);
                break;
            case WEBGL_debug_renderer_info:
                obj = new WebGLExtensionDebugRendererInfo(this);
                break;
            case WEBGL_depth_texture:
                obj = new WebGLExtensionDepthTexture(this);
                break;
            case OES_texture_float:
                obj = new WebGLExtensionTextureFloat(this);
                break;
            default:
                MOZ_ASSERT(false, "should not get there.");
        }
        mExtensions.EnsureLengthAtLeast(ext + 1);
        mExtensions[ext] = obj;
    }

    return WebGLObjectAsJSObject(cx, mExtensions[ext].get(), rv);
}

void
WebGLContext::ForceClearFramebufferWithDefaultValues(uint32_t mask, const nsIntRect& viewportRect)
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
    WebGLenum status = CheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
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
void
WebGLContext::RobustnessTimerCallback(nsITimer* timer)
{
    TerminateContextLossTimer();

    if (!mCanvasElement) {
        // the canvas is gone. That happens when the page was closed before we got
        // this timer event. In this case, there's nothing to do here, just don't crash.
        return;
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
            return;
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
    return;
}

void
WebGLContext::MaybeRestoreContext()
{
    // Don't try to handle it if we already know it's busted.
    if (mContextStatus != ContextStable || gl == nullptr)
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

void
WebGLContext::GetSupportedExtensions(JSContext *cx, Nullable< nsTArray<nsString> > &retval)
{
    retval.SetNull();
    if (!IsContextStable())
        return;

    nsTArray<nsString>& arr = retval.SetValue();

    if (IsExtensionSupported(cx, OES_texture_float))
        arr.AppendElement(NS_LITERAL_STRING("OES_texture_float"));
    if (IsExtensionSupported(cx, OES_standard_derivatives))
        arr.AppendElement(NS_LITERAL_STRING("OES_standard_derivatives"));
    if (IsExtensionSupported(cx, EXT_texture_filter_anisotropic))
        arr.AppendElement(NS_LITERAL_STRING("EXT_texture_filter_anisotropic"));
    if (IsExtensionSupported(cx, WEBGL_lose_context))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_lose_context"));
    if (IsExtensionSupported(cx, WEBGL_compressed_texture_s3tc))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_s3tc"));
    if (IsExtensionSupported(cx, WEBGL_compressed_texture_atc))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_atc"));
    if (IsExtensionSupported(cx, WEBGL_compressed_texture_pvrtc))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_compressed_texture_pvrtc"));
    if (IsExtensionSupported(cx, WEBGL_debug_renderer_info))
        arr.AppendElement(NS_LITERAL_STRING("WEBGL_debug_renderer_info"));
    if (IsExtensionSupported(cx, WEBGL_depth_texture))
        arr.AppendElement(NS_LITERAL_STRING("MOZ_WEBGL_depth_texture"));
}

//
// XPCOM goop
//

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_10(WebGLContext,
  mCanvasElement,
  mExtensions,
  mBound2DTextures,
  mBoundCubeMapTextures,
  mBoundArrayBuffer,
  mBoundElementArrayBuffer,
  mCurrentProgram,
  mBoundFramebuffer,
  mBoundRenderbuffer,
  mAttribBuffers)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMWebGLRenderingContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  // If the exact way we cast to nsISupports here ever changes, fix our
  // PreCreate hook!
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports,
                                   nsICanvasRenderingContextInternal)
NS_INTERFACE_MAP_END
