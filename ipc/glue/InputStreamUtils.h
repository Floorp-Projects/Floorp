/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

// If you want to serialize an inputStream, please use SerializeIPCStream or
// nsIInputStream directly.
class InputStreamHelper {
 public:
  static void SerializedComplexity(nsIInputStream* aInputStream,
                                   uint32_t aMaxSize, uint32_t* aSizeUsed,
                                   uint32_t* aPipes, uint32_t* aTransferables);

  // These 2 methods allow to serialize an inputStream into InputStreamParams.
  // The manager is needed in case a stream needs to serialize itself as
  // IPCRemoteStream.
  // The stream serializes itself fully only if the resulting IPC message will
  // be smaller than |aMaxSize|. Otherwise, the stream serializes itself as a
  // DataPipe, and, its content will be sent to the other side of the IPC pipe
  // in chunks. The IPC message size is returned into |aSizeUsed|.
  static void SerializeInputStream(nsIInputStream* aInputStream,
                                   InputStreamParams& aParams,
                                   uint32_t aMaxSize, uint32_t* aSizeUsed);

  // When a stream wants to serialize itself as a DataPipe, it uses this method.
  static void SerializeInputStreamAsPipe(nsIInputStream* aInputStream,
                                         InputStreamParams& aParams);

  static already_AddRefed<nsIInputStream> DeserializeInputStream(
      const InputStreamParams& aParams);
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_InputStreamUtils_h
