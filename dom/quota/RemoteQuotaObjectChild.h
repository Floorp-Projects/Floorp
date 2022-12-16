/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_REMOTEQUOTAOBJECTCHILD_H_
#define DOM_QUOTA_REMOTEQUOTAOBJECTCHILD_H_

#include "mozilla/dom/quota/PRemoteQuotaObjectChild.h"

namespace mozilla::dom::quota {

class RemoteQuotaObject;

class RemoteQuotaObjectChild : public PRemoteQuotaObjectChild {
 public:
  RemoteQuotaObjectChild();

  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(RemoteQuotaObjectChild, override)

  void SetRemoteQuotaObject(RemoteQuotaObject* aRemoteQuotaObject);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~RemoteQuotaObjectChild();

  // The weak reference is cleared in ActorDestroy.
  RemoteQuotaObject* MOZ_NON_OWNING_REF mRemoteQuotaObject;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_REMOTEQUOTAOBJECTCHILD_H_
