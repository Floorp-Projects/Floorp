/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceGL.h"

#include "GLBlitHelper.h"
#include "GLContext.h"
#include "GLReadTexImageHelper.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/TextureForwarder.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

/*static*/
UniquePtr<SharedSurface_Basic> SharedSurface_Basic::Create(
    const SharedSurfaceDesc& desc) {
  auto fb = MozFramebuffer::Create(desc.gl, desc.size, 0, false);
  if (!fb) return nullptr;

  return AsUnique(new SharedSurface_Basic(desc, std::move(fb)));
}

SharedSurface_Basic::SharedSurface_Basic(const SharedSurfaceDesc& desc,
                                         UniquePtr<MozFramebuffer>&& fb)
    : SharedSurface(desc, std::move(fb)) {}

Maybe<layers::SurfaceDescriptor> SharedSurface_Basic::ToSurfaceDescriptor() {
  return Nothing();
}

////////////////////////////////////////////////////////////////////////

SurfaceFactory_Basic::SurfaceFactory_Basic(GLContext& gl)
    : SurfaceFactory({&gl, SharedSurfaceType::Basic,
                      layers::TextureType::Unknown, true}) {}

}  // namespace gl
}  // namespace mozilla
