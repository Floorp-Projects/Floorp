/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  createStore,
  makeSource
} from "../../../utils/test-head";
import { sourceThreadClient } from "../../tests/helpers/threadClient.js";

describe("loadSourceText", async () => {
  it("should load source text", async () => {
    const store = createStore(sourceThreadClient);
    const { dispatch, getState } = store;

    await dispatch(actions.loadSourceText({ id: "foo1" }));
    const fooSource = selectors.getSource(getState(), "foo1");

    expect(fooSource.text.indexOf("return foo1")).not.toBe(-1);

    await dispatch(actions.loadSourceText({ id: "foo2" }));
    const foo2Source = selectors.getSource(getState(), "foo2");

    expect(foo2Source.text.indexOf("return foo2")).not.toBe(-1);
  });

  it("loads two sources w/ one request", async () => {
    let resolve;
    let count = 0;
    const { dispatch, getState } = createStore({
      sourceContents: () =>
        new Promise(r => {
          count++;
          resolve = r;
        })
    });
    const id = "foo";
    let source = makeSource(id, { loadedState: "unloaded" });

    await dispatch(actions.newSource(source));

    source = selectors.getSource(getState(), id);
    dispatch(actions.loadSourceText(source));

    source = selectors.getSource(getState(), id);
    const loading = dispatch(actions.loadSourceText(source));

    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;
    expect(count).toEqual(1);
    expect(selectors.getSource(getState(), id).text).toEqual("yay");
  });

  it("doesn't re-load loaded sources", async () => {
    let resolve;
    let count = 0;
    const { dispatch, getState } = createStore({
      sourceContents: () =>
        new Promise(r => {
          count++;
          resolve = r;
        })
    });
    const id = "foo";
    let source = makeSource(id, { loadedState: "unloaded" });

    await dispatch(actions.newSource(source));
    source = selectors.getSource(getState(), id);
    const loading = dispatch(actions.loadSourceText(source));
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;

    source = selectors.getSource(getState(), id);
    await dispatch(actions.loadSourceText(source));
    expect(count).toEqual(1);
    expect(selectors.getSource(getState(), id).text).toEqual("yay");
  });

  it("should cache subsequent source text loads", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    await dispatch(actions.loadSourceText({ id: "foo1" }));
    const prevSource = selectors.getSource(getState(), "foo1");

    await dispatch(actions.loadSourceText(prevSource));
    const curSource = selectors.getSource(getState(), "foo1");

    expect(prevSource === curSource).toBeTruthy();
  });

  it("should indicate a loading source", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    // Don't block on this so we can check the loading state.
    dispatch(actions.loadSourceText({ id: "foo1" }));
    const fooSource = selectors.getSource(getState(), "foo1");
    expect(fooSource.loadedState).toEqual("loading");
  });

  it("should indicate an errored source text", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    await dispatch(actions.loadSourceText({ id: "bad-id" }));
    const badSource = selectors.getSource(getState(), "bad-id");
    expect(badSource.error.indexOf("unknown source")).not.toBe(-1);
  });
});
