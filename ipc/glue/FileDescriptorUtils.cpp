/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptorUtils.h"

#include "nsIEventTarget.h"

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "prio.h"
#include "private/pprio.h"

#include <errno.h>
#ifdef XP_WIN
#include <io.h>
#else
#include <unistd.h>
#endif

using mozilla::ipc::CloseFileRunnable;

#ifdef DEBUG

CloseFileRunnable::CloseFileRunnable(const FileDescriptor& aFileDescriptor)
: mFileDescriptor(aFileDescriptor)
{
  MOZ_ASSERT(aFileDescriptor.IsValid());
}

#endif // DEBUG

CloseFileRunnable::~CloseFileRunnable()
{
  if (mFileDescriptor.IsValid()) {
    // It's probably safer to take the main thread IO hit here rather than leak
    // the file descriptor.
    CloseFile();
  }
}

NS_IMPL_ISUPPORTS(CloseFileRunnable, nsIRunnable)

void
CloseFileRunnable::Dispatch()
{
  nsCOMPtr<nsIEventTarget> eventTarget =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(eventTarget);

  nsresult rv = eventTarget->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS_VOID(rv);
}

void
CloseFileRunnable::CloseFile()
{
  // It's possible for this to happen on the main thread if the dispatch to the
  // stream service fails so we can't assert the thread on which we're running.

  MOZ_ASSERT(mFileDescriptor.IsValid());

  PRFileDesc* fd =
    PR_ImportFile(PROsfd(mFileDescriptor.PlatformHandle()));
  NS_WARN_IF_FALSE(fd, "Failed to import file handle!");

  mFileDescriptor = FileDescriptor();

  if (fd) {
    PR_Close(fd);
    fd = nullptr;
  }
}

NS_IMETHODIMP
CloseFileRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  CloseFile();
  return NS_OK;
}

namespace mozilla {
namespace ipc {

FILE*
FileDescriptorToFILE(const FileDescriptor& aDesc,
                     const char* aOpenMode)
{
  // Debug builds check whether the handle was "used", even if it's
  // invalid, so that needs to happen first.
  FileDescriptor::PlatformHandleType handle = aDesc.PlatformHandle();
  if (!aDesc.IsValid()) {
    errno = EBADF;
    return nullptr;
  }
#ifdef XP_WIN
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), 0);
  if (fd == -1) {
    CloseHandle(handle);
    return nullptr;
  }
#else
  int fd = handle;
#endif
  FILE* file = fdopen(fd, aOpenMode);
  if (!file) {
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
  }
  return file;
}

FileDescriptor
FILEToFileDescriptor(FILE* aStream)
{
  if (!aStream) {
    errno = EBADF;
    return FileDescriptor();
  }
#ifdef XP_WIN
  int fd = _fileno(aStream);
  if (fd == -1) {
    return FileDescriptor();
  }
  return FileDescriptor(reinterpret_cast<HANDLE>(_get_osfhandle(fd)));
#else
  return FileDescriptor(fileno(aStream));
#endif
}

} // namespace ipc
} // namespace mozilla
