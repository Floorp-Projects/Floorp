/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"

#include <algorithm>
#include <queue>
#include <regex>

#include "AccessCheck.h"
#include "CompositableHost.h"
#include "gfxConfig.h"
#include "gfxContext.h"
#include "gfxCrashReporterUtils.h"
#include "gfxEnv.h"
#include "gfxPattern.h"
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
#include "mozilla/EnumeratedRange.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsError.h"
#include "nsIClassInfoImpl.h"
#include "nsIGfxInfo.h"
#include "nsIWidget.h"
#include "nsServiceManagerUtils.h"
#include "SharedSurfaceGL.h"
#include "SVGObserverUtils.h"
#include "prenv.h"
#include "ScopedGLHelpers.h"
#include "VRManagerChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/layers/WebRenderCanvasRenderer.h"

// Local
#include "CanvasUtils.h"
#include "ClientWebGLContext.h"
#include "HostWebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLChild.h"
#include "WebGLContextLossHandler.h"
#include "WebGLContextUtils.h"
#include "WebGLExtensions.h"
#include "WebGLFormats.h"
#include "WebGLFramebuffer.h"
#include "WebGLMemoryTracker.h"
#include "WebGLObjectModel.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLShaderValidator.h"
#include "WebGLSync.h"
#include "WebGLTransformFeedback.h"
#include "WebGLValidateStrings.h"
#include "WebGLVertexArray.h"

#ifdef MOZ_WIDGET_COCOA
#  include "nsCocoaFeatures.h"
#endif

#ifdef XP_WIN
#  include "WGLLibrary.h"
#endif

// Generated
#include "mozilla/dom/WebGLRenderingContextBinding.h"

namespace mozilla {

static bool IsFeatureInBlacklist(const nsCOMPtr<nsIGfxInfo>& gfxInfo,
                                 int32_t feature,
                                 nsCString* const out_blacklistId) {
  int32_t status;
  if (!NS_SUCCEEDED(gfxUtils::ThreadSafeGetFeatureStatus(
          gfxInfo, feature, *out_blacklistId, &status))) {
    return false;
  }

  return status != nsIGfxInfo::FEATURE_STATUS_OK;
}

WebGLContextOptions::WebGLContextOptions() {
  // Set default alpha state based on preference.
  alpha = !StaticPrefs::webgl_default_no_alpha();
  antialias = StaticPrefs::webgl_default_antialias();
}

bool WebGLContextOptions::operator==(const WebGLContextOptions& r) const {
  bool eq = true;
  eq &= (alpha == r.alpha);
  eq &= (depth == r.depth);
  eq &= (stencil == r.stencil);
  eq &= (premultipliedAlpha == r.premultipliedAlpha);
  eq &= (antialias == r.antialias);
  eq &= (preserveDrawingBuffer == r.preserveDrawingBuffer);
  eq &= (failIfMajorPerformanceCaveat == r.failIfMajorPerformanceCaveat);
  eq &= (xrCompatible == r.xrCompatible);
  eq &= (powerPreference == r.powerPreference);
  return eq;
}

static std::list<WebGLContext*> sWebglLru;

WebGLContext::LruPosition::LruPosition() : mItr(sWebglLru.end()) {}  // NOLINT

WebGLContext::LruPosition::LruPosition(WebGLContext& context)
    : mItr(sWebglLru.insert(sWebglLru.end(), &context)) {}

void WebGLContext::LruPosition::reset() {
  const auto end = sWebglLru.end();
  if (mItr != end) {
    sWebglLru.erase(mItr);
    mItr = end;
  }
}

WebGLContext::WebGLContext(HostWebGLContext& host,
                           const webgl::InitContextDesc& desc)
    : gl(mGL_OnlyClearInDestroyResourcesAndContext),  // const reference
      mHost(&host),
      mResistFingerprinting(desc.resistFingerprinting),
      mOptions(desc.options),
      mPrincipalKey(desc.principalKey),
      mMaxPerfWarnings(StaticPrefs::webgl_perf_max_warnings()),
      mMaxAcceptableFBStatusInvals(
          StaticPrefs::webgl_perf_max_acceptable_fb_status_invals()),
      mContextLossHandler(this),
      mMaxWarnings(StaticPrefs::webgl_max_warnings_per_context()),
      mAllowFBInvalidation(StaticPrefs::webgl_allow_fb_invalidation()),
      mMsaaSamples((uint8_t)StaticPrefs::webgl_msaa_samples()),
      mRequestedSize(desc.size) {
  host.mContext = this;
  const FuncScope funcScope(*this, "<Create>");

  if (mOptions.antialias && !StaticPrefs::webgl_msaa_force()) {
    const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();

    nsCString blocklistId;
    if (IsFeatureInBlacklist(gfxInfo, nsIGfxInfo::FEATURE_WEBGL_MSAA,
                             &blocklistId)) {
      GenerateWarning(
          "getContext: Disallowing antialiased backbuffers due to "
          "blacklisting. (%s)",
          blocklistId.BeginReading());
      mOptions.antialias = false;
    }
  }
}

WebGLContext::~WebGLContext() { DestroyResourcesAndContext(); }

void WebGLContext::DestroyResourcesAndContext() {
  if (!gl) return;

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
  mBoundVertexArray = nullptr;
  mDefaultVertexArray = nullptr;
  mBoundTransformFeedback = nullptr;
  mDefaultTransformFeedback = nullptr;
#if defined(MOZ_WIDGET_ANDROID)
  mVRScreen = nullptr;
#endif

  mQuerySlot_SamplesPassed = nullptr;
  mQuerySlot_TFPrimsWritten = nullptr;
  mQuerySlot_TimeElapsed = nullptr;

  mIndexedUniformBufferBindings.clear();

  if (mAvailabilityRunnable) {
    mAvailabilityRunnable->Run();
  }

  //////

  if (mEmptyTFO) {
    gl->fDeleteTransformFeedbacks(1, &mEmptyTFO);
    mEmptyTFO = 0;
  }

  //////

  if (mFakeVertexAttrib0BufferObject) {
    gl->fDeleteBuffers(1, &mFakeVertexAttrib0BufferObject);
    mFakeVertexAttrib0BufferObject = 0;
  }

  // disable all extensions except "WEBGL_lose_context". see bug #927969
  // spec: http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
  for (size_t i = 0; i < size_t(WebGLExtensionID::Max); ++i) {
    WebGLExtensionID extension = WebGLExtensionID(i);
    if (extension == WebGLExtensionID::WEBGL_lose_context) continue;
    mExtensions[extension] = nullptr;
  }

  // We just got rid of everything, so the context had better
  // have been going away.
  if (gl::GLContext::ShouldSpew()) {
    printf_stderr("--- WebGL context destroyed: %p\n", gl.get());
  }

  MOZ_ASSERT(gl);
  gl->MarkDestroyed();
  mGL_OnlyClearInDestroyResourcesAndContext = nullptr;
  MOZ_ASSERT(!gl);
}

void ClientWebGLContext::Invalidate() {
  if (!mCanvasElement) return;

  mCapturedFrameInvalidated = true;

  if (mInvalidated) return;

  SVGObserverUtils::InvalidateDirectRenderingObservers(mCanvasElement);

  mInvalidated = true;
  mCanvasElement->InvalidateCanvasContent(nullptr);
}

void WebGLContext::OnMemoryPressure() {
  bool shouldLoseContext = mLoseContextOnMemoryPressure;

  if (!mCanLoseContextInForeground &&
      ProcessPriorityManager::CurrentProcessIsForeground()) {
    shouldLoseContext = false;
  }

  if (shouldLoseContext) LoseContext();
}

//
// nsICanvasRenderingContextInternal
//

static bool HasAcceleratedLayers(const nsCOMPtr<nsIGfxInfo>& gfxInfo) {
  int32_t status;

  nsCString discardFailureId;
  gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                       nsIGfxInfo::FEATURE_DIRECT3D_9_LAYERS,
                                       discardFailureId, &status);
  if (status) return true;
  gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                       nsIGfxInfo::FEATURE_DIRECT3D_10_LAYERS,
                                       discardFailureId, &status);
  if (status) return true;
  gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                       nsIGfxInfo::FEATURE_DIRECT3D_10_1_LAYERS,
                                       discardFailureId, &status);
  if (status) return true;
  gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                       nsIGfxInfo::FEATURE_DIRECT3D_11_LAYERS,
                                       discardFailureId, &status);
  if (status) return true;
  gfxUtils::ThreadSafeGetFeatureStatus(
      gfxInfo, nsIGfxInfo::FEATURE_OPENGL_LAYERS, discardFailureId, &status);
  if (status) return true;

  return false;
}

// --

bool WebGLContext::CreateAndInitGL(
    bool forceEnabled, std::vector<FailureReason>* const out_failReasons) {
  const FuncScope funcScope(*this, "<Create>");

  // Can't use WebGL in headless mode.
  if (gfxPlatform::IsHeadless()) {
    FailureReason reason;
    reason.info =
        "Can't use WebGL in headless mode (https://bugzil.la/1375585).";
    out_failReasons->push_back(reason);
    GenerateWarning("%s", reason.info.BeginReading());
    return false;
  }

  // WebGL2 is separately blocked:
  if (IsWebGL2() && !forceEnabled) {
    const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    const auto feature = nsIGfxInfo::FEATURE_WEBGL2;

    FailureReason reason;
    if (IsFeatureInBlacklist(gfxInfo, feature, &reason.key)) {
      reason.info =
          "Refused to create WebGL2 context because of blacklist"
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

  if (StaticPrefs::webgl_cgl_multithreaded()) {
    flags |= gl::CreateContextFlags::PREFER_MULTITHREADED;
  }

  if (IsWebGL2()) {
    flags |= gl::CreateContextFlags::PREFER_ES3;
  } else {
    // Request and prefer ES2 context for WebGL1.
    flags |= gl::CreateContextFlags::PREFER_EXACT_VERSION;

    if (!StaticPrefs::webgl_1_allow_core_profiles()) {
      flags |= gl::CreateContextFlags::REQUIRE_COMPAT_PROFILE;
    }
  }

  {
    auto powerPref = mOptions.powerPreference;

    // If "Use hardware acceleration when available" option is disabled:
    if (!gfx::gfxConfig::IsEnabled(gfx::Feature::HW_COMPOSITING)) {
      powerPref = dom::WebGLPowerPreference::Low_power;
    }

    const auto overrideVal = StaticPrefs::webgl_power_preference_override();
    if (overrideVal > 0) {
      powerPref = dom::WebGLPowerPreference::High_performance;
    } else if (overrideVal < 0) {
      powerPref = dom::WebGLPowerPreference::Low_power;
    }

    if (powerPref == dom::WebGLPowerPreference::High_performance) {
      flags |= gl::CreateContextFlags::HIGH_POWER;
    }
  }

#ifdef XP_MACOSX
  const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  nsString vendorID, deviceID;

  // Avoid crash for Intel HD Graphics 3000 on OSX. (Bug 1413269)
  gfxInfo->GetAdapterVendorID(vendorID);
  gfxInfo->GetAdapterDeviceID(deviceID);
  if (vendorID.EqualsLiteral("0x8086") &&
      (deviceID.EqualsLiteral("0x0116") || deviceID.EqualsLiteral("0x0126"))) {
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

  if (StaticPrefs::webgl_disable_wgl()) {
    tryNativeGL = false;
  }

  if (StaticPrefs::webgl_disable_angle() ||
      PR_GetEnv("MOZ_WEBGL_FORCE_OPENGL") || useEGL) {
    tryNativeGL = true;
    tryANGLE = false;
  }
#endif

  if (tryNativeGL && !forceEnabled) {
    const nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    const auto feature = nsIGfxInfo::FEATURE_WEBGL_OPENGL;

    FailureReason reason;
    if (IsFeatureInBlacklist(gfxInfo, feature, &reason.key)) {
      reason.info =
          "Refused to create native OpenGL context because of blacklist"
          " entry: ";
      reason.info.Append(reason.key);

      out_failReasons->push_back(reason);

      GenerateWarning("%s", reason.info.BeginReading());
      tryNativeGL = false;
    }
  }

  // --

  typedef decltype(
      gl::GLContextProviderEGL::CreateOffscreen) fnCreateOffscreenT;
  const auto fnCreate = [&](fnCreateOffscreenT* const pfnCreateOffscreen,
                            const char* const info) {
    const gfx::IntSize dummySize(1, 1);
    nsCString failureId;
    const RefPtr<gl::GLContext> gl =
        pfnCreateOffscreen(dummySize, surfaceCaps, flags, &failureId);
    if (!gl) {
      out_failReasons->push_back(WebGLContext::FailureReason(failureId, info));
    }
    return gl;
  };

  const auto newGL = [&]() -> RefPtr<gl::GLContext> {
    if (tryNativeGL) {
      if (useEGL)
        return fnCreate(&gl::GLContextProviderEGL::CreateOffscreen, "useEGL");

      const auto ret =
          fnCreate(&gl::GLContextProvider::CreateOffscreen, "tryNativeGL");
      if (ret) return ret;
    }

    if (tryANGLE) {
      // Force enable alpha channel to make sure ANGLE use correct framebuffer
      // format
      MOZ_ASSERT(surfaceCaps.alpha);
      return fnCreate(&gl::GLContextProviderEGL::CreateOffscreen, "tryANGLE");
    }
    return nullptr;
  }();

  if (!newGL) {
    out_failReasons->push_back(
        FailureReason("FEATURE_FAILURE_WEBGL_EXHAUSTED_DRIVERS",
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

bool WebGLContext::EnsureDefaultFB() {
  if (mDefaultFB) {
    MOZ_ASSERT(*uvec2::FromSize(mDefaultFB->mSize) == mRequestedSize);
    return true;
  }

  const bool depthStencil = mOptions.depth || mOptions.stencil;
  auto attemptSize = gfx::IntSize{mRequestedSize.x, mRequestedSize.y};

  while (attemptSize.width || attemptSize.height) {
    attemptSize.width = std::max(attemptSize.width, 1);
    attemptSize.height = std::max(attemptSize.height, 1);

    [&]() {
      if (mOptions.antialias) {
        MOZ_ASSERT(!mDefaultFB);
        mDefaultFB = gl::MozFramebuffer::Create(gl, attemptSize, mMsaaSamples,
                                                depthStencil);
        if (mDefaultFB) return;
        if (mOptionsFrozen) return;
      }

      MOZ_ASSERT(!mDefaultFB);
      mDefaultFB = gl::MozFramebuffer::Create(gl, attemptSize, 0, depthStencil);
    }();

    if (mDefaultFB) break;

    attemptSize.width /= 2;
    attemptSize.height /= 2;
  }

  if (!mDefaultFB) {
    GenerateWarning("Backbuffer resize failed. Losing context.");
    LoseContext();
    return false;
  }

  mDefaultFB_IsInvalid = true;

  const auto actualSize = *uvec2::FromSize(mDefaultFB->mSize);
  if (actualSize != mRequestedSize) {
    GenerateWarning(
        "Requested size %ux%u was too large, but resize"
        " to %ux%u succeeded.",
        mRequestedSize.x, mRequestedSize.y, actualSize.x, actualSize.y);
  }
  mRequestedSize = actualSize;
  return true;
}

void WebGLContext::Resize(uvec2 requestedSize) {
  // Zero-sized surfaces can cause problems.
  if (!requestedSize.x) {
    requestedSize.x = 1;
  }
  if (!requestedSize.y) {
    requestedSize.y = 1;
  }

  // WebGL 1 spec:
  //   WebGL presents its drawing buffer to the HTML page compositor immediately
  //   before a compositing operation, but only if at least one of the following
  //   has occurred since the previous compositing operation:
  //
  //   * Context creation
  //   * Canvas resize
  //   * clear, drawArrays, or drawElements has been called while the drawing
  //     buffer is the currently bound framebuffer
  mShouldPresent = true;
  if (requestedSize == mRequestedSize) return;

  // Kill our current default fb(s), for later lazy allocation.
  mRequestedSize = requestedSize;
  mDefaultFB = nullptr;
  mResetLayer = true;  // New size means new Layer.
}

UniquePtr<webgl::FormatUsageAuthority> WebGLContext::CreateFormatUsage(
    gl::GLContext* gl) const {
  return webgl::FormatUsageAuthority::CreateForWebGL1(gl);
}

/*static*/
RefPtr<WebGLContext> WebGLContext::Create(HostWebGLContext& host,
                                          const webgl::InitContextDesc& desc,
                                          webgl::InitContextResult* const out) {
  nsCString failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_UNKOWN");
  const bool forceEnabled = StaticPrefs::webgl_force_enabled();
  ScopedGfxFeatureReporter reporter("WebGL", forceEnabled);

  auto res = [&]() -> Result<RefPtr<WebGLContext>, std::string> {
    bool disabled = StaticPrefs::webgl_disabled();

    // TODO: When we have software webgl support we should use that instead.
    disabled |= gfxPlatform::InSafeMode();

    if (disabled) {
      if (gfxPlatform::InSafeMode()) {
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_SAFEMODE");
      } else {
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_DISABLED");
      }
      return Err("WebGL is currently disabled.");
    }

    if (desc.options.failIfMajorPerformanceCaveat) {
      nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
      if (!HasAcceleratedLayers(gfxInfo)) {
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_PERF_CAVEAT");
        return Err(
            "failIfMajorPerformanceCaveat: Compositor is not"
            " hardware-accelerated.");
      }
    }

    // Alright, now let's start trying.

    RefPtr<WebGLContext> webgl;
    if (desc.isWebgl2) {
      webgl = new WebGL2Context(host, desc);
    } else {
      webgl = new WebGLContext(host, desc);
    }

    MOZ_ASSERT(!webgl->gl);
    std::vector<FailureReason> failReasons;
    if (!webgl->CreateAndInitGL(forceEnabled, &failReasons)) {
      nsCString text("WebGL creation failed: ");
      for (const auto& cur : failReasons) {
        // Don't try to accumulate using an empty key if |cur.key| is empty.
        if (cur.key.IsEmpty()) {
          Telemetry::Accumulate(
              Telemetry::CANVAS_WEBGL_FAILURE_ID,
              NS_LITERAL_CSTRING("FEATURE_FAILURE_REASON_UNKNOWN"));
        } else {
          Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID, cur.key);
        }

        text.AppendLiteral("\n* ");
        text.Append(cur.info);
      }
      failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_REASON");
      return Err(text.BeginReading());
    }
    MOZ_ASSERT(webgl->gl);

    if (desc.options.failIfMajorPerformanceCaveat) {
      if (webgl->gl->IsWARP()) {
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_PERF_WARP");
        return Err(
            "failIfMajorPerformanceCaveat: Driver is not"
            " hardware-accelerated.");
      }

#ifdef XP_WIN
      if (webgl->gl->GetContextType() == gl::GLContextType::WGL &&
          !gl::sWGLLib.HasDXInterop2()) {
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_DXGL_INTEROP2");
        return Err("Caveat: WGL without DXGLInterop2.");
      }
#endif
    }

    const FuncScope funcScope(*webgl, "getContext/restoreContext");

    MOZ_ASSERT(!webgl->mDefaultFB);
    if (!webgl->EnsureDefaultFB()) {
      MOZ_ASSERT(!webgl->mDefaultFB);
      MOZ_ASSERT(webgl->IsContextLost());
      failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WEBGL_BACKBUFFER");
      return Err("Initializing WebGL backbuffer failed.");
    }

    return webgl;
  }();
  if (res.isOk()) {
    failureId = NS_LITERAL_CSTRING("SUCCESS");
  }
  Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID, failureId);

  if (!res.isOk()) {
    out->error = res.unwrapErr();
    return nullptr;
  }
  const auto webgl = res.unwrap();

  // Update our internal stuff:
  webgl->FinishInit();

  reporter.SetSuccessful();
  if (gl::GLContext::ShouldSpew()) {
    printf_stderr("--- WebGL context created: %p\n", webgl.get());
  }

  out->options = webgl->mOptions;
  out->limits = *webgl->mLimits;

  return webgl;
}

void WebGLContext::FinishInit() {
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
      gl->Vendor() == gl::GLVendor::Intel) {
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

  mScissorRect = {0, 0, size.width, size.height};
  mScissorRect.Apply(*gl);

  //////
  // Check everything

  AssertCachedBindings();
  AssertCachedGlobalState();

  mShouldPresent = true;

  //////

  gl->ResetSyncCallCount("WebGLContext Initialization");
  LoseLruContextIfLimitExceeded();
}

void WebGLContext::SetCompositableHost(
    RefPtr<layers::CompositableHost>& aCompositableHost) {
  mCompositableHost = aCompositableHost;
}

void WebGLContext::LoseLruContextIfLimitExceeded() {
  const auto maxContexts = std::max(1u, StaticPrefs::webgl_max_contexts());
  const auto maxContextsPerPrincipal =
      std::max(1u, StaticPrefs::webgl_max_contexts_per_principal());

  // it's important to update the index on a new context before losing old
  // contexts, otherwise new unused contexts would all have index 0 and we
  // couldn't distinguish older ones when choosing which one to lose first.
  BumpLru();

  {
    size_t forPrincipal = 0;
    for (const auto& context : sWebglLru) {
      if (context->mPrincipalKey == mPrincipalKey) {
        forPrincipal += 1;
      }
    }

    while (forPrincipal > maxContextsPerPrincipal) {
      const auto text = nsPrintfCString(
          "Exceeded %u live WebGL contexts for this principal, losing the "
          "least recently used one.",
          maxContextsPerPrincipal);
      mHost->JsWarning(ToString(text));

      for (const auto& context : sWebglLru) {
        if (context->mPrincipalKey == mPrincipalKey) {
          MOZ_ASSERT(context != this);
          context->LoseContext(webgl::ContextLossReason::None);
          forPrincipal -= 1;
          break;
        }
      }
    }
  }

  auto total = sWebglLru.size();
  while (total > maxContexts) {
    const auto text = nsPrintfCString(
        "Exceeded %u live WebGL contexts, losing the least "
        "recently used one.",
        maxContexts);
    mHost->JsWarning(ToString(text));

    const auto& context = sWebglLru.front();
    MOZ_ASSERT(context != this);
    context->LoseContext(webgl::ContextLossReason::None);
    total -= 1;
  }
}

// -

namespace webgl {

ScopedPrepForResourceClear::ScopedPrepForResourceClear(
    const WebGLContext& webgl_)
    : webgl(webgl_) {
  const auto& gl = webgl.gl;

  if (webgl.mScissorTestEnabled) {
    gl->fDisable(LOCAL_GL_SCISSOR_TEST);
  }
  if (webgl.mRasterizerDiscardEnabled) {
    gl->fDisable(LOCAL_GL_RASTERIZER_DISCARD);
  }

  // "The clear operation always uses the front stencil write mask
  //  when clearing the stencil buffer."
  webgl.DoColorMask(0x0f);
  gl->fDepthMask(true);
  gl->fStencilMaskSeparate(LOCAL_GL_FRONT, 0xffffffff);

  gl->fClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  gl->fClearDepth(1.0f);  // Depth formats are always cleared to 1.0f, not 0.0f.
  gl->fClearStencil(0);
}

ScopedPrepForResourceClear::~ScopedPrepForResourceClear() {
  const auto& gl = webgl.gl;

  if (webgl.mScissorTestEnabled) {
    gl->fEnable(LOCAL_GL_SCISSOR_TEST);
  }
  if (webgl.mRasterizerDiscardEnabled) {
    gl->fEnable(LOCAL_GL_RASTERIZER_DISCARD);
  }

  // DoColorMask() is lazy.
  gl->fDepthMask(webgl.mDepthWriteMask);
  gl->fStencilMaskSeparate(LOCAL_GL_FRONT, webgl.mStencilWriteMaskFront);

  gl->fClearColor(webgl.mColorClearValue[0], webgl.mColorClearValue[1],
                  webgl.mColorClearValue[2], webgl.mColorClearValue[3]);
  gl->fClearDepth(webgl.mDepthClearValue);
  gl->fClearStencil(webgl.mStencilClearValue);
}

}  // namespace webgl

// -

void WebGLContext::OnEndOfFrame() {
  if (StaticPrefs::webgl_perf_spew_frame_allocs()) {
    GeneratePerfWarning("[webgl.perf.spew-frame-allocs] %" PRIu64
                        " data allocations this frame.",
                        mDataAllocGLCallCount);
  }
  mDataAllocGLCallCount = 0;
  gl->ResetSyncCallCount("WebGLContext PresentScreenBuffer");

  BumpLru();
}

void WebGLContext::BlitBackbufferToCurDriverFB(
    const gl::MozFramebuffer* const source) const {
  DoColorMask(0x0f);

  if (mScissorTestEnabled) {
    gl->fDisable(LOCAL_GL_SCISSOR_TEST);
  }

  [&]() {
    const auto fb = source ? source : mDefaultFB.get();

    if (gl->IsSupported(gl::GLFeature::framebuffer_blit)) {
      gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, fb->mFB);
      gl->fBlitFramebuffer(0, 0, fb->mSize.width, fb->mSize.height, 0, 0,
                           fb->mSize.width, fb->mSize.height,
                           LOCAL_GL_COLOR_BUFFER_BIT, LOCAL_GL_NEAREST);
      return;
    }
    if (mDefaultFB->mSamples &&
        gl->IsExtensionSupported(
            gl::GLContext::APPLE_framebuffer_multisample)) {
      gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, fb->mFB);
      gl->fResolveMultisampleFramebufferAPPLE();
      return;
    }

    gl->BlitHelper()->DrawBlitTextureToFramebuffer(fb->ColorTex(), fb->mSize,
                                                   fb->mSize);
  }();

  if (mScissorTestEnabled) {
    gl->fEnable(LOCAL_GL_SCISSOR_TEST);
  }
}

// TODO: (JG) I think this should be removed, the Client should manage
// TextureClients, and imperitively tell the Host what to render to.
Maybe<ICRData> WebGLContext::InitializeCanvasRenderer(
    layers::LayersBackend backend) {
  if (!gl) {
    return Nothing();
  }

  ICRData ret;
  ret.size = {DrawingBufferSize().x, DrawingBufferSize().y};
  ret.hasAlpha = mOptions.alpha;
  ret.isPremultAlpha = IsPremultAlpha();

  auto flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
  if ((!IsPremultAlpha()) && mOptions.alpha) {
    flags |= layers::TextureFlags::NON_PREMULTIPLIED;
  }

  // NB: This is weak.  Creating TextureClient objects in the host-side
  // WebGLContext class... but these are different concepts of host/client.
  // Host/ClientWebGLContext represent cross-process communication but
  // TextureHost/Client represent synchronous texture access, which can
  // be uniprocess and, for us, is.  Also note that TextureClient couldn't
  // be in the content process like ClientWebGLContext since TextureClient
  // uses a GL context.
  UniquePtr<gl::SurfaceFactory> factory = gl::GLScreenBuffer::CreateFactory(
      gl, gl->Caps(), nullptr, backend, gl->IsANGLE(), flags);
  mBackend = backend;

  if (!factory) {
    // Absolutely must have a factory here, so create a basic one
    factory = MakeUnique<gl::SurfaceFactory_Basic>(gl, gl->Caps(), flags);
    mBackend = layers::LayersBackend::LAYERS_BASIC;
  }

  gl->Screen()->Morph(std::move(factory));

#if defined(MOZ_WIDGET_ANDROID)
  // On Android we are using a different GLScreenBuffer for WebVR, so we need
  // a resize here because PresentScreenBuffer() may not be called for the
  // gl->Screen() after we set the new factory.
  mForceResizeOnPresent = true;
#endif
  mVRReady = true;
  return Some(ret);
}

// -

template <typename T, typename... Args>
constexpr auto MakeArray(Args... args) -> std::array<T, sizeof...(Args)> {
  return {{static_cast<T>(args)...}};
}

// -

// For an overview of how WebGL compositing works, see:
// https://wiki.mozilla.org/Platform/GFX/WebGL/Compositing
bool WebGLContext::PresentScreenBuffer(gl::GLScreenBuffer* const targetScreen) {
  const FuncScope funcScope(*this, "<PresentScreenBuffer>");
  if (IsContextLost()) return false;

  mDrawCallsSinceLastFlush = 0;

  if (!mShouldPresent) return false;

  if (!ValidateAndInitFB(nullptr)) return false;

  const auto& screen = targetScreen ? targetScreen : gl->Screen();
  bool needsResize = mForceResizeOnPresent;
  needsResize |=
      !screen->IsReadBufferReady() || (screen->Size() != mDefaultFB->mSize);
  if (needsResize && !screen->Resize(mDefaultFB->mSize)) {
    GenerateWarning("screen->Resize failed. Losing context.");
    LoseContext();
    return false;
  }
  mForceResizeOnPresent = false;

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
    LoseContext();
    return false;
  }

  if (!mOptions.preserveDrawingBuffer) {
    if (gl->IsSupported(gl::GLFeature::invalidate_framebuffer)) {
      gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mDefaultFB->mFB);
      constexpr auto attachments = MakeArray<GLenum>(
          LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);
      gl->fInvalidateFramebuffer(LOCAL_GL_FRAMEBUFFER, attachments.size(),
                                 attachments.data());
    }
    mDefaultFB_IsInvalid = true;
  }
  mResolvedDefaultFB = nullptr;

  mShouldPresent = false;
  OnEndOfFrame();

  return true;
}

bool WebGLContext::PresentScreenBufferVR(
    gl::GLScreenBuffer* const aTargetScreen,
    const gl::MozFramebuffer* const fb) {
  const FuncScope funcScope(*this, "<PresentScreenBufferVR>");
  if (IsContextLost()) {
    return false;
  }

  if (!fb) {
    // WebVR fallback
    return PresentScreenBuffer(aTargetScreen);
  }

  mDrawCallsSinceLastFlush = 0;

  const auto& screen = aTargetScreen ? aTargetScreen : gl->Screen();
  bool needsResize = mForceResizeOnPresent;
  needsResize |= !screen->IsReadBufferReady() || (screen->Size() != fb->mSize);
  if (needsResize && !screen->Resize(fb->mSize)) {
    GenerateWarning("screen->Resize failed. Losing context.");
    LoseContext();
    return false;
  }

  mForceResizeOnPresent = false;

  gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, 0);
  BlitBackbufferToCurDriverFB(fb);

  if (!screen->PublishFrame(screen->Size())) {
    GenerateWarning("PublishFrame failed. Losing context.");
    LoseContext();
    return false;
  }

  mResolvedDefaultFB = nullptr;

  OnEndOfFrame();

  return true;
}

RefPtr<gfx::DataSourceSurface> GetTempSurface(const gfx::IntSize& aSize,
                                              gfx::SurfaceFormat& aFormat) {
  uint32_t stride =
      gfx::GetAlignedStride<8>(aSize.width, BytesPerPixel(aFormat));
  return gfx::Factory::CreateDataSourceSurfaceWithStride(aSize, aFormat,
                                                         stride);
}

void WriteFrontToFile(gl::GLContext* gl, gl::GLScreenBuffer* screen,
                      const char* fname, bool needsPremult) {
  auto frontbuffer = screen->Front()->Surf();
  const auto& readSize = frontbuffer->mSize;
  auto format = frontbuffer->mHasAlpha ? gfx::SurfaceFormat::B8G8R8A8
                                       : gfx::SurfaceFormat::B8G8R8X8;
  RefPtr<gfx::DataSourceSurface> resultSurf = GetTempSurface(readSize, format);
  if (NS_WARN_IF(!resultSurf)) {
    MOZ_ASSERT_UNREACHABLE("FAIL");
    return;
  }

  if (!gl->Readback(frontbuffer, resultSurf)) {
    NS_WARNING("Failed to read back canvas surface.");
    MOZ_ASSERT_UNREACHABLE("FAIL");
    return;
  }
  if (needsPremult) {
    gfxUtils::PremultiplyDataSurface(resultSurf, resultSurf);
  }
  MOZ_ASSERT(resultSurf);
  gfxUtils::WriteAsPNG(resultSurf, fname);
}

bool WebGLContext::Present() {
  if (!PresentScreenBuffer()) {
    return false;
  }

  if (XRE_IsContentProcess()) {
    // That's all!
    return true;
  }

  // Set the CompositableHost to use the front buffer as the display,
  auto flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
  if ((!IsPremultAlpha()) && mOptions.alpha) {
    flags |= layers::TextureFlags::NON_PREMULTIPLIED;
  }

  const auto& screen = gl->Screen();
  if (!screen->Front()->Surf()) {
    GenerateWarning(
        "Present failed due to missing front buffer. Losing context.");
    LoseContext();
    return false;
  }

  if (mBackend == layers::LayersBackend::LAYERS_NONE) {
    GenerateWarning(
        "Present was not given a valid compositor layer type. Losing context.");
    LoseContext();
    return false;
  }

  // TODO: I probably need to hold onto screen->Front()->Surf() somehow
  layers::SurfaceDescriptor surfaceDescriptor;
  screen->Front()->Surf()->ToSurfaceDescriptor(&surfaceDescriptor);

  if (!mCompositableHost) {
    return false;
  }

  wr::MaybeExternalImageId noExternalImageId = Nothing();
  RefPtr<layers::TextureHost> host = layers::TextureHost::Create(
      surfaceDescriptor, null_t(), nullptr, mBackend, flags, noExternalImageId);

  if (!host) {
    GenerateWarning("Present failed to create TextuteHost. Losing context.");
    LoseContext();
    return false;
  }

  AutoTArray<layers::CompositableHost::TimedTexture, 1> textures;
  const auto t = textures.AppendElement();
  t->mTexture = host;
  t->mTimeStamp = TimeStamp::Now();
  t->mPictureRect = nsIntRect(nsIntPoint(0, 0), nsIntSize(host->GetSize()));
  t->mFrameID = 0;
  t->mProducerID = 0;
  mCompositableHost->UseTextureHost(textures);
  return true;
}

void WebGLContext::DummyReadFramebufferOperation() {
  if (!mBoundReadFramebuffer) return;  // Infallible.

  const auto status = mBoundReadFramebuffer->CheckFramebufferStatus();
  if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE) {
    ErrorInvalidFramebufferOperation("Framebuffer must be complete.");
  }
}

bool WebGLContext::Has64BitTimestamps() const {
  // 'sync' provides glGetInteger64v either by supporting ARB_sync, GL3+, or
  // GLES3+.
  return gl->IsSupported(gl::GLFeature::sync);
}

static bool CheckContextLost(gl::GLContext* gl, bool* const out_isGuilty) {
  MOZ_ASSERT(gl);

  const auto resetStatus = gl->fGetGraphicsResetStatus();
  if (resetStatus == LOCAL_GL_NO_ERROR) {
    *out_isGuilty = false;
    return false;
  }

  // Assume guilty unless we find otherwise!
  bool isGuilty = true;
  switch (resetStatus) {
    case LOCAL_GL_INNOCENT_CONTEXT_RESET_ARB:
    case LOCAL_GL_PURGED_CONTEXT_RESET_NV:
      // Either nothing wrong, or not our fault.
      isGuilty = false;
      break;
    case LOCAL_GL_GUILTY_CONTEXT_RESET_ARB:
      NS_WARNING(
          "WebGL content on the page definitely caused the graphics"
          " card to reset.");
      break;
    case LOCAL_GL_UNKNOWN_CONTEXT_RESET_ARB:
      NS_WARNING(
          "WebGL content on the page might have caused the graphics"
          " card to reset");
      // If we can't tell, assume not-guilty.
      // Todo: Implement max number of "unknown" resets per document or time.
      isGuilty = false;
      break;
    default:
      gfxCriticalError() << "Unexpected glGetGraphicsResetStatus: "
                         << gfx::hexa(resetStatus);
      break;
  }

  if (isGuilty) {
    NS_WARNING(
        "WebGL context on this page is considered guilty, and will"
        " not be restored.");
  }

  *out_isGuilty = isGuilty;
  return true;
}

void WebGLContext::RunContextLossTimer() { mContextLossHandler.RunTimer(); }

// We use this timer for many things. Here are the things that it is activated
// for:
// 1) If a script is using the MOZ_WEBGL_lose_context extension.
// 2) If we are using EGL and _NOT ANGLE_, we query periodically to see if the
//    CONTEXT_LOST_WEBGL error has been triggered.
// 3) If we are using ANGLE, or anything that supports ARB_robustness, query the
//    GPU periodically to see if the reset status bit has been set.
// In all of these situations, we use this timer to send the script context lost
// and restored events asynchronously. For example, if it triggers a context
// loss, the webglcontextlost event will be sent to it the next time the
// robustness timer fires.
// Note that this timer mechanism is not used unless one of these 3 criteria are
// met.
// At a bare minimum, from context lost to context restores, it would take 3
// full timer iterations: detection, webglcontextlost, webglcontextrestored.
void WebGLContext::CheckForContextLoss() {
  bool isGuilty = true;
  const auto isContextLost = CheckContextLost(gl, &isGuilty);
  if (!isContextLost) return;

  mWebGLError = LOCAL_GL_CONTEXT_LOST;

  auto reason = webgl::ContextLossReason::None;
  if (isGuilty) {
    reason = webgl::ContextLossReason::Guilty;
  }
  LoseContext(reason);
}

void WebGLContext::LoseContext(const webgl::ContextLossReason reason) {
  printf_stderr("WebGL(%p)::LoseContext(%u)\n", this,
                static_cast<uint32_t>(reason));
  mIsContextLost = true;
  mHost->OnContextLoss(reason);
}

void WebGLContext::DidRefresh() {
  if (gl) {
    gl->FlushIfHeavyGLCallsSinceLastFlush();
  }
}

////////////////////////////////////////////////////////////////////////////////

uvec2 WebGLContext::DrawingBufferSize() {
  const FuncScope funcScope(*this, "width/height");
  if (IsContextLost()) return {};

  if (!EnsureDefaultFB()) return {};

  return *uvec2::FromSize(mDefaultFB->mSize);
}

bool WebGLContext::ValidateAndInitFB(const WebGLFramebuffer* const fb,
                                     const GLenum incompleteFbError) {
  if (fb) return fb->ValidateAndInitAttachments(incompleteFbError);

  if (!EnsureDefaultFB()) return false;

  if (mDefaultFB_IsInvalid) {
    // Clear it!
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mDefaultFB->mFB);
    const webgl::ScopedPrepForResourceClear scopedPrep(*this);
    if (!mOptions.alpha) {
      gl->fClearColor(0, 0, 0, 1);
    }
    const GLbitfield bits = LOCAL_GL_COLOR_BUFFER_BIT |
                            LOCAL_GL_DEPTH_BUFFER_BIT |
                            LOCAL_GL_STENCIL_BUFFER_BIT;
    gl->fClear(bits);

    mDefaultFB_IsInvalid = false;
  }
  return true;
}

void WebGLContext::DoBindFB(const WebGLFramebuffer* const fb,
                            const GLenum target) const {
  const GLenum driverFB = fb ? fb->mGLName : mDefaultFB->mFB;
  gl->fBindFramebuffer(target, driverFB);
}

bool WebGLContext::BindCurFBForDraw() {
  const auto& fb = mBoundDrawFramebuffer;
  if (!ValidateAndInitFB(fb)) return false;

  DoBindFB(fb);
  return true;
}

bool WebGLContext::BindCurFBForColorRead(
    const webgl::FormatUsageInfo** const out_format, uint32_t* const out_width,
    uint32_t* const out_height, const GLenum incompleteFbError) {
  const auto& fb = mBoundReadFramebuffer;

  if (fb) {
    if (!ValidateAndInitFB(fb, incompleteFbError)) return false;
    if (!fb->ValidateForColorRead(out_format, out_width, out_height))
      return false;

    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, fb->mGLName);
    return true;
  }

  if (!BindDefaultFBForRead()) return false;

  if (mDefaultFB_ReadBuffer == LOCAL_GL_NONE) {
    ErrorInvalidOperation(
        "Can't read from backbuffer when readBuffer mode is NONE.");
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

bool WebGLContext::BindDefaultFBForRead() {
  if (!ValidateAndInitFB(nullptr)) return false;

  if (!mDefaultFB->mSamples) {
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mDefaultFB->mFB);
    return true;
  }

  if (!mResolvedDefaultFB) {
    mResolvedDefaultFB =
        gl::MozFramebuffer::Create(gl, mDefaultFB->mSize, 0, false);
    if (!mResolvedDefaultFB) {
      gfxCriticalNote << FuncName() << ": Failed to create mResolvedDefaultFB.";
      return false;
    }
  }

  gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mResolvedDefaultFB->mFB);
  BlitBackbufferToCurDriverFB();

  gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mResolvedDefaultFB->mFB);
  return true;
}

void WebGLContext::DoColorMask(const uint8_t bitmask) const {
  if (mDriverColorMask != bitmask) {
    mDriverColorMask = bitmask;
    gl->fColorMask(
        bool(mDriverColorMask & (1 << 0)), bool(mDriverColorMask & (1 << 1)),
        bool(mDriverColorMask & (1 << 2)), bool(mDriverColorMask & (1 << 3)));
  }
}

////////////////////////////////////////////////////////////////////////////////

ScopedDrawCallWrapper::ScopedDrawCallWrapper(WebGLContext& webgl)
    : mWebGL(webgl) {
  uint8_t driverColorMask = mWebGL.mColorWriteMask;
  bool driverDepthTest = mWebGL.mDepthTestEnabled;
  bool driverStencilTest = mWebGL.mStencilTestEnabled;
  const auto& fb = mWebGL.mBoundDrawFramebuffer;
  if (!fb) {
    if (mWebGL.mDefaultFB_DrawBuffer0 == LOCAL_GL_NONE) {
      driverColorMask = 0;  // Is this well-optimized enough for depth-first
                            // rendering?
    } else {
      driverColorMask &= ~(uint8_t(mWebGL.mNeedsFakeNoAlpha) << 3);
    }
    driverDepthTest &= !mWebGL.mNeedsFakeNoDepth;
    driverStencilTest &= !mWebGL.mNeedsFakeNoStencil;
  } else {
    if (mWebGL.mNeedsFakeNoStencil_UserFBs &&
        fb->DepthAttachment().HasAttachment() &&
        !fb->StencilAttachment().HasAttachment()) {
      driverStencilTest = false;
    }
  }

  const auto& gl = mWebGL.gl;
  mWebGL.DoColorMask(driverColorMask);
  if (mWebGL.mDriverDepthTest != driverDepthTest) {
    // "When disabled, the depth comparison and subsequent possible updates to
    // the
    //  depth buffer value are bypassed and the fragment is passed to the next
    //  operation." [GLES 3.0.5, p177]
    mWebGL.mDriverDepthTest = driverDepthTest;
    gl->SetEnabled(LOCAL_GL_DEPTH_TEST, mWebGL.mDriverDepthTest);
  }
  if (mWebGL.mDriverStencilTest != driverStencilTest) {
    // "When disabled, the stencil test and associated modifications are not
    // made, and
    //  the fragment is always passed." [GLES 3.0.5, p175]
    mWebGL.mDriverStencilTest = driverStencilTest;
    gl->SetEnabled(LOCAL_GL_STENCIL_TEST, mWebGL.mDriverStencilTest);
  }
}

ScopedDrawCallWrapper::~ScopedDrawCallWrapper() {
  if (mWebGL.mBoundDrawFramebuffer) return;

  mWebGL.mResolvedDefaultFB = nullptr;
  mWebGL.mShouldPresent = true;
}

// -

void WebGLContext::ScissorRect::Apply(gl::GLContext& gl) const {
  gl.fScissor(x, y, w, h);
}

////////////////////////////////////////

IndexedBufferBinding::IndexedBufferBinding() : mRangeStart(0), mRangeSize(0) {}

uint64_t IndexedBufferBinding::ByteCount() const {
  if (!mBufferBinding) return 0;

  uint64_t bufferSize = mBufferBinding->ByteLength();
  if (!mRangeSize)  // BindBufferBase
    return bufferSize;

  if (mRangeStart >= bufferSize) return 0;
  bufferSize -= mRangeStart;

  return std::min(bufferSize, mRangeSize);
}

////////////////////////////////////////

ScopedUnpackReset::ScopedUnpackReset(const WebGLContext* const webgl)
    : mWebGL(webgl) {
  const auto& gl = mWebGL->gl;
  // clang-format off
  if (mWebGL->mPixelStore.mUnpackAlignment != 4) gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 4);

  if (mWebGL->IsWebGL2()) {
    if (mWebGL->mPixelStore.mUnpackRowLength   != 0) gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH  , 0);
    if (mWebGL->mPixelStore.mUnpackImageHeight != 0) gl->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, 0);
    if (mWebGL->mPixelStore.mUnpackSkipPixels  != 0) gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_PIXELS , 0);
    if (mWebGL->mPixelStore.mUnpackSkipRows    != 0) gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS   , 0);
    if (mWebGL->mPixelStore.mUnpackSkipImages  != 0) gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES , 0);

    if (mWebGL->mBoundPixelUnpackBuffer) gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
  }
  // clang-format on
}

ScopedUnpackReset::~ScopedUnpackReset() {
  const auto& gl = mWebGL->gl;
  // clang-format off
  gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, mWebGL->mPixelStore.mUnpackAlignment);

  if (mWebGL->IsWebGL2()) {
    gl->fPixelStorei(LOCAL_GL_UNPACK_ROW_LENGTH  , mWebGL->mPixelStore.mUnpackRowLength  );
    gl->fPixelStorei(LOCAL_GL_UNPACK_IMAGE_HEIGHT, mWebGL->mPixelStore.mUnpackImageHeight);
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_PIXELS , mWebGL->mPixelStore.mUnpackSkipPixels );
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_ROWS   , mWebGL->mPixelStore.mUnpackSkipRows   );
    gl->fPixelStorei(LOCAL_GL_UNPACK_SKIP_IMAGES , mWebGL->mPixelStore.mUnpackSkipImages );

    GLuint pbo = 0;
    if (mWebGL->mBoundPixelUnpackBuffer) {
        pbo = mWebGL->mBoundPixelUnpackBuffer->mGLName;
    }

    gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, pbo);
  }
  // clang-format on
}

////////////////////

ScopedFBRebinder::~ScopedFBRebinder() {
  const auto fnName = [&](WebGLFramebuffer* fb) {
    return fb ? fb->mGLName : 0;
  };

  const auto& gl = mWebGL->gl;
  if (mWebGL->IsWebGL2()) {
    gl->fBindFramebuffer(LOCAL_GL_DRAW_FRAMEBUFFER,
                         fnName(mWebGL->mBoundDrawFramebuffer));
    gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER,
                         fnName(mWebGL->mBoundReadFramebuffer));
  } else {
    MOZ_ASSERT(mWebGL->mBoundDrawFramebuffer == mWebGL->mBoundReadFramebuffer);
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER,
                         fnName(mWebGL->mBoundDrawFramebuffer));
  }
}

////////////////////

void DoBindBuffer(gl::GLContext& gl, const GLenum target,
                  const WebGLBuffer* const buffer) {
  gl.fBindBuffer(target, buffer ? buffer->mGLName : 0);
}

////////////////////////////////////////

bool Intersect(const int32_t srcSize, const int32_t read0,
               const int32_t readSize, int32_t* const out_intRead0,
               int32_t* const out_intWrite0, int32_t* const out_intSize) {
  MOZ_ASSERT(srcSize >= 0);
  MOZ_ASSERT(readSize >= 0);
  const auto read1 = int64_t(read0) + readSize;

  int32_t intRead0 = read0;  // Clearly doesn't need validation.
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
        !CheckedInt<int32_t>(intSize).isValid()) {
      return false;
    }
  }

  *out_intRead0 = intRead0;
  *out_intWrite0 = intWrite0;
  *out_intSize = intSize;
  return true;
}

// --

uint64_t AvailGroups(const uint64_t totalAvailItems,
                     const uint64_t firstItemOffset, const uint32_t groupSize,
                     const uint32_t groupStride) {
  MOZ_ASSERT(groupSize && groupStride);
  MOZ_ASSERT(groupSize <= groupStride);

  if (totalAvailItems <= firstItemOffset) return 0;
  const size_t availItems = totalAvailItems - firstItemOffset;

  size_t availGroups = availItems / groupStride;
  const size_t tailItems = availItems % groupStride;
  if (tailItems >= groupSize) {
    availGroups += 1;
  }
  return availGroups;
}

////////////////////////////////////////////////////////////////////////////////

CheckedUint32 WebGLContext::GetUnpackSize(bool isFunc3D, uint32_t width,
                                          uint32_t height, uint32_t depth,
                                          uint8_t bytesPerPixel) {
  if (!width || !height || !depth) return 0;

  ////////////////

  const auto& maybeRowLength = mPixelStore.mUnpackRowLength;
  const auto& maybeImageHeight = mPixelStore.mUnpackImageHeight;

  const auto usedPixelsPerRow =
      CheckedUint32(mPixelStore.mUnpackSkipPixels) + width;
  const auto stridePixelsPerRow =
      (maybeRowLength ? CheckedUint32(maybeRowLength) : usedPixelsPerRow);

  const auto usedRowsPerImage =
      CheckedUint32(mPixelStore.mUnpackSkipRows) + height;
  const auto strideRowsPerImage =
      (maybeImageHeight ? CheckedUint32(maybeImageHeight) : usedRowsPerImage);

  const uint32_t skipImages = (isFunc3D ? mPixelStore.mUnpackSkipImages : 0);
  const CheckedUint32 usedImages = CheckedUint32(skipImages) + depth;

  ////////////////

  CheckedUint32 strideBytesPerRow = bytesPerPixel * stridePixelsPerRow;
  strideBytesPerRow =
      RoundUpToMultipleOf(strideBytesPerRow, mPixelStore.mUnpackAlignment);

  const CheckedUint32 strideBytesPerImage =
      strideBytesPerRow * strideRowsPerImage;

  ////////////////

  CheckedUint32 usedBytesPerRow = bytesPerPixel * usedPixelsPerRow;
  // Don't round this to the alignment, since alignment here is really just used
  // for establishing stride, particularly in WebGL 1, where you can't set
  // ROW_LENGTH.

  CheckedUint32 totalBytes = strideBytesPerImage * (usedImages - 1);
  totalBytes += strideBytesPerRow * (usedRowsPerImage - 1);
  totalBytes += usedBytesPerRow;

  return totalBytes;
}

void WebGLContext::ClearVRFrame() {
#if defined(MOZ_WIDGET_ANDROID)
  mVRScreen = nullptr;
#endif
}

RefPtr<layers::SharedSurfaceTextureClient> WebGLContext::GetVRFrame(
    WebGLFramebuffer* fb) {
  if (!gl) return nullptr;

  EnsureVRReady();
  const gl::MozFramebuffer* maybeFB = nullptr;
  if (fb) {
    maybeFB = fb->mOpaque.get();
    MOZ_ASSERT(maybeFB);
  }

  UniquePtr<gl::GLScreenBuffer>* maybeVrScreen = nullptr;
#if defined(MOZ_WIDGET_ANDROID)
  maybeVrScreen = &mVRScreen;
#endif
  RefPtr<layers::SharedSurfaceTextureClient> sharedSurface;

  if (maybeVrScreen) {
    auto& vrScreen = *maybeVrScreen;
    // Create a custom GLScreenBuffer for VR.
    if (!vrScreen) {
      auto caps = gl->Screen()->mCaps;
      vrScreen = gl::GLScreenBuffer::Create(gl, gfx::IntSize(1, 1), caps);
      RefPtr<layers::ImageBridgeChild> imageBridge =
          layers::ImageBridgeChild::GetSingleton();
      if (imageBridge) {
        layers::TextureFlags flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
        UniquePtr<gl::SurfaceFactory> factory =
            gl::GLScreenBuffer::CreateFactory(gl, caps, imageBridge.get(),
                                              flags);
        vrScreen->Morph(std::move(factory));
      }
    }
    MOZ_ASSERT(vrScreen);

    // Swap buffers as though composition has occurred.
    // We will then share the resulting front buffer to be submitted to the VR
    // compositor.
    PresentScreenBufferVR(vrScreen.get(), maybeFB);

    if (IsContextLost()) return nullptr;

    sharedSurface = vrScreen->Front();
    if (!sharedSurface || !sharedSurface->Surf() ||
        !sharedSurface->Surf()->IsBufferAvailable())
      return nullptr;

    // Make sure that the WebGL buffer is committed to the attached
    // SurfaceTexture on Android.
    sharedSurface->Surf()->ProducerAcquire();
    sharedSurface->Surf()->Commit();
    sharedSurface->Surf()->ProducerRelease();
  } else {
    /**
     * Swap buffers as though composition has occurred.
     * We will then share the resulting front buffer to be submitted to the VR
     * compositor.
     */
    PresentScreenBufferVR(nullptr, maybeFB);

    gl::GLScreenBuffer* screen = gl->Screen();
    if (!screen) return nullptr;

    sharedSurface = screen->Front();
    if (!sharedSurface) return nullptr;
  }
  return sharedSurface;
}

void WebGLContext::EnsureVRReady() {
  if (mVRReady) {
    return;
  }

  // Make not composited canvases work with WebVR. See bug #1492554
  // WebGLContext::InitializeCanvasRenderer is only called when the 2D
  // compositor renders a WebGL canvas for the first time. This causes canvases
  // not added to the DOM not to work properly with WebVR. Here we mimic what
  // InitializeCanvasRenderer does internally as a workaround.
  const auto caps = gl->Screen()->mCaps;
  auto flags = layers::TextureFlags::ORIGIN_BOTTOM_LEFT;
  if (!IsPremultAlpha() && mOptions.alpha) {
    flags |= layers::TextureFlags::NON_PREMULTIPLIED;
  }
  RefPtr<layers::ImageBridgeChild> imageBridge =
      layers::ImageBridgeChild::GetSingleton();
  if (!imageBridge) {
    return;
  }
  auto factory =
      gl::GLScreenBuffer::CreateFactory(gl, caps, imageBridge.get(), flags);
  gl->Screen()->Morph(std::move(factory));

  bool needsResize = false;
#if defined(MOZ_WIDGET_ANDROID)
  // On Android we are using a different GLScreenBuffer for WebVR, so we need
  // a resize here because PresentScreenBuffer() may not be called for the
  // gl->Screen() after we set the new factory.
  needsResize = true;
#endif
  if (needsResize) {
    const auto& size = DrawingBufferSize();
    gl->Screen()->Resize({size.x, size.y});
  }
  mVRReady = true;
}

////////////////////////////////////////////////////////////////////////////////

const char* WebGLContext::FuncName() const {
  const char* ret;
  if (MOZ_LIKELY(mFuncScope)) {
    ret = mFuncScope->mFuncName;
  } else {
    MOZ_ASSERT(false, "FuncScope not on stack!");
    ret = "<funcName unknown>";
  }
  return ret;
}

// -

WebGLContext::FuncScope::FuncScope(const WebGLContext& webgl,
                                   const char* const funcName)
    : mWebGL(webgl), mFuncName(bool(mWebGL.mFuncScope) ? nullptr : funcName) {
  if (!mFuncName) return;
  mWebGL.mFuncScope = this;
}

WebGLContext::FuncScope::~FuncScope() {
  if (mBindFailureGuard) {
    gfxCriticalError() << "mBindFailureGuard failure: Early exit from "
                       << mWebGL.FuncName();
  }

  if (!mFuncName) return;
  mWebGL.mFuncScope = nullptr;
}

// --

bool ClientWebGLContext::IsXRCompatible() const {
  if (!mNotLost) return false;
  const auto& options = mNotLost->info.options;
  return options.xrCompatible;
}

already_AddRefed<dom::Promise> ClientWebGLContext::MakeXRCompatible(
    ErrorResult& aRv) {
  const FuncScope funcScope(*this, "MakeXRCompatible");
  nsCOMPtr<nsIGlobalObject> global;
  // TODO: Bug 1596921
  // Should use nsICanvasRenderingContextInternal::GetParentObject
  // once it has been updated to work in the offscreencanvas case
  if (mCanvasElement) {
    global = GetOwnerDoc()->GetScopeObject();
  } else if (mOffscreenCanvas) {
    global = mOffscreenCanvas->GetOwnerGlobal();
  }
  if (!global) {
    aRv.ThrowInvalidAccessError(
        "Using a WebGL context that is not attached to either a canvas or an "
        "OffscreenCanvas");
    return nullptr;
  }
  RefPtr<dom::Promise> promise = dom::Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  if (IsContextLost()) {
    promise->MaybeRejectWithInvalidStateError(
        "Can not make context XR compatible when context is already lost.");
    return promise.forget();
  }

  // TODO: Bug 1580258 - WebGLContext.MakeXRCompatible needs to switch to
  //                     the device connected to the XR hardware
  // This should update `options` and lose+restore the context.

  promise->MaybeResolveWithUndefined();
  return promise.forget();
}

// --

webgl::AvailabilityRunnable* WebGLContext::EnsureAvailabilityRunnable() {
  if (!mAvailabilityRunnable) {
    RefPtr<webgl::AvailabilityRunnable> runnable =
        new webgl::AvailabilityRunnable(this);

    NS_DispatchToCurrentThread(runnable.forget());
  }
  return mAvailabilityRunnable;
}

webgl::AvailabilityRunnable::AvailabilityRunnable(WebGLContext* const webgl)
    : Runnable("webgl::AvailabilityRunnable"), mWebGL(webgl) {
  mWebGL->mAvailabilityRunnable = this;
}

webgl::AvailabilityRunnable::~AvailabilityRunnable() {
  MOZ_ASSERT(mQueries.empty());
  MOZ_ASSERT(mSyncs.empty());
}

nsresult webgl::AvailabilityRunnable::Run() {
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

// -

void WebGLContext::GenerateErrorImpl(const GLenum err,
                                     const std::string& text) const {
  if (mFuncScope && mFuncScope->mBindFailureGuard) {
    gfxCriticalError() << "mBindFailureGuard failure: Generating error "
                       << EnumString(err) << ": " << text;
  }
  /* ES2 section 2.5 "GL Errors" states that implementations can have
   * multiple 'flags', as errors might be caught in different parts of
   * a distributed implementation.
   * We're signing up as a distributed implementation here, with
   * separate flags for WebGL and the underlying GLContext.
   */
  if (!mWebGLError) mWebGLError = err;

  if (!mHost) return;  // Impossible?

  if (!ShouldGenerateWarnings()) return;

  mHost->JsWarning(text);
  mWarningCount += 1;

  if (!ShouldGenerateWarnings()) {
    auto info = std::string(
        "WebGL: No further warnings will be reported for this WebGL "
        "context. (already reported ");
    info += std::to_string(mWarningCount);
    info += " warnings)";
    mHost->JsWarning(info);
  }
}

// -

Maybe<std::string> WebGLContext::GetString(const GLenum pname) const {
  const WebGLContext::FuncScope funcScope(*this, "getParameter");
  if (IsContextLost()) return {};

  switch (pname) {
    case LOCAL_GL_EXTENSIONS: {
      if (!gl->IsCoreProfile()) {
        const auto rawExt = (const char*)gl->fGetString(LOCAL_GL_EXTENSIONS);
        return Some(std::string(rawExt));
      }
      std::string ret;
      const auto& numExts = gl->GetIntAs<GLuint>(LOCAL_GL_NUM_EXTENSIONS);
      for (GLuint i = 0; i < numExts; i++) {
        const auto rawExt =
            (const char*)gl->fGetStringi(LOCAL_GL_EXTENSIONS, i);
        if (i > 0) {
          ret += " ";
        }
        ret += rawExt;
      }
      return Some(std::move(ret));
    }

    case LOCAL_GL_RENDERER:
    case LOCAL_GL_VENDOR:
    case LOCAL_GL_VERSION: {
      const auto raw = (const char*)gl->fGetString(pname);
      return Some(std::string(raw));
    }

    case dom::MOZ_debug_Binding::WSI_INFO: {
      nsCString info;
      gl->GetWSIInfo(&info);
      return Some(std::string(info.BeginReading()));
    }

    default:
      ErrorInvalidEnumArg("pname", pname);
      return {};
  }
}

// ---------------------------------

Maybe<webgl::IndexedName> webgl::ParseIndexed(const std::string& str) {
  static const std::regex kRegex("(.*)\\[([0-9]+)\\]");

  std::smatch match;
  if (!std::regex_match(str, match, kRegex)) return {};

  const auto index = std::stoull(match[2]);
  return Some(webgl::IndexedName{match[1], index});
}

// ExplodeName("foo.bar[3].x") -> ["foo", ".", "bar", "[", "3", "]", ".", "x"]
static std::vector<std::string> ExplodeName(const std::string& str) {
  std::vector<std::string> ret;

  static const std::regex kSep("[.[\\]]");

  auto itr = std::regex_token_iterator<decltype(str.begin())>(
      str.begin(), str.end(), kSep, {-1, 0});
  const auto end = decltype(itr)();

  for (; itr != end; ++itr) {
    const auto& part = itr->str();
    if (part.size()) {
      ret.push_back(part);
    }
  }
  return ret;
}

//-

//#define DUMP_MakeLinkResult

webgl::LinkActiveInfo GetLinkActiveInfo(
    gl::GLContext& gl, const GLuint prog, const bool webgl2,
    const std::unordered_map<std::string, std::string>& nameUnmap) {
  webgl::LinkActiveInfo ret;
  [&]() {
    const auto fnGetProgramui = [&](const GLenum pname) {
      GLint ret = 0;
      gl.fGetProgramiv(prog, pname, &ret);
      return static_cast<uint32_t>(ret);
    };

    std::vector<char> stringBuffer(1);
    const auto fnEnsureCapacity = [&](const GLenum pname) {
      const auto maxWithNull = fnGetProgramui(pname);
      if (maxWithNull > stringBuffer.size()) {
        stringBuffer.resize(maxWithNull);
      }
    };

    fnEnsureCapacity(LOCAL_GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);
    fnEnsureCapacity(LOCAL_GL_ACTIVE_UNIFORM_MAX_LENGTH);
    if (webgl2) {
      fnEnsureCapacity(LOCAL_GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH);
      fnEnsureCapacity(LOCAL_GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH);
    }

    // -

    const auto fnUnmapName = [&](const std::string& mappedName) {
      const auto parts = ExplodeName(mappedName);

      std::ostringstream ret;
      for (const auto& part : parts) {
        const auto maybe = MaybeFind(nameUnmap, part);
        if (maybe) {
          ret << *maybe;
        } else {
          ret << part;
        }
      }
      return ret.str();
    };

    // -

    {
      const auto count = fnGetProgramui(LOCAL_GL_ACTIVE_ATTRIBUTES);
      ret.activeAttribs.reserve(count);
      for (const auto i : IntegerRange(count)) {
        GLsizei lengthWithoutNull = 0;
        GLint elemCount = 0;  // `size`
        GLenum elemType = 0;  // `type`
        gl.fGetActiveAttrib(prog, i, stringBuffer.size(), &lengthWithoutNull,
                            &elemCount, &elemType, stringBuffer.data());
        if (!elemType) {
          const auto error = gl.fGetError();
          if (error != LOCAL_GL_CONTEXT_LOST) {
            gfxCriticalError() << "Failed to do glGetActiveAttrib: " << error;
          }
          return;
        }
        const auto mappedName =
            std::string(stringBuffer.data(), lengthWithoutNull);
        const auto userName = fnUnmapName(mappedName);

        auto loc = gl.fGetAttribLocation(prog, mappedName.c_str());
        if (mappedName.find("gl_") == 0) {
          // Bug 1328559: Appears problematic on ANGLE and OSX, but not Linux or
          // Win+GL.
          loc = -1;
        }

#ifdef DUMP_MakeLinkResult
        printf_stderr("[attrib %u/%u] @%i %s->%s\n", i, count, loc,
                      userName.c_str(), mappedName.c_str());
#endif
        webgl::ActiveAttribInfo info;
        info.elemType = elemType;
        info.elemCount = elemCount;
        info.name = userName;
        info.location = loc;
        info.baseType = webgl::ToAttribBaseType(info.elemType);
        ret.activeAttribs.push_back(std::move(info));
      }
    }

    // -

    {
      const auto count = fnGetProgramui(LOCAL_GL_ACTIVE_UNIFORMS);
      ret.activeUniforms.reserve(count);

      std::vector<GLint> blockIndexList(count, -1);
      std::vector<GLint> blockOffsetList(count, -1);
      std::vector<GLint> blockArrayStrideList(count, -1);
      std::vector<GLint> blockMatrixStrideList(count, -1);
      std::vector<GLint> blockIsRowMajorList(count, 0);

      if (webgl2 && count) {
        std::vector<GLuint> activeIndices;
        activeIndices.reserve(count);
        for (const auto i : IntegerRange(count)) {
          activeIndices.push_back(i);
        }

        gl.fGetActiveUniformsiv(
            prog, activeIndices.size(), activeIndices.data(),
            LOCAL_GL_UNIFORM_BLOCK_INDEX, blockIndexList.data());

        gl.fGetActiveUniformsiv(prog, activeIndices.size(),
                                activeIndices.data(), LOCAL_GL_UNIFORM_OFFSET,
                                blockOffsetList.data());

        gl.fGetActiveUniformsiv(
            prog, activeIndices.size(), activeIndices.data(),
            LOCAL_GL_UNIFORM_ARRAY_STRIDE, blockArrayStrideList.data());

        gl.fGetActiveUniformsiv(
            prog, activeIndices.size(), activeIndices.data(),
            LOCAL_GL_UNIFORM_MATRIX_STRIDE, blockMatrixStrideList.data());

        gl.fGetActiveUniformsiv(
            prog, activeIndices.size(), activeIndices.data(),
            LOCAL_GL_UNIFORM_IS_ROW_MAJOR, blockIsRowMajorList.data());
      }

      for (const auto i : IntegerRange(count)) {
        GLsizei lengthWithoutNull = 0;
        GLint elemCount = 0;  // `size`
        GLenum elemType = 0;  // `type`
        gl.fGetActiveUniform(prog, i, stringBuffer.size(), &lengthWithoutNull,
                             &elemCount, &elemType, stringBuffer.data());
        if (!elemType) {
          const auto error = gl.fGetError();
          if (error != LOCAL_GL_CONTEXT_LOST) {
            gfxCriticalError() << "Failed to do glGetActiveUniform: " << error;
          }
          return;
        }
        auto mappedName = std::string(stringBuffer.data(), lengthWithoutNull);

        // Get true name

        auto baseMappedName = mappedName;

        const bool isArray = [&]() {
          const auto maybe = webgl::ParseIndexed(mappedName);
          if (maybe) {
            MOZ_ASSERT(maybe->index == 0);
            baseMappedName = std::move(maybe->name);
            return true;
          }
          return false;
        }();

        const auto userName = fnUnmapName(mappedName);

        // -

        webgl::ActiveUniformInfo info;
        info.elemType = elemType;
        info.elemCount = static_cast<uint32_t>(elemCount);
        info.name = userName;
        info.block_index = blockIndexList[i];
        info.block_offset = blockOffsetList[i];
        info.block_arrayStride = blockArrayStrideList[i];
        info.block_matrixStride = blockMatrixStrideList[i];
        info.block_isRowMajor = bool(blockIsRowMajorList[i]);

#ifdef DUMP_MakeLinkResult
        printf_stderr("[uniform %u/%u] %s->%s\n", i + 1, count,
                      userName.c_str(), mappedName.c_str());
#endif

        // Get uniform locations
        {
          auto locName = baseMappedName;
          const auto baseLength = locName.size();
          for (const auto i : IntegerRange(info.elemCount)) {
            if (isArray) {
              locName.erase(
                  baseLength);  // Erase previous [N], but retain capacity.
              locName += '[';
              locName += std::to_string(i);
              locName += ']';
            }
            const auto loc = gl.fGetUniformLocation(prog, locName.c_str());
            if (loc != -1) {
              info.locByIndex[i] = static_cast<uint32_t>(loc);
#ifdef DUMP_MakeLinkResult
              printf_stderr("   [%u] @%i\n", i, loc);
#endif
            }
          }
        }  // anon

        ret.activeUniforms.push_back(std::move(info));
      }  // for i
    }    // anon

    if (webgl2) {
      // -------------------------------------
      // active uniform blocks
      {
        const auto count = fnGetProgramui(LOCAL_GL_ACTIVE_UNIFORM_BLOCKS);
        ret.activeUniformBlocks.reserve(count);

        for (const auto i : IntegerRange(count)) {
          GLsizei lengthWithoutNull = 0;
          gl.fGetActiveUniformBlockName(prog, i, stringBuffer.size(),
                                        &lengthWithoutNull,
                                        stringBuffer.data());
          const auto mappedName =
              std::string(stringBuffer.data(), lengthWithoutNull);
          const auto userName = fnUnmapName(mappedName);

          // -

          auto info = webgl::ActiveUniformBlockInfo{userName};
          GLint val = 0;

          gl.fGetActiveUniformBlockiv(prog, i, LOCAL_GL_UNIFORM_BLOCK_DATA_SIZE,
                                      &val);
          info.dataSize = static_cast<uint32_t>(val);

          gl.fGetActiveUniformBlockiv(
              prog, i, LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &val);
          info.activeUniformIndices.resize(val);
          gl.fGetActiveUniformBlockiv(
              prog, i, LOCAL_GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
              reinterpret_cast<GLint*>(info.activeUniformIndices.data()));

          gl.fGetActiveUniformBlockiv(
              prog, i, LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,
              &val);
          info.referencedByVertexShader = bool(val);

          gl.fGetActiveUniformBlockiv(
              prog, i, LOCAL_GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER,
              &val);
          info.referencedByFragmentShader = bool(val);

          ret.activeUniformBlocks.push_back(std::move(info));
        }  // for i
      }    // anon

      // -------------------------------------
      // active tf varyings
      {
        const auto count = fnGetProgramui(LOCAL_GL_TRANSFORM_FEEDBACK_VARYINGS);
        ret.activeTfVaryings.reserve(count);

        for (const auto i : IntegerRange(count)) {
          GLsizei lengthWithoutNull = 0;
          GLsizei elemCount = 0;  // `size`
          GLenum elemType = 0;    // `type`
          gl.fGetTransformFeedbackVarying(prog, i, stringBuffer.size(),
                                          &lengthWithoutNull, &elemCount,
                                          &elemType, stringBuffer.data());
          const auto mappedName =
              std::string(stringBuffer.data(), lengthWithoutNull);
          const auto userName = fnUnmapName(mappedName);

          ret.activeTfVaryings.push_back(
              {elemType, static_cast<uint32_t>(elemCount), userName});
        }
      }
    }  // if webgl2
  }();
  return ret;
}

webgl::CompileResult WebGLContext::GetCompileResult(
    const WebGLShader& shader) const {
  webgl::CompileResult ret;
  [&]() {
    ret.pending = false;
    const auto& info = shader.CompileResults();
    if (!info) return;
    if (!info->mValid) {
      ret.log = info->mInfoLog;
      return;
    }
    ret.translatedSource = info->mObjectCode;
    ret.log = shader.CompileLog();
    if (!shader.IsCompiled()) return;
    ret.success = true;
  }();
  return ret;
}

webgl::LinkResult WebGLContext::GetLinkResult(const WebGLProgram& prog) const {
  webgl::LinkResult ret;
  [&]() {
    ret.pending = false;  // Link status polling not yet implemented.
    ret.log = prog.LinkLog();
    const auto& info = prog.LinkInfo();
    if (!info) return;
    ret.success = true;
    ret.active = info->active;
    ret.tfBufferMode = info->transformFeedbackBufferMode;
  }();
  return ret;
}

// -

GLint WebGLContext::GetFragDataLocation(const WebGLProgram& prog,
                                        const std::string& userName) const {
  const auto err = CheckGLSLVariableName(IsWebGL2(), userName);
  if (err) {
    GenerateError(err->type, "%s", err->info.c_str());
    return -1;
  }

  const auto& info = prog.LinkInfo();
  if (!info) return -1;
  const auto& nameMap = info->nameMap;

  const auto parts = ExplodeName(userName);

  std::ostringstream ret;
  for (const auto& part : parts) {
    const auto maybe = MaybeFind(nameMap, part);
    if (maybe) {
      ret << *maybe;
    } else {
      ret << part;
    }
  }
  const auto mappedName = ret.str();

  return gl->fGetFragDataLocation(prog.mGLName, mappedName.c_str());
}

// -

WebGLContextBoundObject::WebGLContextBoundObject(WebGLContext* webgl)
    : mContext(webgl) {}

}  // namespace mozilla
