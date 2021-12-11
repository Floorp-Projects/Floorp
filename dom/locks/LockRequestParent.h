/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_LOCKS_LOCKREQUESTPARENT_H_
#define DOM_LOCKS_LOCKREQUESTPARENT_H_

#include "mozilla/dom/locks/PLockManager.h"
#include "mozilla/dom/locks/PLockRequestParent.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::locks {

class LockRequestParent final : public PLockRequestParent {
 public:
  NS_INLINE_DECL_REFCOUNTING(LockRequestParent)

  explicit LockRequestParent(const IPCLockRequest& aRequest)
      : mRequest(aRequest){};

  const IPCLockRequest& Data() { return mRequest; }

  mozilla::ipc::IPCResult Recv__delete__(bool aAborted);

 private:
  ~LockRequestParent() = default;

  IPCLockRequest mRequest;
};

}  // namespace mozilla::dom::locks

#endif  // DOM_LOCKS_LOCKREQUESTPARENT_H_
