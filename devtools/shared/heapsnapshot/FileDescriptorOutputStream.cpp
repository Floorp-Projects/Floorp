/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/devtools/FileDescriptorOutputStream.h"
#include "private/pprio.h"

namespace mozilla {
namespace devtools {

/* static */ already_AddRefed<FileDescriptorOutputStream>
FileDescriptorOutputStream::Create(const ipc::FileDescriptor& fileDescriptor)
{
  if (NS_WARN_IF(!fileDescriptor.IsValid()))
    return nullptr;

  PRFileDesc* prfd = PR_ImportFile(PROsfd(fileDescriptor.PlatformHandle()));
  if (NS_WARN_IF(!prfd))
    return nullptr;

  nsRefPtr<FileDescriptorOutputStream> stream = new FileDescriptorOutputStream(prfd);
  return stream.forget();
}

NS_IMPL_ISUPPORTS(FileDescriptorOutputStream, nsIOutputStream);

NS_IMETHODIMP
FileDescriptorOutputStream::Close()
{
  // Repeatedly closing is idempotent.
  if (!fd)
    return NS_OK;

  if (PR_Close(fd) != PR_SUCCESS)
    return NS_ERROR_FAILURE;
  fd = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
FileDescriptorOutputStream::Write(const char* buf, uint32_t count, uint32_t* retval)
{
  if (NS_WARN_IF(!fd))
    return NS_ERROR_FAILURE;

  auto written = PR_Write(fd, buf, count);
  if (written < 0)
    return NS_ERROR_FAILURE;
  *retval = written;
  return NS_OK;
}

NS_IMETHODIMP
FileDescriptorOutputStream::Flush()
{
  if (NS_WARN_IF(!fd))
    return NS_ERROR_FAILURE;

  return PR_Sync(fd) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
FileDescriptorOutputStream::WriteFrom(nsIInputStream* fromStream, uint32_t count,
                                      uint32_t* retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorOutputStream::WriteSegments(nsReadSegmentFun reader, void* closure,
                                          uint32_t count, uint32_t* retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
FileDescriptorOutputStream::IsNonBlocking(bool* retval)
{
  *retval = false;
  return NS_OK;
}

} // namespace devtools
} // namespace mozilla
