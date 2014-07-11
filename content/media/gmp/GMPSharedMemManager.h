/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPSharedMemManager_h_
#define GMPSharedMemManager_h_

#include "mozilla/ipc/Shmem.h"
#include "nsTArray.h"

namespace mozilla {
namespace gmp {

class GMPSharedMemManager
{
public:
  typedef enum {
    kGMPFrameData = 0,
    kGMPEncodedData,
    kGMPNumTypes
  } GMPMemoryClasses;

  // This is a heuristic - max of 10 free in the Child pool, plus those
  // in-use for the encoder and decoder at the given moment and not yet
  // returned to the parent pool (which is not included).  If more than
  // this are needed, we presume the client has either crashed or hung
  // (perhaps temporarily).
  static const uint32_t kGMPBufLimit = 20;

  GMPSharedMemManager();
  virtual ~GMPSharedMemManager();

  virtual bool MgrAllocShmem(GMPMemoryClasses aClass, size_t aSize,
                             ipc::Shmem::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aMem);
  virtual bool MgrDeallocShmem(GMPMemoryClasses aClass, ipc::Shmem& aMem);

  // So we can know if data is "piling up" for the plugin - I.e. it's hung or crashed
  virtual uint32_t NumInUse(GMPMemoryClasses aClass);

  // Parent and child impls will differ here
  virtual void CheckThread() = 0;

  // These have to be implemented using the AllocShmem/etc provided by the IPDL-generated interfaces,
  // so have the Parent/Child implement them.
  virtual bool Alloc(size_t aSize, ipc::Shmem::SharedMemory::SharedMemoryType aType, ipc::Shmem* aMem) = 0;
  virtual void Dealloc(ipc::Shmem& aMem) = 0;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPSharedMemManager_h_
