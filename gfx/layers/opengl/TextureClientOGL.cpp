/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"                  // for GLContext, etc
#include "SurfaceStream.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/TextureClientOGL.h"
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
  mFlags |= TextureFlags::DEALLOCATE_CLIENT;
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
  aOutDescriptor = SharedTextureDescriptor(mShareType, mHandle, mSize, mInverted);
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
    AddFlags(TextureFlags::NEEDS_Y_FLIP);
  }
}

bool
SharedTextureClientOGL::Lock(OpenMode mode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid() || !IsAllocated()) {
    return false;
  }
  mIsLocked = true;
  return true;
}

void
SharedTextureClientOGL::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

bool
SharedTextureClientOGL::IsAllocated() const
{
  return mHandle != 0;
}

StreamTextureClientOGL::StreamTextureClientOGL(TextureFlags aFlags)
  : TextureClient(aFlags)
  , mIsLocked(false)
{
}

StreamTextureClientOGL::~StreamTextureClientOGL()
{
  // the data is owned externally.
}

bool
StreamTextureClientOGL::Lock(OpenMode mode)
{
  MOZ_ASSERT(!mIsLocked);
  if (!IsValid() || !IsAllocated()) {
    return false;
  }
  mIsLocked = true;
  return true;
}

void
StreamTextureClientOGL::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

bool
StreamTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  if (!IsAllocated()) {
    return false;
  }

  gfx::SurfaceStreamHandle handle = mStream->GetShareHandle();
  aOutDescriptor = SurfaceStreamDescriptor(handle, false);
  return true;
}

void
StreamTextureClientOGL::InitWith(gfx::SurfaceStream* aStream)
{
  MOZ_ASSERT(!IsAllocated());
  mStream = aStream;
  mGL = mStream->GLContext();
}

bool
StreamTextureClientOGL::IsAllocated() const
{
  return mStream != 0;
}


} // namespace
} // namespace
