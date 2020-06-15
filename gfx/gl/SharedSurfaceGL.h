/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GL_H_
#define SHARED_SURFACE_GL_H_

#include "SharedSurface.h"

namespace mozilla {
namespace gl {

class MozFramebuffer;

// For readback and bootstrapping:
class SharedSurface_Basic final : public SharedSurface {
 public:
  static UniquePtr<SharedSurface_Basic> Create(const SharedSurfaceDesc&);

 private:
  SharedSurface_Basic(const SharedSurfaceDesc& desc,
                      UniquePtr<MozFramebuffer>&& fb);

 public:
  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;
};

class SurfaceFactory_Basic final : public SurfaceFactory {
 public:
  explicit SurfaceFactory_Basic(GLContext& gl);

  virtual UniquePtr<SharedSurface> CreateSharedImpl(
      const SharedSurfaceDesc& desc) override {
    return SharedSurface_Basic::Create(desc);
  }
};

}  // namespace gl
}  // namespace mozilla

#endif  // SHARED_SURFACE_GL_H_
