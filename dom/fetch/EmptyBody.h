/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EmptyBody_h
#define mozilla_dom_EmptyBody_h

#include "nsISupportsImpl.h"

#include "mozilla/dom/Fetch.h"

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

namespace dom {

class EmptyBody final : public FetchBody<EmptyBody> {
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(EmptyBody,
                                                         FetchBody<EmptyBody>)

 public:
  static already_AddRefed<EmptyBody> Create(
      nsIGlobalObject* aGlobal, mozilla::ipc::PrincipalInfo* aPrincipalInfo,
      AbortSignalImpl* aAbortSignalImpl, const nsACString& aMimeType,
      ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const { return mOwner; }

  AbortSignalImpl* GetSignalImpl() const override { return mAbortSignalImpl; }

  const UniquePtr<mozilla::ipc::PrincipalInfo>& GetPrincipalInfo() const {
    return mPrincipalInfo;
  }

  void GetBody(nsIInputStream** aStream, int64_t* aBodyLength = nullptr);

  using FetchBody::BodyBlobURISpec;

  const nsACString& BodyBlobURISpec() const { return EmptyCString(); }

  using FetchBody::BodyLocalPath;

  const nsAString& BodyLocalPath() const { return EmptyString(); }

 private:
  EmptyBody(nsIGlobalObject* aGlobal,
            mozilla::ipc::PrincipalInfo* aPrincipalInfo,
            AbortSignalImpl* aAbortSignalImpl,
            already_AddRefed<nsIInputStream> mBodyStream);

  ~EmptyBody();

  UniquePtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;
  RefPtr<AbortSignalImpl> mAbortSignalImpl;
  nsCOMPtr<nsIInputStream> mBodyStream;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EmptyBody_h
