/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerTestUtils.h"

namespace mozilla::dom {

uint32_t WorkerTestUtils::CurrentTimerNestingLevel(const GlobalObject& aGlobal,
                                                   ErrorResult& aErr) {
  MOZ_ASSERT(!NS_IsMainThread());
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  return worker->GetCurrentTimerNestingLevel();
}

}  // namespace mozilla::dom
