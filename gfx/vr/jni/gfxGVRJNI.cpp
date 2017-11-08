/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <jni.h>

extern "C" __attribute__((visibility("default"))) jlong
Java_com_google_vr_cardboard_DisplaySynchronizer_nativeCreate(
    JNIEnv* env,
    jobject jcaller,
    jclass classLoader,
    jobject appContext);

// Step 2: method stubs.
extern "C" __attribute__((visibility("default"))) void
Java_com_google_vr_cardboard_DisplaySynchronizer_nativeDestroy(
    JNIEnv* env,
    jobject jcaller,
    jlong nativeDisplaySynchronizer);

extern "C" __attribute__((visibility("default"))) void
Java_com_google_vr_cardboard_DisplaySynchronizer_nativeReset(
    JNIEnv* env,
    jobject jcaller,
    jlong nativeDisplaySynchronizer,
    jlong expectedInterval,
    jlong vsyncOffset);

extern "C" __attribute__((visibility("default"))) void
Java_com_google_vr_cardboard_DisplaySynchronizer_nativeUpdate(
    JNIEnv* env,
    jobject jcaller,
    jlong nativeDisplaySynchronizer,
    jlong syncTime,
    jint currentRotation);

namespace {

bool
check(JNIEnv* env) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return false;
  }
  return true;
}

const char kDisplaySynchronizerClassPath[] = "com/google/vr/cardboard/DisplaySynchronizer";

static const JNINativeMethod kMethodsDisplaySynchronizer[] = {
    {"nativeCreate",
     "("
     "Ljava/lang/ClassLoader;"
     "Landroid/content/Context;"
     ")"
     "J",
     reinterpret_cast<void*>(
         Java_com_google_vr_cardboard_DisplaySynchronizer_nativeCreate)},
    {"nativeDestroy",
     "("
     "J"
     ")"
     "V",
     reinterpret_cast<void*>(
         Java_com_google_vr_cardboard_DisplaySynchronizer_nativeDestroy)},
    {"nativeReset",
     "("
     "J"
     "J"
     "J"
     ")"
     "V",
     reinterpret_cast<void*>(
         Java_com_google_vr_cardboard_DisplaySynchronizer_nativeReset)},
    {"nativeUpdate",
     "("
     "J"
     "J"
     "I"
     ")"
     "V",
     reinterpret_cast<void*>(
         Java_com_google_vr_cardboard_DisplaySynchronizer_nativeUpdate)},
};
}

bool
SetupGVRJNI(JNIEnv* env)
{
  jclass displaySynchronizerClazz = env->FindClass(kDisplaySynchronizerClassPath);
  if (!check(env)) { return false; }
  if (displaySynchronizerClazz == nullptr) {
    return false;
  }
  env->RegisterNatives(displaySynchronizerClazz, kMethodsDisplaySynchronizer, sizeof(kMethodsDisplaySynchronizer) / sizeof(kMethodsDisplaySynchronizer[0]));
  if (!check(env)) { return false; }
  env->DeleteLocalRef(displaySynchronizerClazz);
  if (!check(env)) { return false; }

  return true;
}

