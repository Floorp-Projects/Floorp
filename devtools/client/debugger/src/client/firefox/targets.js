/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addThreadEventListeners } from "./events";
import { prefs } from "../../utils/prefs";
import type { DebuggerClient, Target } from "./types";

type Args = {
  currentTarget: Target,
  debuggerClient: DebuggerClient,
  targets: { [string]: Target },
  options: Object,
};

async function attachTargets(targetLists, { options, targets }: Args) {
  const newTargets = {};

  for (const targetFront of targetLists) {
    try {
      await targetFront.attach();
      const threadActorID = targetFront.targetForm.threadActor;
      if (targets[threadActorID]) {
        newTargets[threadActorID] = targets[threadActorID];
      } else {
        const [, threadFront] = await targetFront.attachThread(options);
        // NOTE: resume is not necessary for ProcessDescriptors and can be removed
        // once we switch to WorkerDescriptors
        threadFront.resume();

        addThreadEventListeners(threadFront);

        await targetFront.attachConsole();
        newTargets[threadFront.actor] = targetFront;
      }
    } catch (e) {
      // If any of the workers have terminated since the list command initiated
      // then we will get errors. Ignore these.
    }
  }

  return newTargets;
}

export async function updateWorkerTargets(args: Args) {
  const { currentTarget } = args;
  if (!currentTarget.isBrowsingContext || currentTarget.isContentProcess) {
    return {};
  }

  const { workers } = await currentTarget.listWorkers();
  return attachTargets(workers, args);
}

export async function updateProcessTargets(args: Args): Promise<*> {
  const { currentTarget, debuggerClient } = args;
  if (!prefs.fission || !currentTarget.chrome || currentTarget.isAddon) {
    return Promise.resolve({});
  }

  const { processes } = await debuggerClient.mainRoot.listProcesses();
  const targets = await Promise.all(
    processes
      .filter(descriptor => !descriptor.isParent)
      .map(descriptor => descriptor.getTarget())
  );

  return attachTargets(targets, args);
}

export async function updateTargets(args: Args) {
  const workers = await updateWorkerTargets(args);
  const processes = await updateProcessTargets(args);
  return { ...workers, ...processes };
}
