/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createStore, selectors, actions } from "../../utils/test-head";

const {
  getFileSearchQuery,
  getFileSearchModifiers,
  getFileSearchResults,
} = selectors;

describe("file text search", () => {
  it("should update search results", () => {
    const { dispatch, getState, cx } = createStore();
    expect(getFileSearchResults(getState())).toEqual({
      matches: [],
      matchIndex: -1,
      index: -1,
      count: 0,
    });

    const matches = [{ line: 1, ch: 3 }, { line: 3, ch: 2 }];
    dispatch(actions.updateSearchResults(cx, 2, 3, matches));

    expect(getFileSearchResults(getState())).toEqual({
      count: 2,
      index: 2,
      matchIndex: 1,
      matches: matches,
    });
  });

  it("should update the file search query", () => {
    const { dispatch, getState, cx } = createStore();
    let fileSearchQueryState = getFileSearchQuery(getState());
    expect(fileSearchQueryState).toBe("");
    dispatch(actions.setFileSearchQuery(cx, "foobar"));
    fileSearchQueryState = getFileSearchQuery(getState());
    expect(fileSearchQueryState).toBe("foobar");
  });

  it("should toggle a file search modifier", () => {
    const { dispatch, getState, cx } = createStore();
    let fileSearchModState = getFileSearchModifiers(getState());
    expect(fileSearchModState.get("caseSensitive")).toBe(false);
    dispatch(actions.toggleFileSearchModifier(cx, "caseSensitive"));
    fileSearchModState = getFileSearchModifiers(getState());
    expect(fileSearchModState.get("caseSensitive")).toBe(true);
  });

  it("should toggle a file search query cleaning", () => {
    const { dispatch, getState, cx } = createStore();
    dispatch(actions.setFileSearchQuery(cx, "foobar"));
    let fileSearchQueryState = getFileSearchQuery(getState());
    expect(fileSearchQueryState).toBe("foobar");
    dispatch(actions.setFileSearchQuery(cx, ""));
    fileSearchQueryState = getFileSearchQuery(getState());
    expect(fileSearchQueryState).toBe("");
  });
});
