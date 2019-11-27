/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMSecurityManager_h
#define mozilla_dom_DOMSecurityManager_h

#include "nsIObserver.h"
#include "nsIContentSecurityPolicy.h"

class nsIChannel;

class DOMSecurityManager final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Initialize();

 private:
  DOMSecurityManager() = default;
  ~DOMSecurityManager() = default;

  // Only enforces the frame-ancestor check which needs to happen in
  // the parent because we can only access the window global in the
  // parent. The actual CSP gets parsed and applied in content.
  nsresult ParseCSPAndEnforceFrameAncestorCheck(
      nsIChannel* aChannel, nsIContentSecurityPolicy** aOutCSP);

  // XFO checks are ignored in case CSP frame-ancestors is present,
  void EnforceXFrameOptionsCheck(nsIChannel* aChannel,
                                 nsIContentSecurityPolicy* aCsp);

  static void Shutdown();
};

#endif /* mozilla_dom_DOMSecurityManager_h */
