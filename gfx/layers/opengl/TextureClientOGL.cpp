/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClientOGL.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "GLContext.h"
#include "gfxipc/ShadowLayerUtils.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

SharedTextureClientOGL::SharedTextureClientOGL()
: mHandle(0), mIsCrossProcess(false), mInverted(false)
{
}

SharedTextureClientOGL::~SharedTextureClientOGL()
{
  // the data is released by the host
}


bool
SharedTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  if (!IsAllocated()) {
    return false;
  }
  nsIntSize nsSize(mSize.width, mSize.height);
  aOutDescriptor = SharedTextureDescriptor(mIsCrossProcess ? gl::GLContext::CrossProcess
                                                           : gl::GLContext::SameProcess,
                                           mHandle, nsSize, mInverted);
  return true;
}

void
SharedTextureClientOGL::InitWith(gl::SharedTextureHandle aHandle,
                                 gfx::IntSize aSize,
                                 bool aIsCrossProcess,
                                 bool aInverted)
{
  MOZ_ASSERT(!IsAllocated());
  mHandle = aHandle;
  mSize = aSize;
  mIsCrossProcess = aIsCrossProcess;
  mInverted = aInverted;
}

bool
SharedTextureClientOGL::IsAllocated() const
{
  return mHandle != 0;
}



DeprecatedTextureClientSharedOGL::DeprecatedTextureClientSharedOGL(CompositableForwarder* aForwarder,
                                               const TextureInfo& aTextureInfo)
  : DeprecatedTextureClient(aForwarder, aTextureInfo)
  , mGL(nullptr)
{
}

void
DeprecatedTextureClientSharedOGL::ReleaseResources()
{
  if (!IsSurfaceDescriptorValid(mDescriptor)) {
    return;
  }
  MOZ_ASSERT(mDescriptor.type() == SurfaceDescriptor::TSharedTextureDescriptor);
  mDescriptor = SurfaceDescriptor();
  // It's important our handle gets released! SharedDeprecatedTextureHostOGL will take
  // care of this for us though.
}

bool
DeprecatedTextureClientSharedOGL::EnsureAllocated(gfx::IntSize aSize,
                                        gfxASurface::gfxContentType aContentType)
{
  mSize = aSize;
  return true;
}


} // namespace
} // namespace
