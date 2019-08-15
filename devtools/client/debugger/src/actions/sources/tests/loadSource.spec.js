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
  makeSource,
} from "../../../utils/test-head";
import {
  createSource,
  mockCommandClient,
} from "../../tests/helpers/mockCommandClient";
import { getBreakpointsList } from "../../../selectors";
import { isFulfilled, isRejected } from "../../../utils/async-value";

describe("loadSourceText", () => {
  it("should load source text", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, getState, cx } = store;

    const foo1Source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    await dispatch(actions.loadSourceText({ cx, source: foo1Source }));

    const foo1Content = selectors.getSourceContent(getState(), foo1Source.id);
    expect(
      foo1Content &&
        isFulfilled(foo1Content) &&
        foo1Content.value.type === "text"
        ? foo1Content.value.value.indexOf("return foo1")
        : -1
    ).not.toBe(-1);

    const foo2Source = await dispatch(
      actions.newGeneratedSource(makeSource("foo2"))
    );
    await dispatch(actions.loadSourceText({ cx, source: foo2Source }));

    const foo2Content = selectors.getSourceContent(getState(), foo2Source.id);
    expect(
      foo2Content &&
        isFulfilled(foo2Content) &&
        foo2Content.value.type === "text"
        ? foo2Content.value.value.indexOf("return foo2")
        : -1
    ).not.toBe(-1);
  });

  it("should update breakpoint text when a source loads", async () => {
    const fooOrigContent = createSource("fooOrig", "var fooOrig = 42;");
    const fooGenContent = createSource("fooGen", "var fooGen = 42;");

    const store = createStore(
      {
        ...mockCommandClient,
        sourceContents: async () => fooGenContent,
        getBreakpointPositions: async () => ({ "1": [0] }),
        getBreakableLines: async () => [],
      },
      {},
      {
        getGeneratedRangesForOriginal: async () => [
          { start: { line: 1, column: 0 }, end: { line: 1, column: 1 } },
        ],
        getOriginalLocations: async (sourceId, items) =>
          items.map(item => ({
            ...item,
            sourceId:
              item.sourceId === fooGenSource1.id
                ? fooOrigSource1.id
                : fooOrigSource2.id,
          })),
        getOriginalSourceText: async s => ({
          text: fooOrigContent.source,
          contentType: fooOrigContent.contentType,
        }),
      }
    );
    const { cx, dispatch, getState } = store;

    const fooGenSource1 = await dispatch(
      actions.newGeneratedSource(makeSource("fooGen1"))
    );
    const fooOrigSource1 = await dispatch(
      actions.newOriginalSource(makeOriginalSource(fooGenSource1))
    );
    const fooGenSource2 = await dispatch(
      actions.newGeneratedSource(makeSource("fooGen2"))
    );
    const fooOrigSource2 = await dispatch(
      actions.newOriginalSource(makeOriginalSource(fooGenSource2))
    );

    await dispatch(actions.loadSourceText({ cx, source: fooOrigSource1 }));

    await dispatch(
      actions.addBreakpoint(
        cx,
        {
          sourceId: fooOrigSource1.id,
          line: 1,
          column: 0,
        },
        {}
      )
    );

    const breakpoint1 = getBreakpointsList(getState())[0];
    expect(breakpoint1.text).toBe("");
    expect(breakpoint1.originalText).toBe("var fooOrig = 42;");

    await dispatch(actions.loadSourceText({ cx, source: fooGenSource1 }));

    const breakpoint2 = getBreakpointsList(getState())[0];
    expect(breakpoint2.text).toBe("var fooGen = 42;");
    expect(breakpoint2.originalText).toBe("var fooOrig = 42;");

    await dispatch(actions.loadSourceText({ cx, source: fooGenSource2 }));

    await dispatch(
      actions.addBreakpoint(
        cx,
        {
          sourceId: fooGenSource2.id,
          line: 1,
          column: 0,
        },
        {}
      )
    );

    const breakpoint3 = getBreakpointsList(getState())[1];
    expect(breakpoint3.text).toBe("var fooGen = 42;");
    expect(breakpoint3.originalText).toBe("");

    await dispatch(actions.loadSourceText({ cx, source: fooOrigSource2 }));

    const breakpoint4 = getBreakpointsList(getState())[1];
    expect(breakpoint4.text).toBe("var fooGen = 42;");
    expect(breakpoint4.originalText).toBe("var fooOrig = 42;");
  });

  it("loads two sources w/ one request", async () => {
    let resolve;
    let count = 0;
    const { dispatch, getState, cx } = createStore({
      sourceContents: () =>
        new Promise(r => {
          count++;
          resolve = r;
        }),
      getBreakpointPositions: async () => ({}),
      getBreakableLines: async () => [],
    });
    const id = "foo";

    await dispatch(actions.newGeneratedSource(makeSource(id)));

    let source = selectors.getSourceFromId(getState(), id);
    dispatch(actions.loadSourceText({ cx, source }));

    source = selectors.getSourceFromId(getState(), id);
    const loading = dispatch(actions.loadSourceText({ cx, source }));

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;
    expect(count).toEqual(1);

    const content = selectors.getSourceContent(getState(), id);
    expect(
      content &&
        isFulfilled(content) &&
        content.value.type === "text" &&
        content.value.value
    ).toEqual("yay");
  });

  it("doesn't re-load loaded sources", async () => {
    let resolve;
    let count = 0;
    const { dispatch, getState, cx } = createStore({
      sourceContents: () =>
        new Promise(r => {
          count++;
          resolve = r;
        }),
      getBreakpointPositions: async () => ({}),
      getBreakableLines: async () => [],
    });
    const id = "foo";

    await dispatch(actions.newGeneratedSource(makeSource(id)));
    let source = selectors.getSourceFromId(getState(), id);
    const loading = dispatch(actions.loadSourceText({ cx, source }));

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;

    source = selectors.getSourceFromId(getState(), id);
    await dispatch(actions.loadSourceText({ cx, source }));
    expect(count).toEqual(1);

    const content = selectors.getSourceContent(getState(), id);
    expect(
      content &&
        isFulfilled(content) &&
        content.value.type === "text" &&
        content.value.value
    ).toEqual("yay");
  });

  it("should cache subsequent source text loads", async () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));
    const prevSource = selectors.getSourceFromId(getState(), "foo1");

    await dispatch(actions.loadSourceText({ cx, source: prevSource }));
    const curSource = selectors.getSource(getState(), "foo1");

    expect(prevSource === curSource).toBeTruthy();
  });

  it("should indicate a loading source", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, cx } = store;

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo2"))
    );

    const wasLoading = watchForState(store, state => {
      return !selectors.getSourceContent(state, "foo2");
    });

    await dispatch(actions.loadSourceText({ cx, source }));

    expect(wasLoading()).toBe(true);
  });

  it("should indicate an errored source text", async () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bad-id"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));
    const badSource = selectors.getSource(getState(), "bad-id");

    const content = badSource
      ? selectors.getSourceContent(getState(), badSource.id)
      : null;
    expect(
      content && isRejected(content) && typeof content.value === "string"
        ? content.value.indexOf("unknown source")
        : -1
    ).not.toBe(-1);
  });
});
