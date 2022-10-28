/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAOBJECT_H_
#define DOM_QUOTA_QUOTAOBJECT_H_

#include "nsISupportsImpl.h"

namespace mozilla::dom::quota {

class QuotaObject {
 public:
  virtual ~QuotaObject() = default;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual const nsAString& Path() const = 0;

  [[nodiscard]] virtual bool MaybeUpdateSize(int64_t aSize, bool aTruncate) = 0;

  virtual bool IncreaseSize(int64_t aDelta) = 0;

  virtual void DisableQuotaCheck() = 0;

  virtual void EnableQuotaCheck() = 0;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAOBJECT_H_
