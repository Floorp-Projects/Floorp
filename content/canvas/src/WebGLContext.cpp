/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGL1Context.h"
#include "WebGLObjectModel.h"
#include "WebGLExtensions.h"
#include "WebGLContextUtils.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLMemoryTracker.h"
#include "WebGLFramebuffer.h"
#include "WebGLVertexArray.h"
#include "WebGLQuery.h"

#include "AccessCheck.h"
#include "nsIConsoleService.h"
#include "nsServiceManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsError.h"
#include "nsIGfxInfo.h"
#include "nsIWidget.h"

#include "nsIVariant.h"

#include "ImageEncoder.h"

#include "gfxContext.h"
#include "gfxPattern.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"

#include "CanvasUtils.h"
#include "nsDisplayList.h"

#include "GLContextProvider.h"
#include "GLContext.h"
#include "ScopedGLHelpers.h"
#include "GLReadTexImageHelper.h"

#include "gfxCrashReporterUtils.h"

#include "nsSVGEffects.h"

#include "prenv.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"

#include "nsIObserverService.h"
#include "nsIDOMEvent.h"
#include "mozilla/Services.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/EnumeratedArrayCycleCollection.h"

#include "Layers.h"

#ifdef MOZ_WIDGET_GONK
#include "mozilla/layers/ShadowLayers.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::layers;

WebGLObserver::WebGLObserver(WebGLContext* aContext)
    : mContext(aContext)
{
}

WebGLObserver::~WebGLObserver()
{
}

void
WebGLObserver::Destroy()
{
    UnregisterMemoryPressureEvent();
    UnregisterVisibilityChangeEvent();
    mContext = nullptr;
}

void
WebGLObserver::RegisterVisibilityChangeEvent()
{
    if (!mContext) {
        return;
    }

    HTMLCanvasElement* canvasElement = mContext->GetCanvas();

    MOZ_ASSERT(canvasElement);

    if (canvasElement) {
        nsIDocument* document = canvasElement->OwnerDoc();

        document->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                         this,
                                         true,
                                         false);
    }
}

void
WebGLObserver::UnregisterVisibilityChangeEvent()
{
    if (!mContext) {
        return;
    }

    HTMLCanvasElement* canvasElement = mContext->GetCanvas();

    if (canvasElement) {
        nsIDocument* document = canvasElement->OwnerDoc();

        document->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"),
                                            this,
                                            true);
    }
}

void
WebGLObserver::RegisterMemoryPressureEvent()
{
    if (!mContext) {
        return;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    MOZ_ASSERT(observerService);

    if (observerService) {
        observerService->AddObserver(this, "memory-pressure", false);
    }
}

void
WebGLObserver::UnregisterMemoryPressureEvent()
{
    if (!mContext) {
        return;
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();

    // Do not assert on observerService here. This might be triggered by
    // the cycle collector at a late enough time, that XPCOM services are
    // no longer available. See bug 1029504.
    if (observerService) {
        observerService->RemoveObserver(this, "memory-pressure");
    }
}

NS_IMETHODIMP
WebGLObserver::Observe(nsISupports* aSubject,
                       const char* aTopic,
                       const char16_t* aSomeData)
{
    if (!mContext || strcmp(aTopic, "memory-pressure")) {
        return NS_OK;
    }

    bool wantToLoseContext = mContext->mLoseContextOnMemoryPressure;

    if (!mContext->mCanLoseContextInForeground &&
        ProcessPriorityManager::CurrentProcessIsForeground())
    {
        wantToLoseContext = false;
    }

    if (wantToLoseContext) {
        mContext->ForceLoseContext();
    }

    return NS_OK;
}

NS_IMETHODIMP
WebGLObserver::HandleEvent(nsIDOMEvent* aEvent)
{
    nsAutoString type;
    aEvent->GetType(type);
    if (!mContext || !type.EqualsLiteral("visibilitychange")) {
        return NS_OK;
    }

    HTMLCanvasElement* canvasElement = mContext->GetCanvas();

    MOZ_ASSERT(canvasElement);

    if (canvasElement && !canvasElement->OwnerDoc()->Hidden()) {
        mContext->ForceRestoreContext();
    }

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
    mShouldPresent = true;
    mResetLayer = true;
    mOptionsFrozen = false;

    mActiveTexture = 0;
    mPixelStoreFlipY = false;
    mPixelStorePremultiplyAlpha = false;
    mPixelStoreColorspaceConversion = BROWSER_DEFAULT_WEBGL;

    mShaderValidation = true;

    mFakeBlackStatus = WebGLContextFakeBlackStatus::NotNeeded;

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
    mFakeVertexAttrib0BufferStatus = WebGLVertexAttrib0Status::Default;

    mViewportX = 0;
    mViewportY = 0;
    mViewportWidth = 0;
    mViewportHeight = 0;

    mScissorTestEnabled = 0;
    mDitherEnabled = 1;
    mRasterizerDiscardEnabled = 0; // OpenGL ES 3.0 spec p244

    // initialize some GL values: we're going to get them from the GL and use them as the sizes of arrays,
    // so in case glGetIntegerv leaves them uninitialized because of a GL bug, we would have very weird crashes.
    mGLMaxVertexAttribs = 0;
    mGLMaxTextureUnits = 0;
    mGLMaxTextureSize = 0;
    mGLMaxCubeMapTextureSize = 0;
    mGLMaxRenderbufferSize = 0;
    mGLMaxTextureImageUnits = 0;
    mGLMaxVertexTextureImageUnits = 0;
    mGLMaxVaryingVectors = 0;
    mGLMaxFragmentUniformVectors = 0;
    mGLMaxVertexUniformVectors = 0;
    mGLMaxColorAttachments = 1;
    mGLMaxDrawBuffers = 1;
    mGLMaxTransformFeedbackSeparateAttribs = 0;

    // See OpenGL ES 2.0.25 spec, 6.2 State Tables, table 6.13
    mPixelStorePackAlignment = 4;
    mPixelStoreUnpackAlignment = 4;

    WebGLMemoryTracker::AddWebGLContext(this);

    mAllowContextRestore = true;
    mLastLossWasSimulated = false;
    mContextLossTimerRunning = false;
    mRunContextLossTimerAgain = false;
    mContextRestorer = do_CreateInstance("@mozilla.org/timer;1");
    mContextStatus = ContextNotLost;
    mLoseContextOnMemoryPressure = false;
    mCanLoseContextInForeground = true;
    mRestoreWhenVisible = false;

    mAlreadyGeneratedWarnings = 0;
    mAlreadyWarnedAboutFakeVertexAttrib0 = false;
    mAlreadyWarnedAboutViewportLargerThanDest = false;
    mMaxWarnings = Preferences::GetInt("webgl.max-warnings-per-context", 32);
    if (mMaxWarnings < -1)
    {
        GenerateWarning("webgl.max-warnings-per-context size is too large (seems like a negative value wrapped)");
        mMaxWarnings = 0;
    }

    mContextObserver = new WebGLObserver(this);
    MOZ_RELEASE_ASSERT(mContextObserver, "Can't alloc WebGLContextObserver");

    mLastUseIndex = 0;

    InvalidateBufferFetching();

    mBackbufferNeedsClear = true;

    mDisableFragHighP = false;

    mDrawCallsSinceLastFlush = 0;
}

WebGLContext::~WebGLContext()
{
    mContextObserver->Destroy();

    DestroyResourcesAndContext();
    WebGLMemoryTracker::RemoveWebGLContext(this);
    TerminateContextLossTimer();
    mContextRestorer = nullptr;
}

void
WebGLContext::DestroyResourcesAndContext()
{
    mContextObserver->UnregisterMemoryPressureEvent();

    if (!gl)
        return;

    gl->MakeCurrent();

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();
    mBoundArrayBuffer = nullptr;
    mBoundTransformFeedbackBuffer = nullptr;
    mCurrentProgram = nullptr;
    mBoundFramebuffer = nullptr;
    mActiveOcclusionQuery = nullptr;
    mBoundRenderbuffer = nullptr;
    mBoundVertexArray = nullptr;
    mDefaultVertexArray = nullptr;

    while (!mTextures.isEmpty())
        mTextures.getLast()->DeleteOnce();
    while (!mVertexArrays.isEmpty())
        mVertexArrays.getLast()->DeleteOnce();
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
    while (!mQueries.isEmpty())
        mQueries.getLast()->DeleteOnce();

    mBlackOpaqueTexture2D = nullptr;
    mBlackOpaqueTextureCubeMap = nullptr;
    mBlackTransparentTexture2D = nullptr;
    mBlackTransparentTextureCubeMap = nullptr;

    if (mFakeVertexAttrib0BufferObject) {
        gl->fDeleteBuffers(1, &mFakeVertexAttrib0BufferObject);
    }

    // disable all extensions except "WEBGL_lose_context". see bug #927969
    // spec: http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
    for (size_t i = 0; i < size_t(WebGLExtensionID::Max); ++i) {
        WebGLExtensionID extension = WebGLExtensionID(i);

        if (!IsExtensionEnabled(extension) || (extension == WebGLExtensionID::WEBGL_lose_context))
            continue;

        mExtensions[extension]->MarkLost();
        mExtensions[extension] = nullptr;
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

NS_IMETHODIMP
WebGLContext::SetContextOptions(JSContext* aCx, JS::Handle<JS::Value> aOptions)
{
    if (aOptions.isNullOrUndefined() && mOptionsFrozen) {
        return NS_OK;
    }

    WebGLContextAttributes attributes;
    NS_ENSURE_TRUE(attributes.Init(aCx, aOptions), NS_ERROR_UNEXPECTED);

    WebGLContextOptions newOpts;

    newOpts.stencil = attributes.mStencil;
    newOpts.depth = attributes.mDepth;
    newOpts.premultipliedAlpha = attributes.mPremultipliedAlpha;
    newOpts.antialias = attributes.mAntialias;
    newOpts.preserveDrawingBuffer = attributes.mPreserveDrawingBuffer;
    if (attributes.mAlpha.WasPassed()) {
      newOpts.alpha = attributes.mAlpha.Value();
    }

    // enforce that if stencil is specified, we also give back depth
    newOpts.depth |= newOpts.stencil;

    // Don't do antialiasing if we've disabled MSAA.
    if (!gfxPrefs::MSAALevel()) {
      newOpts.antialias = false;
    }

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

#ifdef DEBUG
int32_t
WebGLContext::GetWidth() const
{
  return mWidth;
}

int32_t
WebGLContext::GetHeight() const
{
  return mHeight;
}
#endif

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
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }

    // If we already have a gl context, then we just need to resize it
    if (gl) {
        MakeContextCurrent();

        // If we've already drawn, we should commit the current buffer.
        PresentScreenBuffer();

        // ResizeOffscreen scraps the current prod buffer before making a new one.
        gl->ResizeOffscreen(gfx::IntSize(width, height)); // Doesn't matter if it succeeds (soft-fail)
        // It's unlikely that we'll get a proper-sized context if we recreate if we didn't on resize

        // everything's good, we're done here
        mWidth = gl->OffscreenSize().width;
        mHeight = gl->OffscreenSize().height;
        mResetLayer = true;

        mBackbufferNeedsClear = true;

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

    SurfaceCaps caps;

    caps.color = true;
    caps.alpha = mOptions.alpha;
    caps.depth = mOptions.depth;
    caps.stencil = mOptions.stencil;

    // we should really have this behind a
    // |gfxPlatform::GetPlatform()->GetScreenDepth() == 16| check, but
    // for now it's just behind a pref for testing/evaluation.
    caps.bpp16 = prefer16bit;

    caps.preserve = mOptions.preserveDrawingBuffer;

#ifdef MOZ_WIDGET_GONK
    nsIWidget *docWidget = nsContentUtils::WidgetForDocument(mCanvasElement->OwnerDoc());
    if (docWidget) {
        layers::LayerManager *layerManager = docWidget->GetLayerManager();
        if (layerManager) {
            // XXX we really want "AsSurfaceAllocator" here for generality
            layers::ShadowLayerForwarder *forwarder = layerManager->AsShadowForwarder();
            if (forwarder) {
                caps.surfaceAllocator = static_cast<layers::ISurfaceAllocator*>(forwarder);
            }
        }
    }
#endif

    bool forceMSAA =
        Preferences::GetBool("webgl.msaa-force", false);

    int32_t status;
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    if (mOptions.antialias &&
        gfxInfo &&
        NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_MSAA, &status))) {
        if (status == nsIGfxInfo::FEATURE_STATUS_OK || forceMSAA) {
            caps.antialias = true;
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
            if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
                useOpenGL = false;
            }
        }
#ifdef XP_WIN
        if (NS_SUCCEEDED(gfxInfo->GetFeatureStatus(nsIGfxInfo::FEATURE_WEBGL_ANGLE, &status))) {
            if (status != nsIGfxInfo::FEATURE_STATUS_OK) {
                useANGLE = false;
            }
        }
#endif
    }

#ifdef XP_WIN
    // allow forcing GL and not EGL/ANGLE
    if (PR_GetEnv("MOZ_WEBGL_FORCE_OPENGL")) {
        preferEGL = false;
        useANGLE = false;
        useOpenGL = true;
    }
#endif

    gfxIntSize size(width, height);

#ifdef XP_WIN
    // if we want EGL, try it now
    if (!gl && (preferEGL || useANGLE) && !preferOpenGL) {
        gl = gl::GLContextProviderEGL::CreateOffscreen(size, caps);
        if (!gl || !InitAndValidateGL()) {
            GenerateWarning("Error during ANGLE OpenGL ES initialization");
            return NS_ERROR_FAILURE;
        }
    }
#endif

    // try the default provider, whatever that is
    if (!gl && useOpenGL) {
        gl = gl::GLContextProvider::CreateOffscreen(size, caps);
        if (gl && !InitAndValidateGL()) {
            GenerateWarning("Error during OpenGL initialization");
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
    mViewportWidth = width;
    mViewportHeight = height;
    mResetLayer = true;
    mOptionsFrozen = true;

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
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);

    AssertCachedBindings();
    AssertCachedState();

    // Clear immediately, because we need to present the cleared initial
    // buffer.
    mBackbufferNeedsClear = true;
    ClearBackbufferIfNeeded();

    mShouldPresent = true;

    MOZ_ASSERT(gl->Caps().color == caps.color);
    MOZ_ASSERT(gl->Caps().alpha == caps.alpha);
    MOZ_ASSERT(gl->Caps().depth == caps.depth || !gl->Caps().depth);
    MOZ_ASSERT(gl->Caps().stencil == caps.stencil || !gl->Caps().stencil);
    MOZ_ASSERT(gl->Caps().antialias == caps.antialias || !gl->Caps().antialias);
    MOZ_ASSERT(gl->Caps().preserve == caps.preserve);

    AssertCachedBindings();
    AssertCachedState();

    reporter.SetSuccessful();
    return NS_OK;
}

void
WebGLContext::ClearBackbufferIfNeeded()
{
    if (!mBackbufferNeedsClear)
        return;

#ifdef DEBUG
    gl->MakeCurrent();

    GLuint fb = 0;
    gl->GetUIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &fb);
    MOZ_ASSERT(fb == 0);
#endif

    ClearScreen();

    mBackbufferNeedsClear = false;
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

    WebGLMemoryTracker::ContextsArrayType &contexts
      = WebGLMemoryTracker::Contexts();

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

void
WebGLContext::GetImageBuffer(uint8_t** aImageBuffer, int32_t* aFormat)
{
    *aImageBuffer = nullptr;
    *aFormat = 0;

    // Use GetSurfaceSnapshot() to make sure that appropriate y-flip gets applied
    bool premult;
    RefPtr<SourceSurface> snapshot =
      GetSurfaceSnapshot(mOptions.premultipliedAlpha ? nullptr : &premult);
    if (!snapshot) {
        return;
    }
    MOZ_ASSERT(mOptions.premultipliedAlpha || !premult, "We must get unpremult when we ask for it!");

    RefPtr<DataSourceSurface> dataSurface = snapshot->GetDataSurface();

    DataSourceSurface::MappedSurface map;
    if (!dataSurface->Map(DataSourceSurface::MapType::READ, &map)) {
        return;
    }

    static const fallible_t fallible = fallible_t();
    uint8_t* imageBuffer = new (fallible) uint8_t[mWidth * mHeight * 4];
    if (!imageBuffer) {
        dataSurface->Unmap();
        return;
    }
    memcpy(imageBuffer, map.mData, mWidth * mHeight * 4);

    dataSurface->Unmap();

    int32_t format = imgIEncoder::INPUT_FORMAT_HOSTARGB;
    if (!mOptions.premultipliedAlpha) {
        // We need to convert to INPUT_FORMAT_RGBA, otherwise
        // we are automatically considered premult, and unpremult'd.
        // Yes, it is THAT silly.
        // Except for different lossy conversions by color,
        // we could probably just change the label, and not change the data.
        gfxUtils::ConvertBGRAtoRGBA(imageBuffer, mWidth * mHeight * 4);
        format = imgIEncoder::INPUT_FORMAT_RGBA;
    }

    *aImageBuffer = imageBuffer;
    *aFormat = format;
}

NS_IMETHODIMP
WebGLContext::GetInputStream(const char* aMimeType,
                             const char16_t* aEncoderOptions,
                             nsIInputStream **aStream)
{
    NS_ASSERTION(gl, "GetInputStream on invalid context?");
    if (!gl)
        return NS_ERROR_FAILURE;

    nsCString enccid("@mozilla.org/image/encoder;2?type=");
    enccid += aMimeType;
    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(enccid.get());
    if (!encoder) {
        return NS_ERROR_FAILURE;
    }

    nsAutoArrayPtr<uint8_t> imageBuffer;
    int32_t format = 0;
    GetImageBuffer(getter_Transfers(imageBuffer), &format);
    if (!imageBuffer) {
        return NS_ERROR_FAILURE;
    }

    return ImageEncoder::GetInputStream(mWidth, mHeight, imageBuffer, format,
                                        encoder, aEncoderOptions, aStream);
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
    WebGLContextUserData(HTMLCanvasElement *aContent)
        : mContent(aContent)
    {}

    /* PreTransactionCallback gets called by the Layers code every time the
     * WebGL canvas is going to be composited.
     */
    static void PreTransactionCallback(void* data)
    {
        WebGLContextUserData* userdata = static_cast<WebGLContextUserData*>(data);
        HTMLCanvasElement* canvas = userdata->mContent;
        WebGLContext* context = static_cast<WebGLContext*>(canvas->GetContextAtIndex(0));

        // Present our screenbuffer, if needed.
        context->PresentScreenBuffer();
        context->mDrawCallsSinceLastFlush = 0;
    }

    /** DidTransactionCallback gets called by the Layers code everytime the WebGL canvas gets composite,
      * so it really is the right place to put actions that have to be performed upon compositing
      */
    static void DidTransactionCallback(void* aData)
    {
        WebGLContextUserData *userdata = static_cast<WebGLContextUserData*>(aData);
        HTMLCanvasElement *canvas = userdata->mContent;
        WebGLContext *context = static_cast<WebGLContext*>(canvas->GetContextAtIndex(0));

        // Mark ourselves as no longer invalidated.
        context->MarkContextClean();

        context->UpdateLastUseIndex();
    }

private:
    nsRefPtr<HTMLCanvasElement> mContent;
};

} // end namespace mozilla

already_AddRefed<layers::CanvasLayer>
WebGLContext::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                             CanvasLayer *aOldLayer,
                             LayerManager *aManager)
{
    if (IsContextLost())
        return nullptr;

    if (!mResetLayer && aOldLayer &&
        aOldLayer->HasUserData(&gWebGLLayerUserData)) {
        nsRefPtr<layers::CanvasLayer> ret = aOldLayer;
        return ret.forget();
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
      canvasLayer->SetPreTransactionCallback(
              WebGLContextUserData::PreTransactionCallback, userData);
    }
    canvasLayer->SetUserData(&gWebGLLayerUserData, userData);

    CanvasLayer::Data data;
    data.mGLContext = gl;
    data.mSize = nsIntSize(mWidth, mHeight);
    data.mHasAlpha = gl->Caps().alpha;
    data.mIsGLAlphaPremult = IsPremultAlpha() || !data.mHasAlpha;

    canvasLayer->Initialize(data);
    uint32_t flags = gl->Caps().alpha ? 0 : Layer::CONTENT_OPAQUE;
    canvasLayer->SetContentFlags(flags);
    canvasLayer->Updated();

    mResetLayer = false;

    return canvasLayer.forget();
}

void
WebGLContext::GetContextAttributes(Nullable<dom::WebGLContextAttributes> &retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    dom::WebGLContextAttributes& result = retval.SetValue();

    const PixelBufferFormat& format = gl->GetPixelFormat();

    result.mAlpha.Construct(format.alpha > 0);
    result.mDepth = format.depth > 0;
    result.mStencil = format.stencil > 0;
    result.mAntialias = format.samples > 1;
    result.mPremultipliedAlpha = mOptions.premultipliedAlpha;
    result.mPreserveDrawingBuffer = mOptions.preserveDrawingBuffer;
}

/* [noscript] DOMString mozGetUnderlyingParamString(in GLenum pname); */
NS_IMETHODIMP
WebGLContext::MozGetUnderlyingParamString(uint32_t pname, nsAString& retval)
{
    if (IsContextLost())
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

void
WebGLContext::ClearScreen()
{
    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = {false};

    MakeContextCurrent();
    ScopedBindFramebuffer autoFB(gl, 0);

    GLbitfield clearMask = LOCAL_GL_COLOR_BUFFER_BIT;
    if (mOptions.depth)
        clearMask |= LOCAL_GL_DEPTH_BUFFER_BIT;
    if (mOptions.stencil)
        clearMask |= LOCAL_GL_STENCIL_BUFFER_BIT;

    colorAttachmentsMask[0] = true;

    ForceClearFramebufferWithDefaultValues(clearMask, colorAttachmentsMask);
}

void
WebGLContext::ForceClearFramebufferWithDefaultValues(GLbitfield mask, const bool colorAttachmentsMask[kMaxColorAttachments])
{
    MakeContextCurrent();

    bool initializeColorBuffer = 0 != (mask & LOCAL_GL_COLOR_BUFFER_BIT);
    bool initializeDepthBuffer = 0 != (mask & LOCAL_GL_DEPTH_BUFFER_BIT);
    bool initializeStencilBuffer = 0 != (mask & LOCAL_GL_STENCIL_BUFFER_BIT);
    bool drawBuffersIsEnabled = IsExtensionEnabled(WebGLExtensionID::WEBGL_draw_buffers);
    bool shouldOverrideDrawBuffers = false;

    GLenum currentDrawBuffers[WebGLContext::kMaxColorAttachments];

    // Fun GL fact: No need to worry about the viewport here, glViewport is just
    // setting up a coordinates transformation, it doesn't affect glClear at all.
    AssertCachedState(); // Can't check cached bindings, as we could
                         // have a different FB bound temporarily.

    // Prepare GL state for clearing.
    gl->fDisable(LOCAL_GL_SCISSOR_TEST);

    if (initializeColorBuffer) {

        if (drawBuffersIsEnabled) {

            GLenum drawBuffersCommand[WebGLContext::kMaxColorAttachments] = { LOCAL_GL_NONE };

            for(int32_t i = 0; i < mGLMaxDrawBuffers; i++) {
                GLint temp;
                gl->fGetIntegerv(LOCAL_GL_DRAW_BUFFER0 + i, &temp);
                currentDrawBuffers[i] = temp;

                if (colorAttachmentsMask[i]) {
                    drawBuffersCommand[i] = LOCAL_GL_COLOR_ATTACHMENT0 + i;
                }
                if (currentDrawBuffers[i] != drawBuffersCommand[i])
                    shouldOverrideDrawBuffers = true;
            }
            // calling draw buffers can cause resolves on adreno drivers so
            // we try to avoid calling it
            if (shouldOverrideDrawBuffers)
                gl->fDrawBuffers(mGLMaxDrawBuffers, drawBuffersCommand);
        }

        gl->fColorMask(1, 1, 1, 1);
        gl->fClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    }

    if (initializeDepthBuffer) {
        gl->fDepthMask(1);
        gl->fClearDepth(1.0f);
    }

    if (initializeStencilBuffer) {
        // "The clear operation always uses the front stencil write mask
        //  when clearing the stencil buffer."
        gl->fStencilMaskSeparate(LOCAL_GL_FRONT, 0xffffffff);
        gl->fStencilMaskSeparate(LOCAL_GL_BACK,  0xffffffff);
        gl->fClearStencil(0);
    }

    if (mRasterizerDiscardEnabled) {
        gl->fDisable(LOCAL_GL_RASTERIZER_DISCARD);
    }

    // Do the clear!
    gl->fClear(mask);

    // And reset!
    if (mScissorTestEnabled)
        gl->fEnable(LOCAL_GL_SCISSOR_TEST);

    if (mRasterizerDiscardEnabled) {
        gl->fEnable(LOCAL_GL_RASTERIZER_DISCARD);
    }

    // Restore GL state after clearing.
    if (initializeColorBuffer) {
        if (shouldOverrideDrawBuffers) {
            gl->fDrawBuffers(mGLMaxDrawBuffers, currentDrawBuffers);
        }

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
        gl->fStencilMaskSeparate(LOCAL_GL_BACK,  mStencilWriteMaskBack);
        gl->fClearStencil(mStencilClearValue);
    }
}

// For an overview of how WebGL compositing works, see:
// https://wiki.mozilla.org/Platform/GFX/WebGL/Compositing
bool
WebGLContext::PresentScreenBuffer()
{
    if (IsContextLost()) {
        return false;
    }

    if (!mShouldPresent) {
        return false;
    }

    gl->MakeCurrent();
    MOZ_ASSERT(!mBackbufferNeedsClear);
    if (!gl->PublishFrame()) {
        this->ForceLoseContext();
        return false;
    }

    if (!mOptions.preserveDrawingBuffer) {
        mBackbufferNeedsClear = true;
    }

    mShouldPresent = false;

    return true;
}

void
WebGLContext::DummyFramebufferOperation(const char *info)
{
    GLenum status = CheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        ErrorInvalidFramebufferOperation("%s: incomplete framebuffer", info);
}

static bool
CheckContextLost(GLContext* gl, bool* out_isGuilty)
{
    MOZ_ASSERT(gl);
    MOZ_ASSERT(out_isGuilty);

    bool isEGL = gl->GetContextType() == gl::GLContextType::EGL;

    GLenum resetStatus = LOCAL_GL_NO_ERROR;
    if (gl->HasRobustness()) {
        gl->MakeCurrent();
        resetStatus = gl->fGetGraphicsResetStatus();
    } else if (isEGL) {
        // Simulate a ARB_robustness guilty context loss for when we
        // get an EGL_CONTEXT_LOST error. It may not actually be guilty,
        // but we can't make any distinction.
        if (!gl->MakeCurrent(true) && gl->IsContextLost()) {
            resetStatus = LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB;
        }
    }

    if (resetStatus == LOCAL_GL_NO_ERROR) {
        *out_isGuilty = false;
        return false;
    }

    // Assume guilty unless we find otherwise!
    bool isGuilty = true;
    switch (resetStatus) {
    case LOCAL_GL_INNOCENT_CONTEXT_RESET_ARB:
        // Either nothing wrong, or not our fault.
        isGuilty = false;
        break;
    case LOCAL_GL_GUILTY_CONTEXT_RESET_ARB:
        NS_WARNING("WebGL content on the page definitely caused the graphics"
                   " card to reset.");
        break;
    case LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB:
        NS_WARNING("WebGL content on the page might have caused the graphics"
                   " card to reset");
        // If we can't tell, assume guilty.
        break;
    default:
        MOZ_ASSERT(false, "Unreachable.");
        // If we do get here, let's pretend to be guilty as an escape plan.
        break;
    }

    if (isGuilty) {
        NS_WARNING("WebGL context on this page is considered guilty, and will"
                   " not be restored.");
    }

    *out_isGuilty = isGuilty;
    return true;
}

bool
WebGLContext::TryToRestoreContext()
{
    if (NS_FAILED(SetDimensions(mWidth, mHeight)))
        return false;

    return true;
}

class UpdateContextLossStatusTask : public nsRunnable
{
    nsRefPtr<WebGLContext> mContext;

public:
    UpdateContextLossStatusTask(WebGLContext* context)
        : mContext(context)
    {
    }

    NS_IMETHOD Run() {
        mContext->UpdateContextLossStatus();

        return NS_OK;
    }
};

void
WebGLContext::EnqueueUpdateContextLossStatus()
{
    nsCOMPtr<nsIRunnable> task = new UpdateContextLossStatusTask(this);
    NS_DispatchToCurrentThread(task);
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
WebGLContext::UpdateContextLossStatus()
{
    if (!mCanvasElement) {
        // the canvas is gone. That happens when the page was closed before we got
        // this timer event. In this case, there's nothing to do here, just don't crash.
        return;
    }
    if (mContextStatus == ContextNotLost) {
        // We don't know that we're lost, but we might be, so we need to
        // check. If we're guilty, don't allow restores, though.

        bool isGuilty = true;
        MOZ_ASSERT(gl); // Shouldn't be missing gl if we're NotLost.
        bool isContextLost = CheckContextLost(gl, &isGuilty);

        if (isContextLost) {
            if (isGuilty)
                mAllowContextRestore = false;

            ForceLoseContext();
        }

        // Fall through.
    }

    if (mContextStatus == ContextLostAwaitingEvent) {
        // The context has been lost and we haven't yet triggered the
        // callback, so do that now.

        bool useDefaultHandler;
        nsContentUtils::DispatchTrustedEvent(mCanvasElement->OwnerDoc(),
                                             static_cast<nsIDOMHTMLCanvasElement*>(mCanvasElement),
                                             NS_LITERAL_STRING("webglcontextlost"),
                                             true,
                                             true,
                                             &useDefaultHandler);
        // We sent the callback, so we're just 'regular lost' now.
        mContextStatus = ContextLost;
        // If we're told to use the default handler, it means the script
        // didn't bother to handle the event. In this case, we shouldn't
        // auto-restore the context.
        if (useDefaultHandler)
            mAllowContextRestore = false;

        // Fall through.
    }

    if (mContextStatus == ContextLost) {
        // Context is lost, and we've already sent the callback. We
        // should try to restore the context if we're both allowed to,
        // and supposed to.

        // Are we allowed to restore the context?
        if (!mAllowContextRestore)
            return;

        // If we're only simulated-lost, we shouldn't auto-restore, and
        // instead we should wait for restoreContext() to be called.
        if (mLastLossWasSimulated)
            return;

        // Restore when the app is visible
        if (mRestoreWhenVisible)
            return;

        ForceRestoreContext();
        return;
    }

    if (mContextStatus == ContextLostAwaitingRestore) {
        // Context is lost, but we should try to restore it.

        if (!mAllowContextRestore) {
            // We might decide this after thinking we'd be OK restoring
            // the context, so downgrade.
            mContextStatus = ContextLost;
            return;
        }

        if (!TryToRestoreContext()) {
            // Failed to restore. Try again later.
            RunContextLossTimer();
            return;
        }

        // Revival!
        mContextStatus = ContextNotLost;
        nsContentUtils::DispatchTrustedEvent(mCanvasElement->OwnerDoc(),
                                             static_cast<nsIDOMHTMLCanvasElement*>(mCanvasElement),
                                             NS_LITERAL_STRING("webglcontextrestored"),
                                             true,
                                             true);
        mEmitContextLostErrorOnce = true;
        return;
    }
}

void
WebGLContext::ForceLoseContext(bool simulateLosing)
{
    printf_stderr("WebGL(%p)::ForceLoseContext\n", this);
    MOZ_ASSERT(!IsContextLost());
    mContextStatus = ContextLostAwaitingEvent;
    mContextLostErrorSet = false;

    // Burn it all!
    DestroyResourcesAndContext();
    mLastLossWasSimulated = simulateLosing;

    // Register visibility change observer to defer the context restoring.
    // Restore the context when the app is visible.
    if (mRestoreWhenVisible && !mLastLossWasSimulated) {
        mContextObserver->RegisterVisibilityChangeEvent();
    }

    // Queue up a task, since we know the status changed.
    EnqueueUpdateContextLossStatus();
}

void
WebGLContext::ForceRestoreContext()
{
    printf_stderr("WebGL(%p)::ForceRestoreContext\n", this);
    mContextStatus = ContextLostAwaitingRestore;
    mAllowContextRestore = true; // Hey, you did say 'force'.

    mContextObserver->UnregisterVisibilityChangeEvent();

    // Queue up a task, since we know the status changed.
    EnqueueUpdateContextLossStatus();
}

void
WebGLContext::MakeContextCurrent() const { gl->MakeCurrent(); }

mozilla::TemporaryRef<mozilla::gfx::SourceSurface>
WebGLContext::GetSurfaceSnapshot(bool* aPremultAlpha)
{
    if (!gl)
        return nullptr;

    RefPtr<DataSourceSurface> surf = Factory::CreateDataSourceSurfaceWithStride(IntSize(mWidth, mHeight),
                                                                                SurfaceFormat::B8G8R8A8,
                                                                                mWidth * 4);
    if (!surf) {
        return nullptr;
    }

    gl->MakeCurrent();
    {
        ScopedBindFramebuffer autoFB(gl, 0);
        ClearBackbufferIfNeeded();
        ReadPixelsIntoDataSurface(gl, surf);
    }

    if (aPremultAlpha) {
        *aPremultAlpha = true;
    }
    bool srcPremultAlpha = mOptions.premultipliedAlpha;
    if (!srcPremultAlpha) {
        if (aPremultAlpha) {
            *aPremultAlpha = false;
        } else {
            gfxUtils::PremultiplyDataSurface(surf, surf);
        }
    }

    RefPtr<DrawTarget> dt =
        Factory::CreateDrawTarget(BackendType::CAIRO,
                                  IntSize(mWidth, mHeight),
                                  SurfaceFormat::B8G8R8A8);

    if (!dt) {
        return nullptr;
    }

    Matrix m;
    m.Translate(0.0, mHeight);
    m.Scale(1.0, -1.0);
    dt->SetTransform(m);

    dt->DrawSurface(surf,
                    Rect(0, 0, mWidth, mHeight),
                    Rect(0, 0, mWidth, mHeight),
                    DrawSurfaceOptions(),
                    DrawOptions(1.0f, CompositionOp::OP_SOURCE));

    return dt->Snapshot();
}

//
// XPCOM goop
//

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLContext,
  mCanvasElement,
  mExtensions,
  mBound2DTextures,
  mBoundCubeMapTextures,
  mBoundArrayBuffer,
  mBoundTransformFeedbackBuffer,
  mCurrentProgram,
  mBoundFramebuffer,
  mBoundRenderbuffer,
  mBoundVertexArray,
  mDefaultVertexArray,
  mActiveOcclusionQuery,
  mActiveTransformFeedbackQuery)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLContext)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMWebGLRenderingContext)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  // If the exact way we cast to nsISupports here ever changes, fix our
  // ToSupports() method.
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMWebGLRenderingContext)
NS_INTERFACE_MAP_END
