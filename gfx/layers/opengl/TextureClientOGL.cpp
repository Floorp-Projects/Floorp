/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClientOGL.h"
#include "GLContext.h"                  // for GLContext, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "nsSize.h"                     // for nsIntSize

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class CompositableForwarder;

SharedTextureClientOGL::SharedTextureClientOGL(TextureFlags aFlags)
  : TextureClient(aFlags)
  , mHandle(0)
  , mInverted(false)
{
  // SharedTextureClient is always owned externally.
  mFlags |= TEXTURE_DEALLOCATE_CLIENT;
}

SharedTextureClientOGL::~SharedTextureClientOGL()
{
  // the shared data is owned externally.
}


bool
SharedTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }
  nsIntSize nsSize(mSize.width, mSize.height);
  aOutDescriptor = SharedTextureDescriptor(mShareType, mHandle, nsSize, mInverted);
  return true;
}

void
SharedTextureClientOGL::InitWith(gl::SharedTextureHandle aHandle,
                                 gfx::IntSize aSize,
                                 gl::SharedTextureShareType aShareType,
                                 bool aInverted)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(!IsAllocated());
  mHandle = aHandle;
  mSize = aSize;
  mShareType = aShareType;
  mInverted = aInverted;
  if (mInverted) {
    AddFlags(TEXTURE_NEEDS_Y_FLIP);
  }
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
                                        gfxContentType aContentType)
{
  mSize = aSize;
  return true;
}


} // namespace
} // namespace
