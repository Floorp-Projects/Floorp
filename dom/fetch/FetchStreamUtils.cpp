/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchStreamUtils.h"

#include "mozilla/NotNull.h"
#include "mozilla/RemoteLazyInputStreamChild.h"
#include "mozilla/RemoteLazyInputStreamStorage.h"
#include "mozilla/dom/FetchTypes.h"
#include "mozilla/dom/IPCBlob.h"
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
  nsCOMPtr<nsIInputStream> result = aStream.stream();
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
    NotNull<mozilla::ipc::PBackgroundParent*> aBackgroundParent) {
  MOZ_ASSERT(XRE_IsParentProcess());

  ParentToChildStream result;
  result.stream() = RemoteLazyInputStream::WrapStream(aStream.get());
  return result;
}

ParentToChildStream ToParentToChildStream(
    const ParentToParentStream& aStream, int64_t aStreamSize,
    NotNull<mozilla::ipc::PBackgroundParent*> aBackgroundParent) {
  return ToParentToChildStream(ToInputStream(aStream), aStreamSize,
                               aBackgroundParent);
}

}  // namespace mozilla::dom
