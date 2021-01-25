/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setupCommands, clientCommands } from "./firefox/commands";
import {
  setupEvents,
  waitForSourceActorToBeRegisteredInStore,
} from "./firefox/events";
import { createPause, prepareSourcePayload } from "./firefox/create";
import { features, prefs } from "../utils/prefs";

import { recordEvent } from "../utils/telemetry";

let actions;
let targetList;
let resourceWatcher;

export async function onConnect(
  connection: any,
  _actions: Object,
  store: any
): Promise<void> {
  const {
    devToolsClient,
    targetList: _targetList,
    resourceWatcher: _resourceWatcher,
  } = connection;
  actions = _actions;
  targetList = _targetList;
  resourceWatcher = _resourceWatcher;

  setupCommands({ devToolsClient, targetList });
  setupEvents({ actions, devToolsClient, store, resourceWatcher });
  const { targetFront } = targetList;
  if (targetFront.isBrowsingContext || targetFront.isParentProcess) {
    targetList.listenForWorkers = true;
    if (targetFront.localTab && features.windowlessServiceWorkers) {
      targetList.listenForServiceWorkers = true;
      targetList.destroyServiceWorkersOnNavigation = true;
    }
    await targetList.startListening();
  }

  await targetList.watchTargets(
    targetList.ALL_TYPES,
    onTargetAvailable,
    onTargetDestroyed
  );

  // Use independant listeners for SOURCE and BREAKPOINT in order to ease
  // doing batching and notify about a set of SOURCE's in one redux action.
  await resourceWatcher.watchResources([resourceWatcher.TYPES.SOURCE], {
    onAvailable: onSourceAvailable,
  });
  await resourceWatcher.watchResources([resourceWatcher.TYPES.BREAKPOINT], {
    onAvailable: onBreakpointAvailable,
  });
}

export function onDisconnect() {
  targetList.unwatchTargets(
    targetList.ALL_TYPES,
    onTargetAvailable,
    onTargetDestroyed
  );
  resourceWatcher.unwatchResources([resourceWatcher.TYPES.SOURCE], {
    onAvailable: onSourceAvailable,
  });
  resourceWatcher.unwatchResources([resourceWatcher.TYPES.BREAKPOINT], {
    onAvailable: onBreakpointAvailable,
  });
}

async function onTargetAvailable({
  targetFront,
  isTargetSwitching,
}): Promise<void> {
  const isBrowserToolbox = targetList.targetFront.isParentProcess;
  const isNonTopLevelFrameTarget =
    !targetFront.isTopLevel &&
    targetFront.targetType === targetList.TYPES.FRAME;

  if (isBrowserToolbox && isNonTopLevelFrameTarget) {
    // In the BrowserToolbox, non-top-level frame targets are already
    // debugged via content-process targets.
    // Do not attach the thread here, as it was already done by the
    // corresponding content-process target.
    return;
  }

  if (!targetFront.isTopLevel) {
    await actions.addTarget(targetFront);
    return;
  }

  if (isTargetSwitching) {
    // Simulate navigation actions when target switching.
    // The will-navigate event will be missed when using target switching,
    // however `navigate` corresponds more or less to the load event, so it
    // should still be received on the new target.
    actions.willNavigate({ url: targetFront.url });
  }

  // At this point, we expect the target and its thread to be attached.
  const { threadFront } = targetFront;
  if (!threadFront) {
    console.error("The thread for", targetFront, "isn't attached.");
    return;
  }

  targetFront.on("will-navigate", actions.willNavigate);
  targetFront.on("navigate", actions.navigated);

  await threadFront.reconfigure({
    observeAsmJS: true,
    pauseWorkersUntilAttach: true,
    skipBreakpoints: prefs.skipPausing,
    logEventBreakpoints: prefs.logEventBreakpoints,
  });

  // Retrieve possible event listener breakpoints
  actions.getEventListenerBreakpointTypes().catch(e => console.error(e));

  // Initialize the event breakpoints on the thread up front so that
  // they are active once attached.
  actions.addEventListenerBreakpoints([]).catch(e => console.error(e));

  const { traits } = targetFront;
  await actions.connect(
    targetFront.url,
    threadFront.actor,
    traits,
    targetFront.isWebExtension
  );

  await actions.addTarget(targetFront);
}

function onTargetDestroyed({ targetFront }): void {
  if (targetFront.isTopLevel) {
    targetFront.off("will-navigate", actions.willNavigate);
    targetFront.off("navigate", actions.navigated);
  }
  actions.removeTarget(targetFront);
}

async function onSourceAvailable(sources) {
  const frontendSources = await Promise.all(
    sources.map(async source => {
      const threadFront = await source.targetFront.getFront("thread");
      const frontendSource = prepareSourcePayload(threadFront, source);
      return frontendSource;
    })
  );
  await actions.newGeneratedSources(frontendSources);
}

async function onBreakpointAvailable(breakpoints) {
  for (const resource of breakpoints) {
    const threadFront = await resource.targetFront.getFront("thread");
    if (resource.state == "paused") {
      if (resource.frame) {
        // When reloading we might receive a pause event before the
        // top frame's source has arrived.
        await waitForSourceActorToBeRegisteredInStore(
          resource.frame.where.actor
        );
      }

      const pause = createPause(threadFront.actor, resource);
      actions.paused(pause);
      recordEvent("pause", { reason: resource.why.type });
    } else if (resource.state == "resumed") {
      actions.resumed(threadFront.actorID);
    }
  }
}

export { clientCommands };
