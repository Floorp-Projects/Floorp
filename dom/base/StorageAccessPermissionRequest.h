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

  typedef std::function<void()> AllowCallback;
  typedef std::function<void()> AllowAnySiteCallback;
  typedef std::function<void()> CancelCallback;

  static already_AddRefed<StorageAccessPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow, AllowCallback&& aAllowCallback,
      AllowAnySiteCallback&& aAllowAnySiteCallback,
      CancelCallback&& aCancelCallback);

  typedef MozPromise<bool, bool, true> AutoGrantDelayPromise;
  RefPtr<AutoGrantDelayPromise> MaybeDelayAutomaticGrants();

 private:
  StorageAccessPermissionRequest(nsPIDOMWindowInner* aWindow,
                                 nsIPrincipal* aNodePrincipal,
                                 AllowCallback&& aAllowCallback,
                                 AllowAnySiteCallback&& aAllowAnySiteCallback,
                                 CancelCallback&& aCancelCallback);
  ~StorageAccessPermissionRequest();

  unsigned CalculateSimulatedDelay();

  AllowCallback mAllowCallback;
  AllowAnySiteCallback mAllowAnySiteCallback;
  CancelCallback mCancelCallback;
  nsTArray<PermissionRequest> mPermissionRequests;
  bool mCallbackCalled;
};

}  // namespace dom
}  // namespace mozilla

#endif  // StorageAccessPermissionRequest_h_
