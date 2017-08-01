/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PREFS_H
#define GFX_PREFS_H

#include <cmath>                 // for M_PI
#include <stdint.h>
#include <string>
#include "mozilla/Assertions.h"
#include "mozilla/gfx/LoggingConstants.h"
#include "nsTArray.h"

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

#define DECL_GFX_PREF(Update, Prefname, Name, Type, Default)                  \
public:                                                                       \
static Type Name() { MOZ_ASSERT(SingletonExists()); return GetSingleton().mPref##Name.mValue; } \
static void Set##Name(Type aVal) { MOZ_ASSERT(SingletonExists());             \
    GetSingleton().mPref##Name.Set(UpdatePolicy::Update, Get##Name##PrefName(), aVal); } \
static const char* Get##Name##PrefName() { return Prefname; }                 \
static Type Get##Name##PrefDefault() { return Default; }                      \
static void Set##Name##ChangeCallback(Pref::ChangeCallback aCallback) {       \
    MOZ_ASSERT(SingletonExists());                                            \
    GetSingleton().mPref##Name.SetChangeCallback(aCallback); }                \
private:                                                                      \
PrefTemplate<UpdatePolicy::Update, Type, Get##Name##PrefDefault, Get##Name##PrefName> mPref##Name

// This declares an "override" pref, which is exposed as a "bool" pref by the API,
// but is internally stored as a tri-state int pref with three possible values:
// - A value of 0 means that it has been force-disabled, and is exposed as a
//   false-valued bool.
// - A value of 1 means that it has been force-enabled, and is exposed as a
//   true-valued bool.
// - A value of 2 (the default) means that it returns the provided BaseValue
//   as a boolean. The BaseValue may be a constant expression or a function.
// If the prefs defined with this macro are listed in prefs files (e.g. all.js),
// then they must be listed with an int value (default to 2, but you can use 0
// or 1 if you want to force it on or off).
#define DECL_OVERRIDE_PREF(Update, Prefname, Name, BaseValue)                 \
public:                                                                       \
static bool Name() { MOZ_ASSERT(SingletonExists());                           \
    int32_t val = GetSingleton().mPref##Name.mValue;                          \
    return val == 2 ? !!(BaseValue) : !!val; }                                  \
static void Set##Name(bool aVal) { MOZ_ASSERT(SingletonExists());             \
    GetSingleton().mPref##Name.Set(UpdatePolicy::Update, Get##Name##PrefName(), aVal ? 1 : 0); } \
static const char* Get##Name##PrefName() { return Prefname; }                 \
static int32_t Get##Name##PrefDefault() { return 2; }                         \
static void Set##Name##ChangeCallback(Pref::ChangeCallback aCallback) {       \
    MOZ_ASSERT(SingletonExists());                                            \
    GetSingleton().mPref##Name.SetChangeCallback(aCallback); }                \
private:                                                                      \
PrefTemplate<UpdatePolicy::Update, int32_t, Get##Name##PrefDefault, Get##Name##PrefName> mPref##Name

namespace mozilla {
namespace gfx {
class GfxPrefValue;   // defined in PGPU.ipdl
} // namespace gfx
} // namespace mozilla

class gfxPrefs;
class gfxPrefs final
{
  typedef mozilla::gfx::GfxPrefValue GfxPrefValue;

private:
  // Enums for the update policy.
  enum class UpdatePolicy {
    Skip, // Set the value to default, skip any Preferences calls
    Once, // Evaluate the preference once, unchanged during the session
    Live  // Evaluate the preference and set callback so it stays current/live
  };

public:
  class Pref
  {
  public:
    Pref() : mChangeCallback(nullptr)
    {
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

  static const nsTArray<Pref*>& all() {
    return *sGfxPrefList;
  }

private:
  // We split out a base class to reduce the number of virtual function
  // instantiations that we do, which saves code size.
  template<class T>
  class TypedPref : public Pref
  {
  public:
    explicit TypedPref(T aValue)
      : mValue(aValue)
    {}

    void GetCachedValue(GfxPrefValue* aOutValue) const override {
      CopyPrefValue(&mValue, aOutValue);
    }
    void SetCachedValue(const GfxPrefValue& aOutValue) override {
      // This is only used in non-XPCOM processes.
      MOZ_ASSERT(!IsPrefsServiceAvailable());

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
  template <UpdatePolicy Update, class T, T Default(void), const char* Prefname(void)>
  class PrefTemplate final : public TypedPref<T>
  {
    typedef TypedPref<T> BaseClass;
  public:
    PrefTemplate()
      : BaseClass(Default())
    {
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
    void Register(UpdatePolicy aUpdate, const char* aPreference)
    {
      AssertMainThread();
      switch (aUpdate) {
        case UpdatePolicy::Skip:
          break;
        case UpdatePolicy::Once:
          this->mValue = PrefGet(aPreference, this->mValue);
          break;
        case UpdatePolicy::Live:
          PrefAddVarCache(&this->mValue, aPreference, this->mValue);
          break;
        default:
          MOZ_CRASH("Incomplete switch");
      }
    }
    void Set(UpdatePolicy aUpdate, const char* aPref, T aValue)
    {
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
    const char *Name() const override {
      return Prefname();
    }
    void GetLiveValue(GfxPrefValue* aOutValue) const override {
      T value = GetLiveValue();
      CopyPrefValue(&value, aOutValue);
    }
    // When using the Preferences service, the change callback can be triggered
    // *before* our cached value is updated, so we expose a method to grab the
    // true live value.
    T GetLiveValue() const {
      return BaseClass::GetLiveValueByName(Prefname());
    }
    bool HasDefaultValue() const override {
      return this->mValue == Default();
    }
  };

  // This is where DECL_GFX_PREF for each of the preferences should go.
  // We will keep these in an alphabetical order to make it easier to see if
  // a method accessing a pref already exists. Just add yours in the list.

  DECL_GFX_PREF(Live, "accessibility.browsewithcaret", AccessibilityBrowseWithCaret, bool, false);

  // The apz prefs are explained in AsyncPanZoomController.cpp
  DECL_GFX_PREF(Live, "apz.allow_checkerboarding",             APZAllowCheckerboarding, bool, true);
  DECL_GFX_PREF(Live, "apz.allow_immediate_handoff",           APZAllowImmediateHandoff, bool, true);
  DECL_GFX_PREF(Live, "apz.allow_zooming",                     APZAllowZooming, bool, false);
  DECL_GFX_PREF(Live, "apz.autoscroll.enabled",                APZAutoscrollEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.axis_lock.breakout_angle",          APZAxisBreakoutAngle, float, float(M_PI / 8.0) /* 22.5 degrees */);
  DECL_GFX_PREF(Live, "apz.axis_lock.breakout_threshold",      APZAxisBreakoutThreshold, float, 1.0f / 32.0f);
  DECL_GFX_PREF(Live, "apz.axis_lock.direct_pan_angle",        APZAllowedDirectPanAngle, float, float(M_PI / 3.0) /* 60 degrees */);
  DECL_GFX_PREF(Live, "apz.axis_lock.lock_angle",              APZAxisLockAngle, float, float(M_PI / 6.0) /* 30 degrees */);
  DECL_GFX_PREF(Live, "apz.axis_lock.mode",                    APZAxisLockMode, int32_t, 0);
  DECL_GFX_PREF(Live, "apz.content_response_timeout",          APZContentResponseTimeout, int32_t, 400);
  DECL_GFX_PREF(Live, "apz.danger_zone_x",                     APZDangerZoneX, int32_t, 50);
  DECL_GFX_PREF(Live, "apz.danger_zone_y",                     APZDangerZoneY, int32_t, 100);
  DECL_GFX_PREF(Live, "apz.disable_for_scroll_linked_effects", APZDisableForScrollLinkedEffects, bool, false);
  DECL_GFX_PREF(Live, "apz.displayport_expiry_ms",             APZDisplayPortExpiryTime, uint32_t, 15000);
  DECL_GFX_PREF(Live, "apz.drag.enabled",                      APZDragEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.drag.initial.enabled",              APZDragInitiationEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.enlarge_displayport_when_clipped",  APZEnlargeDisplayPortWhenClipped, bool, false);
  DECL_GFX_PREF(Live, "apz.fling_accel_base_mult",             APZFlingAccelBaseMultiplier, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.fling_accel_interval_ms",           APZFlingAccelInterval, int32_t, 500);
  DECL_GFX_PREF(Live, "apz.fling_accel_supplemental_mult",     APZFlingAccelSupplementalMultiplier, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.fling_accel_min_velocity",          APZFlingAccelMinVelocity, float, 1.5f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_x1",           APZCurveFunctionX1, float, 0.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_x2",           APZCurveFunctionX2, float, 1.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_y1",           APZCurveFunctionY1, float, 0.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_y2",           APZCurveFunctionY2, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.fling_curve_threshold_inches_per_ms", APZCurveThreshold, float, -1.0f);
  DECL_GFX_PREF(Live, "apz.fling_friction",                    APZFlingFriction, float, 0.002f);
  DECL_GFX_PREF(Live, "apz.fling_min_velocity_threshold",      APZFlingMinVelocityThreshold, float, 0.5f);
  DECL_GFX_PREF(Live, "apz.fling_stop_on_tap_threshold",       APZFlingStopOnTapThreshold, float, 0.05f);
  DECL_GFX_PREF(Live, "apz.fling_stopped_threshold",           APZFlingStoppedThreshold, float, 0.01f);
  DECL_GFX_PREF(Live, "apz.frame_delay.enabled",               APZFrameDelayEnabled, bool, false);
  DECL_GFX_PREF(Once, "apz.keyboard.enabled",                  APZKeyboardEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.max_velocity_inches_per_ms",        APZMaxVelocity, float, -1.0f);
  DECL_GFX_PREF(Once, "apz.max_velocity_queue_size",           APZMaxVelocityQueueSize, uint32_t, 5);
  DECL_GFX_PREF(Live, "apz.min_skate_speed",                   APZMinSkateSpeed, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.minimap.enabled",                   APZMinimap, bool, false);
  DECL_GFX_PREF(Live, "apz.minimap.visibility.enabled",        APZMinimapVisibilityEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.one_touch_pinch.enabled",           APZOneTouchPinchEnabled, bool, true);
  DECL_GFX_PREF(Live, "apz.overscroll.enabled",                APZOverscrollEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.overscroll.min_pan_distance_ratio", APZMinPanDistanceRatio, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.overscroll.spring_friction",        APZOverscrollSpringFriction, float, 0.015f);
  DECL_GFX_PREF(Live, "apz.overscroll.spring_stiffness",       APZOverscrollSpringStiffness, float, 0.001f);
  DECL_GFX_PREF(Live, "apz.overscroll.stop_distance_threshold", APZOverscrollStopDistanceThreshold, float, 5.0f);
  DECL_GFX_PREF(Live, "apz.overscroll.stop_velocity_threshold", APZOverscrollStopVelocityThreshold, float, 0.01f);
  DECL_GFX_PREF(Live, "apz.overscroll.stretch_factor",         APZOverscrollStretchFactor, float, 0.5f);
  DECL_GFX_PREF(Live, "apz.paint_skipping.enabled",            APZPaintSkipping, bool, true);
  DECL_GFX_PREF(Live, "apz.peek_messages.enabled",             APZPeekMessages, bool, true);
  DECL_GFX_PREF(Live, "apz.popups.enabled",                    APZPopupsEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.printtree",                         APZPrintTree, bool, false);
  DECL_GFX_PREF(Live, "apz.record_checkerboarding",            APZRecordCheckerboarding, bool, false);
  DECL_GFX_PREF(Live, "apz.test.fails_with_native_injection",  APZTestFailsWithNativeInjection, bool, false);
  DECL_GFX_PREF(Live, "apz.test.logging_enabled",              APZTestLoggingEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.touch_move_tolerance",              APZTouchMoveTolerance, float, 0.0);
  DECL_GFX_PREF(Live, "apz.touch_start_tolerance",             APZTouchStartTolerance, float, 1.0f/4.5f);
  DECL_GFX_PREF(Live, "apz.velocity_bias",                     APZVelocityBias, float, 0.0f);
  DECL_GFX_PREF(Live, "apz.velocity_relevance_time_ms",        APZVelocityRelevanceTime, uint32_t, 150);
  DECL_GFX_PREF(Live, "apz.x_skate_highmem_adjust",            APZXSkateHighMemAdjust, float, 0.0f);
  DECL_GFX_PREF(Live, "apz.x_skate_size_multiplier",           APZXSkateSizeMultiplier, float, 1.5f);
  DECL_GFX_PREF(Live, "apz.x_stationary_size_multiplier",      APZXStationarySizeMultiplier, float, 3.0f);
  DECL_GFX_PREF(Live, "apz.y_skate_highmem_adjust",            APZYSkateHighMemAdjust, float, 0.0f);
  DECL_GFX_PREF(Live, "apz.y_skate_size_multiplier",           APZYSkateSizeMultiplier, float, 2.5f);
  DECL_GFX_PREF(Live, "apz.y_stationary_size_multiplier",      APZYStationarySizeMultiplier, float, 3.5f);
  DECL_GFX_PREF(Live, "apz.zoom_animation_duration_ms",        APZZoomAnimationDuration, int32_t, 250);
  DECL_GFX_PREF(Live, "apz.scale_repaint_delay_ms",            APZScaleRepaintDelay, int32_t, 500);

  DECL_GFX_PREF(Live, "browser.ui.scroll-toolbar-threshold",   ToolbarScrollThreshold, int32_t, 10);
  DECL_GFX_PREF(Live, "browser.ui.zoom.force-user-scalable",   ForceUserScalable, bool, false);
  DECL_GFX_PREF(Live, "browser.viewport.desktopWidth",         DesktopViewportWidth, int32_t, 980);

  DECL_GFX_PREF(Live, "dom.ipc.plugins.asyncdrawing.enabled",  PluginAsyncDrawingEnabled, bool, false);
  DECL_GFX_PREF(Live, "dom.meta-viewport.enabled",             MetaViewportEnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.enabled",                        VREnabled, bool, false);
  DECL_GFX_PREF(Live, "dom.vr.autoactivate.enabled",           VRAutoActivateEnabled, bool, false);
  DECL_GFX_PREF(Live, "dom.vr.controller_trigger_threshold",   VRControllerTriggerThreshold, float, 0.1f);
  DECL_GFX_PREF(Live, "dom.vr.navigation.timeout",             VRNavigationTimeout, int32_t, 1000);
  DECL_GFX_PREF(Once, "dom.vr.oculus.enabled",                 VROculusEnabled, bool, true);
  DECL_GFX_PREF(Live, "dom.vr.oculus.present.timeout",         VROculusPresentTimeout, int32_t, 10000);
  DECL_GFX_PREF(Live, "dom.vr.oculus.quit.timeout",            VROculusQuitTimeout, int32_t, 30000);
  DECL_GFX_PREF(Once, "dom.vr.openvr.enabled",                 VROpenVREnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.osvr.enabled",                   VROSVREnabled, bool, false);
  DECL_GFX_PREF(Live, "dom.vr.poseprediction.enabled",         VRPosePredictionEnabled, bool, true);
  DECL_GFX_PREF(Live, "dom.vr.require-gesture",                VRRequireGesture, bool, true);
  DECL_GFX_PREF(Live, "dom.vr.puppet.enabled",                 VRPuppetEnabled, bool, false);
  DECL_GFX_PREF(Live, "dom.vr.puppet.submitframe",             VRPuppetSubmitFrame, uint32_t, 0);
  DECL_GFX_PREF(Live, "dom.w3c_pointer_events.enabled",        PointerEventsEnabled, bool, false);
  DECL_GFX_PREF(Live, "dom.w3c_touch_events.enabled",          TouchEventsEnabled, int32_t, 0);

  DECL_GFX_PREF(Live, "general.smoothScroll",                  SmoothScrollEnabled, bool, true);
  DECL_GFX_PREF(Live, "general.smoothScroll.currentVelocityWeighting",
                SmoothScrollCurrentVelocityWeighting, float, 0.25);
  DECL_GFX_PREF(Live, "general.smoothScroll.durationToIntervalRatio",
                SmoothScrollDurationToIntervalRatio, int32_t, 200);
  DECL_GFX_PREF(Live, "general.smoothScroll.lines",            LineSmoothScrollEnabled, bool, true);
  DECL_GFX_PREF(Live, "general.smoothScroll.lines.durationMaxMS",
                LineSmoothScrollMaxDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.lines.durationMinMS",
                LineSmoothScrollMinDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.mouseWheel",       WheelSmoothScrollEnabled, bool, true);
  DECL_GFX_PREF(Live, "general.smoothScroll.mouseWheel.durationMaxMS",
                WheelSmoothScrollMaxDurationMs, int32_t, 400);
  DECL_GFX_PREF(Live, "general.smoothScroll.mouseWheel.durationMinMS",
                WheelSmoothScrollMinDurationMs, int32_t, 200);
  DECL_GFX_PREF(Live, "general.smoothScroll.other.durationMaxMS",
                OtherSmoothScrollMaxDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.other.durationMinMS",
                OtherSmoothScrollMinDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.pages",            PageSmoothScrollEnabled, bool, true);
  DECL_GFX_PREF(Live, "general.smoothScroll.pages.durationMaxMS",
                PageSmoothScrollMaxDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.pages.durationMinMS",
                PageSmoothScrollMinDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.pixels",           PixelSmoothScrollEnabled, bool, true);
  DECL_GFX_PREF(Live, "general.smoothScroll.pixels.durationMaxMS",
                PixelSmoothScrollMaxDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.pixels.durationMinMS",
                PixelSmoothScrollMinDurationMs, int32_t, 150);
  DECL_GFX_PREF(Live, "general.smoothScroll.stopDecelerationWeighting",
                SmoothScrollStopDecelerationWeighting, float, 0.4f);

  DECL_GFX_PREF(Once, "gfx.android.rgb16.force",               AndroidRGB16Force, bool, false);
#if defined(ANDROID)
  DECL_GFX_PREF(Once, "gfx.apitrace.enabled",                  UseApitrace, bool, false);
#endif
#if defined(RELEASE_OR_BETA)
  // "Skip" means this is locked to the default value in beta and release.
  DECL_GFX_PREF(Skip, "gfx.blocklist.all",                     BlocklistAll, int32_t, 0);
#else
  DECL_GFX_PREF(Once, "gfx.blocklist.all",                     BlocklistAll, int32_t, 0);
#endif
  DECL_GFX_PREF(Live, "gfx.compositor.clearstate",             CompositorClearState, bool, false);
  DECL_GFX_PREF(Live, "gfx.canvas.auto_accelerate.min_calls",  CanvasAutoAccelerateMinCalls, int32_t, 4);
  DECL_GFX_PREF(Live, "gfx.canvas.auto_accelerate.min_frames", CanvasAutoAccelerateMinFrames, int32_t, 30);
  DECL_GFX_PREF(Live, "gfx.canvas.auto_accelerate.min_seconds", CanvasAutoAccelerateMinSeconds, float, 5.0f);
  DECL_GFX_PREF(Live, "gfx.canvas.azure.accelerated",          CanvasAzureAccelerated, bool, false);
  DECL_GFX_PREF(Once, "gfx.canvas.azure.accelerated.limit",    CanvasAzureAcceleratedLimit, int32_t, 0);
  // 0x7fff is the maximum supported xlib surface size and is more than enough for canvases.
  DECL_GFX_PREF(Live, "gfx.canvas.max-size",                   MaxCanvasSize, int32_t, 0x7fff);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.cache-items",         CanvasSkiaGLCacheItems, int32_t, 256);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.cache-size",          CanvasSkiaGLCacheSize, int32_t, 96);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.dynamic-cache",       CanvasSkiaGLDynamicCache, bool, false);

  DECL_GFX_PREF(Live, "gfx.color_management.enablev4",         CMSEnableV4, bool, false);
  DECL_GFX_PREF(Live, "gfx.color_management.mode",             CMSMode, int32_t,-1);
  // The zero default here should match QCMS_INTENT_DEFAULT from qcms.h
  DECL_GFX_PREF(Live, "gfx.color_management.rendering_intent", CMSRenderingIntent, int32_t, 0);
  DECL_GFX_PREF(Live, "gfx.content.always-paint",              AlwaysPaint, bool, false);
  // Size in megabytes
  DECL_GFX_PREF(Once, "gfx.content.skia-font-cache-size",      SkiaContentFontCacheSize, int32_t, 10);

  DECL_GFX_PREF(Once, "gfx.device-reset.limit",                DeviceResetLimitCount, int32_t, 10);
  DECL_GFX_PREF(Once, "gfx.device-reset.threshold-ms",         DeviceResetThresholdMilliseconds, int32_t, -1);

  DECL_GFX_PREF(Once, "gfx.direct2d.disabled",                 Direct2DDisabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct2d.force-enabled",            Direct2DForceEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.direct3d11.reuse-decoder-device",   Direct3D11ReuseDecoderDevice, int32_t, -1);
  DECL_GFX_PREF(Live, "gfx.direct3d11.allow-keyed-mutex",      Direct3D11AllowKeyedMutex, bool, true);
  DECL_GFX_PREF(Live, "gfx.direct3d11.use-double-buffering",   Direct3D11UseDoubleBuffering, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct3d11.enable-debug-layer",     Direct3D11EnableDebugLayer, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct3d11.break-on-error",         Direct3D11BreakOnError, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct3d11.sleep-on-create-device", Direct3D11SleepOnCreateDevice, int32_t, 0);
  DECL_GFX_PREF(Live, "gfx.downloadable_fonts.keep_variation_tables", KeepVariationTables, bool, false);
  DECL_GFX_PREF(Live, "gfx.downloadable_fonts.otl_validation", ValidateOTLTables, bool, true);
  DECL_GFX_PREF(Live, "gfx.draw-color-bars",                   CompositorDrawColorBars, bool, false);
  DECL_GFX_PREF(Once, "gfx.e10s.hide-plugins-for-scroll",      HidePluginsForScroll, bool, true);
  DECL_GFX_PREF(Live, "gfx.layerscope.enabled",                LayerScopeEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.layerscope.port",                   LayerScopePort, int32_t, 23456);
  // Note that        "gfx.logging.level" is defined in Logging.h.
  DECL_GFX_PREF(Live, "gfx.logging.level",                     GfxLoggingLevel, int32_t, mozilla::gfx::LOG_DEFAULT);
  DECL_GFX_PREF(Once, "gfx.logging.crash.length",              GfxLoggingCrashLength, uint32_t, 16);
  DECL_GFX_PREF(Live, "gfx.logging.painted-pixel-count.enabled",GfxLoggingPaintedPixelCountEnabled, bool, false);
  // The maximums here are quite conservative, we can tighten them if problems show up.
  DECL_GFX_PREF(Once, "gfx.logging.texture-usage.enabled",     GfxLoggingTextureUsageEnabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.logging.peak-texture-usage.enabled",GfxLoggingPeakTextureUsageEnabled, bool, false);
  // Use gfxPlatform::MaxAllocSize instead of the pref directly
  DECL_GFX_PREF(Once, "gfx.max-alloc-size",                    MaxAllocSizeDoNotUseDirectly, int32_t, (int32_t)500000000);
  // Use gfxPlatform::MaxTextureSize instead of the pref directly
  DECL_GFX_PREF(Once, "gfx.max-texture-size",                  MaxTextureSizeDoNotUseDirectly, int32_t, (int32_t)32767);
  DECL_GFX_PREF(Live, "gfx.partialpresent.force",              PartialPresent, int32_t, 0);
  DECL_GFX_PREF(Live, "gfx.perf-warnings.enabled",             PerfWarnings, bool, false);
  DECL_GFX_PREF(Live, "gfx.SurfaceTexture.detach.enabled",     SurfaceTextureDetachEnabled, bool, true);
  DECL_GFX_PREF(Live, "gfx.testing.device-reset",              DeviceResetForTesting, int32_t, 0);
  DECL_GFX_PREF(Live, "gfx.testing.device-fail",               DeviceFailForTesting, bool, false);
  DECL_GFX_PREF(Once, "gfx.text.disable-aa",                   DisableAllTextAA, bool, false);
  DECL_GFX_PREF(Live, "gfx.ycbcr.accurate-conversion",         YCbCrAccurateConversion, bool, false);

  // Disable surface sharing due to issues with compatible FBConfigs on
  // NVIDIA drivers as described in bug 1193015.
  DECL_GFX_PREF(Live, "gfx.use-glx-texture-from-pixmap",       UseGLXTextureFromPixmap, bool, false);
  DECL_GFX_PREF(Once, "gfx.use-iosurface-textures",            UseIOSurfaceTextures, bool, false);
  DECL_GFX_PREF(Once, "gfx.use-mutex-on-present",              UseMutexOnPresent, bool, false);
  // These times should be in milliseconds
  DECL_GFX_PREF(Once, "gfx.touch.resample.delay-threshold",    TouchResampleVsyncDelayThreshold, int32_t, 20);
  DECL_GFX_PREF(Once, "gfx.touch.resample.max-predict",        TouchResampleMaxPredict, int32_t, 8);
  DECL_GFX_PREF(Once, "gfx.touch.resample.min-delta",          TouchResampleMinDelta, int32_t, 2);
  DECL_GFX_PREF(Once, "gfx.touch.resample.old-touch-threshold",TouchResampleOldTouchThreshold, int32_t, 17);
  DECL_GFX_PREF(Once, "gfx.touch.resample.vsync-adjust",       TouchVsyncSampleAdjust, int32_t, 5);

  DECL_GFX_PREF(Live, "gfx.vsync.collect-scroll-transforms",   CollectScrollTransforms, bool, false);
  DECL_GFX_PREF(Once, "gfx.vsync.compositor.unobserve-count",  CompositorUnobserveCount, int32_t, 10);

  DECL_GFX_PREF(Live, "gfx.webrender.blob-images",             WebRenderBlobImages, bool, false);
  DECL_GFX_PREF(Live, "gfx.webrender.highlight-painted-layers",WebRenderHighlightPaintedLayers, bool, false);
  DECL_GFX_PREF(Live, "gfx.webrender.layers-free",             WebRenderLayersFree, bool, false);
  DECL_GFX_PREF(Live, "gfx.webrender.profiler.enabled",        WebRenderProfilerEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.webrendest.enabled",                WebRendestEnabled, bool, false);

  // Use vsync events generated by hardware
  DECL_GFX_PREF(Once, "gfx.work-around-driver-bugs",           WorkAroundDriverBugs, bool, true);
  DECL_GFX_PREF(Once, "gfx.screen-mirroring.enabled",          ScreenMirroringEnabled, bool, false);

  DECL_GFX_PREF(Live, "gl.ignore-dx-interop2-blacklist",       IgnoreDXInterop2Blacklist, bool, false);
  DECL_GFX_PREF(Live, "gl.msaa-level",                         MSAALevel, uint32_t, 2);
#if defined(XP_MACOSX)
  DECL_GFX_PREF(Live, "gl.multithreaded",                      GLMultithreaded, bool, false);
#endif
  DECL_GFX_PREF(Live, "gl.require-hardware",                   RequireHardwareGL, bool, false);
  DECL_GFX_PREF(Live, "gl.use-tls-is-current",                 UseTLSIsCurrent, int32_t, 0);

  DECL_GFX_PREF(Once, "image.cache.size",                      ImageCacheSize, int32_t, 5*1024*1024);
  DECL_GFX_PREF(Once, "image.cache.timeweight",                ImageCacheTimeWeight, int32_t, 500);
  DECL_GFX_PREF(Live, "image.decode-immediately.enabled",      ImageDecodeImmediatelyEnabled, bool, false);
  DECL_GFX_PREF(Live, "image.downscale-during-decode.enabled", ImageDownscaleDuringDecodeEnabled, bool, true);
  DECL_GFX_PREF(Live, "image.infer-src-animation.threshold-ms", ImageInferSrcAnimationThresholdMS, uint32_t, 2000);
  DECL_GFX_PREF(Live, "image.layout_network_priority",         ImageLayoutNetworkPriority, bool, true);
  DECL_GFX_PREF(Once, "image.mem.decode_bytes_at_a_time",      ImageMemDecodeBytesAtATime, uint32_t, 200000);
  DECL_GFX_PREF(Live, "image.mem.discardable",                 ImageMemDiscardable, bool, false);
  DECL_GFX_PREF(Once, "image.mem.animated.discardable",        ImageMemAnimatedDiscardable, bool, false);
  DECL_GFX_PREF(Live, "image.mem.shared",                      ImageMemShared, bool, false);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.discard_factor", ImageMemSurfaceCacheDiscardFactor, uint32_t, 1);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.max_size_kb",    ImageMemSurfaceCacheMaxSizeKB, uint32_t, 100 * 1024);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.min_expiration_ms", ImageMemSurfaceCacheMinExpirationMS, uint32_t, 60*1000);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.size_factor",    ImageMemSurfaceCacheSizeFactor, uint32_t, 64);
  DECL_GFX_PREF(Once, "image.multithreaded_decoding.limit",    ImageMTDecodingLimit, int32_t, -1);

  DECL_GFX_PREF(Once, "layers.acceleration.disabled",          LayersAccelerationDisabledDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps",          LayersDrawFPS, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps.print-histogram",  FPSPrintHistogram, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps.write-to-file", WriteFPSToFile, bool, false);
  DECL_GFX_PREF(Once, "layers.acceleration.force-enabled",     LayersAccelerationForceEnabledDoNotUseDirectly, bool, false);
  DECL_OVERRIDE_PREF(Live, "layers.advanced.background-color",        LayersAllowBackgroundColorLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.background-image",        LayersAllowBackgroundImage, gfxPrefs::OverrideBase_WebRendest());
  DECL_GFX_PREF(Live, "layers.advanced.basic-layer.enabled",          LayersAdvancedBasicLayerEnabled, bool, false);
  DECL_OVERRIDE_PREF(Live, "layers.advanced.border-layers",           LayersAllowBorderLayers, gfxPrefs::OverrideBase_WebRendest());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.boxshadow-inset-layers",  LayersAllowInsetBoxShadow, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.boxshadow-outer-layers",  LayersAllowOuterBoxShadow, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.bullet-layers",           LayersAllowBulletLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.button-foreground-layers", LayersAllowButtonForegroundLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.canvas-background-color", LayersAllowCanvasBackgroundColorLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.caret-layers",            LayersAllowCaretLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.columnRule-layers",       LayersAllowColumnRuleLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.displaybuttonborder-layers", LayersAllowDisplayButtonBorder, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.filter-layers",           LayersAllowFilterLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.image-layers",            LayersAllowImageLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.outline-layers",          LayersAllowOutlineLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.solid-color",             LayersAllowSolidColorLayers, gfxPrefs::OverrideBase_WebRender());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.table",                   LayersAllowTable, gfxPrefs::OverrideBase_WebRendest());
  DECL_OVERRIDE_PREF(Live, "layers.advanced.text-layers",             LayersAllowTextLayers, gfxPrefs::OverrideBase_WebRendest());
  DECL_GFX_PREF(Once, "layers.amd-switchable-gfx.enabled",     LayersAMDSwitchableGfxEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.async-pan-zoom.enabled",         AsyncPanZoomEnabledDoNotUseDirectly, bool, true);
  DECL_GFX_PREF(Once, "layers.async-pan-zoom.separate-event-thread", AsyncPanZoomSeparateEventThread, bool, false);
  DECL_GFX_PREF(Live, "layers.bench.enabled",                  LayersBenchEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.bufferrotation.enabled",         BufferRotationEnabled, bool, true);
  DECL_GFX_PREF(Live, "layers.child-process-shutdown",         ChildProcessShutdown, bool, true);
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  // If MOZ_GFX_OPTIMIZE_MOBILE is defined, we force component alpha off
  // and ignore the preference.
  DECL_GFX_PREF(Skip, "layers.componentalpha.enabled",         ComponentAlphaEnabled, bool, false);
#else
  // If MOZ_GFX_OPTIMIZE_MOBILE is not defined, we actually take the
  // preference value, defaulting to true.
  DECL_GFX_PREF(Once, "layers.componentalpha.enabled",         ComponentAlphaEnabled, bool, true);
#endif
  DECL_GFX_PREF(Live, "layers.composer2d.enabled",             Composer2DCompositionEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.d3d11.force-warp",               LayersD3D11ForceWARP, bool, false);
  DECL_GFX_PREF(Live, "layers.deaa.enabled",                   LayersDEAAEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.draw-bigimage-borders",          DrawBigImageBorders, bool, false);
  DECL_GFX_PREF(Live, "layers.draw-borders",                   DrawLayerBorders, bool, false);
  DECL_GFX_PREF(Live, "layers.draw-tile-borders",              DrawTileBorders, bool, false);
  DECL_GFX_PREF(Live, "layers.draw-layer-info",                DrawLayerInfo, bool, false);
  DECL_GFX_PREF(Live, "layers.dump",                           LayersDump, bool, false);
  DECL_GFX_PREF(Live, "layers.dump-texture",                   LayersDumpTexture, bool, false);
#ifdef MOZ_DUMP_PAINTING
  DECL_GFX_PREF(Live, "layers.dump-client-layers",             DumpClientLayers, bool, false);
  DECL_GFX_PREF(Live, "layers.dump-decision",                  LayersDumpDecision, bool, false);
  DECL_GFX_PREF(Live, "layers.dump-host-layers",               DumpHostLayers, bool, false);
#endif

  // 0 is "no change" for contrast, positive values increase it, negative values
  // decrease it until we hit mid gray at -1 contrast, after that it gets weird.
  DECL_GFX_PREF(Live, "layers.effect.contrast",                LayersEffectContrast, float, 0.0f);
  DECL_GFX_PREF(Live, "layers.effect.grayscale",               LayersEffectGrayscale, bool, false);
  DECL_GFX_PREF(Live, "layers.effect.invert",                  LayersEffectInvert, bool, false);
  DECL_GFX_PREF(Once, "layers.enable-tiles",                   LayersTilesEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.flash-borders",                  FlashLayerBorders, bool, false);
  DECL_GFX_PREF(Once, "layers.force-shmem-tiles",              ForceShmemTiles, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.allow-software",     GPUProcessAllowSoftware, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.enabled",            GPUProcessEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.force-enabled",      GPUProcessForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.gpu-process.ipc_reply_timeout_ms", GPUProcessIPCReplyTimeoutMs, int32_t, 10000);
  DECL_GFX_PREF(Live, "layers.gpu-process.max_restarts",       GPUProcessMaxRestarts, int32_t, 1);
  // Note: This pref will only be used if it is less than layers.gpu-process.max_restarts.
  DECL_GFX_PREF(Live, "layers.gpu-process.max_restarts_with_decoder", GPUProcessMaxRestartsWithDecoder, int32_t, 0);
  DECL_GFX_PREF(Once, "layers.gpu-process.startup_timeout_ms", GPUProcessTimeoutMs, int32_t, 5000);
  DECL_GFX_PREF(Live, "layers.low-precision-buffer",           UseLowPrecisionBuffer, bool, false);
  DECL_GFX_PREF(Live, "layers.low-precision-opacity",          LowPrecisionOpacity, float, 1.0f);
  DECL_GFX_PREF(Live, "layers.low-precision-resolution",       LowPrecisionResolution, float, 0.25f);
  DECL_GFX_PREF(Live, "layers.max-active",                     MaxActiveLayers, int32_t, -1);
  DECL_GFX_PREF(Once, "layers.mlgpu.dev-enabled",              AdvancedLayersEnabledDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-buffer-cache",      AdvancedLayersEnableBufferCache, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-buffer-sharing",    AdvancedLayersEnableBufferSharing, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-clear-view",        AdvancedLayersEnableClearView, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-cpu-occlusion",     AdvancedLayersEnableCPUOcclusion, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-depth-buffer",      AdvancedLayersEnableDepthBuffer, bool, false);
  DECL_GFX_PREF(Live, "layers.mlgpu.enable-invalidation",      AdvancedLayersUseInvalidation, bool, true);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-on-windows7",       AdvancedLayersEnableOnWindows7, bool, false);
  DECL_GFX_PREF(Once, "layers.mlgpu.enable-container-resizing", AdvancedLayersEnableContainerResizing, bool, true);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.force-disabled", LayersOffMainThreadCompositionForceDisabled, bool, false);
  DECL_GFX_PREF(Live, "layers.offmainthreadcomposition.frame-rate", LayersCompositionFrameRate, int32_t,-1);
  DECL_GFX_PREF(Live, "layers.omtp.force-sync",                LayersOMTPForceSync, bool, true);
  DECL_GFX_PREF(Live, "layers.orientation.sync.timeout",       OrientationSyncMillis, uint32_t, (uint32_t)0);
  DECL_GFX_PREF(Once, "layers.prefer-opengl",                  LayersPreferOpenGL, bool, false);
  DECL_GFX_PREF(Live, "layers.progressive-paint",              ProgressivePaint, bool, false);
  DECL_GFX_PREF(Live, "layers.shared-buffer-provider.enabled", PersistentBufferProviderSharedEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.single-tile.enabled",            LayersSingleTileEnabled, bool, true);
  DECL_GFX_PREF(Once, "layers.stereo-video.enabled",           StereoVideoEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.force-synchronous-resize",       LayersForceSynchronousResize, bool, false);

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
  DECL_GFX_PREF(Once, "layers.tiles.edge-padding",             TileEdgePaddingEnabled, bool, true);
  DECL_GFX_PREF(Live, "layers.tiles.fade-in.enabled",          LayerTileFadeInEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.tiles.fade-in.duration-ms",      LayerTileFadeInDuration, uint32_t, 250);
  DECL_GFX_PREF(Live, "layers.transaction.warning-ms",         LayerTransactionWarning, uint32_t, 200);
  DECL_GFX_PREF(Once, "layers.uniformity-info",                UniformityInfo, bool, false);
  DECL_GFX_PREF(Once, "layers.use-image-offscreen-surfaces",   UseImageOffscreenSurfaces, bool, true);
  DECL_GFX_PREF(Live, "layers.draw-mask-debug",                DrawMaskLayer, bool, false);

  DECL_GFX_PREF(Live, "layers.geometry.opengl.enabled",        OGLLayerGeometry, bool, false);
  DECL_GFX_PREF(Live, "layers.geometry.basic.enabled",         BasicLayerGeometry, bool, false);
  DECL_GFX_PREF(Live, "layers.geometry.d3d11.enabled",         D3D11LayerGeometry, bool, false);

  DECL_GFX_PREF(Live, "layout.animation.prerender.partial", PartiallyPrerenderAnimatedContent, bool, false);
  DECL_GFX_PREF(Live, "layout.animation.prerender.viewport-ratio-limit-x", AnimationPrerenderViewportRatioLimitX, float, 1.125f);
  DECL_GFX_PREF(Live, "layout.animation.prerender.viewport-ratio-limit-y", AnimationPrerenderViewportRatioLimitY, float, 1.125f);
  DECL_GFX_PREF(Live, "layout.animation.prerender.absolute-limit-x", AnimationPrerenderAbsoluteLimitX, uint32_t, 4096);
  DECL_GFX_PREF(Live, "layout.animation.prerender.absolute-limit-y", AnimationPrerenderAbsoluteLimitY, uint32_t, 4096);

  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.damping-ratio", ScrollBehaviorDampingRatio, float, 1.0f);
  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.enabled",    ScrollBehaviorEnabled, bool, true);
  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.spring-constant", ScrollBehaviorSpringConstant, float, 250.0f);
  DECL_GFX_PREF(Live, "layout.css.scroll-snap.prediction-max-velocity", ScrollSnapPredictionMaxVelocity, int32_t, 2000);
  DECL_GFX_PREF(Live, "layout.css.scroll-snap.prediction-sensitivity", ScrollSnapPredictionSensitivity, float, 0.750f);
  DECL_GFX_PREF(Live, "layout.css.scroll-snap.proximity-threshold", ScrollSnapProximityThreshold, int32_t, 200);
  DECL_GFX_PREF(Live, "layout.css.touch_action.enabled",       TouchActionEnabled, bool, false);
  DECL_GFX_PREF(Live, "layout.display-list.dump",              LayoutDumpDisplayList, bool, false);
  DECL_GFX_PREF(Live, "layout.display-list.dump-content",      LayoutDumpDisplayListContent, bool, false);
  DECL_GFX_PREF(Live, "layout.event-regions.enabled",          LayoutEventRegionsEnabledDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Once, "layout.frame_rate",                     LayoutFrameRate, int32_t, -1);
  DECL_GFX_PREF(Live, "layout.min-active-layer-size",          LayoutMinActiveLayerSize, int, 64);
  DECL_GFX_PREF(Once, "layout.paint_rects_separately",         LayoutPaintRectsSeparately, bool, true);

  // This and code dependent on it should be removed once containerless scrolling looks stable.
  DECL_GFX_PREF(Once, "layout.scroll.root-frame-containers",   LayoutUseContainersForRootFrames, bool, true);
  // This pref is to be set by test code only.
  DECL_GFX_PREF(Live, "layout.scrollbars.always-layerize-track", AlwaysLayerizeScrollbarTrackTestOnly, bool, false);
  DECL_GFX_PREF(Live, "layout.smaller-painted-layers",         LayoutSmallerPaintedLayers, bool, false);

  DECL_GFX_PREF(Once, "media.hardware-video-decoding.force-enabled",
                                                               HardwareVideoDecodingForceEnabled, bool, false);
#ifdef XP_WIN
  DECL_GFX_PREF(Live, "media.windows-media-foundation.allow-d3d11-dxva", PDMWMFAllowD3D11, bool, true);
  DECL_GFX_PREF(Live, "media.windows-media-foundation.max-dxva-videos", PDMWMFMaxDXVAVideos, uint32_t, 8);
  DECL_GFX_PREF(Live, "media.windows-media-foundation.use-nv12-format", PDMWMFUseNV12Format, bool, true);
  DECL_GFX_PREF(Once, "media.windows-media-foundation.use-sync-texture", PDMWMFUseSyncTexture, bool, true);
  DECL_GFX_PREF(Live, "media.wmf.low-latency.enabled", PDMWMFLowLatencyEnabled, bool, false);
  DECL_GFX_PREF(Live, "media.wmf.skip-blacklist", PDMWMFSkipBlacklist, bool, false);
#endif

  // These affect how line scrolls from wheel events will be accelerated.
  DECL_GFX_PREF(Live, "mousewheel.acceleration.factor",        MouseWheelAccelerationFactor, int32_t, -1);
  DECL_GFX_PREF(Live, "mousewheel.acceleration.start",         MouseWheelAccelerationStart, int32_t, -1);

  // This affects whether events will be routed through APZ or not.
  DECL_GFX_PREF(Live, "mousewheel.system_scroll_override_on_root_content.enabled",
                                                               MouseWheelHasRootScrollDeltaOverride, bool, false);
  DECL_GFX_PREF(Live, "mousewheel.system_scroll_override_on_root_content.horizontal.factor",
                                                               MouseWheelRootScrollHorizontalFactor, int32_t, 0);
  DECL_GFX_PREF(Live, "mousewheel.system_scroll_override_on_root_content.vertical.factor",
                                                               MouseWheelRootScrollVerticalFactor, int32_t, 0);
  DECL_GFX_PREF(Live, "mousewheel.transaction.ignoremovedelay",MouseWheelIgnoreMoveDelayMs, int32_t, (int32_t)100);
  DECL_GFX_PREF(Live, "mousewheel.transaction.timeout",        MouseWheelTransactionTimeoutMs, int32_t, (int32_t)1500);

  DECL_GFX_PREF(Live, "nglayout.debug.widget_update_flashing", WidgetUpdateFlashing, bool, false);

  DECL_GFX_PREF(Once, "slider.snapMultiplier",                 SliderSnapMultiplier, int32_t, 0);

  DECL_GFX_PREF(Live, "test.events.async.enabled",             TestEventsAsyncEnabled, bool, false);
  DECL_GFX_PREF(Live, "test.mousescroll",                      MouseScrollTestingEnabled, bool, false);

  DECL_GFX_PREF(Live, "toolkit.scrollbox.horizontalScrollDistance", ToolkitHorizontalScrollDistance, int32_t, 5);
  DECL_GFX_PREF(Live, "toolkit.scrollbox.verticalScrollDistance",   ToolkitVerticalScrollDistance, int32_t, 3);

  DECL_GFX_PREF(Live, "ui.click_hold_context_menus.delay",     UiClickHoldContextMenusDelay, int32_t, 500);

  // WebGL (for pref access from Worker threads)
  DECL_GFX_PREF(Live, "webgl.all-angle-options",               WebGLAllANGLEOptions, bool, false);
  DECL_GFX_PREF(Live, "webgl.angle.force-d3d11",               WebGLANGLEForceD3D11, bool, false);
  DECL_GFX_PREF(Live, "webgl.angle.try-d3d11",                 WebGLANGLETryD3D11, bool, false);
  DECL_GFX_PREF(Live, "webgl.angle.force-warp",                WebGLANGLEForceWARP, bool, false);
  DECL_GFX_PREF(Live, "webgl.bypass-shader-validation",        WebGLBypassShaderValidator, bool, true);
  DECL_GFX_PREF(Live, "webgl.can-lose-context-in-foreground",  WebGLCanLoseContextInForeground, bool, true);
  DECL_GFX_PREF(Live, "webgl.default-no-alpha",                WebGLDefaultNoAlpha, bool, false);
  DECL_GFX_PREF(Live, "webgl.disable-angle",                   WebGLDisableANGLE, bool, false);
  DECL_GFX_PREF(Live, "webgl.disable-wgl",                     WebGLDisableWGL, bool, false);
  DECL_GFX_PREF(Live, "webgl.disable-extensions",              WebGLDisableExtensions, bool, false);
  DECL_GFX_PREF(Live, "webgl.dxgl.enabled",                    WebGLDXGLEnabled, bool, false);
  DECL_GFX_PREF(Live, "webgl.dxgl.needs-finish",               WebGLDXGLNeedsFinish, bool, false);

  DECL_GFX_PREF(Live, "webgl.disable-fail-if-major-performance-caveat",
                WebGLDisableFailIfMajorPerformanceCaveat, bool, false);
  DECL_GFX_PREF(Live, "webgl.disable-DOM-blit-uploads",
                WebGLDisableDOMBlitUploads, bool, false);

  DECL_GFX_PREF(Live, "webgl.disabled",                        WebGLDisabled, bool, false);

  DECL_GFX_PREF(Live, "webgl.enable-draft-extensions",         WebGLDraftExtensionsEnabled, bool, false);
  DECL_GFX_PREF(Live, "webgl.enable-privileged-extensions",    WebGLPrivilegedExtensionsEnabled, bool, false);
  DECL_GFX_PREF(Live, "webgl.enable-webgl2",                   WebGL2Enabled, bool, true);
  DECL_GFX_PREF(Live, "webgl.force-enabled",                   WebGLForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "webgl.force-layers-readback",           WebGLForceLayersReadback, bool, false);
  DECL_GFX_PREF(Live, "webgl.force-index-validation",          WebGLForceIndexValidation, bool, false);
  DECL_GFX_PREF(Live, "webgl.lose-context-on-memory-pressure", WebGLLoseContextOnMemoryPressure, bool, false);
  DECL_GFX_PREF(Live, "webgl.max-warnings-per-context",        WebGLMaxWarningsPerContext, uint32_t, 32);
  DECL_GFX_PREF(Live, "webgl.min_capability_mode",             WebGLMinCapabilityMode, bool, false);
  DECL_GFX_PREF(Live, "webgl.msaa-force",                      WebGLForceMSAA, bool, false);
  DECL_GFX_PREF(Live, "webgl.prefer-16bpp",                    WebGLPrefer16bpp, bool, false);
  DECL_GFX_PREF(Live, "webgl.restore-context-when-visible",    WebGLRestoreWhenVisible, bool, true);
  DECL_GFX_PREF(Live, "webgl.allow-immediate-queries",         WebGLImmediateQueries, bool, false);
  DECL_GFX_PREF(Live, "webgl.allow-fb-invalidation",           WebGLFBInvalidation, bool, false);

  DECL_GFX_PREF(Live, "webgl.perf.max-warnings",                    WebGLMaxPerfWarnings, int32_t, 0);
  DECL_GFX_PREF(Live, "webgl.perf.max-acceptable-fb-status-invals", WebGLMaxAcceptableFBStatusInvals, int32_t, 0);
  DECL_GFX_PREF(Live, "webgl.perf.spew-frame-allocs",          WebGLSpewFrameAllocs, bool, true);

  DECL_GFX_PREF(Live, "webgl.webgl2-compat-mode",              WebGL2CompatMode, bool, false);

  DECL_GFX_PREF(Live, "widget.window-transforms.disabled",     WindowTransformsDisabled, bool, false);

  // WARNING:
  // Please make sure that you've added your new preference to the list above in alphabetical order.
  // Please do not just append it to the end of the list.

public:
  // Manage the singleton:
  static gfxPrefs& GetSingleton()
  {
    MOZ_ASSERT(!sInstanceHasBeenDestroyed, "Should never recreate a gfxPrefs instance!");
    if (!sInstance) {
      sGfxPrefList = new nsTArray<Pref*>();
      sInstance = new gfxPrefs;
      sInstance->Init();
    }
    MOZ_ASSERT(SingletonExists());
    return *sInstance;
  }
  static void DestroySingleton();
  static bool SingletonExists();

private:
  static gfxPrefs* sInstance;
  static bool sInstanceHasBeenDestroyed;
  static nsTArray<Pref*>* sGfxPrefList;

private:
  // The constructor cannot access GetSingleton(), since sInstance (necessarily)
  // has not been assigned yet. Follow-up initialization that needs GetSingleton()
  // must be added to Init().
  void Init();

  static bool IsPrefsServiceAvailable();
  static bool IsParentProcess();
  // Creating these to avoid having to include Preferences.h in the .h
  static void PrefAddVarCache(bool*, const char*, bool);
  static void PrefAddVarCache(int32_t*, const char*, int32_t);
  static void PrefAddVarCache(uint32_t*, const char*, uint32_t);
  static void PrefAddVarCache(float*, const char*, float);
  static void PrefAddVarCache(std::string*, const char*, std::string);
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
  static bool OverrideBase_WebRendest();

  gfxPrefs();
  ~gfxPrefs();
  gfxPrefs(const gfxPrefs&) = delete;
  gfxPrefs& operator=(const gfxPrefs&) = delete;
};

#undef DECL_GFX_PREF /* Don't need it outside of this file */

#endif /* GFX_PREFS_H */
