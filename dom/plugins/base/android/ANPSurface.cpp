/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  void* handle = dlopen("/system/lib/libsurfaceflinger_client.so", RTLD_LAZY);

  if (!handle) {
    LOG("Failed to open libsurfaceflinger_client.so");
    return false;
  }

  gSurfaceFunctions.lock = (int (*)(void*, SurfaceInfo*, void*))dlsym(handle, "_ZN7android7Surface4lockEPNS0_11SurfaceInfoEPNS_6RegionEb");
  gSurfaceFunctions.unlockAndPost = (int (*)(void*))dlsym(handle, "_ZN7android7Surface13unlockAndPostEv");

  handle = dlopen("/system/lib/libui.so", RTLD_LAZY);
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
