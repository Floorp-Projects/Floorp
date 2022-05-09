/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerStatus_h
#define mozilla_dom_workers_WorkerStatus_h

namespace mozilla::dom {

/**
 * Use this chart to help figure out behavior during each of the closing
 * statuses. Details below.
 *
 * +========================================================+
 * |                     Closing Statuses                   |
 * +=============+=============+=================+==========+
 * |    status   | clear queue | abort execution | notified |
 * +=============+=============+=================+==========+
 * |   Closing   |     yes     |       no        |    no    |
 * +-------------+-------------+-----------------+----------+
 * |  Canceling  |     yes     |       yes       |   yes    |
 * +-------------+-------------+-----------------+----------+
 * |   Killing   |     yes     |       yes       |   yes    |
 * +-------------+-------------+-----------------+----------+
 */

enum WorkerStatus {
  // Not yet scheduled.
  Pending = 0,

  // This status means that the worker is active.
  Running,

  // Inner script called close() on the worker global scope. Setting this
  // status causes the worker to clear its queue of events but does not abort
  // the currently running script. WorkerRef objects are not going to be
  // notified because the behavior of APIs/Components should not change during
  // this status yet.
  Closing,

  // Either the user navigated away from the owning page or the owning page fell
  // out of bfcache. Setting this status causes the worker to abort immediately.
  // Since the page has gone away the worker may not post any messages.
  Canceling,

  // The application is shutting down. Setting this status causes the worker to
  // abort immediately.
  Killing,

  // The worker is effectively dead.
  Dead
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_workers_WorkerStatus_h */
