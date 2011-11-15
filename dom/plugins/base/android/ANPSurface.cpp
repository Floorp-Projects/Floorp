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

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>
#include "AndroidBridge.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_surface_##name


// used to cache JNI method and field IDs for Surface Objects
static struct ANPSurfaceInterfaceJavaGlue {
  bool        initialized;
  jclass geckoAppShellClass;
  jclass lockInfoCls;
  jmethodID lockSurfaceANP;
  jmethodID jUnlockSurfaceANP;
  jfieldID jDirtyTop;
  jfieldID jDirtyLeft;
  jfieldID jDirtyBottom;
  jfieldID jDirtyRight;
  jfieldID jFormat;
  jfieldID jWidth ;
  jfieldID jHeight;
  jfieldID jBuffer;
} gSurfaceJavaGlue;

#define getClassGlobalRef(env, cname)                                    \
     (jClass = jclass(env->NewGlobalRef(env->FindClass(cname))))

static void init(JNIEnv* env) {
  if (gSurfaceJavaGlue.initialized)
    return;
  
  gSurfaceJavaGlue.geckoAppShellClass = mozilla::AndroidBridge::GetGeckoAppShellClass();
  
  jmethodID getClass = env->GetStaticMethodID(gSurfaceJavaGlue.geckoAppShellClass, 
                                              "getSurfaceLockInfoClass",
                                              "()Ljava/lang/Class;");

  gSurfaceJavaGlue.lockInfoCls = (jclass) env->NewGlobalRef(env->CallStaticObjectMethod(gSurfaceJavaGlue.geckoAppShellClass, getClass));

  gSurfaceJavaGlue.jDirtyTop = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "dirtyTop", "I");
  gSurfaceJavaGlue.jDirtyLeft = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "dirtyLeft", "I");
  gSurfaceJavaGlue.jDirtyBottom = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "dirtyBottom", "I");
  gSurfaceJavaGlue.jDirtyRight = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "dirtyRight", "I");

  gSurfaceJavaGlue.jFormat = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "format", "I");
  gSurfaceJavaGlue.jWidth = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "width", "I");
  gSurfaceJavaGlue.jHeight = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "height", "I");

  gSurfaceJavaGlue.jBuffer = env->GetFieldID(gSurfaceJavaGlue.lockInfoCls, "buffer", "Ljava/nio/Buffer;");
  gSurfaceJavaGlue.lockSurfaceANP = env->GetStaticMethodID(gSurfaceJavaGlue.geckoAppShellClass, "lockSurfaceANP", "(Landroid/view/SurfaceView;IIII)Lorg/mozilla/gecko/SurfaceLockInfo;");
  gSurfaceJavaGlue.jUnlockSurfaceANP = env->GetStaticMethodID(gSurfaceJavaGlue.geckoAppShellClass, "unlockSurfaceANP", "(Landroid/view/SurfaceView;)V");
  gSurfaceJavaGlue.initialized = true;
}

static bool anp_lock(JNIEnv* env, jobject surfaceView, ANPBitmap* bitmap, ANPRectI* dirtyRect) {
  LOG("%s", __PRETTY_FUNCTION__);
  if (!bitmap || !surfaceView) {
    LOG("%s, null bitmap or surface, exiting", __PRETTY_FUNCTION__);
    return false;
  }

  init(env);

  jvalue args[5];
  args[0].l = surfaceView;
  if (dirtyRect) {
    args[1].i = dirtyRect->top;
    args[2].i = dirtyRect->left;
    args[3].i = dirtyRect->bottom;
    args[4].i = dirtyRect->right;
    LOG("dirty rect: %d, %d, %d, %d", dirtyRect->top, dirtyRect->left, dirtyRect->bottom, dirtyRect->right);
  } else {
    args[1].i = args[2].i = args[3].i = args[4].i = 0;
  }
  
  jobject info = env->CallStaticObjectMethod(gSurfaceJavaGlue.geckoAppShellClass,
                                             gSurfaceJavaGlue.lockSurfaceANP, 
                                             surfaceView, args[1].i, args[2].i, args[3].i, args[4].i);

  LOG("info: %p", info);
  if (!info)
    return false;

  // the surface may have expanded the dirty region so we must to pass that
  // information back to the plugin.
  if (dirtyRect) {
    dirtyRect->left   = env->GetIntField(info, gSurfaceJavaGlue.jDirtyLeft);
    dirtyRect->right  = env->GetIntField(info, gSurfaceJavaGlue.jDirtyRight);
    dirtyRect->top    = env->GetIntField(info, gSurfaceJavaGlue.jDirtyTop);
    dirtyRect->bottom = env->GetIntField(info, gSurfaceJavaGlue.jDirtyBottom);
    LOG("dirty rect: %d, %d, %d, %d", dirtyRect->top, dirtyRect->left, dirtyRect->bottom, dirtyRect->right);
  }

  bitmap->width  = env->GetIntField(info, gSurfaceJavaGlue.jWidth);
  bitmap->height = env->GetIntField(info, gSurfaceJavaGlue.jHeight);

  int format = env->GetIntField(info, gSurfaceJavaGlue.jFormat);

  // format is PixelFormat
  if (format & 0x00000001) {
    bitmap->format = kRGBA_8888_ANPBitmapFormat;
    bitmap->rowBytes = bitmap->width * 4;
  }
  else if (format & 0x00000004) {
    bitmap->format = kRGB_565_ANPBitmapFormat;
    bitmap->rowBytes = bitmap->width * 2;
  }
  else {
    LOG("format from glue is unknown %d\n", format);
    return false;
  }

  jobject buf = env->GetObjectField(info, gSurfaceJavaGlue.jBuffer);
  bitmap->baseAddr = env->GetDirectBufferAddress(buf);
  
  LOG("format: %d, width: %d, height: %d",  bitmap->format,  bitmap->width,  bitmap->height);
  env->DeleteLocalRef(info);
  env->DeleteLocalRef(buf);
  return ( bitmap->width > 0 && bitmap->height > 0 );
}

static void anp_unlock(JNIEnv* env, jobject surfaceView) {
  LOG("%s", __PRETTY_FUNCTION__);

  if (!surfaceView) {
    LOG("null surface, exiting %s", __PRETTY_FUNCTION__);
    return;
  }

  init(env);
  env->CallStaticVoidMethod(gSurfaceJavaGlue.geckoAppShellClass, gSurfaceJavaGlue.jUnlockSurfaceANP, surfaceView);
  LOG("returning from %s", __PRETTY_FUNCTION__);

}

///////////////////////////////////////////////////////////////////////////////

#define ASSIGN(obj, name)   (obj)->name = anp_##name

void InitSurfaceInterface(ANPSurfaceInterfaceV0 *i) {

  ASSIGN(i, lock);
  ASSIGN(i, unlock);

  // setup the java glue struct
  gSurfaceJavaGlue.initialized = false;
}
