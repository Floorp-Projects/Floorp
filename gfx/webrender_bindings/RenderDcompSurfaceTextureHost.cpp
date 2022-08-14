/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderDcompSurfaceTextureHost.h"

#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINBLUE
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WINBLUE

#include <dcomp.h>

#define LOG(msg, ...)                                                        \
  MOZ_LOG(gDcompSurface, LogLevel::Debug,                                    \
          ("RenderDcompSurfaceTextureHost=%p, handle=%p, size=[%u,%u] " msg, \
           this, this->mHandle, this->mSize.Width(), this->mSize.Height(),   \
           ##__VA_ARGS__))

namespace mozilla::wr {

RenderDcompSurfaceTextureHost::RenderDcompSurfaceTextureHost(
    HANDLE aHandle, gfx::IntSize aSize, gfx::SurfaceFormat aFormat)
    : mHandle(aHandle), mSize(aSize), mFormat(aFormat) {
  MOZ_ASSERT(aHandle && aHandle != INVALID_HANDLE_VALUE);
}

IDCompositionSurface* RenderDcompSurfaceTextureHost::CreateSurfaceFromDevice(
    IDCompositionDevice* aDevice) {
  // Already created surface, no need to recreate it again.
  if (mDcompSurface) {
    return mDcompSurface;
  }

  auto* surface =
      static_cast<IDCompositionSurface**>(getter_AddRefs(mDcompSurface));
  auto hr = aDevice->CreateSurfaceFromHandle(
      mHandle, reinterpret_cast<IUnknown**>(surface));
  if (FAILED(hr)) {
    LOG("Failed to create surface from Dcomp handle %p, hr=%lx", mHandle, hr);
    return nullptr;
  }
  LOG("Created surface %p correctly", surface);
  return mDcompSurface;
}

}  // namespace mozilla::wr

#undef LOG
