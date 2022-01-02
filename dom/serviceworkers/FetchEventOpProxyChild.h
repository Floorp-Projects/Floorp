/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetcheventopproxychild_h__
#define mozilla_dom_fetcheventopproxychild_h__

#include "nsISupportsImpl.h"

#include "ServiceWorkerOp.h"
#include "ServiceWorkerOpPromise.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/InternalRequest.h"
#include "mozilla/dom/PFetchEventOpProxyChild.h"

namespace mozilla {
namespace dom {

class InternalRequest;
class InternalResponse;
class ParentToChildServiceWorkerFetchEventOpArgs;

class FetchEventOpProxyChild final : public PFetchEventOpProxyChild {
  friend class PFetchEventOpProxyChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FetchEventOpProxyChild, override);

  FetchEventOpProxyChild() = default;

  void Initialize(const ParentToChildServiceWorkerFetchEventOpArgs& aArgs);

  // Must only be called once and on a worker thread.
  SafeRefPtr<InternalRequest> ExtractInternalRequest();

  RefPtr<FetchEventPreloadResponsePromise> GetPreloadResponsePromise();

 private:
  ~FetchEventOpProxyChild() = default;

  mozilla::ipc::IPCResult RecvPreloadResponse(
      ParentToChildInternalResponse&& aResponse);

  void ActorDestroy(ActorDestroyReason) override;

  MozPromiseRequestHolder<FetchEventRespondWithPromise>
      mRespondWithPromiseRequestHolder;

  RefPtr<FetchEventOp> mOp;

  // Initialized on RemoteWorkerService::Thread, read on a worker thread.
  SafeRefPtr<InternalRequest> mInternalRequest;

  RefPtr<FetchEventPreloadResponsePromise::Private> mPreloadResponsePromise;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_fetcheventopproxychild_h__
