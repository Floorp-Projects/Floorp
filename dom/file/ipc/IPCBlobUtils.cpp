/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobUtils.h"
#include "RemoteLazyInputStream.h"
#include "RemoteLazyInputStreamChild.h"
#include "RemoteLazyInputStreamParent.h"
#include "mozilla/dom/IPCBlob.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "RemoteLazyInputStreamStorage.h"
#include "StreamBlobImpl.h"
#include "prtime.h"

namespace mozilla::dom::IPCBlobUtils {

already_AddRefed<BlobImpl> Deserialize(const IPCBlob& aIPCBlob) {
  nsCOMPtr<nsIInputStream> inputStream;

  const RemoteLazyStream& stream = aIPCBlob.inputStream();
  switch (stream.type()) {
    // Parent to child: when an nsIInputStream is sent from parent to child, the
    // child receives a RemoteLazyInputStream actor.
    case RemoteLazyStream::TRemoteLazyInputStream: {
      inputStream = stream.get_RemoteLazyInputStream();
      break;
    }

    // Child to Parent: when a blob is created on the content process send it's
    // sent to the parent, we have an IPCStream object.
    case RemoteLazyStream::TIPCStream:
      MOZ_ASSERT(XRE_IsParentProcess());
      inputStream = DeserializeIPCStream(stream.get_IPCStream());
      break;

    default:
      MOZ_CRASH("Unknown type.");
      break;
  }

  MOZ_ASSERT(inputStream);

  RefPtr<StreamBlobImpl> blobImpl;

  if (aIPCBlob.file().isNothing()) {
    blobImpl = StreamBlobImpl::Create(inputStream.forget(), aIPCBlob.type(),
                                      aIPCBlob.size(), aIPCBlob.blobImplType());
  } else {
    const IPCFile& file = aIPCBlob.file().ref();
    blobImpl = StreamBlobImpl::Create(inputStream.forget(), file.name(),
                                      aIPCBlob.type(), file.lastModified(),
                                      aIPCBlob.size(), aIPCBlob.blobImplType());
    blobImpl->SetDOMPath(file.DOMPath());
    blobImpl->SetFullPath(file.fullPath());
    blobImpl->SetIsDirectory(file.isDirectory());
  }

  blobImpl->SetFileId(aIPCBlob.fileId());

  return blobImpl.forget();
}

nsresult Serialize(BlobImpl* aBlobImpl, IPCBlob& aIPCBlob) {
  MOZ_ASSERT(aBlobImpl);

  nsAutoString value;
  aBlobImpl->GetType(value);
  aIPCBlob.type() = value;

  aBlobImpl->GetBlobImplType(value);
  aIPCBlob.blobImplType() = value;

  ErrorResult rv;
  aIPCBlob.size() = aBlobImpl->GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  if (!aBlobImpl->IsFile()) {
    aIPCBlob.file() = Nothing();
  } else {
    IPCFile file;

    aBlobImpl->GetName(value);
    file.name() = value;

    file.lastModified() = aBlobImpl->GetLastModified(rv) * PR_USEC_PER_MSEC;
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    aBlobImpl->GetDOMPath(value);
    file.DOMPath() = value;

    aBlobImpl->GetMozFullPathInternal(value, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
    file.fullPath() = value;

    file.isDirectory() = aBlobImpl->IsDirectory();

    aIPCBlob.file() = Some(file);
  }

  aIPCBlob.fileId() = aBlobImpl->GetFileId();

  nsCOMPtr<nsIInputStream> inputStream;
  aBlobImpl->CreateInputStream(getter_AddRefs(inputStream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  if (XRE_IsParentProcess()) {
    RefPtr<RemoteLazyInputStream> stream =
        RemoteLazyInputStream::WrapStream(inputStream);
    if (NS_WARN_IF(!stream)) {
      return NS_ERROR_FAILURE;
    }

    aIPCBlob.inputStream() = stream;
    return NS_OK;
  }

  mozilla::ipc::IPCStream stream;
  if (!mozilla::ipc::SerializeIPCStream(inputStream.forget(), stream,
                                        /* aAllowLazy */ true)) {
    return NS_ERROR_FAILURE;
  }
  aIPCBlob.inputStream() = stream;
  return NS_OK;
}

}  // namespace mozilla::dom::IPCBlobUtils

namespace IPC {

void ParamTraits<mozilla::dom::BlobImpl*>::Write(
    IPC::MessageWriter* aWriter, mozilla::dom::BlobImpl* aParam) {
  nsresult rv;
  mozilla::dom::IPCBlob ipcblob;
  if (aParam) {
    rv = mozilla::dom::IPCBlobUtils::Serialize(aParam, ipcblob);
  }
  if (!aParam || NS_WARN_IF(NS_FAILED(rv))) {
    WriteParam(aWriter, false);
  } else {
    WriteParam(aWriter, true);
    WriteParam(aWriter, ipcblob);
  }
}

bool ParamTraits<mozilla::dom::BlobImpl*>::Read(
    IPC::MessageReader* aReader, RefPtr<mozilla::dom::BlobImpl>* aResult) {
  *aResult = nullptr;

  bool notnull = false;
  if (!ReadParam(aReader, &notnull)) {
    return false;
  }
  if (notnull) {
    mozilla::dom::IPCBlob ipcblob;
    if (!ReadParam(aReader, &ipcblob)) {
      return false;
    }
    *aResult = mozilla::dom::IPCBlobUtils::Deserialize(ipcblob);
  }
  return true;
}

}  // namespace IPC
