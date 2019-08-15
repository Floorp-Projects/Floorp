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

const { getProjectDirectoryRoot, getDisplayedSources } = selectors;

describe("setProjectDirectoryRoot", () => {
  it("should set domain directory as root", async () => {
    const { dispatch, getState, cx } = createStore();
    dispatch(actions.setProjectDirectoryRoot(cx, "example.com"));
    expect(getProjectDirectoryRoot(getState())).toBe("example.com");
  });

  it("should set a directory as root directory", async () => {
    const { dispatch, getState, cx } = createStore();
    dispatch(actions.setProjectDirectoryRoot(cx, "/example.com/foo"));
    expect(getProjectDirectoryRoot(getState())).toBe("/example.com/foo");
  });

  it("should add to the directory ", () => {
    const { dispatch, getState, cx } = createStore();
    dispatch(actions.setProjectDirectoryRoot(cx, "/example.com/foo"));
    dispatch(actions.setProjectDirectoryRoot(cx, "/foo/bar"));
    expect(getProjectDirectoryRoot(getState())).toBe("/example.com/foo/bar");
  });

  it("should update the directory ", () => {
    const { dispatch, getState, cx } = createStore();
    dispatch(actions.setProjectDirectoryRoot(cx, "/example.com/foo"));
    dispatch(actions.clearProjectDirectoryRoot(cx));
    dispatch(actions.setProjectDirectoryRoot(cx, "/example.com/bar"));
    expect(getProjectDirectoryRoot(getState())).toBe("/example.com/bar");
  });

  it("should filter sources", async () => {
    const store = createStore({
      getBreakableLines: async () => [],
    });
    const { dispatch, getState, cx } = store;
    await dispatch(actions.newGeneratedSource(makeSource("js/scopes.js")));
    await dispatch(actions.newGeneratedSource(makeSource("lib/vendor.js")));

    dispatch(actions.setProjectDirectoryRoot(cx, "localhost:8000/examples/js"));

    const filteredSourcesByThread = getDisplayedSources(getState());
    const filteredSources = (Object.values(
      filteredSourcesByThread.FakeThread
    ): any)[0];

    expect(filteredSources.url).toEqual(
      "http://localhost:8000/examples/js/scopes.js"
    );

    expect(filteredSources.relativeUrl).toEqual("scopes.js");
  });

  it("should update the child directory ", () => {
    const { dispatch, getState, cx } = createStore({
      getBreakableLines: async () => [],
    });
    dispatch(actions.setProjectDirectoryRoot(cx, "example.com"));
    dispatch(actions.setProjectDirectoryRoot(cx, "example.com/foo/bar"));
    expect(getProjectDirectoryRoot(getState())).toBe("example.com/foo/bar");
  });

  it("should update the child directory when domain name is Webpack://", () => {
    const { dispatch, getState, cx } = createStore({
      getBreakableLines: async () => [],
    });
    dispatch(actions.setProjectDirectoryRoot(cx, "webpack://"));
    dispatch(actions.setProjectDirectoryRoot(cx, "webpack:///app"));
    expect(getProjectDirectoryRoot(getState())).toBe("webpack:///app");
  });
});
