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
#include "SurfaceTexture.h"
#include "GLContext.h"
#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::jni;
using namespace mozilla::widget;
using namespace mozilla::widget::sdk;

namespace mozilla {
namespace gl {

// UGH
static std::map<int, AndroidSurfaceTexture*> sInstances;
static int sNextID = 0;

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

AndroidSurfaceTexture*
AndroidSurfaceTexture::Find(int id)
{
  std::map<int, AndroidSurfaceTexture*>::iterator it;

  it = sInstances.find(id);
  if (it == sInstances.end())
    return nullptr;

  return it->second;
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

  mAttachedContext = aContext;
  mAttachedContext->MakeCurrent();
  aContext->fGenTextures(1, &mTexture);

  UpdateCanDetach();

  return mSurfaceTexture->AttachToGLContext(mTexture);
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

void
AndroidSurfaceTexture::UpdateCanDetach()
{
  // The API for attach/detach only exists on 16+, and PowerVR has some sort of
  // fencing issue. Additionally, attach/detach seems to be busted on at least some
  // Mali adapters (400MP2 for sure, bug 1131793)
  bool canDetach = Preferences::GetBool("gfx.SurfaceTexture.detach.enabled", true);

  mCanDetach = AndroidBridge::Bridge()->GetAPIVersion() >= 16 &&
    (!mAttachedContext || mAttachedContext->Vendor() != GLVendor::Imagination) &&
    (!mAttachedContext || mAttachedContext->Vendor() != GLVendor::ARM /* Mali */) &&
    canDetach;
}

bool
AndroidSurfaceTexture::Init(GLContext* aContext, GLuint aTexture)
{
  UpdateCanDetach();

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

  mNativeWindow = AndroidNativeWindow::CreateFromSurface(GetJNIForThread(),
                                                         mSurface.Get());
  MOZ_ASSERT(mNativeWindow, "Failed to create native window from surface");

  mID = ++sNextID;
  sInstances.insert(std::pair<int, AndroidSurfaceTexture*>(mID, this));

  return true;
}

AndroidSurfaceTexture::AndroidSurfaceTexture()
  : mTexture(0)
  , mSurfaceTexture()
  , mSurface()
  , mMonitor("AndroidSurfaceTexture::mContextMonitor")
  , mAttachedContext(nullptr)
  , mCanDetach(false)
{
}

AndroidSurfaceTexture::~AndroidSurfaceTexture()
{
  sInstances.erase(mID);

  mFrameAvailableCallback = nullptr;

  if (mSurfaceTexture) {
    GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture);
    mSurfaceTexture = nullptr;
  }
}

void
AndroidSurfaceTexture::UpdateTexImage()
{
  mSurfaceTexture->UpdateTexImage();
}

void
AndroidSurfaceTexture::GetTransformMatrix(gfx::Matrix4x4& aMatrix)
{
  JNIEnv* env = GetJNIForThread();

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
