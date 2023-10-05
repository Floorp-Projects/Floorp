/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  actions,
  selectors,
  watchForState,
  createStore,
  makeSource,
} from "../../../utils/test-head";
import { mockCommandClient } from "../../tests/helpers/mockCommandClient";
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
