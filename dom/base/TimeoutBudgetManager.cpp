/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimeoutBudgetManager.h"

#include "mozilla/dom/Timeout.h"

namespace mozilla::dom {

/* static */ TimeoutBudgetManager& TimeoutBudgetManager::Get() {
  static TimeoutBudgetManager gTimeoutBudgetManager;
  return gTimeoutBudgetManager;
}

void TimeoutBudgetManager::StartRecording(const TimeStamp& aNow) {
  mStart = aNow;
}

void TimeoutBudgetManager::StopRecording() { mStart = TimeStamp(); }

TimeDuration TimeoutBudgetManager::RecordExecution(const TimeStamp& aNow,
                                                   const Timeout* aTimeout) {
  if (!mStart) {
    // If we've started a sync operation mStart might be null, in
    // which case we should not record this piece of execution.
    return TimeDuration();
  }

  return aNow - mStart;
}

}  // namespace mozilla::dom
