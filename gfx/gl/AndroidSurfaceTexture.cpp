/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_WIDGET_ANDROID

#include "AndroidSurfaceTexture.h"

using namespace mozilla;

namespace mozilla {
namespace gl {

void
AndroidSurfaceTexture::GetTransformMatrix(java::sdk::SurfaceTexture::Param surfaceTexture,
                                          gfx::Matrix4x4* outMatrix)
{
  JNIEnv* const env = jni::GetEnvForThread();

  auto jarray = jni::FloatArray::LocalRef::Adopt(env, env->NewFloatArray(16));
  surfaceTexture->GetTransformMatrix(jarray);

  jfloat* array = env->GetFloatArrayElements(jarray.Get(), nullptr);

  memcpy(&(outMatrix->_11), array, sizeof(float)*16);

  env->ReleaseFloatArrayElements(jarray.Get(), array, 0);
}

} // gl
} // mozilla
#endif // MOZ_WIDGET_ANDROID
