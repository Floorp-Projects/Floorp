/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmptyBody.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(EmptyBody, FetchBody<EmptyBody>)
NS_IMPL_RELEASE_INHERITED(EmptyBody, FetchBody<EmptyBody>)

NS_IMPL_CYCLE_COLLECTION_CLASS(EmptyBody)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(EmptyBody, FetchBody<EmptyBody>)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAbortSignalImpl)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchStreamReader)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(EmptyBody,
                                                  FetchBody<EmptyBody>)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAbortSignalImpl)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchStreamReader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(EmptyBody, FetchBody<EmptyBody>)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EmptyBody)
NS_INTERFACE_MAP_END_INHERITING(FetchBody<EmptyBody>)

EmptyBody::EmptyBody(nsIGlobalObject* aGlobal,
                     mozilla::ipc::PrincipalInfo* aPrincipalInfo,
                     AbortSignalImpl* aAbortSignalImpl,
                     already_AddRefed<nsIInputStream> aBodyStream)
    : FetchBody<EmptyBody>(aGlobal),
      mAbortSignalImpl(aAbortSignalImpl),
      mBodyStream(std::move(aBodyStream)) {
  if (aPrincipalInfo) {
    mPrincipalInfo = MakeUnique<mozilla::ipc::PrincipalInfo>(*aPrincipalInfo);
  }
}

EmptyBody::~EmptyBody() = default;

/* static */
already_AddRefed<EmptyBody> EmptyBody::Create(
    nsIGlobalObject* aGlobal, mozilla::ipc::PrincipalInfo* aPrincipalInfo,
    AbortSignalImpl* aAbortSignalImpl, const nsACString& aMimeType,
    ErrorResult& aRv) {
  nsCOMPtr<nsIInputStream> bodyStream;
  aRv = NS_NewCStringInputStream(getter_AddRefs(bodyStream), EmptyCString());
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<EmptyBody> emptyBody = new EmptyBody(
      aGlobal, aPrincipalInfo, aAbortSignalImpl, bodyStream.forget());
  emptyBody->OverrideMimeType(aMimeType);
  return emptyBody.forget();
}

void EmptyBody::GetBody(nsIInputStream** aStream, int64_t* aBodyLength) {
  MOZ_ASSERT(aStream);

  if (aBodyLength) {
    *aBodyLength = 0;
  }

  nsCOMPtr<nsIInputStream> bodyStream = mBodyStream;
  bodyStream.forget(aStream);
}

}  // namespace dom
}  // namespace mozilla
