/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PREFS_H
#define GFX_PREFS_H

#include <cmath>  // for M_PI
#include <stdint.h>
#include <string>
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/gfx/LoggingConstants.h"
#include "nsTArray.h"
#include "nsString.h"

// First time gfxPrefs::GetSingleton() needs to be called on the main thread,
// before any of the methods accessing the values are used, but after
// the Preferences system has been initialized.

// The static methods to access the preference value are safe to call
// from any thread after that first call.

// To register a preference, you need to add a line in this file using
// the DECL_GFX_PREF macro.
//
// Update argument controls whether we read the preference value and save it
// or connect with a callback.  See UpdatePolicy enum below.
// Pref is the string with the preference name.
// Name argument is the name of the static function to create.
// Type is the type of the preference - bool, int32_t, uint32_t.
// Default is the default value for the preference.
//
// For example this line in the .h:
//   DECL_GFX_PREF(Once,"layers.dump",LayersDump,bool,false);
// means that you can call
//   bool var = gfxPrefs::LayersDump();
// from any thread, but that you will only get the preference value of
// "layers.dump" as it was set at the start of the session (subject to
// note 2 below). If the value was not set, the default would be false.
//
// In another example, this line in the .h:
//   DECL_GFX_PREF(Live,"gl.msaa-level",MSAALevel,uint32_t,2);
// means that every time you call
//   uint32_t var = gfxPrefs::MSAALevel();
// from any thread, you will get the most up to date preference value of
// "gl.msaa-level".  If the value is not set, the default would be 2.

// Note 1: Changing a preference from Live to Once is now as simple
// as changing the Update argument.  If your code worked before, it will
// keep working, and behave as if the user never changes the preference.
// Things are a bit more complicated and perhaps even dangerous when
// going from Once to Live, or indeed setting a preference to be Live
// in the first place, so be careful.  You need to be ready for the
// values changing mid execution, and if you're using those preferences
// in any setup and initialization, you may need to do extra work.

// Note 2: Prefs can be set by using the corresponding Set method. For
// example, if the accessor is Foo() then calling SetFoo(...) will update
// the preference and also change the return value of subsequent Foo() calls.
// This is true even for 'Once' prefs which otherwise do not change if the
// pref is updated after initialization. Changing gfxPrefs values in content
// processes will not affect the result in other processes. Changing gfxPrefs
// values in the GPU process is not supported at all.

#define DECL_GFX_PREF(Update, Prefname, Name, Type, Default)              \
 public:                                                                  \
  static Type Name() {                                                    \
    MOZ_ASSERT(SingletonExists());                                        \
    return GetSingleton().mPref##Name.mValue;                             \
  }                                                                       \
  static void Set##Name(Type aVal) {                                      \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.Set(UpdatePolicy::Update,                  \
                                   Get##Name##PrefName(), aVal);          \
  }                                                                       \
  static const char* Get##Name##PrefName() { return Prefname; }           \
  static Type Get##Name##PrefDefault() { return Default; }                \
  static void Set##Name##ChangeCallback(Pref::ChangeCallback aCallback) { \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.SetChangeCallback(aCallback);              \
  }                                                                       \
                                                                          \
 private:                                                                 \
  PrefTemplate<UpdatePolicy::Update, Type, Get##Name##PrefDefault,        \
               Get##Name##PrefName>                                       \
      mPref##Name

// This declares an "override" pref, which is exposed as a "bool" pref by the
// API, but is internally stored as a tri-state int pref with three possible
// values:
// - A value of 0 means that it has been force-disabled, and is exposed as a
//   false-valued bool.
// - A value of 1 means that it has been force-enabled, and is exposed as a
//   true-valued bool.
// - A value of 2 (the default) means that it returns the provided BaseValue
//   as a boolean. The BaseValue may be a constant expression or a function.
// If the prefs defined with this macro are listed in prefs files (e.g. all.js),
// then they must be listed with an int value (default to 2, but you can use 0
// or 1 if you want to force it on or off).
#define DECL_OVERRIDE_PREF(Update, Prefname, Name, BaseValue)             \
 public:                                                                  \
  static bool Name() {                                                    \
    MOZ_ASSERT(SingletonExists());                                        \
    int32_t val = GetSingleton().mPref##Name.mValue;                      \
    return val == 2 ? !!(BaseValue) : !!val;                              \
  }                                                                       \
  static void Set##Name(bool aVal) {                                      \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.Set(UpdatePolicy::Update,                  \
                                   Get##Name##PrefName(), aVal ? 1 : 0);  \
  }                                                                       \
  static const char* Get##Name##PrefName() { return Prefname; }           \
  static int32_t Get##Name##PrefDefault() { return 2; }                   \
  static void Set##Name##ChangeCallback(Pref::ChangeCallback aCallback) { \
    MOZ_ASSERT(SingletonExists());                                        \
    GetSingleton().mPref##Name.SetChangeCallback(aCallback);              \
  }                                                                       \
                                                                          \
 private:                                                                 \
  PrefTemplate<UpdatePolicy::Update, int32_t, Get##Name##PrefDefault,     \
               Get##Name##PrefName>                                       \
      mPref##Name

namespace mozilla {
namespace gfx {
class GfxPrefValue;  // defined in PGPU.ipdl
}  // namespace gfx
}  // namespace mozilla

class gfxPrefs;
class gfxPrefs final {
  typedef mozilla::gfx::GfxPrefValue GfxPrefValue;

  typedef mozilla::Atomic<bool, mozilla::Relaxed> AtomicBool;
  typedef mozilla::Atomic<int32_t, mozilla::Relaxed> AtomicInt32;
  typedef mozilla::Atomic<uint32_t, mozilla::Relaxed> AtomicUint32;

 private:
  // Enums for the update policy.
  enum class UpdatePolicy {
    Skip,  // Set the value to default, skip any Preferences calls
    Once,  // Evaluate the preference once, unchanged during the session
    Live   // Evaluate the preference and set callback so it stays current/live
  };

 public:
  class Pref {
   public:
    Pref() : mChangeCallback(nullptr) {
      mIndex = sGfxPrefList->Length();
      sGfxPrefList->AppendElement(this);
    }

    size_t Index() const { return mIndex; }
    void OnChange();

    typedef void (*ChangeCallback)(const GfxPrefValue&);
    void SetChangeCallback(ChangeCallback aCallback);

    virtual const char* Name() const = 0;

    // Returns true if the value is default, false if changed.
    virtual bool HasDefaultValue() const = 0;

    // Returns the pref value as a discriminated union.
    virtual void GetLiveValue(GfxPrefValue* aOutValue) const = 0;

    // Returns the pref value as a discriminated union.
    virtual void GetCachedValue(GfxPrefValue* aOutValue) const = 0;

    // Change the cached value. GfxPrefValue must be a compatible type.
    virtual void SetCachedValue(const GfxPrefValue& aOutValue) = 0;

   protected:
    void FireChangeCallback();

   private:
    size_t mIndex;
    ChangeCallback mChangeCallback;
  };

  static const nsTArray<Pref*>& all() { return *sGfxPrefList; }

 private:
  // We split out a base class to reduce the number of virtual function
  // instantiations that we do, which saves code size.
  template <class T>
  class TypedPref : public Pref {
   public:
    explicit TypedPref(T aValue) : mValue(aValue) {}

    void GetCachedValue(GfxPrefValue* aOutValue) const override {
      CopyPrefValue(&mValue, aOutValue);
    }
    void SetCachedValue(const GfxPrefValue& aOutValue) override {
      T newValue;
      CopyPrefValue(&aOutValue, &newValue);

      if (mValue != newValue) {
        mValue = newValue;
        FireChangeCallback();
      }
    }

   protected:
    T GetLiveValueByName(const char* aPrefName) const {
      if (IsPrefsServiceAvailable()) {
        return PrefGet(aPrefName, mValue);
      }
      return mValue;
    }

   public:
    T mValue;
  };

  // Since we cannot use const char*, use a function that returns it.
  template <UpdatePolicy Update, class T, T Default(void),
            const char* Prefname(void)>
  class PrefTemplate final : public TypedPref<T> {
    typedef TypedPref<T> BaseClass;

   public:
    PrefTemplate() : BaseClass(Default()) {
      // If not using the Preferences service, values are synced over IPC, so
      // there's no need to register us as a Preferences observer.
      if (IsPrefsServiceAvailable()) {
        Register(Update, Prefname());
      }
      // By default we only watch changes in the parent process, to communicate
      // changes to the GPU process.
      if (IsParentProcess() && Update == UpdatePolicy::Live) {
        WatchChanges(Prefname(), this);
      }
    }
    ~PrefTemplate() {
      if (IsParentProcess() && Update == UpdatePolicy::Live) {
        UnwatchChanges(Prefname(), this);
      }
    }
    void Register(UpdatePolicy aUpdate, const char* aPreference) {
      AssertMainThread();
      switch (aUpdate) {
        case UpdatePolicy::Skip:
          break;
        case UpdatePolicy::Once:
          this->mValue = PrefGet(aPreference, this->mValue);
          break;
        case UpdatePolicy::Live: {
          nsCString pref;
          pref.AssignLiteral(aPreference, strlen(aPreference));
          PrefAddVarCache(&this->mValue, pref, this->mValue);
        } break;
        default:
          MOZ_CRASH("Incomplete switch");
      }
    }
    void Set(UpdatePolicy aUpdate, const char* aPref, T aValue) {
      AssertMainThread();
      PrefSet(aPref, aValue);
      switch (aUpdate) {
        case UpdatePolicy::Skip:
        case UpdatePolicy::Live:
          break;
        case UpdatePolicy::Once:
          this->mValue = PrefGet(aPref, this->mValue);
          break;
        default:
          MOZ_CRASH("Incomplete switch");
      }
    }
    const char* Name() const override { return Prefname(); }
    void GetLiveValue(GfxPrefValue* aOutValue) const override {
      T value = GetLiveValue();
      CopyPrefValue(&value, aOutValue);
    }
    // When using the Preferences service, the change callback can be triggered
    // *before* our cached value is updated, so we expose a method to grab the
    // true live value.
    T GetLiveValue() const { return BaseClass::GetLiveValueByName(Prefname()); }
    bool HasDefaultValue() const override { return this->mValue == Default(); }
  };

  // clang-format off

  // This is where DECL_GFX_PREF for each of the preferences should go.  We
  // will keep these in an alphabetical order to make it easier to see if a
  // method accessing a pref already exists. Just add yours in the list.

  // The apz prefs are explained in AsyncPanZoomController.cpp
  DECL_GFX_PREF(Once, "apz.fling_curve_function_x1",           APZCurveFunctionX1, float, 0.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_x2",           APZCurveFunctionX2, float, 1.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_y1",           APZCurveFunctionY1, float, 0.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_y2",           APZCurveFunctionY2, float, 1.0f);
  DECL_GFX_PREF(Once, "apz.keyboard.enabled",                  APZKeyboardEnabled, bool, false);
  DECL_GFX_PREF(Once, "apz.max_velocity_queue_size",           APZMaxVelocityQueueSize, uint32_t, 5);
  DECL_GFX_PREF(Once, "apz.pinch_lock.buffer_max_age",         APZPinchLockBufferMaxAge, int32_t, 50);

  DECL_GFX_PREF(Once, "dom.vr.enabled",                        VREnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.external.enabled",               VRExternalEnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.oculus.enabled",                 VROculusEnabled, bool, true);
  DECL_GFX_PREF(Once, "dom.vr.openvr.enabled",                 VROpenVREnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.openvr.action_input",            VROpenVRActionInputEnabled, bool, true);
  DECL_GFX_PREF(Once, "dom.vr.osvr.enabled",                   VROSVREnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.process.enabled",                VRProcessEnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.process.startup_timeout_ms",     VRProcessTimeoutMs, int32_t, 5000);
  DECL_GFX_PREF(Once, "dom.vr.service.enabled",                VRServiceEnabled, bool, true);

  DECL_GFX_PREF(Once, "gfx.android.rgb16.force",               AndroidRGB16Force, bool, false);
  DECL_GFX_PREF(Once, "gfx.apitrace.enabled",                  UseApitrace, bool, false);
#if defined(RELEASE_OR_BETA)
  // "Skip" means this is locked to the default value in beta and release.
  DECL_GFX_PREF(Skip, "gfx.blocklist.all",                     BlocklistAll, int32_t, 0);
#else
  DECL_GFX_PREF(Once, "gfx.blocklist.all",                     BlocklistAll, int32_t, 0);
#endif
  // Size in megabytes
  DECL_GFX_PREF(Once, "gfx.content.skia-font-cache-size",      SkiaContentFontCacheSize, int32_t, 5);
  DECL_GFX_PREF(Once, "gfx.device-reset.limit",                DeviceResetLimitCount, int32_t, 10);
  DECL_GFX_PREF(Once, "gfx.device-reset.threshold-ms",         DeviceResetThresholdMilliseconds, int32_t, -1);
  DECL_GFX_PREF(Once, "gfx.direct2d.disabled",                 Direct2DDisabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct2d.force-enabled",            Direct2DForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct3d11.enable-debug-layer",     Direct3D11EnableDebugLayer, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct3d11.break-on-error",         Direct3D11BreakOnError, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct3d11.sleep-on-create-device", Direct3D11SleepOnCreateDevice, int32_t, 0);
  DECL_GFX_PREF(Once, "gfx.e10s.hide-plugins-for-scroll",      HidePluginsForScroll, bool, true);
  DECL_GFX_PREF(Once, "gfx.e10s.font-list.shared",             SharedFontList, bool, false);
  // Note that        "gfx.logging.level" is defined in Logging.h.
  DECL_GFX_PREF(Live, "gfx.logging.level",                     GfxLoggingLevel, int32_t, mozilla::gfx::LOG_DEFAULT);
  DECL_GFX_PREF(Once, "gfx.logging.crash.length",              GfxLoggingCrashLength, uint32_t, 16);
  // The maximums here are quite conservative, we can tighten them if problems show up.
  DECL_GFX_PREF(Once, "gfx.logging.texture-usage.enabled",     GfxLoggingTextureUsageEnabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.logging.peak-texture-usage.enabled",GfxLoggingPeakTextureUsageEnabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.logging.slow-frames.enabled",       LoggingSlowFramesEnabled, bool, false);
  // Use gfxPlatform::MaxAllocSize instead of the pref directly
  DECL_GFX_PREF(Once, "gfx.max-alloc-size",                    MaxAllocSizeDoNotUseDirectly, int32_t, (int32_t)500000000);
  // Use gfxPlatform::MaxTextureSize instead of the pref directly
  DECL_GFX_PREF(Once, "gfx.max-texture-size",                  MaxTextureSizeDoNotUseDirectly, int32_t, (int32_t)32767);
  DECL_GFX_PREF(Once, "gfx.text.disable-aa",                   DisableAllTextAA, bool, false);

  // Disable surface sharing due to issues with compatible FBConfigs on
  // NVIDIA drivers as described in bug 1193015.
  DECL_GFX_PREF(Once, "gfx.use-iosurface-textures",            UseIOSurfaceTextures, bool, false);
  DECL_GFX_PREF(Once, "gfx.use-mutex-on-present",              UseMutexOnPresent, bool, false);
  DECL_GFX_PREF(Once, "gfx.use-surfacetexture-textures",       UseSurfaceTextureTextures, bool, false);
  DECL_GFX_PREF(Once, "gfx.allow-texture-direct-mapping",      AllowTextureDirectMapping, bool, true);
  DECL_GFX_PREF(Once, "gfx.vsync.compositor.unobserve-count",  CompositorUnobserveCount, int32_t, 10);

  DECL_GFX_PREF(Once, "gfx.webrender.all",                     WebRenderAll, bool, false);
  DECL_GFX_PREF(Once, "gfx.webrender.all.qualified",           WebRenderAllQualified, bool, true);
  DECL_GFX_PREF(Once,
                "gfx.webrender.all.qualified.default",
                WebRenderAllQualifiedDefault,
                bool,
                false);
  DECL_GFX_PREF(Once, "gfx.webrender.enabled",                 WebRenderEnabledDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Once, "gfx.webrender.force-disabled",          WebRenderForceDisabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.webrender.split-render-roots",      WebRenderSplitRenderRoots, bool, false);

  // Use vsync events generated by hardware
  DECL_GFX_PREF(Once, "gfx.work-around-driver-bugs",           WorkAroundDriverBugs, bool, true);

  DECL_GFX_PREF(Once, "image.cache.size",                      ImageCacheSize, int32_t, 5*1024*1024);
  DECL_GFX_PREF(Once, "image.cache.timeweight",                ImageCacheTimeWeight, int32_t, 500);
  DECL_GFX_PREF(Once, "image.mem.decode_bytes_at_a_time",      ImageMemDecodeBytesAtATime, uint32_t, 200000);
  DECL_GFX_PREF(Once, "image.mem.animated.discardable",        ImageMemAnimatedDiscardable, bool, false);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.discard_factor", ImageMemSurfaceCacheDiscardFactor, uint32_t, 1);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.max_size_kb",    ImageMemSurfaceCacheMaxSizeKB, uint32_t, 100 * 1024);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.min_expiration_ms", ImageMemSurfaceCacheMinExpirationMS, uint32_t, 60*1000);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.size_factor",    ImageMemSurfaceCacheSizeFactor, uint32_t, 64);
  DECL_GFX_PREF(Once, "image.multithreaded_decoding.limit",    ImageMTDecodingLimit, int32_t, -1);
  DECL_GFX_PREF(Once, "image.multithreaded_decoding.idle_timeout", ImageMTDecodingIdleTimeout, int32_t, -1);

  DECL_GFX_PREF(Once, "layers.acceleration.disabled",          LayersAccelerationDisabledDoNotUseDirectly, bool, false);
  // Instead, use gfxConfig::IsEnabled(Feature::HW_COMPOSITING).

  DECL_GFX_PREF(Once, "layers.acceleration.force-enabled",     LayersAccelerationForceEnabledDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Once, "layers.amd-switchable-gfx.enabled",     LayersAMDSwitchableGfxEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.async-pan-zoom.enabled",         AsyncPanZoomEnabledDoNotUseDirectly, bool, true);
  DECL_GFX_PREF(Once, "layers.bufferrotation.enabled",         BufferRotationEnabled, bool, true);
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  // If MOZ_GFX_OPTIMIZE_MOBILE is defined, we force component alpha off
  // and ignore the preference.
  DECL_GFX_PREF(Skip, "layers.componentalpha.enabled",         ComponentAlphaEnabled, bool, false);
#else
  // If MOZ_GFX_OPTIMIZE_MOBILE is not defined, we actually take the
  // preference value, defaulting to true.
  DECL_GFX_PREF(Once, "layers.componentalpha.enabled",         ComponentAlphaEnabled, bool, true);
#endif
  DECL_GFX_PREF(Once, "layers.d3d11.force-warp",               LayersD3D11ForceWARP, bool, false);

  // 0 is "no change" for contrast, positive values increase it, negative values
  // decrease it until we hit mid gray at -1 contrast, after that it gets weird.
  DECL_GFX_PREF(Once, "layers.enable-tiles",                   LayersTilesEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.enable-tiles-if-skia-pomtp",     LayersTilesEnabledIfSkiaPOMTP, bool, false);
  DECL_GFX_PREF(Once, "layers.force-shmem-tiles",              ForceShmemTiles, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.allow-software",     GPUProcessAllowSoftware, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.enabled",            GPUProcessEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.force-enabled",      GPUProcessForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.ipc_reply_timeout_ms", GPUProcessIPCReplyTimeoutMs, int32_t, 10000);
  // Note: This pref will only be used if it is less than layers.gpu-process.max_restarts.
  DECL_GFX_PREF(Once, "layers.gpu-process.startup_timeout_ms", GPUProcessTimeoutMs, int32_t, 5000);
  DECL_GFX_PREF(Once, "layers.mlgpu.enabled",                  AdvancedLayersEnabledDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-buffer-cache",      AdvancedLayersEnableBufferCache, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-buffer-sharing",    AdvancedLayersEnableBufferSharing, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-clear-view",        AdvancedLayersEnableClearView, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-cpu-occlusion",     AdvancedLayersEnableCPUOcclusion, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-depth-buffer",      AdvancedLayersEnableDepthBuffer, bool, false);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-on-windows7",       AdvancedLayersEnableOnWindows7, bool, false);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-container-resizing", AdvancedLayersEnableContainerResizing, bool, true);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.force-disabled", LayersOffMainThreadCompositionForceDisabled, bool, false);
  DECL_GFX_PREF(Once, "layers.omtp.capture-limit",             LayersOMTPCaptureLimit, uint32_t, 25 * 1024 * 1024);
  DECL_GFX_PREF(Once, "layers.omtp.paint-workers",             LayersOMTPPaintWorkers, int32_t, 1);
  DECL_GFX_PREF(Once, "layers.prefer-opengl",                  LayersPreferOpenGL, bool, false);
  DECL_GFX_PREF(Once, "layers.windowrecording.path",           LayersWindowRecordingPath, std::string, std::string());

  // We allow for configurable and rectangular tile size to avoid wasting memory on devices whose
  // screen size does not align nicely to the default tile size. Although layers can be any size,
  // they are often the same size as the screen, especially for width.
  DECL_GFX_PREF(Once, "layers.tile-width",                     LayersTileWidth, int32_t, 256);
  DECL_GFX_PREF(Once, "layers.tile-height",                    LayersTileHeight, int32_t, 256);
  DECL_GFX_PREF(Once, "layers.tile-initial-pool-size",         LayersTileInitialPoolSize, uint32_t, (uint32_t)50);
  DECL_GFX_PREF(Once, "layers.tile-pool-unused-size",          LayersTilePoolUnusedSize, uint32_t, (uint32_t)10);
  DECL_GFX_PREF(Once, "layers.tile-pool-shrink-timeout",       LayersTilePoolShrinkTimeout, uint32_t, (uint32_t)50);
  DECL_GFX_PREF(Once, "layers.tile-pool-clear-timeout",        LayersTilePoolClearTimeout, uint32_t, (uint32_t)5000);
  DECL_GFX_PREF(Once, "layers.tiles.adjust",                   LayersTilesAdjust, bool, true);
  DECL_GFX_PREF(Once, "layers.tiles.edge-padding",             TileEdgePaddingEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.uniformity-info",                UniformityInfo, bool, false);
  DECL_GFX_PREF(Once, "layers.use-image-offscreen-surfaces",   UseImageOffscreenSurfaces, bool, true);

  DECL_GFX_PREF(Live, "layout.frame_rate",                     LayoutFrameRate, int32_t, -1);
  DECL_GFX_PREF(Once, "layout.paint_rects_separately",         LayoutPaintRectsSeparately, bool, true);

  DECL_GFX_PREF(Once, "media.hardware-video-decoding.force-enabled",
                                                               HardwareVideoDecodingForceEnabled, bool, false);
#ifdef XP_WIN
  DECL_GFX_PREF(Once, "media.wmf.use-sync-texture", PDMWMFUseSyncTexture, bool, true);
  DECL_GFX_PREF(Once, "media.wmf.vp9.enabled", MediaWmfVp9Enabled, bool, true);
#endif

  DECL_GFX_PREF(Once, "slider.snapMultiplier",                 SliderSnapMultiplier, int32_t, 0);
  DECL_GFX_PREF(Once, "webgl.force-layers-readback",           WebGLForceLayersReadback, bool, false);

  // WARNING:
  // Please make sure that you've added your new preference to the list above
  // in alphabetical order.
  // Please do not just append it to the end of the list.

  // clang-format on

 public:
  // Manage the singleton:
  static gfxPrefs& GetSingleton() {
    return sInstance ? *sInstance : CreateAndInitializeSingleton();
  }
  static void DestroySingleton();
  static bool SingletonExists();

 private:
  static gfxPrefs& CreateAndInitializeSingleton();

  static gfxPrefs* sInstance;
  static bool sInstanceHasBeenDestroyed;
  static nsTArray<Pref*>* sGfxPrefList;

 private:
  // The constructor cannot access GetSingleton(), since sInstance (necessarily)
  // has not been assigned yet. Follow-up initialization that needs
  // GetSingleton() must be added to Init().
  void Init();

  static bool IsPrefsServiceAvailable();
  static bool IsParentProcess();
  // Creating these to avoid having to include Preferences.h in the .h
  static void PrefAddVarCache(bool*, const nsACString&, bool);
  static void PrefAddVarCache(int32_t*, const nsACString&, int32_t);
  static void PrefAddVarCache(uint32_t*, const nsACString&, uint32_t);
  static void PrefAddVarCache(float*, const nsACString&, float);
  static void PrefAddVarCache(std::string*, const nsCString&, std::string);
  static void PrefAddVarCache(AtomicBool*, const nsACString&, bool);
  static void PrefAddVarCache(AtomicInt32*, const nsACString&, int32_t);
  static void PrefAddVarCache(AtomicUint32*, const nsACString&, uint32_t);
  static bool PrefGet(const char*, bool);
  static int32_t PrefGet(const char*, int32_t);
  static uint32_t PrefGet(const char*, uint32_t);
  static float PrefGet(const char*, float);
  static std::string PrefGet(const char*, std::string);
  static void PrefSet(const char* aPref, bool aValue);
  static void PrefSet(const char* aPref, int32_t aValue);
  static void PrefSet(const char* aPref, uint32_t aValue);
  static void PrefSet(const char* aPref, float aValue);
  static void PrefSet(const char* aPref, std::string aValue);
  static void WatchChanges(const char* aPrefname, Pref* aPref);
  static void UnwatchChanges(const char* aPrefname, Pref* aPref);
  // Creating these to avoid having to include PGPU.h in the .h
  static void CopyPrefValue(const bool* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const int32_t* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const uint32_t* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const float* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const std::string* aValue, GfxPrefValue* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, bool* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, int32_t* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, uint32_t* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, float* aOutValue);
  static void CopyPrefValue(const GfxPrefValue* aValue, std::string* aOutValue);

  static void AssertMainThread();

  // Some wrapper functions for the DECL_OVERRIDE_PREF prefs' base values, so
  // that we don't to include all sorts of header files into this gfxPrefs.h
  // file.
  static bool OverrideBase_WebRender();

  gfxPrefs();
  ~gfxPrefs();
  gfxPrefs(const gfxPrefs&) = delete;
  gfxPrefs& operator=(const gfxPrefs&) = delete;
};

#undef DECL_GFX_PREF /* Don't need it outside of this file */

#endif /* GFX_PREFS_H */
