/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { addThreadEventListeners, attachAllTargets } from "./events";
import { features } from "../../utils/prefs";
import { sameOrigin } from "../../utils/url";
import type { DevToolsClient, TargetList, Target } from "./types";

// $FlowIgnore
const { defaultThreadOptions } = require("devtools/client/shared/thread-utils");

type Args = {
  devToolsClient: DevToolsClient,
  targets: { [string]: Target },
  options: Object,
  targetList: TargetList,
};

/**
 * Ensure attaching to the given Target.
 * The two following actions are done only for workers. For other targets,
 * the toolbox is already doing that.
 *  - Attach the Target
 *  - Attach the Thread = instantiate the Thread actor and register breakpoints.
 *                        Note that, for now, the Toolbox don't have access to breakpoints
 *                        and so, we have to register them somewhere else.
 *                        This might be done via onConnect and syncBreakpoint.
 *
 *  - Register the Thread Actor ID into `targets` arguments, which collect
 *  all Thread Fronts instances keyed by their actor ID.
 *  - Listen to all meaningful RDP event sent by the Thread Actor.
 *
 * @param {Target} targetFront
 *        The target to attach.
 * @param {Object} targets
 *        An object of Target Fronts keyed by their actor ID.
 * @param {Object} options
 *        Thread Actor options to provide to the actor when attaching to worker targets.
 *        Typically includes breakpoints and breaking options.
 */
export async function attachTarget(
  targetFront: Target,
  targets: { [string]: Target },
  options: Object
) {
  try {
    // For all targets but workers, this will already be done by the Toolbox onTargetAvailable function.
    await targetFront.attach();

    const threadActorID = targetFront.targetForm.threadActor;
    if (targets[threadActorID]) {
      return;
    }
    targets[threadActorID] = targetFront;

    // Content process targets have already been attached by the toolbox.
    // And the thread front has been initialized from there.
    // So we only need to retrieve it here.
    let threadFront = targetFront.threadFront;

    // But workers targets are still only managed by the debugger codebase
    // and so we have to attach their thread actor
    if (!threadFront) {
      threadFront = await targetFront.attachThread({
        ...defaultThreadOptions(),
        ...options,
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

/**
 * Process a new list of targets to attach to.
 *
 * @param {Array<Target>} targetLists
 *        List of all targets we should be attached to.
 *        Any targets which is not on this list will be "detached" and stop being used
 *        Any new targets, which wasn't already passed to a previous call to this method
 *        will be attached and start being watched by the Debugger.
 * @param {Args} args
 *        Environment object.
 */
async function attachTargets(targetLists, args): Promise<*> {
  const { targets } = args;

  targetLists = targetLists.filter(target => !!target);

  // First remove from the known list, targets which are no longer in the new list
  for (const actor of Object.keys(targets)) {
    if (!targetLists.some(target => target.targetForm.threadActor == actor)) {
      delete targets[actor];
    }
  }

  // Then attach on all of them. `attachTarget` will be a no-op if it was already
  // attached.
  for (const targetFront of targetLists) {
    await attachTarget(targetFront, targets, args.options);
  }
}

async function listWorkerTargets(args: Args): Promise<*> {
  const { targetList, devToolsClient } = args;
  const currentTarget = targetList.targetFront;
  if (!currentTarget.isBrowsingContext || currentTarget.isContentProcess) {
    return [];
  }

  let workers = [];
  let allWorkers;
  let serviceWorkerRegistrations = [];
  if (attachAllTargets(currentTarget)) {
    workers = await devToolsClient.mainRoot.listAllWorkerTargets();

    // subprocess workers are ignored because they take several seconds to
    // attach to when opening the browser toolbox. See bug 1594597.
    workers = workers.filter(({ url }) => !url.includes("subprocess_worker"));

    allWorkers = workers;

    const {
      registrations,
    } = await devToolsClient.mainRoot.listServiceWorkerRegistrations();
    serviceWorkerRegistrations = registrations;
  } else {
    workers = (await currentTarget.listWorkers()).workers;
    if (currentTarget.url && features.windowlessServiceWorkers) {
      allWorkers = await devToolsClient.mainRoot.listAllWorkerTargets();
      const {
        registrations,
      } = await devToolsClient.mainRoot.listServiceWorkerRegistrations();
      serviceWorkerRegistrations = registrations.filter(front =>
        sameOrigin(front.url, currentTarget.url)
      );
      await pauseMatchingServiceWorkers({ devToolsClient, currentTarget });
    }
  }

  for (const front of serviceWorkerRegistrations) {
    const {
      activeWorker,
      waitingWorker,
      installingWorker,
      evaluatingWorker,
    } = front;

    await maybeMarkServiceWorker(activeWorker);
    await maybeMarkServiceWorker(waitingWorker);
    await maybeMarkServiceWorker(installingWorker);
    await maybeMarkServiceWorker(evaluatingWorker);
  }

  async function maybeMarkServiceWorker(info): Promise<*> {
    if (!info) {
      return;
    }

    const worker = allWorkers.find(front => front && front.id == info.id);
    if (!worker) {
      return;
    }

    worker.debuggerServiceWorkerStatus = info.stateText;
    if (!workers.includes(worker)) {
      workers.push(worker);
    }
  }

  return workers;
}

// Request to all content process to eagerly pause SW matching current page origin
async function pauseMatchingServiceWorkers({ devToolsClient, currentTarget }) {
  // Service workers associated with our target's origin need to pause until
  // we attach, regardless of which process they are running in.
  const origin = new URL(currentTarget.url).origin;
  // Still call `RootFront.listProcesses` instead of using the TargetList
  // as the TargetList doesn't iterate over processes in the content toolbox.
  // We do not care about doing anything with the Content process targets here,
  // the only goal is to call `pauseMatchingServiceWorkers` in all processes.
  // So that the server eagerly freeze SW until we attach to them.
  const processes = await devToolsClient.mainRoot.listProcesses();
  const targets = await Promise.all(
    processes
      .filter(descriptor => !descriptor.isParent)
      .map(descriptor => descriptor.getTarget())
  );
  try {
    await Promise.all(
      targets.map(t => t.pauseMatchingServiceWorkers({ origin }))
    );
  } catch (e) {
    // currentTarget.url might not be a full URL, and old servers without
    // pauseMatchingServiceWorkers will throw.
    // Old servers without pauseMatchingServiceWorkers will throw.
    // @backward-compatibility: remove in Firefox 75
  }
}

async function listProcessTargets(args: Args): Promise<*> {
  const { targetList } = args;
  // First note that the TargetList will only fetch processes following the same
  // rules as `attachAllTargets`. Only if we are attached to the `ParentProcessTarget`
  // and if the browser toolbox fission pref is turned on.
  // Also note that the `ParentProcessTarget` actor is considered to be a FRAME and not a PROCESS.
  // But this is ok, as we expect to return only content processes here.
  return targetList.getAllTargets(targetList.TYPES.PROCESS);
}

export async function updateTargets(args: Args): Promise<*> {
  const currentTopLevelTarget = args.targetList.targetFront;
  const workers = await listWorkerTargets(args);
  const processes = await listProcessTargets(args);
  await attachTargets([currentTopLevelTarget, ...workers, ...processes], args);
}
