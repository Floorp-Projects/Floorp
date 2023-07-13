/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { actions, selectors, createStore } from "../../utils/test-head";

const mockThreadFront = {
  evaluate: (script, { frameId }) =>
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
  getFrames: async () => [],
  sourceContents: () => ({ source: "", contentType: "text/javascript" }),
  getSourceActorBreakpointPositions: async () => ({}),
  getSourceActorBreakableLines: async () => [],
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
    const { dispatch, getState } = createStore(mockThreadFront);

    await dispatch(actions.addExpression("foo"));
    expect(selectors.getExpressions(getState())).toHaveLength(1);
  });

  it("should not add empty expressions", () => {
    const { dispatch, getState } = createStore(mockThreadFront);

    dispatch(actions.addExpression(undefined));
    dispatch(actions.addExpression(""));
    expect(selectors.getExpressions(getState())).toHaveLength(0);
  });

  it("should not add invalid expressions", async () => {
    const { dispatch, getState } = createStore(mockThreadFront);
    await dispatch(actions.addExpression("foo#"));
    const state = getState();
    expect(selectors.getExpressions(state)).toHaveLength(0);
    expect(selectors.getExpressionError(state)).toBe(true);
  });

  it("should update an expression", async () => {
    const { dispatch, getState } = createStore(mockThreadFront);

    await dispatch(actions.addExpression("foo"));
    const expression = selectors.getExpression(getState(), "foo");
    if (!expression) {
      throw new Error("expression must exist");
    }

    await dispatch(actions.updateExpression("bar", expression));
    const bar = selectors.getExpression(getState(), "bar");

    expect(bar && bar.input).toBe("bar");
  });

  it("should not update an expression w/ invalid code", async () => {
    const { dispatch, getState } = createStore(mockThreadFront);

    await dispatch(actions.addExpression("foo"));
    const expression = selectors.getExpression(getState(), "foo");
    if (!expression) {
      throw new Error("expression must exist");
    }
    await dispatch(actions.updateExpression("#bar", expression));
    expect(selectors.getExpression(getState(), "bar")).toBeUndefined();
  });

  it("should delete an expression", async () => {
    const { dispatch, getState } = createStore(mockThreadFront);

    await dispatch(actions.addExpression("foo"));
    await dispatch(actions.addExpression("bar"));
    expect(selectors.getExpressions(getState())).toHaveLength(2);

    const expression = selectors.getExpression(getState(), "foo");

    if (!expression) {
      throw new Error("expression must exist");
    }

    const bar = selectors.getExpression(getState(), "bar");
    dispatch(actions.deleteExpression(expression));
    expect(selectors.getExpressions(getState())).toHaveLength(1);
    expect(bar && bar.input).toBe("bar");
  });

  it("should evaluate expressions global scope", async () => {
    const { dispatch, getState } = createStore(mockThreadFront);
    await dispatch(actions.addExpression("foo"));
    await dispatch(actions.addExpression("bar"));

    let foo = selectors.getExpression(getState(), "foo");
    let bar = selectors.getExpression(getState(), "bar");
    expect(foo && foo.value).toBe("bla");
    expect(bar && bar.value).toBe("bla");

    await dispatch(actions.evaluateExpressions(null));
    foo = selectors.getExpression(getState(), "foo");
    bar = selectors.getExpression(getState(), "bar");
    expect(foo && foo.value).toBe("bla");
    expect(bar && bar.value).toBe("bla");
  });

  it("should get the autocomplete matches for the input", async () => {
    const { dispatch, getState } = createStore(mockThreadFront);
    await dispatch(actions.autocomplete("to", 2));
    expect(selectors.getAutocompleteMatchset(getState())).toMatchSnapshot();
  });
});
