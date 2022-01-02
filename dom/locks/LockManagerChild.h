/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_LOCKS_LOCKMANAGERCHILD_H_
#define DOM_LOCKS_LOCKMANAGERCHILD_H_

#include "mozilla/dom/locks/PLockManagerChild.h"
#include "mozilla/dom/Promise.h"
#include "nsIUUIDGenerator.h"

namespace mozilla::dom::locks {

struct LockRequest;

class LockManagerChild final : public PLockManagerChild {
 public:
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(LockManagerChild)
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(LockManagerChild)

  static void NotifyBFCacheOnMainThread(nsPIDOMWindowInner* aInner,
                                        bool aCreated);

  explicit LockManagerChild(nsIGlobalObject* aOwner) : mOwner(aOwner){};

  nsIGlobalObject* GetParentObject() const { return mOwner; };

  void RequestLock(const LockRequest& aRequest, const LockOptions& aOptions);

  void NotifyRequestDestroy() const;

  void NotifyToWindow(bool aCreated) const;

 private:
  ~LockManagerChild() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
};

}  // namespace mozilla::dom::locks

#endif  // DOM_LOCKS_LOCKMANAGERCHILD_H_
