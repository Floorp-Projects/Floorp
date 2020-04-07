/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setupCommands, clientCommands } from "./firefox/commands";
import {
  removeEventsTopTarget,
  setupEvents,
  setupEventsTopTarget,
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
  await targetList.watchTargets(
    targetList.ALL_TYPES,
    onTargetAvailable,
    onTargetDestroyed
  );
}

async function onTargetAvailable({
  targetFront,
  isTopLevel,
  isTargetSwitching,
}): Promise<void> {
  if (!isTopLevel) {
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

  setupEventsTopTarget(targetFront);
  targetFront.on("will-navigate", actions.willNavigate);
  targetFront.on("navigate", actions.navigated);

  const wasmBinarySource =
    features.wasm && !!targetFront.client.mainRoot.traits.wasmBinarySource;

  await threadFront.reconfigure({
    observeAsmJS: true,
    pauseWorkersUntilAttach: true,
    wasmBinarySource,
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

  // Fetch the sources for all the targets
  //
  // In Firefox, we need to initially request all of the sources. This
  // usually fires off individual `newSource` notifications as the
  // debugger finds them, but there may be existing sources already in
  // the debugger (if it's paused already, or if loading the page from
  // bfcache) so explicity fire `newSource` events for all returned
  // sources.
  const sources = await clientCommands.fetchSources();
  await actions.newGeneratedSources(sources);

  await clientCommands.checkIfAlreadyPaused();
}

function onTargetDestroyed({ targetFront, isTopLevel }): void {
  if (isTopLevel) {
    targetFront.off("will-navigate", actions.willNavigate);
    targetFront.off("navigate", actions.navigated);
    removeEventsTopTarget(targetFront);
  }
}

export { clientCommands, clientEvents };
