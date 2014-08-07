/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PREFS_H
#define GFX_PREFS_H

#include <stdint.h>
#include "mozilla/Assertions.h"
#include "mozilla/TypedEnum.h"

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

class gfxPrefs;
class gfxPrefs MOZ_FINAL
{
private:
  // Enums for the update policy.
  MOZ_BEGIN_NESTED_ENUM_CLASS(UpdatePolicy)
    Skip, // Set the value to default, skip any Preferences calls
    Once, // Evaluate the preference once, unchanged during the session
    Live  // Evaluate the preference and set callback so it stays current/live
  MOZ_END_NESTED_ENUM_CLASS(UpdatePolicy)

  // Since we cannot use const char*, use a function that returns it.
  template <MOZ_ENUM_CLASS_ENUM_TYPE(UpdatePolicy) Update, class T, T Default(void), const char* Pref(void)>
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

  // The apz prefs are explained in AsyncPanZoomController.cpp
  DECL_GFX_PREF(Live, "apz.allow_checkerboarding",             APZAllowCheckerboarding, bool, true);
  DECL_GFX_PREF(Live, "apz.asyncscroll.throttle",              APZAsyncScrollThrottleTime, int32_t, 100);
  DECL_GFX_PREF(Live, "apz.asyncscroll.timeout",               APZAsyncScrollTimeout, int32_t, 300);
  DECL_GFX_PREF(Live, "apz.axis_lock_mode",                    APZAxisLockMode, int32_t, 0);
  DECL_GFX_PREF(Live, "apz.content_response_timeout",          APZContentResponseTimeout, int32_t, 300);
  DECL_GFX_PREF(Live, "apz.cross_slide.enabled",               APZCrossSlideEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.danger_zone_x",                     APZDangerZoneX, int32_t, 50);
  DECL_GFX_PREF(Live, "apz.danger_zone_y",                     APZDangerZoneY, int32_t, 100);
  DECL_GFX_PREF(Live, "apz.enlarge_displayport_when_clipped",  APZEnlargeDisplayPortWhenClipped, bool, false);
  DECL_GFX_PREF(Live, "apz.fling_accel_interval_ms",           APZFlingAccelInterval, int32_t, 500);
  DECL_GFX_PREF(Live, "apz.fling_accel_base_mult",             APZFlingAccelBaseMultiplier, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.fling_accel_supplemental_mult",     APZFlingAccelSupplementalMultiplier, float, 1.0f);
  DECL_GFX_PREF(Once, "apz.fling_friction",                    APZFlingFriction, float, 0.002f);
  DECL_GFX_PREF(Live, "apz.fling_repaint_interval",            APZFlingRepaintInterval, int32_t, 75);
  DECL_GFX_PREF(Once, "apz.fling_stop_on_tap_threshold",       APZFlingStopOnTapThreshold, float, 0.05f);
  DECL_GFX_PREF(Once, "apz.fling_stopped_threshold",           APZFlingStoppedThreshold, float, 0.01f);
  DECL_GFX_PREF(Once, "apz.max_velocity_inches_per_ms",        APZMaxVelocity, float, -1.0f);
  DECL_GFX_PREF(Once, "apz.max_velocity_queue_size",           APZMaxVelocityQueueSize, uint32_t, 5);
  DECL_GFX_PREF(Live, "apz.min_skate_speed",                   APZMinSkateSpeed, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.num_paint_duration_samples",        APZNumPaintDurationSamples, int32_t, 3);
  DECL_GFX_PREF(Live, "apz.overscroll.enabled",                APZOverscrollEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.overscroll.fling_friction",         APZOverscrollFlingFriction, float, 0.02f);
  DECL_GFX_PREF(Live, "apz.overscroll.fling_stopped_threshold", APZOverscrollFlingStoppedThreshold, float, 0.4f);
  DECL_GFX_PREF(Live, "apz.overscroll.clamping",               APZOverscrollClamping, float, 0.5f);
  DECL_GFX_PREF(Live, "apz.overscroll.z_effect",               APZOverscrollZEffect, float, 0.2f);
  DECL_GFX_PREF(Live, "apz.overscroll.snap_back.spring_stiffness", APZOverscrollSnapBackSpringStiffness, float, 0.6f);
  DECL_GFX_PREF(Live, "apz.overscroll.snap_back.spring_friction", APZOverscrollSnapBackSpringFriction, float, 0.1f);
  DECL_GFX_PREF(Live, "apz.overscroll.snap_back.mass",         APZOverscrollSnapBackMass, float, 1000.0f);
  DECL_GFX_PREF(Live, "apz.pan_repaint_interval",              APZPanRepaintInterval, int32_t, 250);
  DECL_GFX_PREF(Live, "apz.printtree",                         APZPrintTree, bool, false);
  DECL_GFX_PREF(Live, "apz.subframe.enabled",                  APZSubframeEnabled, bool, false);
  DECL_GFX_PREF(Once, "apz.test.logging_enabled",              APZTestLoggingEnabled, bool, false);
  DECL_GFX_PREF(Live, "apz.touch_start_tolerance",             APZTouchStartTolerance, float, 1.0f/4.5f);
  DECL_GFX_PREF(Live, "apz.use_paint_duration",                APZUsePaintDuration, bool, true);
  DECL_GFX_PREF(Live, "apz.velocity_bias",                     APZVelocityBias, float, 1.0f);
  DECL_GFX_PREF(Live, "apz.velocity_relevance_time_ms",        APZVelocityRelevanceTime, uint32_t, 150);
  DECL_GFX_PREF(Live, "apz.x_skate_size_multiplier",           APZXSkateSizeMultiplier, float, 1.5f);
  DECL_GFX_PREF(Live, "apz.x_stationary_size_multiplier",      APZXStationarySizeMultiplier, float, 3.0f);
  DECL_GFX_PREF(Live, "apz.y_skate_size_multiplier",           APZYSkateSizeMultiplier, float, 2.5f);
  DECL_GFX_PREF(Live, "apz.y_stationary_size_multiplier",      APZYStationarySizeMultiplier, float, 3.5f);
  DECL_GFX_PREF(Live, "apz.zoom_animation_duration_ms",        APZZoomAnimationDuration, int32_t, 250);

  DECL_GFX_PREF(Once, "gfx.android.rgb16.force",               AndroidRGB16Force, bool, false);
#if defined(ANDROID)
  DECL_GFX_PREF(Once, "gfx.apitrace.enabled",                  UseApitrace, bool, false);
#endif
  DECL_GFX_PREF(Live, "gfx.canvas.azure.accelerated",          CanvasAzureAccelerated, bool, false);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.dynamic-cache",       CanvasSkiaGLDynamicCache, bool, false);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.cache-size",          CanvasSkiaGLCacheSize, int32_t, 96);
  DECL_GFX_PREF(Once, "gfx.canvas.skiagl.cache-items",         CanvasSkiaGLCacheItems, int32_t, 256);

  DECL_GFX_PREF(Live, "gfx.color_management.enablev4",         CMSEnableV4, bool, false);
  DECL_GFX_PREF(Live, "gfx.color_management.mode",             CMSMode, int32_t,-1);
  // The zero default here should match QCMS_INTENT_DEFAULT from qcms.h
  DECL_GFX_PREF(Live, "gfx.color_management.rendering_intent", CMSRenderingIntent, int32_t, 0);

  DECL_GFX_PREF(Once, "gfx.direct2d.disabled",                 Direct2DDisabled, bool, false);
  DECL_GFX_PREF(Once, "gfx.direct2d.force-enabled",            Direct2DForceEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.gralloc.fence-with-readpixels",     GrallocFenceWithReadPixels, bool, false);
  DECL_GFX_PREF(Live, "gfx.layerscope.enabled",                LayerScopeEnabled, bool, false);
  DECL_GFX_PREF(Live, "gfx.layerscope.port",                   LayerScopePort, int32_t, 23456);
  DECL_GFX_PREF(Live, "gfx.perf-warnings.enabled",             PerfWarnings, bool, false);
  DECL_GFX_PREF(Once, "gfx.work-around-driver-bugs",           WorkAroundDriverBugs, bool, true);

  DECL_GFX_PREF(Live, "gfx.draw-color-bars",                   CompositorDrawColorBars, bool, false);

  DECL_GFX_PREF(Live, "gl.msaa-level",                         MSAALevel, uint32_t, 2);

  DECL_GFX_PREF(Once, "layers.acceleration.disabled",          LayersAccelerationDisabled, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps",          LayersDrawFPS, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps.print-histogram",  FPSPrintHistogram, bool, false);
  DECL_GFX_PREF(Live, "layers.acceleration.draw-fps.write-to-file", WriteFPSToFile, bool, false);
  DECL_GFX_PREF(Once, "layers.acceleration.force-enabled",     LayersAccelerationForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.async-video.enabled",            AsyncVideoEnabled, bool, true);
  DECL_GFX_PREF(Once, "layers.async-video-oop.enabled",        AsyncVideoOOPEnabled, bool, true);
  DECL_GFX_PREF(Once, "layers.async-pan-zoom.enabled",         AsyncPanZoomEnabled, bool, false);
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
  DECL_GFX_PREF(Live, "layers.draw-bigimage-borders",          DrawBigImageBorders, bool, false);
  DECL_GFX_PREF(Live, "layers.draw-borders",                   DrawLayerBorders, bool, false);
  DECL_GFX_PREF(Live, "layers.draw-tile-borders",              DrawTileBorders, bool, false);
  DECL_GFX_PREF(Live, "layers.flash-borders",                  FlashLayerBorders, bool, false);
  DECL_GFX_PREF(Live, "layers.draw-layer-info",                DrawLayerInfo, bool, false);
  DECL_GFX_PREF(Live, "layers.dump",                           LayersDump, bool, false);
  DECL_GFX_PREF(Once, "layers.enable-tiles",                   LayersTilesEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.simple-tiles",                   LayersUseSimpleTiles, bool, false);
  DECL_GFX_PREF(Once, "layers.force-per-tile-drawing",         PerTileDrawing, bool, false);
  DECL_GFX_PREF(Once, "layers.tiled-drawtarget.enabled",       TiledDrawTargetEnabled, bool, false);
  // We allow for configurable and rectangular tile size to avoid wasting memory on devices whose
  // screen size does not align nicely to the default tile size. Although layers can be any size,
  // they are often the same size as the screen, especially for width.
  DECL_GFX_PREF(Once, "layers.tile-width",                     LayersTileWidth, int32_t, 256);
  DECL_GFX_PREF(Once, "layers.tile-height",                    LayersTileHeight, int32_t, 256);
  DECL_GFX_PREF(Once, "layers.tile-max-pool-size",             LayersTileMaxPoolSize, uint32_t, (uint32_t)50);
  DECL_GFX_PREF(Once, "layers.tile-shrink-pool-timeout",       LayersTileShrinkPoolTimeout, uint32_t, (uint32_t)1000);
  DECL_GFX_PREF(Once, "layers.overzealous-gralloc-unlocking",  OverzealousGrallocUnlocking, bool, false);
  DECL_GFX_PREF(Once, "layers.force-shmem-tiles",              ForceShmemTiles, bool, false);
  DECL_GFX_PREF(Live, "layers.frame-counter",                  DrawFrameCounter, bool, false);
  DECL_GFX_PREF(Live, "layers.low-precision-buffer",           UseLowPrecisionBuffer, bool, false);
  DECL_GFX_PREF(Live, "layers.low-precision-resolution",       LowPrecisionResolution, float, 0.25f);
  DECL_GFX_PREF(Live, "layers.low-precision-opacity",          LowPrecisionOpacity, float, 1.0f);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.enabled", LayersOffMainThreadCompositionEnabled, bool, false);
  DECL_GFX_PREF(Live, "layers.offmainthreadcomposition.frame-rate", LayersCompositionFrameRate, int32_t,-1);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.force-enabled", LayersOffMainThreadCompositionForceEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.offmainthreadcomposition.testing.enabled", LayersOffMainThreadCompositionTestingEnabled, bool, false);
  DECL_GFX_PREF(Once, "layers.use-image-offscreen-surfaces",   UseImageOffscreenSurfaces, bool, false);
  DECL_GFX_PREF(Live, "layers.orientation.sync.timeout",       OrientationSyncMillis, uint32_t, (uint32_t)0);
  DECL_GFX_PREF(Once, "layers.prefer-d3d9",                    LayersPreferD3D9, bool, false);
  DECL_GFX_PREF(Once, "layers.prefer-opengl",                  LayersPreferOpenGL, bool, false);
  DECL_GFX_PREF(Once, "layers.progressive-paint",              UseProgressiveTilePainting, bool, false);
  DECL_GFX_PREF(Once, "layers.scroll-graph",                   LayersScrollGraph, bool, false);
  DECL_GFX_PREF(Once, "layers.uniformity-info",                UniformityInfo, bool, false);
  DECL_GFX_PREF(Live, "layers.invert",                         Invert, bool, false);
  DECL_GFX_PREF(Live, "layers.grayscale",                      Grayscale, bool, false);

  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.damping-ratio", ScrollBehaviorDampingRatio, float, 1.0f);
  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.enabled",    ScrollBehaviorEnabled, bool, false);
  DECL_GFX_PREF(Live, "layout.css.scroll-behavior.spring-constant", ScrollBehaviorSpringConstant, float, 250.0f);
  DECL_GFX_PREF(Once, "layout.css.touch_action.enabled",       TouchActionEnabled, bool, false);
  DECL_GFX_PREF(Once, "layout.frame_rate",                     LayoutFrameRate, int32_t, -1);
  DECL_GFX_PREF(Live, "layout.display-list.dump",              LayoutDumpDisplayList, bool, false);
  DECL_GFX_PREF(Once, "layout.paint_rects_separately",         LayoutPaintRectsSeparately, bool, true);

  DECL_GFX_PREF(Live, "nglayout.debug.widget_update_flashing", WidgetUpdateFlashing, bool, false);

  DECL_GFX_PREF(Live, "ui.click_hold_context_menus.delay",     UiClickHoldContextMenusDelay, int32_t, 500);

  DECL_GFX_PREF(Once, "webgl.force-layers-readback",           WebGLForceLayersReadback, bool, false);

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
  gfxPrefs(const gfxPrefs&) MOZ_DELETE;
  gfxPrefs& operator=(const gfxPrefs&) MOZ_DELETE;
};

#undef DECL_GFX_PREF /* Don't need it outside of this file */

#endif /* GFX_PREFS_H */
