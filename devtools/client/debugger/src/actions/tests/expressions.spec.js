/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  createStore,
  makeSource,
} from "../../utils/test-head";

import { makeMockFrame } from "../../utils/test-mockup";

const mockThreadClient = {
  evaluateInFrame: (script, { frameId }) =>
    new Promise((resolve, reject) => {
      if (!frameId) {
        resolve("bla");
      } else {
        resolve("boo");
      }
    }),
  evaluateExpressions: (inputs, { frameId }) =>
    Promise.all(
      inputs.map(
        input =>
          new Promise((resolve, reject) => {
            if (!frameId) {
              resolve("bla");
            } else {
              resolve("boo");
            }
          })
      )
    ),
  getFrameScopes: async () => {},
  sourceContents: () => ({ source: "", contentType: "text/javascript" }),
  getBreakpointPositions: async () => ({}),
  getBreakableLines: async () => [],
  autocomplete: () => {
    return new Promise(resolve => {
      resolve({
        from: "foo",
        matches: ["toLocaleString", "toSource", "toString", "toolbar", "top"],
        matchProp: "to",
      });
    });
  },
};

describe("expressions", () => {
  it("should add an expression", async () => {
    const { dispatch, getState, cx } = createStore(mockThreadClient);

    await dispatch(actions.addExpression(cx, "foo"));
    expect(selectors.getExpressions(getState()).size).toBe(1);
  });

  it("should not add empty expressions", () => {
    const { dispatch, getState, cx } = createStore(mockThreadClient);

    dispatch(actions.addExpression(cx, (undefined: any)));
    dispatch(actions.addExpression(cx, ""));
    expect(selectors.getExpressions(getState()).size).toBe(0);
  });

  it("should not add invalid expressions", async () => {
    const { dispatch, getState, cx } = createStore(mockThreadClient);
    await dispatch(actions.addExpression(cx, "foo#"));
    const state = getState();
    expect(selectors.getExpressions(state).size).toBe(0);
    expect(selectors.getExpressionError(state)).toBe(true);
  });

  it("should update an expression", async () => {
    const { dispatch, getState, cx } = createStore(mockThreadClient);

    await dispatch(actions.addExpression(cx, "foo"));
    const expression = selectors.getExpression(getState(), "foo");
    await dispatch(actions.updateExpression(cx, "bar", expression));

    expect(selectors.getExpression(getState(), "bar").input).toBe("bar");
  });

  it("should not update an expression w/ invalid code", async () => {
    const { dispatch, getState, cx } = createStore(mockThreadClient);

    await dispatch(actions.addExpression(cx, "foo"));
    const expression = selectors.getExpression(getState(), "foo");
    await dispatch(actions.updateExpression(cx, "#bar", expression));
    expect(selectors.getExpression(getState(), "bar")).toBeUndefined();
  });

  it("should delete an expression", async () => {
    const { dispatch, getState, cx } = createStore(mockThreadClient);

    await dispatch(actions.addExpression(cx, "foo"));
    await dispatch(actions.addExpression(cx, "bar"));
    expect(selectors.getExpressions(getState()).size).toBe(2);

    const expression = selectors.getExpression(getState(), "foo");
    dispatch(actions.deleteExpression(expression));
    expect(selectors.getExpressions(getState()).size).toBe(1);
    expect(selectors.getExpression(getState(), "bar").input).toBe("bar");
  });

  it("should evaluate expressions global scope", async () => {
    const { dispatch, getState, cx } = createStore(mockThreadClient);
    await dispatch(actions.addExpression(cx, "foo"));
    await dispatch(actions.addExpression(cx, "bar"));
    expect(selectors.getExpression(getState(), "foo").value).toBe("bla");
    expect(selectors.getExpression(getState(), "bar").value).toBe("bla");

    await dispatch(actions.evaluateExpressions(cx));
    expect(selectors.getExpression(getState(), "foo").value).toBe("bla");
    expect(selectors.getExpression(getState(), "bar").value).toBe("bla");
  });

  it("should evaluate expressions in specific scope", async () => {
    const { dispatch, getState } = createStore(mockThreadClient);
    await createFrames(getState, dispatch);

    const cx = selectors.getThreadContext(getState());
    await dispatch(actions.newGeneratedSource(makeSource("source")));
    await dispatch(actions.addExpression(cx, "foo"));
    await dispatch(actions.addExpression(cx, "bar"));

    expect(selectors.getExpression(getState(), "foo").value).toBe("boo");
    expect(selectors.getExpression(getState(), "bar").value).toBe("boo");

    await dispatch(actions.evaluateExpressions(cx));

    expect(selectors.getExpression(getState(), "foo").value).toBe("boo");
    expect(selectors.getExpression(getState(), "bar").value).toBe("boo");
  });

  it("should get the autocomplete matches for the input", async () => {
    const { cx, dispatch, getState } = createStore(mockThreadClient);
    await dispatch(actions.autocomplete(cx, "to", 2));
    expect(selectors.getAutocompleteMatchset(getState())).toMatchSnapshot();
  });
});

async function createFrames(getState, dispatch) {
  const frame = makeMockFrame();
  await dispatch(actions.newGeneratedSource(makeSource("example.js")));
  await dispatch(actions.newGeneratedSource(makeSource("source")));

  await dispatch(
    actions.paused({
      thread: "FakeThread",
      frame,
      frames: [frame],
      why: { type: "just because" },
    })
  );

  await dispatch(
    actions.selectFrame(selectors.getThreadContext(getState()), frame)
  );
}
