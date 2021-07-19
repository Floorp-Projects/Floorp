/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React from "react";
import { bindActionCreators, combineReducers } from "redux";
import ReactDOM from "react-dom";
const { Provider } = require("react-redux");

import ToolboxProvider from "devtools/client/framework/store-provider";
import flags from "devtools/shared/flags";

const { AppConstants } = require("resource://gre/modules/AppConstants.jsm");

import * as search from "../workers/search";
import * as prettyPrint from "../workers/pretty-print";
import { ParserDispatcher } from "../workers/parser";

import configureStore from "../actions/utils/create-store";
import reducers from "../reducers";
import * as selectors from "../selectors";
import App from "../components/App";
import { asyncStore, prefs } from "./prefs";
import { persistTabs } from "../utils/tabs";
const { sanitizeBreakpoints } = require("devtools/client/shared/thread-utils");

let parser;

export function bootstrapStore(client, workers, panel, initialState) {
  const debugJsModules = AppConstants.DEBUG_JS_MODULES == "1";
  const createStore = configureStore({
    log: prefs.logging || flags.testing,
    timing: debugJsModules,
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

export function bootstrapWorkers(panelWorkers) {
  const workerPath = "resource://devtools/client/debugger/dist";

  prettyPrint.start(`${workerPath}/pretty-print-worker.js`);
  parser = new ParserDispatcher();

  parser.start(`${workerPath}/parser-worker.js`);
  search.start(`${workerPath}/search-worker.js`);
  return { ...panelWorkers, prettyPrint, parser, search };
}

export function teardownWorkers() {
  prettyPrint.stop();
  parser.stop();
  search.stop();
}

export function bootstrapApp(store, panel) {
  const mount = getMountElement();
  if (!mount) {
    return;
  }

  const toolboxDoc = panel.panelWin.parent.document;

  ReactDOM.render(
    React.createElement(
      Provider,
      { store },
      React.createElement(
        ToolboxProvider,
        { store: panel.getToolboxStore() },
        React.createElement(App, { toolboxDoc })
      )
    ),
    mount
  );
}

function getMountElement() {
  return document.querySelector("#mount");
}

// This is the opposite of bootstrapApp
export function unmountRoot() {
  ReactDOM.unmountComponentAtNode(getMountElement());
}

function registerStoreObserver(store, subscriber) {
  let oldState = store.getState();
  store.subscribe(() => {
    const state = store.getState();
    subscriber(state, oldState);
    oldState = state;
  });
}

function updatePrefs(state, oldState) {
  const hasChanged = selector =>
    selector(oldState) && selector(oldState) !== selector(state);

  if (hasChanged(selectors.getPendingBreakpoints)) {
    asyncStore.pendingBreakpoints = sanitizeBreakpoints(
      selectors.getPendingBreakpoints(state)
    );
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
