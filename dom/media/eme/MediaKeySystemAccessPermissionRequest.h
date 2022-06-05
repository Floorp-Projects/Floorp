/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EME_MEDIAKEYSYSTEMACCESSPERMISSIONREQUEST_H_
#define DOM_MEDIA_EME_MEDIAKEYSYSTEMACCESSPERMISSIONREQUEST_H_

#include "mozilla/MozPromise.h"
#include "nsContentPermissionHelper.h"

class nsGlobalWindowInner;

namespace mozilla::dom {

/**
 * This class encapsulates a permission request to allow media key system
 * access. The intention is not for this class to be used in all cases of EME,
 * but only when we need to seek explicit approval from an application using
 * Gecko, such as an application embedding via GeckoView.
 *
 * media.eme.require-app-approval should be used to gate this functionality in
 * gecko code, and is also used as the testing pref for
 * ContentPermissionRequestBase. I.e. CheckPromptPrefs() will respond to having
 * `media.eme.require-app-approval.prompt.testing` and
 * `media.eme.require-app-approval.prompt.testing.allow` being set to true or
 * false and will return an appropriate value to allow for test code to short
 * circuit showing a prompt. Note that the code using this class needs to call
 * CheckPromptPrefs and implement test specific logic, it is *not* handled by
 * this class or ContentPermissionRequestBase.
 *
 * Expects to be used on main thread as ContentPermissionRequestBase uses
 * PContentPermissionRequest which is managed by PContent which is main thread
 * to main thread communication.
 */
class MediaKeySystemAccessPermissionRequest
    : public ContentPermissionRequestBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      MediaKeySystemAccessPermissionRequest, ContentPermissionRequestBase)

  using RequestPromise = MozPromise<bool, bool, true /* IsExclusive*/>;

  // Create a MediaKeySystemAccessPermissionRequest.
  // @param aWindow The window associated with this request.
  static already_AddRefed<MediaKeySystemAccessPermissionRequest> Create(
      nsPIDOMWindowInner* aWindow);

  // Returns a promise that will be resolved if this request is allowed or
  // rejected in the case the request is denied. If allowed the promise
  // will resolve with true, otherwise it will resolve with false.
  already_AddRefed<RequestPromise> GetPromise();

  // Helper function that triggers the request. This function will check
  // prefs and cancel or allow the request if the appropriate prefs are set,
  // otherwise it will fire the request to the associated window.
  nsresult Start();

  // nsIContentPermissionRequest methods
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::Handle<JS::Value> choices) override;

 private:
  explicit MediaKeySystemAccessPermissionRequest(nsGlobalWindowInner* aWindow);
  ~MediaKeySystemAccessPermissionRequest();

  MozPromiseHolder<RequestPromise> mPromiseHolder;
};

}  // namespace mozilla::dom

#endif  // DOM_MEDIA_EME_MEDIAKEYSYSTEMACCESSPERMISSIONREQUEST_H_
