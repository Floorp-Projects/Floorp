/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSurfaceTexture_h__
#define nsSurfaceTexture_h__
#ifdef MOZ_WIDGET_ANDROID

#include <jni.h>
#include "nsIRunnable.h"
#include "gfxPlatform.h"
#include "gfx3DMatrix.h"
#include "GLDefs.h"

class gfxASurface;

/**
 * This class is a wrapper around Android's SurfaceTexture class.
 * Usage is pretty much exactly like the Java class, so see
 * the Android documentation for details.
 */
class nsSurfaceTexture {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsSurfaceTexture)

public:
  static nsSurfaceTexture* Create(GLuint aTexture);
  static nsSurfaceTexture* Find(int id);

  // Returns with reasonable certainty whether or not we'll
  // be able to create and use a SurfaceTexture
  static bool Check();
  
  ~nsSurfaceTexture();

  // This is an ANativeWindow. Use AndroidBridge::LockWindow and
  // friends for manipulating it.
  void* GetNativeWindow();

  // This attaches the updated data to the TEXTURE_EXTERNAL target
  void UpdateTexImage();

  bool GetTransformMatrix(gfx3DMatrix& aMatrix);
  int ID() { return mID; }

  // The callback is guaranteed to be called on the main thread even
  // if the upstream callback is received on a different thread
  void SetFrameAvailableCallback(nsIRunnable* aRunnable);

  // Only should be called by AndroidJNI when we get a
  // callback from the underlying SurfaceTexture instance
  void NotifyFrameAvailable();
private:
  nsSurfaceTexture();

  bool Init(GLuint aTexture);

  jobject mSurfaceTexture;
  void* mNativeWindow;
  int mID;
  nsRefPtr<nsIRunnable> mFrameAvailableCallback;
};

#endif
#endif
