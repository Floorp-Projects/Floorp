/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAMANAGERIMPL_H_
#define DOM_QUOTA_QUOTAMANAGERIMPL_H_

#include "QuotaManager.h"

namespace mozilla::dom::quota {

template <typename F>
void QuotaManager::MaybeRecordQuotaManagerShutdownStepWith(F&& aFunc) {
  // Callable on any thread.

  if (IsShuttingDown()) {
    RecordShutdownStep(Nothing{}, std::forward<F>(aFunc)());
  }
}

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAMANAGERIMPL_H_
