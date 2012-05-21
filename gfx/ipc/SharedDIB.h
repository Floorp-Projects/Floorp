/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_SharedDIB_h__
#define gfx_SharedDIB_h__

#include "base/shared_memory.h"
#include "prtypes.h"
#include "nscore.h"

namespace mozilla {
namespace gfx {

class SharedDIB
{
public:
  typedef base::SharedMemoryHandle Handle;

public:
  SharedDIB();
  ~SharedDIB();

  // Create and allocate a new shared dib.
  nsresult Create(PRUint32 aSize);

  // Destroy or release resources associated with this dib.
  nsresult Close();

  // Returns true if this object contains a valid dib.
  bool IsValid();

  // Wrap a new shared dib around allocated shared memory. Note aHandle must point
  // to a memory section large enough to hold a dib of size aSize, otherwise this
  // will fail.
  nsresult Attach(Handle aHandle, PRUint32 aSize);

  // Returns a SharedMemoryHandle suitable for sharing with another process.
  nsresult ShareToProcess(base::ProcessHandle aChildProcess, Handle *aChildHandle);

protected:
  base::SharedMemory *mShMem;
};

} // gfx
} // mozilla

#endif
