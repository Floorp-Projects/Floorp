/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputStreamUtils.h"

#include "nsIIPCSerializableInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/quota/DecryptingInputStream_impl.h"
#include "mozilla/dom/quota/IPCStreamCipherStrategy.h"
#include "mozilla/ipc/IPCStreamDestination.h"
#include "mozilla/ipc/IPCStreamSource.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/RemoteLazyInputStream.h"
#include "mozilla/RemoteLazyInputStreamChild.h"
#include "mozilla/RemoteLazyInputStreamStorage.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/InputStreamLengthWrapper.h"
#include "nsBufferedStreams.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsFileStreams.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsID.h"
#include "nsIMIMEInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsIPipe.h"
#include "nsMIMEInputStream.h"
#include "nsMultiplexInputStream.h"
#include "nsNetCID.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using namespace mozilla::dom;

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

  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(aInputStream);

  // As a fallback, attempt to stream the data across using a IPCStream
  // actor. For blocking streams, create a nonblocking pipe instead,
  bool nonBlocking = false;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(aInputStream->IsNonBlocking(&nonBlocking)));
  if (!nonBlocking || !asyncStream) {
    const uint32_t kBufferSize = 32768;  // matches IPCStream buffer size.
    nsCOMPtr<nsIAsyncOutputStream> sink;
    nsresult rv = NS_NewPipe2(getter_AddRefs(asyncStream), getter_AddRefs(sink),
                              true, false, kBufferSize, UINT32_MAX);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIEventTarget> target =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

    // Since the source stream could be used by others, let's not close it when
    // the copy is done.
    rv = NS_AsyncCopy(aInputStream, sink, target, NS_ASYNCCOPY_VIA_READSEGMENTS,
                      kBufferSize, nullptr, nullptr, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }
  MOZ_DIAGNOSTIC_ASSERT(asyncStream);

  auto* streamSource = IPCStreamSource::Create(asyncStream, aManager);
  if (NS_WARN_IF(!streamSource)) {
    // Failed to create IPCStreamSource, which would cause a failure should we
    // attempt to serialize it later. So abort now.
    return;
  }

  aParams = IPCRemoteStreamParams(aDelayedStart, streamSource, length);
}

}  // namespace

void InputStreamHelper::SerializeInputStream(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    nsTArray<FileDescriptor>& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    ParentToChildStreamActorManager* aManager) {
  SerializeInputStreamInternal(aInputStream, aParams, aFileDescriptors,
                               aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void InputStreamHelper::SerializeInputStream(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    nsTArray<FileDescriptor>& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    ChildToParentStreamActorManager* aManager) {
  SerializeInputStreamInternal(aInputStream, aParams, aFileDescriptors,
                               aDelayedStart, aMaxSize, aSizeUsed, aManager);
}

void InputStreamHelper::SerializeInputStreamAsPipe(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    bool aDelayedStart, ParentToChildStreamActorManager* aManager) {
  SerializeInputStreamAsPipeInternal(aInputStream, aParams, aDelayedStart,
                                     aManager);
}

void InputStreamHelper::SerializeInputStreamAsPipe(
    nsIInputStream* aInputStream, InputStreamParams& aParams,
    bool aDelayedStart, ChildToParentStreamActorManager* aManager) {
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

    case InputStreamParams::TRemoteLazyInputStreamParams:
      break;

    case InputStreamParams::TEncryptedFileInputStreamParams:
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
  if (aParams.type() == InputStreamParams::TRemoteLazyInputStreamParams) {
    const RemoteLazyInputStreamParams& params =
        aParams.get_RemoteLazyInputStreamParams();

    // RemoteLazyInputStreamRefs are not deserializable on the parent side,
    // because the parent is the only one that has a copy of the original stream
    // in the RemoteLazyInputStreamStorage.
    if (params.type() ==
        RemoteLazyInputStreamParams::TRemoteLazyInputStreamRef) {
      MOZ_ASSERT(XRE_IsParentProcess());
      const RemoteLazyInputStreamRef& ref =
          params.get_RemoteLazyInputStreamRef();

      auto storage = RemoteLazyInputStreamStorage::Get().unwrapOr(nullptr);
      MOZ_ASSERT(storage);
      nsCOMPtr<nsIInputStream> stream;
      storage->GetStream(ref.id(), ref.start(), ref.length(),
                         getter_AddRefs(stream));
      return stream.forget();
    }

    // parent -> child serializations receive an RemoteLazyInputStream actor.
    MOZ_ASSERT(params.type() ==
               RemoteLazyInputStreamParams::TPRemoteLazyInputStreamChild);
    RemoteLazyInputStreamChild* actor =
        static_cast<RemoteLazyInputStreamChild*>(
            params.get_PRemoteLazyInputStreamChild());
    nsCOMPtr<nsIInputStream> stream = actor->CreateStream();
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
    case InputStreamParams::TStringInputStreamParams: {
      nsCOMPtr<nsIInputStream> stream;
      NS_NewCStringInputStream(getter_AddRefs(stream), ""_ns);
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TFileInputStreamParams: {
      nsCOMPtr<nsIFileInputStream> stream;
      nsFileInputStream::Create(nullptr, NS_GET_IID(nsIFileInputStream),
                                getter_AddRefs(stream));
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TBufferedInputStreamParams: {
      nsCOMPtr<nsIBufferedInputStream> stream;
      nsBufferedInputStream::Create(nullptr, NS_GET_IID(nsIBufferedInputStream),
                                    getter_AddRefs(stream));
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TMIMEInputStreamParams: {
      nsCOMPtr<nsIMIMEInputStream> stream;
      nsMIMEInputStreamConstructor(nullptr, NS_GET_IID(nsIMIMEInputStream),
                                   getter_AddRefs(stream));
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TMultiplexInputStreamParams: {
      nsCOMPtr<nsIMultiplexInputStream> stream;
      nsMultiplexInputStreamConstructor(
          nullptr, NS_GET_IID(nsIMultiplexInputStream), getter_AddRefs(stream));
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TSlicedInputStreamParams:
      serializable = new SlicedInputStream();
      break;

    case InputStreamParams::TInputStreamLengthWrapperParams:
      serializable = new InputStreamLengthWrapper();
      break;

    case InputStreamParams::TEncryptedFileInputStreamParams:
      serializable = new dom::quota::DecryptingInputStream<
          dom::quota::IPCStreamCipherStrategy>();
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
