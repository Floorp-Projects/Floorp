/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  createStore,
  selectors,
  actions,
  makeSource
} from "../../utils/test-head";

const { getProjectDirectoryRoot, getRelativeSources } = selectors;

describe("setProjectDirectoryRoot", () => {
  it("should set domain directory as root", async () => {
    const { dispatch, getState } = createStore();
    dispatch(actions.setProjectDirectoryRoot("example.com"));
    expect(getProjectDirectoryRoot(getState())).toBe("example.com");
  });

  it("should set a directory as root directory", async () => {
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
