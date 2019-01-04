/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// TODO: we would like to mock this in the local tests
import {
  generateBreakpoint,
  mockPendingBreakpoint
} from "./helpers/breakpoints.js";

import {
  simulateCorrectThreadClient,
  simpleMockThreadClient
} from "./helpers/threadClient.js";

import { asyncStore } from "../../utils/prefs";

function loadInitialState(opts = {}) {
  const mockedPendingBreakpoint = mockPendingBreakpoint(opts);
  const id = makePendingLocationId(mockedPendingBreakpoint.location);
  asyncStore.pendingBreakpoints = { [id]: mockedPendingBreakpoint };

  return { pendingBreakpoints: asyncStore.pendingBreakpoints };
}

jest.mock("../../utils/prefs", () => ({
  prefs: {
    expressions: []
  },
  asyncStore: {
    pendingBreakpoints: {}
  },
  features: {
    replay: false
  },
  clear: jest.fn()
}));

import "../sources/loadSourceText";

import {
  createStore,
  selectors,
  actions,
  makeSource,
  waitForState
} from "../../utils/test-head";

import { makePendingLocationId } from "../../utils/breakpoint";

describe("when adding breakpoints", () => {
  it("a corresponding pending breakpoint should be added", async () => {
    const { dispatch, getState } = createStore(
      simpleMockThreadClient,
      loadInitialState()
    );

    await dispatch(actions.newSource(makeSource("foo.js")));
    await dispatch(actions.loadSourceText(makeSource("foo.js")));

    const bp = generateBreakpoint("foo.js");
    const id = makePendingLocationId(bp.location);

    await dispatch(actions.addBreakpoint(bp.location));
    const pendingBps = selectors.getPendingBreakpoints(getState());

    expect(selectors.getPendingBreakpointList(getState())).toHaveLength(2);
    expect(pendingBps[id]).toMatchSnapshot();
  });

  describe("adding and deleting breakpoints", () => {
    let breakpoint1;
    let breakpoint2;
    let breakpointLocationId1;
    let breakpointLocationId2;

    beforeEach(() => {
      breakpoint1 = generateBreakpoint("foo");
      breakpoint2 = generateBreakpoint("foo2");
      breakpointLocationId1 = makePendingLocationId(breakpoint1.location);
      breakpointLocationId2 = makePendingLocationId(breakpoint2.location);
    });

    it("add a corresponding pendingBreakpoint for each addition", async () => {
      const { dispatch, getState } = createStore(
        simpleMockThreadClient,
        loadInitialState()
      );

      await dispatch(actions.newSource(makeSource("foo")));
      await dispatch(actions.newSource(makeSource("foo2")));

      await dispatch(actions.loadSourceText(makeSource("foo")));
      await dispatch(actions.loadSourceText(makeSource("foo2")));

      await dispatch(actions.addBreakpoint(breakpoint1.location));
      await dispatch(actions.addBreakpoint(breakpoint2.location));

      const pendingBps = selectors.getPendingBreakpoints(getState());
      expect(pendingBps[breakpointLocationId1]).toMatchSnapshot();
      expect(pendingBps[breakpointLocationId2]).toMatchSnapshot();
    });

    it("hidden breakponts do not create pending bps", async () => {
      const { dispatch, getState } = createStore(
        simpleMockThreadClient,
        loadInitialState()
      );

      await dispatch(actions.newSource(makeSource("foo")));
      await dispatch(actions.loadSourceText(makeSource("foo")));

      await dispatch(
        actions.addBreakpoint(breakpoint1.location, { hidden: true })
      );
      const pendingBps = selectors.getPendingBreakpoints(getState());

      expect(pendingBps[breakpointLocationId1]).toBeUndefined();
    });

    it("remove a corresponding pending breakpoint when deleting", async () => {
      const { dispatch, getState } = createStore(
        simpleMockThreadClient,
        loadInitialState()
      );
      await dispatch(actions.newSource(makeSource("foo")));
      await dispatch(actions.newSource(makeSource("foo2")));
      await dispatch(actions.loadSourceText(makeSource("foo")));
      await dispatch(actions.loadSourceText(makeSource("foo2")));

      await dispatch(actions.addBreakpoint(breakpoint1.location));
      await dispatch(actions.addBreakpoint(breakpoint2.location));
      await dispatch(actions.removeBreakpoint(breakpoint1.location));

      const pendingBps = selectors.getPendingBreakpoints(getState());
      expect(pendingBps.hasOwnProperty(breakpointLocationId1)).toBe(false);
      expect(pendingBps.hasOwnProperty(breakpointLocationId2)).toBe(true);
    });
  });
});

describe("when changing an existing breakpoint", () => {
  it("updates corresponding pendingBreakpoint", async () => {
    const { dispatch, getState } = createStore(
      simpleMockThreadClient,
      loadInitialState()
    );
    const bp = generateBreakpoint("foo");
    const id = makePendingLocationId(bp.location);
    await dispatch(actions.newSource(makeSource("foo")));
    await dispatch(actions.loadSourceText(makeSource("foo")));

    await dispatch(actions.addBreakpoint(bp.location));
    await dispatch(
      actions.setBreakpointCondition(bp.location, { condition: "2" })
    );
    const bps = selectors.getPendingBreakpoints(getState());
    const breakpoint = bps[id];
    expect(breakpoint.condition).toBe("2");
  });

  it("if disabled, updates corresponding pendingBreakpoint", async () => {
    const { dispatch, getState } = createStore(
      simpleMockThreadClient,
      loadInitialState()
    );
    const bp = generateBreakpoint("foo");
    const id = makePendingLocationId(bp.location);

    await dispatch(actions.newSource(makeSource("foo")));
    await dispatch(actions.loadSourceText(makeSource("foo")));

    await dispatch(actions.addBreakpoint(bp.location));
    await dispatch(actions.disableBreakpoint(bp.location));
    const bps = selectors.getPendingBreakpoints(getState());
    const breakpoint = bps[id];
    expect(breakpoint.disabled).toBe(true);
  });

  it("does not delete the pre-existing pendingBreakpoint", async () => {
    const { dispatch, getState } = createStore(
      simpleMockThreadClient,
      loadInitialState()
    );
    const bp = generateBreakpoint("foo.js");
    const source = makeSource("foo.js");
    await dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText(makeSource("foo.js")));

    const id = makePendingLocationId(bp.location);

    await dispatch(actions.addBreakpoint(bp.location));
    await dispatch(
      actions.setBreakpointCondition(bp.location, { condition: "2" })
    );
    const bps = selectors.getPendingBreakpoints(getState());
    const breakpoint = bps[id];
    expect(breakpoint.condition).toBe("2");
  });
});

describe("initializing when pending breakpoints exist in prefs", () => {
  it("syncs pending breakpoints", async () => {
    const { getState } = createStore(
      simpleMockThreadClient,
      loadInitialState()
    );
    const bps = selectors.getPendingBreakpoints(getState());
    expect(bps).toMatchSnapshot();
  });

  it("re-adding breakpoints update existing pending breakpoints", async () => {
    const { dispatch, getState } = createStore(
      simpleMockThreadClient,
      loadInitialState()
    );
    const bar = generateBreakpoint("bar.js");
    await dispatch(actions.newSource(makeSource("bar.js")));
    await dispatch(actions.loadSourceText(makeSource("bar.js")));

    await dispatch(actions.addBreakpoint(bar.location));

    const bps = selectors.getPendingBreakpointList(getState());
    expect(bps).toHaveLength(1);
  });

  it("adding bps doesn't remove existing pending breakpoints", async () => {
    const { dispatch, getState } = createStore(
      simpleMockThreadClient,
      loadInitialState()
    );
    const bp = generateBreakpoint("foo.js");

    await dispatch(actions.newSource(makeSource("foo.js")));
    await dispatch(actions.loadSourceText(makeSource("foo.js")));

    await dispatch(actions.addBreakpoint(bp.location));

    const bps = selectors.getPendingBreakpointList(getState());
    expect(bps).toHaveLength(2);
  });
});

describe("initializing with disabled pending breakpoints in prefs", () => {
  it("syncs breakpoints with pending breakpoints", async () => {
    const store = createStore(
      simpleMockThreadClient,
      loadInitialState({ disabled: true })
    );

    const { getState, dispatch } = store;
    const source = makeSource("bar.js");

    await dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText(makeSource("bar.js")));

    await waitForState(store, state => {
      const bps = selectors.getBreakpointsForSource(state, "bar.js");
      return bps && Object.values(bps).length > 0;
    });

    const bp = selectors.getBreakpointForLocation(getState(), {
      line: 5,
      column: undefined,
      sourceUrl: "http://localhost:8000/examples/bar.js",
      sourceId: "bar.js"
    });
    expect(bp.location.sourceId).toEqual("bar.js");
    expect(bp.disabled).toEqual(true);
  });
});

describe("adding sources", () => {
  it("corresponding breakpoints are added for a single source", async () => {
    const store = createStore(simpleMockThreadClient, loadInitialState());
    const { getState, dispatch } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    const source = makeSource("bar.js");
    await dispatch(actions.newSource(source));
    await dispatch(actions.loadSourceText(makeSource("bar.js")));

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("corresponding breakpoints are added to the original source", async () => {
    const source = makeSource("bar.js", { sourceMapURL: "foo" });
    const store = createStore(simpleMockThreadClient, loadInitialState(), {
      getOriginalURLs: async () => [source.url],
      getOriginalSourceText: async () => ({ source: "" }),
      getGeneratedLocation: async (location, _source) => ({
        line: location.line,
        column: location.column,
        sourceId: _source.id
      }),
      getOriginalLocation: async location => location
    });

    const { getState, dispatch } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    await dispatch(actions.newSource(source));

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("add corresponding breakpoints for multiple sources", async () => {
    const store = createStore(simpleMockThreadClient, loadInitialState());
    const { getState, dispatch } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    const source1 = makeSource("bar.js");
    const source2 = makeSource("foo.js");
    await dispatch(actions.newSources([source1, source2]));
    await dispatch(actions.loadSourceText(makeSource("foo.js")));
    await dispatch(actions.loadSourceText(makeSource("bar.js")));

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });
});

describe("invalid breakpoint location", () => {
  it("a corrected corresponding pending breakpoint is added", async () => {
    // setup
    const bp = generateBreakpoint("foo.js");
    const {
      correctedThreadClient,
      correctedLocation
    } = simulateCorrectThreadClient(2, bp.location);
    const { dispatch, getState } = createStore(correctedThreadClient);
    const correctedPendingId = makePendingLocationId(correctedLocation);

    // test
    await dispatch(actions.newSource(makeSource("foo.js")));
    await dispatch(actions.loadSourceText(makeSource("foo.js")));

    await dispatch(actions.addBreakpoint(bp.location));
    const pendingBps = selectors.getPendingBreakpoints(getState());
    const pendingBp = pendingBps[correctedPendingId];
    expect(pendingBp).toMatchSnapshot();
  });
});
