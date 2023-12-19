/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileHandleWrapper.h"

#include "mozilla/ipc/FileDescriptor.h"

namespace mozilla::gfx {

FileHandleWrapper::FileHandleWrapper(mozilla::UniqueFileHandle&& aHandle)
    : mHandle(std::move(aHandle)) {}

FileHandleWrapper::~FileHandleWrapper() {}

mozilla::detail::FileHandleType FileHandleWrapper::GetHandle() {
  return mHandle.get();
}

mozilla::UniqueFileHandle FileHandleWrapper::ClonePlatformHandle() {
  auto handle = ipc::FileDescriptor(GetHandle());
  return handle.TakePlatformHandle();
}

}  // namespace mozilla::gfx
