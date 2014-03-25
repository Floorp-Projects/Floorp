/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_InputStreamUtils_h
#define mozilla_ipc_InputStreamUtils_h

#include "mozilla/ipc/InputStreamParams.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsTArray.h"

namespace mozilla {
namespace ipc {

class FileDescriptor;

void
SerializeInputStream(nsIInputStream* aInputStream,
                     InputStreamParams& aParams,
                     nsTArray<FileDescriptor>& aFileDescriptors);

void
SerializeInputStream(nsIInputStream* aInputStream,
                     OptionalInputStreamParams& aParams,
                     nsTArray<FileDescriptor>& aFileDescriptors);

already_AddRefed<nsIInputStream>
DeserializeInputStream(const InputStreamParams& aParams,
                       const nsTArray<FileDescriptor>& aFileDescriptors);

already_AddRefed<nsIInputStream>
DeserializeInputStream(const OptionalInputStreamParams& aParams,
                       const nsTArray<FileDescriptor>& aFileDescriptors);

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_InputStreamUtils_h
