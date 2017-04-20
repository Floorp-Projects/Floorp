/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPCBlobUtils.h"
#include "mozilla/dom/IPCBlob.h"
#include "prtime.h"

namespace mozilla {
namespace dom {
namespace IPCBlobUtils {

already_AddRefed<BlobImpl>
Deserialize(const IPCBlob& aIPCBlob)
{
  nsCOMPtr<nsIInputStream> inputStream =
    DeserializeIPCStream(aIPCBlob.inputStream());

  RefPtr<BlobImpl> blobImpl;

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
  }

  return blobImpl.forget();
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

    aIPCBlob.file() = file;
  }

  ErrorResult error;
  nsCOMPtr<nsIInputStream> inputStream;
  aBlobImpl->GetInternalStream(getter_AddRefs(inputStream), error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  AutoIPCStream ipcStream(true /* delayStart */);

  if (!ipcStream.Serialize(inputStream, aManager)) {
    return NS_ERROR_FAILURE;
  }

  aIPCBlob.inputStream() = ipcStream.TakeValue();
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
