/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  createStore,
  selectors,
  makeSource
} from "../../utils/test-head";

const {
  getSource,
  getTextSearchQuery,
  getTextSearchResults,
  getTextSearchStatus
} = selectors;

const sources = {
  foo1: {
    source: "function foo1() {\n  const foo = 5; return foo;\n}",
    contentType: "text/javascript"
  },
  foo2: {
    source: "function foo2(x, y) {\n  return x + y;\n}",
    contentType: "text/javascript"
  },
  bar: {
    source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
    contentType: "text/javascript"
  },
  "bar:formatted": {
    source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
    contentType: "text/javascript"
  }
};

const threadClient = {
  sourceContents: async ({ source }) => sources[source],
  getBreakpointPositions: async () => ({})
};

describe("project text search", () => {
  it("should add a project text search query", () => {
    const { dispatch, getState } = createStore();
    const mockQuery = "foo";

    dispatch(actions.addSearchQuery(mockQuery));

    expect(getTextSearchQuery(getState())).toEqual(mockQuery);
  });

  it("should search all the loaded sources based on the query", async () => {
    const { dispatch, getState } = createStore(threadClient);
    const mockQuery = "foo";
    const source1 = makeSource("foo1");
    const source2 = makeSource("foo2");

    await dispatch(actions.newSource(source1));
    await dispatch(actions.newSource(source2));

    await dispatch(actions.searchSources(mockQuery));

    const results = getTextSearchResults(getState());
    expect(results).toMatchSnapshot();
  });

  it("should ignore sources with minified versions", async () => {
    const source1 = makeSource("bar", { sourceMapURL: "bar:formatted" });
    const source2 = makeSource("bar:formatted");

    const mockMaps = {
      getOriginalSourceText: async () => ({
        source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
        contentType: "text/javascript"
      }),
      getOriginalURLs: async () => [source2.url],
      getGeneratedRangesForOriginal: async () => [],
      getOriginalLocations: async items => items
    };

    const { dispatch, getState } = createStore(threadClient, {}, mockMaps);
    const mockQuery = "bla";

    await dispatch(actions.newSource(source1));
    await dispatch(actions.newSource(source2));

    await dispatch(actions.searchSources(mockQuery));

    const results = getTextSearchResults(getState());
    expect(results).toMatchSnapshot();
  });

  it("should search a specific source", async () => {
    const { dispatch, getState } = createStore(threadClient);

    const source = makeSource("bar");
    await dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText(source));

    dispatch(actions.addSearchQuery("bla"));

    const barSource = getSource(getState(), "bar");
    if (!barSource) {
      throw new Error("no barSource");
    }
    const sourceId = barSource.id;

    await dispatch(actions.searchSource(sourceId, "bla"), "bla");

    const results = getTextSearchResults(getState());

    expect(results).toMatchSnapshot();
    expect(results).toHaveLength(1);
  });

  it("should clear all the search results", async () => {
    const { dispatch, getState } = createStore(threadClient);
    const mockQuery = "foo";

    await dispatch(actions.newSource(makeSource("foo1")));
    await dispatch(actions.searchSources(mockQuery));

    expect(getTextSearchResults(getState())).toMatchSnapshot();

    await dispatch(actions.clearSearchResults());

    expect(getTextSearchResults(getState())).toMatchSnapshot();
  });

  it("should set the status properly", () => {
    const { dispatch, getState } = createStore();
    const mockStatus = "Fetching";
    dispatch(actions.updateSearchStatus(mockStatus));
    expect(getTextSearchStatus(getState())).toEqual(mockStatus);
  });

  it("should close project search", async () => {
    const { dispatch, getState } = createStore(threadClient);
    const mockQuery = "foo";

    await dispatch(actions.newSource(makeSource("foo1")));
    await dispatch(actions.searchSources(mockQuery));

    expect(getTextSearchResults(getState())).toMatchSnapshot();

    dispatch(actions.closeProjectSearch());

    expect(getTextSearchQuery(getState())).toEqual("");

    const results = getTextSearchResults(getState());

    expect(results).toMatchSnapshot();
    expect(results).toHaveLength(0);
    const status = getTextSearchStatus(getState());
    expect(status).toEqual("INITIAL");
  });
});
