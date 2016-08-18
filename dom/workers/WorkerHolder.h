/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerHolder_h
#define mozilla_dom_workers_WorkerHolder_h

#include "mozilla/dom/workers/Workers.h"

BEGIN_WORKERS_NAMESPACE

/**
 * Use this chart to help figure out behavior during each of the closing
 * statuses. Details below.
 *
 * +==============================================================+
 * |                       Closing Statuses                       |
 * +=============+=============+=================+================+
 * |    status   | clear queue | abort execution |  close handler |
 * +=============+=============+=================+================+
 * |   Closing   |     yes     |       no        |   no timeout   |
 * +-------------+-------------+-----------------+----------------+
 * | Terminating |     yes     |       yes       |   no timeout   |
 * +-------------+-------------+-----------------+----------------+
 * |  Canceling  |     yes     |       yes       | short duration |
 * +-------------+-------------+-----------------+----------------+
 * |   Killing   |     yes     |       yes       |   doesn't run  |
 * +-------------+-------------+-----------------+----------------+
 */

#ifdef Status
/* Xlib headers insist on this for some reason... Nuke it because
   it'll override our member name */
#undef Status
#endif
enum Status
{
  // Not yet scheduled.
  Pending = 0,

  // This status means that the close handler has not yet been scheduled.
  Running,

  // Inner script called close() on the worker global scope. Setting this
  // status causes the worker to clear its queue of events but does not abort
  // the currently running script. The close handler is also scheduled with
  // no expiration time.
  Closing,

  // Outer script called terminate() on the worker or the worker object was
  // garbage collected in its outer script. Setting this status causes the
  // worker to abort immediately, clear its queue of events, and schedules the
  // close handler with no expiration time.
  Terminating,

  // Either the user navigated away from the owning page or the owning page fell
  // out of bfcache. Setting this status causes the worker to abort immediately
  // and schedules the close handler with a short expiration time. Since the
  // page has gone away the worker may not post any messages.
  Canceling,

  // The application is shutting down. Setting this status causes the worker to
  // abort immediately and the close handler is never scheduled.
  Killing,

  // The close handler has run and the worker is effectively dead.
  Dead
};

class WorkerHolder
{
public:
  NS_DECL_OWNINGTHREAD

  WorkerHolder();
  virtual ~WorkerHolder();

  bool HoldWorker(WorkerPrivate* aWorkerPrivate, Status aFailStatus);
  void ReleaseWorker();

  virtual bool Notify(Status aStatus) = 0;

protected:
  void ReleaseWorkerInternal();

  WorkerPrivate* MOZ_NON_OWNING_REF mWorkerPrivate;

private:
  void AssertIsOwningThread() const;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_WorkerHolder_h */
