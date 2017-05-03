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
#include "prtime.h"

namespace mozilla {
namespace dom {
namespace IPCBlobUtils {

already_AddRefed<BlobImpl>
Deserialize(const IPCBlob& aIPCBlob)
{
  nsCOMPtr<nsIInputStream> inputStream;

  const IPCBlobStream& stream = aIPCBlob.inputStream();
  switch (stream.type()) {

  // Parent to child: when an nsIInputStream is sent from parent to child, the
  // child receives a IPCBlobInputStream actor.
  case IPCBlobStream::TPIPCBlobInputStreamChild: {
    IPCBlobInputStreamChild* actor =
      static_cast<IPCBlobInputStreamChild*>(stream.get_PIPCBlobInputStreamChild());
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

  if (aIPCBlob.file().type() == IPCFileUnion::Tvoid_t) {
    blobImpl = StreamBlobImpl::Create(inputStream,
                                      aIPCBlob.type(),
                                      aIPCBlob.size());
  } else {
    const IPCFile& file = aIPCBlob.file().get_IPCFile();
    blobImpl = StreamBlobImpl::Create(inputStream,
                                      file.name(),
                                      aIPCBlob.type(),
                                      file.lastModified(),
                                      aIPCBlob.size());
    blobImpl->SetDOMPath(file.DOMPath());
    blobImpl->SetFullPath(file.fullPath());
    blobImpl->SetIsDirectory(file.isDirectory());
  }

  return blobImpl.forget();
}

template<typename M>
nsresult
SerializeInputStreamParent(nsIInputStream* aInputStream, uint64_t aSize,
                           IPCBlob& aIPCBlob, M* aManager)
{
  // Parent to Child we always send a IPCBlobInputStream.
  MOZ_ASSERT(XRE_IsParentProcess());

  nsresult rv;
  IPCBlobInputStreamParent* parentActor =
    IPCBlobInputStreamParent::Create(aInputStream, aSize, &rv, aManager);
  if (!parentActor) {
    return rv;
  }

  if (!aManager->SendPIPCBlobInputStreamConstructor(parentActor,
                                                    parentActor->ID(),
                                                    parentActor->Size())) {
    return NS_ERROR_FAILURE;
  }

  aIPCBlob.inputStream() = parentActor;
  return NS_OK;
}

template<typename M>
nsresult
SerializeInputStreamChild(nsIInputStream* aInputStream, IPCBlob& aIPCBlob,
                          M* aManager)
{
  AutoIPCStream ipcStream(true /* delayed start */);
  if (!ipcStream.Serialize(aInputStream, aManager)) {
    return NS_ERROR_FAILURE;
  }

  aIPCBlob.inputStream() = ipcStream.TakeValue();
  return NS_OK;
}

nsresult
SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                     IPCBlob& aIPCBlob, nsIContentParent* aManager)
{
  return SerializeInputStreamParent(aInputStream, aSize, aIPCBlob, aManager);
}

nsresult
SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                     IPCBlob& aIPCBlob, PBackgroundParent* aManager)
{
  return SerializeInputStreamParent(aInputStream, aSize, aIPCBlob, aManager);
}

nsresult
SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                     IPCBlob& aIPCBlob, nsIContentChild* aManager)
{
  return SerializeInputStreamChild(aInputStream, aIPCBlob, aManager);
}

nsresult
SerializeInputStream(nsIInputStream* aInputStream, uint64_t aSize,
                     IPCBlob& aIPCBlob, PBackgroundChild* aManager)
{
  return SerializeInputStreamChild(aInputStream, aIPCBlob, aManager);
}

template<typename M>
nsresult
SerializeInternal(BlobImpl* aBlobImpl, M* aManager, IPCBlob& aIPCBlob)
{
  MOZ_ASSERT(aBlobImpl);

  nsAutoString value;
  aBlobImpl->GetType(value);
  aIPCBlob.type() = value;

  ErrorResult rv;
  aIPCBlob.size() = aBlobImpl->GetSize(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  if (!aBlobImpl->IsFile()) {
    aIPCBlob.file() = void_t();
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

    aIPCBlob.file() = file;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  aBlobImpl->GetInternalStream(getter_AddRefs(inputStream), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  rv = SerializeInputStream(inputStream, aIPCBlob.size(), aIPCBlob, aManager);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  return NS_OK;
}

nsresult
Serialize(BlobImpl* aBlobImpl, nsIContentChild* aManager, IPCBlob& aIPCBlob)
{
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

nsresult
Serialize(BlobImpl* aBlobImpl, PBackgroundChild* aManager, IPCBlob& aIPCBlob)
{
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

nsresult
Serialize(BlobImpl* aBlobImpl, nsIContentParent* aManager, IPCBlob& aIPCBlob)
{
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

nsresult
Serialize(BlobImpl* aBlobImpl, PBackgroundParent* aManager, IPCBlob& aIPCBlob)
{
  return SerializeInternal(aBlobImpl, aManager, aIPCBlob);
}

} // IPCBlobUtils namespace
} // dom namespace
} // mozilla namespace
