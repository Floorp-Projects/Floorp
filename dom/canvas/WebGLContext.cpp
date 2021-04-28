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
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/WebGLContextEvent.h"
#include "mozilla/EnumeratedArrayCycleCollection.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProcessPriorityManager.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/Telemetry.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsError.h"
#include "nsIClassInfoImpl.h"
#include "nsIWidget.h"
#include "nsServiceManagerUtils.h"
#include "SharedSurfaceGL.h"
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

  mQuerySlot_SamplesPassed = nullptr;
  mQuerySlot_TFPrimsWritten = nullptr;
  mQuerySlot_TimeElapsed = nullptr;

  mIndexedUniformBufferBindings.clear();

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

void ClientWebGLContext::MarkCanvasDirty() {
  if (!mCanvasElement) return;

  mCapturedFrameInvalidated = true;

  if (mIsCanvasDirty) return;
  mIsCanvasDirty = true;

  SVGObserverUtils::InvalidateDirectRenderingObservers(mCanvasElement);

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
    FailureReason reason;
    if (!gfx::gfxVars::AllowWebgl2()) {
      reason.info =
          "AllowWebgl2:false restricts context creation on this system.";
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

  if (!gfx::gfxVars::WebglAllowCoreProfile()) {
    flags |= gl::CreateContextFlags::REQUIRE_COMPAT_PROFILE;
  }

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
    FailureReason reason;
    if (!gfx::gfxVars::WebglAllowWindowsNativeGl()) {
      reason.info =
          "WebglAllowWindowsNativeGl:false restricts context creation on this "
          "system.";

      out_failReasons->push_back(reason);

      GenerateWarning("%s", reason.info.BeginReading());
      tryNativeGL = false;
    }
  }

  // --

  typedef decltype(gl::GLContextProviderEGL::CreateHeadless) fnCreateT;
  const auto fnCreate = [&](fnCreateT* const pfnCreate,
                            const char* const info) {
    nsCString failureId;
    const RefPtr<gl::GLContext> gl = pfnCreate({flags}, &failureId);
    if (!gl) {
      out_failReasons->push_back(WebGLContext::FailureReason(failureId, info));
    }
    return gl;
  };

  const auto newGL = [&]() -> RefPtr<gl::GLContext> {
    if (tryNativeGL) {
      if (useEGL)
        return fnCreate(&gl::GLContextProviderEGL::CreateHeadless, "useEGL");

      const auto ret =
          fnCreate(&gl::GLContextProvider::CreateHeadless, "tryNativeGL");
      if (ret) return ret;
    }

    if (tryANGLE) {
      return fnCreate(&gl::GLContextProviderEGL::CreateHeadless, "tryANGLE");
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

  const auto val = StaticPrefs::webgl_debug_incomplete_tex_color();
  if (val) {
    mIncompleteTexOverride.reset(new gl::Texture(*gl));
    const gl::ScopedBindTexture autoBind(gl, mIncompleteTexOverride->name);
    const auto heapVal = std::make_unique<uint32_t>(val);
    gl->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, 1, 1, 0,
                    LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, heapVal.get());
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
  nsCString failureId = "FEATURE_FAILURE_WEBGL_UNKOWN"_ns;
  const bool forceEnabled = StaticPrefs::webgl_force_enabled();
  ScopedGfxFeatureReporter reporter("WebGL", forceEnabled);

  auto res = [&]() -> Result<RefPtr<WebGLContext>, std::string> {
    bool disabled = StaticPrefs::webgl_disabled();

    // TODO: When we have software webgl support we should use that instead.
    disabled |= gfxPlatform::InSafeMode();

    if (disabled) {
      if (gfxPlatform::InSafeMode()) {
        failureId = "FEATURE_FAILURE_WEBGL_SAFEMODE"_ns;
      } else {
        failureId = "FEATURE_FAILURE_WEBGL_DISABLED"_ns;
      }
      return Err("WebGL is currently disabled.");
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
          Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID,
                                "FEATURE_FAILURE_REASON_UNKNOWN"_ns);
        } else {
          Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_FAILURE_ID, cur.key);
        }

        const auto str = nsPrintfCString("\n* %s (%s)", cur.info.BeginReading(),
                                         cur.key.BeginReading());
        text.Append(str);
      }
      failureId = "FEATURE_FAILURE_REASON"_ns;
      return Err(text.BeginReading());
    }
    MOZ_ASSERT(webgl->gl);

    if (desc.options.failIfMajorPerformanceCaveat) {
      if (webgl->gl->IsWARP()) {
        failureId = "FEATURE_FAILURE_WEBGL_PERF_WARP"_ns;
        return Err(
            "failIfMajorPerformanceCaveat: Driver is not"
            " hardware-accelerated.");
      }

#ifdef XP_WIN
      if (webgl->gl->GetContextType() == gl::GLContextType::WGL &&
          !gl::sWGLLib.HasDXInterop2()) {
        failureId = "FEATURE_FAILURE_WEBGL_DXGL_INTEROP2"_ns;
        return Err("failIfMajorPerformanceCaveat: WGL without DXGLInterop2.");
      }
#endif
    }

    const FuncScope funcScope(*webgl, "getContext/restoreContext");

    MOZ_ASSERT(!webgl->mDefaultFB);
    if (!webgl->EnsureDefaultFB()) {
      MOZ_ASSERT(!webgl->mDefaultFB);
      MOZ_ASSERT(webgl->IsContextLost());
      failureId = "FEATURE_FAILURE_WEBGL_BACKBUFFER"_ns;
      return Err("Initializing WebGL backbuffer failed.");
    }

    return webgl;
  }();
  if (res.isOk()) {
    failureId = "SUCCESS"_ns;
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

  // -

  const auto UploadableSdTypes = [&]() {
    webgl::EnumMask<layers::SurfaceDescriptor::Type> types;
    if (webgl->gl->IsANGLE()) {
      types[layers::SurfaceDescriptor::TSurfaceDescriptorD3D10] = true;
      types[layers::SurfaceDescriptor::TSurfaceDescriptorDXGIYCbCr] = true;
    }
    if (kIsMacOS) {
      types[layers::SurfaceDescriptor::TSurfaceDescriptorMacIOSurface] = true;
    }
    return types;
  };

  // -

  out->options = webgl->mOptions;
  out->limits = *webgl->mLimits;
  out->uploadableSdTypes = UploadableSdTypes();

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

  mDrawCallsSinceLastFlush = 0;

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

// -

template <typename T, typename... Args>
constexpr auto MakeArray(Args... args) -> std::array<T, sizeof...(Args)> {
  return {{static_cast<T>(args)...}};
}

// -

// For an overview of how WebGL compositing works, see:
// https://wiki.mozilla.org/Platform/GFX/WebGL/Compositing
bool WebGLContext::PresentInto(gl::SwapChain& swapChain) {
  OnEndOfFrame();

  if (!ValidateAndInitFB(nullptr)) return false;

  {
    auto presenter = swapChain.Acquire(mDefaultFB->mSize);
    if (!presenter) {
      GenerateWarning("Swap chain surface creation failed.");
      LoseContext();
      return false;
    }

    const auto destFb = presenter->Fb();
    gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, destFb);

    BlitBackbufferToCurDriverFB();

    if (!mOptions.preserveDrawingBuffer) {
      if (gl->IsSupported(gl::GLFeature::invalidate_framebuffer)) {
        gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, mDefaultFB->mFB);
        constexpr auto attachments = MakeArray<GLenum>(
            LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);
        gl->fInvalidateFramebuffer(LOCAL_GL_READ_FRAMEBUFFER,
                                   attachments.size(), attachments.data());
      }
      mDefaultFB_IsInvalid = true;
    }

#ifdef DEBUG
    if (!mOptions.alpha) {
      gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, destFb);
      uint32_t pixel = 0xffbadbad;
      gl->fReadPixels(0, 0, 1, 1, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                      &pixel);
      MOZ_ASSERT((pixel & 0xff000000) == 0xff000000);
    }
#endif
  }

  return true;
}

bool WebGLContext::PresentIntoXR(gl::SwapChain& swapChain,
                                 const gl::MozFramebuffer& fb) {
  OnEndOfFrame();

  auto presenter = swapChain.Acquire(fb.mSize);
  if (!presenter) {
    GenerateWarning("Swap chain surface creation failed.");
    LoseContext();
    return false;
  }

  const auto destFb = presenter->Fb();
  gl->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, destFb);

  BlitBackbufferToCurDriverFB(&fb);

  // https://immersive-web.github.io/webxr/#opaque-framebuffer
  // Opaque framebuffers will always be cleared regardless of the
  // associated WebGL context’s preserveDrawingBuffer value.
  if (gl->IsSupported(gl::GLFeature::invalidate_framebuffer)) {
    gl->fBindFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, fb.mFB);
    constexpr auto attachments = MakeArray<GLenum>(
        LOCAL_GL_COLOR_ATTACHMENT0, LOCAL_GL_DEPTH_STENCIL_ATTACHMENT);
    gl->fInvalidateFramebuffer(LOCAL_GL_READ_FRAMEBUFFER, attachments.size(),
                               attachments.data());
  }

  return true;
}

void WebGLContext::Present(WebGLFramebuffer* const xrFb,
                           const layers::TextureType consumerType,
                           const bool webvr) {
  const FuncScope funcScope(*this, "<Present>");
  if (IsContextLost()) return;

  auto swapChain = webvr ? &mWebVRSwapChain : &mSwapChain;
  if (xrFb) {
    swapChain = &xrFb->mOpaqueSwapChain;
  }
  const gl::MozFramebuffer* maybeFB = nullptr;
  if (xrFb) {
    swapChain = &xrFb->mOpaqueSwapChain;
    maybeFB = xrFb->mOpaque.get();
  } else {
    mResolvedDefaultFB = nullptr;
  }

  if (!swapChain->mFactory) {
    auto typedFactory = gl::SurfaceFactory::Create(gl, consumerType);
    if (typedFactory) {
      swapChain->mFactory = std::move(typedFactory);
    }
  }
  if (!swapChain->mFactory) {
    NS_WARNING("Failed to make an ideal SurfaceFactory.");
    swapChain->mFactory = MakeUnique<gl::SurfaceFactory_Basic>(*gl);
  }
  MOZ_ASSERT(swapChain->mFactory);

  if (maybeFB) {
    (void)PresentIntoXR(*swapChain, *maybeFB);
  } else {
    (void)PresentInto(*swapChain);
  }
}

Maybe<layers::SurfaceDescriptor> WebGLContext::GetFrontBuffer(
    WebGLFramebuffer* const xrFb, const bool webvr) {
  auto swapChain = webvr ? &mWebVRSwapChain : &mSwapChain;
  if (xrFb) {
    swapChain = &xrFb->mOpaqueSwapChain;
  }
  const auto& front = swapChain->FrontBuffer();
  if (!front) return {};

  return front->ToSurfaceDescriptor();
}

bool WebGLContext::FrontBufferSnapshotInto(Range<uint8_t> dest) {
  const auto& front = mSwapChain.FrontBuffer();
  if (!front) return false;

  // -

  front->WaitForBufferOwnership();
  front->LockProd();
  front->ProducerReadAcquire();
  auto reset = MakeScopeExit([&] {
    front->ProducerReadRelease();
    front->UnlockProd();
  });

  // -

  gl->fPixelStorei(LOCAL_GL_PACK_ALIGNMENT, 1);
  if (IsWebGL2()) {
    gl->fPixelStorei(LOCAL_GL_PACK_ROW_LENGTH, 0);
    gl->fPixelStorei(LOCAL_GL_PACK_SKIP_PIXELS, 0);
    gl->fPixelStorei(LOCAL_GL_PACK_SKIP_ROWS, 0);
  }

  // -

  const auto readFbWas = mBoundReadFramebuffer;
  const auto pboWas = mBoundPixelPackBuffer;

  GLenum fbTarget = LOCAL_GL_READ_FRAMEBUFFER;
  if (!IsWebGL2()) {
    fbTarget = LOCAL_GL_FRAMEBUFFER;
  }
  auto reset2 = MakeScopeExit([&] {
    DoBindFB(readFbWas, fbTarget);
    if (pboWas) {
      BindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, pboWas);
    }
  });

  if (front->mFb) {
    gl->fBindFramebuffer(fbTarget, front->mFb->mFB);
  } else {
    if (!BindDefaultFBForRead()) {
      gfxCriticalError() << "BindDefaultFBForRead failed";
      return false;
    }
  }
  if (pboWas) {
    BindBuffer(LOCAL_GL_PIXEL_PACK_BUFFER, nullptr);
  }

  // -

  const auto& size = front->mDesc.size;
  const size_t stride = size.width * 4;
  MOZ_ASSERT(dest.length() == stride * size.height);
  gl->fReadPixels(0, 0, size.width, size.height, LOCAL_GL_RGBA,
                  LOCAL_GL_UNSIGNED_BYTE, dest.begin().get());
  gfxUtils::ConvertBGRAtoRGBA(dest.begin().get(), stride * size.height);

  return true;
}

void WebGLContext::ClearVRSwapChain() { mWebVRSwapChain.ClearPool(); }

// ------------------------

RefPtr<gfx::DataSourceSurface> GetTempSurface(const gfx::IntSize& aSize,
                                              gfx::SurfaceFormat& aFormat) {
  uint32_t stride =
      gfx::GetAlignedStride<8>(aSize.width, BytesPerPixel(aFormat));
  return gfx::Factory::CreateDataSourceSurfaceWithStride(aSize, aFormat,
                                                         stride);
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
  mLruPosition = {};
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

const char* WebGLContext::FuncName() const {
  const char* ret;
  if (MOZ_LIKELY(mFuncScope)) {
    ret = mFuncScope->mFuncName;
  } else {
    NS_WARNING("FuncScope not on stack!");
    ret = "<unknown function>";
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

bool ClientWebGLContext::IsXRCompatible() const { return mXRCompatible; }

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
  mXRCompatible = true;
  promise->MaybeResolveWithUndefined();
  return promise.forget();
}

// --

webgl::AvailabilityRunnable& ClientWebGLContext::EnsureAvailabilityRunnable()
    const {
  if (!mAvailabilityRunnable) {
    mAvailabilityRunnable = new webgl::AvailabilityRunnable(this);
    auto forgettable = mAvailabilityRunnable;
    NS_DispatchToCurrentThread(forgettable.forget());
  }
  return *mAvailabilityRunnable;
}

webgl::AvailabilityRunnable::AvailabilityRunnable(
    const ClientWebGLContext* const webgl)
    : Runnable("webgl::AvailabilityRunnable"), mWebGL(webgl) {}

webgl::AvailabilityRunnable::~AvailabilityRunnable() {
  MOZ_ASSERT(mQueries.empty());
  MOZ_ASSERT(mSyncs.empty());
}

nsresult webgl::AvailabilityRunnable::Run() {
  for (const auto& cur : mQueries) {
    if (!cur) continue;
    cur->mCanBeAvailable = true;
  }
  mQueries.clear();

  for (const auto& cur : mSyncs) {
    if (!cur) continue;
    cur->mCanBeAvailable = true;
  }
  mSyncs.clear();

  if (mWebGL) {
    mWebGL->mAvailabilityRunnable = nullptr;
  }
  return NS_OK;
}

// -

void WebGLContext::GenerateErrorImpl(const GLenum errOrWarning,
                                     const std::string& text) const {
  auto err = errOrWarning;
  bool isPerfWarning = false;
  if (err == webgl::kErrorPerfWarning) {
    err = 0;
    isPerfWarning = true;
  }

  if (err && mFuncScope && mFuncScope->mBindFailureGuard) {
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

  // -

  const auto ShouldWarn = [&]() {
    if (isPerfWarning) {
      return ShouldGeneratePerfWarnings();
    }
    return ShouldGenerateWarnings();
  };
  if (!ShouldWarn()) return;

  // -

  auto* pNumWarnings = &mWarningCount;
  const char* warningsType = "warnings";
  if (isPerfWarning) {
    pNumWarnings = &mNumPerfWarnings;
    warningsType = "perf warnings";
  }

  if (isPerfWarning) {
    const auto perfText = std::string("WebGL perf warning: ") + text;
    mHost->JsWarning(perfText);
  } else {
    mHost->JsWarning(text);
  }
  *pNumWarnings += 1;

  if (!ShouldWarn()) {
    const auto& msg = nsPrintfCString(
        "After reporting %i, no further %s will be reported for this WebGL "
        "context.",
        int(*pNumWarnings), warningsType);
    mHost->JsWarning(ToString(msg));
  }
}

// -

Maybe<std::string> WebGLContext::GetString(const GLenum pname) const {
  const WebGLContext::FuncScope funcScope(*this, "getParameter");
  if (IsContextLost()) return {};

  const auto FromRaw = [](const char* const raw) -> Maybe<std::string> {
    if (!raw) return {};
    return Some(std::string(raw));
  };

  switch (pname) {
    case LOCAL_GL_EXTENSIONS: {
      if (!gl->IsCoreProfile()) {
        const auto rawExt = (const char*)gl->fGetString(LOCAL_GL_EXTENSIONS);
        return FromRaw(rawExt);
      }
      std::string ret;
      const auto& numExts = gl->GetIntAs<GLuint>(LOCAL_GL_NUM_EXTENSIONS);
      for (GLuint i = 0; i < numExts; i++) {
        const auto rawExt =
            (const char*)gl->fGetStringi(LOCAL_GL_EXTENSIONS, i);
        if (!rawExt) continue;

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
      return FromRaw(raw);
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

nsCString ToCString(const std::string& s) {
  return nsCString(s.data(), s.size());
}

webgl::CompileResult WebGLContext::GetCompileResult(
    const WebGLShader& shader) const {
  webgl::CompileResult ret;
  [&]() {
    ret.pending = false;
    const auto& info = shader.CompileResults();
    if (!info) return;
    if (!info->mValid) {
      ret.log = info->mInfoLog.c_str();
      return;
    }
    // TODO: These could be large and should be made fallible.
    ret.translatedSource = ToCString(info->mObjectCode);
    ret.log = ToCString(shader.CompileLog());
    if (!shader.IsCompiled()) return;
    ret.success = true;
  }();
  return ret;
}

webgl::LinkResult WebGLContext::GetLinkResult(const WebGLProgram& prog) const {
  webgl::LinkResult ret;
  [&]() {
    ret.pending = false;  // Link status polling not yet implemented.
    ret.log = ToCString(prog.LinkLog());
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
