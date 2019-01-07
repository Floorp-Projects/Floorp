/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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

const threadClient = {
  sourceContents: function(sourceId) {
    return new Promise((resolve, reject) => {
      switch (sourceId) {
        case "foo1":
          resolve({
            source: "function foo1() {\n  const foo = 5; return foo;\n}",
            contentType: "text/javascript"
          });
          break;
        case "foo2":
          resolve({
            source: "function foo2(x, y) {\n  return x + y;\n}",
            contentType: "text/javascript"
          });
          break;
        case "bar":
          resolve({
            source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
            contentType: "text/javascript"
          });
          break;
        case "bar:formatted":
          resolve({
            source: "function bla(x, y) {\n const bar = 4; return 2;\n}",
            contentType: "text/javascript"
          });
          break;
      }

      reject(`unknown source: ${sourceId}`);
    });
  }
};

describe("project text search", () => {
  it("should add a project text search query", () => {
    const { dispatch, getState } = createStore();
    const mockQuery = "foo";

    dispatch(actions.addSearchQuery(mockQuery));

    expect(getTextSearchQuery(getState())).toEqual(mockQuery);
  });

  it("should remove the  project text search query", () => {
    const { dispatch, getState } = createStore();
    const mockQuery = "foo";

    dispatch(actions.addSearchQuery(mockQuery));
    expect(getTextSearchQuery(getState())).toEqual(mockQuery);
    dispatch(actions.updateSearchStatus("DONE"));
    dispatch(actions.clearSearchQuery());
    expect(getTextSearchQuery(getState())).toEqual("");
    const status = getTextSearchStatus(getState());
    expect(status).toEqual("INITIAL");
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
      getOriginalURLs: async () => [source2.url]
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

    await dispatch(actions.newSource(makeSource("bar")));
    await dispatch(actions.loadSourceText({ id: "bar" }));

    dispatch(actions.addSearchQuery("bla"));

    const sourceId = getSource(getState(), "bar").id;

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
