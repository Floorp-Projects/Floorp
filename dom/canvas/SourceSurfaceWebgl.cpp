/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetWebglInternal.h"
#include "SourceSurfaceWebgl.h"

namespace mozilla::gfx {

SourceSurfaceWebgl::SourceSurfaceWebgl() = default;

SourceSurfaceWebgl::~SourceSurfaceWebgl() {
  if (mHandle) {
    // Signal that the texture handle is not being used now.
    mHandle->SetSurface(nullptr);
  }
}

bool SourceSurfaceWebgl::Init(DrawTargetWebgl* aDT) {
  MOZ_ASSERT(!mDT);
  MOZ_ASSERT(aDT);
  mDT = aDT;
  mSharedContext = aDT->mSharedContext;
  mSize = aDT->GetSize();
  mFormat = aDT->GetFormat();
  return true;
}

// Read back the contents of the target or texture handle for data use.
inline bool SourceSurfaceWebgl::EnsureData() {
  if (mData) {
    return true;
  }
  if (!mDT) {
    // Assume that the target changed, so there should be a texture handle
    // holding a copy. Try to read data from the copy since we can't read
    // from the target.
    if (!mHandle || !mSharedContext) {
      return false;
    }
    mData = mSharedContext->ReadSnapshot(mHandle);
  } else {
    mData = mDT->ReadSnapshot();
  }
  return !!mData;
}

uint8_t* SourceSurfaceWebgl::GetData() {
  if (!EnsureData()) {
    return nullptr;
  }
  return mData->GetData();
}

int32_t SourceSurfaceWebgl::Stride() {
  if (!EnsureData()) {
    return 0;
  }
  return mData->Stride();
}

bool SourceSurfaceWebgl::Map(MapType aType, MappedSurface* aMappedSurface) {
  if (!EnsureData()) {
    return false;
  }
  return mData->Map(aType, aMappedSurface);
}

void SourceSurfaceWebgl::Unmap() {
  if (mData) {
    mData->Unmap();
  }
}

// Handler for when the owner DrawTargetWebgl is about to modify its internal
// framebuffer, and so this snapshot must be copied into a new texture, if
// possible, or read back into data, if necessary, to preserve this particular
// version of the framebuffer.
void SourceSurfaceWebgl::DrawTargetWillChange() {
  MOZ_ASSERT(mDT);
  if (!mData && !mHandle) {
    // Prefer copying the framebuffer to a texture if possible.
    mHandle = mDT->CopySnapshot();
    if (mHandle) {
      // Link this surface to the handle.
      mHandle->SetSurface(this);
    } else {
      // If that fails, then try to just read the data to a surface.
      EnsureData();
    }
  }
  mDT = nullptr;
}

// Handler for when the owner DrawTargetWebgl is itself being destroyed and
// needs to transfer ownership of its internal backing texture to the snapshot.
void SourceSurfaceWebgl::GiveTexture(RefPtr<TextureHandle> aHandle) {
  // If we get here, then the target still points to this surface as its
  // snapshot and needs to hand off its backing texture before it is destroyed.
  MOZ_ASSERT(mDT);
  MOZ_ASSERT(!mHandle);
  mHandle = aHandle.forget();
  mHandle->SetSurface(this);
  mDT = nullptr;
}

// Handler for when the owner DrawTargetWebgl is destroying the cached texture
// handle that has been allocated for this snapshot.
void SourceSurfaceWebgl::OnUnlinkTexture(
    DrawTargetWebgl::SharedContext* aContext) {
  // If we get here, then we must have copied a snapshot, which only happens
  // if the target changed.
  MOZ_ASSERT(!mDT);
  MOZ_ASSERT(mHandle);
  if (!mData) {
    mData = aContext->ReadSnapshot(mHandle);
  }
  mHandle = nullptr;
}

}  // namespace mozilla::gfx
