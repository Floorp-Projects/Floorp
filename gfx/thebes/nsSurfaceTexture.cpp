/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_ANDROID

#include <set>
#include <map>
#include <android/log.h>
#include "nsSurfaceTexture.h"
#include "AndroidBridge.h"
#include "nsThreadUtils.h"
#include "mozilla/gfx/Matrix.h"

using namespace mozilla;

// UGH
static std::map<int, nsSurfaceTexture*> sInstances;
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

private:
  bool mInitialized;

  jclass jSurfaceTextureClass;
  jmethodID jSurfaceTexture_Ctor;
  jmethodID jSurfaceTexture_updateTexImage;
  jmethodID jSurfaceTexture_getTransformMatrix;

} sJNIFunctions;

nsSurfaceTexture*
nsSurfaceTexture::Create(GLuint aTexture)
{
  // Right now we only support creating this on the main thread because
  // of the JNIEnv assumptions in JNIHelper and elsewhere
  if (!NS_IsMainThread())
    return nullptr;

  nsSurfaceTexture* st = new nsSurfaceTexture();
  if (!st->Init(aTexture)) {
    printf_stderr("Failed to initialize nsSurfaceTexture");
    delete st;
    st = nullptr;
  }

  return st;
}

nsSurfaceTexture*
nsSurfaceTexture::Find(int id)
{
  std::map<int, nsSurfaceTexture*>::iterator it;

  it = sInstances.find(id);
  if (it == sInstances.end())
    return nullptr;

  return it->second;
}

bool
nsSurfaceTexture::Check()
{
  return sJNIFunctions.EnsureInitialized();
}

bool
nsSurfaceTexture::Init(GLuint aTexture)
{
  if (!sJNIFunctions.EnsureInitialized())
    return false;

  JNIEnv* env = GetJNIForThread();

  mSurfaceTexture = sJNIFunctions.CreateSurfaceTexture(aTexture);
  if (!mSurfaceTexture)
    return false;

  mNativeWindow = AndroidBridge::Bridge()->AcquireNativeWindowFromSurfaceTexture(env, mSurfaceTexture);

  mID = ++sNextID;
  sInstances.insert(std::pair<int, nsSurfaceTexture*>(mID, this));

  return true;
}

nsSurfaceTexture::nsSurfaceTexture()
  : mSurfaceTexture(nullptr), mNativeWindow(nullptr)
{
}

nsSurfaceTexture::~nsSurfaceTexture()
{
  sInstances.erase(mID);

  mFrameAvailableCallback = nullptr;

  if (mNativeWindow) {
    AndroidBridge::Bridge()->ReleaseNativeWindowForSurfaceTexture(mSurfaceTexture);
    mNativeWindow = nullptr;
  }

  JNIEnv* env = GetJNIForThread();

  if (mSurfaceTexture) {
    mozilla::widget::android::GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture);

    env->DeleteGlobalRef(mSurfaceTexture);
    mSurfaceTexture = nullptr;
  }
}

void*
nsSurfaceTexture::GetNativeWindow()
{
  return mNativeWindow;
}

void
nsSurfaceTexture::UpdateTexImage()
{
  sJNIFunctions.UpdateTexImage(mSurfaceTexture);
}

bool
nsSurfaceTexture::GetTransformMatrix(gfx::Matrix4x4& aMatrix)
{
  return sJNIFunctions.GetTransformMatrix(mSurfaceTexture, aMatrix);
}

void
nsSurfaceTexture::SetFrameAvailableCallback(nsIRunnable* aRunnable)
{
  if (aRunnable)
    mozilla::widget::android::GeckoAppShell::RegisterSurfaceTextureFrameListener(mSurfaceTexture, mID);
  else
    mozilla::widget::android::GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture);

  mFrameAvailableCallback = aRunnable;
}

void
nsSurfaceTexture::NotifyFrameAvailable()
{
  if (mFrameAvailableCallback) {
    // Proxy to main thread if we aren't on it
    if (!NS_IsMainThread()) {
      // Proxy to main thread 
      nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &nsSurfaceTexture::NotifyFrameAvailable);
      NS_DispatchToCurrentThread(event);
    } else {
      mFrameAvailableCallback->Run();
    }
  }
}

#endif // MOZ_WIDGET_ANDROID
