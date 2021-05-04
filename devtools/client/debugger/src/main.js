/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as firefox from "./client/firefox";

import { asyncStore, verifyPrefSchema, prefs } from "./utils/prefs";
import { setupHelper } from "./utils/dbg";

import {
  bootstrapApp,
  bootstrapStore,
  bootstrapWorkers,
  unmountRoot,
  teardownWorkers,
} from "./utils/bootstrap";

import { initialBreakpointsState } from "./reducers/breakpoints";
import { initialSourcesState } from "./reducers/sources";

async function syncBreakpoints() {
  const breakpoints = await asyncStore.pendingBreakpoints;
  const breakpointValues = Object.values(breakpoints);
  breakpointValues.forEach(({ disabled, options, generatedLocation }) => {
    if (!disabled) {
      firefox.clientCommands.setBreakpoint(generatedLocation, options);
    }
  });
}

function syncXHRBreakpoints() {
  asyncStore.xhrBreakpoints.then(bps => {
    bps.forEach(({ path, method, disabled }) => {
      if (!disabled) {
        firefox.clientCommands.setXHRBreakpoint(path, method);
      }
    });
  });
}

function setPauseOnExceptions() {
  const { pauseOnExceptions, pauseOnCaughtException } = prefs;
  firefox.clientCommands.pauseOnExceptions(
    pauseOnExceptions,
    pauseOnCaughtException
  );
}

async function loadInitialState() {
  const pendingBreakpoints = await asyncStore.pendingBreakpoints;
  const tabs = { tabs: await asyncStore.tabs };
  const xhrBreakpoints = await asyncStore.xhrBreakpoints;
  const tabsBlackBoxed = await asyncStore.tabsBlackBoxed;
  const eventListenerBreakpoints = await asyncStore.eventListenerBreakpoints;
  const breakpoints = initialBreakpointsState(xhrBreakpoints);
  const sources = initialSourcesState({ tabsBlackBoxed });

  return {
    pendingBreakpoints,
    tabs,
    breakpoints,
    eventListenerBreakpoints,
    sources,
  };
}

export async function bootstrap({
  commands,
  resourceCommand,
  workers: panelWorkers,
  panel,
}) {
  verifyPrefSchema();

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

  await syncBreakpoints();
  syncXHRBreakpoints();
  await setPauseOnExceptions();

  setupHelper({
    store,
    actions,
    selectors,
    workers,
    targetCommand: commands.targetCommand,
    client: firefox.clientCommands,
  });

  bootstrapApp(store, panel);
  await connected;
  return { store, actions, selectors, client: firefox.clientCommands };
}

export async function destroy() {
  firefox.onDisconnect();
  unmountRoot();
  teardownWorkers();
}
