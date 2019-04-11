/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidNativeWindow_h__
#define AndroidNativeWindow_h__
#ifdef MOZ_WIDGET_ANDROID

#  include <jni.h>
#  include <android/native_window.h>
#  include <android/native_window_jni.h>
#  include "GeneratedJNIWrappers.h"
#  include "SurfaceTexture.h"

namespace mozilla {
namespace gl {

class AndroidNativeWindow final {
 public:
  AndroidNativeWindow() : mNativeWindow(nullptr) {}

  explicit AndroidNativeWindow(java::sdk::Surface::Param aSurface) {
    mNativeWindow =
        ANativeWindow_fromSurface(jni::GetEnvForThread(), aSurface.Get());
  }

  explicit AndroidNativeWindow(java::GeckoSurface::Param aSurface) {
    auto surf =
        java::sdk::Surface::LocalRef(java::sdk::Surface::Ref::From(aSurface));
    mNativeWindow =
        ANativeWindow_fromSurface(jni::GetEnvForThread(), surf.Get());
  }

  ~AndroidNativeWindow() {
    if (mNativeWindow) {
      ANativeWindow_release(mNativeWindow);
      mNativeWindow = nullptr;
    }
  }

  ANativeWindow* NativeWindow() const { return mNativeWindow; }

 private:
  ANativeWindow* mNativeWindow;
};

}  // namespace gl
}  // namespace mozilla

#endif  // MOZ_WIDGET_ANDROID
#endif  // AndroidNativeWindow_h__
