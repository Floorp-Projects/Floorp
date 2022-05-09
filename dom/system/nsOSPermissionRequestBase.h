/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsOSPermissionRequestBase_h__
#define nsOSPermissionRequestBase_h__

#include "nsIOSPermissionRequest.h"
#include "nsWeakReference.h"

namespace mozilla::dom {
class Promise;
}  // namespace mozilla::dom

using mozilla::dom::Promise;

/*
 * The base implementation of nsIOSPermissionRequest to be subclassed on
 * platforms that require permission requests for access to resources such
 * as media captures devices. This implementation always returns results
 * indicating access is permitted.
 */
class nsOSPermissionRequestBase : public nsIOSPermissionRequest,
                                  public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOSPERMISSIONREQUEST

  nsOSPermissionRequestBase() = default;

 protected:
  nsresult GetPromise(JSContext* aCx, RefPtr<Promise>& aPromiseOut);
  virtual ~nsOSPermissionRequestBase() = default;
};

#endif
