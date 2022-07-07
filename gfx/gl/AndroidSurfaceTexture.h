/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidSurfaceTexture_h__
#define AndroidSurfaceTexture_h__

#include "mozilla/gfx/Matrix.h"

typedef uint64_t AndroidSurfaceTextureHandle;

#ifdef MOZ_WIDGET_ANDROID

#  include "SurfaceTexture.h"

namespace mozilla {
namespace gl {

class AndroidSurfaceTexture {
 public:
  static void Init();
  static void GetTransformMatrix(
      java::sdk::SurfaceTexture::Param surfaceTexture,
      mozilla::gfx::Matrix4x4* outMatrix);
};

}  // namespace gl
}  // namespace mozilla

#endif  // MOZ_WIDGET_ANDROID

#endif  // AndroidSurfaceTexture_h__
