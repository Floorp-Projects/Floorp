/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as firefox from "./client/firefox";

import { asyncStore, verifyPrefSchema } from "./utils/prefs";
import { setupHelper } from "./utils/dbg";
import { setToolboxTelemetry } from "./utils/telemetry";

import {
  bootstrapApp,
  bootstrapStore,
  bootstrapWorkers,
  unmountRoot,
  teardownWorkers,
} from "./utils/bootstrap";

import { initialBreakpointsState } from "./reducers/breakpoints";
import { initialSourcesState } from "./reducers/sources";
import { initialUIState } from "./reducers/ui";
import { initialSourceBlackBoxState } from "./reducers/source-blackbox";

const { sanitizeBreakpoints } = require("devtools/client/shared/thread-utils");

async function loadInitialState(commands, toolbox) {
  const pendingBreakpoints = sanitizeBreakpoints(
    await asyncStore.pendingBreakpoints
  );
  const tabs = { tabs: await asyncStore.tabs };
  const xhrBreakpoints = await asyncStore.xhrBreakpoints;
  const blackboxedRanges = await asyncStore.blackboxedRanges;
  const eventListenerBreakpoints = await asyncStore.eventListenerBreakpoints;
  const breakpoints = initialBreakpointsState(xhrBreakpoints);
  const sourceBlackBox = initialSourceBlackBoxState({ blackboxedRanges });
  const sources = initialSourcesState();
  const ui = initialUIState();

  return {
    pendingBreakpoints,
    tabs,
    breakpoints,
    eventListenerBreakpoints,
    sources,
    sourceBlackBox,
    ui,
  };
}

export async function bootstrap({
  commands,
  fluentBundles,
  resourceCommand,
  workers: panelWorkers,
  panel,
}) {
  verifyPrefSchema();

  const initialState = await loadInitialState(commands, panel.toolbox);
  const workers = bootstrapWorkers(panelWorkers);

  const { store, actions, selectors } = bootstrapStore(
    firefox.clientCommands,
    workers,
    panel,
    initialState
  );

  const connected = firefox.onConnect(
    commands,
    resourceCommand,
    actions,
    store
  );

  setupHelper({
    store,
    actions,
    selectors,
    workers,
    targetCommand: commands.targetCommand,
    client: firefox.clientCommands,
  });

  setToolboxTelemetry(panel.toolbox.telemetry);

  bootstrapApp(store, panel.getToolboxStore(), {
    fluentBundles,
    toolboxDoc: panel.panelWin.parent.document,
  });
  await connected;
  return { store, actions, selectors, client: firefox.clientCommands };
}

export async function destroy() {
  firefox.onDisconnect();
  unmountRoot();
  teardownWorkers();
}
