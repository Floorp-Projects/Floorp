/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PermissionObserver_h_
#define mozilla_dom_PermissionObserver_h_

#include "mozilla/dom/PermissionsBinding.h"

#include "nsIObserver.h"
#include "nsIPrincipal.h"
#include "nsTArray.h"
#include "nsWeakReference.h"

namespace mozilla::dom {

class PermissionStatus;

// Singleton that watches for perm-changed notifications in order to notify
// PermissionStatus objects.
class PermissionObserver final : public nsIObserver,
                                 public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static already_AddRefed<PermissionObserver> GetInstance();

  void AddSink(PermissionStatus* aObs);
  void RemoveSink(PermissionStatus* aObs);

 private:
  PermissionObserver();
  virtual ~PermissionObserver();

  nsTArray<PermissionStatus*> mSinks;
};

}  // namespace mozilla::dom

#endif
