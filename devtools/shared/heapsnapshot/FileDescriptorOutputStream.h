/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_FileDescriptorOutputStream_h
#define mozilla_devtools_FileDescriptorOutputStream_h

#include <prio.h>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "nsIOutputStream.h"

namespace mozilla {
namespace devtools {

class FileDescriptorOutputStream final : public nsIOutputStream
{
private:
  PRFileDesc* fd;

public:
  static already_AddRefed<FileDescriptorOutputStream>
    Create(const ipc::FileDescriptor& fileDescriptor);

private:
  explicit FileDescriptorOutputStream(PRFileDesc* prfd)
    : fd(prfd)
  { }

  virtual ~FileDescriptorOutputStream() { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
};

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_FileDescriptorOutputStream_h
