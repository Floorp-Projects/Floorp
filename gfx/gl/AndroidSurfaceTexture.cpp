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
AndroidSurfaceTexture::GetTransformMatrix(java::sdk::SurfaceTexture::LocalRef aSurfaceTexture,
                                          gfx::Matrix4x4& aMatrix)
{
  JNIEnv* const env = jni::GetEnvForThread();

  auto jarray = jni::FloatArray::LocalRef::Adopt(env, env->NewFloatArray(16));
  aSurfaceTexture->GetTransformMatrix(jarray);

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

} // gl
} // mozilla
#endif // MOZ_WIDGET_ANDROID
