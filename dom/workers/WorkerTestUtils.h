/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WorkerTestUtils__
#define mozilla_dom_WorkerTestUtils__

namespace mozilla {

class ErrorResult;

namespace dom {

/**
 * dom/webidl/WorkerTestUtils.webidl defines APIs to expose worker's internal
 * status for glass-box testing. The APIs are only exposed to Workers with prefs
 * dom.workers.testing.enabled.
 *
 * WorkerTestUtils is the implementation of dom/webidl/WorkerTestUtils.webidl
 */
class WorkerTestUtils final {
 public:
  /**
   *  Expose the worker's current timer nesting level.
   *
   *  The worker's current timer nesting level means the executing timer
   *  handler's timer nesting level. When there is no executing timer handler, 0
   *  should be returned by this API. The maximum timer nesting level is 5.
   *
   *  https://html.spec.whatwg.org/#timer-initialisation-steps
   */
  static uint32_t CurrentTimerNestingLevel(const GlobalObject&,
                                           ErrorResult& aErr);
};

}  // namespace dom
}  // namespace mozilla
#endif
