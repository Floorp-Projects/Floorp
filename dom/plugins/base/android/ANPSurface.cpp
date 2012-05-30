/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#include <android/log.h>
#include "ANPBase.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_surface_##name

#define CLEAR_EXCEPTION(env) if (env->ExceptionOccurred()) env->ExceptionClear();

#define ANDROID_REGION_SIZE 512

enum {
    PIXEL_FORMAT_RGBA_8888   = 1,
    PIXEL_FORMAT_RGB_565     = 4,
};

struct SurfaceInfo {
    uint32_t    w;
    uint32_t    h;
    uint32_t    s;
    uint32_t    usage;
    uint32_t    format;
    unsigned char* bits;
    uint32_t    reserved[2];
};

typedef struct ARect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} ARect;


// used to cache JNI method and field IDs for Surface Objects
static struct ANPSurfaceInterfaceJavaGlue {
    bool        initialized;
    jmethodID   getSurfaceHolder;
    jmethodID   getSurface;
    jfieldID    surfacePointer;
} gSurfaceJavaGlue;

static struct ANPSurfaceFunctions {
    bool initialized;

    int (* lock)(void*, SurfaceInfo*, void*);
    int (* unlockAndPost)(void*);

    void* (* regionConstructor)(void*);
    void (* setRegion)(void*, ARect const&);
} gSurfaceFunctions;


static inline void* getSurface(JNIEnv* env, jobject view) {
  if (!env || !view) {
    return NULL;
  }

  if (!gSurfaceJavaGlue.initialized) {

    jclass surfaceViewClass = env->FindClass("android/view/SurfaceView");
    gSurfaceJavaGlue.getSurfaceHolder = env->GetMethodID(surfaceViewClass, "getHolder", "()Landroid/view/SurfaceHolder;");

    jclass surfaceHolderClass = env->FindClass("android/view/SurfaceHolder");
    gSurfaceJavaGlue.getSurface = env->GetMethodID(surfaceHolderClass, "getSurface", "()Landroid/view/Surface;");

    jclass surfaceClass = env->FindClass("android/view/Surface");
    gSurfaceJavaGlue.surfacePointer = env->GetFieldID(surfaceClass,
        "mSurfacePointer", "I");

    if (!gSurfaceJavaGlue.surfacePointer) {
      CLEAR_EXCEPTION(env);

      // It was something else in 2.2.
      gSurfaceJavaGlue.surfacePointer = env->GetFieldID(surfaceClass,
          "mSurface", "I");

      if (!gSurfaceJavaGlue.surfacePointer) {
        CLEAR_EXCEPTION(env);

        // And something else in 2.3+
        gSurfaceJavaGlue.surfacePointer = env->GetFieldID(surfaceClass,
            "mNativeSurface", "I");
        
        CLEAR_EXCEPTION(env);
      }
    }

    if (!gSurfaceJavaGlue.surfacePointer) {
      LOG("Failed to acquire surface pointer");
      return NULL;
    }

    env->DeleteLocalRef(surfaceClass);
    env->DeleteLocalRef(surfaceViewClass);
    env->DeleteLocalRef(surfaceHolderClass);

    gSurfaceJavaGlue.initialized = (gSurfaceJavaGlue.surfacePointer != NULL);
  }

  jobject holder = env->CallObjectMethod(view, gSurfaceJavaGlue.getSurfaceHolder);
  jobject surface = env->CallObjectMethod(holder, gSurfaceJavaGlue.getSurface);
  jint surfacePointer = env->GetIntField(surface, gSurfaceJavaGlue.surfacePointer);

  env->DeleteLocalRef(holder);
  env->DeleteLocalRef(surface);

  return (void*)surfacePointer;
}

static ANPBitmapFormat convertPixelFormat(int32_t format) {
  switch (format) {
    case PIXEL_FORMAT_RGBA_8888:  return kRGBA_8888_ANPBitmapFormat;
    case PIXEL_FORMAT_RGB_565:    return kRGB_565_ANPBitmapFormat;
    default:            return kUnknown_ANPBitmapFormat;
  }
}

static int bytesPerPixel(int32_t format) {
  switch (format) {
    case PIXEL_FORMAT_RGBA_8888: return 4;
    case PIXEL_FORMAT_RGB_565: return 2;
    default: return -1;
  }
}

static bool init() {
  if (gSurfaceFunctions.initialized)
    return true;

  void* handle = dlopen("libsurfaceflinger_client.so", RTLD_LAZY);

  if (!handle) {
    LOG("Failed to open libsurfaceflinger_client.so");
    return false;
  }

  gSurfaceFunctions.lock = (int (*)(void*, SurfaceInfo*, void*))dlsym(handle, "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionEb");
  gSurfaceFunctions.unlockAndPost = (int (*)(void*))dlsym(handle, "_ZN7android7Surface13unlockAndPostEv");

  handle = dlopen("libui.so", RTLD_LAZY);
  if (!handle) {
    LOG("Failed to open libui.so");
    return false;
  }

  gSurfaceFunctions.regionConstructor = (void* (*)(void*))dlsym(handle, "_ZN7android6RegionC1Ev");
  gSurfaceFunctions.setRegion = (void (*)(void*, ARect const&))dlsym(handle, "_ZN7android6Region3setERKNS_4RectE");

  gSurfaceFunctions.initialized = (gSurfaceFunctions.lock && gSurfaceFunctions.unlockAndPost &&
                                   gSurfaceFunctions.regionConstructor && gSurfaceFunctions.setRegion);
  LOG("Initialized? %d\n", gSurfaceFunctions.initialized);
  return gSurfaceFunctions.initialized;
}

static bool anp_surface_lock(JNIEnv* env, jobject surfaceView, ANPBitmap* bitmap, ANPRectI* dirtyRect) {
  if (!bitmap || !surfaceView) {
    return false;
  }

  void* surface = getSurface(env, surfaceView);

  if (!bitmap || !surface) {
    return false;
  }

  if (!init()) {
    return false;
  }

  void* region = NULL;
  if (dirtyRect) {
    region = malloc(ANDROID_REGION_SIZE);
    gSurfaceFunctions.regionConstructor(region);

    ARect rect;
    rect.left = dirtyRect->left;
    rect.top = dirtyRect->top;
    rect.right = dirtyRect->right;
    rect.bottom = dirtyRect->bottom;

    gSurfaceFunctions.setRegion(region, rect);
  }

  SurfaceInfo info;
  int err = gSurfaceFunctions.lock(surface, &info, region);
  if (err < 0) {
    LOG("Failed to lock surface");
    return false;
  }

  // the surface may have expanded the dirty region so we must to pass that
  // information back to the plugin.
  if (dirtyRect) {
    ARect* dirtyBounds = (ARect*)region; // The bounds are the first member, so this should work!

    dirtyRect->left = dirtyBounds->left;
    dirtyRect->right = dirtyBounds->right;
    dirtyRect->top = dirtyBounds->top;
    dirtyRect->bottom = dirtyBounds->bottom;
  }

  if (region)
    free(region);

  int bpr = info.s * bytesPerPixel(info.format);

  bitmap->format = convertPixelFormat(info.format);
  bitmap->width = info.w;
  bitmap->height = info.h;
  bitmap->rowBytes = bpr;

  if (info.w > 0 && info.h > 0) {
    bitmap->baseAddr = info.bits;
  } else {
    bitmap->baseAddr = NULL;
    return false;
  }

  return true;
}

static void anp_surface_unlock(JNIEnv* env, jobject surfaceView) {
  if (!surfaceView) {
    return;
  }

  if (!init()) {
    return;
  }

  void* surface = getSurface(env, surfaceView);

  if (!surface) {
    return;
  }

  gSurfaceFunctions.unlockAndPost(surface);
}

///////////////////////////////////////////////////////////////////////////////

void InitSurfaceInterface(ANPSurfaceInterfaceV0* i) {
  ASSIGN(i, lock);
  ASSIGN(i, unlock);

  // setup the java glue struct
  gSurfaceJavaGlue.initialized = false;

  // setup the function struct
  gSurfaceFunctions.initialized = false;
}
