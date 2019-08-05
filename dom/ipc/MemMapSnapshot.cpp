/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemMapSnapshot.h"

#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "mozilla/Assertions.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "nsIFile.h"

#ifdef XP_WIN
#  include "mozilla/RandomNum.h"
#  include "mozilla/WindowsVersion.h"
#  include "nsDebug.h"
#  include "nsString.h"
#  include <windows.h>
#endif

#ifdef XP_UNIX
#  include <sys/stat.h>
#endif

namespace mozilla {

using loader::AutoMemMap;

namespace ipc {

Result<Ok, nsresult> MemMapSnapshot::Init(size_t aSize) {
  MOZ_ASSERT(!mInitialized);

  MOZ_TRY(Create(aSize));

  mInitialized = true;
  return Ok();
}

Result<Ok, nsresult> MemMapSnapshot::Finalize(AutoMemMap& aMem) {
  MOZ_ASSERT(mInitialized);

  MOZ_TRY(Freeze(aMem));

  mInitialized = false;
  return Ok();
}

#if defined(XP_WIN)

Result<Ok, nsresult> MemMapSnapshot::Create(size_t aSize) {
  SECURITY_ATTRIBUTES sa;
  SECURITY_DESCRIPTOR sd;
  ACL dacl;

  sa.nLength = sizeof(sa);
  sa.lpSecurityDescriptor = &sd;
  sa.bInheritHandle = FALSE;

  if (NS_WARN_IF(!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) ||
      NS_WARN_IF(
          !InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) ||
      NS_WARN_IF(!SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE))) {
    return Err(NS_ERROR_FAILURE);
  }

  nsAutoStringN<sizeof("MozSharedMem_") + 16 * 4> name;
  if (!IsWin8Point1OrLater()) {
    name.AssignLiteral("MozSharedMem_");
    for (size_t i = 0; i < 4; ++i) {
      Maybe<uint64_t> randomNum = RandomUint64();
      if (NS_WARN_IF(randomNum.isNothing())) {
        return Err(NS_ERROR_UNEXPECTED);
      }
      name.AppendPrintf("%016llx", *randomNum);
    }
  }

  HANDLE handle =
      CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0,
                        DWORD(aSize), name.IsEmpty() ? nullptr : name.get());

  if (!handle) {
    return Err(NS_ERROR_FAILURE);
  }

  mFile.emplace(handle);
  return mMem.initWithHandle(mFile.ref(), aSize, PR_PROT_READWRITE);
}

Result<Ok, nsresult> MemMapSnapshot::Freeze(AutoMemMap& aMem) {
  auto orig = mFile.ref().ClonePlatformHandle();
  mFile.reset();

  HANDLE handle;
  if (!::DuplicateHandle(
          GetCurrentProcess(), orig.release(), GetCurrentProcess(), &handle,
          GENERIC_READ | FILE_MAP_READ, false, DUPLICATE_CLOSE_SOURCE)) {
    return Err(NS_ERROR_FAILURE);
  }

  return aMem.initWithHandle(FileDescriptor(handle), mMem.size());
}

#elif defined(XP_UNIX)

Result<Ok, nsresult> MemMapSnapshot::Create(size_t aSize) {
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

Result<Ok, nsresult> MemMapSnapshot::Freeze(AutoMemMap& aMem) {
  // Delete the shm file after we're done here, whether we succeed or not. The
  // open file descriptor will keep it alive until all remaining references
  // are closed, at which point it will be automatically freed.
  auto cleanup = MakeScopeExit([&]() { PR_Delete(mPath.get()); });

  // Make the shm file readonly. This doesn't make a difference in practice,
  // since we open and share a read-only file descriptor, and then delete the
  // file. But it doesn't hurt, either.
  chmod(mPath.get(), 0400);

  nsCOMPtr<nsIFile> file;
  MOZ_TRY(NS_NewNativeLocalFile(mPath, /* followLinks = */ false,
                                getter_AddRefs(file)));

  return aMem.init(file);
}

#else
#  error "Unsupported build configuration"
#endif

}  // namespace ipc
}  // namespace mozilla
