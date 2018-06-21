/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AutoplayRequest_h__
#define __AutoplayRequest_h__

#include "mozilla/MozPromise.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIWeakReferenceUtils.h"

class nsGlobalWindowInner;
class nsIEventTarget;

namespace mozilla {

// Encapsulates requesting permission from the user to autoplay with a
// doorhanger. The AutoplayRequest is stored on the top level window,
// and all documents in the tab use the top level window's AutoplayRequest.
// The AutoplayRequest ensures that multiple requests are merged into one,
// in order to avoid showing multiple doorhangers on one tab at once.
class AutoplayRequest final : public nsIContentPermissionRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST

  // Creates a new AutoplayRequest. Don't call this directly, use
  // AutoplayPolicy::RequestFor() to retrieve the appropriate AutoplayRequest
  // to use for a document/window.
  static already_AddRefed<AutoplayRequest> Create(nsGlobalWindowInner* aWindow);

  // Requests permission to autoplay via a user prompt. Promise
  // resolves/rejects when the user grants/denies permission to autoplay.
  // If there is a stored permission for this window's origin, the stored
  // the parent process will resolve/reject the autoplay request using the
  // stored permission immediately (so the promise rejects in the content
  // process after an IPC round trip).
  RefPtr<GenericPromise> RequestWithPrompt();

private:
  AutoplayRequest(nsGlobalWindowInner* aWindow,
                  nsIPrincipal* aNodePrincipal,
                  nsIEventTarget* aMainThreadTarget);
  ~AutoplayRequest();

  nsWeakPtr mWindow;
  nsCOMPtr<nsIPrincipal> mNodePrincipal;
  nsCOMPtr<nsIEventTarget> mMainThreadTarget;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
  MozPromiseHolder<GenericPromise> mPromiseHolder;
  // Tracks whether we've dispatched a request to chrome in the parent process
  // to prompt for user permission to autoplay. This flag ensures we don't
  // request multiple times concurrently.
  bool mRequestDispatched = false;
};

} // namespace mozilla

#endif // __AutoplayRequest_h__
