/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addThreadEventListeners } from "./events";
import { prefs, features } from "../../utils/prefs";
import { sameOrigin } from "../../utils/url";
import type { DebuggerClient, Target } from "./types";
import type { ThreadType } from "../../types";

// $FlowIgnore
const { defaultThreadOptions } = require("devtools/client/shared/thread-utils");

type Args = {
  currentTarget: Target,
  debuggerClient: DebuggerClient,
  targets: { [ThreadType]: { [string]: Target } },
  options: Object,
};

async function attachTargets(type, targetLists, args) {
  const { targets } = args;

  for (const actor of Object.keys(targets[type])) {
    if (!targetLists.some(target => target.targetForm.threadActor == actor)) {
      delete targets[type][actor];
    }
  }

  for (const targetFront of targetLists) {
    try {
      await targetFront.attach();

      const threadActorID = targetFront.targetForm.threadActor;
      if (targets[type][threadActorID]) {
        continue;
      }
      targets[type][threadActorID] = targetFront;

      // Content process targets have already been attached by the toolbox.
      // And the thread front has been initialized from there.
      // So we only need to retrieve it here.
      let threadFront = targetFront.threadFront;

      // But workers targets are still only managed by the debugger codebase
      // and so we have to attach their thread actor
      if (!threadFront) {
        [, threadFront] = await targetFront.attachThread({
          ...defaultThreadOptions(),
          ...args.options,
        });
        // NOTE: resume is not necessary for ProcessDescriptors and can be removed
        // once we switch to WorkerDescriptors
        threadFront.resume();
      }

      addThreadEventListeners(threadFront);
    } catch (e) {
      // If any of the workers have terminated since the list command initiated
      // then we will get errors. Ignore these.
    }
  }
}

export async function updateWorkerTargets(type: ThreadType, args: Args) {
  const { currentTarget, debuggerClient } = args;
  if (!currentTarget.isBrowsingContext || currentTarget.isContentProcess) {
    return;
  }

  const { workers } = await currentTarget.listWorkers();

  if (features.windowlessServiceWorkers && currentTarget.url) {
    const { service } = await debuggerClient.mainRoot.listAllWorkers();
    for (const { active, id, url } of service) {
      // Attach to any service workers that are same-origin with our target.
      // For now, ignore service workers associated with cross-origin iframes.
      if (active && sameOrigin(url, currentTarget.url)) {
        const workerTarget = await debuggerClient.mainRoot.getWorker(id);
        workers.push(workerTarget);
      }
    }
  }

  await attachTargets(type, workers, args);
}

export async function updateProcessTargets(type: ThreadType, args: Args) {
  const { currentTarget, debuggerClient } = args;
  if (!prefs.fission || !currentTarget.chrome || currentTarget.isAddon) {
    return;
  }

  const { processes } = await debuggerClient.mainRoot.listProcesses();
  const targets = await Promise.all(
    processes
      .filter(descriptor => !descriptor.isParent)
      .map(descriptor => descriptor.getTarget())
  );

  await attachTargets(type, targets, args);
}

export async function updateTargets(type: ThreadType, args: Args) {
  if (type == "worker") {
    await updateWorkerTargets(type, args);
  } else if (type == "contentProcess") {
    await updateProcessTargets(type, args);
  } else {
    throw new Error(`Unable to fetch targts for ${type}`);
  }
}
