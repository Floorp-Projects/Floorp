/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_dom_workers_workerfeature_h__
#define mozilla_dom_workers_workerfeature_h__

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

class WorkerFeature
{
public:
  virtual ~WorkerFeature() { }

  virtual bool Suspend(JSContext* aCx) { return true; }
  virtual bool Resume(JSContext* aCx) { return true; }

  virtual bool Notify(JSContext* aCx, Status aStatus) = 0;
};

END_WORKERS_NAMESPACE

#endif /* mozilla_dom_workers_workerfeature_h__ */
