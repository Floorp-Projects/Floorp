/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addThreadEventListeners } from "./events";
import { prefs } from "../../utils/prefs";
import type { DebuggerClient, Target } from "./types";
import type { ThreadType } from "../../types";

type Args = {
  currentTarget: Target,
  debuggerClient: DebuggerClient,
  targets: { [ThreadType]: { [string]: Target } },
  options: Object,
};

async function attachTargets(type, targetLists, args) {
  const newTargets = {};
  const targets = args.targets[type] || {};

  for (const targetFront of targetLists) {
    try {
      await targetFront.attach();
      const threadActorID = targetFront.targetForm.threadActor;
      if (targets[threadActorID]) {
        newTargets[threadActorID] = targets[threadActorID];
      } else {
        // Content process targets have already been attached by the toolbox.
        // And the thread front has been initialized from there.
        // So we only need to retrieve it here.
        let threadFront = targetFront.threadFront;

        // But workers targets are still only managed by the debugger codebase
        // and so we have to attach their thread actor
        if (!threadFront) {
          [, threadFront] = await targetFront.attachThread(args.options);
          // NOTE: resume is not necessary for ProcessDescriptors and can be removed
          // once we switch to WorkerDescriptors
          threadFront.resume();
        }

        addThreadEventListeners(threadFront);

        newTargets[threadFront.actor] = targetFront;
      }
    } catch (e) {
      // If any of the workers have terminated since the list command initiated
      // then we will get errors. Ignore these.
    }
  }

  return newTargets;
}

export async function updateWorkerTargets(
  type: ThreadType,
  args: Args
): Promise<{ string: Target }> {
  const { currentTarget } = args;
  if (!currentTarget.isBrowsingContext || currentTarget.isContentProcess) {
    return {};
  }

  const { workers } = await currentTarget.listWorkers();
  return attachTargets(type, workers, args);
}

export async function updateProcessTargets(
  type: ThreadType,
  args: Args
): Promise<{ string: Target }> {
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

  return attachTargets(type, targets, args);
}

export async function updateTargets(type: ThreadType, args: Args) {
  if (type == "worker") {
    return updateWorkerTargets(type, args);
  }

  if (type == "contentProcess") {
    return updateProcessTargets(type, args);
  }

  throw new Error(`Unable to fetch targts for ${type}`);
}
