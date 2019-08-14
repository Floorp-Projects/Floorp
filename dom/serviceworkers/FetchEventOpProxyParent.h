/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetcheventopproxyparent_h__
#define mozilla_dom_fetcheventopproxyparent_h__

#include "mozilla/RefPtr.h"
#include "mozilla/dom/PFetchEventOpProxyParent.h"

namespace mozilla {
namespace dom {

class FetchEventOpParent;
class PRemoteWorkerParent;
class ServiceWorkerFetchEventOpArgs;

/**
 * FetchEventOpProxyParent owns a FetchEventOpParent and is responsible for
 * calling PFetchEventOpParent::Send__delete__.
 */
class FetchEventOpProxyParent final : public PFetchEventOpProxyParent {
  friend class PFetchEventOpProxyParent;

 public:
  static void Create(PRemoteWorkerParent* aManager,
                     const ServiceWorkerFetchEventOpArgs& aArgs,
                     RefPtr<FetchEventOpParent> aReal);

  ~FetchEventOpProxyParent();

 private:
  explicit FetchEventOpProxyParent(RefPtr<FetchEventOpParent>&& aReal);

  mozilla::ipc::IPCResult RecvAsyncLog(const nsCString& aScriptSpec,
                                       const uint32_t& aLineNumber,
                                       const uint32_t& aColumnNumber,
                                       const nsCString& aMessageName,
                                       nsTArray<nsString>&& aParams);

  mozilla::ipc::IPCResult RecvRespondWith(
      const IPCFetchEventRespondWithResult& aResult);

  mozilla::ipc::IPCResult Recv__delete__(
      const ServiceWorkerFetchEventOpResult& aResult) override;

  void ActorDestroy(ActorDestroyReason) override;

  RefPtr<FetchEventOpParent> mReal;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_fetcheventopproxyparent_h__
