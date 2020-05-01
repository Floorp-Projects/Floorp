/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobUtils.h"
#include "IPCBlobInputStream.h"
#include "IPCBlobInputStreamChild.h"
#include "IPCBlobInputStreamParent.h"
#include "IPCBlobInputStreamStorage.h"
#include "mozilla/dom/IPCBlob.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "StreamBlobImpl.h"
#include "prtime.h"

namespace mozilla {

using namespace ipc;

namespace dom {
namespace IPCBlobUtils {

already_AddRefed<BlobImpl> Deserialize(const IPCBlob& aIPCBlob) {
  nsCOMPtr<nsIInputStream> inputStream;

  const IPCBlobStream& stream = aIPCBlob.inputStream();
  switch (stream.type()) {
    // Parent to child: when an nsIInputStream is sent from parent to child, the
    // child receives a IPCBlobInputStream actor.
    case IPCBlobStream::TPIPCBlobInputStreamChild: {
      IPCBlobInputStreamChild* actor = static_cast<IPCBlobInputStreamChild*>(
          stream.get_PIPCBlobInputStreamChild());
      inputStream = actor->CreateStream();
      break;
    }

    // Child to Parent: when a blob is created on the content process send it's
    // sent to the parent, we have an IPCStream object.
    case IPCBlobStream::TIPCStream:
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

template <typename M>
nsresult SerializeInputStreamParent(nsIInputStream* aInputStream,
                                    uint64_t aSize, uint64_t aChildID,
                                    PIPCBlobInputStreamParent*& aActorParent,
                                    M* aManager) {
  // Parent to Child we always send a IPCBlobInputStream.
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIInputStream> stream = aInputStream;

  // In case this is a IPCBlobInputStream, we don't want to create a loop:
  // IPCBlobInputStreamParent -> IPCBlobInputStream ->
  // IPCBlobInputStreamParent. Let's use the underlying inputStream instead.
  nsCOMPtr<mozIIPCBlobInputStream> ipcBlobInputStream =
      do_QueryInterface(aInputStream);
  if (ipcBlobInputStream) {
    stream = ipcBlobInputStream->GetInternalStream();
    // If we don't have an underlying stream, it's better to terminate here
    // instead of sending an 'empty' IPCBlobInputStream actor on the other side,
    // unable to be used.
    if (NS_WARN_IF(!stream)) {
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv;
  RefPtr<IPCBlobInputStreamParent> parentActor =
      IPCBlobInputStreamParent::Create(stream, aSize, aChildID, &rv, aManager);
  if (!parentActor) {
    return rv;
  }

  if (!aManager->SendPIPCBlobInputStreamConstructor(
          parentActor, parentActor->ID(), parentActor->Size())) {
    return NS_ERROR_FAILURE;
  }

  aActorParent = parentActor;
  return NS_OK;
}

template <typename M>
nsresult SerializeInputStreamChild(nsIInputStream* aInputStream,
                                   IPCBlobStream& aIPCBlob, M* aManager) {
  AutoIPCStream ipcStream(true /* delayed start */);
  if (!ipcStream.Serialize(aInputStream, aManager)) {
    return NS_ERROR_FAILURE;
  }

  aIPCBlob = ipcStream.TakeValue();
  return NS_OK;
}

nsresult SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                              PIPCBlobInputStreamParent*& aActorParent,
                              ContentParent* aManager) {
  return SerializeInputStreamParent(aInputStream, aSize, aManager->ChildID(),
                                    aActorParent, aManager);
}

nsresult SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                              IPCBlobStream& aIPCBlob,
                              ContentParent* aManager) {
  aIPCBlob = (PIPCBlobInputStreamParent*)nullptr;
  return SerializeInputStreamParent(aInputStream, aSize, aManager->ChildID(),
                                    aIPCBlob.get_PIPCBlobInputStreamParent(),
                                    aManager);
}

nsresult SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                              IPCBlobStream& aIPCBlob,
                              PBackgroundParent* aManager) {
  aIPCBlob = (PIPCBlobInputStreamParent*)nullptr;
  return SerializeInputStreamParent(
      aInputStream, aSize, BackgroundParent::GetChildID(aManager),
      aIPCBlob.get_PIPCBlobInputStreamParent(), aManager);
}

nsresult SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                              IPCBlobStream& aIPCBlob, ContentChild* aManager) {
  return SerializeInputStreamChild(aInputStream, aIPCBlob, aManager);
}

nsresult SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                              IPCBlobStream& aIPCBlob,
                              PBackgroundChild* aManager) {
  return SerializeInputStreamChild(aInputStream, aIPCBlob, aManager);
}

template <typename M>
nsresult SerializeInternal(BlobImpl* aBlobImpl, M* aManager,
                           IPCBlob& aIPCBlob) {
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

  rv = SerializeInputStream(inputStream, aIPCBlob.size(),
                            aIPCBlob.inputStream(), aManager);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  return NS_OK;
}

nsresult Serialize(BlobImpl* aBlobImpl, ContentChild* aManager,
                   IPCBlob& aIPCBlob) {
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

nsresult Serialize(BlobImpl* aBlobImpl, PBackgroundChild* aManager,
                   IPCBlob& aIPCBlob) {
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

nsresult Serialize(BlobImpl* aBlobImpl, ContentParent* aManager,
                   IPCBlob& aIPCBlob) {
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

nsresult Serialize(BlobImpl* aBlobImpl, PBackgroundParent* aManager,
                   IPCBlob& aIPCBlob) {
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

nsresult SerializeUntyped(BlobImpl* aBlobImpl, IProtocol* aActor,
                          IPCBlob& aIPCBlob) {
  // We always want to act on the toplevel protocol.
  IProtocol* manager = aActor;
  while (manager->Manager()) {
    manager = manager->Manager();
  }

  // We always need the toplevel protocol
  switch (manager->GetProtocolId()) {
    case PBackgroundMsgStart:
      if (manager->GetSide() == mozilla::ipc::ParentSide) {
        return SerializeInternal(
            aBlobImpl, static_cast<PBackgroundParent*>(manager), aIPCBlob);
      } else {
        return SerializeInternal(
            aBlobImpl, static_cast<PBackgroundChild*>(manager), aIPCBlob);
      }
    case PContentMsgStart:
      if (manager->GetSide() == mozilla::ipc::ParentSide) {
        return SerializeInternal(
            aBlobImpl, static_cast<ContentParent*>(manager), aIPCBlob);
      } else {
        return SerializeInternal(aBlobImpl, static_cast<ContentChild*>(manager),
                                 aIPCBlob);
      }
    default:
      MOZ_CRASH("Unsupported protocol passed to BlobImpl serialize");
  }
}

}  // namespace IPCBlobUtils
}  // namespace dom

namespace ipc {
void IPDLParamTraits<mozilla::dom::BlobImpl*>::Write(
    IPC::Message* aMsg, IProtocol* aActor, mozilla::dom::BlobImpl* aParam) {
  nsresult rv;
  mozilla::dom::IPCBlob ipcblob;
  if (aParam) {
    rv = mozilla::dom::IPCBlobUtils::SerializeUntyped(aParam, aActor, ipcblob);
  }
  if (!aParam || NS_WARN_IF(NS_FAILED(rv))) {
    WriteIPDLParam(aMsg, aActor, false);
  } else {
    WriteIPDLParam(aMsg, aActor, true);
    WriteIPDLParam(aMsg, aActor, ipcblob);
  }
}

bool IPDLParamTraits<mozilla::dom::BlobImpl*>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    RefPtr<mozilla::dom::BlobImpl>* aResult) {
  *aResult = nullptr;

  bool notnull = false;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &notnull)) {
    return false;
  }
  if (notnull) {
    mozilla::dom::IPCBlob ipcblob;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &ipcblob)) {
      return false;
    }
    *aResult = mozilla::dom::IPCBlobUtils::Deserialize(ipcblob);
  }
  return true;
}
}  // namespace ipc
}  // namespace mozilla
