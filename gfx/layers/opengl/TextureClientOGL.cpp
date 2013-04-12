/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClientOGL.h"
#include "GLContext.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

TextureClientSharedOGL::TextureClientSharedOGL(CompositableForwarder* aForwarder,
                                               const TextureInfo& aTextureInfo)
  : TextureClient(aForwarder, aTextureInfo)
  , mGL(nullptr)
{
}

void
TextureClientSharedOGL::ReleaseResources()
{
  if (!IsSurfaceDescriptorValid(mDescriptor)) {
    return;
  }
  MOZ_ASSERT(mDescriptor.type() == SurfaceDescriptor::TSharedTextureDescriptor);
  mDescriptor = SurfaceDescriptor();
  // It's important our handle gets released! SharedTextureHostOGL will take
  // care of this for us though.
}

void
TextureClientSharedOGL::EnsureAllocated(gfx::IntSize aSize,
                                        gfxASurface::gfxContentType aContentType)
{
  mSize = aSize;
}


} // namespace
} // namespace
