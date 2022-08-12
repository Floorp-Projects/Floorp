/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StorageAccessPermissionRequest_h_
#define StorageAccessPermissionRequest_h_

#include "nsContentPermissionHelper.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"

#include <functional>

class nsPIDOMWindowInner;

namespace mozilla::dom {

class StorageAccessPermissionRequest final
    : public ContentPermissionRequestBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(StorageAccessPermissionRequest,
                                           ContentPermissionRequestBase)

  // nsIContentPermissionRequest
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::Handle<JS::Value> choices) override;
  NS_IMETHOD GetTypes(nsIArray** aTypes) override;

  using AllowCallback = std::function<void()>;
  using CancelCallback = std::function<void()>;

  static already_AddRefed<StorageAccessPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow, AllowCallback&& aAllowCallback,
      CancelCallback&& aCancelCallback);

  static already_AddRefed<StorageAccessPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
      AllowCallback&& aAllowCallback, CancelCallback&& aCancelCallback);

  // The argument aTopLevelBaseDomain is used here to optionally indicate what
  // the top-level site of the permission requested will be. This is used in
  // the requestStorageAccessUnderSite call because that call is not made from
  // an embedded context. If aTopLevelBaseDomain is Nothing() the base domain
  // of aPrincipal's Top browsing context is used.
  static already_AddRefed<StorageAccessPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
      const Maybe<nsCString>& aTopLevelBaseDomain,
      AllowCallback&& aAllowCallback, CancelCallback&& aCancelCallback);

  using AutoGrantDelayPromise = MozPromise<bool, bool, true>;
  RefPtr<AutoGrantDelayPromise> MaybeDelayAutomaticGrants();

 private:
  StorageAccessPermissionRequest(nsPIDOMWindowInner* aWindow,
                                 nsIPrincipal* aNodePrincipal,
                                 const Maybe<nsCString>& aTopLevelBaseDomain,
                                 AllowCallback&& aAllowCallback,
                                 CancelCallback&& aCancelCallback);
  ~StorageAccessPermissionRequest() = default;

  unsigned CalculateSimulatedDelay();

  AllowCallback mAllowCallback;
  CancelCallback mCancelCallback;
  nsTArray<nsString> mOptions;
  nsTArray<PermissionRequest> mPermissionRequests;
  bool mCallbackCalled;
};

}  // namespace mozilla::dom

#endif  // StorageAccessPermissionRequest_h_
