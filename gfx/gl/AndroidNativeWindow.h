/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidNativeWindow_h__
#define AndroidNativeWindow_h__
#ifdef MOZ_WIDGET_ANDROID

#include <jni.h>
#include "GLDefs.h"

#include "nsISupports.h"
#include "mozilla/TypedEnum.h"
#include "mozilla/gfx/2D.h"


namespace mozilla {
namespace gl {

MOZ_BEGIN_ENUM_CLASS(AndroidWindowFormat)
  Unknown = -1,
  RGBA_8888 = 1,
  RGBX_8888 = 1 << 1,
  RGB_565 = 1 << 2
MOZ_END_ENUM_CLASS(AndroidWindowFormat)

/**
 * This class is a wrapper around Android's SurfaceTexture class.
 * Usage is pretty much exactly like the Java class, so see
 * the Android documentation for details.
 */
class AndroidNativeWindow {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AndroidNativeWindow)

public:

  static AndroidNativeWindow* CreateFromSurface(JNIEnv* aEnv, jobject aSurface);

  gfx::IntSize Size();
  AndroidWindowFormat Format();

  bool SetBuffersGeometry(int32_t aWidth, int32_t aHeight, AndroidWindowFormat aFormat);

  bool Lock(void** out_bits, int32_t* out_width, int32_t* out_height, int32_t* out_stride, AndroidWindowFormat* out_format);
  bool UnlockAndPost();

  void* Handle() { return mWindow; }

protected:
  AndroidNativeWindow(void* aWindow)
    : mWindow(aWindow)
  {

  }

  virtual ~AndroidNativeWindow();

  void* mWindow;
};

}
}


#endif
#endif
