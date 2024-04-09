/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WorkerTargetWatcherClass } from "resource://devtools/server/connectors/js-process-actor/target-watchers/worker.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

class ServiceWorkerTargetWatcherClass extends WorkerTargetWatcherClass {
  constructor() {
    super("service_worker");
  }

  /**
   * Called whenever the debugged browser element navigates to a new page
   * and the URL's host changes.
   * This is used to maintain the list of active Service Worker targets
   * based on that host name.
   *
   * @param {Object} watcherDataObject
   *        See ContentProcessWatcherRegistry
   */
  async updateBrowserElementHost(watcherDataObject) {
    const { sessionData } = watcherDataObject;

    // Create target actor matching this new host.
    // Note that we may be navigating to the same host name and the target will already exist.
    const promises = [];
    for (const dbg of lazy.wdm.getWorkerDebuggerEnumerator()) {
      const alreadyCreated = watcherDataObject.workers.some(
        info => info.dbg === dbg
      );
      if (
        this.shouldHandleWorker(sessionData, dbg, "service_worker") &&
        !alreadyCreated
      ) {
        promises.push(this.createWorkerTargetActor(watcherDataObject, dbg));
      }
    }
    await Promise.all(promises);
  }
}

export const ServiceWorkerTargetWatcher = new ServiceWorkerTargetWatcherClass();
