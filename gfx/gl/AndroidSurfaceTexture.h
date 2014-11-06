/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidSurfaceTexture_h__
#define AndroidSurfaceTexture_h__
#ifdef MOZ_WIDGET_ANDROID

#include <jni.h>
#include "nsIRunnable.h"
#include "gfxPlatform.h"
#include "GLDefs.h"

#include "AndroidNativeWindow.h"

class gfxASurface;

namespace mozilla {
namespace gfx {
class Matrix4x4;
}
}

namespace mozilla {
namespace gl {

/**
 * This class is a wrapper around Android's SurfaceTexture class.
 * Usage is pretty much exactly like the Java class, so see
 * the Android documentation for details.
 */
class AndroidSurfaceTexture {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AndroidSurfaceTexture)

public:
  static AndroidSurfaceTexture* Create(GLuint aTexture);
  static AndroidSurfaceTexture* Find(int id);

  // Returns with reasonable certainty whether or not we'll
  // be able to create and use a SurfaceTexture
  static bool Check();

  // This attaches the updated data to the TEXTURE_EXTERNAL target
  void UpdateTexImage();

  bool GetTransformMatrix(mozilla::gfx::Matrix4x4& aMatrix);
  int ID() { return mID; }

  void SetDefaultSize(mozilla::gfx::IntSize size);

  // The callback is guaranteed to be called on the main thread even
  // if the upstream callback is received on a different thread
  void SetFrameAvailableCallback(nsIRunnable* aRunnable);

  // Only should be called by AndroidJNI when we get a
  // callback from the underlying SurfaceTexture instance
  void NotifyFrameAvailable();

  GLuint Texture() { return mTexture; }
  jobject JavaSurface() { return mSurface; }
private:
  AndroidSurfaceTexture();
  ~AndroidSurfaceTexture();

  bool Init(GLuint aTexture);

  GLuint mTexture;
  jobject mSurfaceTexture;
  jobject mSurface;

  int mID;
  nsRefPtr<nsIRunnable> mFrameAvailableCallback;
};
  
}
}


#endif
#endif
