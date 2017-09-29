/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidSurfaceTexture_h__
#define AndroidSurfaceTexture_h__
#ifdef MOZ_WIDGET_ANDROID

#include "mozilla/gfx/Matrix.h"
#include "SurfaceTexture.h"

typedef uint32_t AndroidSurfaceTextureHandle;

namespace mozilla {
namespace gl {

class AndroidSurfaceTexture {
public:
  static void GetTransformMatrix(java::sdk::SurfaceTexture::Param surfaceTexture,
                                 mozilla::gfx::Matrix4x4* outMatrix);

};

} // gl
} // mozilla

#endif // MOZ_WIDGET_ANDROID
#endif // AndroidSurfaceTexture_h__
