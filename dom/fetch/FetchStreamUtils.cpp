/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchStreamUtils.h"

#include "mozilla/NotNull.h"
#include "mozilla/RemoteLazyInputStreamChild.h"
#include "mozilla/RemoteLazyInputStreamStorage.h"
#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/IPCBlob.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "nsContentUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla::dom {

namespace {

RefPtr<RemoteLazyInputStreamStorage> GetRemoteLazyInputStreamStorage() {
  auto storageOrErr = RemoteLazyInputStreamStorage::Get();
  MOZ_ASSERT(storageOrErr.isOk());
  return storageOrErr.unwrap();
}

}  // namespace

NotNull<nsCOMPtr<nsIInputStream>> ToInputStream(
    const ParentToParentStream& aStream) {
  MOZ_ASSERT(XRE_IsParentProcess());
  return WrapNotNull(
      GetRemoteLazyInputStreamStorage()->ForgetStream(aStream.uuid()));
}

NotNull<nsCOMPtr<nsIInputStream>> ToInputStream(
    const ParentToChildStream& aStream) {
  MOZ_ASSERT(XRE_IsContentProcess());
  nsCOMPtr<nsIInputStream> result;
  if (aStream.type() == ParentToChildStream::TRemoteLazyInputStream) {
    result = aStream.get_RemoteLazyInputStream();
  } else {
    result = DeserializeIPCStream(aStream.get_IPCStream());
  }
  return WrapNotNull(result);
}

ParentToParentStream ToParentToParentStream(
    const NotNull<nsCOMPtr<nsIInputStream>>& aStream, int64_t aStreamSize) {
  MOZ_ASSERT(XRE_IsParentProcess());

  ParentToParentStream stream;
  stream.uuid() = nsID::GenerateUUID();
  GetRemoteLazyInputStreamStorage()->AddStream(aStream.get(), stream.uuid());
  return stream;
}

ParentToChildStream ToParentToChildStream(
    const NotNull<nsCOMPtr<nsIInputStream>>& aStream, int64_t aStreamSize,
    NotNull<mozilla::ipc::PBackgroundParent*> aBackgroundParent,
    bool aSerializeAsLazy) {
  MOZ_ASSERT(XRE_IsParentProcess());

  ParentToChildStream result;
  if (aSerializeAsLazy) {
    result = RemoteLazyInputStream::WrapStream(aStream.get());
  } else {
    nsCOMPtr<nsIInputStream> stream(aStream.get());
    mozilla::ipc::IPCStream ipcStream;
    Unused << NS_WARN_IF(
        !mozilla::ipc::SerializeIPCStream(stream.forget(), ipcStream, false));
    result = ipcStream;
  }
  return result;
}

ParentToChildStream ToParentToChildStream(
    const ParentToParentStream& aStream, int64_t aStreamSize,
    NotNull<mozilla::ipc::PBackgroundParent*> aBackgroundParent) {
  return ToParentToChildStream(ToInputStream(aStream), aStreamSize,
                               aBackgroundParent);
}

}  // namespace mozilla::dom
