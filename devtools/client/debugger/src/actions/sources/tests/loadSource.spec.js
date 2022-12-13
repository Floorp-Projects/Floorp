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

describe("loadGeneratedSourceText", () => {
  it("should load source text", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, getState, cx } = store;

    const foo1Source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    const foo1SourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      foo1Source.id
    );
    await dispatch(
      actions.loadGeneratedSourceText({
        cx,
        sourceActor: foo1SourceActor,
      })
    );

    const foo1Content = selectors.getSettledSourceTextContent(getState(), {
      sourceId: foo1Source.id,
      sourceActorId: foo1SourceActor.actor,
    });

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

    await dispatch(
      actions.loadGeneratedSourceText({
        cx,
        sourceActor: foo2SourceActor,
      })
    );

    const foo2Content = selectors.getSettledSourceTextContent(getState(), {
      sourceId: foo2Source.id,
      sourceActorId: foo2SourceActor.actor,
    });

    expect(
      foo2Content &&
        isFulfilled(foo2Content) &&
        foo2Content.value.type === "text"
        ? foo2Content.value.value.indexOf("return foo2")
        : -1
    ).not.toBe(-1);
  });

  it("should load source text for a specific source actor", async () => {
    const mockContents = {
      "testSource1-0-actor": "const test1 = 500;",
      "testSource2-0-actor": "const test2 = 442;",
    };

    const store = createStore({
      ...mockCommandClient,
      sourceContents: ({ actor }) => {
        return new Promise(resolve => {
          resolve(createSource(actor, mockContents[actor]));
        });
      },
    });
    const { dispatch, getState, cx } = store;

    const testSource1 = await dispatch(
      actions.newGeneratedSource(makeSource("testSource1"))
    );

    // Load the source text for the specific source actor
    // specified.
    await dispatch(
      actions.loadGeneratedSourceText({
        cx,
        sourceActor: { actor: "testSource2-0-actor", source: testSource1.id },
      })
    );

    const testSource1Content = selectors.getSettledSourceTextContent(
      getState(),
      { sourceId: testSource1.id, sourceActorId: "testSource2-0-actor" }
    );

    expect(
      testSource1Content &&
        isFulfilled(testSource1Content) &&
        testSource1Content.value.type === "text"
        ? testSource1Content.value.value.indexOf(
            mockContents["testSource2-0-actor"]
          )
        : -1
    ).not.toBe(-1);

    // Load the new source text when a different source actor is specified
    await dispatch(
      actions.loadGeneratedSourceText({
        cx,
        sourceActor: { actor: "testSource1-0-actor" },
      })
    );

    const testSource1ContentAfterThirdLoad = selectors.getSettledSourceTextContent(
      getState(),
      {
        sourceId: testSource1.id,
        sourceActorId: "testSource1-0-actor",
      }
    );

    expect(
      testSource1ContentAfterThirdLoad &&
        isFulfilled(testSource1ContentAfterThirdLoad) &&
        testSource1ContentAfterThirdLoad.value.type === "text"
        ? testSource1ContentAfterThirdLoad.value.value.indexOf(
            mockContents["testSource1-0-actor"]
          )
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
        getSourceActorBreakpointPositions: async () => ({ "1": [0] }),
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
    const { cx, dispatch, getState } = store;

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

    await dispatch(
      actions.loadOriginalSourceText({
        cx,
        source: fooOrigSources1[0],
      })
    );

    await dispatch(
      actions.addBreakpoint(
        cx,
        {
          sourceId: fooOrigSources1[0].id,
          line: 1,
          column: 0,
        },
        {}
      )
    );

    const breakpoint1 = getBreakpointsList(getState())[0];
    expect(breakpoint1.text).toBe("");
    expect(breakpoint1.originalText).toBe("var fooOrig = 42;");

    const fooGenSource1SourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      fooGenSource1.id
    );

    await dispatch(
      actions.loadGeneratedSourceText({
        cx,
        sourceActor: fooGenSource1SourceActor,
      })
    );

    const breakpoint2 = getBreakpointsList(getState())[0];
    expect(breakpoint2.text).toBe("var fooGen = 42;");
    expect(breakpoint2.originalText).toBe("var fooOrig = 42;");

    const fooGenSource2SourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      fooGenSource2.id
    );

    await dispatch(
      actions.loadGeneratedSourceText({
        cx,
        sourceActor: fooGenSource2SourceActor,
      })
    );

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

    await dispatch(
      actions.loadOriginalSourceText({
        cx,
        source: fooOrigSources2[0],
      })
    );

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
      getSourceActorBreakpointPositions: async () => ({}),
      getSourceActorBreakableLines: async () => [],
    });
    const id = "foo";

    await dispatch(actions.newGeneratedSource(makeSource(id)));

    let source = selectors.getSourceFromId(getState(), id);
    let sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    dispatch(actions.loadGeneratedSourceText({ cx, sourceActor }));

    source = selectors.getSourceFromId(getState(), id);
    sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    const loading = dispatch(
      actions.loadGeneratedSourceText({ cx, sourceActor })
    );

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;
    expect(count).toEqual(1);

    const content = selectors.getSettledSourceTextContent(getState(), {
      sourceId: id,
      sourceActorId: sourceActor.actor,
    });
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
      getSourceActorBreakpointPositions: async () => ({}),
      getSourceActorBreakableLines: async () => [],
    });
    const id = "foo";

    await dispatch(actions.newGeneratedSource(makeSource(id)));
    let source = selectors.getSourceFromId(getState(), id);
    let sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    const loading = dispatch(
      actions.loadGeneratedSourceText({ cx, sourceActor })
    );

    if (!resolve) {
      throw new Error("no resolve");
    }
    resolve({ source: "yay", contentType: "text/javascript" });
    await loading;

    source = selectors.getSourceFromId(getState(), id);
    sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    await dispatch(actions.loadGeneratedSourceText({ cx, sourceActor }));
    expect(count).toEqual(1);

    const content = selectors.getSettledSourceTextContent(getState(), {
      sourceId: id,
      sourceActorId: sourceActor.actor,
    });
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
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    await dispatch(actions.loadGeneratedSourceText({ cx, sourceActor }));

    const prevSource = selectors.getSourceFromId(getState(), "foo1");
    const prevSourceSourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      prevSource.id
    );
    await dispatch(
      actions.loadGeneratedSourceText({
        cx,
        sourceActor: prevSourceSourceActor,
      })
    );
    const curSource = selectors.getSource(getState(), "foo1");

    expect(prevSource === curSource).toBeTruthy();
  });

  it("should indicate a loading source", async () => {
    const store = createStore(mockCommandClient);
    const { dispatch, cx, getState } = store;

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo2"))
    );

    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    const wasLoading = watchForState(store, state => {
      return !selectors.getSettledSourceTextContent(state, {
        sourceId: "foo2",
        sourceActorId: sourceActor.actor,
      });
    });
    await dispatch(actions.loadGeneratedSourceText({ cx, sourceActor }));

    expect(wasLoading()).toBe(true);
  });

  it("should indicate an errored source text", async () => {
    const { dispatch, getState, cx } = createStore(mockCommandClient);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bad-id"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    await dispatch(actions.loadGeneratedSourceText({ cx, sourceActor }));
    const badSource = selectors.getSource(getState(), "bad-id");

    const content = badSource
      ? selectors.getSettledSourceTextContent(getState(), {
          sourceId: badSource.id,
          sourceActorId: sourceActor.actor,
        })
      : null;
    expect(
      content && isRejected(content) && typeof content.value === "string"
        ? content.value.indexOf("sourceContents failed")
        : -1
    ).not.toBe(-1);
  });
});
