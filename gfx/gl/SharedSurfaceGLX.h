/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_GLX_H_
#define SHARED_SURFACE_GLX_H_

#include "SharedSurface.h"
#include "mozilla/RefPtr.h"

class gfxXlibSurface;

namespace mozilla {
namespace gl {

class SharedSurface_GLXDrawable final : public SharedSurface {
 public:
  const RefPtr<gfxXlibSurface> mXlibSurface;

  static UniquePtr<SharedSurface_GLXDrawable> Create(const SharedSurfaceDesc&);

 private:
  SharedSurface_GLXDrawable(const SharedSurfaceDesc&,
                            const RefPtr<gfxXlibSurface>& xlibSurface);

 public:
  ~SharedSurface_GLXDrawable();

  virtual void ProducerAcquireImpl() override {}
  virtual void ProducerReleaseImpl() override;

  virtual void LockProdImpl() override;
  virtual void UnlockProdImpl() override;

  Maybe<layers::SurfaceDescriptor> ToSurfaceDescriptor() override;
};

class SurfaceFactory_GLXDrawable final : public SurfaceFactory {
 public:
  explicit SurfaceFactory_GLXDrawable(GLContext&);

  virtual UniquePtr<SharedSurface> CreateSharedImpl(
      const SharedSurfaceDesc& desc) override;
};

}  // namespace gl
}  // namespace mozilla

#endif  // SHARED_SURFACE_GLX_H_
