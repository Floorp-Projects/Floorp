/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_REFCOUNTED_SHMEM_H
#define MOZILLA_LAYERS_REFCOUNTED_SHMEM_H

#include "mozilla/ipc/Shmem.h"
#include "chrome/common/ipc_message_utils.h"

namespace mozilla {
namespace ipc {
class IProtocol;
}

namespace layers {

// This class is IPDL-defined
class RefCountedShmem;

// This just implement the methods externally.
class RefCountedShm
{
public:

  static uint8_t* GetBytes(const RefCountedShmem& aShm);

  static size_t GetSize(const RefCountedShmem& aShm);

  static bool IsValid(const RefCountedShmem& aShm);

  static bool Alloc(mozilla::ipc::IProtocol* aAllocator, size_t aSize, RefCountedShmem& aShm);

  static void Dealloc(mozilla::ipc::IProtocol* aAllocator, RefCountedShmem& aShm);

  static int32_t GetReferenceCount(const RefCountedShmem& aShm);

  static int32_t AddRef(const RefCountedShmem& aShm);

  static int32_t Release(const RefCountedShmem& aShm);
};

} // namespace
} // namespace

#endif
