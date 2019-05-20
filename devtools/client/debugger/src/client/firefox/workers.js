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
  options,
}: Object) {
  if (!supportsWorkers(tabTarget)) {
    return {};
  }

  const newWorkerClients = {};

  const { workers } = await tabTarget.listWorkers();
  for (const workerTargetFront of workers) {
    try {
      await workerTargetFront.attach();
      const threadActorID = workerTargetFront._threadActor;
      if (workerClients[threadActorID]) {
        newWorkerClients[threadActorID] = workerClients[threadActorID];
      } else {
        const [, workerThread] = await workerTargetFront.attachThread(options);
        workerThread.resume();

        addThreadEventListeners(workerThread);

        const consoleFront = await workerTargetFront.getFront("console");
        await consoleFront.startListeners([]);

        newWorkerClients[workerThread.actor] = {
          url: workerTargetFront.url,
          thread: workerThread,
          console: consoleFront,
        };
      }
    } catch (e) {
      // If any of the workers have terminated since the list command initiated
      // then we will get errors. Ignore these.
    }
  }

  return newWorkerClients;
}
