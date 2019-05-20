/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  createStore,
  selectors,
  actions,
  makeSource,
} from "../../utils/test-head";

jest.mock("../../utils/editor");

const {
  getActiveSearch,
  getTextSearchQuery,
  getTextSearchResults,
  getTextSearchStatus,
  getFileSearchQuery,
  getFileSearchResults,
} = selectors;

const threadClient = {
  sourceContents: async () => ({
    source: "function foo1() {\n  const foo = 5; return foo;\n}",
    contentType: "text/javascript",
  }),
  getBreakpointPositions: async () => ({}),
  getBreakableLines: async () => [],
  detachWorkers: () => {},
};

describe("navigation", () => {
  it("connect sets the debuggeeUrl", async () => {
    const { dispatch, getState } = createStore({
      fetchWorkers: () => Promise.resolve([]),
      getMainThread: () => "FakeThread",
    });
    await dispatch(
      actions.connect("http://test.com/foo", "actor", false, false)
    );
    expect(selectors.getDebuggeeUrl(getState())).toEqual("http://test.com/foo");
  });

  it("navigation closes project-search", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);
    const mockQuery = "foo";

    await dispatch(actions.newGeneratedSource(makeSource("foo1")));
    await dispatch(actions.searchSources(cx, mockQuery));

    let results = getTextSearchResults(getState());
    expect(results).toHaveLength(1);
    expect(selectors.getTextSearchQuery(getState())).toEqual("foo");
    expect(getTextSearchStatus(getState())).toEqual("DONE");

    await dispatch(actions.willNavigate("will-navigate"));

    results = getTextSearchResults(getState());
    expect(results).toHaveLength(0);
    expect(getTextSearchQuery(getState())).toEqual("");
    expect(getTextSearchStatus(getState())).toEqual("INITIAL");
  });

  it("navigation removes activeSearch 'project' value", async () => {
    const { dispatch, getState } = createStore(threadClient);
    dispatch(actions.setActiveSearch("project"));
    expect(getActiveSearch(getState())).toBe("project");

    await dispatch(actions.willNavigate("will-navigate"));
    expect(getActiveSearch(getState())).toBe(null);
  });

  it("navigation clears the file-search query", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    dispatch(actions.setFileSearchQuery(cx, "foobar"));
    expect(getFileSearchQuery(getState())).toBe("foobar");

    await dispatch(actions.willNavigate("will-navigate"));

    expect(getFileSearchQuery(getState())).toBe("");
  });

  it("navigation clears the file-search results", async () => {
    const { dispatch, getState, cx } = createStore(threadClient);

    const searchResults = [{ line: 1, ch: 3 }, { line: 3, ch: 2 }];
    dispatch(actions.updateSearchResults(cx, 2, 3, searchResults));
    expect(getFileSearchResults(getState())).toEqual({
      count: 2,
      index: 2,
      matchIndex: 1,
      matches: searchResults,
    });

    await dispatch(actions.willNavigate("will-navigate"));

    expect(getFileSearchResults(getState())).toEqual({
      count: 0,
      index: -1,
      matchIndex: -1,
      matches: [],
    });
  });

  it("navigation removes activeSearch 'file' value", async () => {
    const { dispatch, getState } = createStore(threadClient);
    dispatch(actions.setActiveSearch("file"));
    expect(getActiveSearch(getState())).toBe("file");

    await dispatch(actions.willNavigate("will-navigate"));
    expect(getActiveSearch(getState())).toBe(null);
  });
});
