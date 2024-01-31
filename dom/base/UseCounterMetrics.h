/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef UseCounterMetrics_h_
#define UseCounterMetrics_h_

#include "stdint.h"

namespace mozilla {
enum UseCounter : int16_t;
enum class UseCounterWorker : int16_t;

namespace dom {
enum WorkerKind : uint8_t;

/**
 * Increments the metric associated with the specific use counter.
 *
 * @param aUseCounter - The use counter for the feature that was used.
 * @param aIsPage - Whether we should record to the page or document metric.
 */
void IncrementUseCounter(UseCounter aUseCounter, bool aIsPage);

/**
 * Increments the metric associated with the specific worker use counter.
 *
 * @param aUseCounter - The use counter for the feature that was used.
 * @param aKind - The kind of worker that triggered the use counter.
 */
void IncrementWorkerUseCounter(UseCounterWorker aUseCounter, WorkerKind aKind);

}  // namespace dom
}  // namespace mozilla

#endif  // UseCounterMetrics_h_
