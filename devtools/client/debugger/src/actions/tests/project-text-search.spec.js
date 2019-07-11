/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  createStore,
  selectors,
  makeSource,
} from "../../utils/test-head";

const {
  getSource,
  getTextSearchQuery,
  getTextSearchResults,
  getTextSearchStatus,
} = selectors;

const sources = {
  foo1: {
    source: "function foo1() {\n  const foo = 5; return foo;\n}",
    contentType: "text/javascript",
  },
  foo2: {
    source: "function foo2(x, y) {\n  return x + y;\n}",
    contentType: "text/javascript",
  },
  bar: {
    source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
    contentType: "text/javascript",
  },
  "bar:formatted": {
    source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
    contentType: "text/javascript",
  },
};

const threadFront = {
  sourceContents: async ({ source }) => sources[source],
  getBreakpointPositions: async () => ({}),
  getBreakableLines: async () => [],
};

describe("project text search", () => {
  it("should add a project text search query", () => {
    const { dispatch, getState, cx } = createStore();
    const mockQuery = "foo";

    dispatch(actions.addSearchQuery(cx, mockQuery));

    expect(getTextSearchQuery(getState())).toEqual(mockQuery);
  });

  it("should search all the loaded sources based on the query", async () => {
    const { dispatch, getState, cx } = createStore(threadFront);
    const mockQuery = "foo";

    await dispatch(actions.newGeneratedSource(makeSource("foo1")));
    await dispatch(actions.newGeneratedSource(makeSource("foo2")));

    await dispatch(actions.searchSources(cx, mockQuery));

    const results = getTextSearchResults(getState());
    expect(results).toMatchSnapshot();
  });

  it("should ignore sources with minified versions", async () => {
    const mockMaps = {
      getOriginalSourceText: async () => ({
        source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
        contentType: "text/javascript",
      }),
      applySourceMap: async () => {},
      getGeneratedRangesForOriginal: async () => [],
      getOriginalLocations: async items => items,
      getOriginalLocation: async loc => loc,
    };

    const { dispatch, getState, cx } = createStore(threadFront, {}, mockMaps);

    const source1 = await dispatch(
      actions.newGeneratedSource(makeSource("bar"))
    );
    await dispatch(actions.loadSourceText({ cx, source: source1 }));

    await dispatch(actions.togglePrettyPrint(cx, source1.id));

    await dispatch(actions.searchSources(cx, "bla"));

    const results = getTextSearchResults(getState());
    expect(results).toMatchSnapshot();
  });

  it("should search a specific source", async () => {
    const { dispatch, getState, cx } = createStore(threadFront);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bar"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    dispatch(actions.addSearchQuery(cx, "bla"));

    const barSource = getSource(getState(), "bar");
    if (!barSource) {
      throw new Error("no barSource");
    }
    const sourceId = barSource.id;

    await dispatch(actions.searchSource(cx, sourceId, "bla"), "bla");

    const results = getTextSearchResults(getState());

    expect(results).toMatchSnapshot();
    expect(results).toHaveLength(1);
  });

  it("should clear all the search results", async () => {
    const { dispatch, getState, cx } = createStore(threadFront);
    const mockQuery = "foo";

    await dispatch(actions.newGeneratedSource(makeSource("foo1")));
    await dispatch(actions.searchSources(cx, mockQuery));

    expect(getTextSearchResults(getState())).toMatchSnapshot();

    await dispatch(actions.clearSearchResults(cx));

    expect(getTextSearchResults(getState())).toMatchSnapshot();
  });

  it("should set the status properly", () => {
    const { dispatch, getState, cx } = createStore();
    const mockStatus = "Fetching";
    dispatch(actions.updateSearchStatus(cx, mockStatus));
    expect(getTextSearchStatus(getState())).toEqual(mockStatus);
  });

  it("should close project search", async () => {
    const { dispatch, getState, cx } = createStore(threadFront);
    const mockQuery = "foo";

    await dispatch(actions.newGeneratedSource(makeSource("foo1")));
    await dispatch(actions.searchSources(cx, mockQuery));

    expect(getTextSearchResults(getState())).toMatchSnapshot();

    dispatch(actions.closeProjectSearch(cx));

    expect(getTextSearchQuery(getState())).toEqual("");

    const results = getTextSearchResults(getState());

    expect(results).toMatchSnapshot();
    expect(results).toHaveLength(0);
    const status = getTextSearchStatus(getState());
    expect(status).toEqual("INITIAL");
  });
});
