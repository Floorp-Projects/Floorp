/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_REMOTEQUOTAOBJECTPARENTTRACKER_H_
#define DOM_QUOTA_REMOTEQUOTAOBJECTPARENTTRACKER_H_

#include "mozilla/NotNull.h"
#include "nsISupports.h"

#define MOZILLA_DOM_QUOTA_REMOTEQUOTAOBJECTPARENTTRACKER_IID \
  {                                                          \
    0x42f96136, 0x5b2b, 0x4487, {                            \
      0xa4, 0x4e, 0x45, 0x0a, 0x00, 0x8f, 0xc5, 0xd4         \
    }                                                        \
  }

namespace mozilla::dom::quota {

class RemoteQuotaObjectParent;

class RemoteQuotaObjectParentTracker : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(
      NS_DOM_QUOTA_REMOTEQUOTAOBJECTPARENTTRACKER_IID)

  virtual void RegisterRemoteQuotaObjectParent(
      NotNull<RemoteQuotaObjectParent*> aActor) = 0;

  virtual void UnregisterRemoteQuotaObjectParent(
      NotNull<RemoteQuotaObjectParent*> aActor) = 0;

 protected:
  virtual ~RemoteQuotaObjectParentTracker() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(
    RemoteQuotaObjectParentTracker,
    MOZILLA_DOM_QUOTA_REMOTEQUOTAOBJECTPARENTTRACKER_IID)

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_REMOTEQUOTAOBJECTPARENTTRACKER_H_
