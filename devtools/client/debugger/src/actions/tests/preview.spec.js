/* eslint max-nested-callbacks: ["error", 6] */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  createStore,
  selectors,
  actions,
  makeSource,
  makeFrame,
  waitForState,
  waitATick,
} from "../../utils/test-head";
import defer from "../../utils/defer.js";

function waitForPreview(store, expression) {
  return waitForState(store, state => {
    const preview = selectors.getPreview(state);
    return preview && preview.expression == expression;
  });
}

function mockThreadFront(overrides) {
  return {
    evaluateInFrame: async () => ({ result: {} }),
    getFrameScopes: async () => {},
    sourceContents: async () => ({
      source: "",
      contentType: "text/javascript",
    }),
    getSourceActorBreakpointPositions: async () => ({}),
    getSourceActorBreakableLines: async () => [],
    evaluateExpressions: async () => [],
    loadObjectProperties: async () => ({}),
    ...overrides,
  };
}

function dispatchSetPreview(dispatch, context, expression, target) {
  return dispatch(
    actions.setPreview(
      context,
      expression,
      {
        start: { url: "foo.js", line: 1, column: 2 },
        end: { url: "foo.js", line: 1, column: 5 },
      },
      { line: 2, column: 3 },
      target.getBoundingClientRect(),
      target
    )
  );
}

async function pause({ dispatch, cx }) {
  const base = await dispatch(
    actions.newGeneratedSource(makeSource("base.js"))
  );

  await dispatch(actions.selectSource(cx, base.id));
  const frames = [makeFrame({ id: "frame1", sourceId: base.id })];

  await dispatch(
    actions.paused({
      thread: "FakeThread",
      frame: frames[0],
      frames,
      loadedObjects: [],
      why: { type: "debuggerStatement" },
    })
  );
}

describe("preview", () => {
  it("should generate previews", async () => {
    const store = createStore(mockThreadFront());
    const { dispatch, getState, cx } = store;
    const base = await dispatch(
      actions.newGeneratedSource(makeSource("base.js"))
    );

    await dispatch(actions.selectSource(cx, base.id));
    const frames = [makeFrame({ id: "f1", sourceId: base.id })];

    await dispatch(
      actions.paused({
        thread: "FakeThread",
        frame: frames[0],
        frames,
        loadedObjects: [],
        why: { type: "debuggerStatement" },
      })
    );

    const newCx = selectors.getContext(getState());
    const firstTarget = document.createElement("div");

    dispatchSetPreview(dispatch, newCx, "foo", firstTarget);

    expect(selectors.getPreview(getState())).toMatchSnapshot();
  });

  // When a 2nd setPreview is called before a 1st setPreview dispatches
  // and the 2nd setPreview has not dispatched yet,
  // the first setPreview should not finish dispatching
  it("queued previews (w/ the 1st finishing first)", async () => {
    const firstSetPreview = defer();
    const secondSetPreview = defer();
    const promises = [firstSetPreview, secondSetPreview];

    const store = createStore(
      mockThreadFront({
        loadObjectProperties: () => promises.shift().promise,
      })
    );

    const { dispatch, getState } = store;
    await pause(store);

    const newCx = selectors.getContext(getState());
    const firstTarget = document.createElement("div");
    const secondTarget = document.createElement("div");

    // Start the dispatch of the first setPreview. At this point, it will not
    // finish execution until we resolve the firstSetPreview
    dispatchSetPreview(dispatch, newCx, "firstSetPreview", firstTarget);

    // Start the dispatch of the second setPreview. At this point, it will not
    // finish execution until we resolve the secondSetPreview
    dispatchSetPreview(dispatch, newCx, "secondSetPreview", secondTarget);

    let fail = false;

    firstSetPreview.resolve();
    waitForPreview(store, "firstSetPreview").then(() => {
      fail = true;
    });

    secondSetPreview.resolve();
    await waitForPreview(store, "secondSetPreview");
    expect(fail).toEqual(false);

    const preview = selectors.getPreview(getState());
    expect(preview && preview.expression).toEqual("secondSetPreview");
  });

  // When a 2nd setPreview is called before a 1st setPreview dispatches
  // and the 2nd setPreview has dispatched,
  // the first setPreview should not finish dispatching
  it("queued previews (w/ the 2nd finishing first)", async () => {
    const firstSetPreview = defer();
    const secondSetPreview = defer();
    const promises = [firstSetPreview, secondSetPreview];

    const store = createStore(
      mockThreadFront({
        loadObjectProperties: () => promises.shift().promise,
      })
    );

    const { dispatch, getState } = store;
    await pause(store);

    const newCx = selectors.getContext(getState());
    const firstTarget = document.createElement("div");
    const secondTarget = document.createElement("div");

    // Start the dispatch of the first setPreview. At this point, it will not
    // finish execution until we resolve the firstSetPreview
    dispatchSetPreview(dispatch, newCx, "firstSetPreview", firstTarget);

    // Start the dispatch of the second setPreview. At this point, it will not
    // finish execution until we resolve the secondSetPreview
    dispatchSetPreview(dispatch, newCx, "secondSetPreview", secondTarget);

    let fail = false;

    secondSetPreview.resolve();
    await waitForPreview(store, "secondSetPreview");

    firstSetPreview.resolve();
    waitForPreview(store, "firstSetPreview").then(() => {
      fail = true;
    });

    await waitATick(() => expect(fail).toEqual(false));

    const preview = selectors.getPreview(getState());
    expect(preview && preview.expression).toEqual("secondSetPreview");
  });
});
