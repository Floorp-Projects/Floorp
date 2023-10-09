/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  createStore,
  selectors,
  actions,
  makeSource,
  getTelemetryEvents,
} from "../../../utils/test-head";

import { mockCommandClient } from "../../tests/helpers/mockCommandClient";
import { createLocation } from "../../../utils/location";

jest.mock("../../../utils/prefs", () => ({
  prefs: {
    expressions: [],
  },
  asyncStore: {
    pendingBreakpoints: {},
  },
  features: {
    inlinePreview: true,
  },
}));

function mockClient(positionsResponse = {}) {
  return {
    ...mockCommandClient,
    setSkipPausing: jest.fn(),
    getSourceActorBreakpointPositions: async () => positionsResponse,
    getSourceActorBreakableLines: async () => [],
  };
}

describe("breakpoints", () => {
  it("should add a breakpoint", async () => {
    const { dispatch, getState } = createStore(mockClient({ 2: [1] }));
    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    source.url = "http://localhost:8000/examples/a";
    const loc1 = createLocation({
      source,
      line: 2,
      column: 1,
    });
    await dispatch(
      actions.selectLocation(
        createLocation({
          source,
          line: 1,
          column: 1,
        })
      )
    );

    await dispatch(actions.addBreakpoint(loc1));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);
    expect(getTelemetryEvents("add_breakpoint")).toHaveLength(1);

    const bpSources = selectors.getBreakpointSources(getState());
    expect(bpSources).toMatchSnapshot();
  });

  it("should not show a breakpoint that does not have text", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1] }));
    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    source.url = "http://localhost:8000/examples/a";
    const loc1 = createLocation({
      source,
      line: 5,
      column: 1,
    });
    await dispatch(
      actions.selectLocation(
        createLocation({
          source,
          line: 1,
          column: 1,
        })
      )
    );

    await dispatch(actions.addBreakpoint(loc1));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);
    expect(selectors.getBreakpointSources(getState())).toMatchSnapshot();
  });

  it("should show a disabled breakpoint that does not have text", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1] }));
    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    source.url = "http://localhost:8000/examples/a";
    const loc1 = createLocation({
      source,
      line: 5,
      column: 1,
    });
    await dispatch(
      actions.selectLocation(
        createLocation({
          source,
          line: 1,
          column: 1,
        })
      )
    );

    await dispatch(actions.addBreakpoint(loc1));
    const breakpoint = selectors.getBreakpoint(getState(), loc1);
    if (!breakpoint) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(breakpoint));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);
    expect(selectors.getBreakpointSources(getState())).toMatchSnapshot();
  });

  it("should not re-add a breakpoint", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1] }));
    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    source.url = "http://localhost:8000/examples/a";
    const loc1 = createLocation({
      source,
      line: 5,
      column: 1,
    });
    await dispatch(
      actions.selectLocation(
        createLocation({
          source,
          line: 1,
          column: 1,
        })
      )
    );

    await dispatch(actions.addBreakpoint(loc1));
    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);

    await dispatch(actions.addBreakpoint(loc1));
    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("should remove a breakpoint", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1], 6: [2] }));

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    aSource.url = "http://localhost:8000/examples/a";

    const bSource = await dispatch(actions.newGeneratedSource(makeSource("b")));
    bSource.url = "http://localhost:8000/examples/b";

    const loc1 = createLocation({
      source: aSource,
      line: 5,
      column: 1,
    });

    const loc2 = createLocation({
      source: bSource,
      line: 6,
      column: 2,
    });
    const bSourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      bSource.id
    );

    await dispatch(actions.loadGeneratedSourceText(bSourceActor));

    await dispatch(
      actions.selectLocation(
        createLocation({
          source: aSource,
          line: 1,
          column: 1,
        })
      )
    );

    await dispatch(actions.addBreakpoint(loc1));
    await dispatch(actions.addBreakpoint(loc2));

    const bp = selectors.getBreakpoint(getState(), loc1);
    if (!bp) {
      throw new Error("no bp");
    }
    await dispatch(actions.removeBreakpoint(bp));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("should disable a breakpoint", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1], 6: [2] }));

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    aSource.url = "http://localhost:8000/examples/a";
    const aSourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      aSource.id
    );
    await dispatch(actions.loadGeneratedSourceText(aSourceActor));

    const bSource = await dispatch(actions.newGeneratedSource(makeSource("b")));
    bSource.url = "http://localhost:8000/examples/b";
    const bSourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      bSource.id
    );
    await dispatch(actions.loadGeneratedSourceText(bSourceActor));

    const loc1 = createLocation({
      source: aSource,
      line: 5,
      column: 1,
    });

    const loc2 = createLocation({
      source: bSource,
      line: 6,
      column: 2,
    });
    await dispatch(actions.addBreakpoint(loc1));
    await dispatch(actions.addBreakpoint(loc2));

    const breakpoint = selectors.getBreakpoint(getState(), loc1);
    if (!breakpoint) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(breakpoint));

    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.disabled).toBe(true);
  });

  it("should enable breakpoint", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1], 6: [2] }));

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    aSource.url = "http://localhost:8000/examples/a";
    const loc = createLocation({
      source: aSource,
      line: 5,
      column: 1,
    });
    const aSourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      aSource.id
    );
    await dispatch(actions.loadGeneratedSourceText(aSourceActor));

    await dispatch(actions.addBreakpoint(loc));
    let bp = selectors.getBreakpoint(getState(), loc);
    if (!bp) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(bp));

    bp = selectors.getBreakpoint(getState(), loc);
    if (!bp) {
      throw new Error("no breakpoint");
    }

    expect(bp && bp.disabled).toBe(true);

    await dispatch(actions.enableBreakpoint(bp));

    bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && !bp.disabled).toBe(true);
  });

  it("should toggle all the breakpoints", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1], 6: [2] }));

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    aSource.url = "http://localhost:8000/examples/a";
    const aSourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      aSource.id
    );
    await dispatch(actions.loadGeneratedSourceText(aSourceActor));

    const bSource = await dispatch(actions.newGeneratedSource(makeSource("b")));
    bSource.url = "http://localhost:8000/examples/b";
    const bSourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      bSource.id
    );
    await dispatch(actions.loadGeneratedSourceText(bSourceActor));

    const loc1 = createLocation({
      source: aSource,
      line: 5,
      column: 1,
    });

    const loc2 = createLocation({
      source: bSource,
      line: 6,
      column: 2,
    });

    await dispatch(actions.addBreakpoint(loc1));
    await dispatch(actions.addBreakpoint(loc2));

    await dispatch(actions.toggleAllBreakpoints(true));

    let bp1 = selectors.getBreakpoint(getState(), loc1);
    let bp2 = selectors.getBreakpoint(getState(), loc2);

    expect(bp1 && bp1.disabled).toBe(true);
    expect(bp2 && bp2.disabled).toBe(true);

    await dispatch(actions.toggleAllBreakpoints(false));

    bp1 = selectors.getBreakpoint(getState(), loc1);
    bp2 = selectors.getBreakpoint(getState(), loc2);
    expect(bp1 && bp1.disabled).toBe(false);
    expect(bp2 && bp2.disabled).toBe(false);
  });

  it("should toggle a breakpoint at a location", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1] }));

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    const loc = createLocation({ source, line: 5, column: 1 });
    const getBp = () => selectors.getBreakpoint(getState(), loc);
    await dispatch(actions.selectLocation(loc));

    await dispatch(actions.toggleBreakpointAtLine(5));
    const bp = getBp();
    expect(bp && !bp.disabled).toBe(true);

    await dispatch(actions.toggleBreakpointAtLine(5));
    expect(getBp()).toBe(undefined);
  });

  it("should disable/enable a breakpoint at a location", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1] }));

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    const location = createLocation({ source, line: 5, column: 1 });
    const getBp = () => selectors.getBreakpoint(getState(), location);
    await dispatch(actions.selectLocation(createLocation({ source, line: 1 })));

    await dispatch(actions.toggleBreakpointAtLine(5));
    let bp = getBp();
    expect(bp && !bp.disabled).toBe(true);
    bp = getBp();
    if (!bp) {
      throw new Error("no bp");
    }
    await dispatch(actions.toggleDisabledBreakpoint(bp));
    bp = getBp();
    expect(bp && bp.disabled).toBe(true);
  });

  it("should set the breakpoint condition", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1] }));

    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    source.url = "http://localhost:8000/examples/a";
    const loc = createLocation({
      source,
      line: 5,
      column: 1,
    });
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    await dispatch(actions.addBreakpoint(loc));

    let bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && bp.options.condition).toBe(undefined);

    await dispatch(
      actions.setBreakpointOptions(loc, {
        condition: "const foo = 0",
        getTextForLine: () => {},
      })
    );

    bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && bp.options.condition).toBe("const foo = 0");
  });

  it("should set the condition and enable a breakpoint", async () => {
    const { dispatch, getState } = createStore(mockClient({ 5: [1] }));

    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    source.url = "http://localhost:8000/examples/a";
    const loc = createLocation({
      source,
      line: 5,
      column: 1,
    });
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    await dispatch(actions.addBreakpoint(loc));
    let bp = selectors.getBreakpoint(getState(), loc);
    if (!bp) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(bp));

    bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && bp.options.condition).toBe(undefined);

    await dispatch(
      actions.setBreakpointOptions(loc, {
        condition: "const foo = 0",
        getTextForLine: () => {},
      })
    );
    const newBreakpoint = selectors.getBreakpoint(getState(), loc);
    expect(newBreakpoint && !newBreakpoint.disabled).toBe(true);
    expect(newBreakpoint && newBreakpoint.options.condition).toBe(
      "const foo = 0"
    );
  });

  it("should remove the pretty-printed breakpoint that was added", async () => {
    const { dispatch, getState } = createStore(mockClient({ 1: [0] }));

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("a.js"))
    );
    source.url = "http://localhost:8000/examples/a";
    const loc = createLocation({
      source,
      line: 1,
      column: 0,
    });
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );
    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    await dispatch(actions.addBreakpoint(loc));
    await dispatch(actions.prettyPrintAndSelectSource("a.js"));

    const breakpoint = selectors.getBreakpointsList(getState())[0];

    await dispatch(actions.removeBreakpoint(breakpoint));

    const breakpointList = selectors.getPendingBreakpointList(getState());
    expect(breakpointList.length).toBe(0);
  });
});
