/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Utils for Jest
 * @module utils/test-head
 */

import { combineReducers } from "redux";
import reducers from "../reducers";
import actions from "../actions";
import * as selectors from "../selectors";
import {
  searchWorker,
  prettyPrintWorker,
  parserWorker,
} from "../test/tests-setup";
import configureStore from "../actions/utils/create-store";
import sourceQueue from "../utils/source-queue";
import { setupCreate } from "../client/firefox/create";
import { createLocation } from "./location";

// Import the internal module used by the source-map worker
// as node doesn't have Web Worker support and require path mapping
// doesn't work from nodejs worker thread and break mappings to devtools/ folder.
import sourceMapLoader from "devtools/client/shared/source-map-loader/source-map";

/**
 * This file contains older interfaces used by tests that have not been
 * converted to use test-mockup.js
 */

/**
 * @memberof utils/test-head
 * @static
 */
function createStore(client, initialState = {}, sourceMapLoaderMock) {
  const store = configureStore({
    log: false,
    makeThunkArgs: args => {
      return {
        ...args,
        client,
        sourceMapLoader:
          sourceMapLoaderMock !== undefined
            ? sourceMapLoaderMock
            : sourceMapLoader,
        parserWorker,
        prettyPrintWorker,
        searchWorker,
      };
    },
  })(combineReducers(reducers), initialState);
  sourceQueue.clear();
  sourceQueue.initialize({
    newOriginalSources: sources =>
      store.dispatch(actions.newOriginalSources(sources)),
  });

  store.thunkArgs = () => ({
    dispatch: store.dispatch,
    getState: store.getState,
    client,
    sourceMapLoader,
    panel: {},
  });

  // Put the initial context in the store, for convenience to unit tests.
  store.cx = selectors.getThreadContext(store.getState());

  setupCreate({ store });

  return store;
}

/**
 * @memberof utils/test-head
 * @static
 */
function commonLog(msg, data = {}) {
  console.log(`[INFO] ${msg} ${JSON.stringify(data)}`);
}

function makeFrame({ id, sourceId, thread }, opts = {}) {
  const source = createSourceObject(sourceId);
  const sourceActor = {
    id: `${sourceId}-actor`,
    actor: `${sourceId}-actor`,
    source: sourceId,
    sourceObject: source,
  };
  const location = createLocation({ source, sourceActor, line: 4 });
  return {
    id,
    scope: { bindings: { variables: {}, arguments: [] } },
    location,
    generatedLocation: location,
    thread: thread || "FakeThread",
    ...opts,
  };
}

function createSourceObject(filename, props = {}) {
  return {
    id: filename,
    url: makeSourceURL(filename),
    isPrettyPrinted: false,
    isExtension: false,
    isOriginal: filename.includes("originalSource"),
    displayURL: makeSourceURL(filename),
  };
}

function makeSourceURL(filename) {
  return `http://localhost:8000/examples/${filename}`;
}

function createMakeSource() {
  const indicies = {};

  return function (name, props = {}) {
    const index = (indicies[name] | 0) + 1;
    indicies[name] = index;

    // Mock a SOURCE Resource, which happens to be the SourceActor's form
    // with resourceType and targetFront additional attributes
    return {
      resourceType: "source",
      // Mock the targetFront to support makeSourceId function
      targetFront: {
        isDestroyed() {
          return false;
        },
        getCachedFront(typeName) {
          return typeName == "thread" ? { actorID: "FakeThread" } : null;
        },
      },
      // Allow to use custom ID's for reducer source objects
      mockedJestID: name,
      actor: `${name}-${index}-actor`,
      url: `http://localhost:8000/examples/${name}`,
      sourceMapBaseURL: props.sourceMapBaseURL || null,
      sourceMapURL: props.sourceMapURL || null,
      introductionType: props.introductionType || null,
      extensionName: null,
    };
  };
}

/**
 * @memberof utils/test-head
 * @static
 */
let creator;
beforeEach(() => {
  creator = createMakeSource();
});
afterEach(() => {
  creator = null;
});
function makeSource(name, props) {
  if (!creator) {
    throw new Error("makeSource() cannot be called outside of a test");
  }

  return creator(name, props);
}

function makeOriginalSource(source) {
  return {
    id: `${source.id}/originalSource`,
    url: `${source.url}-original`,
    sourceActor: {
      id: `${source.id}-1-actor`,
      thread: "FakeThread",
    },
  };
}

function makeFuncLocation(startLine, endLine) {
  if (!endLine) {
    endLine = startLine + 1;
  }
  return {
    start: {
      line: startLine,
    },
    end: {
      line: endLine,
    },
  };
}

function makeSymbolDeclaration(name, start, end, klass) {
  return {
    id: `${name}:${start}`,
    name,
    location: makeFuncLocation(start, end),
    klass,
  };
}

/**
 * @memberof utils/test-head
 * @static
 */
function waitForState(store, predicate) {
  return new Promise(resolve => {
    let ret = predicate(store.getState());
    if (ret) {
      resolve(ret);
    }

    const unsubscribe = store.subscribe(() => {
      ret = predicate(store.getState());
      if (ret) {
        unsubscribe();
        // NOTE: memoizableAction adds an additional tick for validating context
        setTimeout(() => resolve(ret));
      }
    });
  });
}

function watchForState(store, predicate) {
  let sawState = false;
  const checkState = function () {
    if (!sawState && predicate(store.getState())) {
      sawState = true;
    }
    return sawState;
  };

  let unsubscribe;
  if (!checkState()) {
    unsubscribe = store.subscribe(() => {
      if (checkState()) {
        unsubscribe();
      }
    });
  }

  return function read() {
    if (unsubscribe) {
      unsubscribe();
    }

    return sawState;
  };
}

function getTelemetryEvents(eventName) {
  return window.dbg._telemetry.events[eventName] || [];
}

function waitATick(callback) {
  return new Promise(resolve => {
    setTimeout(() => {
      callback();
      resolve();
    });
  });
}

export {
  actions,
  selectors,
  reducers,
  createStore,
  commonLog,
  getTelemetryEvents,
  makeFrame,
  createSourceObject,
  createMakeSource,
  makeSourceURL,
  makeSource,
  makeOriginalSource,
  makeSymbolDeclaration,
  waitForState,
  watchForState,
  waitATick,
};
