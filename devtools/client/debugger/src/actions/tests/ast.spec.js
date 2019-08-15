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
  makeOriginalSource,
  waitForState,
} from "../../utils/test-head";

import readFixture from "./helpers/readFixture";
const { getSymbols, isSymbolsLoading, getFramework } = selectors;

const threadFront = {
  sourceContents: async ({ source }) => ({
    source: sourceTexts[source],
    contentType: "text/javascript",
  }),
  getFrameScopes: async () => {},
  evaluate: async expression => ({ result: evaluationResult[expression] }),
  evaluateExpressions: async expressions =>
    expressions.map(expression => ({ result: evaluationResult[expression] })),
  getBreakpointPositions: async () => ({}),
  getBreakableLines: async () => [],
};

const sourceMaps = {
  getOriginalSourceText: async ({ id }) => ({
    id,
    text: sourceTexts[id],
    contentType: "text/javascript",
  }),
  getGeneratedRangesForOriginal: async () => [],
  getOriginalLocations: async items => items,
};

const sourceTexts = {
  "base.js": "function base(boo) {}",
  "foo.js": "function base(boo) { return this.bazz; } outOfScope",
  "reactComponent.js/originalSource": readFixture("reactComponent.js"),
  "reactFuncComponent.js/originalSource": readFixture("reactFuncComponent.js"),
};

const evaluationResult = {
  "this.bazz": { actor: "bazz", preview: {} },
  this: { actor: "this", preview: {} },
};

describe("ast", () => {
  describe("setSymbols", () => {
    describe("when the source is loaded", () => {
      it("should be able to set symbols", async () => {
        const store = createStore(threadFront);
        const { dispatch, getState, cx } = store;
        const base = await dispatch(
          actions.newGeneratedSource(makeSource("base.js"))
        );
        await dispatch(actions.loadSourceText({ cx, source: base }));

        const loadedSource = selectors.getSourceFromId(getState(), base.id);
        await dispatch(actions.setSymbols({ cx, source: loadedSource }));
        await waitForState(store, state => !isSymbolsLoading(state, base));

        const baseSymbols = getSymbols(getState(), base);
        expect(baseSymbols).toMatchSnapshot();
      });
    });

    describe("when the source is not loaded", () => {
      it("should return null", async () => {
        const { getState, dispatch } = createStore(threadFront);
        const base = await dispatch(
          actions.newGeneratedSource(makeSource("base.js"))
        );

        const baseSymbols = getSymbols(getState(), base);
        expect(baseSymbols).toEqual(null);
      });
    });

    describe("when there is no source", () => {
      it("should return null", async () => {
        const { getState } = createStore(threadFront);
        const baseSymbols = getSymbols(getState());
        expect(baseSymbols).toEqual(null);
      });
    });

    describe("frameworks", () => {
      it("should detect react components", async () => {
        const store = createStore(threadFront, {}, sourceMaps);
        const { cx, dispatch, getState } = store;

        const genSource = await dispatch(
          actions.newGeneratedSource(makeSource("reactComponent.js"))
        );

        const source = await dispatch(
          actions.newOriginalSource(makeOriginalSource(genSource))
        );

        await dispatch(actions.loadSourceText({ cx, source }));
        const loadedSource = selectors.getSourceFromId(getState(), source.id);
        await dispatch(actions.setSymbols({ cx, source: loadedSource }));

        expect(getFramework(getState(), source)).toBe("React");
      });

      it("should not give false positive on non react components", async () => {
        const store = createStore(threadFront);
        const { cx, dispatch, getState } = store;
        const base = await dispatch(
          actions.newGeneratedSource(makeSource("base.js"))
        );
        await dispatch(actions.loadSourceText({ cx, source: base }));
        await dispatch(actions.setSymbols({ cx, source: base }));

        expect(getFramework(getState(), base)).toBe(undefined);
      });
    });
  });
});
