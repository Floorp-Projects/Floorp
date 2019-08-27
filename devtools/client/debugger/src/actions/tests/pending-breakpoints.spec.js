/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

// TODO: we would like to mock this in the local tests
import {
  generateBreakpoint,
  mockPendingBreakpoint,
} from "./helpers/breakpoints.js";

import { mockCommandClient } from "./helpers/mockCommandClient";
import { asyncStore } from "../../utils/prefs";

function loadInitialState(opts = {}) {
  const mockedPendingBreakpoint = mockPendingBreakpoint({ ...opts, column: 2 });
  const id = makePendingLocationId(mockedPendingBreakpoint.location);
  asyncStore.pendingBreakpoints = { [id]: mockedPendingBreakpoint };

  return { pendingBreakpoints: asyncStore.pendingBreakpoints };
}

jest.mock("../../utils/prefs", () => ({
  prefs: {
    clientSourceMapsEnabled: true,
    expressions: [],
  },
  asyncStore: {
    pendingBreakpoints: {},
  },
  clear: jest.fn(),
  features: {
    inlinePreview: true,
  },
}));

import {
  createStore,
  selectors,
  actions,
  makeSource,
  makeSourceURL,
  waitForState,
} from "../../utils/test-head";

import sourceMaps from "devtools-source-map";

import { makePendingLocationId } from "../../utils/breakpoint";
function mockClient(bpPos = {}) {
  return {
    ...mockCommandClient,

    getSourceActorBreakpointPositions: async () => bpPos,
    getSourceActorBreakableLines: async () => [],
  };
}

function mockSourceMaps() {
  return {
    ...sourceMaps,
    getOriginalSourceText: async id => ({
      id,
      text: "",
      contentType: "text/javascript",
    }),
    getGeneratedRangesForOriginal: async () => [
      { start: { line: 0, column: 0 }, end: { line: 10, column: 10 } },
    ],
    getOriginalLocations: async items => items,
  };
}

describe("when adding breakpoints", () => {
  it("a corresponding pending breakpoint should be added", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [1] }),
      loadInitialState(),
      mockSourceMaps()
    );

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.loadSourceText({ cx, source }));

    const bp = generateBreakpoint("foo.js", 5, 1);
    const id = makePendingLocationId(bp.location);

    await dispatch(actions.addBreakpoint(cx, bp.location));
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
      const { dispatch, getState, cx } = createStore(
        mockClient({ "5": [0] }),
        loadInitialState(),
        mockSourceMaps()
      );

      await dispatch(actions.newGeneratedSource(makeSource("foo")));
      await dispatch(actions.newGeneratedSource(makeSource("foo2")));

      const source1 = await dispatch(
        actions.newGeneratedSource(makeSource("foo"))
      );
      const source2 = await dispatch(
        actions.newGeneratedSource(makeSource("foo2"))
      );

      await dispatch(actions.loadSourceText({ cx, source: source1 }));
      await dispatch(actions.loadSourceText({ cx, source: source2 }));

      await dispatch(actions.addBreakpoint(cx, breakpoint1.location));
      await dispatch(actions.addBreakpoint(cx, breakpoint2.location));

      const pendingBps = selectors.getPendingBreakpoints(getState());

      // NOTE the sourceId should be `foo2/originalSource`, but is `foo2`
      // because we do not have a real source map for `getOriginalLocation`
      // to map.
      expect(pendingBps[breakpointLocationId1]).toMatchSnapshot();
      expect(pendingBps[breakpointLocationId2]).toMatchSnapshot();
    });

    it("hidden breakponts do not create pending bps", async () => {
      const { dispatch, getState, cx } = createStore(
        mockClient({ "5": [0] }),
        loadInitialState(),
        mockSourceMaps()
      );

      await dispatch(actions.newGeneratedSource(makeSource("foo")));
      const source = await dispatch(
        actions.newGeneratedSource(makeSource("foo"))
      );
      await dispatch(actions.loadSourceText({ cx, source }));

      await dispatch(
        actions.addBreakpoint(cx, breakpoint1.location, { hidden: true })
      );
      const pendingBps = selectors.getPendingBreakpoints(getState());

      expect(pendingBps[breakpointLocationId1]).toBeUndefined();
    });

    it("remove a corresponding pending breakpoint when deleting", async () => {
      const { dispatch, getState, cx } = createStore(
        mockClient({ "5": [0] }),
        loadInitialState(),
        mockSourceMaps()
      );

      await dispatch(actions.newGeneratedSource(makeSource("foo")));
      await dispatch(actions.newGeneratedSource(makeSource("foo2")));

      const source1 = await dispatch(
        actions.newGeneratedSource(makeSource("foo"))
      );
      const source2 = await dispatch(
        actions.newGeneratedSource(makeSource("foo2"))
      );

      await dispatch(actions.loadSourceText({ cx, source: source1 }));
      await dispatch(actions.loadSourceText({ cx, source: source2 }));

      await dispatch(actions.addBreakpoint(cx, breakpoint1.location));
      await dispatch(actions.addBreakpoint(cx, breakpoint2.location));
      await dispatch(actions.removeBreakpoint(cx, breakpoint1));

      const pendingBps = selectors.getPendingBreakpoints(getState());
      expect(pendingBps.hasOwnProperty(breakpointLocationId1)).toBe(false);
      expect(pendingBps.hasOwnProperty(breakpointLocationId2)).toBe(true);
    });
  });
});

describe("when changing an existing breakpoint", () => {
  it("updates corresponding pendingBreakpoint", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [0] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bp = generateBreakpoint("foo");
    const id = makePendingLocationId(bp.location);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo"))
    );
    await dispatch(actions.newGeneratedSource(makeSource("foo")));
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.addBreakpoint(cx, bp.location));
    await dispatch(
      actions.setBreakpointOptions(cx, bp.location, { condition: "2" })
    );
    const bps = selectors.getPendingBreakpoints(getState());
    const breakpoint = bps[id];
    expect(breakpoint.options.condition).toBe("2");
  });

  it("if disabled, updates corresponding pendingBreakpoint", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [0] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bp = generateBreakpoint("foo");
    const id = makePendingLocationId(bp.location);

    await dispatch(actions.newGeneratedSource(makeSource("foo")));

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.addBreakpoint(cx, bp.location));
    await dispatch(actions.disableBreakpoint(cx, bp));
    const bps = selectors.getPendingBreakpoints(getState());
    const breakpoint = bps[id];
    expect(breakpoint.disabled).toBe(true);
  });

  it("does not delete the pre-existing pendingBreakpoint", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [0] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bp = generateBreakpoint("foo.js");

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.loadSourceText({ cx, source }));

    const id = makePendingLocationId(bp.location);

    await dispatch(actions.addBreakpoint(cx, bp.location));
    await dispatch(
      actions.setBreakpointOptions(cx, bp.location, { condition: "2" })
    );
    const bps = selectors.getPendingBreakpoints(getState());
    const breakpoint = bps[id];
    expect(breakpoint.options.condition).toBe("2");
  });
});

describe("initializing when pending breakpoints exist in prefs", () => {
  it("syncs pending breakpoints", async () => {
    const { getState } = createStore(
      mockClient({ "5": [0] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bps = selectors.getPendingBreakpoints(getState());
    expect(bps).toMatchSnapshot();
  });

  it("re-adding breakpoints update existing pending breakpoints", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [1, 2] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bar = generateBreakpoint("bar.js", 5, 1);

    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bar.js"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));
    await dispatch(actions.addBreakpoint(cx, bar.location));

    const bps = selectors.getPendingBreakpointList(getState());
    expect(bps).toHaveLength(2);
  });

  it("adding bps doesn't remove existing pending breakpoints", async () => {
    const { dispatch, getState, cx } = createStore(
      mockClient({ "5": [0] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bp = generateBreakpoint("foo.js");

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    await dispatch(actions.loadSourceText({ cx, source }));

    await dispatch(actions.addBreakpoint(cx, bp.location));

    const bps = selectors.getPendingBreakpointList(getState());
    expect(bps).toHaveLength(2);
  });
});

describe("initializing with disabled pending breakpoints in prefs", () => {
  it("syncs breakpoints with pending breakpoints", async () => {
    const store = createStore(
      mockClient({ "5": [2] }),
      loadInitialState({ disabled: true }),
      mockSourceMaps()
    );

    const { getState, dispatch, cx } = store;

    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bar.js"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await waitForState(store, state => {
      const bps = selectors.getBreakpointsForSource(state, source.id);
      return bps && Object.values(bps).length > 0;
    });

    const bp = selectors.getBreakpointForLocation(getState(), {
      line: 5,
      column: 2,
      sourceUrl: source.url,
      sourceId: source.id,
    });
    if (!bp) {
      throw new Error("no bp");
    }
    expect(bp.location.sourceId).toEqual(source.id);
    expect(bp.disabled).toEqual(true);
  });
});

describe("adding sources", () => {
  it("corresponding breakpoints are added for a single source", async () => {
    const store = createStore(
      mockClient({ "5": [2] }),
      loadInitialState({ disabled: true }),
      mockSourceMaps()
    );
    const { getState, dispatch, cx } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bar.js"))
    );
    await dispatch(actions.loadSourceText({ cx, source }));

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("corresponding breakpoints are added to the original source", async () => {
    const sourceURL = makeSourceURL("bar.js");
    const store = createStore(mockClient({ "5": [2] }), loadInitialState(), {
      getOriginalURLs: async source => [
        {
          id: sourceMaps.generatedToOriginalId(source.id, sourceURL),
          url: sourceURL,
        },
      ],
      getOriginalSourceText: async () => ({ text: "" }),
      getGeneratedLocation: async location => ({
        line: location.line,
        column: location.column,
        sourceId: location.sourceId,
      }),
      getOriginalLocation: async location => location,
      getGeneratedRangesForOriginal: async () => [
        { start: { line: 0, column: 0 }, end: { line: 10, column: 10 } },
      ],
      getOriginalLocations: async items =>
        items.map(item => ({
          ...item,
          sourceId: sourceMaps.generatedToOriginalId(item.sourceId, sourceURL),
        })),
    });

    const { getState, dispatch } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    await dispatch(
      actions.newGeneratedSource(makeSource("bar.js", { sourceMapURL: "foo" }))
    );

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("add corresponding breakpoints for multiple sources", async () => {
    const store = createStore(
      mockClient({ "5": [2] }),
      loadInitialState({ disabled: true }),
      mockSourceMaps()
    );
    const { getState, dispatch, cx } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    await dispatch(actions.newGeneratedSource(makeSource("bar.js")));
    await dispatch(actions.newGeneratedSource(makeSource("foo.js")));
    const [source1, source2] = await dispatch(
      actions.newGeneratedSources([makeSource("bar.js"), makeSource("foo.js")])
    );
    await dispatch(actions.loadSourceText({ cx, source: source1 }));
    await dispatch(actions.loadSourceText({ cx, source: source2 }));

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);
    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });
});
