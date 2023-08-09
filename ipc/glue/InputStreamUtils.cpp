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
#include "mozilla/ipc/DataPipe.h"
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

void InputStreamHelper::SerializedComplexity(nsIInputStream* aInputStream,
                                             uint32_t aMaxSize,
                                             uint32_t* aSizeUsed,
                                             uint32_t* aPipes,
                                             uint32_t* aTransferables) {
  MOZ_ASSERT(aInputStream);

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
      do_QueryInterface(aInputStream);
  if (!serializable) {
    MOZ_CRASH("Input stream is not serializable!");
  }

  serializable->SerializedComplexity(aMaxSize, aSizeUsed, aPipes,
                                     aTransferables);
}

void InputStreamHelper::SerializeInputStream(nsIInputStream* aInputStream,
                                             InputStreamParams& aParams,
                                             uint32_t aMaxSize,
                                             uint32_t* aSizeUsed) {
  MOZ_ASSERT(aInputStream);

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
      do_QueryInterface(aInputStream);
  if (!serializable) {
    MOZ_CRASH("Input stream is not serializable!");
  }

  serializable->Serialize(aParams, aMaxSize, aSizeUsed);

  if (aParams.type() == InputStreamParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }
}

void InputStreamHelper::SerializeInputStreamAsPipe(nsIInputStream* aInputStream,
                                                   InputStreamParams& aParams) {
  MOZ_ASSERT(aInputStream);

  // Let's try to take the length using InputStreamLengthHelper. If the length
  // cannot be taken synchronously, and its length is needed, the stream needs
  // to be fully copied in memory on the deserialization side.
  int64_t length;
  if (!InputStreamLengthHelper::GetSyncLength(aInputStream, &length)) {
    length = -1;
  }

  RefPtr<DataPipeSender> sender;
  RefPtr<DataPipeReceiver> receiver;
  nsresult rv = NewDataPipe(kDefaultDataPipeCapacity, getter_AddRefs(sender),
                            getter_AddRefs(receiver));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

  rv =
      NS_AsyncCopy(aInputStream, sender, target, NS_ASYNCCOPY_VIA_WRITESEGMENTS,
                   kDefaultDataPipeCapacity, nullptr, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  aParams = DataPipeReceiverStreamParams(WrapNotNull(receiver));
  if (length != -1) {
    aParams = InputStreamLengthWrapperParams(aParams, length, false);
  }
}

already_AddRefed<nsIInputStream> InputStreamHelper::DeserializeInputStream(
    const InputStreamParams& aParams) {
  if (aParams.type() == InputStreamParams::TRemoteLazyInputStreamParams) {
    const RemoteLazyInputStreamParams& params =
        aParams.get_RemoteLazyInputStreamParams();

    // If the RemoteLazyInputStream already has an internal stream, unwrap it.
    // This is required as some code unfortunately depends on the precise
    // topology of received streams, and cannot handle being passed a
    // `RemoteLazyInputStream` in the parent process.
    nsCOMPtr<nsIInputStream> innerStream;
    if (XRE_IsParentProcess() &&
        NS_SUCCEEDED(
            params.stream()->TakeInternalStream(getter_AddRefs(innerStream)))) {
      return innerStream.forget();
    }
    return do_AddRef(params.stream().get());
  }

  if (aParams.type() == InputStreamParams::TDataPipeReceiverStreamParams) {
    const DataPipeReceiverStreamParams& pipeParams =
        aParams.get_DataPipeReceiverStreamParams();
    return do_AddRef(pipeParams.pipe().get());
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
      nsFileInputStream::Create(NS_GET_IID(nsIFileInputStream),
                                getter_AddRefs(stream));
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TBufferedInputStreamParams: {
      nsCOMPtr<nsIBufferedInputStream> stream;
      nsBufferedInputStream::Create(NS_GET_IID(nsIBufferedInputStream),
                                    getter_AddRefs(stream));
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TMIMEInputStreamParams: {
      nsCOMPtr<nsIMIMEInputStream> stream;
      nsMIMEInputStreamConstructor(NS_GET_IID(nsIMIMEInputStream),
                                   getter_AddRefs(stream));
      serializable = do_QueryInterface(stream);
    } break;

    case InputStreamParams::TMultiplexInputStreamParams: {
      nsCOMPtr<nsIMultiplexInputStream> stream;
      nsMultiplexInputStreamConstructor(NS_GET_IID(nsIMultiplexInputStream),
                                        getter_AddRefs(stream));
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

  if (!serializable->Deserialize(aParams)) {
    MOZ_ASSERT(false, "Deserialize failed!");
    return nullptr;
  }

  nsCOMPtr<nsIInputStream> stream = do_QueryInterface(serializable);
  MOZ_ASSERT(stream);

  return stream.forget();
}

}  // namespace ipc
}  // namespace mozilla
