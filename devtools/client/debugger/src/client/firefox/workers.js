/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addThreadEventListeners } from "./events";
import type { TabTarget } from "./types";

export function supportsWorkers(tabTarget: TabTarget) {
  return tabTarget.isBrowsingContext || tabTarget.isContentProcess;
}

export async function updateWorkerTargets({
  tabTarget,
  debuggerClient,
  threadFront,
  workerTargets,
  options,
}: Object) {
  if (!supportsWorkers(tabTarget)) {
    return {};
  }

  const newWorkerTargets = {};

  const { workers } = await tabTarget.listWorkers();
  for (const workerTargetFront of workers) {
    try {
      await workerTargetFront.attach();
      const threadActorID = workerTargetFront._threadActor;
      if (workerTargets[threadActorID]) {
        newWorkerTargets[threadActorID] = workerTargets[threadActorID];
      } else {
        const [, workerThread] = await workerTargetFront.attachThread(options);
        workerThread.resume();

        addThreadEventListeners(workerThread);

        const consoleFront = await workerTargetFront.getFront("console");
        await consoleFront.startListeners([]);

        newWorkerTargets[workerThread.actor] = workerTargetFront;
      }
    } catch (e) {
      // If any of the workers have terminated since the list command initiated
      // then we will get errors. Ignore these.
    }
  }

  return newWorkerTargets;
}
