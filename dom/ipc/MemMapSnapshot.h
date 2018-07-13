/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_ipc_MemMapSnapshot_h
#define dom_ipc_MemMapSnapshot_h

#include "AutoMemMap.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/Result.h"
#ifdef XP_WIN
#  include "mozilla/ipc/FileDescriptor.h"
#endif

namespace mozilla {
namespace ipc {

/**
 * A helper class for creating a read-only snapshot of memory-mapped data.
 *
 * The Init() method initializes a read-write memory mapped region of the given
 * size, which can be initialized with arbitrary data. The Finalize() method
 * remaps that region as read-only (and backs it with a read-only file
 * descriptor), and initializes an AutoMemMap with the new contents.
 *
 * The file descriptor for the resulting AutoMemMap can be shared among
 * processes, to safely access a shared, read-only copy of the data snapshot.
 */
class MOZ_RAII MemMapSnapshot
{
public:
  Result<Ok, nsresult> Init(size_t aSize);
  Result<Ok, nsresult> Finalize(loader::AutoMemMap& aMap);

  template<typename T = void>
  RangedPtr<T> Get()
  {
    MOZ_ASSERT(mInitialized);
    return mMem.get<T>();
  }

private:
  Result<Ok, nsresult> Create(size_t aSize);
  Result<Ok, nsresult> Freeze(loader::AutoMemMap& aMem);

  loader::AutoMemMap mMem;

  bool mInitialized = false;

#ifdef XP_WIN
  Maybe<FileDescriptor> mFile;
#else
  nsCString mPath;
#endif
};

} // ipc
} // mozilla

#endif // dom_ipc_MemMapSnapshot_h
