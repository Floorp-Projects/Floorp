/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addThreadEventListeners, attachAllTargets } from "./events";
import { features } from "../../utils/prefs";
import { sameOrigin } from "../../utils/url";
import type { DebuggerClient, Target } from "./types";

// $FlowIgnore
const { defaultThreadOptions } = require("devtools/client/shared/thread-utils");

type Args = {
  currentTarget: Target,
  debuggerClient: DebuggerClient,
  targets: { [string]: Target },
  options: Object,
};

async function attachTargets(targetLists, args) {
  const { targets } = args;

  for (const actor of Object.keys(targets)) {
    if (!targetLists.some(target => target.targetForm.threadActor == actor)) {
      delete targets[actor];
    }
  }

  for (const targetFront of targetLists) {
    try {
      await targetFront.attach();

      const threadActorID = targetFront.targetForm.threadActor;
      if (targets[threadActorID]) {
        continue;
      }
      targets[threadActorID] = targetFront;

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

async function listWorkerTargets(args: Args) {
  const { currentTarget, debuggerClient } = args;
  if (!currentTarget.isBrowsingContext || currentTarget.isContentProcess) {
    return [];
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
  } else if (attachAllTargets(currentTarget)) {
    const {
      other,
      service,
      shared,
    } = await debuggerClient.mainRoot.listAllWorkers();

    for (const { workerTargetFront, url } of [...other, ...shared]) {
      // subprocess workers are ignored because they take several seconds to
      // attach to when opening the browser toolbox. See bug 1594597.
      if (!url.includes("subprocess_worker")) {
        workers.push(workerTargetFront);
      }
    }

    for (const { active, id } of service) {
      if (active) {
        const workerTarget = await debuggerClient.mainRoot.getWorker(id);
        workers.push(workerTarget);
      }
    }
  }

  return workers;
}

async function listProcessTargets(args: Args) {
  const { currentTarget, debuggerClient } = args;
  if (!attachAllTargets(currentTarget)) {
    return [];
  }

  const { processes } = await debuggerClient.mainRoot.listProcesses();
  const targets = await Promise.all(
    processes
      .filter(descriptor => !descriptor.isParent)
      .map(descriptor => descriptor.getTarget())
  );

  return targets;
}

export async function updateTargets(args: Args) {
  const workers = await listWorkerTargets(args);
  const processes = await listProcessTargets(args);
  await attachTargets([...workers, ...processes], args);
}
