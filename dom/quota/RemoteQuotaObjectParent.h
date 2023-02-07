/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_REMOTEQUOTAOBJECTPARENT_H_
#define DOM_QUOTA_REMOTEQUOTAOBJECTPARENT_H_

#include "mozilla/dom/quota/PRemoteQuotaObjectParent.h"

namespace mozilla::dom::quota {

class CanonicalQuotaObject;
class RemoteQuotaObjectParentTracker;

class RemoteQuotaObjectParent : public PRemoteQuotaObjectParent {
 public:
  RemoteQuotaObjectParent(RefPtr<CanonicalQuotaObject> aCanonicalQuotaObject,
                          nsCOMPtr<RemoteQuotaObjectParentTracker> aTracker);

  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(RemoteQuotaObjectParent, override)

  mozilla::ipc::IPCResult RecvMaybeUpdateSize(int64_t aSize, bool aTruncate,
                                              bool* aResult);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~RemoteQuotaObjectParent();

  nsresult CheckFileAfterClose();

  RefPtr<CanonicalQuotaObject> mCanonicalQuotaObject;

  nsCOMPtr<RemoteQuotaObjectParentTracker> mTracker;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_REMOTEQUOTAOBJECTPARENT_H_
