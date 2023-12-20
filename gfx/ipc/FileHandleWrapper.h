/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_gfx_ipc_FileHandleWrapper_h__
#define _include_gfx_ipc_FileHandleWrapper_h__

#include "mozilla/UniquePtrExtensions.h"
#include "nsISupportsImpl.h"

namespace mozilla {

namespace ipc {
template <typename T>
struct IPDLParamTraits;
}  // namespace ipc

namespace gfx {

//
// A class for sharing file handle or shared handle among multiple clients.
//
// The file handles or the shared handles consume system resources. The class
// could reduce the number of shared handles in a process.
//
class FileHandleWrapper {
  friend struct mozilla::ipc::IPDLParamTraits<gfx::FileHandleWrapper*>;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileHandleWrapper);

  explicit FileHandleWrapper(mozilla::UniqueFileHandle&& aHandle);

  mozilla::detail::FileHandleType GetHandle();

  mozilla::UniqueFileHandle ClonePlatformHandle();

 protected:
  ~FileHandleWrapper();

  const mozilla::UniqueFileHandle mHandle;
};

struct FenceInfo {
  FenceInfo() = default;
  FenceInfo(FileHandleWrapper* aFenceHandle, uint64_t aFenceValue)
      : mFenceHandle(aFenceHandle), mFenceValue(aFenceValue) {}

  bool operator==(const FenceInfo& aOther) const {
    return mFenceHandle == aOther.mFenceHandle &&
           mFenceValue == aOther.mFenceValue;
  }

  RefPtr<FileHandleWrapper> mFenceHandle;
  uint64_t mFenceValue = 0;
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_gfx_ipc_FileHandleWrapper_h__
