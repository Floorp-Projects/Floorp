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

using namespace mozilla;
using namespace mozilla::widget::android;
using namespace mozilla::widget::android::sdk;

namespace mozilla {
namespace gl {

// UGH
static std::map<int, AndroidSurfaceTexture*> sInstances;
static int sNextID = 0;

static bool
IsDetachSupported()
{
  return AndroidBridge::Bridge()->GetAPIVersion() >= 16; /* Jelly Bean */
}

static bool
IsSTSupported()
{
  return AndroidBridge::Bridge()->GetAPIVersion() >= 14; /* ICS */
}

TemporaryRef<AndroidSurfaceTexture>
AndroidSurfaceTexture::Create()
{
  return Create(nullptr, 0);
}

TemporaryRef<AndroidSurfaceTexture>
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

  if (!IsDetachSupported()) {
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

  nsresult res;
  mSurfaceTexture->AttachToGLContext(mTexture, &res);
  return res;
}

nsresult
AndroidSurfaceTexture::Detach()
{
  MonitorAutoLock lock(mMonitor);

  if (!IsDetachSupported() ||
      !mAttachedContext || !mAttachedContext->IsOwningThreadCurrent()) {
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
AndroidSurfaceTexture::Init(GLContext* aContext, GLuint aTexture)
{
  if (!aTexture && !IsDetachSupported()) {
    // We have no texture and cannot initialize detached, bail out
    return false;
  }

  nsresult res;
  mSurfaceTexture = new SurfaceTexture(aTexture, &res);
  if (NS_FAILED(res)) {
    return false;
  }

  if (!aTexture) {
    mSurfaceTexture->DetachFromGLContext();
  }

  mAttachedContext = aContext;

  mSurface = new Surface(mSurfaceTexture->wrappedObject(), &res);
  if (NS_FAILED(res)) {
    return false;
  }

  mNativeWindow = AndroidNativeWindow::CreateFromSurface(GetJNIForThread(),
                                                         mSurface->wrappedObject());
  MOZ_ASSERT(mNativeWindow, "Failed to create native window from surface");

  mID = ++sNextID;
  sInstances.insert(std::pair<int, AndroidSurfaceTexture*>(mID, this));

  return true;
}

AndroidSurfaceTexture::AndroidSurfaceTexture()
  : mTexture(0)
  , mSurfaceTexture(nullptr)
  , mSurface(nullptr)
  , mMonitor("AndroidSurfaceTexture::mContextMonitor")
  , mAttachedContext(nullptr)
{
}

AndroidSurfaceTexture::~AndroidSurfaceTexture()
{
  sInstances.erase(mID);

  mFrameAvailableCallback = nullptr;

  if (mSurfaceTexture) {
    GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture->wrappedObject());
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

  AutoLocalJNIFrame jniFrame(env);

  jfloatArray jarray = env->NewFloatArray(16);
  mSurfaceTexture->GetTransformMatrix(jarray);

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
}

void
AndroidSurfaceTexture::SetFrameAvailableCallback(nsIRunnable* aRunnable)
{
  if (aRunnable) {
    GeckoAppShell::RegisterSurfaceTextureFrameListener(mSurfaceTexture->wrappedObject(), mID);
  } else {
     GeckoAppShell::UnregisterSurfaceTextureFrameListener(mSurfaceTexture->wrappedObject());
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
