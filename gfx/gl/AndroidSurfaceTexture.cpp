/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_ANDROID

#include <set>
#include <map>
#include <android/log.h>
#include "AndroidSurfaceTexture.h"
#include "gfxImageSurface.h"
#include "AndroidBridge.h"
#include "nsThreadUtils.h"
#include "mozilla/gfx/Matrix.h"
#include "GeneratedJNIWrappers.h"

using namespace mozilla;
using namespace mozilla::widget::android;

namespace mozilla {
namespace gl {

// UGH
static std::map<int, AndroidSurfaceTexture*> sInstances;
static int sNextID = 0;

static class JNIFunctions {
public:

  JNIFunctions() : mInitialized(false)
  {
  }

  bool EnsureInitialized()
  {
    if (mInitialized)
      return true;

    JNIEnv* env = GetJNIForThread();

    AutoLocalJNIFrame jniFrame(env);

    jSurfaceTextureClass = (jclass)env->NewGlobalRef(env->FindClass("android/graphics/SurfaceTexture"));
    jSurfaceTexture_Ctor = env->GetMethodID(jSurfaceTextureClass, "<init>", "(I)V");
    jSurfaceTexture_updateTexImage = env->GetMethodID(jSurfaceTextureClass, "updateTexImage", "()V");
    jSurfaceTexture_getTransformMatrix = env->GetMethodID(jSurfaceTextureClass, "getTransformMatrix", "([F)V");
    jSurfaceTexture_setDefaultBufferSize = env->GetMethodID(jSurfaceTextureClass, "setDefaultBufferSize", "(II)V");

    jSurfaceClass = (jclass)env->NewGlobalRef(env->FindClass("android/view/Surface"));
    jSurface_Ctor = env->GetMethodID(jSurfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");

    mInitialized = true;
    return true;
  }

  jobject CreateSurfaceTexture(GLuint aTexture)
  {
    if (!EnsureInitialized())
      return nullptr;

    JNIEnv* env = GetJNIForThread();

    AutoLocalJNIFrame jniFrame(env);

    return env->NewGlobalRef(env->NewObject(jSurfaceTextureClass, jSurfaceTexture_Ctor, (int) aTexture));
  }

  jobject CreateSurface(jobject aSurfaceTexture)
  {
    if (!EnsureInitialized())
      return nullptr;

    JNIEnv* env = GetJNIForThread();
    AutoLocalJNIFrame jniFrame(env);
    return env->NewGlobalRef(env->NewObject(jSurfaceClass, jSurface_Ctor, aSurfaceTexture));
  }

  void ReleaseSurfaceTexture(jobject aSurfaceTexture)
  {
    JNIEnv* env = GetJNIForThread();

    env->DeleteGlobalRef(aSurfaceTexture);
  }

  void UpdateTexImage(jobject aSurfaceTexture)
  {
    JNIEnv* env = GetJNIForThread();

    AutoLocalJNIFrame jniFrame(env);
    env->CallVoidMethod(aSurfaceTexture, jSurfaceTexture_updateTexImage);
  }

  bool GetTransformMatrix(jobject aSurfaceTexture, gfx::Matrix4x4& aMatrix)
  {
    JNIEnv* env = GetJNIForThread();

    AutoLocalJNIFrame jniFrame(env);

    jfloatArray jarray = env->NewFloatArray(16);
    env->CallVoidMethod(aSurfaceTexture, jSurfaceTexture_getTransformMatrix, jarray);

    jfloat* array = env->GetFloatArrayElements(jarray, nullptr);

    aMatrix._11 = array[0];
    aMatrix._12 = array[1];
    aMatrix._13 = array[2];
    aMatrix._14 = array[3];

    aMatrix._21 = array[4];
    aMatrix._22 = array[5];
    aMatrix._23 = array[6];
    aMatrix._24 = array[7];

    aMatrix._31 = array[8];
    aMatrix._32 = array[9];
    aMatrix._33 = array[10];
    aMatrix._34 = array[11];

    aMatrix._41 = array[12];
    aMatrix._42 = array[13];
    aMatrix._43 = array[14];
    aMatrix._44 = array[15];

    env->ReleaseFloatArrayElements(jarray, array, 0);

    return false;
  }

  void SetDefaultBufferSize(jobject aSurfaceTexture, int32_t width, int32_t height)
  {
    JNIEnv* env = GetJNIForThread();

    AutoLocalJNIFrame jniFrame(env);
    env->CallVoidMethod(aSurfaceTexture, jSurfaceTexture_setDefaultBufferSize, width, height);
  }

private:
  bool mInitialized;

  jclass jSurfaceTextureClass;
  jmethodID jSurfaceTexture_Ctor;
  jmethodID jSurfaceTexture_updateTexImage;
  jmethodID jSurfaceTexture_getTransformMatrix;
  jmethodID jSurfaceTexture_setDefaultBufferSize;

  jclass jSurfaceClass;
  jmethodID jSurface_Ctor;

} sJNIFunctions;

AndroidSurfaceTexture*
AndroidSurfaceTexture::Create(GLuint aTexture)
{
  if (AndroidBridge::Bridge()->GetAPIVersion() < 14 /* Ice Cream Sandwich */) {
    return nullptr;
  }

  AndroidSurfaceTexture* st = new AndroidSurfaceTexture();
  if (!st->Init(aTexture)) {
    printf_stderr("Failed to initialize AndroidSurfaceTexture");
    delete st;
    st = nullptr;
  }

  return st;
}

AndroidSurfaceTexture*
AndroidSurfaceTexture::Find(int id)
{
  std::map<int, AndroidSurfaceTexture*>::iterator it;

  it = sInstances.find(id);
  if (it == sInstances.end())
    return nullptr;

  return it->second;
}

bool
AndroidSurfaceTexture::Check()
{
  return sJNIFunctions.EnsureInitialized();
}

bool
AndroidSurfaceTexture::Init(GLuint aTexture)
{
  if (!sJNIFunctions.EnsureInitialized())
    return false;

  JNIEnv* env = GetJNIForThread();

  mSurfaceTexture = sJNIFunctions.CreateSurfaceTexture(aTexture);
  if (!mSurfaceTexture) {
    return false;
  }

  mSurface = sJNIFunctions.CreateSurface(mSurfaceTexture);
  if (!mSurface) {
    return false;
  }

  mNativeWindow = AndroidNativeWindow::CreateFromSurface(env, mSurface);

  mID = ++sNextID;
  sInstances.insert(std::pair<int, AndroidSurfaceTexture*>(mID, this));

  mTexture = aTexture;

  return true;
}

AndroidSurfaceTexture::AndroidSurfaceTexture()
  : mTexture(0), mSurfaceTexture(nullptr), mSurface(nullptr)
{
}

AndroidSurfaceTexture::~AndroidSurfaceTexture()
{
  sInstances.erase(mID);

  mFrameAvailableCallback = nullptr;

  JNIEnv* env = GetJNIForThread();

  if (mSurfaceTexture) {
    GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture);

    env->DeleteGlobalRef(mSurfaceTexture);
    mSurfaceTexture = nullptr;
  }

  if (mSurface) {
    env->DeleteGlobalRef(mSurface);
    mSurface = nullptr;
  }
}

void
AndroidSurfaceTexture::UpdateTexImage()
{
  sJNIFunctions.UpdateTexImage(mSurfaceTexture);
}

bool
AndroidSurfaceTexture::GetTransformMatrix(gfx::Matrix4x4& aMatrix)
{
  return sJNIFunctions.GetTransformMatrix(mSurfaceTexture, aMatrix);
}

void
AndroidSurfaceTexture::SetFrameAvailableCallback(nsIRunnable* aRunnable)
{
  if (aRunnable) {
    GeckoAppShell::RegisterSurfaceTextureFrameListener(mSurfaceTexture, mID);
  } else {
     GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture);
  }

  mFrameAvailableCallback = aRunnable;
}

void
AndroidSurfaceTexture::SetDefaultSize(mozilla::gfx::IntSize size)
{
  sJNIFunctions.SetDefaultBufferSize(mSurfaceTexture, size.width, size.height);
}

void
AndroidSurfaceTexture::NotifyFrameAvailable()
{
  if (mFrameAvailableCallback) {
    // Proxy to main thread if we aren't on it
    if (!NS_IsMainThread()) {
      // Proxy to main thread
      nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &AndroidSurfaceTexture::NotifyFrameAvailable);
      NS_DispatchToCurrentThread(event);
    } else {
      mFrameAvailableCallback->Run();
    }
  }
}

} // gl
} // mozilla

#endif // MOZ_WIDGET_ANDROID
