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
class PFileDescriptorSetChild;
class PFileDescriptorSetParent;
class PChildToParentStreamChild;
class PParentToChildStreamParent;

// Provide two interfaces for sending PParentToChildStream and
// PFileDescriptorSet constructor messages.
class ParentToChildStreamActorManager {
 public:
  virtual PParentToChildStreamParent* SendPParentToChildStreamConstructor(
      PParentToChildStreamParent* aActor) = 0;
  virtual PFileDescriptorSetParent* SendPFileDescriptorSetConstructor(
      const FileDescriptor& aFD) = 0;
};

class ChildToParentStreamActorManager {
 public:
  virtual PChildToParentStreamChild* SendPChildToParentStreamConstructor(
      PChildToParentStreamChild* aActor) = 0;
  virtual PFileDescriptorSetChild* SendPFileDescriptorSetConstructor(
      const FileDescriptor& aFD) = 0;
};

// If you want to serialize an inputStream, please use AutoIPCStream.
class InputStreamHelper {
 public:
  // These 2 methods allow to serialize an inputStream into InputStreamParams.
  // The manager is needed in case a stream needs to serialize itself as
  // IPCRemoteStream.
  // The stream serializes itself fully only if the resulting IPC message will
  // be smaller than |aMaxSize|. Otherwise, the stream serializes itself as
  // IPCRemoteStream, and, its content will be sent to the other side of the IPC
  // pipe in chunks. This sending can start immediatelly or at the first read
  // based on the value of |aDelayedStart|. The IPC message size is returned
  // into |aSizeUsed|.
  static void SerializeInputStream(nsIInputStream* aInputStream,
                                   InputStreamParams& aParams,
                                   nsTArray<FileDescriptor>& aFileDescriptors,
                                   bool aDelayedStart, uint32_t aMaxSize,
                                   uint32_t* aSizeUsed,
                                   ParentToChildStreamActorManager* aManager);

  static void SerializeInputStream(nsIInputStream* aInputStream,
                                   InputStreamParams& aParams,
                                   nsTArray<FileDescriptor>& aFileDescriptors,
                                   bool aDelayedStart, uint32_t aMaxSize,
                                   uint32_t* aSizeUsed,
                                   ChildToParentStreamActorManager* aManager);

  // When a stream wants to serialize itself as IPCRemoteStream, it uses one of
  // these methods.
  static void SerializeInputStreamAsPipe(
      nsIInputStream* aInputStream, InputStreamParams& aParams,
      bool aDelayedStart, ParentToChildStreamActorManager* aManager);

  static void SerializeInputStreamAsPipe(
      nsIInputStream* aInputStream, InputStreamParams& aParams,
      bool aDelayedStart, ChildToParentStreamActorManager* aManager);

  // After the sending of the inputStream into the IPC pipe, some of the
  // InputStreamParams data struct needs to be activated (IPCRemoteStream).
  // These 2 methods do that.
  static void PostSerializationActivation(InputStreamParams& aParams,
                                          bool aConsumedByIPC,
                                          bool aDelayedStart);

  static void PostSerializationActivation(Maybe<InputStreamParams>& aParams,
                                          bool aConsumedByIPC,
                                          bool aDelayedStart);

  static already_AddRefed<nsIInputStream> DeserializeInputStream(
      const InputStreamParams& aParams,
      const nsTArray<FileDescriptor>& aFileDescriptors);
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_InputStreamUtils_h
