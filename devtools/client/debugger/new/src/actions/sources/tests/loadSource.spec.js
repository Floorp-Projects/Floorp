/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import {
  actions,
  selectors,
  watchForState,
  createStore,
  makeOriginalSource,
  makeSource
} from "../../../utils/test-head";
import {
  createSource,
  sourceThreadClient
} from "../../tests/helpers/threadClient.js";
import { getBreakpointsList } from "../../../selectors";

describe("loadSourceText", () => {
  it("should load source text", async () => {
    const store = createStore(sourceThreadClient);
    const { dispatch, getState } = store;

    const foo1Source = makeSource("foo1");
    await dispatch(actions.newSource(foo1Source));
    await dispatch(actions.loadSourceText({ source: foo1Source }));
    const fooSource = selectors.getSource(getState(), "foo1");

    if (!fooSource || typeof fooSource.text != "string") {
      throw new Error("bad fooSource");
    }
    expect(fooSource.text.indexOf("return foo1")).not.toBe(-1);

    const baseFoo2Source = makeSource("foo2");
    await dispatch(actions.newSource(baseFoo2Source));
    await dispatch(actions.loadSourceText({ source: baseFoo2Source }));
    const foo2Source = selectors.getSource(getState(), "foo2");

    if (!foo2Source || typeof foo2Source.text != "string") {
      throw new Error("bad fooSource");
    }
    expect(foo2Source.text.indexOf("return foo2")).not.toBe(-1);
  });

  it("should update breakpoint text when a source loads", async () => {
    const fooOrigSource = makeOriginalSource("fooGen");
    const fooGenSource = makeSource("fooGen");

    const fooOrigContent = createSource("fooOrig", "var fooOrig = 42;");
    const fooGenContent = createSource("fooGen", "var fooGen = 42;");

    const store = createStore(
      {
        ...sourceThreadClient,
        sourceContents: async () => fooGenContent,
        getBreakpointPositions: async () => ({ "1": [0] })
      },
      {},
      {
        getGeneratedRangesForOriginal: async () => [
          { start: { line: 1, column: 0 }, end: { line: 1, column: 1 } }
        ],
        getOriginalLocations: async items =>
          items.map(item => ({
            ...item,
            sourceId: fooOrigSource.id
          })),
        getOriginalSourceText: async s => ({
          text: fooOrigContent.source,
          contentType: fooOrigContent.contentType
        })
      }
    );
    const { dispatch, getState } = store;

    await dispatch(actions.newSource(fooOrigSource));
    await dispatch(actions.newSource(fooGenSource));

    const location = {
      sourceId: fooOrigSource.id,
      line: 1,
      column: 0
    };
    await dispatch(actions.addBreakpoint(location, {}));

    const breakpoint = getBreakpointsList(getState())[0];

    expect(breakpoint.text).toBe("");
    expect(breakpoint.originalText).toBe("");

    await dispatch(actions.loadSourceText({ source: fooOrigSource }));

    const breakpoint1 = getBreakpointsList(getState())[0];
    expect(breakpoint1.text).toBe("");
    expect(breakpoint1.originalText).toBe("var fooOrig = 42;");

    await dispatch(actions.loadSourceText({ source: fooGenSource }));

    const breakpoint2 = getBreakpointsList(getState())[0];
    expect(breakpoint2.text).toBe("var fooGen = 42;");
    expect(breakpoint2.originalText).toBe("var fooOrig = 42;");
  });

  it("loads two sources w/ one request", async () => {
    let resolve;
    let count = 0;
    const { dispatch, getState } = createStore({
      sourceContents: () =>
        new Promise(r => {
          count++;
          resolve = r;
        }),
      getBreakpointPositions: async () => ({})
    });
    const id = "foo";
    const baseSource = makeSource(id, { loadedState: "unloaded" });

    await dispatch(actions.newSource(baseSource));

    let source = selectors.getSourceFromId(getState(), id);
    dispatch(actions.loadSourceText({ source }));

    source = selectors.getSourceFromId(getState(), id);
    const loading = dispatch(actions.loadSourceText({ source }));

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;
    expect(count).toEqual(1);

    source = selectors.getSource(getState(), id);
    expect(source && source.text).toEqual("yay");
  });

  it("doesn't re-load loaded sources", async () => {
    let resolve;
    let count = 0;
    const { dispatch, getState } = createStore({
      sourceContents: () =>
        new Promise(r => {
          count++;
          resolve = r;
        }),
      getBreakpointPositions: async () => ({})
    });
    const id = "foo";
    const baseSource = makeSource(id, { loadedState: "unloaded" });

    await dispatch(actions.newSource(baseSource));
    let source = selectors.getSourceFromId(getState(), id);
    const loading = dispatch(actions.loadSourceText({ source }));

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;

    source = selectors.getSourceFromId(getState(), id);
    await dispatch(actions.loadSourceText({ source }));
    expect(count).toEqual(1);

    source = selectors.getSource(getState(), id);
    expect(source && source.text).toEqual("yay");
  });

  it("should cache subsequent source text loads", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    const source = makeSource("foo1");
    dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText({ source }));
    const prevSource = selectors.getSourceFromId(getState(), "foo1");

    await dispatch(actions.loadSourceText({ source: prevSource }));
    const curSource = selectors.getSource(getState(), "foo1");

    expect(prevSource === curSource).toBeTruthy();
  });

  it("should indicate a loading source", async () => {
    const store = createStore(sourceThreadClient);
    const { dispatch } = store;

    const source = makeSource("foo2");
    await dispatch(actions.newSource(source));

    const wasLoading = watchForState(store, state => {
      const fooSource = selectors.getSource(state, "foo2");
      return fooSource && fooSource.loadedState === "loading";
    });

    await dispatch(actions.loadSourceText({ source }));

    expect(wasLoading()).toBe(true);
  });

  it("should indicate an errored source text", async () => {
    const { dispatch, getState } = createStore(sourceThreadClient);

    const source = makeSource("bad-id");
    await dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText({ source }));
    const badSource = selectors.getSource(getState(), "bad-id");

    if (!badSource || !badSource.error) {
      throw new Error("bad badSource");
    }
    expect(badSource.error.indexOf("unknown source")).not.toBe(-1);
  });
});
