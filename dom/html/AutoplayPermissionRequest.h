/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AutoplayPermissionRequest_h_
#define AutoplayPermissionRequest_h_

#include "nsGlobalWindowInner.h"
#include "nsISupportsImpl.h"
#include "nsContentPermissionHelper.h"

namespace mozilla {

class AutoplayPermissionManager;

// The AutoplayPermissionRequest is the object we pass off to the chrome JS
// code to represent a request for permission to autoplay. Unfortunately the
// front end code doesn't guarantee to give us an approve/cancel callback in
// all cases. If chrome JS dismisses the permission request for whatever
// reason (tab closed, user presses ESC, navigation, etc), the permission UI
// code will drop its reference to the AutoplayPermissionRequest and it will
// be destroyed. The AutoplayPermissionRequest keeps a weak reference to
// the AutoplayPermissionManager. If the AutoplayPermissionManager is still
// alive when the AutoplayPermissionRequest's destructor runs, the
// AutoplayPermissionRequest's destructor calls the AutoplayPermissionManager
// back with a cancel operation. Thus the AutoplayPermissionManager can
// guarantee to always approve or cancel requests to play.
class AutoplayPermissionRequest final : public nsIContentPermissionRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  static already_AddRefed<AutoplayPermissionRequest> Create(
    nsGlobalWindowInner* aWindow,
    AutoplayPermissionManager* aManager);

private:
  AutoplayPermissionRequest(AutoplayPermissionManager* aManager,
                            nsGlobalWindowInner* aWindow,
                            nsIPrincipal* aNodePrincipal,
                            nsIEventTarget* aMainThreadTarget);
  ~AutoplayPermissionRequest();

  WeakPtr<AutoplayPermissionManager> mManager;

  nsWeakPtr mWindow;
  nsCOMPtr<nsIPrincipal> mNodePrincipal;
  nsCOMPtr<nsIEventTarget> mMainThreadTarget;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

} // namespace mozilla

#endif // AutoplayPermissionRequest_h_
