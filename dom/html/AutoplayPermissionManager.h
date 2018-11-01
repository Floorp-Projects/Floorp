/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __AutoplayPermissionRequestManager_h__
#define __AutoplayPermissionRequestManager_h__

#include "mozilla/MozPromise.h"
#include "mozilla/WeakPtr.h"
#include "nsIContentPermissionPrompt.h"
#include "nsISupports.h"
#include "nsIWeakReferenceUtils.h"

class nsGlobalWindowInner;
class nsIEventTarget;

namespace mozilla {

// Encapsulates requesting permission from the user to autoplay with a
// doorhanger. The AutoplayPermissionManager is stored on the top level window,
// and all documents in the tab use the top level window's
// AutoplayPermissionManager. The AutoplayPermissionManager ensures that
// multiple requests are merged into one, in order to avoid showing multiple
// doorhangers on one tab at once.
class AutoplayPermissionManager final
  : public SupportsWeakPtr<AutoplayPermissionManager>
{
public:
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(AutoplayPermissionManager)
  NS_INLINE_DECL_REFCOUNTING(AutoplayPermissionManager)

  // Creates a new AutoplayPermissionManager. Don't call this directly, use
  // AutoplayPolicy::RequestFor() to retrieve the appropriate
  // AutoplayPermissionManager to use for a document/window.
  explicit AutoplayPermissionManager(nsGlobalWindowInner* aWindow);

  // Requests permission to autoplay via a user prompt. The returned MozPromise
  // resolves/rejects when the user grants/denies permission to autoplay.
  // If the request doorhanger is dismissed, i.e. tab closed, ESC pressed,
  // etc, the MozPromise is rejected. If there is a stored permission for this
  // window's origin, the parent process will approve/deny the
  // autoplay request using the stored permission immediately (so the MozPromise
  // rejects in the content process after an IPC round trip).
  RefPtr<GenericPromise> RequestWithPrompt();

  // Called by AutoplayPermissionRequest to approve the play request,
  // and resolve the MozPromise returned by RequestWithPrompt().
  void ApprovePlayRequestIfExists();
  // Called by AutoplayPermissionRequest to deny the play request or the inner
  // window is going to be destroyed, this function would reject the MozPromise
  // returned by RequestWithPrompt().
  void DenyPlayRequestIfExists();

private:
  ~AutoplayPermissionManager();

  nsWeakPtr mWindow;
  MozPromiseHolder<GenericPromise> mPromiseHolder;
  // Tracks whether we've dispatched a request to chrome in the parent process
  // to prompt for user permission to autoplay. This flag ensures we don't
  // request multiple times concurrently.
  bool mRequestDispatched = false;
};

} // namespace mozilla

#endif // __AutoplayPermissionRequestManager_h__
