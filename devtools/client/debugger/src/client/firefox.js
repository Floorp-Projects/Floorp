/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { setupCommands, clientCommands } from "./firefox/commands";
import { setupEvents, clientEvents } from "./firefox/events";
import { features, prefs } from "../utils/prefs";

export async function onConnect(connection: any, actions: Object) {
  const { debuggerClient, targetList } = connection;
  const currentTarget = targetList.targetFront;
  const threadFront = currentTarget.threadFront;

  if (!currentTarget || !threadFront || !debuggerClient) {
    return;
  }

  setupCommands({
    threadFront,
    currentTarget,
    debuggerClient,
  });

  setupEvents({ threadFront, currentTarget, actions, debuggerClient });

  currentTarget.on("will-navigate", actions.willNavigate);
  currentTarget.on("navigate", actions.navigated);

  const wasmBinarySource =
    features.wasm && !!debuggerClient.mainRoot.traits.wasmBinarySource;

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

  const { traits } = currentTarget;
  await actions.connect(
    currentTarget.url,
    threadFront.actor,
    traits,
    currentTarget.isWebExtension
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

export { clientCommands, clientEvents };
