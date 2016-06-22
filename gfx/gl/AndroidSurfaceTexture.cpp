/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_ANDROID

#include <map>
#include <android/native_window_jni.h>
#include <android/log.h>
#include "AndroidSurfaceTexture.h"
#include "gfxImageSurface.h"
#include "gfxPrefs.h"
#include "AndroidBridge.h"
#include "nsThreadUtils.h"
#include "mozilla/gfx/Matrix.h"
#include "GeneratedJNIWrappers.h"
#include "SurfaceTexture.h"
#include "GLContext.h"

using namespace mozilla;
using namespace mozilla::jni;
using namespace mozilla::java;
using namespace mozilla::java::sdk;

namespace mozilla {
namespace gl {

// Maintains a mapping between AndroidSurfaceTexture instances and their
// unique numerical IDs. [thread-safe]
class InstanceMap
{
  typedef AndroidSurfaceTexture* InstancePtr;
  typedef std::map<int, InstancePtr> MapType;

public:
  InstanceMap()
    : mNextId(0)
    , mMonitor("AndroidSurfaceTexture::InstanceMap::mMonitor")
  {}

  int Add(InstancePtr aInstance)
  {
    MonitorAutoLock lock(mMonitor);
    mInstances.insert({++mNextId, aInstance});
    return mNextId;
  }

  void Remove(int aId)
  {
    MonitorAutoLock lock(mMonitor);
    mInstances.erase(aId);
  }

  InstancePtr Get(int aId) const
  {
    MonitorAutoLock lock(mMonitor);

    auto it = mInstances.find(aId);
    if (it == mInstances.end()) {
      return nullptr;
    }
    return it->second;
  }

private:
  MapType mInstances;
  int mNextId;

  mutable Monitor mMonitor;
};

static InstanceMap sInstances;

AndroidSurfaceTexture*
AndroidSurfaceTexture::Find(int aId)
{
  return sInstances.Get(aId);
}

static bool
IsSTSupported()
{
  return AndroidBridge::Bridge()->GetAPIVersion() >= 14; /* ICS */
}

already_AddRefed<AndroidSurfaceTexture>
AndroidSurfaceTexture::Create()
{
  return Create(nullptr, 0);
}

already_AddRefed<AndroidSurfaceTexture>
AndroidSurfaceTexture::Create(GLContext* aContext, GLuint aTexture)
{
  if (!IsSTSupported()) {
    return nullptr;
  }

  RefPtr<AndroidSurfaceTexture> st = new AndroidSurfaceTexture();
  if (!st->Init(aContext, aTexture)) {
    printf_stderr("Failed to initialize AndroidSurfaceTexture");
    st = nullptr;
  }

  return st.forget();
}

nsresult
AndroidSurfaceTexture::Attach(GLContext* aContext, PRIntervalTime aTimeout)
{
  MonitorAutoLock lock(mMonitor);

  if (mAttachedContext == aContext) {
    NS_WARNING("Tried to attach same GLContext to AndroidSurfaceTexture");
    return NS_OK;
  }

  if (!CanDetach()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  while (mAttachedContext) {
    // Wait until it's detached (or we time out)
    if (NS_FAILED(lock.Wait(aTimeout))) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  MOZ_ASSERT(aContext->IsOwningThreadCurrent(), "Trying to attach GLContext from different thread");

  aContext->fGenTextures(1, &mTexture);

  if (NS_FAILED(mSurfaceTexture->AttachToGLContext(mTexture))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  mAttachedContext = aContext;
  mAttachedContext->MakeCurrent();

  return NS_OK;
}

nsresult
AndroidSurfaceTexture::Detach()
{
  MonitorAutoLock lock(mMonitor);

  if (!CanDetach() ||
      !mAttachedContext ||
      !mAttachedContext->IsOwningThreadCurrent())
  {
    return NS_ERROR_FAILURE;
  }

  mAttachedContext->MakeCurrent();

  mSurfaceTexture->DetachFromGLContext();

  mTexture = 0;
  mAttachedContext = nullptr;
  lock.NotifyAll();
  return NS_OK;
}

bool
AndroidSurfaceTexture::CanDetach() const
{
  // The API for attach/detach only exists on 16+, and PowerVR has some sort of
  // fencing issue. Additionally, attach/detach seems to be busted on at least
  // some Mali adapters (400MP2 for sure, bug 1131793)
  return AndroidBridge::Bridge()->GetAPIVersion() >= 16 &&
    (!mAttachedContext || mAttachedContext->Vendor() != GLVendor::Imagination) &&
    (!mAttachedContext || mAttachedContext->Vendor() != GLVendor::ARM /* Mali */) &&
    gfxPrefs::SurfaceTextureDetachEnabled();
}

bool
AndroidSurfaceTexture::Init(GLContext* aContext, GLuint aTexture)
{

  if (!aTexture && !CanDetach()) {
    // We have no texture and cannot initialize detached, bail out
    return false;
  }

  if (NS_WARN_IF(NS_FAILED(
      SurfaceTexture::New(aTexture, ReturnTo(&mSurfaceTexture))))) {
    return false;
  }

  if (!aTexture) {
    mSurfaceTexture->DetachFromGLContext();
  }

  mAttachedContext = aContext;

  if (NS_WARN_IF(NS_FAILED(
      Surface::New(mSurfaceTexture, ReturnTo(&mSurface))))) {
    return false;
  }

  mNativeWindow = ANativeWindow_fromSurface(jni::GetEnvForThread(),
                                            mSurface.Get());
  MOZ_ASSERT(mNativeWindow, "Failed to create native window from surface");

  mID = sInstances.Add(this);

  return true;
}

AndroidSurfaceTexture::AndroidSurfaceTexture()
  : mTexture(0)
  , mSurfaceTexture()
  , mSurface()
  , mAttachedContext(nullptr)
  , mMonitor("AndroidSurfaceTexture::mContextMonitor")
{
}

AndroidSurfaceTexture::~AndroidSurfaceTexture()
{
  sInstances.Remove(mID);

  mFrameAvailableCallback = nullptr;

  if (mSurfaceTexture) {
    GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture);
    mSurfaceTexture = nullptr;
  }

  if (mNativeWindow) {
    ANativeWindow_release(mNativeWindow);
    mNativeWindow = nullptr;
  }
}

void
AndroidSurfaceTexture::UpdateTexImage()
{
  mSurfaceTexture->UpdateTexImage();
}

void
AndroidSurfaceTexture::GetTransformMatrix(gfx::Matrix4x4& aMatrix) const
{
  JNIEnv* const env = jni::GetEnvForThread();

  auto jarray = FloatArray::LocalRef::Adopt(env, env->NewFloatArray(16));
  mSurfaceTexture->GetTransformMatrix(jarray);

  jfloat* array = env->GetFloatArrayElements(jarray.Get(), nullptr);

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

  env->ReleaseFloatArrayElements(jarray.Get(), array, 0);
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
  mSurfaceTexture->SetDefaultBufferSize(size.width, size.height);
}

void
AndroidSurfaceTexture::NotifyFrameAvailable()
{
  if (mFrameAvailableCallback) {
    // Proxy to main thread if we aren't on it
    if (!NS_IsMainThread()) {
      // Proxy to main thread
      NS_DispatchToCurrentThread(NewRunnableMethod(this, &AndroidSurfaceTexture::NotifyFrameAvailable));
    } else {
      mFrameAvailableCallback->Run();
    }
  }
}

} // gl
} // mozilla

#endif // MOZ_WIDGET_ANDROID
