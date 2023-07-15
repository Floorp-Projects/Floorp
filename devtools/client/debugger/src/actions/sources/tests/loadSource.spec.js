/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
import { createLocation } from "../../../utils/location";

describe("loadGeneratedSourceText", () => {
  it("should load source text", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, getState } = store;

    const foo1Source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    const foo1SourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      foo1Source.id
    );
    await dispatch(actions.loadGeneratedSourceText(foo1SourceActor));

    const foo1Content = selectors.getSettledSourceTextContent(
      getState(),
      createLocation({
        source: foo1Source,
        sourceActor: foo1SourceActor,
      })
    );

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
    const foo2SourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      foo2Source.id
    );

    await dispatch(actions.loadGeneratedSourceText(foo2SourceActor));

    const foo2Content = selectors.getSettledSourceTextContent(
      getState(),
      createLocation({
        source: foo2Source,
        sourceActor: foo2SourceActor,
      })
    );

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
        getSourceActorBreakpointPositions: async () => ({ 1: [0] }),
        getSourceActorBreakableLines: async () => [],
      },
      {},
      {
        getGeneratedRangesForOriginal: async () => [
          { start: { line: 1, column: 0 }, end: { line: 1, column: 1 } },
        ],
        getOriginalLocations: async items =>
          items.map(item => ({
            ...item,
            sourceId:
              item.sourceId === fooGenSource1.id
                ? fooOrigSources1[0].id
                : fooOrigSources2[0].id,
          })),
        getOriginalSourceText: async s => ({
          text: fooOrigContent.source,
          contentType: fooOrigContent.contentType,
        }),
      }
    );
    const { dispatch, getState } = store;

    const fooGenSource1 = await dispatch(
      actions.newGeneratedSource(makeSource("fooGen1"))
    );

    const fooOrigSources1 = await dispatch(
      actions.newOriginalSources([makeOriginalSource(fooGenSource1)])
    );
    const fooGenSource2 = await dispatch(
      actions.newGeneratedSource(makeSource("fooGen2"))
    );

    const fooOrigSources2 = await dispatch(
      actions.newOriginalSources([makeOriginalSource(fooGenSource2)])
    );

    await dispatch(actions.loadOriginalSourceText(fooOrigSources1[0]));

    await dispatch(
      actions.addBreakpoint(
        createLocation({
          source: fooOrigSources1[0],
          line: 1,
          column: 0,
        }),
        {}
      )
    );

    const breakpoint1 = getBreakpointsList(getState())[0];
    expect(breakpoint1.text).toBe("");
    expect(breakpoint1.originalText).toBe("var fooOrig = 42;");

    const fooGenSource1SourceActor =
      selectors.getFirstSourceActorForGeneratedSource(
        getState(),
        fooGenSource1.id
      );

    await dispatch(actions.loadGeneratedSourceText(fooGenSource1SourceActor));

    const breakpoint2 = getBreakpointsList(getState())[0];
    expect(breakpoint2.text).toBe("var fooGen = 42;");
    expect(breakpoint2.originalText).toBe("var fooOrig = 42;");

    const fooGenSource2SourceActor =
      selectors.getFirstSourceActorForGeneratedSource(
        getState(),
        fooGenSource2.id
      );

    await dispatch(actions.loadGeneratedSourceText(fooGenSource2SourceActor));

    await dispatch(
      actions.addBreakpoint(
        createLocation({
          source: fooGenSource2,
          line: 1,
          column: 0,
        }),
        {}
      )
    );

    const breakpoint3 = getBreakpointsList(getState())[1];
    expect(breakpoint3.text).toBe("var fooGen = 42;");
    expect(breakpoint3.originalText).toBe("");

    await dispatch(actions.loadOriginalSourceText(fooOrigSources2[0]));

    const breakpoint4 = getBreakpointsList(getState())[1];
    expect(breakpoint4.text).toBe("var fooGen = 42;");
    expect(breakpoint4.originalText).toBe("var fooOrig = 42;");
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
      getSourceActorBreakpointPositions: async () => ({}),
      getSourceActorBreakableLines: async () => [],
    });
    const id = "foo";

    const source = await dispatch(actions.newGeneratedSource(makeSource(id)));
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    dispatch(actions.loadGeneratedSourceText(sourceActor));

    const loading = dispatch(actions.loadGeneratedSourceText(sourceActor));

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;
    expect(count).toEqual(1);

    const content = selectors.getSettledSourceTextContent(
      getState(),
      createLocation({
        source,
        sourceActor,
      })
    );
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
    const { dispatch, getState } = createStore({
      sourceContents: () =>
        new Promise(r => {
          count++;
          resolve = r;
        }),
      getSourceActorBreakpointPositions: async () => ({}),
      getSourceActorBreakableLines: async () => [],
    });
    const id = "foo";

    const source = await dispatch(actions.newGeneratedSource(makeSource(id)));
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    const loading = dispatch(actions.loadGeneratedSourceText(sourceActor));

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;

    await dispatch(actions.loadGeneratedSourceText(sourceActor));
    expect(count).toEqual(1);

    const content = selectors.getSettledSourceTextContent(
      getState(),
      createLocation({
        source,
        sourceActor,
      })
    );
    expect(
      content &&
        isFulfilled(content) &&
        content.value.type === "text" &&
        content.value.value
    ).toEqual("yay");
  });

  it("should indicate a loading source", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, getState } = store;

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo2"))
    );

    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    const wasLoading = watchForState(store, state => {
      return !selectors.getSettledSourceTextContent(
        state,
        createLocation({
          source,
          sourceActor,
        })
      );
    });
    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    expect(wasLoading()).toBe(true);
  });

  it("should indicate an errored source text", async () => {
    const { dispatch, getState } = createStore(mockCommandClient);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bad-id"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    const content = selectors.getSettledSourceTextContent(
      getState(),
      createLocation({
        source,
        sourceActor,
      })
    );
    expect(
      content && isRejected(content) && typeof content.value === "string"
        ? content.value.indexOf("sourceContents failed")
        : -1
    ).not.toBe(-1);
  });
});
