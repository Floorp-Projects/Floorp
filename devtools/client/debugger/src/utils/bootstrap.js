/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { bindActionCreators, combineReducers } from "redux";
import ReactDOM from "react-dom";
const { Provider } = require("react-redux");

import ToolboxProvider from "devtools/client/framework/store-provider";
import { isFirefoxPanel, isDevelopment, isTesting } from "devtools-environment";
import { AppConstants } from "devtools-modules";

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
import { persistTabs } from "../utils/tabs";

import type { Panel } from "../client/firefox/types";

let parser;

function renderPanel(component, store, panel: Panel) {
  const root = document.createElement("div");
  root.className = "launchpad-root theme-body";
  root.style.setProperty("flex", "1");
  const mount = document.querySelector("#mount");
  if (!mount) {
    return;
  }
  mount.appendChild(root);

  const toolboxDoc = panel.panelWin.parent.document;

  ReactDOM.render(
    React.createElement(
      Provider,
      { store },
      React.createElement(
        ToolboxProvider,
        { store: panel.getToolboxStore() },
        React.createElement(component, { toolboxDoc })
      )
    ),
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
): any {
  const debugJsModules = AppConstants.AppConstants.DEBUG_JS_MODULES == "1";
  const createStore = configureStore({
    log: prefs.logging || isTesting(),
    timing: debugJsModules || isDevelopment(),
    makeThunkArgs: (args, state) => {
      return { ...args, client, ...workers, panel };
    },
  });

  const store = createStore(combineReducers(reducers), initialState);
  registerStoreObserver(store, updatePrefs);

  const actions = bindActionCreators(
    require("../actions").default,
    store.dispatch
  );

  return { store, actions, selectors };
}

export function bootstrapWorkers(panelWorkers: Workers): Object {
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

export function teardownWorkers(): void {
  if (!isFirefoxPanel()) {
    // When used in Firefox, the toolbox manages the source map worker.
    stopSourceMapWorker();
  }
  prettyPrint.stop();
  parser.stop();
  search.stop();
}

export function bootstrapApp(store: any, panel: Panel): void {
  if (isFirefoxPanel()) {
    renderPanel(App, store, panel);
  } else {
    const { renderRoot } = require("devtools-launchpad");
    renderRoot(React, ReactDOM, App, store);
  }
}

function registerStoreObserver(store, subscriber) {
  let oldState = store.getState();
  store.subscribe(() => {
    const state = store.getState();
    subscriber(state, oldState);
    oldState = state;
  });
}

function updatePrefs(state: any, oldState: any): void {
  const hasChanged = selector =>
    selector(oldState) && selector(oldState) !== selector(state);

  if (hasChanged(selectors.getPendingBreakpoints)) {
    asyncStore.pendingBreakpoints = selectors.getPendingBreakpoints(state);
  }

  if (
    oldState.eventListenerBreakpoints &&
    oldState.eventListenerBreakpoints !== state.eventListenerBreakpoints
  ) {
    asyncStore.eventListenerBreakpoints = state.eventListenerBreakpoints;
  }

  if (hasChanged(selectors.getTabs)) {
    asyncStore.tabs = persistTabs(selectors.getTabs(state));
  }

  if (hasChanged(selectors.getXHRBreakpoints)) {
    asyncStore.xhrBreakpoints = selectors.getXHRBreakpoints(state);
  }

  if (hasChanged(selectors.getBlackBoxList)) {
    asyncStore.tabsBlackBoxed = selectors.getBlackBoxList(state);
  }
}
