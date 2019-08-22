/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Utils for Jest
 * @module utils/test-head
 */

import { combineReducers } from "redux";
import sourceMaps from "devtools-source-map";
import reducers from "../reducers";
import actions from "../actions";
import * as selectors from "../selectors";
import { getHistory } from "../test/utils/history";
import { parserWorker, evaluationsParser } from "../test/tests-setup";
import configureStore from "../actions/utils/create-store";
import sourceQueue from "../utils/source-queue";
import type { Source, OriginalSourceData, GeneratedSourceData } from "../types";

/**
 * This file contains older interfaces used by tests that have not been
 * converted to use test-mockup.js
 */

/**
 * @memberof utils/test-head
 * @static
 */
function createStore(client: any, initialState: any = {}, sourceMapsMock: any) {
  client = {
    hasWasmSupport: () => true,
    ...client,
  };

  const store = configureStore({
    log: false,
    history: getHistory(),
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
    newQueuedSources: sources =>
      store.dispatch(actions.newQueuedSources(sources)),
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

  return store;
}

/**
 * @memberof utils/test-head
 * @static
 */
function commonLog(msg: string, data: any = {}) {
  console.log(`[INFO] ${msg} ${JSON.stringify(data)}`);
}

function makeFrame({ id, sourceId }: Object, opts: Object = {}) {
  return {
    id,
    scope: { bindings: { variables: {}, arguments: [] } },
    location: { sourceId, line: 4 },
    thread: "FakeThread",
    ...opts,
  };
}

function createSourceObject(
  filename: string,
  props: {
    sourceMapURL?: string,
    introductionType?: string,
    introductionUrl?: string,
    isBlackBoxed?: boolean,
  } = {}
): Source {
  return ({
    id: filename,
    url: makeSourceURL(filename),
    isBlackBoxed: !!props.isBlackBoxed,
    isPrettyPrinted: false,
    introductionUrl: props.introductionUrl || null,
    introductionType: props.introductionType || null,
    isExtension: false,
  }: any);
}

function createOriginalSourceObject(generated: Source): Source {
  const rv = {
    ...generated,
    id: `${generated.id}/originalSource`,
  };

  return (rv: any);
}

function makeSourceURL(filename: string) {
  return `http://localhost:8000/examples/${filename}`;
}

type MakeSourceProps = {
  sourceMapURL?: string,
  introductionType?: string,
  introductionUrl?: string,
  isBlackBoxed?: boolean,
};
function createMakeSource(): (
  // The name of the file that this actor is part of.
  name: string,
  props?: MakeSourceProps
) => GeneratedSourceData {
  const indicies = {};

  return function(name, props = {}) {
    const index = (indicies[name] | 0) + 1;
    indicies[name] = index;

    return {
      id: name,
      thread: "FakeThread",
      source: {
        actor: `${name}-${index}-actor`,
        url: `http://localhost:8000/examples/${name}`,
        sourceMapURL: props.sourceMapURL || null,
        introductionType: props.introductionType || null,
        introductionUrl: props.introductionUrl || null,
        isBlackBoxed: !!props.isBlackBoxed,
        extensionName: null,
      },
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
function makeSource(name: string, props?: MakeSourceProps) {
  if (!creator) {
    throw new Error("makeSource() cannot be called outside of a test");
  }

  return creator(name, props);
}

function makeOriginalSource(source: Source): OriginalSourceData {
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

function makeSymbolDeclaration(
  name: string,
  start: number,
  end: ?number,
  klass: ?string
) {
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
function waitForState(store: any, predicate: any): Promise<void> {
  return new Promise(resolve => {
    let ret = predicate(store.getState());
    if (ret) {
      resolve(ret);
    }

    const unsubscribe = store.subscribe(() => {
      ret = predicate(store.getState());
      if (ret) {
        unsubscribe();
        resolve(ret);
      }
    });
  });
}

function watchForState(store: any, predicate: any): () => boolean {
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

function getTelemetryEvents(eventName: string) {
  return window.dbg._telemetry.events[eventName] || [];
}

function waitATick(callback: Function): Promise<*> {
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
  getHistory,
  waitATick,
};
