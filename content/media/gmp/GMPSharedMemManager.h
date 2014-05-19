/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPSharedMemManager_h_
#define GMPSharedMemManager_h_

#include "mozilla/ipc/Shmem.h"

namespace mozilla {
namespace gmp {

class GMPSharedMemManager
{
public:
  virtual bool MgrAllocShmem(size_t aSize,
                             ipc::Shmem::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aMem) = 0;
  virtual bool MgrDeallocShmem(ipc::Shmem& aMem) = 0;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPSharedMemManager_h_
