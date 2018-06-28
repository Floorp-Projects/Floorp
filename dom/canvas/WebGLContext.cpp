/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include <algorithm>
#include <queue>

#include "AccessCheck.h"
#include "gfxContext.h"
#include "gfxCrashReporterUtils.h"
#include "gfxPattern.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "MozFramebuffer.h"
#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "GLReadTexImageHelper.h"
#include "GLScreenBuffer.h"
#include "ImageContainer.h"
#include "ImageEncoder.h"
#include "Layers.h"
#include "LayerUserData.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/WebGLContextEvent.h"
#include "mozilla/EnumeratedArrayCycleCollection.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsError.h"
#include "nsIClassInfoImpl.h"
#include "nsIConsoleService.h"
#include "nsIGfxInfo.h"
#include "nsIObserverService.h"
#include "nsIVariant.h"
#include "nsIWidget.h"
#include "nsIXPConnect.h"
#include "nsServiceManagerUtils.h"
#include "SVGObserverUtils.h"
#include "prenv.h"
#include "ScopedGLHelpers.h"
#include "VRManagerChild.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"

// Local
#include "CanvasUtils.h"
#include "WebGL1Context.h"
#include "WebGLActiveInfo.h"
#include "WebGLBuffer.h"
#include "WebGLContextLossHandler.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "WebGLFramebuffer.h"
#include "WebGLMemoryTracker.h"
#include "WebGLObjectModel.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLSync.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArray.h"
#include "WebGLVertexAttribData.h"

#ifdef MOZ_WIDGET_COCOA
#include "nsCocoaFeatures.h"
#endif

#ifdef XP_WIN
#include "WGLLibrary.h"
#endif

// Generated
#include "mozilla/dom/WebGLRenderingContextBinding.h"


namespace mozilla {

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::layers;

WebGLContextOptions::WebGLContextOptions()
{
    // Set default alpha state based on preference.
    if (gfxPrefs::WebGLDefaultNoAlpha())
        alpha = false;
}

bool
WebGLContextOptions::operator==(const WebGLContextOptions& r) const
{
    bool eq = true;
    eq &= (alpha == r.alpha);
    eq &= (depth == r.depth);
    eq &= (stencil == r.stencil);
    eq &= (premultipliedAlpha == r.premultipliedAlpha);
    eq &= (antialias == r.antialias);
    eq &= (preserveDrawingBuffer == r.preserveDrawingBuffer);
    eq &= (failIfMajorPerformanceCaveat == r.failIfMajorPerformanceCaveat);
    eq &= (powerPreference == r.powerPreference);
    return eq;
}

WebGLContext::WebGLContext()
    : WebGLContextUnchecked(nullptr)
    , mMaxPerfWarnings(gfxPrefs::WebGLMaxPerfWarnings())
    , mNumPerfWarnings(0)
    , mMaxAcceptableFBStatusInvals(gfxPrefs::WebGLMaxAcceptableFBStatusInvals())
    , mDataAllocGLCallCount(0)
    , mBypassShaderValidation(false)
    , mEmptyTFO(0)
    , mContextLossHandler(this)
    , mNeedsFakeNoAlpha(false)
    , mNeedsFakeNoDepth(false)
    , mNeedsFakeNoStencil(false)
    , mAllowFBInvalidation(gfxPrefs::WebGLFBInvalidation())
    , mMsaaSamples(gfxPrefs::WebGLMsaaSamples())
{
    mGeneration = 0;
    mInvalidated = false;
    mCapturedFrameInvalidated = false;
    mShouldPresent = true;
    mResetLayer = true;
    mOptionsFrozen = false;
    mDisableExtensions = false;
    mIsMesa = false;
    mEmitContextLostErrorOnce = false;
    mWebGLError = 0;
    mUnderlyingGLError = 0;

    mContextLostErrorSet = false;

    mViewportX = 0;
    mViewportY = 0;
    mViewportWidth = 0;
    mViewportHeight = 0;

    mDitherEnabled = 1;
    mRasterizerDiscardEnabled = 0; // OpenGL ES 3.0 spec p244
    mScissorTestEnabled = 0;
    mStencilTestEnabled = 0;

    if (NS_IsMainThread()) {
        // XXX mtseng: bug 709490, not thread safe
        WebGLMemoryTracker::AddWebGLContext(this);
    }

    mAllowContextRestore = true;
    mLastLossWasSimulated = false;
    mContextStatus = ContextNotLost;
    mLoseContextOnMemoryPressure = false;
    mCanLoseContextInForeground = true;
    mRestoreWhenVisible = false;

    mAlreadyGeneratedWarnings = 0;
    mAlreadyWarnedAboutFakeVertexAttrib0 = false;
    mAlreadyWarnedAboutViewportLargerThanDest = false;

    mMaxWarnings = gfxPrefs::WebGLMaxWarningsPerContext();
    if (mMaxWarnings < -1) {
        GenerateWarning("webgl.max-warnings-per-context size is too large (seems like a negative value wrapped)");
        mMaxWarnings = 0;
    }

    mLastUseIndex = 0;

    mDisableFragHighP = false;

    mDrawCallsSinceLastFlush = 0;
}

WebGLContext::~WebGLContext()
{
    RemovePostRefreshObserver();

    DestroyResourcesAndContext();
    if (NS_IsMainThread()) {
        // XXX mtseng: bug 709490, not thread safe
        WebGLMemoryTracker::RemoveWebGLContext(this);
    }
}

template<typename T>
void
ClearLinkedList(LinkedList<T>& list)
{
    while (!list.isEmpty()) {
        list.getLast()->DeleteOnce();
    }
}

void
WebGLContext::DestroyResourcesAndContext()
{
    if (!gl)
        return;

    mDefaultFB = nullptr;
    mResolvedDefaultFB = nullptr;

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();
    mBound3DTextures.Clear();
    mBound2DArrayTextures.Clear();
    mBoundSamplers.Clear();
    mBoundArrayBuffer = nullptr;
    mBoundCopyReadBuffer = nullptr;
    mBoundCopyWriteBuffer = nullptr;
    mBoundPixelPackBuffer = nullptr;
    mBoundPixelUnpackBuffer = nullptr;
    mBoundTransformFeedbackBuffer = nullptr;
    mBoundUniformBuffer = nullptr;
    mCurrentProgram = nullptr;
    mActiveProgramLinkInfo = nullptr;
    mBoundDrawFramebuffer = nullptr;
    mBoundReadFramebuffer = nullptr;
    mBoundRenderbuffer = nullptr;
    mBoundVertexArray = nullptr;
    mDefaultVertexArray = nullptr;
    mBoundTransformFeedback = nullptr;
    mDefaultTransformFeedback = nullptr;

    mQuerySlot_SamplesPassed = nullptr;
    mQuerySlot_TFPrimsWritten = nullptr;
    mQuerySlot_TimeElapsed = nullptr;

    mIndexedUniformBufferBindings.clear();

    if (mAvailabilityRunnable) {
        mAvailabilityRunnable->Run();
    }

    //////

    ClearLinkedList(mBuffers);
    ClearLinkedList(mFramebuffers);
    ClearLinkedList(mPrograms);
    ClearLinkedList(mQueries);
    ClearLinkedList(mRenderbuffers);
    ClearLinkedList(mSamplers);
    ClearLinkedList(mShaders);
    ClearLinkedList(mSyncs);
    ClearLinkedList(mTextures);
    ClearLinkedList(mTransformFeedbacks);
    ClearLinkedList(mVertexArrays);

    //////

    if (mEmptyTFO) {
        gl->fDeleteTransformFeedbacks(1, &mEmptyTFO);
        mEmptyTFO = 0;
    }

    //////

    mFakeBlack_2D_0000       = nullptr;
    mFakeBlack_2D_0001       = nullptr;
    mFakeBlack_CubeMap_0000  = nullptr;
    mFakeBlack_CubeMap_0001  = nullptr;
    mFakeBlack_3D_0000       = nullptr;
    mFakeBlack_3D_0001       = nullptr;
    mFakeBlack_2D_Array_0000 = nullptr;
    mFakeBlack_2D_Array_0001 = nullptr;

    if (mFakeVertexAttrib0BufferObject) {
        gl->fDeleteBuffers(1, &mFakeVertexAttrib0BufferObject);
        mFakeVertexAttrib0BufferObject = 0;
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
    if (GLContext::ShouldSpew()) {
        printf_stderr("--- WebGL context destroyed: %p\n", gl.get());
    }

    MOZ_ASSERT(gl);
    gl->MarkDestroyed();
    mGL_OnlyClearInDestroyResourcesAndContext = nullptr;
    MOZ_ASSERT(!gl);
}

void
WebGLContext::Invalidate()
{
    if (!mCanvasElement)
        return;

    mCapturedFrameInvalidated = true;

    if (mInvalidated)
        return;

    SVGObserverUtils::InvalidateDirectRenderingObservers(mCanvasElement);

    mInvalidated = true;
    mCanvasElement->InvalidateCanvasContent(nullptr);
}

void
WebGLContext::OnVisibilityChange()
{
    if (!IsContextLost()) {
        return;
    }

    if (!mRestoreWhenVisible || mLastLossWasSimulated) {
        return;
    }

    ForceRestoreContext();
}

void
WebGLContext::OnMemoryPressure()
{
    bool shouldLoseContext = mLoseContextOnMemoryPressure;

    if (!mCanLoseContextInForeground &&
        ProcessPriorityManager::CurrentProcessIsForeground())
    {
        shouldLoseContext = false;
    }

    if (shouldLoseContext)
        ForceLoseContext();
}

//
// nsICanvasRenderingContextInternal
//

NS_IMETHODIMP
WebGLContext::SetContextOptions(JSContext* cx, JS::Handle<JS::Value> options,
                                ErrorResult& aRvForDictionaryInit)
{
    if (options.isNullOrUndefined() && mOptionsFrozen)
        return NS_OK;

    WebGLContextAttributes attributes;
    if (!attributes.Init(cx, options)) {
      aRvForDictionaryInit.Throw(NS_ERROR_UNEXPECTED);
      return NS_ERROR_UNEXPECTED;
    }

    WebGLContextOptions newOpts;

    newOpts.stencil = attributes.mStencil;
    newOpts.depth = attributes.mDepth;
    newOpts.premultipliedAlpha = attributes.mPremultipliedAlpha;
    newOpts.antialias = attributes.mAntialias;
    newOpts.preserveDrawingBuffer = attributes.mPreserveDrawingBuffer;
    newOpts.failIfMajorPerformanceCaveat = attributes.mFailIfMajorPerformanceCaveat;
    newOpts.powerPreference = attributes.mPowerPreference;

    if (attributes.mAlpha.WasPassed()) {
        newOpts.alpha = attributes.mAlpha.Value();
    }

    // Don't do antialiasing if we've disabled MSAA.
    if (!gfxPrefs::MSAALevel()) {
        newOpts.antialias = false;
    }

    if (!gfxPrefs::WebGLForceMSAA()) {
        const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();

        nsCString blocklistId;
        if (IsFeatureInBlacklist(gfxInfo, nsIGfxInfo::FEATURE_WEBGL_MSAA, &blocklistId)) {
            GenerateWarning("Disallowing antialiased backbuffers due to blacklisting.");
            mOptions.antialias = false;
        }
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

    if (mOptionsFrozen && !(newOpts == mOptions)) {
        // Error if the options are already frozen, and the ones that were asked for
        // aren't the same as what they were originally.
        return NS_ERROR_FAILURE;
    }

    mOptions = newOpts;
    return NS_OK;
}

/* So there are a number of points of failure here. We might fail based
 * on EGL vs. WGL, or we might fail to alloc a too-large size, or we
 * might not be able to create a context with a certain combo of context
 * creation attribs.
 *
 * We don't want to test the complete fallback matrix. (for now, at
 * least) Instead, attempt creation in this order:
 * 1. By platform API. (e.g. EGL vs. WGL)
 * 2. By context creation attribs.
 * 3. By size.
 *
 * That is, try to create headless contexts based on the platform API.
 * Next, create dummy-sized backbuffers for the contexts with the right
 * caps. Finally, resize the backbuffer to an acceptable size given the
 * requested size.
 */

static bool
IsFeatureInBlacklist(const nsCOMPtr<nsIGfxInfo>& gfxInfo, int32_t feature,
                     nsCString* const out_blacklistId)
{
    int32_t status;
    if (!NS_SUCCEEDED(gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo, feature,
                                                           *out_blacklistId, &status)))
    {
        return false;
    }

    return status != nsIGfxInfo::FEATURE_STATUS_OK;
}

static bool
HasAcceleratedLayers(const nsCOMPtr<nsIGfxInfo>& gfxInfo)
{
    int32_t status;

    nsCString discardFailureId;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS,
                                         discardFailureId,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS,
                                         discardFailureId,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS,
                                         discardFailureId,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS,
                                         discardFailureId,
                                         &status);
    if (status)
        return true;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_OPENGL_LAYERS,
                                         discardFailureId,
                                         &status);
    if (status)
        return true;

    return false;
}

// --

bool
WebGLContext::CreateAndInitGL(bool forceEnabled,
                              std::vector<FailureReason>* const out_failReasons)
{
    // Can't use WebGL in headless mode.
    if (gfxPlatform::IsHeadless()) {
        FailureReason reason;
        reason.info = "Can't use WebGL in headless mode (https://bugzil.la/1375585).";
        out_failReasons->push_back(reason);
        GenerateWarning("%s", reason.info.BeginReading());
        return false;
    }

    // WebGL2 is separately blocked:
    if (IsWebGL2()) {
        const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
        const auto feature = nsIGfxInfo::FEATURE_WEBGL2;

        FailureReason reason;
        if (IsFeatureInBlacklist(gfxInfo, feature, &reason.key)) {
            reason.info = "Refused to create WebGL2 context because of blacklist"
                          " entry: ";
            reason.info.Append(reason.key);
            out_failReasons->push_back(reason);
            GenerateWarning("%s", reason.info.BeginReading());
            return false;
        }
    }

    gl::CreateContextFlags flags = (gl::CreateContextFlags::NO_VALIDATION |
                                    gl::CreateContextFlags::PREFER_ROBUSTNESS);
    bool tryNativeGL = true;
    bool tryANGLE = false;

    if (forceEnabled) {
        flags |= gl::CreateContextFlags::FORCE_ENABLE_HARDWARE;
    }

    if (IsWebGL2()) {
        flags |= gl::CreateContextFlags::PREFER_ES3;
    } else if (!gfxPrefs::WebGL1AllowCoreProfile()) {
        flags |= gl::CreateContextFlags::REQUIRE_COMPAT_PROFILE;
    }

    switch (mOptions.powerPreference) {
    case dom::WebGLPowerPreference::Low_power:
        break;
        
        // Eventually add a heuristic, but for now default to high-performance.
        // We can even make it dynamic by holding on to a ForceDiscreteGPUHelperCGL iff
        // we decide it's a high-performance application:
        // - Non-trivial canvas size
        // - Many draw calls
        // - Same origin with root page (try to stem bleeding from WebGL ads/trackers)
    case dom::WebGLPowerPreference::High_performance:
    default:
        flags |= gl::CreateContextFlags::HIGH_POWER;
        break;
    }

#ifdef XP_MACOSX
    const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    nsString vendorID, deviceID;

    // Avoid crash for Intel HD Graphics 3000 on OSX. (Bug 1413269)
    gfxInfo->GetAdapterVendorID(vendorID);
    gfxInfo->GetAdapterDeviceID(deviceID);
    if (vendorID.EqualsLiteral("0x8086") &&
        (deviceID.EqualsLiteral("0x0116") || deviceID.EqualsLiteral("0x0126")))
    {
        flags |= gl::CreateContextFlags::REQUIRE_COMPAT_PROFILE;
    }
#endif

    // --

    const auto surfaceCaps = [&]() {
        auto ret = gl::SurfaceCaps::ForRGBA();
        ret.premultAlpha = mOptions.premultipliedAlpha;
        ret.preserve = mOptions.preserveDrawingBuffer;

        if (!mOptions.alpha) {
            ret.premultAlpha = true;
        }
        return ret;
    }();

    // --

    const bool useEGL = PR_GetEnv("MOZ_WEBGL_FORCE_EGL");

#ifdef XP_WIN
    tryNativeGL = false;
    tryANGLE = true;

    if (gfxPrefs::WebGLDisableWGL()) {
        tryNativeGL = false;
    }

    if (gfxPrefs::WebGLDisableANGLE() || PR_GetEnv("MOZ_WEBGL_FORCE_OPENGL") || useEGL) {
        tryNativeGL = true;
        tryANGLE = false;
    }
#endif

    if (tryNativeGL && !forceEnabled) {
        const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
        const auto feature = nsIGfxInfo::FEATURE_WEBGL_OPENGL;

        FailureReason reason;
        if (IsFeatureInBlacklist(gfxInfo, feature, &reason.key)) {
            reason.info = "Refused to create native OpenGL context because of blacklist"
                          " entry: ";
            reason.info.Append(reason.key);

            out_failReasons->push_back(reason);

            GenerateWarning("%s", reason.info.BeginReading());
            tryNativeGL = false;
        }
    }

    // --

    typedef gl::GLContextProviderEGL::CreateOffscreen fnCreateOffscreenT;
    const auto fnCreate = [&](const fnCreateOffscreenT fnCreateOffscreen,
                              const char* const info)
    {
        const gfx::IntSize dummySize(1, 1);
        nsCString failureId;
        const RefPtr<GLContext> gl = fnCreateOffscreen(dummySize, surfaceCaps, flags,
                                                       &failureId);
        if (!gl) {
            out_failReasons->push_back(WebGLContext::FailureReason(failureId, info));
        }
        return gl;
    };

    const auto newGL = [&]() -> RefPtr<gl::GLContext> {
        if (tryNativeGL) {
            if (useEGL)
                return fnCreate(gl::GLContextProviderEGL::CreateOffscreen, "useEGL");

            const auto ret = fnCreate(gl::GLContextProvider::CreateOffscreen,
                                      "tryNativeGL");
            if (ret)
                return ret;
        }

        if (tryANGLE) {
            // Force enable alpha channel to make sure ANGLE use correct framebuffer format
            MOZ_ASSERT(surfaceCaps.alpha);
            return fnCreate(gl::GLContextProviderEGL::CreateOffscreen, "tryANGLE");
        }
        return nullptr;
    }();

    if (!newGL) {
        out_failReasons->push_back(FailureReason("FEATURE_FAILURE_WEBGL_EXHAUSTED_DRIVERS",
                                                 "Exhausted GL driver options."));
        return false;
    }

    // --

    FailureReason reason;

    mGL_OnlyClearInDestroyResourcesAndContext = newGL;
    MOZ_RELEASE_ASSERT(gl);
    if (!InitAndValidateGL(&reason)) {
        DestroyResourcesAndContext();
        MOZ_RELEASE_ASSERT(!gl);

        // The fail reason here should be specific enough for now.
        out_failReasons->push_back(reason);
        return false;
    }

    return true;
}

// Fallback for resizes:

bool
WebGLContext::EnsureDefaultFB(const char* const funcName)
{
    if (mDefaultFB) {
        MOZ_ASSERT(mDefaultFB->mSize == mRequestedSize);
        return true;
    }

    const bool depthStencil = mOptions.depth || mOptions.stencil;
    auto attemptSize = mRequestedSize;

    while (attemptSize.width || attemptSize.height) {
        attemptSize.width = std::max(attemptSize.width, 1);
        attemptSize.height = std::max(attemptSize.height, 1);

        [&]() {
            if (mOptions.antialias) {
                MOZ_ASSERT(!mDefaultFB);
                mDefaultFB = MozFramebuffer::Create(gl, attemptSize, mMsaaSamples,
                                                    depthStencil);
                if (mDefaultFB)
                    return;
                if (mOptionsFrozen)
                    return;
            }

            MOZ_ASSERT(!mDefaultFB);
            mDefaultFB = MozFramebuffer::Create(gl, attemptSize, 0, depthStencil);
        }();

        if (mDefaultFB)
            break;

        attemptSize.width /= 2;
        attemptSize.height /= 2;
    }

    if (!mDefaultFB) {
        GenerateWarning("%s: Backbuffer resize failed. Losing context.", funcName);
        ForceLoseContext();
        return false;
    }

    mDefaultFB_IsInvalid = true;

    if (mDefaultFB->mSize != mRequestedSize) {
        GenerateWarning("Requested size %dx%d was too large, but resize"
                          " to %dx%d succeeded.",
                        mRequestedSize.width, mRequestedSize.height,
                        mDefaultFB->mSize.width, mDefaultFB->mSize.height);
    }
    mRequestedSize = mDefaultFB->mSize;
    return true;
}

void
WebGLContext::ThrowEvent_WebGLContextCreationError(const nsACString& text)
{
    RefPtr<EventTarget> target = mCanvasElement;
    if (!target && mOffscreenCanvas) {
        target = mOffscreenCanvas;
    } else if (!target) {
        GenerateWarning("Failed to create WebGL context: %s", text.BeginReading());
        return;
    }

    const auto kEventName = NS_LITERAL_STRING("webglcontextcreationerror");

    WebGLContextEventInit eventInit;
    // eventInit.mCancelable = true; // The spec says this, but it's silly.
    eventInit.mStatusMessage = NS_ConvertASCIItoUTF16(text);

    const RefPtr<WebGLContextEvent> event = WebGLContextEvent::Constructor(target,
                                                                           kEventName,
                                                                           eventInit);
    event->SetTrusted(true);

    target->DispatchEvent(*event);

    //////

    GenerateWarning("Failed to create WebGL context: %s", text.BeginReading());
}

NS_IMETHODIMP
WebGLContext::SetDimensions(int32_t signedWidth, int32_t signedHeight)
{
    if (signedWidth < 0 || signedHeight < 0) {
        if (!gl) {
            Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID,
                                  NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_SIZE"));
        }
        GenerateWarning("Canvas size is too large (seems like a negative value wrapped)");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    uint32_t width = signedWidth;
    uint32_t height = signedHeight;

    // Early success return cases

    // May have a OffscreenCanvas instead of an HTMLCanvasElement
    if (GetCanvas())
        GetCanvas()->InvalidateCanvas();

    // Zero-sized surfaces can cause problems.
    if (width == 0)
        width = 1;

    if (height == 0)
        height = 1;

    // If we already have a gl context, then we just need to resize it
    if (gl) {
        if (uint32_t(mRequestedSize.width) == width &&
            uint32_t(mRequestedSize.height) == height)
        {
            return NS_OK;
        }

        if (IsContextLost())
            return NS_OK;

        // If we've already drawn, we should commit the current buffer.
        PresentScreenBuffer();

        if (IsContextLost()) {
            GenerateWarning("WebGL context was lost due to swap failure.");
            return NS_OK;
        }

        // Kill our current default fb(s), for later lazy allocation.
        mRequestedSize = {width, height};
        mDefaultFB = nullptr;

        mResetLayer = true;
        return NS_OK;
    }

    nsCString failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_UNKOWN");
    auto autoTelemetry = mozilla::MakeScopeExit([&] {
        Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID,
                              failureId);
    });

    // End of early return cases.
    // At this point we know that we're not just resizing an existing context,
    // we are initializing a new context.

    // if we exceeded either the global or the per-principal limit for WebGL contexts,
    // lose the oldest-used context now to free resources. Note that we can't do that
    // in the WebGLContext constructor as we don't have a canvas element yet there.
    // Here is the right place to do so, as we are about to create the OpenGL context
    // and that is what can fail if we already have too many.
    LoseOldestWebGLContextIfLimitExceeded();

    // We're going to create an entirely new context.  If our
    // generation is not 0 right now (that is, if this isn't the first
    // context we're creating), we may have to dispatch a context lost
    // event.

    // If incrementing the generation would cause overflow,
    // don't allow it.  Allowing this would allow us to use
    // resource handles created from older context generations.
    if (!(mGeneration + 1).isValid()) {
        // exit without changing the value of mGeneration
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_TOO_MANY");
        const nsLiteralCString text("Too many WebGL contexts created this run.");
        ThrowEvent_WebGLContextCreationError(text);
        return NS_ERROR_FAILURE;
    }

    // increment the generation number - Do this early because later
    // in CreateOffscreenGL(), "default" objects are created that will
    // pick up the old generation.
    ++mGeneration;

    bool disabled = gfxPrefs::WebGLDisabled();

    // TODO: When we have software webgl support we should use that instead.
    disabled |= gfxPlatform::InSafeMode();

    if (disabled) {
        if (gfxPlatform::InSafeMode()) {
            failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_SAFEMODE");
        } else {
            failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_DISABLED");
        }
        const nsLiteralCString text("WebGL is currently disabled.");
        ThrowEvent_WebGLContextCreationError(text);
        return NS_ERROR_FAILURE;
    }

    if (gfxPrefs::WebGLDisableFailIfMajorPerformanceCaveat()) {
        mOptions.failIfMajorPerformanceCaveat = false;
    }

    if (mOptions.failIfMajorPerformanceCaveat) {
        nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
        if (!HasAcceleratedLayers(gfxInfo)) {
            failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_PERF_CAVEAT");
            const nsLiteralCString text("failIfMajorPerformanceCaveat: Compositor is not"
                                        " hardware-accelerated.");
            ThrowEvent_WebGLContextCreationError(text);
            return NS_ERROR_FAILURE;
        }
    }

    // Alright, now let's start trying.
    bool forceEnabled = gfxPrefs::WebGLForceEnabled();
    ScopedGfxFeatureReporter reporter("WebGL", forceEnabled);

    MOZ_ASSERT(!gl);
    std::vector<FailureReason> failReasons;
    if (!CreateAndInitGL(forceEnabled, &failReasons)) {
        nsCString text("WebGL creation failed: ");
        for (const auto& cur : failReasons) {
            // Don't try to accumulate using an empty key if |cur.key| is empty.
            if (cur.key.IsEmpty()) {
                Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID,
                                      NS_LITERAL_CSTRING("FEATURE_FAILURE_REASON_UNKNOWN"));
            } else {
                Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID, cur.key);
            }

            text.AppendASCII("\n* ");
            text.Append(cur.info);
        }
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_REASON");
        ThrowEvent_WebGLContextCreationError(text);
        return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(gl);

    if (mOptions.failIfMajorPerformanceCaveat) {
        if (gl->IsWARP()) {
            DestroyResourcesAndContext();
            MOZ_ASSERT(!gl);

            failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_PERF_WARP");
            const nsLiteralCString text("failIfMajorPerformanceCaveat: Driver is not"
                                        " hardware-accelerated.");
            ThrowEvent_WebGLContextCreationError(text);
            return NS_ERROR_FAILURE;
        }

#ifdef XP_WIN
        if (gl->GetContextType() == gl::GLContextType::WGL &&
            !gl::sWGLLib.HasDXInterop2())
        {
            DestroyResourcesAndContext();
            MOZ_ASSERT(!gl);

            failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_DXGL_INTEROP2");
            const nsLiteralCString text("Caveat: WGL without DXGLInterop2.");
            ThrowEvent_WebGLContextCreationError(text);
            return NS_ERROR_FAILURE;
        }
#endif
    }

    MOZ_ASSERT(!mDefaultFB);
    mRequestedSize = {width, height};
    if (!EnsureDefaultFB("context initialization")) {
        MOZ_ASSERT(!gl);

        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_BACKBUFFER");
        const nsLiteralCString text("Initializing WebGL backbuffer failed.");
        ThrowEvent_WebGLContextCreationError(text);
        return NS_ERROR_FAILURE;
    }

    if (GLContext::ShouldSpew()) {
        printf_stderr("--- WebGL context created: %p\n", gl.get());
    }

    // Update our internal stuff:

    mOptions.antialias &= bool(mDefaultFB->mSamples);

    if (!mOptions.alpha) {
        // We always have alpha.
        mNeedsFakeNoAlpha = true;
    }

    if (mOptions.depth || mOptions.stencil) {
        // We always have depth+stencil if we have either.
        if (!mOptions.depth) {
            mNeedsFakeNoDepth = true;
        }
        if (!mOptions.stencil) {
            mNeedsFakeNoStencil = true;
        }
    }

    mNeedsFakeNoStencil_UserFBs = false;
#ifdef MOZ_WIDGET_COCOA
    if (!nsCocoaFeatures::IsAtLeastVersion(10, 12) &&
        gl->Vendor() == GLVendor::Intel)
    {
        mNeedsFakeNoStencil_UserFBs = true;
    }
#endif

    mResetLayer = true;
    mOptionsFrozen = true;

    //////
    // Initial setup.

    gl->mImplicitMakeCurrent = true;

    const auto& size = mDefaultFB->mSize;

    mViewportX = mViewportY = 0;
    mViewportWidth = size.width;
    mViewportHeight = size.height;
    gl->fViewport(mViewportX, mViewportY, mViewportWidth, mViewportHeight);

    gl->fScissor(0, 0, size.width, size.height);

    //////
    // Check everything

    AssertCachedBindings();
    AssertCachedGlobalState();

    mShouldPresent = true;

    //////

    reporter.SetSuccessful();

    failureId = NS_LITERAL_CSTRING("SUCCESS");

    gl->ResetSyncCallCount("WebGLContext Initialization");
    return NS_OK;
}

void
WebGLContext::LoseOldestWebGLContextIfLimitExceeded()
{
    const auto maxWebGLContexts = gfxPrefs::WebGLMaxContexts();
    auto maxWebGLContextsPerPrincipal = gfxPrefs::WebGLMaxContextsPerPrincipal();

    // maxWebGLContextsPerPrincipal must be less than maxWebGLContexts
    MOZ_ASSERT(maxWebGLContextsPerPrincipal <= maxWebGLContexts);
    maxWebGLContextsPerPrincipal = std::min(maxWebGLContextsPerPrincipal, maxWebGLContexts);

    if (!NS_IsMainThread()) {
        // XXX mtseng: bug 709490, WebGLMemoryTracker is not thread safe.
        return;
    }

    // it's important to update the index on a new context before losing old contexts,
    // otherwise new unused contexts would all have index 0 and we couldn't distinguish older ones
    // when choosing which one to lose first.
    UpdateLastUseIndex();

    WebGLMemoryTracker::ContextsArrayType& contexts = WebGLMemoryTracker::Contexts();

    // quick exit path, should cover a majority of cases
    if (contexts.Length() <= maxWebGLContextsPerPrincipal)
        return;

    // note that here by "context" we mean "non-lost context". See the check for
    // IsContextLost() below. Indeed, the point of this function is to maybe lose
    // some currently non-lost context.

    uint64_t oldestIndex = UINT64_MAX;
    uint64_t oldestIndexThisPrincipal = UINT64_MAX;
    const WebGLContext* oldestContext = nullptr;
    const WebGLContext* oldestContextThisPrincipal = nullptr;
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

        nsIPrincipal* ourPrincipal = GetCanvas()->NodePrincipal();
        nsIPrincipal* theirPrincipal = contexts[i]->GetCanvas()->NodePrincipal();
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

    if (numContextsThisPrincipal > maxWebGLContextsPerPrincipal) {
        GenerateWarning("Exceeded %u live WebGL contexts for this principal, losing the "
                        "least recently used one.", maxWebGLContextsPerPrincipal);
        MOZ_ASSERT(oldestContextThisPrincipal); // if we reach this point, this can't be null
        const_cast<WebGLContext*>(oldestContextThisPrincipal)->LoseContext();
    } else if (numContexts > maxWebGLContexts) {
        GenerateWarning("Exceeded %u live WebGL contexts, losing the least "
                        "recently used one.", maxWebGLContexts);
        MOZ_ASSERT(oldestContext); // if we reach this point, this can't be null
        const_cast<WebGLContext*>(oldestContext)->LoseContext();
    }
}

UniquePtr<uint8_t[]>
WebGLContext::GetImageBuffer(int32_t* out_format)
{
    *out_format = 0;

    // Use GetSurfaceSnapshot() to make sure that appropriate y-flip gets applied
    gfxAlphaType any;
    RefPtr<SourceSurface> snapshot = GetSurfaceSnapshot(&any);
    if (!snapshot)
        return nullptr;

    RefPtr<DataSourceSurface> dataSurface = snapshot->GetDataSurface();

    return gfxUtils::GetImageBuffer(dataSurface, mOptions.premultipliedAlpha,
                                    out_format);
}

NS_IMETHODIMP
WebGLContext::GetInputStream(const char* mimeType,
                             const char16_t* encoderOptions,
                             nsIInputStream** out_stream)
{
    NS_ASSERTION(gl, "GetInputStream on invalid context?");
    if (!gl)
        return NS_ERROR_FAILURE;

    // Use GetSurfaceSnapshot() to make sure that appropriate y-flip gets applied
    gfxAlphaType any;
    RefPtr<SourceSurface> snapshot = GetSurfaceSnapshot(&any);
    if (!snapshot)
        return NS_ERROR_FAILURE;

    RefPtr<DataSourceSurface> dataSurface = snapshot->GetDataSurface();
    return gfxUtils::GetInputStream(dataSurface, mOptions.premultipliedAlpha, mimeType,
                                    encoderOptions, out_stream);
}

void
WebGLContext::UpdateLastUseIndex()
{
    static CheckedInt<uint64_t> sIndex = 0;

    sIndex++;

    // should never happen with 64-bit; trying to handle this would be riskier than
    // not handling it as the handler code would never get exercised.
    if (!sIndex.isValid())
        MOZ_CRASH("Can't believe it's been 2^64 transactions already!");
    mLastUseIndex = sIndex.value();
}

static uint8_t gWebGLLayerUserData;

class WebGLContextUserData : public LayerUserData
{
public:
    explicit WebGLContextUserData(HTMLCanvasElement* canvas)
        : mCanvas(canvas)
    {}

    /* PreTransactionCallback gets called by the Layers code every time the
     * WebGL canvas is going to be composited.
     */
    static void PreTransactionCallback(void* data) {
        WebGLContext* webgl = static_cast<WebGLContext*>(data);

        // Prepare the context for composition
        webgl->BeginComposition();
    }

    /** DidTransactionCallback gets called by the Layers code everytime the WebGL canvas gets composite,
      * so it really is the right place to put actions that have to be performed upon compositing
      */
    static void DidTransactionCallback(void* data) {
        WebGLContext* webgl = static_cast<WebGLContext*>(data);

        // Clean up the context after composition
        webgl->EndComposition();
    }

private:
    RefPtr<HTMLCanvasElement> mCanvas;
};

already_AddRefed<layers::Layer>
WebGLContext::GetCanvasLayer(nsDisplayListBuilder* builder,
                             Layer* oldLayer,
                             LayerManager* manager)
{
    if (!mResetLayer && oldLayer &&
        oldLayer->HasUserData(&gWebGLLayerUserData))
    {
        RefPtr<layers::Layer> ret = oldLayer;
        return ret.forget();
    }

    RefPtr<CanvasLayer> canvasLayer = manager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nullptr;
    }

    WebGLContextUserData* userData = nullptr;
    if (builder->IsPaintingToWindow() && mCanvasElement) {
        userData = new WebGLContextUserData(mCanvasElement);
    }

    canvasLayer->SetUserData(&gWebGLLayerUserData, userData);

    CanvasRenderer* canvasRenderer = canvasLayer->CreateOrGetCanvasRenderer();
    if (!InitializeCanvasRenderer(builder, canvasRenderer))
      return nullptr;

    uint32_t flags = gl->Caps().alpha ? 0 : Layer::CONTENT_OPAQUE;
    canvasLayer->SetContentFlags(flags);

    mResetLayer = false;

    return canvasLayer.forget();
}

bool
WebGLContext::UpdateWebRenderCanvasData(nsDisplayListBuilder* aBuilder,
                                        WebRenderCanvasData* aCanvasData)
{
  CanvasRenderer* renderer = aCanvasData->GetCanvasRenderer();

  if(!mResetLayer && renderer) {
    return true;
  }

  renderer = aCanvasData->CreateCanvasRenderer();
  if (!InitializeCanvasRenderer(aBuilder, renderer)) {
    // Clear CanvasRenderer of WebRenderCanvasData
    aCanvasData->ClearCanvasRenderer();
    return false;
  }

  MOZ_ASSERT(renderer);
  mResetLayer = false;
  return true;
}

bool
WebGLContext::InitializeCanvasRenderer(nsDisplayListBuilder* aBuilder,
                                       CanvasRenderer* aRenderer)
{
    if (IsContextLost())
        return false;

    CanvasInitializeData data;
    if (aBuilder->IsPaintingToWindow() && mCanvasElement) {
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
        data.mPreTransCallback = WebGLContextUserData::PreTransactionCallback;
        data.mPreTransCallbackData = this;
        data.mDidTransCallback = WebGLContextUserData::DidTransactionCallback;
        data.mDidTransCallbackData = this;
    }

    data.mGLContext = gl;
    data.mSize = DrawingBufferSize("InitializeCanvasRenderer");
    data.mHasAlpha = mOptions.alpha;
    data.mIsGLAlphaPremult = IsPremultAlpha() || !data.mHasAlpha;

    aRenderer->Initialize(data);
    aRenderer->SetDirty();
    return true;
}

layers::LayersBackend
WebGLContext::GetCompositorBackendType() const
{
    if (mCanvasElement) {
        return mCanvasElement->GetCompositorBackendType();
    } else if (mOffscreenCanvas) {
        return mOffscreenCanvas->GetCompositorBackendType();
    }

    return LayersBackend::LAYERS_NONE;
}

nsIDocument*
WebGLContext::GetOwnerDoc() const
{
    MOZ_ASSERT(mCanvasElement);
    if (!mCanvasElement) {
        return nullptr;
    }
    return mCanvasElement->OwnerDoc();
}

void
WebGLContext::Commit()
{
    if (mOffscreenCanvas) {
        mOffscreenCanvas->CommitFrameToCompositor();
    }
}

void
WebGLContext::GetCanvas(Nullable<dom::OwningHTMLCanvasElementOrOffscreenCanvas>& retval)
{
    if (mCanvasElement) {
        MOZ_RELEASE_ASSERT(!mOffscreenCanvas, "GFX: Canvas is offscreen.");

        if (mCanvasElement->IsInNativeAnonymousSubtree()) {
          retval.SetNull();
        } else {
          retval.SetValue().SetAsHTMLCanvasElement() = mCanvasElement;
        }
    } else if (mOffscreenCanvas) {
        retval.SetValue().SetAsOffscreenCanvas() = mOffscreenCanvas;
    } else {
        retval.SetNull();
    }
}

void
WebGLContext::GetContextAttributes(dom::Nullable<dom::WebGLContextAttributes>& retval)
{
    retval.SetNull();
    if (IsContextLost())
        return;

    dom::WebGLContextAttributes& result = retval.SetValue();

    result.mAlpha.Construct(mOptions.alpha);
    result.mDepth = mOptions.depth;
    result.mStencil = mOptions.stencil;
    result.mAntialias = mOptions.antialias;
    result.mPremultipliedAlpha = mOptions.premultipliedAlpha;
    result.mPreserveDrawingBuffer = mOptions.preserveDrawingBuffer;
    result.mFailIfMajorPerformanceCaveat = mOptions.failIfMajorPerformanceCaveat;
    result.mPowerPreference = mOptions.powerPreference;
}

void
WebGLContext::ForceClearFramebufferWithDefaultValues(const GLbitfield clearBits,
                                                     const bool fakeNoAlpha) const
{
    const bool initializeColorBuffer = bool(clearBits & LOCAL_GL_COLOR_BUFFER_BIT);
    const bool initializeDepthBuffer = bool(clearBits & LOCAL_GL_DEPTH_BUFFER_BIT);
    const bool initializeStencilBuffer = bool(clearBits & LOCAL_GL_STENCIL_BUFFER_BIT);

    // Fun GL fact: No need to worry about the viewport here, glViewport is just
    // setting up a coordinates transformation, it doesn't affect glClear at all.
    AssertCachedGlobalState();

    // Prepare GL state for clearing.
    if (mScissorTestEnabled) {
        gl->fDisable(LOCAL_GL_SCISSOR_TEST);
    }

    if (initializeColorBuffer) {
        DoColorMask(0x0f);

        if (fakeNoAlpha) {
            gl->fClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        } else {
            gl->fClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        }
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
    gl->fClear(clearBits);

    // And reset!
    if (mScissorTestEnabled) {
        gl->fEnable(LOCAL_GL_SCISSOR_TEST);
    }

    if (mRasterizerDiscardEnabled) {
        gl->fEnable(LOCAL_GL_RASTERIZER_DISCARD);
    }

    // Restore GL state after clearing.
    if (initializeColorBuffer) {
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

void
WebGLContext::OnEndOfFrame() const
{
   if (gfxPrefs::WebGLSpewFrameAllocs()) {
      GeneratePerfWarning("[webgl.perf.spew-frame-allocs] %" PRIu64 " data allocations this frame.",
                           mDataAllocGLCallCount);
   }
   mDataAllocGLCallCount = 0;
   gl->ResetSyncCallCount("WebGLContext PresentScreenBuffer");
}

void
WebGLContext::BlitBackbufferToCurDriverFB() const
{
    DoColorMask(0x0f);

    if (mScissorTestEnabled) {
        gl->fDisable(LOCAL_GL_SCISSOR_TEST);
    }

    [&]() {
        const auto& size = mDefaultFB->mSize;

        if (gl->IsSupported(GLFeature::framebuffer_blit)) {
            gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, mDefaultFB->mFB);
            gl->fBlitFramebuffer(0, 0, size.width, size.height,
                                 0, 0, size.width, size.height,
                                 LOCAL_GL_COLOR_BUFFER_BIT, LOCAL_GL_NEAREST);
            return;
        }
        if (mDefaultFB->mSamples &&
            gl->IsExtensionSupported(GLContext::APPLE_framebuffer_multisample))
        {
            gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, mDefaultFB->mFB);
            gl->fResolveMultisampleFramebufferAPPLE();
            return;
        }

        gl->BlitHelper()->DrawBlitTextureToFramebuffer(mDefaultFB->ColorTex(), size,
                                                       size);
    }();

    if (mScissorTestEnabled) {
        gl->fEnable(LOCAL_GL_SCISSOR_TEST);
    }
}

// For an overview of how WebGL compositing works, see:
// https://wiki.mozilla.org/Platform/GFX/WebGL/Compositing
bool
WebGLContext::PresentScreenBuffer()
{
    if (IsContextLost())
        return false;

    if (!mShouldPresent)
        return false;

    if (!ValidateAndInitFB("Present", nullptr))
        return false;

    const auto& screen = gl->Screen();
    if (screen->Size() != mDefaultFB->mSize &&
        !screen->Resize(mDefaultFB->mSize))
    {
        GenerateWarning("screen->Resize failed. Losing context.");
        ForceLoseContext();
        return false;
    }

    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
    BlitBackbufferToCurDriverFB();

#ifdef DEBUG
    if (!mOptions.alpha) {
        gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
        uint32_t pixel = 3;
        gl->fReadPixels(0, 0, 1, 1, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, &pixel);
        MOZ_ASSERT((pixel & 0xff000000) == 0xff000000);
    }
#endif

    if (!screen->PublishFrame(screen->Size())) {
        GenerateWarning("PublishFrame failed. Losing context.");
        ForceLoseContext();
        return false;
    }

    if (!mOptions.preserveDrawingBuffer) {
        if (gl->IsSupported(gl::GLFeature::invalidate_framebuffer)) {
            gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mDefaultFB->mFB);
            const GLenum attachments[] = { LOCAL_GL_COLOR_ATTACHMENT0 };
            gl->fInvalidateFramebuffer(LOCAL_GL_FRAMEBUFFER, 1, attachments);
        }
        mDefaultFB_IsInvalid = true;
    }
    mResolvedDefaultFB = nullptr;

    mShouldPresent = false;
    OnEndOfFrame();

    return true;
}

// Prepare the context for capture before compositing
void
WebGLContext::BeginComposition()
{
    // Present our screenbuffer, if needed.
    PresentScreenBuffer();
    mDrawCallsSinceLastFlush = 0;
}

// Clean up the context after captured for compositing
void
WebGLContext::EndComposition()
{
    // Mark ourselves as no longer invalidated.
    MarkContextClean();
    UpdateLastUseIndex();
}

void
WebGLContext::DummyReadFramebufferOperation(const char* funcName)
{
    if (!mBoundReadFramebuffer)
        return; // Infallible.

    const auto status = mBoundReadFramebuffer->CheckFramebufferStatus(funcName);

    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
        ErrorInvalidFramebufferOperation("%s: Framebuffer must be complete.",
                                         funcName);
    }
}

bool
WebGLContext::Has64BitTimestamps() const
{
    // 'sync' provides glGetInteger64v either by supporting ARB_sync, GL3+, or GLES3+.
    return gl->IsSupported(GLFeature::sync);
}

static bool
CheckContextLost(GLContext* gl, bool* const out_isGuilty)
{
    MOZ_ASSERT(gl);
    MOZ_ASSERT(out_isGuilty);

    bool isEGL = gl->GetContextType() == gl::GLContextType::EGL;

    GLenum resetStatus = LOCAL_GL_NO_ERROR;
    if (gl->IsSupported(GLFeature::robustness)) {
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
    if (NS_FAILED(SetDimensions(mRequestedSize.width, mRequestedSize.height)))
        return false;

    return true;
}

void
WebGLContext::RunContextLossTimer()
{
    mContextLossHandler.RunTimer();
}

class UpdateContextLossStatusTask : public CancelableRunnable
{
    RefPtr<WebGLContext> mWebGL;

public:
  explicit UpdateContextLossStatusTask(WebGLContext* webgl)
    : CancelableRunnable("UpdateContextLossStatusTask")
    , mWebGL(webgl)
  {
    }

    NS_IMETHOD Run() override {
        if (mWebGL)
            mWebGL->UpdateContextLossStatus();

        return NS_OK;
    }

    nsresult Cancel() override {
        mWebGL = nullptr;
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
    if (!mCanvasElement && !mOffscreenCanvas) {
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
        const auto kEventName = NS_LITERAL_STRING("webglcontextlost");
        const auto kCanBubble = CanBubble::eYes;
        const auto kIsCancelable = Cancelable::eYes;
        bool useDefaultHandler;

        if (mCanvasElement) {
            nsContentUtils::DispatchTrustedEvent(
                mCanvasElement->OwnerDoc(),
                static_cast<nsIContent*>(mCanvasElement),
                kEventName,
                kCanBubble,
                kIsCancelable,
                &useDefaultHandler);
        } else {
            // OffscreenCanvas case
            RefPtr<Event> event = new Event(mOffscreenCanvas, nullptr, nullptr);
            event->InitEvent(kEventName, kCanBubble, kIsCancelable);
            event->SetTrusted(true);
            useDefaultHandler =
                mOffscreenCanvas->DispatchEvent(*event, CallerType::System,
                                                IgnoreErrors());
        }

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
            mContextLossHandler.RunTimer();
            return;
        }

        // Revival!
        mContextStatus = ContextNotLost;

        if (mCanvasElement) {
            nsContentUtils::DispatchTrustedEvent(
                mCanvasElement->OwnerDoc(),
                static_cast<nsIContent*>(mCanvasElement),
                NS_LITERAL_STRING("webglcontextrestored"),
                CanBubble::eYes,
                Cancelable::eYes);
        } else {
            RefPtr<Event> event = new Event(mOffscreenCanvas, nullptr, nullptr);
            event->InitEvent(NS_LITERAL_STRING("webglcontextrestored"),
                             CanBubble::eYes,
                             Cancelable::eYes);
            event->SetTrusted(true);
            mOffscreenCanvas->DispatchEvent(*event);
        }

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

    // Queue up a task, since we know the status changed.
    EnqueueUpdateContextLossStatus();
}

void
WebGLContext::ForceRestoreContext()
{
    printf_stderr("WebGL(%p)::ForceRestoreContext\n", this);
    mContextStatus = ContextLostAwaitingRestore;
    mAllowContextRestore = true; // Hey, you did say 'force'.

    // Queue up a task, since we know the status changed.
    EnqueueUpdateContextLossStatus();
}

already_AddRefed<mozilla::gfx::SourceSurface>
WebGLContext::GetSurfaceSnapshot(gfxAlphaType* const out_alphaType)
{
    if (!gl)
        return nullptr;

    if (!BindDefaultFBForRead("GetSurfaceSnapshot"))
        return nullptr;

    const auto surfFormat = mOptions.alpha ? SurfaceFormat::B8G8R8A8
                                           : SurfaceFormat::B8G8R8X8;
    const auto& size = mDefaultFB->mSize;
    RefPtr<DataSourceSurface> surf;
    surf = Factory::CreateDataSourceSurfaceWithStride(size, surfFormat, size.width * 4);
    if (NS_WARN_IF(!surf))
        return nullptr;

    ReadPixelsIntoDataSurface(gl, surf);

    gfxAlphaType alphaType;
    if (!mOptions.alpha) {
        alphaType = gfxAlphaType::Opaque;
    } else if (mOptions.premultipliedAlpha) {
        alphaType = gfxAlphaType::Premult;
    } else {
        alphaType = gfxAlphaType::NonPremult;
    }

    if (out_alphaType) {
        *out_alphaType = alphaType;
    } else {
        // Expects Opaque or Premult
        if (alphaType == gfxAlphaType::NonPremult) {
            gfxUtils::PremultiplyDataSurface(surf, surf);
        }
    }

    RefPtr<DrawTarget> dt =
        Factory::CreateDrawTarget(gfxPlatform::GetPlatform()->GetSoftwareBackend(),
                                  size, SurfaceFormat::B8G8R8A8);
    if (!dt)
        return nullptr;

    dt->SetTransform(Matrix::Translation(0.0, size.height).PreScale(1.0, -1.0));

    const gfx::Rect rect{0, 0, float(size.width), float(size.height)};
    dt->DrawSurface(surf, rect, rect, DrawSurfaceOptions(),
                    DrawOptions(1.0f, CompositionOp::OP_SOURCE));

    return dt->Snapshot();
}

void
WebGLContext::DidRefresh()
{
    if (gl) {
        gl->FlushIfHeavyGLCallsSinceLastFlush();
    }
}

////////////////////////////////////////////////////////////////////////////////

gfx::IntSize
WebGLContext::DrawingBufferSize(const char* const funcName)
{
    const gfx::IntSize zeros{0, 0};
    if (IsContextLost())
        return zeros;

    if (!EnsureDefaultFB(funcName))
        return zeros;

    return mDefaultFB->mSize;
}

bool
WebGLContext::ValidateAndInitFB(const char* const funcName,
                                const WebGLFramebuffer* const fb)
{
    if (fb)
        return fb->ValidateAndInitAttachments(funcName);

    if (!EnsureDefaultFB(funcName))
        return false;

    if (mDefaultFB_IsInvalid) {
        gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mDefaultFB->mFB);
        const GLbitfield bits = LOCAL_GL_COLOR_BUFFER_BIT |
                                LOCAL_GL_DEPTH_BUFFER_BIT |
                                LOCAL_GL_STENCIL_BUFFER_BIT;
        const bool fakeNoAlpha = !mOptions.alpha;
        ForceClearFramebufferWithDefaultValues(bits, fakeNoAlpha);
        mDefaultFB_IsInvalid = false;
    }
    return true;
}

void
WebGLContext::DoBindFB(const WebGLFramebuffer* const fb, const GLenum target) const
{
    const GLenum driverFB = fb ? fb->mGLName : mDefaultFB->mFB;
    gl->fBindFramebuffer(target, driverFB);
}

bool
WebGLContext::BindCurFBForDraw(const char* const funcName)
{
    const auto& fb = mBoundDrawFramebuffer;
    if (!ValidateAndInitFB(funcName, fb))
        return false;

    DoBindFB(fb);
    return true;
}

bool
WebGLContext::BindCurFBForColorRead(const char* const funcName,
                                    const webgl::FormatUsageInfo** const out_format,
                                    uint32_t* const out_width,
                                    uint32_t* const out_height)
{
    const auto& fb = mBoundReadFramebuffer;

    if (fb) {
        if (!ValidateAndInitFB(funcName, fb))
            return false;
        if (!fb->ValidateForColorRead(funcName, out_format, out_width, out_height))
            return false;

        gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fb->mGLName);
        return true;
    }

    if (!BindDefaultFBForRead(funcName))
        return false;

    if (mDefaultFB_ReadBuffer == LOCAL_GL_NONE) {
        ErrorInvalidOperation("%s: Can't read from backbuffer when readBuffer mode is"
                              " NONE.",
                              funcName);
        return false;
    }

    auto effFormat = mOptions.alpha ? webgl::EffectiveFormat::RGBA8
                                    : webgl::EffectiveFormat::RGB8;

    *out_format = mFormatUsage->GetUsage(effFormat);
    MOZ_ASSERT(*out_format);

    *out_width = mDefaultFB->mSize.width;
    *out_height = mDefaultFB->mSize.height;
    return true;
}

bool
WebGLContext::BindDefaultFBForRead(const char* const funcName)
{
    if (!ValidateAndInitFB(funcName, nullptr))
        return false;

    if (!mDefaultFB->mSamples) {
        gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mDefaultFB->mFB);
        return true;
    }

    if (!mResolvedDefaultFB) {
        mResolvedDefaultFB = MozFramebuffer::Create(gl, mDefaultFB->mSize, 0, false);
        if (!mResolvedDefaultFB) {
            gfxCriticalNote << funcName << ": Failed to create mResolvedDefaultFB.";
            return false;
        }
    }

    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mResolvedDefaultFB->mFB);
    BlitBackbufferToCurDriverFB();

    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mResolvedDefaultFB->mFB);
    return true;
}

void
WebGLContext::DoColorMask(const uint8_t bitmask) const
{
    if (mDriverColorMask != bitmask) {
        mDriverColorMask = bitmask;
        gl->fColorMask(bool(mDriverColorMask & (1 << 0)),
                       bool(mDriverColorMask & (1 << 1)),
                       bool(mDriverColorMask & (1 << 2)),
                       bool(mDriverColorMask & (1 << 3)));
    }
}

////////////////////////////////////////////////////////////////////////////////

ScopedDrawCallWrapper::ScopedDrawCallWrapper(WebGLContext& webgl)
    : mWebGL(webgl)
{
    uint8_t driverColorMask = mWebGL.mColorWriteMask;
    bool driverDepthTest    = mWebGL.mDepthTestEnabled;
    bool driverStencilTest  = mWebGL.mStencilTestEnabled;
    const auto& fb = mWebGL.mBoundDrawFramebuffer;
    if (!fb) {
        if (mWebGL.mDefaultFB_DrawBuffer0 == LOCAL_GL_NONE) {
            driverColorMask = 0; // Is this well-optimized enough for depth-first
                                 // rendering?
        } else {
            driverColorMask &= ~(uint8_t(mWebGL.mNeedsFakeNoAlpha) << 3);
        }
        driverDepthTest   &= !mWebGL.mNeedsFakeNoDepth;
        driverStencilTest &= !mWebGL.mNeedsFakeNoStencil;
    } else {
        if (mWebGL.mNeedsFakeNoStencil_UserFBs &&
            fb->DepthAttachment().IsDefined() &&
            !fb->StencilAttachment().IsDefined())
        {
            driverStencilTest = false;
        }
    }

    const auto& gl = mWebGL.gl;
    mWebGL.DoColorMask(driverColorMask);
    if (mWebGL.mDriverDepthTest != driverDepthTest) {
        // "When disabled, the depth comparison and subsequent possible updates to the
        //  depth buffer value are bypassed and the fragment is passed to the next
        //  operation." [GLES 3.0.5, p177]
        mWebGL.mDriverDepthTest = driverDepthTest;
        gl->SetEnabled(LOCAL_GL_DEPTH_TEST, mWebGL.mDriverDepthTest);
    }
    if (mWebGL.mDriverStencilTest != driverStencilTest) {
        // "When disabled, the stencil test and associated modifications are not made, and
        //  the fragment is always passed." [GLES 3.0.5, p175]
        mWebGL.mDriverStencilTest = driverStencilTest;
        gl->SetEnabled(LOCAL_GL_STENCIL_TEST, mWebGL.mDriverStencilTest);
    }
}

ScopedDrawCallWrapper::~ScopedDrawCallWrapper()
{
    if (mWebGL.mBoundDrawFramebuffer)
        return;

    mWebGL.mResolvedDefaultFB = nullptr;

    mWebGL.Invalidate();
    mWebGL.mShouldPresent = true;
}

////////////////////////////////////////

IndexedBufferBinding::IndexedBufferBinding()
    : mRangeStart(0)
    , mRangeSize(0)
{ }

uint64_t
IndexedBufferBinding::ByteCount() const
{
    if (!mBufferBinding)
        return 0;

    uint64_t bufferSize = mBufferBinding->ByteLength();
    if (!mRangeSize) // BindBufferBase
        return bufferSize;

    if (mRangeStart >= bufferSize)
        return 0;
    bufferSize -= mRangeStart;

    return std::min(bufferSize, mRangeSize);
}

////////////////////////////////////////

ScopedUnpackReset::ScopedUnpackReset(WebGLContext* webgl)
    : ScopedGLWrapper<ScopedUnpackReset>(webgl->gl)
    , mWebGL(webgl)
{
    if (mWebGL->mPixelStore_UnpackAlignment != 4) mGL->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);

    if (mWebGL->IsWebGL2()) {
        if (mWebGL->mPixelStore_UnpackRowLength   != 0) mGL->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH  , 0);
        if (mWebGL->mPixelStore_UnpackImageHeight != 0) mGL->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, 0);
        if (mWebGL->mPixelStore_UnpackSkipPixels  != 0) mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_PIXELS , 0);
        if (mWebGL->mPixelStore_UnpackSkipRows    != 0) mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS   , 0);
        if (mWebGL->mPixelStore_UnpackSkipImages  != 0) mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES , 0);

        if (mWebGL->mBoundPixelUnpackBuffer) mGL->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
    }
}

void
ScopedUnpackReset::UnwrapImpl()
{
    mGL->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, mWebGL->mPixelStore_UnpackAlignment);

    if (mWebGL->IsWebGL2()) {
        mGL->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH  , mWebGL->mPixelStore_UnpackRowLength  );
        mGL->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, mWebGL->mPixelStore_UnpackImageHeight);
        mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_PIXELS , mWebGL->mPixelStore_UnpackSkipPixels );
        mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS   , mWebGL->mPixelStore_UnpackSkipRows   );
        mGL->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES , mWebGL->mPixelStore_UnpackSkipImages );

        GLuint pbo = 0;
        if (mWebGL->mBoundPixelUnpackBuffer) {
            pbo = mWebGL->mBoundPixelUnpackBuffer->mGLName;
        }

        mGL->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, pbo);
    }
}

////////////////////

void
ScopedFBRebinder::UnwrapImpl()
{
    const auto fnName = [&](WebGLFramebuffer* fb) {
        return fb ? fb->mGLName : 0;
    };

    if (mWebGL->IsWebGL2()) {
        mGL->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER, fnName(mWebGL->mBoundDrawFramebuffer));
        mGL->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, fnName(mWebGL->mBoundReadFramebuffer));
    } else {
        MOZ_ASSERT(mWebGL->mBoundDrawFramebuffer == mWebGL->mBoundReadFramebuffer);
        mGL->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fnName(mWebGL->mBoundDrawFramebuffer));
    }
}

////////////////////

static GLenum
TargetIfLazy(GLenum target)
{
    switch (target) {
    case LOCAL_GL_PIXEL_PACK_BUFFER:
    case LOCAL_GL_PIXEL_UNPACK_BUFFER:
        return target;

    default:
        return 0;
    }
}

ScopedLazyBind::ScopedLazyBind(gl::GLContext* gl, GLenum target, const WebGLBuffer* buf)
    : ScopedGLWrapper<ScopedLazyBind>(gl)
    , mTarget(buf ? TargetIfLazy(target) : 0)
    , mBuf(buf)
{
    if (mTarget) {
        mGL->fBindBuffer(mTarget, mBuf->mGLName);
    }
}

void
ScopedLazyBind::UnwrapImpl()
{
    if (mTarget) {
        mGL->fBindBuffer(mTarget, 0);
    }
}

////////////////////////////////////////

bool
Intersect(const int32_t srcSize, const int32_t read0, const int32_t readSize,
          int32_t* const out_intRead0, int32_t* const out_intWrite0,
          int32_t* const out_intSize)
{
    MOZ_ASSERT(srcSize >= 0);
    MOZ_ASSERT(readSize >= 0);
    const auto read1 = int64_t(read0) + readSize;

    int32_t intRead0 = read0; // Clearly doesn't need validation.
    int64_t intWrite0 = 0;
    int64_t intSize = readSize;

    if (read1 <= 0 || read0 >= srcSize) {
        // Disjoint ranges.
        intSize = 0;
    } else {
        if (read0 < 0) {
            const auto diff = int64_t(0) - read0;
            MOZ_ASSERT(diff >= 0);
            intRead0 = 0;
            intWrite0 = diff;
            intSize -= diff;
        }
        if (read1 > srcSize) {
            const auto diff = int64_t(read1) - srcSize;
            MOZ_ASSERT(diff >= 0);
            intSize -= diff;
        }

        if (!CheckedInt<int32_t>(intWrite0).isValid() ||
            !CheckedInt<int32_t>(intSize).isValid())
        {
            return false;
        }
    }

    *out_intRead0 = intRead0;
    *out_intWrite0 = intWrite0;
    *out_intSize = intSize;
    return true;
}

// --

uint64_t
AvailGroups(const uint64_t totalAvailItems, const uint64_t firstItemOffset,
            const uint32_t groupSize, const uint32_t groupStride)
{
    MOZ_ASSERT(groupSize && groupStride);
    MOZ_ASSERT(groupSize <= groupStride);

    if (totalAvailItems <= firstItemOffset)
        return 0;
    const size_t availItems = totalAvailItems - firstItemOffset;

    size_t availGroups     = availItems / groupStride;
    const size_t tailItems = availItems % groupStride;
    if (tailItems >= groupSize) {
        availGroups += 1;
    }
    return availGroups;
}

////////////////////////////////////////////////////////////////////////////////

CheckedUint32
WebGLContext::GetUnpackSize(bool isFunc3D, uint32_t width, uint32_t height,
                            uint32_t depth, uint8_t bytesPerPixel)
{
    if (!width || !height || !depth)
        return 0;

    ////////////////

    const auto& maybeRowLength = mPixelStore_UnpackRowLength;
    const auto& maybeImageHeight = mPixelStore_UnpackImageHeight;

    const auto usedPixelsPerRow = CheckedUint32(mPixelStore_UnpackSkipPixels) + width;
    const auto stridePixelsPerRow = (maybeRowLength ? CheckedUint32(maybeRowLength)
                                                    : usedPixelsPerRow);

    const auto usedRowsPerImage = CheckedUint32(mPixelStore_UnpackSkipRows) + height;
    const auto strideRowsPerImage = (maybeImageHeight ? CheckedUint32(maybeImageHeight)
                                                      : usedRowsPerImage);

    const uint32_t skipImages = (isFunc3D ? mPixelStore_UnpackSkipImages
                                          : 0);
    const CheckedUint32 usedImages = CheckedUint32(skipImages) + depth;

    ////////////////

    CheckedUint32 strideBytesPerRow = bytesPerPixel * stridePixelsPerRow;
    strideBytesPerRow = RoundUpToMultipleOf(strideBytesPerRow,
                                            mPixelStore_UnpackAlignment);

    const CheckedUint32 strideBytesPerImage = strideBytesPerRow * strideRowsPerImage;

    ////////////////

    CheckedUint32 usedBytesPerRow = bytesPerPixel * usedPixelsPerRow;
    // Don't round this to the alignment, since alignment here is really just used for
    // establishing stride, particularly in WebGL 1, where you can't set ROW_LENGTH.

    CheckedUint32 totalBytes = strideBytesPerImage * (usedImages - 1);
    totalBytes += strideBytesPerRow * (usedRowsPerImage - 1);
    totalBytes += usedBytesPerRow;

    return totalBytes;
}

already_AddRefed<layers::SharedSurfaceTextureClient>
WebGLContext::GetVRFrame()
{
  /**
   * Swap buffers as though composition has occurred.
   * We will then share the resulting front buffer to be submitted to the VR
   * compositor.
   */
  BeginComposition();
  EndComposition();

  if (IsContextLost())
      return nullptr;

  gl::GLScreenBuffer* screen = gl->Screen();
  if (!screen)
      return nullptr;

  RefPtr<SharedSurfaceTextureClient> sharedSurface = screen->Front();
  if (!sharedSurface)
      return nullptr;

  return sharedSurface.forget();
}

////////////////////////////////////////////////////////////////////////////////

static inline size_t
SizeOfViewElem(const dom::ArrayBufferView& view)
{
    const auto& elemType = view.Type();
    if (elemType == js::Scalar::MaxTypedArrayViewType) // DataViews.
        return 1;

    return js::Scalar::byteSize(elemType);
}

bool
WebGLContext::ValidateArrayBufferView(const char* funcName,
                                      const dom::ArrayBufferView& view, GLuint elemOffset,
                                      GLuint elemCountOverride, uint8_t** const out_bytes,
                                      size_t* const out_byteLen)
{
    view.ComputeLengthAndData();
    uint8_t* const bytes = view.DataAllowShared();
    const size_t byteLen = view.LengthAllowShared();

    const auto& elemSize = SizeOfViewElem(view);

    size_t elemCount = byteLen / elemSize;
    if (elemOffset > elemCount) {
        ErrorInvalidValue("%s: Invalid offset into ArrayBufferView.", funcName);
        return false;
    }
    elemCount -= elemOffset;

    if (elemCountOverride) {
        if (elemCountOverride > elemCount) {
            ErrorInvalidValue("%s: Invalid sub-length for ArrayBufferView.", funcName);
            return false;
        }
        elemCount = elemCountOverride;
    }

    *out_bytes = bytes + (elemOffset * elemSize);
    *out_byteLen = elemCount * elemSize;
    return true;
}

////

void
WebGLContext::UpdateMaxDrawBuffers()
{
    mGLMaxColorAttachments = gl->GetIntAs<uint32_t>(LOCAL_GL_MAX_COLOR_ATTACHMENTS);
    mGLMaxDrawBuffers = gl->GetIntAs<uint32_t>(LOCAL_GL_MAX_DRAW_BUFFERS);

    // WEBGL_draw_buffers:
    // "The value of the MAX_COLOR_ATTACHMENTS_WEBGL parameter must be greater than or
    //  equal to that of the MAX_DRAW_BUFFERS_WEBGL parameter."
    mGLMaxDrawBuffers = std::min(mGLMaxDrawBuffers, mGLMaxColorAttachments);
}

// --

webgl::AvailabilityRunnable*
WebGLContext::EnsureAvailabilityRunnable()
{
    if (!mAvailabilityRunnable) {
        RefPtr<webgl::AvailabilityRunnable> runnable = new webgl::AvailabilityRunnable(this);

        nsIDocument* document = GetOwnerDoc();
        if (document) {
            document->Dispatch(TaskCategory::Other, runnable.forget());
        } else {
            NS_DispatchToCurrentThread(runnable.forget());
        }
    }
    return mAvailabilityRunnable;
}

webgl::AvailabilityRunnable::AvailabilityRunnable(WebGLContext* const webgl)
    : Runnable("webgl::AvailabilityRunnable")
    , mWebGL(webgl)
{
    mWebGL->mAvailabilityRunnable = this;
}

webgl::AvailabilityRunnable::~AvailabilityRunnable()
{
    MOZ_ASSERT(mQueries.empty());
    MOZ_ASSERT(mSyncs.empty());
}

nsresult
webgl::AvailabilityRunnable::Run()
{
    for (const auto& cur : mQueries) {
        cur->mCanBeAvailable = true;
    }
    mQueries.clear();

    for (const auto& cur : mSyncs) {
        cur->mCanBeAvailable = true;
    }
    mSyncs.clear();

    mWebGL->mAvailabilityRunnable = nullptr;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// XPCOM goop

void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& callback,
                            const std::vector<IndexedBufferBinding>& field,
                            const char* name, uint32_t flags)
{
    for (const auto& cur : field) {
        ImplCycleCollectionTraverse(callback, cur.mBufferBinding, name, flags);
    }
}

void
ImplCycleCollectionUnlink(std::vector<IndexedBufferBinding>& field)
{
    field.clear();
}

////

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebGLContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebGLContext)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebGLContext,
  mCanvasElement,
  mOffscreenCanvas,
  mExtensions,
  mBound2DTextures,
  mBoundCubeMapTextures,
  mBound3DTextures,
  mBound2DArrayTextures,
  mBoundSamplers,
  mBoundArrayBuffer,
  mBoundCopyReadBuffer,
  mBoundCopyWriteBuffer,
  mBoundPixelPackBuffer,
  mBoundPixelUnpackBuffer,
  mBoundTransformFeedback,
  mBoundTransformFeedbackBuffer,
  mBoundUniformBuffer,
  mCurrentProgram,
  mBoundDrawFramebuffer,
  mBoundReadFramebuffer,
  mBoundRenderbuffer,
  mBoundVertexArray,
  mDefaultVertexArray,
  mQuerySlot_SamplesPassed,
  mQuerySlot_TFPrimsWritten,
  mQuerySlot_TimeElapsed)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebGLContext)
    NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
    NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    // If the exact way we cast to nsISupports here ever changes, fix our
    // ToSupports() method.
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICanvasRenderingContextInternal)
NS_INTERFACE_MAP_END

} // namespace mozilla
