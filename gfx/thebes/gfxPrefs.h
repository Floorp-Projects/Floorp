/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PREFS_H
#define GFX_PREFS_H

#include <stdint.h>
#include "mozilla/Assertions.h"
#include "mozilla/Constants.h"   // for M_PI

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
// pref is updated after initialization.

#define DECL_GFX_PREF(Update, Pref, Name, Type, Default)                     \
public:                                                                       \
static Type Name() { MOZ_ASSERT(SingletonExists()); return GetSingleton().mPref##Name.mValue; } \
static void Set##Name(Type aVal) { MOZ_ASSERT(SingletonExists()); \
    GetSingleton().mPref##Name.Set(UpdatePolicy::Update, Get##Name##PrefName(), aVal); } \
private:                                                                      \
static const char* Get##Name##PrefName() { return Pref; }                     \
static Type Get##Name##PrefDefault() { return Default; }                      \
PrefTemplate<UpdatePolicy::Update, Type, Get##Name##PrefDefault, Get##Name##PrefName> mPref##Name

class PreferenceAccessImpl;
class gfxPrefs;
class gfxPrefs final
{
private:
  /// See Logging.h.  This lets Moz2D access preference values it owns.
  PreferenceAccessImpl* mMoz2DPrefAccess;

private:
  // Enums for the update policy.
  enum class UpdatePolicy {
    Skip, // Set the value to default, skip any Preferences calls
    Once, // Evaluate the preference once, unchanged during the session
    Live  // Evaluate the preference and set callback so it stays current/live
  };

  // Since we cannot use const char*, use a function that returns it.
  template <UpdatePolicy Update, class T, T Default(void), const char* Pref(void)>
  class PrefTemplate
  {
  public:
    PrefTemplate()
    : mValue(Default())
    {
      Register(Update, Pref());
    }
    void Register(UpdatePolicy aUpdate, const char* aPreference)
    {
      AssertMainThread();
      switch(aUpdate) {
        case UpdatePolicy::Skip:
          break;
        case UpdatePolicy::Once:
          mValue = PrefGet(aPreference, mValue);
          break;
        case UpdatePolicy::Live:
          PrefAddVarCache(&mValue,aPreference, mValue);
          break;
        default:
          MOZ_CRASH();
          break;
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
          mValue = PrefGet(aPref, mValue);
          break;
        default:
          MOZ_CRASH();
          break;
      }
    }
    T mValue;
  };

  // This is where DECL_GFX_PREF for each of the preferences should go.
  // We will keep these in an alphabetical order to make it easier to see if
  // a method accessing a pref already exists. Just add yours in the list.

  // It's a short time fix, and will be removed after landing bug 1206637.
  DECL_GFX_PREF(Live, "accessibility.monoaudio.enable",        MonoAudio, bool, false);

  // The apz prefs are explained in AsyncPanZoomController.cpp
  DECL_GFX_PREF(Live, "apz.allow_checkerboarding",             APZAllowCheckerboarding, bool, true);
  DECL_GFX_PREF(Live, "apz.allow_zooming",                     APZAllowZooming, bool, false);
  DECL_GFX_PREF(Live, "apz.asyncscroll.throttle",              APZAsyncScrollThrottleTime, int32_t, 100);
  DECL_GFX_PREF(Live, "apz.asyncscroll.timeout",               APZAsyncScrollTimeout, int32_t, 300);
  DECL_GFX_PREF(Live, "apz.axis_lock.breakout_angle",          APZAxisBreakoutAngle, float, float(M_PI / 8.0) /* 22.5 degrees */);
  DECL_GFX_PREF(Live, "apz.axis_lock.breakout_threshold",      APZAxisBreakoutThreshold, float, 1.0f / 32.0f);
  DECL_GFX_PREF(Live, "apz.axis_lock.direct_pan_angle",        APZAllowedDirectPanAngle, float, float(M_PI / 3.0) /* 60 degrees */);
  DECL_GFX_PREF(Live, "apz.axis_lock.lock_angle",              APZAxisLockAngle, float, float(M_PI / 6.0) /* 30 degrees */);
  DECL_GFX_PREF(Live, "apz.axis_lock.mode",                    APZAxisLockMode, int32_t, 0);
  DECL_GFX_PREF(Live, "apz.content_response_timeout",          APZContentResponseTimeout, int32_t, 300);
  DECL_GFX_PREF(Live, "apz.cross_slide.enabled",               APZCrossSlideEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.danger_zone_x",                     APZDangerZoneX, int32_t, 50);
  DECL_GFX_PREF(Live, "apz.danger_zone_y",                     APZDangerZoneY, int32_t, 100);
  DECL_GFX_PREF(Live, "apz.drag.enabled",                      APZDragEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.enlarge_displayport_when_clipped",  APZEnlargeDisplayPortWhenClipped, bool, false);
  DECL_GFX_PREF(Live, "apz.fling_accel_base_mult",             APZFlingAccelBaseMultiplier, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.fling_accel_interval_ms",           APZFlingAccelInterval, int32_t, 500);
  DECL_GFX_PREF(Live, "apz.fling_accel_supplemental_mult",     APZFlingAccelSupplementalMultiplier, float, 1.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_x1",           APZCurveFunctionX1, float, 0.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_x2",           APZCurveFunctionX2, float, 1.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_y1",           APZCurveFunctionY1, float, 0.0f);
  DECL_GFX_PREF(Once, "apz.fling_curve_function_y2",           APZCurveFunctionY2, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.fling_curve_threshold_inches_per_ms", APZCurveThreshold, float, -1.0f);
  DECL_GFX_PREF(Once, "apz.fling_friction",                    APZFlingFriction, float, 0.002f);
  DECL_GFX_PREF(Live, "apz.fling_repaint_interval",            APZFlingRepaintInterval, int32_t, 75);
  DECL_GFX_PREF(Once, "apz.fling_stop_on_tap_threshold",       APZFlingStopOnTapThreshold, float, 0.05f);
  DECL_GFX_PREF(Once, "apz.fling_stopped_threshold",           APZFlingStoppedThreshold, float, 0.01f);
  DECL_GFX_PREF(Once, "apz.highlight_checkerboarded_areas",    APZHighlightCheckerboardedAreas, bool, false);
  DECL_GFX_PREF(Once, "apz.max_velocity_inches_per_ms",        APZMaxVelocity, float, -1.0f);
  DECL_GFX_PREF(Once, "apz.max_velocity_queue_size",           APZMaxVelocityQueueSize, uint32_t, 5);
  DECL_GFX_PREF(Live, "apz.min_skate_speed",                   APZMinSkateSpeed, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.minimap.enabled",                   APZMinimap, bool, false);
  DECL_GFX_PREF(Live, "apz.num_paint_duration_samples",        APZNumPaintDurationSamples, int32_t, 3);
  DECL_GFX_PREF(Live, "apz.overscroll.enabled",                APZOverscrollEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.overscroll.min_pan_distance_ratio", APZMinPanDistanceRatio, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.overscroll.spring_friction",        APZOverscrollSpringFriction, float, 0.015f);
  DECL_GFX_PREF(Live, "apz.overscroll.spring_stiffness",       APZOverscrollSpringStiffness, float, 0.001f);
  DECL_GFX_PREF(Live, "apz.overscroll.stop_distance_threshold", APZOverscrollStopDistanceThreshold, float, 5.0f);
  DECL_GFX_PREF(Live, "apz.overscroll.stop_velocity_threshold", APZOverscrollStopVelocityThreshold, float, 0.01f);
  DECL_GFX_PREF(Live, "apz.overscroll.stretch_factor",         APZOverscrollStretchFactor, float, 0.5f);
  DECL_GFX_PREF(Live, "apz.pan_repaint_interval",              APZPanRepaintInterval, int32_t, 250);
  DECL_GFX_PREF(Live, "apz.printtree",                         APZPrintTree, bool, false);
  DECL_GFX_PREF(Live, "apz.smooth_scroll_repaint_interval",    APZSmoothScrollRepaintInterval, int32_t, 75);
  DECL_GFX_PREF(Live, "apz.test.logging_enabled",              APZTestLoggingEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.touch_start_tolerance",             APZTouchStartTolerance, float, 1.0f/4.5f);
  DECL_GFX_PREF(Live, "apz.use_paint_duration",                APZUsePaintDuration, bool, true);
  DECL_GFX_PREF(Live, "apz.velocity_bias",                     APZVelocityBias, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.velocity_relevance_time_ms",        APZVelocityRelevanceTime, uint32_t, 150);
  DECL_GFX_PREF(Live, "apz.x_skate_size_multiplier",           APZXSkateSizeMultiplier, float, 1.5f);
  DECL_GFX_PREF(Live, "apz.x_stationary_size_multiplier",      APZXStationarySizeMultiplier, float, 3.0f);
  DECL_GFX_PREF(Live, "apz.y_skate_size_multiplier",           APZYSkateSizeMultiplier, float, 2.5f);
  DECL_GFX_PREF(Live, "apz.y_stationary_size_multiplier",      APZYStationarySizeMultiplier, float, 3.5f);
  DECL_GFX_PREF(Live, "apz.zoom_animation_duration_ms",        APZZoomAnimationDuration, int32_t, 250);

  DECL_GFX_PREF(Live, "browser.ui.zoom.force-user-scalable",   ForceUserScalable, bool, false);
  DECL_GFX_PREF(Live, "browser.viewport.desktopWidth",         DesktopViewportWidth, int32_t, 980);

  DECL_GFX_PREF(Live, "dom.meta-viewport.enabled",             MetaViewportEnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.enabled",                        VREnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.oculus.enabled",                 VROculusEnabled, bool, true);
  DECL_GFX_PREF(Once, "dom.vr.oculus050.enabled",              VROculus050Enabled, bool, true);
  DECL_GFX_PREF(Once, "dom.vr.cardboard.enabled",              VRCardboardEnabled, bool, false);
  DECL_GFX_PREF(Once, "dom.vr.add-test-devices",               VRAddTestDevices, int32_t, 1);
  DECL_GFX_PREF(Live, "dom.w3c_pointer_events.enabled",        PointerEventsEnabled, bool, false);

  DECL_GFX_PREF(Live, "general.smoothScroll",                  SmoothScrollEnabled, bool, true);
  DECL_GFX_PREF(Live, "general.smoothScroll.durationToIntervalRatio",
                SmoothScrollDurationToIntervalRatio, int32_t, 200);
  DECL_GFX_PREF(Live, "general.smoothScroll.mouseWheel",       WheelSmoothScrollEnabled, bool, true);
  DECL_GFX_PREF(Live, "general.smoothScroll.mouseWheel.durationMaxMS",
                WheelSmoothScrollMaxDurationMs, int32_t, 400);
  DECL_GFX_PREF(Live, "general.smoothScroll.mouseWheel.durationMinMS",
                WheelSmoothScrollMinDurationMs, int32_t, 200);

  DECL_GFX_PREF(Once, "gfx.android.rgb16.force",               AndroidRGB16Force, bool, false);
#if defined(ANDROID)
  DECL_GFX_PREF(Once, "gfx.apitrace.enabled",                  UseApitrace, bool, false);
#endif
#if defined(RELEASE_BUILD)
  // "Skip" means this is locked to the default value in beta and release.
  DECL_GFX_PREF(Skip, "gfx.blocklist.all",                     BlocklistAll, int32_t, 0);
#else
  DECL_GFX_PREF(Once, "gfx.blocklist.all",                     BlocklistAll, int32_t, 0);
#endif
  DECL_GFX_PREF(Live, "gfx.canvas.auto_accelerate.min_calls",  CanvasAutoAccelerateMinCalls, int32_t, 4);
  DECL_GFX_PREF(Live, "gfx.canvas.auto_accelerate.min_frames", CanvasAutoAccelerateMinFrames, int32_t, 30);
  DECL_GFX_PREF(Live, "gfx.canvas.auto_accelerate.min_seconds", CanvasAutoAccelerateMinSeconds, float, 5.0f);
  DECL_GFX_PREF(Live, "gfx.canvas.azure.accelerated",          CanvasAzureAccelerated, bool, false);
  // 0x7fff is the maximum supported xlib surface size and is more than enough for canvases.
  DECL_GFX_PREF(Live, "gfx.canvas.max-size",                   MaxCanvasSize, int32_t, 0x7fff);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.cache-items",         CanvasSkiaGLCacheItems, int32_t, 256);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.cache-size",          CanvasSkiaGLCacheSize, int32_t, 96);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.dynamic-cache",       CanvasSkiaGLDynamicCache, bool, false);

  DECL_GFX_PREF(Live, "gfx.color_management.enablev4",         CMSEnableV4, bool, false);
  DECL_GFX_PREF(Live, "gfx.color_management.mode",             CMSMode, int32_t,-1);
  // The zero default here should match QCMS_INTENT_DEFAULT from qcms.h
  DECL_GFX_PREF(Live, "gfx.color_management.rendering_intent", CMSRenderingIntent, int32_t, 0);

  DECL_GFX_PREF(Once, "gfx.direct2d.disabled",                 Direct2DDisabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct2d.force-enabled",            Direct2DForceEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.direct2d.use1_1",                   Direct2DUse1_1, bool, false);
  DECL_GFX_PREF(Live, "gfx.direct2d.allow1_0",                 Direct2DAllow1_0, bool, false);
  DECL_GFX_PREF(Live, "gfx.draw-color-bars",                   CompositorDrawColorBars, bool, false);
  DECL_GFX_PREF(Once, "gfx.e10s.hide-plugins-for-scroll",      HidePluginsForScroll, bool, true);
  DECL_GFX_PREF(Once, "gfx.font_rendering.directwrite.force-enabled", DirectWriteFontRenderingForceEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.gralloc.fence-with-readpixels",     GrallocFenceWithReadPixels, bool, false);
  DECL_GFX_PREF(Live, "gfx.layerscope.enabled",                LayerScopeEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.layerscope.port",                   LayerScopePort, int32_t, 23456);
  // Note that        "gfx.logging.level" is defined in Logging.h
  DECL_GFX_PREF(Once, "gfx.logging.crash.length",              GfxLoggingCrashLength, uint32_t, 6);
  DECL_GFX_PREF(Live, "gfx.perf-warnings.enabled",             PerfWarnings, bool, false);
  DECL_GFX_PREF(Live, "gfx.SurfaceTexture.detach.enabled",     SurfaceTextureDetachEnabled, bool, true);
  DECL_GFX_PREF(Live, "gfx.testing.device-reset",              DeviceResetForTesting, int32_t, 0);
  DECL_GFX_PREF(Live, "gfx.testing.device-fail",               DeviceFailForTesting, bool, false);

  // These times should be in milliseconds
  DECL_GFX_PREF(Once, "gfx.touch.resample.delay-threshold",    TouchResampleVsyncDelayThreshold, int32_t, 20);
  DECL_GFX_PREF(Once, "gfx.touch.resample.max-predict",        TouchResampleMaxPredict, int32_t, 8);
  DECL_GFX_PREF(Once, "gfx.touch.resample.min-delta",          TouchResampleMinDelta, int32_t, 2);
  DECL_GFX_PREF(Once, "gfx.touch.resample.old-touch-threshold",TouchResampleOldTouchThreshold, int32_t, 17);
  DECL_GFX_PREF(Once, "gfx.touch.resample.vsync-adjust",       TouchVsyncSampleAdjust, int32_t, 5);

  DECL_GFX_PREF(Once, "gfx.vr.mirror-textures",                VRMirrorTextures, bool, false);

  DECL_GFX_PREF(Live, "gfx.vsync.collect-scroll-transforms",   CollectScrollTransforms, bool, false);
  // On b2g, in really bad cases, I've seen up to 80 ms delays between touch events and the main thread
  // processing them. So 80 ms / 16 = 5 vsync events. Double it up just to be on the safe side, so 10.
  DECL_GFX_PREF(Once, "gfx.vsync.compositor.unobserve-count",  CompositorUnobserveCount, int32_t, 10);
  // Use vsync events generated by hardware
  DECL_GFX_PREF(Once, "gfx.work-around-driver-bugs",           WorkAroundDriverBugs, bool, true);
  DECL_GFX_PREF(Once, "gfx.screen-mirroring.enabled",          ScreenMirroringEnabled, bool, false);

  DECL_GFX_PREF(Live, "gl.msaa-level",                         MSAALevel, uint32_t, 2);
  DECL_GFX_PREF(Live, "gl.require-hardware",                   RequireHardwareGL, bool, false);

  DECL_GFX_PREF(Once, "image.cache.size",                      ImageCacheSize, int32_t, 5*1024*1024);
  DECL_GFX_PREF(Once, "image.cache.timeweight",                ImageCacheTimeWeight, int32_t, 500);
  DECL_GFX_PREF(Live, "image.decode-immediately.enabled",      ImageDecodeImmediatelyEnabled, bool, false);
  DECL_GFX_PREF(Live, "image.downscale-during-decode.enabled", ImageDownscaleDuringDecodeEnabled, bool, true);
  DECL_GFX_PREF(Live, "image.infer-src-animation.threshold-ms", ImageInferSrcAnimationThresholdMS, uint32_t, 2000);
  DECL_GFX_PREF(Once, "image.mem.decode_bytes_at_a_time",      ImageMemDecodeBytesAtATime, uint32_t, 200000);
  DECL_GFX_PREF(Live, "image.mem.discardable",                 ImageMemDiscardable, bool, false);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.discard_factor", ImageMemSurfaceCacheDiscardFactor, uint32_t, 1);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.max_size_kb",    ImageMemSurfaceCacheMaxSizeKB, uint32_t, 100 * 1024);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.min_expiration_ms", ImageMemSurfaceCacheMinExpirationMS, uint32_t, 60*1000);
  DECL_GFX_PREF(Once, "image.mem.surfacecache.size_factor",    ImageMemSurfaceCacheSizeFactor, uint32_t, 64);
  DECL_GFX_PREF(Live, "image.mozsamplesize.enabled",           ImageMozSampleSizeEnabled, bool, false);
  DECL_GFX_PREF(Once, "image.multithreaded_decoding.limit",    ImageMTDecodingLimit, int32_t, -1);
  DECL_GFX_PREF(Live, "image.single-color-optimization.enabled", ImageSingleColorOptimizationEnabled, bool, true);

  DECL_GFX_PREF(Once, "layers.acceleration.disabled",          LayersAccelerationDisabled, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps",          LayersDrawFPS, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps.print-histogram",  FPSPrintHistogram, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps.write-to-file", WriteFPSToFile, bool, false);
  DECL_GFX_PREF(Once, "layers.acceleration.force-enabled",     LayersAccelerationForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.async-pan-zoom.enabled",         AsyncPanZoomEnabledDoNotUseDirectly, bool, true);
  DECL_GFX_PREF(Once, "layers.async-pan-zoom.separate-event-thread", AsyncPanZoomSeparateEventThread, bool, false);
  DECL_GFX_PREF(Live, "layers.bench.enabled",                  LayersBenchEnabled, bool, false);
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
  DECL_GFX_PREF(Live, "layers.composer2d.enabled",             Composer2DCompositionEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.d3d11.disable-warp",             LayersD3D11DisableWARP, bool, false);
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
  DECL_GFX_PREF(Live, "layers.frame-counter",                  DrawFrameCounter, bool, false);
  DECL_GFX_PREF(Once, "layers.gralloc.disable",                DisableGralloc, bool, false);
  DECL_GFX_PREF(Live, "layers.low-precision-buffer",           UseLowPrecisionBuffer, bool, false);
  DECL_GFX_PREF(Live, "layers.low-precision-opacity",          LowPrecisionOpacity, float, 1.0f);
  DECL_GFX_PREF(Live, "layers.low-precision-resolution",       LowPrecisionResolution, float, 0.25f);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.enabled", LayersOffMainThreadCompositionEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.force-enabled", LayersOffMainThreadCompositionForceEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.offmainthreadcomposition.frame-rate", LayersCompositionFrameRate, int32_t,-1);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.testing.enabled", LayersOffMainThreadCompositionTestingEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.orientation.sync.timeout",       OrientationSyncMillis, uint32_t, (uint32_t)0);
  DECL_GFX_PREF(Once, "layers.overzealous-gralloc-unlocking",  OverzealousGrallocUnlocking, bool, false);
  DECL_GFX_PREF(Once, "layers.prefer-d3d9",                    LayersPreferD3D9, bool, false);
  DECL_GFX_PREF(Once, "layers.prefer-opengl",                  LayersPreferOpenGL, bool, false);
  DECL_GFX_PREF(Live, "layers.progressive-paint",              ProgressivePaintDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Once, "layers.stereo-video.enabled",           StereoVideoEnabled, bool, false);

  // We allow for configurable and rectangular tile size to avoid wasting memory on devices whose
  // screen size does not align nicely to the default tile size. Although layers can be any size,
  // they are often the same size as the screen, especially for width.
  DECL_GFX_PREF(Once, "layers.tile-width",                     LayersTileWidth, int32_t, 256);
  DECL_GFX_PREF(Once, "layers.tile-height",                    LayersTileHeight, int32_t, 256);
  DECL_GFX_PREF(Once, "layers.tile-max-pool-size",             LayersTileMaxPoolSize, uint32_t, (uint32_t)50);
  DECL_GFX_PREF(Once, "layers.tile-shrink-pool-timeout",       LayersTileShrinkPoolTimeout, uint32_t, (uint32_t)1000);
  DECL_GFX_PREF(Once, "layers.tiled-drawtarget.enabled",       TiledDrawTargetEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.tiles.adjust",                   LayersTilesAdjust, bool, true);
  DECL_GFX_PREF(Once, "layers.tiles.edge-padding",             TileEdgePaddingEnabled, bool, true);
  DECL_GFX_PREF(Live, "layers.transaction.warning-ms",         LayerTransactionWarning, uint32_t, 200);
  DECL_GFX_PREF(Once, "layers.uniformity-info",                UniformityInfo, bool, false);
  DECL_GFX_PREF(Once, "layers.use-image-offscreen-surfaces",   UseImageOffscreenSurfaces, bool, false);
  DECL_GFX_PREF(Live, "layers.single-tile.enabled",            LayersSingleTileEnabled, bool, true);

  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.damping-ratio", ScrollBehaviorDampingRatio, float, 1.0f);
  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.enabled",    ScrollBehaviorEnabled, bool, false);
  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.spring-constant", ScrollBehaviorSpringConstant, float, 250.0f);
  DECL_GFX_PREF(Live, "layout.css.scroll-snap.prediction-max-velocity", ScrollSnapPredictionMaxVelocity, int32_t, 2000);
  DECL_GFX_PREF(Live, "layout.css.scroll-snap.prediction-sensitivity", ScrollSnapPredictionSensitivity, float, 0.750f);
  DECL_GFX_PREF(Once, "layout.css.touch_action.enabled",       TouchActionEnabled, bool, false);
  DECL_GFX_PREF(Live, "layout.display-list.dump",              LayoutDumpDisplayList, bool, false);
  DECL_GFX_PREF(Live, "layout.event-regions.enabled",          LayoutEventRegionsEnabledDoNotUseDirectly, bool, false);
  DECL_GFX_PREF(Once, "layout.frame_rate",                     LayoutFrameRate, int32_t, -1);
  DECL_GFX_PREF(Once, "layout.paint_rects_separately",         LayoutPaintRectsSeparately, bool, true);

  // This and code dependent on it should be removed once containerless scrolling looks stable.
  DECL_GFX_PREF(Once, "layout.scroll.root-frame-containers",   LayoutUseContainersForRootFrames, bool, true);

  // This affects whether events will be routed through APZ or not.
  DECL_GFX_PREF(Live, "mousewheel.system_scroll_override_on_root_content.enabled",
                                                               MouseWheelHasRootScrollDeltaOverride, bool, false);
  DECL_GFX_PREF(Live, "mousewheel.system_scroll_override_on_root_content.horizontal.factor",
                                                               MouseWheelRootHScrollDeltaFactor, int32_t, 100);
  DECL_GFX_PREF(Live, "mousewheel.system_scroll_override_on_root_content.vertical.factor",
                                                               MouseWheelRootVScrollDeltaFactor, int32_t, 100);
  DECL_GFX_PREF(Live, "mousewheel.transaction.ignoremovedelay",MouseWheelIgnoreMoveDelayMs, int32_t, (int32_t)100);
  DECL_GFX_PREF(Live, "mousewheel.transaction.timeout",        MouseWheelTransactionTimeoutMs, int32_t, (int32_t)1500);

  DECL_GFX_PREF(Live, "nglayout.debug.widget_update_flashing", WidgetUpdateFlashing, bool, false);

  DECL_GFX_PREF(Live, "test.events.async.enabled",             TestEventsAsyncEnabled, bool, false);
  DECL_GFX_PREF(Live, "test.mousescroll",                      MouseScrollTestingEnabled, bool, false);

  DECL_GFX_PREF(Live, "ui.click_hold_context_menus.delay",     UiClickHoldContextMenusDelay, int32_t, 500);

  // WebGL (for pref access from Worker threads)
  DECL_GFX_PREF(Live, "webgl.all-angle-options",               WebGLAllANGLEOptions, bool, false);
  DECL_GFX_PREF(Live, "webgl.angle.force-d3d11",               WebGLANGLEForceD3D11, bool, false);
  DECL_GFX_PREF(Live, "webgl.angle.try-d3d11",                 WebGLANGLETryD3D11, bool, false);
  DECL_GFX_PREF(Once, "webgl.angle.force-warp",                WebGLANGLEForceWARP, bool, false);
  DECL_GFX_PREF(Live, "webgl.bypass-shader-validation",        WebGLBypassShaderValidator, bool, true);
  DECL_GFX_PREF(Live, "webgl.can-lose-context-in-foreground",  WebGLCanLoseContextInForeground, bool, true);
  DECL_GFX_PREF(Live, "webgl.default-no-alpha",                WebGLDefaultNoAlpha, bool, false);
  DECL_GFX_PREF(Live, "webgl.disable-angle",                   WebGLDisableANGLE, bool, false);
  DECL_GFX_PREF(Live, "webgl.disable-extensions",              WebGLDisableExtensions, bool, false);

  DECL_GFX_PREF(Live, "webgl.disable-fail-if-major-performance-caveat",
                WebGLDisableFailIfMajorPerformanceCaveat, bool, false);
  DECL_GFX_PREF(Live, "webgl.disabled",                        WebGLDisabled, bool, false);

  DECL_GFX_PREF(Live, "webgl.enable-draft-extensions",         WebGLDraftExtensionsEnabled, bool, false);
  DECL_GFX_PREF(Live, "webgl.enable-privileged-extensions",    WebGLPrivilegedExtensionsEnabled, bool, false);
  DECL_GFX_PREF(Live, "webgl.force-enabled",                   WebGLForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "webgl.force-layers-readback",           WebGLForceLayersReadback, bool, false);
  DECL_GFX_PREF(Live, "webgl.lose-context-on-memory-pressure", WebGLLoseContextOnMemoryPressure, bool, false);
  DECL_GFX_PREF(Live, "webgl.max-warnings-per-context",        WebGLMaxWarningsPerContext, uint32_t, 32);
  DECL_GFX_PREF(Live, "webgl.min_capability_mode",             WebGLMinCapabilityMode, bool, false);
  DECL_GFX_PREF(Live, "webgl.msaa-force",                      WebGLForceMSAA, bool, false);
  DECL_GFX_PREF(Live, "webgl.prefer-16bpp",                    WebGLPrefer16bpp, bool, false);
  DECL_GFX_PREF(Live, "webgl.restore-context-when-visible",    WebGLRestoreWhenVisible, bool, true);

  // WARNING:
  // Please make sure that you've added your new preference to the list above in alphabetical order.
  // Please do not just append it to the end of the list.

public:
  // Manage the singleton:
  static gfxPrefs& GetSingleton()
  {
    MOZ_ASSERT(!sInstanceHasBeenDestroyed, "Should never recreate a gfxPrefs instance!");
    if (!sInstance) {
      sInstance = new gfxPrefs;
    }
    MOZ_ASSERT(SingletonExists());
    return *sInstance;
  }
  static void DestroySingleton();
  static bool SingletonExists();

private:
  static gfxPrefs* sInstance;
  static bool sInstanceHasBeenDestroyed;

private:
  // Creating these to avoid having to include Preferences.h in the .h
  static void PrefAddVarCache(bool*, const char*, bool);
  static void PrefAddVarCache(int32_t*, const char*, int32_t);
  static void PrefAddVarCache(uint32_t*, const char*, uint32_t);
  static void PrefAddVarCache(float*, const char*, float);
  static bool PrefGet(const char*, bool);
  static int32_t PrefGet(const char*, int32_t);
  static uint32_t PrefGet(const char*, uint32_t);
  static float PrefGet(const char*, float);
  static void PrefSet(const char* aPref, bool aValue);
  static void PrefSet(const char* aPref, int32_t aValue);
  static void PrefSet(const char* aPref, uint32_t aValue);
  static void PrefSet(const char* aPref, float aValue);

  static void AssertMainThread();

  gfxPrefs();
  ~gfxPrefs();
  gfxPrefs(const gfxPrefs&) = delete;
  gfxPrefs& operator=(const gfxPrefs&) = delete;
};

#undef DECL_GFX_PREF /* Don't need it outside of this file */

#endif /* GFX_PREFS_H */
