/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// TODO: we would like to mock this in the local tests
import {
  generateBreakpoint,
  mockPendingBreakpoint,
} from "./helpers/breakpoints.js";

import { mockCommandClient } from "./helpers/mockCommandClient";
import { asyncStore } from "../../utils/prefs";

function loadInitialState(opts = {}) {
  const mockedPendingBreakpoint = mockPendingBreakpoint({ ...opts, column: 2 });
  const l = mockedPendingBreakpoint.location;
  const id = `${l.sourceUrl}:${l.line}:${l.column}`;
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
  waitForState,
} from "../../utils/test-head";

import sourceMapLoader from "devtools/client/shared/source-map-loader/index";

function mockClient(bpPos = {}) {
  return {
    ...mockCommandClient,
    setSkipPausing: jest.fn(),
    getSourceActorBreakpointPositions: async () => bpPos,
    getSourceActorBreakableLines: async () => [],
  };
}

function mockSourceMaps() {
  return {
    ...sourceMapLoader,
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
    const { dispatch, getState } = createStore(
      mockClient({ 5: [1] }),
      loadInitialState(),
      mockSourceMaps()
    );

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    const bp = generateBreakpoint("foo.js", 5, 1);

    await dispatch(actions.addBreakpoint(bp.location));

    expect(selectors.getPendingBreakpointList(getState())).toHaveLength(2);
  });
});

describe("initializing when pending breakpoints exist in prefs", () => {
  it("syncs pending breakpoints", async () => {
    const { getState } = createStore(
      mockClient({ 5: [0] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bps = selectors.getPendingBreakpoints(getState());
    expect(bps).toMatchSnapshot();
  });

  it("re-adding breakpoints update existing pending breakpoints", async () => {
    const { dispatch, getState } = createStore(
      mockClient({ 5: [1, 2] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bar = generateBreakpoint("bar.js", 5, 1);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bar.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    await dispatch(actions.loadGeneratedSourceText(sourceActor));
    await dispatch(actions.addBreakpoint(bar.location));

    const bps = selectors.getPendingBreakpointList(getState());
    expect(bps).toHaveLength(2);
  });

  it("adding bps doesn't remove existing pending breakpoints", async () => {
    const { dispatch, getState } = createStore(
      mockClient({ 5: [0] }),
      loadInitialState(),
      mockSourceMaps()
    );
    const bp = generateBreakpoint("foo.js");

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("foo.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    await dispatch(actions.addBreakpoint(bp.location));

    const bps = selectors.getPendingBreakpointList(getState());
    expect(bps).toHaveLength(2);
  });
});

describe("initializing with disabled pending breakpoints in prefs", () => {
  it("syncs breakpoints with pending breakpoints", async () => {
    const store = createStore(
      mockClient({ 5: [2] }),
      loadInitialState({ disabled: true }),
      mockSourceMaps()
    );

    const { getState, dispatch } = store;

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bar.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    await waitForState(store, state => {
      const bps = selectors.getBreakpointsForSource(state, source);
      return bps && !!Object.values(bps).length;
    });

    const bp = selectors.getBreakpointsList(getState()).find(({ location }) => {
      return (
        location.line == 5 &&
        location.column == 2 &&
        location.source.id == source.id
      );
    });

    if (!bp) {
      throw new Error("no bp");
    }
    expect(bp.location.source.id).toEqual(source.id);
    expect(bp.disabled).toEqual(true);
  });
});

describe("adding sources", () => {
  it("corresponding breakpoints are added for a single source", async () => {
    const store = createStore(
      mockClient({ 5: [2] }),
      loadInitialState({ disabled: true }),
      mockSourceMaps()
    );
    const { getState, dispatch } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    const source = await dispatch(
      actions.newGeneratedSource(makeSource("bar.js"))
    );
    const sourceActor = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source.id
    );

    await dispatch(actions.loadGeneratedSourceText(sourceActor));

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);

    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });

  it("add corresponding breakpoints for multiple sources", async () => {
    const store = createStore(
      mockClient({ 5: [2] }),
      loadInitialState({ disabled: true }),
      mockSourceMaps()
    );
    const { getState, dispatch } = store;

    expect(selectors.getBreakpointCount(getState())).toEqual(0);

    const [source1, source2] = await dispatch(
      actions.newGeneratedSources([makeSource("bar.js"), makeSource("foo.js")])
    );
    const sourceActor1 = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source1.id
    );
    const sourceActor2 = selectors.getFirstSourceActorForGeneratedSource(
      getState(),
      source2.id
    );

    await dispatch(actions.loadGeneratedSourceText(sourceActor1));
    await dispatch(actions.loadGeneratedSourceText(sourceActor2));

    await waitForState(store, state => selectors.getBreakpointCount(state) > 0);
    expect(selectors.getBreakpointCount(getState())).toEqual(1);
  });
});
