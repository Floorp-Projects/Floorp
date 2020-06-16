/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setupCommands, clientCommands } from "./firefox/commands";
import {
  removeEventsTopTarget,
  setupEvents,
  clientEvents,
} from "./firefox/events";
import { features, prefs } from "../utils/prefs";

let actions;

export async function onConnect(
  connection: any,
  _actions: Object
): Promise<void> {
  const { devToolsClient, targetList } = connection;
  actions = _actions;

  setupCommands({ devToolsClient, targetList });
  setupEvents({ actions, devToolsClient });
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
}

async function onTargetAvailable({
  targetFront,
  isTargetSwitching,
}): Promise<void> {
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

  // Make sure targetFront.threadFront is availabled and attached.
  await targetFront.onThreadAttached;

  const { threadFront } = targetFront;
  if (!threadFront) {
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

  await clientCommands.checkIfAlreadyPaused();
  await actions.addTarget(targetFront);
}

function onTargetDestroyed({ targetFront }): void {
  if (targetFront.isTopLevel) {
    targetFront.off("will-navigate", actions.willNavigate);
    targetFront.off("navigate", actions.navigated);
    removeEventsTopTarget(targetFront);
  }
  actions.removeTarget(targetFront);
}

export { clientCommands, clientEvents };
