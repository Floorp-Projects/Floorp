/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_SHARED_MANAGEDMOZPROMISEREQUESTHOLDER_H_
#define DOM_FS_SHARED_MANAGEDMOZPROMISEREQUESTHOLDER_H_

#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::fs {

template <typename Manager, typename PromiseType>
class ManagedMozPromiseRequestHolder final
    : public MozPromiseRequestHolder<PromiseType> {
 public:
  explicit ManagedMozPromiseRequestHolder(Manager* aManager)
      : mManager(aManager) {
    mManager->RegisterPromiseRequestHolder(this);
  }

  NS_INLINE_DECL_REFCOUNTING(ManagedMozPromiseRequestHolder)

 private:
  ~ManagedMozPromiseRequestHolder() {
    mManager->UnregisterPromiseRequestHolder(this);
  }

  RefPtr<Manager> mManager;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_SHARED_MANAGEDMOZPROMISEREQUESTHOLDER_H_
