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
import { mockPendingBreakpoint } from "../../tests/helpers/breakpoints.js";
import { makePendingLocationId } from "../../../utils/breakpoint";
import { asyncStore } from "../../../utils/prefs";
const {
  registerStoreObserver,
} = require("devtools/client/shared/redux/subscriber");

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
    const { dispatch, getState, cx } = createStore(mockClient({ "2": [1] }));
    const loc1 = {
      sourceId: "a",
      line: 2,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source }));
    await dispatch(
      actions.setSelectedLocation(cx, source, {
        line: 1,
        column: 1,
        sourceId: source.id,
      })
    );

    await dispatch(actions.addBreakpoint(cx, loc1));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);
    expect(getTelemetryEvents("add_breakpoint")).toHaveLength(1);

    const bpSources = selectors.getBreakpointSources(getState());
    expect(bpSources).toMatchSnapshot();
  });

  it("should not show a breakpoint that does not have text", async () => {
    const { dispatch, getState, cx } = createStore(mockClient({ "5": [1] }));
    const loc1 = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };
    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source }));
    await dispatch(
      actions.setSelectedLocation(cx, source, {
        line: 1,
        column: 1,
        sourceId: source.id,
      })
    );

    await dispatch(actions.addBreakpoint(cx, loc1));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);
    expect(selectors.getBreakpointSources(getState())).toMatchSnapshot();
  });

  it("should show a disabled breakpoint that does not have text", async () => {
    const { dispatch, getState, cx } = createStore(mockClient({ "5": [1] }));
    const loc1 = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };
    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source }));
    await dispatch(
      actions.setSelectedLocation(cx, source, {
        line: 1,
        column: 1,
        sourceId: source.id,
      })
    );

    await dispatch(actions.addBreakpoint(cx, loc1));
    const breakpoint = selectors.getBreakpoint(getState(), loc1);
    if (!breakpoint) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(cx, breakpoint));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);
    expect(selectors.getBreakpointSources(getState())).toMatchSnapshot();
  });

  it("should not re-add a breakpoint", async () => {
    const { dispatch, getState, cx } = createStore(mockClient({ "5": [1] }));
    const loc1 = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source }));
    await dispatch(
      actions.setSelectedLocation(cx, source, {
        line: 1,
        column: 1,
        sourceId: source.id,
      })
    );

    await dispatch(actions.addBreakpoint(cx, loc1));
    expect(selectors.getBreakpointCount(getState())).toEqual(1);
    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.location).toEqual(loc1);

    await dispatch(actions.addBreakpoint(cx, loc1));
    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("should remove a breakpoint", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [1], "6": [2] })
    );

    const loc1 = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const loc2 = {
      sourceId: "b",
      line: 6,
      column: 2,
      sourceUrl: "http://localhost:8000/examples/b",
    };

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source: aSource }));

    const bSource = await dispatch(actions.newGeneratedSource(makeSource("b")));
    await dispatch(actions.loadSourceText({ cx, source: bSource }));

    await dispatch(
      actions.setSelectedLocation(cx, aSource, {
        line: 1,
        column: 1,
        sourceId: aSource.id,
      })
    );

    await dispatch(actions.addBreakpoint(cx, loc1));
    await dispatch(actions.addBreakpoint(cx, loc2));

    const bp = selectors.getBreakpoint(getState(), loc1);
    if (!bp) {
      throw new Error("no bp");
    }
    await dispatch(actions.removeBreakpoint(cx, bp));

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("should disable a breakpoint", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [1], "6": [2] })
    );

    const loc1 = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const loc2 = {
      sourceId: "b",
      line: 6,
      column: 2,
      sourceUrl: "http://localhost:8000/examples/b",
    };

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source: aSource }));

    const bSource = await dispatch(actions.newGeneratedSource(makeSource("b")));
    await dispatch(actions.loadSourceText({ cx, source: bSource }));

    await dispatch(actions.addBreakpoint(cx, loc1));
    await dispatch(actions.addBreakpoint(cx, loc2));

    const breakpoint = selectors.getBreakpoint(getState(), loc1);
    if (!breakpoint) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(cx, breakpoint));

    const bp = selectors.getBreakpoint(getState(), loc1);
    expect(bp && bp.disabled).toBe(true);
  });

  it("should enable breakpoint", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [1], "6": [2] })
    );
    const loc = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source: aSource }));

    await dispatch(actions.addBreakpoint(cx, loc));
    let bp = selectors.getBreakpoint(getState(), loc);
    if (!bp) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(cx, bp));

    bp = selectors.getBreakpoint(getState(), loc);
    if (!bp) {
      throw new Error("no breakpoint");
    }

    expect(bp && bp.disabled).toBe(true);

    await dispatch(actions.enableBreakpoint(cx, bp));

    bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && !bp.disabled).toBe(true);
  });

  it("should toggle all the breakpoints", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [1], "6": [2] })
    );

    const loc1 = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const loc2 = {
      sourceId: "b",
      line: 6,
      column: 2,
      sourceUrl: "http://localhost:8000/examples/b",
    };

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source: aSource }));

    const bSource = await dispatch(actions.newGeneratedSource(makeSource("b")));
    await dispatch(actions.loadSourceText({ cx, source: bSource }));

    await dispatch(actions.addBreakpoint(cx, loc1));
    await dispatch(actions.addBreakpoint(cx, loc2));

    await dispatch(actions.toggleAllBreakpoints(cx, true));

    let bp1 = selectors.getBreakpoint(getState(), loc1);
    let bp2 = selectors.getBreakpoint(getState(), loc2);

    expect(bp1 && bp1.disabled).toBe(true);
    expect(bp2 && bp2.disabled).toBe(true);

    await dispatch(actions.toggleAllBreakpoints(cx, false));

    bp1 = selectors.getBreakpoint(getState(), loc1);
    bp2 = selectors.getBreakpoint(getState(), loc2);
    expect(bp1 && bp1.disabled).toBe(false);
    expect(bp2 && bp2.disabled).toBe(false);
  });

  it("should remove all the breakpoints", async () => {
    const mockedPendingBreakpoint = mockPendingBreakpoint({ column: 2 });
    const id = makePendingLocationId(mockedPendingBreakpoint.location);

    const pendingBreakpoints = {
      [id]: mockedPendingBreakpoint,
    };

    asyncStore.pendingBreakpoints = pendingBreakpoints;

    const store = createStore(mockClient({ "5": [1], "6": [2] }), {
      pendingBreakpoints,
    });

    const { dispatch, getState, cx } = store;

    // This mocks the updatePrefs observer which listens & updates the asyncStore
    // when the reducer state changes. See https://searchfox.org/mozilla-central/
    // rev/287583a4a605eee8cd2d41381ffaea7a93d7b987/devtools/client/debugger/
    // src/utils/bootstrap.js#114-142
    function mockUpdatePrefs(state, oldState) {
      if (
        selectors.getPendingBreakpoints(oldState) &&
        selectors.getPendingBreakpoints(oldState) !==
          selectors.getPendingBreakpoints(state)
      ) {
        asyncStore.pendingBreakpoints = selectors.getPendingBreakpoints(state);
      }
    }

    registerStoreObserver(store, mockUpdatePrefs);

    const loc1 = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const loc2 = {
      sourceId: "b",
      line: 6,
      column: 2,
      sourceUrl: "http://localhost:8000/examples/b",
    };

    const aSource = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source: aSource }));

    const bSource = await dispatch(actions.newGeneratedSource(makeSource("b")));
    await dispatch(actions.loadSourceText({ cx, source: bSource }));

    expect(selectors.getBreakpointsList(getState())).toHaveLength(0);
    expect(selectors.getPendingBreakpointList(getState())).toHaveLength(1);
    expect(Object.keys(asyncStore.pendingBreakpoints)).toHaveLength(1);

    await dispatch(actions.addBreakpoint(cx, loc1));
    await dispatch(actions.addBreakpoint(cx, loc2));

    // The breakpoint list contains all the live breakpoints while the pending-breakpoint
    // list stores contains all the breakpoints persisted across toolbox reopenings.
    expect(selectors.getBreakpointsList(getState())).toHaveLength(2);
    expect(selectors.getPendingBreakpointList(getState())).toHaveLength(3);
    expect(Object.keys(asyncStore.pendingBreakpoints)).toHaveLength(3);

    await dispatch(actions.removeAllBreakpoints(cx));

    expect(selectors.getBreakpointsList(getState())).toHaveLength(0);
    expect(selectors.getPendingBreakpointList(getState())).toHaveLength(0);
    expect(Object.keys(asyncStore.pendingBreakpoints)).toHaveLength(0);
  });

  it("should toggle a breakpoint at a location", async () => {
    const loc = { sourceId: "foo1", line: 5, column: 1 };
    const getBp = () => selectors.getBreakpoint(getState(), loc);

    const { dispatch, getState, cx } = createStore(mockClient({ "5": [1] }));

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.selectLocation(cx, loc));

    await dispatch(actions.toggleBreakpointAtLine(cx, 5));
    const bp = getBp();
    expect(bp && !bp.disabled).toBe(true);

    await dispatch(actions.toggleBreakpointAtLine(cx, 5));
    expect(getBp()).toBe(undefined);
  });

  it("should disable/enable a breakpoint at a location", async () => {
    const location = { sourceId: "foo1", line: 5, column: 1 };
    const getBp = () => selectors.getBreakpoint(getState(), location);

    const { dispatch, getState, cx } = createStore(mockClient({ "5": [1] }));

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo1"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.selectLocation(cx, { sourceId: "foo1", line: 1 }));

    await dispatch(actions.toggleBreakpointAtLine(cx, 5));
    let bp = getBp();
    expect(bp && !bp.disabled).toBe(true);
    bp = getBp();
    if (!bp) {
      throw new Error("no bp");
    }
    await dispatch(actions.toggleDisabledBreakpoint(cx, bp));
    bp = getBp();
    expect(bp && bp.disabled).toBe(true);
  });

  it("should set the breakpoint condition", async () => {
    const { dispatch, getState, cx } = createStore(mockClient({ "5": [1] }));

    const loc = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.addBreakpoint(cx, loc));

    let bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && bp.options.condition).toBe(undefined);

    await dispatch(
      actions.setBreakpointOptions(cx, loc, {
        condition: "const foo = 0",
        getTextForLine: () => {},
      })
    );

    bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && bp.options.condition).toBe("const foo = 0");
  });

  it("should set the condition and enable a breakpoint", async () => {
    const { dispatch, getState, cx } = createStore(mockClient({ "5": [1] }));

    const loc = {
      sourceId: "a",
      line: 5,
      column: 1,
      sourceUrl: "http://localhost:8000/examples/a",
    };

    const source = await dispatch(actions.newGeneratedSource(makeSource("a")));
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.addBreakpoint(cx, loc));
    let bp = selectors.getBreakpoint(getState(), loc);
    if (!bp) {
      throw new Error("no breakpoint");
    }

    await dispatch(actions.disableBreakpoint(cx, bp));

    bp = selectors.getBreakpoint(getState(), loc);
    expect(bp && bp.options.condition).toBe(undefined);

    await dispatch(
      actions.setBreakpointOptions(cx, loc, {
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

  it("should remap breakpoints on pretty print", async () => {
    const { dispatch, getState, cx } = createStore(mockClient({ "1": [0] }));

    const loc = {
      sourceId: "a.js",
      line: 1,
      column: 0,
      sourceUrl: "http://localhost:8000/examples/a.js",
    };

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("a.js"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.addBreakpoint(cx, loc));
    await dispatch(actions.togglePrettyPrint(cx, "a.js"));

    const breakpoint = selectors.getBreakpointsList(getState())[0];

    expect(
      breakpoint.location.sourceUrl &&
        breakpoint.location.sourceUrl.includes("formatted")
    ).toBe(true);
    expect(breakpoint).toMatchSnapshot();
  });

  it("should remove the pretty-printed breakpoint that was added", async () => {
    const { dispatch, getState, cx } = createStore(mockClient({ "1": [0] }));

    const loc = {
      sourceId: "a.js",
      line: 1,
      column: 0,
      sourceUrl: "http://localhost:8000/examples/a.js",
    };

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("a.js"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.addBreakpoint(cx, loc));
    await dispatch(actions.togglePrettyPrint(cx, "a.js"));

    const breakpoint = selectors.getBreakpointsList(getState())[0];

    await dispatch(actions.removeBreakpoint(cx, breakpoint));

    const breakpointList = selectors.getPendingBreakpointList(getState());
    expect(breakpointList.length).toBe(0);
  });
});
