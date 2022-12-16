/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_REMOTEQUOTAOBJECT_H_
#define DOM_QUOTA_REMOTEQUOTAOBJECT_H_

#include "mozilla/dom/quota/QuotaObject.h"

namespace mozilla::dom::quota {

class RemoteQuotaObjectChild;

// This object can only be used on the thread which it was created on
class RemoteQuotaObject final : public QuotaObject {
 public:
  explicit RemoteQuotaObject(RefPtr<RemoteQuotaObjectChild> aActor);

  NS_INLINE_DECL_REFCOUNTING_ONEVENTTARGET(RemoteQuotaObject, override)

  void ClearActor();

  void Close();

  const nsAString& Path() const override;

  // This method should never be called on the main thread or the PBackground
  // thread or a DOM worker thread (It does sync IPC).
  [[nodiscard]] bool MaybeUpdateSize(int64_t aSize, bool aTruncate) override;

  bool IncreaseSize(int64_t aDelta) override { return false; }

  void DisableQuotaCheck() override {}

  void EnableQuotaCheck() override {}

 private:
  ~RemoteQuotaObject();

  RefPtr<RemoteQuotaObjectChild> mActor;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_REMOTEQUOTAOBJECT_H_
