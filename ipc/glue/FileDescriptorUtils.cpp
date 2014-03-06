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

NS_IMPL_ISUPPORTS1(CloseFileRunnable, nsIRunnable)

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
