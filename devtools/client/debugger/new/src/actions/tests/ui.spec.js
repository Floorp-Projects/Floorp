/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  createStore,
  selectors,
  actions,
  makeSource
} from "../../utils/test-head";

const {
  getActiveSearch,
  getFrameworkGroupingState,
  getPaneCollapse,
  getHighlightedLineRange,
  getProjectDirectoryRoot,
  getRelativeSources
} = selectors;

describe("ui", () => {
  it("should toggle the visible state of project search", () => {
    const { dispatch, getState } = createStore();
    expect(getActiveSearch(getState())).toBe(null);
    dispatch(actions.setActiveSearch("project"));
    expect(getActiveSearch(getState())).toBe("project");
  });

  it("should close project search", () => {
    const { dispatch, getState } = createStore();
    expect(getActiveSearch(getState())).toBe(null);
    dispatch(actions.setActiveSearch("project"));
    dispatch(actions.closeActiveSearch());
    expect(getActiveSearch(getState())).toBe(null);
  });

  it("should toggle the visible state of file search", () => {
    const { dispatch, getState } = createStore();
    expect(getActiveSearch(getState())).toBe(null);
    dispatch(actions.setActiveSearch("file"));
    expect(getActiveSearch(getState())).toBe("file");
  });

  it("should close file search", () => {
    const { dispatch, getState } = createStore();
    expect(getActiveSearch(getState())).toBe(null);
    dispatch(actions.setActiveSearch("file"));
    dispatch(actions.closeActiveSearch());
    expect(getActiveSearch(getState())).toBe(null);
  });

  it("should toggle the collapse state of a pane", () => {
    const { dispatch, getState } = createStore();
    expect(getPaneCollapse(getState(), "start")).toBe(false);
    dispatch(actions.togglePaneCollapse("start", true));
    expect(getPaneCollapse(getState(), "start")).toBe(true);
  });

  it("should toggle the collapsed state of frameworks in the callstack", () => {
    const { dispatch, getState } = createStore();
    const currentState = getFrameworkGroupingState(getState());
    dispatch(actions.toggleFrameworkGrouping(!currentState));
    expect(getFrameworkGroupingState(getState())).toBe(!currentState);
  });

  it("should highlight lines", () => {
    const { dispatch, getState } = createStore();
    const range = { start: 3, end: 5, sourceId: 2 };
    dispatch(actions.highlightLineRange(range));
    expect(getHighlightedLineRange(getState())).toEqual(range);
  });

  it("should clear highlight lines", () => {
    const { dispatch, getState } = createStore();
    const range = { start: 3, end: 5, sourceId: 2 };
    dispatch(actions.highlightLineRange(range));
    dispatch(actions.clearHighlightLineRange());
    expect(getHighlightedLineRange(getState())).toEqual({});
  });
});

describe("setProjectDirectoryRoot", () => {
  it("should set domain directory as root", () => {
    const { dispatch, getState } = createStore();
    dispatch(actions.setProjectDirectoryRoot("example.com"));
    expect(getProjectDirectoryRoot(getState())).toBe("example.com");
  });

  it("should set a directory as root directory", () => {
    const { dispatch, getState } = createStore();
    dispatch(actions.setProjectDirectoryRoot("/example.com/foo"));
    expect(getProjectDirectoryRoot(getState())).toBe("/example.com/foo");
  });

  it("should add to the directory ", () => {
    const { dispatch, getState } = createStore();
    dispatch(actions.setProjectDirectoryRoot("/example.com/foo"));
    dispatch(actions.setProjectDirectoryRoot("/foo/bar"));
    expect(getProjectDirectoryRoot(getState())).toBe("/example.com/foo/bar");
  });

  it("should update the directory ", () => {
    const { dispatch, getState } = createStore();
    dispatch(actions.setProjectDirectoryRoot("/example.com/foo"));
    dispatch(actions.clearProjectDirectoryRoot());
    dispatch(actions.setProjectDirectoryRoot("/example.com/bar"));
    expect(getProjectDirectoryRoot(getState())).toBe("/example.com/bar");
  });

  it("should filter sources", async () => {
    const store = createStore({});
    const { dispatch, getState } = store;
    await dispatch(actions.newSource(makeSource("js/scopes.js")));
    await dispatch(actions.newSource(makeSource("lib/vendor.js")));

    dispatch(actions.setProjectDirectoryRoot("localhost:8000/examples/js"));

    const filteredSourcesByThread = getRelativeSources(getState());
    const filteredSources = Object.values(filteredSourcesByThread)[0];
    const firstSource = Object.values(filteredSources)[0];

    expect(firstSource.url).toEqual(
      "http://localhost:8000/examples/js/scopes.js"
    );

    expect(firstSource.relativeUrl).toEqual("scopes.js");
  });

  it("should update the child directory ", () => {
    const { dispatch, getState } = createStore();
    dispatch(actions.setProjectDirectoryRoot("example.com"));
    dispatch(actions.setProjectDirectoryRoot("example.com/foo/bar"));
    expect(getProjectDirectoryRoot(getState())).toBe("example.com/foo/bar");
  });

  it("should update the child directory when domain name is Webpack://", () => {
    const { dispatch, getState } = createStore();
    dispatch(actions.setProjectDirectoryRoot("webpack://"));
    dispatch(actions.setProjectDirectoryRoot("webpack:///app"));
    expect(getProjectDirectoryRoot(getState())).toBe("webpack:///app");
  });
});
