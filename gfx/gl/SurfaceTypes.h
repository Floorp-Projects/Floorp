/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SURFACE_TYPES_H_
#define SURFACE_TYPES_H_

#include <cstdint>

namespace mozilla {
namespace layers {
class LayersIPCChannel;
}  // namespace layers

namespace gl {

enum class SharedSurfaceType : uint8_t {
  Basic,
  EGLImageShare,
  EGLSurfaceANGLE,
  DXGLInterop,
  DXGLInterop2,
  IOSurface,
  GLXDrawable,
  SharedGLTexture,
  AndroidSurfaceTexture,
  AndroidHardwareBuffer,
  EGLSurfaceDMABUF,
};

}  // namespace gl
}  // namespace mozilla

#endif  // SURFACE_TYPES_H_
