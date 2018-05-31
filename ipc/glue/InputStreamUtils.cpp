/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputStreamUtils.h"

#include "nsIIPCSerializableInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/IPCBlobInputStream.h"
#include "mozilla/dom/ipc/IPCBlobInputStreamStorage.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/InputStreamLengthWrapper.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsID.h"
#include "nsIXULRuntime.h"
#include "nsMIMEInputStream.h"
#include "nsMultiplexInputStream.h"
#include "nsNetCID.h"
#include "nsStringStream.h"
#include "nsXULAppAPI.h"

using namespace mozilla::dom;

namespace {

NS_DEFINE_CID(kStringInputStreamCID, NS_STRINGINPUTSTREAM_CID);
NS_DEFINE_CID(kFileInputStreamCID, NS_LOCALFILEINPUTSTREAM_CID);
NS_DEFINE_CID(kBufferedInputStreamCID, NS_BUFFEREDINPUTSTREAM_CID);
NS_DEFINE_CID(kMIMEInputStreamCID, NS_MIMEINPUTSTREAM_CID);
NS_DEFINE_CID(kMultiplexInputStreamCID, NS_MULTIPLEXINPUTSTREAM_CID);

} // namespace

namespace mozilla {
namespace ipc {

void
InputStreamHelper::SerializeInputStream(nsIInputStream* aInputStream,
                                        InputStreamParams& aParams,
                                        nsTArray<FileDescriptor>& aFileDescriptors)
{
  MOZ_ASSERT(aInputStream);

  nsCOMPtr<nsIIPCSerializableInputStream> serializable =
    do_QueryInterface(aInputStream);
  if (!serializable) {
    MOZ_CRASH("Input stream is not serializable!");
  }

  serializable->Serialize(aParams, aFileDescriptors);

  if (aParams.type() == InputStreamParams::T__None) {
    MOZ_CRASH("Serialize failed!");
  }
}

already_AddRefed<nsIInputStream>
InputStreamHelper::DeserializeInputStream(const InputStreamParams& aParams,
                                          const nsTArray<FileDescriptor>& aFileDescriptors)
{
  nsCOMPtr<nsIInputStream> stream;
  nsCOMPtr<nsIIPCSerializableInputStream> serializable;

  // IPCBlobInputStreams are not deserializable on the parent side.
  if (aParams.type() == InputStreamParams::TIPCBlobInputStreamParams) {
    MOZ_ASSERT(XRE_IsParentProcess());
    IPCBlobInputStreamStorage::Get()->GetStream(aParams.get_IPCBlobInputStreamParams().id(),
                                                aParams.get_IPCBlobInputStreamParams().start(),
                                                aParams.get_IPCBlobInputStreamParams().length(),
                                                getter_AddRefs(stream));
    return stream.forget();
  }

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
      serializable = new mozilla::SlicedInputStream();
      break;

    case InputStreamParams::TInputStreamLengthWrapperParams:
      serializable = new mozilla::InputStreamLengthWrapper();
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

  stream = do_QueryInterface(serializable);
  MOZ_ASSERT(stream);

  return stream.forget();
}

} // namespace ipc
} // namespace mozilla
