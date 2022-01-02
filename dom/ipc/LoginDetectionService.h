/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LoginDetectionService_h
#define mozilla_dom_LoginDetectionService_h

#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsILoginDetectionService.h"
#include "nsILoginManager.h"
#include "nsWeakReference.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"

namespace mozilla::dom {

/**
 * Detect whether the user is 'possibly' logged in to a site, and add the
 * HighValue permission to the permission manager. We say 'possibly' because
 * the detection is done in a very loose way. For example, for sites that have
 * an associated login stored in the password manager are considered `logged in`
 * by the service, which is not always true in terms of whether the users is
 * really logged in to the site.
 */
class LoginDetectionService final : public nsILoginDetectionService,
                                    public nsILoginSearchCallback,
                                    public nsIObserver,
                                    public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSILOGINDETECTIONSERVICE
  NS_DECL_NSILOGINSEARCHCALLBACK
  NS_DECL_NSIOBSERVER

  static already_AddRefed<LoginDetectionService> GetSingleton();

  void MaybeStartMonitoring();

 private:
  LoginDetectionService();
  virtual ~LoginDetectionService();

  // Fetch saved logins from the password manager.
  void FetchLogins();

  void RegisterObserver();
  void UnregisterObserver();

  nsCOMPtr<nsIObserverService> mObs;

  // Used by testcase to make sure logins are fetched.
  bool mIsLoginsLoaded;
};

}  // namespace mozilla::dom

#endif
