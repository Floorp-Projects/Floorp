/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Utils for Jest
 * @module utils/test-head
 */

import { combineReducers } from "redux";
import sourceMaps from "devtools-source-map";
import reducers from "../reducers";
import actions from "../actions";
import * as selectors from "../selectors";
import { parserWorker, evaluationsParser } from "../test/tests-setup";
import configureStore from "../actions/utils/create-store";
import sourceQueue from "../utils/source-queue";
import { setupCreate } from "../client/firefox/create";
/**
 * This file contains older interfaces used by tests that have not been
 * converted to use test-mockup.js
 */

/**
 * @memberof utils/test-head
 * @static
 */
function createStore(client, initialState = {}, sourceMapsMock) {
  const store = configureStore({
    log: false,
    makeThunkArgs: args => {
      return {
        ...args,
        client,
        sourceMaps: sourceMapsMock !== undefined ? sourceMapsMock : sourceMaps,
        parser: parserWorker,
        evaluationsParser,
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
    sourceMaps,
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
  return {
    id,
    scope: { bindings: { variables: {}, arguments: [] } },
    location: { sourceId, line: 4 },
    thread: thread || "FakeThread",
    ...opts,
  };
}

function createSourceObject(filename, props = {}) {
  return {
    id: filename,
    url: makeSourceURL(filename),
    thread: props.thread || "FakeThread",
    isPrettyPrinted: false,
    isExtension: false,
    isOriginal: filename.includes("originalSource"),
  };
}

function createOriginalSourceObject(generated) {
  const rv = {
    ...generated,
    id: `${generated.id}/originalSource`,
  };

  return rv;
}

function makeSourceURL(filename) {
  return `http://localhost:8000/examples/${filename}`;
}

function createMakeSource() {
  const indicies = {};

  return function(name, props = {}) {
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
  const checkState = function() {
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
  createOriginalSourceObject,
  createMakeSource,
  makeSourceURL,
  makeSource,
  makeOriginalSource,
  makeSymbolDeclaration,
  waitForState,
  watchForState,
  waitATick,
};
