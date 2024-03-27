/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as firefox from "./client/firefox";

import { asyncStore, verifyPrefSchema, prefs } from "./utils/prefs";
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

const {
  sanitizeBreakpoints,
} = require("resource://devtools/client/shared/thread-utils.js");

async function syncXHRBreakpoints() {
  const breakpoints = await asyncStore.xhrBreakpoints;
  return Promise.all(
    breakpoints.map(({ path, method, disabled }) => {
      if (!disabled) {
        firefox.clientCommands.setXHRBreakpoint(path, method);
      }
    })
  );
}

function setPauseOnDebuggerStatement() {
  const { pauseOnDebuggerStatement } = prefs;
  return firefox.clientCommands.pauseOnDebuggerStatement(
    pauseOnDebuggerStatement
  );
}

function setPauseOnExceptions() {
  const { pauseOnExceptions, pauseOnCaughtException } = prefs;
  return firefox.clientCommands.pauseOnExceptions(
    pauseOnExceptions,
    pauseOnCaughtException
  );
}

async function loadInitialState() {
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

  // Set telemetry at the very beginning as some actions fired from this function might
  // record events.
  setToolboxTelemetry(panel.toolbox.telemetry);

  const initialState = await loadInitialState();
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

  await syncXHRBreakpoints();
  await setPauseOnDebuggerStatement();
  await setPauseOnExceptions();

  setupHelper({
    store,
    actions,
    selectors,
    workers,
    targetCommand: commands.targetCommand,
    client: firefox.clientCommands,
  });

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
