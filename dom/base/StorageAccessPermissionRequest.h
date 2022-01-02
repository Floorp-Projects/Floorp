/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StorageAccessPermissionRequest_h_
#define StorageAccessPermissionRequest_h_

#include "nsContentPermissionHelper.h"
#include "mozilla/MozPromise.h"

#include <functional>

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class StorageAccessPermissionRequest final
    : public ContentPermissionRequestBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(StorageAccessPermissionRequest,
                                           ContentPermissionRequestBase)

  // nsIContentPermissionRequest
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::HandleValue choices) override;

  using AllowCallback = std::function<void()>;
  using CancelCallback = std::function<void()>;

  static already_AddRefed<StorageAccessPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow, AllowCallback&& aAllowCallback,
      CancelCallback&& aCancelCallback);

  static already_AddRefed<StorageAccessPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
      AllowCallback&& aAllowCallback, CancelCallback&& aCancelCallback);

  using AutoGrantDelayPromise = MozPromise<bool, bool, true>;
  RefPtr<AutoGrantDelayPromise> MaybeDelayAutomaticGrants();

 private:
  StorageAccessPermissionRequest(nsPIDOMWindowInner* aWindow,
                                 nsIPrincipal* aNodePrincipal,
                                 AllowCallback&& aAllowCallback,
                                 CancelCallback&& aCancelCallback);
  ~StorageAccessPermissionRequest() = default;

  unsigned CalculateSimulatedDelay();

  AllowCallback mAllowCallback;
  CancelCallback mCancelCallback;
  nsTArray<PermissionRequest> mPermissionRequests;
  bool mCallbackCalled;
};

}  // namespace dom
}  // namespace mozilla

#endif  // StorageAccessPermissionRequest_h_
