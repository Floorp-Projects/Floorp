/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_nsIIPCSerializableInputStream_h
#define mozilla_ipc_nsIIPCSerializableInputStream_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace ipc {

class FileDescriptor;
class InputStreamParams;

}  // namespace ipc

}  // namespace mozilla

#define NS_IIPCSERIALIZABLEINPUTSTREAM_IID           \
  {                                                  \
    0xb0211b14, 0xea6d, 0x40d4, {                    \
      0x87, 0xb5, 0x7b, 0xe3, 0xdf, 0xac, 0x09, 0xd1 \
    }                                                \
  }

class NS_NO_VTABLE nsIIPCSerializableInputStream : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

  // Determine the serialized complexity of this input stream, initializing
  // `*aSizeUsed`, `*aPipes` and `*aTransferables` to the number of inline
  // bytes/pipes/transferable resources which would be used. This will be used
  // by other `Serialize` implementations to potentially simplify the resulting
  // stream, reducing the number of pipes or file descriptors required.
  //
  // Each outparameter corresponds to a type of resource which will be included
  // in the serialized message, as follows:
  //
  // *aSizeUsed:
  //    Raw bytes to be included inline in the message's payload, usually in the
  //    form of a nsCString for a StringInputStreamParams. This must be less
  //    than or equal to `aMaxSize`. Larger payloads should instead be
  //    serialized using SerializeInputStreamAsPipe.
  // *aPipes:
  //    New pipes, created using SerializeInputStreamAsPipe, which will be used
  //    to asynchronously transfer part of the pipe over IPC. Callers such as
  //    nsMultiplexInputStream may choose to serialize themselves as a DataPipe
  //    if they contain DataPipes themselves, so existing DataPipe instances
  //    which are cheaply transferred should be counted as transferrables.
  // *aTransferables:
  //    Existing objects which can be more cheaply transferred over IPC than by
  //    serializing them inline in a payload or transferring them through a new
  //    DataPipe. This includes RemoteLazyInputStreams, FileDescriptors, and
  //    existing DataPipeReceiver instances.
  //
  // Callers of this method must have initialized all of `*aSizeUsed`,
  // `*aPipes`, and `*aTransferables` to 0, so implementations are not required
  // to initialize all outparameters. The outparameters must not be null.
  virtual void SerializedComplexity(uint32_t aMaxSize, uint32_t* aSizeUsed,
                                    uint32_t* aPipes,
                                    uint32_t* aTransferables) = 0;

  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,
                         uint32_t aMaxSize, uint32_t* aSizeUsed) = 0;

  virtual bool Deserialize(const mozilla::ipc::InputStreamParams& aParams) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIIPCSerializableInputStream,
                              NS_IIPCSERIALIZABLEINPUTSTREAM_IID)

#define NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM                               \
  virtual void SerializedComplexity(uint32_t aMaxSize, uint32_t* aSizeUsed, \
                                    uint32_t* aPipes,                       \
                                    uint32_t* aTransferrables) override;    \
  virtual void Serialize(mozilla::ipc::InputStreamParams&, uint32_t,        \
                         uint32_t*) override;                               \
                                                                            \
  virtual bool Deserialize(const mozilla::ipc::InputStreamParams&) override;

#define NS_FORWARD_NSIIPCSERIALIZABLEINPUTSTREAM(_to)                       \
  virtual void SerializedComplexity(uint32_t aMaxSize, uint32_t* aSizeUsed, \
                                    uint32_t* aPipes,                       \
                                    uint32_t* aTransferables) override {    \
    _to SerializedComplexity(aMaxSize, aSizeUsed, aPipes, aTransferables);  \
  };                                                                        \
                                                                            \
  virtual void Serialize(mozilla::ipc::InputStreamParams& aParams,          \
                         uint32_t aMaxSize, uint32_t* aSizeUsed) override { \
    _to Serialize(aParams, aMaxSize, aSizeUsed);                            \
  }                                                                         \
                                                                            \
  virtual bool Deserialize(const mozilla::ipc::InputStreamParams& aParams)  \
      override {                                                            \
    return _to Deserialize(aParams);                                        \
  }

#endif  // mozilla_ipc_nsIIPCSerializableInputStream_h
