/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputStreamUtils.h"

#include "nsIIPCSerializableInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IPCBlobInputStream.h"
#include "mozilla/dom/IPCBlobInputStreamStorage.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/InputStreamLengthWrapper.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsID.h"
#include "nsIPipe.h"
#include "nsIXULRuntime.h"
#include "nsMIMEInputStream.h"
#include "nsMultiplexInputStream.h"
#include "nsNetCID.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {

NS_DEFINE_CID(kStringInputStreamCID, NS_STRINGINPUTSTREAM_CID);
NS_DEFINE_CID(kFileInputStreamCID, NS_LOCALFILEINPUTSTREAM_CID);
NS_DEFINE_CID(kBufferedInputStreamCID, NS_BUFFEREDINPUTSTREAM_CID);
NS_DEFINE_CID(kMIMEInputStreamCID, NS_MIMEINPUTSTREAM_CID);
NS_DEFINE_CID(kMultiplexInputStreamCID, NS_MULTIPLEXINPUTSTREAM_CID);

}  // namespace

namespace mozilla {
namespace ipc {

namespace {

template <typename M>
void SerializeInputStreamInternal(nsIInputStream* aInputStream,
                                  InputStreamParams& aParams,
                                  nsTArray<FileDescriptor>& aFileDescriptors,
                                  bool aDelayedStart, uint32_t aMaxSize,
                                  uint32_t* aSizeUsed, M* aManager) {
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
      do_QueryInterface(aInputStream);
  if (!serializable) {
    MOZ_CRASH("Input stream is not serializable!");
  }

  serializable->Serialize(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                          aSizeUsed, aManager);

  if (aParams.type() == InputStreamParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }
}

template <typename M>
void SerializeInputStreamAsPipeInternal(nsIInputStream* aInputStream,
                                        InputStreamParams& aParams,
                                        bool aDelayedStart, M* aManager) {
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aManager);

  // Let's try to take the length using InputStreamLengthHelper. If the length
  // cannot be taken synchronously, and its length is needed, the stream needs
  // to be fully copied in memory on the deserialization side.
  int64_t length;
  if (!InputStreamLengthHelper::GetSyncLength(aInputStream, &length)) {
    length = -1;
  }

  // As a fallback, attempt to stream the data across using a IPCStream
  // actor. For blocking streams, create a nonblocking pipe instead,
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(aInputStream);
  if (!asyncStream) {
    const uint32_t kBufferSize = 32768;  // matches IPCStream buffer size.
    nsCOMPtr<nsIAsyncOutputStream> sink;
    nsresult rv = NS_NewPipe2(getter_AddRefs(asyncStream), getter_AddRefs(sink),
                              true, false, kBufferSize, UINT32_MAX);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIEventTarget> target =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

    rv = NS_AsyncCopy(aInputStream, sink, target, NS_ASYNCCOPY_VIA_READSEGMENTS,
                      kBufferSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  MOZ_ASSERT(asyncStream);

  aParams = IPCRemoteStreamParams(
      aDelayedStart, IPCStreamSource::Create(asyncStream, aManager), length);
}

}  // namespace

void InputStreamHelper::SerializeInputStream(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    nsTArray<FileDescriptor>& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed, ContentChild* aManager) {
  SerializeInputStreamInternal(aInputStream, aParams, aFileDescriptors,
                               aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void InputStreamHelper::SerializeInputStream(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    nsTArray<FileDescriptor>& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed, PBackgroundChild* aManager) {
  SerializeInputStreamInternal(aInputStream, aParams, aFileDescriptors,
                               aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void InputStreamHelper::SerializeInputStream(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    nsTArray<FileDescriptor>& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed, ContentParent* aManager) {
  SerializeInputStreamInternal(aInputStream, aParams, aFileDescriptors,
                               aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void InputStreamHelper::SerializeInputStream(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    nsTArray<FileDescriptor>& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed, PBackgroundParent* aManager) {
  SerializeInputStreamInternal(aInputStream, aParams, aFileDescriptors,
                               aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void InputStreamHelper::SerializeInputStreamAsPipe(nsIInputStream* aInputStream,
                                                   InputStreamParams& aParams,
                                                   bool aDelayedStart,
                                                   ContentChild* aManager) {
  SerializeInputStreamAsPipeInternal(aInputStream, aParams, aDelayedStart,
                                     aManager);
}

void InputStreamHelper::SerializeInputStreamAsPipe(nsIInputStream* aInputStream,
                                                   InputStreamParams& aParams,
                                                   bool aDelayedStart,
                                                   PBackgroundChild* aManager) {
  SerializeInputStreamAsPipeInternal(aInputStream, aParams, aDelayedStart,
                                     aManager);
}

void InputStreamHelper::SerializeInputStreamAsPipe(nsIInputStream* aInputStream,
                                                   InputStreamParams& aParams,
                                                   bool aDelayedStart,
                                                   ContentParent* aManager) {
  SerializeInputStreamAsPipeInternal(aInputStream, aParams, aDelayedStart,
                                     aManager);
}

void InputStreamHelper::SerializeInputStreamAsPipe(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    bool aDelayedStart, PBackgroundParent* aManager) {
  SerializeInputStreamAsPipeInternal(aInputStream, aParams, aDelayedStart,
                                     aManager);
}

void InputStreamHelper::PostSerializationActivation(InputStreamParams& aParams,
                                                    bool aConsumedByIPC,
                                                    bool aDelayedStart) {
  switch (aParams.type()) {
    case InputStreamParams::TBufferedInputStreamParams: {
      BufferedInputStreamParams& params =
          aParams.get_BufferedInputStreamParams();
      InputStreamHelper::PostSerializationActivation(
          params.optionalStream(), aConsumedByIPC, aDelayedStart);
      return;
    }

    case InputStreamParams::TMIMEInputStreamParams: {
      MIMEInputStreamParams& params = aParams.get_MIMEInputStreamParams();
      InputStreamHelper::PostSerializationActivation(
          params.optionalStream(), aConsumedByIPC, aDelayedStart);
      return;
    }

    case InputStreamParams::TMultiplexInputStreamParams: {
      MultiplexInputStreamParams& params =
          aParams.get_MultiplexInputStreamParams();
      for (InputStreamParams& subParams : params.streams()) {
        InputStreamHelper::PostSerializationActivation(
            subParams, aConsumedByIPC, aDelayedStart);
      }
      return;
    }

    case InputStreamParams::TSlicedInputStreamParams: {
      SlicedInputStreamParams& params = aParams.get_SlicedInputStreamParams();
      InputStreamHelper::PostSerializationActivation(
          params.stream(), aConsumedByIPC, aDelayedStart);
      return;
    }

    case InputStreamParams::TInputStreamLengthWrapperParams: {
      InputStreamLengthWrapperParams& params =
          aParams.get_InputStreamLengthWrapperParams();
      InputStreamHelper::PostSerializationActivation(
          params.stream(), aConsumedByIPC, aDelayedStart);
      return;
    }

    case InputStreamParams::TIPCRemoteStreamParams: {
      IPCRemoteStreamType& remoteInputStream =
          aParams.get_IPCRemoteStreamParams().stream();

      IPCStreamSource* source = nullptr;
      if (remoteInputStream.type() ==
          IPCRemoteStreamType::TPChildToParentStreamChild) {
        source = IPCStreamSource::Cast(
            remoteInputStream.get_PChildToParentStreamChild());
      } else {
        MOZ_ASSERT(remoteInputStream.type() ==
                   IPCRemoteStreamType::TPParentToChildStreamParent);
        source = IPCStreamSource::Cast(
            remoteInputStream.get_PParentToChildStreamParent());
      }

      MOZ_ASSERT(source);

      // If the source stream has not been taken to be sent to the other side,
      // we can destroy it.
      if (!aConsumedByIPC) {
        source->StartDestroy();
        return;
      }

      if (!aDelayedStart) {
        // If we don't need to do a delayedStart, we start it now. Otherwise,
        // the Start() will be called at the first use by the
        // IPCStreamDestination::DelayedStartInputStream.
        source->Start();
      }

      return;
    }

    case InputStreamParams::TStringInputStreamParams:
      break;

    case InputStreamParams::TFileInputStreamParams:
      break;

    case InputStreamParams::TIPCBlobInputStreamParams:
      break;

    default:
      MOZ_CRASH(
          "A new stream? Should decide if it must be processed recursively or "
          "not.");
  }
}

void InputStreamHelper::PostSerializationActivation(
    Maybe<InputStreamParams>& aParams, bool aConsumedByIPC,
    bool aDelayedStart) {
  if (aParams.isSome()) {
    InputStreamHelper::PostSerializationActivation(
        aParams.ref(), aConsumedByIPC, aDelayedStart);
  }
}

already_AddRefed<nsIInputStream> InputStreamHelper::DeserializeInputStream(
    const InputStreamParams& aParams,
    const nsTArray<FileDescriptor>& aFileDescriptors) {
  // IPCBlobInputStreams are not deserializable on the parent side.
  if (aParams.type() == InputStreamParams::TIPCBlobInputStreamParams) {
    MOZ_ASSERT(XRE_IsParentProcess());

    nsCOMPtr<nsIInputStream> stream;
    IPCBlobInputStreamStorage::Get()->GetStream(
        aParams.get_IPCBlobInputStreamParams().id(),
        aParams.get_IPCBlobInputStreamParams().start(),
        aParams.get_IPCBlobInputStreamParams().length(),
        getter_AddRefs(stream));
    return stream.forget();
  }

  if (aParams.type() == InputStreamParams::TIPCRemoteStreamParams) {
    const IPCRemoteStreamParams& remoteStream =
        aParams.get_IPCRemoteStreamParams();
    const IPCRemoteStreamType& remoteStreamType = remoteStream.stream();
    IPCStreamDestination* destinationStream;

    if (remoteStreamType.type() ==
        IPCRemoteStreamType::TPChildToParentStreamParent) {
      destinationStream = IPCStreamDestination::Cast(
          remoteStreamType.get_PChildToParentStreamParent());
    } else {
      MOZ_ASSERT(remoteStreamType.type() ==
                 IPCRemoteStreamType::TPParentToChildStreamChild);
      destinationStream = IPCStreamDestination::Cast(
          remoteStreamType.get_PParentToChildStreamChild());
    }

    destinationStream->SetDelayedStart(remoteStream.delayedStart());
    destinationStream->SetLength(remoteStream.length());
    return destinationStream->TakeReader();
  }

  nsCOMPtr<nsIIPCSerializableInputStream> serializable;

  switch (aParams.type()) {
    case InputStreamParams::TStringInputStreamParams:
      serializable = do_CreateInstance(kStringInputStreamCID);
      break;

    case InputStreamParams::TFileInputStreamParams:
      serializable = do_CreateInstance(kFileInputStreamCID);
      break;

    case InputStreamParams::TBufferedInputStreamParams:
      serializable = do_CreateInstance(kBufferedInputStreamCID);
      break;

    case InputStreamParams::TMIMEInputStreamParams:
      serializable = do_CreateInstance(kMIMEInputStreamCID);
      break;

    case InputStreamParams::TMultiplexInputStreamParams:
      serializable = do_CreateInstance(kMultiplexInputStreamCID);
      break;

    case InputStreamParams::TSlicedInputStreamParams:
      serializable = new SlicedInputStream();
      break;

    case InputStreamParams::TInputStreamLengthWrapperParams:
      serializable = new InputStreamLengthWrapper();
      break;

    default:
      MOZ_ASSERT(false, "Unknown params!");
      return nullptr;
  }

  MOZ_ASSERT(serializable);

  if (!serializable->Deserialize(aParams, aFileDescriptors)) {
    MOZ_ASSERT(false, "Deserialize failed!");
    return nullptr;
  }

  nsCOMPtr<nsIInputStream> stream = do_QueryInterface(serializable);
  MOZ_ASSERT(stream);

  return stream.forget();
}

}  // namespace ipc
}  // namespace mozilla
