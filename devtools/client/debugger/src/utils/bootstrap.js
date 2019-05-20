/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { bindActionCreators, combineReducers } from "redux";
import ReactDOM from "react-dom";
const { Provider } = require("react-redux");

import { isFirefoxPanel, isDevelopment, isTesting } from "devtools-environment";
import SourceMaps, {
  startSourceMapWorker,
  stopSourceMapWorker,
} from "devtools-source-map";
import * as search from "../workers/search";
import * as prettyPrint from "../workers/pretty-print";
import { ParserDispatcher } from "../workers/parser";

import configureStore from "../actions/utils/create-store";
import reducers from "../reducers";
import * as selectors from "../selectors";
import App from "../components/App";
import { asyncStore, prefs } from "./prefs";

import type { Panel } from "../client/firefox/types";

let parser;

function renderPanel(component, store) {
  const root = document.createElement("div");
  root.className = "launchpad-root theme-body";
  root.style.setProperty("flex", "1");
  const mount = document.querySelector("#mount");
  if (!mount) {
    return;
  }
  mount.appendChild(root);

  ReactDOM.render(
    React.createElement(Provider, { store }, React.createElement(component)),
    root
  );
}

type Workers = {
  sourceMaps: typeof SourceMaps,
  evaluationsParser: typeof ParserDispatcher,
};

export function bootstrapStore(
  client: any,
  workers: Workers,
  panel: Panel,
  initialState: Object
) {
  const createStore = configureStore({
    log: prefs.logging || isTesting(),
    timing: isDevelopment(),
    makeThunkArgs: (args, state) => {
      return { ...args, client, ...workers, panel };
    },
  });

  const store = createStore(combineReducers(reducers), initialState);
  store.subscribe(() => updatePrefs(store.getState()));

  const actions = bindActionCreators(
    require("../actions").default,
    store.dispatch
  );

  return { store, actions, selectors };
}

export function bootstrapWorkers(panelWorkers: Workers) {
  const workerPath = isDevelopment()
    ? "assets/build"
    : "resource://devtools/client/debugger/dist";

  if (isDevelopment()) {
    // When used in Firefox, the toolbox manages the source map worker.
    startSourceMapWorker(
      `${workerPath}/source-map-worker.js`,
      // This is relative to the worker itself.
      "./source-map-worker-assets/"
    );
  }

  prettyPrint.start(`${workerPath}/pretty-print-worker.js`);
  parser = new ParserDispatcher();

  parser.start(`${workerPath}/parser-worker.js`);
  search.start(`${workerPath}/search-worker.js`);
  return { ...panelWorkers, prettyPrint, parser, search };
}

export function teardownWorkers() {
  if (!isFirefoxPanel()) {
    // When used in Firefox, the toolbox manages the source map worker.
    stopSourceMapWorker();
  }
  prettyPrint.stop();
  parser.stop();
  search.stop();
}

export function bootstrapApp(store: any) {
  if (isFirefoxPanel()) {
    renderPanel(App, store);
  } else {
    const { renderRoot } = require("devtools-launchpad");
    renderRoot(React, ReactDOM, App, store);
  }
}

let currentPendingBreakpoints;
let currentXHRBreakpoints;
function updatePrefs(state: any) {
  const previousPendingBreakpoints = currentPendingBreakpoints;
  const previousXHRBreakpoints = currentXHRBreakpoints;
  currentPendingBreakpoints = selectors.getPendingBreakpoints(state);
  currentXHRBreakpoints = selectors.getXHRBreakpoints(state);

  if (
    previousPendingBreakpoints &&
    currentPendingBreakpoints !== previousPendingBreakpoints
  ) {
    asyncStore.pendingBreakpoints = currentPendingBreakpoints;
  }

  if (currentXHRBreakpoints !== previousXHRBreakpoints) {
    asyncStore.xhrBreakpoints = currentXHRBreakpoints;
  }
}
