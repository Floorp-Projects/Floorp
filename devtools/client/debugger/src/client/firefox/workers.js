/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addThreadEventListeners } from "./events";
import type { TabTarget } from "./types";

export function supportsWorkers(tabTarget: TabTarget) {
  return tabTarget.isBrowsingContext || tabTarget.isContentProcess;
}

export async function updateWorkerClients({
  tabTarget,
  debuggerClient,
  threadClient,
  workerClients,
  options
}: Object) {
  if (!supportsWorkers(tabTarget)) {
    return {};
  }

  const newWorkerClients = {};

  const { workers } = await tabTarget.listWorkers();
  for (const workerTargetFront of workers) {
    try {
      await workerTargetFront.attach();
      const [, workerThread] = await workerTargetFront.attachThread(options);

      const actor = workerThread.actor;
      if (workerClients[actor]) {
        if (workerClients[actor].thread != workerThread) {
          console.error(`Multiple clients for actor ID: ${workerThread.actor}`);
        }
        newWorkerClients[actor] = workerClients[actor];
      } else {
        addThreadEventListeners(workerThread);
        workerThread.resume();

        const consoleFront = await workerTargetFront.getFront("console");
        await consoleFront.startListeners([]);

        newWorkerClients[actor] = {
          url: workerTargetFront.url,
          thread: workerThread,
          console: consoleFront
        };
      }
    } catch (e) {
      // If any of the workers have terminated since the list command initiated
      // then we will get errors. Ignore these.
    }
  }

  return newWorkerClients;
}
