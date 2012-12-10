/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TaskThrottler_h
#define mozilla_dom_TaskThrottler_h

#include "nsAutoPtr.h"

class CancelableTask;
namespace tracked_objects {
  class Location;
}

namespace mozilla {
namespace layers {

/** The TaskThrottler prevents update event overruns. It is used in cases where
 * you're sending an async message and waiting for a reply. You need to call
 * PostTask to queue a task and TaskComplete when you get a response.
 *
 * The call to TaskComplete will run the recent task posted since the last
 * request was sent, if any. This means that at any time there can be at most 1
 * outstanding request being processed and at most 1 queued behind it.
 *
 * This is used in the context of repainting a scrollable region. While another
 * process is painting you might get several updates from the UI thread but when
 * the paint is complete you want to send the most recent.
 */

class TaskThrottler {
public:
  TaskThrottler();

  /** Post a task to be run as soon as there are no outstanding tasks.
   *
   * @param aLocation Use the macro FROM_HERE
   * @param aTask     Ownership of this object is transferred to TaskThrottler
   *                  which will delete it when it is either run or becomes
   *                  obsolete or the TaskThrottler is destructed.
   */
  void PostTask(const tracked_objects::Location& aLocation,
                CancelableTask* aTask);
  void TaskComplete();

private:
  bool mOutstanding;
  nsAutoPtr<CancelableTask> mQueuedTask;
};

}
}

#endif // mozilla_dom_TaskThrottler_h
