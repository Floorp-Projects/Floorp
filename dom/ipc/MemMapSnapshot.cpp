/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemMapSnapshot.h"

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "nsIFile.h"

#ifdef XP_UNIX
#  include <sys/stat.h>
#endif

namespace mozilla {

using loader::AutoMemMap;

namespace ipc {

Result<Ok, nsresult>
MemMapSnapshot::Init(size_t aSize)
{
  MOZ_ASSERT(!mInitialized);

  MOZ_TRY(Create(aSize));

  mInitialized = true;
  return Ok();
}

Result<Ok, nsresult>
MemMapSnapshot::Finalize(AutoMemMap& aMem)
{
  MOZ_ASSERT(mInitialized);

  MOZ_TRY(Freeze(aMem));

  mInitialized = false;
  return Ok();
}

#if defined(XP_WIN)

Result<Ok, nsresult>
MemMapSnapshot::Create(size_t aSize)
{
  HANDLE handle = CreateFileMapping(
      INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
      0, DWORD(aSize), nullptr);

  if (!handle) {
    return Err(NS_ERROR_FAILURE);
  }

  mFile.emplace(handle);
  return mMem.initWithHandle(mFile.ref(), aSize, PR_PROT_READWRITE);
}

Result<Ok, nsresult>
MemMapSnapshot::Freeze(AutoMemMap& aMem)
{
  auto orig = mFile.ref().ClonePlatformHandle();
  mFile.reset();

  HANDLE handle;
  if (!::DuplicateHandle(GetCurrentProcess(), orig.release(), GetCurrentProcess(),
                         &handle, GENERIC_READ | FILE_MAP_READ,
                         false, DUPLICATE_CLOSE_SOURCE)) {
    return Err(NS_ERROR_FAILURE);
  }

  return aMem.initWithHandle(FileDescriptor(handle), mMem.size());
}

#elif defined(XP_UNIX)

Result<Ok, nsresult>
MemMapSnapshot::Create(size_t aSize)
{
  FilePath path;
  ScopedCloseFile fd(file_util::CreateAndOpenTemporaryShmemFile(&path));
  if (!fd) {
    return Err(NS_ERROR_FAILURE);
  }

  if (HANDLE_EINTR(ftruncate(fileno(fd), aSize)) != 0) {
    return Err(NS_ERROR_FAILURE);
  }

  MOZ_TRY(mMem.init(FILEToFileDescriptor(fd), PR_PROT_READWRITE));

  mPath.Assign(path.value().data(), path.value().length());
  return Ok();
}

Result<Ok, nsresult>
MemMapSnapshot::Freeze(AutoMemMap& aMem)
{
  // Delete the shm file after we're done here, whether we succeed or not. The
  // open file descriptor will keep it alive until all remaining references
  // are closed, at which point it will be automatically freed.
  auto cleanup = MakeScopeExit([&]() {
    PR_Delete(mPath.get());
  });

  // Make the shm file readonly. This doesn't make a difference in practice,
  // since we open and share a read-only file descriptor, and then delete the
  // file. But it doesn't hurt, either.
  chmod(mPath.get(), 0400);

  nsCOMPtr<nsIFile> file;
  MOZ_TRY(NS_NewNativeLocalFile(mPath, /* followLinks = */ false,
                                getter_AddRefs(file)));

  auto result = aMem.init(file);
#ifdef XP_LINUX
  // On Linux automation runs, every few hundred thousand calls, our attempt to
  // stat the file that we just successfully opened fails with EBADF (bug
  // 1472889). Presumably this is a race with a background thread that double
  // closes a file, but is difficult to diagnose, so work around it by making a
  // second mapping attempt if the first one fails.
  if (!result.isOk()) {
    aMem.reset();
    result = aMem.init(file);
  }
#endif
  return result;
}

#else
#  error "Unsupported build configuration"
#endif

} // namespace ipc
} // namespace mozilla
