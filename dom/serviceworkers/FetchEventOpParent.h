/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_fetcheventopparent_h__
#define mozilla_dom_fetcheventopparent_h__

#include "nsISupports.h"

#include "mozilla/dom/FetchEventOpProxyParent.h"
#include "mozilla/dom/PFetchEventOpParent.h"

namespace mozilla {
namespace dom {

class FetchEventOpParent final : public PFetchEventOpParent {
  friend class PFetchEventOpParent;

 public:
  NS_INLINE_DECL_REFCOUNTING(FetchEventOpParent)

  FetchEventOpParent() = default;

  // Transition from the Pending state to the Started state. Returns the preload
  // response, if it has already arrived.
  Maybe<ParentToParentResponseWithTiming> OnStart(
      MovingNotNull<RefPtr<FetchEventOpProxyParent>> aFetchEventOpProxyParent);

  // Transition from the Started state to the Finished state.
  void OnFinish();

 private:
  ~FetchEventOpParent() = default;

  // IPDL methods

  mozilla::ipc::IPCResult RecvPreloadResponse(
      ParentToParentResponseWithTiming&& aResponse);

  void ActorDestroy(ActorDestroyReason) override;

  struct Pending {
    Maybe<ParentToParentResponseWithTiming> mPreloadResponse;
  };

  struct Started {
    NotNull<RefPtr<FetchEventOpProxyParent>> mFetchEventOpProxyParent;
  };

  struct Finished {};

  using State = Variant<Pending, Started, Finished>;

  // Tracks the state of the fetch event.
  //
  // Pending:  the fetch event is waiting in RemoteWorkerController::mPendingOps
  //           and if the preload response arrives, we have to save it.
  // Started:  the FetchEventOpProxyParent has been created, and if the preload
  //           response arrives then we should forward it.
  // Finished: the response has been propagated to the parent process, if the
  //           preload response arrives now then we simply drop it.
  State mState = AsVariant(Pending());
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_fetcheventopparent_h__
