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
  makeFrame,
  waitForState
} from "../../utils/test-head";

import readFixture from "./helpers/readFixture";
const {
  getSource,
  getSymbols,
  getOutOfScopeLocations,
  getSourceMetaData,
  getInScopeLines,
  isSymbolsLoading
} = selectors;

import { prefs } from "../../utils/prefs";

const threadClient = {
  sourceContents: async ({ source }) => ({
    source: sourceTexts[source],
    contentType: "text/javascript"
  }),
  getFrameScopes: async () => {},
  evaluate: async expression => ({ result: evaluationResult[expression] }),
  evaluateExpressions: async expressions =>
    expressions.map(expression => ({ result: evaluationResult[expression] })),
  getBreakpointPositions: async () => ({})
};

const sourceMaps = {
  getOriginalSourceText: async ({ id }) => ({
    id,
    text: sourceTexts[id],
    contentType: "text/javascript"
  }),
  getGeneratedRangesForOriginal: async () => [],
  getOriginalLocations: async items => items
};

const sourceTexts = {
  "base.js": "function base(boo) {}",
  "foo.js": "function base(boo) { return this.bazz; } outOfScope",
  "scopes.js": readFixture("scopes.js"),
  "reactComponent.js/originalSource": readFixture("reactComponent.js"),
  "reactFuncComponent.js/originalSource": readFixture("reactFuncComponent.js")
};

const evaluationResult = {
  "this.bazz": { actor: "bazz", preview: {} },
  this: { actor: "this", preview: {} }
};

describe("ast", () => {
  describe("setSourceMetaData", () => {
    it("should detect react components", async () => {
      const store = createStore(threadClient, {}, sourceMaps);
      const { dispatch, getState } = store;
      const source = makeOriginalSource("reactComponent.js");

      await dispatch(actions.newSource(makeSource("reactComponent.js")));

      await dispatch(actions.newSource(source));

      await dispatch(actions.loadSourceText(getSource(getState(), source.id)));
      await dispatch(actions.setSourceMetaData(source.id));

      await waitForState(store, state => {
        const metaData = getSourceMetaData(state, source.id);
        return metaData && metaData.framework;
      });

      const sourceMetaData = getSourceMetaData(getState(), source.id);
      expect(sourceMetaData.framework).toBe("React");
    });

    it("should not give false positive on non react components", async () => {
      const store = createStore(threadClient);
      const { dispatch, getState } = store;
      const base = makeSource("base.js");
      await dispatch(actions.newSource(base));
      await dispatch(actions.loadSourceText(base));
      await dispatch(actions.setSourceMetaData("base.js"));

      const sourceMetaData = getSourceMetaData(getState(), base.id);
      expect(sourceMetaData.framework).toBe(undefined);
    });
  });

  describe("setSymbols", () => {
    describe("when the source is loaded", () => {
      it("should be able to set symbols", async () => {
        const store = createStore(threadClient);
        const { dispatch, getState } = store;
        const base = makeSource("base.js");
        await dispatch(actions.newSource(base));
        await dispatch(actions.loadSourceText(base));
        await dispatch(actions.setSymbols("base.js"));
        await waitForState(store, state => !isSymbolsLoading(state, base));

        const baseSymbols = getSymbols(getState(), base);
        expect(baseSymbols).toMatchSnapshot();
      });
    });

    describe("when the source is not loaded", () => {
      it("should return null", async () => {
        const { getState, dispatch } = createStore(threadClient);
        const base = makeSource("base.js");
        await dispatch(actions.newSource(base));

        const baseSymbols = getSymbols(getState(), base);
        expect(baseSymbols).toEqual(null);
      });
    });

    describe("when there is no source", () => {
      it("should return null", async () => {
        const { getState } = createStore(threadClient);
        const baseSymbols = getSymbols(getState());
        expect(baseSymbols).toEqual(null);
      });
    });
  });

  describe("getOutOfScopeLocations", () => {
    beforeEach(async () => {
      prefs.autoPrettyPrint = false;
    });

    it("with selected line", async () => {
      const store = createStore(threadClient);
      const { dispatch, getState } = store;
      const source = makeSource("scopes.js");
      await dispatch(actions.newSource(source));

      await dispatch(
        actions.selectLocation({ sourceId: "scopes.js", line: 5 })
      );

      const frame = makeFrame({ id: "1", sourceId: "scopes.js" });
      await dispatch(
        actions.paused({
          thread: "FakeThread",
          why: { type: "debuggerStatement" },
          frame,
          frames: [frame]
        })
      );

      await dispatch(actions.setOutOfScopeLocations());

      await waitForState(store, state => getOutOfScopeLocations(state));

      const locations = getOutOfScopeLocations(getState());
      const lines = getInScopeLines(getState());

      expect(locations).toMatchSnapshot();
      expect(lines).toMatchSnapshot();
    });

    it("without a selected line", async () => {
      const { dispatch, getState } = createStore(threadClient);
      const base = makeSource("base.js");
      await dispatch(actions.newSource(base));
      await dispatch(actions.selectSource("base.js"));

      const locations = getOutOfScopeLocations(getState());
      // const lines = getInScopeLines(getState());

      expect(locations).toEqual(null);

      // This check is disabled as locations that are in/out of scope may not
      // have completed yet when the selectSource promise finishes.
      // expect(lines).toEqual([1]);
    });
  });
});
