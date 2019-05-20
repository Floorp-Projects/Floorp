/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import * as firefox from "./firefox";
import * as chrome from "./chrome";

import { asyncStore, verifyPrefSchema } from "../utils/prefs";
import { setupHelper } from "../utils/dbg";

import {
  bootstrapApp,
  bootstrapStore,
  bootstrapWorkers,
} from "../utils/bootstrap";
import { initialBreakpointsState } from "../reducers/breakpoints";

import type { Panel } from "./firefox/types";

async function syncBreakpoints() {
  const breakpoints = await asyncStore.pendingBreakpoints;
  const breakpointValues = (Object.values(breakpoints): any);
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

async function loadInitialState() {
  const pendingBreakpoints = await asyncStore.pendingBreakpoints;
  const tabs = await asyncStore.tabs;
  const xhrBreakpoints = await asyncStore.xhrBreakpoints;
  const eventListenerBreakpoints = await asyncStore.eventListenerBreakpoints;

  const breakpoints = initialBreakpointsState(xhrBreakpoints);

  return { pendingBreakpoints, tabs, breakpoints, eventListenerBreakpoints };
}

function getClient(connection: any) {
  const {
    tab: { clientType },
  } = connection;
  return clientType == "firefox" ? firefox : chrome;
}

export async function onConnect(
  connection: Object,
  panelWorkers: Object,
  panel: Panel
) {
  // NOTE: the landing page does not connect to a JS process
  if (!connection) {
    return;
  }

  verifyPrefSchema();

  const client = getClient(connection);
  const commands = client.clientCommands;

  const initialState = await loadInitialState();
  const workers = bootstrapWorkers(panelWorkers);

  const { store, actions, selectors } = bootstrapStore(
    commands,
    workers,
    panel,
    initialState
  );

  await client.onConnect(connection, actions);

  await syncBreakpoints();
  syncXHRBreakpoints();
  setupHelper({
    store,
    actions,
    selectors,
    workers,
    connection,
    client: client.clientCommands,
  });

  bootstrapApp(store);
  return { store, actions, selectors, client: commands };
}
